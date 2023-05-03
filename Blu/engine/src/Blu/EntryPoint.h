#pragma once

#ifdef BLU_PLATFORM_WINDOWS
extern Blu::Application* Blu::CreateApplication();
int main(int argc, char** argv)
{
	auto app = Blu::CreateApplication();
	app->Run();
	delete app;
}
#endif // BLU_PLATFORM_WINDOWS

