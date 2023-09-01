#include "Blupch.h"
#include "ScriptEngine.h"
#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "ScriptJoiner.h"
#include <string>
#include <format>
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Component.h"
#include "mono/metadata/tabledefs.h"


namespace Blu
{
	static std::unordered_map< std::string, ScriptFieldType> s_ScriptFieldTypeMap =
	{ 
		{ "System.Single", ScriptFieldType::Float },
		{ "System.Double", ScriptFieldType::Double },
		{ "System.Boolean", ScriptFieldType::Bool },
		{ "System.Char", ScriptFieldType::Char },
		{ "System.Int16", ScriptFieldType::Short },
		{ "System.Int32", ScriptFieldType::Int },
		{ "System.Int64", ScriptFieldType::Long },
		{ "System.Byte", ScriptFieldType::Byte },
		{ "System.UInt16", ScriptFieldType::UShort },
		{ "System.UInt32", ScriptFieldType::UInt },
		{ "System.UInt64", ScriptFieldType::ULong },

		{ "Hazel.Vector2", ScriptFieldType::Vector2 },
		{ "Hazel.Vector3", ScriptFieldType::Vector3 },
		{ "Hazel.Vector4", ScriptFieldType::Vector4 },

		{ "Blu.Entity", ScriptFieldType::Entity },
	};
	namespace Utils
	{
		static char* ReadBytes(const std::filesystem::path& filepath, uint32_t* outSize)
		{
			std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

			if (!stream)
			{
				return nullptr;
			}

			std::streampos end = stream.tellg();
			stream.seekg(0, std::ios::beg);
			uint32_t size = end - stream.tellg();

			if (size == 0)
			{
				return nullptr;
			}

			char* buffer = new char[size];
			stream.read((char*)buffer, size);

			*outSize = size;
			return buffer;
		}
		ScriptFieldType MonoTypeToScriptFieldType(MonoType* type)
		{
			std::string typeName = mono_type_get_name(type);
			return s_ScriptFieldTypeMap.at(typeName);

		}
		MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
		{
			uint32_t fileSize = 0;
			char* fileData = ReadBytes(assemblyPath, &fileSize);

			MonoImageOpenStatus status;
			MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* errorMessage = mono_image_strerror(status);

				return nullptr;
			}
			std::string path = assemblyPath.string();
			MonoAssembly* assembly = mono_assembly_load_from_full(image, path.c_str(), &status, 0);
			mono_image_close(image);
			delete[] fileData;

