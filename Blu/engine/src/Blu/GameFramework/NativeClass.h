#pragma once
#include "UObject.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace Blu
{
	using NativeClassID = std::string;
	using ActorClassID = NativeClassID;

	struct NativeAssetReference
	{
		std::string Path;
	};

	struct NativeEntityReference
	{
		uint64_t UUID = 0;
	};

	using NativePropertyValue = std::variant<
		bool,
		int64_t,
		float,
		std::string,
		glm::vec2,
		glm::vec3,
		glm::vec4,
		NativeAssetReference,
		NativeEntityReference>;

	using PropertyOverrideMap = std::unordered_map<std::string, NativePropertyValue>;

	enum class NativePropertyType
	{
		Bool,
		Integer,
		Float,
		String,
		Vec2,
		Vec3,
		Vec4,
		AssetReference,
		EntityReference
	};

	template<typename T>
	struct NativePropertyTypeOf;

	template<> struct NativePropertyTypeOf<bool> { static constexpr NativePropertyType Value = NativePropertyType::Bool; };
	template<> struct NativePropertyTypeOf<int64_t> { static constexpr NativePropertyType Value = NativePropertyType::Integer; };
	template<> struct NativePropertyTypeOf<float> { static constexpr NativePropertyType Value = NativePropertyType::Float; };
	template<> struct NativePropertyTypeOf<std::string> { static constexpr NativePropertyType Value = NativePropertyType::String; };
	template<> struct NativePropertyTypeOf<glm::vec2> { static constexpr NativePropertyType Value = NativePropertyType::Vec2; };
	template<> struct NativePropertyTypeOf<glm::vec3> { static constexpr NativePropertyType Value = NativePropertyType::Vec3; };
	template<> struct NativePropertyTypeOf<glm::vec4> { static constexpr NativePropertyType Value = NativePropertyType::Vec4; };
	template<> struct NativePropertyTypeOf<NativeAssetReference> { static constexpr NativePropertyType Value = NativePropertyType::AssetReference; };
	template<> struct NativePropertyTypeOf<NativeEntityReference> { static constexpr NativePropertyType Value = NativePropertyType::EntityReference; };

	struct NativePropertyDescriptor
	{
		std::string Name;
		NativePropertyType Type = NativePropertyType::String;
		NativePropertyValue DefaultValue = std::string();
		std::function<bool(UObject&, const NativePropertyValue&)> Apply;
	};

	template<typename TObject, typename TValue>
	NativePropertyDescriptor MakeNativeProperty(const std::string& name, TValue TObject::* member)
	{
		static_assert(std::is_base_of_v<UObject, TObject>);
		TObject defaults;
		return {
			name,
			NativePropertyTypeOf<TValue>::Value,
			defaults.*member,
			[member](UObject& object, const NativePropertyValue& value)
			{
				auto* typedObject = dynamic_cast<TObject*>(&object);
				const auto* typedValue = std::get_if<TValue>(&value);
				if (!typedObject || !typedValue)
					return false;
				typedObject->*member = *typedValue;
				return true;
			}
		};
	}
}
