#include "CubeExecute.hpp"
#include "CubePipeline.hpp"
#include "GPUFace.hpp"
#include "../../VK.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    //main wrapped in a try-catch so we can print some debug info about uncaught exceptions:
	try 
	{
		//configure application:
		CubeExecute::Configuration configuration;

		configuration.application_info = VkApplicationInfo
		{
			.pApplicationName = "Image Processor",
			.applicationVersion = VK_MAKE_VERSION(0,0,0),
			.pEngineName = "Unknown",
			.engineVersion = VK_MAKE_VERSION(0,0,0),
			.apiVersion = VK_API_VERSION_1_3
		};

		bool print_usage = false;

		try 
		{
			configuration.parse(argc, argv);
		} 
		catch (std::runtime_error &e) 
		{
			std::cerr << "Failed to parse arguments:\n" << e.what() << std::endl;
			print_usage = true;
		}

        configuration.headless = true;

		if (print_usage) 
		{
			std::cerr << "Usage:" << std::endl;
			CubeExecute::Configuration::usage( [](const char *arg, const char *desc)
			{ 
				std::cerr << "    " << arg << "\n        " << desc << std::endl;
			});
			return 1;
		}

		//loads vulkan library, load device, initializes helpers:
		CubeExecute CubeExe(configuration);

		// Initialize Pipeline & Process Logic
		FCubePipeline Pipeline;
		Pipeline.Create(CubeExe);

		VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
		// create descriptor pool
		{
			std::array< VkDescriptorPoolSize, 2> PoolSizes
			{
				VkDescriptorPoolSize
				{
					.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 6*2 + 1, // one for each input and output cube face, plus one for params
				},
				VkDescriptorPoolSize
				{
					.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.descriptorCount = 6*2, // one for each input and output cube face
				},
			};

			VkDescriptorPoolCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
				.maxSets = 12 + 1, //one set per in/out cube face, plus one for params
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

		size_t Size = 128;
		std::vector< vec3 > Data (Size * Size, vec3(1.0f, 1.0f, 1.0f));
		FGPUFace InFace;
		InFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)Size, Data.data());
		FGPUFace OutFace;
		OutFace.Create(CubeExe, DescriptorPool, Pipeline, (uint32_t)Size, Data.data());

		// run pipeline
		{ 
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
					CommandBuffer, // command buffer
					VK_PIPELINE_BIND_POINT_COMPUTE, // pipeline bind point
					Pipeline.Layout, // pipeline layout
					0, //first set
					uint32_t(DescriptorSets.size()), DescriptorSets.data(), //descriptor sets count, ptr
					0, nullptr //dynamic offsets count, ptr
				);
			}

	
			// actually run the thing:
			vkCmdDispatchBase(CommandBuffer, 0, 0, 1, (uint32_t)Size, (uint32_t)Size, 1);

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
		}
		std::cout << "Computing: done." << std::endl;
		
	} 
	catch (std::exception &e) 
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
    return 0;
}