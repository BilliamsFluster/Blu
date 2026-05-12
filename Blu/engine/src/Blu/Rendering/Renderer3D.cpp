#include "Blupch.h"
#include "Renderer3D.h"
#include "Renderer.h"
#include "EditorCamera.h"
#include "Camera.h"
#include "Shader.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"

#include "RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Blu
{
    struct Renderer3DData
    {
        Shared<Shader> MeshShader;
        glm::mat4      ViewProjectionMatrix = glm::mat4(1.0f);
        glm::mat4      ViewMatrix           = glm::mat4(1.0f);
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
        s_Data3D->ViewMatrix           = camera.GetViewMatrix();
        s_Data3D->ViewPos              = camera.GetPosition();
    }

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
    {
        s_Data3D->ViewMatrix           = glm::inverse(transform);
        s_Data3D->ViewProjectionMatrix = camera.GetProjectionMatrix() * s_Data3D->ViewMatrix;
        s_Data3D->ViewPos              = glm::vec3(transform[3]);
    }

    void Renderer3D::EndScene()
    {
    }

    // -------------------------------------------------------------------------
    // HLSL cbuffer-padded light structs — matches Renderer3D_Mesh.hlsl exactly.
    // Each float3 occupies 16 bytes (padded to float4 stride) in a cbuffer.
    // -------------------------------------------------------------------------
    struct alignas(16) DirLightGPU
    {
        glm::vec3 Direction; float pad0;      // 16 bytes
        glm::vec3 Ambient;   float Intensity; // 16
        glm::vec3 Diffuse;   float pad1;      // 16
        glm::vec3 Specular;  float pad2;      // 16
        // total: 64
    };

    struct alignas(16) PointLightGPU
    {
        glm::vec3 Position;  float Range;     // 16
        glm::vec3 Ambient;   float Intensity; // 16
        glm::vec3 Diffuse;   float pad0;      // 16
        glm::vec3 Specular;  float pad1;      // 16
        glm::vec3 Att;       float pad2;      // 16
        // total: 80
    };

    struct alignas(16) SpotLightGPU
    {
        glm::vec3 Position;     float Range;       // 16
        glm::vec3 Direction;    float Intensity;   // 16
        glm::vec3 Ambient;      float pad0;        // 16
        glm::vec3 Diffuse;      float pad1;        // 16
        glm::vec3 Specular;     float pad2;        // 16
        glm::vec3 Att;          float InnerCutoff; // 16
        float      OuterCutoff; float pad3[3];     // 16
        // total: 112
    };

    struct alignas(16) LightDataGPU
    {
        DirLightGPU   DirLights[4];   // 256
        PointLightGPU PointLights[8]; // 640
        SpotLightGPU  SpotLights[4];  // 448
        int           NumDirLights;   // 4
        int           NumPointLights; // 4
        int           NumSpotLights;  // 4
        float         PadL;           // 4 → 16
        // total: 1360
    };
    static_assert(sizeof(LightDataGPU) == 1360, "LightDataGPU size must match HLSL cbuffer");

    // -------------------------------------------------------------------------
    // PassLights — upload all lights as a single cbuffer blob (no string allocs).
    // -------------------------------------------------------------------------
    void Renderer3D::PassLights(
        const std::vector<DirLightData>&   dirLights,
        const std::vector<PointLightData>& pointLights,
        const std::vector<SpotLightData>&  spotLights)
    {
        auto& sh = *s_Data3D->MeshShader;

        constexpr int kMaxDir   = 4;
        constexpr int kMaxPoint = 8;
        constexpr int kMaxSpot  = 4;

        const int nd = std::min((int)dirLights.size(),   kMaxDir);
        const int np = std::min((int)pointLights.size(), kMaxPoint);
        const int ns = std::min((int)spotLights.size(),  kMaxSpot);

        LightDataGPU gpu = {};
        gpu.NumDirLights   = nd;
        gpu.NumPointLights = np;
        gpu.NumSpotLights  = ns;

        for (int i = 0; i < nd; ++i)
        {
            const auto& L = dirLights[i];
            gpu.DirLights[i].Direction = glm::normalize(L.Direction);
            gpu.DirLights[i].Ambient   = L.Ambient   * L.Intensity;
            gpu.DirLights[i].Diffuse   = L.Diffuse   * L.Intensity;
            gpu.DirLights[i].Specular  = L.Specular  * L.Intensity;
            gpu.DirLights[i].Intensity = L.Intensity;
        }
        for (int i = 0; i < np; ++i)
        {
            const auto& L = pointLights[i];
            gpu.PointLights[i].Position  = L.Position;
            gpu.PointLights[i].Ambient   = L.Ambient   * L.Intensity;
            gpu.PointLights[i].Diffuse   = L.Diffuse   * L.Intensity;
            gpu.PointLights[i].Specular  = L.Specular  * L.Intensity;
            gpu.PointLights[i].Intensity = L.Intensity;
            gpu.PointLights[i].Range     = L.Range;
            gpu.PointLights[i].Att       = glm::vec3(L.AttConstant, L.AttLinear, L.AttQuadratic);
        }
        for (int i = 0; i < ns; ++i)
        {
            const auto& L = spotLights[i];
            gpu.SpotLights[i].Position     = L.Position;
            gpu.SpotLights[i].Direction    = glm::normalize(L.Direction);
            gpu.SpotLights[i].Ambient      = L.Ambient   * L.Intensity;
            gpu.SpotLights[i].Diffuse      = L.Diffuse   * L.Intensity;
            gpu.SpotLights[i].Specular     = L.Specular  * L.Intensity;
            gpu.SpotLights[i].Intensity    = L.Intensity;
            gpu.SpotLights[i].Range        = L.Range;
            gpu.SpotLights[i].InnerCutoff  = L.InnerCutoffCos;
            gpu.SpotLights[i].OuterCutoff  = L.OuterCutoffCos;
            gpu.SpotLights[i].Att          = glm::vec3(L.AttConstant, L.AttLinear, L.AttQuadratic);
        }

        // Single bulk upload — no string allocations, one memcpy into cbuffer shadow.
        sh.SetUniformBuffer("LightData", &gpu, sizeof(gpu));

        // View position is a separate PerFrame cbuffer member, still uploaded via
        // the normal path (single float3, negligible cost).
        sh.SetUniformFloat3("u_ViewPos", s_Data3D->ViewPos);
    }

    void Renderer3D::SetLights(
        const std::vector<DirLightData>&   dirLights,
        const std::vector<PointLightData>& pointLights,
        const std::vector<SpotLightData>&  spotLights)
    {
        s_Data3D->MeshShader->Bind();
        PassLights(dirLights, pointLights, spotLights);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }

    static void UploadSubMesh(SubMesh& submesh)
    {
        submesh.VAO = VertexArray::Create();
        Shared<VertexBuffer> vb = VertexBuffer::Create((uint32_t)(submesh.Vertices.size() * sizeof(MeshVertex)));
        vb->SetData(submesh.Vertices.data(), (uint32_t)(submesh.Vertices.size() * sizeof(MeshVertex)));
        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" },
        };
        vb->SetLayout(layout);
        submesh.VAO->AddVertexBuffer(vb);
        Shared<IndexBuffer> ib = IndexBuffer::Create(submesh.Indices.data(), (uint32_t)submesh.Indices.size());
        submesh.VAO->AddIndexBuffer(ib);
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID)
    {
        if (!mc.MeshData && !mc.ModelAsset) return;

        s_Data3D->MeshShader->Bind();
        s_Data3D->MeshShader->SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
        s_Data3D->MeshShader->SetUniformMat4("u_Model", transform);

        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
        if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
        {
            s_Data3D->MeshShader->SetUniformFloat3("u_NormalCol0", normalMatrix[0]);
            s_Data3D->MeshShader->SetUniformFloat3("u_NormalCol1", normalMatrix[1]);
            s_Data3D->MeshShader->SetUniformFloat3("u_NormalCol2", normalMatrix[2]);
        }
        else
        {
            s_Data3D->MeshShader->SetUniformMat3("u_NormalMatrix", normalMatrix);
        }

        if (mc.MaterialInstance)
        {
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.ambient",  mc.MaterialInstance->GetAmbientColor());
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.diffuse",  mc.MaterialInstance->GetDiffuseColor());
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.specular", mc.MaterialInstance->GetSpecularColor());
            s_Data3D->MeshShader->SetUniformFloat ("u_Material.shininess", mc.MaterialInstance->GetShininess());
        }
        else
        {
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.ambient",  glm::vec3(0.2f));
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.diffuse",  glm::vec3(0.8f));
            s_Data3D->MeshShader->SetUniformFloat3("u_Material.specular", glm::vec3(0.5f));
            s_Data3D->MeshShader->SetUniformFloat ("u_Material.shininess", 32.0f);
        }

        s_Data3D->MeshShader->SetUniformInt("u_EntityID", entityID);

        // Upload all per-object uniforms once
        s_Data3D->MeshShader->Flush();

        if (mc.ModelAsset)
        {
            for (auto& submesh : mc.ModelAsset->Meshes)
            {
                if (!submesh.VAO)
                    UploadSubMesh(submesh);

                bool hasAlbedo = false;
                if (submesh.MaterialIndex >= 0 && submesh.MaterialIndex < (int)mc.ModelAsset->Materials.size())
                {
                    auto& mat = mc.ModelAsset->Materials[submesh.MaterialIndex];
                    if (mat && mat->AlbedoMap)
                    {
                        mat->AlbedoMap->Bind(0);
                        s_Data3D->MeshShader->SetUniformInt("u_AlbedoTexture", 0);
                        hasAlbedo = true;
                    }
                }
                // Update texture flag per-submesh and upload before draw
                s_Data3D->MeshShader->SetUniformInt("u_HasAlbedoTexture", hasAlbedo ? 1 : 0);
                s_Data3D->MeshShader->Flush();

                submesh.VAO->Bind();
                RenderCommand::DrawIndexed(submesh.VAO, (uint32_t)submesh.Indices.size());
                submesh.VAO->UnBind();
            }
        }
        else if (mc.MeshData)
        {
            bool hasAlbedo = mc.MaterialInstance && mc.MaterialInstance->AlbedoMap;
            if (hasAlbedo)
            {
                mc.MaterialInstance->AlbedoMap->Bind(0);
                s_Data3D->MeshShader->SetUniformInt("u_AlbedoTexture", 0);
            }
            s_Data3D->MeshShader->SetUniformInt("u_HasAlbedoTexture", hasAlbedo ? 1 : 0);
            s_Data3D->MeshShader->Flush();

            mc.MeshData->GetVertexArray()->Bind();
            RenderCommand::DrawIndexed(mc.MeshData->GetVertexArray(), mc.MeshData->GetIndexCount());
            mc.MeshData->GetVertexArray()->UnBind();
        }

        s_Data3D->MeshShader->UnBind();
    }
}
