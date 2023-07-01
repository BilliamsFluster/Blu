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
			void OnEvent(Events::Event& event) {}
			virtual void OnGuiDraw() override;
			void DrawDockspace();

			void UpdateWindowSize();
			void SetDarkColors();
			void Begin();
			void End();

		private:
			
			
			float m_Time = 0.0f;
			bool m_BlockEvents = true;
		};
	}
}



