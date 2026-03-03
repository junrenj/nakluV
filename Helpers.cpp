#include "Helpers.hpp"

#include "RTG.hpp"
#include "VK.hpp"

#include <vulkan/utility/vk_format_utils.h> // useful for byte counting

#include <utility>
#include <cassert>
#include <cstring>
#include <iostream>

Helpers::Allocation::Allocation(Allocation &&from) {
	assert(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr);

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);
}

Helpers::Allocation &Helpers::Allocation::operator=(Allocation &&from) {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		//not fatal, just sloppy, so complain but don't throw:
		std::cerr << "Replacing a non-empty allocation; device memory will leak." << std::endl;
	}

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);

	return *this;
}

Helpers::Allocation::~Allocation() {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		std::cerr << "Destructing a non-empty Allocation; device memory will leak." << std::endl;
	}
}

//----------------------------

Helpers::Allocation Helpers::Allocate(VkDeviceSize size, VkDeviceSize alignment, uint32_t memory_type_index, MapFlag map)
{
	Helpers::Allocation AllocationTemp;

	VkMemoryAllocateInfo AllocationInfo
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = size,
		.memoryTypeIndex = memory_type_index
	};

	VK( vkAllocateMemory( rtg.device, &AllocationInfo, nullptr, &AllocationTemp.handle ) );

	AllocationTemp.size = size;
	AllocationTemp.offset = 0;

	if (map == Mapped) 
	{
		VK( vkMapMemory(rtg.device, AllocationTemp.handle, 0, AllocationTemp.size, 0, &AllocationTemp.mapped) );
	}

	return AllocationTemp;
}

Helpers::Allocation Helpers::Allocate(VkMemoryRequirements const &req, VkMemoryPropertyFlags properties, MapFlag map)
{
	return Allocate(req.size, req.alignment, FindMemoryType(req.memoryTypeBits, properties), map);
}

void Helpers::Free(Helpers::Allocation &&Allocation)
{
	if(Allocation.mapped != nullptr)
	{
		vkUnmapMemory(rtg.device, Allocation.handle);
		Allocation.mapped = nullptr;
	}

	vkFreeMemory(rtg.device, Allocation.handle, nullptr);

	Allocation.handle = VK_NULL_HANDLE;
	Allocation.offset = 0;
	Allocation.size = 0;
}

Helpers::AllocatedBuffer Helpers::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedBuffer buffer;
	VkBufferCreateInfo CreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VK( vkCreateBuffer(rtg.device, &CreateInfo, nullptr, &buffer.handle));
	buffer.size = size;

	// determine memory requirements
	VkMemoryRequirements Request;
	vkGetBufferMemoryRequirements(rtg.device, buffer.handle, &Request);

	// allocate memory
	buffer.allocation = Allocate(Request, properties, map);

	// bind memory
	VK( vkBindBufferMemory(rtg.device, buffer.handle, buffer.allocation.handle, buffer.allocation.offset));

	return buffer;
}

void Helpers::destroy_buffer(AllocatedBuffer &&buffer) 
{
	vkDestroyBuffer(rtg.device, buffer.handle, nullptr);
	buffer.handle = VK_NULL_HANDLE;
	buffer.size = 0;

	this->Free(std::move(buffer.allocation));
}

Helpers::AllocatedImage Helpers::create_image(VkExtent2D const &extent, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map, uint8_t layers, VkImageCreateFlags flag, uint32_t mipLevels) {
	AllocatedImage image;
	
	image.extent = extent;
	image.format = format;
	image.layers = layers;
	image.mipLevels = mipLevels;

	VkImageCreateInfo CreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = flag,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent
		{
			.width = extent.width,
			.height = extent.height,
			.depth = 1
		},
		.mipLevels = mipLevels,
		.arrayLayers = layers,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VK( vkCreateImage(rtg.device, &CreateInfo, nullptr, &image.handle));

	VkMemoryRequirements Require;
	vkGetImageMemoryRequirements(rtg.device, image.handle, &Require);

	image.allocation = Allocate(Require, properties, map);

	VK( vkBindImageMemory(rtg.device, image.handle, image.allocation.handle, image.allocation.offset));

	return image;
}

