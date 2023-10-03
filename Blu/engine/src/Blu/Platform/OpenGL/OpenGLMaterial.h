#pragma once
#include "Blu/Rendering/Material.h"

namespace Blu
{
    class OpenGLMaterial : public Material
    {
    public:
        OpenGLMaterial(Shader* shader) : m_Shader(shader) {}
        OpenGLMaterial();

        virtual void SetShaderData(/* parameters */) override;
        virtual Shader* GetShader() const override { return m_Shader; }

    private:
        Shader* m_Shader;
        // Other OpenGL-specific shader data
    };
}
