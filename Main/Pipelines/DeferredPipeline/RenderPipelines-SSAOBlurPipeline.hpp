#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FDeferredSSAOBlurPipeline
{
    VkDescriptorSetLayout Set0_InputInfo = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
    void Destroy(RTG &);
};

struct FSSAOBlurData
{
    VkFormat AOBlurFormat = VK_FORMAT_R8_UNORM;

    VkRenderPass BlurPass = VK_NULL_HANDLE;

    Helpers::AllocatedImage AOBlurImage;
    VkImageView AOBlurView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> AOBlurFramebuffers;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
};
