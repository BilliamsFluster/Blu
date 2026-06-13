#include "Blupch.h"
#include "AssetMeta.h"
#include "Blu/Utils/FileSystemService.h"
#include "yaml-cpp/yaml.h"
#include <filesystem>
#include <system_error>

namespace Blu
{
    std::string AssetMetaIO::MetaPathFor(const std::string& sourceVirtualPath)
    {
        if (sourceVirtualPath.empty())
            return {};
        return sourceVirtualPath + ".meta";
    }

    bool AssetMetaIO::Read(const std::string& sourceVirtualPath, AssetMeta& outMeta)
    {
        const std::string metaPath = MetaPathFor(sourceVirtualPath);
        if (metaPath.empty())
            return false;

        auto& fileSystem = FileSystemService::Get();
        std::string contents;
        if (!fileSystem.Read(metaPath, contents) || contents.empty())
            return false;

        try
        {
            const YAML::Node root = YAML::Load(contents);
            const YAML::Node meta = root["AssetMeta"];
            if (!meta)
                return false;

            const uint64_t rawHandle = meta["Handle"].as<uint64_t>(0);
            if (rawHandle == 0)
                return false;

            outMeta.Handle = AssetHandle(rawHandle);
            outMeta.Type = (AssetType)meta["Type"].as<int>((int)AssetType::None);
            outMeta.SourcePath = meta["Source"].as<std::string>(sourceVirtualPath);
            outMeta.SourceMTime = meta["SourceMTime"].as<uint64_t>(0);
            outMeta.SourceSize = meta["SourceSize"].as<uint64_t>(0);

            if (const YAML::Node mesh = meta["Mesh"])
            {
                outMeta.Mesh.Scale = mesh["Scale"].as<float>(outMeta.Mesh.Scale);
                outMeta.Mesh.GenerateLODs = mesh["GenerateLODs"].as<bool>(outMeta.Mesh.GenerateLODs);
                outMeta.Mesh.LODCount = mesh["LODCount"].as<int>(outMeta.Mesh.LODCount);
                outMeta.Mesh.ImportMaterials = mesh["ImportMaterials"].as<bool>(outMeta.Mesh.ImportMaterials);
            }
            if (const YAML::Node texture = meta["Texture"])
            {
                outMeta.Texture.SRGB = texture["SRGB"].as<bool>(outMeta.Texture.SRGB);
                outMeta.Texture.GenerateMips = texture["GenerateMips"].as<bool>(outMeta.Texture.GenerateMips);
            }
            return true;
        }
        catch (const std::exception&)
        {
            // Malformed sidecar — treat as absent so the caller re-mints/re-writes it.
            return false;
        }
    }

    bool AssetMetaIO::Write(const std::string& sourceVirtualPath, const AssetMeta& meta)
    {
        const std::string metaPath = MetaPathFor(sourceVirtualPath);
        if (metaPath.empty() || !meta.IsValid())
            return false;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AssetMeta" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Handle" << YAML::Value << (uint64_t)meta.Handle;
        out << YAML::Key << "Type" << YAML::Value << (int)meta.Type;
        out << YAML::Key << "Source" << YAML::Value << (meta.SourcePath.empty() ? sourceVirtualPath : meta.SourcePath);
        out << YAML::Key << "SourceMTime" << YAML::Value << meta.SourceMTime;
        out << YAML::Key << "SourceSize" << YAML::Value << meta.SourceSize;
        out << YAML::Key << "Mesh" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Scale" << YAML::Value << meta.Mesh.Scale;
        out << YAML::Key << "GenerateLODs" << YAML::Value << meta.Mesh.GenerateLODs;
        out << YAML::Key << "LODCount" << YAML::Value << meta.Mesh.LODCount;
        out << YAML::Key << "ImportMaterials" << YAML::Value << meta.Mesh.ImportMaterials;
        out << YAML::EndMap; // Mesh
        out << YAML::Key << "Texture" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "SRGB" << YAML::Value << meta.Texture.SRGB;
        out << YAML::Key << "GenerateMips" << YAML::Value << meta.Texture.GenerateMips;
        out << YAML::EndMap; // Texture
        out << YAML::EndMap; // AssetMeta
        out << YAML::EndMap;

        return FileSystemService::Get().Write(metaPath, out.c_str());
    }

    void AssetMetaIO::StampSourceInfo(const std::string& sourceVirtualPath, AssetMeta& meta)
    {
        auto& fileSystem = FileSystemService::Get();
        const std::filesystem::path source(sourceVirtualPath);
        const std::filesystem::path resolved = fileSystem.IsVirtualPath(source)
            ? fileSystem.Resolve(source)
            : source;
        if (resolved.empty())
            return;

        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(resolved, error);
        if (!error)
            meta.SourceMTime = (uint64_t)writeTime.time_since_epoch().count();

        const auto size = std::filesystem::file_size(resolved, error);
        if (!error)
            meta.SourceSize = (uint64_t)size;
    }
}
