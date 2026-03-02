#include "GPUFace.hpp"
#include "../../VK.hpp"
#include <iostream>

// for cubemap
void FGPUFace::Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FCubePipeline const &Pipeline, uint32_t const Width, uint32_t const Height, vec3 * const Data, bool isOutput) 
{
    assert(Width * 6 == Height);
	uint32_t MipLevels = isOutput ? 1 : static_cast<uint32_t>(std::floor(std::log2(Width))) + 1;
	std::vector< vec4 > DataPadded;
	DataPadded.reserve(Width * Height);
	for (uint32_t i = 0; i < Width * Height; ++i) 
	{
		DataPadded.emplace_back(vec4(Data[i], 0.0f));
	}

	// create image:
    uint32_t FaceSize = Width;
	uint32_t Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if(isOutput)
	{
		Usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	else
	{
		Usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	Image = CubeExe.helpers.create_image
	(
		VkExtent2D{ .width = FaceSize, .height = FaceSize },
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		Usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		CubeHelpers::Unmapped,
        6,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		MipLevels
	);

	//actually upload the image:
	CubeExe.helpers.transfer_to_image(DataPadded.data(), sizeof(DataPadded[0]) * DataPadded.size(), Image, VK_IMAGE_LAYOUT_GENERAL);

	// image view:
	{
		VkImageViewCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.flags = 0,
			.image = Image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.format = Image.format,
			// .components sets swizzling and is fine when zero-initialized
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = MipLevels,
				.baseArrayLayer = 0,
				.layerCount = 6,
			},
		};
		VK( vkCreateImageView(CubeExe.device, &CreateInfo, nullptr, &View) );
	}

	// Texture sampler
	VkSamplerCreateInfo SamplerInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.minLod = 0.0f,
		.maxLod = isOutput ? 1.0f : float(MipLevels),
	};
	vkCreateSampler(CubeExe.device, &SamplerInfo, nullptr, &Sampler);

	{ //descriptor set with world_from_px and storage image:
		{ //allocate:
			VkDescriptorSetAllocateInfo AllocInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = isOutput  ? &Pipeline.Set1_OutFace : &Pipeline.Set0_InFace,
			};
			VK( vkAllocateDescriptorSets(CubeExe.device, &AllocInfo, &Descriptors) );
		}

		//write:
		VkDescriptorBufferInfo BufferInfo
		{
			.buffer = Buffer.handle,
			.offset = 0,
			.range = Buffer.size,
		};
		VkDescriptorImageInfo ImageInfo
		{
			.sampler = isOutput ? VK_NULL_HANDLE : Sampler,
			.imageView = View,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};

		std::array< VkWriteDescriptorSet, 1 > Writes
		{
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = Descriptors,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = isOutput ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &ImageInfo,
			},
		};

		vkUpdateDescriptorSets(CubeExe.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
	}
}

// for image2D(brdf LUT)
void FGPUFace::Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FLUTPipeline const &Pipeline, uint32_t const ImageSize, vec2 * const Data)
{
	std::vector<uint32_t> DataPadded;
	DataPadded.reserve(ImageSize * ImageSize);
	for (uint32_t i = 0; i < ImageSize * ImageSize; ++i) 
	{
		vec2 v = Data[i];
		DataPadded.emplace_back(glm::packHalf2x16(v));
	}

	// create image:
	uint32_t Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	Image = CubeExe.helpers.create_image
	(
		VkExtent2D{ .width = ImageSize, .height = ImageSize },
		VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		Usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		CubeHelpers::Unmapped,
        1
	);

	//actually upload the image:
	CubeExe.helpers.transfer_to_image(DataPadded.data(), sizeof(uint32_t) * DataPadded.size(), Image, VK_IMAGE_LAYOUT_GENERAL);

	// image view:
	{
		VkImageViewCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.flags = 0,
			.image = Image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.format = Image.format,
			// .components sets swizzling and is fine when zero-initialized
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VK( vkCreateImageView(CubeExe.device, &CreateInfo, nullptr, &View) );
	}

	{ //descriptor set with world_from_px and storage image:
		{ //allocate:
			VkDescriptorSetAllocateInfo AllocInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &Pipeline.Set0_OutFace,
			};
			VK( vkAllocateDescriptorSets(CubeExe.device, &AllocInfo, &Descriptors) );
		}

		//write:
		VkDescriptorBufferInfo BufferInfo
		{
			.buffer = Buffer.handle,
			.offset = 0,
			.range = Buffer.size,
		};
		VkDescriptorImageInfo ImageInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = View,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};

		std::array< VkWriteDescriptorSet, 1 > Writes
		{
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = Descriptors,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &ImageInfo,
			},
		};

		vkUpdateDescriptorSets(CubeExe.device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
	}
}

void FGPUFace::Destroy(CubeExecute &CubeExe) 
{
	//well c'mon we just quit the process anyway the driver can take care of it for us right?
	vkDestroyImageView(CubeExe.device, View, nullptr);
	View = VK_NULL_HANDLE;
	CubeExe.helpers.destroy_buffer(std::move(Buffer));
	CubeExe.helpers.destroy_image(std::move(Image));
}