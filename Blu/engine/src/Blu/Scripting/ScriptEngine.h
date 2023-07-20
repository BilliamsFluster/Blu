#pragma once
#include <filesystem>

extern "C"
{
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
}


namespace Blu
{
	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::filesystem::path& filepath);
	private:
		static void InitMono();
		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static void ShutdownMono();

		friend class ScriptClass;
	};
	
	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& classNamespace, const std::string& className);
		MonoObject* Instantiate();
		MonoMethod* GetMethod(const std::string& name, int parameterCount);
		MonoObject* InvokeMethod(MonoMethod* method, void* obj, void** params, MonoObject** exc);
	private:
		const std::string m_ClassNamespace;
		const std::string m_ClassName;
		MonoClass* m_MonoClass = nullptr;
	};
	

}

