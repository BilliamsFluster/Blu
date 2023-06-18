#include "Blupch.h"
#include "Texture.h"
#include "Blu/Rendering/Renderer.h"
#include"Blu/Platform/OpenGL/OpenGLTexture.h"
#include <memory>
namespace Blu
{
	Shared<Texture2D> Blu::Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLTexture2D>(path);
			
		}
		}
		return nullptr;
	}
	Shared<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLTexture2D>(width, height);

		}
		}
		return nullptr;
	}
}

