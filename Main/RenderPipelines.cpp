#include "RenderPipelines.hpp"

#include "../VK.hpp"

#include <GLFW/glfw3.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include "Render/RenderScene.hpp"
#include "Render/RenderExtractor.hpp"
#include "Animation/AnimationPlayer.hpp"
#include "../mat4.hpp"
#include "glm/glm/gtc/type_ptr.hpp"
#include "Debug/Profile.hpp"

URenderPipelines::URenderPipelines(RTG &rtg_) : rtg(rtg_)
{
	// load the scene and debug scene
	InitializeRenderScene();
    InitializeDebugRenderScene();
	InitializeCommandLineSettings();


    // select a depth format:
    DepthFormat = rtg.helpers.find_image_format
    (
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    // Create render pass
    {
        // attachments
		std::array< VkAttachmentDescription, 2 > Attachments
		{
			VkAttachmentDescription
			{
				// Color attachment:
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = rtg.present_layout,
			},
			VkAttachmentDescription
			{
				// Depth Attachment
				.format = DepthFormat,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

        // Subpass
		VkAttachmentReference ColorAttachmentRef
		{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference DepthAttachmentRef
		{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription Subpass
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &ColorAttachmentRef,
			.pDepthStencilAttachment = &DepthAttachmentRef,
		};

        // dependencies
		// this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > Dependencies
		{
			VkSubpassDependency
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(Attachments.size()),
			.pAttachments = Attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &Subpass,
			.dependencyCount = uint32_t(Dependencies.size()),
			.pDependencies = Dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &RenderPass));
    }

	// Create Shadow Pass
	{
		VkAttachmentDescription DepthAttachment 
		{
			.format = DepthFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		};

		VkAttachmentReference DepthRef { .attachment = 0, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription Subpass 
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.pDepthStencilAttachment = &DepthRef,
		};

		VkRenderPassCreateInfo CreateInfo 
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &DepthAttachment,
			.subpassCount = 1,
			.pSubpasses = &Subpass,
		};
		VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &ShadowData.ShadowPass));
	}

    // Create Command pool
	{
		VkCommandPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(rtg.device, &CreateInfo, nullptr, &CommandPool) );
	}
	
    // create descriptor pool:
	{
		uint32_t PerWorkspace = uint32_t(rtg.workspaces.size());	// for easier-to-read counting

		std::array< VkDescriptorPoolSize, 2> PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2 * PerWorkspace, 	 // one descriptor per set, two sets per workspace
			},
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 3 * PerWorkspace,	// one descriptor per set, one set per workspace
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, // because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 5 * PerWorkspace, // five set per workspace
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &DescriptorPool));
	}
	// Debug Geometry Buffer
	{
		VkDescriptorPoolSize PoolSize
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 4,
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 1,
			.poolSizeCount = 1,
			.pPoolSizes = &PoolSize,
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &GBufferDebugDescriptorPool));
	}
	// Deferred Lighting
	{
		VkDescriptorPoolSize PoolSize
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 6,
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 2,
			.poolSizeCount = 1,
			.pPoolSizes = &PoolSize,
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &DeferredLightingDescriptorPool));
	}
	

	CreateGBufferPass();
	CreateSSAOPass();
	CreateSSAOBlurPass();

	CreateSSDOPass();
	CreateSSDOFilterPass();

	LinesPipeline.Create(rtg, RenderPass, 0);
    LambertPipeline.Create(rtg, RenderPass, 0);
	ShadowPipeline.Create(rtg, ShadowData.ShadowPass, 0, LambertPipeline.Set2_Transforms);
	DeferredGeometryPipeline.Create(rtg, GBufferData.GBufferPass, 0, LambertPipeline.Set0_Camera, LambertPipeline.Set1_World, LambertPipeline.Set2_Transforms, LambertPipeline.Set3_TEXTURE);
	
	// postprocessing
	SSAOPipeline.Create(rtg, SSAOData.SSAOPass, 0);
	SSAOBlurPipeline.Create(rtg, SSAOBlurData.BlurPass, 0);
	SSDOPipeline.Create(rtg, SSDOData.SSDOPass, 0);
	SSDOFilterPipeline.Create(rtg, SSDOFilterData.FilterPass, 0);

	GBufferDebugPipeline.Create(rtg, RenderPass, 0);
	DeferredLightingPipeline.Create(rtg, RenderPass, 0, LambertPipeline.Set1_World, LambertPipeline.Set4_Lights, LambertPipeline.Set5_EnvTex, LambertPipeline.Set6_Shadowmap);

    workspaces.resize(rtg.workspaces.size());
	for (FWorkspace &workspace : workspaces)
	{
		// allocate command buffer:
		{
			VkCommandBufferAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = CommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			VK( vkAllocateCommandBuffers(rtg.device, &AllocInfo, &workspace.command_buffer));
		}

		workspace.CameraSrc = rtg.helpers.create_buffer
		(
			sizeof(FLambertPipeline::FCamera),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// going to have GPU copy from this memory
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	// host-visible memory, coherent (no special sync needed)
			Helpers::Mapped 						// get a pointer to the memory
		);

		workspace.Camera = rtg.helpers.create_buffer
		(
			sizeof(FLambertPipeline::FCamera),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT, 	// going to use as a uniform buffer, also going to have GPU copy into this memory
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 	// GPU-local memory
			Helpers::Unmapped 						// don't get a pointer to the memory
		);

        
		// allocate descriptor set for Camera descriptor
		{
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set0_Camera,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.CameraDescriptors) );
		}

		workspace.WorldSrc = rtg.helpers.create_buffer
		(
			sizeof(FLambertPipeline::FWorld),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);
		workspace.World = rtg.helpers.create_buffer
		(
			sizeof(FLambertPipeline::FWorld),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		{
			// Allocate descriptor set for world descriptor
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set1_World,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.WorldDescriptors));
		}

		// allocate descriptor set for Transforms descriptor
		{
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set2_Transforms,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.TransformDescriptors));
			// NOTE: will fill in this descriptor set in render when buffers are [re-]allocated
		}

		// allocate descriptor set for Light descriptor
		{
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set4_Lights,
			};
			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.LightsDescriptors));
		}

		// SSAO UBO
		workspace.SSAOSrc = rtg.helpers.create_buffer
		(
			sizeof(FSSAOPassUBO),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);

		workspace.SSAO = rtg.helpers.create_buffer
		(
			sizeof(FSSAOPassUBO),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		// SSDO UBO
		workspace.SSDOSrc = rtg.helpers.create_buffer
		(
			sizeof(FSSDOPassUBO),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);

		workspace.SSDO = rtg.helpers.create_buffer
		(
			sizeof(FSSDOPassUBO),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		 // point descriptor to Camera buffer:
		{
			VkDescriptorBufferInfo CameraInfo
			{
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size,
			};

			VkDescriptorBufferInfo WorldInfo
			{
				.buffer = workspace.World.handle,
				.offset = 0,
				.range = workspace.World.size,
			};

			std::array< VkWriteDescriptorSet, 2 > Writes
			{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.CameraDescriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &CameraInfo,
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.WorldDescriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &WorldInfo,
				},
			};

			vkUpdateDescriptorSets
			(
				rtg.device, 				// device
				uint32_t(Writes.size()), 	// descriptorWriteCount
				Writes.data(), 				// pDescriptorWrites
				0, 							// descriptorCopyCount
				nullptr 					// pDescriptorCopies
			);
		}
	}
	
	// Create Object Vertices
	{
		ObjectVertices = rtg.helpers.create_buffer
		(
			Scene.TotalBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);
		assert(Scene.AllVertexData.size() == Scene.TotalBytes);
		rtg.helpers.transfer_to_buffer(Scene.AllVertexData.data(), Scene.TotalBytes, ObjectVertices);
	}

    // make some Textures
	ReserveTextures();

     // make image views for the textures
	{
		TextureViews.reserve(Textures.size());
		for (Helpers::AllocatedImage const &Image : Textures)
		{
			VkImageViewCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = Image.handle,
				.viewType = Image.layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_CUBE,
				.format = Image.format,
				// .components sets swizzling and is fine when zero-initialized
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = Image.mipLevels,
					.baseArrayLayer = 0,
					.layerCount = Image.layers,
				},
			};

			VkImageView ImageView = VK_NULL_HANDLE;
			VK( vkCreateImageView(rtg.device, &CreateInfo, nullptr, &ImageView));

			TextureViews.emplace_back(ImageView);
		}
		assert(TextureViews.size() == Textures.size());
	}

	 // make a sampler for the textures
	{
		VkSamplerCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.flags = 0,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 0.0f, 				// doesn't matter if anisotropy isn't enabled
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS, // doesn't matter if compare isn't enabled
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK( vkCreateSampler(rtg.device, &CreateInfo, nullptr, &TextureSamplerNearest) );
	}

	// make a linear sampler
	{
		VkSamplerCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.flags = 0,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 0.0f, 				// doesn't matter if anisotropy isn't enabled
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS, // doesn't matter if compare isn't enabled
			.minLod = 0.0f,
			.maxLod = 5.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK( vkCreateSampler(rtg.device, &CreateInfo, nullptr, &TextureSamplerLinear) );
	}

	// create the texture descriptor pool	
	{
		const std::vector<FMaterial*>& Materials = Scene.Materials;
		const uint32_t MaterialCount = uint32_t(Materials.size());

		std::array< VkDescriptorPoolSize, 1 > PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 5 * MaterialCount,	 // one descriptor per set, one set 5 texture
			}
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, 	// because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = MaterialCount, 	// one set per texture
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &TextureDescriptorPool));
	}

	// create env texture descriptor pool
	{
		const std::vector<UEnvironment*>& Environments = Scene.Environments;
		const uint32_t EnvCount = uint32_t(Environments.size());

		std::array< VkDescriptorPoolSize, 1 > PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 3 * EnvCount,	 // one descriptor per set, one set 5 texture
			}
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, 	// because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = EnvCount, 	// one set per texture
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &EnvTexDescriptorPool));
	}

	 // allocate and write the texture descriptor sets
	{
		const uint32_t TexCountPerMat = 5;
		const std::vector<FMaterial*>& Materials = Scene.Materials;
		const size_t MaterialCount = Materials.size();

		MaterialDescriptors.assign(MaterialCount, VK_NULL_HANDLE);

		for (uint32_t i = 0; i < MaterialCount; i++)
		{
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = TextureDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set3_TEXTURE,
			};
			VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &MaterialDescriptors[i]));

			std::vector<VkDescriptorImageInfo> Infos(TexCountPerMat);
			std::vector<VkWriteDescriptorSet> Writes(TexCountPerMat);
			const FMaterial* Material = Materials[i];
			std::array<uint32_t, TexCountPerMat> Indices = 
			{
				Material->AlbedoTexIdx, 
				Material->RoughnessTexIdx, 
				Material->MetalnessTexIdx,
				Material->NormalTexIdx, 
				Material->DisplacementTexIdx
			};

			for (uint32_t k = 0; k < TexCountPerMat; k++)
			{
				const uint32_t Index = Indices[k];
				Infos[k] = VkDescriptorImageInfo
				{
					.sampler = TextureSamplerNearest,
					.imageView = TextureViews[Index],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
				Writes[k] = VkWriteDescriptorSet 
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = MaterialDescriptors[i],
					.dstBinding = k,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &Infos[k],
				};
			}

			vkUpdateDescriptorSets(rtg.device, TexCountPerMat, Writes.data(), 0, nullptr);
		}
	}
	
	// env Texture push
	{
		const std::vector<UEnvironment*>& Environments = Scene.Environments;
		EnvTexDescriptors.clear();
		EnvTexDescriptors.reserve(Environments.size());

		for (auto& Env : Environments)
		{
			if (!Env) continue;

			EnvTexDescriptors.push_back(VK_NULL_HANDLE);

			VkDescriptorSetAllocateInfo AllocInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = EnvTexDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LambertPipeline.Set5_EnvTex,
			};

			VK(vkAllocateDescriptorSets(
				rtg.device,
				&AllocInfo,
				&EnvTexDescriptors.back()
			));

			std::vector<VkDescriptorImageInfo> Infos(3);
			std::vector<VkWriteDescriptorSet> Writes(3);

			for (uint32_t i = 0; i < 3; i++)
			{
				Infos[i] = VkDescriptorImageInfo
				{
					.sampler = TextureSamplerLinear,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
				if(i == 0)
				{
					Infos[i].imageView = TextureViews[Env->EnvTexture];
				}
				else if(i == 1)
				{
					Infos[i].imageView = TextureViews[Scene.IrradianceTexIdx];
				}
				else if(i == 2)
				{
					Infos[i].imageView = TextureViews[Scene.LutTexIdx];
				}

				Writes[i] = VkWriteDescriptorSet 
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = EnvTexDescriptors.back(),
					.dstBinding = i,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &Infos[i],
				};
			}
			vkUpdateDescriptorSets(rtg.device,uint32_t(Writes.size()),Writes.data(),0,nullptr);
		}
	}

	// SHADOW PASS
	// make a sampler for shadow map
	{
		VkSamplerCreateInfo CreateInfo 
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// used by sphere light cube shadow
			.compareEnable = VK_TRUE,
			.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		};
		VK(vkCreateSampler(rtg.device, &CreateInfo, nullptr, &ShadowData.ShadowSamplerPCF));
	}

	// dispatch shadow map
	{
		for (auto* Light : Scene.Lights) 
		{
			if (Light->LightType == ELightType::Spot && Light->ShadowResolution > 0)
			{
				GenerateShadowRes(Light);
			}
			else if(Light->LightType == ELightType::Sphere && Light->ShadowResolution > 0)
			{
				GenerateCubeShadowRes(Light);
			}
		}
	}
	// create descriptor pool for shadow map
	{
		VkDescriptorPoolSize PoolSize
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = ShadowData.MAX_SPOT_SHADOWS,
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 1,
			.poolSizeCount = 1,
			.pPoolSizes = &PoolSize,
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &ShadowDescriptorPool));
	}
	
	// allocate descriptor set for Shadowmap descriptor
	{
		VkDescriptorSetAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = ShadowDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &LambertPipeline.Set6_Shadowmap,
		};
		VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &ShadowDescriptors));
	}
	// write shadow map into descriptor
	{
		const std::vector<FShadowResource>& SpotLightShadows = ShadowData.SpotLightShadows;
		if(!SpotLightShadows.empty())
		{
			std::vector<VkDescriptorImageInfo> Infos(ShadowData.MAX_SPOT_SHADOWS);
			for (uint32_t i = 0; i < ShadowData.MAX_SPOT_SHADOWS; ++i)
			{
				if (i < SpotLightShadows.size())
				{
					Infos[i] = VkDescriptorImageInfo
					{
						.sampler = ShadowData.ShadowSamplerPCF,
						.imageView = SpotLightShadows[i].ImageView,
						.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
					};
				}
				else
				{
					Infos[i] = VkDescriptorImageInfo
					{
						.sampler = ShadowData.ShadowSamplerPCF,
						.imageView = SpotLightShadows[0].ImageView,
						.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
					};
				}
			}

			VkWriteDescriptorSet Write
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = ShadowDescriptors,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = ShadowData.MAX_SPOT_SHADOWS,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = Infos.data(),
			};

			vkUpdateDescriptorSets(rtg.device, 1, &Write, 0, nullptr);
		}
	}

}

