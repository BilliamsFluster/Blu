//#include "Azure2D.h"
//#include "Blu/Rendering/Renderer2D.h"
//#include <glm/gtc/matrix_transform.hpp>
//#include <chrono>
//#include "Blu/Events/MouseEvent.h"
//
//
//
//
//
//
//Azure2D::Azure2D()
//	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
//{
//
//}
//
//void Azure2D::OnAttach()
//{
//	m_Texture = Blu::Texture2D::Create("assets/textures/StickMan.png");
//	m_WallpaperTexture = Blu::Texture2D::Create("assets/spriteSheets/blockPack_spritesheet@2.png");
//	
//	
//}
//
//void Azure2D::OnDetach()
//{
//}
//
//void Azure2D::OnUpdate(Blu::Timestep deltaTime)
//{
//	Blu::Renderer2D::ResetStats();
//	m_FrameBuffer->Bind();
//	BLU_PROFILE_FUNCTION();
//	{
//
//		
//	}
//	
//	Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
//	Blu::RenderCommand::Clear();
//	
//	//Blu::Renderer2D::BeginScene(m_CameraController.GetCamera()); 
//	
//
//
//
//	
//
//	//Blu::Renderer2D::EndScene();
//	m_FrameBuffer->UnBind();
//
//	 
//
//}
//
//void Azure2D::OnEvent(Blu::Events::Event& event)
//{
//	m_CameraController.OnEvent(event);
//	if (event.GetType() == Blu::Events::Event::Type::MouseMoved)
//	{
//		OnMouseMoved(event);
//	}
//	if (event.GetType() == Blu::Events::Event::Type::MouseButtonPressed)
//	{
//		OnMousePressed(event);
//	}
//	if (event.GetType() == Blu::Events::Event::Type::MouseButtonReleased)
//	{
//		OnMouseButtonReleased(event);
//	}
//}
//
//void Azure2D::OnGuiDraw()
//{
//	
//	uint32_t textureID = m_FrameBuffer->GetColorAttachment();
//	Blu::GuiManager::Image(textureID, { 1280.0f, 720.0f });
//	if (Blu::GuiManager::BeginMenu("Renderer2D Statistics"))
//	{
//		Blu::GuiManager::Text("Draw Calls: %d", Blu::Renderer2D::GetStats().DrawCalls);
//		Blu::GuiManager::Text("Index Count: %d", Blu::Renderer2D::GetStats().GetTotalIndexCount());
//		Blu::GuiManager::Text("Vertex Count: %d", Blu::Renderer2D::GetStats().GetTotalVertexCount());
//		Blu::GuiManager::Text("Quad Count: %d", Blu::Renderer2D::GetStats().QuadCount);
//		Blu::GuiManager::EndMenu();
//	}
//	static bool open = true;
//	Blu::GuiManager::ShowDockSpace(&open);
//}
//
//void Azure2D::OnMouseMoved(Blu::Events::Event& event)
//{
//	Blu::Events::MouseMovedEvent& MouseEvent = dynamic_cast<Blu::Events::MouseMovedEvent&>(event);
//	m_MousePosX = MouseEvent.GetX();
//	m_MousePosY = MouseEvent.GetY();
//	Blu::GuiManager::OnMouseMovedEvent(MouseEvent);
//	MouseEvent.Handled = true;
//
//}
//
//
//void Azure2D::OnMouseButtonReleased(Blu::Events::Event& event)
//{
//	Blu::Events::MouseButtonReleasedEvent& MouseEvent = dynamic_cast<Blu::Events::MouseButtonReleasedEvent&>(event);
//
//	Blu::GuiManager::OnMouseButtonReleased(MouseEvent);
//	MouseEvent.Handled = true;
//
//}
//
//
//void Azure2D::OnMousePressed(Blu::Events::Event& event)
//{
//	Blu::Events::MouseButtonPressedEvent& MouseEvent = dynamic_cast<Blu::Events::MouseButtonPressedEvent&>(event);
//	Blu::GuiManager::OnMouseButtonPressed(MouseEvent);
//	MouseEvent.Handled = true;
//
//}
//
