#include "Texture.hpp"
#include <cmath>
#include <algorithm>

void UTexture::GenerateMipmap(uint8_t MipmapLevel)
{
    // TODO: Generate New Mipmap Data
}

void UTexture::Get1x1PixelTexture(float R, float G, float B)
{
    std::vector<uint8_t> Data(4);
    auto LinearToSRGB = [](float linear) -> uint8_t 
    {
        linear = std::clamp(linear, 0.0f, 1.0f);
        float srgb = std::pow(linear, 1.0f / 2.2f); 
        return static_cast<uint8_t>(std::round(srgb * 255.0f));
    };

    Data[0] = LinearToSRGB(R);
    Data[1] = LinearToSRGB(G);
    Data[2] = LinearToSRGB(B);
    Data[3] = 255;

    UTexture::FTextureMipMap* NewMipMap = new UTexture::FTextureMipMap();
    NewMipMap->SizeX = 1;
    NewMipMap->SizeY = 1;
    NewMipMap->BulkData = Data;

    MipmapsData.push_back(std::move(NewMipMap));
}