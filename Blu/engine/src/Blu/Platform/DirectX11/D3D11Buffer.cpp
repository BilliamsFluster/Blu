#include "Blupch.h"
#include "D3D11Buffer.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

namespace Blu
{
    // -----------------------------------------------------------------------
    // D3D11VertexBuffer
    // -----------------------------------------------------------------------

    D3D11VertexBuffer::D3D11VertexBuffer(uint32_t size)
        : m_Dynamic(true)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth      = size;
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = D3D11Context::Get()->GetDevice()->CreateBuffer(&desc, nullptr, m_Buffer.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11VertexBuffer (dynamic) creation failed");
    }

    D3D11VertexBuffer::D3D11VertexBuffer(float* vertices, uint32_t size)
        : m_Dynamic(false)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth  = size;
        desc.Usage      = D3D11_USAGE_DEFAULT;
        desc.BindFlags  = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA srd = {};
        srd.pSysMem = vertices;

        HRESULT hr = D3D11Context::Get()->GetDevice()->CreateBuffer(&desc, &srd, m_Buffer.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11VertexBuffer (static) creation failed");
    }

    void D3D11VertexBuffer::SetData(const void* data, uint32_t size)
    {
        BLU_CORE_ASSERT(m_Dynamic, "SetData called on a non-dynamic vertex buffer");
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        D3D11_MAPPED_SUBRESOURCE mapped;
        dc->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, data, size);
        dc->Unmap(m_Buffer.Get(), 0);
    }

    // -----------------------------------------------------------------------
    // D3D11IndexBuffer
    // -----------------------------------------------------------------------

    D3D11IndexBuffer::D3D11IndexBuffer(uint32_t* indices, uint32_t count)
        : m_Count(count)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(uint32_t) * count;
        desc.Usage     = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA srd = {};
        srd.pSysMem = indices;

        HRESULT hr = D3D11Context::Get()->GetDevice()->CreateBuffer(&desc, &srd, m_Buffer.GetAddressOf());
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11IndexBuffer creation failed");
    }

    void D3D11IndexBuffer::Bind() const
    {
        D3D11Context::Get()->GetDeviceContext()
            ->IASetIndexBuffer(m_Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
}
