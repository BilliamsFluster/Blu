#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{
	struct FrameBufferSpecifications
	{
		uint32_t Width, Height;
		uint32_t Samples = 1;
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
		virtual uint32_t GetColorAttachment() const = 0;

		static Shared<FrameBuffer> Create(const FrameBufferSpecifications& specs);
	};
}

