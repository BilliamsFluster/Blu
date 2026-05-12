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
#include "Blu/Rendering/ModelLoader.h"
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

		// DX11 render targets are stored top-down: screen Y == framebuffer row, no flip needed.
		// OpenGL framebuffers are stored bottom-up: flip Y so row 0 maps to the bottom of the viewport.
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL)
			my = viewportSize.y - my - m_ViewportOffset.y;
		float mouseX = (float)mx;
		float mouseY = (float)my;
		m_MousePosX = mouseX;
		m_MousePosY = mouseY;

		// m_DrawnEntityID is only needed on left-click (entity selection).
		// ReadPixel is a synchronous GPU stall — do NOT call it every frame.



		

		

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
		
		



	}

	void BluEditorLayer::OnEvent(Events::Event& event)
	{
		m_CameraController.OnEvent(event);
		// Only forward events to the editor camera when the viewport is hovered.
		// Without this guard, scroll-wheel events zoom the camera even when the
		// mouse is over a detail panel, properties window, etc.
		if (m_ViewPortHovered)
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
		ImGui::Image((ImTextureID)m_AppHeaderIcon->GetImTextureID(),
		             ImVec2(logoSz, logoSz), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::SameLine(0, 8.f);

		// ── Menus ───────────────────────────────────────────────────────────────
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New",        "Ctrl+N"))         NewScene();
			if (ImGui::MenuItem("Open...",    "Ctrl+O"))         OpenScene();
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))   SaveSceneAs();
			if (ImGui::MenuItem("Save",       "Ctrl+S"))         SaveCurrentScene();
			ImGui::Separator();
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
				serializer.DeserializeEntityScriptInstances(
				    m_ActiveScene->GetSceneFilePath().string());
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Outliner",        nullptr, &m_ShowOutliner);
			ImGui::MenuItem("Details",         nullptr, &m_ShowDetails);
			ImGui::MenuItem("Content Browser", nullptr, &m_ShowContentBrowser);
			ImGui::MenuItem("Output Log",      nullptr, &m_ShowOutputLog);
			ImGui::MenuItem("Rendering",       nullptr, &m_ShowRendering);
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
				ImGui::InvisibleButton(id, ImVec2(ctrlW, barH));
				bool clicked = ImGui::IsItemClicked();
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
				return clicked;
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

	void BluEditorLayer::OnGuiDraw()
	{
		float height = 5.0f;
		UIDrawTitlebar(height);

		// Always pass pointers so ImGui::Begin handles hide/show from both the
		// Window menu checkboxes and the title-bar close (X) button.
		m_SceneHierarchyPanel->OnImGuiRender(&m_ShowOutliner, &m_ShowDetails);

		if (m_ShowContentBrowser)
			m_ContentBrowserPanel->OnImGuiRender();

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

		// ---- Draw Statistics ----
		if (ImGui::CollapsingHeader("Draw Statistics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Draw Calls  %d", Renderer2D::GetStats().DrawCalls);
			ImGui::Text("Quad Count  %d", Renderer2D::GetStats().QuadCount);
			ImGui::Text("Vertices    %d", Renderer2D::GetStats().GetTotalVertexCount());
		}

		ImGui::End(); // Rendering
		} // if (m_ShowRendering)
		
		
		
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
				float rightX = ImGui::GetWindowWidth() - perspW - litW - 12.f;
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
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();  // FramePadding 3,3
			ImGui::PopStyleColor(); // ChildBg
		}
		ImGui::PopStyleVar(); // WindowPadding from outer Begin("Viewport")
		
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
					Entity modelEntity = m_ActiveScene->CreateEntity(payloadPath.stem().string());
					auto& mc = modelEntity.AddComponent<MeshComponent>();
					mc.ModelAsset = ModelLoader::Load(payloadPath.string());
					mc.FilePath = payloadPath.string();
				}
				else
				{
					OpenScene(payloadPath);
					m_ActiveScene->SetSceneFilePath(payloadPath);
					SceneSerializer serializer(m_EditorScene);
					serializer.SerializeLoadedScene(payloadPath.string());
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
		if (m_MousePosX >= 0 && m_MousePosY >= 0 && m_MousePosX < (int)m_ViewportSize.x && m_MousePosY < (int)m_ViewportSize.y)
		{
			if (m_ViewPortHovered)
			{
				// Don't select when clicking a gizmo handle — let ImGuizmo consume the input.
				if (ImGuizmo::IsOver() && m_ImGuizmoType != -1)
					return false;

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