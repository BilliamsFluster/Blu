#pragma once
#include "Blu/Rendering/RendererAPI.h"

namespace Blu
{
	class OpenGLRendererAPI: public RendererAPI
	{
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;
		virtual void Clear() override;
	};

}

