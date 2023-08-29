#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"
#include <filesystem>

class b2World;
namespace Blu
{
	class Entity;
	class Scene
	{
	public:
		Scene();
		~Scene();

		static Shared<Scene> Copy(Shared<Scene> scene);
		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		Entity GetPrimaryCameraEntity();
		Entity GetEntityByUUID(UUID id);
		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}
		
		Entity DuplicateEntity(Entity& targetEntity);
		const std::filesystem::path& GetSceneFilePath() const
		{
			return m_SceneFilePath;
		}
		void SetSceneFilePath(std::filesystem::path& filepath)
		{
			m_SceneFilePath = filepath;
		}

		void OnRuntimeStart();
		void OnPhysics2DStart();
		void OnRuntimeStop();

		void OnScriptSystemStart();
		void OnScriptSystemStop();
		void OnScriptSystemUpdate(Timestep deltaTime);
		void UpdateActiveCameraComponent(Timestep deltaTime);

		void DestroyEntity(Entity entity);
		void OnUpdateEditor(Timestep deltaTime, class EditorCamera& camera);
		void OnUpdateRuntime(Timestep deltaTime);
		void OnViewportResize(float width, float height);
	private:
		entt::registry m_Registry; // container for all of our entt components
		float m_ViewportWidth = 0.0f, m_ViewportHeight = 0.0f;
		std::unordered_map<UUID, entt::entity> m_EntityMap;
		std::filesystem::path m_SceneFilePath;
		b2World* m_PhysicsWorld = nullptr;
		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}