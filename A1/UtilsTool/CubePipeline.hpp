#pragma once

#include "CubeExecute.hpp"

struct FCubePipeline
{
    // descriptor set layouts:
    VkDescriptorSetLayout Set1_Face = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_Params = VK_NULL_HANDLE;

    // types for descriptors
    struct FFace
    {
        struct {
            float m0, m1, m2, padding0_;
            float m3, m4, m5, padding1_;
            float m6, m7, m8, padding2_;
        } WORLD_FROM_PX;
    };
    static_assert(sizeof(FFace) == (3*4)*4, "Face descriptor is the expected size.");

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