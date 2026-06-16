#pragma once
#include "Blu/Core/Core.h"
#include "Asset.h"
#include <cstddef>
#include <list>
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

	// Snapshot of the resident-asset cache, surfaced in the editor profiler/diagnostics panel.
	struct AssetCacheStats
	{
		size_t ResidentCount   = 0; // assets currently held in the cache
		size_t ReferencedCount = 0; // of those, how many have ReferenceCount > 0 (pinned)
		size_t ResidentBytes   = 0; // sum of GetMemoryUsage() across resident assets
		size_t BudgetBytes     = 0; // 0 == unlimited
		size_t Evictions       = 0; // cumulative evictions since startup
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

		// Editor content operations (registry-synced). RenameAsset moves the source file + its .meta
		// sidecar to a new virtual path while keeping the AssetHandle STABLE (path index, metadata, and
		// any loaded asset's FilePath are updated). DeleteAsset removes the source + .meta and forgets
		// the asset; it refuses while ReferenceCount > 0 unless `force` is set (so you can't delete an
		// asset a live scene still uses by accident). Both return false on failure.
		bool RenameAsset(AssetHandle handle, const std::string& newVirtualPath);
		bool DeleteAsset(AssetHandle handle, bool force = false);
		const AssetMetadata* FindMetadata(AssetHandle handle) const;
		// Returns the existing handle for an (un-normalized) virtual path WITHOUT importing/minting,
		// or AssetHandle(0) if the path was never imported. Lets editor UI (Content Browser) decide
		// whether a file is a managed asset before routing a rename/delete through the registry.
		AssetHandle FindHandleForPath(const std::string& virtualPath) const;
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

		// Memory-budget cache control. Released assets (ReferenceCount == 0) are retained in an LRU
		// instead of freed immediately — so reloading a recently-used asset is a cache hit, not a
		// disk hit. When the resident byte total exceeds the budget, the least-recently-released
		// unreferenced assets are evicted oldest-first. Budget 0 == unlimited (never evict).
		void SetMemoryBudget(size_t budgetBytes);
		size_t GetMemoryBudget() const { return m_MemoryBudgetBytes; }
		AssetCacheStats GetCacheStats() const;
		void EvictToBudget();
		// True when the asset's payload is currently held in the cache (referenced or LRU-retained).
		bool IsResident(AssetHandle handle) const { return m_Assets.find(handle) != m_Assets.end(); }
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

		// LRU bookkeeping for unreferenced (ReferenceCount == 0) resident assets.
		void TrackResident(const Shared<Asset>& asset);     // on first insert into m_Assets
		void MarkReferenced(AssetHandle handle);            // ReferenceCount 0 -> >0: pin (remove from LRU)
		void MarkUnreferenced(AssetHandle handle);          // ReferenceCount -> 0: push to LRU front
		void DropResident(AssetHandle handle);              // erase from cache + accounting

		std::unordered_map<AssetHandle, Shared<Asset>> m_Assets;
		std::unordered_map<AssetHandle, AssetMetadata> m_Metadata;
		std::unordered_map<std::string, AssetHandle> m_PathIndex;
		std::vector<std::string> m_Diagnostics;
		std::string m_RegistryPath = "cache://AssetRegistry.yaml";
		bool m_Initialized = false;

		// Memory budget + LRU of unreferenced handles (front = most-recently released, back = oldest).
		size_t m_MemoryBudgetBytes = 0; // 0 == unlimited
		size_t m_ResidentBytes = 0;
		size_t m_Evictions = 0;
		std::unordered_map<AssetHandle, size_t> m_AssetBytes;                 // snapshot at insert
		std::list<AssetHandle> m_LruUnreferenced;                            // eviction order
		std::unordered_map<AssetHandle, std::list<AssetHandle>::iterator> m_LruPos;
	};
}
