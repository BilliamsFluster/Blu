#include "Blupch.h"
#include "ScriptEngine.h"
#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"


namespace Blu
{
	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
	}; 
	
	static ScriptEngineData* s_Data;

	void ScriptEngine::Init()
	{
		s_Data = new ScriptEngineData();
		InitMono();
	}

	char* ReadBytes(const std::string& filepath, uint32_t* outSize)
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

	MonoAssembly* LoadCSharpAssembly(const std::string& assemblyPath)
	{
		uint32_t fileSize = 0;
		char* fileData = ReadBytes(assemblyPath, &fileSize);

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

		if(status != MONO_IMAGE_OK)
		{ 
			const char* errorMessage = mono_image_strerror(status);

			return nullptr;
		}
		MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.c_str(), &status, 0);
		mono_image_close(image);
		delete[] fileData;

		return assembly;
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
		delete s_Data;
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

		//Create App Domain
		s_Data->AppDomain = mono_domain_create_appdomain(const_cast<char*>("BluScriptRuntime"), nullptr);
		mono_domain_set(s_Data->AppDomain, true);


		s_Data->CoreAssembly = LoadCSharpAssembly("Resources/Scripts/Blu-ScriptCore.dll");
		PrintAssemblyTypes(s_Data->CoreAssembly);
	}
	
	void ScriptEngine::ShutdownMono()
	{

	}
}
