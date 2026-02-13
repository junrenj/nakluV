#pragma once

enum class ELightType
{
    Sun = 0,
    Sphere = 1,
    Spot = 2,
};

class ULight
{
public:
    uint32_t shadow = 0; 
    struct FColor
    {
        float r, g, b = 1.0f;
    }Tint;
    ELightType LightType;
};

class ULight_Sun : public ULight
{
public:
    ULight_Sun() { LightType = ELightType::Sun; }
    float Angle;
    float Strength;
};

class ULight_Sphere : public ULight
{
public:
    ULight_Sphere() { LightType = ELightType::Sphere; }
    float Radius;
    float Power;
    float Limit = std::numeric_limits< float >::infinity();
};

class ULight_Spot : public ULight
{
public:
    ULight_Spot() { LightType = ELightType::Spot; }
    float Radius;
    float Power;
    float Limit = std::numeric_limits< float >::infinity();
    float Fov;
    float Blend;
};

struct FLightRenderProxy
{
public:
    uint32_t Type;  // 0-Sun 1-Sphere 2-Spot
    glm::vec3 Direction;    // WorldSpace
    glm::vec3 Position;     // Sun don't need it
    glm::vec3 Color;        // Tint * Strength
};