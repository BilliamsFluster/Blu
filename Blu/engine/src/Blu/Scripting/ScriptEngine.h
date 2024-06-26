#pragma once
#include <filesystem>
#include <unordered_map>
#include "Blu/Core/Core.h"
#include <any>
extern "C"
{
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClassField MonoClassField;
}


namespace Blu
{
	class Scene;
	class Entity;
	class ScriptJoiner;
	class ScriptClass;
	class ScriptInstance;
	class ScriptEngine;
	class UUID;
	
	enum class ScriptFieldType
	{
		None = 0,
		Float, Double,
		Bool, Char, Byte, Short, Int, Long,
		UByte, UShort, UInt, ULong,
		Vector2, Vector3, Vector4,
		Entity
	};
	struct ScriptField
	{
		ScriptFieldType Type;
		std::string Name;
		MonoClassField* ClassField;


	};
	
	

	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& classNamespace, const std::string& className, std::unordered_map<std::string, ScriptField> fields, bool isCore = false);
		ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);
		MonoObject* Instantiate();
		MonoMethod* GetMethod(const std::string& name, int parameterCount);
		MonoObject* InvokeMethod(MonoMethod* method, void* obj, void** params, MonoObject** exc);
		const std::unordered_map<std::string, ScriptField>& GetScriptFields() const { return m_Fields; }
		
		std::any GetFieldData(const std::string& fieldName);
		void SetFieldData(const std::string& fieldName, const std::any& value);

	private:
		const std::string m_ClassNamespace;  
		const std::string m_ClassName;
		std::unordered_map<std::string, ScriptField> m_Fields; 


		MonoClass* m_MonoClass = nullptr;

		friend class ScriptEngine;
		friend class EditorScriptFieldAPI;
	};

	
	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();
		static void ReloadAssembly();
		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();
		static void OnCreateEntity(Entity* entity);
		static void OnUpdateEntity(Entity* entity, float deltaTime);
		static Scene* GetSceneContext();
		static bool EntityClassExists(const std::string& fullName);
		static bool RemoveEntityInstance(Entity* entity);


		static std::unordered_map<std::string, Shared<ScriptClass>> GetEntities();
		static MonoImage* GetCoreAssemblyImage();
		static Shared<ScriptClass> GetEntityScriptClass(const std::string& className);

		static void LoadAssembly(const std::filesystem::path& filepath);
		static void LoadAppAssembly(const std::filesystem::path& filepath);
		static Shared<ScriptInstance> GetEntityScriptInstance(UUID entityID);

	private:
		static void InitMono();
		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static void LoadAssemblyClasses();
		static void ShutdownMono();

		friend class ScriptClass;
		friend class ScriptJoiner;
	};
	
	
	class ScriptInstance
	{
	public:
		ScriptInstance(Shared<ScriptClass> scriptClass, Entity* entity);

		void InvokeOnCreate();
		void InvokeOnUpdate(float deltaTime);
		Shared<ScriptClass> GetScriptClass() { return m_ScriptClass; }

		template<typename T>
		T GetFieldValue(const std::string& name)
		{
			static_assert(sizeof(T) <= 8, "Type too large!");

			bool success =  GetFieldValueInternal(name,s_FieldValueBuffer);
			if (!success)
				return T();

			
			return *(T*)s_FieldValueBuffer;
		}
		template<typename T>
		void SetFieldValue(const std::string& name, T value)
		{
			//static_assert(sizeof(T) <= 8, "Type too large!");
 
			SetFieldValueInternal(name, &value);
			
		}

		MonoObject* GetInstance() { return m_Instance; }

	private:
		bool GetFieldValueInternal(const std::string& name, void* buffer);
		bool SetFieldValueInternal(const std::string& name, const void* value);

	private:
		Shared<ScriptClass> m_ScriptClass;
		MonoObject* m_Instance = nullptr;
		MonoMethod* m_Constructor = nullptr;
		MonoMethod* m_OnCreateMethod = nullptr;
		MonoMethod* m_OnUpdateMethod = nullptr;
		inline static char s_FieldValueBuffer[64]; // need to adjust for largest type
		
	};
	namespace Utils {

		inline const char* ScriptFieldTypeToString(ScriptFieldType fieldType)
		{
			switch (fieldType)
			{
			case ScriptFieldType::None:    return "None";
			case ScriptFieldType::Float:   return "Float";
			case ScriptFieldType::Double:  return "Double";
			case ScriptFieldType::Bool:    return "Bool";
			case ScriptFieldType::Char:    return "Char";
			case ScriptFieldType::Byte:    return "Byte";
			case ScriptFieldType::Short:   return "Short";
			case ScriptFieldType::Int:     return "Int";
			case ScriptFieldType::Long:    return "Long";
			case ScriptFieldType::UByte:   return "UByte";
			case ScriptFieldType::UShort:  return "UShort";
			case ScriptFieldType::UInt:    return "UInt";
			case ScriptFieldType::ULong:   return "ULong";
			case ScriptFieldType::Vector2: return "Vector2";
			case ScriptFieldType::Vector3: return "Vector3";
			case ScriptFieldType::Vector4: return "Vector4";
			case ScriptFieldType::Entity:  return "Entity";
			}
			BLU_CORE_ASSERT(false, "Unknown ScriptFieldType");
			return "None";
		}

		inline ScriptFieldType ScriptFieldTypeFromString(std::string_view fieldType)
		{
			if (fieldType == "None")    return ScriptFieldType::None;
			if (fieldType == "Float")   return ScriptFieldType::Float;
			if (fieldType == "Double")  return ScriptFieldType::Double;
			if (fieldType == "Bool")    return ScriptFieldType::Bool;
			if (fieldType == "Char")    return ScriptFieldType::Char;
			if (fieldType == "Byte")    return ScriptFieldType::Byte;
			if (fieldType == "Short")   return ScriptFieldType::Short;
			if (fieldType == "Int")     return ScriptFieldType::Int;
			if (fieldType == "Long")    return ScriptFieldType::Long;
			if (fieldType == "UByte")   return ScriptFieldType::UByte;
			if (fieldType == "UShort")  return ScriptFieldType::UShort;
			if (fieldType == "UInt")    return ScriptFieldType::UInt;
			if (fieldType == "ULong")   return ScriptFieldType::ULong;
			if (fieldType == "Vector2") return ScriptFieldType::Vector2;
			if (fieldType == "Vector3") return ScriptFieldType::Vector3;
			if (fieldType == "Vector4") return ScriptFieldType::Vector4;
			if (fieldType == "Entity")  return ScriptFieldType::Entity;

			return ScriptFieldType::None;
		} 

	}

}

