#include "Azure2D.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>




Azure2D::Azure2D()
	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Azure2D::OnAttach()
{
	m_Texture = Blu::Texture2D::Create("assets/textures/StickMan.png");


}

void Azure2D::OnDetach()
{
}

void Azure2D::OnUpdate(Blu::Timestep deltaTime)
{
	m_CameraController.OnUpdate(deltaTime);
	Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Blu::RenderCommand::Clear();
	
	Blu::Renderer2D::BeginScene(m_CameraController.GetCamera());
	
	Blu::Renderer2D::DrawQuad({ 0, 0 }, { 1, 1 }, m_Texture);
	
	

	Blu::Renderer2D::EndScene();
}

void Azure2D::OnEvent(Blu::Events::Event& event)
{
	m_CameraController.OnEvent(event);
}
