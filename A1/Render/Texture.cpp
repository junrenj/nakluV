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

    Data[0] = static_cast<uint8_t>(std::round(std::clamp(R, 0.0f, 1.0f) * 255.0f));
    Data[1] = static_cast<uint8_t>(std::round(std::clamp(G, 0.0f, 1.0f) * 255.0f));
    Data[2] = static_cast<uint8_t>(std::round(std::clamp(B, 0.0f, 1.0f) * 255.0f));
    Data[3] = 255;

    UTexture::FTextureMipMap* NewMipMap = new UTexture::FTextureMipMap();
    NewMipMap->SizeX = 1;
    NewMipMap->SizeY = 1;
    NewMipMap->BulkData = Data;

    MipmapsData.push_back(std::move(NewMipMap));
}