URenderPipelines::~URenderPipelines()
{
    // just in case rendering is still in flight, don't destroy resources:
	// (not using VK macro to avoid throw-ing in destructor)
    if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) 
	{
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if(TextureDescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, TextureDescriptorPool, nullptr);
		TextureDescriptorPool = nullptr;

		// this also frees the descriptor sets allocated from the pool:
		MaterialDescriptors.clear();
	}

	if(EnvTexDescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, EnvTexDescriptorPool, nullptr);
		EnvTexDescriptorPool = nullptr;

		// this also frees the descriptor sets allocated from the pool:
		EnvTexDescriptors.clear();
	}

	if(ShadowDescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, ShadowDescriptorPool, nullptr);
		ShadowDescriptorPool = nullptr;
	}

	GBufferDebugPipeline.Destroy(rtg);

	if (GBufferDebugDescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, GBufferDebugDescriptorPool, nullptr);
		GBufferDebugDescriptorPool = VK_NULL_HANDLE;
	}

	if(DeferredLightingDescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, DeferredLightingDescriptorPool, nullptr);
		DeferredLightingDescriptorPool = VK_NULL_HANDLE;
	}

	if(TextureSamplerNearest)
	{
		vkDestroySampler(rtg.device, TextureSamplerNearest, nullptr);
		TextureSamplerNearest = VK_NULL_HANDLE;
	}

	if (TextureSamplerLinear)
	{
		vkDestroySampler(rtg.device, TextureSamplerLinear, nullptr);
		TextureSamplerLinear = VK_NULL_HANDLE;
	}

	for (VkImageView &View : TextureViews)
	{
		vkDestroyImageView(rtg.device, View, nullptr);
		View = VK_NULL_HANDLE;
	}
	TextureViews.clear();

	for (auto &Texture : Textures)
	{
		rtg.helpers.destroy_image(std::move(Texture));
	}
	Textures.clear();

	rtg.helpers.destroy_buffer(std::move(ObjectVertices));

	if (SwapchainDepthImage.handle != VK_NULL_HANDLE) 
	{
		DestroyFramebuffers();
	}

	VkSampler& ShadowSamplerPCF = ShadowData.ShadowSamplerPCF;
	if (ShadowSamplerPCF != VK_NULL_HANDLE) 
	{
    	vkDestroySampler(rtg.device, ShadowSamplerPCF, nullptr);
    	ShadowSamplerPCF = VK_NULL_HANDLE;
	}

	std::vector<FShadowResource>& SpotLightShadows = ShadowData.SpotLightShadows;
	for (auto& shadow : SpotLightShadows) 
	{
		if (shadow.Framebuffer != VK_NULL_HANDLE) 
		{
			vkDestroyFramebuffer(rtg.device, shadow.Framebuffer, nullptr);
			shadow.Framebuffer = VK_NULL_HANDLE;
		}
		if (shadow.ImageView != VK_NULL_HANDLE) 
		{
			vkDestroyImageView(rtg.device, shadow.ImageView, nullptr);
			shadow.ImageView = VK_NULL_HANDLE;
		}
		if (shadow.Image.handle != VK_NULL_HANDLE) 
		{
			rtg.helpers.destroy_image(std::move(shadow.Image));
		}
	}
	SpotLightShadows.clear();

	for (auto &shadow : ShadowData.SphereLightShadows)
	{
		for (uint32_t i = 0; i < 6; ++i)
		{
			if (shadow.FaceFramebuffers[i] != VK_NULL_HANDLE)
			{
				vkDestroyFramebuffer(rtg.device, shadow.FaceFramebuffers[i], nullptr);
				shadow.FaceFramebuffers[i] = VK_NULL_HANDLE;
			}

			if (shadow.FaceViews[i] != VK_NULL_HANDLE)
			{
				vkDestroyImageView(rtg.device, shadow.FaceViews[i], nullptr);
				shadow.FaceViews[i] = VK_NULL_HANDLE;
			}
		}

		if (shadow.Image.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_image(std::move(shadow.Image));
		}
	}
	ShadowData.SphereLightShadows.clear();

	for (FWorkspace &workspace : workspaces) 
	{
		if(workspace.command_buffer != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(rtg.device, CommandPool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}

		if(workspace.LinesVerticesSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.LinesVerticesSrc));
		}
		if(workspace.LinesVertices.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.LinesVertices));
		}

		if(workspace.CameraSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.CameraSrc));
		}
		if(workspace.Camera.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}

		if (workspace.WorldSrc.handle != VK_NULL_HANDLE) 
		{
			rtg.helpers.destroy_buffer(std::move(workspace.WorldSrc));
		}
		if (workspace.World.handle != VK_NULL_HANDLE) 
		{
			rtg.helpers.destroy_buffer(std::move(workspace.World));
		}
		//World_descriptors freed when pool is destroyed.

		if(workspace.TransformsSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.TransformsSrc));
		}
		if(workspace.Transforms.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
		}
		// Transforms_descriptors freed when pool is destroyed.

		if(workspace.LightsSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.LightsSrc));
		}
		if(workspace.Lights.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Lights));
		}
		// Lights_Descriptors freed when pool is destroyed.

		if (workspace.SSAOSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.SSAOSrc));
		}
		if (workspace.SSAO.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.SSAO));
		}
		// SSAO_Descriptors freed when pool is destroyed.
	}
	workspaces.clear();

	if(DescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, DescriptorPool, nullptr);
		DescriptorPool = nullptr;
		// (this also frees the descriptor sets allocated from the pool)
	}

	BackgroundPipeline.Destroy(rtg);
	LinesPipeline.Destroy(rtg);
	ShadowPipeline.Destroy(rtg);
    LambertPipeline.Destroy(rtg);

	DeferredGeometryPipeline.Destroy(rtg);
	// SSAO
	SSAOPipeline.Destroy(rtg);
	SSAOBlurPipeline.Destroy(rtg);

	DeferredLightingPipeline.Destroy(rtg);
	
	DestroyGBufferResources();
	DestroySSAOResources();
	DestroySSDOResources();

	// Destroy command pool
	if(CommandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(rtg.device, CommandPool, nullptr);
		CommandPool = VK_NULL_HANDLE;
	}

	if(RenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(rtg.device, RenderPass, nullptr);
		RenderPass = VK_NULL_HANDLE;
	}

	VkRenderPass& ShadowPass = ShadowData.ShadowPass;
	if(ShadowPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(rtg.device, ShadowPass, nullptr);
		ShadowPass= VK_NULL_HANDLE;
	}
}

void URenderPipelines::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain)
{
    // clean up existing framebuffers
	if(SwapchainDepthImage.handle != VK_NULL_HANDLE)
	{
		DestroyFramebuffers();
	}

	// allocate depth image for framebuffers to share
	SwapchainDepthImage = rtg.helpers.create_image
	(
		swapchain.extent,
		DepthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	// create an image view of the depth image
	{
		VkImageViewCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = SwapchainDepthImage.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = DepthFormat,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &CreateInfo, nullptr, &SwapchainDepthImageView));
	}

	// Make framebuffers for each swapchain image:
	SwapchainFramebuffer.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i)
	{
		std::array< VkImageView, 2 > Attachments
		{
			swapchain.image_views[i],
			SwapchainDepthImageView,
		};
		VkFramebufferCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = RenderPass,
			.attachmentCount = uint32_t(Attachments.size()),
			.pAttachments = Attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &CreateInfo, nullptr, &SwapchainFramebuffer[i]));
	}

	CreateGBufferTargets(swapchain.extent, swapchain.image_views.size());

	CreateSSAOTargets(swapchain.extent, swapchain.image_views.size());
	CreateSSAOBlurTargets(swapchain.extent, swapchain.image_views.size());
	CreateSSDOTargets(swapchain.extent, swapchain.image_views.size());
	CreateSSDOFilterTargets(swapchain.extent, swapchain.image_views.size());

	CreateGBufferDebugDescriptors();
	CreateDeferredLightingDescriptors();
	for (auto& workspace : workspaces)
	{
		CreateSSAODescriptors(workspace);
		CreateSSDODescriptors(workspace);
	}
	
	CreateSSAOBlurDescriptors();
	CreateSSDOFilterDescriptors();
}

void URenderPipelines::DestroyFramebuffers()
{
    for (VkFramebuffer &FrameBuffer : SwapchainFramebuffer)
	{
		assert(FrameBuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, FrameBuffer, nullptr);
		FrameBuffer = VK_NULL_HANDLE;
	}
	SwapchainFramebuffer.clear();

	assert(SwapchainDepthImageView != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, SwapchainDepthImageView, nullptr);
	SwapchainDepthImageView = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(SwapchainDepthImage));
}

