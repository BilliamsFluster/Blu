#pragma once
#include "Blu/Rendering/TextureCube.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    class D3D11TextureCube : public TextureCube
    {
    public:
        // Creates an empty RGBA16F cubemap with the given size and mip count.
        D3D11TextureCube(uint32_t size, uint32_t mipLevels);
        ~D3D11TextureCube() = default;

        uint32_t GetWidth()      const override { return m_Size; }
        uint32_t GetHeight()     const override { return m_Size; }
        uint32_t GetRendererID() const override { return m_ID; }
        uint64_t GetImTextureID() const override { return reinterpret_cast<uint64_t>(m_SRV.Get()); }
        uint32_t GetMipLevels()  const override { return m_MipLevels; }

        void SetData(void*, uint32_t) override {}  // not used; use SetFaceData
        void SetFaceData(int face, int mipLevel, const void* data, uint32_t rowPitch) override;
        void Bind(uint32_t slot = 0) const override;

        bool operator==(const Texture& other) const override
        {
            return m_ID == static_cast<const D3D11TextureCube&>(other).m_ID;
        }

        ID3D11ShaderResourceView* GetSRV() const { return m_SRV.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_Texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_Sampler;

        uint32_t m_Size      = 0;
        uint32_t m_MipLevels = 1;
        uint32_t m_ID        = 0;

        static uint32_t s_NextID;
    };
}
