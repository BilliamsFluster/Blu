#include "Blupch.h"
#include "D3D11Texture.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

#include <stb_image.h>

namespace Blu
{
    uint32_t D3D11Texture2D::s_NextID = 1;

    D3D11Texture2D::D3D11Texture2D(const std::string& path)
        : m_Path(path), m_ID(s_NextID++)
    {
        int w, h, channels;
        stbi_set_flip_vertically_on_load(0);
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // Force RGBA
        if (!data)
        {
            BLU_CORE_WARN("Failed to load texture: {0} — {1}", path, stbi_failure_reason() ? stbi_failure_reason() : "unknown error");
            // Create 1x1 magenta fallback
            uint8_t fallback[4] = { 255, 0, 255, 255 };
            m_Width = m_Height = 1;
            CreateFromData(fallback, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
            return;
        }

        m_Width  = (uint32_t)w;
        m_Height = (uint32_t)h;
        CreateFromData(data, m_Width, m_Height, DXGI_FORMAT_R8G8B8A8_UNORM);
        stbi_image_free(data);
    }

    D3D11Texture2D::D3D11Texture2D(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height), m_ID(s_NextID++)
    {
        std::vector<uint8_t> blank(width * height * 4, 255);
        CreateFromData(blank.data(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    void D3D11Texture2D::CreateFromData(const void* data, uint32_t width, uint32_t height,
                                        DXGI_FORMAT format)
    {
        auto* dev = D3D11Context::Get()->GetDevice();
        auto* dc  = D3D11Context::Get()->GetDeviceContext();

        // Compute full mip chain count
        UINT mipCount = 1u;
        {
            UINT w = width, h = height;
            while (w > 1u || h > 1u) { w = std::max(1u, w / 2u); h = std::max(1u, h / 2u); ++mipCount; }
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width              = width;
        desc.Height             = height;
        desc.MipLevels          = mipCount;
        desc.ArraySize          = 1;
        desc.Format             = format;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags          = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        // Create texture without initial data (we upload it below); this avoids
        // having to supply pInitialData entries for every mip level.
        HRESULT hr = dev->CreateTexture2D(&desc, nullptr, m_Texture.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11Texture2D creation failed");

        // Upload top-level data via UpdateSubresource
        if (data)
            dc->UpdateSubresource(m_Texture.Get(), 0, nullptr, data, width * 4, 0);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format              = format;
        srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = -1;   // all mip levels available
        dev->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SRV.GetAddressOf());

        // Generate mip chain from the top-level data
        dc->GenerateMips(m_SRV.Get());

        // Linear wrap sampler with full mip clamping
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxAnisotropy  = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        dev->CreateSamplerState(&sd, m_Sampler.GetAddressOf());
    }

    void D3D11Texture2D::SetData(void* data, uint32_t /*size*/)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->UpdateSubresource(m_Texture.Get(), 0, nullptr, data, m_Width * 4, 0);
    }

    void D3D11Texture2D::Bind(uint32_t slot) const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->PSSetShaderResources(slot, 1, m_SRV.GetAddressOf());
        dc->PSSetSamplers(slot, 1, m_Sampler.GetAddressOf());
    }
}
