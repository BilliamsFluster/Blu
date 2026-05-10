#include "Blupch.h"
#include "RenderCommand.h"
#include "Blu/Platform/OpenGL/OpenGLRendererAPI.h"
#include "Blu/Platform/DirectX11/D3D11RendererAPI.h"

namespace Blu
{
	// Change RendererAPI::s_API before this translation unit is loaded to switch backends.
	static RendererAPI* CreateRendererAPI()
	{
		switch (RendererAPI::GetAPI())
		{
		case RendererAPI::API::OpenGL:   return new OpenGLRendererAPI;
		case RendererAPI::API::Direct3D: return new D3D11RendererAPI;
		default:                         return nullptr;
		}
	}

	RendererAPI* RenderCommand::s_RendererAPI = CreateRendererAPI();
}