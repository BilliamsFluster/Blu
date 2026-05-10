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
#include "Blu/Utils/Helpers.h"
#include "Blu/Rendering/RendererAPI.h"
// D3D11Context.h already pulls in <d3d11.h> — include it last so Windows headers
// don't stomp on the glad/GLFW type definitions established above.
#include "Blu/Platform/DirectX11/D3D11Context.h"





namespace Blu
{
	BluEditorLayer::BluEditorLayer()
		:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
	{
		if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL)
		{
			GLenum error = glGetError();
			if (error != GL_NO_ERROR)
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

		

		m_CameraEntity = m_ActiveScene->CreateEntity("Camera");
		m_CameraEntity.AddComponent<CameraComponent>();
		m_CameraEntity.GetComponent<CameraComponent>().Primary = true;
		m_CameraEntity.AddComponent<NativeScriptComponent>().Bind<CameraController>();

		m_EditorCamera = EditorCamera(30, 1.778f, 0.1f, 1000.0f);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
		m_OperationMode = 0; // local operation
		// Intentionally not auto-loading the last scene to avoid startup crashes from corrupted scene files.
		// Use File > Open or drag a .blu file to load a scene.

		// ---- GPU timer setup ----
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
		{
			D3D11Context* ctx = D3D11Context::Get();
			if (ctx && ctx->GetDevice())
			{
				D3D11_QUERY_DESC tsDesc  = { D3D11_QUERY_TIMESTAMP,          0 };
				D3D11_QUERY_DESC djDesc  = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
				for (int i = 0; i < 2; ++i)
				{
					ctx->GetDevice()->CreateQuery(&djDesc, reinterpret_cast<ID3D11Query**>(&m_GPUDisjointQuery[i]));
					ctx->GetDevice()->CreateQuery(&tsDesc, reinterpret_cast<ID3D11Query**>(&m_GPUTimestampBegin[i]));
					ctx->GetDevice()->CreateQuery(&tsDesc, reinterpret_cast<ID3D11Query**>(&m_GPUTimestampEnd[i]));
				}
			}
		}
		else if (glGenQueries)   // guard: ensure GLAD loaded this entry-point
		{
			glGenQueries(2, m_GLTimeQuery);
		}
	}

