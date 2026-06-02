#include "Blupch.h"
#include "NativeClassRegistry.h"
#include <algorithm>

namespace Blu
{
	NativeClassRegistry& NativeClassRegistry::Get()
	{
		static NativeClassRegistry instance;
		return instance;
	}

	void NativeClassRegistry::RegisterClass(NativeClassDescriptor descriptor)
	{
		for (auto alias = m_Aliases.begin(); alias != m_Aliases.end();)
		{
			if (alias->second == descriptor.ID)
				alias = m_Aliases.erase(alias);
			else
				++alias;
		}

		for (const std::string& alias : descriptor.Aliases)
			m_Aliases[alias] = descriptor.ID;
		m_Aliases[descriptor.ID] = descriptor.ID;
		m_Descriptors[descriptor.ID] = std::move(descriptor);
	}

	const NativeClassDescriptor* NativeClassRegistry::FindDescriptor(const NativeClassID& classID) const
	{
		const NativeClassID resolvedClassID = ResolveClassID(classID);
		auto descriptor = m_Descriptors.find(resolvedClassID);
		return descriptor != m_Descriptors.end() ? &descriptor->second : nullptr;
	}

	NativeClassID NativeClassRegistry::ResolveClassID(const NativeClassID& classIDOrAlias) const
	{
		auto alias = m_Aliases.find(classIDOrAlias);
		return alias != m_Aliases.end() ? alias->second : classIDOrAlias;
	}

	std::vector<const NativeClassDescriptor*> NativeClassRegistry::GetClasses(NativeClassKind kind) const
	{
		std::vector<const NativeClassDescriptor*> classes;
		for (const auto& [_, descriptor] : m_Descriptors)
		{
			if (descriptor.Kind == kind)
				classes.push_back(&descriptor);
		}
		std::sort(classes.begin(), classes.end(), [](const auto* left, const auto* right)
		{
			return left->DisplayName < right->DisplayName;
		});
		return classes;
	}

	Unique<UObject> NativeClassRegistry::CreateObject(const NativeClassID& classID) const
	{
		const NativeClassDescriptor* descriptor = FindDescriptor(classID);
		return descriptor && descriptor->Factory ? descriptor->Factory() : nullptr;
	}

	void NativeClassRegistry::Clear()
	{
		m_Descriptors.clear();
		m_Aliases.clear();
	}
}
