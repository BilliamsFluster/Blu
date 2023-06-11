#pragma once
#include "Blu/Rendering/VertexArray.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class OpenGLVertexArray : public VertexArray
	{
	public:
		OpenGLVertexArray();
		virtual ~OpenGLVertexArray();

		virtual void Bind() const override;
		virtual void UnBind() const override;

		virtual void  AddVertexBuffer(const Shared<VertexBuffer>& vertexBuffer) override;
		virtual void  AddIndexBuffer(const Shared<IndexBuffer>& indexBuffer) override;

		virtual const std::vector<Shared<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; } 
		virtual const Shared<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
	private:
		std::vector<Shared<VertexBuffer>> m_VertexBuffers;
		Shared<IndexBuffer> m_IndexBuffer;
		uint32_t m_RendererID;


	};
}