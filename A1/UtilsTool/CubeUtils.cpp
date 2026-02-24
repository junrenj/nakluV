#include "CubeUtils.hpp"
#include "../../VK.hpp"
#include "../Render/Color.hpp"
#include <vulkan/vulkan_core.h>

#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/utility/vk_format_utils.h>

#include <GLFW/glfw3.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../../stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../stb_image_write.h"

UCubeUtils::UCubeUtils(const char* Path, uint32_t OutputSize)
{
    GetPhyscialDevice();
    LoadImage(Path);
    // Create Cube Image
    {
        VkImageCreateInfo CreateInfo
        {
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = OutputSize,
            .mipLevels = 1,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkImage Image;
        vkCreateImage(physical_device, &CreateInfo, nullptr, &Image);
    }

    {
        VkImageViewCreateInfo View
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            // View.image = 
        };
    }
}

UCubeUtils::~UCubeUtils()
{

}

void UCubeUtils::LoadImage(const char* Path)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    uint8_t* data = stbi_load(Path, &width, &height, &channels, 4);
    
    TextureSize = width;
    for (int i = 0; i < TextureSize * height; ++i) 
    {
        DataFloat.push_back(RGBE2Float(glm::u8vec4(data[i*4], data[i*4+1], data[i*4+2], data[i*4+3])));
    }
}

void UCubeUtils::GetPhyscialDevice()
{
    // create the `instance` (main handle to Vulkan library):
	{
		VkInstanceCreateFlags InstanceFlags = 0;
		std::vector< const char * > InstanceExtensions;
		std::vector< const char * >InstanceLayers;

		// add extensions for MoltenVK portability layer on macOS
		#if defined(__APPLE__)
		InstanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		InstanceExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		InstanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		InstanceExtensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		#endif
		
        // add extensions needed by glfw
        {
            glfwInit();
            if(!glfwVulkanSupported())
            {
                throw std::runtime_error("GLFW reports Vulkan is not supported");
            }

            uint32_t Count;
            const char **Extensions = glfwGetRequiredInstanceExtensions(&Count);
            if(Extensions == nullptr)
            {
                throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
            }
            for (uint32_t i = 0; i < Count; i++)
            {
                InstanceExtensions.emplace_back(Extensions[i]);
            }
        }

		VkInstanceCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = InstanceFlags,
			.pApplicationInfo = &application_info,
			.enabledLayerCount = uint32_t(InstanceLayers.size()),
			.ppEnabledLayerNames = InstanceLayers.data(),
			.enabledExtensionCount = uint32_t(InstanceExtensions.size()),
			.ppEnabledExtensionNames = InstanceExtensions.data()
		};
		VK( vkCreateInstance(&CreateInfo, nullptr, &instance));

        // select the `physical_device` -- the gpu that will be used to draw:
        {
            std::vector< std::string > PhysicalDeviceNames;
            // Pick a physical device
            {
                uint32_t Count = 0;
                VK( vkEnumeratePhysicalDevices(instance, &Count, nullptr));
                std::vector< VkPhysicalDevice > PhysicalDevices(Count);
                VK( vkEnumeratePhysicalDevices(instance, &Count, PhysicalDevices.data()));

                uint32_t BestScore = 0;

                for (auto const &Pd : PhysicalDevices)
                {
                    VkPhysicalDeviceProperties Properties;
                    vkGetPhysicalDeviceProperties(Pd, &Properties);

                    VkPhysicalDeviceFeatures Features;
                    vkGetPhysicalDeviceFeatures(Pd, &Features);

                    PhysicalDeviceNames.emplace_back(Properties.deviceName);
                    
                    uint32_t Score = 1;
                    if(Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                    {
                        Score += 0x8000;
                    }

                    if(Score > BestScore)
                    {
                        BestScore = Score;
                        physical_device = Pd;
                    }
                }
            }

            // report device name:
            {
                VkPhysicalDeviceProperties Properties;
                vkGetPhysicalDeviceProperties(physical_device, &Properties);
                std::cout << "Selected physical device '" << Properties.deviceName << "'." << std::endl;
            }
	    }
    }
}