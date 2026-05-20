#pragma once
#include "Blu/Core/Core.h"
#include <glm/glm.hpp>
#include <functional>
#include <cfloat>
#include <unordered_map>

namespace Blu
{
    enum class FillMode : uint8_t { Solid, Wireframe };
    enum class CullMode : uint8_t  { None, Front, Back };
    enum class BlendOp : uint8_t   { Add, Subtract, ReverseSubtract, Min, Max };
    enum class BlendFactor : uint8_t
    {
        Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha,
        DstColor, InvDstColor, DstAlpha, InvDstAlpha,
        SrcAlphaSat, BlendFactor, InvBlendFactor
    };
    enum class ComparisonFunc : uint8_t
    {
        Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
    };
    enum class StencilOp : uint8_t
    {
        Keep, Zero, Replace, IncrSat, DecrSat, Invert, Incr, Decr
    };
    enum class TextureFilter : uint8_t
    {
        Point, Linear, Anisotropic,
        MinPointMagLinearMipPoint, MinPointMagLinearMipLinear,
        MinLinearMagPointMipPoint, MinLinearMagPointMipLinear
    };
    enum class TextureAddressMode : uint8_t
    {
        Wrap, Mirror, Clamp, Border, MirrorOnce
    };

    struct RTBlendDesc
    {
        bool        BlendEnable          = false;
        BlendFactor SrcBlend             = BlendFactor::One;
        BlendFactor DstBlend             = BlendFactor::Zero;
        BlendOp     BlendOperation       = BlendOp::Add;
        BlendFactor SrcBlendAlpha        = BlendFactor::One;
        BlendFactor DstBlendAlpha        = BlendFactor::Zero;
        BlendOp     BlendOperationAlpha  = BlendOp::Add;
        uint8_t     RenderTargetWriteMask = 0x0F;
    };

    struct BlendStateDesc
    {
        bool        AlphaToCoverageEnable = false;
        bool        IndependentBlend      = false;
        RTBlendDesc RenderTarget[8];
    };

    struct DepthStencilOpDesc
    {
        StencilOp      FailOp      = StencilOp::Keep;
        StencilOp      DepthFailOp = StencilOp::Keep;
        StencilOp      PassOp      = StencilOp::Keep;
        ComparisonFunc Func        = ComparisonFunc::Always;
    };

    struct DepthStencilStateDesc
    {
        bool               DepthEnable      = true;
        bool               DepthWriteMask   = true;
        ComparisonFunc     DepthFunc        = ComparisonFunc::Less;
        bool               StencilEnable    = false;
        uint8_t            StencilReadMask  = 0xFF;
        uint8_t            StencilWriteMask = 0xFF;
        DepthStencilOpDesc FrontFace;
        DepthStencilOpDesc BackFace;
    };

    struct RasterizerStateDesc
    {
        FillMode    FillMode              = FillMode::Solid;
        CullMode    CullMode              = CullMode::Back;
        bool        FrontCounterClockwise = true;
        int32_t     DepthBias             = 0;
        float       DepthBiasClamp        = 0.0f;
        float       SlopeScaledDepthBias  = 0.0f;
        bool        DepthClipEnable       = true;
        bool        ScissorEnable         = false;
        bool        MultisampleEnable     = false;
        bool        AntialiasedLineEnable = false;
    };

    struct SamplerStateDesc
    {
        TextureFilter      Filter        = TextureFilter::Linear;
        TextureAddressMode AddressU      = TextureAddressMode::Wrap;
        TextureAddressMode AddressV      = TextureAddressMode::Wrap;
        TextureAddressMode AddressW      = TextureAddressMode::Wrap;
        float              MipLODBias    = 0.0f;
        uint32_t           MaxAnisotropy = 1;
        ComparisonFunc     ComparisonFunc = ComparisonFunc::Never;
        glm::vec4          BorderColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float              MinLOD        = 0.0f;
        float              MaxLOD        = FLT_MAX;
    };

    struct PipelineStateDesc
    {
        BlendStateDesc        BlendState;
        DepthStencilStateDesc DepthStencilState;
        RasterizerStateDesc   RasterizerState;
        SamplerStateDesc      SamplerState;
    };

    // Default pipeline states for common configurations
    struct DefaultPipelineStates
    {
        static PipelineStateDesc Opaque();
        static PipelineStateDesc AlphaBlend();
        static PipelineStateDesc Transparent();   // AlphaBlend + depth write off
        static PipelineStateDesc Wireframe();
        static PipelineStateDesc NoDepth();
        static PipelineStateDesc ShadowMap();
        static PipelineStateDesc AdditiveBlend();
        static PipelineStateDesc CullNone();      // Opaque, two-sided
    };

    class PipelineState
    {
    public:
        virtual ~PipelineState() = default;

        virtual void Bind()   = 0;
        virtual void Unbind() = 0;

        static Shared<PipelineState> Create(const PipelineStateDesc& desc);
    };

    class PipelineStateCache
    {
    public:
        static Shared<PipelineState> GetOrCreate(const PipelineStateDesc& desc);

        static Shared<PipelineState> GetOpaque();
        static Shared<PipelineState> GetAlphaBlend();
        static Shared<PipelineState> GetTransparent();   // AlphaBlend + depth write off
        static Shared<PipelineState> GetWireframe();
        static Shared<PipelineState> GetNoDepth();
        static Shared<PipelineState> GetShadowMap();
        static Shared<PipelineState> GetAdditiveBlend();
        static Shared<PipelineState> GetCullNone();      // Opaque, two-sided

        static void Clear();

    private:
        static std::unordered_map<size_t, Shared<PipelineState>> s_Cache;
    };
}
