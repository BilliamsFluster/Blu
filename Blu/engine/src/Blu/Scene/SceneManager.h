#pragma once
#include <string>

namespace Blu
{
	// Lightweight request queue for runtime scene transitions (menu → level → next level).
	// The active Scene is owned by the host layer (editor play-mode / standalone game /
	// headless capture), so this singleton only records "load scene X"; the host calls
	// HasPendingLoad()/ConsumePendingLoad() each frame and performs the actual deserialize +
	// OnRuntimeStop/Start swap. Keeps scene ownership where it is while enabling game flow.
	class SceneManager
	{
	public:
		static SceneManager& Get()
		{
			static SceneManager s_Instance;
			return s_Instance;
		}

		// Queue a scene to load (path relative to the project, e.g. "assets/scenes/X.blu").
		void RequestLoadScene(const std::string& path)
		{
			m_PendingPath = path;
			m_HasPending  = true;
		}

		bool HasPendingLoad() const { return m_HasPending; }

		// Returns the queued path and clears the request.
		std::string ConsumePendingLoad()
		{
			m_HasPending = false;
			std::string p = m_PendingPath;
			m_PendingPath.clear();
			return p;
		}

	private:
		SceneManager() = default;
		std::string m_PendingPath;
		bool        m_HasPending = false;
	};
}
