#include "BluEditorLayer.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Blu/Events/MouseEvent.h"
#include "imgui.h"
#include <glad/glad.h>
#include "GLFW/glfw3.h"





namespace Blu
{
	BluEditorLayer::BluEditorLayer()
		:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
	{

	}

	void BluEditorLayer::OnAttach()
	{
		m_Texture = Blu::Texture2D::Create("assets/textures/StickMan.png");
		m_WallpaperTexture = Blu::Texture2D::Create("assets/spriteSheets/blockPack_spritesheet@2.png");

		m_ParticleProps.Position = glm::vec2(-0.5f, 1.0f);
		m_ParticleProps.Velocity = glm::vec2(1.0f, 0.0f);
		m_ParticleProps.ColorBegin = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
		m_ParticleProps.ColorEnd = glm::vec4(0.0f, 0.0f, 0.5f, 1.0f);
		m_ParticleProps.SizeBegin = 1.0f;
		m_ParticleProps.SizeEnd = 0.0f;
		m_ParticleProps.SizeVariation = 0.5f;
		m_ParticleProps.LifeTime = 10.0f;

		Blu::FrameBufferSpecifications fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_FrameBuffer = Blu::FrameBuffer::Create(fbSpec);

	}

	void BluEditorLayer::OnDetach()
	{
	}

	void BluEditorLayer::OnUpdate(Blu::Timestep deltaTime)
	{
		Blu::Renderer2D::ResetStats();
		m_FrameBuffer->Bind();
		
		BLU_PROFILE_FUNCTION();
		{

			BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ");
			if (m_ViewPortFocused)
			{
				m_CameraController.OnUpdate(deltaTime);
			}
		}

		Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Blu::RenderCommand::Clear();

		Blu::Renderer2D::BeginScene(m_CameraController.GetCamera());

		m_ParticleProps.Position = glm::vec2((m_MousePosX / 100.f) - 8, -m_MousePosY / 100.0f + 5);


		m_ParticleSystem.Emit(m_ParticleProps);
		m_ParticleSystem.OnUpdate(deltaTime);

		m_ParticleSystem.OnRender();

		static float rotation = 0.0f;
		rotation += deltaTime * 150.0f;
		Blu::Renderer2D::DrawRotatedQuad({ -1, 0 }, { 1, 1 }, glm::radians(rotation), { 1.0f ,1.0f ,0.0f ,1.0f });

		Blu::Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);




		Blu::Renderer2D::EndScene();
		m_FrameBuffer->UnBind();



	}

	void BluEditorLayer::OnEvent(Blu::Events::Event& event)
	{
		m_CameraController.OnEvent(event);
		if (event.GetType() == Blu::Events::Event::Type::MouseMoved)
		{
			Events::MouseMovedEvent& e = static_cast<Events::MouseMovedEvent&>(event);

			OnMouseMovedEvent(e);
		}
		if (event.GetType() == Blu::Events::Event::Type::MouseButtonPressed)
		{
			
			Events::MouseButtonPressedEvent& e = static_cast<Events::MouseButtonPressedEvent&>(event);
			OnMouseButtonPressed(e);
		}
		if (event.GetType() == Blu::Events::Event::Type::MouseButtonReleased)
		{
			Events::MouseButtonReleasedEvent& e = static_cast<Events::MouseButtonReleasedEvent&>(event);
			
			OnMouseButtonReleased(e);
		}
		if (event.GetType() == Blu::Events::Event::Type::KeyPressed)
		{
			Events::KeyPressedEvent& e = static_cast<Events::KeyPressedEvent&>(event);
			OnKeyPressedEvent(e);
		}
		if (event.GetType() == Blu::Events::Event::Type::KeyPressed)
		{
			Events::KeyReleasedEvent& e = static_cast<Events::KeyReleasedEvent&>(event);
			OnKeyReleasedEvent(e);
		}
		if (event.GetType() == Blu::Events::Event::Type::KeyPressed)
		{
			Events::KeyTypedEvent& e = static_cast<Events::KeyTypedEvent&>(event);
			OnKeyTypedEvent(e);
		}
	}

	void BluEditorLayer::OnGuiDraw()
	{
		

		ImGui::Begin("Renderer2D Statistics");
		
		if (Blu::GuiManager::BeginMenu("Renderer2D Statistics"))
		{
			Blu::GuiManager::Text("Draw Calls: %d", Blu::Renderer2D::GetStats().DrawCalls); 
			Blu::GuiManager::Text("Vertex Count: %d", Blu::Renderer2D::GetStats().GetTotalVertexCount());
			Blu::GuiManager::Text("Quad Count: %d", Blu::Renderer2D::GetStats().QuadCount);
			Blu::GuiManager::EndMenu();
		}
		ImGui::End();
		
		Blu::GuiManager::ClearColor();
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		m_ViewPortFocused = ImGui::IsWindowFocused();
	
		Application::Get().GetImGuiLayer()->SetBlockEvents(!m_ViewPortFocused);
	
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *(glm::vec2*)&viewportSize)
		{
			m_FrameBuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_ViewportSize = { viewportSize.x, viewportSize.y };	
			m_CameraController.ResizeCamera(viewportSize.x, viewportSize.y);


		}
		uint32_t textureID = m_FrameBuffer->GetColorAttachment();
		ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y}, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::PopStyleVar();
		ImGui::End();
		
	}

	bool BluEditorLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[event.GetButton()] = true;
		event.Handled = true;

		return false;



	}
	bool BluEditorLayer::OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[event.GetButton()] = false;
		event.Handled = true;
		return false;


	}
	bool BluEditorLayer::OnMouseScrolledEvent(Events::MouseScrolledEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheel = event.GetYOffset();
		io.MouseWheelH = event.GetXOffset();
		event.Handled = true;
		return false;

	}
	bool BluEditorLayer::OnMouseMovedEvent(Events::MouseMovedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = ImVec2(event.GetX(), event.GetY());
		event.Handled = true;
		return false;
	}

	bool BluEditorLayer::OnKeyPressedEvent(Events::KeyPressedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[event.GetKeyCode()] = true;
		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

		event.Handled = true;
		return false;

	}

	bool BluEditorLayer::OnKeyReleasedEvent(Events::KeyReleasedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[event.GetKeyCode()] = false;
		event.Handled = true;
		return false;

	}

	bool BluEditorLayer::OnKeyTypedEvent(Events::KeyTypedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		int KeyCode = event.GetKeyCode();
		if (KeyCode > 0 && KeyCode < 0x10000)
		{
			io.AddInputCharacter((unsigned short)KeyCode);
		}
		event.Handled = true;
		return false;

	}

	

}