void Helpers::destroy_image(AllocatedImage &&image) 
{
	vkDestroyImage(rtg.device, image.handle, nullptr);

	image.handle = VK_NULL_HANDLE;
	image.extent = VkExtent2D{.width = 0, .height = 0};
	image.format = VK_FORMAT_UNDEFINED;

	this->Free(std::move(image.allocation));
}

//----------------------------

void Helpers::transfer_to_buffer(void const *data, size_t size, AllocatedBuffer &target) 
{
	// NOTE: could let this stick around and use it for all uploads, but this function isn't for performant transfers anyway:
	AllocatedBuffer TransferSrc = create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		Mapped
	);
	
	// copy data to transfer buffer
	std::memcpy(TransferSrc.allocation.data(), data, size);

	// record CPU->GPU transfer to command buffer
	{
		VK( vkResetCommandBuffer(TransferCommandBuffer, 0));
		VkCommandBufferBeginInfo BeginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // will record again every submit
		};
		VK( vkBeginCommandBuffer(TransferCommandBuffer, &BeginInfo));

		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};
		vkCmdCopyBuffer(TransferCommandBuffer, TransferSrc.handle, target.handle, 1, &CopyRegion);

		VK( vkEndCommandBuffer(TransferCommandBuffer) );
	}

	// run command buffer
	{
		VkSubmitInfo SubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &TransferCommandBuffer
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &SubmitInfo, VK_NULL_HANDLE));
	}

	// wait for command buffer to finish
	VK(vkQueueWaitIdle(rtg.graphics_queue));

	//don't leak buffer memory:
	destroy_buffer(std::move(TransferSrc));
}

