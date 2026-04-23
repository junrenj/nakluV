#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct RTG;

struct FDeferredLightingPipeline
{
    VkDescriptorSetLayout Set0_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_Lights = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set3_EnvTex = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set4_Shadowmap = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set5_ScreenProcess = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    struct FConstant
    {
        int LightsCount;	// How many lights on the scene
        int ScreenProcessMode;  // 0->None 1->SSAO 2->SSDO
        float Padding1;
        float Padding2;
    };

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass, VkDescriptorSetLayout WorldLayout, VkDescriptorSetLayout LightsLayout, VkDescriptorSetLayout EnvTexLayout, VkDescriptorSetLayout ShadowLayout);
    void Destroy(RTG &);
};
