#pragma once
#include "Blu/Rendering/PipelineState.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    class D3D11PipelineState : public PipelineState
    {
    public:
        explicit D3D11PipelineState(const PipelineStateDesc& desc);
        ~D3D11PipelineState() = default;

        void Bind()   override;
        void Unbind() override;

    private:
        void CreateBlendState(const BlendStateDesc& desc);
        void CreateDepthStencilState(const DepthStencilStateDesc& desc);
        void CreateRasterizerState(const RasterizerStateDesc& desc);
        void CreateSamplerState(const SamplerStateDesc& desc);

        Microsoft::WRL::ComPtr<ID3D11BlendState>         m_BlendState;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthStencilState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>    m_RasterizerState;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_SamplerState;

        float m_BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t m_SampleMask  = 0xFFFFFFFF;
        uint32_t m_StencilRef  = 0;
        uint32_t m_Slot        = 0;
    };
}
