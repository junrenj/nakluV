#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

struct FVertexDataSet
{
    struct { float x,y,z; } Position;
    struct { float x,y,z; } Normal;
    struct { float x,y,z,w;} Tangent;
    struct { float u,v; } Texcoord;

    //a pipeline vertex input state that works with a buffer holding a PosNorTexVertex[] array:
    static const VkPipelineVertexInputStateCreateInfo ArrayInputState;
};

static_assert(sizeof(FVertexDataSet) == 3*4 + 3*4 + 4*4 + 2*4, "FVertexDataSet  is packed.");
