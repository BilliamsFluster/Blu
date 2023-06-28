#include "Blupch.h"
#include "Entity.h"

namespace Blu
{
	Entity::Entity(entt::entity handle, Scene* scene)
		:m_EntityHandle(handle), m_Scene(scene)
	{

	}
}