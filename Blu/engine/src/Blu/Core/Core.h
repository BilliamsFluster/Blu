#pragma once

#ifdef BLU_PLATFORM_WINDOWS
	#ifdef BLU_BUILD_DLL
		#define BLU_API __declspec(dllexport)
	#else
		#define BLU_API __declspec(dllimport)
	#endif
#else
	#error Blu only supports windows
#endif 

#ifdef BLU_ENABLE_ASSERTS
	#define BLU_ASSERT(x, ...) {if(!(x)) {BLU_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); }}
	#define BLU_CORE_ASSERT(x, ...) {if(!(x)) {BLU_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); }}
#else
	#define BLU_ASSERT(x, ...)
	#define BLU_CORE_ASSERT(x, ...)
#endif 

#define BIT(x) (1 << x)
