#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

#include "ImageProcessor.hpp"
#include "CubePipeline.hpp"
#include "LUTPipeline.hpp"
#include "GPUFace.hpp"
#include "../Main/Render/Color.hpp"
#include "../VK.hpp"

#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

void UImageProcessor::GGXProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, float InRoughness, float InOutRatio, const std::string& OutputFileName)
{
	// Initialize Pipeline & Process Logic
	FCubePipeline Pipeline;
	Pipeline.Create(CubeExe);

	VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
	// create descriptor pool
	{
		std::array< VkDescriptorPoolSize, 1> PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 3, // one for each input and output cube face
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 2 + 1, //one set per in/out cube face, plus one for params
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};
		VK( vkCreateDescriptorPool(CubeExe.device, &CreateInfo, nullptr, &DescriptorPool) );
	}
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	// create command pool
	{
		VkCommandPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = CubeExe.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(CubeExe.device, &CreateInfo, nullptr, &CommandPool) );
	}

	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	// allocate a command buffer from the command pool:
	{ 
		VkCommandBufferAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VK( vkAllocateCommandBuffers(CubeExe.device, &AllocInfo, &CommandBuffer) );
	}

	// load images / create descriptors
	int Width, Height, Channels;
	stbi_set_flip_vertically_on_load(false);
	const char* InPath = CubeExe.configuration.InImagePath.c_str();
	uint8_t* Data = stbi_load(InPath, &Width, &Height, &Channels, 4);
	
	std::vector< vec3 > InputFloats;
	for (int i = 0; i < Width * Height; ++i) 
	{
		InputFloats.push_back(RGBE2Float(glm::u8vec4(Data[i*4], Data[i*4+1], Data[i*4+2], Data[i*4+3])));
	}

	const float CurrentSize = (float)Width * InOutRatio;
	size_t OutSize = (size_t)CurrentSize < 1 ? 1 : (size_t)CurrentSize;	// clamp for size
	std::vector< vec3 > OutData (OutSize * OutSize * 6, vec3(0.0f));

	// Create 
	FGPUFace InFace;
	FGPUFace OutFace;
	InFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)Width, (uint32_t)Height, InputFloats.data());
	OutFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)OutSize, (uint32_t)OutSize * 6, OutData.data(), true);

	FCubePipeline::Params params;
	params.Roughness = InRoughness;

	CubeHelpers::AllocatedBuffer ParamsBuffer = CubeExe.helpers.create_buffer
	(
		sizeof(FCubePipeline::Params),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		CubeHelpers::Unmapped
	);
	CubeExe.helpers.transfer_to_buffer(&params, sizeof(params), ParamsBuffer);

	VkDescriptorSet ParamsSet;
	{
		VkDescriptorSetAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = DescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &Pipeline.Set2_Params,
		};
		VK(vkAllocateDescriptorSets(CubeExe.device, &AllocInfo, &ParamsSet));

		VkDescriptorBufferInfo ParamsBufferInfo
		{
			.buffer = ParamsBuffer.handle,
			.offset = 0,
			.range = sizeof(FCubePipeline::Params),
		};

		VkWriteDescriptorSet ParamsWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ParamsSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &ParamsBufferInfo,
		};

		vkUpdateDescriptorSets(CubeExe.device, 1, &ParamsWrite, 0, nullptr);
	}


	// run pipeline
	VK( vkResetCommandBuffer(CommandBuffer, 0) );

	// begin recording:
	{
		VkCommandBufferBeginInfo BeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		VK( vkBeginCommandBuffer(CommandBuffer, &BeginInfo) );
	}
	// use the cube pipeline:
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline.Handle);
	

	{ //bind in/out descriptor sets:
		std::array< VkDescriptorSet, 3 > DescriptorSets
		{
			InFace.Descriptors,
			OutFace.Descriptors,
			ParamsSet,
		};
		vkCmdBindDescriptorSets
		(
			CommandBuffer,                  // command buffer
			VK_PIPELINE_BIND_POINT_COMPUTE, // pipeline bind point
			Pipeline.Layout,                // pipeline layout
			0,                              // first set
			uint32_t(DescriptorSets.size()), DescriptorSets.data(), // descriptor sets count, ptr
			0, nullptr // dynamic offsets count, ptr
		);
	}
	// actually run the thing:
	vkCmdDispatch(CommandBuffer, (uint32_t)(OutSize + 7) / 8, (uint32_t)(OutSize + 7) / 8, 6);

	// Barrier
	{
		VkImageMemoryBarrier Barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = OutFace.Image.handle,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 6 }
		};

		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);
	}
	
	CubeHelpers::AllocatedBuffer StagingBuffer = CubeExe.helpers.create_buffer
	(
		OutSize * OutSize * 6 * sizeof(glm::vec4),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		CubeHelpers::Mapped
	);

	VkBufferImageCopy CopyRegion
	{
		.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 6 },
		.imageExtent = { .width = (uint32_t)OutSize, .height = (uint32_t)OutSize, .depth = 1 }
	};

	vkCmdCopyImageToBuffer(CommandBuffer, OutFace.Image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer.handle, 1, &CopyRegion);

	// done recording:
	VK( vkEndCommandBuffer(CommandBuffer) );

	// submit command buffer:
	{
		VkSubmitInfo SubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &CommandBuffer,
		};

		VK( vkQueueSubmit(CubeExe.graphics_queue, 1, &SubmitInfo, nullptr) );
	}

	VK( vkDeviceWaitIdle(CubeExe.device) );

	// read texture back to memory
	vec4* RawData = reinterpret_cast<vec4*>(StagingBuffer.allocation.data());
	if (RawData == nullptr) 
	{
		std::cerr << "error: memory is invisible!" << std::endl;
		return;
	}
	else
	{
		size_t TotalPixels = OutSize * OutSize * 6;
		std::vector< glm::u8vec4 > OutputData;

		for (uint32_t i = 0; i < TotalPixels; ++i) 
		{
			glm::u8vec4 floatValue = Float2RGBE(vec3(RawData[i]));
			OutputData.push_back(floatValue);
		}

		// 3. write to disk
		const char* OutPath = OutputFileName.c_str();
		if (stbi_write_png(OutPath, (int)OutSize, (int)OutSize * 6, 4, OutputData.data(), (int)OutSize * 4)) 
		{
			std::cout << "save successfully: " << OutPath << std::endl;
		} 
		else 
		{
			std::cerr << "save failed!" << std::endl;
		}
	}
}

