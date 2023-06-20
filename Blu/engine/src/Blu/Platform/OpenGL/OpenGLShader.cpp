#include "Blupch.h"
#include "OpenGLShader.h"
#include <glad/glad.h>
#include <fstream>
#include "Blu/Core/Log.h"
#include <glm/gtc/type_ptr.hpp>

namespace Blu
{
	static GLenum  ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex") return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel") return GL_FRAGMENT_SHADER;

		BLU_CORE_ASSERT(false, "Unknown Shader Type");

		return 0;
	}
	OpenGLShader::OpenGLShader(const std::string& filepath)
	{
		BLU_PROFILE_FUNCTION();
		std::string src = ReadFile(filepath);
		auto shaderSources = PreProcess(src);
		Compile(shaderSources);

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.'); filepath.substr();

		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash: lastDot - lastSlash;

		m_Name = filepath.substr(lastSlash, count);


	}
	void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
	{
		BLU_PROFILE_FUNCTION();

		GLuint program = glCreateProgram();
		BLU_CORE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders");
		std::array < GLenum, 2> glShaderIDs;
		int glShaderIDIndex = 0;
		for (auto& kv : shaderSources)
		{
			GLenum type = kv.first;
			const std::string& source = kv.second;
			GLuint shader = glCreateShader(type);

			// Create an empty vertex shader handle
			

			// Send the vertex shader source code to GL
			// Note that std::string's .c_str is NULL character terminated.
			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);

			// Compile the vertex shader
			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				// The maxLength includes the NULL character
				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				// We don't need the shader anymore.
				glDeleteShader(shader);

				// Use the infoLog as you see fit.

				// In this simple program, we'll just leave
				break;
			}
			glAttachShader(program, shader);
			glShaderIDs[glShaderIDIndex++] = shader;
			

			
		}
		

		// Link our program
		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(program);
			
			for (auto id : glShaderIDs)
			{
				glDeleteShader(id);

			}
			
			

			// Use the infoLog as you see fit.

			BLU_CORE_ERROR("Fragment shader link failure");

			return;
		}

		for (auto id : glShaderIDs)
		{
			glDetachShader(program, id);

		}
		
		m_RendererID = program;

		
	}

	std::string OpenGLShader::ReadFile(const std::string& filepath)
	{
		BLU_PROFILE_FUNCTION();

		// Initialize an empty string to hold the file contents
		std::string result;

		// Open the file at the given filepath in read-only and binary mode
		std::ifstream in(filepath, std::ios::in | std::ios::binary);

		// If the file is successfully opened
		if (in)
		{
			// Seek to the end of the file
			in.seekg(0, std::ios::end);

			// Resize the result string to match the size of the file
			result.resize(in.tellg());

			// Seek back to the beginning of the file
			in.seekg(0, std::ios::beg);

			// Read the contents of the file into the result string
			in.read(&result[0], result.size());

			// Close the file
			in.close();
		}
		else
		{
			// If the file could not be opened, log an error message
			BLU_CORE_ERROR("Could not open file {0}", filepath);
		}

		// Return the file contents
		return result;
	}

	// Define the OpenGLShader class method PreProcess, which takes a string of source code as input
	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source)
	{
		BLU_PROFILE_FUNCTION();

		// Initialize an unordered map to hold the shader types and their corresponding source code
		std::unordered_map<GLenum, std::string> shaderSources;

		// Define the token used to indicate shader type in the source code
		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);

		// Start searching from the beginning of the source code
		size_t pos = source.find(typeToken, 0);

		// Loop through the source code until no more type tokens are found
		while (pos != std::string::npos)
		{
			// Find the end of the line where the type token is located
			size_t eol = source.find_first_of("\r\n", pos);
			// Assert that end of line is found after the type token
			BLU_CORE_ASSERT(eol != std::string::npos, "Syntax error");

			// Find the start of the shader type string
			size_t begin = pos + typeTokenLength + 1;

			// Extract the shader type string
			std::string type = source.substr(begin, eol - begin);

			// Assert that the shader type is valid
			BLU_CORE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

			// Find the start of the next line after the shader type declaration
			size_t nextLinePos = source.find_first_not_of("\r\n", eol);

			// Find the position of the next type token after the current shader declaration
			pos = source.find(typeToken, nextLinePos);

			// Extract the shader source code and map it to the corresponding shader type
			shaderSources[ShaderTypeFromString(type)] = source.substr
			(nextLinePos, pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
		}
		return shaderSources;
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		:m_Name(name)
	{
		BLU_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;
		Compile(sources);


	}
	
	OpenGLShader::~OpenGLShader()
	{
		BLU_PROFILE_FUNCTION();

		glDeleteProgram(m_RendererID);
	}
	void OpenGLShader::Bind() const
	{
		BLU_PROFILE_FUNCTION();

		glUseProgram(m_RendererID);
	}
	void OpenGLShader::UnBind() const
	{
		BLU_PROFILE_FUNCTION();

		glUseProgram(0);
	}
	void OpenGLShader::SetUniformInt(const std::string& name, int value)
	{
		GLint location = GetUniformLocation(name);
		glUniform1i(location, value);
	}
	void OpenGLShader::SetUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		GLint location = GetUniformLocation(name);
		glUniform2f(location, value.x, value.y);
	}
	void OpenGLShader::SetUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		GLint location = GetUniformLocation(name);
		glUniform3f(location, value.x, value.y, value.z);
	}
	void OpenGLShader::SetUniformFloat4(const std::string& name, const glm::vec4& color)
	{
		GLint location = GetUniformLocation(name);
		glUniform4f(location, color.x, color.y, color.z, color.w);

	}

	void OpenGLShader::SetUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		GLint location = GetUniformLocation(name);
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::SetUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		GLint location = GetUniformLocation(name);
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}


	int OpenGLShader::GetUniformLocation(const std::string& name)
	{
		if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
			return m_UniformLocationCache[name];

		int location = glGetUniformLocation(m_RendererID, name.c_str());
		if (location == -1)
			std::cout << "Warning: uniform '" << name << "' doesn't exist!" << std::endl;

		m_UniformLocationCache[name] = location;
		return location;
	}

	

}