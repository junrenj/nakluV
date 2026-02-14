#pragma once
#include "glm/glm/glm.hpp"

using vec3 = glm::vec3;

struct FCullingFrustum
{
    float NearRight;
    float NearTop;
    float NearPlane;
    float FarPlane;
};

struct FAABB
{
    vec3 Max;
    vec3 Min;
};

struct FOBB
{
    vec3 Center;
    vec3 Extents;
    vec3 Axes[3];
};

