#include "Blupch.h"
#include "D3D11Context.h"
#include "Blu/Core/Log.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Blu
{
    D3D11Context* D3D11Context::s_Instance = nullptr;

    D3D11Context::D3D11Context(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle)
    {
        BLU_CORE_ASSERT(!s_Instance, "D3D11Context already created - only one allowed");
        s_Instance = this;
    }

    D3D11Context::~D3D11Context()
    {
        s_Instance = nullptr;
    }

    void D3D11Context::Init()
    {
        HWND hwnd = glfwGetWin32Window(m_WindowHandle);
        int  w, h;
        glfwGetFramebufferSize(m_WindowHandle, &w, &h);

        DXGI_SWAP_CHAIN_DESC scd               = {};
        scd.BufferCount                        = 2;
        scd.BufferDesc.Width                   = (UINT)w;
        scd.BufferDesc.Height                  = (UINT)h;
        scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator   = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow                       = hwnd;
        scd.SampleDesc.Count                   = 1;
        scd.SampleDesc.Quality                 = 0;
        scd.Windowed                           = TRUE;
        scd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        UINT deviceFlags = 0;
#ifdef BLU_DEBUG
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL chosenLevel;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            deviceFlags,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            &scd,
            m_SwapChain.GetAddressOf(),
            m_Device.GetAddressOf(),
            &chosenLevel,
            m_DeviceContext.GetAddressOf()
        );
        BLU_CORE_ASSERT(SUCCEEDED(hr), "D3D11CreateDeviceAndSwapChain failed");

        CreateBackbuffer();
        BLU_CORE_INFO("D3D11 context initialised (feature level 0x{0:X})", (uint32_t)chosenLevel);
    }

    void D3D11Context::CreateBackbuffer()
    {
        // Colour RTV from swapchain backbuffer
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
        m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(backbuffer.GetAddressOf()));
        m_Device->CreateRenderTargetView(backbuffer.Get(), nullptr,
            m_BackbufferRTV.GetAddressOf());

        // Depth-stencil matching backbuffer dimensions
        D3D11_TEXTURE2D_DESC bbDesc = {};
        backbuffer->GetDesc(&bbDesc);

        D3D11_TEXTURE2D_DESC dsDesc = {};
        dsDesc.Width              = bbDesc.Width;
        dsDesc.Height             = bbDesc.Height;
        dsDesc.MipLevels          = 1;
        dsDesc.ArraySize          = 1;
        dsDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsDesc.SampleDesc.Count   = 1;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage              = D3D11_USAGE_DEFAULT;
        dsDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

        m_Device->CreateTexture2D(&dsDesc, nullptr, m_DepthStencilTexture.GetAddressOf());
        m_Device->CreateDepthStencilView(m_DepthStencilTexture.Get(), nullptr,
            m_DepthStencilView.GetAddressOf());

        m_DeviceContext->OMSetRenderTargets(1,
            m_BackbufferRTV.GetAddressOf(),
            m_DepthStencilView.Get());

        // Match viewport
        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)bbDesc.Width;
        vp.Height   = (float)bbDesc.Height;
        vp.MaxDepth = 1.0f;
        m_DeviceContext->RSSetViewports(1, &vp);
    }

    void D3D11Context::SwapBuffers()
    {
        // SyncInterval = 0 → immediate present, no VSync cap.
        // Use DXGI_PRESENT_ALLOW_TEARING when the swap chain was created with
        // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING so the driver can avoid tearing
        // artefacts on VRR displays without re-enabling the vsync stall.
        m_SwapChain->Present(0, 0);
    }

    void D3D11Context::Resize(uint32_t width, uint32_t height)
    {
        if (!m_Device || width == 0 || height == 0) return;

        m_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_BackbufferRTV.Reset();
        m_DepthStencilView.Reset();
        m_DepthStencilTexture.Reset();

        m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        CreateBackbuffer();
    }
}
