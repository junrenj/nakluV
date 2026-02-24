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
    Displacement = 4,
};


struct FMaterial
{
    EMaterialType Type;

    // for all kind of materials
    uint32_t NormalTexIdx = INVALID_TEXTURE;
    uint32_t DisplacementTexIdx = INVALID_TEXTURE;

    // PBR and Lambertian Only
    // glm::vec4 Albedo;   // for better alignment
    // PBR Only
    // float Roughness;
    // float Metalness;

    uint32_t AlbedoTexIdx = INVALID_TEXTURE;
    uint32_t RoughnessTexIdx = INVALID_TEXTURE;
    uint32_t MetalnessTexIdx = INVALID_TEXTURE;

    static FMaterial* GetDefaultMaterial()
    {
        static FMaterial Fallback = []() 
        {
            FMaterial Mat;
            Mat.Type = EMaterialType::PBR;
            
            Mat.AlbedoTexIdx        = 3;    // Magenta
            Mat.RoughnessTexIdx     = 0;    // white
            Mat.MetalnessTexIdx     = 1;    // black
            Mat.NormalTexIdx        = 2;    // normal
            Mat.DisplacementTexIdx  = 0;    // white
            // Mat.Albedo = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
            // Mat.Roughness = 1.0f;
            // Mat.Metalness = 0.0f;

            return Mat;
        }();

        return &Fallback;
    }
};
