#pragma once
#include "Entity.h"
namespace Blu
{
	class ScriptableEntity  
	{
	public:
		virtual ~ScriptableEntity() = default;
		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}
	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate(Timestep deltaTime) {}

	private:
		Entity m_Entity;
		friend class Scene;
	};

}

