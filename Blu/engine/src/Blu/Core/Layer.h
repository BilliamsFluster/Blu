#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Core/Timestep.h"

namespace Blu
{
	namespace Layers
	{
		class BLU_API Layer
		{
		public:
			Layer(const std::string& name = "Layer");
			virtual ~Layer();
			virtual void OnAttach(){} // layer attached
			virtual void OnDetach() {} // layer detatched
			virtual void OnUpdate(Timestep deltaTime) {} // update layer
			virtual void OnEvent(Blu::Events::Event& event) {} // events ffor layers
			virtual void OnGuiDraw() {} // draw UI fofr the layer

			inline const std::string& GetName() const { return m_DebugName;}
		private:
			std::string m_DebugName;
		};
	}
}


