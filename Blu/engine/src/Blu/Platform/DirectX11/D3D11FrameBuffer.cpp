#include "Blupch.h"
#include "D3D11FrameBuffer.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

namespace Blu
{
    static bool IsDepthFormat(FrameBufferTextureFormat f)
    {
        return f == FrameBufferTextureFormat::DEPTH24STENCIL8;
    }

    static DXGI_FORMAT ToColorDXGI(FrameBufferTextureFormat f)
    {
        switch (f)
        {
        case FrameBufferTextureFormat::RGBA8:        return DXGI_FORMAT_R8G8B8A8_UNORM;
        case FrameBufferTextureFormat::RGBA16F:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case FrameBufferTextureFormat::RED_INTEGER:  return DXGI_FORMAT_R32_SINT;
        default: BLU_CORE_ASSERT(false, "Unknown colour format"); return DXGI_FORMAT_UNKNOWN;
        }
    }

    // -----------------------------------------------------------------------
    D3D11FrameBuffer::D3D11FrameBuffer(const FrameBufferSpecifications& specs)
        : m_Spec(specs)
    {
        Invalidate();
    }

    D3D11FrameBuffer::~D3D11FrameBuffer()
    {
        Release();
    }

    void D3D11FrameBuffer::Release()
    {
        m_ColorAttachments.clear();
        m_DepthTexture.Reset();
        m_DSV.Reset();
        m_DepthSRV.Reset();
        m_DepthStaging.Reset();
    }

    void D3D11FrameBuffer::Invalidate()
    {
        Release();

        auto* dev = D3D11Context::Get()->GetDevice();
        UINT  w   = m_Spec.Width;
        UINT  h   = m_Spec.Height;

        for (auto& attSpec : m_Spec.Attachments.Attachments)
        {
            if (IsDepthFormat(attSpec.TextureFormat))
            {
                // Depth-stencil — typeless so we can also bind as SRV for SSAO sampling
                D3D11_TEXTURE2D_DESC td = {};
                td.Width              = w;
                td.Height             = h;
                td.MipLevels          = 1;
                td.ArraySize          = 1;
                td.Format             = DXGI_FORMAT_R24G8_TYPELESS;
                td.SampleDesc.Count   = 1;
                td.Usage              = D3D11_USAGE_DEFAULT;
                td.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
                dev->CreateTexture2D(&td, nullptr, m_DepthTexture.GetAddressOf());

                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
                dsvDesc.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dev->CreateDepthStencilView(m_DepthTexture.Get(), &dsvDesc, m_DSV.GetAddressOf());

                D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
                depthSRVDesc.Format                    = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                depthSRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
                depthSRVDesc.Texture2D.MipLevels       = 1;
                depthSRVDesc.Texture2D.MostDetailedMip = 0;
                dev->CreateShaderResourceView(m_DepthTexture.Get(), &depthSRVDesc, m_DepthSRV.GetAddressOf());

                // Cached 1×1 staging for ReadDepth (R24_UNORM to read back the depth channel)
                {
                    D3D11_TEXTURE2D_DESC sd = {};
                    sd.Width          = 1;
                    sd.Height         = 1;
                    sd.MipLevels      = 1;
                    sd.ArraySize      = 1;
                    sd.Format         = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                    sd.SampleDesc     = { 1, 0 };
                    sd.Usage          = D3D11_USAGE_STAGING;
                    sd.BindFlags      = 0;
                    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    dev->CreateTexture2D(&sd, nullptr, m_DepthStaging.GetAddressOf());
                }
            }
            else
            {
                // Colour attachment
                DXGI_FORMAT fmt = ToColorDXGI(attSpec.TextureFormat);

                D3D11ColorAttachment att;
                att.format = attSpec.TextureFormat;

                const bool isInteger = (attSpec.TextureFormat == FrameBufferTextureFormat::RED_INTEGER);

                D3D11_TEXTURE2D_DESC td = {};
                td.Width              = w;
                td.Height             = h;
                td.MipLevels          = 1;
                td.ArraySize          = 1;
                td.Format             = fmt;
                td.SampleDesc.Count   = 1;
                td.Usage              = D3D11_USAGE_DEFAULT;
                td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
                // Integer attachments also need a UAV so we can clear them with
                // ClearUnorderedAccessViewUint — avoids a 3.6 MB CPU upload every frame.
                if (isInteger) td.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
                dev->CreateTexture2D(&td, nullptr, att.texture.GetAddressOf());

                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                rtvDesc.Format        = fmt;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                dev->CreateRenderTargetView(att.texture.Get(), &rtvDesc, att.rtv.GetAddressOf());

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format              = fmt;
                srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;
                dev->CreateShaderResourceView(att.texture.Get(), &srvDesc, att.srv.GetAddressOf());

                if (isInteger)
                {
                    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.Format        = fmt;
                    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                    dev->CreateUnorderedAccessView(att.texture.Get(), &uavDesc, att.uav.GetAddressOf());
                }

                m_ColorAttachments.push_back(std::move(att));
            }
        }
    }

