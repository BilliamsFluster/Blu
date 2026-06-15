#pragma once
#include "Asset.h"
#include "Blu/Core/Core.h"

namespace Blu
{
    struct Model;

    // A handle-addressable static mesh asset. The actual geometry (LoadedModel) is
    // populated LAZILY by AssetManager::LoadModel(): ModelLoader::Load performs GPU
    // buffer uploads, which require a live graphics device, so resolving a handle
    // (AssetManager::Load) only records type/path — the upload is deferred until the
    // model is genuinely needed at runtime. This keeps handle resolution usable in
    // headless/tooling contexts that have no renderer.
    class StaticMeshAsset : public Asset
    {
    public:
        StaticMeshAsset() { Type = AssetType::StaticMesh; }
        explicit StaticMeshAsset(const std::string& filepath)
            : Asset(AssetType::StaticMesh, filepath) {}

        Shared<Model> LoadedModel; // null until AssetManager::LoadModel resolves it

        // Re-runs the importer on FilePath (used by reimport). Needs a graphics device.
        bool Reload() override;
    };
}
