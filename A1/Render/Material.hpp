#pragma once
#include "Texture.hpp"
#include "glm/glm/glm.hpp"

#define INVALID_TEXTURE INT32_MAX

// for easy just use a uniform material stand for all materials
enum class EMaterialType
{
    PBR = 0,
    Lambertian = 1,
    Mirror = 2,
    Environment = 3,
};


class UMaterial
{
public:
    EMaterialType Type;

    // for all kind of materials
    uint32_t NormalTexIdx = INVALID_TEXTURE;
    uint32_t DisplacementIdx = INVALID_TEXTURE;

    // PBR and Lambertian Only
    glm::vec4 Albedo;   // for better alignment
    // PBR Only
    float Roughness;
    float Metalness;

    uint32_t AlbedoTex = INVALID_TEXTURE;
    uint32_t RoughnessTex = INVALID_TEXTURE;
    uint32_t MetalnessTex = INVALID_TEXTURE;
};
