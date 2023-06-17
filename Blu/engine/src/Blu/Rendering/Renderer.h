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
		static void OnWindowResize(uint32_t width, uint32_t height);
		inline static const RendererAPI::API GetAPI()  { return RendererAPI::GetAPI(); }
		inline static  ShaderLibrary* GetShaderLibrary() { return m_ShaderLibrary; }
		
		
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* m_SceneData;
		static ShaderLibrary* m_ShaderLibrary;
	};

}

