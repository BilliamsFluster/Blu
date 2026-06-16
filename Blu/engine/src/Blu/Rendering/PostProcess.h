#pragma once
#include "Blu/Core/Core.h"
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace Blu
{
    class FrameBuffer;
    class Shader;

    // One localized fog region, packed for the fog-volume composite pass. Scene fills these
    // each frame from FogVolumeComponent + the entity transform; the renderer uploads them to
    // PostProcess_FogVolume.hlsl. Layout matches the HLSL FogVolume struct (3 x float4).
    struct FogVolumeGPU
    {
        glm::vec3 Position = glm::vec3(0.0f); int   Shape   = 0;    // 0 = box, 1 = sphere
        glm::vec3 Extents  = glm::vec3(5.0f); float Density = 0.25f;// box half-extents; sphere radius in .x
        glm::vec3 Color    = glm::vec3(0.6f); float Falloff = 1.0f;
    };

    // One projected decal, packed for the decal composite pass. InvWorld maps world->decal-local
    // (unit box centred at origin); a surface pixel is decaled when its local position is inside
    // [-0.5,0.5]^3. Layout matches the HLSL Decal struct (float4x4 + 2 x float4 = 96 bytes).
    struct DecalGPU
    {
        glm::mat4 InvWorld = glm::mat4(1.0f);
        glm::vec3 Color    = glm::vec3(0.35f, 0.03f, 0.03f); float Opacity = 0.85f;
        float     Falloff  = 0.55f; float _pad0 = 0.0f; float _pad1 = 0.0f; float _pad2 = 0.0f;
    };

    // The decal composite pass uploads at most this many decals (matches the HLSL MAX_DECALS and the
    // D3D11 cbuffer array). Defined once here so the gather and the GPU pass agree on the cap.
    static constexpr size_t kMaxDecals = 32;

    // Trims `decals` (and the parallel `worldPositions`) in place to the `maxCount` entries nearest
    // to `camPos` (by squared distance). No-op when already within the cap. Lets the gather honor the
    // GPU decal cap by keeping the most relevant (closest) impacts instead of an arbitrary
    // ECS-iteration-order subset — so bullet holes / blood around the player always render once a
    // firefight pushes the decal count past the cap.
    inline void SelectNearestDecals(std::vector<DecalGPU>& decals,
                                    std::vector<glm::vec3>& worldPositions,
                                    const glm::vec3& camPos, size_t maxCount)
    {
        if (decals.size() <= maxCount || decals.size() != worldPositions.size())
            return;
        std::vector<size_t> order(decals.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        auto distSq = [&](size_t i) { glm::vec3 d = worldPositions[i] - camPos; return glm::dot(d, d); };
        std::nth_element(order.begin(), order.begin() + maxCount, order.end(),
            [&](size_t a, size_t b) { return distSq(a) < distSq(b); });
        order.resize(maxCount);
        std::vector<DecalGPU>  keptDecals;    keptDecals.reserve(maxCount);
        std::vector<glm::vec3> keptPositions; keptPositions.reserve(maxCount);
        for (size_t i : order) { keptDecals.push_back(decals[i]); keptPositions.push_back(worldPositions[i]); }
        decals.swap(keptDecals);
        worldPositions.swap(keptPositions);
    }

    class PostProcess
    {
    public:
        enum class PreviewMode
        {
            Full = 0,
            TonemapOnly,
            BloomOnly,
            FXAAOnly,
            SSAOOnly,
            Bypass
        };

        virtual ~PostProcess() = default;

        static Shared<PostProcess> Create(uint32_t width, uint32_t height);

        // Bind the HDR scene framebuffer so the renderer draws into it.
        virtual void Begin() = 0;

        // Run the full post-process stack (bloom → tonemap → FXAA) and blit to backbuffer.
        virtual void Submit(Shared<Shader> shader = nullptr) = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual Shared<FrameBuffer> GetFrameBuffer() = 0;

        // ── Settings ─────────────────────────────────────────────────────────────
        bool  EnableBloom     = true;
        float BloomThreshold  = 1.0f;
        float BloomStrength   = 0.05f;

        bool  EnableFXAA      = true;
        PreviewMode Preview   = PreviewMode::Full;

        // SSAO
        bool  EnableSSAO     = true;
        float SSAORadius     = 0.5f;
        float SSAOBias       = 0.025f;
        float SSAOPower      = 2.0f;
        int   SSAOSamples    = 16;
        float SSAOStrength   = 1.0f;

        // Camera matrices — set by Scene before Submit so SSAO can reconstruct depth
        glm::mat4 SSAOProjection    = glm::mat4(1.0f);
        glm::mat4 SSAOInvProjection = glm::mat4(1.0f);

        // God rays (screen-space radial light shafts from the sun). Scene::Render3DPass projects
        // the sun to screen UV and sets GodRaySunVisible; the composite shader marches toward it
        // only when the sun is actually on-screen — so this is a no-op (zero cost, no visual
        // change) whenever you're not looking toward the sun. Toggle/intensity exposed in the
        // editor Rendering panel. GodRaySunFade [0..1] smoothly eases the shafts in/out as the
        // sun nears a screen edge or goes behind the camera (no hard brightness pop).
        bool      EnableGodRays   = true;
        float     GodRayIntensity = 0.6f;   // strength of the additive shafts
        glm::vec2 GodRaySunUV     = { 0.5f, 0.5f };
        float     GodRaySunFade   = 0.0f;   // 1 when the sun is well on-screen, 0 at edges/behind camera

        // Localized fog volumes — composited after SSAO, before tonemap. Set by Scene each
        // frame from FogVolumeComponent. When FogVolumes is empty the pass is skipped entirely
        // (byte-identical to a scene with no volumes). FogInvViewProj/FogCameraPos let the
        // shader reconstruct world position from the depth buffer.
        bool                      EnableFogVolumes = false;
        glm::mat4                 FogInvViewProj   = glm::mat4(1.0f); // scene clip->world (also used by decals)
        glm::vec3                 FogCameraPos     = glm::vec3(0.0f);
        std::vector<FogVolumeGPU> FogVolumes;

        // Projected decals — composited after fog volumes, before tonemap, via the same depth
        // reconstruction (uses FogInvViewProj). Skipped entirely (byte-identical) when empty.
        bool                  EnableDecals = false;
        std::vector<DecalGPU> Decals;
    };
}
