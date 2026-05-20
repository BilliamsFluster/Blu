#pragma once
#include "Blu/Core/Core.h"
#include <string>

namespace Blu
{
    class TextureCube;
    class Texture2D;

    // IBL (Image-Based Lighting) manager.
    // Loads an equirectangular HDR environment map and precomputes:
    //   - 32x32 irradiance cubemap  (diffuse ambient term)
    //   - 128x128 prefiltered env cubemap with 5 mip levels (specular)
    //   - 256x256 BRDF integration LUT (split-sum, R=scale G=bias)
    // All convolution is done on the CPU once at load time.
    // Bind slots: irradiance=t6, prefilter=t7, brdfLUT=t8 (matched in PBR_Mesh.hlsl)
    class IBLSystem
    {
    public:
        static void Init();     // precompute BRDF LUT (done once at engine startup)
        static void Shutdown();

        // Load an equirectangular .hdr file and precompute irradiance + prefilter maps.
        // Returns false if the file couldn't be loaded.
        static bool LoadEnvironment(const std::string& hdrPath);

        // Bind all three IBL textures to PS slots (irradianceSlot, prefilterSlot, brdfSlot).
        static void BindIBL(uint32_t irradianceSlot = 6,
                            uint32_t prefilterSlot  = 7,
                            uint32_t brdfSlot       = 8);

        static void UnbindIBL(uint32_t irradianceSlot = 6,
                              uint32_t prefilterSlot  = 7,
                              uint32_t brdfSlot       = 8);

        static bool                 IsReady()           { return s_IrradianceMap && s_PrefilterMap && s_BRDFLut; }
        static const std::string&   GetHDRPath()        { return s_HdrPath; }
        static Shared<TextureCube>  GetIrradianceMap()  { return s_IrradianceMap; }
        static Shared<TextureCube>  GetPrefilterMap()   { return s_PrefilterMap;  }
        static Shared<Texture2D>    GetBRDFLUT()        { return s_BRDFLut;       }
        static int                  GetPrefilterMips()  { return kPrefilterMips;  }

    private:
        static void ComputeBRDFLUT();
        static void BuildFromHDR(const float* pixels, int w, int h);
        static void ComputeIrradiance(const float* pixels, int w, int h);
        static void ComputePrefilter (const float* pixels, int w, int h);

        static Shared<TextureCube>  s_IrradianceMap;
        static Shared<TextureCube>  s_PrefilterMap;
        static Shared<Texture2D>    s_BRDFLut;
        static std::string          s_HdrPath;

        static constexpr int kIrradianceSize    = 32;
        static constexpr int kPrefilterSize     = 128;
        static constexpr int kPrefilterMips     = 5;
        static constexpr int kBRDFLUTSize       = 256;
        static constexpr int kIrradianceSamples = 256;
        static constexpr int kPrefilterSamples  = 64;
        static constexpr int kBRDFSamples       = 512;
    };
}
