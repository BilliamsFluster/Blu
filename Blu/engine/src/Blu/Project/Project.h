#pragma once
#include <filesystem>
#include <string>

namespace Blu
{
	// A Blu project is a self-contained game folder with its own assets, scenes, and a
	// ".bluproj" manifest at its root. It is the unit an editor (or a future launcher/hub)
	// opens: activating a project re-points the "project://" and "cache://" virtual mounts so
	// that asset resolution and the asset registry become project-scoped, while "engine://"
	// and "editor://" continue to point at the installed engine/editor.
	struct Project
	{
		std::string           Name;                 // display name
		std::filesystem::path Root;                 // absolute folder that contains the .bluproj
		std::string           AssetsDirectory = "assets"; // content root, relative to Root
		std::string           StartupScene;         // scene to open on launch, relative to Root (optional)
	};

	// Owns the single active project for the process. Lazy singleton, no GPU/asset dependencies
	// so it is safe to use from headless tools and tests.
	class ProjectManager
	{
	public:
		static ProjectManager& Get();

		// Load and activate a project from a ".bluproj" file, or from a directory that contains
		// exactly one ".bluproj". On success the active project is replaced and the "project://"
		// and "cache://" mounts are re-pointed at the project. On failure the current project is
		// left untouched and false is returned.
		bool LoadProject(const std::filesystem::path& bluprojOrDir);

		// Write a project manifest to disk (does not activate it). Convenience for tooling/tests.
		static bool SaveProject(const Project& project, const std::filesystem::path& bluprojPath);

		bool HasActiveProject() const { return m_HasActive; }
		const Project& GetActiveProject() const { return m_Active; }

		// Absolute content root of the active project (Root / AssetsDirectory). Empty if none.
		std::filesystem::path GetAssetsPath() const;
		// Absolute path of the active project's startup scene. Empty if none / unset.
		std::filesystem::path GetStartupScenePath() const;

		// Deactivate the current project (used by tests; does not restore mounts).
		void Clear();

	private:
		ProjectManager() = default;

		Project m_Active;
		bool    m_HasActive = false;
	};
}
