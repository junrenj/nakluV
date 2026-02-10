#pragma once
#include <vector>
#include <cstdint>

class UTexture
{
public:
    enum class EType
    {
        Flat,
        Cube,
    }Type = EType::Flat;

    enum class EFormat
    {
        Linear,
        SRGB,
        RGBE
    }Format = EFormat::Linear;

    struct FTextureMipMap
    {
        uint32_t SizeX;
        uint32_t SizeY;

        // Real Data
        std::vector<uint8_t> BulkData;
    };

    std::vector<FTextureMipMap* > MipmapsData;

    void GenerateMipmap(uint8_t MipmapLevel);

    void Get1x1PixelTexture(float R, float G, float B);
};
