#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include "glm/glm/glm.hpp"
#include "../../Helpers.hpp"

using mat4 = glm::mat4;

struct FShadowResource
{
    uint32_t Resolutions = 0;

    Helpers::AllocatedImage Image;
    VkImageView ImageView;
    VkFramebuffer Framebuffer = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSet;

};

struct FCubeShadowResource
{
    uint32_t Resolution = 0;

    Helpers::AllocatedImage Image;
    VkImageView CubeView = VK_NULL_HANDLE;
    std::array<VkImageView, 6> FaceViews{};
    std::array<VkFramebuffer, 6> FaceFramebuffers{};
    std::array<mat4, 6> ShadowClipFromWorld{};
};

struct FShadowDataSet
{
    // 0. renderpass
	VkRenderPass ShadowPass;
    
	// 1. sampler
	VkSampler ShadowSamplerPCF;
    
	// 2. data
	const uint32_t MAX_SPOT_SHADOWS = 64;
	std::vector<FShadowResource> SpotLightShadows;
	std::vector<FCubeShadowResource> SphereLightShadows;
};

struct FShadowPipeline
{
    VkDescriptorSetLayout Set0_Transform = VK_NULL_HANDLE;
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    struct FPush
    {
        mat4 SHADOW_CLIP_FROM_WORLD;
    };

    VkPipeline Handle = VK_NULL_HANDLE;

    void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass, VkDescriptorSetLayout TransformsLayout);
    void Destroy(RTG &);
};

