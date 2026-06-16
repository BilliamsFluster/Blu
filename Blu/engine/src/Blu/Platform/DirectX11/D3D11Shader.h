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
        bool Reload() override; // re-read m_Filepath + recompile (keeps old shader on failure)

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
        void SetUniformSpotlight       (const std::string&, const struct SpotLightComponent&)    override {}

        // Upload a whole cbuffer shadow blob by cbuffer name (not uniform name).
        void SetUniformBuffer(const std::string& cbufferName, const void* data, uint32_t size) override;

        void DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;

    private:
        void LoadFromFile(); // (re)read m_Filepath, split #type sections, compile
        void Compile(const std::string& vertexSrc, const std::string& pixelSrc,
                     const std::string& geometrySrc = {},
                     const std::string& hullSrc = {},
                     const std::string& domainSrc = {});
        void CompileCompute(const std::string& src);
        bool CompileStage(const std::string& src, const char* target, ID3DBlob** outBlob);
        void TraverseType(ID3D11ShaderReflectionType* type,
                          const std::string& baseName,
                          uint32_t baseOffset,
                          uint32_t cbufferIndex,
                          uint32_t totalSize);
        void ReflectConstantBuffers(ID3DBlob* blob);

        void WriteToShadow(const std::string& name, const void* data, uint32_t size);
        void BindConstantBuffers()  const; // called from Bind(): maps all buffer slots once
        void UploadDirtyCBuffers()  const; // called from Flush(): only UpdateSubresource dirty ones

        std::string m_Name;
        std::string m_Filepath;

        Microsoft::WRL::ComPtr<ID3D11VertexShader>   m_VS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_PS;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_GS;
        Microsoft::WRL::ComPtr<ID3D11HullShader>     m_HS;
        Microsoft::WRL::ComPtr<ID3D11DomainShader>   m_DS;
        Microsoft::WRL::ComPtr<ID3D11ComputeShader>  m_CS;

        Microsoft::WRL::ComPtr<ID3DBlob> m_VSBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_GSBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_HSBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_DSBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_CSBlob;

        // Constant buffers indexed by their natural order (slot order from reflection)
        std::vector<D3D11CBuffer>                    m_CBuffers;
        std::unordered_map<std::string, UniformInfo> m_UniformMap;
        std::unordered_map<std::string, uint32_t>    m_CBufferNameMap;  // cbuffer name → slot index
    };
}
