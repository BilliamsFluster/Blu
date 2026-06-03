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

	static constexpr int s_MinWindowWidth  = 640;
	static constexpr int s_MinWindowHeight = 360;

	static bool GetMonitorInfoForWindow(HWND hwnd, MONITORINFO& monitorInfo)
	{
		monitorInfo = {};
		monitorInfo.cbSize = sizeof(MONITORINFO);

		HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		return monitor && GetMonitorInfo(monitor, &monitorInfo);
	}

	static int ClampCoord(int value, int minValue, int maxValue)
	{
		if (maxValue < minValue)
			return minValue;
		return std::clamp(value, minValue, maxValue);
	}

	static void ClampWindowRectToNearestWorkArea(HWND hwnd)
	{
		if (!hwnd)
			return;

		MONITORINFO monitorInfo;
		if (!GetMonitorInfoForWindow(hwnd, monitorInfo))
			return;

		RECT rect{};
		if (!GetWindowRect(hwnd, &rect))
			return;

		const RECT& work = monitorInfo.rcWork;
		const int workWidth = work.right - work.left;
		const int workHeight = work.bottom - work.top;
		if (workWidth <= 0 || workHeight <= 0)
			return;

		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		width = std::min(std::max(width, 1), workWidth);
		height = std::min(std::max(height, 1), workHeight);

		const int x = ClampCoord(rect.left, work.left, work.right - width);
		const int y = ClampCoord(rect.top, work.top, work.bottom - height);

		if (x == rect.left && y == rect.top && width == rect.right - rect.left && height == rect.bottom - rect.top)
			return;

		SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	// -------------------------------------------------------------------------
	// Custom Win32 window procedure
	//
	// Intercepts five messages that the stock GLFW proc does not handle
	// correctly for a borderless-but-resizable window:
	//
	//  WM_NCCALCSIZE  – Returns 0 so Windows treats the entire window rect as
	//                   client area (no NC frame / caption drawn).  When the
	//                   window is maximised Windows may propose a rect outside
	//                   the usable work area; we clamp it so the ImGui titlebar
	//                   remains visible.
	//
	//  WM_GETMINMAXINFO - Makes maximise use the monitor work area instead of
	//                   raw monitor bounds.
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
						// Clamp maximized client content to the usable monitor area
						// so the custom titlebar never starts above the screen.
						MONITORINFO monitorInfo;
						if (GetMonitorInfoForWindow(hwnd, monitorInfo))
						{
							const RECT& work = monitorInfo.rcWork;
							r.left   = std::max(r.left,   work.left);
							r.top    = std::max(r.top,    work.top);
							r.right  = std::min(r.right,  work.right);
							r.bottom = std::min(r.bottom, work.bottom);
						}
					}
					return 0;   // entire window rect → client area
				}
				break;
			}

			case WM_GETMINMAXINFO:
			{
				auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
				MONITORINFO monitorInfo;
				if (GetMonitorInfoForWindow(hwnd, monitorInfo))
				{
					const RECT& monitor = monitorInfo.rcMonitor;
					const RECT& work = monitorInfo.rcWork;
					const int workWidth = work.right - work.left;
					const int workHeight = work.bottom - work.top;

					minMaxInfo->ptMaxPosition.x = work.left - monitor.left;
					minMaxInfo->ptMaxPosition.y = work.top - monitor.top;
					minMaxInfo->ptMaxSize.x = workWidth;
					minMaxInfo->ptMaxSize.y = workHeight;
					minMaxInfo->ptMaxTrackSize.x = workWidth;
					minMaxInfo->ptMaxTrackSize.y = workHeight;
				}
				return 0;
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

			case WM_CLOSE:
				Application::Get().Close();
				return 0;

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
		m_Data.Title = props.Title;

		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			BLU_CORE_ASSERT(success, "Could not initialise GLFW!");
			s_GLFWInitialized = true;
		}

		int initialWidth = static_cast<int>(props.Width);
		int initialHeight = static_cast<int>(props.Height);
		int initialX = 0;
		int initialY = 0;
		bool placeAtWorkAreaOrigin = false;

		if (props.CustomTitleBar)
		{
			if (GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor())
			{
				int workX = 0, workY = 0, workWidth = 0, workHeight = 0;
				glfwGetMonitorWorkarea(primaryMonitor, &workX, &workY, &workWidth, &workHeight);
				if (workWidth > 0 && workHeight > 0)
				{
					placeAtWorkAreaOrigin = initialWidth >= workWidth || initialHeight >= workHeight;
					initialWidth = std::min(std::max(initialWidth, s_MinWindowWidth), workWidth);
					initialHeight = std::min(std::max(initialHeight, s_MinWindowHeight), workHeight);
					initialX = workX;
					initialY = workY;
				}
			}
		}

		m_Data.Width = static_cast<unsigned int>(initialWidth);
		m_Data.Height = static_cast<unsigned int>(initialHeight);
		m_WindowProps.Width = m_Data.Width;
		m_WindowProps.Height = m_Data.Height;
		BLU_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, m_Data.Width, m_Data.Height);

		// Hint for custom GLFW forks that expose GLFW_TITLEBAR.
		// Standard GLFW silently ignores unknown hints — safe to leave in.
		if (props.CustomTitleBar)
			glfwWindowHint(GLFW_TITLEBAR, false);

		// DX11 manages its own context — tell GLFW not to create an OpenGL one.
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_Window = glfwCreateWindow(initialWidth, initialHeight,
		                            m_Data.Title.c_str(), nullptr, nullptr);
		if (placeAtWorkAreaOrigin)
			glfwSetWindowPos(m_Window, initialX, initialY);

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
			ClampWindowRectToNearestWorkArea(hwnd);
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

	void Blu::WindowsWindow::ClampToWorkArea()
	{
		if (!m_Window)
			return;

		ClampWindowRectToNearestWorkArea(glfwGetWin32Window(m_Window));
	}

	void Blu::WindowsWindow::Shutdown()
	{
		BLU_PROFILE_FUNCTION();
		glfwDestroyWindow(m_Window);
		delete m_Context;
	}
}