void URenderPipelines::render(RTG &rtg_, RTG::RenderParams const &render_params)
{
	TRACE_SIMPLE_CLOCK("Render");
    //assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < SwapchainFramebuffer.size());

	//get more convenient names for the current workspace and target framebuffer:
	FWorkspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = SwapchainFramebuffer[render_params.image_index];

	// Reset the command buffer (clear old commands)
	VK(vkResetCommandBuffer(workspace.command_buffer, 0));
	{
		// Begin recording:
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// will record again every submit
		};
		VK(vkBeginCommandBuffer(workspace.command_buffer, &begin_info));
	}

	if(!Scene.MeshProxyInstances.empty())
	{
		// upload object transforms:
		size_t NeededBytes = Scene.MeshProxyInstances.size() * sizeof(URenderPipelines::FLambertPipeline::FTransform);
		if(workspace.TransformsSrc.handle == VK_NULL_HANDLE ||
			workspace.TransformsSrc.size < NeededBytes)
		{
			//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
			size_t NewBytes = ((NeededBytes + 4096) / 4096) * 4096;
			if(workspace.TransformsSrc.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.TransformsSrc));
			}
			if(workspace.Transforms.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
			}
			workspace.TransformsSrc = rtg.helpers.create_buffer
			(
				NewBytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // going to have GPU copy from this memory
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host-visible memory, coherent (no special sync needed)
				Helpers::Mapped // get a pointer to the memory
			);
			workspace.Transforms = rtg.helpers.create_buffer(
				NewBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // going to use as storage buffer, also going to have GPU into this memory
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU-local memory
				Helpers::Unmapped // don't get a pointer to the memory
			);

			// update the descriptor set:
			VkDescriptorBufferInfo TransformInfo
			{
				.buffer = workspace.Transforms.handle,
				.offset = 0,
				.range = workspace.Transforms.size,
			};

			std::array< VkWriteDescriptorSet, 1 > Writes
			{
				VkWriteDescriptorSet
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.TransformDescriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &TransformInfo,
				},
			};

			vkUpdateDescriptorSets
			(
				rtg.device,
				uint32_t(Writes.size()), Writes.data(), // descriptorWrites count, data
				0, nullptr // descriptorCopies count, data
			);

			std::cout << "Re-allocated object transforms buffers to " << NewBytes << " bytes." << std::endl;

			assert(workspace.TransformsSrc.size == workspace.Transforms.size);
			assert(workspace.TransformsSrc.size >= NeededBytes);
		}

		// Copy Transform into TransformSrc
		{
			assert(workspace.TransformsSrc.allocation.mapped);
			FMeshRenderProxy::FTransform *Out = reinterpret_cast< FMeshRenderProxy::FTransform * >(workspace.TransformsSrc.allocation.data()); // Strict aliasing violation, but it doesn't matter
			for (FMeshRenderProxy* Inst : Scene.MeshProxyInstances)
			{
				*Out = Inst->Transform;
				++Out;
			}
		}

		// device-side copy from Transforms_src -> Transforms:
		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = NeededBytes,
		};

		vkCmdCopyBuffer(workspace.command_buffer, workspace.TransformsSrc.handle, workspace.Transforms.handle, 1, &CopyRegion);
	}

	if(!Scene.LightProxyInstances.empty())
	{
		// upload object transforms:
		size_t NeededBytes = Scene.LightProxyInstances.size() * sizeof(FLightRenderProxy);
		if(workspace.LightsSrc.handle == VK_NULL_HANDLE ||
			workspace.LightsSrc.size < NeededBytes)
		{
			size_t NewBytes = ((NeededBytes + 4096) / 4096) * 4096;
			if(workspace.LightsSrc.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.LightsSrc));
			}
			if(workspace.Lights.handle)
			{
				rtg.helpers.destroy_buffer(std::move(workspace.Lights));
			}
			workspace.LightsSrc = rtg.helpers.create_buffer
			(
				NewBytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // going to have GPU copy from this memory
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host-visible memory, coherent (no special sync needed)
				Helpers::Mapped // get a pointer to the memory
			);
			workspace.Lights = rtg.helpers.create_buffer(
				NewBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // going to use as storage buffer, also going to have GPU into this memory
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU-local memory
				Helpers::Unmapped // don't get a pointer to the memory
			);

			// update the descriptor set:
			VkDescriptorBufferInfo LightsInfo
			{
				.buffer = workspace.Lights.handle,
				.offset = 0,
				.range = workspace.Lights.size,
			};

			std::array< VkWriteDescriptorSet, 1 > Writes
			{
				VkWriteDescriptorSet
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.LightsDescriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &LightsInfo,
				},
			};

			vkUpdateDescriptorSets
			(
				rtg.device,
				uint32_t(Writes.size()), Writes.data(), // descriptorWrites count, data
				0, nullptr // descriptorCopies count, data
			);

			std::cout << "Re-allocated light proxy buffers to " << NewBytes << " bytes." << std::endl;

			assert(workspace.LightsSrc.size == workspace.Lights.size);
			assert(workspace.LightsSrc.size >= NeededBytes);
		}

		// Copy Light into LightSrc
		{
			assert(workspace.LightsSrc.allocation.mapped);
			FLightRenderProxy *Out = reinterpret_cast< FLightRenderProxy * >(workspace.LightsSrc.allocation.data()); // Strict aliasing violation, but it doesn't matter
			for (FLightRenderProxy* Inst : Scene.LightProxyInstances)
			{
				*Out = *Inst;
				++Out;
			}
		}

		// device-side copy from Transforms_src -> Transforms:
		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = NeededBytes,
		};

		vkCmdCopyBuffer(workspace.command_buffer, workspace.LightsSrc.handle, workspace.Lights.handle, 1, &CopyRegion);
	}

	// Line Render Pipeline
	{
		// Upload line vertices
		if(!LinesVertices.empty())
		{
			// [re-]allocate lines buffers if needed:
			size_t NeededBytes = LinesVertices.size() * sizeof(LinesVertices[0]);
			if(workspace.LinesVerticesSrc.handle == VK_NULL_HANDLE ||
				workspace.LinesVerticesSrc.size < NeededBytes)
			{
				size_t NewBytes = ((NeededBytes + 4096) / 4096) * 4096;
				if(workspace.LinesVerticesSrc.handle)
				{
					rtg.helpers.destroy_buffer(std::move(workspace.LinesVerticesSrc));
				}
				if(workspace.LinesVertices.handle)
				{
					rtg.helpers.destroy_buffer(std::move(workspace.LinesVertices));
				}

				workspace.LinesVerticesSrc = rtg.helpers.create_buffer
				(
					NewBytes,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 											// going to have GPU copy from this memory
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host-visible memory, coherent (no special sync needed)
					Helpers::Mapped 															// get a pointer to the memory
				);
				workspace.LinesVertices = rtg.helpers.create_buffer
				(
					NewBytes,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 	// going to use as vertex buffer, also going to have GPU into this memory
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 									// GPU-local memory
					Helpers::Unmapped 														// don't get a pointer to the memory
				);

				std::cout << "Re-allocated lines buffers to " << NewBytes << " bytes." << std::endl;
			}

			assert(workspace.LinesVerticesSrc.size == workspace.LinesVertices.size);
			assert(workspace.LinesVerticesSrc.size >= NeededBytes);

			// Host-side copy into LinesVerticesSrc:
			assert(workspace.LinesVerticesSrc.allocation.mapped);
			std::memcpy(workspace.LinesVerticesSrc.allocation.data(), LinesVertices.data(), NeededBytes);

			// Device-side copy from LinesVerticesSrc -> LineVertices
			VkBufferCopy CopyRegion
			{
				.srcOffset = 0,
				.dstOffset = 0,
				.size = NeededBytes,
			};
			vkCmdCopyBuffer(workspace.command_buffer, workspace.LinesVerticesSrc.handle, 
							workspace.LinesVertices.handle, 1, &CopyRegion);
		}
	}
	
	// upload camera info:
	{ 
		FLambertPipeline::FCamera Camera
		{
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD
		};
		assert(workspace.CameraSrc.size == sizeof(Camera));

		// host-side copy into Camera_src:
		memcpy(workspace.CameraSrc.allocation.data(), &Camera, sizeof(Camera));

		// add device-side copy from Camera_src -> Camera:
		assert(workspace.CameraSrc.size == workspace.Camera.size);
		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.CameraSrc.size,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.CameraSrc.handle, workspace.Camera.handle, 1, &CopyRegion);
	}

	// upload world info:
	{
		assert(workspace.WorldSrc.size == sizeof(World));

		//host-side copy into World_src:
		memcpy(workspace.WorldSrc.allocation.data(), &World, sizeof(World));

		//add device-side copy from World_src -> World:
		assert(workspace.WorldSrc.size == workspace.World.size);
		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.WorldSrc.size,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.WorldSrc.handle, workspace.World.handle, 1, &CopyRegion);
	}

	// upload UBO Info
	{
		UpdateSSAOBuffer(workspace);
		UpdateSSDOBuffer(workspace);
	}

	// Memory Barrier
	{
		// Memory barrier to make sure copies complete before rendering happens:
		VkMemoryBarrier MemoryBarrier
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		};

		vkCmdPipelineBarrier
		(
			workspace.command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,  // srcStageMask
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
			0, 					// dependencyFlags
			1, &MemoryBarrier,  // memoryBarriers (count, data)
			0, nullptr,  		// bufferMemoryBarriers (count, data)
			0, nullptr			// imageMemoryBarriers (count, data)
		);
	}

	// Shadow map render
	RenderShadowMaps(workspace);

	ViewportFullScreen(workspace);

	// Render G-Buffer
	RenderDeferredGeometryPass(workspace, render_params.image_index);

	// Barrier
	for (size_t i = 0; i < GBufferData.GBufferImages.size(); i++)
	{
		VkImageMemoryBarrier gbufferBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = GBufferData.GBufferImages[i].handle,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier
		(
			workspace.command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &gbufferBarrier
		);
	}
	
	RenderSSAOPass(workspace, render_params.image_index);
	RenderSSAOBlurPass(workspace, render_params.image_index);

	RenderSSDOPass(workspace, render_params.image_index);
	RenderSSDOFilterPass(workspace, render_params.image_index);

	// Render Pass
	{
		std::array<VkClearValue, 2> clear_values
		{
			VkClearValue{ .color{ .float32{.0f, .0f, 0.f, 1.0f}}},
			VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0}},
		};

		VkRenderPassBeginInfo begin_info
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = RenderPass,
			.framebuffer = framebuffer,
			.renderArea
			{
				.offset = { .x = 0, .y = 0},
				.extent = rtg.swapchain_extent,
			},
			.clearValueCount = uint32_t(clear_values.size()),
			.pClearValues = clear_values.data(),
		};

		vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);


		if (GBufferDebugView == EGBufferDebugView::None)
		{
			// ViewportPillarBoxing(workspace);

			// RenderLambertPipeline(workspace);

			ViewportPillarBoxing(workspace);

			RenderDeferredLightingPipeline(workspace);
			
			RenderLinesPipeline(workspace);
		}
		else
		{
			ViewportPillarBoxing(workspace);

			RenderGBufferDebugPipeline(workspace);
			
			RenderLinesPipeline(workspace);
		}
		
		vkCmdEndRenderPass(workspace.command_buffer);
	}

	// end recording:
	VK(vkEndCommandBuffer(workspace.command_buffer));

	//submit `workspace.command buffer` for the GPU to run:
	{
		std::array< VkSemaphore, 1 > WaitSemaphores
		{
			render_params.image_available
		};
		std::array< VkPipelineStageFlags, 1 > WaitStages
		{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(WaitSemaphores.size() == WaitStages.size(), "every semaphore needs a stage");

		
		std::array< VkSemaphore, 1 > SignalSemaphores
		{
			render_params.image_done
		};
		VkSubmitInfo SubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(WaitSemaphores.size()),
			.pWaitSemaphores = WaitSemaphores.data(),
			.pWaitDstStageMask = WaitStages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(SignalSemaphores.size()),
			.pSignalSemaphores = SignalSemaphores.data(),
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &SubmitInfo, render_params.workspace_available));
	}
}

void URenderPipelines::update(float dt)
{
	TRACE_SIMPLE_CLOCK("Update");
    Time = std::fmod(Time + dt, 60.0f);

	if(bIsPlay)
	{
		UAnimPlayer::UpdateAnimations(dt);
	}
	if(rtg.configuration.debug)
	{
		DebugScene.Update(Scene.Cameras, ActiveCameraIdx, Scene.Nodes, Scene.LightProxyInstances);
	}
	{
		Scene.Update(ActiveCameraIdx, rtg.configuration.CullMode == RTG::Configuration::ECullingMode::Normal);
	}
	UpdateCamera();
    DrawDebugLines();
}

