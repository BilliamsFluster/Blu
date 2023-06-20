#pragma once
#include "RendererAPI.h"
namespace Blu
{
	class RenderCommand
	{
	public:
		inline static void Init()
		{
			BLU_PROFILE_FUNCTION();

			s_RendererAPI->Init();
		}
		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}
		inline static void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray)
		{
			s_RendererAPI->DrawIndexed(vertexArray);
		}
		inline static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}
		inline static void Clear()
		{
			s_RendererAPI->Clear();
		}

	private:
		static RendererAPI* s_RendererAPI;
	};
}