void UImageProcessor::IrradianceProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, size_t OutputSize, const std::string& OutputFileName)
{
	// Initialize Pipeline & Process Logic
	FCubePipeline Pipeline;
	Pipeline.Create(CubeExe);

	VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
	// create descriptor pool
	{
		std::array< VkDescriptorPoolSize, 1> PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 3, // one for each input and output cube face
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 2 + 1, //one set per in/out cube face, plus one for params
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};
		VK( vkCreateDescriptorPool(CubeExe.device, &CreateInfo, nullptr, &DescriptorPool) );
	}
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	// create command pool
	{
		VkCommandPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = CubeExe.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(CubeExe.device, &CreateInfo, nullptr, &CommandPool) );
	}

	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	// allocate a command buffer from the command pool:
	{ 
		VkCommandBufferAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VK( vkAllocateCommandBuffers(CubeExe.device, &AllocInfo, &CommandBuffer) );
	}

	// load images / create descriptors
	int Width, Height, Channels;
	stbi_set_flip_vertically_on_load(false);
	const char* InPath = CubeExe.configuration.InImagePath.c_str();
	uint8_t* Data = stbi_load(InPath, &Width, &Height, &Channels, 4);
	
	std::vector< vec3 > InputFloats;
	for (int i = 0; i < Width * Height; ++i) 
	{
		InputFloats.push_back(RGBE2Float(glm::u8vec4(Data[i*4], Data[i*4+1], Data[i*4+2], Data[i*4+3])));
	}

	size_t OutSize = OutputSize;
	std::vector< vec3 > OutData (OutSize * OutSize * 6, vec3(0.0f));

	// Create 
	FGPUFace InFace;
	FGPUFace OutFace;
	InFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)Width, (uint32_t)Height, InputFloats.data());
	OutFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)OutSize, (uint32_t)OutSize * 6, OutData.data(), true);

	// run pipeline
	VK( vkResetCommandBuffer(CommandBuffer, 0) );

	// begin recording:
	{
		VkCommandBufferBeginInfo BeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		VK( vkBeginCommandBuffer(CommandBuffer, &BeginInfo) );
	}
	// use the cube pipeline:
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline.Handle);
	

	{ //bind in/out descriptor sets:
		std::array< VkDescriptorSet, 2 > DescriptorSets
		{
			InFace.Descriptors,
			OutFace.Descriptors,
		};
		vkCmdBindDescriptorSets
		(
			CommandBuffer,                  // command buffer
			VK_PIPELINE_BIND_POINT_COMPUTE, // pipeline bind point
			Pipeline.Layout,                // pipeline layout
			0,                              // first set
			uint32_t(DescriptorSets.size()), DescriptorSets.data(), // descriptor sets count, ptr
			0, nullptr // dynamic offsets count, ptr
		);
	}
	// actually run the thing:
	vkCmdDispatch(CommandBuffer, (uint32_t)(OutSize + 7) / 8, (uint32_t)(OutSize + 7) / 8, 6);

	// Barrier
	{
		VkImageMemoryBarrier Barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = OutFace.Image.handle,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 6 }
		};

		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);
	}
	
	CubeHelpers::AllocatedBuffer StagingBuffer = CubeExe.helpers.create_buffer
	(
		OutSize * OutSize * 6 * sizeof(glm::vec4),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		CubeHelpers::Mapped
	);

	VkBufferImageCopy CopyRegion
	{
		.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 6 },
		.imageExtent = { .width = (uint32_t)OutSize, .height = (uint32_t)OutSize, .depth = 1 }
	};

	vkCmdCopyImageToBuffer(CommandBuffer, OutFace.Image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer.handle, 1, &CopyRegion);

	// done recording:
	VK( vkEndCommandBuffer(CommandBuffer) );

	// submit command buffer:
	{
		VkSubmitInfo SubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &CommandBuffer,
		};

		VK( vkQueueSubmit(CubeExe.graphics_queue, 1, &SubmitInfo, nullptr) );
	}

	VK( vkDeviceWaitIdle(CubeExe.device) );

	// read texture back to memory
	vec4* RawData = reinterpret_cast<vec4*>(StagingBuffer.allocation.data());
	if (RawData == nullptr) 
	{
		std::cerr << "error: memory is invisible!" << std::endl;
		return;
	}
	else
	{
		size_t TotalPixels = OutSize * OutSize * 6;
		std::vector< glm::u8vec4 > OutputData;

		for (uint32_t i = 0; i < TotalPixels; ++i) 
		{
			glm::u8vec4 floatValue = Float2RGBE(vec3(RawData[i]));
			OutputData.push_back(floatValue);
		}

		// 3. write to disk
		const char* OutPath = OutputFileName.c_str();
		if (stbi_write_png(OutPath, (int)OutSize, (int)OutSize * 6, 4, OutputData.data(), (int)OutSize * 4)) 
		{
			std::cout << "save successfully: " << OutPath << std::endl;
		} 
		else 
		{
			std::cerr << "save failed!" << std::endl;
		}
	}
}

