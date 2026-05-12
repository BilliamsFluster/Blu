#include "Blupch.h"
#include "D3D11PipelineState.h"
#include "D3D11Context.h"

namespace Blu
{
    static D3D11_BLEND ToD3D11(BlendFactor f)
    {
        switch (f)
        {
        case BlendFactor::Zero:            return D3D11_BLEND_ZERO;
        case BlendFactor::One:             return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor:        return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::InvSrcColor:     return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::SrcAlpha:        return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::InvSrcAlpha:     return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstColor:        return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::InvDstColor:     return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::DstAlpha:        return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::InvDstAlpha:     return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendFactor::SrcAlphaSat:     return D3D11_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::BlendFactor:     return D3D11_BLEND_BLEND_FACTOR;
        case BlendFactor::InvBlendFactor:  return D3D11_BLEND_INV_BLEND_FACTOR;
        default:                           return D3D11_BLEND_ONE;
        }
    }

    static D3D11_BLEND_OP ToD3D11(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Add:          return D3D11_BLEND_OP_ADD;
        case BlendOp::Subtract:     return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:          return D3D11_BLEND_OP_MIN;
        case BlendOp::Max:          return D3D11_BLEND_OP_MAX;
        default:                    return D3D11_BLEND_OP_ADD;
        }
    }

    static D3D11_COMPARISON_FUNC ToD3D11(ComparisonFunc f)
    {
        switch (f)
        {
        case ComparisonFunc::Never:         return D3D11_COMPARISON_NEVER;
        case ComparisonFunc::Less:          return D3D11_COMPARISON_LESS;
        case ComparisonFunc::Equal:         return D3D11_COMPARISON_EQUAL;
        case ComparisonFunc::LessEqual:     return D3D11_COMPARISON_LESS_EQUAL;
        case ComparisonFunc::Greater:       return D3D11_COMPARISON_GREATER;
        case ComparisonFunc::NotEqual:      return D3D11_COMPARISON_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual:  return D3D11_COMPARISON_GREATER_EQUAL;
        case ComparisonFunc::Always:        return D3D11_COMPARISON_ALWAYS;
        default:                            return D3D11_COMPARISON_LESS;
        }
    }

    static D3D11_STENCIL_OP ToD3D11(StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Keep:     return D3D11_STENCIL_OP_KEEP;
        case StencilOp::Zero:     return D3D11_STENCIL_OP_ZERO;
        case StencilOp::Replace:  return D3D11_STENCIL_OP_REPLACE;
        case StencilOp::IncrSat:  return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrSat:  return D3D11_STENCIL_OP_DECR_SAT;
        case StencilOp::Invert:   return D3D11_STENCIL_OP_INVERT;
        case StencilOp::Incr:     return D3D11_STENCIL_OP_INCR;
        case StencilOp::Decr:     return D3D11_STENCIL_OP_DECR;
        default:                  return D3D11_STENCIL_OP_KEEP;
        }
    }

    static D3D11_FILL_MODE ToD3D11(FillMode m)
    {
        switch (m)
        {
        case FillMode::Solid:     return D3D11_FILL_SOLID;
        case FillMode::Wireframe: return D3D11_FILL_WIREFRAME;
        default:                  return D3D11_FILL_SOLID;
        }
    }

    static D3D11_CULL_MODE ToD3D11(CullMode m)
    {
        switch (m)
        {
        case CullMode::None:  return D3D11_CULL_NONE;
        case CullMode::Front: return D3D11_CULL_FRONT;
        case CullMode::Back:  return D3D11_CULL_BACK;
        default:              return D3D11_CULL_BACK;
        }
    }

    static D3D11_FILTER ToD3D11(TextureFilter f)
    {
        switch (f)
        {
        case TextureFilter::Point:                        return D3D11_FILTER_MIN_MAG_MIP_POINT;
        case TextureFilter::Linear:                       return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        case TextureFilter::Anisotropic:                  return D3D11_FILTER_ANISOTROPIC;
        case TextureFilter::MinPointMagLinearMipPoint:    return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case TextureFilter::MinPointMagLinearMipLinear:   return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        case TextureFilter::MinLinearMagPointMipPoint:    return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        case TextureFilter::MinLinearMagPointMipLinear:   return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        default:                                           return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    static D3D11_TEXTURE_ADDRESS_MODE ToD3D11(TextureAddressMode m)
    {
        switch (m)
        {
        case TextureAddressMode::Wrap:       return D3D11_TEXTURE_ADDRESS_WRAP;
        case TextureAddressMode::Mirror:     return D3D11_TEXTURE_ADDRESS_MIRROR;
        case TextureAddressMode::Clamp:      return D3D11_TEXTURE_ADDRESS_CLAMP;
        case TextureAddressMode::Border:     return D3D11_TEXTURE_ADDRESS_BORDER;
        case TextureAddressMode::MirrorOnce: return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
        default:                             return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    D3D11PipelineState::D3D11PipelineState(const PipelineStateDesc& desc)
    {
        CreateBlendState(desc.BlendState);
        CreateDepthStencilState(desc.DepthStencilState);
        CreateRasterizerState(desc.RasterizerState);
        CreateSamplerState(desc.SamplerState);
    }

    void D3D11PipelineState::CreateBlendState(const BlendStateDesc& desc)
    {
        D3D11_BLEND_DESC bd = {};
        bd.AlphaToCoverageEnable  = desc.AlphaToCoverageEnable;
        bd.IndependentBlendEnable = desc.IndependentBlend;

        for (uint32_t i = 0; i < 8; ++i)
        {
            bd.RenderTarget[i].BlendEnable           = desc.RenderTarget[i].BlendEnable;
            bd.RenderTarget[i].SrcBlend               = ToD3D11(desc.RenderTarget[i].SrcBlend);
            bd.RenderTarget[i].DestBlend              = ToD3D11(desc.RenderTarget[i].DstBlend);
            bd.RenderTarget[i].BlendOp                = ToD3D11(desc.RenderTarget[i].BlendOperation);
            bd.RenderTarget[i].SrcBlendAlpha          = ToD3D11(desc.RenderTarget[i].SrcBlendAlpha);
            bd.RenderTarget[i].DestBlendAlpha         = ToD3D11(desc.RenderTarget[i].DstBlendAlpha);
            bd.RenderTarget[i].BlendOpAlpha           = ToD3D11(desc.RenderTarget[i].BlendOperationAlpha);
            bd.RenderTarget[i].RenderTargetWriteMask  = desc.RenderTarget[i].RenderTargetWriteMask;
        }

        auto* dev = D3D11Context::Get()->GetDevice();
        dev->CreateBlendState(&bd, m_BlendState.GetAddressOf());
    }

    void D3D11PipelineState::CreateDepthStencilState(const DepthStencilStateDesc& desc)
    {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable      = desc.DepthEnable;
        dsd.DepthWriteMask   = desc.DepthWriteMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc        = ToD3D11(desc.DepthFunc);
        dsd.StencilEnable    = desc.StencilEnable;
        dsd.StencilReadMask  = desc.StencilReadMask;
        dsd.StencilWriteMask = desc.StencilWriteMask;
        dsd.FrontFace.StencilFailOp      = ToD3D11(desc.FrontFace.FailOp);
        dsd.FrontFace.StencilDepthFailOp = ToD3D11(desc.FrontFace.DepthFailOp);
        dsd.FrontFace.StencilPassOp      = ToD3D11(desc.FrontFace.PassOp);
        dsd.FrontFace.StencilFunc        = ToD3D11(desc.FrontFace.Func);
        dsd.BackFace.StencilFailOp       = ToD3D11(desc.BackFace.FailOp);
        dsd.BackFace.StencilDepthFailOp  = ToD3D11(desc.BackFace.DepthFailOp);
        dsd.BackFace.StencilPassOp       = ToD3D11(desc.BackFace.PassOp);
        dsd.BackFace.StencilFunc         = ToD3D11(desc.BackFace.Func);

        auto* dev = D3D11Context::Get()->GetDevice();
        dev->CreateDepthStencilState(&dsd, m_DepthStencilState.GetAddressOf());
    }

    void D3D11PipelineState::CreateRasterizerState(const RasterizerStateDesc& desc)
    {
        D3D11_RASTERIZER_DESC rsd = {};
        rsd.FillMode              = ToD3D11(desc.FillMode);
        rsd.CullMode              = ToD3D11(desc.CullMode);
        rsd.FrontCounterClockwise = desc.FrontCounterClockwise;
        rsd.DepthBias             = desc.DepthBias;
        rsd.DepthBiasClamp        = desc.DepthBiasClamp;
        rsd.SlopeScaledDepthBias  = desc.SlopeScaledDepthBias;
        rsd.DepthClipEnable       = desc.DepthClipEnable;
        rsd.ScissorEnable         = desc.ScissorEnable;
        rsd.MultisampleEnable     = desc.MultisampleEnable;
        rsd.AntialiasedLineEnable = desc.AntialiasedLineEnable;

        auto* dev = D3D11Context::Get()->GetDevice();
        dev->CreateRasterizerState(&rsd, m_RasterizerState.GetAddressOf());
    }

    void D3D11PipelineState::CreateSamplerState(const SamplerStateDesc& desc)
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = ToD3D11(desc.Filter);
        sd.AddressU       = ToD3D11(desc.AddressU);
        sd.AddressV       = ToD3D11(desc.AddressV);
        sd.AddressW       = ToD3D11(desc.AddressW);
        sd.MipLODBias     = desc.MipLODBias;
        sd.MaxAnisotropy  = desc.MaxAnisotropy;
        sd.ComparisonFunc = ToD3D11(desc.ComparisonFunc);
        sd.BorderColor[0] = desc.BorderColor.r;
        sd.BorderColor[1] = desc.BorderColor.g;
        sd.BorderColor[2] = desc.BorderColor.b;
        sd.BorderColor[3] = desc.BorderColor.a;
        sd.MinLOD         = desc.MinLOD;
        sd.MaxLOD         = desc.MaxLOD;

        auto* dev = D3D11Context::Get()->GetDevice();
        dev->CreateSamplerState(&sd, m_SamplerState.GetAddressOf());
    }

    void D3D11PipelineState::Bind()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        if (m_BlendState)
            dc->OMSetBlendState(m_BlendState.Get(), m_BlendFactor, m_SampleMask);
        if (m_DepthStencilState)
            dc->OMSetDepthStencilState(m_DepthStencilState.Get(), m_StencilRef);
        if (m_RasterizerState)
            dc->RSSetState(m_RasterizerState.Get());
        if (m_SamplerState)
            dc->PSSetSamplers(m_Slot, 1, m_SamplerState.GetAddressOf());
    }

    void D3D11PipelineState::Unbind()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        if (m_BlendState)
            dc->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        if (m_DepthStencilState)
            dc->OMSetDepthStencilState(nullptr, 0);
        if (m_RasterizerState)
            dc->RSSetState(nullptr);
    }
}
