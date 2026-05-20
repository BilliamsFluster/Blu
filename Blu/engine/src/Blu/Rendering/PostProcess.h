#pragma once
#include "Blu/Core/Core.h"
#include <glm/glm.hpp>

namespace Blu
{
    class FrameBuffer;
    class Shader;

    class PostProcess
    {
    public:
        virtual ~PostProcess() = default;

        static Shared<PostProcess> Create(uint32_t width, uint32_t height);

        // Bind the HDR scene framebuffer so the renderer draws into it.
        virtual void Begin() = 0;

        // Run the full post-process stack (bloom → tonemap → FXAA) and blit to backbuffer.
        virtual void Submit(Shared<Shader> shader = nullptr) = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual Shared<FrameBuffer> GetFrameBuffer() = 0;

        // ── Settings ─────────────────────────────────────────────────────────────
        bool  EnableBloom     = true;
        float BloomThreshold  = 1.0f;
        float BloomStrength   = 0.05f;

        bool  EnableFXAA      = true;

        // SSAO
        bool  EnableSSAO     = true;
        float SSAORadius     = 0.5f;
        float SSAOBias       = 0.025f;
        float SSAOPower      = 2.0f;
        int   SSAOSamples    = 16;
        float SSAOStrength   = 1.0f;

        // Camera matrices — set by Scene before Submit so SSAO can reconstruct depth
        glm::mat4 SSAOProjection    = glm::mat4(1.0f);
        glm::mat4 SSAOInvProjection = glm::mat4(1.0f);
    };
}