void URenderPipelines::on_input(InputEvent const &evt)
{
    // If there is a current action, it gets input priority:
	if(Action)
	{
		Action(evt);
		return;
	}

	// General Controls:
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_TAB)
	{
		// Switch Camera Modes
		CameraMode = ECameraMode((int(CameraMode) + 1) % 2);
		return;
	}
	// Switch scene's cameras:
	if (evt.type == InputEvent::KeyDown && (evt.key.key == GLFW_KEY_EQUAL || evt.key.key == GLFW_KEY_KP_ADD))
	{
		if(Scene.Cameras.size() >= 1)
		{
			ActiveCameraIdx = (ActiveCameraIdx + 1) % Scene.Cameras.size(); 
			return;
		}
	}

	// Free Camera Controls
	if(CameraMode == ECameraMode::Free)
	{
		if(evt.type == InputEvent::MouseWheel)
		{
			// change distance by 10% every scroll click:
			FreeCamera.Radius *= std::exp(std::log(1.1f) * -evt.wheel.y);
			// make sure camera isn't too close or too far from target:
			FreeCamera.Radius = std::max(FreeCamera.Radius, 0.5f * FreeCamera.Near);
			FreeCamera.Radius = std::min(FreeCamera.Radius, 2.0f * FreeCamera.Far);
			return;
		}
		
		if(evt.type == InputEvent::MouseButtonDown)
		{
			// start panning
			float InitX = evt.button.x;
			float InitY = evt.button.y;
			FOrbitCamera InitCamera = FreeCamera;
			if (evt.button.button == GLFW_MOUSE_BUTTON_LEFT)
			{
				Action = [this, InitX, InitY, InitCamera](InputEvent const &evt)
				{
					if(evt.type == InputEvent::MouseButtonUp &&
						evt.button.button == GLFW_MOUSE_BUTTON_LEFT)
					{
						// Cancel upon button lifted:
						Action = nullptr;
						return;
					}
					if(evt.type == InputEvent::MouseMotion)
					{
						float Height = 2.0f * std::tan(FreeCamera.FOV * 0.5f) * FreeCamera.Radius;

						//motion, therefore, at target point:
						float Dx = (evt.motion.x - InitX) / rtg.swapchain_extent.height * Height;
						float Dy =-(evt.motion.y - InitY) / rtg.swapchain_extent.height * Height; //note: negated because glfw uses y-down coordinate system

						//compute camera transform to extract right (first row) and up (second row):
						mat4 CameraFromWorld = orbit
						(
							InitCamera.TargetX, InitCamera.TargetY, InitCamera.TargetZ,
							InitCamera.Azimuth, InitCamera.Elevation, InitCamera.Radius
						);

						// move the desired distance:
						vec3 InitTarget = vec3(InitCamera.TargetX, InitCamera.TargetY, InitCamera.TargetZ);
						vec3 Right = vec3(CameraFromWorld[0]);
						vec3 Up = vec3(CameraFromWorld[1]);
						vec3 NewTarget = InitTarget - (Dx * Right) - (Dy * Up);

						FreeCamera.TargetX = NewTarget.x;
						FreeCamera.TargetY = NewTarget.y;
						FreeCamera.TargetZ = NewTarget.z;

						return;
					}
				};
			}
			if (evt.button.button == GLFW_MOUSE_BUTTON_MIDDLE) 
			{
				Action = [this, InitX, InitY, InitCamera](InputEvent const &evt) 
				{
					if (evt.type == InputEvent::MouseButtonUp && evt.button.button == GLFW_MOUSE_BUTTON_MIDDLE) 
					{
						Action = nullptr;
						return;
					}
					if (evt.type == InputEvent::MouseMotion) 
					{
						float Height = 2.0f * std::tan(FreeCamera.FOV * 0.5f) * FreeCamera.Radius;
						
						float Dx = (evt.motion.x - InitX) / (float)rtg.swapchain_extent.height * Height;
						float Dy = -(evt.motion.y - InitY) / (float)rtg.swapchain_extent.height * Height;

						mat4 CameraFromWorld = orbit(
							InitCamera.TargetX, InitCamera.TargetY, InitCamera.TargetZ,
							InitCamera.Azimuth, InitCamera.Elevation, InitCamera.Radius
						);

						vec3 Right = vec3(CameraFromWorld[0][0], CameraFromWorld[1][0], CameraFromWorld[2][0]);
						vec3 Up    = vec3(CameraFromWorld[0][1], CameraFromWorld[1][1], CameraFromWorld[2][1]);

						vec3 InitTarget = vec3(InitCamera.TargetX, InitCamera.TargetY, InitCamera.TargetZ);
						
						vec3 NewTarget = InitTarget - (Dx * Right) - (Dy * Up);

						FreeCamera.TargetX = NewTarget.x;
						FreeCamera.TargetY = NewTarget.y;
						FreeCamera.TargetZ = NewTarget.z;
					}
				};
				return;
			}
			
		}

		if(evt.type == InputEvent::MouseButtonDown &&
			 evt.button.button ==  GLFW_MOUSE_BUTTON_LEFT)
		{
			// Start tumbling

			float InitX = evt.button.x;
			float InitY = evt.button.y;
			FOrbitCamera InitCamera = FreeCamera;
			
			Action = [this, InitX, InitY, InitCamera](InputEvent const &evt) {
				if (evt.type == InputEvent::MouseButtonUp &&
					 evt.button.button == GLFW_MOUSE_BUTTON_LEFT) 
				{
					//cancel upon button lifted:
					Action = nullptr;
					return;
				}
				if (evt.type == InputEvent::MouseMotion) 
				{
					float Dx = (evt.motion.x - InitX) / rtg.swapchain_extent.height;
					float Dy = (evt.motion.y - InitY) / rtg.swapchain_extent.height; //note: negated because glfw uses y-down coordinate system
					
					// Rotate camera based on motion:
					float Speed = float(M_PI);	// how much rotation happens at one full window height
					float FlipX = (std::abs(InitCamera.Elevation) > 0.5f * float(M_PI) ? -1.0f : 1.0f); // switch azimuth rotation when camera is upside-down
					FreeCamera.Azimuth = InitCamera.Azimuth -Dx * Speed * FlipX;
					FreeCamera.Elevation = InitCamera.Elevation - Dy * Speed;
					
					// Reduce Azimuth and elevation to [-pi, pi] range
					const float TwoPi = 2.0f * float(M_PI);
					FreeCamera.Azimuth -= std::round(FreeCamera.Azimuth / TwoPi) * TwoPi;
					FreeCamera.Elevation -= std::round(FreeCamera.Elevation / TwoPi) * TwoPi;
					return;
				}
			};

			return;
		}
	}

	// Switch Debug mode
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_SLASH)
	{
		rtg.configuration.debug = !rtg.configuration.debug;
		return;
	}
	// Gbuffer - Albedo
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_1)
	{
		if (GBufferDebugView == EGBufferDebugView::Albedo)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::Albedo;
		return;
	}
	// Gbuffer - Normal
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_2)
	{
		if (GBufferDebugView == EGBufferDebugView::Normal)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::Normal;
		return;
	}
	// Gbuffer - Position
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_3)
	{
		if (GBufferDebugView == EGBufferDebugView::Position)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::Position;
		return;
	}
	// Gbuffer - Roughness
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_4)
	{
		if (GBufferDebugView == EGBufferDebugView::Roughness)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::Roughness;
		return;
	}
	// Gbuffer - Metalness
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_5)
	{
		if (GBufferDebugView == EGBufferDebugView::Metalness)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::Metalness;
		return;
	}
	// Screen processing - SSAO
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_6)
	{
		if (GBufferDebugView == EGBufferDebugView::SSAO)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::SSAO;
		return;
	}
	// Screen processing - SSDO
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_7)
	{
		if (GBufferDebugView == EGBufferDebugView::SSDO)
			GBufferDebugView = EGBufferDebugView::None;
		else
			GBufferDebugView = EGBufferDebugView::SSDO;
		return;
	}

	// Animation Play
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_SPACE)
	{
		bIsPlay = !bIsPlay;
	}
	
	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_P)
	{
		UAnimPlayer::ResetAnimationsTime();
	}

	if(evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_C)
	{
		rtg.configuration.CullMode = RTG::Configuration::ECullingMode((int(rtg.configuration.CullMode) + 1) % 2);
	}
}

//~BEGIN Load Scene
void URenderPipelines::InitializeRenderScene()
{
	URenderExtractor::BuildRenderScene(Scene, rtg);
	// Generate RenderProxy
    Scene.GenerateMeshProxy();
    Scene.GenerateLightProxy();
	// Data Prepare
    Scene.GenerateWholeVertexBuffer();
	Scene.GenerateFallbackResource();
}
//~END Load Scene

//~BEGIN Load Texture
void URenderPipelines::ReserveTextures()
{
    const std::vector<UTexture*>& TexturesData = Scene.Textures;
    Textures.reserve(TexturesData.size());
    for (uint32_t i = 0; i < TexturesData.size(); i++)
    {
		const UTexture* Texture = TexturesData[i];
		const size_t MipmapsCount = Texture->MipmapsData.size();
		std::vector<uint8_t> CombinedData;
		// Combine all mipmapsData
		for (uint32_t mip = 0; mip < MipmapsCount; ++mip)
		{
			const UTexture::FTextureMipMap& MipmapData = *(Texture->MipmapsData[mip]);
    		const uint8_t* src = reinterpret_cast<const uint8_t*>(MipmapData.BulkData.data());
    		size_t mipSize = sizeof(MipmapData.BulkData[0]) * MipmapData.BulkData.size();
    		CombinedData.insert(CombinedData.end(), src, src + mipSize);
		}
		
        const UTexture::FTextureMipMap& MipmapData = *(Texture->MipmapsData[0]);
		
		VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;
		switch (Texture->Type)
		{
			case UTexture::EType::Flat:
				switch (Texture->Format)
				{
					case UTexture::EFormat::SRGB:
						Format = VK_FORMAT_R8G8B8A8_SRGB;
						break;
					case UTexture::EFormat::Linear:
						Format = VK_FORMAT_R8G8B8A8_UNORM;
						break;
					case UTexture::EFormat::RGBE:
						Format = VK_FORMAT_R32G32B32A32_SFLOAT;
						break;
				}
			Textures.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = MipmapData.SizeX , .height = MipmapData.SizeY }, //size of image
				Format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
				Helpers::Unmapped,
				1,
				0u,
				(uint32_t)MipmapsCount
				));
				break;

			case UTexture::EType::Cube:
				uint32_t faceSize = MipmapData.SizeX;
				Textures.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = faceSize , .height = faceSize }, //size of image
				VK_FORMAT_R32G32B32A32_SFLOAT, //how to interpret image data (in this case, 32 FRGBA)
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
				Helpers::Unmapped,
				6,
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
				(uint32_t)MipmapsCount
				));
				break;
		}
        // transfer data
        rtg.helpers.transfer_to_image(CombinedData.data(), CombinedData.size(), Textures.back());
    }
}
//~END Load Texture

//~BEGIN Viewport and Camera
void URenderPipelines::ViewportPillarBoxing(FWorkspace &workspace)
{
    float TargetAspect = DefaultAspect;
    if (CameraMode == ECameraMode::Scene) 
    {
        TargetAspect = Scene.Cameras[ActiveCameraIdx]->Projection.Aspect;
    }

    float Width = float(rtg.swapchain_extent.width);
    float Height = float(rtg.swapchain_extent.height);
    float CurrentAspect = Width / Height;

    float TargetWidth, TargetHeight;
    float OffsetX = 0.0f;
    float OffsetY = 0.0f;

    if (CurrentAspect > TargetAspect) 
    {
        TargetHeight = Height;
        TargetWidth = Height * TargetAspect;
        OffsetX = (Width - TargetWidth) / 2.0f;
    } 
    else 
    {
        TargetWidth = Width;
        TargetHeight = Width / TargetAspect;
        OffsetY = (Height - TargetHeight) / 2.0f;
    }

    {
        VkViewport Viewport 
        {
            .x = OffsetX,
            .y = OffsetY,
            .width = TargetWidth,
            .height = TargetHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(workspace.command_buffer, 0, 1, &Viewport);
    }

    {
        VkRect2D Scissor 
        {
            .offset = { .x = int32_t(OffsetX), .y = int32_t(OffsetY) },
            .extent = { uint32_t(TargetWidth), uint32_t(TargetHeight) },
        };
        vkCmdSetScissor(workspace.command_buffer, 0, 1, &Scissor);
    }
}

void URenderPipelines::UpdateCamera()
{
	// camera orbiting the origin:
	if(CameraMode == ECameraMode::Scene)
    {
        const UCamera& NowCamera = *(Scene.Cameras[ActiveCameraIdx]);
		mat4 Projection = Perspective
		(
			NowCamera.Projection.Vfov,
			NowCamera.Projection.Aspect,
			NowCamera.Projection.Near,
			NowCamera.Projection.Far
		);
		mat4 ViewInverse = UNode::GetLocal2WorldMatrix(NowCamera.BindingNode);
		vec3 WorldSpaceCameraPos = vec3(ViewInverse[3]);
		World.VIEW_POS.x = WorldSpaceCameraPos.x;
		World.VIEW_POS.y = WorldSpaceCameraPos.y;
		World.VIEW_POS.z = WorldSpaceCameraPos.z;
		mat4 View = glm::inverse(ViewInverse);

		PROJECTION = Projection;
		VIEW_FROM_WORLD = View;
		INV_PROJECTION = glm::inverse(Projection);
		CLIP_FROM_WORLD = Projection * View;
    }
	else if(CameraMode == ECameraMode::Free)
	{
		mat4 Orbit = orbit
		(
			FreeCamera.TargetX, FreeCamera.TargetY, FreeCamera.TargetZ,
			FreeCamera.Azimuth, FreeCamera.Elevation, FreeCamera.Radius
		);
		mat4 ViewInverse = glm::inverse(Orbit);

		vec3 WorldSpaceCameraPos = vec3(ViewInverse[3]);
		World.VIEW_POS.x = WorldSpaceCameraPos.x;
		World.VIEW_POS.y = WorldSpaceCameraPos.y;
		World.VIEW_POS.z = WorldSpaceCameraPos.z;

		mat4 Projection = Perspective
		(
			FreeCamera.FOV,
			DefaultAspect,
			FreeCamera.Near,
			FreeCamera.Far
		);

		PROJECTION = Projection;
		VIEW_FROM_WORLD = Orbit;
		INV_PROJECTION = glm::inverse(Projection);
		CLIP_FROM_WORLD = Projection * Orbit;
	}
	else
	{
		assert(0 && "Only Two Camera Mode!");
	}
}
//~END Viewport and Camera

//~BEGIN Debug
void URenderPipelines::InitializeDebugRenderScene()
{
    // Get Vertices of BBox
    DebugScene.UpdateBBoxVertices(Scene.Nodes);

    // Get Vertices of Frustum
	DebugScene.UpdateFrustumVertices(Scene.Cameras, ActiveCameraIdx);
}

void URenderPipelines::DrawDebugLines()
{
    // For now, we just clear everything and push data into it again and again, maybe could be done as proxy
    LinesVertices.clear();
    if(rtg.configuration.debug)
    {
        DebugScene.GetAllVerticesData(LinesVertices);
    }
}
// ~END Debug

//~BEGIN Render Pipeline
void URenderPipelines::RenderBackgroundPipeline(FWorkspace &workspace)
{
    // draw with the background pipeline:
	{
		vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, BackgroundPipeline.Handle);
		
		// Push time here
		{
			FBackgroundPipeline::FPush push
			{
				.time = Time,
			};
			vkCmdPushConstants(workspace.command_buffer, BackgroundPipeline.Layout, 
								VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		}
		vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
	}
}

void URenderPipelines::RenderLinesPipeline(FWorkspace &workspace)
{
	// Draw with the lines pipeline:
	{
		vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
						LinesPipeline.Handle);
		{
			// Use LinesVertices (offset 0) as vertex buffer binding 0:
			std::array< VkBuffer, 1 > VertexBuffers{ workspace.LinesVertices.handle };
			std::array< VkDeviceSize, 1 > Offsets{ 0 };
			vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(VertexBuffers.size()),
									VertexBuffers.data(), Offsets.data());
		}

		// bind Camera descriptor set:
		{
			std::array< VkDescriptorSet, 1 > DescriptorSets
			{
				workspace.CameraDescriptors,
			};
			vkCmdBindDescriptorSets
			(
				workspace.command_buffer, 			// command buffer
				VK_PIPELINE_BIND_POINT_GRAPHICS, 	// pipeline bind point
				LinesPipeline.Layout, 				// pipeline layout
				0, 									// first set
				uint32_t(DescriptorSets.size()),
				DescriptorSets.data(), 				// descriptor sets count, ptr
				0, nullptr 							// dynamic offsets count, ptr
			);
		}
		// Draw Lines vertices
		vkCmdDraw(workspace.command_buffer, uint32_t(LinesVertices.size()), 1, 0, 0);
	}
}

