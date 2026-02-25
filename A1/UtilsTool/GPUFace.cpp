#include "GPUFace.hpp"
#include "../../VK.hpp"

void FGPUFace::Create(CubeExecute &CubeExe, VkDescriptorPool DescriptorPool, FCubePipeline const &Pipeline, uint32_t const Size, vec3 * const Data) 
{
		
		std::vector< vec4 > DataPadded;
		DataPadded.reserve(Size * Size);
		for (uint32_t i = 0; i < Size * Size; ++i) 
        {
			DataPadded.emplace_back(vec4(Data[i], 0.0f));
		}

		//create image:
		Image = CubeExe.helpers.create_image
        (
			VkExtent2D{ .width = Size, .height = Size },
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			CubeHelpers::Unmapped
		);

		//actually upload the image:
		CubeExe.helpers.transfer_to_image(DataPadded.data(), sizeof(DataPadded[0]) * Size * Size, Image, VK_IMAGE_LAYOUT_GENERAL);

		//---- buffer ----
		{
			Buffer = CubeExe.helpers.create_buffer
            (
				sizeof(FCubePipeline::FFace),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				CubeHelpers::Unmapped
			);

			FCubePipeline::FFace face_info{};

			vec3 s = vec3(0.0f, 0.0f, -1.0f);
			vec3 t = vec3(0.0f,-1.0f, -0.0f);
			vec3 center = vec3(1.0f, 0.0f, 0.0f);

			face_info.WORLD_FROM_PX.m0 = 2.0f * s.x / float(Size);
			face_info.WORLD_FROM_PX.m1 = 2.0f * s.y / float(Size);
			face_info.WORLD_FROM_PX.m2 = 2.0f * s.z / float(Size);

			face_info.WORLD_FROM_PX.m3 = 2.0f * t.x / float(Size);
			face_info.WORLD_FROM_PX.m4 = 2.0f * t.y / float(Size);
			face_info.WORLD_FROM_PX.m5 = 2.0f * t.z / float(Size);

			float corner = 1.0f - 2.0f / float(Size) * 0.5f;
			face_info.WORLD_FROM_PX.m6 = center.x - corner * s.x - corner * t.x;
			face_info.WORLD_FROM_PX.m7 = center.y - corner * s.y - corner * t.y;
			face_info.WORLD_FROM_PX.m8 = center.z - corner * s.z - corner * t.z;

			CubeExe.helpers.transfer_to_buffer( &face_info, sizeof(face_info), Buffer );
		}

        // image view:
		{
			VkImageViewCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = Image.handle,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
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
			VK( vkCreateImageView(CubeExe.device, &create_info, nullptr, &View) );
		}

		{ //descriptor set with world_from_px and storage image:
			{ //allocate:
				VkDescriptorSetAllocateInfo alloc_info{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					.descriptorPool = DescriptorPool,
					.descriptorSetCount = 1,
					.pSetLayouts = &Pipeline.Set1_Face,
				};
				VK( vkAllocateDescriptorSets(CubeExe.device, &alloc_info, &Descriptors) );
			}

			//write:
			VkDescriptorBufferInfo buffer_info{
				.buffer = Buffer.handle,
				.offset = 0,
				.range = Buffer.size,
			};
			VkDescriptorImageInfo image_info{
				.imageView = View,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			};
			std::array< VkWriteDescriptorSet, 2 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = Descriptors,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &buffer_info,
				},

				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = Descriptors,
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.pImageInfo = &image_info,
				},
			};

			vkUpdateDescriptorSets(CubeExe.device,
				uint32_t(writes.size()), writes.data(),
				0, nullptr
			);
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