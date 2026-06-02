#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Blu/Core/Core.h"
#include "Mesh.h"
#include "Frustum.h"
#include "CascadedShadowMap.h"
#include "Animation.h"
#include "LightBufferData.h"

namespace Blu
{
    class EditorCamera;
    class Camera;
    struct MeshComponent;
    class CascadedShadowMap;
    class DeferredRenderer;

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

    struct FogSettings
    {
        glm::vec3 Color          = {0.50f, 0.55f, 0.62f};
        float     Density        = 0.0f;
        float     HeightStart    = 0.0f;  // world-Y below which fog is at max density
        float     HeightDensity  = 0.0f;  // how quickly fog thins above HeightStart
        bool      Enabled        = false;
        // Aerial perspective: fog colour blends toward sky horizon colour at distance.
        // Eliminates the flat white/grey horizon band when fog colour doesn't match sky.
        glm::vec3 AerialColor    = {0.55f, 0.73f, 0.90f}; // set to match Skybox horizon
        float     AerialStrength = 0.0f;   // 0 = off; 1 = full sky-colour bleed at horizon
    };

    struct FoliageWindSettings
    {
        bool Enabled = true;
        glm::vec3 Direction = { 1.0f, 0.0f, 0.0f };
        float Strength = 0.05f;
        float Frequency = 1.5f;
        float Time = 0.0f;
    };

    class Renderer3D
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void EndScene();

        static void DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID = -1);

        // GPU-instanced draw for identical meshes (foliage, rocks, etc.).
        // Splits into batches of kMaxInstances internally.
        static void DrawMeshInstanced(const Shared<Model>& model,
                                      const std::vector<glm::mat4>& transforms,
                                      const Material* overrideMat = nullptr,
                                      FoliageWindSettings wind = {});

        // Skinned mesh draw — uses Skinned_Mesh.hlsl with bone matrix cbuffer.
        static void DrawSkinnedMesh(const glm::mat4& transform, MeshComponent& mc,
                                    const std::vector<glm::mat4>& boneMatrices,
                                    int entityID = -1);

        // Flush all DrawMesh calls collected this frame: sort by blend mode and
        // distance, bind appropriate pipeline states, then issue GPU draw calls.
        // Must be called once after all DrawMesh calls, before EndScene.
        static void FlushDrawCalls();

        static void SetLights(const std::vector<DirLightData>&   dirLights,
                              const std::vector<PointLightData>& pointLights,
                              const std::vector<SpotLightData>&  spotLights);

        static void SetFog(const FogSettings& fog);

        // Toggle IBL and set strength multiplier (uploaded to shaders on next FlushDrawCalls).
        static void SetIBL(bool enabled, float strength = 1.0f);

        // Cascaded shadow mapping — one cascade rendered at a time.
        static void BeginCSMPass(int cascadeIndex, const glm::mat4& lightVP);
        static void EndCSMPass();
        static void DrawMeshShadow(const glm::mat4& transform, MeshComponent& mc);

        // Upload all cascade matrices + splits to the mesh shader, bind the CSM array texture.
        static void BindCSM(const glm::mat4 lightVPs[CascadedShadowMap::NUM_CASCADES],
                            const glm::vec3& cascadeSplits);
        static void SetShadowsEnabled(bool enabled);

        static Shared<CascadedShadowMap> GetCSM() { return s_Data3D->CSMInstance; }

    private:
        static void PassLights(const std::vector<DirLightData>&   dirLights,
                               const std::vector<PointLightData>& pointLights,
                               const std::vector<SpotLightData>&  spotLights);
        static void DrawSkinnedMeshForward(const glm::mat4& transform, MeshComponent& mc,
                                           const std::vector<glm::mat4>& boneMatrices,
                                           int entityID);

        static constexpr int kMaxInstances = 256; // max per-batch for cbuffer packing

        struct Renderer3DData
        {
            Shared<class Shader>       MeshShader;
            Shared<class Shader>       DepthOnlyShader;
            Shared<class Shader>       InstancedMeshShader;
            Shared<class Shader>       SkinnedMeshShader;
            Shared<CascadedShadowMap>  CSMInstance;
            Unique<DeferredRenderer>    Deferred;
            glm::mat4                  ViewProjectionMatrix = glm::mat4(1.0f);
            glm::mat4                  ViewMatrix           = glm::mat4(1.0f);
            glm::vec3                  ViewPos              = glm::vec3(0.0f);
            Frustum                    ViewFrustum;
            LightDataGPU               Lights = {};
            ShadowDataGPU              Shadows = {};
            FogSettings                Fog;
            bool                       IBLEnabled  = false;
            bool                       HasShadowMap = false;
            float                      IBLStrength = 1.0f;
        };
        static Renderer3DData* s_Data3D;
    };
}