void URenderPipelines::RenderLambertPipeline(FWorkspace &workspace)
{
    if (Scene.MeshProxyInstances.empty())
    {
        return;
    }

    vkCmdBindPipeline(
        workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        LambertPipeline.Handle
    );

    {
        std::array<VkBuffer, 1> VertexBuffers{ ObjectVertices.handle };
        std::array<VkDeviceSize, 1> Offsets{ 0 };
        vkCmdBindVertexBuffers(
            workspace.command_buffer,
            0,
            uint32_t(VertexBuffers.size()),
            VertexBuffers.data(),
            Offsets.data()
        );
    }

    const std::vector<FMeshRenderProxy*>& ProxyList = Scene.MeshProxyInstances;

    for (uint32_t i = 0; i < ProxyList.size(); ++i)
    {
        const FMeshRenderProxy* Proxy = ProxyList[i];
        if (!Proxy || !Proxy->bCanSee)
        {
            continue;
        }

        const FMaterial* Material = Scene.Materials[Proxy->MaterialIdx];
        if (!Material)
        {
            continue;
        }

        FLambertPipeline::FConstant Constant
        {
            .MaterialType = int(Material->Type),
            .LightsCount  = int(Scene.Lights2Nodes.size()),
        };

        vkCmdPushConstants(
            workspace.command_buffer,
            LambertPipeline.Layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Constant),
            &Constant
        );

        std::array<VkDescriptorSet, 7> DescriptorSets
        {
            workspace.CameraDescriptors,                  // set 0
            workspace.WorldDescriptors,                   // set 1
            workspace.TransformDescriptors,               // set 2
            MaterialDescriptors[Proxy->MaterialIdx],      // set 3
            workspace.LightsDescriptors,                  // set 4
            EnvTexDescriptors[0],                         // set 5
            ShadowDescriptors                             // set 6
        };

        vkCmdBindDescriptorSets(
            workspace.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            LambertPipeline.Layout,
            0,
            uint32_t(DescriptorSets.size()),
            DescriptorSets.data(),
            0,
            nullptr
        );

        vkCmdDraw(
            workspace.command_buffer,
            Proxy->VertexNum,
            1,
            Proxy->FirstVertexIdx,
            i
        );
    }
}

void URenderPipelines::RenderShadowMaps(FWorkspace& Workspace)
{
	const std::vector<FShadowResource>& SpotLightShadows = ShadowData.SpotLightShadows;
	if(SpotLightShadows.empty() || Scene.MeshProxyInstances.empty())
	{
		return;
	}

	for (size_t i = 0; i < SpotLightShadows.size(); ++i)
	{
		const FShadowResource& Shadow = SpotLightShadows[i];

        if (i >= Scene.SpotLightsMapProxyInstances.size())
		{
			break;
		}

        FLightRenderProxy* LightProxy = Scene.SpotLightsMapProxyInstances[i];
        if (!LightProxy)
		{
			continue;
		}

        VkClearValue ClearValue{};
        ClearValue.depthStencil.depth = 1.0f;
        ClearValue.depthStencil.stencil = 0;

        VkRenderPassBeginInfo BeginInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = ShadowData.ShadowPass,
            .framebuffer = Shadow.Framebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = { Shadow.Resolutions, Shadow.Resolutions }
            },
            .clearValueCount = 1,
            .pClearValues = &ClearValue,
        };

        vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		vkCmdBindPipeline(Workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Handle);

		{
			std::array<VkBuffer, 1> VertexBuffers{ ObjectVertices.handle };
			std::array<VkDeviceSize, 1> Offsets{ 0 };
			vkCmdBindVertexBuffers(Workspace.command_buffer, 0, uint32_t(VertexBuffers.size()), VertexBuffers.data(), Offsets.data());
		}

		// transforms descriptor
		{
			vkCmdBindDescriptorSets(Workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Layout, 
									0, 1, &Workspace.TransformDescriptors, 0, nullptr);
		}

        VkViewport Viewport
        {
            .x = 0.0f,
            .y = 0.0f,
            .width = float(Shadow.Resolutions),
            .height = float(Shadow.Resolutions),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(Workspace.command_buffer, 0, 1, &Viewport);

        VkRect2D Scissor
        {
            .offset = {0, 0},
            .extent = { Shadow.Resolutions, Shadow.Resolutions },
        };
        vkCmdSetScissor(Workspace.command_buffer, 0, 1, &Scissor);

        FShadowPipeline::FPush Push
        {
            .SHADOW_CLIP_FROM_WORLD = LightProxy->LIGHT_FROM_WORLD
        };

        vkCmdPushConstants
		(
            Workspace.command_buffer,
            ShadowPipeline.Layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Push),
            &Push
        );

        for (uint32_t meshIdx = 0; meshIdx < Scene.MeshProxyInstances.size(); ++meshIdx)
        {
            const FMeshRenderProxy* Proxy = Scene.MeshProxyInstances[meshIdx];

            vkCmdDraw
			(
                Workspace.command_buffer,
                Proxy->VertexNum,
                1,
                Proxy->FirstVertexIdx,
                meshIdx
            );
        }
        vkCmdEndRenderPass(Workspace.command_buffer);
		
		{
			VkImageMemoryBarrier barrier
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = SpotLightShadows[i].Image.handle,
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			vkCmdPipelineBarrier
			(
				Workspace.command_buffer,
				VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
	}

}

void URenderPipelines::RenderCubeShadowMaps(FWorkspace& Workspace)
{
	const std::vector<FCubeShadowResource>& SphereLightShadows = ShadowData.SphereLightShadows;
	if(SphereLightShadows.empty() || Scene.MeshProxyInstances.empty())
	{
		return;
	}

	for (size_t lightIdx = 0; lightIdx < SphereLightShadows.size(); ++lightIdx)
    {
        const FCubeShadowResource& Shadow = SphereLightShadows[lightIdx];

        for (uint32_t FaceIdx = 0; FaceIdx < 6; ++FaceIdx)
        {
            VkClearValue ClearValue{};
            ClearValue.depthStencil.depth = 1.0f;

            VkRenderPassBeginInfo BeginInfo
			{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = ShadowData.ShadowPass,
                .framebuffer = Shadow.FaceFramebuffers[FaceIdx],
                .renderArea = 
				{
                    .offset = {0, 0},
                    .extent = {Shadow.Resolution, Shadow.Resolution}
                },
                .clearValueCount = 1,
                .pClearValues = &ClearValue,
            };

            vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(Workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Handle);

            std::array<VkBuffer, 1> VertexBuffers{ ObjectVertices.handle };
            std::array<VkDeviceSize, 1> Offsets{ 0 };
            vkCmdBindVertexBuffers(Workspace.command_buffer, 0, 1, VertexBuffers.data(), Offsets.data());

            vkCmdBindDescriptorSets
			(
                Workspace.command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                ShadowPipeline.Layout,
                0, 1, &Workspace.TransformDescriptors,
                0, nullptr
            );

            VkViewport Viewport
			{
                .x = 0.0f,
                .y = 0.0f,
                .width = float(Shadow.Resolution),
                .height = float(Shadow.Resolution),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(Workspace.command_buffer, 0, 1, &Viewport);

            VkRect2D Scissor
			{
                .offset = {0, 0},
                .extent = {Shadow.Resolution, Shadow.Resolution},
            };
            vkCmdSetScissor(Workspace.command_buffer, 0, 1, &Scissor);

            FShadowPipeline::FPush Push
			{
                .SHADOW_CLIP_FROM_WORLD = Shadow.ShadowClipFromWorld[FaceIdx]
            };
            vkCmdPushConstants
			(
                Workspace.command_buffer,
                ShadowPipeline.Layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(Push),
                &Push
            );

            for (uint32_t meshIdx = 0; meshIdx < Scene.MeshProxyInstances.size(); ++meshIdx)
            {
                const FMeshRenderProxy* proxy = Scene.MeshProxyInstances[meshIdx];
                vkCmdDraw
				(
                    Workspace.command_buffer,
                    proxy->VertexNum,
                    1,
                    proxy->FirstVertexIdx,
                    meshIdx
                );
            }

            vkCmdEndRenderPass(Workspace.command_buffer);
        }
    }
}
//~END Render Pipeline

//~BEGIN CommandLine Settings
void URenderPipelines::InitializeCommandLineSettings()
{
	// Active Camera
	const std::vector<UCamera*>& Cameras = Scene.Cameras;
	bool hasFound = false;
	for (uint8_t i = 0; i < Cameras.size(); i++)
	{
		if(Cameras[i] && Cameras[i]->Name == rtg.configuration.CameraName)
		{
			hasFound = true;
			ActiveCameraIdx = i;
			CameraMode = ECameraMode::Scene;
		}
	}
	if(!hasFound)
	{
		CameraMode = ECameraMode::Free;
	}

	// tonemapping and exposure
	World.AJUST_VAR.exposure = rtg.configuration.ExposureIntensity;
	World.AJUST_VAR.tonemappingMode = (float)rtg.configuration.TonemappingMode;
	
}
//~END CommandLine Settings

//~BEGIN Shadow
void URenderPipelines::GenerateShadowRes(const ULight* Light)
{
	FShadowResource ShadowRes;
	const uint32_t ShadowResolution = (uint32_t)Light->ShadowResolution;
	ShadowRes.Resolutions = ShadowResolution;

	ShadowRes.Image = rtg.helpers.create_image
	(
		VkExtent2D{ .width = ShadowResolution,  .height = ShadowResolution},
		DepthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	
	VkImageViewCreateInfo ViewInfo 
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = ShadowRes.Image.handle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = DepthFormat,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 },
	};
	VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &ShadowRes.ImageView));

	VkFramebufferCreateInfo FrameBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = ShadowData.ShadowPass,
		.attachmentCount = 1,
		.pAttachments = &ShadowRes.ImageView,
		.width = ShadowResolution,
		.height = ShadowResolution,
		.layers = 1,
	};
	VK(vkCreateFramebuffer(rtg.device, &FrameBufferInfo, nullptr, &ShadowRes.Framebuffer));

	ShadowData.SpotLightShadows.push_back(std::move(ShadowRes));
}

