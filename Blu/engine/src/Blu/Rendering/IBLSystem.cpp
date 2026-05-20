#include "Blupch.h"
#include "IBLSystem.h"
#include "TextureCube.h"
#include "Texture.h"
#include "Blu/Core/Log.h"

#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Blu
{
    Shared<TextureCube> IBLSystem::s_IrradianceMap;
    Shared<TextureCube> IBLSystem::s_PrefilterMap;
    Shared<Texture2D>   IBLSystem::s_BRDFLut;
    std::string         IBLSystem::s_HdrPath;

    // ── Float32 → Float16 (no denormals, clamp overflow to Inf) ─────────────────
    static uint16_t F32toF16(float f)
    {
        uint32_t x;
        std::memcpy(&x, &f, 4);
        uint32_t s = (x >> 31) & 1u;
        int e = ((x >> 23) & 0xffu) - 127 + 15;
        uint32_t m = x & 0x7fffffu;
        if (e >= 31) return (uint16_t)((s << 15) | 0x7c00u); // overflow → inf
        if (e <= 0)  return (uint16_t)(s << 15);              // underflow → zero
        return (uint16_t)((s << 15) | ((uint32_t)e << 10) | (m >> 13));
    }

    // Pack a vec3 into four float16s (alpha=1.0) — matches DXGI_FORMAT_R16G16B16A16_FLOAT row
    static void WriteRGBA16F(uint16_t* dst, const glm::vec3& c)
    {
        dst[0] = F32toF16(c.r);
        dst[1] = F32toF16(c.g);
        dst[2] = F32toF16(c.b);
        dst[3] = F32toF16(1.0f);
    }

    // ── Equirectangular sampling ─────────────────────────────────────────────────
    static glm::vec3 SampleEquirect(const float* pixels, int w, int h, const glm::vec3& dir)
    {
        // atan2/acos give phi in [-PI,PI] and theta in [0,PI]
        float phi   = std::atan2(dir.z, dir.x);
        float theta = std::acos(std::max(-1.0f, std::min(1.0f, dir.y)));

        float u = phi   / (glm::two_pi<float>()) + 0.5f;
        float v = theta / glm::pi<float>();

        // Clamp to valid range (no wrap on v since equirect covers [0,PI] exactly)
        u = std::max(0.0f, std::min(1.0f, u));
        v = std::max(0.0f, std::min(1.0f, v));

        // Bilinear sample
        float fx = u * (w - 1);
        float fy = v * (h - 1);
        int   ix = (int)fx, iy = (int)fy;
        float tx = fx - ix, ty = fy - iy;

        int ix1 = std::min(ix + 1, w - 1);
        int iy1 = std::min(iy + 1, h - 1);

        auto px = [&](int x, int y) -> glm::vec3 {
            const float* p = pixels + (y * w + x) * 3;
            return { p[0], p[1], p[2] };
        };

        glm::vec3 c00 = px(ix,  iy),  c10 = px(ix1, iy);
        glm::vec3 c01 = px(ix,  iy1), c11 = px(ix1, iy1);

        return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
    }

    // ── Cubemap face UV → world direction ───────────────────────────────────────
    // (u,v) ∈ [0,1]² on the given face; output is normalised.
    static glm::vec3 FaceUVToDir(int face, float u, float v)
    {
        float s = 2.0f * u - 1.0f; // [-1,1]
        float t = 2.0f * v - 1.0f; // [-1,1]

        switch (face)
        {
        case 0: return glm::normalize(glm::vec3( 1, -t, -s)); // +X
        case 1: return glm::normalize(glm::vec3(-1, -t,  s)); // -X
        case 2: return glm::normalize(glm::vec3( s,  1,  t)); // +Y
        case 3: return glm::normalize(glm::vec3( s, -1, -t)); // -Y
        case 4: return glm::normalize(glm::vec3( s, -t,  1)); // +Z
        case 5: return glm::normalize(glm::vec3(-s, -t, -1)); // -Z
        default: return glm::vec3(0, 1, 0);
        }
    }

    // ── Hammersley low-discrepancy sequence ──────────────────────────────────────
    static glm::vec2 Hammersley(uint32_t i, uint32_t N)
    {
        uint32_t bits = i;
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        float ri = float(bits) * 2.3283064365386963e-10f; // / 2^32
        return { float(i) / float(N), ri };
    }

    // ── GGX importance sampling (tangent space, N = (0,0,1)) ────────────────────
    static glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, const glm::vec3& N, float roughness)
    {
        float a  = roughness * roughness;
        float phi      = glm::two_pi<float>() * Xi.x;
        float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

        glm::vec3 H = { sinTheta * std::cos(phi),
                        sinTheta * std::sin(phi),
                        cosTheta };

        glm::vec3 up    = std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(up, N));
        up = glm::cross(N, right);

        return glm::normalize(right * H.x + up * H.y + N * H.z);
    }

    // ── Smith G term ─────────────────────────────────────────────────────────────
    static float GeometrySchlickGGX(float NdotV, float roughness)
    {
        float k = (roughness * roughness) * 0.5f; // IBL remapping (not direct light)
        return NdotV / (NdotV * (1.0f - k) + k);
    }

    static float GeometrySmith(float NdotV, float NdotL, float roughness)
    {
        return GeometrySchlickGGX(NdotV, roughness)
             * GeometrySchlickGGX(NdotL, roughness);
    }

    // ── Build a cosine-weighted TBN basis around N ────────────────────────────────
    static void BuildTBN(const glm::vec3& N, glm::vec3& right, glm::vec3& up)
    {
        glm::vec3 worldUp = std::abs(N.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        right = glm::normalize(glm::cross(worldUp, N));
        up    = glm::cross(N, right);
    }

    // =============================================================================
    // Public API
    // =============================================================================

    void IBLSystem::Init()
    {
        ComputeBRDFLUT();
    }

    void IBLSystem::Shutdown()
    {
        s_IrradianceMap.reset();
        s_PrefilterMap.reset();
        s_BRDFLut.reset();
    }

    bool IBLSystem::LoadEnvironment(const std::string& hdrPath)
    {
        stbi_set_flip_vertically_on_load(0);
        int w, h, channels;
        float* data = stbi_loadf(hdrPath.c_str(), &w, &h, &channels, 3);
        if (!data)
        {
            BLU_CORE_WARN("IBLSystem: failed to load HDR '{0}'", hdrPath);
            return false;
        }

        BLU_CORE_INFO("IBLSystem: precomputing IBL from '{0}' ({1}x{2})", hdrPath, w, h);
        s_HdrPath = hdrPath;
        BuildFromHDR(data, w, h);
        stbi_image_free(data);
        BLU_CORE_INFO("IBLSystem: done");
        return true;
    }

    void IBLSystem::BindIBL(uint32_t irradianceSlot, uint32_t prefilterSlot, uint32_t brdfSlot)
    {
        if (s_IrradianceMap) s_IrradianceMap->Bind(irradianceSlot);
        if (s_PrefilterMap)  s_PrefilterMap->Bind(prefilterSlot);
        if (s_BRDFLut)       s_BRDFLut->Bind(brdfSlot);
    }

    void IBLSystem::UnbindIBL(uint32_t irradianceSlot, uint32_t prefilterSlot, uint32_t brdfSlot)
    {
        // Unbind by binding null SRVs — done implicitly by DX11 when the next draw binds
        // different resources.  For now just leave the engine to handle this naturally.
        (void)irradianceSlot; (void)prefilterSlot; (void)brdfSlot;
    }

    // =============================================================================
    // Private — convolution
    // =============================================================================

    void IBLSystem::BuildFromHDR(const float* pixels, int w, int h)
    {
        ComputeIrradiance(pixels, w, h);
        ComputePrefilter (pixels, w, h);
    }

    void IBLSystem::ComputeIrradiance(const float* pixels, int w, int h)
    {
        const uint32_t N    = (uint32_t)kIrradianceSamples;
        const uint32_t SIZE = (uint32_t)kIrradianceSize;
        const uint32_t BPP  = 4 * sizeof(uint16_t); // RGBA16F, 4 channels × 2 bytes

        s_IrradianceMap = TextureCube::Create(SIZE, 1);

        std::vector<uint16_t> face(SIZE * SIZE * 4);

        for (int f = 0; f < 6; ++f)
        {
            for (uint32_t y = 0; y < SIZE; ++y)
            {
                for (uint32_t x = 0; x < SIZE; ++x)
                {
                    glm::vec3 N_dir = FaceUVToDir(f,
                        (x + 0.5f) / SIZE,
                        (y + 0.5f) / SIZE);

                    glm::vec3 right, up;
                    BuildTBN(N_dir, right, up);

                    glm::vec3 irradiance(0.0f);

                    for (uint32_t i = 0; i < N; ++i)
                    {
                        glm::vec2 xi = Hammersley(i, N);

                        // Cosine-weighted hemisphere sample
                        float phi      = glm::two_pi<float>() * xi.x;
                        float cosTheta = std::sqrt(1.0f - xi.y);
                        float sinTheta = std::sqrt(xi.y);

                        glm::vec3 wi = right * (sinTheta * std::cos(phi))
                                     + up    * (sinTheta * std::sin(phi))
                                     + N_dir *  cosTheta;

                        irradiance += SampleEquirect(pixels, w, h, glm::normalize(wi));
                    }

                    // PI factor cancels with PDF denominator for cosine-weighted sampling
                    irradiance = irradiance * glm::pi<float>() / float(N);

                    uint16_t* dst = face.data() + (y * SIZE + x) * 4;
                    WriteRGBA16F(dst, irradiance);
                }
            }

            s_IrradianceMap->SetFaceData(f, 0, face.data(), SIZE * BPP);
        }
    }

    void IBLSystem::ComputePrefilter(const float* pixels, int w, int h)
    {
        const uint32_t SIZE     = (uint32_t)kPrefilterSize;
        const uint32_t MIPS     = (uint32_t)kPrefilterMips;
        const uint32_t SAMPLES  = (uint32_t)kPrefilterSamples;
        const uint32_t BPP      = 4 * sizeof(uint16_t);

        s_PrefilterMap = TextureCube::Create(SIZE, MIPS);

        for (uint32_t mip = 0; mip < MIPS; ++mip)
        {
            uint32_t mipSize = SIZE >> mip;
            if (mipSize < 1) mipSize = 1;

            float roughness = float(mip) / float(MIPS - 1);

            std::vector<uint16_t> face(mipSize * mipSize * 4);

            for (int f = 0; f < 6; ++f)
            {
                for (uint32_t y = 0; y < mipSize; ++y)
                {
                    for (uint32_t x = 0; x < mipSize; ++x)
                    {
                        glm::vec3 N = FaceUVToDir(f,
                            (x + 0.5f) / mipSize,
                            (y + 0.5f) / mipSize);

                        // Split-sum approximation: assume V = R = N
                        glm::vec3 V = N;

                        glm::vec3 prefilteredColor(0.0f);
                        float     totalWeight = 0.0f;

                        for (uint32_t i = 0; i < SAMPLES; ++i)
                        {
                            glm::vec2 xi = Hammersley(i, SAMPLES);
                            glm::vec3 H  = ImportanceSampleGGX(xi, N, roughness);
                            glm::vec3 L  = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                            float NdotL = std::max(glm::dot(N, L), 0.0f);
                            if (NdotL > 0.0f)
                            {
                                prefilteredColor += SampleEquirect(pixels, w, h, L) * NdotL;
                                totalWeight      += NdotL;
                            }
                        }

                        glm::vec3 result = prefilteredColor / std::max(totalWeight, 1e-4f);

                        uint16_t* dst = face.data() + (y * mipSize + x) * 4;
                        WriteRGBA16F(dst, result);
                    }
                }

                s_PrefilterMap->SetFaceData(f, (int)mip, face.data(), mipSize * BPP);
            }
        }
    }

    void IBLSystem::ComputeBRDFLUT()
    {
        const uint32_t SIZE    = (uint32_t)kBRDFLUTSize;
        const uint32_t SAMPLES = (uint32_t)kBRDFSamples;

        s_BRDFLut = Texture2D::Create(SIZE, SIZE);

        std::vector<uint8_t> pixels(SIZE * SIZE * 4);

        for (uint32_t y = 0; y < SIZE; ++y)
        {
            // v = roughness (0 at bottom, 1 at top in texture space)
            float roughness = std::max((y + 0.5f) / SIZE, 1e-4f);

            for (uint32_t x = 0; x < SIZE; ++x)
            {
                float NdotV = std::max((x + 0.5f) / SIZE, 1e-4f);

                glm::vec3 V = { std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV };
                glm::vec3 N = { 0.0f, 0.0f, 1.0f };

                float A = 0.0f, B = 0.0f;

                for (uint32_t i = 0; i < SAMPLES; ++i)
                {
                    glm::vec2 xi = Hammersley(i, SAMPLES);
                    glm::vec3 H  = ImportanceSampleGGX(xi, N, roughness);
                    glm::vec3 L  = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                    float NdotL = std::max(L.z, 0.0f);
                    float NdotH = std::max(H.z, 0.0f);
                    float VdotH = std::max(glm::dot(V, H), 0.0f);

                    if (NdotL > 0.0f)
                    {
                        float G    = GeometrySmith(NdotV, NdotL, roughness);
                        float GVis = (G * VdotH) / std::max(NdotH * NdotV, 1e-4f);
                        float Fc   = std::pow(1.0f - VdotH, 5.0f);
                        A += (1.0f - Fc) * GVis;
                        B += Fc          * GVis;
                    }
                }

                A /= float(SAMPLES);
                B /= float(SAMPLES);

                uint8_t* dst  = pixels.data() + (y * SIZE + x) * 4;
                dst[0] = (uint8_t)(std::max(0.0f, std::min(1.0f, A)) * 255.0f);
                dst[1] = (uint8_t)(std::max(0.0f, std::min(1.0f, B)) * 255.0f);
                dst[2] = 0;
                dst[3] = 255;
            }
        }

        s_BRDFLut->SetData(pixels.data(), (uint32_t)pixels.size());
    }
}
