#include "Blupch.h"
#include "Blu.h"
#include "Renderer2D.h"
#include "Blu/Core/Core.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>



namespace Blu
{
	struct Renderer2DStorage
	{
		Shared<VertexArray> QuadVertexArray;
		Shared<Shader> FlatColorShader;
		Shared<VertexBuffer> QuadVertexBuffer;
		Shared<IndexBuffer> QuadIndexBuffer;
		Shared<Shader> TextureShader;

	};
	static Renderer2DStorage* s_RendererData;
	void Renderer2D::Init()
	{
		s_RendererData = new Renderer2DStorage();

		s_RendererData->QuadVertexArray = Blu::VertexArray::Create();
		float vertices[5 * 3] = {
			-0.5f, -0.5f, 0.0f, //0.0f, 0.0f, // Bottom left corner
			 0.5f, -0.5f, 0.0f, //1.0f, 0.0f, // Bottom right corner
			 0.5f,  0.5f, 0.0f, //1.0f, 1.0f, // Top right corner
			-0.5f,  0.5f, 0.0f //0.0f, 1.0f  // Top left corner
		};


		s_RendererData->QuadVertexBuffer = Blu::VertexBuffer::Create(vertices, sizeof(vertices));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float3, "a_Position"}
		};
		s_RendererData->QuadVertexBuffer->SetLayout(layout);
		s_RendererData->QuadVertexArray->AddVertexBuffer(s_RendererData->QuadVertexBuffer);






		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

		s_RendererData->QuadIndexBuffer = (Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		s_RendererData->QuadVertexArray->AddIndexBuffer(s_RendererData->QuadIndexBuffer);


		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/FlatColor.glsl");
		s_RendererData->FlatColorShader = Blu::Renderer::GetShaderLibrary()->Get("FlatColor");

		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Texture.glsl");
		s_RendererData->TextureShader = Blu::Renderer::GetShaderLibrary()->Get("Texture");

		
		

	}
	void Renderer2D::Shutdown()
	{
		delete s_RendererData;
	}
	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		s_RendererData->FlatColorShader->Bind();
		s_RendererData->FlatColorShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
	}
	void Renderer2D::EndScene()
	{
	}
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		s_RendererData->FlatColorShader->Bind();
		s_RendererData->FlatColorShader->SetUniformFloat4("u_Color", color);
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) // * rotation
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		s_RendererData->FlatColorShader->SetUniformMat4("u_Transform", transform);
		

		s_RendererData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray);
	}
}