void Helpers::transfer_to_image(void const *data, size_t size, AllocatedImage &target) 
{
	assert(target.handle != VK_NULL_HANDLE);	// target iamgen should be allocated already

	// check data is the right size
	size_t BytesPerBlock = vkuFormatTexelBlockSize(target.format);
	size_t TexelsPerBlock = vkuFormatTexelsPerBlock(target.format);

	size_t ExpectedSize = 0;

	for (uint32_t mip = 0; mip < target.mipLevels; ++mip)
	{
		uint32_t Width = std::max(1u, target.extent.width >> mip);
		uint32_t Height = std::max(1u, target.extent.height >> mip);

		ExpectedSize += Width * Height * target.layers * BytesPerBlock / TexelsPerBlock;
	}
	assert(size == ExpectedSize);
	
	// create a host-coherent source buffer
	AllocatedBuffer TransferSrc = create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		Mapped
	);

	// copy image data into the source buffer
	std::memcpy(TransferSrc.allocation.data(), data, size);

	// begin recording a command buffer
	VK(vkResetCommandBuffer(TransferCommandBuffer, 0));

	VkCommandBufferBeginInfo BeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // will record again every submit
	};

	VK(vkBeginCommandBuffer(TransferCommandBuffer, &BeginInfo));
	
	VkImageSubresourceRange WholeImage
	{
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = target.mipLevels,
		.baseArrayLayer = 0,
		.layerCount = target.layers,
	};

	// put the receiving image in destination-optimal layout
	{
		VkImageMemoryBarrier Barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, 	// throw away old image
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.handle,
			.subresourceRange = WholeImage,
		};

		vkCmdPipelineBarrier
		(
			TransferCommandBuffer, 				// commandBuffer
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 	// srcStageMask
			VK_PIPELINE_STAGE_TRANSFER_BIT, 	// dstStageMask
			0, // dependencyFlags
			0, nullptr, // memory barrier count, pointer
			0, nullptr, // buffer memory barrier count, pointer
			1, &Barrier // image memory barrier count, pointer
		);
	}
	// copy the source buffer to the image
	{
		if(target.layers != 6)
		{
			std::vector<VkBufferImageCopy> Regions;
			size_t Offset = 0;
			for (uint32_t mip = 0; mip < target.mipLevels; ++mip)
			{
				uint32_t MipWidth  = std::max(1u, target.extent.width  >> mip);
				uint32_t MipHeight = std::max(1u, target.extent.height >> mip);

				size_t MipSize = MipWidth * MipHeight * BytesPerBlock / TexelsPerBlock;

				VkBufferImageCopy Region
				{
					.bufferOffset = Offset,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
					.imageSubresource
					{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = mip,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
					.imageOffset
					{
						.x = 0,
						.y = 0,
						.z = 0
					},
					.imageExtent
					{ 
						.width = MipWidth,
						.height = MipHeight,
						.depth = 1 
					},
				};

				Regions.push_back(Region);

				Offset += MipSize;
			}

			vkCmdCopyBufferToImage
			(
				TransferCommandBuffer,
				TransferSrc.handle,
				target.handle,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				(uint32_t)Regions.size(),
				Regions.data()
			);
		}
		else
		{
			const uint32_t FaceSize = target.extent.width;
			const size_t BytesPerTexel = BytesPerBlock / TexelsPerBlock;
			size_t Offset = 0;

			std::vector<VkBufferImageCopy> Regions;
			for (uint32_t mip = 0; mip < target.mipLevels; ++mip)
			{
				uint32_t MipSize = std::max(1u, FaceSize >> mip);
				size_t SingleFaceSize = MipSize * MipSize * BytesPerTexel;

				for (uint32_t face = 0; face < 6; ++face)
				{
					VkBufferImageCopy Region
					{
						.bufferOffset = Offset,
						.bufferRowLength = 0,
						.bufferImageHeight = 0,
						.imageSubresource
						{
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.mipLevel = mip,
							.baseArrayLayer = face,
							.layerCount = 1,
						},
						.imageExtent
						{ 
							.width = MipSize,
							.height = MipSize,
							.depth = 1 
						},
					};
					Regions.push_back(Region);

					Offset += SingleFaceSize;
				}
			}

			vkCmdCopyBufferToImage(
				TransferCommandBuffer,
				TransferSrc.handle,
				target.handle,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(Regions.size()),
				Regions.data()
			);
		}
	}

	// transition the image memory to shader-read-only-optimal layout
	{
		VkImageMemoryBarrier Barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.handle,
			.subresourceRange = WholeImage,
		};

		vkCmdPipelineBarrier
		(
			TransferCommandBuffer, 			// commandBuffer
			VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
			0, // dependencyFlags
			0, nullptr, // memory barrier count, pointer
			0, nullptr, // buffer memory barrier count, pointer
			1, &Barrier // image memory barrier count, pointer
		);
	}

	// end and submit the command buffer
	VK(vkEndCommandBuffer(TransferCommandBuffer));

	VkSubmitInfo SubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &TransferCommandBuffer
	};

	VK(vkQueueSubmit(rtg.graphics_queue, 1, &SubmitInfo, VK_NULL_HANDLE));

	// wait for command buffer to finish executing
	VK( vkQueueWaitIdle(rtg.graphics_queue));

	// destroy the source buffer
	destroy_buffer(std::move(TransferSrc));
}

