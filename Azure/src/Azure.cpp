#include <Blu.h>
#include <Blu/Core/EntryPoint.h>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Azure2D.h"



class Engine : public Blu::Layers::Layer
{
public:
	Engine()
		:Layer("Engine"),m_CameraController(1280.0f / 720.0f, true)
	{
		m_VertexArray = Blu::VertexArray::Create();

		float vertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // Bottom left corner
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // Bottom right corner
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, // Top right corner
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f  // Top left corner
		};


		m_VertexBuffer = Blu::VertexBuffer::Create(vertices, sizeof(vertices));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float2, "a_TexCoord"}
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);






		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

		m_IndexBuffer = Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));
		m_VertexArray->AddIndexBuffer(m_IndexBuffer);


		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			
			uniform mat4 u_ViewProjectionMatrix;
			uniform mat4 u_Transform; 
			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjectionMatrix * u_Transform * vec4(a_Position, 1.0);	
				v_Color = a_Color;
			}



		)";

		std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 o_color;
			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				o_color = v_Color;
			}



		)";
		
		m_Shader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Shader::Create("VertexPosColor", vertexSrc, fragmentSrc));
		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Texture.glsl");
		m_TextureShader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Renderer::GetShaderLibrary()->Get("Texture"));

		m_Texture = (Blu::Texture2D::Create("assets/textures/Wallpaper.png"));

		m_TextureShader->Bind();
		//std::dynamic_pointer_cast<Blu::OpenGLShader>(textureShader)->Bind();
		std::dynamic_pointer_cast<Blu::OpenGLShader>(m_TextureShader)->SetUniformInt("u_Texture", 0);





		

	}
	
	void OnUpdate(Blu::Timestep deltaTime) override
	{
		//BLU_CORE_WARN("delta time: {0}s ({1}ms) ", deltaTime.GetSeconds(), deltaTime.GetMilliseconds());
		m_CameraController.OnUpdate(deltaTime);
		


		Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Blu::RenderCommand::Clear();
		// Clear the framebuffer
		//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT);
		Blu::Renderer::BeginScene(m_CameraController.GetCamera());
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f });
		glm::mat4 viewProjectionMatrix = m_CameraController.GetCamera().GetViewProjectionMatrix();
		m_Shader->SetUniformMat4("u_ViewProjectionMatrix", viewProjectionMatrix);

		Blu::Renderer::Submit(m_VertexArray, m_Shader);
		m_Texture->Bind();
		Blu::Renderer::Submit(m_VertexArray, m_TextureShader, transform); //for another triangle
		//m_Shader->SetUniformFloat4("u_Color", m_Color.x, m_Color.y, m_Color.z, m_Color.w); // set color of triangle 

		Blu::Renderer::EndScene();

	}

	void OnEvent(Blu::Events::Event& event) override
	{
		m_CameraController.OnEvent(event);
		/*Blu::Events::EventHandler handler;
		
		event.Accept(handler);
		event.Handled = true;*/
		
	}
private:
	Blu::ShaderLibrary m_ShaderLibrary;
	Blu::Shared<Blu::VertexArray> m_VertexArray;
	Blu::Shared<Blu::OpenGLShader> m_Shader, m_TextureShader;
	Blu::OrthographicCameraController m_CameraController;
	Blu::Shared<Blu::Texture2D> m_Texture;
	
	Blu::Shared< Blu::IndexBuffer> m_IndexBuffer;
	Blu::Shared< Blu::VertexBuffer> m_VertexBuffer;
	glm::vec4 m_Color;
	glm::vec3 m_TrianglePosition;
};


class Azure : public Blu::Application
{
public:
	Azure()
	{
		//PushLayer(new Engine());
		PushLayer(new Azure2D());
		//PushOverlay(new Blu::Layers::ImGuiLayer());
		//PushOverlay(new Blu::Layers::ImGuiLayer());

		
		

	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{

	return new Azure();
}