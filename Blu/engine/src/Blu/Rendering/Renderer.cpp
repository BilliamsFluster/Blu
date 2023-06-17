#include "Blupch.h"
#include "Renderer.h"
#include "Blu/Rendering/Shader.h"

namespace Blu
{
	ShaderLibrary* Renderer::m_ShaderLibrary = new ShaderLibrary();

	Renderer::SceneData* Renderer::m_SceneData = new Renderer::SceneData();
	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		m_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}
	void Renderer::EndScene()
	{
	}
	void Renderer::Init()
	{
		RenderCommand::Init();
	}
	void Renderer::Submit(const Shared<VertexArray>& vertexArray, const Shared<Shader>& shader, const glm::mat4& transform)
	{
		shader->Bind();
		// wouldnt cast with multiple API's its ok for only one atm
		std::dynamic_pointer_cast<OpenGLShader>(shader)->SetUniformMat4("u_ViewProjectionMatrix", m_SceneData->ViewProjectionMatrix);
		std::dynamic_pointer_cast<OpenGLShader>(shader)->SetUniformMat4("u_Transform", transform); // aka model matrix

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}
}