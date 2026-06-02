#pragma once
#include "Blu/Core/Core.h"
#include "AActor.h"
#include "AGameMode.h"
#include "NativeClass.h"
#include <initializer_list>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Blu
{
	enum class NativeClassKind
	{
		Actor,
		GameMode
	};

	struct NativeClassDescriptor
	{
		NativeClassID ID;
		std::string DisplayName;
		std::string Category;
		NativeClassKind Kind = NativeClassKind::Actor;
		std::function<Unique<UObject>()> Factory;
		std::vector<std::string> Aliases;
		std::vector<NativePropertyDescriptor> Properties;
	};

	class NativeClassRegistry
	{
	public:
		static NativeClassRegistry& Get();

		void RegisterClass(NativeClassDescriptor descriptor);
		const NativeClassDescriptor* FindDescriptor(const NativeClassID& classID) const;
		NativeClassID ResolveClassID(const NativeClassID& classIDOrAlias) const;
		std::vector<const NativeClassDescriptor*> GetClasses(NativeClassKind kind) const;
		Unique<UObject> CreateObject(const NativeClassID& classID) const;
		void Clear();

		template<typename T>
		Unique<T> Create(const NativeClassID& classID) const
		{
			Unique<UObject> object = CreateObject(classID);
			if (!object)
				return {};

			T* typedObject = dynamic_cast<T*>(object.get());
			if (!typedObject)
				return {};

			object.release();
			return Unique<T>(typedObject);
		}

		template<typename T>
		void RegisterActor(
			const NativeClassID& classID,
			const std::string& displayName,
			const std::string& category,
			std::initializer_list<std::string> aliases = {},
			std::initializer_list<NativePropertyDescriptor> properties = {})
		{
			static_assert(std::is_base_of_v<AActor, T>);
			RegisterClass({
				classID,
				displayName,
				category,
				NativeClassKind::Actor,
				[]() -> Unique<UObject> { return std::make_unique<T>(); },
				std::vector<std::string>(aliases),
				std::vector<NativePropertyDescriptor>(properties)
			});
		}

		template<typename T>
		void RegisterGameMode(
			const NativeClassID& classID,
			const std::string& displayName,
			const std::string& category,
			std::initializer_list<std::string> aliases = {},
			std::initializer_list<NativePropertyDescriptor> properties = {})
		{
			static_assert(std::is_base_of_v<AGameMode, T>);
			RegisterClass({
				classID,
				displayName,
				category,
				NativeClassKind::GameMode,
				[]() -> Unique<UObject> { return std::make_unique<T>(); },
				std::vector<std::string>(aliases),
				std::vector<NativePropertyDescriptor>(properties)
			});
		}

	private:
		std::unordered_map<NativeClassID, NativeClassDescriptor> m_Descriptors;
		std::unordered_map<std::string, NativeClassID> m_Aliases;
	};
}
