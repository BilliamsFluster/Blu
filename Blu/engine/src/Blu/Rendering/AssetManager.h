#pragma once
#include "Blu/Core/Core.h"
#include "Asset.h"
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Blu
{
	struct Model;
	class MaterialAsset;

	struct AssetMetadata
	{
		AssetHandle Handle = AssetHandle(0);
		AssetType Type = AssetType::None;
		std::string VirtualPath;
		std::string SourcePath;
		std::vector<AssetHandle> Dependencies;
	};

	class AssetManager
	{
	public:
		static AssetManager& Get();

		void Initialize();
		void Shutdown();
		void Reset();
		void SetRegistryPath(std::string registryPath);

		AssetHandle Import(const std::string& filepath);
		// Re-reads the source for an already-imported asset, refreshing its .meta
		// (mtime/size) while PRESERVING the AssetHandle, and reloads it if currently
		// loaded. Returns false for a stale/unknown handle. Foundation for the editor's
		// reimport action (Phase 7).
		bool Reimport(AssetHandle handle);
		Shared<Asset> Load(AssetHandle handle);

		// Resolves a StaticMesh handle to its geometry, performing the (GPU-touching)
		// ModelLoader import on first use and caching it on the asset. Returns nullptr
		// for a non-mesh/stale handle or when the source is missing. Runtime-only —
		// requires a live graphics device.
		Shared<Model> LoadModel(AssetHandle handle);

		// Resolves a Material handle to a loaded MaterialAsset (reads the .blumat).
		// Pure data — no graphics device required. Returns nullptr for a non-material
		// or stale handle.
		Shared<MaterialAsset> LoadMaterial(AssetHandle handle);
		bool Save(AssetHandle handle);
		void Release(AssetHandle handle);
		const AssetMetadata* FindMetadata(AssetHandle handle) const;
		bool AddDependency(AssetHandle asset, AssetHandle dependency);
		bool SaveRegistry() const;

		// Compatibility entry points retained while existing rendering code migrates.
		Shared<Asset> GetAsset(AssetHandle handle) { return Load(handle); }
		bool HasAsset(AssetHandle handle) const;
		AssetHandle ImportAsset(const std::string& filepath) { return Import(filepath); }
		bool SaveAsset(AssetHandle handle) { return Save(handle); }

		template<typename T>
		Shared<T> GetAssetAs(AssetHandle handle)
		{
			static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");
			return std::dynamic_pointer_cast<T>(Load(handle));
		}

		uint32_t GetReferenceCount(AssetHandle handle) const;
		size_t GetLoadedAssetCount() const { return m_Assets.size(); }
		const std::vector<std::string>& GetDiagnostics() const { return m_Diagnostics; }
		const std::unordered_map<AssetHandle, Shared<Asset>>& GetAllAssets() const { return m_Assets; }
		const std::unordered_map<std::string, AssetHandle>& GetPathIndex() const { return m_PathIndex; }

	private:
		AssetManager() = default;
		~AssetManager() = default;
		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;

		void LoadAssetRegistry();
		void AddDiagnostic(const std::string& diagnostic);
		std::string NormalizeAssetPath(const std::string& filepath) const;
		static AssetType InferAssetType(const std::string& filepath);

		std::unordered_map<AssetHandle, Shared<Asset>> m_Assets;
		std::unordered_map<AssetHandle, AssetMetadata> m_Metadata;
		std::unordered_map<std::string, AssetHandle> m_PathIndex;
		std::vector<std::string> m_Diagnostics;
		std::string m_RegistryPath = "cache://AssetRegistry.yaml";
		bool m_Initialized = false;
	};
}
