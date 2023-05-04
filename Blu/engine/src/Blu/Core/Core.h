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

#define BIT(x) (1 << x)
