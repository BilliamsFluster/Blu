#include "Blupch.h"
#include "Blu.h"
#include "Renderer2D.h"
#include "Blu/Core/Core.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>



namespace Blu
{
	struct QuadVertex
	{
		glm::vec4 Color;
		glm::vec3 Position;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;

	};

	struct Renderer2DStorage
	{
		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = (MaxQuads * 4);
		static const uint32_t MaxIndices = (MaxQuads * 6);
		static const uint32_t MaxTextureSlots = 32;
		Shared<VertexArray> QuadVertexArray;
		Shared<VertexBuffer> QuadVertexBuffer;
		Shared<IndexBuffer> QuadIndexBuffer;
		Shared<Shader> TextureShader;
		Shared<Texture2D> WhiteTexture;
		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		std::array<Shared<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = WhiteTexture
		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;
		
	};

	Unique<Renderer2DStorage> s_RendererData;
	void Renderer2D::Init()
	{
		BLU_PROFILE_FUNCTION();
		s_RendererData = std::make_unique<Renderer2DStorage>();
		

		s_RendererData->QuadVertexArray = Blu::VertexArray::Create();
		s_RendererData->QuadVertexBuffer = Blu::VertexBuffer::Create(s_RendererData->MaxVertices * sizeof(QuadVertex));

		Blu::BufferLayout layout = {
			{Blu::ShaderDataType::Float4, "a_Color"},
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float2, "a_TexCoord"},
			{Blu::ShaderDataType::Float, "a_TexIndex"},
			{Blu::ShaderDataType::Float, "a_TilingFactor"}
		};

		s_RendererData->QuadVertexBuffer->SetLayout(layout);

		s_RendererData->QuadVertexBufferBase = new QuadVertex[s_RendererData->MaxVertices];
		uint32_t* quadIndices = new uint32_t[s_RendererData->MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_RendererData->MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;
			offset += 4;
		}

		
		s_RendererData->QuadVertexArray->AddVertexBuffer(s_RendererData->QuadVertexBuffer);

		Shared<IndexBuffer> quadIB = (Blu::IndexBuffer::Create(quadIndices, s_RendererData->MaxIndices ));
		s_RendererData->QuadVertexArray->AddIndexBuffer(quadIB);

		delete[] quadIndices;
		
		std::vector<int32_t> samplers(s_RendererData->MaxTextureSlots);
		for (uint32_t i = 0; i < s_RendererData->MaxTextureSlots; i++)
		{
			samplers[i] = i;
		}
		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Texture.glsl");
		s_RendererData->TextureShader = Blu::Renderer::GetShaderLibrary()->Get("Texture");
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformIntArray("u_Textures", samplers.data(), s_RendererData->MaxTextureSlots);
		s_RendererData->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_RendererData->WhiteTexture->SetData(&whiteTextureData, sizeof(whiteTextureData));

		s_RendererData->TextureSlots[0] = s_RendererData->WhiteTexture;
		s_RendererData->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[2] = {  0.5f, 0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[3] = { -0.5f, 0.5f, 0.0f, 1.0f };
		

	}
	void Renderer2D::Shutdown()
	{
		//delete s_RendererData;
	}
	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		BLU_PROFILE_FUNCTION();
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
	}
	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		BLU_PROFILE_FUNCTION();

		glm::mat4 viewProj = camera.GetProjectionMatrix() * glm::inverse(transform);
		s_RendererData->TextureShader->Bind();
		s_RendererData->TextureShader->SetUniformMat4("u_ViewProjectionMatrix", viewProj);
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
	}

	void Renderer2D::Flush()
	{
		for (uint32_t i = 0; i < s_RendererData->TextureSlotIndex; i++)
		{
			s_RendererData->TextureSlots[i]->Bind(i);
		}
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray, s_RendererData->QuadIndexCount);
		s_RendererData->Stats.DrawCalls++;
	}
	void Renderer2D::FlushAndReset()
	{
		EndScene();
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
	}
	void Renderer2D::EndScene()
	{
		BLU_PROFILE_FUNCTION();
		uint32_t dataSize = (uint8_t*)s_RendererData->QuadVertexBufferPtr - (uint8_t*)s_RendererData->QuadVertexBufferBase;
		s_RendererData->QuadVertexBuffer->SetData(s_RendererData->QuadVertexBufferBase, dataSize);
		Flush();

	}
	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndReset();
		}
		float textureIndex = 0.0f;
		float tilingFactor = 1.0f;
		
		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };

		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndReset();
		}
		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < s_RendererData->TextureSlotIndex; i++)
		{
			if (*s_RendererData->TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}
		if (textureIndex == 0.0f)
		{
			textureIndex = (float)s_RendererData->TextureSlotIndex;
			s_RendererData->TextureSlots[s_RendererData->TextureSlotIndex] = texture;
			s_RendererData->TextureSlotIndex++;
		}
		
		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };

		BLU_PROFILE_FUNCTION();
		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
		
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color, tilingFactor);
	}
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndReset();
		}
		float textureIndex = 0.0f;
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		DrawQuad(transform, color);
		
		
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor)
	{

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		DrawQuad(transform, size, texture, tilingFactor);
		
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
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndReset();
		}
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };
		
		BLU_PROFILE_FUNCTION();
		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = 0.0f;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;


	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor);

	}
	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndReset();
		}
		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < s_RendererData->TextureSlotIndex; i++)
		{
			if (*s_RendererData->TextureSlots[i].get() == *texture.get())
			{
				textureIndex = (float)i;
				break;
			}
		}
		if (textureIndex == 0.0f)
		{
			textureIndex = (float)s_RendererData->TextureSlotIndex;
			s_RendererData->TextureSlots[s_RendererData->TextureSlotIndex] = texture;
			s_RendererData->TextureSlotIndex++;
		}
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		
		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };
		BLU_PROFILE_FUNCTION();
		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
		
	}

	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return s_RendererData->Stats;
	}
	void Renderer2D::ResetStats()
	{
		s_RendererData->Stats.reset();
	}

}