#pragma once


#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <cstring>

#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


class ImageLoader
{
public:
    ImageLoader() = delete;
    static std::vector<uint32_t> Load(const std::string& relativePath, int& outWidth, int& outHeight)
    {
        stbi_set_flip_vertically_on_load(true);
        std::cout << "Current path: " << fs::current_path() << std::endl;

        int channels = 0;
        unsigned char* pixels = stbi_load
        (
            relativePath.c_str(),
            &outWidth,
            &outHeight,
            &channels,
            STBI_rgb_alpha
        );

        if (!pixels)
        {
            outWidth = 1;
            outHeight = 1;
            return std::vector<uint32_t>{ 0xFFFFFFFF };
        }

        std::vector<uint32_t> data(static_cast<size_t>(outWidth) * outHeight);
        std::memcpy(data.data(), pixels, data.size() * sizeof(uint32_t));
        stbi_image_free(pixels);
        return data;
    }
};