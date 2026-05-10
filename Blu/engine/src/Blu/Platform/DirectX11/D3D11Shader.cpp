#include "Blupch.h"
#include "D3D11Shader.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

#include <fstream>
#include <sstream>
#include <d3dcompiler.h>
#include <glm/gtc/type_ptr.hpp>

#pragma comment(lib, "d3dcompiler.lib")

namespace Blu
{
    // -----------------------------------------------------------------------
    // File loading helpers
    // -----------------------------------------------------------------------
    static std::unordered_map<std::string, std::string> SplitShaderSource(
        const std::string& src)
    {
        std::unordered_map<std::string, std::string> shaderMap;
        const char* typeToken = "#type";
        size_t typeTokenLen = strlen(typeToken);
        size_t pos = src.find(typeToken, 0);

        // Everything before the first #type (cbuffer defs, shared structs) is the
        // preamble — prepend it to each section so all stages see shared declarations.
        std::string preamble = (pos != std::string::npos) ? src.substr(0, pos) : "";

        while (pos != std::string::npos)
        {
            size_t eol  = src.find_first_of("\r\n", pos);
            size_t begin = pos + typeTokenLen + 1;
            std::string type = src.substr(begin, eol - begin);
            // Trim trailing \r or spaces
            while (!type.empty() && (type.back() == '\r' || type.back() == ' '))
                type.pop_back();

            size_t nextLinePos = src.find_first_not_of("\r\n", eol);
            pos = src.find(typeToken, nextLinePos);
            std::string body = (pos == std::string::npos)
                ? src.substr(nextLinePos)
                : src.substr(nextLinePos, pos - nextLinePos);

            shaderMap[type] = preamble + body;
        }
        return shaderMap;
    }

    static std::string ReadFile(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::in | std::ios::binary);
        BLU_CORE_ASSERT(file, "Could not open shader file: {0}", filepath);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static std::string NameFromPath(const std::string& filepath)
    {
        auto lastSlash = filepath.find_last_of("/\\");
        lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        auto lastDot = filepath.rfind('.');
        auto count   = (lastDot == std::string::npos) ? filepath.size() - lastSlash
                                                       : lastDot - lastSlash;
        return filepath.substr(lastSlash, count);
    }

    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------
    D3D11Shader::D3D11Shader(const std::string& filepath)
        : m_Filepath(filepath)
    {
        m_Name = NameFromPath(filepath);
        auto src = SplitShaderSource(ReadFile(filepath));
        BLU_CORE_ASSERT(src.count("vertex") && (src.count("pixel") || src.count("fragment")),
            "HLSL shader must have #type vertex and #type pixel sections");
        std::string& psSrc = src.count("pixel") ? src["pixel"] : src["fragment"];
        Compile(src["vertex"], psSrc);
    }

    D3D11Shader::D3D11Shader(const std::string& name,
                             const std::string& vertexSrc,
                             const std::string& pixelSrc)
        : m_Name(name)
    {
        Compile(vertexSrc, pixelSrc);
    }

    // -----------------------------------------------------------------------
    // Compile HLSL source using D3DCompile
    // -----------------------------------------------------------------------
    void D3D11Shader::Compile(const std::string& vertexSrc, const std::string& pixelSrc)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef BLU_DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        auto Compile = [&](const std::string& src, const char* target,
                           ID3DBlob** outBlob) -> bool
        {
            Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
            HRESULT hr = D3DCompile(
                src.c_str(), src.size(),
                m_Filepath.empty() ? m_Name.c_str() : m_Filepath.c_str(),
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "main", target, flags, 0,
                outBlob, errBlob.GetAddressOf());

            if (FAILED(hr))
            {
                if (errBlob)
                    BLU_CORE_ERROR("HLSL compile error ({0} {1}): {2}",
                        m_Name, target,
                        reinterpret_cast<const char*>(errBlob->GetBufferPointer()));
                BLU_CORE_ASSERT(false, "Shader compilation failed");
                return false;
            }
            return true;
        };

        if (!Compile(vertexSrc, "vs_5_0", m_VSBlob.GetAddressOf())) return;

        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
        if (!Compile(pixelSrc,  "ps_5_0", psBlob.GetAddressOf()))  return;

