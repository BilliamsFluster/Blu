#pragma once
#include "Blu/Rendering/Buffer.h"
#include <memory>
#include "Blu/Core/Core.h"

namespace Blu
{
	class VertexArray
	{
	public:
		virtual ~VertexArray() {}

		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;

		virtual void  AddVertexBuffer(const Shared<VertexBuffer>& vertexBuffer)  = 0;
		virtual void  AddIndexBuffer(const Shared<IndexBuffer>& vertexBuffer)  = 0;
		virtual const std::vector<Shared<VertexBuffer>>& GetVertexBuffers() const = 0;
		virtual const Shared<IndexBuffer>& GetIndexBuffer() const = 0;


		static VertexArray* Create();
	};

}