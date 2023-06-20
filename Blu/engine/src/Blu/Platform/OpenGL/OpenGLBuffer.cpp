#include "Blupch.h"
#include "OpenGLBuffer.h"
#include <glad/glad.h>
namespace Blu
{
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
	{
		BLU_PROFILE_FUNCTION();

		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}
	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		BLU_PROFILE_FUNCTION();

		glDeleteBuffers(1, &m_RendererID);
	}
	void OpenGLVertexBuffer::Bind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}
	void OpenGLVertexBuffer::UnBind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindBuffer(GL_ARRAY_BUFFER, 0); // unbinds buffer
	}


	//--------------IndexBuffer-----------------------------------------------


	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count)
		:m_Count(count)
	{
		BLU_PROFILE_FUNCTION();

		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}
	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		BLU_PROFILE_FUNCTION();

		glDeleteBuffers(1, &m_RendererID);

	}
	void OpenGLIndexBuffer::Bind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);

	}
	void OpenGLIndexBuffer::UnBind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	}
}