#include "Blupch.h"
#include "ImGuiLayer.h"
#include "Blu/Core/Log.h"
#include "Blu/Platform/OpenGL/ImGuiOpenGLRenderer.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"
#include "Blu/Events/EventDispatcher.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Rendering/RendererAPI.h"
#include <imgui.h>
#include "ImGuizmo.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_glfw.h>
#include "Blu/Platform/DirectX11/D3D11Context.h"

namespace Blu
{
	namespace Layers
	{
		
		ImGuiLayer::ImGuiLayer()
			:Layer("ImGuiLayer")
		{
		}
		ImGuiLayer::~ImGuiLayer()
		{
		}
		void ImGuiLayer::OnAttach()
		{
			BLU_PROFILE_FUNCTION();
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			//ImGui::StyleColorsDark();
			SetDarkColors();


			ImGuiIO& io = ImGui::GetIO();
			Application& app = Application::Get();
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			// Get the framebuffer size
			int FrameBufferWidth, FrameBufferHeight;
			glfwGetFramebufferSize(window, &FrameBufferWidth, &FrameBufferHeight);

			// Get the window size
			int WindowWidth, WindowHeight;
			glfwGetWindowSize(window, &WindowWidth, &WindowHeight);

			io.DisplaySize = ImVec2((float)FrameBufferWidth, (float)FrameBufferHeight);
			float time = (float)glfwGetTime();
			static float m_Time = 0.0f;
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;
			io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/static/OpenSans-Bold.ttf", 18.0f);

			io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/static/OpenSans-Light.ttf", 18.0f);
			
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			// ViewportsEnable (floating platform windows) is intentionally disabled.
			// With the DX11 + GLFW backend, popup menus become separate OS windows
			// whose per-window swap chains are not created reliably, so they never
			// appear on screen.  Keeping popups in-window via the standard ImGui
			// draw-list z-order fixes File/Script/Window dropdowns.
			// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
			
			ImGuiStyle& style = ImGui::GetStyle();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}
			// Always initialize the GLFW backend so it installs GLFW callbacks and
			// forwards input via io.AddKeyEvent() / io.AddMouseButtonEvent() etc.
			// (the new ImGui API). This prevents the "not both APIs" assertion that
			// fires when the application also writes legacy io.KeysDown[].
			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			{
				auto* ctx = D3D11Context::Get();
				ImGui_ImplGlfw_InitForOther(window, true);
				ImGui_ImplDX11_Init(ctx->GetDevice(), ctx->GetDeviceContext());
			}
			else
			{
				ImGui_ImplGlfw_InitForOpenGL(window, true);
				ImGui_ImplOpenGL3_Init("#version 410");
			}

		}
		void ImGuiLayer::OnDetach()
		{
			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				ImGui_ImplDX11_Shutdown();
			else
				ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();   // always present now
			ImGui::DestroyContext();
		}

		void ImGuiLayer::OnGuiDraw()
		{
			ImGuiIO& io = ImGui::GetIO();
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			
			
			float time = (float)glfwGetTime();
			static float m_Time = 0.0f;
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;
			
		}

