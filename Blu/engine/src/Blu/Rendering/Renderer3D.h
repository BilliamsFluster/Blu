#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Blu/Core/Core.h"
#include "Mesh.h"

namespace Blu
{
    class EditorCamera;
    class Camera;
    struct MeshComponent;

    // ─────────────────────────────────────────────────────────────────────────
    // POD light descriptors assembled each frame directly from the ECS.
    // These mirror the HLSL / GLSL struct layouts in Renderer3D_Mesh.*
    // ─────────────────────────────────────────────────────────────────────────

    struct DirLightData
    {
        glm::vec3 Direction;   // normalised world-space direction (toward light)
        glm::vec3 Ambient;
        glm::vec3 Diffuse;
        glm::vec3 Specular;
        float     Intensity = 1.0f;
    };

    struct PointLightData
    {
        glm::vec3 Position;
        glm::vec3 Ambient;
        glm::vec3 Diffuse;
        glm::vec3 Specular;
        float     Intensity    = 1.0f;
        float     Range        = 20.0f;
        float     AttConstant  = 1.0f;
        float     AttLinear    = 0.09f;
        float     AttQuadratic = 0.032f;
    };

    struct SpotLightData
    {
        glm::vec3 Position;
        glm::vec3 Direction;       // normalised
        glm::vec3 Ambient;
        glm::vec3 Diffuse;
        glm::vec3 Specular;
        float     Intensity       = 1.0f;
        float     Range           = 30.0f;
        float     InnerCutoffCos  = 0.0f;  // cos(InnerConeAngle)
        float     OuterCutoffCos  = 0.0f;  // cos(OuterConeAngle)
        float     AttConstant     = 1.0f;
        float     AttLinear       = 0.09f;
        float     AttQuadratic    = 0.032f;
    };

    // ─────────────────────────────────────────────────────────────────────────

    class Renderer3D
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void EndScene();

        static void DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID = -1);

        static void SetLights(const std::vector<DirLightData>&   dirLights,
                              const std::vector<PointLightData>& pointLights,
                              const std::vector<SpotLightData>&  spotLights);

    private:
        static void PassLights(const std::vector<DirLightData>&   dirLights,
                               const std::vector<PointLightData>& pointLights,
                               const std::vector<SpotLightData>&  spotLights);
    };
}
