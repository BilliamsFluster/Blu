#include "Blupch.h"
#include "ParticleSystem.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/constants.hpp>




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
            Blu::Renderer2D::DrawRotatedQuad(particle.Position, { size, size }, glm::radians(particle.Rotation).z, color);

        }
    }
    void ParticleSystem::EmitExplosion(const ParticleProps& particleProps, uint32_t count, float speed)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            ParticleProps p = particleProps;  // Copy the properties

            // Generate a random direction (normalized)
            float direction = ((float)rand() / (RAND_MAX)) * 2.0f * glm::pi<float>();
            glm::vec2 directionVector(cos(direction), sin(direction));

            // Set velocity based on direction and speed
            p.Velocity = glm::vec3{ directionVector, 0.0f} *speed;

            Emit(p);
        }
    }

    void ParticleSystem::EmitFountain(const ParticleProps& particleProps, uint32_t count, float speed)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            ParticleProps p = particleProps;  // Copy the properties

            // Generate a random direction mostly upwards
            float direction = ((float)rand() / (RAND_MAX)) * glm::pi<float>() - glm::pi<float>() / 2.0f;
            glm::vec2 directionVector(cos(direction), sin(direction));

            // Set velocity based on direction and speed
            p.Velocity = glm::vec3{ directionVector, 0.0f} *speed;

            Emit(p);
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