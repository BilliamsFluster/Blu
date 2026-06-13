#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include "Blu/Core/Core.h"

namespace Blu
{
    struct Model;
    class Material;

    // Lightweight instanced particle system: a CPU-simulated pool of points rendered as
    // emissive instanced cubes via Renderer3D::DrawMeshInstanced (256/batch). Used for
    // muzzle sparks, bullet impacts, and ambient embers. (CPU sim + GPU instancing now;
    // a compute-driven sim is a later upgrade.)
    class GpuParticleSystem
    {
    public:
        static GpuParticleSystem& Get();

        // Spawn `count` particles at `pos` with velocity `baseVel` jittered by `velSpread`.
        void Emit(const glm::vec3& pos, int count, const glm::vec3& baseVel, float velSpread,
                  float life, float sizeBegin, float sizeEnd);

        void OnUpdate(float dt);
        void Render();                 // requires Renderer3D::BeginScene to have run this frame
        void Clear();
        size_t AliveCount() const { return m_Particles.size(); }

    private:
        struct Particle
        {
            glm::vec3 Pos;
            glm::vec3 Vel;
            float     Life;
            float     MaxLife;
            float     SizeBegin;
            float     SizeEnd;
        };

        void EnsureResources();
        float Rand01(); // deterministic LCG (engine forbids Math::random in some contexts)

        std::vector<Particle> m_Particles;
        Shared<Model>         m_CubeModel;
        Shared<Material>      m_Material;
        uint32_t              m_Seed = 0x1234567u;
        bool                  m_Initialized = false;
    };
}
