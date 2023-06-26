#pragma once
#include "Blu/Core/Core.h"
#include "Layer.h"

#include <vector>

namespace Blu
{
	namespace Layers
	{

		class BLU_API LayerStack
		{
		public:

			LayerStack();
			~LayerStack();
			
			void PushLayer(Shared<Layer> layer);
			void PushOverlay(Shared<Layer> overlay);
			void PopLayer(Shared<Layer> layer);
			void PopOverlay(Shared<Layer> overlay);

			std::vector<Shared<Layer>>::iterator begin() { return m_Layers.begin(); }
			std::vector<Shared<Layer>>::iterator end() { return m_Layers.end(); }

			std::vector<Shared<Layer>>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
			std::vector<Shared<Layer>>::reverse_iterator rend() { return m_Layers.rend(); }

		private:
			std::vector<Shared<Layer>> m_Layers;
			unsigned int m_LayerInsertIndex = 0;

		};

	}
}

