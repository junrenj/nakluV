#pragma once

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

    std::vector<std::unique_ptr<FTextureMipMap> > MipmapData;
};
