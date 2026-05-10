#pragma once
#include "Blu/Rendering/Buffer.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
    class D3D11VertexBuffer : public VertexBuffer
    {
    public:
        // Dynamic (CPU-writeable) — used for batched 2D geometry
        D3D11VertexBuffer(uint32_t size);
        // Immutable — used for static meshes
        D3D11VertexBuffer(float* vertices, uint32_t size);
        ~D3D11VertexBuffer() = default;

        void Bind()   const override {}   // Binding happens in D3D11VertexArray
        void UnBind() const override {}

        void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
        void SetData(const void* data, uint32_t size)       override;

        const BufferLayout& GetLayout() const override { return m_Layout; }

        ID3D11Buffer* GetBuffer() const { return m_Buffer.Get(); }
        uint32_t      GetStride() const { return m_Layout.GetStride(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
        BufferLayout m_Layout;
        bool         m_Dynamic = false;
    };

    class D3D11IndexBuffer : public IndexBuffer
    {
    public:
        D3D11IndexBuffer(uint32_t* indices, uint32_t count);
        ~D3D11IndexBuffer() = default;

        void Bind()   const override;
        void UnBind() const override {}

        uint32_t GetCount() const override { return m_Count; }

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
        uint32_t m_Count = 0;
    };
}
