#pragma once
#include "RenderCommand.h"

namespace Blu
{
	

	class Renderer
	{
	public:
		static void BeginScene();
		static void EndScene();

		static void Submit(const std::shared_ptr<VertexArray>& vertexArray);

		inline static const RendererAPI::API GetAPI()  { return RendererAPI::GetAPI(); }
		
	};

}

