#pragma once
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class OpenGLRendererAPI: public RendererAPI
	{
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void DrawIndexed(const Shared<VertexArray>& vertexArray) override;
		virtual void Clear() override;
		virtual void Init() override;
	};

}

