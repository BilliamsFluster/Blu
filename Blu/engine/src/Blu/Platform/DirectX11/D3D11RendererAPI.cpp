#include "Blupch.h"
#include "D3D11RendererAPI.h"
#include "D3D11Context.h"
#include "Blu/Rendering/PipelineState.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    void D3D11RendererAPI::Init()
    {
        auto* ctx = D3D11Context::Get();
        auto* dev = ctx->GetDevice();
        auto* dc  = ctx->GetDeviceContext();

        // Rasterizer state is created by D3D11Context::Init() for wireframe toggle.
        // Use the PipelineStateCache for blend + depth/stencil defaults.
        auto defaultState = PipelineStateCache::GetOpaque();
        defaultState->Bind();
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

        ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* dsv = nullptr;
        dc->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);

        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        {
            if (rtvs[i])
            {
                dc->ClearRenderTargetView(rtvs[i], rgba);
                rtvs[i]->Release();
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
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
