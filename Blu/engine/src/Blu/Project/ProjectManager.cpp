#include "Blupch.h"
#include "Project.h"
#include "Blu/Core/Log.h"
#include "Blu/Utils/FileSystemService.h"
#include "yaml-cpp/yaml.h"
#include <fstream>

namespace Blu
{
	namespace
	{
		// Find the single ".bluproj" inside a directory (returns empty if none / ambiguous).
		std::filesystem::path FindManifestInDirectory(const std::filesystem::path& dir)
		{
			std::error_code ec;
			std::filesystem::path found;
			for (std::filesystem::directory_iterator it(dir, ec), end; it != end; it.increment(ec))
			{
				if (ec)
					break;
				if (it->is_regular_file(ec) && it->path().extension() == ".bluproj")
				{
					if (!found.empty())
						return {}; // ambiguous: more than one manifest
					found = it->path();
				}
			}
			return found;
		}
	}

	ProjectManager& ProjectManager::Get()
	{
		static ProjectManager instance;
		return instance;
	}

	bool ProjectManager::LoadProject(const std::filesystem::path& bluprojOrDir)
	{
		if (bluprojOrDir.empty())
		{
			BLU_CORE_WARN("ProjectManager: empty project path");
			return false;
		}

		std::error_code ec;
		std::filesystem::path manifest = bluprojOrDir;
		if (std::filesystem::is_directory(manifest, ec))
		{
			manifest = FindManifestInDirectory(bluprojOrDir);
			if (manifest.empty())
			{
				BLU_CORE_WARN("ProjectManager: no unique .bluproj found in directory '{0}'",
					bluprojOrDir.generic_string());
				return false;
			}
		}

		if (!std::filesystem::exists(manifest, ec) || manifest.extension() != ".bluproj")
		{
			BLU_CORE_WARN("ProjectManager: '{0}' is not a .bluproj file", manifest.generic_string());
			return false;
		}

		// Parse the manifest. Tolerate a missing "Project" node by reading from the root.
		Project loaded;
		try
		{
			YAML::Node doc = YAML::LoadFile(manifest.string());
			YAML::Node node = doc["Project"] ? doc["Project"] : doc;
			if (node["Name"])            loaded.Name            = node["Name"].as<std::string>();
			if (node["AssetsDirectory"]) loaded.AssetsDirectory = node["AssetsDirectory"].as<std::string>();
			if (node["StartupScene"])    loaded.StartupScene    = node["StartupScene"].as<std::string>();
		}
		catch (const std::exception& e)
		{
			BLU_CORE_ERROR("ProjectManager: failed to parse '{0}': {1}", manifest.generic_string(), e.what());
			return false;
		}

		loaded.Root = std::filesystem::weakly_canonical(manifest.parent_path(), ec);
		if (ec || loaded.Root.empty())
		{
			// weakly_canonical can fail on exotic paths; still hand callers an absolute Root so
			// GetAssetsPath()/GetStartupScenePath() don't silently become CWD-relative.
			std::error_code absEc;
			loaded.Root = std::filesystem::absolute(manifest.parent_path(), absEc);
			if (absEc || loaded.Root.empty())
				loaded.Root = manifest.parent_path();
		}
		if (loaded.Name.empty())
			loaded.Name = manifest.stem().string();
		if (loaded.AssetsDirectory.empty())
			loaded.AssetsDirectory = "assets";

		// Re-point the project-scoped mounts. "engine://" and "editor://" are intentionally left
		// alone so the editor's own fonts/shaders/icons keep loading from the installed editor.
		auto& fs = FileSystemService::Get();
		fs.Mount("project", loaded.Root);
		fs.Mount("cache", loaded.Root / ".cache");

		m_Active = std::move(loaded);
		m_HasActive = true;
		BLU_CORE_INFO("ProjectManager: activated project '{0}' at '{1}'",
			m_Active.Name, m_Active.Root.generic_string());
		return true;
	}

	bool ProjectManager::SaveProject(const Project& project, const std::filesystem::path& bluprojPath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << project.Name;
		out << YAML::Key << "AssetsDirectory" << YAML::Value << project.AssetsDirectory;
		out << YAML::Key << "StartupScene" << YAML::Value << project.StartupScene;
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::error_code ec;
		std::filesystem::create_directories(bluprojPath.parent_path(), ec);
		std::ofstream file(bluprojPath);
		if (!file)
		{
			BLU_CORE_ERROR("ProjectManager: cannot write '{0}'", bluprojPath.generic_string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	std::filesystem::path ProjectManager::GetAssetsPath() const
	{
		if (!m_HasActive)
			return {};
		return m_Active.Root / m_Active.AssetsDirectory;
	}

	std::filesystem::path ProjectManager::GetStartupScenePath() const
	{
		if (!m_HasActive || m_Active.StartupScene.empty())
			return {};
		return m_Active.Root / m_Active.StartupScene;
	}

	void ProjectManager::Clear()
	{
		m_Active = Project{};
		m_HasActive = false;
	}
}
