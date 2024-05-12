#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Scene/Scene.h"


namespace Blu
{
	namespace Helpers
	{
		class SceneHelpers
		{
		public:
			static Shared<Scene> GetHelperActiveScene();
			static void SetHelperActiveScene(Shared<Scene> scene);
		private:

			static Shared<Scene> s_CurrentActiveScene;
		};
	}
}
