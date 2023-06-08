#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
 

namespace Blu
{
	class OpenGLShader
	{
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader();

		void Bind() const;
		void UnBind() const;
		void SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
		int GetUniformLocation(const std::string& name);


	private:
		uint32_t m_RendererID;
		std::unordered_map<std::string, int> m_UniformLocationCache;


	};
}