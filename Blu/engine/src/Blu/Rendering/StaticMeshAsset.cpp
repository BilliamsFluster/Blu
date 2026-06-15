#include "Blupch.h"
#include "StaticMeshAsset.h"
#include "Mesh.h"
#include "ModelLoader.h"
#include "Blu/Utils/FileSystemService.h"
#include <filesystem>

namespace Blu
{
    bool StaticMeshAsset::Reload()
    {
        if (FilePath.empty())
            return false;

        auto& fileSystem = FileSystemService::Get();
        const std::filesystem::path source(FilePath);
        const std::filesystem::path resolved = fileSystem.IsVirtualPath(source)
            ? fileSystem.Resolve(source)
            : source;
        if (resolved.empty() || !std::filesystem::exists(resolved))
            return false;

        LoadedModel = ModelLoader::Load(resolved.string());
        IsLoaded = (LoadedModel != nullptr);
        return IsLoaded;
    }
}
