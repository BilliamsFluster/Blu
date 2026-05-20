#pragma once
#include "Blu/Rendering/ShadowMap.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    class D3D11ShadowMap : public ShadowMap
    {
    public:
        D3D11ShadowMap(uint32_t size);
        ~D3D11ShadowMap() override = default;

        void BindForWriting()  override;
        void UnbindForWriting() override;
        void BindTexture(uint32_t slot) override;
        uint32_t GetSize() const override { return m_Size; }

        ID3D11ShaderResourceView* GetSRV() const { return m_SRV.Get(); }

    private:
        uint32_t m_Size;
        D3D11_VIEWPORT m_PrevViewport = {};

        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_DepthTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_DSV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
    };
}
