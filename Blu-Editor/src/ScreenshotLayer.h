#pragma once
#include <cstdint>
#include <string>
#include "Blu/Core/Layer.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "Blu/Rendering/EditorCamera.h"

namespace Blu
{
    class Scene;
    class FrameBuffer;

    // Headless capture layer. Renders one frame of a scene to an offscreen RGBA8
    // framebuffer, writes it to a PNG, then closes the app. Driven by:
    //   Blu-Editor.exe --screenshot <scene.blu> <out.png> [width height]
    // This lets rendering changes be verified from the command line without the editor
    // UI or a computer-use screen grant — and doubles as CI / visual-regression tooling.
    class ScreenshotLayer : public Blu::Layers::Layer
    {
    public:
        ScreenshotLayer(std::string scenePath, std::string outputPath, uint32_t width, uint32_t height,
                        bool enableFog = false, bool playMode = false, bool enablePostFX = false);

        void OnAttach() override;
        void OnUpdate(Blu::Timestep deltaTime) override;

    private:
        std::string m_ScenePath;
        std::string m_OutputPath;
        uint32_t    m_Width;
        uint32_t    m_Height;
        bool        m_EnableFog;
        bool        m_PlayMode;   // run the runtime (actors/HUD/FP camera) instead of the editor camera
        bool        m_EnablePostFX; // force-enable the post-process stack (bloom/SSAO/fog volumes/etc.)

        Shared<Scene>       m_Scene;
        Shared<FrameBuffer> m_FrameBuffer;
        EditorCamera        m_Camera;
        int                 m_WarmupFrames = 2; // let shadows/IBL/post settle before capture
        bool                m_Done = false;
    };
}
