#pragma once
#include "Asset.h"
#include "Blu/Rendering/Texture.h"

namespace Blu
{
    enum class TextureType
    {
        None = 0,
        Albedo,
        Normal,
        MetallicRoughness,
        AO,
        Emissive
    };

    class TextureAsset : public Asset
    {
    public:
        TextureAsset() { Type = AssetType::Texture; }
        TextureAsset(const std::string& filepath, TextureType texType);

        static Shared<TextureAsset> Create(const std::string& filepath, TextureType texType);
        static Shared<TextureAsset> CreateFromMemory(const std::string& name, const void* data, uint32_t size, TextureType texType);

        Shared<Texture2D> GetTexture() const { return m_Texture; }
        TextureType GetTextureType() const { return m_TextureType; }
        std::string GetTexturePath() const { return m_Texture ? m_Texture->GetTexturePath() : ""; }

        uint32_t GetWidth() const { return m_Texture ? m_Texture->GetWidth() : 0; }
        uint32_t GetHeight() const { return m_Texture ? m_Texture->GetHeight() : 0; }
        uint64_t GetImTextureID() const { return m_Texture ? m_Texture->GetImTextureID() : 0; }

        bool Reload() override;

    private:
        Shared<Texture2D> m_Texture;
        TextureType m_TextureType = TextureType::None;
    };
}
