#include <Blu.h>
#include <Blu/Core/EntryPoint.h>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "BluEditorLayer.h"
#include "ScreenshotLayer.h"

#include <string>


namespace Blu
{
	class BluEditor : public Blu::Application
	{
	public:
		BluEditor()
			:Application("Blu Editor")
		{
			// Headless capture mode: `Blu-Editor.exe --screenshot <scene.blu> <out.png> [w h]`
			// renders one frame of a scene to a PNG and exits — used to verify rendering
			// changes from the command line. Falls through to the normal editor otherwise.
			const auto& args = Application::GetCommandLineArgs();
			bool enableFog = false;
			for (const auto& a : args)
				if (a == "--fog") enableFog = true;
			for (size_t i = 0; i < args.size(); ++i)
			{
				if (args[i] == "--screenshot" && i + 2 < args.size())
				{
					uint32_t w = 1280, h = 720;
					if (i + 4 < args.size())
					{
						try { w = (uint32_t)std::stoul(args[i + 3]); h = (uint32_t)std::stoul(args[i + 4]); }
						catch (...) {}
					}
					PushLayer(std::make_shared<ScreenshotLayer>(args[i + 1], args[i + 2], w, h, enableFog));
					return;
				}
			}

			PushLayer(std::make_shared<BluEditorLayer>());
		}
		~BluEditor()
		{

		}
	};
		
	

	Blu::Application* Blu::CreateApplication()
	{

		return new BluEditor();
	}
}
