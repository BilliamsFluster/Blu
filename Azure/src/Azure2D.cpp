#include "Azure2D.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Blu/Events/MouseEvent.h"




Azure2D::Azure2D()
	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Azure2D::OnAttach()
{
	m_Texture = Blu::Texture2D::Create("assets/textures/StickMan.png");
	
	m_ParticleProps.Position = glm::vec2(0.0f, 0.0f);
	m_ParticleProps.Velocity = glm::vec2(1.0f, 0.0f);
	m_ParticleProps.ColorBegin = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // red
	m_ParticleProps.ColorEnd = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // blue
	m_ParticleProps.SizeBegin = 1.0f;
	m_ParticleProps.SizeEnd = 0.0f;
	m_ParticleProps.SizeVariation = 0.5f;  
	m_ParticleProps.LifeTime = 10.0f;

}

void Azure2D::OnDetach()
{
}

void Azure2D::OnUpdate(Blu::Timestep deltaTime)
{
	BLU_PROFILE_FUNCTION();
	{

		BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ")
		m_CameraController.OnUpdate(deltaTime);
	}
	
	Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Blu::RenderCommand::Clear();
	
	Blu::Renderer2D::BeginScene(m_CameraController.GetCamera()); // causes error
	
	m_ParticleProps.Position = glm::vec2( (m_MousePosX/ 100.f) -5 , -m_MousePosY /100.0f);

	m_ParticleSystem.Emit(m_ParticleProps);
	

	m_ParticleSystem.OnUpdate(deltaTime);
	m_ParticleSystem.OnRender();
	
	static float rotation = 0.0f;
	rotation += deltaTime * 150.0f;
	Blu::Renderer2D::DrawRotatedQuad({ -1, 0 }, { 1, 1 }, glm::radians(rotation), { 1.0f ,1.0f ,0.0f ,1.0f });

	Blu::Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);
	//Blu::Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1,0,1,1 });


	

	Blu::Renderer2D::EndScene();
}

void Azure2D::OnEvent(Blu::Events::Event& event)
{
	m_CameraController.OnEvent(event);
	if (event.GetType() == Blu::Events::Event::Type::MouseMoved)
	{
		OnMouseMoved(event);
	}
}

void Azure2D::OnMouseMoved(Blu::Events::Event& event)
{
	Blu::Events::MouseMovedEvent& MouseEvent = dynamic_cast<Blu::Events::MouseMovedEvent&>(event);
	m_MousePosX = MouseEvent.GetX();
	m_MousePosY = MouseEvent.GetY();
}
