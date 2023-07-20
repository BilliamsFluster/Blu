#include "Blupch.h"
#include "ScriptEngine.h"
#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "ScriptJoiner.h"
#include "Blu/Core/Core.h"


namespace Blu
{
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

		Shared<ScriptClass> EntityClass = nullptr;
	}; 
	
	static ScriptEngineData* s_Data;

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
		:m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		m_MonoClass = mono_class_from_name(s_Data->CoreAssemblyImage, classNamespace.c_str(), className.c_str());

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

	void ScriptEngine::Init()
	{
		s_Data = new ScriptEngineData();
		InitMono();
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

			printf("%s.%s", nameSpace, name); 
		}
	}
	void ScriptEngine::Shutdown()
	{
		ShutdownMono();
		delete s_Data;

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
		ScriptJoiner::RegisterFunctions();

		
		s_Data->EntityClass =  std::make_shared<ScriptClass>("Blu", "Entity");
		MonoObject* instance = s_Data->EntityClass->Instantiate();

		MonoMethod* printFunction = s_Data->EntityClass->GetMethod("PrintMessage", 0);
		s_Data->EntityClass->InvokeMethod(printFunction, instance, nullptr, nullptr);
		
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
}
