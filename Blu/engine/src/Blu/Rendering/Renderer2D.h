#pragma once
#include "OrthographicCamera.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();
		static void BeginScene(const OrthographicCamera& camera);
		static void Flush();
		static void EndScene();


		//primitives
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor = 1.0f);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor = 1.0f);
		
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor = 1.0f);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const glm::vec4& color, float tilingFactor = 1.0f);

		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation, const Shared<Texture2D>& texture, float tilingFactor = 1.0f);

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
	private:
		static void FlushAndReset();
	};

}


