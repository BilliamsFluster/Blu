#include "Blupch.h"
#include "Renderer.h"
#include "Blu/Rendering/Shader.h"
#include "Renderer2D.h"
#include "Renderer3D.h"

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
		BLU_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		Renderer3D::Init();
	}

	void Renderer::Shutdown()
	{
		BLU_PROFILE_FUNCTION();

		Renderer3D::Shutdown();
		Renderer2D::Shutdown();
		delete m_SceneData;
		m_SceneData = nullptr;
		delete m_ShaderLibrary;
		m_ShaderLibrary = nullptr;
	}
	void Renderer::Submit(const Shared<VertexArray>& vertexArray, const Shared<Shader>& shader, const glm::mat4& transform)
	{
		shader->Bind();
		shader->SetUniformMat4("u_ViewProjectionMatrix", m_SceneData->ViewProjectionMatrix);
		shader->SetUniformMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		if (width > 0 && height > 0)
		{
			RenderCommand::SetViewport(0, 0, width, height);
		}
		else
		{
			width, height = 0;
		}
	}
}