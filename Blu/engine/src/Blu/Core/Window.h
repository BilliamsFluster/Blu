#pragma once
#include "Blupch.h"
#include "Blu/Core/Core.h"
#include "Blu/Events/Event.h"

namespace Blu
{
	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;

		WindowProps(const std::string& title = "Blu Engine",
			uint32_t width = 1600,
			uint32_t height = 900)
			: Title(title), Width(width), Height(height)
		{
		}
	};
	class BLU_API Window
	{
	public:
		virtual unsigned int GetWidth() const = 0;
		virtual unsigned int GetHeight() const = 0;

		virtual void OnUpdate() = 0;
		virtual void* GetNativeWindow() const = 0;
		
		
		virtual ~Window() {}
	public:

		static Window* Create(const WindowProps& props = WindowProps());
		virtual bool ShouldClose() const = 0;

	};
	
}