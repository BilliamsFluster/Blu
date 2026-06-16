#pragma once
#include "Blu/Core/Core.h"
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Blu
{
	class Shader
	{
	public:

		virtual ~Shader() = default;

		static Shared<Shader> Create(const std::string& filepath);
		static Shared<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		
		virtual void SetUniformInt(const std::string& name, int value) = 0;

		virtual void SetUniformIntArray(const std::string& name, int* values, uint32_t count) = 0;
		virtual void SetUniformVec3Array(const std::string& name, const glm::vec3* values, uint32_t count) = 0;

		virtual void SetUniformFloat(const std::string& name, float value) = 0;
		virtual void SetUniformFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void SetUniformFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetUniformFloat4(const std::string& name, const glm::vec4& color) = 0;
		virtual void SetUniformMat3(const std::string & name, const glm::mat3 & matrix) = 0;
		virtual void SetUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;

		// Fast-path uniform writes: resolve a uniform's handle ONCE via GetUniformHandle, then write
		// by handle every draw to skip the per-call std::string hash + unordered_map lookup (the
		// dominant per-draw CPU cost, especially in Debug builds). GetUniformHandle returns < 0 for an
		// unknown uniform or a backend that doesn't implement it; the handle setters no-op on < 0, so
		// callers fall back to the string overloads. Default impls keep non-DX11 backends working.
		virtual int32_t GetUniformHandle(const std::string& name) { return -1; }
		virtual void SetUniformMat4(int32_t handle, const glm::mat4& matrix) {}
		virtual void SetUniformMat3(int32_t handle, const glm::mat3& matrix) {}
		virtual void SetUniformInt (int32_t handle, int value) {}
		
		virtual void SetUniformPointLight(const std::string& name, const struct PointLightComponent& light) = 0;
		virtual void SetUniformDirectionalLight(const std::string& name, const struct DirectionalLightComponent& light) = 0;
		virtual void SetUniformSpotlight(const std::string& name, const struct SpotLightComponent& light) = 0;

		// Bulk-upload a constant buffer by name (D3D11: uploads entire cbuffer shadow;
		// OpenGL: no-op since GL doesn't map cbuffers the same way).
		virtual void SetUniformBuffer(const std::string& cbufferName, const void* data, uint32_t size) {}

        virtual void Bind()   const = 0;
        virtual void UnBind() const = 0;
        // Flush any pending uniform data to the GPU (no-op for OpenGL)
        virtual void Flush()  const {}

        // Re-read this shader's source file and recompile in place. Returns false if unsupported
        // (e.g. source-string shaders). A failed recompile keeps the previously-working shader, so
        // it's safe to call after an in-editor edit even if the new source has errors.
        virtual bool Reload() { return false; }

        // Live shader editing: every shader created via Create(filepath) is tracked by its source
        // file. ReloadFile recompiles all tracked shaders whose source matches `filepath` (matched by
        // file name) and returns how many were reloaded — the hook the in-editor HLSL editor calls on
        // save so changes take effect without restarting.
        static int ReloadFile(const std::string& filepath);

        // Compute dispatch (no-op base — only implemented by D3D11).
        // Call Bind() first to set up the compute shader and cbuffers.
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {}
		virtual const std::string& GetName() const = 0;
		virtual uint32_t GetProgramID() = 0;

	};
	class ShaderLibrary
	{
	public:
		void Add(const Shared<Shader>& shader);
		void Add(const std::string& name, const Shared<Shader>& shader);
		Shared<Shader>Load(const std::string& filepath);
		Shared<Shader>Load(std::string name, const std::string& filepath);
		Shared<Shader> Get(const std::string& name);


	private:
		std::unordered_map<std::string, Shared<Shader>> m_Shaders;
		friend class LightManager;
	};
}


