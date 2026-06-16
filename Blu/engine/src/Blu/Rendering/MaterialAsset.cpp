#include "Blupch.h"
#include "MaterialAsset.h"
#include "AssetManager.h"
#include "Blu/Core/Log.h"
#include "Blu/Utils/FileSystemService.h"
#include "yaml-cpp/yaml.h"

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
        shader->SetUniformFloat ("u_AlphaCutoff",       m_AlphaCutoff);
        shader->SetUniformInt   ("u_ShadingModel",      (int)m_Shading);
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

    bool MaterialAsset::SaveToFile(const std::string& virtualPath) const
    {
        if (virtualPath.empty())
            return false;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "MaterialAsset" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "AlbedoColor" << YAML::Value << YAML::Flow << YAML::BeginSeq
            << m_Properties.AlbedoColor.r << m_Properties.AlbedoColor.g
            << m_Properties.AlbedoColor.b << m_Properties.AlbedoColor.a << YAML::EndSeq;
        out << YAML::Key << "Metallic" << YAML::Value << m_Properties.Metallic;
        out << YAML::Key << "Roughness" << YAML::Value << m_Properties.Roughness;
        out << YAML::Key << "AO" << YAML::Value << m_Properties.AO;
        out << YAML::Key << "EmissiveColor" << YAML::Value << YAML::Flow << YAML::BeginSeq
            << m_Properties.EmissiveColor.r << m_Properties.EmissiveColor.g
            << m_Properties.EmissiveColor.b << YAML::EndSeq;
        out << YAML::Key << "EmissiveStrength" << YAML::Value << m_Properties.EmissiveStrength;
        out << YAML::Key << "BlendMode" << YAML::Value << (int)m_Blend;
        out << YAML::Key << "ShadingModel" << YAML::Value << (int)m_Shading;
        out << YAML::Key << "TwoSided" << YAML::Value << m_TwoSided;
        out << YAML::Key << "AlphaCutoff" << YAML::Value << m_AlphaCutoff;
        out << YAML::Key << "AlbedoTexture" << YAML::Value << (uint64_t)m_AlbedoTexture;
        out << YAML::Key << "NormalTexture" << YAML::Value << (uint64_t)m_NormalTexture;
        out << YAML::Key << "MetallicRoughnessTexture" << YAML::Value << (uint64_t)m_MetallicRoughnessTexture;
        out << YAML::Key << "AOTexture" << YAML::Value << (uint64_t)m_AOTexture;
        out << YAML::Key << "EmissiveTexture" << YAML::Value << (uint64_t)m_EmissiveTexture;
        out << YAML::EndMap; // MaterialAsset
        out << YAML::EndMap;

        return FileSystemService::Get().Write(virtualPath, out.c_str());
    }

    bool MaterialAsset::LoadFromFile(const std::string& virtualPath)
    {
        if (virtualPath.empty())
            return false;

        std::string contents;
        if (!FileSystemService::Get().Read(virtualPath, contents) || contents.empty())
            return false;

        try
        {
            const YAML::Node root = YAML::Load(contents);
            const YAML::Node node = root["MaterialAsset"];
            if (!node)
                return false;

            auto readVec = [](const YAML::Node& n, int count, float* out)
            {
                if (n && n.IsSequence() && (int)n.size() >= count)
                    for (int i = 0; i < count; ++i) out[i] = n[i].as<float>(out[i]);
            };
            readVec(node["AlbedoColor"], 4, &m_Properties.AlbedoColor.x);
            readVec(node["EmissiveColor"], 3, &m_Properties.EmissiveColor.x);
            m_Properties.Metallic = node["Metallic"].as<float>(m_Properties.Metallic);
            m_Properties.Roughness = node["Roughness"].as<float>(m_Properties.Roughness);
            m_Properties.AO = node["AO"].as<float>(m_Properties.AO);
            m_Properties.EmissiveStrength = node["EmissiveStrength"].as<float>(m_Properties.EmissiveStrength);
            // Render-state metadata (defaults preserve back-compat with older .blumat files).
            m_Blend       = static_cast<BlendMode>(node["BlendMode"].as<int>((int)m_Blend));
            m_Shading     = static_cast<ShadingModel>(node["ShadingModel"].as<int>((int)m_Shading));
            m_TwoSided    = node["TwoSided"].as<bool>(m_TwoSided);
            m_AlphaCutoff = node["AlphaCutoff"].as<float>(m_AlphaCutoff);
            m_AlbedoTexture = AssetHandle(node["AlbedoTexture"].as<uint64_t>(0));
            m_NormalTexture = AssetHandle(node["NormalTexture"].as<uint64_t>(0));
            m_MetallicRoughnessTexture = AssetHandle(node["MetallicRoughnessTexture"].as<uint64_t>(0));
            m_AOTexture = AssetHandle(node["AOTexture"].as<uint64_t>(0));
            m_EmissiveTexture = AssetHandle(node["EmissiveTexture"].as<uint64_t>(0));
            FilePath = virtualPath;
            IsLoaded = true;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    MaterialAsset MaterialAsset::FromMaterial(const Material& material)
    {
        MaterialAsset asset;
        asset.m_Properties.AlbedoColor      = material.AlbedoColor;
        asset.m_Properties.Metallic         = material.Metallic;
        asset.m_Properties.Roughness        = material.Roughness;
        asset.m_Properties.AO               = material.AO;
        asset.m_Properties.EmissiveColor    = material.EmissiveColor;
        asset.m_Properties.EmissiveStrength = material.EmissiveStrength;
        asset.m_Blend       = material.Blend;
        asset.m_Shading     = material.Shading;
        asset.m_TwoSided    = material.TwoSided;
        asset.m_AlphaCutoff = material.AlphaCutoff;
        // Capturing texture *handles* from the Material's raw Texture2D pointers needs an
        // AssetManager reverse lookup (deferred); scalar + metadata capture is exact.
        return asset;
    }

    Shared<Material> MaterialAsset::ToMaterial() const
    {
        Shared<Material> material = Material::Create();
        material->AlbedoColor       = m_Properties.AlbedoColor;
        material->Metallic          = m_Properties.Metallic;
        material->Roughness         = m_Properties.Roughness;
        material->AO                = m_Properties.AO;
        material->EmissiveColor     = m_Properties.EmissiveColor;
        material->EmissiveStrength  = m_Properties.EmissiveStrength;
        material->Blend             = m_Blend;
        material->Shading           = m_Shading;
        material->TwoSided          = m_TwoSided;
        material->AlphaCutoff       = m_AlphaCutoff;

        // Resolve referenced texture assets to GPU textures (best-effort: a zero or unresolved
        // handle simply leaves the slot empty, matching an untextured material).
        auto& assetManager = AssetManager::Get();
        auto resolve = [&](AssetHandle handle) -> Shared<Texture2D>
        {
            if (!handle) return nullptr;
            auto texAsset = assetManager.GetAssetAs<TextureAsset>(handle);
            return texAsset ? texAsset->GetTexture() : nullptr;
        };
        material->AlbedoMap            = resolve(m_AlbedoTexture);
        material->NormalMap            = resolve(m_NormalTexture);
        material->MetallicRoughnessMap = resolve(m_MetallicRoughnessTexture);
        material->AOMap                = resolve(m_AOTexture);
        material->EmissiveMap          = resolve(m_EmissiveTexture);
        return material;
    }

    bool MaterialAsset::Reload()
    {
        // Reload PBR properties + texture handles from the .blumat source.
        return LoadFromFile(FilePath);
    }
}
