#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FSSAOPassUBO
{
    float ViewFromWorld[16];
    float Projection[16];
    float InvProjection[16];
    float Radius;
    float Bias;
    float Power;
    int SampleCount;
};

struct FDeferredSSAOPipeline
{
    VkDescriptorSetLayout Set0_GBuffer = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
    void Destroy(RTG &);
};

struct FSSAOData
{
    VkFormat AOFormat = VK_FORMAT_R8_UNORM;

    VkRenderPass SSAOPass = VK_NULL_HANDLE;

    Helpers::AllocatedImage AOImage;
    VkImageView AOView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> AOFramebuffers;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
};
