#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{
    class CascadedShadowMap
    {
    public:
        static constexpr int NUM_CASCADES = 3;

        virtual ~CascadedShadowMap() = default;

        static Shared<CascadedShadowMap> Create(uint32_t size = 2048);

        // Bind slice i as the current render target and clear it.
        virtual void BindCascadeForWriting(int cascadeIndex) = 0;

        // Restore the previous render target after all cascades have been rendered.
        virtual void UnbindForWriting() = 0;

        // Bind the full Texture2DArray SRV so the pixel shader can sample all cascades.
        virtual void BindTexture(uint32_t slot) = 0;

        virtual uint32_t GetSize() const = 0;
    };
}