void URenderPipelines::GenerateCubeShadowRes(const ULight* Light)
{
	FCubeShadowResource CubeShadowRes;
	const uint32_t ShadowResolution = (uint32_t)Light->ShadowResolution;
	CubeShadowRes.Resolution = ShadowResolution;

	CubeShadowRes.Image = rtg.helpers.create_image
	(
		VkExtent2D{ .width = ShadowResolution, .height = ShadowResolution },
		DepthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped,
		6, // layers
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		1  // mipLevels
	);
	
	VkImageViewCreateInfo CubeViewInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = CubeShadowRes.Image.handle,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = DepthFormat,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 6,
		},
	};

	for (uint8_t i = 0; i < 6; i++)
	{
		VkImageViewCreateInfo FaceViewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = CubeShadowRes.Image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = DepthFormat,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = i,
				.layerCount = 1,
			},
		};
		VK(vkCreateImageView(rtg.device, &FaceViewInfo, nullptr, &CubeShadowRes.FaceViews[i]));

		VkFramebufferCreateInfo FrameBufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = ShadowData.ShadowPass,
			.attachmentCount = 1,
			.pAttachments = &CubeShadowRes.FaceViews[i],
			.width = ShadowResolution,
			.height = ShadowResolution,
			.layers = 1,
		};
		VK(vkCreateFramebuffer(rtg.device, &FrameBufferInfo, nullptr, &CubeShadowRes.FaceFramebuffers[i]));
	}

	ShadowData.SphereLightShadows.push_back(std::move(CubeShadowRes));
}
//~END Shadow

//~BEGIN GBuffer
void URenderPipelines::CreateGBufferPass()
{
	std::array<VkAttachmentDescription, 4> Attachments
	{
		VkAttachmentDescription
		{
			.format = GBufferData.GBuffer0Format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		VkAttachmentDescription
		{
			.format = GBufferData.GBuffer1Format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		VkAttachmentDescription{
            .format = GBufferData.GBuffer2Format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkAttachmentDescription{
            .format = DepthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
	};

	std::array<VkAttachmentReference, 3> ColorRefs
	{
		VkAttachmentReference
		{ 
			.attachment = 0, 
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
		},
        VkAttachmentReference
		{ 
			.attachment = 1, 
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
		},
        VkAttachmentReference
		{ 
			.attachment = 2, 
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
		},
	};

	VkAttachmentReference DepthRef
    {
        .attachment = 3,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

	VkSubpassDescription Subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = uint32_t(ColorRefs.size()),
        .pColorAttachments = ColorRefs.data(),
        .pDepthStencilAttachment = &DepthRef,
    };

	std::array<VkSubpassDependency, 2> Dependencies
    {
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        }
    };

    VkRenderPassCreateInfo CreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = uint32_t(Attachments.size()),
        .pAttachments = Attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &Subpass,
        .dependencyCount = uint32_t(Dependencies.size()),
        .pDependencies = Dependencies.data(),
    };

	VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &GBufferData.GBufferPass));
}

void URenderPipelines::CreateGBufferTargets(VkExtent2D Extent, size_t FramebufferCount)
{
	for (auto framebuffer : GBufferData.GBufferFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		}
    }
    GBufferData.GBufferFramebuffers.clear();

    for (auto view : GBufferData.GBufferViews)
    {
        if (view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(rtg.device, view, nullptr);
		}
    }
    GBufferData.GBufferViews.clear();

    for (auto &img : GBufferData.GBufferImages)
    {
        if (img.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_image(std::move(img));
		}
    }
    GBufferData.GBufferImages.clear();

    if (GBufferData.DepthView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, GBufferData.DepthView, nullptr);
        GBufferData.DepthView = VK_NULL_HANDLE;
    }
    if (GBufferData.DepthImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(GBufferData.DepthImage));
    }

    std::array<VkFormat, 3> Formats
    {
        GBufferData.GBuffer0Format,
        GBufferData.GBuffer1Format,
        GBufferData.GBuffer2Format
    };

    for (uint32_t i = 0; i < 3; ++i)
    {
        GBufferData.GBufferImages.emplace_back
		(
            rtg.helpers.create_image
			(
                Extent,
                Formats[i],
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                Helpers::Unmapped
            )
        );

        VkImageViewCreateInfo ViewInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = GBufferData.GBufferImages.back().handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = Formats[i],
            .subresourceRange
			{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkImageView view = VK_NULL_HANDLE;
        VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &view));
        GBufferData.GBufferViews.push_back(view);
    }

    GBufferData.DepthImage = rtg.helpers.create_image
	(
        Extent,
        DepthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    VkImageViewCreateInfo DepthViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = GBufferData.DepthImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = DepthFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK(vkCreateImageView(rtg.device, &DepthViewInfo, nullptr, &GBufferData.DepthView));

    GBufferData.GBufferFramebuffers.resize(FramebufferCount, VK_NULL_HANDLE);

    for (size_t i = 0; i < FramebufferCount; ++i)
    {
        std::array<VkImageView, 4> Attachments
        {
            GBufferData.GBufferViews[0],
            GBufferData.GBufferViews[1],
            GBufferData.GBufferViews[2],
            GBufferData.DepthView,
        };

        VkFramebufferCreateInfo FBInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = GBufferData.GBufferPass,
            .attachmentCount = uint32_t(Attachments.size()),
            .pAttachments = Attachments.data(),
            .width = Extent.width,
            .height = Extent.height,
            .layers = 1,
        };

        VK(vkCreateFramebuffer(rtg.device, &FBInfo, nullptr, &GBufferData.GBufferFramebuffers[i]));
    }
}

void URenderPipelines::RenderDeferredGeometryPass(FWorkspace &Workspace, uint32_t FramebufferIndex)
{
	if (Scene.MeshProxyInstances.empty())
	{
		return;
	}

	std::array<VkClearValue, 4> ClearValues
    {
        VkClearValue{ .color{ .float32{0.5f, 0.5f, 1.0f, 1.0f} } }, // normal default
        VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 0.0f} } }, // albedo/met
        VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 0.0f} } }, // pos/type
        VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } }
    };

	VkRenderPassBeginInfo BeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = GBufferData.GBufferPass,
        .framebuffer = GBufferData.GBufferFramebuffers[FramebufferIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = rtg.swapchain_extent,
        },
        .clearValueCount = uint32_t(ClearValues.size()),
        .pClearValues = ClearValues.data(),
    };

	vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    ViewportFullScreen(Workspace);

    vkCmdBindPipeline(Workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredGeometryPipeline.Handle);

    {
        std::array<VkBuffer, 1> VertexBuffers{ ObjectVertices.handle };
        std::array<VkDeviceSize, 1> Offsets{ 0 };
        vkCmdBindVertexBuffers(Workspace.command_buffer, 0, 1, VertexBuffers.data(), Offsets.data());
    }

    {
        std::array<VkDescriptorSet, 3> DescriptorSets
        {
            Workspace.CameraDescriptors,
            Workspace.WorldDescriptors,
            Workspace.TransformDescriptors,
        };

        vkCmdBindDescriptorSets
		(
            Workspace.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            DeferredGeometryPipeline.Layout,
            0,
            uint32_t(DescriptorSets.size()),
            DescriptorSets.data(),
            0, nullptr
        );
    }

    const std::vector<FMeshRenderProxy*>& ProxyList = Scene.MeshProxyInstances;
    for (uint32_t i = 0; i < ProxyList.size(); ++i)
    {
        const FMeshRenderProxy* Proxy = ProxyList[i];
        if (!Proxy->bCanSee)
		{
			continue;
		}
		
        const FMaterial* Material = Scene.Materials[Proxy->MaterialIdx];
        FDeferredGeometryPipeline::FPush Push
        {
            .MaterialType = int(Material->Type),
            .Padding0 = 0.0f,
            .Padding1 = 0.0f,
            .Padding2 = 0.0f,
        };

        vkCmdPushConstants
		(
            Workspace.command_buffer,
            DeferredGeometryPipeline.Layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Push),
            &Push
        );

        vkCmdBindDescriptorSets
		(
            Workspace.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            DeferredGeometryPipeline.Layout,
            3,
            1,
            &MaterialDescriptors[Proxy->MaterialIdx],
            0,
            nullptr
        );

        vkCmdDraw
		(
            Workspace.command_buffer,
            Proxy->VertexNum,
            1,
            Proxy->FirstVertexIdx,
            i
        );
    }

    vkCmdEndRenderPass(Workspace.command_buffer);
}

void URenderPipelines::CreateDeferredLightingDescriptors()
{
	if (DeferredLightingDescriptors == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = DeferredLightingDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &DeferredLightingPipeline.Set0_GBuffer,
        };
        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &DeferredLightingDescriptors));
    }

    {
		std::array<VkDescriptorImageInfo, 3> Infos
		{
			VkDescriptorImageInfo
			{
				.sampler = TextureSamplerNearest,
				.imageView = GBufferData.GBufferViews[0],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkDescriptorImageInfo
			{
				.sampler = TextureSamplerNearest,
				.imageView = GBufferData.GBufferViews[1],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkDescriptorImageInfo
			{
				.sampler = TextureSamplerNearest,
				.imageView = GBufferData.GBufferViews[2],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		};

		std::array<VkWriteDescriptorSet, 3> Writes
		{
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = DeferredLightingDescriptors,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[0],
			},
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = DeferredLightingDescriptors,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[1],
			},
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = DeferredLightingDescriptors,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[2],
			},
		};
		
    	vkUpdateDescriptorSets(rtg.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
	}


	if(DeferredLightingScreenProcessDescriptors == VK_NULL_HANDLE)
	{
		VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = DeferredLightingDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &DeferredLightingPipeline.Set5_ScreenProcess,
        };
        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &DeferredLightingScreenProcessDescriptors));
	}
	{
		std::array<VkDescriptorImageInfo, 2> Infos
		{
			VkDescriptorImageInfo
			{
				.sampler = TextureSamplerNearest,
				.imageView = SSAOBlurData.AOBlurView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkDescriptorImageInfo
			{
				.sampler = TextureSamplerNearest,
				.imageView = SSDOFilterData.SSDOFilterView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		};

		std::array<VkWriteDescriptorSet, 2> Writes
		{
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = DeferredLightingScreenProcessDescriptors,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[0],
			},
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = DeferredLightingScreenProcessDescriptors,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[1],
			},
		};
		
    	vkUpdateDescriptorSets(rtg.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
	}
}

void URenderPipelines::RenderDeferredLightingPipeline(FWorkspace &Workspace)
{
	vkCmdBindPipeline(Workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredLightingPipeline.Handle);

	std::array<VkDescriptorSet, 6> DescriptorSets
    {
        DeferredLightingDescriptors,   // set 0: GBuffer
        Workspace.WorldDescriptors,    // set 1: World
        Workspace.LightsDescriptors,   // set 2: Lights
        EnvTexDescriptors[0],          // set 3: EnvTex
        ShadowDescriptors,              // set 4: Shadowmap
		DeferredLightingScreenProcessDescriptors,	// set 5 : Screen Processing
    };

    vkCmdBindDescriptorSets(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        DeferredLightingPipeline.Layout,
        0,
        uint32_t(DescriptorSets.size()),
        DescriptorSets.data(),
        0,
        nullptr
    );

    FDeferredLightingPipeline::FConstant Constant
    {
        .LightsCount = int(Scene.LightProxyInstances.size()),
        .Padding0 = 0.0f,
        .Padding1 = 0.0f,
        .Padding2 = 0.0f,
    };

    vkCmdPushConstants(
        Workspace.command_buffer,
        DeferredLightingPipeline.Layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(Constant),
        &Constant
    );

    vkCmdDraw(Workspace.command_buffer, 3, 1, 0, 0);
}

