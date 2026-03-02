#pragma once

#include "CubeExecute.hpp"

struct FCubePipeline
{
    // descriptor set layouts:
    VkDescriptorSetLayout Set0_InFace = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set1_OutFace = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_Params = VK_NULL_HANDLE;

    struct Params
    {
        float Roughness;
    };
    static_assert(sizeof(Params)==4, "Params descriptor is the expected size.");

    // No such constant
    VkPipelineLayout Layout = VK_NULL_HANDLE;

    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(CubeExecute &);
    void Destroy(CubeExecute &);
};