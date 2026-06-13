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
#include "Blu/Core/Application.h"
#include "Blu/Platform/DirectX11/D3D11FrameBuffer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // GLFW/deps (added to Blu-Editor include dirs)

#include <iostream>
#include <vector>

namespace Blu
{
    ScreenshotLayer::ScreenshotLayer(std::string scenePath, std::string outputPath, uint32_t width, uint32_t height)
        : Layer("ScreenshotLayer"),
          m_ScenePath(std::move(scenePath)), m_OutputPath(std::move(outputPath)),
          m_Width(width ? width : 1280), m_Height(height ? height : 720)
    {
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

        // Frame the scene from a sensible default viewpoint.
        m_Camera = EditorCamera(45.0f, (float)m_Width / (float)m_Height, 0.1f, 1000.0f);
        m_Camera.SetViewportSize((float)m_Width, (float)m_Height);
        m_Camera.SetFocalPoint({ 0.0f, 1.0f, 0.0f });
        m_Camera.SetDistance(18.0f);
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
        m_Camera.SetViewportSize((float)m_Width, (float)m_Height);
        m_Scene->OnUpdateEditor(deltaTime, m_Camera);
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
