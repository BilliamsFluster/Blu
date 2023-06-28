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
		void OnUpdate(Timestep deltaTime);
	private:
		entt::registry m_Registry; // container for all of our entt components

		friend class Entity;
	};
}