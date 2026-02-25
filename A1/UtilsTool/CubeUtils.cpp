#include "CubeUtils.hpp"
#include "../../VK.hpp"
#include <array>

UCubeUtils::UCubeUtils(VulkanContext* InContext, const char* Path, uint32_t InSize, uint32_t OutSize)
{
    VkDevice Device = InContext->Device;
    VkImage CubemapImage = this->CreateCubemapImage(Device, InSize, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    VkImage IrradianceImage = this->CreateCubemapImage(Device, OutSize, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    VkImageView EnvCubeView = this->CreateCubeView(Device, CubemapImage, VK_FORMAT_R32G32B32A32_SFLOAT);
    VkImageView IrradianceView = this->CreateCubeView(Device, IrradianceImage, VK_FORMAT_R32G32B32A32_SFLOAT);

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
		VK( vkCreateSampler(InContext->Device, &CreateInfo, nullptr, &TextureSampler) );
	}

    // Descriptor Set
    {
        VkDescriptorImageInfo EnvInfo{};
        EnvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        EnvInfo.imageView = EnvCubeView;
        EnvInfo.sampler = TextureSampler;

        VkDescriptorImageInfo OutInfo{};
        OutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        OutInfo.imageView = IrradianceView;

        std::array<VkWriteDescriptorSet,2> Writes
        {
            VkWriteDescriptorSet
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = EnvDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &EnvInfo,
            },
            VkWriteDescriptorSet
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = IrradianceDescriptorSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &OutInfo,
            },
        };

        vkUpdateDescriptorSets(Device, uint32_t(Writes.size()), Writes.data(), 0, nullptr);
    }

    {
        VkPipelineLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        LayoutInfo.setLayoutCount = 1;
        LayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout PipelineLayout;
        vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout);

        VkComputePipelineCreateInfo PipelineInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = CreateShaderStage("irradiance.comp.spv"),
            .layout = PipelineLayout,
        };

        VkPipeline Pipeline;
        vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline);
    }

    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDispatch(cmd,
            (32 + 7) / 8,
            (32 + 7) / 8,
            6);
    }

    {
        VkImageMemoryBarrier Barrier
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = irradianceImage,
            .subresourceRange
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 6,
            },
        };
        vkCmdPipelineBarrier
        (
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,nullptr,
            0,nullptr,
            1,&Barrier
        );
    }
}

VkImage UCubeUtils::CreateCubemapImage( VkDevice Device, uint32_t Size, VkFormat Format, VkImageUsageFlags Usage)
{
    VkImageCreateInfo Info
    { 
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = Format,
        .extent = { Size, Size, 1 },
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = Usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    

    VkImage Image;
    vkCreateImage(Device, &Info, nullptr, &Image);
    return Image;
}

VkImageView UCubeUtils::CreateCubeView(VkDevice Device, VkImage Image, VkFormat Format)
{
    VkImageViewCreateInfo View
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = Format,
        .image = Image,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 6,
        },
    };
    

    VkImageView ImageView;
    vkCreateImageView(Device, &View, nullptr, &ImageView);
    return ImageView;
}

VkDescriptorSetLayout UCubeUtils::CreateComputeLayout(VkDevice Device)
{
    std::array<VkDescriptorSetLayoutBinding, 2> Bindings
    {
        VkDescriptorSetLayoutBinding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        VkDescriptorSetLayoutBinding
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
    };

    VkDescriptorSetLayoutCreateInfo Info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = Bindings.size(),
        .pBindings = Bindings.data(),
    };
    

    VkDescriptorSetLayout Layout;
    vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Layout);
    return Layout;
}

