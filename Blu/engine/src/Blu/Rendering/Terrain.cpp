#include "Blupch.h"
#include "Terrain.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

// stb_image is compiled as part of the engine's ExternalDependencies.
extern "C"
{
    unsigned char* stbi_load(const char* filename, int* x, int* y, int* ch, int desired_ch);
    void           stbi_image_free(void* ptr);
    void           stbi_set_flip_vertically_on_load(int flag);
}

namespace Blu
{
    // Bilinear sample of a greyscale heightmap image (values [0,255] → [0,1]).
    static float SampleHeight(const unsigned char* data, int imgW, int imgH, float u, float v)
    {
        u = glm::clamp(u, 0.0f, 1.0f);
        v = glm::clamp(v, 0.0f, 1.0f);

        float px = u * (imgW - 1);
        float py = v * (imgH - 1);

        int x0 = (int)px, x1 = std::min(x0 + 1, imgW - 1);
        int y0 = (int)py, y1 = std::min(y0 + 1, imgH - 1);
        float fx = px - x0, fy = py - y0;

        auto sample = [&](int x, int y) -> float
        {
            int idx = (y * imgW + x) * 1; // single channel
            return data[idx] / 255.0f;
        };

        return glm::mix(
            glm::mix(sample(x0, y0), sample(x1, y0), fx),
            glm::mix(sample(x0, y1), sample(x1, y1), fx),
            fy);
    }

    float TerrainProceduralHeight(float x, float z, const TerrainSpec& spec)
    {
        if (spec.ProceduralAmplitude <= 0.0f)
            return 0.0f;

        const float f = spec.ProceduralFrequency;
        // Three octaves of smooth, deterministic rolling hills (no RNG).
        float h = std::sin(x * f) * std::cos(z * f);
        h += 0.5f  * std::sin(x * f * 2.13f + 1.7f) * std::cos(z * f * 1.87f + 0.4f);
        h += 0.25f * std::sin(x * f * 4.07f + 3.1f) * std::cos(z * f * 3.71f + 2.2f);
        h = glm::clamp((h + 1.75f) / 3.5f, 0.0f, 1.0f); // → roughly [0,1]

        // Flat central play area, hills ramp in over a 10 m band beyond ProceduralFlatRadius.
        const float r       = std::sqrt(x * x + z * z);
        const float falloff = glm::smoothstep(spec.ProceduralFlatRadius,
                                              spec.ProceduralFlatRadius + 10.0f, r);
        return h * spec.ProceduralAmplitude * falloff;
    }

    TerrainSpec SanitizeTerrainSpec(const TerrainSpec& spec)
    {
        TerrainSpec sanitized = spec;
        sanitized.GridWidth = std::max(1, sanitized.GridWidth);
        sanitized.GridHeight = std::max(1, sanitized.GridHeight);
        sanitized.CellSize = std::max(0.001f, sanitized.CellSize);
        sanitized.HeightScale = std::max(0.0f, sanitized.HeightScale);
        return sanitized;
    }

