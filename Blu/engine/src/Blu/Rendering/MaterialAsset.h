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

        // Material metadata (blend/shading/two-sided/alpha-cutoff). Storing these makes a
        // MaterialAsset a LOSSLESS mirror of a concrete Material, not just its scalar PBR subset —
        // required so a .blumat can reproduce the exact runtime material the renderer binds.
        void         SetBlendMode(BlendMode blend)       { m_Blend = blend; }
        void         SetShadingModel(ShadingModel model) { m_Shading = model; }
        void         SetTwoSided(bool twoSided)          { m_TwoSided = twoSided; }
        void         SetAlphaCutoff(float cutoff)        { m_AlphaCutoff = cutoff; }
        BlendMode    GetBlendMode() const                { return m_Blend; }
        ShadingModel GetShadingModel() const             { return m_Shading; }
        bool         GetTwoSided() const                 { return m_TwoSided; }
        float        GetAlphaCutoff() const              { return m_AlphaCutoff; }

        // Lossless conversion to/from the concrete runtime Material the renderer binds. Scalar PBR
        // plus blend/shading/two-sided/alpha-cutoff round-trip EXACTLY. ToMaterial resolves texture
        // handles to GPU textures via the AssetManager (best-effort; a zero/missing handle leaves
        // the slot empty). FromMaterial captures scalars + metadata; deriving texture *handles* from
        // a Material's raw Texture2D pointers is deferred (needs an AssetManager reverse lookup).
        static MaterialAsset FromMaterial(const Material& material);
        Shared<Material> ToMaterial() const;

        void UploadUniforms(const Shared<Shader>& shader) const;
        void BindTextures(const Shared<Shader>& shader) const;

        // .blumat persistence: PBR properties + texture asset handles as YAML, written
        // and read through the FileSystemService (honors project:// mounts). Pure data —
        // no GPU device required.
        bool SaveToFile(const std::string& virtualPath) const;
        bool LoadFromFile(const std::string& virtualPath);

        bool Reload() override;

        // Resident-size estimate for the AssetManager memory budget. A material is small (scalar
        // params + texture handles); the referenced textures are separate assets with their own size.
        size_t GetMemoryUsage() const override { return sizeof(PBRProperties) + 5 * sizeof(AssetHandle) + 256; }

    private:
        PBRProperties m_Properties;

        // Render-state metadata (mirrors the concrete Material fields outside PBRProperties).
        BlendMode    m_Blend       = BlendMode::Opaque;
        ShadingModel m_Shading     = ShadingModel::PBR;
        bool         m_TwoSided    = false;
        float        m_AlphaCutoff = 0.5f;

        // Texture asset handles
        AssetHandle m_AlbedoTexture;
        AssetHandle m_NormalTexture;
        AssetHandle m_MetallicRoughnessTexture;
        AssetHandle m_AOTexture;
        AssetHandle m_EmissiveTexture;
    };
}
