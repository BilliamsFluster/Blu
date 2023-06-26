#include "Blupch.h"
#include "FrameBuffer.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLFrameBuffer.h"

namespace Blu
{
	Shared<FrameBuffer> FrameBuffer::Create(const FrameBufferSpecifications& specs)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			return nullptr;
		}
		case RendererAPI::API::OpenGL:
		{
			return std::make_shared<OpenGLFrameBuffer>(specs);
		}
		}
		return nullptr;
	}

}
