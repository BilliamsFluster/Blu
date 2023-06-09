#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Blu/Rendering/Shader.h"
 

namespace Blu
{
	class OpenGLShader: public Shader
	{
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader() override;

		void Bind() const override;
		void UnBind() const override;
		void SetUniformInt(const std::string& name, int value);
		void SetUniformFloat2(const std::string& name, const glm::vec2& value);
		void SetUniformFloat3(const std::string& name, const glm::vec3& value);
		void SetUniformFloat4(const std::string& name, float v0, float v1, float v2, float v3);

		void SetUniformMat3(const std::string& name, const glm::mat3& matrix);
		void SetUniformMat4(const std::string& name, const glm::mat4& matrix);
		int GetUniformLocation(const std::string& name);


	private:
		uint32_t m_RendererID;
		std::unordered_map<std::string, int> m_UniformLocationCache;


	};
}