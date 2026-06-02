#pragma once
#include <cstdint>

namespace Blu
{
    // Selects how the 3D scene is shaded. Read once per frame by Scene::Render3DPass.
    enum class RenderPath : uint8_t
    {
        Forward  = 0, // existing single-pass forward+ PBR
        Deferred = 1  // G-buffer geometry pass + full-screen lighting pass
    };

    // Process-global render configuration, mirroring how PostProcess settings live
    // as a single owned struct. The editor writes these; the renderer reads them.
    // Kept deliberately tiny so toggling is allocation-free and thread-agnostic.
    class RenderSettings
    {
    public:
        static RenderPath GetPath()            { return s_Path; }
        static void       SetPath(RenderPath p){ s_Path = p; }

        // When deferred is active, reuse the G-buffer's world normals + depth for a
        // higher-quality SSAO than the depth-only reconstruction. Optional; off by default.
        static bool GetUseGBufferSSAO()        { return s_UseGBufferSSAO; }
        static void SetUseGBufferSSAO(bool v)  { s_UseGBufferSSAO = v; }

    private:
        static RenderPath s_Path;
        static bool       s_UseGBufferSSAO;
    };
}
