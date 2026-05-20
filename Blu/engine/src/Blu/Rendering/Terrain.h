#pragma once
#include "Blu/Core/Core.h"
#include "Mesh.h"
#include <string>

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

    // Generates a terrain Mesh from a TerrainSpec.
    // If HeightmapPath is non-empty the image is sampled (greyscale average per pixel,
    // bilinearly filtered to the grid resolution). Normals and tangents are computed.
    Shared<Mesh> GenerateTerrain(const TerrainSpec& spec);
}
