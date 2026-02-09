#include "AssignmentOne.hpp"

#include "../VK.hpp"

#include <GLFW/glfw3.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>	// TODO: delete this include
#include "../A1/Render/RenderScene.hpp"
#include "../A1/Render/RenderExtractor.hpp"
#define GLM_ENABLE_EXPERIMENTAL	// TODO: delete this include
#include "glm/glm/gtx/string_cast.hpp"	// TODO: delete this include

UAssignmentOne::UAssignmentOne(RTG &rtg_) : rtg(rtg_)
{
	// load the scene
	InitializeRenderScene();
	PrintRenderSceneMesh();
	PrintRenderProxies();

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

    BackgroundPipeline.Create(rtg, RenderPass, 0);
    LambertPipeline.Create(rtg, RenderPass, 0);

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
				.descriptorCount = 1 * PerWorkspace,	// one descriptor per set, one set per workspace
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, // because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 3 * PerWorkspace, // three set per workspace
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &DescriptorPool));
	}
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
        
		// Create Object Vertices
		{
			// std::vector< URenderMesh::FVertex > Vertices;
			// Create Quadrilateral:
			// InstantializePlane(Vertices);

			// Create Torus
			// InstantializeTorus(Vertices);

			// size_t Bytes = Vertices.size() * sizeof(Vertices[0]);

			// ObjectVertices = rtg.helpers.create_buffer
			// (
			// 	Bytes,
			// 	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			// 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			// 	Helpers::Unmapped
			// );

			// // copy data to buffer
			// rtg.helpers.transfer_to_buffer(Vertices.data(), Bytes, ObjectVertices);
			// TODO: Upload all mesh vertices
			ObjectVertices = rtg.helpers.create_buffer
			(
				Scene.TotalBytes,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped
			);
			rtg.helpers.transfer_to_buffer(Scene.StagingData.data(), Scene.TotalBytes, ObjectVertices);
		}
	}

    // make some Textures
	LoadDefaultComputeTextures();

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
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = Image.format,
				// .components sets swizzling and is fine when zero-initialized
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
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
		VK( vkCreateSampler(rtg.device, &CreateInfo, nullptr, &TextureSampler) );
	}

	// create the texture descriptor pool	
	{
		uint32_t PerTexture = uint32_t(Textures.size());

		std::array< VkDescriptorPoolSize, 1 > PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1 * 1 * PerTexture,	 // one descriptor per set, one set per texture
			}
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, 	// because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 1 * PerTexture, 	// one set per texture
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK(vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &TextureDescriptorPool));
	}

	 // allocate and write the texture descriptor sets
	{
		// allocate the descriptors (using the same alloc_info):
		VkDescriptorSetAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = TextureDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &LambertPipeline.Set3_TEXTURE,
		};

		TextureDescriptors.assign(Textures.size(), VK_NULL_HANDLE);

		for (VkDescriptorSet &DescriptorSet : TextureDescriptors)
		{
			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &DescriptorSet));
		}
		
		// write descriptors for textures
		std::vector< VkDescriptorImageInfo > Infos(Textures.size());
		std::vector< VkWriteDescriptorSet > Writes(Textures.size());

		for (Helpers::AllocatedImage const &Image : Textures)
		{
			size_t Index = &Image - &Textures[0];

			Infos[Index] = VkDescriptorImageInfo
			{
				.sampler = TextureSampler,
				.imageView = TextureViews[Index],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			Writes[Index] = VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = TextureDescriptors[Index],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &Infos[Index],
			};
		}

		vkUpdateDescriptorSets(rtg.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
	}
}

UAssignmentOne::~UAssignmentOne()
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
		TextureDescriptors.clear();
	}

	if(TextureSampler)
	{
		vkDestroySampler(rtg.device, TextureSampler, nullptr);
		TextureSampler = VK_NULL_HANDLE;
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

	for (FWorkspace &workspace : workspaces) 
	{
		if(workspace.command_buffer != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(rtg.device, CommandPool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
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
	}
	workspaces.clear();

	if(DescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, DescriptorPool, nullptr);
		DescriptorPool = nullptr;
		// (this also frees the descriptor sets allocated from the pool)
	}

	BackgroundPipeline.Destroy(rtg);
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
}

void UAssignmentOne::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain)
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

void UAssignmentOne::DestroyFramebuffers()
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