    TerrainMeshData BuildTerrainMeshData(const TerrainSpec& authoredSpec)
    {
        const TerrainSpec spec = SanitizeTerrainSpec(authoredSpec);
        const int numCols = spec.GridWidth  + 1; // vertex columns
        const int numRows = spec.GridHeight + 1; // vertex rows

        // Load heightmap
        unsigned char* imgData = nullptr;
        int imgW = 0, imgH = 0, imgCh = 0;
        if (!spec.HeightmapPath.empty())
        {
            stbi_set_flip_vertically_on_load(0); // DX11 top-down (consistent with other textures)
            imgData = stbi_load(spec.HeightmapPath.c_str(), &imgW, &imgH, &imgCh, 1); // force greyscale
        }

        const float halfW = spec.GridWidth  * spec.CellSize * 0.5f;
        const float halfH = spec.GridHeight * spec.CellSize * 0.5f;

        // ── Build vertices ────────────────────────────────────────────────────
        std::vector<Vertex3D> vertices;
        vertices.resize(numCols * numRows);

        for (int row = 0; row < numRows; ++row)
        {
            for (int col = 0; col < numCols; ++col)
            {
                float u = (float)col / spec.GridWidth;
                float v = (float)row / spec.GridHeight;

                const float worldX = col * spec.CellSize - halfW;
                const float worldZ = row * spec.CellSize - halfH;

                float height = 0.0f;
                if (imgData)
                    height = SampleHeight(imgData, imgW, imgH, u, v) * spec.HeightScale;
                else
                    height = TerrainProceduralHeight(worldX, worldZ, spec); // 0 unless amplitude set

                auto& vert    = vertices[row * numCols + col];
                vert.Position = { worldX, height, worldZ };
                vert.Normal   = { 0.0f, 1.0f, 0.0f }; // recalculated below
                vert.TexCoord = { u, v };
                vert.Tangent  = { 1.0f, 0.0f, 0.0f }; // recalculated below
            }
        }

        if (imgData) stbi_image_free(imgData);

        // ── Build indices ─────────────────────────────────────────────────────
        std::vector<uint32_t> indices;
        indices.reserve(spec.GridWidth * spec.GridHeight * 6);

        for (int row = 0; row < spec.GridHeight; ++row)
        {
            for (int col = 0; col < spec.GridWidth; ++col)
            {
                uint32_t tl = row * numCols + col;
                uint32_t tr = tl + 1;
                uint32_t bl = (row + 1) * numCols + col;
                uint32_t br = bl + 1;

                indices.push_back(tl); indices.push_back(bl); indices.push_back(tr);
                indices.push_back(tr); indices.push_back(bl); indices.push_back(br);
            }
        }

        // ── Accumulate smooth normals ─────────────────────────────────────────
        for (auto& v : vertices) v.Normal = glm::vec3(0.0f);

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            auto& v0 = vertices[indices[i]];
            auto& v1 = vertices[indices[i + 1]];
            auto& v2 = vertices[indices[i + 2]];
            glm::vec3 n = glm::cross(v1.Position - v0.Position, v2.Position - v0.Position);
            v0.Normal += n;
            v1.Normal += n;
            v2.Normal += n;
        }
        for (auto& v : vertices)
            v.Normal = (glm::length(v.Normal) > 1e-6f)
                ? glm::normalize(v.Normal) : glm::vec3(0.0f, 1.0f, 0.0f);

        // ── Accumulate tangents (for normal map support) ──────────────────────
        for (auto& v : vertices) v.Tangent = glm::vec3(0.0f);

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            auto& v0 = vertices[indices[i]];
            auto& v1 = vertices[indices[i + 1]];
            auto& v2 = vertices[indices[i + 2]];

            glm::vec2 dUV1 = v1.TexCoord - v0.TexCoord;
            glm::vec2 dUV2 = v2.TexCoord - v0.TexCoord;
            glm::vec3 e1   = v1.Position - v0.Position;
            glm::vec3 e2   = v2.Position - v0.Position;

            float denom = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
            if (glm::abs(denom) < 1e-8f) continue;
            float f = 1.0f / denom;

            glm::vec3 t = f * (dUV2.y * e1 - dUV1.y * e2);
            v0.Tangent += t;
            v1.Tangent += t;
            v2.Tangent += t;
        }
        for (auto& v : vertices)
            v.Tangent = (glm::length(v.Tangent) > 1e-6f)
                ? glm::normalize(v.Tangent) : glm::vec3(1.0f, 0.0f, 0.0f);

        return { std::move(vertices), std::move(indices) };
    }

    Shared<Mesh> GenerateTerrain(const TerrainSpec& spec)
    {
        TerrainMeshData data = BuildTerrainMeshData(spec);
        return std::make_shared<Mesh>(data.Vertices, data.Indices);
    }
}
