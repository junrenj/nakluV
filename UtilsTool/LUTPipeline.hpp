#pragma once

#include "CubeExecute.hpp"

struct FLUTPipeline
{
    // descriptor set layouts:
    VkDescriptorSetLayout Set0_OutFace = VK_NULL_HANDLE;

    // No such constant
    VkPipelineLayout Layout = VK_NULL_HANDLE;

    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(CubeExecute &);
    void Destroy(CubeExecute &);
};