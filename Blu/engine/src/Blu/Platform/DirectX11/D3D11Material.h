#pragma once
#include "Blu/Rendering/Material.h"

namespace Blu
{
    class D3D11Material : public Material
    {
    public:
        D3D11Material() { IsTransparent = false; }
        explicit D3D11Material(Shader* shader) : m_Shader(shader) { IsTransparent = false; }

        bool operator==(const Material& other) const override
        {
            return MaterialProperties.AmbientColor == static_cast<const D3D11Material&>(other).MaterialProperties.AmbientColor;
        }

        void SetShaderData(Blu::MaterialProperties& props) override
        {
            // Upload via Shader::SetUniform* when drawing — nothing to do here
        }

        Shader* GetShader() const override { return m_Shader; }

        void BindMaterialToShader(Material* material, Shader* shader) override
        {
            if (!shader || !material) return;
            auto& p = material->MaterialProperties;
            shader->SetUniformFloat3("u_Material.ambient",  p.AmbientColor);
            shader->SetUniformFloat3("u_Material.diffuse",  p.DiffuseColor);
            shader->SetUniformFloat3("u_Material.specular", p.SpecularColor);
            shader->SetUniformFloat ("u_Material.shininess",p.Shininess);
        }

        uint32_t GetProgramID() override { return 0; }

    private:
        Shader* m_Shader = nullptr;
    };
}
