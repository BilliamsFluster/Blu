#include "Blupch.h"
#include "TimeOfDay.h"
#include "Skybox.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace Blu
{
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
    static glm::vec3 lerp3(const glm::vec3& a, const glm::vec3& b, float t)
    {
        return a + (b - a) * t;
    }

    glm::vec3 TimeOfDayController::SunDirectionFromAngles(float elevDeg, float azimuthDeg)
    {
        float elevRad = glm::radians(elevDeg);
        float azimRad = glm::radians(azimuthDeg);
        // Direction FROM scene TOWARD the sun (same convention as DirLightData::Direction)
        return glm::normalize(glm::vec3(
             std::cos(elevRad) * std::sin(azimRad),
             std::sin(elevRad),
             std::cos(elevRad) * std::cos(azimRad)));
    }

    TimeOfDayController::Output TimeOfDayController::Evaluate(float t) const
    {
        Output out{};

        // Sun follows a full great-circle orbit so it rises on one side and sets on
        // the opposite side.  Using cos(phase) as the horizontal radius is the key:
        // it is +1 at sunrise, 0 at noon, -1 at sunset — flipping the horizontal
        // component to the opposite azimuth at sunset.
        float phase   = (t - 0.25f) * glm::two_pi<float>();
        float azimRad = glm::radians(SunAzimuthDeg);
        out.SunDirection = glm::normalize(glm::vec3(
            std::cos(phase) * std::sin(azimRad),
            std::sin(phase),
            std::cos(phase) * std::cos(azimRad)));
        out.SunElevationDeg = glm::degrees(std::asin(
            glm::clamp(out.SunDirection.y, -1.0f, 1.0f)));

        // Fraction of "day" brightness: 1 at noon, 0 at horizon crossings, 0 at night.
        // SunDirection.y == sin(phase), so reuse it rather than recomputing.
        float dayFrac = std::max(0.0f, out.SunDirection.y);

        // Sunrise/sunset factor: peaks when sun is near the horizon but still above
        float horizonFrac = 1.0f - std::abs(dayFrac - 0.5f) * 2.0f; // 0 at noon/night, 1 at horizon
        horizonFrac = std::max(0.0f, horizonFrac);

        // Sky exposure: bright at noon, tiny floor at night
        // (0.001 = moonlit night floor, 0.025 = noon; Preetham kcd/m² → scene linear)
        out.SkyExposure     = lerp(0.001f, 0.025f, dayFrac);
        out.SunStrength     = SunMaxStrength * dayFrac;
        out.AmbientIntensity= lerp(0.02f, 0.20f, dayFrac);

        // Turbidity: slightly hazier at sunrise/sunset
        out.Turbidity = lerp(SunNoonTurbidity, SunHazeTurbidity, horizonFrac * dayFrac);

        // Aerial / horizon colour transitions:
        //   Day     → blue sky horizon
        //   Sunrise / sunset → warm orange-pink
        //   Night   → dark blue
        static const glm::vec3 kDayBlue    = {0.55f, 0.72f, 0.90f};
        static const glm::vec3 kSunsetWarm = {1.00f, 0.45f, 0.10f};
        static const glm::vec3 kNightBlue  = {0.02f, 0.04f, 0.12f};

        // Blend day ↔ sunset ↔ night
        float sunsetBlend = horizonFrac * dayFrac;                // peaks at sunrise/sunset
        glm::vec3 daySky   = lerp3(kNightBlue, kDayBlue,    dayFrac);
        out.AerialColor    = lerp3(daySky,     kSunsetWarm,  sunsetBlend * 0.8f);
        out.AerialStrength = 0.5f + 0.4f * sunsetBlend;          // stronger at dawn/dusk

        return out;
    }

    void TimeOfDayController::Update(float dt, Skybox& sky, FogSettings& fog,
                                     glm::vec3& outSunDirection, float& outAmbientIntensity)
    {
        if (AutoAdvance && DayDurationSecs > 0.0f)
            NormalizedTime = std::fmod(NormalizedTime + dt / DayDurationSecs, 1.0f);

        Output o = Evaluate(NormalizedTime);

        sky.Turbidity   = o.Turbidity;
        sky.SkyExposure = o.SkyExposure;
        sky.SunStrength = o.SunStrength;

        fog.AerialColor    = o.AerialColor;
        fog.AerialStrength = o.AerialStrength;

        outSunDirection      = o.SunDirection;
        outAmbientIntensity  = o.AmbientIntensity;
    }
}
