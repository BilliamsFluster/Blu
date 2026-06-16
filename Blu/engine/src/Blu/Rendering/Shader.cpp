#include "Blupch.h"
#include "Shader.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"
#include "Blu/Platform/DirectX11/D3D11Shader.h"
#include "Blu/Core/Log.h"

#include <filesystem>
#include <utility>
#include <vector>

namespace Blu
{
	namespace
	{
		// Shaders created from a file, tracked (by source file name) so the editor can hot-reload
		// them. Weak refs so tracking never keeps a shader alive; dead entries are pruned on reload.
		std::vector<std::pair<std::string, std::weak_ptr<Shader>>>& TrackedShaders()
		{
			static std::vector<std::pair<std::string, std::weak_ptr<Shader>>> s_Tracked;
			return s_Tracked;
		}
		std::string FileNameOf(const std::string& path)
		{
			return std::filesystem::path(path).filename().string();
		}
	}

	Shared<Shader> Shader::Create(const std::string& filepath)
	{
		Shared<Shader> shader;
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   shader = std::make_shared<OpenGLShader>(filepath); break;
		case RendererAPI::API::Direct3D: shader = std::make_shared<D3D11Shader>(filepath); break;
		}
		if (shader)
			TrackedShaders().push_back({ FileNameOf(filepath), shader });
		return shader;
	}

	int Shader::ReloadFile(const std::string& filepath)
	{
		const std::string target = FileNameOf(filepath);
		int reloaded = 0;
		auto& tracked = TrackedShaders();
		for (auto it = tracked.begin(); it != tracked.end(); )
		{
			if (Shared<Shader> shader = it->second.lock())
			{
				if (it->first == target && shader->Reload())
					++reloaded;
				++it;
			}
			else
			{
				it = tracked.erase(it); // prune expired
			}
		}
		BLU_CORE_INFO("Shader::ReloadFile('{0}') reloaded {1} shader(s)", target, reloaded);
		return reloaded;
	}

	Shared<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11Shader>(name, vertexSrc, fragmentSrc);
		}
		return nullptr;
	}

	void ShaderLibrary::Add(const std::string& name, const Shared<Shader>& shader)
	{
		BLU_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end(), "Shader already exists!");
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
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}
	Shared<Shader> ShaderLibrary::Get(const std::string& name)
	{
		BLU_CORE_ASSERT(m_Shaders.find(name) != m_Shaders.end(), "Shader not found!");

		return m_Shaders[name];
	}
}
