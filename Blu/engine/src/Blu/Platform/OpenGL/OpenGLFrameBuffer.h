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
		virtual uint32_t GetColorAttachmentID(uint32_t index = 0) const override { BLU_CORE_ASSERT(index < m_ColorAttachments.size()); return m_ColorAttachments[index]; }
		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;

		virtual ~OpenGLFrameBuffer() override;
		
		virtual void Bind() override;
		virtual void UnBind() override;
		

		void Invalidate();
	private:
		uint32_t m_RendererID;
		//uint32_t m_ColorAttachment = 0, m_DepthAttachment = 0;
		FrameBufferSpecifications m_Specification;

		std::vector<FrameBufferTextureSpecifications> m_ColorAttachmentSpecs;
		FrameBufferTextureSpecifications m_DepthAttachmentSpecs;
		
		uint32_t m_DepthAttachment;
		std::vector<uint32_t> m_ColorAttachments;
		std::vector<uint32_t> m_DepthAttachments;
	};
}
