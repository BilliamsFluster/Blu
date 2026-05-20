#include "Blupch.h"
#include "MaterialAsset.h"
#include "AssetManager.h"
#include "Blu/Core/Log.h"

namespace Blu
{
    MaterialAsset::MaterialAsset(const std::string& name)
        : Asset(AssetType::Material, name)
    {
        FilePath = name;
        IsLoaded = true;
    }

    void MaterialAsset::UploadUniforms(const Shared<Shader>& shader) const
    {
        if (!shader) return;

        shader->SetUniformFloat4("u_AlbedoColor",       m_Properties.AlbedoColor);
        shader->SetUniformFloat ("u_Metallic",          m_Properties.Metallic);
        shader->SetUniformFloat ("u_Roughness",         m_Properties.Roughness);
        shader->SetUniformFloat ("u_AO",                m_Properties.AO);
        shader->SetUniformFloat3("u_EmissiveColor",     m_Properties.EmissiveColor);
        shader->SetUniformFloat ("u_EmissiveStrength",  m_Properties.EmissiveStrength);
        // AlphaCutoff and ShadingModel are not exposed on MaterialAsset; upload safe defaults
        shader->SetUniformFloat ("u_AlphaCutoff",       0.0f);
        shader->SetUniformInt   ("u_ShadingModel",      0);    // PBR
    }

    void MaterialAsset::BindTextures(const Shared<Shader>& shader) const
    {
        if (!shader) return;

        auto& assetManager = AssetManager::Get();

        auto bindTexture = [&](AssetHandle handle, int slot, const char* uniformName, const char* hasUniform)
        {
            if (handle)
            {
                auto texAsset = assetManager.GetAssetAs<TextureAsset>(handle);
                if (texAsset && texAsset->GetTexture())
                {
                    texAsset->GetTexture()->Bind(slot);
                    shader->SetUniformInt(uniformName, slot);
                    shader->SetUniformInt(hasUniform, 1);
                    return;
                }
            }
            shader->SetUniformInt(hasUniform, 0);
        };

        bindTexture(m_AlbedoTexture,          0, "u_AlbedoTexture",          "u_HasAlbedoMap");
        bindTexture(m_NormalTexture,          1, "u_NormalTexture",          "u_HasNormalMap");
        bindTexture(m_MetallicRoughnessTexture, 2, "u_MetallicRoughnessTexture", "u_HasMetallicRoughnessMap");
        bindTexture(m_AOTexture,              3, "u_AOTexture",              "u_HasAOMap");
        bindTexture(m_EmissiveTexture,        4, "u_EmissiveTexture",        "u_HasEmissiveMap");
    }

    bool MaterialAsset::Reload()
    {
        // Re-import from source file (re-parse assimp material, etc.)
        BLU_CORE_INFO("MaterialAsset::Reload not fully implemented");
        IsLoaded = true;
        return true;
    }
}
