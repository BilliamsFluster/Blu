#include <Blu.h>
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
class Rendering : public Blu::Layers::Layer
{
public:
	Rendering()
		:Layer("Rendering")
	{

	}
	void OnUpdate(Blu::Timestep deltaTime) override
	{
	} 

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{
		
	}
}; 


class Engine : public Blu::Layers::Layer
{
public:
	Engine()
		:Layer("Engine"), m_Camera({ -1.0f, 1.0f, -1.0f, 1.0f }), m_TrianglePosition(0.0f)
	{
		m_VertexArray.reset(Blu::VertexArray::Create());

		float vertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // Bottom left corner
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // Bottom right corner
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, // Top right corner
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f  // Top left corner
		};


		m_VertexBuffer.reset(Blu::VertexBuffer::Create(vertices, sizeof(vertices)));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float2, "a_TexCoord"}
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);






		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

		m_IndexBuffer.reset(Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
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
		
		m_Shader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Shader::Create(vertexSrc, fragmentSrc));
		m_TextureShader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Shader::Create("assets/shaders/Texture.glsl"));


		m_Texture = (Blu::Texture2D::Create("assets/textures/Wallpaper.png"));

		std::dynamic_pointer_cast<Blu::OpenGLShader>(m_TextureShader)->Bind();
		std::dynamic_pointer_cast<Blu::OpenGLShader>(m_TextureShader)->SetUniformInt("u_Texture", 0);





		

	}
	
	void OnUpdate(Blu::Timestep deltaTime) override
	{
		//BLU_CORE_WARN("delta time: {0}s ({1}ms) ", deltaTime.GetSeconds(), deltaTime.GetMilliseconds());

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_UP))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x, m_Camera.GetPosition().y - 1.f * deltaTime, 0.0f });
		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_DOWN))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x, m_Camera.GetPosition().y + 1.f * deltaTime, 0.0f });
		}
		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x + 1.f * deltaTime, m_Camera.GetPosition().y, 0.0f });

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_RIGHT))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x - 1.f * deltaTime, m_Camera.GetPosition().y, 0.0f });

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT_SHIFT))
		{
			m_Camera.SetRotation(m_Camera.GetRotation() + 1.f * deltaTime);

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT_CONTROL))
		{
			m_Camera.SetRotation(m_Camera.GetRotation() - 1.f * deltaTime);

		}


		Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Blu::RenderCommand::Clear();
		// Clear the framebuffer
		//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT);
		Blu::Renderer::BeginScene(m_Camera);
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_TrianglePosition);
		glm::mat4 viewProjectionMatrix = m_Camera.GetViewProjectionMatrix();
		m_Shader->SetUniformMat4("u_ViewProjectionMatrix", viewProjectionMatrix);

		Blu::Renderer::Submit(m_VertexArray, m_Shader);
		m_Texture->Bind();
		Blu::Renderer::Submit(m_VertexArray, m_TextureShader, transform); //for another triangle
		//m_Shader->SetUniformFloat4("u_Color", m_Color.x, m_Color.y, m_Color.z, m_Color.w); // set color of triangle 

		Blu::Renderer::EndScene();

	}

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{
		if (event.GetType() == Blu::Events::Event::Type::KeyPressed)
		{
			Blu::Events::KeyPressedEvent& keyEvent = dynamic_cast<Blu::Events::KeyPressedEvent&>(event);
			BLU_CORE_WARN("KeyPresed: {0}", keyEvent.GetKeyCode());

		}
		if (event.GetType() == Blu::Events::Event::Type::KeyReleased)
		{
		}
		
		
		handler.HandleEvent(event);
		event.Handled = true;
		
	}
private:
	Blu::Shared<Blu::VertexArray> m_VertexArray;
	Blu::Shared<Blu::OpenGLShader> m_Shader, m_TextureShader;
	Blu::OrthographicCamera m_Camera;
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
		PushLayer(new Rendering());//1st layer
		PushLayer(new Engine()); //2nd layer 
		PushOverlay(new Blu::Layers::ImGuiLayer());
		
		

	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{

	return new Azure();
}