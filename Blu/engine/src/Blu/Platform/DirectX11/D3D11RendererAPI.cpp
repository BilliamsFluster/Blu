#include "Blupch.h"
#include "D3D11RendererAPI.h"
#include "D3D11Context.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    void D3D11RendererAPI::Init()
    {
        auto* ctx = D3D11Context::Get();
        auto* dev = ctx->GetDevice();
        auto* dc  = ctx->GetDeviceContext();

        // Alpha blending
        D3D11_BLEND_DESC bd                           = {};
        bd.RenderTarget[0].BlendEnable               = TRUE;
        bd.RenderTarget[0].SrcBlend                  = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend                 = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp                   = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha             = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha            = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha              = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask     = D3D11_COLOR_WRITE_ENABLE_ALL;
        Microsoft::WRL::ComPtr<ID3D11BlendState> bs;
        dev->CreateBlendState(&bd, bs.GetAddressOf());
        dc->OMSetBlendState(bs.Get(), nullptr, 0xFFFFFFFF);

        // Depth test, write enabled, less comparison
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable              = TRUE;
        dsd.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc                = D3D11_COMPARISON_LESS;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dss;
        dev->CreateDepthStencilState(&dsd, dss.GetAddressOf());
        dc->OMSetDepthStencilState(dss.Get(), 1);

        // Solid rasteriser, back-face culling
        D3D11_RASTERIZER_DESC rsd = {};
        rsd.FillMode              = D3D11_FILL_SOLID;
        rsd.CullMode              = D3D11_CULL_BACK;
        rsd.FrontCounterClockwise = FALSE;
        rsd.DepthClipEnable       = TRUE;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
        dev->CreateRasterizerState(&rsd, rs.GetAddressOf());
        dc->RSSetState(rs.Get());
    }

    void D3D11RendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    {
        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = (float)x;
        vp.TopLeftY = (float)y;
        vp.Width    = (float)w;
        vp.Height   = (float)h;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        D3D11Context::Get()->GetDeviceContext()->RSSetViewports(1, &vp);
    }

    void D3D11RendererAPI::SetClearColor(const glm::vec4& color)
    {
        D3D11Context::Get()->SetClearColorValue(color);
    }

    void D3D11RendererAPI::Clear()
    {
        auto* ctx = D3D11Context::Get();
        auto* dc  = ctx->GetDeviceContext();
        const glm::vec4& c = ctx->GetClearColorValue();
        float rgba[4] = { c.r, c.g, c.b, c.a };

        // Query whatever RTVs/DSV are currently bound (works whether we're
        // rendering to a framebuffer texture or the swap-chain backbuffer).
        ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* dsv = nullptr;
        dc->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);

        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        {
            if (rtvs[i])
            {
                dc->ClearRenderTargetView(rtvs[i], rgba);
                rtvs[i]->Release();   // OMGetRenderTargets adds a ref
            }
        }
        if (dsv)
        {
            dc->ClearDepthStencilView(dsv,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            dsv->Release();
        }
    }

    void D3D11RendererAPI::DrawIndexed(const Shared<VertexArray>& va, uint32_t indexCount)
    {
        va->Bind();
        uint32_t count = indexCount ? indexCount : va->GetIndexBuffer()->GetCount();
        D3D11Context::Get()->GetDeviceContext()->DrawIndexed(count, 0, 0);
    }

    void D3D11RendererAPI::DrawLines(const Shared<VertexArray>& va, uint32_t vertexCount)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        va->Bind();
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        dc->Draw(vertexCount, 0);
        // Restore triangle topology for subsequent draws
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
