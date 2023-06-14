#pragma once
#include "RenderCommand.h"
#include "OrthographicCamera.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	

	class Renderer
	{
	public:
		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Init();
		static void Submit(const Shared<VertexArray>& vertexArray, const Shared<Shader>& shader, const glm::mat4& transform = glm::mat4(1.0f));

		inline static const RendererAPI::API GetAPI()  { return RendererAPI::GetAPI(); }
		
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* m_SceneData;
	};

}

