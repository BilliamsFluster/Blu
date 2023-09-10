#pragma once

namespace Blu
{
    class LightManager
    {
    public:
        LightManager();
        ~LightManager();

        // Add methods to create and manage different types of lights (point lights, directional lights, etc.)
        void AddPointLight(const struct PointLightComponent& light);
        void AddDirectionalLight(const struct DirectionalLightComponent& light);
        // ...

        // Update and render methods for the lights
        void UpdateLights();
        void RenderLights();

    private:
        std::vector<PointLightComponent> pointLights;
        std::vector<DirectionalLightComponent> directionalLights;
        friend class SceneHierarchyPanel;
    };

}