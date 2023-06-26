#pragma once
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/FrameBuffer.h"

namespace Blu
{
	class OpenGLFrameBuffer : public FrameBuffer
	{
	public:
		OpenGLFrameBuffer(const FrameBufferSpecifications& spec);
		virtual const FrameBufferSpecifications& GetSpecification() const override { return m_Specification; }
		virtual uint32_t GetColorAttachment() const override { return m_ColorAttachment; }

		virtual ~OpenGLFrameBuffer();
		
		virtual void Bind() override;
		virtual void UnBind() override;
		

		void Invalidate();
	private:
		uint32_t m_RendererID;
		uint32_t m_ColorAttachment, m_DepthAttachment;
		FrameBufferSpecifications m_Specification;
	};
}
