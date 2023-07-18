#include "Blupch.h"
#include "Blu.h"
#include "Renderer2D.h"
#include "Blu/Core/Core.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Blu/Platform/OpenGL/OpenGLBuffer.h"


#define MODE_EDITOR

namespace Blu
{
	struct QuadVertex
	{
		glm::vec4 Color;
		glm::vec3 Position;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;
		float Thickness = 1.0f;
		int EntityID;

		
		

	};

	struct CircleVertex
	{
		glm::vec4 Color;
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		float Fade;
		float Thicknes;
		int EntityID;
	};

	
	Unique<Renderer2DStorage> Renderer2D::s_RendererData = std::make_unique<Renderer2DStorage>();	
	
	void Renderer2D::Init()
	{
		BLU_PROFILE_FUNCTION();
		

		s_RendererData->QuadVertexArray = Blu::VertexArray::Create();
		s_RendererData->QuadVertexBuffer = Blu::VertexBuffer::Create(s_RendererData->MaxVertices * sizeof(QuadVertex));

		s_RendererData->CircleVertexArray = Blu::VertexArray::Create();
		s_RendererData->CircleVertexBuffer = Blu::VertexBuffer::Create(s_RendererData->MaxVertices * sizeof(CircleVertex));



		// Define the layout for quadrilateral and circle objects
		Blu::BufferLayout quadLayout = {
			{Blu::ShaderDataType::Float4, "a_Color"},
			{Blu::ShaderDataType::Float3, "a_Position"},
			{Blu::ShaderDataType::Float2, "a_TexCoord"},
			{Blu::ShaderDataType::Float, "a_TexIndex"},
			{Blu::ShaderDataType::Float, "a_TilingFactor"},
			{Blu::ShaderDataType::Float, "a_Thickness"},
			{Blu::ShaderDataType::Int, "a_EntityID"}
		};

		// Circle layout
		Blu::BufferLayout circleLayout = {
			{Blu::ShaderDataType::Float4, "a_Color"},
			{Blu::ShaderDataType::Float3, "a_WorldPosition"},
			{Blu::ShaderDataType::Float3, "a_LocalPosition"},
			{Blu::ShaderDataType::Float, "a_Fade"},
			{Blu::ShaderDataType::Float, "a_Thickness"},
			{Blu::ShaderDataType::Int, "a_EntityID"}
		};

		// Set the layouts for the quad and circle vertex buffers
		s_RendererData->QuadVertexBuffer->SetLayout(quadLayout);
		s_RendererData->CircleVertexBuffer->SetLayout(circleLayout);

		// Initialize the base vertex buffer for quad and circle objects
		s_RendererData->QuadVertexBufferBase = new QuadVertex[s_RendererData->MaxVertices];
		s_RendererData->CircleVertexBufferBase = new CircleVertex[s_RendererData->MaxVertices];

		// Initialize quad indices and set up the offsets

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

		
		// Add vertex buffers to the quad and circle vertex arrays
		s_RendererData->QuadVertexArray->AddVertexBuffer(s_RendererData->QuadVertexBuffer);
		s_RendererData->CircleVertexArray->AddVertexBuffer(s_RendererData->CircleVertexBuffer);

		// Create quad index buffer and add it to the quad and circle vertex arrays
		Shared<IndexBuffer> quadIB = (Blu::IndexBuffer::Create(quadIndices, s_RendererData->MaxIndices ));
		s_RendererData->QuadVertexArray->AddIndexBuffer(quadIB);
		s_RendererData->CircleVertexArray->AddIndexBuffer(quadIB); // this is intentional

		// Free the memory allocated for quadIndices
		delete[] quadIndices;
		
		// Initialize the texture slots
		std::vector<int32_t> samplers(s_RendererData->MaxTextureSlots);
		for (uint32_t i = 0; i < s_RendererData->MaxTextureSlots; i++)
		{
			samplers[i] = i;
		}
		
		// Load the shaders and bind the quad shader
		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Renderer2D_Quad.glsl");
		Blu::Renderer::GetShaderLibrary()->Load("assets/shaders/Renderer2D_Circle.glsl");
		s_RendererData->QuadShader = Blu::Renderer::GetShaderLibrary()->Get("Renderer2D_Quad");
		s_RendererData->CircleShader = Blu::Renderer::GetShaderLibrary()->Get("Renderer2D_Circle");
		s_RendererData->QuadShader->Bind();

		// Set the texture slots for the quad shader
		s_RendererData->QuadShader->SetUniformIntArray("u_Textures", samplers.data(), s_RendererData->MaxTextureSlots);
		
		// Create a white texture and set it as the first texture slot
		s_RendererData->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_RendererData->WhiteTexture->SetData(&whiteTextureData, sizeof(whiteTextureData));

		// Set the vertex positions for the quad
		s_RendererData->TextureSlots[0] = s_RendererData->WhiteTexture;
		s_RendererData->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[2] = {  0.5f, 0.5f, 0.0f, 1.0f };
		s_RendererData->QuadVertexPositions[3] = { -0.5f, 0.5f, 0.0f, 1.0f };
		

	}
	void Renderer2D::Shutdown()
	{
	}

