#include "BluEditorLayer.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Blu/Events/MouseEvent.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include <glm/gtc/type_ptr.hpp>
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Utils/PlatformUtils.h"
#include "ImGuizmo.h"
#include "Blu/Math/Math.h"
#include "Blu/Core/Application.h"
#include "Blu/Platform/Windows/WindowsWindow.h"
#include "Blu/Scripting/ScriptEngine.h"





namespace Blu
{
	BluEditorLayer::BluEditorLayer()
		:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
	{
		GLenum error = glGetError();  // Consume any existing errors
		if (error != GL_NO_ERROR) {
			std::cout << "OpenGL error in Blu editor layer on construction: " << error << std::endl;
		}
	}

	void BluEditorLayer::OnAttach()
	{
		m_ActiveScene = std::make_shared<Scene>();
		m_SceneHierarchyPanel = std::make_shared<SceneHierarchyPanel>();
		m_ContentBrowserPanel = std::make_shared<ContentBrowserPanel>();
		
		
		m_Texture = Texture2D::Create("assets/textures/StickMan.png");
		m_AppHeaderIcon = Texture2D::Create("assets/textures/BluLogo.png");
		
		m_PlayIcon = Texture2D::Create("assets/textures/PlayButton.png");
		m_PauseIcon = Texture2D::Create("assets/textures/PauseButton.png");
		m_StopIcon = Texture2D::Create("assets/textures/StopButton.png");
		m_ExpandPlayOptionsIcon = Texture2D::Create("assets/textures/VerticalElipisis.png");
		m_StepIcon = Texture2D::Create("assets/textures/StepButton.png");
		
		m_TranslationIcon = Texture2D::Create("assets/textures/AxisIcon.png");
		m_RotationIcon = Texture2D::Create("assets/textures/RotateIcon.png");
		m_ScaleIcon = Texture2D::Create("assets/textures/ScaleIcon.png");
		m_WorldSpaceIcon = Texture2D::Create("assets/textures/WorldSpaceIcon.png");
		m_LocalSpaceIcon = Texture2D::Create("assets/textures/LocalSpaceIcon.png");
		m_CameraIcon = Texture2D::Create("assets/textures/CameraIcon.png");
		m_SelectIcon = Texture2D::Create("assets/textures/SelectIcon.png");
		m_SnappingIcon = Texture2D::Create("assets/textures/ToolsIcon.png");
		

		

		FrameBufferSpecifications fbSpec;
		fbSpec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::RED_INTEGER, FrameBufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_FrameBuffer = FrameBuffer::Create(fbSpec);

		FrameBufferSpecifications fbCameraSpec;
		fbCameraSpec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::Depth };
		fbCameraSpec.Width = 1280;
		fbCameraSpec.Height = 720;
		m_CameraViewFrameBuffer = FrameBuffer::Create(fbCameraSpec);

		
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

		m_EditorCamera = EditorCamera(30, 1.778f, 0.1f, 1000.0f);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
		m_OperationMode = 0; // local operation
		SceneSerializer loadSceneSerializer(m_EditorScene);

		std::string scene = loadSceneSerializer.DeserializeLoadedScene();
		if (!scene.empty())
		{
			OpenScene(scene);
		}
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
			
