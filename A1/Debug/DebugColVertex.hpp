#pragma once

#include <vulkan/vulkan_core.h>
#include "glm/glm/glm.hpp"

#include <cstdint>

struct FDebugColVertex
{
    struct { float x,y,z; } Position;
    struct { uint8_t r,g,b,a; } Color;

    // a pipeline vertex input state that works with a buffer holding a PosColVertex[] array:
    static const VkPipelineVertexInputStateCreateInfo ArrayInputState;

    FDebugColVertex()
    {

    }
    FDebugColVertex(glm::vec3 InPosition, const uint8_t* InColor)
    {
        Position.x = InPosition.x;
        Position.y = InPosition.y;
        Position.z = InPosition.z;
        Color.r = InColor[0];
        Color.g = InColor[1];
        Color.b = InColor[2];
        Color.a = InColor[3];
    }
};

static_assert(sizeof(FDebugColVertex) == 3*4 + 4*1, "FDebugColVertex is packed.");