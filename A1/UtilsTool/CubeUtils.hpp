#pragma once

#include <vulkan/vulkan_core.h>
#include <string>
#include <vector>
#include "glm/glm/glm.hpp"

using vec3 = glm::vec3;

struct UCubeUtils
{
    VkImage CreateCubemapImage(VkDevice Device, uint32_t Size, VkFormat Format, VkImageUsageFlags Uasge);
    VkImageView CreateCubeView(VkDevice Device, VkImage Image, VkFormat Format);
    VkDescriptorSetLayout CreateComputeLayout(VkDevice Device);

    struct VulkanContext
    {
        VkInstance Instance;
        VkPhysicalDevice PhysicalDevice;
        VkDevice Device;

        uint32_t ComputeQueueFamily;
        VkQueue ComputeQueue;

        VkCommandPool CommandPool;
    };
    
	VkSampler TextureSampler = VK_NULL_HANDLE;
    VkDescriptorSet EnvDescriptorSet;
    VkDescriptorSet IrradianceDescriptorSet;

    
    UCubeUtils(VulkanContext* InContext, const char* Path, uint32_t InSize, uint32_t OutputSize);
    ~UCubeUtils();
};
