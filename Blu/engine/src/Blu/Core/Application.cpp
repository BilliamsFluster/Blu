#include "Blupch.h"
#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Input.h"
#include <glad/glad.h>
#include "imgui.h"
#include "Blu/Rendering/Buffer.h"





namespace Blu
{
	

	Application* Application::s_Instance = nullptr;
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
	Application::Application()
	{
		m_Color = { 1,1,1,1 };
		m_Window = std::unique_ptr<Window>(Window::Create());
		s_Instance = this;
		unsigned int id;

		glGenVertexArrays(1, &m_VertexArray);
		glBindVertexArray(m_VertexArray);

		
		float vertices[3 * 7] = {
			-0.8f, -0.3f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			-0.4f, -0.8f, 0.0f, 0.2f, 0.3f, 0.6f, 1.0f,
			-0.1f, -0.3f, 0.0f, 0.3f, 0.8f, 0.2f, 1.0f
		};
		m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
		{
			BufferLayout layout = {
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float4, "a_Color"}
			//{ShaderDataType::Float3, "a_Normal", true}
			};
			m_VertexBuffer->SetLayout(layout);
		}
		

		uint32_t index = 0;
		for (const auto& element : m_VertexBuffer->GetLayout())
		{
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, element.GetComponentCount(),
				ShaderDataTypeToOpenGlBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE, m_VertexBuffer->GetLayout().GetStride(),
				(const void*)element.Offset);
			index++;
		}

		

		

		uint32_t indices[3] = { 0, 1, 2 };

		m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		// Create a framebuffer
		glGenFramebuffers(1, &m_FrameBufferObject);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);

		// Create a texture
		glGenTextures(1, &m_Texture);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); // change 800x600 to your window size

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Attach it to the framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

		// Check if framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		// Unbind to reset to default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		std::cout << "OpenGl Error: " << glGetError() << std::endl;
		// no 1281 error here but error 0 here

		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				gl_Position = vec4(a_Position, 1.0);	
				v_Color = a_Color;
			}



		)";

		std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 o_color;
			in vec3 v_Position;
			uniform vec4 u_Color;
			in vec4 v_Color;

			void main()
			{
				o_color = v_Color;
			}



		)";
		m_Shader.reset(new OpenGLShader(vertexSrc, fragmentSrc));
		std::cout << "OpenGl Error: " << glGetError() << std::endl; // Error 0
	}
	
	Application::~Application()
	{

	}

	void Application::PushLayer(Layers::Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layers::Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();

	}

	void Application::Run()
	{
		
		
		while (m_Running)
		{

			m_Window->OnUpdate();
			m_Running = !m_Window->ShouldClose();
			std::cout << "OpenGl Error: " << glGetError() << std::endl; // Error 1281

			m_Shader->Bind();

			glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);
			glBindVertexArray(m_VertexArray);
			glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//m_Shader->SetUniform4f("u_Color", m_Color.x, m_Color.y, m_Color.z, m_Color.w); // set color of triangle 

			for (Layers::Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}
			auto [x, y] = WindowInput::Input::GetMousePosition();
			
		}
		
		
	}
	void Application::OnEvent(Events::Event& event)
	{
		//// Dispatch the event to the layer stack
		//m_EventDispatcher.Dispatch(*this, event);

		//// If the event is a window close event, stop the application
		//if (event.GetType() == Events::Event::Type::WindowClose)
		//	m_Running = false;
	}

	
}