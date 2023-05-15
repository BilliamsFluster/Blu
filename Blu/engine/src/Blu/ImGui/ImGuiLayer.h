#pragma once
#include "Blu/Core/Layer.h"

#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"

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
			float m_Time = 0.0f;
		};
	}
}



