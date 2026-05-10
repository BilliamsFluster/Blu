#pragma once
#include "Blu/Rendering/Texture.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    class D3D11Texture2D : public Texture2D
    {
    public:
        // Load from file (uses stb_image)
        explicit D3D11Texture2D(const std::string& path);
        // Create empty texture of given dimensions
        D3D11Texture2D(uint32_t width, uint32_t height);
        ~D3D11Texture2D() = default;

        uint32_t GetWidth()      const override { return m_Width; }
        uint32_t GetHeight()     const override { return m_Height; }
        uint32_t GetRendererID() const override { return m_ID; }
        uint64_t GetImTextureID() const override { return reinterpret_cast<uint64_t>(m_SRV.Get()); }

        void SetData(void* data, uint32_t size)        override;
        void Bind(uint32_t slot = 0)             const override;
        std::string GetTexturePath()                   override { return m_Path; }
        void ConfigureTexture()                        override {}

        bool operator==(const Texture& other) const override
        {
            return m_ID == static_cast<const D3D11Texture2D&>(other).m_ID;
        }

        // Expose SRV for use with ImGui DX11 backend
        ID3D11ShaderResourceView* GetSRV() const { return m_SRV.Get(); }

    private:
        void CreateFromData(const void* data, uint32_t width, uint32_t height,
                            DXGI_FORMAT format);

        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_Texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_Sampler;

        uint32_t    m_Width  = 0;
        uint32_t    m_Height = 0;
        uint32_t    m_ID     = 0;  // Sequential ID for operator==
        std::string m_Path;

        static uint32_t s_NextID;
    };
}
