#include "Blupch.h"
#include "D3D11PostProcess.h"
#include "D3D11Context.h"
#include "D3D11FrameBuffer.h"
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/Shader.h"
#include "Blu/Rendering/VertexArray.h"
#include "Blu/Rendering/Buffer.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Rendering/RenderCommand.h"
#include "Blu/Rendering/PipelineState.h"
#include "Blu/Core/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

namespace Blu
{
    static Shared<FrameBuffer> MakeColorFB(uint32_t w, uint32_t h,
                                           FrameBufferTextureFormat fmt = FrameBufferTextureFormat::RGBA16F,
                                           bool withDepth = false)
    {
        FrameBufferSpecifications specs;
        specs.Width  = std::max(w, 1u);
        specs.Height = std::max(h, 1u);
        if (withDepth)
            specs.Attachments = { fmt, FrameBufferTextureFormat::DEPTH24STENCIL8 };
        else
            specs.Attachments = { fmt };
        return FrameBuffer::Create(specs);
    }

    D3D11PostProcess::D3D11PostProcess(uint32_t width, uint32_t height)
    {
        // HDR scene capture — needs depth so 3D rendering sorts correctly
        m_SceneFB = MakeColorFB(width, height, FrameBufferTextureFormat::RGBA16F, true);

        // Bloom cascade framebuffers
        CreateBloomFBs(width, height);

        // SSAO framebuffers (RGBA8: only R channel used; A kept for alignment)
        m_SSAOFB     = MakeColorFB(width, height, FrameBufferTextureFormat::RGBA8);
        m_SSAOBlurFB = MakeColorFB(width, height, FrameBufferTextureFormat::RGBA8);

        // Fog-volume composite target (HDR, full res) — holds scene color after fog blending
        m_FogVolumeFB = MakeColorFB(width, height, FrameBufferTextureFormat::RGBA16F);

        // Load shaders
        auto& lib = *Renderer::GetShaderLibrary();
        lib.Load("assets/shaders/DX11/PostProcess_Blit.hlsl");
        lib.Load("assets/shaders/DX11/PostProcess_BloomExtract.hlsl");
        lib.Load("assets/shaders/DX11/PostProcess_BloomDown.hlsl");
        lib.Load("assets/shaders/DX11/PostProcess_BloomUp.hlsl");
        lib.Load("assets/shaders/DX11/PostProcess_Composite.hlsl");
        lib.Load("assets/shaders/DX11/SSAO.hlsl");
        lib.Load("assets/shaders/DX11/SSAOBlur.hlsl");
        lib.Load("assets/shaders/DX11/PostProcess_FogVolume.hlsl");

        m_DefaultShader      = lib.Get("PostProcess_Blit");
        m_BloomExtractShader = lib.Get("PostProcess_BloomExtract");
        m_BloomDownShader    = lib.Get("PostProcess_BloomDown");
        m_BloomUpShader      = lib.Get("PostProcess_BloomUp");
        m_CompositeShader    = lib.Get("PostProcess_Composite");
        m_SSAOShader         = lib.Get("SSAO");
        m_SSAOBlurShader     = lib.Get("SSAOBlur");
        m_FogVolumeShader    = lib.Get("PostProcess_FogVolume");

        // Generate hemisphere kernel in tangent space (z always positive = toward normal)
        {
            std::uniform_real_distribution<float> rand01(0.0f, 1.0f);
            std::default_random_engine engine(42u);
            for (int i = 0; i < 32; ++i)
            {
                glm::vec3 s = glm::normalize(glm::vec3(
                    rand01(engine) * 2.0f - 1.0f,
                    rand01(engine) * 2.0f - 1.0f,
                    rand01(engine)));
                s *= rand01(engine);
                // Accelerate distribution toward origin so more samples are closer
                float scale = float(i) / 32.0f;
                scale = glm::mix(0.1f, 1.0f, scale * scale);
                m_SSAOKernel[i] = glm::vec4(s * scale, 0.0f);
            }
        }

        // Create shared samplers used by SSAO/blur passes
        auto* dev = D3D11Context::Get()->GetDevice();
        {
            D3D11_SAMPLER_DESC sd = {};
            sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
            sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            dev->CreateSamplerState(&sd, m_PointSampler.GetAddressOf());

            sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            dev->CreateSamplerState(&sd, m_LinearSampler.GetAddressOf());
        }

        CreateFallbackTextures();
        CreateFullscreenQuad();
    }

