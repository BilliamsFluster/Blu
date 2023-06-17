#include "Blupch.h"
#include "Buffer.h"
#include "Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLBuffer.h"

namespace Blu
{
	Shared<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			{
			return nullptr;
			}
		case RendererAPI::API::OpenGL:
			{
			return std::make_shared<OpenGLIndexBuffer>(indices, size);
			}
		}
		return nullptr;
	}
	Shared<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLVertexBuffer>(vertices, size);
		}
		}
		return nullptr;
	}
}