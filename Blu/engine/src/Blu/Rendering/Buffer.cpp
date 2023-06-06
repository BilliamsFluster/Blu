#include "Blupch.h"
#include "Buffer.h"
#include "Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLBuffer.h"

namespace Blu
{
	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			{
			return nullptr;
			}
		case RendererAPI::API::OpenGL:
			{
			return new OpenGLIndexBuffer(indices, size);
			}
		}
		return nullptr;
	}
	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return new OpenGLVertexBuffer(vertices, size);
		}
		}
		return nullptr;
	}
}