#include "Blupch.h"
#include "AssetManager.h"
#include "Blu/Core/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Blu
{
    AssetManager& AssetManager::Get()
    {
        static AssetManager instance;
        return instance;
    }

    void AssetManager::Initialize()
    {
        if (m_Initialized) return;
        LoadAssetRegistry();
        m_Initialized = true;
        BLU_CORE_INFO("AssetManager initialized");
    }

    void AssetManager::Shutdown()
    {
        m_Assets.clear();
        m_PathIndex.clear();
        m_Initialized = false;
        BLU_CORE_INFO("AssetManager shutdown");
    }

    Shared<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        auto it = m_Assets.find(handle);
        if (it != m_Assets.end())
        {
            it->second->ReferenceCount++;
            return it->second;
        }
        return nullptr;
    }

    bool AssetManager::HasAsset(AssetHandle handle) const
    {
        return m_Assets.find(handle) != m_Assets.end();
    }

    AssetHandle AssetManager::ImportAsset(const std::string& filepath)
    {
        // Check if already imported
        std::string normalized = std::filesystem::path(filepath).lexically_normal().string();
        auto pathIt = m_PathIndex.find(normalized);
        if (pathIt != m_PathIndex.end())
        {
            BLU_CORE_INFO("AssetManager: asset already imported: {0}", normalized);
            return pathIt->second;
        }

        BLU_CORE_INFO("AssetManager: import not yet implemented for: {0}", normalized);
        return AssetHandle(0);
    }

    bool AssetManager::SaveAsset(AssetHandle handle)
    {
        BLU_CORE_INFO("AssetManager: SaveAsset not yet implemented");
        return false;
    }

    void AssetManager::LoadAssetRegistry()
    {
        BLU_CORE_INFO("AssetManager: loading asset registry");
    }
}
