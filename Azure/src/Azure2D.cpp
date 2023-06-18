#include "Azure2D.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>




Azure2D::Azure2D()
	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Azure2D::OnAttach()
{
	





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
	Blu::Renderer2D::DrawQuad({ 0, 0 }, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 2, 0 }, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 0, 1 }, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 2, 1 }, { 1, 1 }, { 1, 0, 0, 1 });
	
	Blu::Renderer2D::DrawQuad({ 0, 2 }, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 2, 2 }, { 1, 1 }, { 1, 0, 0, 1 });
	
	
	Blu::Renderer2D::DrawQuad({ 1, 2}, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 1, 3}, { 1, 1 }, { 1, 1, 0, 1 });

	Blu::Renderer2D::DrawQuad({ -1, 2 }, { 1, 1 }, { 1, 0, 0, 1 });
	Blu::Renderer2D::DrawQuad({ 3, 2 }, { 1, 1 }, { 1, 0, 0, 1 });
	
	Blu::Renderer2D::DrawQuad({ 3, 3 }, { 0.5, 1 }, { 128, 128, 128, 1 });
	Blu::Renderer2D::DrawQuad({ 3, 4 }, { 0.5, 1 }, { 128, 128, 128, 1 });
	
	

	Blu::Renderer2D::EndScene();
}

void Azure2D::OnEvent(Blu::Events::Event& event)
{
	m_CameraController.OnEvent(event);
}
