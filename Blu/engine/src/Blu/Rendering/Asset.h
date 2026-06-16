#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Core/UUID.h"

namespace Blu
{
    using AssetHandle = UUID;

    enum class AssetType
    {
        None = 0,
        StaticMesh,
        Material,
        Texture,
        Shader
    };

    class Asset
    {
    public:
        Asset() = default;
        Asset(AssetType type, const std::string& filepath)
            : Handle(), Type(type), FilePath(filepath) {}
        virtual ~Asset() = default;

        AssetHandle Handle;
        AssetType   Type = AssetType::None;
        std::string FilePath;
        bool        IsLoaded = false;
        uint32_t    ReferenceCount = 0;

        virtual bool Reload() { return false; }

        // Approximate resident size in bytes, used by the AssetManager LRU/memory budget.
        // Default 0 (record-only assets); concrete assets that own CPU/GPU memory override it.
        virtual size_t GetMemoryUsage() const { return 0; }
    };
}