			m_FrameBuffer->ClearAttachment(1, -1);
		}

		BLU_PROFILE_FUNCTION();
		{

			BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ");
			
		}
		
		
		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_EditorCamera.OnUpdate(deltaTime);
				m_ActiveScene->OnUpdateEditor(deltaTime, m_EditorCamera);
				
				break;
			}
			case SceneState::Play:
			{
				
				m_ActiveScene->OnUpdateRuntime(deltaTime);
				break;
			}
			case SceneState::Pause:
			{

				m_ActiveScene->OnUpdatePaused(deltaTime); // If you would like to do anything with the time argument
				break;
			}
		}
		
		/* Clicking Functionality*/
		// Get the mouse cursor position in screen coordinates
		auto [mx, my] = ImGui::GetMousePos();

		// Adjust for the viewport position
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;

		// Adjust for camera position and zoom level
		glm::vec3 cameraPosition = m_EditorCamera.GetPosition();
		float zoomLevel = m_EditorCamera.GetDistance(); // Get the current zoom level

		//std::cout << mx << ";" << my << std::endl;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my - m_ViewportOffset.y;
		float mouseX = (float)mx;
		float mouseY = (float)my;
		m_MousePosX = mouseX;
		m_MousePosY = mouseY;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			int data = m_FrameBuffer->ReadPixel(1, mouseX, mouseY); // get the red int channel so the second attachment
			//std::cout << data << std::endl;
			m_DrawnEntityID = data;
		}



		

		

		OnOverlayRender();
		m_FrameBuffer->UnBind();
		
		m_CameraViewFrameBuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();
		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_ActiveScene->UpdateActiveCameraComponent(deltaTime); // for popup camera viewer window

			}
		}
		m_CameraViewFrameBuffer->UnBind();
		
		



	}

	void BluEditorLayer::OnEvent(Events::Event& event)
	{
		m_CameraController.OnEvent(event);
		m_EditorCamera.OnEvent(event);
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

	void BluEditorLayer::OnOverlayRender()
	{


		if (m_SceneState != SceneState::Edit)
		{
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			Renderer2D::BeginScene(camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetTransform());
		}
		else
		{
			Renderer2D::BeginScene(m_EditorCamera);

		}
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto e : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(e);

				glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size, 1.0f);
				glm::vec3 rotation = tc.Rotation;

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::scale(glm::mat4(1.0f), scale);

				glm::vec4 color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
				if(bc2d.ShowCollision)
					Renderer2D::DrawRect(translation, rotation, scale, color, 2);
			}
		}
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto e : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(e);

				glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) * glm::scale(glm::mat4(1.0f), scale);
				Renderer2D::DrawCircle(transform, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 0.05f);
			}

		}
		Renderer2D::EndScene();
	}
		
		
		
	bool BluEditorLayer::OnWindowResizedEvent(Events::WindowResizeEvent& event)
	{
		BLU_PROFILE_FUNCTION();
		ImGuiIO& io = ImGui::GetIO();
		// Update the display size
		io.DisplaySize = ImVec2(event.GetWidth(), event.GetHeight());
		if (m_ViewportSize != glm::vec2(0.0f, 0.0f) && io.DisplaySize.x > 0 && io.DisplaySize.y > 0)
		{
			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // Assuming no scale here
			m_EditorCamera.SetViewportSize(io.DisplaySize.x, io.DisplaySize.y);

		}

		return false;
	}

	void BluEditorLayer::GizmosTransform(glm::mat4& view, const glm::mat4& projection, glm::mat4& transform)
	{
		if (enableTranslationSnap && m_ImGuizmoType == ImGuizmo::OPERATION::TRANSLATE)
		{
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
				(ImGuizmo::OPERATION)m_ImGuizmoType, (ImGuizmo::MODE)m_OperationMode, glm::value_ptr(transform), nullptr, &translationSnapValue);
			return;
		}
		else if (enableRotationSnap && m_ImGuizmoType == ImGuizmo::OPERATION::ROTATE)
		{
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
				(ImGuizmo::OPERATION)m_ImGuizmoType, (ImGuizmo::MODE)m_OperationMode, glm::value_ptr(transform), nullptr, &rotationSnapValue);
			return;
		}
		else if (enableScaleSnap && m_ImGuizmoType == ImGuizmo::OPERATION::SCALE)
		{
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
				(ImGuizmo::OPERATION)m_ImGuizmoType, (ImGuizmo::MODE)m_OperationMode, glm::value_ptr(transform), nullptr, &scaleSnapValue);
			return;
		}
		else
		{
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
				(ImGuizmo::OPERATION)m_ImGuizmoType, (ImGuizmo::MODE)m_OperationMode, glm::value_ptr(transform), nullptr);
			return;
		}
	}

	void BluEditorLayer::NewScene()
	{
		m_ActiveScene = std::make_shared<Scene>();
		m_ActiveScene->OnViewportResize((float)m_ViewportSize.x, (float)m_ViewportSize.y);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
	}

	void BluEditorLayer::Toolbar()
	{
		ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float size = 20.0f;
		float windowWidth = ImGui::GetWindowWidth();
		float buttonWidth = 3 * size; // width of 3 buttons
		float spacing = 10.0f; // space between each button
		float totalWidth = buttonWidth + 2 * spacing; // total width of elements
		float offset = (windowWidth - totalWidth) / 2.0f; // calculate the offset to center the elements

		ImGui::Dummy(ImVec2(offset, 0)); // create an invisible widget to offset elements
		ImGui::SameLine();

		ImTextureID playPauseButton = nullptr;
		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Pause)
			playPauseButton = (ImTextureID)m_PlayIcon->GetRendererID();
		else if(m_SceneState == SceneState::Play)
			playPauseButton = (ImTextureID)m_PauseIcon->GetRendererID();

			
		
		
		if (ImGui::ImageButton(playPauseButton, ImVec2(size, size)))
		{
			if (m_SceneState == SceneState::Edit)
			{
				SaveCurrentScene();
				OnScenePlay();
				
			}
			else if (m_SceneState == SceneState::Play)
				OnScenePause();
			
			else if (m_SceneState == SceneState::Pause)
				OnSceneResume();
			
		}
		if (m_SceneState == SceneState::Pause)
		{
			ImGui::SameLine();
			if (ImGui::ImageButton((ImTextureID)m_StepIcon->GetRendererID(), ImVec2(size, size)))
			{
				m_ActiveScene->OnSceneStep(1);
			}


		}

		ImGui::SameLine();
		ImTextureID stopButton = m_SceneState == SceneState::Edit ? nullptr : (ImTextureID)m_StopIcon->GetRendererID();
		if (ImGui::ImageButton(stopButton, ImVec2(size, size)))
		{
			if (m_SceneState != SceneState::Edit)
			{
				OnSceneStop();
			}

		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)m_ExpandPlayOptionsIcon->GetRendererID(), ImVec2(size - 5, size)))
		{
			ImGui::OpenPopup("PlayOptions");
		}

		if (m_SceneMissing)
		{
			DisplayMissingSceneWarning();
		}
		ImGui::SameLine();
		
		if (ImGui::BeginPopup("PlayOptions"))
		{
			if (ImGui::MenuItem("Play"))
			{
				SaveCurrentScene();
				
				OnScenePlay();

			}

			if (ImGui::MenuItem("Play In New Window"))
			{
				m_PlayButtonHit = true;
				OnScenePlayNewWindow();
			}

			if (ImGui::MenuItem("Simulate"))
			{
				OnSceneSimulate();
			}

			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void BluEditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Blu Scene (*.blu)\0*.blu\0");
		if (!filepath.empty())
		{
			OpenScene(filepath);

		}
	}

	void BluEditorLayer::OpenScene(const std::filesystem::path& path)
	{
		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		m_ActiveScene = std::make_shared<Scene>();
		
		SceneSerializer serializer(m_ActiveScene);
		if (serializer.Deserialize(path.string()))
		{
			m_EditorScene = m_ActiveScene;
			m_ActiveScene->OnViewportResize((float)m_ViewportSize.x, (float)m_ViewportSize.y);
			m_SceneHierarchyPanel->SetContext(m_ActiveScene);
			ScriptEngine::OnRuntimeStart(&(*m_ActiveScene));
			m_ActiveScene->OnScriptSystemStart();
			std::filesystem::path scenePath = path;
			m_ActiveScene->SetSceneFilePath(scenePath);
			serializer.DeserializeEntityScriptInstances(path.string());

			

		}

	}

	void BluEditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("Blu Scene (*.blu)\0*.blu\0");
		if (!filepath.empty())
		{
			if (m_SceneState == SceneState::Edit)
			{
				SceneSerializer serializer(m_ActiveScene);
				serializer.Serialize(filepath);

			}
		}
	}

	void BluEditorLayer::SaveCurrentScene()
	{
		SceneSerializer serializer(m_ActiveScene);
		if (m_EditorScene)
		{
			if (m_SceneState == SceneState::Edit)
			{
				std::string filepath = m_EditorScene->GetSceneFilePath().string();
				serializer.Serialize(filepath);
			}

		}
	}

	void BluEditorLayer::OnScenePlay()
	{
		if (m_EditorScene)
		{
			m_SceneState = SceneState::Play;
			m_ViewPortFocused = true;
			ImGui::SetWindowFocus("viewport"); 

			m_PlayButtonHit = true;
			m_ActiveScene = Scene::Copy(m_EditorScene);
			ScriptEngine::OnRuntimeStart(&(*m_ActiveScene)); // do this to update the context
			m_ActiveScene->OnRuntimeStart();
			m_SceneMissing = false;
			

		}
		else
		{
			m_SceneMissing = true;
		}
	}

	void BluEditorLayer::OnScenePause()
	{
		m_SceneState = SceneState::Pause;
		m_ActiveScene->SetScenePaused(true);

	}

	void BluEditorLayer::OnSceneResume()
	{
		m_SceneState = SceneState::Play;
		m_ActiveScene->SetScenePaused(false);
	}

	void BluEditorLayer::OnSceneStop()
	{
		if (m_SceneState != SceneState::Edit)
		{
			m_ActiveScene->OnRuntimeStop();
			SceneSerializer serializer(m_ActiveScene);
			m_ActiveScene = m_EditorScene;
			std::string filepath = m_EditorScene->GetSceneFilePath().string();
			serializer.DeserializeEntityScriptInstances(filepath);
			m_PlayButtonHit = false;
			m_SceneState = SceneState::Edit;

		}
			


	}

	void BluEditorLayer::OnScenePlayNewWindow()
	{
	}

	void BluEditorLayer::OnSceneSimulate()
	{
	}
	void BluEditorLayer::DisplayMissingSceneWarning()
	{
		glm::vec2 viewportSize = m_ViewportSize;

		float windowWidth = viewportSize.x * 0.4f; // Adjust the factor as needed
		float windowHeight = viewportSize.y * 0.4f; // Adjust the factor as needed

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

		ImGui::Begin("MissingScene", nullptr, flags);


		if (ImGui::Button("X", ImVec2(20, 22)))
		{
			m_SceneMissing = false;
		}

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Missing Scene").x) * 0.5f);
		ImGui::SetWindowFontScale(1.5f); // Adjust the font size as needed
		ImGui::Text("Missing Scene");

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Scene may be missing or not active").x) * 0.5f);
		ImGui::Text("Scene may be missing or not active");

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Please use a valid scene").x) * 0.5f);
		ImGui::Text("Please use a valid scene");

		ImGui::SetWindowFontScale(1.0f); // Reset the font scale


		ImGui::End();

	}
	ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
	{
		return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
	}

	ImVec2 operator*(const ImVec2& lhs, const float& rhs)
	{
		return ImVec2(lhs.x * rhs, lhs.y * rhs);
	}

	void BluEditorLayer::UIDrawTitlebar(float& outTitlebarHeight)
	{

		if (ImGui::BeginMainMenuBar())
		{
			ImGui::Image((ImTextureID)m_AppHeaderIcon->GetRendererID(), ImVec2(30, 30), ImVec2(0, 1), ImVec2(1, 0));
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
				if (ImGui::MenuItem("Save ...", "Ctrl+S"))
				{
					SaveCurrentScene();

				}
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();

			}

			if (ImGui::BeginMenu("Script"))
			{
				if (ImGui::MenuItem("Reload Assembly", "Ctrl+R"))
				{
					SceneSerializer serializer(m_ActiveScene);
					ScriptEngine::ReloadAssembly();
					ScriptEngine::OnRuntimeStart(&(*m_ActiveScene));
					m_ActiveScene->OnScriptSystemStart();
					serializer.DeserializeEntityScriptInstances(m_ActiveScene->GetSceneFilePath().string());
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

	}

	void BluEditorLayer::OnGuiDraw()
	{
		float height = 5.0f;
		UIDrawTitlebar(height);
		
		
		m_SceneHierarchyPanel->OnImGuiRender();
		m_ContentBrowserPanel->OnImGuiRender();

		ImGui::Begin("Renderer2D Statistics");

		if (ImGui::BeginMenu("Renderer2D Statistics"))
		{
			ImGui::Text("Draw Calls: %d", Renderer2D::GetStats().DrawCalls); 
			ImGui::Text("Vertex Count: %d", Renderer2D::GetStats().GetTotalVertexCount());
			ImGui::Text("Quad Count: %d", Renderer2D::GetStats().QuadCount);
			ImGui::EndMenu();
		}
		
		ImGui::End();
		
		
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		static const char* items[] = { "Translation", "Rotation", "Scale" };
		static int current_item = 0;

		if (m_SceneState == SceneState::Edit)
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Set background color alpha to 0 for transparency
			ImGui::BeginChild("Operations", ImVec2(m_ViewportSize.x, 25), false);
		
			const ImVec2 buttonSize(16,16);
			//const float buttonSpacing = 10.0f;
			// Create image buttons for translation, rotation, scale, and world space.
			ImGui::SameLine(0, (m_ViewportSize.x - 280));
			
			if (ImGui::ImageButton((ImTextureID)m_TranslationIcon->GetRendererID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::TRANSLATE;

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Translate Selected Objects (W)"); // Tooltip text
			}

			ImGui::SameLine();

			if (ImGui::ImageButton((ImTextureID)m_RotationIcon->GetRendererID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::ROTATE;

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Rotate Selected Objects (E)"); // Tooltip text
			}

			ImGui::SameLine();

			if (ImGui::ImageButton((ImTextureID)m_ScaleIcon->GetRendererID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::SCALE;
			

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Scale Selected Objects (R)"); // Tooltip text
			}

			ImGui::SameLine(0, 40);
			static bool isLocalMode = true;
			ImTextureID spaceIcon = m_OperationMode == (int)ImGuizmo::MODE::LOCAL ? (ImTextureID)m_LocalSpaceIcon->GetRendererID() : (ImTextureID)m_WorldSpaceIcon->GetRendererID();
			if (ImGui::ImageButton(spaceIcon, buttonSize))
			{
				isLocalMode = !isLocalMode; // Toggle the mode
				m_OperationMode = isLocalMode ? (int)ImGuizmo::MODE::LOCAL : (int)ImGuizmo::MODE::WORLD;

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Toggle Local/World Space"); // Tooltip text
			}
			ImGui::SameLine(0, 40);
			if (ImGui::ImageButton((ImTextureID)m_SnappingIcon->GetRendererID(), buttonSize))
			{
				ImGui::OpenPopup("Snapping");

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Snapping Options"); // Tooltip text
			}
			if (ImGui::BeginPopup("Snapping"))
			{
				// Snapping options for Translation, Rotation, and Scale
				if (current_item >= 0 && current_item <= 2)
				{
					// Radio buttons for selecting the snapping mode
					ImGui::RadioButton("Translation", &current_item, 0);
					ImGui::SameLine();
					ImGui::RadioButton("Rotation", &current_item, 1);
					ImGui::SameLine();
					ImGui::RadioButton("Scale", &current_item, 2);
				
				


					// Display specific snapping options based on the selected mode
					switch (current_item)
					{
					case 0: // Translation
						// Common checkbox for enabling snapping
						ImGui::Checkbox("Enabled", &enableTranslationSnap);
						ImGui::Text(" Value");
						ImGui::SameLine();

						ImGui::PushItemWidth(50);
						ImGui::DragFloat("##Value", &translationSnapValue, 0.1, 0, 10000, "%.3f");


						ImGui::PopItemWidth();
						break;

					case 1: // Rotation
						// Common checkbox for enabling snapping
						ImGui::Checkbox("Enabled", &enableRotationSnap);
						ImGui::Text(" Value");
						ImGui::SameLine();
						ImGui::PushItemWidth(50);
						ImGui::DragFloat("##Value", &rotationSnapValue, 0.1, 0, 180, "%.3f");


						ImGui::PopItemWidth();
						break;

					case 2: // Scale
						// Common checkbox for enabling snapping
						ImGui::Checkbox("Enabled", &enableScaleSnap);
						ImGui::Text(" Value");
						ImGui::SameLine();
						ImGui::PushItemWidth(50);
						ImGui::DragFloat("##Value", &scaleSnapValue, 0.1, 0, 100, "%.3f");

						ImGui::PopItemWidth();
						break;
					}
				}
				ImGui::EndPopup();
			}
		
			ImGui::SameLine();

			// Camera options
			if (ImGui::ImageButton((ImTextureID)m_CameraIcon->GetRendererID(), buttonSize))
			{
				ImGui::OpenPopup("Camera");
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Camera Speed"); // Tooltip text
			}


			if (ImGui::BeginPopup("Camera"))
			{
				ImGui::Text("Editor Camera");
				ImGui::Text("Camera Speed");
				ImGui::SameLine();
				ImGui::PushItemWidth(50);
				ImGui::DragFloat("##Value", &m_EditorCamera.GetCameraSpeed(), 0.1, 0, 32, "%.4f");

				ImGui::PopItemWidth();
				ImGui::EndPopup();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
		ImGui::PopStyleVar();
		
		m_ViewPortFocused = ImGui::IsWindowFocused();
		/* Clicking */
		m_ViewportOffset = glm::vec2(ImGui::GetCursorPos().x, ImGui::GetCursorPos().y);
		auto windowSize = ImGui::GetWindowSize();
		ImVec2 minBound = ImGui::GetWindowPos(); //352;120
		minBound.x += m_ViewportOffset.x;
		minBound.y += m_ViewportOffset.y;
		ImVec2 maxBound = { minBound.x + windowSize.x , minBound.y + windowSize.y }; // 1568;818

		m_ViewportBounds[0] = { minBound.x, minBound.y };
		m_ViewportBounds[1] = { maxBound.x, maxBound.y };

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		
		if (m_ViewportSize != *(glm::vec2*)& viewportSize)
		{
			
			if (viewportSize.x > 0 && viewportSize.y > 0)
			{

				m_ViewportSize = { viewportSize.x, viewportSize.y };
			
			
				m_FrameBuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				m_CameraViewFrameBuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				m_CameraController.ResizeCamera(m_ViewportSize.x, m_ViewportSize.y);
				m_ActiveScene->OnViewportResize(m_ViewportSize.x, m_ViewportSize.y);

			}
		}
		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				auto selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
				if (selectedEntity)
				{
					if (selectedEntity.HasComponent<CameraComponent>())
					{
						if (selectedEntity.GetComponent<CameraComponent>().Primary)
						{
							ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(800, 600));
							ImGui::Begin("Camera View Window", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
							uint32_t textureIDForCameraView = m_CameraViewFrameBuffer->GetColorAttachmentID();
							ImGui::Image((void*)textureIDForCameraView, ImVec2{ ImGui::GetWindowWidth(), ImGui::GetWindowHeight() }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
							ImGui::End();

						}
					}

				}

			}
		}
		
		

		uint32_t textureID = m_FrameBuffer->GetColorAttachmentID();// Get the very first color attachment for rendering basic colors
		ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y}, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				std::filesystem::path payloadPath = std::string(reinterpret_cast<const char*>(payload->Data));
				OpenScene(payloadPath);
				m_ActiveScene->SetSceneFilePath(payloadPath);
				SceneSerializer serializer(m_EditorScene);
				serializer.SerializeLoadedScene(payloadPath.string());

			}
			ImGui::EndDragDropTarget();
		}



		//Guizmos
		Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
		if (m_SceneState == SceneState::Edit)
		{
			if (selectedEntity && m_ImGuizmoType != -1)
			{
				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist();
				float windowWidth = (float)ImGui::GetWindowWidth();
				float windowHeight = (float)ImGui::GetWindowHeight() ;
				ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
				
				//Entity Transform
				auto& tc = selectedEntity.GetComponent<TransformComponent>();
				glm::mat4 transform = tc.GetTransform();
				
				/*glm::vec3 centerPoint = tc.Translation +(tc.Scale * 0.5f);
				float gizmoOffsetY = centerPoint.y * (m_EditorCamera.GetDistance() / 100);
				std::cout << gizmoOffsetY << std::endl;
				transform[3][1] += gizmoOffsetY;*/

				

				//Editor Camera
				const glm::mat4& cameraProjection = m_EditorCamera.GetProjectionMatrix();
				glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();
			
				//snapping 
			
			
				GizmosTransform(cameraView, cameraProjection, transform);
			

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
		}
		Toolbar();
		
		ImGui::End();
		
	}

	bool BluEditorLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[event.GetButton()] = true;
		
		if (m_MousePosX >= 0 && m_MousePosY >= 0 && m_MousePosX < (int)m_ViewportSize.x && m_MousePosY < (int)m_ViewportSize.y)
		{
			
			if (m_ViewPortFocused)
			{

				Entity e = Entity{ (entt::entity)m_DrawnEntityID, m_ActiveScene.get() };
				if (e.HasComponent<TransformComponent>())
				{
					m_SceneHierarchyPanel->SetSelectedEntity(e);
					return true;

				}

			}
			

		}
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
		bool escape = Input::IsKeyPressed(BLU_KEY_ESCAPE);
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
		case BLU_KEY_ESCAPE:
		{
			if (shift)
			{
				if (m_SceneState == SceneState::Play)
				{
					OnSceneStop();
				}
			}
		}
		case BLU_KEY_D:
		{
			if (control)
			{
				Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
				m_ActiveScene->DuplicateEntity(selectedEntity);
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
				if(m_SceneState == SceneState::Edit)
					SaveSceneAs();
			}
			if (control)
			{
				if (m_SceneState == SceneState::Edit)
					SaveCurrentScene();
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