void Helpers::GenerateMipmaps(AllocatedImage& Image)
{
    uint32_t MipLevels = Image.mipLevels;
    int32_t MipWidth = Image.extent.width;
    int32_t MipHeight = Image.extent.height;

    for (uint32_t i = 1; i < MipLevels; i++)
    {
        VkImageMemoryBarrier Barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        Barrier.image = Image.handle;
        Barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 6 };
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(TransferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);

        VkImageBlit Blit{};
        Blit.srcOffsets[0] = {0, 0, 0};
        Blit.srcOffsets[1] = {MipWidth, MipHeight, 1};
        Blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 6};
        Blit.dstOffsets[0] = {0, 0, 0};
        Blit.dstOffsets[1] = { MipWidth > 1 ? MipWidth / 2 : 1, MipHeight > 1 ? MipHeight / 2 : 1, 1 };
        Blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 6};

        vkCmdBlitImage(TransferCommandBuffer, Image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Blit, VK_FILTER_LINEAR);

        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(TransferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);

        if (MipWidth > 1) MipWidth /= 2;
        if (MipHeight > 1) MipHeight /= 2;
    }

    VkImageMemoryBarrier LastBarrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    LastBarrier.image = Image.handle;
    LastBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, MipLevels - 1, 1, 0, 6 };
    LastBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    LastBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    LastBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    LastBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(TransferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &LastBarrier);
}

//----------------------------

uint32_t Helpers::FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags flags) const
{
	for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; ++i) 
	{
		VkMemoryType const &type = MemoryProperties.memoryTypes[i];
		if ((TypeFilter & (1 << i)) != 0
		 && (type.propertyFlags & flags) == flags) 
		 {
			return i;
		}
	}
	throw std::runtime_error("No suitable memory type found.");
}

VkFormat Helpers::find_image_format(std::vector< VkFormat > const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const 
{
	for (VkFormat Format : candidates)
	{
		VkFormatProperties Props;
		vkGetPhysicalDeviceFormatProperties(rtg.physical_device, Format, &Props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (Props.linearTilingFeatures & features) == features) 
		{
			return Format;
		} 
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (Props.optimalTilingFeatures & features) == features) 
		{
			return Format;
		}
	}
	throw std::runtime_error("No supported format matches request.");
}

VkShaderModule Helpers::create_shader_module(uint32_t const *code, size_t bytes) const 
{
	VkShaderModule shader_module = VK_NULL_HANDLE;
	VkShaderModuleCreateInfo CreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = bytes,
		.pCode = code
	};
	VK( vkCreateShaderModule(rtg.device, &CreateInfo, nullptr, &shader_module));
	return shader_module;
}

//----------------------------

Helpers::Helpers(RTG const &rtg_) : rtg(rtg_) {
}

Helpers::~Helpers() {
}

void Helpers::create() 
{
	VkCommandPoolCreateInfo CreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = rtg.graphics_queue_family.value(),
	};
	VK( vkCreateCommandPool(rtg.device, &CreateInfo, nullptr, &TransferCommandPool) );

	VkCommandBufferAllocateInfo AllocInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = TransferCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK( vkAllocateCommandBuffers(rtg.device, &AllocInfo, &TransferCommandBuffer) );

	vkGetPhysicalDeviceMemoryProperties(rtg.physical_device, &MemoryProperties);

	if(rtg.configuration.debug)
	{
		std::cout << "Memory types:\n";
		for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; ++i) 
		{
			VkMemoryType const &Type = MemoryProperties.memoryTypes[i];
			std::cout << " [" << i << "] heap " << Type.heapIndex << ", flags: " << string_VkMemoryPropertyFlags(Type.propertyFlags) << '\n';
		}
		std::cout << "Memory heaps:\n";
		for (uint32_t i = 0; i < MemoryProperties.memoryHeapCount; ++i) 
		{
			VkMemoryHeap const &Heap = MemoryProperties.memoryHeaps[i];
			std::cout << " [" << i << "] " << Heap.size << " bytes, flags: " << string_VkMemoryHeapFlags( Heap.flags ) << '\n';
		}
		std::cout.flush();
	}
}

void Helpers::destroy() 
{
	// Technically not needed since freeing the pool will free all contained buffers:
	if(TransferCommandBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(rtg.device, TransferCommandPool, 1, &TransferCommandBuffer);
		TransferCommandBuffer = VK_NULL_HANDLE;
	}

	if(TransferCommandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(rtg.device, TransferCommandPool, nullptr);
		TransferCommandPool = VK_NULL_HANDLE;
	}

}
