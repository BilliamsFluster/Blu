#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{
    class LightManager
    {
    public:
        LightManager();
        ~LightManager();

        // Add methods to create and manage different types of lights (point lights, directional lights, etc.)
        void AddPointLight(Shared<class Scene> scene, class Entity& light);
        void AddDirectionalLight(Shared<class Scene> scene, Entity& light);
        // ...

        // Update and render methods for the lights
        void UpdateLights();
        void RenderLights();

        std::vector<Entity> GetPointLights() { return m_PointLights; }

    private:
        std::vector<Entity> m_PointLights;
        std::vector<Entity> m_DirectionalLights;
        friend class SceneHierarchyPanel;
    };

}