#pragma once
#include "Core.h"
#include "spdlog/spdlog.h"
#include <memory>
#include <iostream>

namespace Blu
{

	class BLU_API Log
	{
	public:
		static void Init();
		Log();
		~Log();
		
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
		


	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;


	};
}
//core logging macros
#define BLU_CORE_TRACE(...)  ::Blu::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BLU_CORE_INFO(...)   ::Blu::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BLU_CORE_WARN(...)   ::Blu::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BLU_CORE_ERROR(...)  ::Blu::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BLU_CORE_FATAL(...)  ::Blu::Log::GetCoreLogger()->fatal(__VA_ARGS__)

// client logging macros

#define BLU_TRACE(...)		 ::Blu::Log::GetClientLogger()->trace(__VA_ARGS__)
#define BLU_INFO(...)		 ::Blu::Log::GetClientLogger()->info(__VA_ARGS__)
#define BLU_WARN(...)		 ::Blu::Log::GetClientLogger()->warn(__VA_ARGS__)
#define BLU_ERROR(...)		 ::Blu::Log::GetClientLogger()->error(__VA_ARGS__)
#define BLU_FATAL(...)		 ::Blu::Log::GetClientLogger()->fatal(__VA_ARGS__)

// if dist build

//#define BLU_CORE_INFO 




