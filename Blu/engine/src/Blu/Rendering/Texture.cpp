#include "Blupch.h"
#include "Texture.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLTexture.h"
#include "Blu/Platform/DirectX11/D3D11Texture.h"
#include <memory>

namespace Blu
{
	Shared<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLTexture2D>(path);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11Texture2D>(path);
		}
		return nullptr;
	}

	Shared<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLTexture2D>(width, height);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11Texture2D>(width, height);
		}
		return nullptr;
	}
}

