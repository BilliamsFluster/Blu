#include <Blu.h>
#include "glm/glm.hpp"

class Rendering : public Blu::Layers::Layer
{
public:
	Rendering()
		:Layer("Rendering")
	{

	}
	void OnUpdate() override
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
		:Layer("Engine"), m_Camera({ -1.0f, 1.0f, -1.0f, 1.0f })
	{
		m_VertexArray.reset(Blu::VertexArray::Create());


		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 1.f, 0.2f, 0.8f, 1.0f,
			0.5f, 0.5f, 0.0f, 0.2f, 0.3f, 0.6f, 1.0f,
			0.5f, -0.5f, 0.0f, 0.3f, 0.8f, 0.2f, 1.0f
		};
		m_VertexBuffer.reset(Blu::VertexBuffer::Create(vertices, sizeof(vertices)));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float4, "a_Color"}
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);






		uint32_t indices[3] = { 0, 1, 2 };

		m_IndexBuffer.reset(Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->AddIndexBuffer(m_IndexBuffer);


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
		m_Shader.reset(new Blu::OpenGLShader(vertexSrc, fragmentSrc));



		

	}
	void OnUpdate() override
	{

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_UP))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x, m_Camera.GetPosition().y - 0.01, 0.0f });
		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_DOWN))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x, m_Camera.GetPosition().y + 0.01, 0.0f });

		}
		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x + 0.01, m_Camera.GetPosition().y, 0.0f });

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_RIGHT))
		{
			m_Camera.SetPosition({ m_Camera.GetPosition().x - 0.01, m_Camera.GetPosition().y, 0.0f });

		}

		/*if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT))
		{
			m_Camera.SetRotation(m_Camera.GetRotation() - 10.f);

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_RIGHT))
		{
			m_Camera.SetRotation(m_Camera.GetRotation() + 10.f);

		}*/

		Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Blu::RenderCommand::Clear();
		// Clear the framebuffer
		//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT);
		Blu::Renderer::BeginScene(m_Camera);

		Blu::Renderer::Submit(m_VertexArray, m_Shader);
		Blu::Renderer::EndScene();

	}

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{
		if (event.GetType() == Blu::Events::Event::Type::KeyPressed)
		{
			Blu::Events::KeyPressedEvent& keyEvent = dynamic_cast<Blu::Events::KeyPressedEvent&>(event);
			std::cout << keyEvent.GetKeyCode() << std::endl;
		}
		
		handler.HandleEvent(event);
		event.Handled = true;
		
	}
private:
	std::shared_ptr<Blu::VertexArray> m_VertexArray;
	std::shared_ptr<Blu::OpenGLShader> m_Shader;
	Blu::OrthographicCamera m_Camera;
	
	std::shared_ptr< Blu::IndexBuffer> m_IndexBuffer;
	std::shared_ptr< Blu::VertexBuffer> m_VertexBuffer;

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