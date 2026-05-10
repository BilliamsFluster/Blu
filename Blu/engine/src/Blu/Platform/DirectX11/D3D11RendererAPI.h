#pragma once
#include "Blu/Rendering/RendererAPI.h"

namespace Blu
{
    class D3D11RendererAPI : public RendererAPI
    {
    public:
        void Init()                                                              override;
        void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)       override;
        void SetClearColor(const glm::vec4& color)                              override;
        void DrawIndexed(const Shared<VertexArray>& va, uint32_t indexCount = 0) override;
        void DrawLines(const Shared<VertexArray>& va, uint32_t vertexCount)     override;
        void Clear()                                                             override;
    };
}
