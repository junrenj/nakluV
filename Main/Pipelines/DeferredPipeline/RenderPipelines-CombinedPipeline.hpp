#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct RTG;

struct FDeferredCombinedPipeline
{
    VkDescriptorSetLayout Set0_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_EnvTex = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set3_ScreenProcess = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    struct FConstant
    {
        int ScreenProcessMode;  // 0->None 1->SSAO 2->SSDO
        float Padding0;
        float Padding1;
        float Padding2;
    };

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass, VkDescriptorSetLayout WorldLayout, VkDescriptorSetLayout EnvTexLayout);
    void Destroy(RTG &);
};