			return assembly;
		}
		
	}

		
	
	

	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		MonoAssembly* AppAssembly = nullptr;
		MonoImage* AppAssemblyImage = nullptr;

		Shared<ScriptClass> EntityClass = nullptr;
		Scene* SceneContext = nullptr;

		std::unordered_map<std::string, Shared<ScriptClass>> Entities;
		std::unordered_map<UUID, Shared<ScriptInstance>> EntityInstances;

	}; 
	
	static ScriptEngineData* s_Data;

	




	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, std::unordered_map<std::string, ScriptField> fields, bool isCore)
		:m_ClassNamespace(classNamespace), m_ClassName(className), m_Fields(fields)
	{
		m_MonoClass = mono_class_from_name(isCore ? s_Data->CoreAssemblyImage :s_Data->AppAssemblyImage, classNamespace.c_str(), className.c_str());

	}
	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
		:m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		m_MonoClass = mono_class_from_name(isCore ? s_Data->CoreAssemblyImage : s_Data->AppAssemblyImage, classNamespace.c_str(), className.c_str());
		
	}
	


	MonoObject* ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(m_MonoClass);

	}

	

	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		MonoMethod* method = mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
		return method;

	}

	MonoObject* ScriptClass::InvokeMethod(MonoMethod* method, void* obj, void** params, MonoObject** exc)
	{
		return mono_runtime_invoke(method, obj, params, exc);

	}

	std::any ScriptClass::GetFieldData(const std::string& fieldName)
	{
		return std::any();
	}

	void ScriptClass::SetFieldData(const std::string& fieldName, const std::any& value)
	{
	}

	void ScriptEngine::Init()
	{
		s_Data = new ScriptEngineData();
		InitMono();
		ScriptJoiner::RegisterComponents();
	}

	void PrintAssemblyTypes(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* typeDefinitionTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionTable);

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionTable, i, cols, MONO_TYPEDEF_SIZE);


			const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
			
			printf("%s.%s \n", nameSpace, name);
			
		}
	}


	void ScriptEngine::LoadAssemblyClasses()
	{
		s_Data->Entities.clear();
		const MonoTableInfo* typeDefinitionTable = mono_image_get_table_info(s_Data->AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionTable);
		MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Blu", "Entity");

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionTable, i, cols, MONO_TYPEDEF_SIZE);


			const char* nameSpace = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);

			std::string fullName;
			if (strlen(nameSpace) != 0)
			{
				fullName = std::format("{}.{}", nameSpace, name);
			}
			else
			{
				fullName = name;
			}
			std::cout << nameSpace << std::endl;
			MonoClass* monoClass = mono_class_from_name(s_Data->AppAssemblyImage, nameSpace, name);

			if (monoClass == entityClass)
				continue;

			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);

			if (!isEntity)
				continue;
			Shared<ScriptClass> scriptClass = std::make_shared<ScriptClass>(nameSpace, name);
			s_Data->Entities[fullName] = scriptClass;
			
			
			int count = mono_class_num_fields(monoClass);
			BLU_CORE_WARN("{} fields:", count);
			void* iterator = nullptr;
			
			while (MonoClassField* field = mono_class_get_fields(monoClass, &iterator))
			{
				const char* fieldName = mono_field_get_name(field);
				uint32_t flags = mono_field_get_flags(field);
				if (flags & FIELD_ATTRIBUTE_PUBLIC)
				{
					MonoType* type = mono_field_get_type(field);
					ScriptFieldType fieldType = Utils::MonoTypeToScriptFieldType(type);
					Utils::ScriptFieldTypeToString(fieldType);
					BLU_CORE_WARN("{}, {}:", fieldName, Utils::ScriptFieldTypeToString(fieldType));

					scriptClass->m_Fields[fieldName] = { fieldType, fieldName, field }; // -- The more attributes you add to the script field make sure you update this 

				}
			}
			

			printf("%s.%s \n", nameSpace, name);

		}

	}

	void ScriptEngine::Shutdown()
	{
		ShutdownMono();
		delete s_Data;

	}
	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		s_Data->SceneContext = scene;
	}
	void ScriptEngine::OnRuntimeStop()
	{
		s_Data->SceneContext = nullptr;
		s_Data->EntityInstances.clear();
	}
	void ScriptEngine::OnCreateEntity(Entity* entity)
	{
		const auto& sc = entity->GetComponent<ScriptComponent>();
		if (EntityClassExists(sc.Name))
		{
			Shared<ScriptInstance> instance = std::make_shared<ScriptInstance>(s_Data->Entities[sc.Name], entity);
			s_Data->EntityInstances[entity->GetUUID()] = instance;
			instance->InvokeOnCreate();
		}
	}
	void ScriptEngine::OnUpdateEntity(Entity* entity, float deltaTime)
	{
		UUID entityUUID = entity->GetUUID();
		Shared<ScriptInstance> instance = s_Data->EntityInstances[entityUUID];
		if(instance)
			instance->InvokeOnUpdate(deltaTime);
		
	}
	Scene* ScriptEngine::GetSceneContext()
	{
		return s_Data->SceneContext;
	}
	bool ScriptEngine::EntityClassExists(const std::string& fullName)
	{
		return s_Data->Entities.find(fullName) != s_Data->Entities.end();
	}
	bool ScriptEngine::RemoveEntityInstance(Entity* entity)
	{
		if (entity)
		{
			UUID entityUUID = entity->GetUUID();
			if (entityUUID)
			{
				if (s_Data->EntityInstances[entityUUID])
				{
					s_Data->EntityInstances[entityUUID].reset();
					return true;
				}

			}

		}
		return false;
	}
	std::unordered_map<std::string, Shared<ScriptClass>> ScriptEngine::GetEntities()
	{
		return s_Data->Entities;
	}
	void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		//Create App Domain
		s_Data->AppDomain = mono_domain_create_appdomain(const_cast<char*>("BluScriptRuntime"), nullptr);
		mono_domain_set(s_Data->AppDomain, true);


		s_Data->CoreAssembly = Utils::LoadMonoAssembly(filepath);
		//PrintAssemblyTypes(s_Data->CoreAssembly); -- for debug
		s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
		
	}

	void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		s_Data->AppAssembly = Utils::LoadMonoAssembly(filepath);
		//PrintAssemblyTypes(s_Data->CoreAssembly); -- for debug
		s_Data->AppAssemblyImage = mono_assembly_get_image(s_Data->AppAssembly);
	}
	
	void ScriptEngine::InitMono()
	{
		mono_set_assemblies_path("mono/lib"); // this is done first, you need to provide a path for mono

		MonoDomain* rootDomain = mono_jit_init("BluJITRuntime");
		if (rootDomain == nullptr)
		{
			return;
		}

		s_Data->RootDomain = rootDomain;

		
		
		LoadAssembly("Resources/Scripts/Blu-ScriptCore.dll");
		LoadAppAssembly("AzureProject/Assets/Scripts/Binaries/Azure-Project.dll");
		ScriptJoiner::RegisterFunctions();
		auto& classes = s_Data->Entities;
		
		s_Data->EntityClass =  std::make_shared<ScriptClass>("Blu", "Entity", true);
		MonoObject* instance = s_Data->EntityClass->Instantiate();
		
		LoadAssemblyClasses();

		
		
	}
	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		
		// Create object then call constructor
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}
	
	void ScriptEngine::ShutdownMono()
	{
		//mono_domain_unload(s_Data->AppDomain);
		s_Data->AppDomain = nullptr;

		//mono_jit_cleanup(s_Data->RootDomain);
		s_Data->RootDomain = nullptr;
	}
	Shared<ScriptClass> ScriptEngine::GetEntityScriptClass(const std::string& className)
	{
		auto it = s_Data->Entities.find(className);
		if (it != s_Data->Entities.end())
		{
			return it->second;
		}

		// If not found, return nullptr
		return nullptr;
	
	}

	


	Shared<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
	{
		auto it = s_Data->EntityInstances.find(entityID);
		if (it == s_Data->EntityInstances.end())
			return nullptr;

		return it->second;
	}

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		const auto& fields = m_ScriptClass->GetScriptFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_get_value(m_Instance, field.ClassField, buffer);
		return true;

	}

	bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
	{
		const auto& fields = m_ScriptClass->GetScriptFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_set_value(m_Instance, field.ClassField, (void*)value);
		return true;

	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return s_Data->CoreAssemblyImage;
	}
	ScriptInstance::ScriptInstance(Shared<ScriptClass> scriptClass, Entity* entity)
		:m_ScriptClass(scriptClass)
	{
		m_Instance = scriptClass->Instantiate();

		m_Constructor = s_Data->EntityClass->GetMethod(".ctor", 1);
		m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
		m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);

		{
			UUID entityID = entity->GetUUID();
			void* param = &entityID;
			scriptClass->InvokeMethod(m_Constructor, m_Instance, &param, nullptr);
		}
	}
	void ScriptInstance::InvokeOnCreate()
	{
		m_ScriptClass->InvokeMethod(m_OnCreateMethod, m_Instance, nullptr, nullptr);

	}
	void ScriptInstance::InvokeOnUpdate(float deltaTime)
	{
		void* param = &deltaTime;
		m_ScriptClass->InvokeMethod(m_OnUpdateMethod, m_Instance, &param, nullptr);
	}
}
