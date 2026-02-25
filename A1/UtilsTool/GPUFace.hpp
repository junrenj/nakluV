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

	void Create(CubeExecute &CubeExe, VkDescriptorPool descriptor_pool, FCubePipeline const &pipeline, uint32_t const sz, vec3 * const data);
	void Destroy(CubeExecute &CubeExe);
};