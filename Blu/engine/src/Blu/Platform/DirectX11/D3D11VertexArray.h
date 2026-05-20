#pragma once
#include "Blu/Rendering/VertexArray.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>

namespace Blu
{
    // DX11 has no VAO equivalent.  This class stores vertex/index buffers and
    // creates ID3D11InputLayouts lazily, keyed on the VS bytecode pointer that
    // D3D11Shader deposits in D3D11Context just before its Bind() call returns.
    class D3D11VertexArray : public VertexArray
    {
    public:
        D3D11VertexArray()  = default;
        ~D3D11VertexArray() = default;

        void Bind()   const override;
        void UnBind() const override {}

        void AddVertexBuffer(const Shared<VertexBuffer>& vb) override;
        void AddIndexBuffer(const Shared<IndexBuffer>&   ib) override;

        const std::vector<Shared<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
        const Shared<IndexBuffer>&               GetIndexBuffer()   const override { return m_IndexBuffer; }

    private:
        ID3D11InputLayout* GetOrCreateInputLayout() const;

        std::vector<Shared<VertexBuffer>> m_VertexBuffers;
        Shared<IndexBuffer>               m_IndexBuffer;

        // One InputLayout per distinct VS bytecode hash (not raw pointer — avoids
        // stale cache hits when old blobs are freed and reallocated at the same address).
        mutable std::unordered_map<size_t,
            Microsoft::WRL::ComPtr<ID3D11InputLayout>> m_InputLayouts;
    };
}
