#pragma once
#include "Blu/Core/Layer.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Events/MouseEvent.h"
#include "Blu/Events/KeyEvent.h"
#include "Blu/Events/WindowEvent.h"
#include <functional>

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

			void SetDarkColors();
			void Begin();
			void End();

			// Optional status bar drawn in a reserved strip at the bottom of the dockspace host
			// (Unreal-style). The app supplies the content (e.g. the editor's Trace button + perf
			// readout); the dockspace reserves the space so panels never overlap it.
			void SetStatusBarCallback(std::function<void()> callback) { m_StatusBarCallback = std::move(callback); }

		private:


			float m_Time = 0.0f;
			bool m_BlockEvents = true;
			std::function<void()> m_StatusBarCallback;
		};
	}
}