void UAssignmentOne::render(RTG &rtg_, RTG::RenderParams const &render_params)
{
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

	if(!Scene.ProxyInstances.empty())
	{
		// upload object transforms:
		size_t NeededBytes = Scene.ProxyInstances.size() * sizeof(UAssignmentOne::FLambertPipeline::FTransform);
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

			// Copy Transform into TransformSrc
			{
				assert(workspace.TransformsSrc.allocation.mapped);
				URenderProxy::FTransform *Out = reinterpret_cast< URenderProxy::FTransform * >(workspace.TransformsSrc.allocation.data()); // Strict aliasing violation, but it doesn't matter
				for (URenderProxy* Inst : Scene.ProxyInstances)
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
		assert(workspace.CameraSrc.size == sizeof(World));

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
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, // dstStageMask
			0, 					// dependencyFlags
			1, &MemoryBarrier,  // memoryBarriers (count, data)
			0, nullptr,  		// bufferMemoryBarriers (count, data)
			0, nullptr			// imageMemoryBarriers (count, data)
		);
	}

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

		
		// set scissor rectangle
		{
			VkRect2D scissor
			{
				.offset = { .x = 0, .y = 0 },
				.extent = rtg.swapchain_extent,
			};
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor);
		}
		
		// configure viewport transform:
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
		}
		
		RenderBackgroundPipeline(workspace);
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

void UAssignmentOne::RenderBackgroundPipeline(FWorkspace &workspace)
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

void UAssignmentOne::RenderLambertPipeline(FWorkspace &workspace)
{
    // Draw with the objects pipeline:
	if (!Scene.ProxyInstances.empty()) 
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

	// Camera descriptor set is still bound, but unused(!)

	// Use FRenderProxy as ObjectInstance
	const std::vector<URenderProxy*>& ProxyList = Scene.ProxyInstances;
	for (uint32_t i = 0; i < ProxyList.size(); i++)
	{
		const URenderProxy* Inst = ProxyList[i];
		vkCmdBindDescriptorSets
		(
			workspace.command_buffer,			// Command buffer
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// Pipeline bind point
			LambertPipeline.Layout,				// Pipeline Layout
			3, 	// Third Sets
			1, &TextureDescriptors[Inst->Texture],	// descriptor sets count, ptr
			0, nullptr	// Dynamic offsets count, ptr
		);

		vkCmdDraw(workspace.command_buffer, Inst->VertexNum, 1, Inst->FirstVertexIdx, i);
	}


}

void UAssignmentOne::update(float dt)
{
    Time = std::fmod(Time + dt, 60.0f);

	// camera orbiting the origin:
	if(CameraMode == ECameraMode::Scene)
	{
		// camera rotating around the origin:
		float Angle = float(M_PI) * 2.0f * 2.0f * (Time / 60.0f);
		CLIP_FROM_WORLD = Perspective
		(
			60.0f * float(M_PI) / 180.0f, // vfov
			rtg.swapchain_extent.width / float(rtg.swapchain_extent.height), // aspect
			0.1f, // near
			10000.0f // far
		) * Look_at
		(
			10.0f * std::cos(Angle), 10.0f * std::sin(Angle), 10.0f, // eye
			0.0f, 0.0f, 0.5f, // target
			0.0f, 0.0f, 1.0f // up
		);
	}
	else if(CameraMode == ECameraMode::Free)
	{
		CLIP_FROM_WORLD = Perspective
		(
			FreeCamera.FOV,
			rtg.swapchain_extent.width / float(rtg.swapchain_extent.height),	// Aspect
			FreeCamera.Near,
			FreeCamera.Far
		) * orbit
		(
			FreeCamera.TargetX, FreeCamera.TargetY, FreeCamera.TargetZ,
			FreeCamera.Azimuth, FreeCamera.Elevation, FreeCamera.Radius
		);
	}
	else
	{
		assert(0 && "Only Two Camera Mode!");
	}

	// static sun and sky
	{
		World.SKY_DIRECTION.x = 0.0f;
		World.SKY_DIRECTION.y = 0.0f;
		World.SKY_DIRECTION.z = 1.0f;

		World.SKY_ENERGY.r = 0.1f;
		World.SKY_ENERGY.g = 0.1f;
		World.SKY_ENERGY.b = 0.2f;

		World.SUN_DIRECTION.x = Time * 10 / 23.0f;
		World.SUN_DIRECTION.y = 13.0f / 23.0f;
		World.SUN_DIRECTION.z = 18.0f / 23.0f;
		World.DirectionNormalize();		// Normalize vector


		World.SUN_ENERGY.r = 1.0f;
		World.SUN_ENERGY.g = 1.0f;
		World.SUN_ENERGY.b = 0.9f;		
	}
}

