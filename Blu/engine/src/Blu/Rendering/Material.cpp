#include "Blupch.h"
#include "Material.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Platform/OpenGL/OpenGLMaterial.h"

namespace Blu
{
    Shared<Material> Material::Create(Shader* shader)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::None:
            return nullptr;
        case RendererAPI::API::OpenGL:
            return std::make_shared<OpenGLMaterial>(shader);
            
        }
        return nullptr;
    }

    Shared<Material> Material::Create()
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::None:
            return nullptr;
        case RendererAPI::API::OpenGL:
            return std::make_shared<OpenGLMaterial>();

        }
        return nullptr;
    }
}
