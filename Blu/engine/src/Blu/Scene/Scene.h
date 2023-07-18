#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"

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
		
		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		Entity DuplicateEntity(Entity& targetEntity);
		

		void OnRuntimeStart();
		void OnRuntimeStop();
		void UpdateActiveCameraComponent(Timestep deltaTime);

		void DestroyEntity(Entity entity);
		void OnUpdateEditor(Timestep deltaTime, class EditorCamera& camera);
		void OnUpdateRuntime(Timestep deltaTime);
		void OnViewportResize(float width, float height);
	private:
		entt::registry m_Registry; // container for all of our entt components
		float m_ViewportWidth = 0.0f, m_ViewportHeight = 0.0f;

		b2World* m_PhysicsWorld = nullptr;
		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}