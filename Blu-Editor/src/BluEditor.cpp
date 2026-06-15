#include <Blu.h>
#include <Blu/Core/EntryPoint.h>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "BluEditorLayer.h"
#include "ScreenshotLayer.h"
#include "Blu/Rendering/RenderSettings.h"
#include "Blu/Project/Project.h"
#include "Blu/Core/Log.h"

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
			bool playMode = false;
			bool deferred = false;
			for (const auto& a : args)
			{
				if (a == "--fog")  enableFog = true;
				if (a == "--play") playMode = true; // capture the running game (FP camera + HUD), not the editor view
				if (a == "--deferred") deferred = true; // force the deferred render path (default is forward)
			}
			if (deferred)
				RenderSettings::SetPath(RenderPath::Deferred);
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
					PushLayer(std::make_shared<ScreenshotLayer>(args[i + 1], args[i + 2], w, h, enableFog, playMode));
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
		// `Blu-Editor.exe --project <path>` activates a Blu project (a .bluproj file, or a folder
		// containing one) BEFORE the Application is constructed. This matters: the base Application
		// constructor initializes the AssetManager, which reads its registry from "cache://". By
		// re-pointing project:// / cache:// here first, the editor opens fully scoped to the project.
		// With no --project, nothing is activated and the editor behaves exactly as before.
		const auto& args = Application::GetCommandLineArgs();
		for (size_t i = 0; i + 1 < args.size(); ++i)
		{
			if (args[i] == "--project")
			{
				if (ProjectManager::Get().LoadProject(args[i + 1]))
					BLU_CORE_INFO("Editor: launching project '{0}'", ProjectManager::Get().GetActiveProject().Name);
				else
					BLU_CORE_WARN("Editor: could not load project '{0}' — starting with no active project.", args[i + 1]);
				break;
			}
		}

		return new BluEditor();
	}
}
