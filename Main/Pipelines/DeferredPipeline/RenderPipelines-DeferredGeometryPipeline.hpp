#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FGBufferDataSet
{
    // 0. render pass
	VkRenderPass GBufferPass;

    // 1. data set
    std::vector<Helpers::AllocatedImage> GBufferImages;
    std::vector<VkImageView> GBufferViews;
    std::vector<VkFramebuffer> GBufferFramebuffers;
};

struct FDeferredGeometryPipeline
{
    VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set2_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout Set3_Texture = VK_NULL_HANDLE;

    VkPipelineLayout Layout = VK_NULL_HANDLE;

    VkPipeline Handle = VK_NULL_HANDLE;

    struct FPush
    {
        int MaterialType;
        float Padding0;
        float Padding1;
        float Padding2;
    };

    void Create(RTG &, 
        VkRenderPass RenderPass, 
        uint32_t subpass,
        VkDescriptorSetLayout CameraLayout,
        VkDescriptorSetLayout WorldLayout,
        VkDescriptorSetLayout TransformsLayout,
        VkDescriptorSetLayout TextureLayout);
    void Destroy(RTG &);
};