#include "Blupch.h"
#include "Shader.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"

namespace Blu
{
	Shared<Shader> Shader::Create(const std::string& filepath)
	{

		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLShader>(filepath);
		}
		}
		return nullptr;
	}
	Shared<Shader> Shader::Create(const std::string& name,const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLShader>(name,vertexSrc, fragmentSrc);
		}
		}
		return nullptr;
	}

	void ShaderLibrary::Add(const std::string& name, const Shared<Shader>& shader)
	{
		BLU_CORE_ASSERT(mShaders.find(name) == m_Shaders.end(), "Shader already exists!");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Add(const Shared<Shader>& shader)
	{
		auto& name = shader->GetName();
		Add(name, shader);
		
	}
	
	Shared<Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}
	Shared<Shader> ShaderLibrary::Load(std::string name, const std::string& filepath)
	{
		return Shared<Shader>();
	}
	Shared<Shader> ShaderLibrary::Get(const std::string& name)
	{
		BLU_CORE_ASSERT(mShaders.find(name) != m_Shaders.end(), "Shader not found!");

		return m_Shaders[name];
	}
}
