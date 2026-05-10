#pragma once
#include "Blu/Rendering/GraphicsContext.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Blu
{
    class D3D11Context : public GraphicsContext
    {
    public:
        D3D11Context(GLFWwindow* windowHandle);
        ~D3D11Context();

        void Init()         override;
        void SwapBuffers()  override;

        void Resize(uint32_t width, uint32_t height);

        // --- Singleton accessor for platform implementations ---
        static D3D11Context* Get() { return s_Instance; }

        ID3D11Device*           GetDevice()           const { return m_Device.Get(); }
        ID3D11DeviceContext*    GetDeviceContext()     const { return m_DeviceContext.Get(); }
        IDXGISwapChain*         GetSwapChain()         const { return m_SwapChain.Get(); }
        ID3D11RenderTargetView* GetBackbufferRTV()     const { return m_BackbufferRTV.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView()  const { return m_DepthStencilView.Get(); }

        // Shared by D3D11VertexArray so it can create InputLayouts lazily
        void        SetCurrentVSBytecode(const void* bytecode, SIZE_T size) { m_CurrentVSBytecode = bytecode; m_CurrentVSBytecodeSize = size; }
        const void* GetCurrentVSBytecode()     const { return m_CurrentVSBytecode; }
        SIZE_T      GetCurrentVSBytecodeSize() const { return m_CurrentVSBytecodeSize; }

        // Used by D3D11RendererAPI::Clear
        void SetClearColorValue(const glm::vec4& color) { m_ClearColor = color; }
        const glm::vec4& GetClearColorValue()     const { return m_ClearColor; }

    private:
        void CreateBackbuffer();

        GLFWwindow* m_WindowHandle;

        Microsoft::WRL::ComPtr<ID3D11Device>            m_Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>     m_DeviceContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain>          m_SwapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_BackbufferRTV;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_DepthStencilView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_DepthStencilTexture;

        const void* m_CurrentVSBytecode     = nullptr;
        SIZE_T      m_CurrentVSBytecodeSize = 0;

        glm::vec4 m_ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

        static D3D11Context* s_Instance;
    };
}
