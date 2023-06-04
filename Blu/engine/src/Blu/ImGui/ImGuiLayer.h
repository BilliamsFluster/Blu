#pragma once
#include "Blu/Core/Layer.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Events/MouseEvent.h"
#include "Blu/Events/KeyEvent.h"
#include "Blu/Events/WindowEvent.h"

namespace Blu
{
	namespace Layers
	{
		class BLU_API ImGuiLayer : public Layer
		{
		public:

			ImGuiLayer();
			~ImGuiLayer();
			void OnAttach();
			void OnDetatch();
			void OnUpdate();
			void OnEvent(Events::EventHandler& handler, Events::Event& event);

		private:
		private:
			bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& event);
			bool OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event);
			bool OnMouseScrolledEvent(Events::MouseScrolledEvent& event);
			bool OnMouseMovedEvent(Events::MouseMovedEvent& event);
			bool OnKeyPressedEvent(Events::KeyPressedEvent& event);
			bool OnKeyReleasedEvent(Events::KeyReleasedEvent& event);
			bool OnKeyTypedEvent(Events::KeyTypedEvent& event);
			bool OnWindowResizedEvent(Events::WindowResizeEvent& event);

			void RenderGui();
			float m_Time = 0.0f;
		};
	}
}


