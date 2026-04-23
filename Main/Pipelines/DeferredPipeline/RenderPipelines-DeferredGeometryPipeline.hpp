#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../../../Helpers.hpp"

struct RTG;

struct FGBufferDataSet
{
    // 0. render pass
	VkRenderPass GBufferPass;

    // 1. format for each geometry buffer
    VkFormat GBuffer0Format = VK_FORMAT_R16G16B16A16_SFLOAT; // normal + roughness
    VkFormat GBuffer1Format = VK_FORMAT_R8G8B8A8_UNORM;      // albedo + metalness
    VkFormat GBuffer2Format = VK_FORMAT_R16G16B16A16_SFLOAT; // worldPos + materialType

    // 2. data set
    std::vector<Helpers::AllocatedImage> GBufferImages;
    std::vector<VkImageView> GBufferViews;
    std::vector<VkFramebuffer> GBufferFramebuffers;

    // 3. depth
    Helpers::AllocatedImage DepthImage;
    VkImageView DepthView = VK_NULL_HANDLE;
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
        int Padding0;
        float Padding1;
        float Padding2;
    };

    void Create
    (
        RTG &, 
        VkRenderPass RenderPass, 
        uint32_t subpass,
        VkDescriptorSetLayout CameraLayout,
        VkDescriptorSetLayout WorldLayout,
        VkDescriptorSetLayout TransformsLayout,
        VkDescriptorSetLayout TextureLayout);
    void Destroy(RTG &);
};