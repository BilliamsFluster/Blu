#include "Blupch.h"
#include "Helpers.h"


namespace Blu
{
	namespace Helpers
	{
		Shared<Scene> SceneHelpers::s_CurrentActiveScene = std::make_shared<Scene>();
		Shared<Scene> SceneHelpers::GetHelperActiveScene()
		{
			return s_CurrentActiveScene;
		}
		void SceneHelpers::SetHelperActiveScene(Shared<Scene> scene)
		{
			s_CurrentActiveScene = scene;
		}
	}
}