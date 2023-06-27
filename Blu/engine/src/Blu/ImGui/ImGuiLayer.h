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
			void OnAttach() override;
			void OnDetach() override;
			//void OnUpdate(Timestep deltaTime) override;
			void OnEvent(Events::Event& event);
			virtual void OnGuiDraw() override;
			void DrawDockspace();

			void UpdateWindowSize();

			void Begin();
			void End();
			void SetBlockEvents(bool block) { m_BlockEvents = block; }

		private:
			bool OnWindowResizedEvent(Events::WindowResizeEvent& event);
			bool OnKeyPressedEvent(Events::KeyPressedEvent& event);
			bool OnKeyReleasedEvent(Events::KeyReleasedEvent& event);
			bool OnKeyTypedEvent(Events::KeyTypedEvent& event);
			bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& event);
			bool OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event);
			bool OnMouseScrolledEvent(Events::MouseScrolledEvent& event);
			bool OnMouseMovedEvent(Events::MouseMovedEvent& event);
			
			float m_Time = 0.0f;
			bool m_BlockEvents = true;
		};
	}
}



