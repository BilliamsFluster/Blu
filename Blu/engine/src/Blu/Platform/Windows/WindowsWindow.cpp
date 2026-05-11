#include "Blupch.h"
#include "WindowsWindow.h"
#include <glad/glad.h>
#include "Blu/Core/Log.h"
#include "Blu/Core/Application.h"
#include "Blu/Events/GLFWCallbacks.h"
#include "Blu/Platform/OpenGL/OpenGLContext.h"
#include "Blu/Platform/DirectX11/D3D11Context.h"
#include "Blu/Rendering/RendererAPI.h"

#include "imgui.h"
#include "imgui_internal.h"

// Win32 native GLFW handle — must be defined before glfw3native.h
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Blu
{
	static bool    s_GLFWInitialized = false;
	static WNDPROC s_GlfwWndProc     = nullptr;   // original GLFW window proc

	// -------------------------------------------------------------------------
	// Custom Win32 window procedure
	//
	// Intercepts three messages that the stock GLFW proc does not handle
	// correctly for a borderless-but-resizable window:
	//
	//  WM_NCCALCSIZE  – Returns 0 so Windows treats the entire window rect as
	//                   client area (no NC frame / caption drawn).  When the
	//                   window is maximised Windows adds an ~8 px "sizeframe"
	//                   margin on every side to push resize handles off-screen;
	//                   we strip that margin so content fills the monitor edge
	//                   to edge and our ImGui controls land at the right pixel.
	//
	//  WM_NCHITTEST   – Delegates resize-border detection to DefWindowProc (it
	//                   honours WS_THICKFRAME), then returns HTCAPTION over
	//                   whatever the ImGui layer marked as the title bar so the
	//                   OS allows window-dragging without a native caption.
	//
	//  WM_NCACTIVATE  – Suppresses the NC-area repaint Windows triggers on
	//                   focus change (which would flash a ghost title bar).
	//
	//  WM_SIZE        – Keeps the D3D11 swap chain in sync whenever the window
	//                   surface changes dimensions (maximise, restore, drag).
	// -------------------------------------------------------------------------
	static LRESULT CALLBACK BlaWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
			case WM_NCCALCSIZE:
			{
				if (wParam)
				{
					auto& r = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0];
					if (IsZoomed(hwnd))
					{
						// Trim the margins Windows adds beyond the monitor edges
						// when maximised so client content fills the whole screen.
						int bx = GetSystemMetrics(SM_CXSIZEFRAME)
						       + GetSystemMetrics(SM_CXPADDEDBORDER);
						int by = GetSystemMetrics(SM_CYSIZEFRAME)
						       + GetSystemMetrics(SM_CXPADDEDBORDER);
						r.left   += bx;  r.right  -= bx;
						r.top    += by;  r.bottom -= by;
					}
					return 0;   // entire window rect → client area
				}
				break;
			}

			case WM_NCHITTEST:
			{
				// WM_NCCALCSIZE returns 0 so the ENTIRE window rect is client area.
				// DefWindowProc therefore returns HTCLIENT for the whole window —
				// it can no longer find the WS_THICKFRAME border pixels because
				// they are inside the client rect.  Detect resize zones manually
				// by comparing the cursor against the physical window rectangle.
				if (!IsZoomed(hwnd))
				{
					POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
					RECT  rc;
					GetWindowRect(hwnd, &rc);

					const int bx = GetSystemMetrics(SM_CXSIZEFRAME)
					             + GetSystemMetrics(SM_CXPADDEDBORDER);
					const int by = GetSystemMetrics(SM_CYSIZEFRAME)
					             + GetSystemMetrics(SM_CXPADDEDBORDER);

					bool l = pt.x <  rc.left   + bx;
					bool r = pt.x >= rc.right  - bx;
					bool t = pt.y <  rc.top    + by;
					bool b = pt.y >= rc.bottom - by;

					if (l && t) return HTTOPLEFT;
					if (r && t) return HTTOPRIGHT;
					if (l && b) return HTBOTTOMLEFT;
					if (r && b) return HTBOTTOMRIGHT;
					if (l)      return HTLEFT;
					if (r)      return HTRIGHT;
					if (t)      return HTTOP;
					if (b)      return HTBOTTOM;
				}

				// Inner region: let ImGui decide caption (drag) vs client.
				auto& win = static_cast<WindowsWindow&>(Application::Get().GetWindow());
				if (win.IsTitleBarHovered()) return HTCAPTION;

				return HTCLIENT;
			}

			case WM_NCACTIVATE:
				// Suppress the NC repaint on focus-change so no ghost title bar
				// flashes.  lParam = -1 tells Windows to skip NC drawing.
				return DefWindowProc(hwnd, msg, wParam, -1L);

			case WM_SIZE:
			{
				if (wParam != SIZE_MINIMIZED)
				{
					UINT w = LOWORD(lParam), h = HIWORD(lParam);
					if (w && h && RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
						if (auto* ctx = D3D11Context::Get())
							ctx->Resize(w, h);
				}
				break;   // still let GLFW dispatch the resize event
			}
		}
		return CallWindowProc(s_GlfwWndProc, hwnd, msg, wParam, lParam);
	}

	// -------------------------------------------------------------------------

	Window* Window::Create(const WindowProps& props)
	{
		return new WindowsWindow(props);
	}

	Blu::WindowsWindow::WindowsWindow(const WindowProps& props)
		: m_WindowProps(props)
	{
		BLU_PROFILE_FUNCTION();
		Init(props);
	}

	bool Blu::WindowsWindow::ShouldClose() const
	{
		return glfwWindowShouldClose(m_Window);
	}

	Blu::WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void Blu::WindowsWindow::OnUpdate()
	{
		BLU_PROFILE_FUNCTION();
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void Blu::WindowsWindow::Init(const WindowProps& props)
	{
		BLU_PROFILE_FUNCTION();
		m_Data.Title  = props.Title;
		m_Data.Width  = props.Width;
		m_Data.Height = props.Height;
		BLU_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			BLU_CORE_ASSERT(success, "Could not initialise GLFW!");
			s_GLFWInitialized = true;
		}

		// Hint for custom GLFW forks that expose GLFW_TITLEBAR.
		// Standard GLFW silently ignores unknown hints — safe to leave in.
		if (props.CustomTitleBar)
			glfwWindowHint(GLFW_TITLEBAR, false);

		// DX11 manages its own context — tell GLFW not to create an OpenGL one.
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height,
		                            m_Data.Title.c_str(), nullptr, nullptr);

		// Create and initialise the graphics context before subclassing so that
		// D3D11Context::Get() is valid when WM_SIZE first fires.
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			m_Context = new D3D11Context(m_Window);
		else
			m_Context = new OpenGLContext(m_Window);

		m_Context->Init();

		// ── Win32 custom title-bar ────────────────────────────────────────────
		// Strip the OS caption while keeping the thick-frame resize border, then
		// subclass the window proc so we can intercept WM_NCCALCSIZE (full client
		// area / maximise margins), WM_NCHITTEST (drag + resize), WM_NCACTIVATE
		// (suppress NC repaint), and WM_SIZE (D3D11 swapchain resize).
		if (props.CustomTitleBar)
		{
			HWND hwnd = glfwGetWin32Window(m_Window);

			LONG style = GetWindowLong(hwnd, GWL_STYLE);
			style &= ~WS_CAPTION;    // remove OS title bar / caption
			style |=  WS_THICKFRAME; // keep resize handles
			SetWindowLong(hwnd, GWL_STYLE, style);

			s_GlfwWndProc = reinterpret_cast<WNDPROC>(
				SetWindowLongPtr(hwnd, GWLP_WNDPROC,
				                 reinterpret_cast<LONG_PTR>(BlaWindowProc)));

			// SWP_FRAMECHANGED forces Windows to recalculate the client rect
			// immediately so the caption removal takes visible effect.
			SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		// ─────────────────────────────────────────────────────────────────────

		glfwSetWindowUserPointer(m_Window, &m_Data);

		glfwSetKeyCallback(m_Window,         GLFWCallbacks::KeyCallback);
		glfwSetMouseButtonCallback(m_Window,  GLFWCallbacks::MouseButtonCallback);
		glfwSetWindowSizeCallback(m_Window,   GLFWCallbacks::WindowSizeCallback);
		glfwSetCursorPosCallback(m_Window,    GLFWCallbacks::MouseMovedCallback);
		glfwSetScrollCallback(m_Window,       GLFWCallbacks::MouseButtonScrolledCallback);
		glfwSetCharCallback(m_Window,         GLFWCallbacks::CharCallback);
	}

	void Blu::WindowsWindow::Shutdown()
	{
		BLU_PROFILE_FUNCTION();
		glfwDestroyWindow(m_Window);
		delete m_Context;
	}
}
