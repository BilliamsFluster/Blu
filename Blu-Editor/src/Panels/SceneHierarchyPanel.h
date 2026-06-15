#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"
#include <functional>
#include <set>
#include <string>

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
		// Fired when the panel mutates the scene (create/delete/rename/add-component) so the
		// host can flag the scene dirty.
		void SetSceneModifiedCallback(std::function<void()> callback) { m_SceneModifiedCallback = std::move(callback); }
		void NotifySceneModified() { if (m_SceneModifiedCallback) m_SceneModifiedCallback(); }
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
		std::set<std::string> m_OutlinerFolders; // organisational folders (incl. empty ones this session)
		std::function<void(Entity)> m_OpenActorEditorCallback;
		std::function<void(Entity)> m_RequestDeleteCallback;
		std::function<void()>       m_SceneModifiedCallback;
		size_t m_LastEntityCount = (size_t)-1; // -1 = re-baseline (no dirty) on next frame / scene swap
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

