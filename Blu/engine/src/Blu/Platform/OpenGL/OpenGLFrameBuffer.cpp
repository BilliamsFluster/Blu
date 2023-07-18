#include "Blupch.h"
#include "OpenGLFrameBuffer.h"
#include <glad/glad.h>

#define GL_CALL(x) \
    x; \
    { \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) \
            std::cout << "OpenGL error " << err << " at " << #x << std::endl; \
    }

namespace Blu
{
	namespace Utils
	{
		static bool IsDepthFormat(FrameBufferTextureFormat format)
		{
			switch (format)
			{
				case FrameBufferTextureFormat::DEPTH24STENCIL8: return true;
			}
			return false;
		}
		static GLenum TextureTarget(bool multisampled)
		{
			return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}
		static void CreateTextures(bool multisampled, uint32_t* outID, uint32_t count)
		{
			glCreateTextures(TextureTarget(multisampled), count, outID);
		}

		static void BindTexture(bool multisampled, uint32_t id)
		{
			glBindTexture(TextureTarget(multisampled), id);
		}
		
		static void AttachColorTexture(uint32_t id, int samples, GLenum internalFormat,  GLenum format, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_INT, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);

		}

		static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height )
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
			}
			else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType , TextureTarget(multisampled), id, 0);

		}

		static GLenum BluTextureFormatToGL(FrameBufferTextureFormat format)
		{
			switch (format)
			{
			case FrameBufferTextureFormat::RGBA8:		return GL_RGBA8;
			case FrameBufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			

					
			}
			BLU_CORE_ASSERT(false); //format invalid
			return 0;
		}

		static GLenum GLDataType(FrameBufferTextureFormat format)
		{
			switch (format)
			{
			case FrameBufferTextureFormat::RGBA8:		return GL_UNSIGNED_BYTE;
			case FrameBufferTextureFormat::RED_INTEGER: return GL_INT;



			}
			BLU_CORE_ASSERT(false); //format invalid
			return 0;
		}
		
	}
	OpenGLFrameBuffer::OpenGLFrameBuffer(const FrameBufferSpecifications& spec)
		:m_Specification(spec)
	{
		m_RendererID = 0;
		for (auto spec : m_Specification.Attachments.Attachments)
		{
			if (!Utils::IsDepthFormat(spec.TextureFormat))
				m_ColorAttachmentSpecs.emplace_back(spec);
			else
				m_DepthAttachmentSpecs = spec;
		}

		GLenum error = glGetError();  // Consume any existing errors
		if (error != GL_NO_ERROR) {
			std::cout << "OpenGL error in FBO construction: " << error << std::endl;
		}
		Invalidate();
	}
	void OpenGLFrameBuffer::Resize(uint32_t width, uint32_t height)
	{
		
		m_Specification.Width = width;
		m_Specification.Height = height;
		Invalidate();
	}
	int OpenGLFrameBuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		
		BLU_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size());
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
		int pixelData;
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
		return pixelData;
	}
	void OpenGLFrameBuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		
		BLU_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size());

		auto& spec = m_ColorAttachmentSpecs[attachmentIndex];
		glClearTexImage(m_ColorAttachments[attachmentIndex], 0, Utils::BluTextureFormatToGL(spec.TextureFormat),Utils::GLDataType(spec.TextureFormat), &value);
	}
	OpenGLFrameBuffer::~OpenGLFrameBuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);
	}
	void OpenGLFrameBuffer::Bind()
	{

		GLenum error = glGetError();  // Consume any existing errors
		if (error != GL_NO_ERROR) {
			std::cout << "OpenGL error in FBO bind(): " << error << std::endl;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);

		
	}
	void OpenGLFrameBuffer::UnBind()
	{
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}
	static void CheckOpenGLError(const char* stmt)
	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
		{
			std::cout << "OpenGL error " << err << " at " << stmt << std::endl;
		}
	}
	void OpenGLFrameBuffer::Invalidate()
	{
		if (m_RendererID > 0 && glIsFramebuffer(m_RendererID))
		{
			glDeleteFramebuffers(1, &m_RendererID);
			CheckOpenGLError("glDeleteFramebuffers"); // Check for errors

			glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
			CheckOpenGLError("glDeleteTextures (Color Attachments)"); // Check for errors

			glDeleteTextures(1, &m_DepthAttachment);
			CheckOpenGLError("glDeleteTextures (Depth Attachment)"); // Check for errors

			m_ColorAttachments.clear();
			m_DepthAttachment = 0;
		}
		if (m_RendererID > 0 ) {
			GLint current_fbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
			std::cout << "Current FBO: " << current_fbo << std::endl;

			if (glIsFramebuffer(m_RendererID)) {
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDeleteFramebuffers(1, &m_RendererID);
				CheckOpenGLError("glDeleteFramebuffers"); // Check for errors
				m_RendererID = 0;  // Reset m_RendererID after deletion
			}
			else {
				std::cout << "FBO " << m_RendererID << " is not valid" << std::endl;
			}
		}
		if (m_Specification.Width != 0 && m_Specification.Height != 0)
		{


			GLenum error = glGetError();  // Consume any existing errors
			if (error != GL_NO_ERROR) {
				std::cout << "OpenGL error before glCreateFramebuffers: " << error << std::endl;
			}
			glCreateFramebuffers(1, &m_RendererID);
			CheckOpenGLError("glCreateFramebuffers"); // Check for errors
			std::cout << m_RendererID << std::endl;

			glObjectLabel(GL_FRAMEBUFFER, m_RendererID, -1, "Frame Buffer"); // debug

			glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

			bool multisample = m_Specification.Samples > 1;
			//Attachments
			if (m_ColorAttachmentSpecs.size())
			{
				m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());
				Utils::CreateTextures(multisample, m_ColorAttachments.data(), m_ColorAttachments.size());

				for (size_t i = 0; i < m_ColorAttachments.size(); i++)
				{
					Utils::BindTexture(multisample, m_ColorAttachments[i]);
					std::string label = "My Color Attachment " + std::to_string(i);
					glObjectLabel(GL_TEXTURE, m_ColorAttachments[i], -1, label.c_str()); // debug

					switch (m_ColorAttachmentSpecs[i].TextureFormat)
					{
					case FrameBufferTextureFormat::RGBA8:
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, i);
						break;
					case FrameBufferTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, m_Specification.Width, m_Specification.Height, i);
						break;
					}
				}

			}
			if (m_DepthAttachmentSpecs.TextureFormat != FrameBufferTextureFormat::None)
			{
				Utils::CreateTextures(multisample, &m_DepthAttachment, 1);
				Utils::BindTexture(multisample, m_DepthAttachment);

				switch (m_DepthAttachmentSpecs.TextureFormat)
				{
				case FrameBufferTextureFormat::DEPTH24STENCIL8:
					Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
					break;
				}
			}

			if (m_ColorAttachments.size() > 1)
			{
				BLU_CORE_ASSERT(m_ColorAttachments.size() <= 4);
				GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };

				glDrawBuffers(m_ColorAttachments.size(), buffers);
			}
			else if (m_ColorAttachments.empty())
			{
				glDrawBuffer(GL_NONE);
			}


			BLU_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is complete");
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (glGetError() > 100)
			{
				std::cout << "GlError FrameBuffer" << glGetError() << std::endl;

			}
		}

	}
}