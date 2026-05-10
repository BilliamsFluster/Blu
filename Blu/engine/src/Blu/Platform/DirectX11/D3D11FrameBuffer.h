#pragma once
#include "Blu/Rendering/FrameBuffer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace Blu
{
    // Stores per-attachment render-target and shader-resource views.
    struct D3D11ColorAttachment
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D>             texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>      rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    srv;
        // UAV created for integer attachments so ClearAttachment can use
        // ClearUnorderedAccessViewUint instead of a full CPU-side buffer upload.
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>   uav;
        FrameBufferTextureFormat                            format = FrameBufferTextureFormat::None;
        // Cached 1×1 staging texture for ReadPixel — created once, reused on every click.
        Microsoft::WRL::ComPtr<ID3D11Texture2D>             stagingTexture;
    };

    class D3D11FrameBuffer : public FrameBuffer
    {
    public:
        explicit D3D11FrameBuffer(const FrameBufferSpecifications& specs);
        ~D3D11FrameBuffer();

        const FrameBufferSpecifications& GetSpecification() const override { return m_Spec; }

        void Bind()   override;
        void UnBind() override;

        void Resize(uint32_t width, uint32_t height) override;

        // Reads back a single integer pixel from the colour attachment (e.g. entity ID)
        int   ReadPixel(uint32_t attachmentIndex, int x, int y) override;
        float ReadDepth(uint32_t attachmentIndex, int x, int y) override;

        // Returns the SRV pointer as uint64 (full 64-bit; safe to cast to ImTextureID / ID3D11ShaderResourceView*).
        uint64_t GetColorAttachmentID(uint32_t index = 0) const override;

        void ClearAttachment(uint32_t attachmentIndex, int value) override;

        // Convenience for DX11-aware code that wants the raw SRV
        ID3D11ShaderResourceView* GetColorAttachmentSRV(uint32_t index = 0) const;

    private:
        void Invalidate();
        void Release();

        FrameBufferSpecifications m_Spec;

        std::vector<D3D11ColorAttachment>                m_ColorAttachments;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_DepthTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_DSV;
    };
}
