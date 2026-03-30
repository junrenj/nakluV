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

    static void UpdateBBoxWithTransform(FAABB& OutBox, const FAABB& LocalBox, const glm::mat4& Transform);
};

struct FOBB
{
    vec3 Center;
    vec3 Extents;
    vec3 Axes[3];
};

