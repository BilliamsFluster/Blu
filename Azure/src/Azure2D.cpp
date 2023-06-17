#include "Azure2D.h"
#include <glm/gtc/matrix_transform.hpp>


Azure2D::Azure2D()
	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Azure2D::OnAttach()
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

	m_IndexBuffer = (Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
	m_VertexArray->AddIndexBuffer(m_IndexBuffer);


	

	m_FlatColorShader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Shader::Create("assets/shaders/FlatColor.glsl"));
	Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Texture.glsl");
	m_TextureShader = std::dynamic_pointer_cast<Blu::OpenGLShader>(Blu::Renderer::GetShaderLibrary()->Get("Texture"));

	m_Texture = (Blu::Texture2D::Create("assets/textures/Wallpaper.png"));

	m_TextureShader->Bind();
	std::dynamic_pointer_cast<Blu::OpenGLShader>(m_TextureShader)->SetUniformInt("u_Texture", 0);






}

void Azure2D::OnDetach()
{
}

void Azure2D::OnUpdate(Blu::Timestep deltaTime)
{
	m_CameraController.OnUpdate(deltaTime);
	Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Blu::RenderCommand::Clear();
	// Clear the framebuffer
	//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	//glClear(GL_COLOR_BUFFER_BIT);
	Blu::Renderer::BeginScene(m_CameraController.GetCamera());
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f });
	glm::mat4 viewProjectionMatrix = m_CameraController.GetCamera().GetViewProjectionMatrix();
	m_FlatColorShader->SetUniformMat4("u_ViewProjectionMatrix", viewProjectionMatrix);

	Blu::Renderer::Submit(m_VertexArray, m_FlatColorShader);
	m_Texture->Bind();
	Blu::Renderer::Submit(m_VertexArray, m_TextureShader, transform); //for another triangle
	//m_Shader->SetUniformFloat4("u_Color", m_Color.x, m_Color.y, m_Color.z, m_Color.w); // set color of triangle 

	Blu::Renderer::EndScene();
}

void Azure2D::OnEvent(Blu::Events::Event& event)
{
	m_CameraController.OnEvent(event);
}
