#include "Blupch.h"
#include "D3D11TextureCube.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

namespace Blu
{
    uint32_t D3D11TextureCube::s_NextID = 1;

    D3D11TextureCube::D3D11TextureCube(uint32_t size, uint32_t mipLevels)
        : m_Size(size), m_MipLevels(mipLevels), m_ID(s_NextID++)
    {
        auto* dev = D3D11Context::Get()->GetDevice();
        auto* dc  = D3D11Context::Get()->GetDeviceContext();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width              = size;
        desc.Height             = size;
        desc.MipLevels          = mipLevels;
        desc.ArraySize          = 6;
        desc.Format             = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT hr = dev->CreateTexture2D(&desc, nullptr, m_Texture.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11TextureCube creation failed");

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                      = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels       = mipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        dev->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SRV.GetAddressOf());

        // Trilinear clamp sampler — clamp avoids seams at cube face edges
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxAnisotropy  = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        dev->CreateSamplerState(&sd, m_Sampler.GetAddressOf());
    }

    void D3D11TextureCube::SetFaceData(int face, int mipLevel,
                                        const void* data, uint32_t rowPitch)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        UINT sub = D3D11CalcSubresource((UINT)mipLevel, (UINT)face, m_MipLevels);
        dc->UpdateSubresource(m_Texture.Get(), sub, nullptr, data, rowPitch, 0);
    }

    void D3D11TextureCube::Bind(uint32_t slot) const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->PSSetShaderResources(slot, 1, m_SRV.GetAddressOf());
        dc->PSSetSamplers(slot, 1, m_Sampler.GetAddressOf());
    }
}
