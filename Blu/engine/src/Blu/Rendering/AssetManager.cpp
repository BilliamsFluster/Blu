#include "Blupch.h"
#include "AssetManager.h"
#include "AssetMeta.h"
#include "StaticMeshAsset.h"
#include "MaterialAsset.h"
#include "Mesh.h"
#include "ModelLoader.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/JobSystem.h"
#include "Blu/Utils/FileSystemService.h"
#include "yaml-cpp/yaml.h"
#include "stb_image.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace Blu
{
	AssetManager& AssetManager::Get()
	{
		static AssetManager instance;
		return instance;
	}

	void AssetManager::Initialize()
	{
		if (m_Initialized)
			return;
		LoadAssetRegistry();
		m_Initialized = true;
		if (Log::GetCoreLogger())
			BLU_CORE_INFO("AssetManager initialized");
	}

	void AssetManager::Shutdown()
	{
		if (m_Initialized)
			SaveRegistry();
		m_Assets.clear();
		m_Metadata.clear();
		m_PathIndex.clear();
		m_AssetBytes.clear();
		m_LruUnreferenced.clear();
		m_LruPos.clear();
		m_ResidentBytes = 0;
		m_Initialized = false;
		if (Log::GetCoreLogger())
			BLU_CORE_INFO("AssetManager shutdown");
	}

	void AssetManager::Reset()
	{
		m_Assets.clear();
		m_Metadata.clear();
		m_PathIndex.clear();
		m_Diagnostics.clear();
		m_AssetBytes.clear();
		m_LruUnreferenced.clear();
		m_LruPos.clear();
		m_ResidentBytes = 0;
		m_Evictions = 0;
		m_MemoryBudgetBytes = 0;
		m_Initialized = false;
	}

	void AssetManager::SetRegistryPath(std::string registryPath)
	{
		m_RegistryPath = std::move(registryPath);
	}

	AssetHandle AssetManager::Import(const std::string& filepath)
	{
		if (!m_Initialized)
			Initialize();

		const std::string normalizedPath = NormalizeAssetPath(filepath);
		if (normalizedPath.empty())
		{
			AddDiagnostic("AssetManager: rejected invalid asset path '" + filepath + "'");
			return AssetHandle(0);
		}

		auto existing = m_PathIndex.find(normalizedPath);
		if (existing != m_PathIndex.end())
			return existing->second;

		// Consult a .meta sidecar first so the handle is STABLE across a lost/rebuilt
		// registry, a moved project, or sharing across machines. If absent (or invalid),
		// mint a fresh UUID and write the sidecar so future imports recover it.
		AssetMeta meta;
		const bool hasMeta = AssetMetaIO::Read(normalizedPath, meta);
		const bool minted = !(hasMeta && meta.IsValid());

		AssetMetadata metadata;
		metadata.Handle = minted ? AssetHandle() : meta.Handle;
		metadata.Type = InferAssetType(normalizedPath);
		metadata.VirtualPath = normalizedPath;
		metadata.SourcePath = normalizedPath;

		// Only (re)write the sidecar when minting a fresh handle, so an existing .meta —
		// including user-tuned import settings — is preserved across ordinary imports
		// (e.g. when a scene load registers the assets it references).
		if (minted)
		{
			meta = AssetMeta{};
			meta.Handle = metadata.Handle;
			meta.Type = metadata.Type;
			meta.SourcePath = normalizedPath;
			AssetMetaIO::StampSourceInfo(normalizedPath, meta);
			AssetMetaIO::Write(normalizedPath, meta); // best-effort; tolerated if unwritable
		}

		m_PathIndex[normalizedPath] = metadata.Handle;
		m_Metadata[metadata.Handle] = metadata;
		return metadata.Handle;
	}

	bool AssetManager::Reimport(AssetHandle handle)
	{
		if (!m_Initialized)
			Initialize();

		const AssetMetadata* metadata = FindMetadata(handle);
		if (!metadata)
		{
			AddDiagnostic("AssetManager: cannot reimport stale asset handle " + std::to_string((uint64_t)handle));
			return false;
		}

		// Refresh the sidecar's source stamp while preserving the handle, so callers can
		// detect a changed source and the UUID survives.
		AssetMeta meta;
		AssetMetaIO::Read(metadata->SourcePath, meta);
		meta.Handle = metadata->Handle;
		meta.Type = metadata->Type;
		meta.SourcePath = metadata->SourcePath;
		AssetMetaIO::StampSourceInfo(metadata->SourcePath, meta);
		AssetMetaIO::Write(metadata->SourcePath, meta);

		// If the asset is resident, reload its contents in place (handle/refs unchanged).
		auto loaded = m_Assets.find(handle);
		if (loaded != m_Assets.end() && loaded->second)
			loaded->second->Reload();
		return true;
	}

	Shared<Asset> AssetManager::Load(AssetHandle handle)
	{
		if (!m_Initialized)
			Initialize();

		auto loaded = m_Assets.find(handle);
		if (loaded != m_Assets.end())
		{
			const uint32_t before = loaded->second->ReferenceCount;
			++loaded->second->ReferenceCount;
			if (before == 0)
				MarkReferenced(handle); // was an LRU candidate; pin it again
			return loaded->second;
		}

		const AssetMetadata* metadata = FindMetadata(handle);
		if (!metadata)
		{
			AddDiagnostic("AssetManager: stale asset handle " + std::to_string((uint64_t)handle));
			return nullptr;
		}

		const std::filesystem::path resolved = FileSystemService::Get().IsVirtualPath(metadata->VirtualPath)
			? FileSystemService::Get().Resolve(metadata->VirtualPath)
			: std::filesystem::path(metadata->VirtualPath);
		if (resolved.empty() || !std::filesystem::exists(resolved))
		{
			AddDiagnostic("AssetManager: missing asset '" + metadata->VirtualPath + "'");
			return nullptr;
		}

		// Construct the concrete asset for the metadata's type. StaticMesh geometry is
		// loaded lazily (see LoadModel) so handle resolution stays GPU-free; other types
		// use the base Asset record for now (typed texture/material loaders come later).
		Shared<Asset> asset;
		switch (metadata->Type)
		{
		case AssetType::StaticMesh:
			asset = std::make_shared<StaticMeshAsset>(metadata->VirtualPath);
			break;
		case AssetType::Material:
		{
			auto materialAsset = std::make_shared<MaterialAsset>(metadata->VirtualPath);
			materialAsset->LoadFromFile(metadata->VirtualPath); // .blumat is pure data (no GPU)
			asset = materialAsset;
			break;
		}
		default:
			asset = std::make_shared<Asset>(metadata->Type, metadata->VirtualPath);
			break;
		}
		asset->Handle = metadata->Handle;
		asset->IsLoaded = true;
		asset->ReferenceCount = 1;
		m_Assets[handle] = asset;
		TrackResident(asset); // record bytes; referenced (count==1) so not an eviction candidate yet
		return asset;
	}

	Shared<Model> AssetManager::LoadModel(AssetHandle handle)
	{
		auto staticMesh = std::dynamic_pointer_cast<StaticMeshAsset>(Load(handle));
		if (!staticMesh)
			return nullptr;

		if (!staticMesh->LoadedModel)
		{
			const std::filesystem::path resolved = FileSystemService::Get().IsVirtualPath(staticMesh->FilePath)
				? FileSystemService::Get().Resolve(staticMesh->FilePath)
				: std::filesystem::path(staticMesh->FilePath);
			if (resolved.empty() || !std::filesystem::exists(resolved))
			{
				AddDiagnostic("AssetManager: static mesh source missing '" + staticMesh->FilePath + "'");
				return nullptr;
			}
			staticMesh->LoadedModel = ModelLoader::Load(resolved.string());
		}
		return staticMesh->LoadedModel;
	}

	Shared<MaterialAsset> AssetManager::LoadMaterial(AssetHandle handle)
	{
		return std::dynamic_pointer_cast<MaterialAsset>(Load(handle));
	}

	bool AssetManager::Save(AssetHandle handle)
	{
		if (!FindMetadata(handle))
		{
			AddDiagnostic("AssetManager: cannot save stale asset handle " + std::to_string((uint64_t)handle));
			return false;
		}
		return SaveRegistry();
	}

	void AssetManager::Release(AssetHandle handle)
	{
		auto loaded = m_Assets.find(handle);
		if (loaded == m_Assets.end())
			return;

		if (loaded->second->ReferenceCount > 0)
			--loaded->second->ReferenceCount;
		if (loaded->second->ReferenceCount == 0)
		{
			// Retain in the LRU (a recent reload is then a cache hit, not a disk hit) and only free
			// under memory pressure. Budget 0 (unlimited) keeps everything resident as before.
			MarkUnreferenced(handle);
			EvictToBudget();
		}
	}

	// ── Resident-cache LRU + memory budget ───────────────────────────────────────
	void AssetManager::TrackResident(const Shared<Asset>& asset)
	{
		if (!asset)
			return;
		const size_t bytes = asset->GetMemoryUsage();
		m_AssetBytes[asset->Handle] = bytes;
		m_ResidentBytes += bytes;
	}

	void AssetManager::MarkReferenced(AssetHandle handle)
	{
		auto pos = m_LruPos.find(handle);
		if (pos != m_LruPos.end())
		{
			m_LruUnreferenced.erase(pos->second);
			m_LruPos.erase(pos);
		}
	}

	void AssetManager::MarkUnreferenced(AssetHandle handle)
	{
		if (m_LruPos.find(handle) != m_LruPos.end())
			return; // already an eviction candidate
		m_LruUnreferenced.push_front(handle);
		m_LruPos[handle] = m_LruUnreferenced.begin();
	}

	void AssetManager::DropResident(AssetHandle handle)
	{
		auto bytes = m_AssetBytes.find(handle);
		if (bytes != m_AssetBytes.end())
		{
			m_ResidentBytes -= std::min(m_ResidentBytes, bytes->second);
			m_AssetBytes.erase(bytes);
		}
		auto pos = m_LruPos.find(handle);
		if (pos != m_LruPos.end())
		{
			m_LruUnreferenced.erase(pos->second);
			m_LruPos.erase(pos);
		}
		m_Assets.erase(handle);
	}

	void AssetManager::SetMemoryBudget(size_t budgetBytes)
	{
		m_MemoryBudgetBytes = budgetBytes;
		EvictToBudget();
	}

	void AssetManager::EvictToBudget()
	{
		if (m_MemoryBudgetBytes == 0)
			return; // unlimited
		// Evict least-recently-released (back of the list) unreferenced assets until within budget.
		while (m_ResidentBytes > m_MemoryBudgetBytes && !m_LruUnreferenced.empty())
		{
			const AssetHandle oldest = m_LruUnreferenced.back();
			DropResident(oldest);
			++m_Evictions;
		}
	}

	AssetCacheStats AssetManager::GetCacheStats() const
	{
		AssetCacheStats stats;
		stats.ResidentCount = m_Assets.size();
		for (const auto& [handle, asset] : m_Assets)
		{
			if (asset && asset->ReferenceCount > 0)
				++stats.ReferencedCount;
		}
		stats.ResidentBytes = m_ResidentBytes;
		stats.BudgetBytes   = m_MemoryBudgetBytes;
		stats.Evictions     = m_Evictions;
		return stats;
	}

	bool AssetManager::RenameAsset(AssetHandle handle, const std::string& newVirtualPath)
	{
		auto meta = m_Metadata.find(handle);
		if (meta == m_Metadata.end())
		{
			AddDiagnostic("AssetManager: cannot rename stale asset handle " + std::to_string((uint64_t)handle));
			return false;
		}
		const std::string oldVirtual = meta->second.VirtualPath;
		const std::string newNormalized = NormalizeAssetPath(newVirtualPath);
		if (newNormalized.empty() || newNormalized == NormalizeAssetPath(oldVirtual))
			return false;
		if (m_PathIndex.find(newNormalized) != m_PathIndex.end())
		{
			AddDiagnostic("AssetManager: rename target already exists '" + newNormalized + "'");
			return false;
		}

		auto& fs = FileSystemService::Get();
		auto resolve = [&fs](const std::string& v) {
			return fs.IsVirtualPath(v) ? fs.Resolve(v) : std::filesystem::path(v);
		};
		std::error_code ec;

		const std::filesystem::path oldResolved = resolve(oldVirtual);
		const std::filesystem::path newResolved = resolve(newNormalized);
		if (!oldResolved.empty() && std::filesystem::exists(oldResolved))
		{
			if (!newResolved.parent_path().empty())
				std::filesystem::create_directories(newResolved.parent_path(), ec);
			std::filesystem::rename(oldResolved, newResolved, ec);
			if (ec)
			{
				AddDiagnostic("AssetManager: rename failed: " + ec.message());
				return false;
			}
		}
		// Move the .meta sidecar alongside (best-effort — a missing sidecar is fine).
		const std::filesystem::path oldMeta = resolve(AssetMetaIO::MetaPathFor(oldVirtual));
		const std::filesystem::path newMeta = resolve(AssetMetaIO::MetaPathFor(newNormalized));
		if (!oldMeta.empty() && std::filesystem::exists(oldMeta))
			std::filesystem::rename(oldMeta, newMeta, ec);

		// Update the in-memory registry; the handle is unchanged so all references stay valid.
		m_PathIndex.erase(NormalizeAssetPath(oldVirtual));
		m_PathIndex[newNormalized] = handle;
		meta->second.VirtualPath = newNormalized;
		if (!meta->second.SourcePath.empty())
			meta->second.SourcePath = newNormalized;
		auto loaded = m_Assets.find(handle);
		if (loaded != m_Assets.end() && loaded->second)
			loaded->second->FilePath = newNormalized;
		// Re-point the sidecar's recorded source path so a future reimport resolves correctly.
		AssetMeta sidecar;
		if (AssetMetaIO::Read(newNormalized, sidecar))
		{
			sidecar.SourcePath = newNormalized;
			AssetMetaIO::Write(newNormalized, sidecar);
		}
		SaveRegistry();
		return true;
	}

	JobHandle AssetManager::DecodeTextureAsync(const std::string& virtualPath,
		std::function<void(const DecodedImage&)> onComplete)
	{
		auto& fileSystem = FileSystemService::Get();
		const std::filesystem::path resolved = fileSystem.IsVirtualPath(virtualPath)
			? fileSystem.Resolve(virtualPath) : std::filesystem::path(virtualPath);
		const std::string path = resolved.string();

		// Shared so the worker fills it and the main-thread continuation reads it.
		auto image = std::make_shared<DecodedImage>();
		auto callback = std::make_shared<std::function<void(const DecodedImage&)>>(std::move(onComplete));

		return JobSystem::Get().Submit([image, callback, path]()
		{
			// CPU decode on a worker thread (stb_image is thread-safe; we never touch the global
			// flip flag here so we can't race the synchronous texture path).
			int w = 0, h = 0, channels = 0;
			unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
			if (pixels)
			{
				image->Width = w;
				image->Height = h;
				image->Pixels.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
				image->Ok = true;
				stbi_image_free(pixels);
			}
			// Hand the decoded pixels to the main thread, where the GPU texture is created safely.
			JobSystem::Get().EnqueueMainThread([image, callback]() { (*callback)(*image); });
		}, "DecodeTexture");
	}

	bool AssetManager::DeleteAsset(AssetHandle handle, bool force)
	{
		auto meta = m_Metadata.find(handle);
		if (meta == m_Metadata.end())
		{
			AddDiagnostic("AssetManager: cannot delete stale asset handle " + std::to_string((uint64_t)handle));
			return false;
		}
		if (!force && GetReferenceCount(handle) > 0)
		{
			AddDiagnostic("AssetManager: refusing to delete referenced asset " + std::to_string((uint64_t)handle));
			return false;
		}

		const std::string virtualPath = meta->second.VirtualPath;
		auto& fs = FileSystemService::Get();
		auto resolve = [&fs](const std::string& v) {
			return fs.IsVirtualPath(v) ? fs.Resolve(v) : std::filesystem::path(v);
		};
		std::error_code ec;
		const std::filesystem::path resolved = resolve(virtualPath);
		if (!resolved.empty())
			std::filesystem::remove(resolved, ec); // best-effort
		const std::filesystem::path metaResolved = resolve(AssetMetaIO::MetaPathFor(virtualPath));
		if (!metaResolved.empty())
			std::filesystem::remove(metaResolved, ec);

		DropResident(handle); // drop cache entry + byte accounting + LRU position
		m_Metadata.erase(handle);
		m_PathIndex.erase(NormalizeAssetPath(virtualPath));
		SaveRegistry();
		return true;
	}

	const AssetMetadata* AssetManager::FindMetadata(AssetHandle handle) const
	{
		auto metadata = m_Metadata.find(handle);
		return metadata != m_Metadata.end() ? &metadata->second : nullptr;
	}

	AssetHandle AssetManager::FindHandleForPath(const std::string& virtualPath) const
	{
		const std::string normalized = NormalizeAssetPath(virtualPath);
		if (normalized.empty())
			return AssetHandle(0);
		auto it = m_PathIndex.find(normalized);
		return it != m_PathIndex.end() ? it->second : AssetHandle(0);
	}

	bool AssetManager::AddDependency(AssetHandle asset, AssetHandle dependency)
	{
		auto metadata = m_Metadata.find(asset);
		if (metadata == m_Metadata.end() || !m_Metadata.contains(dependency))
			return false;

		auto& dependencies = metadata->second.Dependencies;
		if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end())
			dependencies.push_back(dependency);
		return true;
	}

	bool AssetManager::SaveRegistry() const
	{
		YAML::Emitter output;
		output << YAML::BeginMap;
		output << YAML::Key << "AssetRegistry" << YAML::Value << YAML::BeginSeq;
		for (const auto& [_, metadata] : m_Metadata)
		{
			output << YAML::BeginMap;
			output << YAML::Key << "Handle" << YAML::Value << (uint64_t)metadata.Handle;
			output << YAML::Key << "Type" << YAML::Value << (int)metadata.Type;
			output << YAML::Key << "Path" << YAML::Value << metadata.VirtualPath;
			output << YAML::Key << "Source" << YAML::Value << metadata.SourcePath;
			output << YAML::Key << "Dependencies" << YAML::Value << YAML::BeginSeq;
			for (AssetHandle dependency : metadata.Dependencies)
				output << (uint64_t)dependency;
			output << YAML::EndSeq;
			output << YAML::EndMap;
		}
		output << YAML::EndSeq;
		output << YAML::EndMap;
		return FileSystemService::Get().Write(m_RegistryPath, output.c_str());
	}

	bool AssetManager::HasAsset(AssetHandle handle) const
	{
		return m_Metadata.contains(handle);
	}

	uint32_t AssetManager::GetReferenceCount(AssetHandle handle) const
	{
		auto loaded = m_Assets.find(handle);
		return loaded != m_Assets.end() ? loaded->second->ReferenceCount : 0;
	}

	void AssetManager::LoadAssetRegistry()
	{
		m_Assets.clear();
		m_Metadata.clear();
		m_PathIndex.clear();

		std::string registryContents;
		if (!FileSystemService::Get().Read(m_RegistryPath, registryContents))
			return;

		const YAML::Node root = YAML::Load(registryContents);
		const YAML::Node registry = root["AssetRegistry"];
		if (!registry || !registry.IsSequence())
			return;

		for (const YAML::Node& assetNode : registry)
		{
			const uint64_t rawHandle = assetNode["Handle"].as<uint64_t>(0);
			if (rawHandle == 0)
				continue;

			AssetMetadata metadata;
			metadata.Handle = AssetHandle(rawHandle);
			metadata.Type = (AssetType)assetNode["Type"].as<int>((int)AssetType::None);
			metadata.VirtualPath = assetNode["Path"].as<std::string>("");
			metadata.SourcePath = assetNode["Source"].as<std::string>(metadata.VirtualPath);
			if (const YAML::Node dependencies = assetNode["Dependencies"]; dependencies && dependencies.IsSequence())
			{
				for (const YAML::Node& dependency : dependencies)
					metadata.Dependencies.emplace_back(dependency.as<uint64_t>());
			}
			if (!metadata.VirtualPath.empty())
			{
				m_PathIndex[metadata.VirtualPath] = metadata.Handle;
				m_Metadata[metadata.Handle] = std::move(metadata);
			}
		}
	}

	void AssetManager::AddDiagnostic(const std::string& diagnostic)
	{
		m_Diagnostics.push_back(diagnostic);
		if (Log::GetCoreLogger())
			BLU_CORE_WARN("{0}", diagnostic);
	}

	std::string AssetManager::NormalizeAssetPath(const std::string& filepath) const
	{
		if (filepath.empty())
			return {};

		auto& fileSystem = FileSystemService::Get();
		const std::filesystem::path path(filepath);
		if (fileSystem.IsVirtualPath(path))
		{
			const auto resolved = fileSystem.Resolve(path);
			return resolved.empty() ? std::string() : fileSystem.ToVirtualPath(resolved, filepath.substr(0, filepath.find("://")));
		}

		if (path.is_absolute())
			return fileSystem.ToVirtualPath(path, "project");

		for (const auto& part : path)
		{
			if (part == "..")
				return {};
		}
		return "project://" + path.lexically_normal().generic_string();
	}

	AssetType AssetManager::InferAssetType(const std::string& filepath)
	{
		std::string extension = std::filesystem::path(filepath).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character) { return (char)std::tolower(character); });
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp")
			return AssetType::Texture;
		if (extension == ".blumat" || extension == ".material")
			return AssetType::Material;
		if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb")
			return AssetType::StaticMesh;
		if (extension == ".hlsl" || extension == ".glsl")
			return AssetType::Shader;
		return AssetType::None;
	}
}