    void D3D11PostProcess::CreateFallbackTextures()
    {
        auto* dev = D3D11Context::Get()->GetDevice();
        auto createSolid = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                               Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV)
        {
            uint8_t pixel[4] = { r, g, b, a };

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = 1;
            desc.Height = 1;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA data = {};
            data.pSysMem = pixel;
            data.SysMemPitch = sizeof(pixel);

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            HRESULT hr = dev->CreateTexture2D(&desc, &data, texture.GetAddressOf());
            if (FAILED(hr))
            {
                BLU_CORE_ERROR("D3D11PostProcess: failed to create fallback texture");
                return;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            hr = dev->CreateShaderResourceView(texture.Get(), &srvDesc, outSRV.GetAddressOf());
            if (FAILED(hr))
                BLU_CORE_ERROR("D3D11PostProcess: failed to create fallback SRV");
        };

        createSolid(0, 0, 0, 255, m_BlackFallbackSRV);
        createSolid(255, 255, 255, 255, m_WhiteFallbackSRV);
    }

    void D3D11PostProcess::CreateBloomFBs(uint32_t w, uint32_t h)
    {
        for (int i = 0; i < kBloomMips; ++i)
        {
            uint32_t mw = std::max(w >> (i + 1), 1u);
            uint32_t mh = std::max(h >> (i + 1), 1u);
            m_BloomDownFBs[i] = MakeColorFB(mw, mh);
            m_BloomUpFBs[i]   = MakeColorFB(mw, mh);
        }
    }

