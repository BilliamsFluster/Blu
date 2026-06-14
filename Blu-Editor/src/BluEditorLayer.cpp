#include "BluEditorLayer.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <chrono>
#include "Blu/Events/MouseEvent.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include <glm/gtc/type_ptr.hpp>
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Utils/PlatformUtils.h"
#include "Blu/Utils/AssetPath.h"
#include "ImGuizmo.h"
#include "Blu/Math/Math.h"
#include "Blu/Core/Application.h"
#include "Blu/Platform/Windows/WindowsWindow.h"
#include "Blu/Utils/Helpers.h"
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Rendering/PostProcess.h"
#include "Blu/Rendering/Terrain.h"
#include "Blu/Rendering/Skybox.h"
#include "Blu/Rendering/IBLSystem.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/RenderSettings.h"
#include "Blu/Core/InputMap.h"
#include "Blu/Audio/AudioEngine.h"
#include "FreeFlyCamera.h"
#include "AssetPreviewService.h"
#include "AzureGameModule.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
// D3D11Context.h already pulls in <d3d11.h> — include it last so Windows headers
// don't stomp on the glad/GLFW type definitions established above.
#include "Blu/Platform/DirectX11/D3D11Context.h"
#include "Blu/Platform/Windows/WindowsWindow.h"

// stb_image is compiled into the engine (ExternalDependencies/stb_image/stb_image.cpp).
// Forward-declare the two symbols we need so we don't have to add the include
// directory to the editor project's search paths.
extern "C"
{
    unsigned char* stbi_load(const char* filename, int* x, int* y,
                             int* channels_in_file, int desired_channels);
    void           stbi_image_free(void* retval_from_stbi_load);
    void           stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
}





