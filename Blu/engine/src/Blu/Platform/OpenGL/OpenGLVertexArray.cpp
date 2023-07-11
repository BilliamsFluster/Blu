#include "Blupch.h"
#include "OpenGLVertexArray.h"
#include <glad/glad.h>

namespace Blu
{
	static GLenum ShaderDataTypeToOpenGlBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:		return	GL_FLOAT;
		case ShaderDataType::Float2:	return  GL_FLOAT;
		case ShaderDataType::Float3:	return  GL_FLOAT;
		case ShaderDataType::Float4:	return  GL_FLOAT;
		case ShaderDataType::Mat3:		return  GL_FLOAT;
		case ShaderDataType::Mat4:		return  GL_FLOAT;
		case ShaderDataType::Int:		return	GL_INT;
		case ShaderDataType::Int2:		return	GL_INT;
		case ShaderDataType::Int3:		return	GL_INT;
		case ShaderDataType::Int4:		return	GL_INT;
		case ShaderDataType::Bool:		return	GL_BOOL;
		}
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
		glCreateVertexArrays(1, &m_RendererID);
		glObjectLabel(GL_VERTEX_ARRAY, m_RendererID, -1, "Vertex Array"); // debug

	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		BLU_PROFILE_FUNCTION();
		GLchar label[128];
		GLsizei length;
		glGetObjectLabel(GL_VERTEX_ARRAY, m_RendererID, sizeof(label), &length, label); // debug
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);
	}

	void OpenGLVertexArray::UnBind() const
	{
		BLU_PROFILE_FUNCTION();

		glBindVertexArray(0);

	}

	void OpenGLVertexArray::AddVertexBuffer(const Shared<VertexBuffer>& vertexBuffer)
	{
		BLU_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

//#define ENABLE
		#ifndef ENABLE

		uint32_t index = 0;
		for (const auto& element : vertexBuffer->GetLayout())
		{
			if (element.Type != ShaderDataType::Int)
			{
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, element.GetComponentCount(),
				ShaderDataTypeToOpenGlBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE, vertexBuffer->GetLayout().GetStride(),
				(const void*)element.Offset);
			index++;

			}
			else
			{
				glEnableVertexAttribArray(index);
				glVertexAttribIPointer(index, element.GetComponentCount(),
					ShaderDataTypeToOpenGlBaseType(element.Type),
					vertexBuffer->GetLayout().GetStride(),
					(const void*)element.Offset);
				index++;
			}
			
		}
		m_VertexBuffers.push_back(vertexBuffer);
		#endif // !ENABLE

		#ifdef ENABLE
		uint32_t index = 0;
		for (const auto& element : vertexBuffer->GetLayout())
		{
			switch (element.Type)
			{
			case ShaderDataType::Float:
			case ShaderDataType::Float2:
			case ShaderDataType::Float3:
			case ShaderDataType::Float4:
			{
				glVertexAttribPointer(index, element.GetComponentCount(),
					ShaderDataTypeToOpenGlBaseType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE, vertexBuffer->GetLayout().GetStride(),
					(const void*)element.Offset);
				break;
			}
			case ShaderDataType::Mat3:
			case ShaderDataType::Mat4:
			{
				break;

			}
			case ShaderDataType::Int:
			{
				glVertexAttribIPointer(index, element.GetComponentCount(),
					ShaderDataTypeToOpenGlBaseType(element.Type),
					vertexBuffer->GetLayout().GetStride(),
					(const void*)element.Offset);
				break;
			}
			case ShaderDataType::Int2:
			case ShaderDataType::Int3:
			case ShaderDataType::Int4:
			case ShaderDataType::Bool:
			{
				glVertexAttribIPointer(index, element.GetComponentCount(),
					ShaderDataTypeToOpenGlBaseType(element.Type),
					vertexBuffer->GetLayout().GetStride(),
					(const void*)element.Offset);
				break;
			}
			}
			index++;
		}
		m_VertexBuffers.push_back(vertexBuffer);
		#endif
		

		
	}

	void OpenGLVertexArray::AddIndexBuffer(const Shared<IndexBuffer>& indexBuffer)
	{
		BLU_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}
}