    // -----------------------------------------------------------------------
    void D3D11FrameBuffer::Bind()
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        std::vector<ID3D11RenderTargetView*> rtvs;
        rtvs.reserve(m_ColorAttachments.size());
        for (auto& att : m_ColorAttachments)
            rtvs.push_back(att.rtv.Get());

        dc->OMSetRenderTargets((UINT)rtvs.size(),
            rtvs.data(), m_DSV.Get());

        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)m_Spec.Width;
        vp.Height   = (float)m_Spec.Height;
        vp.MaxDepth = 1.0f;
        dc->RSSetViewports(1, &vp);
    }

    void D3D11FrameBuffer::UnBind()
    {
        // Restore backbuffer
        auto* ctx = D3D11Context::Get();
        auto* dc  = ctx->GetDeviceContext();
        ID3D11RenderTargetView* rtv = ctx->GetBackbufferRTV();
        dc->OMSetRenderTargets(1, &rtv, ctx->GetDepthStencilView());
    }

    // -----------------------------------------------------------------------
    void D3D11FrameBuffer::Resize(uint32_t width, uint32_t height)
    {
        m_Spec.Width  = width;
        m_Spec.Height = height;
        Invalidate();
    }

    // -----------------------------------------------------------------------
    // Readback helpers — copy a single texel via a staging texture
    // -----------------------------------------------------------------------
    int D3D11FrameBuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
    {
        BLU_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Attachment index out of range");
        auto* dev = D3D11Context::Get()->GetDevice();
        auto* dc  = D3D11Context::Get()->GetDeviceContext();
        auto& att = m_ColorAttachments[attachmentIndex];

        D3D11_TEXTURE2D_DESC td = {};
        att.texture->GetDesc(&td);

        if (x < 0 || y < 0 || (UINT)x >= td.Width || (UINT)y >= td.Height)
            return -1;

        // Re-create the staging texture only when the source format/size changes.
        // Previously a new 1×1 staging texture was allocated on every call which
        // costs a device allocation + immediate deallocation each time.
        if (!att.stagingTexture)
        {
            D3D11_TEXTURE2D_DESC sd = {};
            sd.Width          = 1;
            sd.Height         = 1;
            sd.MipLevels      = 1;
            sd.ArraySize      = 1;
            sd.Format         = td.Format;
            sd.SampleDesc     = { 1, 0 };
            sd.Usage          = D3D11_USAGE_STAGING;
            sd.BindFlags      = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags      = 0;
            if (FAILED(dev->CreateTexture2D(&sd, nullptr, att.stagingTexture.GetAddressOf())))
                return -1;
        }

        D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)(x + 1), (UINT)(y + 1), 1 };
        dc->CopySubresourceRegion(att.stagingTexture.Get(), 0, 0, 0, 0, att.texture.Get(), 0, &box);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(dc->Map(att.stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped)) || !mapped.pData)
            return -1;

        int value = *static_cast<int*>(mapped.pData);
        dc->Unmap(att.stagingTexture.Get(), 0);
        return value;
    }

    bool D3D11FrameBuffer::ReadColorAttachmentRGBA8(uint32_t attachmentIndex, std::vector<uint8_t>& outPixels,
                                                    uint32_t& outWidth, uint32_t& outHeight) const
    {
        if (attachmentIndex >= m_ColorAttachments.size())
            return false;
        auto* dev = D3D11Context::Get()->GetDevice();
        auto* dc  = D3D11Context::Get()->GetDeviceContext();
        const auto& att = m_ColorAttachments[attachmentIndex];
        if (!att.texture)
            return false;

        D3D11_TEXTURE2D_DESC td = {};
        att.texture->GetDesc(&td);
        if (td.Format != DXGI_FORMAT_R8G8B8A8_UNORM) // screenshot attachment is RGBA8
            return false;

        D3D11_TEXTURE2D_DESC sd = td;
        sd.Usage          = D3D11_USAGE_STAGING;
        sd.BindFlags      = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags      = 0;
        sd.MipLevels      = 1;
        sd.ArraySize      = 1;
        sd.SampleDesc     = { 1, 0 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if (FAILED(dev->CreateTexture2D(&sd, nullptr, staging.GetAddressOf())))
            return false;

        dc->CopyResource(staging.Get(), att.texture.Get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(dc->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)) || !mapped.pData)
            return false;

        outWidth  = td.Width;
        outHeight = td.Height;
        outPixels.resize((size_t)td.Width * td.Height * 4);
        const uint8_t* src = static_cast<const uint8_t*>(mapped.pData);
        for (uint32_t row = 0; row < td.Height; ++row)
            std::memcpy(outPixels.data() + (size_t)row * td.Width * 4,
                        src + (size_t)row * mapped.RowPitch,
                        (size_t)td.Width * 4);

        dc->Unmap(staging.Get(), 0);
        return true;
    }

    float D3D11FrameBuffer::ReadDepth(uint32_t /*attachmentIndex*/, int x, int y)
    {
        if (!m_DepthTexture) return 1.0f;
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)(x + 1), (UINT)(y + 1), 1 };
        dc->CopySubresourceRegion(m_DepthStaging.Get(), 0, 0, 0, 0, m_DepthTexture.Get(), 0, &box);

        D3D11_MAPPED_SUBRESOURCE mapped;
        dc->Map(m_DepthStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        float depth = *static_cast<float*>(mapped.pData);
        dc->Unmap(m_DepthStaging.Get(), 0);
        return depth;
    }

    // -----------------------------------------------------------------------
    uint64_t D3D11FrameBuffer::GetColorAttachmentID(uint32_t index) const
    {
        BLU_CORE_ASSERT(index < m_ColorAttachments.size(), "Attachment index out of range");
        return reinterpret_cast<uint64_t>(m_ColorAttachments[index].srv.Get());
    }

    ID3D11ShaderResourceView* D3D11FrameBuffer::GetColorAttachmentSRV(uint32_t index) const
    {
        BLU_CORE_ASSERT(index < m_ColorAttachments.size(), "Attachment index out of range");
        return m_ColorAttachments[index].srv.Get();
    }

    ID3D11RenderTargetView* D3D11FrameBuffer::GetColorAttachmentRTV(uint32_t index) const
    {
        BLU_CORE_ASSERT(index < m_ColorAttachments.size(), "Attachment index out of range");
        return m_ColorAttachments[index].rtv.Get();
    }

    void D3D11FrameBuffer::ClearAttachment(uint32_t attachmentIndex, int value)
    {
        BLU_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Attachment index out of range");
        auto* dc  = D3D11Context::Get()->GetDeviceContext();
        auto& att = m_ColorAttachments[attachmentIndex];

        if (att.format == FrameBufferTextureFormat::RED_INTEGER && att.uav)
        {
            // D3D11 forbids a resource being simultaneously bound as RTV and UAV.
            // Temporarily clear all render target bindings so the UAV clear succeeds,
            // then restore them. The caller is expected to call Bind() afterwards.
            ID3D11RenderTargetView* nullRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
            dc->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, nullRTVs, nullptr);

            UINT v[4] = { static_cast<UINT>(value), 0u, 0u, 0u };
            dc->ClearUnorderedAccessViewUint(att.uav.Get(), v);
        }
        else
        {
            float rgba[4] = { (float)value, 0.0f, 0.0f, 1.0f };
            dc->ClearRenderTargetView(att.rtv.Get(), rgba);
        }
    }
}
