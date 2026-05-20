#pragma once
#include "Blu/Rendering/CascadedShadowMap.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>

namespace Blu
{
    class D3D11CascadedShadowMap : public CascadedShadowMap
    {
    public:
        explicit D3D11CascadedShadowMap(uint32_t size);
        ~D3D11CascadedShadowMap() override = default;

        void BindCascadeForWriting(int cascadeIndex) override;
        void UnbindForWriting()                      override;
        void BindTexture(uint32_t slot)              override;
        uint32_t GetSize() const override { return m_Size; }

    private:
        uint32_t m_Size;
        D3D11_VIEWPORT m_PrevViewport = {};

        // Saved render target state so UnbindForWriting can restore to the correct FB.
        // Two slots: [0] = colour, [1] = entity-ID (picking).  Saving only slot 0
        // loses the entity-ID RTV and breaks pixel-picking after the shadow pass.
        static constexpr UINT kMaxSavedRTVs = 2;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_PrevRTVs[kMaxSavedRTVs];
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_PrevDSV;

        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_DepthArray;
        std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>, NUM_CASCADES> m_DSVs;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
    };
}