namespace Blu
{
	static Entity ImportModelEntity(const Shared<Scene>& scene, const std::filesystem::path& sourcePath)
	{
		if (!scene || sourcePath.empty())
			return {};

		Shared<Model> model = ModelLoader::Load(sourcePath.string());
		std::string importedPath = AssetPath::ImportModelPath(sourcePath);

		Entity modelEntity = scene->CreateEntity(sourcePath.stem().string());
		auto& mesh = modelEntity.AddComponent<MeshComponent>();
		mesh.FilePath = importedPath;
		mesh.ModelAsset = model;

		if (!mesh.ModelAsset && !mesh.FilePath.empty())
			mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath).string());

		if (!mesh.ModelAsset)
			BLU_CORE_WARN("BluEditor: failed to import model: {0}", sourcePath.string());

		return modelEntity;
	}

	void BluEditorLayer::QueueStaticCollisionPrompt(Entity entity)
	{
		if (!entity || !entity.HasComponent<MeshComponent>())
			return;

		auto& mesh = entity.GetComponent<MeshComponent>();
		if (!mesh.ModelAsset)
			return;

		m_PendingStaticCollisionEntity = entity;
		m_PendingStaticCollisionModelName = entity.HasComponent<TagComponent>()
			? entity.GetComponent<TagComponent>().Tag
			: "Imported Model";
		m_ShowStaticCollisionImportPrompt = true;
	}

	void BluEditorLayer::DrawStaticCollisionImportPrompt()
	{
		if (!m_ShowStaticCollisionImportPrompt)
			return;

		ImGui::OpenPopup("Generate Static Collision?");
		if (ImGui::BeginPopupModal("Generate Static Collision?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Generate static mesh collision for:");
			ImGui::TextWrapped("%s", m_PendingStaticCollisionModelName.c_str());
			ImGui::Spacing();

			if (ImGui::Button("Generate", ImVec2(110.0f, 0.0f)))
			{
				std::string message;
				if (m_ActiveScene && m_ActiveScene->GenerateStaticMeshCollision(m_PendingStaticCollisionEntity, &message))
				{
					BLU_CORE_INFO("BluEditor: {0}", message);
					if (m_SceneHierarchyPanel)
						m_SceneHierarchyPanel->SetSelectedEntity(m_PendingStaticCollisionEntity);
				}
				else
				{
					BLU_CORE_WARN("BluEditor: {0}", message);
				}

				m_ShowStaticCollisionImportPrompt = false;
				m_PendingStaticCollisionEntity = {};
				m_PendingStaticCollisionModelName.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Skip", ImVec2(90.0f, 0.0f)))
			{
				m_ShowStaticCollisionImportPrompt = false;
				m_PendingStaticCollisionEntity = {};
				m_PendingStaticCollisionModelName.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void BluEditorLayer::SaveSelectedAsPrefab()
	{
		if (!m_ActiveScene || !m_SceneHierarchyPanel)
			return;

		Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
		if (!selected && m_ActorEditorEntity)
			selected = m_ActorEditorEntity;
		if (!selected)
		{
			BLU_CORE_WARN("Prefab: select an actor before saving a prefab");
			return;
		}

		std::filesystem::path prefabDirectory = AssetPath::AssetsRoot() / "prefabs";
		std::filesystem::create_directories(prefabDirectory);
		std::string name = selected.HasComponent<TagComponent>() ? selected.GetComponent<TagComponent>().Tag : "Actor";
		name = AssetPath::SanitizeName(name, "Actor");
		std::filesystem::path path = prefabDirectory / (name + ".bluprefab");

		SceneSerializer serializer(m_ActiveScene);
		if (serializer.SerializePrefab(selected, path.string()))
		{
			AssetPreviewService::Get().Invalidate(path);
			BLU_CORE_INFO("Prefab: saved {0}", AssetPath::ToProjectRelative(path));
		}
		else
		{
			BLU_CORE_WARN("Prefab: failed to save {0}", AssetPath::ToProjectRelative(path));
		}
	}

	Entity BluEditorLayer::InstantiatePrefabAsset(const std::filesystem::path& path)
	{
		if (!m_ActiveScene)
			return {};

		SceneSerializer serializer(m_ActiveScene);
		Entity instance;
		if (serializer.DeserializePrefab(path.string(), &instance))
		{
			if (m_SceneHierarchyPanel)
				m_SceneHierarchyPanel->SetSelectedEntity(instance);
			m_ActorEditorEntity = instance;
			BLU_CORE_INFO("Prefab: instantiated {0}", path.generic_string());
			return instance;
		}
		else
		{
			BLU_CORE_WARN("Prefab: failed to instantiate {0}", path.generic_string());
		}
		return {};
	}

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
		Azure::RegisterAzureGameModule();
		NativeClassRegistry::Get().RegisterActor<CameraController>(
			"Blu::CameraController", "Camera Controller", "Engine", { "CameraController" });
		NativeClassRegistry::Get().RegisterActor<FreeFlyCamera>(
			"BluEditor::FreeFlyCamera", "Free Fly Camera", "Editor", { "FreeFlyCamera" });

		std::filesystem::create_directories("Blu-Editor/config");
		m_EditorSettingsPath = "Blu-Editor/config/EditorSettings.yaml";
		m_ImGuiIniPath = std::filesystem::path("Blu-Editor/config/imgui.ini").generic_string();
		ImGui::GetIO().IniFilename = m_ImGuiIniPath.c_str();

		m_ActiveScene = std::make_shared<Scene>();
		m_SceneHierarchyPanel = std::make_shared<SceneHierarchyPanel>();
		m_ContentBrowserPanel = std::make_shared<ContentBrowserPanel>();
		m_MaterialGraphPanel = std::make_shared<MaterialGraphPanel>();
		m_SceneHierarchyPanel->SetOpenActorEditorCallback([this](Entity entity)
		{
			if (!entity)
				return;
			m_ActorEditorEntity = entity;
			m_ShowActorEditor = true;
			m_ResetActorPreviewCamera = true;
		});
		m_ContentBrowserPanel->SetSaveAllCallback([this]() { SaveCurrentScene(); });
		m_ContentBrowserPanel->SetImportModelCallback([this](const std::filesystem::path& path)
		{
			QueueStaticCollisionPrompt(ImportModelEntity(m_ActiveScene, path));
		});
		m_ContentBrowserPanel->SetGenerateStaticCollisionCallback([this](const std::filesystem::path&)
		{
			Entity selected = m_SceneHierarchyPanel ? m_SceneHierarchyPanel->GetSelectedEntity() : Entity{};
			std::string message;
			if (selected && m_ActiveScene && m_ActiveScene->GenerateStaticMeshCollision(selected, &message))
				BLU_CORE_INFO("ContentBrowser: {0}", message);
			else
				BLU_CORE_WARN("ContentBrowser: select a mesh actor first. {0}", message);
		});
		m_ContentBrowserPanel->SetInstantiatePrefabCallback([this](const std::filesystem::path& path)
		{
			InstantiatePrefabAsset(path);
		});
		m_ContentBrowserPanel->SetSaveSelectedAsPrefabCallback([this]()
		{
			SaveSelectedAsPrefab();
		});
		
		
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

		FrameBufferSpecifications actorPreviewSpec;
		actorPreviewSpec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::Depth };
		actorPreviewSpec.Width = 512;
		actorPreviewSpec.Height = 512;
		m_ActorPreviewFrameBuffer = FrameBuffer::Create(actorPreviewSpec);
		

		m_CameraEntity = m_ActiveScene->CreateEntity("Camera");
		m_CameraEntity.AddComponent<CameraComponent>();
		m_CameraEntity.GetComponent<CameraComponent>().Primary = true;
		m_CameraEntity.AddComponent<ActorComponent>().ClassID = "Blu::CameraController";

		m_EditorCamera = EditorCamera(30, 1.778f, 0.1f, 1000.0f);
		m_ActorPreviewCamera = EditorCamera(35.0f, 1.0f, 0.1f, 5000.0f);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);
		m_OperationMode = 0; // local operation
		LoadEditorSettings();
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

		// ---- Output Log: attach editor sink to both spdlog loggers ----
		EditorLog::RegisterSinks();
		BLU_CORE_INFO("Blu Editor started.");

		// ---- Window icon (taskbar / alt-tab) --------------------------------
		// Load the Blu logo as RGBA and hand it to GLFW so the OS shows it in
		// the taskbar, alt-tab switcher, and the window's system menu.
		{
			GLFWwindow* glfwWin =
			    (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			// Icons must be loaded top-down; disable the global flip that the
			// texture system enables for OpenGL's bottom-left origin.
			stbi_set_flip_vertically_on_load(0);
			int iconW = 0, iconH = 0, iconCh = 0;
			unsigned char* iconPx =
			    stbi_load("assets/textures/BluLogo.png", &iconW, &iconH, &iconCh, 4);
			if (iconPx)
			{
				GLFWimage icon{ iconW, iconH, iconPx };
				glfwSetWindowIcon(glfwWin, 1, &icon);
				stbi_image_free(iconPx);
			}
		}
	}

	void BluEditorLayer::OnDetach()
	{
		SaveEditorSettings();
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
			// Clear entity ID (UAV clear) BEFORE binding the framebuffer as RTV.
			// ClearUnorderedAccessViewUint requires the resource NOT be simultaneously
			// bound as an RTV — binding first then clearing silently unbinds the RTV
			// at slot 1, causing SV_Target1 writes to go nowhere (entityID always -1).
			m_FrameBuffer->ClearAttachment(1, -1);
			m_FrameBuffer->Bind();
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();
		}

		BLU_PROFILE_FUNCTION();
		{

			BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ");
			
		}
		
		
		// ---- Apply view mode (wireframe / lit) ----
		const bool wantWireframe = (m_ViewMode == ViewMode::Wireframe);
		if (wantWireframe)
		{
			if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL && glPolygonMode)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			else if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				if (auto* ctx = D3D11Context::Get()) ctx->SetWireframe(true);
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
				GLFWwindow* _win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
				if (glfwGetKey(_win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				{
					OnSceneStop();
					break;
				}
				// F8 — eject from pawn (cursor freed, editor camera takes over)
				{
					bool f8Now = glfwGetKey(_win, GLFW_KEY_F8) == GLFW_PRESS;
					if (f8Now && !m_F8Prev) { m_F8Prev = f8Now; OnSceneEject(); break; }
					m_F8Prev = f8Now;
				}
				m_ActiveScene->OnUpdateRuntime(deltaTime);
				break;
			}
			case SceneState::Eject:
			{
				GLFWwindow* _win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
				if (glfwGetKey(_win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				{
					OnSceneStop();
					break;
				}
				// F8 — repossess pawn (cursor re-locked, game input restored)
				{
					bool f8Now = glfwGetKey(_win, GLFW_KEY_F8) == GLFW_PRESS;
					if (f8Now && !m_F8Prev) { m_F8Prev = f8Now; OnSceneRepossess(); break; }
					m_F8Prev = f8Now;
				}
				// Editor camera moves freely with WASD + RMB
				if (m_ViewPortFocused && m_ViewPortHovered)
					m_EditorCamera.OnUpdate(deltaTime);
				// Game logic continues; rendering goes through editor camera (set via BeginEject)
				m_ActiveScene->OnUpdateRuntime(deltaTime);
				break;
			}
			case SceneState::Pause:
			{

				m_ActiveScene->OnUpdatePaused(deltaTime); // If you would like to do anything with the time argument
				break;
			}
		}
		
		// ---- Deferred entity pick --------------------------------------------------
		// OnMouseButtonPressed only sets m_PendingEntityPick. We do the actual
		// ReadPixel here, after the scene has rendered to m_FrameBuffer this frame,
		// so the pixel data is current and the OS cursor position is accurate.
		if (m_PendingEntityPick && m_SceneState == SceneState::Edit)
		{
			m_PendingEntityPick = false;

			auto [rawX, rawY] = Input::GetMousePosition();
			float pickX = rawX - m_ViewportBounds[0].x;
			float pickY = rawY - m_ViewportBounds[0].y;

			if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL)
				pickY = (m_ViewportBounds[1].y - m_ViewportBounds[0].y) - pickY;

			if (pickX >= 0.f && pickY >= 0.f &&
				pickX < m_ViewportSize.x && pickY < m_ViewportSize.y)
			{
				int entityID = m_FrameBuffer->ReadPixel(1, (int)pickX, (int)pickY);
				if (entityID == -1)
				{
					m_SceneHierarchyPanel->SetSelectedEntity({});
				}
				else
				{
					Entity candidate{ (entt::entity)entityID, m_ActiveScene.get() };
					if (candidate.HasComponent<IDComponent>())
						m_SceneHierarchyPanel->SetSelectedEntity(candidate);
				}
			}
		}

		// Track mouse position for other systems (gizmos, overlay, etc.)
		auto [mx, my] = Input::GetMousePosition();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL)
			my = (m_ViewportBounds[1].y - m_ViewportBounds[0].y) - my;
		m_MousePosX = (float)mx;
		m_MousePosY = (float)my;



		

		

		// ---- Restore solid fill for overlay (collider outlines etc.) ----
		if (wantWireframe)
		{
			if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL && glPolygonMode)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			else if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				if (auto* ctx = D3D11Context::Get()) ctx->SetWireframe(false);
		}

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

		if (m_ShowActorEditor && m_ActorEditorEntity && m_SceneState == SceneState::Edit &&
			(m_ActorPreviewHovered || m_ActorPreviewFocused))
		{
			m_ActorPreviewCamera.OnUpdate(deltaTime);
		}
		RenderActorPreview();
		
		



	}

	void BluEditorLayer::OnEvent(Events::Event& event)
	{
		const bool editorCameraActive = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Eject;
		if (editorCameraActive)
		{
			m_CameraController.OnEvent(event);
			// Only forward events to the editor camera when the viewport is hovered.
			// Without this guard, scroll-wheel events zoom the camera even when the
			// mouse is over a detail panel, properties window, etc.
			if (m_ViewPortHovered)
				m_EditorCamera.OnEvent(event);
			else if (m_ActorPreviewHovered && m_ShowActorEditor)
				m_ActorPreviewCamera.OnEvent(event);
		}
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

		// ── 2D physics collider outlines ─────────────────────────────────────────
		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto e : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(e);
				glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size, 1.0f);
				glm::vec3 rotation = tc.Rotation;
				glm::vec4 color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
				if (bc2d.ShowCollision)
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
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);
				Renderer2D::DrawCircle(transform, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 0.05f);
			}
		}

		// ── Light gizmos (Unreal-style) — editor view only ─────────────────────
		// Lambda helpers are defined in dependency order so each can capture the ones before it.
		if (m_SceneState == SceneState::Edit)
		{
			constexpr float   kTwoPi        = 6.283185307f;
			constexpr int     kCircleSeg    = 32;   // segments per wire circle
			constexpr int     kSpokeCount   = 8;    // cone/sphere spokes

			const glm::vec3   camRight   = m_EditorCamera.GetRightDirection();
			const glm::vec3   camUp      = m_EditorCamera.GetUpDirection();
			const glm::vec3   camForward = m_EditorCamera.GetForwardDirection();

			// ── Wire circle on an arbitrary plane (axis1/axis2 are orthogonal) ────
			auto DrawWireCircle = [&](const glm::vec3& center, float radius,
			                          const glm::vec3& axis1, const glm::vec3& axis2,
			                          const glm::vec4& color, float thickness = 1.5f)
			{
				for (int i = 0; i < kCircleSeg; ++i)
				{
					float a0 = kTwoPi *  i      / kCircleSeg;
					float a1 = kTwoPi * (i + 1) / kCircleSeg;
					glm::vec3 p0 = center + (axis1 * glm::cos(a0) + axis2 * glm::sin(a0)) * radius;
					glm::vec3 p1 = center + (axis1 * glm::cos(a1) + axis2 * glm::sin(a1)) * radius;
					Renderer2D::DrawLine(p0, p1, color, thickness);
				}
			};

			// ── Attenuation sphere: 3 orthogonal wire circles ─────────────────────
			auto DrawWireSphere = [&](const glm::vec3& center, float radius,
			                          const glm::vec4& color)
			{
				DrawWireCircle(center, radius, glm::vec3(1,0,0), glm::vec3(0,1,0), color); // XY
				DrawWireCircle(center, radius, glm::vec3(1,0,0), glm::vec3(0,0,1), color); // XZ
				DrawWireCircle(center, radius, glm::vec3(0,1,0), glm::vec3(0,0,1), color); // YZ
			};

			// ── Spot light cone: outer (bright) + inner (dim) + spokes ────────────
			auto DrawSpotCone = [&](const glm::vec3& apex, const glm::vec3& dir,
			                        float range, float innerDeg, float outerDeg,
			                        const glm::vec4& color)
			{
				glm::vec3 d = glm::normalize(dir);

				// Build an orthonormal basis (right/up) perpendicular to the cone axis
				glm::vec3 worldUp  = std::abs(d.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
				glm::vec3 coneRight = glm::normalize(glm::cross(d, worldUp));
				glm::vec3 coneUp    = glm::normalize(glm::cross(coneRight, d));

				glm::vec3  baseCenter = apex + d * range;
				float outerR = glm::tan(glm::radians(outerDeg)) * range;
				float innerR = glm::tan(glm::radians(innerDeg)) * range;

				// Outer circle + full spokes (full brightness)
				DrawWireCircle(baseCenter, outerR, coneRight, coneUp, color, 1.5f);
				for (int i = 0; i < kSpokeCount; ++i)
				{
					float     a    = kTwoPi * i / kSpokeCount;
					glm::vec3 rim  = baseCenter + (coneRight * glm::cos(a) + coneUp * glm::sin(a)) * outerR;
					glm::vec3 apexCopy = apex;  // local lvalue for non-const DrawLine p0
					Renderer2D::DrawLine(apexCopy, rim, color, 1.5f);
				}

				// Inner circle + 4 spokes (dim, showing the inner cone boundary)
				glm::vec4 innerColor = glm::vec4(color.r, color.g, color.b, color.a * 0.45f);
				DrawWireCircle(baseCenter, innerR, coneRight, coneUp, innerColor, 1.0f);
				for (int i = 0; i < 4; ++i)
				{
					float     a    = kTwoPi * i / 4;
					glm::vec3 rim  = baseCenter + (coneRight * glm::cos(a) + coneUp * glm::sin(a)) * innerR;
					glm::vec3 apexCopy = apex;
					Renderer2D::DrawLine(apexCopy, rim, innerColor, 1.0f);
				}
			};

			// ── Billboard transform: a quad always facing the camera ──────────────
			constexpr float kIconSize = 0.35f;
			auto MakeBillboard = [&](const glm::vec3& pos) -> glm::mat4
			{
				glm::mat4 t(1.0f);
				t[0] = glm::vec4(camRight   * kIconSize, 0.0f);
				t[1] = glm::vec4(camUp      * kIconSize, 0.0f);
				t[2] = glm::vec4(camForward * kIconSize, 0.0f);
				t[3] = glm::vec4(pos, 1.0f);
				return t;
			};

			// ── Draw a sun/light icon: filled disk + 8 radiating spokes ──────────
			// entityID is written to the pick buffer so clicking the icon selects the entity.
			auto DrawLightIcon = [&](const glm::vec3& pos, const glm::vec4& color, int entityID)
			{
				// Filled disk (body of the icon) — entity ID makes it clickable
				Renderer2D::DrawCircle(MakeBillboard(pos), color, 1.0f, 0.05f, entityID);

				// 8 short rays extending beyond the disk in camera-space
				const float kInnerR = kIconSize * 0.65f;
				const float kOuterR = kIconSize * 1.15f;
				for (int i = 0; i < 8; ++i)
				{
					float     angle  = kTwoPi * i / 8;
					glm::vec3 dir2d  = camRight * glm::cos(angle) + camUp * glm::sin(angle);
					glm::vec3 p0     = pos + dir2d * kInnerR;
					glm::vec3 p1     = pos + dir2d * kOuterR;
					Renderer2D::DrawLine(p0, p1, color, 1.5f);
				}
			};

			// ── Draw icons for every light entity in the scene ────────────────────

			// Point lights — warm yellow
			{
				const glm::vec4 kColor(1.0f, 0.87f, 0.27f, 0.95f);
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, PointLightComponent>();
				for (auto e : view)
				{
					auto&& [tc, plc] = view.get<TransformComponent, PointLightComponent>(e);
					DrawLightIcon(tc.Translation, kColor, (int)(uint32_t)e);
				}
			}

			// Spot lights — cool cyan
			{
				const glm::vec4 kColor(0.35f, 0.82f, 1.0f, 0.95f);
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, SpotLightComponent>();
				for (auto e : view)
				{
					auto&& [tc, slc] = view.get<TransformComponent, SpotLightComponent>(e);
					DrawLightIcon(tc.Translation, kColor, (int)(uint32_t)e);
				}
			}

			// Directional lights — soft white/gold
			{
				const glm::vec4 kColor(1.0f, 0.97f, 0.80f, 0.95f);
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();
				for (auto e : view)
				{
					auto&& [tc, dlc] = view.get<TransformComponent, DirectionalLightComponent>(e);
					DrawLightIcon(tc.Translation, kColor, (int)(uint32_t)e);
				}
			}

			// ── Extra debug gizmos for the selected entity ────────────────────────
			Entity sel = m_SceneHierarchyPanel->GetSelectedEntity();
			if (sel)
			{
				// Point light: wire-sphere showing attenuation range
				if (sel.HasComponent<TransformComponent>() && sel.HasComponent<PointLightComponent>())
				{
					auto& tc  = sel.GetComponent<TransformComponent>();
					auto& plc = sel.GetComponent<PointLightComponent>();
					DrawWireSphere(tc.Translation, plc.Range, glm::vec4(1.0f, 0.87f, 0.27f, 0.85f));
				}

				// Spot light: inner + outer cone wireframe
				if (sel.HasComponent<TransformComponent>() && sel.HasComponent<SpotLightComponent>())
				{
					auto& tc  = sel.GetComponent<TransformComponent>();
					auto& slc = sel.GetComponent<SpotLightComponent>();
					const glm::vec4 coneColor(0.35f, 0.82f, 1.0f, 0.85f);
					DrawSpotCone(
						tc.Translation,
						slc.Direction,
						slc.Range,
						slc.InnerConeAngle,
						slc.OuterConeAngle,
						coneColor);
				}

				auto DrawWireBox = [&](const glm::mat4& transform, const glm::vec4& color, float thickness = 1.5f)
				{
					glm::vec3 corners[8] = {
						{-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
						{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
					};
					for (auto& c : corners)
						c = glm::vec3(transform * glm::vec4(c, 1.0f));
					const int edges[12][2] = {
						{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
					};
					for (auto& edge : edges)
					{
						glm::vec3 p1 = corners[edge[1]];
						Renderer2D::DrawLine(corners[edge[0]], p1, color, thickness);
					}
				};

				auto DrawWireCapsule = [&](const glm::vec3& feet, const CapsuleCollider3DComponent& capsule, const glm::vec4& color)
				{
					const float radius = std::max(0.01f, capsule.Radius);
					const float halfHeight = std::max(0.01f, capsule.HalfHeight);
					const glm::vec3 center = feet + capsule.Offset + glm::vec3(0.0f, halfHeight + radius, 0.0f);
					const glm::vec3 top = center + glm::vec3(0.0f, halfHeight, 0.0f);
					const glm::vec3 bottom = center - glm::vec3(0.0f, halfHeight, 0.0f);
					DrawWireCircle(top, radius, glm::vec3(1,0,0), glm::vec3(0,0,1), color, 1.6f);
					DrawWireCircle(bottom, radius, glm::vec3(1,0,0), glm::vec3(0,0,1), color, 1.6f);
					DrawWireSphere(top, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.55f));
					DrawWireSphere(bottom, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.55f));
					glm::vec3 p0 = bottom + glm::vec3(radius, 0, 0); glm::vec3 p1 = top + glm::vec3(radius, 0, 0);
					Renderer2D::DrawLine(p0, p1, color, 1.4f);
					p0 = bottom + glm::vec3(-radius, 0, 0); p1 = top + glm::vec3(-radius, 0, 0);
					Renderer2D::DrawLine(p0, p1, color, 1.4f);
					p0 = bottom + glm::vec3(0, 0, radius); p1 = top + glm::vec3(0, 0, radius);
					Renderer2D::DrawLine(p0, p1, color, 1.4f);
					p0 = bottom + glm::vec3(0, 0, -radius); p1 = top + glm::vec3(0, 0, -radius);
					Renderer2D::DrawLine(p0, p1, color, 1.4f);
				};

				if (sel.HasComponent<TransformComponent>() && m_ShowSelectedColliderDebug)
				{
					auto& tc = sel.GetComponent<TransformComponent>();
					if (sel.HasComponent<BoxCollider3DComponent>())
					{
						auto& box = sel.GetComponent<BoxCollider3DComponent>();
						glm::mat4 t = glm::translate(glm::mat4(1.0f), tc.Translation + box.Offset)
							* glm::toMat4(glm::quat(tc.Rotation))
							* glm::scale(glm::mat4(1.0f), box.HalfExtents * 2.0f * tc.Scale);
						DrawWireBox(t, glm::vec4(0.3f, 0.9f, 0.5f, 0.95f));
					}
					if (sel.HasComponent<SphereCollider3DComponent>())
					{
						auto& sphere = sel.GetComponent<SphereCollider3DComponent>();
						float scale = std::max(tc.Scale.x, std::max(tc.Scale.y, tc.Scale.z));
						DrawWireSphere(tc.Translation + sphere.Offset, sphere.Radius * scale, glm::vec4(0.3f, 0.9f, 0.5f, 0.95f));
					}
					if (sel.HasComponent<CapsuleCollider3DComponent>())
						DrawWireCapsule(tc.Translation, sel.GetComponent<CapsuleCollider3DComponent>(), glm::vec4(0.25f, 0.85f, 1.0f, 0.95f));
				}

				if (sel.HasComponent<TransformComponent>() && sel.HasComponent<CharacterControllerComponent>() && m_ShowCharacterDebug)
				{
					auto& tc = sel.GetComponent<TransformComponent>();
					const glm::vec4 color = sel.GetComponent<CharacterControllerComponent>().IsGrounded
						? glm::vec4(0.25f, 1.0f, 0.35f, 1.0f)
						: glm::vec4(1.0f, 0.65f, 0.2f, 1.0f);
					if (sel.HasComponent<CapsuleCollider3DComponent>())
						DrawWireCapsule(tc.Translation, sel.GetComponent<CapsuleCollider3DComponent>(), color);
					DrawWireCircle(tc.Translation, 0.18f, glm::vec3(1,0,0), glm::vec3(0,0,1), color, 2.0f);
				}

				if (sel.HasComponent<TransformComponent>() && sel.HasComponent<MeshCollider3DComponent>() && m_ShowMeshColliderDebug)
				{
					auto& tc = sel.GetComponent<TransformComponent>();
					glm::mat4 t = glm::translate(glm::mat4(1.0f), tc.Translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), glm::max(tc.Scale, glm::vec3(0.1f)));
					DrawWireBox(t, glm::vec4(1.0f, 0.55f, 0.2f, 0.95f), 2.0f);
				}

				if (sel.HasComponent<TransformComponent>() && sel.HasComponent<CameraComponent>() && m_ShowCameraDebug)
				{
					auto& tc = sel.GetComponent<TransformComponent>();
					glm::mat4 t = glm::translate(glm::mat4(1.0f), tc.Translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 0.25f, 0.6f));
					DrawWireBox(t, glm::vec4(0.45f, 0.65f, 1.0f, 0.95f), 1.5f);
				}
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

	void BluEditorLayer::CreatePhysicsDemoScene()
	{
		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		m_ActiveScene = std::make_shared<Scene>();
		m_ActiveScene->OnViewportResize((float)m_ViewportSize.x, (float)m_ViewportSize.y);
		m_SceneHierarchyPanel->SetContext(m_ActiveScene);

		// ── Directional sun light ────────────────────────────────────────────
		{
			auto sun = m_ActiveScene->CreateEntity("Sun");
			sun.GetComponent<TransformComponent>().Rotation =
				glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f);
			auto& dl = sun.AddComponent<DirectionalLightComponent>();
			dl.Diffuse    = glm::vec3(1.0f, 0.95f, 0.85f);
			dl.Ambient    = glm::vec3(0.08f, 0.08f, 0.10f);
			dl.Specular   = glm::vec3(0.6f);
			dl.Intensity  = 1.2f;
			dl.Direction  = glm::normalize(glm::vec3(0.3f, -1.0f, 0.5f));
		}

		// ── Ground plane (static physics box + visible cube) ─────────────────
		{
			auto ground = m_ActiveScene->CreateEntity("Ground");
			auto& tc = ground.GetComponent<TransformComponent>();
			tc.Translation = { 0.0f, -0.5f, 0.0f };
			tc.Scale       = { 50.0f, 1.0f, 50.0f };

			auto& mc = ground.AddComponent<MeshComponent>();
			mc.MeshData = Mesh::CreateCube();
			mc.Primitive = MeshComponent::PrimitiveType::Cube;
			mc.MaterialInstance = Material::Create();
			mc.MaterialInstance->AlbedoColor = glm::vec4(0.45f, 0.42f, 0.40f, 1.0f);
			mc.MaterialInstance->Roughness   = 0.85f;
			mc.MaterialInstance->Metallic    = 0.0f;

			auto& rb = ground.AddComponent<Rigidbody3DComponent>();
			rb.Type = Rigidbody3DComponent::BodyType::Static;

			auto& bc = ground.AddComponent<BoxCollider3DComponent>();
			bc.HalfExtents = { 25.0f, 0.5f, 25.0f };
			bc.Friction    = 0.7f;
		}

		// ── Camera with free-fly NativeScript ───────────────────────────────
		{
			auto cam = m_ActiveScene->CreateEntity("Camera");
			auto& tc = cam.GetComponent<TransformComponent>();
			tc.Translation = { 0.0f, 4.0f, 14.0f };
			tc.Rotation    = { 0.0f, 0.0f, 0.0f };

			auto& cc = cam.AddComponent<CameraComponent>();
			cc.Primary = true;
			cc.Camera.SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

			cam.AddComponent<ActorComponent>().ClassID = "BluEditor::FreeFlyCamera";
		}

		// ── Dynamic boxes ────────────────────────────────────────────────────
		struct BoxDef { glm::vec3 pos; glm::vec4 color; float metallic; float roughness; };
		const BoxDef boxes[] = {
			{{ -4.0f, 2.0f, -3.0f }, { 0.85f, 0.15f, 0.15f, 1.0f }, 0.0f, 0.5f },
			{{  0.0f, 4.0f, -3.0f }, { 0.15f, 0.75f, 0.20f, 1.0f }, 0.0f, 0.6f },
			{{  4.0f, 6.0f, -3.0f }, { 0.15f, 0.30f, 0.85f, 1.0f }, 0.0f, 0.4f },
			{{ -2.0f, 8.0f,  0.0f }, { 0.90f, 0.70f, 0.10f, 1.0f }, 0.0f, 0.5f },
			{{  2.0f,10.0f,  0.0f }, { 0.80f, 0.30f, 0.80f, 1.0f }, 0.1f, 0.3f },
			{{ -5.0f, 3.0f,  2.0f }, { 0.95f, 0.95f, 0.95f, 1.0f }, 0.9f, 0.2f }, // chrome
			{{  5.0f, 5.0f,  2.0f }, { 0.80f, 0.55f, 0.20f, 1.0f }, 0.8f, 0.3f }, // gold
			{{  0.0f, 7.0f,  3.0f }, { 0.20f, 0.60f, 0.70f, 1.0f }, 0.0f, 0.7f },
		};

		for (int i = 0; i < (int)(sizeof(boxes)/sizeof(boxes[0])); ++i)
		{
			auto box = m_ActiveScene->CreateEntity("Box_" + std::to_string(i));
			auto& tc = box.GetComponent<TransformComponent>();
			tc.Translation = boxes[i].pos;

			auto& mc = box.AddComponent<MeshComponent>();
			mc.MeshData = Mesh::CreateCube();
			mc.Primitive = MeshComponent::PrimitiveType::Cube;
			mc.MaterialInstance = Material::Create();
			mc.MaterialInstance->AlbedoColor = boxes[i].color;
			mc.MaterialInstance->Metallic    = boxes[i].metallic;
			mc.MaterialInstance->Roughness   = boxes[i].roughness;

			auto& rb = box.AddComponent<Rigidbody3DComponent>();
			rb.Type         = Rigidbody3DComponent::BodyType::Dynamic;
			rb.GravityScale = 1.0f;
			rb.LinearDamping  = 0.02f;
			rb.AngularDamping = 0.05f;

			auto& bc = box.AddComponent<BoxCollider3DComponent>();
			bc.HalfExtents = { 0.5f, 0.5f, 0.5f };
			bc.Friction    = 0.5f;
			bc.Restitution = 0.3f;
		}

		// ── Dynamic spheres ──────────────────────────────────────────────────
		struct SphereDef { glm::vec3 pos; glm::vec4 color; float restitution; };
		const SphereDef spheres[] = {
			{{ -3.0f, 12.0f, -2.0f }, { 1.0f, 0.4f, 0.1f, 1.0f }, 0.7f },
			{{  3.0f, 14.0f,  1.0f }, { 0.1f, 0.9f, 0.8f, 1.0f }, 0.5f },
			{{  0.0f, 16.0f, -5.0f }, { 0.9f, 0.9f, 0.1f, 1.0f }, 0.6f },
			{{ -6.0f, 10.0f,  4.0f }, { 0.5f, 0.1f, 0.9f, 1.0f }, 0.4f },
		};

		for (int i = 0; i < (int)(sizeof(spheres)/sizeof(spheres[0])); ++i)
		{
			auto sphere = m_ActiveScene->CreateEntity("Sphere_" + std::to_string(i));
			auto& tc = sphere.GetComponent<TransformComponent>();
			tc.Translation = spheres[i].pos;

			auto& mc = sphere.AddComponent<MeshComponent>();
			mc.MeshData = Mesh::CreateCube(); // cube visual, sphere physics
			mc.Primitive = MeshComponent::PrimitiveType::Cube;
			mc.MaterialInstance = Material::Create();
			mc.MaterialInstance->AlbedoColor = spheres[i].color;
			mc.MaterialInstance->Roughness   = 0.3f;
			mc.MaterialInstance->Metallic    = 0.05f;

			auto& rb = sphere.AddComponent<Rigidbody3DComponent>();
			rb.Type           = Rigidbody3DComponent::BodyType::Dynamic;
			rb.GravityScale   = 1.0f;
			rb.LinearDamping  = 0.01f;
			rb.AngularDamping = 0.01f;

			auto& sc = sphere.AddComponent<SphereCollider3DComponent>();
			sc.Radius      = 0.5f;
			sc.Friction    = 0.3f;
			sc.Restitution = spheres[i].restitution;
		}

		// ── Player Character ────────────────────────────────────────────────────
		{
			auto player = m_ActiveScene->CreateEntity("PlayerCharacter");
			auto& tc = player.GetComponent<TransformComponent>();
			tc.Translation = { 0.0f, 2.0f, 5.0f };
			tc.Scale       = { 1.0f, 1.0f, 1.0f };

			// Visible mesh so we can see where the player is
			auto& mc = player.AddComponent<MeshComponent>();
			mc.MeshData = Mesh::CreateCube();
			mc.Primitive = MeshComponent::PrimitiveType::Cube;
			mc.MaterialInstance = Material::Create();
			mc.MaterialInstance->AlbedoColor = glm::vec4(0.2f, 0.5f, 1.0f, 1.0f);
			mc.MaterialInstance->Roughness   = 0.5f;
			mc.MaterialInstance->Metallic    = 0.0f;

			// Character physics uses the entity transform as a feet origin.
			auto& capsule = player.AddComponent<CapsuleCollider3DComponent>();
			capsule.Radius = 0.3f;
			capsule.HalfHeight = 0.55f;
			player.AddComponent<CharacterControllerComponent>();
			auto& visual = player.AddComponent<VisualOffsetComponent>();
			visual.Translation = glm::vec3(0.0f, capsule.HalfHeight + capsule.Radius, 0.0f);
			visual.Scale = glm::vec3(capsule.Radius * 2.0f, (capsule.HalfHeight + capsule.Radius) * 2.0f, capsule.Radius * 2.0f);

			// Spring arm — TargetCameraUUID stays 0; EnsurePrimaryCamera() links it on Play
			auto& arm = player.AddComponent<SpringArmComponent>();
			arm.ArmLength        = 6.0f;
			arm.Pitch            = -15.0f;
			arm.Yaw              = 0.0f;
			arm.InheritYaw       = false;
			arm.SocketOffset     = glm::vec3(0.0f, 1.0f, 0.0f);
			arm.EnableLag        = true;
			arm.PositionLagSpeed = 10.0f;

			// Resolved through the native class registry on Play.
			player.AddComponent<ActorComponent>().ClassID = "Azure::PlayerCharacter";
		}

		// Enable the full pipeline by default
		m_ActiveScene->SetUseShadows(true);
		m_ActiveScene->SetUsePostProcess(true);

		// Track as the editor scene so Play/Stop works correctly
		m_EditorScene = m_ActiveScene;
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

		if (ImGui::Button("Save", ImVec2(54.0f, size)))
			SaveCurrentScene();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Save current scene");
		ImGui::SameLine();
		if (ImGui::Button("Import Model", ImVec2(100.0f, size)))
		{
			std::string filepath = FileDialogs::OpenFile("FBX (*.fbx)\0*.fbx\0OBJ (*.obj)\0*.obj\0GLTF (*.gltf)\0*.gltf\0GLB (*.glb)\0*.glb\0All Models\0*.fbx;*.obj;*.gltf;*.glb;*.dae;*.blend;*.ply\0");
			if (!filepath.empty())
				QueueStaticCollisionPrompt(ImportModelEntity(m_ActiveScene, filepath));
		}
		ImGui::SameLine();
		const char* stateText = "Edit";
		if (m_SceneState == SceneState::Play) stateText = "Play";
		else if (m_SceneState == SceneState::Pause) stateText = "Pause";
		else if (m_SceneState == SceneState::Simulate) stateText = "Simulate";
		else if (m_SceneState == SceneState::Eject) stateText = "Eject";
		ImGui::TextDisabled("%s", stateText);

		if (ImGui::GetCursorPosX() < offset)
			ImGui::SameLine(offset);
		else
			ImGui::SameLine();

		ImTextureID playPauseButton = nullptr;
		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Pause || m_SceneState == SceneState::Eject)
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
			else if (m_SceneState == SceneState::Eject)
				OnSceneRepossess();
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

		// ── Eject / Possess button ────────────────────────────────────────────
		if (m_SceneState == SceneState::Play)
		{
			ImGui::SameLine();
			if (ImGui::Button("Eject", ImVec2(50, size)))
				OnSceneEject();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("F8 — free the camera, game keeps running");
		}
		else if (m_SceneState == SceneState::Eject)
		{
			ImGui::SameLine();
			if (ImGui::Button("Possess", ImVec2(60, size)))
				OnSceneRepossess();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("F8 — re-possess the pawn");
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

		// ── Import button (right side of toolbar) ─────────────────────────────
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - 170.0f, 0));
		ImGui::SameLine();
		if (m_ActiveScene)
		{
			auto d = m_ActiveScene->GetDiagnostics();
			uint32_t warnings = d.MissingAssetCount + d.InvalidMeshCollider3DCount +
			                    d.InvalidCharacterColliderCount + d.MissingCollider3DCount +
			                    d.PhysicsBodyCreationFailureCount;
			if (warnings > 0)
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "%u warnings", warnings);
			else
				ImGui::TextDisabled("No warnings");
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
			std::filesystem::path scenePath = path;
			m_ActiveScene->SetSceneFilePath(scenePath);
			

			

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
				if (!filepath.empty())
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

			m_ActiveScene->OnRuntimeStart();

			// Guarantee a primary camera exists; auto-links to any unconnected SpringArm
			m_ActiveScene->EnsurePrimaryCamera();
			m_ActiveScene->SetPlayerInputEnabled(true);

			m_SceneMissing = false;
			Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);

			// Capture mouse for game input — hide cursor and lock it to the window
			m_F8Prev = false;
			GLFWwindow* win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
		else
		{
			m_SceneMissing = true;
		}
	}

	void BluEditorLayer::OnScenePause()
	{
		m_SceneState = SceneState::Pause;
		m_ActiveScene->SetPlayerInputEnabled(false);
		m_ActiveScene->SetScenePaused(true);

	}

	void BluEditorLayer::OnSceneResume()
	{
		m_SceneState = SceneState::Play;
		m_ActiveScene->SetPlayerInputEnabled(true);
		m_ActiveScene->SetScenePaused(false);
	}

	void BluEditorLayer::OnSceneStop()
	{
		if (m_SceneState != SceneState::Edit)
		{
			m_ActiveScene->EndEject(); // no-op if not ejected
			m_ActiveScene->SetPlayerInputEnabled(false);
			m_ActiveScene->OnRuntimeStop();
			m_ActiveScene = m_EditorScene;
			Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);
			m_SceneHierarchyPanel->SetContext(m_ActiveScene);
			m_PlayButtonHit = false;
			m_SceneState = SceneState::Edit;

			// Release mouse back to the editor
			GLFWwindow* win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
			


	}

	void BluEditorLayer::OnSceneEject()
	{
		if (m_SceneState != SceneState::Play) return;
		m_SceneState = SceneState::Eject;
		m_ActiveScene->SetPlayerInputEnabled(false);
		m_ActiveScene->BeginEject(&m_EditorCamera);

		GLFWwindow* win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	void BluEditorLayer::OnSceneRepossess()
	{
		if (m_SceneState != SceneState::Eject) return;
		m_ActiveScene->EndEject();
		m_SceneState = SceneState::Play;

		GLFWwindow* win = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		m_ActiveScene->SetPlayerInputEnabled(true);
	}

	void BluEditorLayer::OnScenePlayNewWindow()
	{
	}

	void BluEditorLayer::OnSceneSimulate()
	{
		if (!m_EditorScene) { m_SceneMissing = true; return; }

		m_SceneState     = SceneState::Play;
		m_ViewPortFocused = true;
		ImGui::SetWindowFocus("viewport");
		m_PlayButtonHit  = true;
		m_ActiveScene    = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnRuntimeStart();
		m_SceneMissing   = false;
		Helpers::SceneHelpers::SetHelperActiveScene(m_ActiveScene);
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
		// ── Style: make the bar taller and give it a flat dark background ──────
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(5.f, 9.f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));

		if (!ImGui::BeginMainMenuBar())
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			return;
		}

		const float barH    = ImGui::GetWindowHeight();
		const float barW    = ImGui::GetWindowWidth();
		outTitlebarHeight   = barH;

		// ── App logo ────────────────────────────────────────────────────────────
		const float logoSz = barH - 10.f;
		ImGui::SetCursorPosY((barH - logoSz) * 0.5f);
		{
			const bool isDX11 = RendererAPI::GetAPI() == RendererAPI::API::Direct3D;
			ImGui::Image((ImTextureID)m_AppHeaderIcon->GetImTextureID(),
			             ImVec2(logoSz, logoSz),
			             isDX11 ? ImVec2(0, 0) : ImVec2(0, 1),
			             isDX11 ? ImVec2(1, 1) : ImVec2(1, 0));
		}
		ImGui::SameLine(0, 8.f);

		// ── Menus ───────────────────────────────────────────────────────────────
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New",        "Ctrl+N"))         NewScene();
			if (ImGui::MenuItem("Open...",    "Ctrl+O"))         OpenScene();
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))   SaveSceneAs();
			if (ImGui::MenuItem("Save",       "Ctrl+S"))         SaveCurrentScene();
			ImGui::Separator();
			if (ImGui::MenuItem("New Physics Demo"))              CreatePhysicsDemoScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Import Model...", 0))
			{
				std::string filepath = FileDialogs::OpenFile("FBX (*.fbx)\0*.fbx\0OBJ (*.obj)\0*.obj\0GLTF (*.gltf)\0*.gltf\0GLB (*.glb)\0*.glb\0All Models\0*.fbx;*.obj;*.gltf;*.glb;*.dae;*.blend;*.ply\0");
				if (!filepath.empty())
				{
					QueueStaticCollisionPrompt(ImportModelEntity(m_ActiveScene, filepath));
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) Application::Get().Close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Outliner",        nullptr, &m_ShowOutliner);
			ImGui::MenuItem("Details",         nullptr, &m_ShowDetails);
			ImGui::MenuItem("Content Browser", nullptr, &m_ShowContentBrowser);
			ImGui::MenuItem("Output Log",      nullptr, &m_ShowOutputLog);
			ImGui::MenuItem("Rendering",       nullptr, &m_ShowRendering);
			ImGui::MenuItem("Diagnostics",     nullptr, &m_ShowDiagnostics);
			ImGui::MenuItem("Actor Editor",    nullptr, &m_ShowActorEditor);
			ImGui::MenuItem("Input Map",       nullptr, &m_ShowInputMap);
			ImGui::MenuItem("Material Graph",  nullptr, &m_ShowMaterialGraph);
			ImGui::Separator();
			ImGui::MenuItem("Settings",        nullptr, &m_ShowSettings);
			if (ImGui::MenuItem("Reset Editor Layout"))
				m_ResetEditorLayout = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools"))
		{
			if (ImGui::MenuItem("Terrain Editor", nullptr, &m_ShowTerrainPanel)) {}
			if (ImGui::MenuItem("Material Graph", nullptr, &m_ShowMaterialGraph)) {}
			if (ImGui::MenuItem("Input Map", nullptr, &m_ShowInputMap)) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Build"))
		{
			ImGui::TextDisabled("Packaging is deferred until the play loop and authoring path are stable.");
			ImGui::EndMenu();
		}

		// ── Centred app / scene title ────────────────────────────────────────────
		{
			// Show active scene name if one is loaded, otherwise just the app name.
			std::string titleStr = "Blu Editor";
			if (m_EditorScene)
			{
				auto p = m_EditorScene->GetSceneFilePath();
				if (!p.empty())
					titleStr = p.stem().string();
			}
			const float titleW = ImGui::CalcTextSize(titleStr.c_str()).x;
			const float ctrlTotal = 45.f * 3.f;
			const float menuEndX  = ImGui::GetCursorPosX();
			const float centerX   = (barW - titleW) * 0.5f;
			if (centerX > menuEndX && (centerX + titleW) < (barW - ctrlTotal))
			{
				ImGui::SameLine(centerX);
				ImGui::TextDisabled("%s", titleStr.c_str());
			}
		}

		// ── Window controls (right-aligned, drawn with ImDrawList) ──────────────
		{
			GLFWwindow* glfwWin =
			    (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			const bool  maxed  = (bool)glfwGetWindowAttrib(glfwWin, GLFW_MAXIMIZED);

			// In windowed (non-maximised) mode the WS_THICKFRAME border pixels are
			// still client area but live inside the resize-hit zone.  Pull the
			// controls left by that width so they don't land under the resize handle
			// and the close button doesn't clip against the physical screen edge.
			float rightInset = 0.f;
			if (!maxed)
			{
				rightInset = (float)(GetSystemMetrics(SM_CXSIZEFRAME)
				                   + GetSystemMetrics(SM_CXPADDEDBORDER));
			}

			constexpr float ctrlW = 45.f;
			ImDrawList*     dl    = ImGui::GetWindowDrawList();
			const ImU32     kIcon = IM_COL32(210, 210, 210, 255);
			const float     iconR = 5.f;   // half-extent of each drawn symbol

			// Helper: invisible button at current cursor, background + custom icon
			bool controlHovered = false;
			auto MakeCtrl = [&](const char* id, ImVec4 hoverBg, ImVec4 activeBg,
			                    auto drawIcon) -> bool
			{
				const bool pressed = ImGui::InvisibleButton(id, ImVec2(ctrlW, barH));
				bool hov     = ImGui::IsItemHovered();
				bool act     = ImGui::IsItemActive();
				if (hov) controlHovered = true;

				ImVec2 mn = ImGui::GetItemRectMin();
				ImVec2 mx = ImGui::GetItemRectMax();
				if (act)
					dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(activeBg));
				else if (hov)
					dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(hoverBg));

				ImVec2 c = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f };
				drawIcon(dl, c, kIcon);
				return pressed;
			};

			// Jump to right-side start, inset from the frame border
			ImGui::SameLine(barW - ctrlW * 3.f - rightInset);

			// Minimize ─
			if (MakeCtrl("##min",
			             ImVec4(0.22f, 0.22f, 0.22f, 1.f),
			             ImVec4(0.35f, 0.35f, 0.35f, 1.f),
			             [&](ImDrawList* d, ImVec2 c, ImU32 col)
			             {
			                 d->AddLine({ c.x - iconR, c.y + 2.f },
			                            { c.x + iconR, c.y + 2.f }, col, 1.5f);
			             }))
				glfwIconifyWindow(glfwWin);

			ImGui::SameLine(0, 0);

			// Maximize □ / Restore ❐
			if (MakeCtrl("##max",
			             ImVec4(0.22f, 0.22f, 0.22f, 1.f),
			             ImVec4(0.35f, 0.35f, 0.35f, 1.f),
			             [&](ImDrawList* d, ImVec2 c, ImU32 col)
			             {
			                 if (!maxed)
			                 {
			                     d->AddRect({ c.x - iconR, c.y - iconR },
			                                { c.x + iconR, c.y + iconR }, col, 0, 0, 1.5f);
			                 }
			                 else
			                 {
			                     // Restore: two overlapping squares
			                     constexpr float o = 2.5f;
			                     d->AddRect({ c.x - iconR + o, c.y - iconR     },
			                                { c.x + iconR,     c.y + iconR - o }, col, 0, 0, 1.5f);
			                     d->AddRect({ c.x - iconR,     c.y - iconR + o },
			                                { c.x + iconR - o, c.y + iconR     }, col, 0, 0, 1.5f);
			                 }
			             }))
			{
				if (maxed) glfwRestoreWindow(glfwWin);
				else       glfwMaximizeWindow(glfwWin);
			}

			ImGui::SameLine(0, 0);

			// Close ✕ (red on hover)
			if (MakeCtrl("##cls",
			             ImVec4(0.85f, 0.12f, 0.12f, 1.f),
			             ImVec4(0.65f, 0.08f, 0.08f, 1.f),
			             [&](ImDrawList* d, ImVec2 c, ImU32 col)
			             {
			                 d->AddLine({ c.x - iconR, c.y - iconR },
			                            { c.x + iconR, c.y + iconR }, col, 1.5f);
			                 d->AddLine({ c.x + iconR, c.y - iconR },
			                            { c.x - iconR, c.y + iconR }, col, 1.5f);
			             }))
				Application::Get().Close();

			// Update titlebar hover for GLFW drag.
			// Must exclude:
			//   - the min/max/close control buttons (controlHovered)
			//   - any hovered ImGui item (menu labels, logo image, etc.)
			//     so that clicking "File/Script/Window" yields HTCLIENT to Windows,
			//     not HTCAPTION — otherwise WM_NCHITTEST swallows the click as a
			//     title-bar drag and the menu popup never opens.
			m_TitleBarHovered = ImGui::IsWindowHovered()
			                 && !controlHovered
			                 && !ImGui::IsAnyItemHovered();
			static_cast<WindowsWindow&>(Application::Get().GetWindow())
			    .SetTitleBarHovered(m_TitleBarHovered);
		}

		ImGui::EndMainMenuBar();
		ImGui::PopStyleColor(); // MenuBarBg
		ImGui::PopStyleVar();   // FramePadding
	}

	void BluEditorLayer::LoadEditorSettings()
	{
		if (m_ImGuiIniPath.empty())
			m_ImGuiIniPath = std::filesystem::path("Blu-Editor/config/imgui.ini").generic_string();

		ImGui::GetIO().IniFilename = m_ImGuiIniPath.c_str();
		m_ResetEditorLayout = !std::filesystem::exists(m_ImGuiIniPath);

		if (m_EditorSettingsPath.empty())
			m_EditorSettingsPath = "Blu-Editor/config/EditorSettings.yaml";
		if (!std::filesystem::exists(m_EditorSettingsPath))
			return;

		try
		{
			YAML::Node settings = YAML::LoadFile(m_EditorSettingsPath.string());
			if (auto panels = settings["Panels"])
			{
				if (panels["Outliner"])       m_ShowOutliner = panels["Outliner"].as<bool>();
				if (panels["Details"])        m_ShowDetails = panels["Details"].as<bool>();
				if (panels["ContentBrowser"]) m_ShowContentBrowser = panels["ContentBrowser"].as<bool>();
				if (panels["OutputLog"])      m_ShowOutputLog = panels["OutputLog"].as<bool>();
				if (panels["Rendering"])      m_ShowRendering = panels["Rendering"].as<bool>();
				if (panels["Diagnostics"])    m_ShowDiagnostics = panels["Diagnostics"].as<bool>();
				if (panels["ActorEditor"])    m_ShowActorEditor = panels["ActorEditor"].as<bool>();
			}
			if (auto debug = settings["Debug"])
			{
				if (debug["SelectedCollider"]) m_ShowSelectedColliderDebug = debug["SelectedCollider"].as<bool>();
				if (debug["Character"])        m_ShowCharacterDebug = debug["Character"].as<bool>();
				if (debug["MeshCollider"])     m_ShowMeshColliderDebug = debug["MeshCollider"].as<bool>();
				if (debug["Camera"])           m_ShowCameraDebug = debug["Camera"].as<bool>();
			}
			if (auto content = settings["ContentBrowser"])
			{
				if (content["Directory"] && m_ContentBrowserPanel)
					m_ContentBrowserPanel->SetBrowserDirectory(content["Directory"].as<std::string>());
				if (content["ThumbnailSize"] && m_ContentBrowserPanel)
					m_ContentBrowserPanel->SetThumbnailSize(content["ThumbnailSize"].as<float>());
			}
			if (auto window = settings["Window"])
			{
				GLFWwindow* glfwWin = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
				if (glfwWin)
				{
					if (window["X"] && window["Y"])
						glfwSetWindowPos(glfwWin, window["X"].as<int>(), window["Y"].as<int>());
					if (window["Width"] && window["Height"])
						glfwSetWindowSize(glfwWin, window["Width"].as<int>(), window["Height"].as<int>());
					static_cast<WindowsWindow&>(Application::Get().GetWindow()).ClampToWorkArea();
					if (window["Maximized"] && window["Maximized"].as<bool>())
						glfwMaximizeWindow(glfwWin);
				}
			}
		}
		catch (const std::exception& e)
		{
			BLU_CORE_WARN("BluEditor: failed to load editor settings: {0}", e.what());
		}
	}

	void BluEditorLayer::SaveEditorSettings()
	{
		if (m_EditorSettingsPath.empty())
			m_EditorSettingsPath = "Blu-Editor/config/EditorSettings.yaml";

		std::filesystem::create_directories(m_EditorSettingsPath.parent_path());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << 1;

		GLFWwindow* glfwWin = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		if (glfwWin)
		{
			const bool maximized = glfwGetWindowAttrib(glfwWin, GLFW_MAXIMIZED) == GLFW_TRUE;
			if (!maximized)
				static_cast<WindowsWindow&>(Application::Get().GetWindow()).ClampToWorkArea();

			int x = 0, y = 0, w = 0, h = 0;
			glfwGetWindowPos(glfwWin, &x, &y);
			glfwGetWindowSize(glfwWin, &w, &h);
			out << YAML::Key << "Window" << YAML::BeginMap;
			out << YAML::Key << "X" << YAML::Value << x;
			out << YAML::Key << "Y" << YAML::Value << y;
			out << YAML::Key << "Width" << YAML::Value << w;
			out << YAML::Key << "Height" << YAML::Value << h;
			out << YAML::Key << "Maximized" << YAML::Value << maximized;
			out << YAML::EndMap;
		}

		out << YAML::Key << "Panels" << YAML::BeginMap;
		out << YAML::Key << "Outliner" << YAML::Value << m_ShowOutliner;
		out << YAML::Key << "Details" << YAML::Value << m_ShowDetails;
		out << YAML::Key << "ContentBrowser" << YAML::Value << m_ShowContentBrowser;
		out << YAML::Key << "OutputLog" << YAML::Value << m_ShowOutputLog;
		out << YAML::Key << "Rendering" << YAML::Value << m_ShowRendering;
		out << YAML::Key << "Diagnostics" << YAML::Value << m_ShowDiagnostics;
		out << YAML::Key << "ActorEditor" << YAML::Value << m_ShowActorEditor;
		out << YAML::EndMap;

		out << YAML::Key << "Debug" << YAML::BeginMap;
		out << YAML::Key << "SelectedCollider" << YAML::Value << m_ShowSelectedColliderDebug;
		out << YAML::Key << "Character" << YAML::Value << m_ShowCharacterDebug;
		out << YAML::Key << "MeshCollider" << YAML::Value << m_ShowMeshColliderDebug;
		out << YAML::Key << "Camera" << YAML::Value << m_ShowCameraDebug;
		out << YAML::EndMap;

		if (m_ContentBrowserPanel)
		{
			out << YAML::Key << "ContentBrowser" << YAML::BeginMap;
			out << YAML::Key << "Directory" << YAML::Value << m_ContentBrowserPanel->GetCurrentDirectory().generic_string();
			out << YAML::Key << "ThumbnailSize" << YAML::Value << m_ContentBrowserPanel->GetThumbnailSize();
			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream fout(m_EditorSettingsPath);
		fout << out.c_str();

		if (!m_ImGuiIniPath.empty())
			ImGui::SaveIniSettingsToDisk(m_ImGuiIniPath.c_str());
	}

	void BluEditorLayer::ResetEditorLayout()
	{
		ImGuiID dockspaceID = ImGui::GetID("BluDockspace");
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport)
			return;

		ImGui::DockBuilderRemoveNode(dockspaceID);
		ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

		ImGuiID dockMain = dockspaceID;
		ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.255f, nullptr, &dockMain);
		ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
		ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.48f, nullptr, &dockRight);

		ImGui::DockBuilderDockWindow("Viewport", dockMain);
		ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
		ImGui::DockBuilderDockWindow("Output Log", dockBottom);
		ImGui::DockBuilderDockWindow("Outliner", dockRight);
		ImGui::DockBuilderDockWindow("Rendering", dockRight);
		ImGui::DockBuilderDockWindow("Details", dockRightBottom);
		ImGui::DockBuilderDockWindow("Diagnostics", dockRightBottom);
		ImGui::DockBuilderFinish(dockspaceID);

		m_ShowOutliner = true;
		m_ShowDetails = true;
		m_ShowContentBrowser = true;
		m_ShowOutputLog = true;
		m_ShowRendering = true;
		m_ShowDiagnostics = true;
		m_ShowActorEditor = false;

		if (!m_ImGuiIniPath.empty())
			ImGui::SaveIniSettingsToDisk(m_ImGuiIniPath.c_str());
		SaveEditorSettings();
	}

	static void ComputePreviewBounds(Entity entity, glm::vec3& center, float& radius)
	{
		center = glm::vec3(0.0f);
		radius = 1.0f;
		if (!entity || !entity.HasComponent<MeshComponent>())
			return;

		auto& mesh = entity.GetComponent<MeshComponent>();
		if (mesh.ModelAsset && !mesh.ModelAsset->Meshes.empty())
		{
			uint32_t count = 0;
			for (const auto& submesh : mesh.ModelAsset->Meshes)
			{
				center += glm::vec3(submesh.LocalTransform * glm::vec4(submesh.BoundingCenter, 1.0f));
				count++;
			}
			if (count > 0)
				center /= (float)count;

			float maxRadius = 0.5f;
			for (const auto& submesh : mesh.ModelAsset->Meshes)
			{
				glm::vec3 subCenter = glm::vec3(submesh.LocalTransform * glm::vec4(submesh.BoundingCenter, 1.0f));
				float scale = std::max({
					glm::length(glm::vec3(submesh.LocalTransform[0])),
					glm::length(glm::vec3(submesh.LocalTransform[1])),
					glm::length(glm::vec3(submesh.LocalTransform[2])),
					0.001f });
				maxRadius = std::max(maxRadius, glm::length(subCenter - center) + submesh.BoundingRadius * scale);
			}
			radius = std::max(maxRadius, 0.5f);
		}
		else if (entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			radius = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z, 0.5f });
		}
	}

	void BluEditorLayer::RenderActorPreview()
	{
		if (!m_ShowActorEditor || !m_ActorPreviewFrameBuffer || !m_ActorEditorEntity)
			return;
		if (!m_ActorEditorEntity.HasComponent<IDComponent>())
			return;
		AssetPreviewService::Get().RenderEntityPreview(
			m_ActorEditorEntity,
			m_ActorPreviewFrameBuffer,
			m_ActorPreviewCamera,
			m_ActorPreviewSize,
			m_ResetActorPreviewCamera,
			m_LastActorPreviewEntityID);
		m_ResetActorPreviewCamera = false;
	}

	void BluEditorLayer::DrawActorEditor()
	{
		if (!m_ActorEditorEntity && m_SceneHierarchyPanel)
			m_ActorEditorEntity = m_SceneHierarchyPanel->GetSelectedEntity();
		if (m_ActorEditorEntity && !m_ActorEditorEntity.HasComponent<IDComponent>())
			m_ActorEditorEntity = {};

		if (ImGui::Begin("Actor Editor", &m_ShowActorEditor))
		{
			if (!m_ActorEditorEntity)
			{
				ImGui::TextDisabled("Select an entity and choose Open Actor Editor.");
				ImGui::End();
				return;
			}

			auto& tag = m_ActorEditorEntity.GetComponent<TagComponent>().Tag;
			ImGui::Text("%s", tag.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Use Selected") && m_SceneHierarchyPanel)
			{
				m_ActorEditorEntity = m_SceneHierarchyPanel->GetSelectedEntity();
				m_ResetActorPreviewCamera = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Frame Actor"))
				m_ResetActorPreviewCamera = true;
			ImGui::SameLine();
			if (ImGui::SmallButton("Save As Prefab"))
				SaveSelectedAsPrefab();
			ImGui::Separator();

			float rightWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x * 0.38f);
			float previewWidth = std::max(240.0f, ImGui::GetContentRegionAvail().x - rightWidth - 8.0f);
			float previewHeight = std::max(240.0f, ImGui::GetContentRegionAvail().y);
			m_ActorPreviewSize = { previewWidth, previewHeight };

			ImGui::BeginChild("##ActorPreview", ImVec2(previewWidth, 0), true, ImGuiWindowFlags_NoScrollbar);
			uint64_t textureID = m_ActorPreviewFrameBuffer ? m_ActorPreviewFrameBuffer->GetColorAttachmentID() : 0;
			const bool isDX11 = RendererAPI::GetAPI() == RendererAPI::API::Direct3D;
			if (textureID)
				ImGui::Image((ImTextureID)textureID, ImGui::GetContentRegionAvail(),
					isDX11 ? ImVec2(0, 0) : ImVec2(0, 1),
					isDX11 ? ImVec2(1, 1) : ImVec2(1, 0));
			m_ActorPreviewHovered = ImGui::IsWindowHovered();
			m_ActorPreviewFocused = ImGui::IsWindowFocused();
			ImVec2 overlay = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText({ overlay.x + 10.0f, overlay.y + 10.0f }, IM_COL32(220, 220, 220, 255),
				m_ActorEditorEntity.HasComponent<MeshComponent>() ? "Preview" : "No renderable mesh");
			ImGui::GetWindowDrawList()->AddText({ overlay.x + 10.0f, overlay.y + 30.0f }, IM_COL32(170, 170, 170, 255),
				"RMB + WASD/QE, MMB pan, wheel zoom");
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("##ActorInspector", ImVec2(0, 0), false);
			if (m_SceneHierarchyPanel)
				m_SceneHierarchyPanel->DrawEntityComponents(m_ActorEditorEntity);
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void BluEditorLayer::DrawPlaytestHUD()
	{
		// Gameplay HUD is now rendered by RuntimeUI inside the scene framebuffer.
	}

	void BluEditorLayer::OnGuiDraw()
	{
		float height = 5.0f;
		UIDrawTitlebar(height);

		// Host the dockspace each frame so editor panels dock (Unreal-style) instead of
		// free-floating. DrawDockspace() builds the fullscreen "Blu Dockspace" window +
		// ImGui::DockSpace; panels' subsequent ImGui::Begin calls snap into it (positions
		// restored from imgui.ini, or from ResetEditorLayout below on first run/reset).
		if (auto imguiLayer = Blu::Application::Get().GetImGuiLayer())
			imguiLayer->DrawDockspace();

		if (m_ResetEditorLayout)
		{
			ResetEditorLayout();
			m_ResetEditorLayout = false;
		}

		// Always pass pointers so ImGui::Begin handles hide/show from both the
		// Window menu checkboxes and the title-bar close (X) button.
		m_SceneHierarchyPanel->OnImGuiRender(&m_ShowOutliner, &m_ShowDetails);

		if (m_ShowContentBrowser)
			m_ContentBrowserPanel->OnImGuiRender();

		if (m_ShowActorEditor)
			DrawActorEditor();

		DrawPlaytestHUD();

		DrawStaticCollisionImportPrompt();

		// ---- Settings / Preferences ----
		if (m_ShowSettings)
		{
			if (ImGui::Begin("Settings", &m_ShowSettings))
			{
				ImGui::TextDisabled("Editor preferences");
				ImGui::Spacing();
				if (ImGui::CollapsingHeader("Gizmo Snapping", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Snap Translate", &enableTranslationSnap);
					ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
					ImGui::DragFloat("m##snapT", &translationSnapValue, 0.05f, 0.01f, 10.0f, "%.2f");
					ImGui::Checkbox("Snap Rotate",    &enableRotationSnap);
					ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
					ImGui::DragFloat("deg##snapR", &rotationSnapValue, 0.5f, 1.0f, 90.0f, "%.0f");
					ImGui::Checkbox("Snap Scale",     &enableScaleSnap);
					ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
					ImGui::DragFloat("x##snapS", &scaleSnapValue, 0.05f, 0.01f, 10.0f, "%.2f");
				}
				if (ImGui::CollapsingHeader("Viewport Debug Overlays", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Collider outlines", &m_ShowSelectedColliderDebug);
					ImGui::Checkbox("Character capsule", &m_ShowCharacterDebug);
					ImGui::Checkbox("Mesh collider",     &m_ShowMeshColliderDebug);
					ImGui::Checkbox("Camera frustum",    &m_ShowCameraDebug);
				}
				if (ImGui::CollapsingHeader("Layout"))
				{
					if (ImGui::Button("Reset Editor Layout"))
						m_ResetEditorLayout = true;
					ImGui::TextDisabled("Re-docks all panels to the default arrangement.");
				}
			}
			ImGui::End();
		}

		// ---- Output Log ----
		if (m_ShowOutputLog)
		{
			if (ImGui::Begin("Output Log", &m_ShowOutputLog))
			{
				// Filter toggles
				if (ImGui::Button("Clear"))
					EditorLog::Get().Clear();
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
				ImGui::Checkbox("Trace", &m_LogShowTrace);
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::Checkbox("Info",  &m_LogShowInfo);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.0f, 1.0f));
				ImGui::Checkbox("Warn",  &m_LogShowWarn);
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
				ImGui::Checkbox("Error", &m_LogShowError);
				ImGui::PopStyleColor();

				ImGui::Separator();

				ImGui::BeginChild("##log_scroll", ImVec2(0, 0), false,
				                  ImGuiWindowFlags_HorizontalScrollbar);

				const auto& messages = EditorLog::Get().GetMessages();
				for (const auto& entry : messages)
				{
					bool show = false;
					if (entry.Level == EditorLogLevel::Trace && m_LogShowTrace) show = true;
					if (entry.Level == EditorLogLevel::Info  && m_LogShowInfo)  show = true;
					if (entry.Level == EditorLogLevel::Warn  && m_LogShowWarn)  show = true;
					if (entry.Level == EditorLogLevel::Error && m_LogShowError) show = true;
					if (!show) continue;

					ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
					if      (entry.Level == EditorLogLevel::Trace) color = { 0.55f, 0.55f, 0.55f, 1.0f };
					else if (entry.Level == EditorLogLevel::Warn)  color = { 1.00f, 0.85f, 0.00f, 1.0f };
					else if (entry.Level == EditorLogLevel::Error) color = { 1.00f, 0.35f, 0.35f, 1.0f };

					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(entry.Text.c_str());
					ImGui::PopStyleColor();
				}

				if (EditorLog::Get().ConsumeScrollToBottom())
					ImGui::SetScrollHereY(1.0f);

				ImGui::EndChild();
			}
			ImGui::End();
		}

		if (m_ShowDiagnostics && m_ActiveScene)
		{
			ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Diagnostics", &m_ShowDiagnostics))
			{
				auto sceneStateName = [&]() -> const char*
				{
					switch (m_SceneState)
					{
						case SceneState::Edit:     return "Edit";
						case SceneState::Play:     return "Play";
						case SceneState::Pause:    return "Pause";
						case SceneState::Simulate: return "Simulate";
						case SceneState::Eject:    return "Eject";
					}
					return "Unknown";
				};

				SceneDiagnostics diagnostics = m_ActiveScene->GetDiagnostics();
				std::string scenePath = m_ActiveScene->GetSceneFilePath().string();
				if (scenePath.empty())
					scenePath = "<unsaved>";

				ImGui::SeparatorText("Scene");
				ImGui::Text("State                 %s", sceneStateName());
				ImGui::Text("Scene                 %s", scenePath.c_str());
				ImGui::Text("Entities              %u", diagnostics.EntityCount);
				ImGui::Text("Native Scripts        %u", diagnostics.NativeScriptCount);
				ImGui::Text("Spring Arms           %u", diagnostics.SpringArmCount);
				ImGui::Text("Meshes                %u / %u drawable",
				            diagnostics.DrawableMeshCount, diagnostics.MeshEntityCount);
				ImGui::Text("Visual Offsets        %u", diagnostics.VisualOffsetCount);
				if (diagnostics.DrawableMeshCount != diagnostics.MeshEntityCount)
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Some mesh entities have no drawable asset");
				ImGui::SeparatorText("Assets");
				ImGui::Text("Assets                %u referenced", diagnostics.AssetReferenceCount);
				if (diagnostics.MissingAssetCount > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Missing Assets        %u", diagnostics.MissingAssetCount);
				else
					ImGui::Text("Missing Assets        0");
				ImGui::Text("External Assets       %u", diagnostics.ExternalAssetCount);
				ImGui::Text("Imported Assets       %u", diagnostics.ImportedAssetCount);

				ImGui::SeparatorText("Physics");
				ImGui::Text("3D Rigidbodies        %u", diagnostics.Rigidbody3DCount);
				ImGui::Text("3D Bodies             %u active", diagnostics.RuntimePhysicsBody3DCount);
				ImGui::Text("Body Types            %u static / %u dynamic / %u kinematic",
				            diagnostics.StaticBody3DCount, diagnostics.DynamicBody3DCount, diagnostics.KinematicBody3DCount);
				ImGui::Text("Primitive Colliders   %u box / %u sphere / %u capsule",
				            diagnostics.BoxCollider3DCount, diagnostics.SphereCollider3DCount, diagnostics.CapsuleCollider3DCount);
				ImGui::Text("Mesh Colliders        %u", diagnostics.MeshCollider3DCount);
				ImGui::Text("Mesh Collider Tris    %u", diagnostics.MeshColliderTriangleCount);
				if (diagnostics.MissingCollider3DCount > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Missing Colliders     %u", diagnostics.MissingCollider3DCount);
				else
					ImGui::Text("Missing Colliders     0");
				if (diagnostics.InvalidMeshCollider3DCount > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Invalid Mesh Colliders %u", diagnostics.InvalidMeshCollider3DCount);
				if (diagnostics.PhysicsBodyCreationFailureCount > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Body Creation Failures %u", diagnostics.PhysicsBodyCreationFailureCount);
				ImGui::Text("Characters            %u / %u active",
				            diagnostics.RuntimeCharacterControllerCount, diagnostics.CharacterControllerCount);
				if (diagnostics.InvalidCharacterColliderCount > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Invalid Characters     %u", diagnostics.InvalidCharacterColliderCount);

				ImGui::SeparatorText("Cameras");
				ImGui::Text("Cameras               %u", diagnostics.CameraCount);
				if (diagnostics.PrimaryCameraCount == 1)
					ImGui::Text("Primary Cameras       1");
				else
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
					                   "Primary Cameras       %u", diagnostics.PrimaryCameraCount);
				ImGui::Text("Active Camera         %s",
				            diagnostics.PrimaryCameraName.empty() ? "<none>" : diagnostics.PrimaryCameraName.c_str());

				ImGui::SeparatorText("Player");
				ImGui::Text("Player Input          %s", diagnostics.PlayerInputEnabled ? "Enabled" : "Disabled");
				ImGui::Text("Eject Camera          %s", diagnostics.EjectCameraActive ? "Active" : "Inactive");
				ImGui::Text("Possessed Pawn        %s",
				            diagnostics.PossessedPawnName.empty() ? "<none>" : diagnostics.PossessedPawnName.c_str());
				if (!diagnostics.PossessedPawnName.empty())
				{
					ImGui::Text("Pawn Grounded         %s", diagnostics.PossessedPawnGrounded ? "Yes" : "No");
					ImGui::Text("Pawn Feet             %.2f, %.2f, %.2f",
					            diagnostics.PossessedPawnFeetPosition.x,
					            diagnostics.PossessedPawnFeetPosition.y,
					            diagnostics.PossessedPawnFeetPosition.z);
					ImGui::Text("Pawn Velocity         %.2f, %.2f, %.2f",
					            diagnostics.PossessedPawnVelocity.x,
					            diagnostics.PossessedPawnVelocity.y,
					            diagnostics.PossessedPawnVelocity.z);
					ImGui::Text("Pawn Capsule          %.2f radius / %.2f half",
					            diagnostics.PossessedPawnCapsuleRadius,
					            diagnostics.PossessedPawnCapsuleHalfHeight);
					ImGui::Text("Pawn Visual Offset    %s", diagnostics.PossessedPawnHasVisualOffset ? "Present" : "Missing");
					if (diagnostics.PossessedPawnHasStats)
					{
						ImGui::Text("Health                %.0f / %.0f",
						            diagnostics.PossessedPawnHealth, diagnostics.PossessedPawnMaxHealth);
						ImGui::Text("Stamina               %.0f / %.0f",
						            diagnostics.PossessedPawnStamina, diagnostics.PossessedPawnMaxStamina);
					}
				}

				ImGui::SeparatorText("Gameplay");
				ImGui::Text("Interactables         %u", diagnostics.InteractableCount);
				ImGui::Text("Pickups               %u", diagnostics.PickupCount);
				ImGui::Text("Zombies               %u active", diagnostics.ActiveZombieCount);
				ImGui::Text("Nearby Interactable   %s",
				            diagnostics.NearbyInteractableName.empty() ? "<none>" : diagnostics.NearbyInteractableName.c_str());

				ImGui::SeparatorText("Runtime UI");
				ImGui::Text("UI Roots              %u", diagnostics.UIRootCount);
				ImGui::Text("UI Document           %s",
				            diagnostics.RuntimeUIDocumentPath.empty() ? "<none>" : diagnostics.RuntimeUIDocumentPath.c_str());
				ImGui::Text("UI Viewport           %.0f x %.0f",
				            diagnostics.RuntimeUIViewportWidth, diagnostics.RuntimeUIViewportHeight);
				ImGui::Text("UI Widgets            %u", diagnostics.RuntimeUIWidgetCount);
				if (diagnostics.RuntimeUIRendered)
					ImGui::Text("UI Draw Status        Rendered");
				else
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "UI Draw Status        Not rendered");
				if (diagnostics.RuntimeUIMissingDocument)
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "UI Warning            Missing document, fallback active");
			}
			ImGui::End();
		}

		if (m_ShowRenderPath)
		{
			// Small panel anchored top-left for switching the scene render path.
			ImGui::SetNextWindowPos(ImVec2(8.0f, 28.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(220.0f, 0.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Render Path", &m_ShowRenderPath);

			int path = (int)Blu::RenderSettings::GetPath();
			const char* items[] = { "Forward", "Deferred" };
			if (ImGui::Combo("Mode", &path, items, IM_ARRAYSIZE(items)))
				Blu::RenderSettings::SetPath((Blu::RenderPath)path);

			if (Blu::RenderSettings::GetPath() == Blu::RenderPath::Deferred)
			{
				bool gbufSSAO = Blu::RenderSettings::GetUseGBufferSSAO();
				if (ImGui::Checkbox("G-Buffer SSAO", &gbufSSAO))
					Blu::RenderSettings::SetUseGBufferSSAO(gbufSSAO);
			}
			ImGui::End();
		}

		if (m_ShowRendering)
		{
		ImGui::Begin("Rendering", &m_ShowRendering);

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

		// ---- Lighting ----
		if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto dirLights = m_ActiveScene->GetAllEntitiesWith<DirectionalLightComponent>();
			DirectionalLightComponent* sun = nullptr;
			for (auto e : dirLights)
			{
				sun = &dirLights.get<DirectionalLightComponent>(e);
				break;
			}

			if (!sun)
				ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Warning: no directional light in scene");

			bool useTod = m_ActiveScene->GetUseTimeOfDay();
			if (ImGui::Checkbox("Time of Day", &useTod))
				m_ActiveScene->SetUseTimeOfDay(useTod);
			ImGui::SameLine();
			if (ImGui::Button("Morning")) { auto& tod = m_ActiveScene->GetTimeOfDay(); tod.NormalizedTime = 0.30f; tod.AutoAdvance = false; }
			ImGui::SameLine();
			if (ImGui::Button("Noon")) { auto& tod = m_ActiveScene->GetTimeOfDay(); tod.NormalizedTime = 0.50f; tod.AutoAdvance = false; }
			ImGui::SameLine();
			if (ImGui::Button("Golden Hour")) { auto& tod = m_ActiveScene->GetTimeOfDay(); tod.NormalizedTime = 0.72f; tod.AutoAdvance = false; }
			ImGui::SameLine();
			if (ImGui::Button("Night")) { auto& tod = m_ActiveScene->GetTimeOfDay(); tod.NormalizedTime = 0.00f; tod.AutoAdvance = false; }

			auto& tod = m_ActiveScene->GetTimeOfDay();
			ImGui::SliderFloat("Time##Lighting", &tod.NormalizedTime, 0.0f, 1.0f, "%.3f");
			ImGui::DragFloat("Sun Azimuth##Lighting", &tod.SunAzimuthDeg, 1.0f, 0.0f, 360.0f, "%.1f deg");
			ImGui::DragFloat("ToD Sun Intensity", &tod.SunMaxStrength, 0.25f, 0.0f, 100.0f);

			if (sun)
			{
				ImGui::DragFloat("Sun Intensity", &sun->Intensity, 0.05f, 0.0f, 100.0f);
				float ambient = (sun->Ambient.x + sun->Ambient.y + sun->Ambient.z) / 3.0f;
				if (ImGui::DragFloat("Ambient Intensity", &ambient, 0.005f, 0.0f, 2.0f, "%.3f"))
					sun->Ambient = glm::vec3(ambient);
			}

			bool useShadows = m_ActiveScene->GetUseShadows();
			if (ImGui::Checkbox("Shadows (CSM)##Lighting", &useShadows))
				m_ActiveScene->SetUseShadows(useShadows);
			ImGui::BeginDisabled();
			float shadowStrength = useShadows ? 1.0f : 0.0f;
			ImGui::SliderFloat("Shadow Strength", &shadowStrength, 0.0f, 1.0f, "%.2f");
			ImGui::EndDisabled();

			auto out = tod.Evaluate(tod.NormalizedTime);
			ImGui::TextDisabled("Elevation %.1f deg | Exposure %.3f | Ambient %.3f",
				out.SunElevationDeg, out.SkyExposure, out.AmbientIntensity);
		}

		ImGui::Spacing();

		// ---- Draw Statistics ----
		if (ImGui::CollapsingHeader("Draw Statistics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Draw Calls  %d", Renderer2D::GetStats().DrawCalls);
			ImGui::Text("Quad Count  %d", Renderer2D::GetStats().QuadCount);
			ImGui::Text("Vertices    %d", Renderer2D::GetStats().GetTotalVertexCount());
		}

		ImGui::Spacing();

		// ---- Time of Day ----
		if (ImGui::CollapsingHeader("Time of Day"))
		{
			bool useTod = m_ActiveScene->GetUseTimeOfDay();
			if (ImGui::Checkbox("Enable ToD", &useTod))
				m_ActiveScene->SetUseTimeOfDay(useTod);

			if (useTod)
			{
				auto& tod = m_ActiveScene->GetTimeOfDay();
				ImGui::Indent();
				ImGui::SliderFloat("Time", &tod.NormalizedTime, 0.0f, 1.0f, "%.3f");
				ImGui::Checkbox("Auto Advance", &tod.AutoAdvance);
				if (tod.AutoAdvance)
					ImGui::DragFloat("Day Duration (s)", &tod.DayDurationSecs, 10.0f, 60.0f, 7200.0f, "%.0f s");
				ImGui::DragFloat("Sun Azimuth",      &tod.SunAzimuthDeg,    1.0f, 0.0f, 360.0f, "%.1f°");
				ImGui::DragFloat("Max Sun Strength", &tod.SunMaxStrength,   0.5f, 0.0f, 100.0f);
				ImGui::DragFloat("Turbidity (noon)", &tod.SunNoonTurbidity, 0.05f, 1.0f, 10.0f, "%.2f");
				ImGui::DragFloat("Turbidity (haze)", &tod.SunHazeTurbidity, 0.05f, 1.0f, 10.0f, "%.2f");

				// Show computed output as a read-only preview
				auto out = tod.Evaluate(tod.NormalizedTime);
				ImGui::Spacing();
				ImGui::TextDisabled("Elevation: %.1f°   Exposure: %.3f", out.SunElevationDeg, out.SkyExposure);
				ImGui::Unindent();
			}
		}

		// ---- Scene Rendering ----
		if (ImGui::CollapsingHeader("Scene Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool useShadows = m_ActiveScene->GetUseShadows();
			if (ImGui::Checkbox("Shadows (CSM)", &useShadows))
				m_ActiveScene->SetUseShadows(useShadows);

			bool usePostProcess = m_ActiveScene->GetUsePostProcess();
			if (ImGui::Checkbox("Post Process", &usePostProcess))
				m_ActiveScene->SetUsePostProcess(usePostProcess);

			if (usePostProcess)
			{
				auto pp = m_ActiveScene->GetPostProcess();
				if (pp)
				{
					ImGui::Indent();
					if (ImGui::Button("Reset Post Process"))
					{
						pp->EnableBloom = true;
						pp->BloomThreshold = 1.0f;
						pp->BloomStrength = 0.05f;
						pp->EnableFXAA = true;
						pp->Preview = PostProcess::PreviewMode::Full;
						pp->EnableSSAO = true;
						pp->SSAORadius = 0.5f;
						pp->SSAOBias = 0.025f;
						pp->SSAOPower = 1.5f;
						pp->SSAOSamples = 16;
						pp->SSAOStrength = 1.0f;
					}
					const char* previewModes[] = { "Full", "Tonemap Only", "Bloom Only", "FXAA Only", "SSAO Only", "Bypass" };
					int preview = (int)pp->Preview;
					if (ImGui::Combo("Preview", &preview, previewModes, IM_ARRAYSIZE(previewModes)))
						pp->Preview = (PostProcess::PreviewMode)preview;
					ImGui::Checkbox("Bloom", &pp->EnableBloom);
					ImGui::DragFloat("Threshold",  &pp->BloomThreshold, 0.05f, 0.0f, 10.0f);
					ImGui::DragFloat("Strength",   &pp->BloomStrength,  0.005f, 0.0f, 2.0f);
					ImGui::Checkbox("FXAA", &pp->EnableFXAA);

					ImGui::Separator();
					ImGui::Checkbox("SSAO", &pp->EnableSSAO);
					if (pp->EnableSSAO)
					{
						ImGui::DragFloat("Radius",   &pp->SSAORadius,   0.01f, 0.05f, 5.0f);
						ImGui::DragFloat("Bias",     &pp->SSAOBias,     0.001f, 0.0f, 0.1f);
						ImGui::DragFloat("Power",    &pp->SSAOPower,    0.1f, 0.5f, 8.0f);
						ImGui::DragFloat("Strength", &pp->SSAOStrength, 0.01f, 0.0f, 1.0f);
						int samples = pp->SSAOSamples;
						if (ImGui::SliderInt("Samples", &samples, 4, 32))
							pp->SSAOSamples = samples;
					}
					ImGui::Unindent();
				}
			}

			ImGui::Separator();

			auto& fog = m_ActiveScene->GetFog();
			if (ImGui::Checkbox("Fog", &fog.Enabled))
				m_ActiveScene->SetFogEnabled(fog.Enabled);

			if (fog.Enabled)
			{
				ImGui::Indent();
				ImGui::ColorEdit3("Fog Color",      &fog.Color.x);
				ImGui::DragFloat("Density",         &fog.Density,       0.0001f, 0.0f,    0.1f,  "%.4f");
				ImGui::DragFloat("Height Start",    &fog.HeightStart,   0.1f,   -100.0f,  100.0f);
				ImGui::DragFloat("Height Falloff",  &fog.HeightDensity, 0.01f,   0.0f,    2.0f,  "%.3f");
				ImGui::SeparatorText("Aerial Perspective");
				ImGui::ColorEdit3("Aerial Color",   &fog.AerialColor.x);
				ImGui::DragFloat("Aerial Strength", &fog.AerialStrength, 0.01f, 0.0f, 1.0f, "%.2f");
				ImGui::Unindent();
			}

			ImGui::Separator();

			bool useSkybox = m_ActiveScene->GetUseSkybox();
			if (ImGui::Checkbox("Skybox", &useSkybox))
				m_ActiveScene->SetUseSkybox(useSkybox);

			if (useSkybox)
			{
				auto sky = m_ActiveScene->GetSkybox();
				if (sky)
				{
					ImGui::Indent();
					ImGui::SeparatorText("Atmosphere (Preetham)");
					ImGui::DragFloat("Turbidity",     &sky->Turbidity,    0.05f,  1.8f, 10.0f,  "%.2f");
					ImGui::DragFloat("Sky Exposure",  &sky->SkyExposure,  0.001f, 0.0f, 0.5f, "%.4f");
					ImGui::ColorEdit3("Ground Color", &sky->GroundColor.x);
					ImGui::SeparatorText("Sun");
					ImGui::ColorEdit3("Sun Color",    &sky->SunColor.x);
					ImGui::DragFloat("Sun Size",      &sky->SunSize,      0.00001f, 0.990f, 0.99999f, "%.5f");
					ImGui::DragFloat("Sun Strength",  &sky->SunStrength,  0.5f,     0.0f,   100.0f);
					ImGui::SeparatorText("Clouds");
					if (ImGui::Button("Clear")) { sky->CloudDensity = 0.0f; sky->CloudCoverage = 0.10f; }
					ImGui::SameLine();
					if (ImGui::Button("Scattered")) { sky->CloudDensity = 0.45f; sky->CloudCoverage = 0.38f; sky->CloudSoftness = 0.55f; sky->CloudShadowing = 0.45f; }
					ImGui::SameLine();
					if (ImGui::Button("Broken")) { sky->CloudDensity = 0.70f; sky->CloudCoverage = 0.58f; sky->CloudSoftness = 0.50f; sky->CloudShadowing = 0.65f; }
					ImGui::SameLine();
					if (ImGui::Button("Overcast")) { sky->CloudDensity = 0.92f; sky->CloudCoverage = 0.82f; sky->CloudSoftness = 0.75f; sky->CloudShadowing = 0.80f; }
					ImGui::ColorEdit3("Cloud Color",     &sky->CloudColor.x);
					ImGui::DragFloat("Coverage",         &sky->CloudCoverage,    0.01f,  0.0f,    1.0f,    "%.2f");
					ImGui::DragFloat("Density",          &sky->CloudDensity,     0.01f,  0.0f,    1.0f,    "%.2f");
					ImGui::DragFloat("Softness",         &sky->CloudSoftness,    0.01f,  0.0f,    1.0f,    "%.2f");
					ImGui::DragFloat("Height",           &sky->CloudHeight,      10.0f,  0.0f,    5000.0f, "%.0f");
					ImGui::DragFloat("Scale",            &sky->CloudScale,       10.0f,  50.0f,   5000.0f, "%.0f");
					ImGui::DragFloat2("Wind Direction",  glm::value_ptr(sky->CloudWindDirection), 0.01f, -1.0f, 1.0f, "%.2f");
					ImGui::DragFloat("Wind Speed",       &sky->CloudScrollSpeed, 0.001f, 0.0f,    0.2f,    "%.3f");
					ImGui::DragFloat("Shadowing",        &sky->CloudShadowing,   0.01f,  0.0f,    1.0f,    "%.2f");
					ImGui::DragFloat("Horizon Fade",     &sky->CloudHorizonFade, 0.01f,  0.02f,   0.8f,    "%.2f");
					ImGui::Unindent();
				}
			}
		}

		// ── Audio ────────────────────────────────────────────────────────────────
		if (ImGui::CollapsingHeader("Audio"))
		{
			float master = AudioEngine::Get().GetMasterVolume();
			if (ImGui::SliderFloat("Master Volume", &master, 0.0f, 1.0f))
				AudioEngine::Get().SetMasterVolume(master);
		}

		// ── IBL (Image-Based Lighting) ────────────────────────────────────────
		if (ImGui::CollapsingHeader("IBL (Image-Based Lighting)"))
		{
			static bool  s_IBLEnabled  = false;
			static float s_IBLStrength = 1.0f;
			static char  s_HDRPath[512] = "";

			if (ImGui::Checkbox("Enable IBL", &s_IBLEnabled))
				Renderer3D::SetIBL(s_IBLEnabled, s_IBLStrength);

			if (s_IBLEnabled)
			{
				if (ImGui::SliderFloat("IBL Strength", &s_IBLStrength, 0.0f, 3.0f))
					Renderer3D::SetIBL(s_IBLEnabled, s_IBLStrength);
			}

			ImGui::InputText("HDR Path", s_HDRPath, sizeof(s_HDRPath));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const char* droppedPath = static_cast<const char*>(payload->Data);
					strncpy_s(s_HDRPath, droppedPath, sizeof(s_HDRPath) - 1);
					s_HDRPath[sizeof(s_HDRPath) - 1] = '\0';
					if (IBLSystem::LoadEnvironment(s_HDRPath))
					{
						s_IBLEnabled = true;
						Renderer3D::SetIBL(true, s_IBLStrength);
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load##IBL"))
			{
				if (IBLSystem::LoadEnvironment(s_HDRPath))
				{
					s_IBLEnabled = true;
					Renderer3D::SetIBL(true, s_IBLStrength);
				}
			}

			if (IBLSystem::IsReady())
			{
				ImGui::TextDisabled("Loaded: %s", IBLSystem::GetHDRPath().c_str());
			}
			else
			{
				ImGui::TextDisabled("No environment loaded — drop an .hdr file path above.");
			}
		}

		ImGui::End(); // Rendering
		} // if (m_ShowRendering)

		// ── Input Map panel ─────────────────────────────────────────────────────
		if (m_ShowInputMap)
		{
			ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Input Map", &m_ShowInputMap))
			{
				auto& imap = InputMap::Get();
				ImGui::TextDisabled("Named actions and axes mapped to key/mouse codes (GLFW).");
				ImGui::Separator();

				// ── File I/O ──────────────────────────────────────────────────
				if (ImGui::Button("Load from file"))
				{
					auto path = FileDialogs::OpenFile("YAML Input Map (*.inputmap)\0*.inputmap\0All Files\0*.*\0");
					if (!path.empty()) imap.LoadFromFile(path);
				}
				ImGui::SameLine();
				if (ImGui::Button("Save to file"))
				{
					auto path = FileDialogs::SaveFile("YAML Input Map (*.inputmap)\0*.inputmap\0All Files\0*.*\0");
					if (!path.empty()) imap.SaveToFile(path);
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear All"))
					imap.Clear();
				ImGui::Separator();

				// ── Quick-add action ─────────────────────────────────────────
				if (ImGui::CollapsingHeader("Add Action"))
				{
					static char actionName[64] = "Jump";
					static int  actionKey = 32; // GLFW_KEY_SPACE
					ImGui::InputText("Name##act",  actionName, sizeof(actionName));
					ImGui::InputInt("Key Code##act", &actionKey);
					if (ImGui::Button("Add Action"))
						imap.AddAction(actionName, actionKey);
				}

				// ── Quick-add axis ───────────────────────────────────────────
				if (ImGui::CollapsingHeader("Add Axis"))
				{
					static char axisName[64] = "Horizontal";
					static int  posKey = 68; // D
					static int  negKey = 65; // A
					ImGui::InputText("Name##ax",   axisName, sizeof(axisName));
					ImGui::InputInt("Positive Key", &posKey);
					ImGui::InputInt("Negative Key", &negKey);
					if (ImGui::Button("Add Axis"))
						imap.AddAxis(axisName, posKey, negKey);
				}

				ImGui::Separator();
				ImGui::TextDisabled("Live state (IsActionPressed / GetAxis):");
				// Display a few well-known actions for quick debugging
				const char* testActions[] = {"Jump","Fire","Sprint","Crouch","Interact"};
				for (const char* a : testActions)
				{
					bool pressed = imap.IsActionPressed(a);
					ImGui::TextColored(pressed ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "%s", a);
					ImGui::SameLine(120.f);
					ImGui::Text(pressed ? "PRESSED" : "---");
				}
				const char* testAxes[] = {"Horizontal","Vertical"};
				for (const char* ax : testAxes)
				{
					float v = imap.GetAxis(ax);
					ImGui::Text("%-12s  %.2f", ax, v);
				}
			}
			ImGui::End();
		}

		// ── Terrain Editor panel ────────────────────────────────────────────────
		if (m_ShowTerrainPanel)
		{
			ImGui::SetNextWindowSize(ImVec2(320, 380), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Terrain Editor", &m_ShowTerrainPanel))
			{
				ImGui::SeparatorText("Grid");
				ImGui::DragInt("Width  (quads)",  &m_TerrainSpec.GridWidth,  1.0f, 2, 1024);
				ImGui::DragInt("Height (quads)",  &m_TerrainSpec.GridHeight, 1.0f, 2, 1024);
				ImGui::DragFloat("Cell Size",     &m_TerrainSpec.CellSize,   0.1f, 0.1f, 100.0f, "%.1f m");

				ImGui::SeparatorText("Height");
				ImGui::DragFloat("Max Height",    &m_TerrainSpec.HeightScale, 0.5f, 0.0f, 2000.0f, "%.1f m");

				ImGui::SeparatorText("Heightmap");
				if (!m_TerrainSpec.HeightmapPath.empty())
				{
					// Show just the filename for readability
					auto filename = std::filesystem::path(m_TerrainSpec.HeightmapPath).filename().string();
					ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", filename.c_str());
				}
				else
				{
					ImGui::TextDisabled("No heightmap — flat terrain");
				}

				if (ImGui::Button("Load Heightmap...", ImVec2(-1, 0)))
				{
					std::string path = FileDialogs::OpenFile(
					    "Image (*.png;*.jpg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All\0*.*\0");
					if (!path.empty())
						m_TerrainSpec.HeightmapPath = AssetPath::ImportTexturePath(path, "Terrain");
				}
				if (!m_TerrainSpec.HeightmapPath.empty() && ImGui::Button("Clear Heightmap", ImVec2(-1, 0)))
					m_TerrainSpec.HeightmapPath.clear();

				ImGui::Spacing();
				ImGui::TextDisabled("~%dk vertices | ~%dk triangles",
				    (m_TerrainSpec.GridWidth + 1) * (m_TerrainSpec.GridHeight + 1) / 1000,
				    m_TerrainSpec.GridWidth * m_TerrainSpec.GridHeight * 2 / 1000);

				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.60f, 0.20f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f));
				bool generate = ImGui::Button("Generate Terrain", ImVec2(-1, 36));
				ImGui::PopStyleColor(2);

				if (generate)
				{
					m_TerrainSpec = Blu::SanitizeTerrainSpec(m_TerrainSpec);
					Entity terrainEntity = m_ActiveScene->CreateEntity("Terrain");
					auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
					terrain.Spec = m_TerrainSpec;
					std::string message;
					if (m_ActiveScene->RebuildTerrain(terrainEntity, &message))
					{
						m_SceneHierarchyPanel->SetSelectedEntity(terrainEntity);
						BLU_CORE_INFO("Terrain generated: {}×{} grid", m_TerrainSpec.GridWidth, m_TerrainSpec.GridHeight);
					}
				}
			}
			ImGui::End();
		}

		if (m_MaterialGraphPanel)
			m_MaterialGraphPanel->OnImGuiRender(&m_ShowMaterialGraph);
		
		
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		// ---- Viewport toolbar (always visible, UE-style) ----
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 0.92f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.f, 3.f));
			ImGui::BeginChild("##VPToolbar", ImVec2(m_ViewportSize.x, 30.f), false,
			                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			const ImVec2 iconSz(16.f, 16.f);
			// Colors for active tool highlight (blue) and active snap (orange)
			const ImVec4 kColActive { 0.20f, 0.45f, 0.85f, 1.f };
			const ImVec4 kColActHov { 0.30f, 0.55f, 0.95f, 1.f };
			const ImVec4 kColSnapOn { 0.85f, 0.55f, 0.10f, 1.f };
			const ImVec4 kColSnapHov{ 0.95f, 0.65f, 0.20f, 1.f };

			// Helper: push 3 button colours when active
			auto PushActive = [&](bool active, bool snap = false) {
				if (active) {
					const ImVec4& c  = snap ? kColSnapOn  : kColActive;
					const ImVec4& ch = snap ? kColSnapHov : kColActHov;
					ImGui::PushStyleColor(ImGuiCol_Button,        c);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ch);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive,  c);
				}
			};
			auto PopActive = [&](bool active) {
				if (active) ImGui::PopStyleColor(3);
			};

			// ---- LEFT: tool buttons ----
			// Select (no gizmo)
			{
				bool a = (m_ImGuizmoType == -1);
				PushActive(a);
				if (ImGui::ImageButton("##sel", (ImTextureID)m_SelectIcon->GetImTextureID(), iconSz))
					m_ImGuizmoType = -1;
				PopActive(a);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select (Q)");
			}
			ImGui::SameLine(0, 4);
			ImGui::TextDisabled("|");
			ImGui::SameLine(0, 4);

			// Translate
			{
				bool a = (m_ImGuizmoType == ImGuizmo::OPERATION::TRANSLATE);
				PushActive(a);
				if (ImGui::ImageButton("##trans", (ImTextureID)m_TranslationIcon->GetImTextureID(), iconSz))
					m_ImGuizmoType = ImGuizmo::OPERATION::TRANSLATE;
				PopActive(a);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate (W)");
			}
			ImGui::SameLine(0, 2);
			// Rotate
			{
				bool a = (m_ImGuizmoType == ImGuizmo::OPERATION::ROTATE);
				PushActive(a);
				if (ImGui::ImageButton("##rot", (ImTextureID)m_RotationIcon->GetImTextureID(), iconSz))
					m_ImGuizmoType = ImGuizmo::OPERATION::ROTATE;
				PopActive(a);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (E)");
			}
			ImGui::SameLine(0, 2);
			// Scale
			{
				bool a = (m_ImGuizmoType == ImGuizmo::OPERATION::SCALE);
				PushActive(a);
				if (ImGui::ImageButton("##scl", (ImTextureID)m_ScaleIcon->GetImTextureID(), iconSz))
					m_ImGuizmoType = ImGuizmo::OPERATION::SCALE;
				PopActive(a);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (R)");
			}
			ImGui::SameLine(0, 6);
			ImGui::TextDisabled("|");
			ImGui::SameLine(0, 6);

			// World / Local space toggle
			{
				bool isLocal = (m_OperationMode == (int)ImGuizmo::MODE::LOCAL);
				ImTextureID spIcon = isLocal
					? (ImTextureID)m_LocalSpaceIcon->GetImTextureID()
					: (ImTextureID)m_WorldSpaceIcon->GetImTextureID();
				if (ImGui::ImageButton("##space", spIcon, iconSz))
					m_OperationMode = isLocal ? (int)ImGuizmo::MODE::WORLD : (int)ImGuizmo::MODE::LOCAL;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(isLocal ? "Local Space (click for World)" : "World Space (click for Local)");
			}
			ImGui::SameLine(0, 6);
			ImGui::TextDisabled("|");
			ImGui::SameLine(0, 6);

			// ---- SNAP BUTTONS (toggle + preset dropdown) ----
			// Preset tables
			static const char* kGLabels[] = { "1","2","5","10","25","50","100" };
			static const float kGVals[]   = { 1.f,2.f,5.f,10.f,25.f,50.f,100.f };
			static const char* kRLabels[] = { "5","10","15","22.5","45","90" };
			static const float kRVals[]   = { 5.f,10.f,15.f,22.5f,45.f,90.f };
			static const char* kSLabels[] = { "0.0625","0.125","0.25","0.5","1" };
			static const float kSVals[]   = { 0.0625f,0.125f,0.25f,0.5f,1.f };

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 2.f));

			// Grid / Translation snap — snapshot bool BEFORE button so push/pop use the same value
			{ bool was = enableTranslationSnap;
			PushActive(was, /*snap=*/true);
			if (ImGui::ImageButton("##snapG", (ImTextureID)m_SnappingIcon->GetImTextureID(), ImVec2(14,14)))
				enableTranslationSnap = !enableTranslationSnap;
			PopActive(was); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Translation Snap");
			ImGui::SameLine(0, 1);
			{
				char lbl[24]; snprintf(lbl, sizeof(lbl), "%.4g##tval", translationSnapValue);
				if (ImGui::Button(lbl)) ImGui::OpenPopup("##tsnap");
				if (ImGui::BeginPopup("##tsnap")) {
					ImGui::TextDisabled("Grid Snap (cm)");
					ImGui::Separator();
					for (int i = 0; i < IM_ARRAYSIZE(kGVals); i++) {
						bool s = (fabsf(translationSnapValue - kGVals[i]) < 0.001f);
						if (ImGui::Selectable(kGLabels[i], s)) translationSnapValue = kGVals[i];
					}
					ImGui::EndPopup();
				}
			}
			ImGui::SameLine(0, 6);

			// Rotation snap
			{ bool was = enableRotationSnap;
			PushActive(was, true);
			if (ImGui::ImageButton("##snapR", (ImTextureID)m_RotationIcon->GetImTextureID(), ImVec2(14,14)))
				enableRotationSnap = !enableRotationSnap;
			PopActive(was); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Rotation Snap");
			ImGui::SameLine(0, 1);
			{
				char lbl[24]; snprintf(lbl, sizeof(lbl), "%.4g\xc2\xb0##rval", rotationSnapValue); // °
				if (ImGui::Button(lbl)) ImGui::OpenPopup("##rsnap");
				if (ImGui::BeginPopup("##rsnap")) {
					ImGui::TextDisabled("Rotation Snap (\xc2\xb0)");
					ImGui::Separator();
					for (int i = 0; i < IM_ARRAYSIZE(kRVals); i++) {
						bool s = (fabsf(rotationSnapValue - kRVals[i]) < 0.001f);
						char rl[16]; snprintf(rl, sizeof(rl), "%s\xc2\xb0", kRLabels[i]);
						if (ImGui::Selectable(rl, s)) rotationSnapValue = kRVals[i];
					}
					ImGui::EndPopup();
				}
			}
			ImGui::SameLine(0, 6);

			// Scale snap
			{ bool was = enableScaleSnap;
			PushActive(was, true);
			if (ImGui::ImageButton("##snapS", (ImTextureID)m_ScaleIcon->GetImTextureID(), ImVec2(14,14)))
				enableScaleSnap = !enableScaleSnap;
			PopActive(was); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Scale Snap");
			ImGui::SameLine(0, 1);
			{
				char lbl[24]; snprintf(lbl, sizeof(lbl), "%.4g##sval", scaleSnapValue);
				if (ImGui::Button(lbl)) ImGui::OpenPopup("##ssnap");
				if (ImGui::BeginPopup("##ssnap")) {
					ImGui::TextDisabled("Scale Snap");
					ImGui::Separator();
					for (int i = 0; i < IM_ARRAYSIZE(kSVals); i++) {
						bool s = (fabsf(scaleSnapValue - kSVals[i]) < 0.001f);
						if (ImGui::Selectable(kSLabels[i], s)) scaleSnapValue = kSVals[i];
					}
					ImGui::EndPopup();
				}
			}

			// Camera speed (kept but compact)
			ImGui::SameLine(0, 6);
			ImGui::TextDisabled("|");
			ImGui::SameLine(0, 6);
			if (ImGui::ImageButton("##cam", (ImTextureID)m_CameraIcon->GetImTextureID(), ImVec2(14,14)))
				ImGui::OpenPopup("##camspd");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Camera Speed");
			if (ImGui::BeginPopup("##camspd")) {
				ImGui::Text("Camera Speed");
				ImGui::PushItemWidth(80.f);
				ImGui::DragFloat("##cs", &m_EditorCamera.GetCameraSpeed(), 0.05f, 0.05f, 500.f, "%.2f");
				ImGui::PopItemWidth();
				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(); // FramePadding 2,2

			// ── Terrain mode toggle (left side, before right-aligned dropdowns) ──
			ImGui::SameLine(0, 6);
			ImGui::TextDisabled("|");
			ImGui::SameLine(0, 6);
			{
				bool active = m_ShowTerrainPanel;
				if (active) {
					ImGui::PushStyleColor(ImGuiCol_Button,        kColActive);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColActHov);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kColActive);
				}
				if (ImGui::Button("Terrain"))
					m_ShowTerrainPanel = !m_ShowTerrainPanel;
				if (active) ImGui::PopStyleColor(3);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Terrain Editor");
			}

			// ---- RIGHT: Perspective + Lit dropdowns ----
			{
				const char* perspStr = m_EditorCamera.IsOrthographic() ? "Orthographic" : "Perspective";
				const char* litStr   = (m_ViewMode == ViewMode::Wireframe) ? "Wireframe"
				                     : (m_ViewMode == ViewMode::Unlit)     ? "Unlit" : "Lit";

				// Compute right-side button widths so we can right-align them.
				// Use SameLine(rightX) instead of SetCursorPosX so ImGui correctly
				// jumps to the target X even if the cursor is already past it from
				// the accumulated SameLine calls on the left side.
				float perspW = ImGui::CalcTextSize(perspStr).x + 14.f;
				float litW   = ImGui::CalcTextSize(litStr).x   + 14.f;
				float showW  = 54.0f;
				float rightX = ImGui::GetWindowWidth() - perspW - litW - showW - 20.f;
				ImGui::SameLine(rightX);

				// Perspective / Orthographic dropdown
				if (ImGui::Button(perspStr, ImVec2(perspW, 0)))
					ImGui::OpenPopup("##persp");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Camera Projection");
				if (ImGui::BeginPopup("##persp")) {
					ImGui::TextDisabled("PERSPECTIVE");
					if (ImGui::Selectable("Perspective", !m_EditorCamera.IsOrthographic())) {
						m_EditorCamera.SetOrthographic(false);
					}
					ImGui::Separator();
					ImGui::TextDisabled("ORTHOGRAPHIC");
					// Cardinal view entries: name, pitch, yaw, switch to ortho
					struct CardinalView { const char* name; float pitch; float yaw; };
					static const CardinalView kViews[] = {
						{ "Top",    glm::radians( 90.f), 0.f                  },
						{ "Bottom", glm::radians(-90.f), 0.f                  },
						{ "Front",  0.f,                 0.f                  },
						{ "Back",   0.f,                 glm::radians(180.f)  },
						{ "Left",   0.f,                 glm::radians(-90.f)  },
						{ "Right",  0.f,                 glm::radians( 90.f)  },
					};
					for (const auto& v : kViews) {
						if (ImGui::Selectable(v.name)) {
							m_EditorCamera.SetOrthographic(true);
							m_EditorCamera.SetPitchYaw(v.pitch, v.yaw);
						}
					}
					ImGui::EndPopup();
				}
				ImGui::SameLine(0, 4);

				// Lit / Unlit / Wireframe dropdown
				if (ImGui::Button(litStr, ImVec2(litW, 0)))
					ImGui::OpenPopup("##litmode");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("View Mode");
				if (ImGui::BeginPopup("##litmode")) {
					ImGui::TextDisabled("VIEW MODE");
					ImGui::Separator();
					if (ImGui::Selectable("Lit",       m_ViewMode == ViewMode::Lit))       m_ViewMode = ViewMode::Lit;
					if (ImGui::Selectable("Unlit",     m_ViewMode == ViewMode::Unlit))     m_ViewMode = ViewMode::Unlit;
					if (ImGui::Selectable("Wireframe", m_ViewMode == ViewMode::Wireframe)) m_ViewMode = ViewMode::Wireframe;
					ImGui::EndPopup();
				}
				ImGui::SameLine(0, 4);
				if (ImGui::Button("Show", ImVec2(showW, 0)))
					ImGui::OpenPopup("##viewportShow");
				if (ImGui::BeginPopup("##viewportShow"))
				{
					ImGui::TextDisabled("DEBUG VISIBILITY");
					ImGui::Separator();
					ImGui::Checkbox("Selected Colliders", &m_ShowSelectedColliderDebug);
					ImGui::Checkbox("Character Capsule / Feet", &m_ShowCharacterDebug);
					ImGui::Checkbox("Mesh Collider Bounds", &m_ShowMeshColliderDebug);
					ImGui::Checkbox("Camera Markers", &m_ShowCameraDebug);
					ImGui::EndPopup();
				}
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();  // FramePadding 3,3
			ImGui::PopStyleColor(); // ChildBg
		}
		ImGui::PopStyleVar(); // WindowPadding from outer Begin("Viewport")
		
		m_ViewPortFocused = ImGui::IsWindowFocused();
		m_ViewPortHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
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
				m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
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

		if (m_ActiveScene && m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
		{
			auto stateName = [&]() -> const char*
			{
				switch (m_SceneState)
				{
					case SceneState::Edit:     return "Edit";
					case SceneState::Play:     return "Play";
					case SceneState::Pause:    return "Pause";
					case SceneState::Simulate: return "Simulate";
					case SceneState::Eject:    return "Eject";
				}
				return "Unknown";
			};

			SceneDiagnostics diagnostics = m_ActiveScene->GetDiagnostics();
			char overlay[256];
			snprintf(overlay, sizeof(overlay), "%s | Pawn %s | Input %s | Character %u/%u | %s",
			         stateName(),
			         diagnostics.PossessedPawnName.empty() ? "<none>" : diagnostics.PossessedPawnName.c_str(),
			         diagnostics.PlayerInputEnabled ? "On" : "Off",
			         diagnostics.RuntimeCharacterControllerCount,
			         diagnostics.CharacterControllerCount,
			         diagnostics.PossessedPawnGrounded ? "Grounded" : "Airborne");

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImVec2(m_ViewportBounds[0].x + 10.0f, m_ViewportBounds[0].y + 10.0f);
			ImVec2 textSize = ImGui::CalcTextSize(overlay);
			drawList->AddRectFilled(pos, ImVec2(pos.x + textSize.x + 16.0f, pos.y + textSize.y + 10.0f),
			                        IM_COL32(18, 18, 18, 190), 4.0f);
			drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 5.0f), IM_COL32(225, 225, 225, 255), overlay);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				std::filesystem::path payloadPath = std::string(reinterpret_cast<const char*>(payload->Data));
				std::string ext = payloadPath.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				static const std::unordered_set<std::string> s_ModelExtensions = {
					".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply"
				};

				if (s_ModelExtensions.count(ext))
				{
					QueueStaticCollisionPrompt(ImportModelEntity(m_ActiveScene, payloadPath));
				}
				else if (ext == ".bluprefab")
				{
					Entity instance = InstantiatePrefabAsset(payloadPath);
					if (instance && instance.HasComponent<TransformComponent>())
					{
						auto& transform = instance.GetComponent<TransformComponent>();
						transform.Translation = m_EditorCamera.GetPosition() + m_EditorCamera.GetForwardDirection() * 8.0f;
					}
				}
				else if (ext == ".blu" || ext == ".scene")
				{
					OpenScene(payloadPath);
					m_ActiveScene->SetSceneFilePath(payloadPath);
					SceneSerializer serializer(m_EditorScene);
					serializer.SerializeLoadedScene(payloadPath.string());
				}
				else
				{
					BLU_CORE_WARN("Viewport drop: unsupported asset type '{0}' for {1}", ext, payloadPath.generic_string());
				}
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
		// Queue a deferred pick. We avoid ReadPixel here because:
		//   1. Events fire in Window::OnUpdate() before layer->OnUpdate() renders the scene.
		//   2. m_ViewPortHovered is stale (set in the previous frame's OnGuiDraw).
		// The deferred pick runs in OnUpdate after rendering; it uses the live OS
		// mouse position and validates viewport containment via bounds, so we do
		// NOT need m_ViewPortHovered here.
		if (event.GetButton() == BLU_MOUSE_BUTTON_LEFT)
			m_PendingEntityPick = true;
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
			if (!ImGui::GetIO().WantCaptureKeyboard) m_ImGuizmoType = -1;
			break;
		case BLU_KEY_W:
			if (!ImGui::GetIO().WantCaptureKeyboard) m_ImGuizmoType = ImGuizmo::OPERATION::TRANSLATE;
			break;
		case BLU_KEY_E:
			if (!ImGui::GetIO().WantCaptureKeyboard) m_ImGuizmoType = ImGuizmo::OPERATION::ROTATE;
			break;
		case BLU_KEY_R:
			if (!ImGui::GetIO().WantCaptureKeyboard) m_ImGuizmoType = ImGuizmo::OPERATION::SCALE;
			break;
		case BLU_KEY_F:
		{
			// Focus the editor camera's orbit pivot on the selected entity.
			// This makes the camera orbit around the entity rather than a stale focal point.
			if (!ImGui::GetIO().WantCaptureKeyboard && (m_ViewPortFocused || m_ViewPortHovered))
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
		case BLU_KEY_DELETE:
		{
			if (!ImGui::GetIO().WantCaptureKeyboard)
			{
				Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
				if (selectedEntity)
				{
					m_ActiveScene->DestroyEntity(selectedEntity);
					m_SceneHierarchyPanel->SetSelectedEntity({});
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
		// ImGui_ImplGlfw_InitForOther(window, true) already installs the GLFW
		// char callback and chains to ours — characters are added once by ImGui.
		// Do NOT call io.AddInputCharacter() here; that would double every keystroke.
		return false;
	}

	

}
