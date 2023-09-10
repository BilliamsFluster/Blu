#pragma once
#include "Blu/Core/Core.h"
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
		virtual void SetUniformFloat(const std::string& name, float value) = 0;
		virtual void SetUniformFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void SetUniformFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetUniformFloat4(const std::string& name, const glm::vec4& color) = 0;
		virtual void SetUniformMat3(const std::string & name, const glm::mat3 & matrix) = 0;
		virtual void SetUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;
		
		virtual void SetUniformPointLight(const std::string& name, const struct PointLightComponent& light) = 0;
		virtual void SetUniformDirectionalLight(const std::string& name, const struct DirectionalLightComponent& light) = 0;
		virtual void SetUniformSpotlight(const std::string& name, const struct SpotlightComponent& light) = 0;
	
		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;
		virtual const std::string& GetName() const = 0;

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


