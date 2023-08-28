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
		bool IsTitleBarHovered() const { return m_TitleBarHovered; }
		void SetTitleBarHovered(bool hovered)  {m_TitleBarHovered = hovered; }
		WindowProps GetWindowProps() const { return m_WindowProps; }
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
		WindowProps m_WindowProps;
		bool m_TitleBarHovered = false;


		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
		};

		WindowData m_Data;

	};
}