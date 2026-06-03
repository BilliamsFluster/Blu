#pragma once
#include "Blu/Core/Core.h"
#include "Mesh.h"
#include <string>
#include <vector>

namespace Blu
{
    struct TerrainSpec
    {
        int   GridWidth   = 64;    // quads along X
        int   GridHeight  = 64;    // quads along Z
        float CellSize    = 2.0f;  // world units per quad
        float HeightScale = 30.0f; // max height displacement from heightmap
        std::string HeightmapPath; // empty = flat terrain
    };

    struct TerrainMeshData
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
    };

    // Keeps authored and loaded terrain values within the generator's valid range.
    TerrainSpec SanitizeTerrainSpec(const TerrainSpec& spec);

    // CPU-only geometry build used by tooling and tests without a graphics context.
    TerrainMeshData BuildTerrainMeshData(const TerrainSpec& spec);

    // Generates a terrain Mesh from a TerrainSpec.
    // If HeightmapPath is non-empty the image is sampled (greyscale average per pixel,
    // bilinearly filtered to the grid resolution). Normals and tangents are computed.
    Shared<Mesh> GenerateTerrain(const TerrainSpec& spec);
}
