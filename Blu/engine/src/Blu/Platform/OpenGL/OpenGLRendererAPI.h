#pragma once
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class OpenGLRendererAPI: public RendererAPI
	{
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void DrawIndexed(const Shared<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void Clear() override;
		virtual void Init() override;
	};

}

