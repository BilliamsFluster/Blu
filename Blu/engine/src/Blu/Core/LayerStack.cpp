#include "Blupch.h"
#include "LayerStack.h"


namespace Blu
{
	namespace Layers
	{
		LayerStack::LayerStack()
		{
		}
		LayerStack::~LayerStack()
		{
			
		}

		void LayerStack::PushLayer(Shared<Layer> layer)
		{
			m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
			m_LayerInsertIndex++;
		}

		void LayerStack::PushOverlay(Shared<Layer> overlay)
		{
			m_Layers.emplace_back(overlay);
		}

		void LayerStack::PopLayer(Shared<Layer> layer)
		{
			auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
			if (it != m_Layers.end())
			{
				m_Layers.erase(it);
				m_LayerInsertIndex--;
			}
		}

		void LayerStack::PopOverlay(Shared<Layer> overlay)
		{
			auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
			if (it != m_Layers.end())
			{
				m_Layers.erase(it);
			}
		}
	}
}

