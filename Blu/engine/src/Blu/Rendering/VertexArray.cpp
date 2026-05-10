#include "Blupch.h"
#include "VertexArray.h"
#include "Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLVertexArray.h"
#include "Blu/Platform/DirectX11/D3D11VertexArray.h"

namespace Blu
{
	Shared<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLVertexArray>();
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11VertexArray>();
		}
		return nullptr;
	}
}