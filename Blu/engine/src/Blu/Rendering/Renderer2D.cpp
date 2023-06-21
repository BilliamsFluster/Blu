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
		
		Shared<VertexBuffer> QuadVertexBuffer;
		Shared<IndexBuffer> QuadIndexBuffer;
		Shared<Shader> TextureShader;
		Shared<Texture2D> WhiteTexture;

	};
	static Renderer2DStorage* s_RendererData;
	void Renderer2D::Init()
	{
		BLU_PROFILE_FUNCTION();

		s_RendererData = new Renderer2DStorage();

		s_RendererData->QuadVertexArray = Blu::VertexArray::Create();
		float vertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // Bottom left corner
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // Bottom right corner
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, // Top right corner
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f  // Top left corner
		};


		s_RendererData->QuadVertexBuffer = Blu::VertexBuffer::Create(vertices, sizeof(vertices));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float2, "a_TexCoord"}


		};
		s_RendererData->QuadVertexBuffer->SetLayout(layout);
		s_RendererData->QuadVertexArray->AddVertexBuffer(s_RendererData->QuadVertexBuffer);






		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

		s_RendererData->QuadIndexBuffer = (Blu::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		s_RendererData->QuadVertexArray->AddIndexBuffer(s_RendererData->QuadIndexBuffer);


		

		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Texture.glsl");
		s_RendererData->TextureShader = Blu::Renderer::GetShaderLibrary()->Get("Texture");
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformInt("u_Texture", 0);
		s_RendererData->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_RendererData->WhiteTexture->SetData(&whiteTextureData, sizeof(whiteTextureData));

		
		

	}
	void Renderer2D::Shutdown()
	{
		delete s_RendererData;
	}
	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		BLU_PROFILE_FUNCTION();

		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());


	}
	void Renderer2D::EndScene()
	{
		BLU_PROFILE_FUNCTION();

	}
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color, tilingFactor);
	}
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor)
	{
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformFloat4("u_Color", color);
		s_RendererData->TextureShader->SetUniformFloat("u_TilingFactor", tilingFactor);
		s_RendererData->WhiteTexture->Bind();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) // * rotation
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		s_RendererData->TextureShader->SetUniformMat4("u_Transform", transform);
		

		s_RendererData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor)
	{
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformFloat4("u_Color", glm::vec4(1.0f));
		s_RendererData->TextureShader->SetUniformFloat("u_TilingFactor", tilingFactor);


		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) // * rotation
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		s_RendererData->TextureShader->SetUniformMat4("u_Transform", transform);

		texture->Bind();



		s_RendererData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor);

	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color, tilingFactor);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor)
	{

		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformFloat4("u_Color", color);
		s_RendererData->TextureShader->SetUniformFloat("u_TilingFactor", tilingFactor);
		s_RendererData->WhiteTexture->Bind();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) 
			*glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)) 
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		s_RendererData->TextureShader->SetUniformMat4("u_Transform", transform);


		s_RendererData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray);
		
		
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor);

	}
	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor)
	{
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformFloat4("u_Color", glm::vec4(1.0f));
		s_RendererData->TextureShader->SetUniformFloat("u_TilingFactor", tilingFactor);
		


		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)) 
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		s_RendererData->TextureShader->SetUniformMat4("u_Transform", transform);
		texture->Bind();

		s_RendererData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray);

	}

}