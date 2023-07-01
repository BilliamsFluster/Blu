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

		void OnImGuiRender();
	private:
		void DrawEntityNode(Entity entity);
		void DrawEntityComponents(Entity entity);
	private:
		Shared<Scene> m_Context;
		Entity m_SelectedEntity;
		bool m_EntityHovered = false;
	};

}

