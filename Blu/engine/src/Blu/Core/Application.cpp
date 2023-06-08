#include "Blupch.h"
#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Input.h"
#include <glad/glad.h>
#include "imgui.h"
#include "Blu/Rendering/Buffer.h"
#include "Blu/Rendering/VertexArray.h"
#include "Blu/Rendering/Renderer.h"





namespace Blu
{
	

	Application* Application::s_Instance = nullptr;
	
	Application::Application()
		:m_Camera({-1.0f, 1.0f, -1.0f, 1.0f})
	{
		m_Color = { 1,1,1,1 };
		m_Window = std::unique_ptr<Window>(Window::Create());
		s_Instance = this;
		unsigned int id;

		m_VertexArray.reset(VertexArray::Create());

		
		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 1.f, 0.2f, 0.8f, 1.0f,
			0.5f, 0.5f, 0.0f, 0.2f, 0.3f, 0.6f, 1.0f,
			0.5f, -0.5f, 0.0f, 0.3f, 0.8f, 0.2f, 1.0f
		};
		m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
		
		BufferLayout layout = {
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float4, "a_Color"}
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);
		

		

		

		uint32_t indices[3] = { 0, 1, 2 };

		m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->AddIndexBuffer(m_IndexBuffer);


		float squareVertices[3 * 4] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.5f
		};

		

		m_SquareVertexArray.reset(VertexArray::Create());
		std::shared_ptr<VertexBuffer> squareVertexBuffer;
		squareVertexBuffer.reset(VertexBuffer::Create(squareVertices, sizeof(squareVertices)));

		BufferLayout squareLayout = {
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float4, "a_Color"}
		};
		m_VertexBuffer->SetLayout(squareLayout);
		m_SquareVertexArray->AddVertexBuffer(squareVertexBuffer);


		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };

		std::shared_ptr<IndexBuffer> squareIndexBuffer;
		squareIndexBuffer.reset(IndexBuffer::Create(squareIndices, sizeof(squareIndices)));

		squareIndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_SquareVertexArray->AddIndexBuffer(squareIndexBuffer);

		//// Create a framebuffer
		//glGenFramebuffers(1, &m_FrameBufferObject);
		//glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);

		//// Create a texture
		//glGenTextures(1, &m_Texture);
		//glBindTexture(GL_TEXTURE_2D, m_Texture);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); // change 800x600 to your window size

		//// Set texture parameters
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//// Attach it to the framebuffer
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

		//// Check if framebuffer is complete
		//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		//	std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		//// Unbind to reset to default framebuffer
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//std::cout << "OpenGl Error: " << glGetError() << std::endl;
		// no 1281 error here but error 0 here

		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			
			uniform mat4 u_ViewProjectionMatrix;
			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);	
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
			
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();
			glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);
			// Clear the framebuffer
			//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			//glClear(GL_COLOR_BUFFER_BIT);
			Renderer::BeginScene(m_Camera);
			
			Renderer::Submit(m_VertexArray, m_Shader);
			Renderer::EndScene();
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