void UImageProcessor::BRDFLUTProcess(CubeExecute& CubeExe, CubeExecute::Configuration& configuration, const std::string& OutputFileName)
{
	stbi_flip_vertically_on_write(true);
	// Initialize Pipeline & Process Logic
	FLUTPipeline Pipeline;
	Pipeline.Create(CubeExe);

	VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
	// create descriptor pool
	{
		std::array< VkDescriptorPoolSize, 1> PoolSizes
		{
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 3, // one for each input and output cube face
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 2 + 1, //one set per in/out cube face, plus one for params
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};
		VK( vkCreateDescriptorPool(CubeExe.device, &CreateInfo, nullptr, &DescriptorPool) );
	}
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	// create command pool
	{
		VkCommandPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = CubeExe.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(CubeExe.device, &CreateInfo, nullptr, &CommandPool) );
	}

	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	// allocate a command buffer from the command pool:
	{ 
		VkCommandBufferAllocateInfo AllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VK( vkAllocateCommandBuffers(CubeExe.device, &AllocInfo, &CommandBuffer) );
	}

	const uint32_t OutSize = 256;
	std::vector< vec2 > OutData (OutSize * OutSize, vec2(0.0f));

	// Create 
	FGPUFace OutFace;
	OutFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)OutSize, OutData.data());

	// run pipeline
	VK( vkResetCommandBuffer(CommandBuffer, 0) );

	// begin recording:
	{
		VkCommandBufferBeginInfo BeginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		VK( vkBeginCommandBuffer(CommandBuffer, &BeginInfo) );
	}
	// use the cube pipeline:
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline.Handle);
	

	{ //bind in/out descriptor sets:
		std::array< VkDescriptorSet, 1 > DescriptorSets
		{
			OutFace.Descriptors,
		};
		vkCmdBindDescriptorSets
		(
			CommandBuffer,                  // command buffer
			VK_PIPELINE_BIND_POINT_COMPUTE, // pipeline bind point
			Pipeline.Layout,                // pipeline layout
			0,                              // first set
			uint32_t(DescriptorSets.size()), DescriptorSets.data(), // descriptor sets count, ptr
			0, nullptr // dynamic offsets count, ptr
		);
	}
	// actually run the thing:
	vkCmdDispatch(CommandBuffer, (uint32_t)(OutSize + 7) / 8, (uint32_t)(OutSize + 7) / 8, 1);

	// Barrier
	{
		VkImageMemoryBarrier Barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = OutFace.Image.handle,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		};

		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);
	}
	
	CubeHelpers::AllocatedBuffer StagingBuffer = CubeExe.helpers.create_buffer
	(
		OutSize * OutSize * 1 * sizeof(uint32_t),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		CubeHelpers::Mapped
	);

	VkBufferImageCopy CopyRegion
	{
		.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
		.imageExtent = { .width = (uint32_t)OutSize, .height = (uint32_t)OutSize, .depth = 1 }
	};

	vkCmdCopyImageToBuffer(CommandBuffer, OutFace.Image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer.handle, 1, &CopyRegion);

	// done recording:
	VK( vkEndCommandBuffer(CommandBuffer) );

	// submit command buffer:
	{
		VkSubmitInfo SubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &CommandBuffer,
		};

		VK( vkQueueSubmit(CubeExe.graphics_queue, 1, &SubmitInfo, nullptr) );
	}

	VK( vkDeviceWaitIdle(CubeExe.device) );

	// read texture back to memory
	uint32_t* RawData = reinterpret_cast<uint32_t*>(StagingBuffer.allocation.data());
	if (RawData == nullptr) 
	{
		std::cerr << "error: memory is invisible!" << std::endl;
		return;
	}
	else
	{
		size_t TotalPixels = OutSize * OutSize;
		std::vector<vec2> OutData_1(TotalPixels);

		for (size_t i = 0; i < TotalPixels; ++i)
		{
			OutData_1[i] = glm::unpackHalf2x16(RawData[i]); // uint32_t -> vec2
		}

		std::vector< glm::u8vec4 > OutputData;
		OutputData.reserve(TotalPixels);

		for (uint32_t i = 0; i < TotalPixels; ++i) 
		{
			uint8_t r = static_cast<uint8_t>(glm::clamp(OutData_1[i].x * 255.0f, 0.0f, 255.0f));
    		uint8_t g = static_cast<uint8_t>(glm::clamp(OutData_1[i].y * 255.0f, 0.0f, 255.0f));
    		OutputData.push_back(glm::u8vec4(r, g, 0, 255));
		}

		std::vector<float> FloatData;
		for (uint32_t i = 0; i < TotalPixels; ++i) 
		{
			FloatData.push_back(OutData_1[i].x);
			FloatData.push_back(OutData_1[i].y);
			FloatData.push_back(0.0f);
		}

		// 3. write to disk
		const char* OutPath = OutputFileName.c_str();
		if (stbi_write_hdr(OutPath, (int)OutSize, (int)OutSize , 3, FloatData.data())) 
		{
			std::cout << "save successfully: " << OutPath << std::endl;
		} 
		else 
		{
			std::cerr << "save failed!" << std::endl;
		}
	}
}
