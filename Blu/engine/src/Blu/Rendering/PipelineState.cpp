#include "Blupch.h"
#include "PipelineState.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/DirectX11/D3D11PipelineState.h"

namespace Blu
{
    std::unordered_map<size_t, Shared<PipelineState>> PipelineStateCache::s_Cache;

    Shared<PipelineState> PipelineState::Create(const PipelineStateDesc& desc)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::Direct3D: return std::make_shared<D3D11PipelineState>(desc);
        case RendererAPI::API::OpenGL:   return nullptr;
        default:                         return nullptr;
        }
    }

    Shared<PipelineState> PipelineStateCache::GetOrCreate(const PipelineStateDesc& desc)
    {
        size_t hash = 0;
        hash ^= std::hash<uint32_t>{}(*(const uint32_t*)&desc.BlendState);
        hash ^= std::hash<uint32_t>{}(*(const uint32_t*)&desc.DepthStencilState);
        hash ^= std::hash<uint32_t>{}(*(const uint32_t*)&desc.RasterizerState);
        hash ^= std::hash<uint32_t>{}(*(const uint32_t*)&desc.SamplerState);

        auto it = s_Cache.find(hash);
        if (it != s_Cache.end())
            return it->second;

        auto state = PipelineState::Create(desc);
        s_Cache[hash] = state;
        return state;
    }

    static PipelineStateDesc MakeOpaque()
    {
        PipelineStateDesc desc;
        desc.BlendState.RenderTarget[0].BlendEnable = false;
        desc.DepthStencilState.DepthEnable = true;
        desc.DepthStencilState.DepthWriteMask = true;
        desc.DepthStencilState.DepthFunc = ComparisonFunc::Less;
        desc.RasterizerState.FillMode = FillMode::Solid;
        desc.RasterizerState.CullMode = CullMode::Back;
        desc.RasterizerState.FrontCounterClockwise = true;
        return desc;
    }

    static PipelineStateDesc MakeAlphaBlend()
    {
        PipelineStateDesc desc = MakeOpaque();
        desc.BlendState.RenderTarget[0].BlendEnable = true;
        desc.BlendState.RenderTarget[0].SrcBlend = BlendFactor::SrcAlpha;
        desc.BlendState.RenderTarget[0].DstBlend = BlendFactor::InvSrcAlpha;
        desc.BlendState.RenderTarget[0].BlendOperation = BlendOp::Add;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = BlendFactor::One;
        desc.BlendState.RenderTarget[0].DstBlendAlpha = BlendFactor::Zero;
        desc.BlendState.RenderTarget[0].BlendOperationAlpha = BlendOp::Add;
        return desc;
    }

    static PipelineStateDesc MakeWireframe()
    {
        PipelineStateDesc desc = MakeOpaque();
        desc.RasterizerState.FillMode = FillMode::Wireframe;
        desc.RasterizerState.CullMode = CullMode::None;
        return desc;
    }

    static PipelineStateDesc MakeNoDepth()
    {
        PipelineStateDesc desc = MakeOpaque();
        desc.DepthStencilState.DepthEnable = false;
        desc.DepthStencilState.DepthWriteMask = false;
        return desc;
    }

    static PipelineStateDesc MakeShadowMap()
    {
        PipelineStateDesc desc;
        desc.BlendState.RenderTarget[0].BlendEnable = false;
        desc.DepthStencilState.DepthEnable = true;
        desc.DepthStencilState.DepthWriteMask = true;
        desc.DepthStencilState.DepthFunc = ComparisonFunc::Less;
        desc.RasterizerState.FillMode = FillMode::Solid;
        desc.RasterizerState.CullMode = CullMode::Back;
        desc.RasterizerState.FrontCounterClockwise = true;
        desc.RasterizerState.DepthBias = 1000;
        desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
        return desc;
    }

    static PipelineStateDesc MakeAdditiveBlend()
    {
        PipelineStateDesc desc = MakeOpaque();
        desc.BlendState.RenderTarget[0].BlendEnable = true;
        desc.BlendState.RenderTarget[0].SrcBlend = BlendFactor::One;
        desc.BlendState.RenderTarget[0].DstBlend = BlendFactor::One;
        desc.BlendState.RenderTarget[0].BlendOperation = BlendOp::Add;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = BlendFactor::One;
        desc.BlendState.RenderTarget[0].DstBlendAlpha = BlendFactor::One;
        desc.BlendState.RenderTarget[0].BlendOperationAlpha = BlendOp::Add;
        return desc;
    }

    PipelineStateDesc DefaultPipelineStates::Opaque()       { return MakeOpaque(); }
    PipelineStateDesc DefaultPipelineStates::AlphaBlend()   { return MakeAlphaBlend(); }
    PipelineStateDesc DefaultPipelineStates::Wireframe()    { return MakeWireframe(); }
    PipelineStateDesc DefaultPipelineStates::NoDepth()      { return MakeNoDepth(); }
    PipelineStateDesc DefaultPipelineStates::ShadowMap()    { return MakeShadowMap(); }
    PipelineStateDesc DefaultPipelineStates::AdditiveBlend(){ return MakeAdditiveBlend(); }

    Shared<PipelineState> PipelineStateCache::GetOpaque()       { return GetOrCreate(MakeOpaque()); }
    Shared<PipelineState> PipelineStateCache::GetAlphaBlend()   { return GetOrCreate(MakeAlphaBlend()); }
    Shared<PipelineState> PipelineStateCache::GetWireframe()    { return GetOrCreate(MakeWireframe()); }
    Shared<PipelineState> PipelineStateCache::GetNoDepth()      { return GetOrCreate(MakeNoDepth()); }
    Shared<PipelineState> PipelineStateCache::GetShadowMap()    { return GetOrCreate(MakeShadowMap()); }
    Shared<PipelineState> PipelineStateCache::GetAdditiveBlend(){ return GetOrCreate(MakeAdditiveBlend()); }

    void PipelineStateCache::Clear() { s_Cache.clear(); }
}
