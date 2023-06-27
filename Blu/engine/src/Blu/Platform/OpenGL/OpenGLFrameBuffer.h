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
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual ~OpenGLFrameBuffer();
		
		virtual void Bind() override;
		virtual void UnBind() override;
		

		void Invalidate();
	private:
		uint32_t m_RendererID;
		uint32_t m_ColorAttachment = 0, m_DepthAttachment = 0;
		FrameBufferSpecifications m_Specification;
	};
}
