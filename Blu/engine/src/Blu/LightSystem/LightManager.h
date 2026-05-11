#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{
    // LightManager is now a thin helper.  Actual light data is collected
    // directly from the ECS each frame in Scene::OnUpdateEditor/Runtime via
    // GatherLights() — no stale entity references.
    class LightManager
    {
    public:
        LightManager()  = default;
        ~LightManager() = default;

        // Legacy stubs kept for call-sites that haven't been removed yet.
        // They are no-ops: lights are discovered from the ECS automatically.
        void AddPointLight(class Entity&)       {}
        void AddDirectionalLight(class Entity&) {}
        void AddSpotLight(class Entity&)        {}

        void UpdateLights() {}
        void RenderLights() {}
    };
}
