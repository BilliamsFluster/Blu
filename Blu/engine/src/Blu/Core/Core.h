#pragma once
#include <memory>

#ifdef BLU_PLATFORM_WINDOWS
#if BLU_DYNAMIC_LINK
	#ifdef BLU_BUILD_DLL
		#define BLU_API __declspec(dllexport)
	#else
		#define BLU_API __declspec(dllimport)
	#endif
#else
	#define BLU_API
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
#define BLU_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Blu
{
	template<typename T>
	using Unique = std::unique_ptr<T>;

	template<typename T>
	using Shared = std::shared_ptr<T>;
}