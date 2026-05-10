#include "Blupch.h"
#include "Buffer.h"
#include "Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLBuffer.h"
#include "Blu/Platform/DirectX11/D3D11Buffer.h"

namespace Blu
{
	Shared<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    return nullptr;
		case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLIndexBuffer>(indices, size);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11IndexBuffer>(indices, size);
		}
		return nullptr;
	}

	Shared<VertexBuffer> VertexBuffer::Create(uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    return nullptr;
		case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLVertexBuffer>(size);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11VertexBuffer>(size);
		}
		return nullptr;
	}

	Shared<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    return nullptr;
		case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLVertexBuffer>(vertices, size);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11VertexBuffer>(vertices, size);
		}
		return nullptr;
	}
}