    void D3D11PostProcess::CreateFullscreenQuad()
    {
        // DX11: NDC y=+1 is viewport top (texture row 0, UV.y=0).
        // Pair each NDC corner with the matching UV so the blit is not Y-flipped.
        float vertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,   // bottom-left  → UV bottom-left
             1.0f, -1.0f, 1.0f, 1.0f,   // bottom-right → UV bottom-right
             1.0f,  1.0f, 1.0f, 0.0f,   // top-right    → UV top-right
            -1.0f,  1.0f, 0.0f, 0.0f    // top-left     → UV top-left
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
        m_IndexCount = 6;

        m_FullscreenQuadVAO = VertexArray::Create();
        auto vb = VertexBuffer::Create(sizeof(vertices));
        vb->SetData(vertices, sizeof(vertices));
        BufferLayout layout = {
            { ShaderDataType::Float2, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoord" }
        };
        vb->SetLayout(layout);
        m_FullscreenQuadVAO->AddVertexBuffer(vb);
        auto ib = IndexBuffer::Create(indices, m_IndexCount);
        m_FullscreenQuadVAO->AddIndexBuffer(ib);
    }

    // ─────────────────────────────────────────────────────────────────────────────

    void D3D11PostProcess::Begin()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        // Save both RTVs from the caller:
        //   slot 0 = editor color output (composite target for Submit)
        //   slot 1 = editor entity-ID attachment (must stay live so picking shaders write into it)
        // OMGetRenderTargets AddRefs each pointer; Attach() transfers that ownership to the ComPtrs.
        m_SavedOutputRTV.Reset();
        m_SavedEntityIDRTV.Reset();
        ID3D11RenderTargetView* rawRTVs[2] = {};
        dc->OMGetRenderTargets(2, rawRTVs, nullptr);
        m_SavedOutputRTV.Attach(rawRTVs[0]);
        m_SavedEntityIDRTV.Attach(rawRTVs[1]);

        // Bind the HDR scene framebuffer so the scene renders into RGBA16F.
        m_SceneFB->Bind();

        // Re-attach the entity-ID RTV at slot 1 alongside the scene color RT, so
        // SV_Target1 writes from mesh/2D shaders land in the editor's picking buffer.
        if (m_SavedEntityIDRTV)
        {
            ID3D11RenderTargetView* rtv0;
            ID3D11DepthStencilView* dsv;
            dc->OMGetRenderTargets(1, &rtv0, &dsv);
            ID3D11RenderTargetView* rtvs[2] = { rtv0, m_SavedEntityIDRTV.Get() };
            dc->OMSetRenderTargets(2, rtvs, dsv);
            if (rtv0) rtv0->Release();
            if (dsv)  dsv->Release();
        }

        // Clear scene color + depth (entity-ID RT was already cleared by the editor layer).
        float black[4] = {};
        ID3D11RenderTargetView* rtv;
        ID3D11DepthStencilView* dsv;
        dc->OMGetRenderTargets(1, &rtv, &dsv);
        if (rtv) { dc->ClearRenderTargetView(rtv, black); rtv->Release(); }
        if (dsv) { dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0); dsv->Release(); }
    }

    void D3D11PostProcess::Submit(Shared<Shader> /*overrideShader*/)
    {
        m_SceneFB->UnBind();

        auto* dc      = D3D11Context::Get()->GetDeviceContext();
        auto* d3dScene = static_cast<D3D11FrameBuffer*>(m_SceneFB.get());
        ID3D11ShaderResourceView* sceneSRV = d3dScene->GetColorAttachmentSRV(0);

        const auto& spec = m_SceneFB->GetSpecification();
        float fw = static_cast<float>(spec.Width);
        float fh = static_cast<float>(spec.Height);

        bool useBloom = EnableBloom;
        bool useFXAA = EnableFXAA;
        bool useSSAO = EnableSSAO;
        switch (Preview)
        {
            case PreviewMode::Full:
                break;
            case PreviewMode::TonemapOnly:
                useBloom = false; useFXAA = false; useSSAO = false;
                break;
            case PreviewMode::BloomOnly:
                useBloom = true; useFXAA = false; useSSAO = false;
                break;
            case PreviewMode::FXAAOnly:
                useBloom = false; useFXAA = true; useSSAO = false;
                break;
            case PreviewMode::SSAOOnly:
                useBloom = false; useFXAA = false; useSSAO = true;
                break;
            case PreviewMode::Bypass:
            {
                auto* ctx = D3D11Context::Get();
                ID3D11RenderTargetView* rtv = m_SavedOutputRTV
                    ? m_SavedOutputRTV.Get()
                    : ctx->GetBackbufferRTV();
                dc->OMSetRenderTargets(1, &rtv, nullptr);
                D3D11_VIEWPORT vp = {};
                vp.Width = fw;
                vp.Height = fh;
                vp.MaxDepth = 1.0f;
                dc->RSSetViewports(1, &vp);
                dc->PSSetSamplers(0, 1, m_LinearSampler.GetAddressOf());
                m_DefaultShader->Bind();
                dc->PSSetShaderResources(0, 1, &sceneSRV);
                m_FullscreenQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                m_FullscreenQuadVAO->UnBind();
                m_DefaultShader->UnBind();
                ID3D11ShaderResourceView* nullSRV = nullptr;
                dc->PSSetShaderResources(0, 1, &nullSRV);
                m_SavedOutputRTV.Reset();
                return;
            }
        }

        if (useBloom)
        {
            // ── Bloom extract + first downsample ─────────────────────────────────
            {
                m_BloomDownFBs[0]->Bind();
                const auto& ds = m_BloomDownFBs[0]->GetSpecification();
                float clearV[4] = {};
                ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv; UINT n = 1;
                dc->OMGetRenderTargets(1, &rtv, &dsv);
                if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                if (dsv) dsv->Release();

                m_BloomExtractShader->Bind();
                m_BloomExtractShader->SetUniformFloat("u_Threshold", BloomThreshold);
                m_BloomExtractShader->SetUniformFloat("u_InvSrcW", 1.0f / fw);
                m_BloomExtractShader->SetUniformFloat("u_InvSrcH", 1.0f / fh);
                m_BloomExtractShader->Flush();
                dc->PSSetShaderResources(0, 1, &sceneSRV);
                m_FullscreenQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                m_FullscreenQuadVAO->UnBind();
                m_BloomExtractShader->UnBind();
                m_BloomDownFBs[0]->UnBind();
            }

            // ── Progressive downsample ────────────────────────────────────────────
            for (int i = 1; i < kBloomMips; ++i)
            {
                auto* srcFB  = static_cast<D3D11FrameBuffer*>(m_BloomDownFBs[i - 1].get());
                ID3D11ShaderResourceView* src = srcFB->GetColorAttachmentSRV(0);
                const auto& srcSpec = m_BloomDownFBs[i - 1]->GetSpecification();

                m_BloomDownFBs[i]->Bind();
                float clearV[4] = {};
                ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv; UINT n = 1;
                dc->OMGetRenderTargets(1, &rtv, &dsv);
                if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                if (dsv) dsv->Release();

                m_BloomDownShader->Bind();
                m_BloomDownShader->SetUniformFloat("u_InvSrcW", 1.0f / (float)srcSpec.Width);
                m_BloomDownShader->SetUniformFloat("u_InvSrcH", 1.0f / (float)srcSpec.Height);
                m_BloomDownShader->Flush();
                dc->PSSetShaderResources(0, 1, &src);
                m_FullscreenQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                m_FullscreenQuadVAO->UnBind();
                m_BloomDownShader->UnBind();
                m_BloomDownFBs[i]->UnBind();
            }

            // ── Progressive upsample (deepest → shallowest, accumulate) ──────────
            // Copy BloomDownFBs[kBloomMips-1] → BloomUpFBs[kBloomMips-1] first
            {
                int top = kBloomMips - 1;
                auto* src = static_cast<D3D11FrameBuffer*>(m_BloomDownFBs[top].get());
                const auto& srcSpec = m_BloomDownFBs[top]->GetSpecification();

                m_BloomUpFBs[top]->Bind();
                float clearV[4] = {};
                ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv; UINT n = 1;
                dc->OMGetRenderTargets(1, &rtv, &dsv);
                if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                if (dsv) dsv->Release();

                m_BloomUpShader->Bind();
                m_BloomUpShader->SetUniformFloat("u_InvSrcW", 1.0f / (float)srcSpec.Width);
                m_BloomUpShader->SetUniformFloat("u_InvSrcH", 1.0f / (float)srcSpec.Height);
                m_BloomUpShader->SetUniformFloat("u_FilterRadius", 0.5f);
                m_BloomUpShader->Flush();
                ID3D11ShaderResourceView* topSRV = src->GetColorAttachmentSRV(0);
                dc->PSSetShaderResources(0, 1, &topSRV);
                m_FullscreenQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                m_FullscreenQuadVAO->UnBind();
                m_BloomUpShader->UnBind();
                m_BloomUpFBs[top]->UnBind();
            }

            // Upsample remaining levels: src = upFB[i+1] + downFB[i] (additive blend)
            for (int i = kBloomMips - 2; i >= 0; --i)
            {
                auto* upSrc   = static_cast<D3D11FrameBuffer*>(m_BloomUpFBs[i + 1].get());
                auto* downSrc = static_cast<D3D11FrameBuffer*>(m_BloomDownFBs[i].get());
                const auto& upSpec = m_BloomUpFBs[i + 1]->GetSpecification();

                // First pass: copy downFB[i] into upFB[i]
                {
                    m_BloomUpFBs[i]->Bind();
                    float clearV[4] = {};
                    ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv; UINT n = 1;
                    dc->OMGetRenderTargets(1, &rtv, &dsv);
                    if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                    if (dsv) dsv->Release();

                    const auto& dSpec = m_BloomDownFBs[i]->GetSpecification();
                    m_BloomUpShader->Bind();
                    m_BloomUpShader->SetUniformFloat("u_InvSrcW", 1.0f / (float)dSpec.Width);
                    m_BloomUpShader->SetUniformFloat("u_InvSrcH", 1.0f / (float)dSpec.Height);
                    m_BloomUpShader->SetUniformFloat("u_FilterRadius", 0.5f);
                    m_BloomUpShader->Flush();
                    ID3D11ShaderResourceView* downSRV = downSrc->GetColorAttachmentSRV(0);
                    dc->PSSetShaderResources(0, 1, &downSRV);
                    m_FullscreenQuadVAO->Bind();
                    RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                    m_FullscreenQuadVAO->UnBind();
                    m_BloomUpShader->UnBind();
                    m_BloomUpFBs[i]->UnBind();
                }

                // Second pass (additive): blend upFB[i+1] upsampled into upFB[i]
                {
                    m_BloomUpFBs[i]->Bind();
                    PipelineStateCache::GetAdditiveBlend()->Bind();

                    m_BloomUpShader->Bind();
                    m_BloomUpShader->SetUniformFloat("u_InvSrcW", 1.0f / (float)upSpec.Width);
                    m_BloomUpShader->SetUniformFloat("u_InvSrcH", 1.0f / (float)upSpec.Height);
                    m_BloomUpShader->SetUniformFloat("u_FilterRadius", 0.5f);
                    m_BloomUpShader->Flush();
                    ID3D11ShaderResourceView* upSRV = upSrc->GetColorAttachmentSRV(0);
                    dc->PSSetShaderResources(0, 1, &upSRV);
                    m_FullscreenQuadVAO->Bind();
                    RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                    m_FullscreenQuadVAO->UnBind();
                    m_BloomUpShader->UnBind();

                    PipelineStateCache::GetOpaque()->Bind();
                    m_BloomUpFBs[i]->UnBind();
                }
            }
        }

        // ── SSAO pass ────────────────────────────────────────────────────────────
        ID3D11ShaderResourceView* aoSRV = nullptr;
        if (useSSAO && m_SSAOShader && m_SSAOBlurShader)
        {
            auto* d3dScene  = static_cast<D3D11FrameBuffer*>(m_SceneFB.get());
            ID3D11ShaderResourceView* depthSRV = d3dScene->GetDepthSRV();

            if (depthSRV)
            {
                // Build the SSAO cbuffer on the CPU and upload via SetUniformBuffer
                struct alignas(16) SSAOParamsCB
                {
                    glm::mat4 Projection;
                    glm::mat4 InvProjection;
                    float     Radius, Bias, Power, TexelW, TexelH;
                    int       NumSamples;
                    float     _pad[2];
                    glm::vec4 Kernel[32];
                };
                static_assert(sizeof(SSAOParamsCB) == 672, "SSAOParamsCB layout mismatch");

                SSAOParamsCB cb = {};
                cb.Projection    = SSAOProjection;
                cb.InvProjection = SSAOInvProjection;
                cb.Radius        = SSAORadius;
                cb.Bias          = SSAOBias;
                cb.Power         = SSAOPower;
                cb.TexelW        = 1.0f / fw;
                cb.TexelH        = 1.0f / fh;
                cb.NumSamples    = SSAOSamples;
                for (int i = 0; i < 32; ++i) cb.Kernel[i] = m_SSAOKernel[i];

                // SSAO raw pass
                {
                    m_SSAOFB->Bind();
                    float clearV[4] = { 1, 1, 1, 1 };
                    ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv;
                    dc->OMGetRenderTargets(1, &rtv, &dsv);
                    if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                    if (dsv)   dsv->Release();

                    dc->PSSetSamplers(0, 1, m_PointSampler.GetAddressOf());
                    m_SSAOShader->Bind();
                    m_SSAOShader->SetUniformBuffer("SSAOParams", &cb, sizeof(cb));
                    m_SSAOShader->Flush();
                    dc->PSSetShaderResources(0, 1, &depthSRV);
                    m_FullscreenQuadVAO->Bind();
                    RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                    m_FullscreenQuadVAO->UnBind();
                    m_SSAOShader->UnBind();
                    m_SSAOFB->UnBind();
                }

                // Unbind depth SRV before next pass
                ID3D11ShaderResourceView* nullSRV = nullptr;
                dc->PSSetShaderResources(0, 1, &nullSRV);

                // SSAO blur pass
                {
                    auto* ssaoD3D = static_cast<D3D11FrameBuffer*>(m_SSAOFB.get());
                    ID3D11ShaderResourceView* rawAOSRV = ssaoD3D->GetColorAttachmentSRV(0);

                    m_SSAOBlurFB->Bind();
                    float clearV[4] = { 1, 1, 1, 1 };
                    ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv;
                    dc->OMGetRenderTargets(1, &rtv, &dsv);
                    if (rtv) { dc->ClearRenderTargetView(rtv, clearV); rtv->Release(); }
                    if (dsv)   dsv->Release();

                    dc->PSSetSamplers(0, 1, m_PointSampler.GetAddressOf());
                    m_SSAOBlurShader->Bind();
                    m_SSAOBlurShader->SetUniformFloat("u_InvW", 1.0f / fw);
                    m_SSAOBlurShader->SetUniformFloat("u_InvH", 1.0f / fh);
                    m_SSAOBlurShader->Flush();
                    dc->PSSetShaderResources(0, 1, &rawAOSRV);
                    m_FullscreenQuadVAO->Bind();
                    RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                    m_FullscreenQuadVAO->UnBind();
                    m_SSAOBlurShader->UnBind();
                    m_SSAOBlurFB->UnBind();

                    dc->PSSetShaderResources(0, 1, &nullSRV);
                }

                auto* blurD3D = static_cast<D3D11FrameBuffer*>(m_SSAOBlurFB.get());
                aoSRV = blurD3D->GetColorAttachmentSRV(0);
            }
        }

        // ── Localized fog volumes: integrate density along the view ray, write fogged scene ─
        // Runs only when the scene supplied volumes — otherwise sceneSRV is untouched and the
        // composite is byte-identical to a scene with no fog volumes.
        if (EnableFogVolumes && m_FogVolumeShader && !FogVolumes.empty())
        {
            auto* d3dScene = static_cast<D3D11FrameBuffer*>(m_SceneFB.get());
            ID3D11ShaderResourceView* depthSRV = d3dScene->GetDepthSRV();
            if (depthSRV && sceneSRV)
            {
                struct alignas(16) FogVolumeCB
                {
                    glm::vec3 Position; float Shape;
                    glm::vec3 Extents;  float Density;
                    glm::vec3 Color;    float Falloff;
                };
                struct alignas(16) FogParamsCB
                {
                    glm::mat4   InvViewProj;
                    glm::vec3   CameraPos; float VolumeCount;
                    FogVolumeCB Volumes[16];
                };
                static_assert(sizeof(FogParamsCB) == 848, "FogParamsCB layout must match PostProcess_FogVolume.hlsl");

                FogParamsCB cb = {};
                cb.InvViewProj = FogInvViewProj;
                cb.CameraPos   = FogCameraPos;
                const int n    = (int)std::min<size_t>(FogVolumes.size(), 16);
                cb.VolumeCount = (float)n;
                for (int i = 0; i < n; ++i)
                {
                    cb.Volumes[i].Position = FogVolumes[i].Position;
                    cb.Volumes[i].Shape    = (float)FogVolumes[i].Shape;
                    cb.Volumes[i].Extents  = FogVolumes[i].Extents;
                    cb.Volumes[i].Density  = FogVolumes[i].Density;
                    cb.Volumes[i].Color    = FogVolumes[i].Color;
                    cb.Volumes[i].Falloff  = FogVolumes[i].Falloff;
                }

                m_FogVolumeFB->Bind();
                D3D11_VIEWPORT vp = {}; vp.Width = fw; vp.Height = fh; vp.MaxDepth = 1.0f;
                dc->RSSetViewports(1, &vp);
                dc->PSSetSamplers(0, 1, m_PointSampler.GetAddressOf());
                m_FogVolumeShader->Bind();
                m_FogVolumeShader->SetUniformBuffer("FogParams", &cb, sizeof(cb));
                m_FogVolumeShader->Flush();
                ID3D11ShaderResourceView* fogSrcs[2] = { sceneSRV, depthSRV };
                dc->PSSetShaderResources(0, 2, fogSrcs);
                m_FullscreenQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
                m_FullscreenQuadVAO->UnBind();
                m_FogVolumeShader->UnBind();
                m_FogVolumeFB->UnBind();

                ID3D11ShaderResourceView* nullSRV2[2] = {};
                dc->PSSetShaderResources(0, 2, nullSRV2);

                // Composite now samples the fogged scene instead of the raw HDR scene.
                auto* fogD3D = static_cast<D3D11FrameBuffer*>(m_FogVolumeFB.get());
                sceneSRV = fogD3D->GetColorAttachmentSRV(0);
            }
        }

        // ── Final composite: ACES + bloom + SSAO + FXAA → caller's framebuffer ─
        {
            auto* ctx = D3D11Context::Get();
            ID3D11RenderTargetView* rtv = m_SavedOutputRTV
                ? m_SavedOutputRTV.Get()
                : ctx->GetBackbufferRTV();
            dc->OMSetRenderTargets(1, &rtv, nullptr);

            // Bloom chain may have changed the viewport — reset it to scene FB dimensions.
            D3D11_VIEWPORT vp = {};
            vp.Width    = fw;
            vp.Height   = fh;
            vp.MaxDepth = 1.0f;
            dc->RSSetViewports(1, &vp);

            dc->PSSetSamplers(0, 1, m_LinearSampler.GetAddressOf());

            auto* bloomFB = static_cast<D3D11FrameBuffer*>(m_BloomUpFBs[0].get());
            ID3D11ShaderResourceView* bloomSRV = (useBloom && bloomFB)
                ? bloomFB->GetColorAttachmentSRV(0)
                : m_BlackFallbackSRV.Get();
            if (!bloomSRV)
                bloomSRV = m_BlackFallbackSRV.Get();

            ID3D11ShaderResourceView* finalAOSRV = (useSSAO && aoSRV) ? aoSRV : m_WhiteFallbackSRV.Get();
            if (!sceneSRV)
                BLU_CORE_ERROR("D3D11PostProcess: scene color SRV is null before composite");
            if (!bloomSRV)
                BLU_CORE_ERROR("D3D11PostProcess: bloom fallback SRV is null before composite");
            if (!finalAOSRV)
                BLU_CORE_ERROR("D3D11PostProcess: AO fallback SRV is null before composite");

            m_CompositeShader->Bind();
            m_CompositeShader->SetUniformFloat("u_EnableBloom",   useBloom ? 1.0f : 0.0f);
            m_CompositeShader->SetUniformFloat("u_BloomStrength", useBloom ? BloomStrength : 0.0f);
            m_CompositeShader->SetUniformFloat("u_EnableFXAA",    useFXAA ? 1.0f : 0.0f);
            m_CompositeShader->SetUniformFloat("u_InvW",          1.0f / fw);
            m_CompositeShader->SetUniformFloat("u_InvH",          1.0f / fh);
            m_CompositeShader->SetUniformFloat("u_SSAOStrength",  (useSSAO && aoSRV) ? SSAOStrength : 0.0f);
            // God rays — radial light shafts from the sun's screen position (Scene-supplied).
            const bool useGodRays = EnableGodRays && GodRaySunVisible;
            m_CompositeShader->SetUniformFloat("u_GodRayIntensity", useGodRays ? GodRayIntensity : 0.0f);
            m_CompositeShader->SetUniformFloat("u_GodRayVisible",   useGodRays ? 1.0f : 0.0f);
            m_CompositeShader->SetUniformFloat("u_SunU",            GodRaySunUV.x);
            m_CompositeShader->SetUniformFloat("u_SunV",            GodRaySunUV.y);
            m_CompositeShader->Flush();
            dc->PSSetShaderResources(0, 1, &sceneSRV);
            dc->PSSetShaderResources(1, 1, &bloomSRV);
            dc->PSSetShaderResources(2, 1, &finalAOSRV);
            m_FullscreenQuadVAO->Bind();
            RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_IndexCount);
            m_FullscreenQuadVAO->UnBind();
            m_CompositeShader->UnBind();

            ID3D11ShaderResourceView* nullSRVs[3] = {};
            dc->PSSetShaderResources(0, 3, nullSRVs);
        }

        m_SavedOutputRTV.Reset();
    }

    void D3D11PostProcess::Resize(uint32_t width, uint32_t height)
    {
        m_SceneFB->Resize(width, height);

        for (int i = 0; i < kBloomMips; ++i)
        {
            uint32_t mw = std::max(width  >> (i + 1), 1u);
            uint32_t mh = std::max(height >> (i + 1), 1u);
            m_BloomDownFBs[i]->Resize(mw, mh);
            m_BloomUpFBs[i]->Resize(mw, mh);
        }

        m_SSAOFB->Resize(width, height);
        m_SSAOBlurFB->Resize(width, height);
        m_FogVolumeFB->Resize(width, height);
    }
}

namespace Blu
{
    Shared<PostProcess> PostProcess::Create(uint32_t width, uint32_t height)
    {
        return std::make_shared<D3D11PostProcess>(width, height);
    }
}
