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
        glm::vec3 gravity(0.0f, -9.81f, 0.0f); // Assuming y direction is up, adjust as necessary

        for (auto& particle : m_ParticlePool)
        {
            if (!particle.Active)
                continue;

            if (particle.LifeRemaining <= 0.0f)
            {
                particle.Active = false;
                continue;
            }

            // Apply gravity
            particle.Velocity += gravity * (float)ts;

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
    void ParticleSystem::EmitExplosion(ParticleProps& particleProps)
    {
        if (particleProps.Looping)
        {
            for (int i = 0; i < particleProps.MaxParticlesPerEmit; i++)
            {
                ParticleProps p = particleProps;  // Copy the properties

                // Generate a random direction in 3D space
                float theta = ((float)rand() / (RAND_MAX)) * 2.0f * glm::pi<float>(); // Azimuthal angle
                float phi = ((float)rand() / (RAND_MAX)) * glm::pi<float>(); // Polar angle

                // Convert spherical coordinates to cartesian
                glm::vec3 directionVector(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));

                // Set velocity based on direction and speed
                p.Velocity = directionVector * ((float)rand() / (RAND_MAX)) * 10.0f;

                Emit(p);
            }

        }
        else
        {
            for (;particleProps.LoopCount < particleProps.MaxLoopCount; particleProps.LoopCount++)
            {
                ParticleProps p = particleProps;  // Copy the properties

                // Generate a random direction in 3D space
                float theta = ((float)rand() / (RAND_MAX)) * 2.0f * glm::pi<float>(); // Azimuthal angle
                float phi = ((float)rand() / (RAND_MAX)) * glm::pi<float>(); // Polar angle

                // Convert spherical coordinates to cartesian
                glm::vec3 directionVector(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));

                // Set velocity based on direction and speed
                p.Velocity = directionVector * ((float)rand() / (RAND_MAX)) * 10.0f;

                Emit(p);
            }

        }
        
        
    }
    void ParticleSystem::EmitRainFall(ParticleProps& particleProps)
    {
        if (particleProps.Looping)
        {
            for (int i = 0; i < particleProps.MaxParticlesPerEmit; i++)
            {
                ParticleProps p = particleProps;  // Copy the properties

                // Generate a random direction (normalized)
                float direction = ((float)rand() / (RAND_MAX)) * 2.0f * glm::pi<float>();
                glm::vec2 directionVector(cos(direction), sin(direction));

                // Set velocity based on direction and speed
                p.Velocity = glm::vec3{ directionVector, 0.0f } *((float)rand() / (RAND_MAX)) * 10.0f;

                Emit(p);
            }

        }
        else
        {
        for (; particleProps.LoopCount < particleProps.MaxLoopCount; particleProps.LoopCount++)
        {

            ParticleProps p = particleProps;  // Copy the properties

            // Generate a random direction (normalized)
            float direction = ((float)rand() / (RAND_MAX)) * 2.0f * glm::pi<float>();
            glm::vec2 directionVector(cos(direction), sin(direction));

            // Set velocity based on direction and speed
            p.Velocity = glm::vec3{ directionVector, 0.0f } *((float)rand() / (RAND_MAX)) * 10.0f;

            Emit(p);

        }

        }


    }

    void ParticleSystem::EmitFountain(ParticleProps& particleProps)
    {
        if (particleProps.Looping)
        {
            for (int i = 0;  i < particleProps.MaxParticlesPerEmit; i++)
            {
                ParticleProps p = particleProps;  // Copy the properties

                // Generate a random direction mostly upwards
                float direction = ((float)rand() / (RAND_MAX)) * glm::pi<float>() / 2.0f + glm::pi<float>() / 4.0f;
                glm::vec2 directionVector(cos(direction), sin(direction));

                // Set velocity based on direction and speed
                // Velocity in the x-direction and z-direction for a more fountain-like effect
                p.Velocity = glm::vec3{ directionVector.x, ((float)rand() / (RAND_MAX)) * 10.0f, directionVector.y } *2.0f;

                Emit(p);
            }

        }
        else
        {
            for (; particleProps.LoopCount < particleProps.MaxLoopCount; particleProps.LoopCount++)
            {
                ParticleProps p = particleProps;  // Copy the properties

                // Generate a random direction mostly upwards
                float direction = ((float)rand() / (RAND_MAX)) * glm::pi<float>() / 2.0f + glm::pi<float>() / 4.0f;
                glm::vec2 directionVector(cos(direction), sin(direction));

                // Set velocity based on direction and speed
                // Velocity in the x-direction and z-direction for a more fountain-like effect
                p.Velocity = glm::vec3{ directionVector.x, ((float)rand() / (RAND_MAX)) * 10.0f, directionVector.y } *2.0f;

                Emit(p);
            }
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