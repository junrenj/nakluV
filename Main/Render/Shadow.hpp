#pragma once

#include <vulkan/vulkan.h>
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
