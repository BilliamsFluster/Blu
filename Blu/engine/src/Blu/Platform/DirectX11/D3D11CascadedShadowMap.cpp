#include "Blupch.h"
#include "D3D11CascadedShadowMap.h"
#include "D3D11Context.h"
#include "Blu/Rendering/CascadedShadowMap.h"

namespace Blu
{
    D3D11CascadedShadowMap::D3D11CascadedShadowMap(uint32_t size)
        : m_Size(size)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

        D3D11_TEXTURE2D_DESC td = {};
        td.Width              = size;
        td.Height             = size;
        td.MipLevels          = 1;
        td.ArraySize          = NUM_CASCADES;
        td.Format             = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc.Count   = 1;
        td.Usage              = D3D11_USAGE_DEFAULT;
        td.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        dev->CreateTexture2D(&td, nullptr, m_DepthArray.GetAddressOf());

        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format                         = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice        = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(i);
            dsvDesc.Texture2DArray.ArraySize       = 1;
            dev->CreateDepthStencilView(m_DepthArray.Get(), &dsvDesc, m_DSVs[i].GetAddressOf());
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                            = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension                     = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip    = 0;
        srvDesc.Texture2DArray.MipLevels          = 1;
        srvDesc.Texture2DArray.FirstArraySlice    = 0;
        srvDesc.Texture2DArray.ArraySize          = NUM_CASCADES;
        dev->CreateShaderResourceView(m_DepthArray.Get(), &srvDesc, m_SRV.GetAddressOf());
    }

    void D3D11CascadedShadowMap::BindCascadeForWriting(int cascadeIndex)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        if (cascadeIndex == 0)
        {
            // Save both RTVs (colour + entity-ID at slot 1) so UnbindForWriting
            // can restore them.  Saving only slot 0 drops the entity-ID RTV and
            // breaks pixel-picking after the shadow pass.
            UINT n = 1;
            dc->RSGetViewports(&n, &m_PrevViewport);
            ID3D11RenderTargetView* rawRTVs[kMaxSavedRTVs] = {};
            dc->OMGetRenderTargets(kMaxSavedRTVs, rawRTVs, m_PrevDSV.GetAddressOf());
            for (UINT i = 0; i < kMaxSavedRTVs; ++i)
                m_PrevRTVs[i].Attach(rawRTVs[i]);
        }

        dc->OMSetRenderTargets(0, nullptr, m_DSVs[cascadeIndex].Get());
        dc->ClearDepthStencilView(m_DSVs[cascadeIndex].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        D3D11_VIEWPORT vp = {};
        vp.Width    = static_cast<float>(m_Size);
        vp.Height   = static_cast<float>(m_Size);
        vp.MaxDepth = 1.0f;
        dc->RSSetViewports(1, &vp);
    }

    void D3D11CascadedShadowMap::UnbindForWriting()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        // Restore both RTVs (colour at slot 0, entity-ID at slot 1) that were
        // active before the shadow pass started.
        ID3D11RenderTargetView* rawRTVs[kMaxSavedRTVs] = {};
        for (UINT i = 0; i < kMaxSavedRTVs; ++i)
            rawRTVs[i] = m_PrevRTVs[i].Get();
        dc->OMSetRenderTargets(kMaxSavedRTVs, rawRTVs, m_PrevDSV.Get());

        if (m_PrevViewport.Width > 0.0f && m_PrevViewport.Height > 0.0f)
            dc->RSSetViewports(1, &m_PrevViewport);

        for (auto& rtv : m_PrevRTVs) rtv.Reset();
        m_PrevDSV.Reset();
    }

    void D3D11CascadedShadowMap::BindTexture(uint32_t slot)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->PSSetShaderResources(slot, 1, m_SRV.GetAddressOf());

        // Shadow comparison sampler is always bound to s1 in the HLSL.
        auto* shadowSampler = D3D11Context::Get()->GetShadowSampler();
        dc->PSSetSamplers(1, 1, &shadowSampler);
    }

    // ── Factory ──────────────────────────────────────────────────────────────────

    Shared<CascadedShadowMap> CascadedShadowMap::Create(uint32_t size)
    {
        return std::make_shared<D3D11CascadedShadowMap>(size);
    }
}
