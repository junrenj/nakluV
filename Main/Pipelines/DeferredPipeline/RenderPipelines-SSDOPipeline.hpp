#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FSSDOPassUBO
{
    float ViewFromWorld[16];
    float Projection[16];
    float InvProjection[16];
    float Radius;
    float Bias;
    float Power;
    int SampleCount;
};

struct FDeferredSSDOPipeline
{
    VkDescriptorSetLayout InputBuffer = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
    void Destroy(RTG &);
};

struct FSSDOData
{
    VkFormat DOFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VkRenderPass SSDOPass = VK_NULL_HANDLE;

    Helpers::AllocatedImage DOImage;
    VkImageView DOView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> DOFramebuffers;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
};
