#pragma once
#include "BBox.hpp"
#include "glm/glm/glm.hpp"

using mat4 = glm::mat4;

class UCullingUtils
{
public:
    static bool SATVisibilityTest(const FCullingFrustum& Frustum, const mat4& Transform, const FAABB& AABB);
};
