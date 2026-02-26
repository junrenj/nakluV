#include "GPUFace.hpp"
#include "../../VK.hpp"

void FGPUFace::Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FCubePipeline const &Pipeline, uint32_t const Width, uint32_t const Height, vec3 * const Data) 
{
    assert(Width * 6 == Height);

	std::vector< vec4 > DataPadded;
	DataPadded.reserve(Width * Height);
	for (uint32_t i = 0; i < Width * Height; ++i) 
	{
		DataPadded.emplace_back(vec4(Data[i], 0.0f));
	}

	//create image:
    uint32_t FaceSize = Width;
	Image = CubeExe.helpers.create_image
	(
		VkExtent2D{ .width = FaceSize, .height = FaceSize },
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		CubeHelpers::Unmapped,
        6,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	);

	//actually upload the image:
	CubeExe.helpers.transfer_to_image(DataPadded.data(), sizeof(DataPadded[0]) * DataPadded.size(), Image, VK_IMAGE_LAYOUT_GENERAL);

	//---- buffer ----
	{
		Buffer = CubeExe.helpers.create_buffer
		(
			sizeof(FCubePipeline::FFace),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			CubeHelpers::Unmapped
		);

		FCubePipeline::FFace FaceInfo{};

		vec3 s = vec3(0.0f, 0.0f, -1.0f);
		vec3 t = vec3(0.0f, -1.0f, -0.0f);
		vec3 Center = vec3(1.0f, 0.0f, 0.0f);

		FaceInfo.WORLD_FROM_PX.m0 = 2.0f * s.x / float(Width);
		FaceInfo.WORLD_FROM_PX.m1 = 2.0f * s.y / float(Width);
		FaceInfo.WORLD_FROM_PX.m2 = 2.0f * s.z / float(Width);

		FaceInfo.WORLD_FROM_PX.m3 = 2.0f * t.x / float(Width);
		FaceInfo.WORLD_FROM_PX.m4 = 2.0f * t.y / float(Width);
		FaceInfo.WORLD_FROM_PX.m5 = 2.0f * t.z / float(Width);

		float Corner = 1.0f - 2.0f / float(Width) * 0.5f;
		FaceInfo.WORLD_FROM_PX.m6 = Center.x - Corner * s.x - Corner * t.x;
		FaceInfo.WORLD_FROM_PX.m7 = Center.y - Corner * s.y - Corner * t.y;
		FaceInfo.WORLD_FROM_PX.m8 = Center.z - Corner * s.z - Corner * t.z;

		CubeExe.helpers.transfer_to_buffer( &FaceInfo, sizeof(FaceInfo), Buffer );
	}

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
				.layerCount = 6,
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
				.pSetLayouts = &Pipeline.Set1_Face,
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
			.imageView = View,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		std::array< VkWriteDescriptorSet, 2 > Writes
		{
			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = Descriptors,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &BufferInfo,
			},

			VkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = Descriptors,
				.dstBinding = 1,
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