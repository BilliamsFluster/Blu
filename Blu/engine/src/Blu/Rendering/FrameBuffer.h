#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{
	enum class FrameBufferTextureFormat
	{
		None = 0,

		RGBA8,
		RED_INTEGER,

		DEPTH24STENCIL8,

		Depth = DEPTH24STENCIL8
	};
	struct FrameBufferTextureSpecifications
	{
		FrameBufferTextureSpecifications() = default;
		FrameBufferTextureSpecifications(FrameBufferTextureFormat format)
			: TextureFormat(format) {}

		FrameBufferTextureFormat TextureFormat = FrameBufferTextureFormat::None;
	};

	struct FrameBufferAttachmentSpecifications
	{
		FrameBufferAttachmentSpecifications() = default;
		FrameBufferAttachmentSpecifications(std::initializer_list<FrameBufferTextureSpecifications> attachments)
			:Attachments(attachments) {}

		std::vector<FrameBufferTextureSpecifications> Attachments;
	};
	struct FrameBufferSpecifications
	{
		uint32_t Width, Height;
		uint32_t Samples = 1;
		FrameBufferAttachmentSpecifications Attachments;
		bool SwapChainTarget = false;
	};
	class FrameBuffer
	{
	public:
		virtual const FrameBufferSpecifications& GetSpecification() const = 0;
		virtual ~FrameBuffer() = default;
		//virtual FrameBufferSpecifications& GetSpecification() = 0;
		virtual void Bind() = 0;
		virtual void UnBind() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;
		virtual uint32_t GetColorAttachmentID(uint32_t index = 0) const = 0;

		static Shared<FrameBuffer> Create(const FrameBufferSpecifications& specs);
	};
}

