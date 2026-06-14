#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"
#include <functional>

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
		void SetOpenActorEditorCallback(std::function<void(Entity)> callback) { m_OpenActorEditorCallback = std::move(callback); }
		// When set, the context-menu "Delete" routes the request to the host (for a
		// confirmation modal) instead of destroying the entity immediately.
		void SetRequestDeleteCallback(std::function<void(Entity)> callback) { m_RequestDeleteCallback = std::move(callback); }
		// pShowOutliner / pShowDetails: optional bool* passed to ImGui::Begin so the
		// title-bar close button hides the panel (Window menu keeps them in sync).
		void OnImGuiRender(bool* pShowOutliner = nullptr, bool* pShowDetails = nullptr);
		void DrawEntityComponents(Entity entity);
	private:
		void DrawEntityNode(Entity entity);
		bool PassesActiveFilters(Entity entity) const;
	private:
		Shared<Scene> m_Context;
		Entity m_SelectedEntity;
		Entity m_RenamingEntity;          // entity being inline-renamed in the Outliner
		bool   m_RenameRequestFocus = false; // focus the rename field on its first frame
		std::function<void(Entity)> m_OpenActorEditorCallback;
		std::function<void(Entity)> m_RequestDeleteCallback;
		bool m_EntityHovered = false;
		char m_SearchBuffer[256] = {};
		bool m_FilterCameras = true;
		bool m_FilterLights = true;
		bool m_FilterMeshes = true;
		bool m_FilterPhysics = true;
		bool m_FilterScripts = true;
		bool m_FilterCharacters = true;
		bool m_FilterOther = true;
	};

}

