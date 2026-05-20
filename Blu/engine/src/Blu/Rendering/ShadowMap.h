#pragma once
#include "Blu/Core/Core.h"
#include <glm/glm.hpp>

namespace Blu
{
    class ShadowMap
    {
    public:
        virtual ~ShadowMap() = default;

        static Shared<ShadowMap> Create(uint32_t size = 1024);

        virtual void BindForWriting()  = 0;
        virtual void UnbindForWriting() = 0;
        virtual void BindTexture(uint32_t slot) = 0;

        virtual uint32_t GetSize() const = 0;
    };
}
