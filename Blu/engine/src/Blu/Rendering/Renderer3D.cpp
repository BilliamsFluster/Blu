#include "Blupch.h"
#include "Renderer3D.h"
#include "Renderer.h"
#include "EditorCamera.h"
#include "Camera.h"
#include "Shader.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"
#include "Blu/LightSystem/LightManager.h"

#include "RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Blu
{
    struct Renderer3DData
    {
        Shared<Shader> MeshShader;
        glm::mat4      ViewProjectionMatrix = glm::mat4(1.0f);
        glm::vec3      ViewPos              = glm::vec3(0.0f);
    };

    static Renderer3DData* s_Data3D = nullptr;

    void Renderer3D::Init()
    {
        s_Data3D = new Renderer3DData();
        if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
            Renderer::GetShaderLibrary()->Load("assets/shaders/DX11/Renderer3D_Mesh.hlsl");
        else
            Renderer::GetShaderLibrary()->Load("assets/shaders/Renderer3D_Mesh.glsl");
        s_Data3D->MeshShader = Renderer::GetShaderLibrary()->Get("Renderer3D_Mesh");
    }

    void Renderer3D::Shutdown()
    {
        delete s_Data3D;
        s_Data3D = nullptr;
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        s_Data3D->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
        s_Data3D->ViewPos              = camera.GetPosition();
    }

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
    {
        s_Data3D->ViewProjectionMatrix = camera.GetProjectionMatrix() * glm::inverse(transform);
        s_Data3D->ViewPos              = glm::vec3(transform[3]);
    }

    void Renderer3D::EndScene()
    {
    }

    void Renderer3D::PassLights(Shared<LightManager> lightManager)
    {
        auto lights = lightManager->GetPointLights();  // copy so elements are non-const
        int numLights = (int)lights.size();
        constexpr int MaxLights = 8;

        s_Data3D->MeshShader->SetUniformInt("u_NumLights", std::min(numLights, MaxLights));
        s_Data3D->MeshShader->SetUniformFloat3("u_ViewPos", s_Data3D->ViewPos);

        for (int i = 0; i < numLights && i < MaxLights; i++)
        {
            auto& tc  = lights[i].GetComponent<TransformComponent>();
            auto& plc = lights[i].GetComponent<PointLightComponent>();

            std::string p = "u_Lights[" + std::to_string(i) + "].";
            s_Data3D->MeshShader->SetUniformFloat3(p + "position", tc.Translation);
            s_Data3D->MeshShader->SetUniformFloat3(p + "ambient",  plc.AmbientColor);
            s_Data3D->MeshShader->SetUniformFloat3(p + "diffuse",  plc.DiffuseColor);
            s_Data3D->MeshShader->SetUniformFloat3(p + "specular", plc.SpecularColor);
        }
    }

    void Renderer3D::SetLights(Shared<LightManager> lightManager)
    {
        s_Data3D->MeshShader->Bind();
        PassLights(lightManager);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID)
    {
        if (!mc.MeshData) return;

        s_Data3D->MeshShader->Bind();
        s_Data3D->MeshShader->SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
        s_Data3D->MeshShader->SetUniformMat4("u_Model", transform);

        // Normal matrix (handles non-uniform scaling correctly)
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));
        s_Data3D->MeshShader->SetUniformMat3("u_NormalMatrix", normalMatrix);

        if (mc.MaterialInstance)
        {
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.ambient",  mc.MaterialInstance->GetAmbientColor());
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.diffuse",  mc.MaterialInstance->GetDiffuseColor());
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.specular", mc.MaterialInstance->GetSpecularColor());
            s_Data3D->MeshShader->SetUniformFloat("u_Material.shininess",  mc.MaterialInstance->GetShininess());
        }
        else
        {
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.ambient",  glm::vec3(0.2f));
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.diffuse",  glm::vec3(0.8f));
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.specular", glm::vec3(0.5f));
            s_Data3D->MeshShader->SetUniformFloat("u_Material.shininess",  32.0f);
        }

        s_Data3D->MeshShader->SetUniformInt("u_EntityID", entityID);

        // Flush cbuffer shadow to GPU now that all uniforms are written (DX11 no-op on OpenGL)
        s_Data3D->MeshShader->Flush();

        mc.MeshData->GetVertexArray()->Bind();
        RenderCommand::DrawIndexed(mc.MeshData->GetVertexArray(), mc.MeshData->GetIndexCount());
        mc.MeshData->GetVertexArray()->UnBind();

        s_Data3D->MeshShader->UnBind();
    }
}
