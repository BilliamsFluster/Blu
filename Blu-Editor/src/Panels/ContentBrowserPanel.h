#pragma once
#include <filesystem>
#include <map>
#include <deque>
#include "Blu/Core/Core.h"
#include "Blu/Rendering/Texture.h"


namespace Blu
{
	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();

		void OnImGuiRender();


		//class ImTextureID GetTextureFromFile(const std::filesystem::path& filePath);

		void SortEntries(std::vector<std::filesystem::directory_entry>& entries, int sort_option);

		void ShowDirectoryNodes(const std::filesystem::path& directoryPath);

		std::deque<std::filesystem::path> GetDirectoryPath(const std::filesystem::path& directory);

		void CreateNewFile(const std::filesystem::path& directory, const std::string& baseName);

		void CreateNewFolder(const std::filesystem::path& directory, const std::string& baseName);

	private:
		std::filesystem::path m_CurrentDirectory;
		std::map<std::string, bool> m_DirectoryExpandedState;

		float m_ThumbnailSize = 80.0f;
		Blu::Shared<Blu::Texture2D> m_FolderOpenIcon;
		Blu::Shared<Blu::Texture2D> m_FolderClosedIcon;
		std::map<std::string, Blu::Shared<Blu::Texture2D>> m_FileIcons;
		std::map<std::string, Blu::Shared<Blu::Texture2D>> m_Textures;
		std::map<std::string, Shared<Texture2D>> m_TextureCache;
		std::deque<std::filesystem::path> m_NavigationHistory;

		bool m_ObjectClicked = false;



	};

}

