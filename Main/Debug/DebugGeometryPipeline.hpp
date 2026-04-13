#pragma once
#include <vulkan/vulkan.h>
#include <array>

struct RTG;

struct FGBufferDebugPipeline
{
    VkDescriptorSetLayout Set0_GBuffer = VK_NULL_HANDLE;
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;

    struct FPush
    {
        int Mode;      // 1=albedo, 2=normal, 3=position
        float Padding0;
        float Padding1;
        float Padding2;
    };

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
    void Destroy(RTG &);
};