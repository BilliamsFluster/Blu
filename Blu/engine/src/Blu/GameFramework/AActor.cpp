#include "Blupch.h"
#include "AActor.h"
#include "Blu/Scene/Component.h"

namespace Blu
{
	TransformComponent& AActor::GetTransform()
	{
		return m_Entity.GetComponent<TransformComponent>();
	}
}
