#pragma once
#include "Blu/Core/Core.h"
#include <glm/glm.hpp>
#include <vector>

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
        // editor Rendering panel.
        bool      EnableGodRays   = true;
        float     GodRayIntensity = 0.9f;   // strength of the additive shafts
        glm::vec2 GodRaySunUV     = { 0.5f, 0.5f };
        bool      GodRaySunVisible = false; // sun in front of camera & roughly on-screen

        // Localized fog volumes — composited after SSAO, before tonemap. Set by Scene each
        // frame from FogVolumeComponent. When FogVolumes is empty the pass is skipped entirely
        // (byte-identical to a scene with no volumes). FogInvViewProj/FogCameraPos let the
        // shader reconstruct world position from the depth buffer.
        bool                      EnableFogVolumes = false;
        glm::mat4                 FogInvViewProj   = glm::mat4(1.0f);
        glm::vec3                 FogCameraPos     = glm::vec3(0.0f);
        std::vector<FogVolumeGPU> FogVolumes;
    };
}
