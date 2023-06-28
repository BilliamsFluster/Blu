#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"

namespace Blu
{
	class Scene
	{
	public:
		Scene();
		~Scene();
		entt::entity CreateEntity();
		void OnUpdate(Timestep deltaTime);
		entt::registry& Reg() { return m_Registry; }
	private:
		entt::registry m_Registry; // container for all of our entt components
	};
}