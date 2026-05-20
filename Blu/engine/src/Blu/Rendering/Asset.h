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
    };
}
