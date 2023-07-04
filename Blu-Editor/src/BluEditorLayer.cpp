#include "BluEditorLayer.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Blu/Events/MouseEvent.h"
#include "imgui.h"
#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include <glm/gtc/type_ptr.hpp>
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Utils/PlatformUtils.h"
#include "ImGuizmo.h"
#include "Blu/Math/Math.h"





namespace Blu
{
	BluEditorLayer::BluEditorLayer()
		:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
	{

	}

	void BluEditorLayer::OnAttach()
	{
		m_ActiveScene = std::make_shared<Scene>();
		m_SceneHierarchyPanel = std::make_shared<SceneHierarchyPanel>();
		m_Texture = Texture2D::Create("assets/textures/StickMan.png");
		m_WallpaperTexture = Texture2D::Create("assets/spriteSheets/blockPack_spritesheet@2.png");


		FrameBufferSpecifications fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_FrameBuffer = FrameBuffer::Create(fbSpec);
		ImGuiIO& io = ImGui::GetIO();

		// Key map setup
		io.KeyMap[ImGuiKey_Tab] = BLU_KEY_TAB; // And so on for all other keys...
		io.KeyMap[ImGuiKey_LeftArrow] = BLU_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = BLU_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = BLU_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = BLU_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = BLU_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = BLU_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = BLU_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = BLU_KEY_END;
		io.KeyMap[ImGuiKey_Insert] = BLU_KEY_INSERT;
		io.KeyMap[ImGuiKey_Delete] = BLU_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = BLU_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Space] = BLU_KEY_SPACE;
		io.KeyMap[ImGuiKey_Enter] = BLU_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = BLU_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_A] = BLU_KEY_A;
		io.KeyMap[ImGuiKey_C] = BLU_KEY_C;
		io.KeyMap[ImGuiKey_V] = BLU_KEY_V;
		io.KeyMap[ImGuiKey_X] = BLU_KEY_X;
		io.KeyMap[ImGuiKey_Y] = BLU_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = BLU_KEY_Z;
		

		m_CameraEntity = m_ActiveScene->CreateEntity("Camera");
		m_CameraEntity.AddComponent<CameraComponent>();
		m_CameraEntity.GetComponent<CameraComponent>().Primary = true;
		m_CameraEntity.AddComponent<NativeScriptComponent>().Bind<CameraController>();

		
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
	}

	void BluEditorLayer::OnDetach()
	{
	}

	void BluEditorLayer::OnUpdate(Timestep deltaTime)
	{
		
		
		
		Renderer2D::ResetStats();
		{
			BLU_PROFILE_SCOPE("Renderer2D::ResetStats: ");
			m_FrameBuffer->Bind();
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();
		}

		BLU_PROFILE_FUNCTION();
		{

			BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ");
			if (m_ViewPortFocused)
			{
				//m_CameraController.OnUpdate(deltaTime);
			}
		}
		
		m_ActiveScene->OnUpdate(deltaTime);

		m_FrameBuffer->UnBind();



	}

	void BluEditorLayer::OnEvent(Events::Event& event)
	{
		m_CameraController.OnEvent(event);
		switch (event.GetType())
		{
			case Events::Event::Type::MouseMoved:
			{
				Events::MouseMovedEvent& e = static_cast<Events::MouseMovedEvent&>(event);
				OnMouseMovedEvent(e);
				break;
			}
		
			case Events::Event::Type::MouseButtonPressed:
			{
				Events::MouseButtonPressedEvent& e = static_cast<Events::MouseButtonPressedEvent&>(event);
				OnMouseButtonPressed(e);
				break;
			}
			case Events::Event::Type::MouseButtonReleased:
			{
				Events::MouseButtonReleasedEvent& e = static_cast<Events::MouseButtonReleasedEvent&>(event);
				OnMouseButtonReleased(e);
				break;
			}
			case Events::Event::Type::MouseScrolled:
			{
				Events::MouseScrolledEvent& e = static_cast<Events::MouseScrolledEvent&>(event);
				OnMouseScrolledEvent(e);
				break;
			}
			case Events::Event::Type::KeyPressed:
			{
				Events::KeyPressedEvent& e = static_cast<Events::KeyPressedEvent&>(event);
				OnKeyPressedEvent(e);
				break;
			}
			case Events::Event::Type::KeyReleased:
			{
				Events::KeyReleasedEvent& e = static_cast<Events::KeyReleasedEvent&>(event);
				OnKeyReleasedEvent(e);
				break;
			}
			case Events::Event::Type::KeyTyped:
			{
				Events::KeyTypedEvent& e = static_cast<Events::KeyTypedEvent&>(event);
				OnKeyTypedEvent(e);
				break;
			}
			case Events::Event::Type::WindowResize:
			{
				Events::WindowResizeEvent& e = static_cast<Events::WindowResizeEvent&>(event);
				OnWindowResizedEvent(e);
				break;


			}
		}
	}
		
		
		
	bool BluEditorLayer::OnWindowResizedEvent(Events::WindowResizeEvent& event)
	{
		BLU_PROFILE_FUNCTION();
		ImGuiIO& io = ImGui::GetIO();
		// Update the display size
		io.DisplaySize = ImVec2(event.GetWidth(), event.GetHeight());
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // Assuming no scale here

		return false;
	}

	void BluEditorLayer::NewScene()
	{
		m_ActiveScene = std::make_shared<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
	}

	void BluEditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Blu Scene (*.blu)\0*.blu\0");
		if (!filepath.empty())
		{
			m_ActiveScene = std::make_shared<Scene>();
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel->SetContext(m_ActiveScene);

			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(filepath);

		}
	}

	void BluEditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("Blu Scene (*.blu)\0*.blu\0");
		if (!filepath.empty())
		{
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filepath);
		}
	}

	void BluEditorLayer::OnGuiDraw()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New", "Ctrl+N"))
				{
					NewScene();
				}
				if (ImGui::MenuItem("Open...", "Ctrl+O"))
				{
					OpenScene();
				}
				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
				{
					SaveSceneAs();

				}
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();

			}
			ImGui::EndMainMenuBar();
		}
		m_SceneHierarchyPanel->OnImGuiRender();

		ImGui::Begin("Renderer2D Statistics");

		if (GuiManager::BeginMenu("Renderer2D Statistics"))
		{
			GuiManager::Text("Draw Calls: %d", Renderer2D::GetStats().DrawCalls); 
			GuiManager::Text("Vertex Count: %d", Renderer2D::GetStats().GetTotalVertexCount());
			GuiManager::Text("Quad Count: %d", Renderer2D::GetStats().QuadCount);
			GuiManager::EndMenu();
		}
		
		ImGui::End();
		
		GuiManager::ClearColor();
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		m_ViewPortFocused = ImGui::IsWindowFocused();
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *(glm::vec2*)&viewportSize)
		{
			m_ViewportSize = { viewportSize.x, viewportSize.y };
			m_FrameBuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.ResizeCamera(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize(m_ViewportSize.x, m_ViewportSize.y);
			BLU_CORE_ERROR("ViewportSize x: {0}, viewport size y: {1}", m_ViewportSize.x, m_ViewportSize.y);
		}
	
		
		uint32_t textureID = m_FrameBuffer->GetColorAttachment();
		ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y}, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		//Guizmos
		Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
		if (selectedEntity && m_ImGuizmoType != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			float windowWidth = (float)ImGui::GetWindowWidth();
			float windowHeight = (float)ImGui::GetWindowHeight();
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

			// Camera
			auto cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
			const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;

			const glm::mat4& cameraProjection = camera.GetProjectionMatrix();
			glm::mat4 cameraView = glm::inverse(cameraEntity.GetComponent<TransformComponent>().GetTransform());

			//Entity Transform
			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			
			//snapping 
			
			
			if (enableTranslationSnap && m_ImGuizmoType == ImGuizmo::OPERATION::TRANSLATE)
			{
				ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
					(ImGuizmo::OPERATION)m_ImGuizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, &translationSnapValue);
			}
			else if (enableRotationSnap && m_ImGuizmoType == ImGuizmo::OPERATION::ROTATE)
			{
				ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
					(ImGuizmo::OPERATION)m_ImGuizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, &rotationSnapValue);
			}
			else if (enableScaleSnap && m_ImGuizmoType == ImGuizmo::OPERATION::SCALE)
			{
				ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
					(ImGuizmo::OPERATION)m_ImGuizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, &scaleSnapValue);
			}
			else
			{
				ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
					(ImGuizmo::OPERATION)m_ImGuizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr);
			}
			

			if (ImGuizmo::IsUsing())
			{
				glm::vec3 translation, rotation, scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);
				
				glm::vec3 deltaRotation =  rotation - tc.Rotation;
				tc.Translation = translation;
				tc.Rotation += deltaRotation;
				tc.Scale = scale;
			}
		}
		static const char* items[] = { "Translation", "Rotation", "Scale" };
		static int current_item = 0;

		// Position the 'tab' button at the top right corner of the viewport
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110); // Keep dropdown on the right
		ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 665);
		static bool showSnappingDropdown;
		if (ImGui::Button("Snapping Options"))
		{
			showSnappingDropdown = !showSnappingDropdown; // Toggle dropdown visibility when button is pressed
		}

		if (showSnappingDropdown)
		{
			// Show the dropdown below the 'tab' button
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110); // Keep dropdown on the right
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 640);

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{ 0.05f, 0.055f, 0.05f, 1.0f }); // Push a new color for window backgrounds
			ImGui::BeginChild("dropdown", ImVec2(120, 110), true);

			ImGui::Text("Snapping");
			ImGui::Separator();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));  // Set padding to zero
			ImGui::Combo("##Snapping", &current_item, items, IM_ARRAYSIZE(items));
			ImGui::PopStyleVar();  // Restore previous padding

			switch (current_item)
			{
			case 0: // Translation
				ImGui::Checkbox("Enabled", &enableTranslationSnap);
				ImGui::SameLine();
				ImGui::Text("?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable or disable snapping for Translation");
				if (enableTranslationSnap)
				{
					ImGui::Text(" Value");
					ImGui::SameLine();
					ImGui::InputFloat("##Value", &translationSnapValue);
				}
				break;

			case 1: // Rotation
				ImGui::Checkbox("Enabled", &enableRotationSnap);
				ImGui::SameLine();
				ImGui::Text("?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable or disable snapping for Rotation");
				if (enableRotationSnap)
				{
					ImGui::Text(" Value");
					ImGui::SameLine();
					ImGui::InputFloat("##Value", &rotationSnapValue);
				}
				break;

			case 2: // Scale
				ImGui::Checkbox("Enabled", &enableScaleSnap);
				ImGui::SameLine();
				ImGui::Text("?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable or disable snapping for Scale");
				if (enableScaleSnap)
				{
					ImGui::Text(" Value");
					ImGui::SameLine();
					ImGui::InputFloat("##Value", &scaleSnapValue);
				}
				break;
			}

			ImGui::EndChild();
			ImGui::PopStyleColor();

		}

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
		io.KeyCtrl = io.KeysDown[BLU_KEY_LEFT_CONTROL] || io.KeysDown[BLU_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[BLU_KEY_LEFT_SHIFT] || io.KeysDown[BLU_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[BLU_KEY_LEFT_ALT] || io.KeysDown[BLU_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[BLU_KEY_LEFT_SUPER] || io.KeysDown[BLU_KEY_RIGHT_SUPER];

		bool control = Input::IsKeyPressed(BLU_KEY_LEFT_CONTROL) || Input::IsKeyPressed(BLU_KEY_RIGHT_CONTROL);
		bool shift = Input::IsKeyPressed(BLU_KEY_LEFT_SHIFT) || Input::IsKeyPressed(BLU_KEY_RIGHT_SHIFT);
		switch (event.GetKeyCode())
		{
		case BLU_KEY_O:
		{
			if (control)
			{
				OpenScene();
			}
			break;
		}
		case BLU_KEY_N:
		{
			if (control)
			{
				NewScene();
			}
			break;
		}
		case BLU_KEY_S:
		{
			if (control && shift)
			{
				SaveSceneAs();
			}
			break;
		}
		case BLU_KEY_Q:
			m_ImGuizmoType = -1;
			break;
		case BLU_KEY_W:
			m_ImGuizmoType = ImGuizmo::OPERATION::TRANSLATE;
			break;
		case BLU_KEY_E:
			m_ImGuizmoType = ImGuizmo::OPERATION::ROTATE;
			break;
		case BLU_KEY_R:
			m_ImGuizmoType = ImGuizmo::OPERATION::SCALE;
			break;
		
		}
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