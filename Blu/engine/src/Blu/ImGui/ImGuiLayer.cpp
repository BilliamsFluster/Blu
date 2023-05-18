#include "Blupch.h"
#include "ImGuiLayer.h"
#include "imgui.h"
#include "Blu/Core/Log.h"
#include "Blu/Platform/OpenGL/ImGuiOpenGLRenderer.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"
#include "Blu/Events/EventDispatcher.h"

//Temporary

#include <GLFW/glfw3.h>
#include <glad/glad.h>

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
			ImGui::CreateContext();
			ImGui::StyleColorsDark();

			ImGuiIO& io = ImGui::GetIO();
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
			io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

			ImGui_ImplOpenGL3_Init("#version 410");

		}
		void ImGuiLayer::OnDetatch()
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui::DestroyContext();
		}
		void ImGuiLayer::OnUpdate()
		{
			//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			//glClear(GL_COLOR_BUFFER_BIT);

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
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui::NewFrame();

			
			RenderGui();
			ImGui::ShowDemoWindow();

				
			

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}
		void ImGuiLayer::OnEvent(Events::EventHandler& handler, Events::Event& event)
		{
			ImGuiIO& io = ImGui::GetIO();

			switch (event.GetType())
			{
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
			case Events::Event::Type::MouseMoved:
			{
				Events::MouseMovedEvent& e = static_cast<Events::MouseMovedEvent&>(event);
				OnMouseMovedEvent(e);
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
			case Events::Event::Type::WindowResize:
			{
				Events::WindowResizeEvent& e = static_cast<Events::WindowResizeEvent&>(event);
				OnWindowResizedEvent(e);
				
				break;
			}
			case Events::Event::Type::KeyTyped:
			{
				Events::KeyTypedEvent& e = static_cast<Events::KeyTypedEvent&>(event);
				OnKeyTypedEvent(e);

				break;
			}
			}

			// Indicate that the event has been handled if ImGui wants to capture the mouse or keyboard
			event.Handled = io.WantCaptureMouse || io.WantCaptureKeyboard;
		}
		bool ImGuiLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[event.GetButton()] = true;
			return false;



		}
		bool ImGuiLayer::OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[event.GetButton()] = false;
			return false;


		}
		bool ImGuiLayer::OnMouseScrolledEvent(Events::MouseScrolledEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseWheel = event.GetYOffset();
			io.MouseWheelH = event.GetXOffset();
			return false;

		}
		bool ImGuiLayer::OnMouseMovedEvent(Events::MouseMovedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MousePos = ImVec2(event.GetX(), event.GetY());
			return false;
		}

		bool ImGuiLayer::OnKeyPressedEvent(Events::KeyPressedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.KeysDown[event.GetKeyCode()] = true;
			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
			return false;

		}

		bool ImGuiLayer::OnKeyReleasedEvent(Events::KeyReleasedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.KeysDown[event.GetKeyCode()] = false;
			return false;
		}

		bool ImGuiLayer::OnKeyTypedEvent(Events::KeyTypedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			int KeyCode = event.GetKeyCode();
			if (KeyCode > 0 && KeyCode < 0x10000)
			{
				io.AddInputCharacter((unsigned short)KeyCode);
			}
			return false;

		}

		bool ImGuiLayer::OnWindowResizedEvent(Events::WindowResizeEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			//GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			
			// Update the display size
			io.DisplaySize = ImVec2(event.GetWidth(), event.GetHeight());
			
			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // Assuming no scale here
			
			// Update the viewport
			glViewport(0, 0, event.GetWidth(), event.GetHeight());

			return false;
		}
		void ImGuiLayer::RenderGui()
		{
			
			
			// Get the current display size
			ImVec2 displaySize = ImGui::GetIO().DisplaySize;

			// Window 1: Main Menu
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(displaySize.x, 20));
			ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
			if (ImGui::BeginMenu("File"))
			{
				// File menu options
				ImGui::MenuItem("New");
				ImGui::MenuItem("Open");
				ImGui::MenuItem("Save");
				ImGui::EndMenu();
			}
			// Add more menu items as needed
			ImGui::End();

			// Window 2: Toolbar
			float toolbarHeight = 80.0f;
			ImGui::SetNextWindowPos(ImVec2(0, 20));
			ImGui::SetNextWindowSize(ImVec2(displaySize.x, toolbarHeight));
			ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoResize);
			// Add toolbar buttons, icons, etc.
			ImGui::Text("Toolbar content");
			ImGui::End();

			// Window 3: Viewport
			float viewportHeight = displaySize.y - toolbarHeight - 200;
			ImGui::SetNextWindowPos(ImVec2(0, toolbarHeight));
			ImGui::SetNextWindowSize(ImVec2(displaySize.x - 200, viewportHeight));
			ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoDecoration);
			// Render your 3D scene or image here
			ImGui::Text("Viewport content");
			ImGui::End();

			// Window 4: Outliner
			float outlinerWidth = 200.0f;
			float outlinerHeight = displaySize.y - toolbarHeight;
			ImGui::SetNextWindowPos(ImVec2(displaySize.x - outlinerWidth, toolbarHeight));
			ImGui::SetNextWindowSize(ImVec2(outlinerWidth, outlinerHeight));
			ImGui::Begin("Outliner");
			// Show hierarchical view of objects, scene graph, etc.
			ImGui::Text("Outliner content");
			ImGui::End();

			// Window 5: Content Browser
			float contentBrowserHeight = 200.0f;
			ImGui::SetNextWindowPos(ImVec2(0, displaySize.y - contentBrowserHeight));
			ImGui::SetNextWindowSize(ImVec2(displaySize.x - outlinerWidth, contentBrowserHeight));
			ImGui::Begin("Content Browser");
			// Show file/folder tree view, assets, etc.
			ImGui::Text("Content Browser content");
			ImGui::End();


			static ImVec4 clearColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);  // Default clear color (dark gray)
			static bool showColorPicker = false;

			// Window 6: File
			ImGui::SetNextWindowPos(ImVec2(displaySize.x - outlinerWidth, toolbarHeight));
			ImGui::SetNextWindowSize(ImVec2(outlinerWidth, outlinerHeight));
			ImGui::Begin("File");

			// ... Previous file window content ...

			if (ImGui::Button("Change Window Color"))
			{
				// Toggle the flag to show/hide the color picker
				showColorPicker = !showColorPicker;
			}

			// Option to change window color
			if (showColorPicker)
			{
				if (ImGui::ColorEdit3("Color", (float*)&clearColor))
				{
					// Set the clear color for the window
					glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
				}
			}

			ImGui::End();

			// Clear the buffers with the selected color
			glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
			glClear(GL_COLOR_BUFFER_BIT);

		}
	}
}
