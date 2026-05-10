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
        stbi_set_flip_vertically_on_load(1);
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // Force RGBA
        BLU_CORE_ASSERT(data, "Failed to load texture: {0}", path);

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

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width              = width;
        desc.Height             = height;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.Format             = format;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA srd = {};
        srd.pSysMem          = data;
        srd.SysMemPitch      = width * 4;
        srd.SysMemSlicePitch = 0;

        HRESULT hr = dev->CreateTexture2D(&desc, data ? &srd : nullptr,
            m_Texture.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11Texture2D creation failed");

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format              = format;
        srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        dev->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SRV.GetAddressOf());

        // Linear wrap sampler
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
