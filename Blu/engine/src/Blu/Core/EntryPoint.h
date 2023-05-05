#pragma once


#ifdef BLU_PLATFORM_WINDOWS
extern  Blu::Application* Blu::CreateApplication();
int main(int argc, char** argv)
{
	Blu::Log::Init();
	BLU_CORE_ERROR("Warning");
	int a = 5;
	BLU_TRACE("ClientWarning", a);


	auto app = Blu::CreateApplication();
	app->Run();
	delete app;
}
#endif // BLU_PLATFORM_WINDOWS

