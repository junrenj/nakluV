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
    glm::vec3 Tint;
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
    // 0. Basic Info
    glm::vec4 Position_Type;      //  xyz -> position w->Type   ---- Sun don't need position ----

    // 1. Dir and Radius
    glm::vec4 Direction_Limit;     // xyz->Direction   ---- Sphere don't need it ----       
                                    // w->Limit        ---- Sun don't need it ----

    // 2. Color and Intensity
    glm::vec4 Color_Falloff;        // xyz->Color = Tint * Power / Strength || w->FalloffExponent

    // 3. Extra Info
    /*
        Sun[Angle, 0, 0, 0]
        Sphere[SourceRadius, 0, 0, 0]
        Spot[cosInner, cosOuter, 0, 0]
    */
    glm::vec4 SpecialParams;
};