#include "Blupch.h"
#include "Renderer3D.h"
#include "Renderer.h"
#include "EditorCamera.h"
#include "Camera.h"
#include "Shader.h"
#include "CascadedShadowMap.h"
#include "IBLSystem.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"

#include "RenderCommand.h"
#include "PipelineState.h"
#include "MaterialSystem.h"
#include "DeferredRenderer.h"
#include "SceneRenderPipeline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Blu
{
    Renderer3D::Renderer3DData* Renderer3D::s_Data3D = nullptr;

    void Renderer3D::Init()
    {
        s_Data3D = new Renderer3DData();
        if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
        {
            Renderer::GetShaderLibrary()->Load("assets/shaders/DX11/PBR_Mesh.hlsl");
            Renderer::GetShaderLibrary()->Load("assets/shaders/DX11/DepthOnly.hlsl");
            Renderer::GetShaderLibrary()->Load("assets/shaders/DX11/Foliage_Instanced.hlsl");
            Renderer::GetShaderLibrary()->Load("assets/shaders/DX11/Skinned_Mesh.hlsl");
        }
        else
        {
            Renderer::GetShaderLibrary()->Load("assets/shaders/Renderer3D_Mesh.glsl");
        }
        s_Data3D->MeshShader          = Renderer::GetShaderLibrary()->Get("PBR_Mesh");
        s_Data3D->DepthOnlyShader     = Renderer::GetShaderLibrary()->Get("DepthOnly");
        s_Data3D->InstancedMeshShader = Renderer::GetShaderLibrary()->Get("Foliage_Instanced");
        s_Data3D->SkinnedMeshShader   = Renderer::GetShaderLibrary()->Get("Skinned_Mesh");
        s_Data3D->CSMInstance         = CascadedShadowMap::Create(2048);
        s_Data3D->Deferred            = DeferredRenderer::Create();

        s_Data3D->MeshShader->Bind();
        s_Data3D->MeshShader->SetUniformInt("u_HasShadowMap", 0);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }

    void Renderer3D::Shutdown()
    {
        delete s_Data3D;
        s_Data3D = nullptr;
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        s_Data3D->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
        s_Data3D->ViewMatrix           = camera.GetViewMatrix();
        s_Data3D->ViewPos              = camera.GetPosition();
        s_Data3D->ViewFrustum.ExtractFromVP(s_Data3D->ViewProjectionMatrix);
    }

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
    {
        s_Data3D->ViewMatrix           = glm::inverse(transform);
        s_Data3D->ViewProjectionMatrix = camera.GetProjectionMatrix() * s_Data3D->ViewMatrix;
        s_Data3D->ViewPos              = glm::vec3(transform[3]);
        s_Data3D->ViewFrustum.ExtractFromVP(s_Data3D->ViewProjectionMatrix);
    }

    void Renderer3D::EndScene()
    {
    }

    struct alignas(16) IBLDataGPU
    {
        int   IBLEnabled;
        float IBLStrength;
        int   IBLMipLevels;
        float _pad;
    };
    static_assert(sizeof(IBLDataGPU) == 16, "IBLDataGPU must be 16 bytes");

    void Renderer3D::SetIBL(bool enabled, float strength)
    {
        s_Data3D->IBLEnabled  = enabled;
        s_Data3D->IBLStrength = strength;
    }

    void Renderer3D::SetFog(const FogSettings& fog)
    {
        s_Data3D->Fog = fog;
        auto& sh = *s_Data3D->MeshShader;
        sh.Bind();
        sh.SetUniformInt   ("u_FogEnabled",      fog.Enabled ? 1 : 0);
        sh.SetUniformFloat3("u_FogColor",        fog.Color);
        sh.SetUniformFloat ("u_FogDensity",      fog.Density);
        sh.SetUniformFloat ("u_FogHeightStart",  fog.HeightStart);
        sh.SetUniformFloat ("u_FogHeightDensity",fog.HeightDensity);
        sh.SetUniformFloat3("u_AerialColor",     fog.AerialColor);
        sh.SetUniformFloat ("u_AerialStrength",  fog.AerialStrength);
        sh.Flush();
        sh.UnBind();
    }

    // -------------------------------------------------------------------------
    // PassLights — upload all lights as a single cbuffer blob (no string allocs).
    // -------------------------------------------------------------------------
    void Renderer3D::PassLights(
        const std::vector<DirLightData>&   dirLights,
        const std::vector<PointLightData>& pointLights,
        const std::vector<SpotLightData>&  spotLights)
    {
        auto& sh = *s_Data3D->MeshShader;
        const LightDataGPU gpu = BuildLightDataGPU(dirLights, pointLights, spotLights);
        s_Data3D->Lights = gpu;

        // Single bulk upload — no string allocations, one memcpy into cbuffer shadow.
        sh.SetUniformBuffer("LightData", &gpu, sizeof(gpu));

        // View position is a separate PerFrame cbuffer member, still uploaded via
        // the normal path (single float3, negligible cost).
        sh.SetUniformFloat3("u_ViewPos", s_Data3D->ViewPos);
    }

    void Renderer3D::SetLights(
        const std::vector<DirLightData>&   dirLights,
        const std::vector<PointLightData>& pointLights,
        const std::vector<SpotLightData>&  spotLights)
    {
        s_Data3D->MeshShader->Bind();
        PassLights(dirLights, pointLights, spotLights);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MeshDrawCall — internal per-submesh record used for blend-mode sorting.
    // Collected during DrawMesh, flushed to GPU in Renderer3D::FlushDrawCalls.
    // ─────────────────────────────────────────────────────────────────────────
    struct MeshDrawCall
    {
        glm::mat4            Transform;
        glm::mat4            NormalMatrix;
        const Material*      Mat;
        Shared<VertexArray>  VAO;
        uint32_t             IndexCount;
        int                  EntityID;
        float                DistToCamera; // world-space distance, for back-to-front sort
    };

    static std::vector<MeshDrawCall> s_OpaqueDrawCalls;
    static std::vector<MeshDrawCall> s_TransparentDrawCalls;
    struct SkinnedDrawCall
    {
        glm::mat4 Transform;
        MeshComponent* Mesh = nullptr;
        std::vector<glm::mat4> BoneMatrices;
        int EntityID = -1;
    };
    static std::vector<SkinnedDrawCall> s_DeferredSkinnedDrawCalls;

    // Resolve the effective material for a given submesh index
    static const Material& ResolveMaterial(const MeshComponent& mc, int materialIndex)
    {
        static const Material s_DefaultMaterial;
        if (mc.ModelAsset && materialIndex >= 0 && materialIndex < (int)mc.ModelAsset->Materials.size())
        {
            const auto& m = mc.ModelAsset->Materials[materialIndex];
            if (m) return *m;
        }
        return mc.MaterialInstance ? *mc.MaterialInstance : s_DefaultMaterial;
    }

    // Issue a single draw call with the correct pipeline state already bound
    static void IssueDrawCall(Shader& sh, const MeshDrawCall& dc)
    {
        sh.SetUniformMat4("u_Model", dc.Transform);
        sh.SetUniformMat3("u_NormalMatrix", dc.NormalMatrix);
        sh.SetUniformInt("u_EntityID", dc.EntityID);
        dc.Mat->Bind(sh);
        sh.Flush();
        dc.VAO->Bind();
        RenderCommand::DrawIndexed(dc.VAO, dc.IndexCount);
    }


    // Choose and bind the correct pipeline state for a material
    static MaterialRenderContext GetMaterialRenderContext(bool skinned = false, bool foliage = false)
    {
        return { RenderSettings::GetPath(), skinned, foliage };
    }

    static bool IsTransparentMaterial(const Material& mat, const MaterialRenderContext& context = GetMaterialRenderContext())
    {
        const ResolvedMaterial resolved = MaterialResolver::Get().ResolveLegacy(mat, context);
        return resolved.Blend == BlendMode::Transparent || resolved.Blend == BlendMode::Additive;
    }

    static void BindPipelineForMaterial(const Material& mat, const MaterialRenderContext& context = GetMaterialRenderContext())
    {
        const ResolvedMaterial resolved = MaterialResolver::Get().ResolveLegacy(mat, context);
        switch (resolved.Blend)
        {
        case BlendMode::Transparent:
            PipelineStateCache::GetTransparent()->Bind();
            break;
        case BlendMode::Additive:
            PipelineStateCache::GetAdditiveBlend()->Bind();
            break;
        default:
            // Opaque or Masked — use CullNone for TwoSided, Back-cull otherwise
            if (resolved.TwoSided)
                PipelineStateCache::GetCullNone()->Bind();
            else
                PipelineStateCache::GetOpaque()->Bind();
            break;
        }
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, MeshComponent& mc, int entityID)
    {
        if (!mc.MeshData && !mc.ModelAsset) return;

        if (mc.ModelAsset)
        {
            for (auto& submesh : mc.ModelAsset->Meshes)
            {
                glm::mat4 submeshWorld = transform * submesh.LocalTransform;

                glm::vec3 worldCenter = glm::vec3(submeshWorld * glm::vec4(submesh.BoundingCenter, 1.0f));
                float worldRadius = submesh.BoundingRadius * std::max({
                    glm::length(glm::vec3(submeshWorld[0])),
                    glm::length(glm::vec3(submeshWorld[1])),
                    glm::length(glm::vec3(submeshWorld[2]))});
                if (!s_Data3D->ViewFrustum.TestSphere(worldCenter, worldRadius))
                    continue;

                const Material& mat = ResolveMaterial(mc, submesh.MaterialIndex);

                MeshDrawCall dc;
                dc.Transform    = submeshWorld;
                dc.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(submeshWorld)));
                dc.Mat          = &mat;
                dc.VAO          = submesh.VAO;
                dc.IndexCount   = submesh.IndexCount;
                dc.EntityID     = entityID;
                dc.DistToCamera = glm::length(worldCenter - s_Data3D->ViewPos);

                if (IsTransparentMaterial(mat))
                    s_TransparentDrawCalls.push_back(dc);
                else
                    s_OpaqueDrawCalls.push_back(dc);
            }
        }
        else if (mc.MeshData)
        {
            // Frustum-cull the primitive against its local bounding sphere
            // (transformed to world space). Matches the Model path above so
            // ground planes, cubes and terrain stop drawing when off-screen.
            glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(mc.MeshData->GetBoundingCenter(), 1.0f));
            float worldRadius = mc.MeshData->GetBoundingRadius() * std::max({
                glm::length(glm::vec3(transform[0])),
                glm::length(glm::vec3(transform[1])),
                glm::length(glm::vec3(transform[2]))});
            if (worldRadius > 0.0f && !s_Data3D->ViewFrustum.TestSphere(worldCenter, worldRadius))
                return;

            const Material& mat = mc.MaterialInstance ? *mc.MaterialInstance
                                                       : ResolveMaterial(mc, -1);

            glm::vec3 worldPos = glm::vec3(transform[3]);
            MeshDrawCall dc;
            dc.Transform    = transform;
            dc.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
            dc.Mat          = &mat;
            dc.VAO          = mc.MeshData->GetVertexArray();
            dc.IndexCount   = mc.MeshData->GetIndexCount();
            dc.EntityID     = entityID;
            dc.DistToCamera = glm::length(worldPos - s_Data3D->ViewPos);

            if (IsTransparentMaterial(mat))
                s_TransparentDrawCalls.push_back(dc);
            else
                s_OpaqueDrawCalls.push_back(dc);
        }
    }

    void Renderer3D::FlushDrawCalls()
    {
        IBLDataGPU iblGPU;
        iblGPU.IBLEnabled  = (s_Data3D->IBLEnabled && IBLSystem::IsReady()) ? 1 : 0;
        iblGPU.IBLStrength = s_Data3D->IBLStrength;
        iblGPU.IBLMipLevels = IBLSystem::GetPrefilterMips();
        iblGPU._pad        = 0.0f;

        std::sort(s_OpaqueDrawCalls.begin(), s_OpaqueDrawCalls.end(),
            [](const MeshDrawCall& a, const MeshDrawCall& b) {
                return a.DistToCamera < b.DistToCamera;
            });

        bool usedDeferred = false;
        const SceneRenderPipelinePlan plan =
            BuildSceneRenderPipelinePlan(RenderSettings::GetPath(), RendererAPI::GetAPI());
        if (plan.UsesDeferred() && s_Data3D->Deferred && s_Data3D->Deferred->BeginGeometryPass())
        {
            auto geometryShader = s_Data3D->Deferred->GetGeometryShader();
            if (geometryShader)
            {
                geometryShader->Bind();
                geometryShader->SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
                const MaterialRenderContext geometryContext { RenderPath::Deferred, false, false };
                const Material* lastGeometryMaterial = nullptr;
                for (const auto& dc : s_OpaqueDrawCalls)
                {
                    if (dc.Mat != lastGeometryMaterial)
                    {
                        BindPipelineForMaterial(*dc.Mat, geometryContext);
                        lastGeometryMaterial = dc.Mat;
                    }
                    IssueDrawCall(*geometryShader, dc);
                }
                geometryShader->UnBind();

                DeferredLightingData lighting;
                lighting.Lights = s_Data3D->Lights;
                lighting.Shadows = s_Data3D->Shadows;
                lighting.ViewPosition = s_Data3D->ViewPos;
                lighting.FogColor = s_Data3D->Fog.Color;
                lighting.FogDensity = s_Data3D->Fog.Density;
                lighting.FogHeightStart = s_Data3D->Fog.HeightStart;
                lighting.FogHeightDensity = s_Data3D->Fog.HeightDensity;
                lighting.FogEnabled = s_Data3D->Fog.Enabled ? 1 : 0;
                lighting.AerialColor = s_Data3D->Fog.AerialColor;
                lighting.AerialStrength = s_Data3D->Fog.AerialStrength;
                lighting.HasShadowMap = s_Data3D->HasShadowMap ? 1 : 0;
                lighting.IBLEnabled = iblGPU.IBLEnabled;
                lighting.IBLStrength = iblGPU.IBLStrength;
                lighting.IBLMipLevels = iblGPU.IBLMipLevels;
                s_Data3D->Deferred->SubmitLightingPass(lighting);
                usedDeferred = true;
            }
        }

        for (auto& dc : s_DeferredSkinnedDrawCalls)
            DrawSkinnedMeshForward(dc.Transform, *dc.Mesh, dc.BoneMatrices, dc.EntityID);
        s_DeferredSkinnedDrawCalls.clear();

        auto& sh = *s_Data3D->MeshShader;
        sh.Bind();
        sh.SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
        sh.SetUniformBuffer("IBLData", &iblGPU, sizeof(iblGPU));
        sh.Flush();
        if (iblGPU.IBLEnabled)
            IBLSystem::BindIBL(6, 7, 8);

        const Material* lastMat = nullptr;
        if (!usedDeferred)
        {
            for (const auto& dc : s_OpaqueDrawCalls)
            {
                if (dc.Mat != lastMat)
                {
                    BindPipelineForMaterial(*dc.Mat);
                    lastMat = dc.Mat;
                }
                IssueDrawCall(sh, dc);
            }
        }

        std::sort(s_TransparentDrawCalls.begin(), s_TransparentDrawCalls.end(),
            [](const MeshDrawCall& a, const MeshDrawCall& b) {
                return a.DistToCamera > b.DistToCamera;
            });

        lastMat = nullptr;
        for (const auto& dc : s_TransparentDrawCalls)
        {
            if (dc.Mat != lastMat)
            {
                BindPipelineForMaterial(*dc.Mat);
                lastMat = dc.Mat;
            }
            IssueDrawCall(sh, dc);
        }

        s_OpaqueDrawCalls.clear();
        s_TransparentDrawCalls.clear();

        PipelineStateCache::GetOpaque()->Bind();
        sh.UnBind();
    }

    // ─── CSM shadow pass ────────────────────────────────────────────────────

    void Renderer3D::BeginCSMPass(int cascadeIndex, const glm::mat4& lightVP)
    {
        s_Data3D->CSMInstance->BindCascadeForWriting(cascadeIndex);
        PipelineStateCache::GetShadowMap()->Bind();
        s_Data3D->DepthOnlyShader->Bind();
        s_Data3D->DepthOnlyShader->SetUniformMat4("u_LightVP", lightVP);
        s_Data3D->DepthOnlyShader->Flush();
    }

    void Renderer3D::EndCSMPass()
    {
        s_Data3D->DepthOnlyShader->UnBind();
        PipelineStateCache::GetOpaque()->Bind();
        // Do not restore render target yet — caller loops over cascades.
    }

    void Renderer3D::DrawMeshShadow(const glm::mat4& transform, MeshComponent& mc)
    {
        if (!mc.MeshData && !mc.ModelAsset) return;

        if (mc.ModelAsset)
        {
            for (auto& submesh : mc.ModelAsset->Meshes)
            {
                s_Data3D->DepthOnlyShader->SetUniformMat4("u_Model", transform * submesh.LocalTransform);
                s_Data3D->DepthOnlyShader->Flush();
                submesh.VAO->Bind();
                RenderCommand::DrawIndexed(submesh.VAO, submesh.IndexCount);
            }
        }
        else if (mc.MeshData)
        {
            s_Data3D->DepthOnlyShader->SetUniformMat4("u_Model", transform);
            s_Data3D->DepthOnlyShader->Flush();
            mc.MeshData->GetVertexArray()->Bind();
            RenderCommand::DrawIndexed(mc.MeshData->GetVertexArray(), mc.MeshData->GetIndexCount());
        }
    }

    // ─── Skinned mesh draw ────────────────────────────────────────────────────

    // BoneData cbuffer: 128 mat4s (8192 bytes).  Matches Skinned_Mesh.hlsl b6.
    struct alignas(16) BoneDataGPU
    {
        glm::mat4 Bones[128]; // 128 * 64 = 8 192 bytes
    };
    static_assert(sizeof(BoneDataGPU) == 8192, "BoneDataGPU size mismatch");

    void Renderer3D::DrawSkinnedMesh(const glm::mat4& transform, MeshComponent& mc,
                                     const std::vector<glm::mat4>& boneMatrices,
                                     int entityID)
    {
        const SceneRenderPipelinePlan plan =
            BuildSceneRenderPipelinePlan(RenderSettings::GetPath(), RendererAPI::GetAPI());
        if (plan.UsesDeferred() && s_Data3D->Deferred)
        {
            s_DeferredSkinnedDrawCalls.push_back({ transform, &mc, boneMatrices, entityID });
            return;
        }
        DrawSkinnedMeshForward(transform, mc, boneMatrices, entityID);
    }

    void Renderer3D::DrawSkinnedMeshForward(const glm::mat4& transform, MeshComponent& mc,
                                            const std::vector<glm::mat4>& boneMatrices,
                                            int entityID)
    {
        if (!mc.ModelAsset || !mc.ModelAsset->HasSkeleton()) return;
        if (!s_Data3D->SkinnedMeshShader) return;

        auto& sh = *s_Data3D->SkinnedMeshShader;
        sh.Bind();
        sh.SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
        sh.SetUniformFloat3("u_ViewPos", s_Data3D->ViewPos);
        sh.SetUniformMat4("u_Model", transform);

        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(transform)));
        sh.SetUniformMat3("u_NormalMatrix", normalMat);
        sh.SetUniformInt("u_EntityID", entityID);

        // Upload bone matrices
        BoneDataGPU boneGPU = {};
        int count = std::min((int)boneMatrices.size(), 128);
        for (int i = 0; i < count; ++i)
            boneGPU.Bones[i] = boneMatrices[i];
        sh.SetUniformBuffer("BoneData", &boneGPU, sizeof(boneGPU));

        // IBL (b7 in Skinned_Mesh.hlsl, same GPU struct)
        IBLDataGPU iblGPU;
        iblGPU.IBLEnabled   = (s_Data3D->IBLEnabled && IBLSystem::IsReady()) ? 1 : 0;
        iblGPU.IBLStrength  = s_Data3D->IBLStrength;
        iblGPU.IBLMipLevels = IBLSystem::GetPrefilterMips();
        iblGPU._pad         = 0.0f;
        sh.SetUniformBuffer("IBLData", &iblGPU, sizeof(iblGPU));
        sh.Flush();

        if (iblGPU.IBLEnabled)
            IBLSystem::BindIBL(6, 7, 8);

        PipelineStateCache::GetOpaque()->Bind();

        for (auto& submesh : mc.ModelAsset->SkinnedMeshes)
        {
            const Material& mat = (submesh.MaterialIndex >= 0 &&
                submesh.MaterialIndex < (int)mc.ModelAsset->Materials.size() &&
                mc.ModelAsset->Materials[submesh.MaterialIndex])
                ? *mc.ModelAsset->Materials[submesh.MaterialIndex]
                : (mc.MaterialInstance ? *mc.MaterialInstance : Material{});

            BindPipelineForMaterial(mat, { RenderPath::Forward, true, false });
            mat.Bind(sh);
            sh.Flush();
            submesh.VAO->Bind();
            RenderCommand::DrawIndexed(submesh.VAO, submesh.IndexCount);
        }

        sh.UnBind();
    }

    // ─── Instanced draw support ───────────────────────────────────────────────

    // Matches Foliage_Instanced.hlsl InstanceData cbuffer (b1) exactly.
    struct alignas(16) InstanceDataGPU
    {
        glm::mat4 Transforms[256]; // 256 * 64 = 16 384 bytes
    };
    static_assert(sizeof(InstanceDataGPU) == 16384, "InstanceDataGPU size mismatch");

    // Matches Foliage_Instanced.hlsl WindData cbuffer (b5).
    struct alignas(16) WindDataGPU
    {
        glm::vec3 Direction; float Strength;   // 16
        float     Frequency; float Time;
        glm::vec2 _pad;                        // 8 → 32 total
    };
    static_assert(sizeof(WindDataGPU) == 32, "WindDataGPU size mismatch");

    void Renderer3D::DrawMeshInstanced(const Shared<Model>& model,
        const std::vector<glm::mat4>& transforms,
                                       const Material* overrideMat,
                                       FoliageWindSettings wind)
    {
        if (!model || model->Meshes.empty() || transforms.empty()) return;
        if (!s_Data3D->InstancedMeshShader) return;
        auto& sh = *s_Data3D->InstancedMeshShader;

        sh.Bind();
        sh.SetUniformMat4("u_ViewProjectionMatrix", s_Data3D->ViewProjectionMatrix);
        sh.SetUniformFloat3("u_ViewPos", s_Data3D->ViewPos);
        WindDataGPU windGPU = {};
        windGPU.Direction = wind.Enabled ? wind.Direction : glm::vec3(0.0f);
        windGPU.Strength = wind.Enabled ? wind.Strength : 0.0f;
        windGPU.Frequency = wind.Frequency;
        windGPU.Time = wind.Time;
        sh.SetUniformBuffer("WindData", &windGPU, sizeof(windGPU));
        sh.Flush();

        PipelineStateCache::GetCullNone()->Bind(); // two-sided for foliage leaves

        const int total = (int)transforms.size();
        for (int batchStart = 0; batchStart < total; batchStart += kMaxInstances)
        {
            int batchCount = std::min(kMaxInstances, total - batchStart);

            InstanceDataGPU instGPU = {};
            for (int i = 0; i < batchCount; ++i)
                instGPU.Transforms[i] = transforms[batchStart + i];

            sh.SetUniformBuffer("InstanceData", &instGPU, sizeof(instGPU));

            for (const auto& submesh : model->Meshes)
            {
                const Material& mat = overrideMat ? *overrideMat
                    : (submesh.MaterialIndex >= 0 && submesh.MaterialIndex < (int)model->Materials.size()
                        && model->Materials[submesh.MaterialIndex]
                        ? *model->Materials[submesh.MaterialIndex]
                        : Material{});

                (void)MaterialResolver::Get().ResolveLegacy(mat, GetMaterialRenderContext(false, true));
                mat.Bind(sh);
                sh.Flush();
                RenderCommand::DrawIndexedInstanced(submesh.VAO, submesh.IndexCount, (uint32_t)batchCount);
            }
        }

        PipelineStateCache::GetOpaque()->Bind();
        sh.UnBind();
    }

    void Renderer3D::BindCSM(const glm::mat4 lightVPs[CascadedShadowMap::NUM_CASCADES],
                              const glm::vec3& cascadeSplits)
    {
        ShadowDataGPU gpuData = {};
        for (int i = 0; i < CascadedShadowMap::NUM_CASCADES; ++i)
            gpuData.LightVPs[i] = lightVPs[i];
        gpuData.CascadeSplits = cascadeSplits;
        gpuData.ShadowMapSize = static_cast<float>(s_Data3D->CSMInstance->GetSize());
        s_Data3D->Shadows = gpuData;
        s_Data3D->HasShadowMap = true;

        s_Data3D->MeshShader->Bind();
        s_Data3D->MeshShader->SetUniformInt("u_HasShadowMap", 1);
        s_Data3D->MeshShader->SetUniformBuffer("ShadowData", &gpuData, sizeof(gpuData));
        s_Data3D->CSMInstance->BindTexture(5);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }

    void Renderer3D::SetShadowsEnabled(bool enabled)
    {
        s_Data3D->HasShadowMap = enabled;
        s_Data3D->MeshShader->Bind();
        s_Data3D->MeshShader->SetUniformInt("u_HasShadowMap", enabled ? 1 : 0);
        s_Data3D->MeshShader->Flush();
        s_Data3D->MeshShader->UnBind();
    }
}
