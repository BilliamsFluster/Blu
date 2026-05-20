#include "Blupch.h"
#include "Material.h"

namespace Blu
{
    Shared<Material> Material::Create()
    {
        return std::make_shared<Material>();
    }

    void Material::Bind(Shader& shader) const
    {
        // PBR scalars — AlbedoColor is now float4 (alpha carries transparency)
        shader.SetUniformFloat4("u_AlbedoColor",     AlbedoColor);
        shader.SetUniformFloat ("u_Metallic",         Metallic);
        shader.SetUniformFloat ("u_Roughness",        Roughness);
        shader.SetUniformFloat ("u_AO",               AO);
        shader.SetUniformFloat3("u_EmissiveColor",    EmissiveColor);
        shader.SetUniformFloat ("u_EmissiveStrength", EmissiveStrength);

        // Alpha cutoff: non-zero activates clip() in the pixel shader (Masked mode)
        shader.SetUniformFloat("u_AlphaCutoff", (Blend == BlendMode::Masked) ? AlphaCutoff : 0.0f);

        // Shading model: 0 = PBR, 1 = Unlit
        shader.SetUniformInt("u_ShadingModel", static_cast<int>(Shading));

        // Texture presence flags + bind
        auto bindSlot = [&](const Shared<Texture2D>& tex, int slot, const char* hasUniform)
        {
            if (tex) { tex->Bind(slot); shader.SetUniformInt(hasUniform, 1); }
            else       shader.SetUniformInt(hasUniform, 0);
        };

        bindSlot(AlbedoMap,            0, "u_HasAlbedoMap");
        bindSlot(NormalMap,            1, "u_HasNormalMap");
        bindSlot(MetallicRoughnessMap, 2, "u_HasMetallicRoughnessMap");
        bindSlot(AOMap,                3, "u_HasAOMap");
        bindSlot(EmissiveMap,          4, "u_HasEmissiveMap");
    }
}
