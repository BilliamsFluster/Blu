#include "Blupch.h"
#include "AssetManager.h"
#include "AssetMeta.h"
#include "StaticMeshAsset.h"
#include "Mesh.h"
#include "ModelLoader.h"
#include "Blu/Core/Log.h"
#include "Blu/Utils/FileSystemService.h"
#include "yaml-cpp/yaml.h"
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

		AssetMetadata metadata;
		metadata.Handle = (hasMeta && meta.IsValid()) ? meta.Handle : AssetHandle();
		metadata.Type = InferAssetType(normalizedPath);
		metadata.VirtualPath = normalizedPath;
		metadata.SourcePath = normalizedPath;

		meta.Handle = metadata.Handle;
		meta.Type = metadata.Type;
		meta.SourcePath = normalizedPath;
		AssetMetaIO::StampSourceInfo(normalizedPath, meta);
		AssetMetaIO::Write(normalizedPath, meta); // best-effort; tolerated if unwritable

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
			++loaded->second->ReferenceCount;
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
		default:
			asset = std::make_shared<Asset>(metadata->Type, metadata->VirtualPath);
			break;
		}
		asset->Handle = metadata->Handle;
		asset->IsLoaded = true;
		asset->ReferenceCount = 1;
		m_Assets[handle] = asset;
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
			m_Assets.erase(loaded);
	}

	const AssetMetadata* AssetManager::FindMetadata(AssetHandle handle) const
	{
		auto metadata = m_Metadata.find(handle);
		return metadata != m_Metadata.end() ? &metadata->second : nullptr;
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