void UAssignmentOne::on_input(InputEvent const &evt)
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
		
		if(evt.type == InputEvent::MouseButtonDown &&
			evt.button.button == GLFW_MOUSE_BUTTON_LEFT &&
			evt.button.mods & GLFW_MOD_SHIFT)
		{
			// start panning
			float InitX = evt.button.x;
			float InitY = evt.button.y;
			FOrbitCamera InitCamera = FreeCamera;

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
					Mat4 CameraFromWorld = orbit
					(
						InitCamera.TargetX, InitCamera.TargetY, InitCamera.TargetZ,
						InitCamera.Azimuth, InitCamera.Elevation, InitCamera.Radius
					);

					//move the desired distance:
					FreeCamera.TargetX = InitCamera.TargetX - Dx * CameraFromWorld[0] - Dy * CameraFromWorld[1];
					FreeCamera.TargetY = InitCamera.TargetY - Dx * CameraFromWorld[4] - Dy * CameraFromWorld[5];
					FreeCamera.TargetZ = InitCamera.TargetZ - Dx * CameraFromWorld[8] - Dy * CameraFromWorld[9];

					return;
				}
			};
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
}

void UAssignmentOne::InitializeRenderScene()
{
	URenderExtractor::BuildRenderScene(FilePath, Scene);
}

//TODO: Delete this function
void UAssignmentOne::PrintRenderSceneMesh()
{
	std::cout << "======= Render Scene Mesh Debug Info =======" << std::endl;
    
    int MeshIndex = 0;
    for (const auto& [NodePtr, Mesh] : Scene.Nodes2RenderMeshes) {
        std::cout << "\n[Mesh #" << MeshIndex++ << "]" << std::endl;
        
        
        std::cout << "Bounding Box Min: " << glm::to_string(Mesh->BoundingBox.Min) << std::endl;
        std::cout << "Bounding Box Max: " << glm::to_string(Mesh->BoundingBox.Max) << std::endl;
        
        
        std::cout << "Vertex Count: " << Mesh->VertexCount << std::endl;
        
        
        const uint8_t* DataPtr = Mesh->VertexData.data();
        
        for (uint32_t i = 0; i < Mesh->VertexCount; ++i) {
            size_t Offset = i * Mesh->VertexStride;
            
            const auto* Vertex = reinterpret_cast<const URenderMesh::FVertex*>(DataPtr + Offset);
            
            if (i < 5 || i == Mesh->VertexCount - 1) { 
                std::cout << "  Vertex [" << i << "] Position: " 
                          << glm::to_string(Vertex->Position) << std::endl;
            } else if (i == 5) {
                std::cout << "  ... (skipping intermediate vertices) ..." << std::endl;
            }
        }
    }
    std::cout << "============================================" << std::endl;
}

void UAssignmentOne::PrintMatrix(const std::string& name, const glm::mat4& m) {
    std::cout << "    " << name << ":" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "      [ ";
        for (int j = 0; j < 4; ++j) {
            std::cout << std::setw(8) << std::fixed << std::setprecision(2) << m[j][i] << " ";
        }
        std::cout << "]" << std::endl;
    }
}

void UAssignmentOne::PrintRenderProxies() {
    std::cout << "=== Render Scene Proxies (Count: " << Scene.ProxyInstances.size() << ") ===" << std::endl;
    for (size_t i = 0; i < Scene.ProxyInstances.size(); ++i) {
        const auto& p = Scene.ProxyInstances[i];
        std::cout << "Proxy [" << i << "]:" << std::endl;
        std::cout << "  - Vertex: StartIdx=" << p->FirstVertexIdx << ", Count=" << p->VertexNum << std::endl;
        std::cout << "  - Texture ID: " << p->Texture << std::endl;
        
        PrintMatrix("WORLD_FROM_LOCAL", p->Transform.WORLD_FROM_LOCAL);
        std::cout << "-----------------------------------------------" << std::endl;
    }
}

//BEGIN~ Instantialize Mesh's Vertices
void UAssignmentOne::InstantializePlane(std::vector< FVertexDataSet > &Vertices)
{
	PlaneVertices.first = uint32_t(Vertices.size());
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = -10.0f, .y = -10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		.Texcoord{ .u = 0.0f, .v = 0.0f },
	});
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = 10.0f, .y = -10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		.Texcoord{ .u = 10.0f, .v = 0.0f },
	});
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = -10.0f, .y = 10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		.Texcoord{ .u = 0.0f, .v = 10.0f },
	});
	
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = 10.0f, .y = 10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		.Texcoord{ .u = 10.0f, .v = 10.0f },
	});
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = -10.0f, .y = 10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		.Texcoord{ .u = 0.0f, .v = 10.0f },
	});
	Vertices.emplace_back(FVertexDataSet
	{
		.Position{ .x = 10.0f, .y = -10.0f, .z = -0.14f },
		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		.Texcoord{ .u = 10.0f, .v = 0.0f },
	});

	PlaneVertices.count = uint32_t(Vertices.size() - PlaneVertices.first);
}

