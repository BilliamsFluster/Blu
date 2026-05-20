#pragma once
#include <glm/glm.hpp>
#include "Renderer3D.h"  // FogSettings

namespace Blu
{
    class Skybox;

    // Drives sun direction, sky exposure, and fog aerial colour based on a
    // normalised time value [0, 1] where 0/1 = midnight, 0.25 = sunrise,
    // 0.5 = noon, 0.75 = sunset.  Push the output back into the scene's
    // Skybox and FogSettings each frame; the ToD object itself owns no GPU state.
    class TimeOfDayController
    {
    public:
        // ── Time ───────────────────────────────────────────────────────────────
        float NormalizedTime  = 0.5f;   // [0, 1]
        bool  AutoAdvance     = false;
        float DayDurationSecs = 600.0f; // 10 real minutes = 1 in-game day

        // ── Sun placement ─────────────────────────────────────────────────────
        float SunAzimuthDeg   = 45.0f;  // compass direction the sun travels across (0=N, 90=E)
        float SunMaxStrength  = 5.0f;   // sun disk brightness (linear, not scaled by SkyExposure)
        float SunNoonTurbidity= 2.0f;   // Preetham turbidity at noon (clear sky)
        float SunHazeTurbidity= 3.5f;   // turbidity at sunrise / sunset

        // ── Advance time by dt and push params into sky + fog ─────────────────
        void Update(float dt, Skybox& sky, FogSettings& fog,
                    glm::vec3& outSunDirection, float& outAmbientIntensity);

        // ── Compute output for a given t (does not advance m_Time) ────────────
        struct Output
        {
            glm::vec3 SunDirection;
            float     SunElevationDeg;
            float     SkyExposure;
            float     Turbidity;
            float     SunStrength;
            float     AmbientIntensity;   // for DirLightData.Ambient multiplier
            glm::vec3 AerialColor;        // push into FogSettings.AerialColor
            float     AerialStrength;
        };

        Output Evaluate(float t) const;

    private:
        static glm::vec3 SunDirectionFromAngles(float elevDeg, float azimuthDeg);
    };
}
