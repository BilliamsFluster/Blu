#include "Blupch.h"
#include "TextureAsset.h"
#include "Blu/Core/Log.h"

namespace Blu
{
    TextureAsset::TextureAsset(const std::string& filepath, TextureType texType)
        : Asset(AssetType::Texture, filepath), m_TextureType(texType)
    {
        m_Texture = Texture2D::Create(filepath);
        IsLoaded = m_Texture != nullptr;
    }

    Shared<TextureAsset> TextureAsset::Create(const std::string& filepath, TextureType texType)
    {
        return std::make_shared<TextureAsset>(filepath, texType);
    }

    Shared<TextureAsset> TextureAsset::CreateFromMemory(const std::string& name, const void* data, uint32_t size, TextureType texType)
    {
        BLU_CORE_WARN("TextureAsset::CreateFromMemory not fully implemented");
        return nullptr;
    }

    bool TextureAsset::Reload()
    {
        if (FilePath.empty()) return false;
        m_Texture = Texture2D::Create(FilePath);
        IsLoaded = m_Texture != nullptr;
        return IsLoaded;
    }
}
