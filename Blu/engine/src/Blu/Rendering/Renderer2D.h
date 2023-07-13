#pragma once
#include "OrthographicCamera.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Core/Core.h"
#include "Blu/Rendering/Camera.h"
#include "EditorCamera.h"
#include "Blu/Scene/Component.h"

namespace Blu
{
	
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();
		static void BeginScene(const OrthographicCamera& camera);
		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void Flush();
		static void EndScene();


		//primitives
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor = 1.0f);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor = 1.0f);
		
		static void DrawTexturedQuad(const glm::mat4& transform, const Shared<Texture2D>& texture, const glm::vec4& color, int entityID = -1, float tilingFactor = 1.0f);


		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		
		static void DrawRotatedQuad(const glm::mat4& transform, const glm::vec4& color, const float rotation);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor = 1.0f);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor = 1.0f);

		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		
		static void DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID);

		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;


			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
			
			void reset()
			{
				DrawCalls = 0;
				QuadCount = 0;
			}
		};
		static Statistics GetStats();
		static void ResetStats();
		static struct Renderer2DStorage* GetRendererData() { return s_RendererData.get(); }
	private:
		static void FlushAndReset();

	private:
		static Unique<Renderer2DStorage> s_RendererData;
	};

	struct Renderer2DStorage
	{
		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = (MaxQuads * 4);
		static const uint32_t MaxIndices = (MaxQuads * 6);
		static const uint32_t MaxTextureSlots = 32;
		Shared<class VertexArray> QuadVertexArray;
		Shared<class VertexBuffer> QuadVertexBuffer;
		Shared<class IndexBuffer> QuadIndexBuffer;
		Shared<class Shader> TextureShader;
		Shared<class Texture2D> WhiteTexture;
		uint32_t QuadIndexCount = 0;
		struct QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		std::array<Shared<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = WhiteTexture
		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;

	};

}


