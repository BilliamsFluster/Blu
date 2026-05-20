#pragma once
#include "Blu/Core/Core.h"
#include "Asset.h"
#include <unordered_map>

namespace Blu
{
    class AssetManager
    {
    public:
        static AssetManager& Get();

        void Initialize();
        void Shutdown();

        Shared<Asset> GetAsset(AssetHandle handle);
        bool HasAsset(AssetHandle handle) const;

        AssetHandle ImportAsset(const std::string& filepath);
        bool SaveAsset(AssetHandle handle);

        template<typename T>
        Shared<T> GetAssetAs(AssetHandle handle)
        {
            static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");
            return std::static_pointer_cast<T>(GetAsset(handle));
        }

        const std::unordered_map<AssetHandle, Shared<Asset>>& GetAllAssets() const { return m_Assets; }
        const std::unordered_map<std::string, AssetHandle>& GetPathIndex() const { return m_PathIndex; }

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        void LoadAssetRegistry();

        std::unordered_map<AssetHandle, Shared<Asset>> m_Assets;
        std::unordered_map<std::string, AssetHandle>   m_PathIndex;
        bool m_Initialized = false;
    };
}
