#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FDeferredDirectLightingPipeline
{
    VkDescriptorSetLayout Set0_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_Lights = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set3_Shadowmap = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    struct FConstant
    {
        int LightsCount;	// How many lights on the scene
        float Padding0;
        float Padding1;
        float Padding2;
    };

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass, VkDescriptorSetLayout WorldLayout, VkDescriptorSetLayout LightsLayout, VkDescriptorSetLayout ShadowLayout);
    void Destroy(RTG &);
};

struct FDirectLightingData
{
    VkFormat DirectLightingFormat = VK_FORMAT_R16G16B16A16_SFLOAT ;

    VkRenderPass DirectLightingPass = VK_NULL_HANDLE;

    Helpers::AllocatedImage DirectLightingImage;
    VkImageView DirectLightingView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> DirectLightingFramebuffers;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
};
