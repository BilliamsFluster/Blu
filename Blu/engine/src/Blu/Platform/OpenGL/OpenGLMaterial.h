#pragma once
#include "Blu/Rendering/Material.h"

namespace Blu
{
    class OpenGLMaterial : public Material
    {
    public:
        OpenGLMaterial(Shader* shader);
        OpenGLMaterial();
        virtual bool operator ==(const Material& other) const override { return MaterialProperties.AmbientColor == MaterialProperties.AmbientColor; }

        virtual void SetShaderData(Blu::MaterialProperties& properties) override;
        virtual Shader* GetShader() const override { return m_Shader; }
        virtual void BindMaterialToShader(Material* material, Shader* shader) override;
        virtual uint32_t GetProgramID() override { return m_RendererID; }
            
        
    private:
        Shader* m_Shader;
        uint32_t m_RendererID;
    };
}
