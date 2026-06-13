#pragma once
#include "Asset.h"
#include <cstdint>
#include <string>

namespace Blu
{
    // Per-type import settings persisted in an asset's .meta sidecar. These are the
    // knobs the editor's import dialog (Phase 7) edits and the importer (MeshImporter)
    // consumes; they live here so a reimport reproduces the original import exactly.
    struct MeshImportSettings
    {
        float Scale = 1.0f;
        bool  GenerateLODs = false;
        int   LODCount = 3;
        bool  ImportMaterials = true;
    };

    struct TextureImportSettings
    {
        bool SRGB = true;
        bool GenerateMips = true;
    };

    // The on-disk record that pins an imported source file to a STABLE AssetHandle
    // (UUID). Written next to the source as "<source>.meta". Because the UUID lives
    // with the source rather than only in the registry, asset references survive a
    // lost/rebuilt registry, a moved project, and sharing across machines.
    struct AssetMeta
    {
        AssetHandle Handle = AssetHandle(0);
        AssetType   Type = AssetType::None;
        std::string SourcePath;          // virtual path to the source file
        uint64_t    SourceMTime = 0;     // last_write_time tick count (reimport change-detection)
        uint64_t    SourceSize = 0;      // bytes (reimport change-detection)

        MeshImportSettings    Mesh;
        TextureImportSettings Texture;

        bool IsValid() const { return (uint64_t)Handle != 0; }
    };

    // Reads/writes the "<source>.meta" YAML sidecar through the FileSystemService so it
    // honors virtual mounts (project://, cache://). All operations are best-effort:
    // Read returns false when the sidecar is absent or malformed; Write returns false
    // when the destination cannot be written. Callers must tolerate failure.
    class AssetMetaIO
    {
    public:
        // Appends ".meta" to the source's (normalized/virtual) path.
        static std::string MetaPathFor(const std::string& sourceVirtualPath);

        static bool Read(const std::string& sourceVirtualPath, AssetMeta& outMeta);
        static bool Write(const std::string& sourceVirtualPath, const AssetMeta& meta);

        // Stamps SourceMTime/SourceSize from the resolved source file (best-effort; on
        // failure the fields are left untouched). Used to detect when a reimport is due.
        static void StampSourceInfo(const std::string& sourceVirtualPath, AssetMeta& meta);
    };
}
