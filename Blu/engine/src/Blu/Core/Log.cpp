#include "Blupch.h"

#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>

namespace Blu
{
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	void Log::Init()
	{
		std::cout << "Logger running";
		spdlog::set_pattern("[%H:%M:%S] %n: %v%$");		
		s_CoreLogger = spdlog::stdout_color_mt("BLU");
		s_CoreLogger->set_level(spdlog::level::trace);
		
		s_ClientLogger = spdlog::stdout_color_mt("APP");
		s_ClientLogger->set_level(spdlog::level::trace);

		std::cout << "Logger initialized successfully!" << std::endl;
	}
	Log::Log()
	{

	}
	Log::~Log()
	{

	}
	
}