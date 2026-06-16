#pragma once
#include "Blu/Rendering/PostProcess.h"
#include "Blu/Rendering/VertexArray.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <glm/glm.hpp>

namespace Blu
{
    class D3D11PostProcess : public PostProcess
    {
    public:
        D3D11PostProcess(uint32_t width, uint32_t height);
        ~D3D11PostProcess() override = default;

        void Begin()                              override;
        void Submit(Shared<Shader> shader)        override;
        void Resize(uint32_t width, uint32_t height) override;

        Shared<FrameBuffer> GetFrameBuffer() override { return m_SceneFB; }

    private:
        void CreateBloomFBs(uint32_t w, uint32_t h);
        void CreateFullscreenQuad();
        void CreateFallbackTextures();

        // ── Frame buffers ─────────────────────────────────────────────────────────
        Shared<FrameBuffer> m_SceneFB;              // HDR scene capture (RGBA16F, full res)

        // Bloom mip chain  [0]=1/2, [1]=1/4, [2]=1/8 resolution  (RGBA16F each)
        static constexpr int kBloomMips = 3;
        std::array<Shared<FrameBuffer>, kBloomMips> m_BloomDownFBs;
        std::array<Shared<FrameBuffer>, kBloomMips> m_BloomUpFBs;

        // ── Shaders ───────────────────────────────────────────────────────────────
        Shared<Shader> m_DefaultShader;        // PostProcess_Blit (fallback)
        Shared<Shader> m_BloomExtractShader;
        Shared<Shader> m_BloomDownShader;
        Shared<Shader> m_BloomUpShader;
        Shared<Shader> m_CompositeShader;      // ACES tonemap + bloom + FXAA + SSAO
        Shared<Shader> m_SSAOShader;
        Shared<Shader> m_SSAOBlurShader;
        Shared<Shader> m_FogVolumeShader;      // localized fog composite
        Shared<Shader> m_DecalShader;          // projected decal composite

        // ── SSAO ─────────────────────────────────────────────────────────────────
        Shared<FrameBuffer> m_SSAOFB;
        Shared<FrameBuffer> m_SSAOBlurFB;

        // ── Fog volumes ───────────────────────────────────────────────────────────
        Shared<FrameBuffer> m_FogVolumeFB;     // RGBA16F, holds scene color with fog applied
        Shared<FrameBuffer> m_DecalFB;         // RGBA16F, holds scene color with decals applied
        glm::vec4           m_SSAOKernel[32];  // tangent-space hemisphere samples

        // ── Geometry ─────────────────────────────────────────────────────────────
        Shared<VertexArray> m_FullscreenQuadVAO;
        uint32_t            m_IndexCount = 0;

        // ── Samplers ─────────────────────────────────────────────────────────────
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_PointSampler;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_LinearSampler;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_BlackFallbackSRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_WhiteFallbackSRV;

        // RTVs saved when Begin() was called:
        //   slot 0 = editor color attachment — composite target for Submit()
        //   slot 1 = editor entity-ID attachment — kept bound so picking still works
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_SavedOutputRTV;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_SavedEntityIDRTV;
    };
}
