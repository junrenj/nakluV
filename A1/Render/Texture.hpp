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

    static UTexture* GetDefaultBlackTex()
    {
        static UTexture Tex = []
        {
            UTexture T;
            T.Get1x1PixelTexture(0,0,0);
            return T;
        }();
        return &Tex;
    }

    static UTexture* GetDefaultWhiteTex()
    {
        static UTexture Tex = []
        {
            UTexture T;
            T.Get1x1PixelTexture(1,1,1);
            return T;
        }();
        return &Tex;
    }

    static UTexture* GetDefaultNormalTex()
    {
        static UTexture Tex = []
        {
            UTexture T;
            T.Get1x1PixelTexture(0.5,0.5,1);
            return T;
        }();
        return &Tex;
    }

    static UTexture* GetDefualtFallbackColorTex()
    {
        static UTexture Tex = []
        {
            UTexture T;
            T.Get1x1PixelTexture(1,0,1);
            return T;
        }();
        return &Tex;
    }
};
