#include "Blupch.h"
#include "ScriptJoiner.h"
#include "mono/jit/jit.h"
#include <glm/glm.hpp>
#include "Blu/Scene/Scene.h"
#include "Blu/Scripting/ScriptEngine.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Core/Input.h"


//the main file that binds all of the functions to be used in c#       
namespace Blu
{
#define BLU_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Blu.InternalCalls::" #Name, Name)
	static void Log(MonoString* string, int parameter)
	{
		char* cStr = mono_string_to_utf8(string);
		std::string str(cStr);
		mono_free(cStr);
		std::cout << str << "YOOOOO" << parameter << std::endl;
	}
	static void Entity_GetTranslation(UUID entityID, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->GetEntityByUUID(entityID);
		if (entity)
		{
			*outTranslation = entity.GetComponent<TransformComponent>().Translation;
			
		}
		 
	}

	static void Entity_SetTranslation(UUID entityID, glm::vec3* translation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();

		Entity entity = scene->GetEntityByUUID(entityID);
		entity.GetComponent<TransformComponent>().Translation = *translation;
	}

	static bool Input_IsKeyDown(int keycode)
	{
		return false;
		//return Input::IsKeyPressed(keycode);
	}

	void ScriptJoiner::RegisterFunctions()
	{
		BLU_ADD_INTERNAL_CALL(Log);
		BLU_ADD_INTERNAL_CALL(Entity_GetTranslation);
		BLU_ADD_INTERNAL_CALL(Entity_SetTranslation);
		BLU_ADD_INTERNAL_CALL(Input_IsKeyDown);
	}
}