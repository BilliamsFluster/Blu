#include "Blupch.h"
#include "ScriptJoiner.h"
#include "mono/jit/jit.h"


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

	void ScriptJoiner::RegisterFunctions()
	{
		BLU_ADD_INTERNAL_CALL(Log);
	}
}