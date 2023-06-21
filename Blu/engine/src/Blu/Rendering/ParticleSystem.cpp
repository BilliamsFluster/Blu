#include "Blupch.h"
#include "ParticleSystem.h"
#include "Blu/Rendering/Renderer2D.h"



namespace Blu
{
    ParticleSystem::ParticleSystem(uint32_t maxParticles)
        : m_PoolIndex(maxParticles - 1)
    {
        m_ParticlePool.resize(maxParticles);
    }
    void ParticleSystem::OnUpdate(Blu::Timestep ts)
    {
        for (auto& particle : m_ParticlePool)
        {
            if (!particle.Active)
                continue;

            if (particle.LifeRemaining <= 0.0f)
            {
                particle.Active = false;
                continue;
            }

            particle.LifeRemaining -= ts;
            particle.Position += particle.Velocity * (float)ts;
            particle.Rotation += 0.01f;
        }
    }

    void ParticleSystem::OnRender()
    {
        for (auto& particle : m_ParticlePool)
        {
            if (!particle.Active)
                continue;

            float life = particle.LifeRemaining / particle.LifeTime;
            glm::vec4 color = glm::mix(particle.ColorEnd, particle.ColorBegin, life);
            float size = glm::mix(particle.SizeEnd, particle.SizeBegin, life);
            Blu::Renderer2D::DrawRotatedQuad(glm::vec3({ particle.Position, 0.1f }), { size, size }, glm::radians(particle.Rotation), color);

        }
    }

    void ParticleSystem::Emit(const ParticleProps& particleProps)
    {
        Particle& particle = m_ParticlePool[m_PoolIndex];
        particle.Active = true;
        particle.Position = particleProps.Position;
        particle.Velocity = particleProps.Velocity;
        particle.ColorBegin = particleProps.ColorBegin;
        particle.ColorEnd = particleProps.ColorEnd;
        particle.LifeTime = particleProps.LifeTime;
        particle.LifeRemaining = particleProps.LifeTime;
        particle.SizeBegin = particleProps.SizeBegin * (1.0f + particleProps.SizeVariation * (std::rand() / (float)RAND_MAX));
        particle.SizeEnd = particleProps.SizeEnd;

        m_PoolIndex = --m_PoolIndex % m_ParticlePool.size();
    }
}