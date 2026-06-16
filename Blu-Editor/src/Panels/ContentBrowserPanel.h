#pragma once
#include <filesystem>
#include <map>
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include "Blu/Core/Core.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/EditorCamera.h"


namespace Blu
{
	struct Model;

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();

		void OnImGuiRender();
		void SetSaveAllCallback(std::function<void()> callback) { m_SaveAllCallback = std::move(callback); }
		void SetImportModelCallback(std::function<void(const std::filesystem::path&)> callback) { m_ImportModelCallback = std::move(callback); }
		void SetGenerateStaticCollisionCallback(std::function<void(const std::filesystem::path&)> callback) { m_GenerateStaticCollisionCallback = std::move(callback); }
		void SetInstantiatePrefabCallback(std::function<void(const std::filesystem::path&)> callback) { m_InstantiatePrefabCallback = std::move(callback); }
		void SetSaveSelectedAsPrefabCallback(std::function<void()> callback) { m_SaveSelectedAsPrefabCallback = std::move(callback); }
		void SetOpenSoundEditorCallback(std::function<void(const std::filesystem::path&)> callback) { m_OpenSoundEditorCallback = std::move(callback); }
		void SetOpenMeshEditorCallback(std::function<void(const std::filesystem::path&)> callback) { m_OpenMeshEditorCallback = std::move(callback); }
		void SetOpenShaderEditorCallback(std::function<void(const std::filesystem::path&)> callback) { m_OpenShaderEditorCallback = std::move(callback); }
		void SetBrowserDirectory(const std::filesystem::path& directory);
		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		void SetThumbnailSize(float size) { m_ThumbnailSize = size; }
		float GetThumbnailSize() const { return m_ThumbnailSize; }


		//class ImTextureID GetTextureFromFile(const std::filesystem::path& filePath);

		void SortEntries(std::vector<std::filesystem::directory_entry>& entries, int sort_option);

		void ShowDirectoryNodes(const std::filesystem::path& directoryPath);

		std::deque<std::filesystem::path> GetDirectoryPath(const std::filesystem::path& directory);

		void CreateNewFile(const std::filesystem::path& directory, const std::string& baseName);

		void CreateNewFolder(const std::filesystem::path& directory, const std::string& baseName);

	private:
		struct ThumbnailCacheEntry
		{
			Shared<Texture2D> Texture;
			std::filesystem::file_time_type LastWriteTime;
		};

		struct ModelThumbnailCacheEntry
		{
			Shared<FrameBuffer> Framebuffer;
			std::filesystem::file_time_type LastWriteTime;
			bool Failed = false;
		};

		Shared<Texture2D> GetImageThumbnail(const std::filesystem::path& path);
		uint64_t GetModelThumbnail(const std::filesystem::path& path, bool& failed);
		bool RenderModelThumbnail(const std::filesystem::path& path, ModelThumbnailCacheEntry& entry);

		std::filesystem::path m_CurrentDirectory;
		std::map<std::string, bool> m_DirectoryExpandedState;

		float m_ThumbnailSize = 80.0f;
		Blu::Shared<Blu::Texture2D> m_FolderOpenIcon;
		Blu::Shared<Blu::Texture2D> m_FolderClosedIcon;
		std::map<std::string, Blu::Shared<Blu::Texture2D>> m_FileIcons;
		std::map<std::string, Blu::Shared<Blu::Texture2D>> m_Textures;
		std::map<std::string, Shared<Texture2D>> m_TextureCache;
		std::map<std::string, ThumbnailCacheEntry> m_ImageThumbnailCache;
		std::map<std::string, ModelThumbnailCacheEntry> m_ModelThumbnailCache;
		std::deque<std::filesystem::path> m_NavigationHistory;
		EditorCamera m_ModelThumbnailCamera;
		uint32_t m_ModelThumbnailsRenderedThisFrame = 0;

		bool m_ObjectClicked = false;
		int m_SortMode = 0;
		int m_TypeFilter = 0; // index into s_TypeFilterItems (0 = All)
		std::vector<std::filesystem::path> m_FavoritePaths; // pinned folders (persisted)
		std::string m_SelectedFilename;
		void LoadFavorites();
		void SaveFavorites() const;
		bool IsFavorite(const std::filesystem::path& p) const;
		void ToggleFavorite(const std::filesystem::path& p);
		std::function<void()> m_SaveAllCallback;
		std::function<void(const std::filesystem::path&)> m_ImportModelCallback;
		std::function<void(const std::filesystem::path&)> m_GenerateStaticCollisionCallback;
		std::function<void(const std::filesystem::path&)> m_InstantiatePrefabCallback;
		std::function<void()> m_SaveSelectedAsPrefabCallback;
		std::function<void(const std::filesystem::path&)> m_OpenSoundEditorCallback;
		std::function<void(const std::filesystem::path&)> m_OpenMeshEditorCallback;
		std::function<void(const std::filesystem::path&)> m_OpenShaderEditorCallback;



	};

}

