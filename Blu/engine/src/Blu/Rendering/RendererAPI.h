#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "VertexArray.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1, Direct3D = 2
		};
	public:
		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void DrawIndexed(const Shared<VertexArray>& vertexArray) = 0;
		virtual void Clear() = 0;

		inline static API GetAPI() { return s_API; }

	private:
		static API s_API;

	};
}


