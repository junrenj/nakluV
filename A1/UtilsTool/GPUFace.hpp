#pragma once
#include "CubeHelpers.hpp"
#include "CubePipeline.hpp"
#include "LUTPipeline.hpp"
#include "CubeExecute.hpp"
#include "glm/glm/glm.hpp"

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

struct FGPUFace 
{
	CubeHelpers::AllocatedImage Image;
	CubeHelpers::AllocatedBuffer Buffer; //for transform info
	VkImageView View = VK_NULL_HANDLE;
	VkDescriptorSet Descriptors = VK_NULL_HANDLE;
	VkSampler Sampler;

	void Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FCubePipeline const &Pipeline, uint32_t const Width, uint32_t const Height, vec3 * const Data, bool isOutput = false);
	void Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FLUTPipeline const &Pipeline, uint32_t const ImageSize, vec2 * const Data);
	void Destroy(CubeExecute &CubeExe);
};