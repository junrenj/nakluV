#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FDeferredSSDOFilterPipeline
{
    VkDescriptorSetLayout Set0_InputInfo = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
    void Destroy(RTG &);
};

struct FSSDOFilterData
{
    VkFormat SSDOFilterFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VkRenderPass FilterPass = VK_NULL_HANDLE;

    Helpers::AllocatedImage SSDOFilterImage;
    VkImageView SSDOFilterView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> SSDOFilterFramebuffers;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
};
