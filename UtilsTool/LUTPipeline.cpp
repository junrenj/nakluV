#include "LUTPipeline.hpp"

#include "CubeHelpers.hpp"
#include "../VK.hpp"

static uint32_t brdfLut_code[] =
#include "../spv/UtilsTool/Shaders/brdfLut.comp.inl"
;

void FLUTPipeline::Create(CubeExecute &CubeExe)
{
	VkShaderModule module = CubeExe.helpers.create_shader_module(brdfLut_code);

	{ //the set1_in layout holds output face info:
		std::array< VkDescriptorSetLayoutBinding, 1 > Bindings
		{
			VkDescriptorSetLayoutBinding
            {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo CreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(Bindings.size()),
			.pBindings = Bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(CubeExe.device, &CreateInfo, nullptr, &Set0_OutFace) );
	}

	{ //create pipeline layout:
		std::array< VkDescriptorSetLayout, 1 > Layouts
		{
			Set0_OutFace,
		};

		VkPipelineLayoutCreateInfo CreateInfo
        {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = uint32_t(Layouts.size()),
			.pSetLayouts = Layouts.data(),
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};

		VK( vkCreatePipelineLayout(CubeExe.device, &CreateInfo, nullptr, &Layout) );
	}

	{ //create pipelines:

		VkComputePipelineCreateInfo CreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.flags = VK_PIPELINE_CREATE_DISPATCH_BASE,
			.stage = VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.module = module,
				.pName = "main"
			},
			.layout = Layout,
		};

		VK( vkCreateComputePipelines(CubeExe.device, VK_NULL_HANDLE, 1, &CreateInfo, nullptr, &Handle) );

	}

	// modules no longer needed now that pipeline is created:
	vkDestroyShaderModule(CubeExe.device, module, nullptr);
}

void FLUTPipeline::Destroy(CubeExecute &CubeExe) 
{
	if (Handle != VK_NULL_HANDLE) 
    {
		vkDestroyPipeline(CubeExe.device, Handle, nullptr);
		Handle = VK_NULL_HANDLE;
	}

	if (Layout != VK_NULL_HANDLE) 
    {
		vkDestroyPipelineLayout(CubeExe.device, Layout, nullptr);
		Layout = VK_NULL_HANDLE;
    }

	if (Set0_OutFace != VK_NULL_HANDLE) 
    {
		vkDestroyDescriptorSetLayout(CubeExe.device, Set0_OutFace, nullptr);
		Set0_OutFace = VK_NULL_HANDLE;
	}
}