	void BluEditorLayer::OnDetach()
	{
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (m_GPUDisjointQuery[i])     { reinterpret_cast<ID3D11Query*>(m_GPUDisjointQuery[i])->Release();     m_GPUDisjointQuery[i] = nullptr; }
				if (m_GPUTimestampBegin[i])    { reinterpret_cast<ID3D11Query*>(m_GPUTimestampBegin[i])->Release();    m_GPUTimestampBegin[i] = nullptr; }
				if (m_GPUTimestampEnd[i])      { reinterpret_cast<ID3D11Query*>(m_GPUTimestampEnd[i])->Release();      m_GPUTimestampEnd[i] = nullptr; }
			}
		}
		else
		{
			if (m_GLTimeQuery[0] && glDeleteQueries)
				glDeleteQueries(2, m_GLTimeQuery);
		}
	}

	void BluEditorLayer::OnUpdate(Timestep deltaTime)
	{
		// ---- FPS / frame-time via wall-clock interval between OnUpdate calls ----
		// deltaTime comes from the engine's physics loop and can include vsync/present
		// blocking time, making it unreliable for display.  Measuring the wall-clock
		// interval between consecutive OnUpdate entries gives the true frame period.
		auto wallNow = std::chrono::high_resolution_clock::now();
		if (m_GPUQueryFrame > 0)  // skip the first frame (no previous timestamp yet)
		{
			m_FrameTimeMs = std::chrono::duration<float, std::milli>(wallNow - m_CpuTimerStart).count();
			m_FPS         = (m_FrameTimeMs > 0.001f) ? (1000.0f / m_FrameTimeMs) : 0.0f;

			// Throttle graph samples so the lines scroll at a readable pace
			// regardless of FPS.  kPlotIntervalMs controls the scroll speed.
			m_PerfPlotAccumMs += m_FrameTimeMs;
			if (m_PerfPlotAccumMs >= kPlotIntervalMs)
			{
				m_FrameTimePlot[m_PerfPlotOffset] = m_FrameTimeMs;
				m_FpsPlot[m_PerfPlotOffset]       = m_FPS;
				m_PerfPlotOffset  = (m_PerfPlotOffset + 1) % kPerfSamples;
				m_PerfPlotAccumMs = 0.0f;
			}
		}
		// Store this frame's wall-clock start; also serves as the CPU-work timer origin.
		m_CpuTimerStart = wallNow;

		// ---- GPU timer: read back the previous frame's result ----
		const int writeIdx = m_GPUQueryFrame & 1;
		const int readIdx  = 1 - writeIdx;

		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D && m_GPUQueryFrame >= 2)
		{
			D3D11Context* ctx = D3D11Context::Get();
			if (ctx && ctx->GetDeviceContext())
			{
				auto* dc = ctx->GetDeviceContext();
				D3D11_QUERY_DATA_TIMESTAMP_DISJOINT djData{};
				auto* djQuery = reinterpret_cast<ID3D11Query*>(m_GPUDisjointQuery[readIdx]);
				if (djQuery && dc->GetData(djQuery, &djData, sizeof(djData), 0) == S_OK && !djData.Disjoint)
				{
					UINT64 tsBegin = 0, tsEnd = 0;
					dc->GetData(reinterpret_cast<ID3D11Query*>(m_GPUTimestampBegin[readIdx]), &tsBegin, sizeof(UINT64), 0);
					dc->GetData(reinterpret_cast<ID3D11Query*>(m_GPUTimestampEnd[readIdx]),   &tsEnd,   sizeof(UINT64), 0);
					m_GpuTimeMs = static_cast<float>(tsEnd - tsBegin) / static_cast<float>(djData.Frequency) * 1000.0f;
				}
			}
		}
		else if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL && m_GPUQueryFrame >= 2
		         && glGetQueryObjectuiv && glGetQueryObjectui64v)
		{
			GLuint available = 0;
			glGetQueryObjectuiv(m_GLTimeQuery[readIdx], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available)
			{
				GLuint64 gpuNs = 0;
				glGetQueryObjectui64v(m_GLTimeQuery[readIdx], GL_QUERY_RESULT, &gpuNs);
				m_GpuTimeMs = static_cast<float>(gpuNs) / 1e6f;
			}
		}

		// ---- GPU timer: begin this frame's queries ----
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D && m_GPUDisjointQuery[writeIdx])
		{
			D3D11Context* ctx = D3D11Context::Get();
			if (ctx && ctx->GetDeviceContext())
			{
				auto* dc = ctx->GetDeviceContext();
				dc->Begin(reinterpret_cast<ID3D11Query*>(m_GPUDisjointQuery[writeIdx]));
				dc->End(reinterpret_cast<ID3D11Query*>(m_GPUTimestampBegin[writeIdx]));
			}
		}
		else if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL && glBeginQuery && m_GLTimeQuery[writeIdx])
		{
			glBeginQuery(GL_TIME_ELAPSED, m_GLTimeQuery[writeIdx]);
		}

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
				// Only feed input to the camera when the user is actually inside the
				// viewport — prevents accidental panning/rotating while typing in
				// property fields or interacting with other ImGui panels.
				if (m_ViewPortFocused || m_ViewPortHovered)
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

		
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my - m_ViewportOffset.y;
		float mouseX = (float)mx;
		float mouseY = (float)my;
		m_MousePosX = mouseX;
		m_MousePosY = mouseY;

		// m_DrawnEntityID is only needed on left-click (entity selection).
		// ReadPixel is a synchronous GPU stall — do NOT call it every frame.



		

		

		OnOverlayRender();

		// ---- GPU timer: close this frame's queries ----
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D && m_GPUDisjointQuery[writeIdx])
		{
			D3D11Context* ctx = D3D11Context::Get();
			if (ctx && ctx->GetDeviceContext())
			{
				auto* dc = ctx->GetDeviceContext();
				dc->End(reinterpret_cast<ID3D11Query*>(m_GPUTimestampEnd[writeIdx]));
				dc->End(reinterpret_cast<ID3D11Query*>(m_GPUDisjointQuery[writeIdx]));
			}
		}
		else if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL && glEndQuery && m_GLTimeQuery[writeIdx])
		{
			glEndQuery(GL_TIME_ELAPSED);
		}
		++m_GPUQueryFrame;

		// ---- CPU timer end ----
		auto cpuEnd    = std::chrono::high_resolution_clock::now();
		m_CpuTimeMs    = std::chrono::duration<float, std::milli>(cpuEnd - m_CpuTimerStart).count();

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
			playPauseButton = (ImTextureID)m_PlayIcon->GetImTextureID();
		else if(m_SceneState == SceneState::Play)
			playPauseButton = (ImTextureID)m_PauseIcon->GetImTextureID();

			
		
		
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
			if (ImGui::ImageButton((ImTextureID)m_StepIcon->GetImTextureID(), ImVec2(size, size)))
			{
				m_ActiveScene->OnSceneStep(1);
			}


		}

		ImGui::SameLine();
		ImTextureID stopButton = m_SceneState == SceneState::Edit ? nullptr : (ImTextureID)m_StopIcon->GetImTextureID();
		if (ImGui::ImageButton(stopButton, ImVec2(size, size)))
		{
			if (m_SceneState != SceneState::Edit)
			{
				OnSceneStop();
			}

		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)m_ExpandPlayOptionsIcon->GetImTextureID(), ImVec2(size - 5, size)))
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
			Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);

			m_ActiveScene->OnViewportResize((float)m_ViewportSize.x, (float)m_ViewportSize.y);
			m_SceneHierarchyPanel->SetContext(m_ActiveScene);
			ScriptEngine::OnRuntimeStart(&(*m_ActiveScene));
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
			m_ActiveScene->OnScriptSystemStart(true);
			m_SceneMissing = false;
			Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);
			

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
			Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);
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
			ImGui::Image((ImTextureID)m_AppHeaderIcon->GetImTextureID(), ImVec2(30, 30), ImVec2(0, 1), ImVec2(1, 0));
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
					m_ActiveScene->OnScriptSystemStart(false);
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

		ImGui::Begin("Rendering");

		// ---- Performance ----
		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("FPS         %.1f",  m_FPS);
			ImGui::Text("Frame Time  %.2f ms", m_FrameTimeMs);
			ImGui::Text("CPU Time    %.2f ms", m_CpuTimeMs);
			if (m_GpuTimeMs > 0.0f)
				ImGui::Text("GPU Time    %.2f ms", m_GpuTimeMs);
			else
				ImGui::TextDisabled("GPU Time    --");

			ImGui::Spacing();

			// Frame-time graph
			{
				char overlay[32];
				snprintf(overlay, sizeof(overlay), "%.1f ms", m_FrameTimeMs);
				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.35f, 0.85f, 0.35f, 1.0f));
				ImGui::PlotLines("##ft", m_FrameTimePlot, kPerfSamples, m_PerfPlotOffset,
				                 overlay, 0.0f, 50.0f,
				                 ImVec2(ImGui::GetContentRegionAvail().x, 55.0f));
				ImGui::PopStyleColor();
				ImGui::TextDisabled("Frame Time");

				ImGui::Spacing();

				snprintf(overlay, sizeof(overlay), "%.0f fps", m_FPS);
				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.35f, 0.60f, 1.0f, 1.0f));
				ImGui::PlotLines("##fps", m_FpsPlot, kPerfSamples, m_PerfPlotOffset,
				                 overlay, 0.0f, 300.0f,
				                 ImVec2(ImGui::GetContentRegionAvail().x, 55.0f));
				ImGui::PopStyleColor();
				ImGui::TextDisabled("FPS");
			}
		}

		ImGui::Spacing();

		// ---- Draw Statistics ----
		if (ImGui::CollapsingHeader("Draw Statistics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Draw Calls  %d", Renderer2D::GetStats().DrawCalls);
			ImGui::Text("Quad Count  %d", Renderer2D::GetStats().QuadCount);
			ImGui::Text("Vertices    %d", Renderer2D::GetStats().GetTotalVertexCount());
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
			
			if (ImGui::ImageButton((ImTextureID)m_TranslationIcon->GetImTextureID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::TRANSLATE;

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Translate Selected Objects (W)"); // Tooltip text
			}

			ImGui::SameLine();

			if (ImGui::ImageButton((ImTextureID)m_RotationIcon->GetImTextureID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::ROTATE;

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Rotate Selected Objects (E)"); // Tooltip text
			}

			ImGui::SameLine();

			if (ImGui::ImageButton((ImTextureID)m_ScaleIcon->GetImTextureID(), buttonSize))
			{
				m_ImGuizmoType = ImGuizmo::OPERATION::SCALE;
			

			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Scale Selected Objects (R)"); // Tooltip text
			}

			ImGui::SameLine(0, 40);
			static bool isLocalMode = true;
			ImTextureID spaceIcon = m_OperationMode == (int)ImGuizmo::MODE::LOCAL ? (ImTextureID)m_LocalSpaceIcon->GetImTextureID() : (ImTextureID)m_WorldSpaceIcon->GetImTextureID();
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
			if (ImGui::ImageButton((ImTextureID)m_SnappingIcon->GetImTextureID(), buttonSize))
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
			if (ImGui::ImageButton((ImTextureID)m_CameraIcon->GetImTextureID(), buttonSize))
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
		m_ViewPortHovered = ImGui::IsWindowHovered();
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
							uint64_t textureIDForCameraView = m_CameraViewFrameBuffer->GetColorAttachmentID();
							ImGui::Image((ImTextureID)textureIDForCameraView, ImVec2{ ImGui::GetWindowWidth(), ImGui::GetWindowHeight() }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
							ImGui::End();

						}
					}

				}

			}
		}
		
		

		uint64_t textureID = m_FrameBuffer->GetColorAttachmentID();
		// OpenGL textures are stored bottom-up → flip V so Y+ appears at the top of the screen.
		// DX11 render targets are stored top-down → no flip needed (flip would invert the image
		// and also misalign ImGuizmo, whose screen-space math uses the un-flipped coordinate).
		const bool isDX11 = RendererAPI::GetAPI() == RendererAPI::API::Direct3D;
		ImVec2 uv0 = isDX11 ? ImVec2{0, 0} : ImVec2{0, 1};
		ImVec2 uv1 = isDX11 ? ImVec2{1, 1} : ImVec2{1, 0};
		ImGui::Image((ImTextureID)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y}, uv0, uv1);

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
				// Use the actual viewport image bounds (excludes the toolbar strip at the
				// top of the panel window) so ImGuizmo's screen-space projection lines up
				// with what the camera rendered.  Using GetWindowPos() without the toolbar
				// offset shifts the gizmo up by the toolbar height, making it drift away
				// from the mesh as the camera moves.
				ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
				                  m_ViewportSize.x,      m_ViewportSize.y);
				
				//Entity Transform
				auto& tc = selectedEntity.GetComponent<TransformComponent>();
				glm::mat4 transform = tc.GetTransform();
				
				/*glm::vec3 centerPoint = tc.Translation +(tc.Scale * 0.5f);
				float gizmoOffsetY = centerPoint.y * (m_EditorCamera.GetDistance() / 100);
				
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
		if (m_MousePosX >= 0 && m_MousePosY >= 0 && m_MousePosX < (int)m_ViewportSize.x && m_MousePosY < (int)m_ViewportSize.y)
		{
			if (m_ViewPortFocused)
			{
				// Read the entity-ID pixel exactly once at click time.
				// This is a synchronous GPU stall so we do it here (once per click)
				// rather than every frame.
				m_DrawnEntityID = m_FrameBuffer->ReadPixel(1, (int)m_MousePosX, (int)m_MousePosY);

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
		event.Handled = true;
		return false;
	}
	bool BluEditorLayer::OnMouseScrolledEvent(Events::MouseScrolledEvent& event)
	{
		event.Handled = true;
		return false;
	}
	bool BluEditorLayer::OnMouseMovedEvent(Events::MouseMovedEvent& event)
	{
		event.Handled = true;
		return false;
	}

	bool BluEditorLayer::OnKeyPressedEvent(Events::KeyPressedEvent& event)
	{
		// ImGui input (KeysDown, MouseDown, etc.) is now handled entirely by the
		// ImGui GLFW backend (install_callbacks=true). Do NOT write to the legacy
		// io.KeysDown[] here — mixing both APIs triggers an ImGui assertion.
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
		case BLU_KEY_F:
		{
			// Focus the editor camera's orbit pivot on the selected entity.
			// This makes the camera orbit around the entity rather than a stale focal point.
			if (m_ViewPortFocused || m_ViewPortHovered)
			{
				Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
				if (selectedEntity && selectedEntity.HasComponent<TransformComponent>())
				{
					const auto& tc = selectedEntity.GetComponent<TransformComponent>();
					m_EditorCamera.SetFocalPoint(tc.Translation);
				}
			}
			break;
		}
		}
		event.Handled = true;
		return false;

	}

	bool BluEditorLayer::OnKeyReleasedEvent(Events::KeyReleasedEvent& event)
	{
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