void UAssignmentOne::InstantializeTorus(std::vector< FVertexDataSet > &Vertices)
{
	TorusVertices.first = uint32_t(Vertices.size());

	// will parameterize with (u,v) where:
	// - u is angle around main axis (+z)
	// - v is angle around the tube

	constexpr float R1 = 0.75f; // main radius
	constexpr float R2 = 0.25f; // tube radius

	constexpr uint32_t U_STEPS = 20;
	constexpr uint32_t V_STEPS = 16;

	// texture repeats around the torus:
	constexpr float V_REPEATS = 2.0f;
	constexpr float U_REPEATS = int(V_REPEATS / R2 * R1 + 0.999f); // approximately square, rounded up

	auto emplace_vertex = [&](uint32_t ui, uint32_t vi) {
		// convert steps to angles:
		// (doing the mod since trig on 2 M_PI may not exactly match 0)
		float ua = (ui % U_STEPS) / float(U_STEPS) * 2.0f * float(M_PI);
		float va = (vi % V_STEPS) / float(V_STEPS) * 2.0f * float(M_PI);

		Vertices.emplace_back( FVertexDataSet
			{
			.Position
			{
				.x = (R1 + R2 * std::cos(va)) * std::cos(ua),
				.y = (R1 + R2 * std::cos(va)) * std::sin(ua),
				.z = R2 * std::sin(va),
			},
			.Normal
			{
				.x = std::cos(va) * std::cos(ua),
				.y = std::cos(va) * std::sin(ua),
				.z = std::sin(va),
			},
			.Texcoord
			{
				.u = ui / float(U_STEPS) * U_REPEATS,
				.v = vi / float(V_STEPS) * V_REPEATS,
			},
		});
	};

	for (uint32_t ui = 0; ui < U_STEPS; ++ui) 
	{
		for (uint32_t vi = 0; vi < V_STEPS; ++vi) 
		{
			emplace_vertex(ui, vi);
			emplace_vertex(ui+1, vi);
			emplace_vertex(ui, vi+1);

			emplace_vertex(ui, vi+1);
			emplace_vertex(ui+1, vi);
			emplace_vertex(ui+1, vi+1);
		}
	}

	TorusVertices.count = uint32_t(Vertices.size()) - TorusVertices.first;
}

void UAssignmentOne::LoadDefaultComputeTextures()
{
    Textures.reserve(2);
	//texture 0 will be a dark grey / light grey checkerboard with a red square at the origin.
	{ 
		//actually make the texture:
		uint32_t size = 128;
		std::vector< uint32_t > data;
		data.reserve(size * size);
		for (uint32_t y = 0; y < size; ++y) {
			float fy = (y + 0.5f) / float(size);
			for (uint32_t x = 0; x < size; ++x) {
				float fx = (x + 0.5f) / float(size);
				//highlight the origin:
				if      (fx < 0.05f && fy < 0.05f) data.emplace_back(0xff0000ff); //red
				else if ( (fx < 0.5f) == (fy < 0.5f)) data.emplace_back(0xff444444); //dark grey
				else data.emplace_back(0xffbbbbbb); //light grey
			}
		}
		assert(data.size() == size*size);

		// make a place for the texture to live on the GPU
		Textures.emplace_back(rtg.helpers.create_image(
			VkExtent2D{ .width = size , .height = size }, //size of image
			VK_FORMAT_R8G8B8A8_UNORM, //how to interpret image data (in this case, linearly-encoded 8-bit RGBA)
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
			Helpers::Unmapped
		));

		// transfer data
		rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), Textures.back());
	}
	// texture 1 will be a classic 'xor' texture:
	{ 
		//actually make the texture:
		uint32_t size = 256;
		std::vector< uint32_t > data;
		data.reserve(size * size);
		for (uint32_t y = 0; y < size; ++y) {
			for (uint32_t x = 0; x < size; ++x) {
				uint8_t r = uint8_t(x) ^ uint8_t(y);
				uint8_t g = uint8_t(x + 128) ^ uint8_t(y);
				uint8_t b = uint8_t(x) ^ uint8_t(y + 27);
				uint8_t a = 0xff;
				data.emplace_back( uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24) );
			}
		}
		assert(data.size() == size*size);

		//make a place for the texture to live on the GPU:
		Textures.emplace_back(rtg.helpers.create_image(
			VkExtent2D{ .width = size , .height = size }, //size of image
			VK_FORMAT_R8G8B8A8_SRGB, //how to interpret image data (in this case, SRGB-encoded 8-bit RGBA)
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
			Helpers::Unmapped
		));

		//transfer data:
		rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), Textures.back());
	}
}
//END~
