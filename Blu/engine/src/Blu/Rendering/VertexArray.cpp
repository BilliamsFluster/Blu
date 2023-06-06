#include "Blupch.h"
#include "VertexArray.h"
#include "Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLVertexArray.h"

namespace Blu
{
	VertexArray* VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return new OpenGLVertexArray();
		}
		}
		return nullptr;
	}
}