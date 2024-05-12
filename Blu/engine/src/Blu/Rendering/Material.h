#pragma once
#include "Shader.h"
#include "Blu/Core/Core.h"
#include "Texture.h"

namespace Blu
{
    struct MaterialProperties
    {
        glm::vec3 AmbientColor; // Remember to align fields according to std140 rules
        float Padding1; // Padding for alignment
        glm::vec3 DiffuseColor;
        float Padding2; // Padding for alignment
        glm::vec3 SpecularColor;
        float Padding3; // Padding for alignment
        float Shininess;
        float Padding4[3]; // Additional padding to ensure alignment
        MaterialProperties() : AmbientColor(1.0f, 0.5f, 0.31f), DiffuseColor(1.0f, 0.5f, 0.31f), SpecularColor(0.5f, 0.5f, 0.5f), Shininess(32.0f) {
            // Initialize padding to ensure it has predictable content
            Padding1 = Padding2 = Padding3 = 0.0f;
            std::fill(std::begin(Padding4), std::end(Padding4), 0.0f);
        }
    };
    class Material
    {
    public:
        
        
        Blu::MaterialProperties MaterialProperties;

        // Textures
        Shared<Texture2D> DiffuseMap;
        Shared<Texture2D> SpecularMap;
        Shared<Texture2D> NormalMap;
        Shared<Texture2D> AlbedoMap;

        // Flags or other properties
        bool IsTransparent;
        // ... other flags or properties

        // Common methods
        virtual void SetShaderData(Blu::MaterialProperties& properties) = 0;
        virtual Shader* GetShader() const = 0;
        
        Blu::MaterialProperties& GetMaterialProperties() { return MaterialProperties; }

        // Methods to set/get properties
        void SetAmbientColor( glm::vec3& color) { MaterialProperties.AmbientColor = color; }
        glm::vec3& GetAmbientColor()  { return MaterialProperties.AmbientColor; }

        void SetDiffuseColor(glm::vec3& color) { MaterialProperties.DiffuseColor = color; }
        glm::vec3& GetDiffuseColor()  { return MaterialProperties.DiffuseColor; }

        void SetSpecularColor( glm::vec3& color) { MaterialProperties.SpecularColor = color; }
        glm::vec3& GetSpecularColor()  { return MaterialProperties.SpecularColor; }

        void SetShininess(float shininess) { MaterialProperties.Shininess = shininess; }
        float& GetShininess()  { return MaterialProperties.Shininess; }

        virtual bool operator ==(const Material& other) const = 0;


        static Shared<Material> Create(Shader* shader);
        static Shared<Material> Create();
        virtual void BindMaterialToShader(Material* material, Shader* shader) = 0;
        virtual uint32_t GetProgramID() = 0;
            

        
    };
}