void URenderPipelines::DestroyGBufferResources()
{
    for (auto &fb : GBufferData.GBufferFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(rtg.device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    GBufferData.GBufferFramebuffers.clear();

    for (auto &view : GBufferData.GBufferViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(rtg.device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
    }
    GBufferData.GBufferViews.clear();

    for (auto &img : GBufferData.GBufferImages)
    {
        if (img.handle != VK_NULL_HANDLE)
        {
            rtg.helpers.destroy_image(std::move(img));
        }
    }
    GBufferData.GBufferImages.clear();

    if (GBufferData.DepthView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, GBufferData.DepthView, nullptr);
        GBufferData.DepthView = VK_NULL_HANDLE;
    }

    if (GBufferData.DepthImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(GBufferData.DepthImage));
    }

    if (GBufferData.GBufferPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(rtg.device, GBufferData.GBufferPass, nullptr);
        GBufferData.GBufferPass = VK_NULL_HANDLE;
    }
}
//~END GBuffer

//~BEGIN GBuffer Debug
void URenderPipelines::CreateGBufferDebugDescriptors()
{
    if (GBufferDebugDescriptors == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = GBufferDebugDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &GBufferDebugPipeline.Set0_GBuffer,
        };
        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &GBufferDebugDescriptors));
    }

    std::array<VkDescriptorImageInfo, 5> Infos
    {
        VkDescriptorImageInfo	// GBuffer0 xyz - normal w - roughness
		{
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[0],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo	// GBuffer1 xyz - albedo w - metalness
		{
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[1],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo	// Gbuffer2 xyz - position w - materialType
		{
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[2],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// SSAO
		{
            .sampler = TextureSamplerNearest,
            .imageView = SSAOBlurData.AOBlurView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// SSDO
		{
            .sampler = TextureSamplerNearest,
            .imageView = SSDOFilterData.SSDOFilterView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    std::array<VkWriteDescriptorSet, 5> Writes
    {
        VkWriteDescriptorSet
		{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = GBufferDebugDescriptors,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &Infos[0],
        },
        VkWriteDescriptorSet
		{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = GBufferDebugDescriptors,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &Infos[1],
        },
        VkWriteDescriptorSet
		{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = GBufferDebugDescriptors,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &Infos[2],
        },
		VkWriteDescriptorSet
		{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = GBufferDebugDescriptors,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &Infos[3],
        },
		VkWriteDescriptorSet
		{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = GBufferDebugDescriptors,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &Infos[4],
        },
    };

    vkUpdateDescriptorSets(rtg.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
}

void URenderPipelines::RenderGBufferDebugPipeline(FWorkspace &workspace)
{
    vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GBufferDebugPipeline.Handle);

    vkCmdBindDescriptorSets(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GBufferDebugPipeline.Layout, 0, 1, &GBufferDebugDescriptors, 0, nullptr);

    FGBufferDebugPipeline::FPush Push{};
    switch (GBufferDebugView)
    {
        case EGBufferDebugView::Albedo:   Push.Mode = 1; break;
        case EGBufferDebugView::Normal:   Push.Mode = 2; break;
        case EGBufferDebugView::Position: Push.Mode = 3; break;
		case EGBufferDebugView::Roughness: Push.Mode = 4; break;
		case EGBufferDebugView::Metalness: Push.Mode = 5; break;
		case EGBufferDebugView::SSAO:		Push.Mode = 6; break;
		case EGBufferDebugView::SSDO:		Push.Mode = 7; break;
        default:                          Push.Mode = 0; break;
    }

    vkCmdPushConstants(workspace.command_buffer, GBufferDebugPipeline.Layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push), &Push);

    vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
}

void URenderPipelines::ViewportFullScreen(FWorkspace &workspace)
{
    VkViewport Viewport
    {
        .x = 0.0f,
        .y = 0.0f,
        .width = float(rtg.swapchain_extent.width),
        .height = float(rtg.swapchain_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(workspace.command_buffer, 0, 1, &Viewport);

    VkRect2D Scissor
    {
        .offset = { 0, 0 },
        .extent = rtg.swapchain_extent,
    };
    vkCmdSetScissor(workspace.command_buffer, 0, 1, &Scissor);
}
//~END GBuffer Debug

//~BEGIN Screen processing
// SSAO
void URenderPipelines::CreateSSAOPass()
{
    VkAttachmentDescription Attachment
    {
        .format = SSAOData.AOFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference ColorRef
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription Subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorRef,
    };

    VkSubpassDependency Dependency
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo CreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &Attachment,
        .subpassCount = 1,
        .pSubpasses = &Subpass,
        .dependencyCount = 1,
        .pDependencies = &Dependency,
    };

    VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &SSAOData.SSAOPass));
}

void URenderPipelines::CreateSSAOTargets(VkExtent2D Extent, size_t FramebufferCount)
{
    if (SSAOData.AOView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSAOData.AOView, nullptr);
        SSAOData.AOView = VK_NULL_HANDLE;
    }

    if (SSAOData.AOImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSAOData.AOImage));
    }

    for (auto fb : SSAOData.AOFramebuffers)
    {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(rtg.device, fb, nullptr);
    }
    SSAOData.AOFramebuffers.clear();

    SSAOData.AOImage = rtg.helpers.create_image(
        Extent,
        SSAOData.AOFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    VkImageViewCreateInfo ViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = SSAOData.AOImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SSAOData.AOFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &SSAOData.AOView));

    SSAOData.AOFramebuffers.resize(FramebufferCount, VK_NULL_HANDLE);
    for (size_t i = 0; i < FramebufferCount; ++i)
    {
        VkFramebufferCreateInfo FBInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = SSAOData.SSAOPass,
            .attachmentCount = 1,
            .pAttachments = &SSAOData.AOView,
            .width = Extent.width,
            .height = Extent.height,
            .layers = 1,
        };
        VK(vkCreateFramebuffer(rtg.device, &FBInfo, nullptr, &SSAOData.AOFramebuffers[i]));
    }
}

void URenderPipelines::CreateSSAODescriptors(FWorkspace &workspace)
{
    if (SSAOData.DescriptorPool == VK_NULL_HANDLE)
    {
        std::array<VkDescriptorPoolSize, 2> PoolSizes
        {
            VkDescriptorPoolSize
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 3 * uint32_t(workspaces.size()),
            },
            VkDescriptorPoolSize
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1 * uint32_t(workspaces.size()),
            }
        };

        VkDescriptorPoolCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = uint32_t(workspaces.size()),
            .poolSizeCount = uint32_t(PoolSizes.size()),
            .pPoolSizes = PoolSizes.data(),
        };

        VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &SSAOData.DescriptorPool));
    }

    if (workspace.SSAOUboDescriptors == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = SSAOData.DescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &SSAOPipeline.Set0_GBuffer,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.SSAOUboDescriptors));
    }

    std::array<VkDescriptorImageInfo, 3> ImageInfos
    {
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[0],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[1],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[2],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    VkDescriptorBufferInfo UBOInfo
    {
        .buffer = workspace.SSAO.handle,
        .offset = 0,
        .range = workspace.SSAO.size,
    };

    std::array<VkWriteDescriptorSet, 4> Writes
    {
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSAOUboDescriptors,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[0],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSAOUboDescriptors,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[1],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSAOUboDescriptors,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[2],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSAOUboDescriptors,
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &UBOInfo,
        },
    };

    vkUpdateDescriptorSets(
        rtg.device,
        uint32_t(Writes.size()),
        Writes.data(),
        0,
        nullptr
    );
}

void URenderPipelines::RenderSSAOPass(FWorkspace& Workspace, uint32_t FramebufferIndex)
{
    VkClearValue ClearValue{};
    ClearValue.color.float32[0] = 1.0f;
    ClearValue.color.float32[1] = 1.0f;
    ClearValue.color.float32[2] = 1.0f;
    ClearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo BeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = SSAOData.SSAOPass,
        .framebuffer = SSAOData.AOFramebuffers[FramebufferIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = rtg.swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues = &ClearValue,
    };

    vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    ViewportFullScreen(Workspace);

    vkCmdBindPipeline(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSAOPipeline.Handle
    );

    vkCmdBindDescriptorSets(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSAOPipeline.Layout,
        0,
        1,
        &Workspace.SSAOUboDescriptors,
        0,
        nullptr
    );

    vkCmdDraw(Workspace.command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(Workspace.command_buffer);

	VkImageMemoryBarrier aoBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = SSAOData.AOImage.handle,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(
		Workspace.command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &aoBarrier
	);
}

void URenderPipelines::UpdateSSAOBuffer(FWorkspace &workspace)
{
	FSSAOPassUBO ssao{};

	std::memcpy(ssao.ViewFromWorld, glm::value_ptr(VIEW_FROM_WORLD), sizeof(float) * 16);
	std::memcpy(ssao.Projection, glm::value_ptr(PROJECTION), sizeof(float) * 16);
	std::memcpy(ssao.InvProjection, glm::value_ptr(INV_PROJECTION), sizeof(float) * 16);

	ssao.Radius = 1.5f;
	ssao.Bias   = 0.08f;
	ssao.Power  = 1.2f;

	assert(workspace.SSAOSrc.size == sizeof(FSSAOPassUBO));
	assert(workspace.SSAOSrc.allocation.mapped);

	// CPU -> staging buffer
	std::memcpy(workspace.SSAOSrc.allocation.data(), &ssao, sizeof(FSSAOPassUBO));

	// staging -> device local
	VkBufferCopy CopyRegion
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = sizeof(FSSAOPassUBO),
	};

	vkCmdCopyBuffer(
		workspace.command_buffer,
		workspace.SSAOSrc.handle,
		workspace.SSAO.handle,
		1,
		&CopyRegion
	);
}

void URenderPipelines::DestroySSAOResources()
{
    for (auto &fb : SSAOData.AOFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(rtg.device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    SSAOData.AOFramebuffers.clear();

    if (SSAOData.AOView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSAOData.AOView, nullptr);
        SSAOData.AOView = VK_NULL_HANDLE;
    }

    if (SSAOData.AOImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSAOData.AOImage));
    }

    if (SSAOData.DescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(rtg.device, SSAOData.DescriptorPool, nullptr);
        SSAOData.DescriptorPool = VK_NULL_HANDLE;
    }

    SSAOData.DescriptorSet = VK_NULL_HANDLE;

    if (SSAOData.SSAOPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(rtg.device, SSAOData.SSAOPass, nullptr);
        SSAOData.SSAOPass = VK_NULL_HANDLE;
    }
}

void URenderPipelines::CreateSSAOBlurPass()
{
    VkAttachmentDescription Attachment
    {
        .format = SSAOBlurData.AOBlurFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference ColorRef
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription Subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorRef,
    };

    VkSubpassDependency Dependency
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo CreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &Attachment,
        .subpassCount = 1,
        .pSubpasses = &Subpass,
        .dependencyCount = 1,
        .pDependencies = &Dependency,
    };

    VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &SSAOBlurData.BlurPass));
}

void URenderPipelines::CreateSSAOBlurTargets(VkExtent2D Extent, size_t FramebufferCount)
{
    if (SSAOBlurData.AOBlurView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSAOBlurData.AOBlurView, nullptr);
        SSAOBlurData.AOBlurView = VK_NULL_HANDLE;
    }

    if (SSAOBlurData.AOBlurImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSAOBlurData.AOBlurImage));
    }

    for (auto fb : SSAOBlurData.AOBlurFramebuffers)
    {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(rtg.device, fb, nullptr);
    }
    SSAOBlurData.AOBlurFramebuffers.clear();

    SSAOBlurData.AOBlurImage = rtg.helpers.create_image(
        Extent,
        SSAOBlurData.AOBlurFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    VkImageViewCreateInfo ViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = SSAOBlurData.AOBlurImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SSAOBlurData.AOBlurFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &SSAOBlurData.AOBlurView));

    SSAOBlurData.AOBlurFramebuffers.resize(FramebufferCount, VK_NULL_HANDLE);
    for (size_t i = 0; i < FramebufferCount; ++i)
    {
        VkFramebufferCreateInfo FBInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = SSAOBlurData.BlurPass,
            .attachmentCount = 1,
            .pAttachments = &SSAOBlurData.AOBlurView,
            .width = Extent.width,
            .height = Extent.height,
            .layers = 1,
        };
        VK(vkCreateFramebuffer(rtg.device, &FBInfo, nullptr, &SSAOBlurData.AOBlurFramebuffers[i]));
    }
}

void URenderPipelines::CreateSSAOBlurDescriptors()
{
    if (SSAOBlurData.DescriptorPool == VK_NULL_HANDLE)
    {
        VkDescriptorPoolSize PoolSize
        {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 3,
        };

        VkDescriptorPoolCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &PoolSize,
        };

        VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &SSAOBlurData.DescriptorPool));
    }

    if (SSAOBlurData.DescriptorSet == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = SSAOBlurData.DescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &SSAOBlurPipeline.Set0_InputInfo,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &SSAOBlurData.DescriptorSet));
    }

    std::array<VkDescriptorImageInfo, 3> ImageInfos
    {
        VkDescriptorImageInfo	// SSAO
        {
            .sampler = TextureSamplerNearest,
            .imageView = SSAOData.AOView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// normalWS
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[0],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// positionWS
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[2],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    std::array<VkWriteDescriptorSet, 3> Writes
    {
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSAOBlurData.DescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[0],
        },
		VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSAOBlurData.DescriptorSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[1],
        },
		VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSAOBlurData.DescriptorSet,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[2],
        },
    };

    vkUpdateDescriptorSets(
        rtg.device,
        uint32_t(Writes.size()),
        Writes.data(),
        0,
        nullptr
    );
}

