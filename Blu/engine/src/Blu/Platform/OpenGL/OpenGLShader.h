#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Blu/Rendering/Shader.h"
 
typedef unsigned int GLenum;
namespace Blu
{
	class OpenGLShader: public Shader
	{
	public:
		OpenGLShader(const std::string&filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual const std::string& GetName() const override { return m_Name; }

		~OpenGLShader() override;

		void Bind() const override;
		void UnBind() const override;
		void SetUniformInt(const std::string& name, int value);
		void SetUniformFloat(const std::string& name, float value);
		void SetUniformFloat2(const std::string& name, const glm::vec2& value);
		void SetUniformFloat3(const std::string& name, const glm::vec3& value);
		void SetUniformFloat4(const std::string& name, const glm::vec4& color);

		void SetUniformMat3(const std::string& name, const glm::mat3& matrix);
		void SetUniformMat4(const std::string& name, const glm::mat4& matrix);
		int GetUniformLocation(const std::string& name);

	private:
		void Compile(const std::unordered_map<GLenum, std::string>& shaderSources);
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);
	private:
		uint32_t m_RendererID;
		std::string m_Name;
		std::unordered_map<std::string, int> m_UniformLocationCache;
		


	};
}