        dev->CreateVertexShader(m_VSBlob->GetBufferPointer(),
            m_VSBlob->GetBufferSize(), nullptr, m_VS.GetAddressOf());
        dev->CreatePixelShader(psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),  nullptr, m_PS.GetAddressOf());

        Reflect(m_VSBlob.Get(), psBlob.Get());
    }

    // -----------------------------------------------------------------------
    // Shader reflection — build constant-buffer objects + uniform name map
    // -----------------------------------------------------------------------
    static uint32_t GetScalarBytes(const D3D11_SHADER_TYPE_DESC& td)
    {
        // Rows * Columns * 4 bytes per component.
        // Matrix rows are padded to 16-bytes in HLSL (each row = float4).
        if (td.Class == D3D_SVC_MATRIX_ROWS || td.Class == D3D_SVC_MATRIX_COLUMNS)
            return td.Rows * 16u;        // each row stored as float4
        return td.Rows * td.Columns * 4u;
    }

    void D3D11Shader::TraverseType(
        ID3D11ShaderReflectionType* type,
        const std::string& baseName,
        uint32_t baseOffset,
        uint32_t cbufferIndex,
        uint32_t totalSize)
    {
        D3D11_SHADER_TYPE_DESC td;
        type->GetDesc(&td);

        if (td.Elements > 0)
        {
            // Array — recurse for each element
            uint32_t elementStride = totalSize / td.Elements;
            for (uint32_t e = 0; e < td.Elements; e++)
            {
                std::string elemName = baseName + "[" + std::to_string(e) + "]";
                uint32_t    elemOff  = baseOffset + e * elementStride;

                if (td.Members > 0)
                {
                    // Array of structs — walk members
                    for (uint32_t m = 0; m < td.Members; m++)
                    {
                        LPCSTR memberName = type->GetMemberTypeName(m);
                        auto*  memberType = type->GetMemberTypeByIndex(m);
                        D3D11_SHADER_TYPE_DESC mtd;
                        memberType->GetDesc(&mtd);
                        uint32_t memberSize = GetScalarBytes(mtd);
                        TraverseType(memberType,
                                     elemName + "." + memberName,
                                     elemOff + mtd.Offset,
                                     cbufferIndex,
                                     memberSize);
                    }
                }
                else
                {
                    // Array of scalars/vectors/matrices
                    uint32_t leafSize = GetScalarBytes(td);
                    m_UniformMap[elemName] = { cbufferIndex, elemOff, leafSize };
                }
            }
        }
        else if (td.Members > 0)
        {
            // Struct — walk members
            for (uint32_t m = 0; m < td.Members; m++)
            {
                LPCSTR memberName = type->GetMemberTypeName(m);
                auto*  memberType = type->GetMemberTypeByIndex(m);
                D3D11_SHADER_TYPE_DESC mtd;
                memberType->GetDesc(&mtd);
                uint32_t memberSize = GetScalarBytes(mtd);
                TraverseType(memberType,
                             baseName + "." + memberName,
                             baseOffset + mtd.Offset,
                             cbufferIndex,
                             memberSize);
            }
        }
        else
        {
            // Leaf scalar / vector / matrix
            uint32_t size = GetScalarBytes(td);
            m_UniformMap[baseName] = { cbufferIndex, baseOffset, size };
        }
    }

    void D3D11Shader::Reflect(ID3DBlob* vsBlob, ID3DBlob* psBlob)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

        // We reflect both stages and merge cbuffer info (same slot -> same buffer)
        auto ReflectStage = [&](ID3DBlob* blob)
        {
            Microsoft::WRL::ComPtr<ID3D11ShaderReflection> refl;
            D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(),
                IID_ID3D11ShaderReflection, reinterpret_cast<void**>(refl.GetAddressOf()));

            D3D11_SHADER_DESC shDesc;
            refl->GetDesc(&shDesc);

            for (UINT cb = 0; cb < shDesc.ConstantBuffers; cb++)
            {
                auto* cbRefl = refl->GetConstantBufferByIndex(cb);
                D3D11_SHADER_BUFFER_DESC cbDesc;
                cbRefl->GetDesc(&cbDesc);

                // Find binding slot
                UINT slot = cb;
                for (UINT r = 0; r < shDesc.BoundResources; r++)
                {
                    D3D11_SHADER_INPUT_BIND_DESC bd;
                    refl->GetResourceBindingDesc(r, &bd);
                    if (bd.Type == D3D_SIT_CBUFFER && strcmp(bd.Name, cbDesc.Name) == 0)
                    {
                        slot = bd.BindPoint;
                        break;
                    }
                }

                // Ensure m_CBuffers is large enough
                if (slot >= m_CBuffers.size())
                    m_CBuffers.resize(slot + 1);

                auto& cbBuf = m_CBuffers[slot];
                if (cbBuf.gpuBuffer) continue;  // Already set up by other stage

                cbBuf.slot = slot;
                cbBuf.shadow.assign(cbDesc.Size, 0);

                D3D11_BUFFER_DESC gpuDesc = {};
                gpuDesc.ByteWidth      = (cbDesc.Size + 15) & ~15u;  // 16-byte aligned
                gpuDesc.Usage          = D3D11_USAGE_DEFAULT;
                gpuDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

                HRESULT hr = dev->CreateBuffer(&gpuDesc, nullptr, cbBuf.gpuBuffer.GetAddressOf());
                BLU_CORE_ASSERT(SUCCEEDED(hr), "Failed to create constant buffer");

                // Map variable names -> (cbufferIndex, offset, size)
                // m_CBuffers[slot] corresponds to cbufferIndex = slot
                for (UINT v = 0; v < cbDesc.Variables; v++)
                {
                    auto* varRefl = cbRefl->GetVariableByIndex(v);
                    D3D11_SHADER_VARIABLE_DESC varDesc;
                    varRefl->GetDesc(&varDesc);

                    TraverseType(varRefl->GetType(),
                                 varDesc.Name,
                                 varDesc.StartOffset,
                                 slot,
                                 varDesc.Size);
                }
            }
        };

        ReflectStage(vsBlob);
        ReflectStage(psBlob);
    }

    // -----------------------------------------------------------------------
    // Uniform writing
    // -----------------------------------------------------------------------
    void D3D11Shader::WriteToShadow(const std::string& name, const void* data, uint32_t size)
    {
        auto it = m_UniformMap.find(name);
        if (it == m_UniformMap.end())
        {
            // Silently ignore unknowns (shader may not use every uniform)
            return;
        }
        const UniformInfo& info = it->second;
        auto& cb = m_CBuffers[info.cbufferIndex];
        BLU_CORE_ASSERT(info.byteOffset + size <= cb.shadow.size(),
            "Uniform '{0}' overflows constant buffer", name);
        memcpy(cb.shadow.data() + info.byteOffset, data, size);
        cb.dirty = true;
    }

    void D3D11Shader::SetUniformInt      (const std::string& name, int v)                { WriteToShadow(name, &v, 4); }
    void D3D11Shader::SetUniformFloat    (const std::string& name, float v)              { WriteToShadow(name, &v, 4); }
    void D3D11Shader::SetUniformFloat2   (const std::string& name, const glm::vec2& v)   { WriteToShadow(name, glm::value_ptr(v), 8); }
    void D3D11Shader::SetUniformFloat3   (const std::string& name, const glm::vec3& v)   { WriteToShadow(name, glm::value_ptr(v), 12); }
    void D3D11Shader::SetUniformFloat4   (const std::string& name, const glm::vec4& v)   { WriteToShadow(name, glm::value_ptr(v), 16); }

    void D3D11Shader::SetUniformMat3(const std::string& name, const glm::mat3& m)
    {
        // HLSL float3x3 is column-major by default (same as GLM).
        // Each column is padded to float4 → 3 columns × 16 bytes = 48 bytes.
        // Pack column c into padded[c*4 .. c*4+2]; slot c*4+3 stays 0.
        float padded[12] = {};
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 3; r++)
                padded[c * 4 + r] = m[c][r];  // m[col][row], column-major
        WriteToShadow(name, padded, 48);
    }

    void D3D11Shader::SetUniformMat4(const std::string& name, const glm::mat4& m)
    {
        // HLSL cbuffers default to column-major packing — same layout as GLM.
        // Upload raw: no transpose needed. Transposing would give HLSL M^T instead of M.
        WriteToShadow(name, glm::value_ptr(m), 64);
    }

    void D3D11Shader::SetUniformIntArray(const std::string& name, int* values, uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            std::string elemName = name + "[" + std::to_string(i) + "]";
            WriteToShadow(elemName, &values[i], 4);
        }
    }

    void D3D11Shader::SetUniformVec3Array(const std::string& name, const glm::vec3* values, uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            std::string elemName = name + "[" + std::to_string(i) + "]";
            WriteToShadow(elemName, glm::value_ptr(values[i]), 12);
        }
    }

    // -----------------------------------------------------------------------
    // Upload dirty cbuffers and bind shaders
    // -----------------------------------------------------------------------
    void D3D11Shader::UploadDirtyCBuffers() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        for (const auto& cb : m_CBuffers)
        {
            if (!cb.gpuBuffer) continue;
            if (cb.dirty)
            {
                dc->UpdateSubresource(cb.gpuBuffer.Get(), 0, nullptr,
                    cb.shadow.data(), 0, 0);
                // Mark clean
                const_cast<D3D11CBuffer&>(cb).dirty = false;
            }
            dc->VSSetConstantBuffers(cb.slot, 1, cb.gpuBuffer.GetAddressOf());
            dc->PSSetConstantBuffers(cb.slot, 1, cb.gpuBuffer.GetAddressOf());
        }
    }

    void D3D11Shader::Bind() const
    {
        BLU_CORE_ASSERT(m_VSBlob, "D3D11Shader::Bind — vertex shader was not compiled successfully");
        auto* dc  = D3D11Context::Get()->GetDeviceContext();
        auto* ctx = D3D11Context::Get();

        // Expose VS bytecode so D3D11VertexArray can create InputLayouts
        ctx->SetCurrentVSBytecode(
            m_VSBlob->GetBufferPointer(),
            m_VSBlob->GetBufferSize());

        dc->VSSetShader(m_VS.Get(), nullptr, 0);
        dc->PSSetShader(m_PS.Get(), nullptr, 0);

        UploadDirtyCBuffers();
    }

    void D3D11Shader::UnBind() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->VSSetShader(nullptr, nullptr, 0);
        dc->PSSetShader(nullptr, nullptr, 0);
        D3D11Context::Get()->SetCurrentVSBytecode(nullptr, 0);
    }
}
