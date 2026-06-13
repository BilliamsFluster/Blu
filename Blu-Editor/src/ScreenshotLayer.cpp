// D3D11FrameBuffer.h pulls in <windows.h>; suppress the min/max macros so they don't
// break entt's std::numeric_limits<>::max() usage in this translation unit.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "ScreenshotLayer.h"

#include <Blu.h>
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/RenderCommand.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Core/Application.h"
#include "Blu/Platform/DirectX11/D3D11FrameBuffer.h"
#include "AzureGameModule.h" // register Azure gameplay classes for --play captures

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // GLFW/deps (added to Blu-Editor include dirs)

#include <iostream>
#include <vector>

namespace Blu
{
    ScreenshotLayer::ScreenshotLayer(std::string scenePath, std::string outputPath, uint32_t width, uint32_t height,
                                     bool enableFog, bool playMode)
        : Layer("ScreenshotLayer"),
          m_ScenePath(std::move(scenePath)), m_OutputPath(std::move(outputPath)),
          m_Width(width ? width : 1280), m_Height(height ? height : 720),
          m_EnableFog(enableFog), m_PlayMode(playMode)
    {
        if (m_PlayMode)
            m_WarmupFrames = 40; // let physics settle, actors spawn, the FP camera/HUD come up
    }

    void ScreenshotLayer::OnAttach()
    {
        FrameBufferSpecifications spec;
        spec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::Depth };
        spec.Width = m_Width;
        spec.Height = m_Height;
        m_FrameBuffer = FrameBuffer::Create(spec);

        m_Scene = std::make_shared<Scene>();
        SceneSerializer serializer(m_Scene);
        if (!serializer.Deserialize(m_ScenePath))
        {
            std::cerr << "[screenshot] failed to load scene: " << m_ScenePath << std::endl;
            m_Scene = nullptr;
            return;
        }

        // Optional atmospheric fog preset for verifying the fog path (scenes ship with
        // fog disabled). Tuned for a S.T.A.L.K.E.R.-ish hazy distance.
        if (m_EnableFog)
        {
            FogSettings& fog = m_Scene->GetFog();
            fog.Enabled       = true;
            fog.Color         = { 0.62f, 0.66f, 0.72f };
            fog.Density       = 0.035f;
            fog.HeightStart   = 0.0f;
            fog.HeightDensity = 0.06f;
            fog.AerialColor   = { 0.70f, 0.80f, 0.92f };
            fog.AerialStrength = 0.85f;
        }

        if (m_PlayMode)
        {
            // Runtime capture: start the game so actors spawn, the player drives the
            // first-person camera, and the runtime HUD renders. Frames are stepped in
            // OnUpdate; the FP view + HUD are what we capture.
            Azure::RegisterAzureGameModule(); // so Azure::PlayerCharacter / ZombieTestActor spawn
            m_Scene->OnViewportResize((float)m_Width, (float)m_Height);
            m_Scene->SetPlayerInputEnabled(true);
            m_Scene->OnRuntimeStart();
        }
        else
        {
            // Editor capture: frame the scene from a sensible default viewpoint.
            m_Camera = EditorCamera(45.0f, (float)m_Width / (float)m_Height, 0.1f, 1000.0f);
            m_Camera.SetViewportSize((float)m_Width, (float)m_Height);
            m_Camera.SetFocalPoint({ 0.0f, 1.0f, 0.0f });
            m_Camera.SetDistance(18.0f);
        }
    }

    void ScreenshotLayer::OnUpdate(Blu::Timestep deltaTime)
    {
        if (m_Done)
            return;
        if (!m_Scene || !m_FrameBuffer)
        {
            Application::Get().Close();
            return;
        }

        m_FrameBuffer->Bind();
        RenderCommand::SetClearColor({ 0.10f, 0.11f, 0.13f, 1.0f });
        RenderCommand::Clear();
        m_Scene->OnViewportResize((float)m_Width, (float)m_Height);
        const float fixedDt = 1.0f / 60.0f; // deterministic step for warmup/physics
        if (m_PlayMode)
        {
            m_Scene->OnUpdateRuntime(fixedDt);
        }
        else
        {
            m_Camera.SetViewportSize((float)m_Width, (float)m_Height);
            m_Scene->OnUpdateEditor(deltaTime, m_Camera);
        }
        m_FrameBuffer->UnBind();

        if (--m_WarmupFrames > 0)
            return; // render a couple of frames before capturing

        std::vector<uint8_t> pixels;
        uint32_t w = 0, h = 0;
        auto* d3dFB = static_cast<D3D11FrameBuffer*>(m_FrameBuffer.get());
        if (d3dFB && d3dFB->ReadColorAttachmentRGBA8(0, pixels, w, h))
        {
            stbi_write_png(m_OutputPath.c_str(), (int)w, (int)h, 4, pixels.data(), (int)(w * 4));
            std::cout << "[screenshot] wrote " << m_OutputPath << " (" << w << "x" << h << ")" << std::endl;
        }
        else
        {
            std::cerr << "[screenshot] color readback failed" << std::endl;
        }

        m_Done = true;
        Application::Get().Close();
    }
}
