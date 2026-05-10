#include "Blupch.h"
#include "FrameBuffer.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLFrameBuffer.h"
#include "Blu/Platform/DirectX11/D3D11FrameBuffer.h"

namespace Blu
{
	Shared<FrameBuffer> FrameBuffer::Create(const FrameBufferSpecifications& specs)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:     return nullptr;
		case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLFrameBuffer>(specs);
		case RendererAPI::API::Direct3D: return std::make_shared<D3D11FrameBuffer>(specs);
		}
		return nullptr;
	}
}