void URenderPipelines::RenderSSAOBlurPass(FWorkspace& Workspace, uint32_t FramebufferIndex)
{
    VkClearValue ClearValue{};
    ClearValue.color.float32[0] = 1.0f;
    ClearValue.color.float32[1] = 1.0f;
    ClearValue.color.float32[2] = 1.0f;
    ClearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo BeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = SSAOBlurData.BlurPass,
        .framebuffer = SSAOBlurData.AOBlurFramebuffers[FramebufferIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = rtg.swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues = &ClearValue,
    };

    vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    ViewportFullScreen(Workspace);

    vkCmdBindPipeline(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSAOBlurPipeline.Handle
    );

    vkCmdBindDescriptorSets(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSAOBlurPipeline.Layout,
        0,
        1,
        &SSAOBlurData.DescriptorSet,
        0,
        nullptr
    );

    vkCmdDraw(Workspace.command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(Workspace.command_buffer);

	VkImageMemoryBarrier blurAOBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = SSAOBlurData.AOBlurImage.handle,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(
		Workspace.command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &blurAOBarrier
	);
}

// SSDO
void URenderPipelines::CreateSSDOPass()
{
    VkAttachmentDescription Attachment
    {
        .format = SSDOData.DOFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference ColorRef
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription Subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorRef,
    };

    VkSubpassDependency Dependency
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo CreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &Attachment,
        .subpassCount = 1,
        .pSubpasses = &Subpass,
        .dependencyCount = 1,
        .pDependencies = &Dependency,
    };

    VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &SSDOData.SSDOPass));
}

void URenderPipelines::CreateSSDOTargets(VkExtent2D Extent, size_t FramebufferCount)
{
    if (SSDOData.DOView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSDOData.DOView, nullptr);
        SSDOData.DOView = VK_NULL_HANDLE;
    }

    if (SSDOData.DOImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSDOData.DOImage));
    }

    for (auto fb : SSDOData.DOFramebuffers)
    {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(rtg.device, fb, nullptr);
    }
    SSDOData.DOFramebuffers.clear();

    SSDOData.DOImage = rtg.helpers.create_image(
        Extent,
        SSDOData.DOFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    VkImageViewCreateInfo ViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = SSDOData.DOImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SSDOData.DOFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &SSDOData.DOView));

    SSDOData.DOFramebuffers.resize(FramebufferCount, VK_NULL_HANDLE);
    for (size_t i = 0; i < FramebufferCount; ++i)
    {
        VkFramebufferCreateInfo FBInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = SSDOData.SSDOPass,
            .attachmentCount = 1,
            .pAttachments = &SSDOData.DOView,
            .width = Extent.width,
            .height = Extent.height,
            .layers = 1,
        };
        VK(vkCreateFramebuffer(rtg.device, &FBInfo, nullptr, &SSDOData.DOFramebuffers[i]));
    }
}

void URenderPipelines::CreateSSDODescriptors(FWorkspace &workspace)
{
    if (SSDOData.DescriptorPool == VK_NULL_HANDLE)
    {
        std::array<VkDescriptorPoolSize, 2> PoolSizes
        {
            VkDescriptorPoolSize
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 3 * uint32_t(workspaces.size()),
            },
            VkDescriptorPoolSize
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1 * uint32_t(workspaces.size()),
            }
        };

        VkDescriptorPoolCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = uint32_t(workspaces.size()),
            .poolSizeCount = uint32_t(PoolSizes.size()),
            .pPoolSizes = PoolSizes.data(),
        };

        VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &SSDOData.DescriptorPool));
    }

    if (workspace.SSDOUboDescriptors == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = SSDOData.DescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &SSDOPipeline.Set0_GBuffer,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.SSDOUboDescriptors));
    }

    std::array<VkDescriptorImageInfo, 3> ImageInfos
    {
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[0],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[1],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[2],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    VkDescriptorBufferInfo UBOInfo
    {
        .buffer = workspace.SSAO.handle,
        .offset = 0,
        .range = workspace.SSAO.size,
    };

    std::array<VkWriteDescriptorSet, 4> Writes
    {
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSDOUboDescriptors,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[0],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSDOUboDescriptors,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[1],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSDOUboDescriptors,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[2],
        },
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = workspace.SSDOUboDescriptors,
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &UBOInfo,
        },
    };

    vkUpdateDescriptorSets(
        rtg.device,
        uint32_t(Writes.size()),
        Writes.data(),
        0,
        nullptr
    );
}

void URenderPipelines::RenderSSDOPass(FWorkspace& Workspace, uint32_t FramebufferIndex)
{
    VkClearValue ClearValue{};
    ClearValue.color.float32[0] = 1.0f;
    ClearValue.color.float32[1] = 1.0f;
    ClearValue.color.float32[2] = 1.0f;
    ClearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo BeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = SSDOData.SSDOPass,
        .framebuffer = SSDOData.DOFramebuffers[FramebufferIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = rtg.swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues = &ClearValue,
    };

    vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    ViewportFullScreen(Workspace);

    vkCmdBindPipeline(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSDOPipeline.Handle
    );

    vkCmdBindDescriptorSets(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSDOPipeline.Layout,
        0,
        1,
        &Workspace.SSAOUboDescriptors,
        0,
        nullptr
    );

    vkCmdDraw(Workspace.command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(Workspace.command_buffer);

	VkImageMemoryBarrier doBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = SSDOData.DOImage.handle,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(
		Workspace.command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &doBarrier
	);
}

void URenderPipelines::UpdateSSDOBuffer(FWorkspace &workspace)
{
	FSSDOPassUBO ssdo{};

	std::memcpy(ssdo.ViewFromWorld, glm::value_ptr(VIEW_FROM_WORLD), sizeof(float) * 16);
	std::memcpy(ssdo.Projection, glm::value_ptr(PROJECTION), sizeof(float) * 16);
	std::memcpy(ssdo.InvProjection, glm::value_ptr(INV_PROJECTION), sizeof(float) * 16);

	ssdo.Radius = 1.5f;
	ssdo.Bias   = 0.08f;
	ssdo.Power  = 1.2f;

	assert(workspace.SSDOSrc.size == sizeof(FSSDOPassUBO));
	assert(workspace.SSDOSrc.allocation.mapped);

	// CPU -> staging buffer
	std::memcpy(workspace.SSDOSrc.allocation.data(), &ssdo, sizeof(FSSDOPassUBO));

	// staging -> device local
	VkBufferCopy CopyRegion
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = sizeof(FSSAOPassUBO),
	};

	vkCmdCopyBuffer(
		workspace.command_buffer,
		workspace.SSDOSrc.handle,
		workspace.SSDO.handle,
		1,
		&CopyRegion
	);
}

void URenderPipelines::DestroySSDOResources()
{
    for (auto &fb : SSDOData.DOFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(rtg.device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    SSDOData.DOFramebuffers.clear();

    if (SSDOData.DOView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSDOData.DOView, nullptr);
        SSDOData.DOView = VK_NULL_HANDLE;
    }

    if (SSDOData.DOImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSDOData.DOImage));
    }

    if (SSDOData.DescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(rtg.device, SSDOData.DescriptorPool, nullptr);
        SSDOData.DescriptorPool = VK_NULL_HANDLE;
    }

    SSDOData.DescriptorSet = VK_NULL_HANDLE;

    if (SSDOData.SSDOPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(rtg.device, SSDOData.SSDOPass, nullptr);
        SSDOData.SSDOPass = VK_NULL_HANDLE;
    }
}

void URenderPipelines::CreateSSDOFilterPass()
{
    VkAttachmentDescription Attachment
    {
        .format = SSDOFilterData.SSDOFilterFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference ColorRef
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription Subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorRef,
    };

    VkSubpassDependency Dependency
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo CreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &Attachment,
        .subpassCount = 1,
        .pSubpasses = &Subpass,
        .dependencyCount = 1,
        .pDependencies = &Dependency,
    };

    VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &SSDOFilterData.FilterPass));
}

void URenderPipelines::CreateSSDOFilterTargets(VkExtent2D Extent, size_t FramebufferCount)
{
    if (SSDOFilterData.SSDOFilterView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(rtg.device, SSDOFilterData.SSDOFilterView, nullptr);
        SSDOFilterData.SSDOFilterView = VK_NULL_HANDLE;
    }

    if (SSDOFilterData.SSDOFilterImage.handle != VK_NULL_HANDLE)
    {
        rtg.helpers.destroy_image(std::move(SSDOFilterData.SSDOFilterImage));
    }

    for (auto fb : SSDOFilterData.SSDOFilterFramebuffers)
    {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(rtg.device, fb, nullptr);
    }
    SSDOFilterData.SSDOFilterFramebuffers.clear();

    SSDOFilterData.SSDOFilterImage = rtg.helpers.create_image(
        Extent,
        SSDOFilterData.SSDOFilterFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    VkImageViewCreateInfo ViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = SSDOFilterData.SSDOFilterImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SSDOFilterData.SSDOFilterFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK(vkCreateImageView(rtg.device, &ViewInfo, nullptr, &SSDOFilterData.SSDOFilterView));

    SSDOFilterData.SSDOFilterFramebuffers.resize(FramebufferCount, VK_NULL_HANDLE);
    for (size_t i = 0; i < FramebufferCount; ++i)
    {
        VkFramebufferCreateInfo FBInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = SSDOFilterData.FilterPass,
            .attachmentCount = 1,
            .pAttachments = &SSDOFilterData.SSDOFilterView,
            .width = Extent.width,
            .height = Extent.height,
            .layers = 1,
        };
        VK(vkCreateFramebuffer(rtg.device, &FBInfo, nullptr, &SSDOFilterData.SSDOFilterFramebuffers[i]));
    }
}

void URenderPipelines::CreateSSDOFilterDescriptors()
{
    if (SSDOFilterData.DescriptorPool == VK_NULL_HANDLE)
    {
        VkDescriptorPoolSize PoolSize
        {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 3,
        };

        VkDescriptorPoolCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &PoolSize,
        };

        VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &SSDOFilterData.DescriptorPool));
    }

    if (SSDOFilterData.DescriptorSet == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo AllocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = SSDOFilterData.DescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &SSDOFilterPipeline.Set0_InputInfo,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &AllocInfo, &SSDOFilterData.DescriptorSet));
    }

    std::array<VkDescriptorImageInfo, 3> ImageInfos
    {
        VkDescriptorImageInfo	// SSDO
        {
            .sampler = TextureSamplerNearest,
            .imageView = SSDOData.DOView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// normalWS
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[0],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
		VkDescriptorImageInfo	// positionWS
        {
            .sampler = TextureSamplerNearest,
            .imageView = GBufferData.GBufferViews[2],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    std::array<VkWriteDescriptorSet, 3> Writes
    {
        VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSDOFilterData.DescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[0],
        },
		VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSDOFilterData.DescriptorSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[1],
        },
		VkWriteDescriptorSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = SSDOFilterData.DescriptorSet,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ImageInfos[2],
        },
    };

    vkUpdateDescriptorSets(
        rtg.device,
        uint32_t(Writes.size()),
        Writes.data(),
        0,
        nullptr
    );
}

void URenderPipelines::RenderSSDOFilterPass(FWorkspace& Workspace, uint32_t FramebufferIndex)
{
    VkClearValue ClearValue{};
    ClearValue.color.float32[0] = 1.0f;
    ClearValue.color.float32[1] = 1.0f;
    ClearValue.color.float32[2] = 1.0f;
    ClearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo BeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = SSDOFilterData.FilterPass,
        .framebuffer = SSDOFilterData.SSDOFilterFramebuffers[FramebufferIndex],
        .renderArea{
            .offset = {0, 0},
            .extent = rtg.swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues = &ClearValue,
    };

    vkCmdBeginRenderPass(Workspace.command_buffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    ViewportFullScreen(Workspace);

    vkCmdBindPipeline(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSDOFilterPipeline.Handle
    );

    vkCmdBindDescriptorSets(
        Workspace.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        SSDOFilterPipeline.Layout,
        0,
        1,
        &SSDOFilterData.DescriptorSet,
        0,
        nullptr
    );

    vkCmdDraw(Workspace.command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(Workspace.command_buffer);

	VkImageMemoryBarrier SSDOFilterBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = SSAOBlurData.AOBlurImage.handle,
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(
		Workspace.command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &SSDOFilterBarrier
	);
}

//~END Postprocess