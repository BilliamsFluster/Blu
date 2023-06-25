#pragma once
#include <glm/glm.hpp>
#include "Blu/Core/Timestep.h"

namespace Blu
{
    struct ParticleProps
    {
        glm::vec2 Position;
        glm::vec2 Velocity;
        glm::vec4 ColorBegin, ColorEnd;
        float SizeBegin, SizeEnd, SizeVariation;
        float LifeTime = 1.0f;
    };

    // A single particle
    struct Particle
    {
        glm::vec2 Position;
        glm::vec2 Velocity;
        glm::vec4 ColorBegin, ColorEnd;
        float Rotation = 0.0f;
        float SizeBegin, SizeEnd, SizeVariation;
        float LifeTime = 1.0f;
        float LifeRemaining = 0.0f;

        bool Active = false;
    };

    class ParticleSystem
    {
    public:
        ParticleSystem(uint32_t maxParticles = 10000);

        void OnUpdate( Blu::Timestep ts);
        void OnRender();

        void Emit(const ParticleProps& particleProps);

    private:
        std::vector<Particle> m_ParticlePool;
        uint32_t m_PoolIndex;
    };

    

    



}
