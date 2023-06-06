#pragma once
#include "glm.hpp"
#include <memory>
#include "VertexArray.h"
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
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) = 0;
		virtual void Clear() = 0;

		inline static API GetAPI() { return s_API; }

	private:
		static API s_API;

	};
}


