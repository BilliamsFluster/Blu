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
		static void EndScene();

		//primitives

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Shared<Texture2D>& texture);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Shared<Texture2D>& texture);
	};

}


