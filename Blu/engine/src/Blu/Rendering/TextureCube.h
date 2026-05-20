#pragma once
#include "Texture.h"

namespace Blu
{
    // Abstract cubemap texture. Each face is a square of the same size.
    // Subclasses expose SetFaceData to upload individual faces / mip levels.
    class TextureCube : public Texture
    {
    public:
        // Create an empty HDR (R16G16B16A16_FLOAT) cubemap.
        static Shared<TextureCube> Create(uint32_t size, uint32_t mipLevels = 1);

        // Upload pixel data for one face + mip level.
        // data is tightly packed, rowPitch = (size >> mipLevel) * bytesPerPixel.
        virtual void SetFaceData(int face, int mipLevel,
                                 const void* data, uint32_t rowPitch) = 0;

        virtual uint32_t GetMipLevels() const = 0;
    };
}
