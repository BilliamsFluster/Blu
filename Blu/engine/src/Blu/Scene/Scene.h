#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"

namespace Blu
{
	class Entity;
	class Scene
	{
	public:
		Scene();
		~Scene();
		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);
		void OnUpdate(Timestep deltaTime);
		void OnViewportResize(float width, float height);
	private:
		entt::registry m_Registry; // container for all of our entt components
		float m_ViewportWidth = 0.0f, m_ViewportHeight = 0.0f;
		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}