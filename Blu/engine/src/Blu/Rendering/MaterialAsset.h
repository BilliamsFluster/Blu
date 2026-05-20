#pragma once
#include "Asset.h"
#include "TextureAsset.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Shader.h"

namespace Blu
{
    // Scalar PBR properties stored inside a MaterialAsset.
    // Mirrors the flat layout of Material (minus blend/shading metadata).
    struct PBRProperties
    {
        glm::vec4 AlbedoColor      = glm::vec4(1.0f);  // rgba
        float     Metallic         = 0.0f;
        float     Roughness        = 0.5f;
        float     AO               = 1.0f;
        glm::vec3 EmissiveColor    = glm::vec3(0.0f);
        float     EmissiveStrength = 0.0f;
    };

    class MaterialAsset : public Asset
    {
    public:
        MaterialAsset() { Type = AssetType::Material; }
        explicit MaterialAsset(const std::string& name);

        PBRProperties& GetProperties() { return m_Properties; }
        const PBRProperties& GetProperties() const { return m_Properties; }

        void SetAlbedoTexture(AssetHandle textureHandle) { m_AlbedoTexture = textureHandle; }
        void SetNormalTexture(AssetHandle textureHandle) { m_NormalTexture = textureHandle; }
        void SetMetallicRoughnessTexture(AssetHandle textureHandle) { m_MetallicRoughnessTexture = textureHandle; }
        void SetAOTexture(AssetHandle textureHandle) { m_AOTexture = textureHandle; }
        void SetEmissiveTexture(AssetHandle textureHandle) { m_EmissiveTexture = textureHandle; }

        AssetHandle GetAlbedoTexture() const { return m_AlbedoTexture; }
        AssetHandle GetNormalTexture() const { return m_NormalTexture; }
        AssetHandle GetMetallicRoughnessTexture() const { return m_MetallicRoughnessTexture; }
        AssetHandle GetAOTexture() const { return m_AOTexture; }
        AssetHandle GetEmissiveTexture() const { return m_EmissiveTexture; }

        void UploadUniforms(const Shared<Shader>& shader) const;
        void BindTextures(const Shared<Shader>& shader) const;

        bool Reload() override;

    private:
        PBRProperties m_Properties;

        // Texture asset handles
        AssetHandle m_AlbedoTexture;
        AssetHandle m_NormalTexture;
        AssetHandle m_MetallicRoughnessTexture;
        AssetHandle m_AOTexture;
        AssetHandle m_EmissiveTexture;
    };
}
