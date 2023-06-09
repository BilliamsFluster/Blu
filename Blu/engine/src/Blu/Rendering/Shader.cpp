#include "Blupch.h"
#include "Shader.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"

namespace Blu
{
	Shader* Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return new OpenGLShader(vertexSrc, fragmentSrc);
		}
		}
		return nullptr;
	}
}
