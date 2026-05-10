#include "Blupch.h"
#include "D3D11VertexArray.h"
#include "D3D11Buffer.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"
#include "Blu/Rendering/Buffer.h"

namespace Blu
{
    // Map engine ShaderDataType -> DXGI_FORMAT
    static DXGI_FORMAT ToDXGIFormat(ShaderDataType type)
    {
        switch (type)
        {
        case ShaderDataType::Float:  return DXGI_FORMAT_R32_FLOAT;
        case ShaderDataType::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case ShaderDataType::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case ShaderDataType::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case ShaderDataType::Int:    return DXGI_FORMAT_R32_SINT;
        case ShaderDataType::Int2:   return DXGI_FORMAT_R32G32_SINT;
        case ShaderDataType::Int3:   return DXGI_FORMAT_R32G32B32_SINT;
        case ShaderDataType::Int4:   return DXGI_FORMAT_R32G32B32A32_SINT;
        default:
            BLU_CORE_ASSERT(false, "Unknown ShaderDataType");
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    void D3D11VertexArray::AddVertexBuffer(const Shared<VertexBuffer>& vb)
    {
        BLU_CORE_ASSERT(!vb->GetLayout().GetElements().empty(), "VertexBuffer has no layout");
        m_VertexBuffers.push_back(vb);
        m_InputLayouts.clear();  // Invalidate cached layouts
    }

    void D3D11VertexArray::AddIndexBuffer(const Shared<IndexBuffer>& ib)
    {
        m_IndexBuffer = ib;
    }

    ID3D11InputLayout* D3D11VertexArray::GetOrCreateInputLayout() const
    {
        auto* ctx     = D3D11Context::Get();
        const void*  vs     = ctx->GetCurrentVSBytecode();
        SIZE_T       vsSize = ctx->GetCurrentVSBytecodeSize();

        if (!vs)
        {
            BLU_CORE_WARN("D3D11VertexArray::Bind() called without a bound D3D11Shader");
            return nullptr;
        }

        auto it = m_InputLayouts.find(vs);
        if (it != m_InputLayouts.end())
            return it->second.Get();

        // Build D3D11_INPUT_ELEMENT_DESC array from all vertex buffer layouts.
        // The SemanticName is the engine's BufferElement.Name (e.g. "a_Position").
        // The HLSL VS input struct must use the same name after the colon.
        std::vector<D3D11_INPUT_ELEMENT_DESC> descs;
        UINT slot = 0;
        for (auto& vb : m_VertexBuffers)
        {
            for (auto& elem : vb->GetLayout())
            {
                D3D11_INPUT_ELEMENT_DESC d = {};
                d.SemanticName         = elem.Name.c_str();
                d.SemanticIndex        = 0;
                d.Format               = ToDXGIFormat(elem.Type);
                d.InputSlot            = slot;
                d.AlignedByteOffset    = elem.Offset;
                d.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
                d.InstanceDataStepRate = 0;
                descs.push_back(d);
            }
            ++slot;
        }

        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        HRESULT hr = ctx->GetDevice()->CreateInputLayout(
            descs.data(), (UINT)descs.size(), vs, vsSize, layout.GetAddressOf());

        if (FAILED(hr))
        {
            BLU_CORE_ERROR("D3D11: CreateInputLayout failed (0x{0:X})", (uint32_t)hr);
            return nullptr;
        }

        m_InputLayouts[vs] = layout;
        return layout.Get();
    }

    void D3D11VertexArray::Bind() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        // Set the input layout (created from current VS bytecode + our buffer layout)
        ID3D11InputLayout* il = GetOrCreateInputLayout();
        if (il) dc->IASetInputLayout(il);

        // Bind all vertex buffers
        std::vector<ID3D11Buffer*> buffers;
        std::vector<UINT>         strides;
        std::vector<UINT>         offsets;
        for (auto& vb : m_VertexBuffers)
        {
            auto* d3dVB = static_cast<D3D11VertexBuffer*>(vb.get());
            buffers.push_back(d3dVB->GetBuffer());
            strides.push_back(d3dVB->GetStride());
            offsets.push_back(0);
        }
        if (!buffers.empty())
            dc->IASetVertexBuffers(0, (UINT)buffers.size(),
                buffers.data(), strides.data(), offsets.data());

        // Bind index buffer
        if (m_IndexBuffer)
            m_IndexBuffer->Bind();

        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
