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
#include "Render/Shadow.hpp"
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
		VK(vkCreateRenderPass(rtg.device, &CreateInfo, nullptr, &ShadowPass));
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

	LinesPipeline.Create(rtg, RenderPass, 0);
    LambertPipeline.Create(rtg, RenderPass, 0);
	ShadowPipeline.Create(rtg, ShadowPass, 0, LambertPipeline.Set2_Transforms);

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
				.descriptorCount = 1 * EnvCount,	 // one descriptor per set, one set 5 texture
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
		VK(vkCreateSampler(rtg.device, &CreateInfo, nullptr, &ShadowSamplerPCF));
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
			.descriptorCount = MAX_SPOT_SHADOWS,
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
		if(SpotLightShadows.empty())
		{
			return;
		}
		
		std::vector<VkDescriptorImageInfo> Infos(MAX_SPOT_SHADOWS);
		for (uint32_t i = 0; i < MAX_SPOT_SHADOWS; ++i)
		{
			if (i < SpotLightShadows.size())
			{
				Infos[i] = VkDescriptorImageInfo
				{
					.sampler = ShadowSamplerPCF,
					.imageView = SpotLightShadows[i].ImageView,
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				};
			}
			else
			{
				Infos[i] = VkDescriptorImageInfo
				{
					.sampler = ShadowSamplerPCF,
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
			.descriptorCount = MAX_SPOT_SHADOWS,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = Infos.data(),
		};

		vkUpdateDescriptorSets(rtg.device, 1, &Write, 0, nullptr);
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

	if(TextureSamplerNearest)
	{
		vkDestroySampler(rtg.device, TextureSamplerNearest, nullptr);
		TextureSamplerNearest = VK_NULL_HANDLE;
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
	if (ShadowSamplerPCF != VK_NULL_HANDLE) 
	{
    	vkDestroySampler(rtg.device, ShadowSamplerPCF, nullptr);
    	ShadowSamplerPCF = VK_NULL_HANDLE;
	}

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

		ViewportPillarBoxing(workspace);
		
		RenderLinesPipeline(workspace);
        RenderLambertPipeline(workspace);
		
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
		CLIP_FROM_WORLD = Perspective
		(
			FreeCamera.FOV,
			DefaultAspect,	// Aspect
			FreeCamera.Near,
			FreeCamera.Far
		) * Orbit;
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
    // Draw with the objects pipeline:
	if (!Scene.MeshProxyInstances.empty()) 
	{ 
		vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LambertPipeline.Handle);
	}

	{
		// use object_vertices (offset 0) as vertex buffer binding 0:
		std::array< VkBuffer, 1 > VertexBuffers{ ObjectVertices.handle };
		std::array< VkDeviceSize, 1 > Offsets{ 0 };
		vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(VertexBuffers.size()), VertexBuffers.data(), Offsets.data());
	}

	// Bind World and Transforms descriptor sets:
	{
		std::array< VkDescriptorSet, 3 > DescriptorSets
		{
            workspace.CameraDescriptors,    // 0：Camera
			workspace.WorldDescriptors, 	// 1: World
			workspace.TransformDescriptors, // 2: Transforms
		};
		vkCmdBindDescriptorSets
		(
			workspace.command_buffer, 			// Command Buffer
			VK_PIPELINE_BIND_POINT_GRAPHICS, 	// Pipeline bind point
			LambertPipeline.Layout, 			// Pipeline Layout
			0, 									// First Set
			uint32_t(DescriptorSets.size()), DescriptorSets.data(), // descriptor sets count, ptr
			0, nullptr // DynamicOffsets Count, ptr
		);
	}
	// Set4_Light
	{
    	vkCmdBindDescriptorSets(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            LambertPipeline.Layout, 4, 1, &workspace.LightsDescriptors, 0, nullptr);
	}
	// Set5_EnvTex
	{
    	vkCmdBindDescriptorSets(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            LambertPipeline.Layout, 5, 1, &EnvTexDescriptors[0], 0, nullptr);
	}
	// Set6_Shadowmap
	{
		vkCmdBindDescriptorSets(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        						LambertPipeline.Layout, 6, 1, &ShadowDescriptors, 0, nullptr);
	}

	// Set3_Material Set
	const std::vector<FMeshRenderProxy*>& ProxyList = Scene.MeshProxyInstances;
	for (uint32_t i = 0; i < ProxyList.size(); i++)
	{
		const FMeshRenderProxy* Proxy = ProxyList[i];
		if(!Proxy->bCanSee)
		{
			continue;
		}
		// Push constant here
		{
			const FMaterial* Material = Scene.Materials[Proxy->MaterialIdx];
			FLambertPipeline::FConstant Constant
			{
				.MaterialType = (int)Material->Type,
				.LightsCount = 	(int)Scene.Lights2Nodes.size(),
			};
			vkCmdPushConstants(workspace.command_buffer, LambertPipeline.Layout, 
								VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Constant), &Constant);

		}
		vkCmdBindDescriptorSets
		(
			workspace.command_buffer,			// Command buffer
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// Pipeline bind point
			LambertPipeline.Layout,				// Pipeline Layout
			3, 	// Third Sets
			1, &MaterialDescriptors[Proxy->MaterialIdx],	// descriptor sets count, ptr
			0, nullptr	// Dynamic offsets count, ptr
		);
		vkCmdDraw(workspace.command_buffer, Proxy->VertexNum, 1, Proxy->FirstVertexIdx, i);
	}

}

void URenderPipelines::RenderShadowMaps(FWorkspace& Workspace)
{
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
            .renderPass = ShadowPass,
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
	}

}

void URenderPipelines::RenderCubeShadowMaps(FWorkspace& Workspace)
{
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
                .renderPass = ShadowPass,
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
		.renderPass = ShadowPass,
		.attachmentCount = 1,
		.pAttachments = &ShadowRes.ImageView,
		.width = ShadowResolution,
		.height = ShadowResolution,
		.layers = 1,
	};
	VK(vkCreateFramebuffer(rtg.device, &FrameBufferInfo, nullptr, &ShadowRes.Framebuffer));

	SpotLightShadows.push_back(std::move(ShadowRes));
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
			.renderPass = ShadowPass,
			.attachmentCount = 1,
			.pAttachments = &CubeShadowRes.FaceViews[i],
			.width = ShadowResolution,
			.height = ShadowResolution,
			.layers = 1,
		};
		VK(vkCreateFramebuffer(rtg.device, &FrameBufferInfo, nullptr, &CubeShadowRes.FaceFramebuffers[i]));
	}

	SphereLightShadows.push_back(std::move(CubeShadowRes));
}
//~END Shadow