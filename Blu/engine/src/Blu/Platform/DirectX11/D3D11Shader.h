#pragma once
#include "Blu/Rendering/Shader.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>

namespace Blu
{
    // -----------------------------------------------------------------------
    // Constant-buffer management
    // The HLSL cbuffer slot index (b0, b1 ...) is used as the key.
    // All SetUniform* calls write into CPU shadow memory; Bind() uploads dirty
    // buffers to the GPU and binds them to both VS and PS.
    // -----------------------------------------------------------------------
    struct D3D11CBuffer
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> gpuBuffer;
        std::vector<uint8_t>                 shadow;      // CPU-side mirror
        bool                                 dirty = false;
        uint32_t                             slot  = 0;
    };

    struct UniformInfo
    {
        uint32_t cbufferIndex = 0;  // Which D3D11CBuffer in m_CBuffers
        uint32_t byteOffset   = 0;
        uint32_t byteSize     = 0;
    };

    class D3D11Shader : public Shader
    {
    public:
        // Load from a single file with #type vertex / #type pixel sections
        explicit D3D11Shader(const std::string& filepath);
        // Compile from source strings
        D3D11Shader(const std::string& name,
                    const std::string& vertexSrc,
                    const std::string& pixelSrc);
        ~D3D11Shader() = default;

        // --- Shader interface ---
        void Bind()   const override;
        void UnBind() const override;
        void Flush()  const override { UploadDirtyCBuffers(); }

        const std::string& GetName()      const override { return m_Name; }
        uint32_t           GetProgramID()       override { return 0; } // N/A for DX11

        // --- Uniform setters (all write into shadow cbuffer, Bind uploads) ---
        void SetUniformInt      (const std::string& name, int value)                override;
        void SetUniformIntArray (const std::string& name, int* values, uint32_t n)  override;
        void SetUniformFloat    (const std::string& name, float value)              override;
        void SetUniformFloat2   (const std::string& name, const glm::vec2& value)   override;
        void SetUniformFloat3   (const std::string& name, const glm::vec3& value)   override;
        void SetUniformFloat4   (const std::string& name, const glm::vec4& value)   override;
        void SetUniformMat3     (const std::string& name, const glm::mat3& matrix)  override;
        void SetUniformMat4     (const std::string& name, const glm::mat4& matrix)  override;
        void SetUniformVec3Array(const std::string& name, const glm::vec3* values, uint32_t n) override;

        // Not used by 3D/2D renderers — no-op stubs
        void SetUniformPointLight      (const std::string&, const struct PointLightComponent&)   override {}
        void SetUniformDirectionalLight(const std::string&, const struct DirectionalLightComponent&) override {}
        void SetUniformSpotlight       (const std::string&, const struct SpotlightComponent&)    override {}

    private:
        void Compile(const std::string& vertexSrc, const std::string& pixelSrc);
        void Reflect(ID3DBlob* vsBlob, ID3DBlob* psBlob);
        void TraverseType(ID3D11ShaderReflectionType* type,
                          const std::string& baseName,
                          uint32_t baseOffset,
                          uint32_t cbufferIndex,
                          uint32_t totalSize);

        void WriteToShadow(const std::string& name, const void* data, uint32_t size);
        void UploadDirtyCBuffers() const;

        std::string m_Name;
        std::string m_Filepath;

        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;

        // VS bytecode stored so D3D11VertexArray can create InputLayouts
        Microsoft::WRL::ComPtr<ID3DBlob> m_VSBlob;

        // Constant buffers indexed by their natural order (slot order from reflection)
        std::vector<D3D11CBuffer>                    m_CBuffers;
        std::unordered_map<std::string, UniformInfo> m_UniformMap;
    };
}
