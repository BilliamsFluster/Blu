#pragma once
#include <unordered_map>
#include <functional>
#include <string>

namespace Blu
{
	class AActor;

	class ActorRegistry
	{
	public:
		using FactoryFn = std::function<AActor*()>;

		static ActorRegistry& Get();
		void    Register(const std::string& name, FactoryFn fn);
		AActor* Instantiate(const std::string& name) const;
		bool    Contains(const std::string& name) const;
		const std::unordered_map<std::string, FactoryFn>& GetAll() const { return m_Factories; }

	private:
		std::unordered_map<std::string, FactoryFn> m_Factories;
	};

	struct ActorAutoRegister
	{
		ActorAutoRegister(const std::string& name, ActorRegistry::FactoryFn fn)
		{
			ActorRegistry::Get().Register(name, fn);
		}
	};
}

// Place in the .cpp of any AActor subclass to make it selectable from the editor.
// EditorName  — the string shown in the inspector dropdown (no namespaces, no colons).
// FullType    — the fully-qualified C++ type (e.g. Azure::PlayerCharacter).
// Usage:  BLU_REGISTER_ACTOR(PlayerCharacter, Azure::PlayerCharacter);
#define BLU_REGISTER_ACTOR(EditorName, FullType) \
	static Blu::ActorAutoRegister _actor_auto_reg_##EditorName(#EditorName, []() -> Blu::AActor* { return new FullType(); })
