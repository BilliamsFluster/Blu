#include "Blupch.h"
#include "Material.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLMaterial.h"
#include "Blu/Platform/DirectX11/D3D11Material.h"

namespace Blu
{
    Shared<Material> Material::Create(Shader* shader)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::None:      return nullptr;
        case RendererAPI::API::OpenGL:    return std::make_shared<OpenGLMaterial>(shader);
        case RendererAPI::API::Direct3D:  return std::make_shared<D3D11Material>(shader);
        }
        return nullptr;
    }

    Shared<Material> Material::Create()
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::None:      return nullptr;
        case RendererAPI::API::OpenGL:    return std::make_shared<OpenGLMaterial>();
        case RendererAPI::API::Direct3D:  return std::make_shared<D3D11Material>();
        }
        return nullptr;
    }
}
