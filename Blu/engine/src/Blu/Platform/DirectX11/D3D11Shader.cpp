#include "Blupch.h"
#include "D3D11Shader.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"
#include "Blu/Utils/FileSystemService.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
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
        LoadFromFile();
    }

    void D3D11Shader::LoadFromFile()
    {
        auto src = SplitShaderSource(ReadFile(m_Filepath));

        bool hasCS = src.count("compute") > 0;
        if (hasCS)
        {
            CompileCompute(src["compute"]);
            return;
        }

        // Soft failure (not an assert): a malformed source — e.g. a mid-edit save from the in-editor
        // shader editor — logs and bails, leaving the previously-compiled shader objects intact.
        if (!(src.count("vertex") && (src.count("pixel") || src.count("fragment"))))
        {
            BLU_CORE_ERROR("HLSL shader '{0}' must have #type vertex and #type pixel (#type compute "
                "for compute-only); keeping previous shader", m_Name);
            return;
        }
        std::string& psSrc = src.count("pixel") ? src["pixel"] : src["fragment"];
        std::string gsSrc, hsSrc, dsSrc;
        if (src.count("geometry")) gsSrc = src["geometry"];
        if (src.count("hull"))     hsSrc = src["hull"];
        if (src.count("domain"))   dsSrc = src["domain"];
        Compile(src["vertex"], psSrc, gsSrc, hsSrc, dsSrc);
    }

    bool D3D11Shader::Reload()
    {
        if (m_Filepath.empty())
            return false;
        LoadFromFile();
        return true;
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
    // ── Internal compile helper (lambda extracted from Compile) ──────────────
    // Content-addressed compiled-shader cache path under cache://shadercache/. The key hashes the
    // stage source + target + compile flags, so any source edit (or flag/format change) misses the
    // cache and recompiles — there is no staleness window. Returns empty if no cache mount exists,
    // in which case caching is skipped and the shader simply compiles as before.
    static std::filesystem::path ShaderCachePathFor(const std::string& src, const char* target, UINT flags)
    {
        constexpr uint64_t kCacheFormatVersion = 1; // bump to invalidate every cached blob
        auto mix = [](uint64_t h, uint64_t v) {
            return h ^ (v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
        };
        uint64_t h = std::hash<std::string>{}(src);
        h = mix(h, std::hash<std::string>{}(std::string(target)));
        h = mix(h, ((uint64_t)flags << 1) ^ kCacheFormatVersion);

        auto& fs = FileSystemService::Get();
        const std::string virtualPath = "cache://shadercache/" + std::to_string(h) + ".cso";
        if (!fs.IsVirtualPath(virtualPath))
            return {};
        std::filesystem::path resolved = fs.Resolve(virtualPath);
        // Resolve returns the input unchanged when the scheme isn't mounted — treat that as no cache.
        if (resolved.empty() || fs.IsVirtualPath(resolved))
            return {};
        return resolved;
    }

    bool D3D11Shader::CompileStage(const std::string& src, const char* target,
                                   ID3DBlob** outBlob)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef BLU_DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        // Cache hit: load the previously-compiled bytecode instead of re-running D3DCompile.
        const std::filesystem::path cachePath = ShaderCachePathFor(src, target, flags);
        if (!cachePath.empty())
        {
            std::error_code ec;
            if (std::filesystem::exists(cachePath, ec))
            {
                Microsoft::WRL::ComPtr<ID3DBlob> cached;
                if (SUCCEEDED(D3DReadFileToBlob(cachePath.wstring().c_str(), cached.GetAddressOf())))
                {
                    *outBlob = cached.Detach();
                    return true;
                }
            }
        }

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

        // Persist the freshly compiled bytecode for next launch (best-effort).
        if (!cachePath.empty() && *outBlob)
        {
            std::error_code ec;
            std::filesystem::create_directories(cachePath.parent_path(), ec);
            D3DWriteBlobToFile(*outBlob, cachePath.wstring().c_str(), TRUE);
        }
        return true;
    }

    void D3D11Shader::Compile(
        const std::string& vertexSrc,
        const std::string& pixelSrc,
        const std::string& geometrySrc,
        const std::string& hullSrc,
        const std::string& domainSrc)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

        // Reset reflection + handle state so a reload/recompile rebuilds cleanly (the reflection
        // passes below APPEND, so without this a hot-reload would duplicate cbuffers / stale offsets).
        m_CBuffers.clear();
        m_UniformMap.clear();
        m_CBufferNameMap.clear();
        m_HandleCache.clear();
        m_HandleByName.clear();

        // Vertex
        if (!CompileStage(vertexSrc, "vs_5_0", m_VSBlob.GetAddressOf())) return;
        dev->CreateVertexShader(m_VSBlob->GetBufferPointer(),
            m_VSBlob->GetBufferSize(), nullptr, m_VS.GetAddressOf());
        ReflectConstantBuffers(m_VSBlob.Get());

        // Pixel
        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
        if (!CompileStage(pixelSrc, "ps_5_0", psBlob.GetAddressOf())) return;
        dev->CreatePixelShader(psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(), nullptr, m_PS.GetAddressOf());
        ReflectConstantBuffers(psBlob.Get());

        // Geometry (optional)
        if (!geometrySrc.empty())
        {
            if (!CompileStage(geometrySrc, "gs_5_0", m_GSBlob.GetAddressOf())) return;
            dev->CreateGeometryShader(m_GSBlob->GetBufferPointer(),
                m_GSBlob->GetBufferSize(), nullptr, m_GS.GetAddressOf());
            ReflectConstantBuffers(m_GSBlob.Get());
        }

        // Hull (optional)
        if (!hullSrc.empty())
        {
            if (!CompileStage(hullSrc, "hs_5_0", m_HSBlob.GetAddressOf())) return;
            dev->CreateHullShader(m_HSBlob->GetBufferPointer(),
                m_HSBlob->GetBufferSize(), nullptr, m_HS.GetAddressOf());
            ReflectConstantBuffers(m_HSBlob.Get());
        }

        // Domain (optional)
        if (!domainSrc.empty())
        {
            if (!CompileStage(domainSrc, "ds_5_0", m_DSBlob.GetAddressOf())) return;
            dev->CreateDomainShader(m_DSBlob->GetBufferPointer(),
                m_DSBlob->GetBufferSize(), nullptr, m_DS.GetAddressOf());
            ReflectConstantBuffers(m_DSBlob.Get());
        }
    }

    void D3D11Shader::CompileCompute(const std::string& src)
    {
        auto* dev = D3D11Context::Get()->GetDevice();
        m_CBuffers.clear();
        m_UniformMap.clear();
        m_CBufferNameMap.clear();
        m_HandleCache.clear();
        m_HandleByName.clear();

        if (!CompileStage(src, "cs_5_0", m_CSBlob.GetAddressOf())) return;
        dev->CreateComputeShader(m_CSBlob->GetBufferPointer(),
            m_CSBlob->GetBufferSize(), nullptr, m_CS.GetAddressOf());
        ReflectConstantBuffers(m_CSBlob.Get());
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

    void D3D11Shader::ReflectConstantBuffers(ID3DBlob* blob)
    {
        auto* dev = D3D11Context::Get()->GetDevice();

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

            m_CBufferNameMap[cbDesc.Name] = slot;

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
    }

    // -----------------------------------------------------------------------
    // Bulk upload — write the entire shadow buffer of a named cbuffer
    // -----------------------------------------------------------------------
    void D3D11Shader::SetUniformBuffer(const std::string& cbufferName, const void* data, uint32_t size)
    {
        auto it = m_CBufferNameMap.find(cbufferName);
        if (it == m_CBufferNameMap.end())
            return;
        auto& cb = m_CBuffers[it->second];
        if (cb.shadow.size() < size)
            return;
        memcpy(cb.shadow.data(), data, size);
        cb.dirty = true;
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
        WriteToShadowInfo(it->second, data, size);
    }

    void D3D11Shader::WriteToShadowInfo(const UniformInfo& info, const void* data, uint32_t size)
    {
        auto& cb = m_CBuffers[info.cbufferIndex];
        if (info.byteOffset + size > cb.shadow.size())
            return;
        memcpy(cb.shadow.data() + info.byteOffset, data, size);
        cb.dirty = true;
    }

    // Resolve (and cache) a uniform handle. Called a handful of times per frame (not per draw), so
    // the one string lookup here is amortized; the per-draw setters below skip it entirely.
    int32_t D3D11Shader::GetUniformHandle(const std::string& name)
    {
        auto cached = m_HandleByName.find(name);
        if (cached != m_HandleByName.end())
            return cached->second;
        int32_t handle = -1;
        auto it = m_UniformMap.find(name);
        if (it != m_UniformMap.end())
        {
            handle = (int32_t)m_HandleCache.size();
            m_HandleCache.push_back(it->second);
        }
        m_HandleByName[name] = handle; // cache misses (-1) too, so repeats don't re-query
        return handle;
    }

    void D3D11Shader::SetUniformMat4(int32_t handle, const glm::mat4& m)
    {
        if (handle < 0 || handle >= (int32_t)m_HandleCache.size()) return;
        WriteToShadowInfo(m_HandleCache[handle], glm::value_ptr(m), 64);
    }

    void D3D11Shader::SetUniformInt(int32_t handle, int v)
    {
        if (handle < 0 || handle >= (int32_t)m_HandleCache.size()) return;
        WriteToShadowInfo(m_HandleCache[handle], &v, 4);
    }

    void D3D11Shader::SetUniformMat3(int32_t handle, const glm::mat3& m)
    {
        if (handle < 0 || handle >= (int32_t)m_HandleCache.size()) return;
        float padded[12] = {}; // float3x3 → 3 columns padded to float4 (48 bytes), matching the string path
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 3; r++)
                padded[c * 4 + r] = m[c][r];
        WriteToShadowInfo(m_HandleCache[handle], padded, 48);
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
    // Cbuffer management
    // BindConstantBuffers — called once from Bind().  Maps every slot to the
    // engine's GPU buffer so subsequent UpdateSubresource calls in Flush()
    // are immediately visible to the pipeline without re-binding.
    // -----------------------------------------------------------------------
    void D3D11Shader::BindConstantBuffers() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        for (const auto& cb : m_CBuffers)
        {
            if (!cb.gpuBuffer) continue;
            ID3D11Buffer* buf = cb.gpuBuffer.Get();
            dc->VSSetConstantBuffers(cb.slot, 1, &buf);
            dc->PSSetConstantBuffers(cb.slot, 1, &buf);
            if (m_GS) dc->GSSetConstantBuffers(cb.slot, 1, &buf);
            if (m_HS) dc->HSSetConstantBuffers(cb.slot, 1, &buf);
            if (m_DS) dc->DSSetConstantBuffers(cb.slot, 1, &buf);
            if (m_CS) dc->CSSetConstantBuffers(cb.slot, 1, &buf);
        }
    }

    // UploadDirtyCBuffers — called from Flush().  Only uploads data for dirty
    // cbuffers; no SetConstantBuffers needed since Bind() already mapped the
    // slots and D3D11 reads from the buffer object at draw time.
    void D3D11Shader::UploadDirtyCBuffers() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        for (const auto& cb : m_CBuffers)
        {
            if (!cb.gpuBuffer || !cb.dirty) continue;
            dc->UpdateSubresource(cb.gpuBuffer.Get(), 0, nullptr,
                cb.shadow.data(), 0, 0);
            const_cast<D3D11CBuffer&>(cb).dirty = false;
        }
    }

    void D3D11Shader::Bind() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();

        // Compute-only path
        if (!m_VS && m_CS)
        {
            dc->CSSetShader(m_CS.Get(), nullptr, 0);
            BindConstantBuffers();
            UploadDirtyCBuffers();
            return;
        }

        if (!m_VSBlob)
        {
            BLU_CORE_WARN("D3D11Shader::Bind — vertex shader not compiled for '{0}', skipping", m_Name);
            return;
        }
        D3D11Context::Get()->SetCurrentVSBytecode(
            m_VSBlob->GetBufferPointer(),
            m_VSBlob->GetBufferSize());

        dc->VSSetShader(m_VS.Get(), nullptr, 0);
        dc->PSSetShader(m_PS.Get(), nullptr, 0);
        if (m_GS) dc->GSSetShader(m_GS.Get(), nullptr, 0);
        if (m_HS) dc->HSSetShader(m_HS.Get(), nullptr, 0);
        if (m_DS) dc->DSSetShader(m_DS.Get(), nullptr, 0);

        // Bind all cbuffer slots first, then push any data that was marked
        // dirty before this Bind() call (e.g. from SetLights / BindShadowMap).
        BindConstantBuffers();
        UploadDirtyCBuffers();
    }

    void D3D11Shader::UnBind() const
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        if (!m_VS && m_CS)
        {
            dc->CSSetShader(nullptr, nullptr, 0);
            return;
        }
        dc->VSSetShader(nullptr, nullptr, 0);
        dc->PSSetShader(nullptr, nullptr, 0);
        if (m_GS) dc->GSSetShader(nullptr, nullptr, 0);
        if (m_HS) dc->HSSetShader(nullptr, nullptr, 0);
        if (m_DS) dc->DSSetShader(nullptr, nullptr, 0);
        D3D11Context::Get()->SetCurrentVSBytecode(nullptr, 0);
    }

    void D3D11Shader::DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        auto* dc = D3D11Context::Get()->GetDeviceContext();
        dc->Dispatch(groupsX, groupsY, groupsZ);
    }
}
