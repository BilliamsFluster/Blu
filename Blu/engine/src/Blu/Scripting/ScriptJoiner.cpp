#include "Blupch.h"
#include "ScriptJoiner.h"
#include "mono/jit/jit.h"
#include <glm/glm.hpp>
#include "Blu/Scene/Scene.h"
#include "Blu/Scripting/ScriptEngine.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Core/Input.h"

#include "Blu/Physics/Physics2D.h"


#include "box2d/b2_body.h"


//the main file that binds all of the functions to be used in c#       
namespace Blu
{
	namespace Utils {

		std::string MonoStringToString(MonoString* string)
		{
			char* cStr = mono_string_to_utf8(string);
			std::string str(cStr);
			mono_free(cStr);
			return str;
		}

	}
	static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
#define BLU_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Blu.InternalCalls::" #Name, Name)
	static void Log(MonoString* string, int parameter)
	{
		char* cStr = mono_string_to_utf8(string);
		std::string str(cStr);
		mono_free(cStr);
		std::cout << str << "YOOOOO" << parameter << std::endl;
	}
	static void TransformComponent_GetTranslation(UUID entityID, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->GetEntityByUUID(entityID);
		if (entity)
		{
			*outTranslation = entity.GetComponent<TransformComponent>().Translation;
			
		}
		 
	}

	static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}", scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}",entity);

		
		MonoType* managedType = mono_reflection_type_get_type(componentType);
		BLU_CORE_ASSERT("{0}", s_EntityHasComponentFuncs.find(managedType) != s_EntityHasComponentFuncs.end());
		return s_EntityHasComponentFuncs.at(managedType)(entity);
	}

	static void TransformComponent_SetTranslation(UUID entityID, glm::vec3* translation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->GetEntityByUUID(entityID);
		std::cout << translation->x << std::endl;
		entity.GetComponent<TransformComponent>().Translation = *translation;
	}

	static void Rigidbody2DComponent_ApplyLinearImpulse(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}",scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}", entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(UUID entityID, glm::vec2* impulse, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}", scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}", entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	static void Rigidbody2DComponent_GetLinearVelocity(UUID entityID, glm::vec2* outLinearVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}", scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}", entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

	static Rigidbody2DComponent::BodyType Rigidbody2DComponent_GetType(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}", scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}", entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		return Utils::Rigidbody2DTypeFromBox2DBody(body->GetType());
	}

	static void Rigidbody2DComponent_SetType(UUID entityID, Rigidbody2DComponent::BodyType bodyType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		BLU_CORE_ASSERT("{0}", scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		BLU_CORE_ASSERT("{0}", entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->SetType(Utils::Rigidbody2DTypeToBox2DBody(bodyType));
	}
	static bool Input_IsKeyDown(int keycode)
	{
		return Input::IsKeyPressed(keycode);
	}

	void ScriptJoiner::RegisterFunctions()
	{
		BLU_ADD_INTERNAL_CALL(Log);
		BLU_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		BLU_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		BLU_ADD_INTERNAL_CALL(Input_IsKeyDown);
		BLU_ADD_INTERNAL_CALL(Entity_HasComponent);

		BLU_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse);
		BLU_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulseToCenter);
		BLU_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity);
		//BLU_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetType);
		//BLU_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetType);
	}

	template<typename... Component>
	static void RegisterComponent()
	{
		([]()
			{
				std::string_view typeName = typeid(Component).name();
				size_t pos = typeName.find_last_of(':');
				std::string_view structName = typeName.substr(pos + 1);
				std::string managedName = fmt::format("Blu.{}", structName);

				MonoType* managedType = mono_reflection_type_from_name(managedName.data(), ScriptEngine::GetCoreAssemblyImage());
				if (!managedType)
				{
					BLU_CORE_ERROR("Could not find component type {}", managedName);
					return;
				}
				s_EntityHasComponentFuncs[managedType] = [](Entity entity) {return entity.HasComponent<Component>(); };
			}(), ...);
		
	}

	template<typename... Component>
	static void RegisterComponent(Components<Component...>)
	{
		RegisterComponent<Component...>();
	}
	void ScriptJoiner::RegisterComponents()
	{
		RegisterComponent(AllComponents{});
		
	}
}