		void ImGuiLayer::SetDarkColors()
		{
			ImGuiStyle& style = ImGui::GetStyle();

			// ── Geometry: rounded + breathable so panels read less "geometric" ──────
			style.WindowRounding        = 6.0f;
			style.ChildRounding         = 6.0f;
			style.FrameRounding         = 4.0f;
			style.PopupRounding         = 5.0f;
			style.GrabRounding          = 4.0f;
			style.TabRounding           = 5.0f;
			style.ScrollbarRounding     = 9.0f;
			style.WindowBorderSize      = 1.0f;
			style.ChildBorderSize       = 1.0f;
			style.FrameBorderSize       = 0.0f;
			style.PopupBorderSize       = 1.0f;
			style.WindowPadding         = ImVec2(10.0f, 10.0f);
			style.FramePadding          = ImVec2(8.0f, 5.0f);
			style.CellPadding           = ImVec2(6.0f, 4.0f);
			style.ItemSpacing           = ImVec2(8.0f, 6.0f);
			style.ItemInnerSpacing      = ImVec2(6.0f, 6.0f);
			style.IndentSpacing         = 18.0f;
			style.ScrollbarSize         = 13.0f;
			style.GrabMinSize           = 9.0f;
			style.WindowTitleAlign      = ImVec2(0.0f, 0.5f);

			// ── Palette: neutral dark with a single blue accent (selection/active) ──
			const ImVec4 accent    = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			const ImVec4 accentDim = ImVec4(0.26f, 0.59f, 0.98f, 0.55f);
			auto& c = style.Colors;
			c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
			c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
			c[ImGuiCol_WindowBg]             = ImVec4(0.090f, 0.094f, 0.102f, 1.00f);
			c[ImGuiCol_ChildBg]              = ImVec4(0.107f, 0.112f, 0.122f, 1.00f);
			c[ImGuiCol_PopupBg]              = ImVec4(0.118f, 0.124f, 0.135f, 0.98f);
			c[ImGuiCol_Border]               = ImVec4(0.000f, 0.000f, 0.000f, 0.40f);
			c[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
			c[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
			c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
			c[ImGuiCol_FrameBgActive]        = ImVec4(0.26f, 0.28f, 0.31f, 1.00f);
			c[ImGuiCol_TitleBg]              = ImVec4(0.070f, 0.075f, 0.085f, 1.00f);
			c[ImGuiCol_TitleBgActive]        = ImVec4(0.100f, 0.105f, 0.118f, 1.00f);
			c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.070f, 0.075f, 0.085f, 1.00f);
			c[ImGuiCol_MenuBarBg]            = ImVec4(0.085f, 0.090f, 0.098f, 1.00f);
			c[ImGuiCol_ScrollbarBg]          = ImVec4(0.040f, 0.040f, 0.045f, 0.60f);
			c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.30f, 0.33f, 1.00f);
			c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.38f, 0.42f, 1.00f);
			c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.44f, 0.48f, 1.00f);
			c[ImGuiCol_CheckMark]            = accent;
			c[ImGuiCol_SliderGrab]           = accent;
			c[ImGuiCol_SliderGrabActive]     = ImVec4(0.40f, 0.69f, 1.00f, 1.00f);
			c[ImGuiCol_Button]               = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
			c[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.31f, 0.35f, 1.00f);
			c[ImGuiCol_ButtonActive]         = ImVec4(0.24f, 0.27f, 0.31f, 1.00f);
			c[ImGuiCol_Header]               = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
			c[ImGuiCol_HeaderHovered]        = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
			c[ImGuiCol_HeaderActive]         = accentDim;
			c[ImGuiCol_Separator]            = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
			c[ImGuiCol_SeparatorHovered]     = accentDim;
			c[ImGuiCol_SeparatorActive]      = accent;
			c[ImGuiCol_ResizeGrip]           = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
			c[ImGuiCol_ResizeGripHovered]    = accentDim;
			c[ImGuiCol_ResizeGripActive]     = accent;
			c[ImGuiCol_Tab]                  = ImVec4(0.100f, 0.105f, 0.118f, 1.00f);
			c[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
			c[ImGuiCol_TabActive]            = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
			c[ImGuiCol_TabUnfocused]         = ImVec4(0.085f, 0.090f, 0.100f, 1.00f);
			c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.130f, 0.140f, 0.160f, 1.00f);
			c[ImGuiCol_DockingPreview]       = accentDim;
			c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.090f, 0.094f, 0.100f, 1.00f);
			c[ImGuiCol_TextSelectedBg]       = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
			c[ImGuiCol_NavHighlight]         = accent;
			c[ImGuiCol_DragDropTarget]       = ImVec4(1.00f, 0.80f, 0.20f, 0.90f);
			c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
			c[ImGuiCol_TableBorderStrong]    = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
			c[ImGuiCol_TableBorderLight]     = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
			c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
		}

		

		
		void ImGuiLayer::Begin()
		{
			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				ImGui_ImplDX11_NewFrame();
			else
				ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();   // always — processes GLFW input for ImGui
			ImGui::NewFrame();
			ImGuizmo::BeginFrame();
		}
		void ImGuiLayer::End()
		{
			ImGuiIO& io = ImGui::GetIO();

			ImGui::Render();

			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			{
				auto* ctx = D3D11Context::Get();
				auto* dc  = ctx->GetDeviceContext();
				// Bind the main window backbuffer so ImGui draws onto it
				ID3D11RenderTargetView* rtv = ctx->GetBackbufferRTV();
				dc->OMSetRenderTargets(1, &rtv, nullptr);
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
				}
			}
			else
			{
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					GLFWwindow* backup_current_context = glfwGetCurrentContext();
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
					glfwMakeContextCurrent(backup_current_context);
				}
			}
		}
		

		
		void ImGuiLayer::DrawDockspace()
		{

			static bool dockspaceOpen = true;
			static bool opt_fullscreen = true;
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			// Do NOT include ImGuiWindowFlags_MenuBar here — the menu bar is rendered
			// separately via BeginMainMenuBar() in UIDrawTitlebar. Keeping MenuBar in
			// the dockspace flags creates a hidden blank strip that sits over docked
			// panels' tab bars, offsetting hit-testing from the visual rendering.
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
			if (opt_fullscreen)
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				// Use WorkPos/WorkSize instead of Pos/Size so the dockspace correctly
				// sits below the main menu bar (WorkPos.y > Pos.y after BeginMainMenuBar
				// has been called on the previous frame).
				ImGui::SetNextWindowPos(viewport->WorkPos);
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			}

			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Blu Dockspace", &dockspaceOpen, window_flags);
			ImGui::PopStyleVar();

			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();



			if (opt_fullscreen)
				ImGui::PopStyleVar(2);

			

			ImGuiStyle& style = ImGui::GetStyle();
			float minWinSizeX = style.WindowMinSize.x;
			style.WindowMinSize.x = 350.0f;
			ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("BluDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			style.WindowMinSize.x = minWinSizeX;

			ImGui::End();
		}


	}
}
