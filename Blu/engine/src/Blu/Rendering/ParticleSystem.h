#pragma once
#include <glm/glm.hpp>
#include "Blu/Core/Timestep.h"

namespace Blu
{
    struct ParticleProps
    {
        glm::vec3 Position;
        glm::vec3 Velocity;
        glm::vec4 ColorBegin = { 1.0f, 0.0f, 0.0f, 1.0f }, ColorEnd = { 1.0f, 0.0f, 1.0f, 1.0f };
        float SizeBegin = 1, SizeEnd = 0, SizeVariation = 0.5;
        int MaxParticlesPerEmit = 100;
        int ParticleCount = 100;
        bool Looping = true;
        int MaxLoopCount = 100;
        int LoopCount = 0;
        float LifeTime = 1.0f;
    };

    // A single particle
    struct Particle
    {
        glm::vec3 Position;
        glm::vec3 Velocity;
        glm::vec4 ColorBegin = { 1.0f, 1.0f, 1.0f, 1.0f }, ColorEnd = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
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
        void EmitExplosion( ParticleProps& particleProps);
        void EmitFountain( ParticleProps& particleProps);
        void EmitRainFall( ParticleProps& particleProps);


    private:
        std::vector<Particle> m_ParticlePool;
        uint32_t m_PoolIndex;
    };

    

    



}
