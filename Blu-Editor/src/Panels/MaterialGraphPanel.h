#pragma once
#include "Blu/Rendering/MaterialGraph.h"
#include <utility>
#include <vector>

namespace Blu
{
	class MaterialGraphPanel
	{
	public:
		MaterialGraphPanel();
		void OnImGuiRender(bool* open);

	private:
		void ResetGraph();

		MaterialGraph m_Graph;
		AssetHandle m_TemplateHandle;
		Shared<MaterialTemplate> m_CompiledTemplate;
		std::vector<std::pair<MaterialGraphInput, MaterialGraphNodeID>> m_OutputNodes;
		std::vector<std::string> m_Diagnostics;
	};
}
