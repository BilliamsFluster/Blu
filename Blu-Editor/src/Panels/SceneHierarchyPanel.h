#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"

namespace Blu
{
	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Shared<Scene>& scene);
		void SetContext(const Shared<Scene>& scene);
		Entity GetSelectedEntity() const { return m_SelectedEntity; }
		void SetSelectedEntity(Entity entity);
		// pShowOutliner / pShowDetails: optional bool* passed to ImGui::Begin so the
		// title-bar close button hides the panel (Window menu keeps them in sync).
		void OnImGuiRender(bool* pShowOutliner = nullptr, bool* pShowDetails = nullptr);
	private:
		void DrawEntityNode(Entity entity);
		void DrawEntityComponents(Entity entity);
	private:
		Shared<Scene> m_Context;
		Entity m_SelectedEntity;
		bool m_EntityHovered = false;
		char m_SearchBuffer[256] = {};
	};

}

