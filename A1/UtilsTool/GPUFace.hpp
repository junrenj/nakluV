#pragma once
#include "CubeHelpers.hpp"
#include "CubePipeline.hpp"
#include "CubeExecute.hpp"
#include "glm/glm/glm.hpp"

using vec3 = glm::vec3;
using vec4 = glm::vec4;

struct FGPUFace 
{
	CubeHelpers::AllocatedImage Image;
	CubeHelpers::AllocatedBuffer Buffer; //for transform info
	VkImageView View = VK_NULL_HANDLE;
	VkDescriptorSet Descriptors = VK_NULL_HANDLE;

	void Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FCubePipeline const &Pipeline, uint32_t const Width, uint32_t const Height, vec3 * const Data);
	void Destroy(CubeExecute &CubeExe);
};