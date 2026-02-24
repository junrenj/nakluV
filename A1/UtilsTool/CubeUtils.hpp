#pragma once

#include <vulkan/vulkan_core.h>
#include <string>
#include <vector>
#include "glm/glm/glm.hpp"
#include "../../RTG.hpp"

using vec3 = glm::vec3;

class UCubeUtils : RTG::Application
{
private:
	//Basic vulkan handles:
	VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkApplicationInfo application_info
    {
        .pApplicationName = "Unknown",
        .applicationVersion = VK_MAKE_VERSION(0,0,0),
        .pEngineName = "Unknown",
        .engineVersion = VK_MAKE_VERSION(0,0,0),
        .apiVersion = VK_API_VERSION_1_3
	};

    std::vector<vec3> DataFloat;
    int TextureSize;

    void LoadImage(const char* Path);
    void GetPhyscialDevice();
public:
    
    UCubeUtils(const char* Path, uint32_t OutputSize);
    ~UCubeUtils();
};
