#pragma once


#ifdef BLU_PLATFORM_WINDOWS
extern  Blu::Application* Blu::CreateApplication();
int main(int argc, char** argv) // main entry point
{
	Blu::Log::Init();
	

	BLU_PROFILE_BEGIN_SESSION("Startup", "BluProfile-Startup.json");
	auto app = Blu::CreateApplication();
	BLU_PROFILE_END_SESSION();

	BLU_PROFILE_BEGIN_SESSION("Runtime", "BluProfile-Runtime.json");
	app->Run();
	BLU_PROFILE_END_SESSION();

	BLU_PROFILE_BEGIN_SESSION("Shutdown", "BluProfile-Shutdown.json");
	delete app;
	BLU_PROFILE_END_SESSION();
}
#endif // BLU_PLATFORM_WINDOWS

