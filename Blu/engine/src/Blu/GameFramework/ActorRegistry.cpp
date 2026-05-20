#include "Blupch.h"
#include "ActorRegistry.h"
#include "AActor.h"

namespace Blu
{
	ActorRegistry& ActorRegistry::Get()
	{
		static ActorRegistry instance;
		return instance;
	}

	void ActorRegistry::Register(const std::string& name, FactoryFn fn)
	{
		m_Factories[name] = std::move(fn);
	}

	AActor* ActorRegistry::Instantiate(const std::string& name) const
	{
		auto it = m_Factories.find(name);
		return (it != m_Factories.end()) ? it->second() : nullptr;
	}

	bool ActorRegistry::Contains(const std::string& name) const
	{
		return m_Factories.count(name) > 0;
	}
}
