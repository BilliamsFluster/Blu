#pragma once
#include <glm/glm.hpp>
#include "Blu/Core/Core.h"

namespace Blu
{
    class Shader;
    class PipelineState;

    // Procedural sky rendered as a full-screen triangle after all opaque geometry.
    // Depth test = LESS_EQUAL with depth write off so it only fills unfilled pixels.
    // The sky is reconstructed from the view ray via InvView * InvProjection.
    class Skybox
    {
    public:
        Skybox();
        ~Skybox() = default;

        // Call after opaque geometry FlushDrawCalls, before EndScene.
        // sunDirection: world-space vector pointing FROM the scene TOWARD the sun.
        // time: elapsed seconds (used to animate cloud scrolling).
        void Render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                    const glm::vec3& sunDirection, float time = 0.0f);

        // ── Physical sky parameters ───────────────────────────────────────────
        // Preetham analytical sky model — replaces the old lerp-based gradient.
        glm::vec3 GroundColor  = {0.22f, 0.18f, 0.14f}; // colour below horizon
        float     Turbidity    = 2.5f;   // 1.8–10; Preetham model invalid below ~1.8
        float     SkyExposure  = 0.025f; // Preetham kcd/m² → scene linear; ~0.025 gives a rich blue noon sky
        glm::vec3 SunColor     = {1.00f, 0.95f, 0.80f};
        float     SunSize      = 0.9995f; // cos half-angle (larger value = smaller disk)
        float     SunStrength  = 3.0f;    // sun disk brightness (linear, not scaled by SkyExposure)

        // ── Procedural clouds ─────────────────────────────────────────────────
        glm::vec3 CloudColor       = {0.92f, 0.93f, 0.97f};
        float     CloudCoverage    = 0.45f;  // 0 = clear sky, 1 = fully overcast
        float     CloudDensity     = 0.0f;   // 0 = off; ramp up to ~0.9 for thick clouds
        float     CloudSoftness    = 0.35f;  // soft edge width for the coverage threshold
        float     CloudHeight      = 600.0f; // world-space Y of the cloud plane
        float     CloudScale       = 900.0f; // XZ world units covered by one noise tile
        glm::vec2 CloudWindDirection = {1.0f, 0.35f};
        float     CloudScrollSpeed = 0.015f; // UV units per second (wind speed)
        float     CloudShadowing    = 0.65f; // self-shadow contrast
        float     CloudHorizonFade  = 0.22f; // hides plane flattening at horizon

    private:
        Shared<Shader>        m_Shader;
        Shared<PipelineState> m_PipelineState;
        bool                  m_Ready = false;
    };
}
