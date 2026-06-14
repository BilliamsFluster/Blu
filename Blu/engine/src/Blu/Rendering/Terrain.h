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
        std::string HeightmapPath; // empty = flat terrain (unless procedural below)

        // Procedural rolling hills — used when HeightmapPath is empty and amplitude > 0.
        // Deterministic (no RNG), so the same surface can be queried at runtime to place
        // foliage/objects on it. The central disc of ProceduralFlatRadius is kept flat so
        // gameplay spaces stay level while hills frame the edges.
        float ProceduralAmplitude  = 0.0f;  // peak hill height in metres (0 = disabled)
        float ProceduralFrequency  = 0.08f; // spatial frequency of the hills
        float ProceduralFlatRadius = 0.0f;  // world radius from origin kept perfectly flat
    };

    struct TerrainMeshData
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
    };

    // Keeps authored and loaded terrain values within the generator's valid range.
    TerrainSpec SanitizeTerrainSpec(const TerrainSpec& spec);

    // Deterministic procedural terrain height at a world XZ position (metres). Returns 0
    // when ProceduralAmplitude <= 0 or inside ProceduralFlatRadius. Used by the mesh
    // generator and by gameplay/foliage to conform objects to the rolling surface.
    float TerrainProceduralHeight(float worldX, float worldZ, const TerrainSpec& spec);

    // CPU-only geometry build used by tooling and tests without a graphics context.
    TerrainMeshData BuildTerrainMeshData(const TerrainSpec& spec);

    // Generates a terrain Mesh from a TerrainSpec.
    // If HeightmapPath is non-empty the image is sampled (greyscale average per pixel,
    // bilinearly filtered to the grid resolution). Normals and tangents are computed.
    Shared<Mesh> GenerateTerrain(const TerrainSpec& spec);
}
