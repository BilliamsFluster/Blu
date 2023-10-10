#pragma once
#include "Shader.h"
#include "Blu/Core/Core.h"
#include "Texture.h"

namespace Blu
{
    class Material
    {
    public:
        // Common material properties
        glm::vec3 AmbientColor = glm::vec3(1.0f, 0.5f, 0.31f);
        glm::vec3 DiffuseColor = glm::vec3(1.0f, 0.5f, 0.31f);
        glm::vec3 SpecularColor = glm::vec3(0.5f, 0.5f, 0.5f);
        float Shininess = 32.0f;

        // Textures
        Shared<Texture2D> DiffuseMap;
        Shared<Texture2D> SpecularMap;
        Shared<Texture2D> NormalMap;
        Shared<Texture2D> AlbedoMap;

        // Flags or other properties
        bool IsTransparent;
        // ... other flags or properties

        // Common methods
        virtual void SetShaderData(/* parameters */) = 0;
        virtual Shader* GetShader() const = 0;

        // Methods to set/get properties
        void SetAmbientColor(const glm::vec3& color) { AmbientColor = color; }
        glm::vec3 GetAmbientColor() const { return AmbientColor; }

        void SetDiffuseColor(const glm::vec3& color) { DiffuseColor = color; }
        glm::vec3 GetDiffuseColor() const { return DiffuseColor; }

        void SetSpecularColor(const glm::vec3& color) { SpecularColor = color; }
        glm::vec3 GetSpecularColor() const { return SpecularColor; }

        void SetShininess(float shininess) { Shininess = shininess; }
        float GetShininess() const { return Shininess; }

        

        static Shared<Material> Create(Shader* shader);
        static Shared<Material> Create();
    };
}
