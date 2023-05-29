#pragma once
#include "Blu/Core/Window.h"
#include "Blupch.h"
#include "Blu/Rendering/GraphicsContext.h"

struct GLFWwindow;
namespace Blu
{
	class BLU_API WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		inline unsigned int GetWidth() const override { return m_Data.Width; }
		inline unsigned int GetHeight() const override { return m_Data.Height; }
		
		virtual void* GetNativeWindow() const override { return m_Window; }

		//Windows attributes
	private:
		virtual void Init(const WindowProps& props);
	public:
		virtual void Shutdown();
	public:
		 bool ShouldClose() const override;
	private:
		GLFWwindow* m_Window;
		GraphicsContext* m_Context;

		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
		};

		WindowData m_Data;

	};
}