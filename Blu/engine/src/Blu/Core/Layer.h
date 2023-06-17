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
			virtual void OnAttach(){}
			virtual void OnDetatch() {}
			virtual void OnUpdate(Timestep deltaTime) {}
			virtual void OnEvent(Blu::Events::Event& event) {}

			inline const std::string& GetName() const { return m_DebugName;}
		private:
			std::string m_DebugName;
		};
	}
}


