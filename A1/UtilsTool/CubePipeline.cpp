#include "CubePipeline.hpp"

#include "CubeHelpers.hpp"
#include "../../VK.hpp"

static uint32_t cube_code[] =
#include "../../spv/A1/UtilsTool/cube.comp.inl"
;

void FCubePipeline::Create(CubeExecute &CubeExe) {
	VkShaderModule module = CubeExe.helpers.create_shader_module(cube_code);
	
	{ //the set0_in layout holds input face info:
		std::array< VkDescriptorSetLayoutBinding, 2 > Bindings{
			VkDescriptorSetLayoutBinding
            {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
			},
			VkDescriptorSetLayoutBinding
            {
				.binding = 1,
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

		VK( vkCreateDescriptorSetLayout(CubeExe.device, &CreateInfo, nullptr, &Set1_Face) );
	}

	{ //the set2 layout holds roughness info (and maybe, someday, more brdf params):
		std::array< VkDescriptorSetLayoutBinding, 1 > Bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo CreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(Bindings.size()),
			.pBindings = Bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(CubeExe.device, &CreateInfo, nullptr, &Set2_Params) );
	}


	{ //create pipeline layout:
		std::array< VkDescriptorSetLayout, 3 > Layouts{
			Set1_Face,
			Set1_Face,
			Set2_Params,
		};

		VkPipelineLayoutCreateInfo CreateInfo{
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

	//modules no longer needed now that pipeline is created:
	vkDestroyShaderModule(CubeExe.device, module, nullptr);
}

void FCubePipeline::Destroy(CubeExecute &CubeExe) {
	if (Handle != VK_NULL_HANDLE) {
		vkDestroyPipeline(CubeExe.device, Handle, nullptr);
		Handle = VK_NULL_HANDLE;
	}

	if (Layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(CubeExe.device, Layout, nullptr);
		Layout = VK_NULL_HANDLE;
	}

	if (Set2_Params != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(CubeExe.device, Set2_Params, nullptr);
		Set2_Params = VK_NULL_HANDLE;
	}

	if (Set1_Face != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(CubeExe.device, Set1_Face, nullptr);
		Set1_Face = VK_NULL_HANDLE;
	}
}
