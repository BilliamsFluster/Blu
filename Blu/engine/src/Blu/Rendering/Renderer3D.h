#pragma once
#include <glm/glm.hpp>
#include "Blu/Core/Core.h"
#include "Mesh.h"

namespace Blu
{
    class EditorCamera;
    class Camera;
    class LightManager;
    struct MeshComponent;

    class Renderer3D
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void EndScene();

        static void DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID = -1);
        static void SetLights(Shared<LightManager> lightManager);

    private:
        static void PassLights(Shared<LightManager> lightManager);
    };
}
