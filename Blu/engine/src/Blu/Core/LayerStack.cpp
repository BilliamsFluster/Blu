#include "Blupch.h"
#include "LayerStack.h"


namespace Blu
{
	namespace Layers
	{
		// LayerStack constructor
		LayerStack::LayerStack()
		{
		}

		// LayerStack destructor
		LayerStack::~LayerStack()
		{

		}

		// Adds a layer to the layer stack at the current layer insert index
		// Layers are inserted before overlays
		void LayerStack::PushLayer(Shared<Layer> layer)
		{
			// Inserts the layer at the current insert index and increments the insert index
			m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
			m_LayerInsertIndex++;
		}

		// Adds an overlay to the layer stack
		// Overlays are inserted at the end of the stack (after all layers)
		void LayerStack::PushOverlay(Shared<Layer> overlay)
		{
			// Inserts the overlay at the end of the layer vector
			m_Layers.emplace_back(overlay);
		}

		// Removes a specific layer from the layer stack
		void LayerStack::PopLayer(Shared<Layer> layer)
		{
			// Finds the layer in the layer vector
			auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
			// If the layer is found, it is removed from the vector and the layer insert index is decremented
			if (it != m_Layers.end())
			{
				m_Layers.erase(it);
				m_LayerInsertIndex--;
			}
		}

		// Removes a specific overlay from the layer stack
		void LayerStack::PopOverlay(Shared<Layer> overlay)
		{
			// Finds the overlay in the layer vector
			auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
			// If the overlay is found, it is removed from the vector
			if (it != m_Layers.end())
			{
				m_Layers.erase(it);
			}
		}
	}
}

