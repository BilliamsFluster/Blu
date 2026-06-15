#include "Blupch.h"
#include "GpuParticleSystem.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer3D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Blu
{
    GpuParticleSystem& GpuParticleSystem::Get()
    {
        static GpuParticleSystem instance;
        return instance;
    }

    float GpuParticleSystem::Rand01()
    {
        // xorshift-ish LCG → [0,1)
        m_Seed = m_Seed * 1664525u + 1013904223u;
        return (float)((m_Seed >> 8) & 0xFFFFFF) / (float)0x1000000;
    }

    void GpuParticleSystem::EnsureResources()
    {
        if (m_Initialized)
            return;
        m_Initialized = true;

        // Build a single-submesh cube model from the primitive cube mesh.
        auto cube = Mesh::CreateCube();
        m_CubeModel = std::make_shared<Model>();
        SubMesh sm;
        sm.VAO = cube->GetVertexArray();
        sm.IndexCount = cube->GetIndexCount();
        sm.MaterialIndex = -1;
        sm.LocalTransform = glm::mat4(1.0f);
        sm.BoundingCenter = cube->GetBoundingCenter();
        sm.BoundingRadius = cube->GetBoundingRadius();
        m_CubeModel->Meshes.push_back(std::move(sm));

        // Warm emissive material so particles glow regardless of scene lighting.
        m_Material = Material::Create();
        m_Material->AlbedoColor = glm::vec4(1.0f, 0.55f, 0.15f, 1.0f);
        m_Material->Metallic = 0.0f;
        m_Material->Roughness = 1.0f;
        m_Material->EmissiveColor = glm::vec3(1.0f, 0.5f, 0.12f);
        m_Material->EmissiveStrength = 5.0f;
    }

    void GpuParticleSystem::Emit(const glm::vec3& pos, int count, const glm::vec3& baseVel, float velSpread,
                                 float life, float sizeBegin, float sizeEnd)
    {
        for (int i = 0; i < count; ++i)
        {
            Particle p;
            p.Pos = pos;
            glm::vec3 jitter(
                (Rand01() * 2.0f - 1.0f) * velSpread,
                (Rand01() * 2.0f - 1.0f) * velSpread,
                (Rand01() * 2.0f - 1.0f) * velSpread);
            p.Vel = baseVel + jitter;
            p.MaxLife = life * (0.6f + 0.4f * Rand01());
            p.Life = p.MaxLife;
            p.SizeBegin = sizeBegin;
            p.SizeEnd = sizeEnd;
            m_Particles.push_back(p);
        }
    }

    void GpuParticleSystem::OnUpdate(float dt)
    {
        const glm::vec3 gravity(0.0f, -1.5f, 0.0f); // gentle settle
        for (auto& p : m_Particles)
        {
            p.Vel += gravity * dt;
            p.Pos += p.Vel * dt;
            p.Life -= dt;
        }
        m_Particles.erase(
            std::remove_if(m_Particles.begin(), m_Particles.end(),
                           [](const Particle& p) { return p.Life <= 0.0f; }),
            m_Particles.end());
    }

    void GpuParticleSystem::Render()
    {
        if (m_Particles.empty())
            return;
        EnsureResources();

        std::vector<glm::mat4> transforms;
        transforms.reserve(m_Particles.size());
        for (const auto& p : m_Particles)
        {
            float t = (p.MaxLife > 0.0f) ? (1.0f - p.Life / p.MaxLife) : 1.0f;
            float size = glm::mix(p.SizeBegin, p.SizeEnd, t);
            transforms.push_back(glm::scale(glm::translate(glm::mat4(1.0f), p.Pos), glm::vec3(size)));
        }
        Renderer3D::DrawMeshInstanced(m_CubeModel, transforms, m_Material.get());
    }

    void GpuParticleSystem::Clear()
    {
        m_Particles.clear();
    }
}
