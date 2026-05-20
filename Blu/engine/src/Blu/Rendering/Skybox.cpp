#include "Blupch.h"
#include "Skybox.h"
#include "Shader.h"
#include "Renderer.h"
#include "PipelineState.h"
#include "RendererAPI.h"
#include "Blu/Core/Log.h"
#include "Blu/Platform/DirectX11/D3D11Context.h"
#include <d3d11.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace Blu
{
    Skybox::Skybox()
    {
        if (RendererAPI::GetAPI() != RendererAPI::API::Direct3D)
        {
            BLU_CORE_WARN("Skybox: only implemented for DirectX 11");
            return;
        }

        auto& lib = *Renderer::GetShaderLibrary();
        lib.Load("assets/shaders/DX11/Skybox.hlsl");
        m_Shader = lib.Get("Skybox");
        if (!m_Shader)
        {
            BLU_CORE_ERROR("Skybox: failed to load Skybox.hlsl");
            return;
        }

        // Depth read-only LESS_EQUAL so the sky fills only background pixels.
        PipelineStateDesc desc;
        desc.DepthStencilState.DepthEnable    = true;
        desc.DepthStencilState.DepthWriteMask = false;
        desc.DepthStencilState.DepthFunc      = ComparisonFunc::LessEqual;
        desc.RasterizerState.CullMode         = CullMode::None;
        m_PipelineState = PipelineState::Create(desc);

        m_Ready = true;
    }

    void Skybox::Render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                        const glm::vec3& sunDirection, float time)
    {
        if (!m_Ready) return;

        glm::mat4 invView = glm::inverse(viewMatrix);
        glm::mat4 invProj = glm::inverse(projMatrix);

        m_PipelineState->Bind();
        m_Shader->Bind();

        m_Shader->SetUniformMat4("u_InvView",           invView);
        m_Shader->SetUniformMat4("u_InvProjection",     invProj);
        m_Shader->SetUniformFloat3("u_GroundColor",     GroundColor);
        m_Shader->SetUniformFloat("u_Turbidity",        Turbidity);
        m_Shader->SetUniformFloat3("u_SunDir",          glm::normalize(sunDirection));
        m_Shader->SetUniformFloat("u_SkyExposure",      SkyExposure);
        m_Shader->SetUniformFloat3("u_SunColor",        SunColor);
        m_Shader->SetUniformFloat("u_SunSize",          SunSize);
        m_Shader->SetUniformFloat("u_SunStrength",      SunStrength);
        m_Shader->SetUniformFloat3("u_CloudColor",      CloudColor);
        m_Shader->SetUniformFloat("u_CloudCoverage",    CloudCoverage);
        m_Shader->SetUniformFloat("u_CloudDensity",     CloudDensity);
        m_Shader->SetUniformFloat("u_CloudHeight",      CloudHeight);
        m_Shader->SetUniformFloat("u_CloudScale",       CloudScale);
        m_Shader->SetUniformFloat("u_CloudScrollSpeed", CloudScrollSpeed);
        m_Shader->SetUniformFloat("u_Time",             time);
        m_Shader->Flush();

        auto* dc = D3D11Context::Get()->GetDeviceContext();
        // No vertex buffer or input layout — shader uses SV_VertexID only.
        dc->IASetInputLayout(nullptr);
        dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dc->Draw(3, 0);
        // Restore default topology (other systems expect trianglelist anyway).
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_Shader->UnBind();
        m_PipelineState->Unbind();
    }
}