	// Begin rendering the scene with a given camera
	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		BLU_PROFILE_FUNCTION();
		// Bind the quad shader, set the view projection matrix and initialize other data for quad rendering
		s_RendererData->QuadShader->Bind();
		s_RendererData->QuadShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
		s_RendererData->QuadShader->UnBind();

		// Bind the circle shader, set the view projection matrix and initialize other data for circle rendering
		s_RendererData->CircleShader->Bind();
		s_RendererData->CircleShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
		s_RendererData->CircleIndexCount = 0;
		s_RendererData->CircleVertexBufferPtr = s_RendererData->CircleVertexBufferBase;
		s_RendererData->CircleShader->UnBind();


	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		BLU_PROFILE_FUNCTION();
		s_RendererData->QuadShader->Bind();
		s_RendererData->QuadShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
		s_RendererData->QuadShader->UnBind();



		s_RendererData->CircleShader->Bind();
		s_RendererData->CircleShader->SetUniformMat4("u_ViewProjectionMatrix", camera.GetViewProjectionMatrix());
		s_RendererData->CircleIndexCount = 0;
		s_RendererData->CircleVertexBufferPtr = s_RendererData->CircleVertexBufferBase;
		s_RendererData->CircleShader->UnBind();

	}
	static glm::mat4 viewProj;
	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		BLU_PROFILE_FUNCTION();
		
		viewProj = camera.GetProjectionMatrix() *glm::inverse(transform);
		
		s_RendererData->QuadShader->Bind();
		s_RendererData->QuadShader->SetUniformMat4("u_ViewProjectionMatrix", viewProj);
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
		s_RendererData->QuadShader->UnBind();

		s_RendererData->CircleShader->Bind();
		s_RendererData->CircleShader->SetUniformMat4("u_ViewProjectionMatrix", viewProj);
		s_RendererData->CircleIndexCount = 0;
		s_RendererData->CircleVertexBufferPtr = s_RendererData->CircleVertexBufferBase;
		s_RendererData->CircleShader->UnBind();
		
		

	}

	void Renderer2D::FlushQuad()
	{
		
		s_RendererData->QuadShader->Bind();
		for (uint32_t i = 0; i < s_RendererData->TextureSlotIndex; i++)
		{
			s_RendererData->TextureSlots[i]->Bind(i);
		}
		RenderCommand::DrawIndexed(s_RendererData->QuadVertexArray, s_RendererData->QuadIndexCount);
		s_RendererData->Stats.DrawCalls++;
	}
	void Renderer2D::FlushCircle()
	{
		
		s_RendererData->CircleShader->Bind();		
		uint32_t dataSize = (uint32_t)((uint8_t*)s_RendererData->CircleVertexBufferPtr - (uint8_t*)s_RendererData->CircleVertexBufferBase);
		s_RendererData->CircleVertexBuffer->SetData(s_RendererData->CircleVertexBufferBase, dataSize);

		RenderCommand::DrawIndexed(s_RendererData->CircleVertexArray, s_RendererData->CircleIndexCount);
		s_RendererData->Stats.DrawCalls++;
	}

	void Renderer2D::FlushAndResetQuad()
	{
		EndScene();
		s_RendererData->QuadIndexCount = 0;
		s_RendererData->QuadVertexBufferPtr = s_RendererData->QuadVertexBufferBase;
		s_RendererData->TextureSlotIndex = 1;
	}

	void Renderer2D::FlushAndResetCircle()
	{
		EndScene();
		s_RendererData->CircleIndexCount = 0;
		s_RendererData->CircleVertexBufferPtr = s_RendererData->CircleVertexBufferBase;
	}

	void Renderer2D::EndScene()
	{
		BLU_PROFILE_FUNCTION();
		uint32_t dataSize = (uint8_t*)s_RendererData->QuadVertexBufferPtr - (uint8_t*)s_RendererData->QuadVertexBufferBase;
		s_RendererData->QuadVertexBuffer->SetData(s_RendererData->QuadVertexBufferBase, dataSize);
		FlushQuad();
		FlushCircle();

	}
	void Renderer2D::DrawRotatedQuad(const glm::mat4& transform, const glm::vec4& color, const float rotation)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetQuad();
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
	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID)
	{
		if (src.Texture)
		{
			DrawTexturedQuad(transform, src.Texture, src.Color, entityID);
		}
		DrawQuad(transform, src.Color, entityID);

	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID)
	{
		if (s_RendererData->CircleIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetCircle();
		}
		

		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };

		for (int i = 0; i < 4; i++) {
			s_RendererData->CircleVertexBufferPtr->WorldPosition = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->CircleVertexBufferPtr->LocalPosition = s_RendererData->QuadVertexPositions[i] * 2.0f;
			s_RendererData->CircleVertexBufferPtr->Color = color;
			s_RendererData->CircleVertexBufferPtr->Thicknes = thickness;
			s_RendererData->CircleVertexBufferPtr->Fade = fade;
#ifdef MODE_EDITOR
			s_RendererData->CircleVertexBufferPtr->EntityID = entityID;
#endif // MODE_EDITOR

			s_RendererData->CircleVertexBufferPtr++;
		}

		s_RendererData->CircleIndexCount += 6;
		s_RendererData->Stats.CircleCount++;
	}

	void Renderer2D::DrawTexturedQuad(const glm::mat4& transform, const Shared<Texture2D>& texture, const glm::vec4& color, int entityID, float tilingFactor)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetQuad();
		}

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
			s_RendererData->QuadVertexBufferPtr->Thickness = 1.0f;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
	}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetQuad();
		}
		float textureIndex = 0.0f;
		float tilingFactor = 1.0f;

		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };

		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_RendererData->QuadVertexBufferPtr->Thickness = thickness;
			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;

			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetQuad();
		}
		float textureIndex = 0.0f;
		float tilingFactor = 1.0f;
		
		std::array<glm::vec2, 4> texCoords = { { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} } };

		for (int i = 0; i < 4; i++) {
			s_RendererData->QuadVertexBufferPtr->Position = transform * s_RendererData->QuadVertexPositions[i];
			s_RendererData->QuadVertexBufferPtr->Color = color;
			s_RendererData->QuadVertexBufferPtr->TexCoord = texCoords[i];
			s_RendererData->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_RendererData->QuadVertexBufferPtr->Thickness = 1.0f;

			s_RendererData->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			#ifdef MODE_EDITOR
			s_RendererData->QuadVertexBufferPtr->EntityID = entityID;
			#endif // MODE_EDITOR

			s_RendererData->QuadVertexBufferPtr++;
		}

		s_RendererData->QuadIndexCount += 6;
		s_RendererData->Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor)
	{
		if (s_RendererData->QuadIndexCount >= Renderer2DStorage::MaxIndices)
		{
			FlushAndResetQuad();
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
			s_RendererData->QuadVertexBufferPtr->Thickness = 1.0f;

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
			FlushAndResetQuad();
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
			FlushAndResetQuad();
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
			s_RendererData->QuadVertexBufferPtr->Thickness = 1.0f;

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
			FlushAndResetQuad();
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
			s_RendererData->QuadVertexBufferPtr->Thickness = 1.0f;

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