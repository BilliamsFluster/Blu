#pragma once
#include "RenderCommand.h"
#include "OrthographicCamera.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"

namespace Blu
{
	

	class Renderer
	{
	public:
		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Submit(const std::shared_ptr<VertexArray>& vertexArray, const std::shared_ptr<OpenGLShader>& shader);

		inline static const RendererAPI::API GetAPI()  { return RendererAPI::GetAPI(); }
		
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* m_SceneData;
	};

}

