#pragma once
#include "Shader.h"
#include "Blu/Core/Core.h"
#include "Texture.h"
#include <string>

namespace Blu
{
    enum class BlendMode : uint8_t
    {
        Opaque      = 0,  // No blending, depth write on
        Masked      = 1,  // Alpha test (clip), depth write on, no sorting needed
        Transparent = 2,  // Alpha blend, depth write off, sort back-to-front
        Additive    = 3   // Additive blend, depth write off
    };

    enum class ShadingModel : uint8_t
    {
        PBR   = 0,
        Unlit = 1,
    };

    class Material
    {
    public:
        std::string  Name;
        BlendMode    Blend          = BlendMode::Opaque;
        ShadingModel Shading        = ShadingModel::PBR;
        bool         TwoSided       = false;
        float        AlphaCutoff    = 0.5f;   // only used when Blend == Masked

        glm::vec4    AlbedoColor    = glm::vec4(1.0f);  // alpha used for Transparent/Masked
        float        Metallic       = 0.0f;
        float        Roughness      = 0.5f;
        float        AO             = 1.0f;
        glm::vec3    EmissiveColor  = glm::vec3(0.0f);
        float        EmissiveStrength = 0.0f;

        Shared<Texture2D> AlbedoMap;
        Shared<Texture2D> NormalMap;
        Shared<Texture2D> MetallicRoughnessMap;
        Shared<Texture2D> AOMap;
        Shared<Texture2D> EmissiveMap;

        // Uploads PBR uniforms and binds all texture slots into `shader`.
        void Bind(Shader& shader) const;

        bool IsTransparent() const { return Blend == BlendMode::Transparent || Blend == BlendMode::Additive; }

        static Shared<Material> Create();
    };
}
