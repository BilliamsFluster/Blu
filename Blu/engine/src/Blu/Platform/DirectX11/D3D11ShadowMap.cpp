#include "Blupch.h"
#include "D3D11ShadowMap.h"
#include "D3D11Context.h"

namespace Blu
{
    D3D11ShadowMap::D3D11ShadowMap(uint32_t size)
        : m_Size(size)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

        D3D11_TEXTURE2D_DESC td = {};
        td.Width          = size;
        td.Height         = size;
        td.MipLevels      = 1;
        td.ArraySize      = 1;
        td.Format         = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage          = D3D11_USAGE_DEFAULT;
        td.BindFlags      = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        dev->CreateTexture2D(&td, nullptr, m_DepthTexture.GetAddressOf());

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dev->CreateDepthStencilView(m_DepthTexture.Get(), &dsvDesc, m_DSV.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format              = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        dev->CreateShaderResourceView(m_DepthTexture.Get(), &srvDesc, m_SRV.GetAddressOf());
    }

    void D3D11ShadowMap::BindForWriting()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        UINT numViewports = 1;
        dc->RSGetViewports(&numViewports, &m_PrevViewport);

        ID3D11RenderTargetView* nullRTV = nullptr;
        dc->OMSetRenderTargets(0, nullptr, m_DSV.Get());
        dc->ClearDepthStencilView(m_DSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)m_Size;
        vp.Height   = (float)m_Size;
        vp.MaxDepth = 1.0f;
        dc->RSSetViewports(1, &vp);
    }

    void D3D11ShadowMap::UnbindForWriting()
    {
        auto* ctx = D3D11Context::Get();
        auto* dc  = ctx->GetDeviceContext();

        ID3D11RenderTargetView* rtv = ctx->GetBackbufferRTV();
        dc->OMSetRenderTargets(1, &rtv, ctx->GetDepthStencilView());

        if (m_PrevViewport.Width > 0 && m_PrevViewport.Height > 0)
            dc->RSSetViewports(1, &m_PrevViewport);
    }

    void D3D11ShadowMap::BindTexture(uint32_t slot)
    {
        auto* dc  = D3D11Context::Get()->GetDeviceContext();
        dc->PSSetShaderResources(slot, 1, m_SRV.GetAddressOf());

        auto* shadowSampler = D3D11Context::Get()->GetShadowSampler();
        dc->PSSetSamplers(slot, 1, &shadowSampler);
    }
}

namespace Blu
{
    Shared<ShadowMap> ShadowMap::Create(uint32_t size)
    {
        return std::make_shared<D3D11ShadowMap>(size);
    }
}
