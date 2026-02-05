#include "RTG.hpp"

#include "VK.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <vulkan/utility/vk_format_utils.h> //for getting format sizes
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>

void RTG::Configuration::parse(int argc, char **argv) 
{
	for (int argi = 1; argi < argc; ++argi) {
		std::string arg = argv[argi];
		if (arg == "--debug") {
			debug = true;
		} else if (arg == "--no-debug") {
			debug = false;
		} else if (arg == "--physical-device") {
			if (argi + 1 >= argc) throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		} else if (arg == "--drawing-size") {
			if (argi + 2 >= argc) throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what) {
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i) {
					if (val[i] < '0' || val[i] > '9') {
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		}
		else if (arg == "--headless") 
		{
			headless = true; 
		}
		else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
	callback("--headless", "Don't create a window; read events from stdin.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type,
	const VkDebugUtilsMessengerCallbackDataEXT *Data,
	void *UserData
) 
{
	if (Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) 
	{
		std::cerr << "\x1b[91m" << "E: ";
	} 
	else if (Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
	{
		std::cerr << "\x1b[33m" << "w: ";
	} 
	else if (Severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) 
	{
		std::cerr << "\x1b[90m" << "i: ";
	} 
	else 
	{ 
		//VERBOSE
		std::cerr << "\x1b[90m" << "v: ";
	}
	std::cerr << Data->pMessage << "\x1b[0m" << std::endl;

	return VK_FALSE;
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	configuration = configuration_;

	//fill in flags/extensions/layers information:

	// create the `instance` (main handle to Vulkan library):
	{
		VkInstanceCreateFlags InstanceFlags = 0;
		std::vector< const char * > InstanceExtensions;
		std::vector< const char * >InstanceLayers;

		// add extensions for MoltenVK portability layer on macOS
		#if defined(__APPLE__)
		InstanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		InstanceExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		InstanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		InstanceExtensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		#endif
		
		// add extensions and layers for debugging:
		if(configuration.debug)
		{
			InstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			InstanceLayers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		if(!configuration.headless)
		{
			// add extensions needed by glfw
			{
				glfwInit();
				if(!glfwVulkanSupported())
				{
					throw std::runtime_error("GLFW reports Vulkan is not supported");
				}

				uint32_t Count;
				const char **Extensions = glfwGetRequiredInstanceExtensions(&Count);
				if(Extensions == nullptr)
				{
					throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
				}
				for (uint32_t i = 0; i < Count; i++)
				{
					InstanceExtensions.emplace_back(Extensions[i]);
				}
				
			}
		}

		// write debug messenger structure
		VkDebugUtilsMessengerCreateInfoEXT DebugMessengerCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debug_callback,
			.pUserData = nullptr
		};

		VkInstanceCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = configuration.debug ? &DebugMessengerCreateInfo : nullptr,
			.flags = InstanceFlags,
			.pApplicationInfo = &configuration.application_info,
			.enabledLayerCount = uint32_t(InstanceLayers.size()),
			.ppEnabledLayerNames = InstanceLayers.data(),
			.enabledExtensionCount = uint32_t(InstanceExtensions.size()),
			.ppEnabledExtensionNames = InstanceExtensions.data()
		};
		VK( vkCreateInstance(&CreateInfo, nullptr, &instance));

		// Create debug messenger
		if(configuration.debug)
		{
			PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (!vkCreateDebugUtilsMessengerEXT) 
			{
				throw std::runtime_error("Failed to lookup debug utils create fn.");
			}
			VK( vkCreateDebugUtilsMessengerEXT(instance, &DebugMessengerCreateInfo, nullptr, &debug_messenger) );
		}
	}
	

	if(!configuration.headless)
	{
		// create the `window` and `surface` (where things get drawn):
		{
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			window = glfwCreateWindow(configuration.surface_extent.width, configuration.surface_extent.height, configuration.application_info.pApplicationName, nullptr, nullptr);

			if(!window)
			{
				throw std::runtime_error("GLFW failed to create a window.");
			}

			VK(glfwCreateWindowSurface(instance, window, nullptr, &surface) );
		}
	}
	

	// select the `physical_device` -- the gpu that will be used to draw:
	{
		std::vector< std::string > PhysicalDeviceNames;
		// Pick a physical device
		{
			uint32_t Count = 0;
			VK( vkEnumeratePhysicalDevices(instance, &Count, nullptr));
			std::vector< VkPhysicalDevice > PhysicalDevices(Count);
			VK( vkEnumeratePhysicalDevices(instance, &Count, PhysicalDevices.data()));

			uint32_t BestScore = 0;

			for (auto const &Pd : PhysicalDevices)
			{
				VkPhysicalDeviceProperties Properties;
				vkGetPhysicalDeviceProperties(Pd, &Properties);

				VkPhysicalDeviceFeatures Features;
				vkGetPhysicalDeviceFeatures(Pd, &Features);

				PhysicalDeviceNames.emplace_back(Properties.deviceName);

				if(!configuration.physical_device_name.empty())
				{
					if(configuration.physical_device_name == Properties.deviceName)
					{
						if(physical_device)
						{
							std::cerr << "WARNING: have two physical devices with the name '" << Properties.deviceName << "'; using the first to be enumerated." << std::endl;
						}
						else
						{
							physical_device = Pd;
						}
					}
				}
				else
				{
					uint32_t Score = 1;
					if(Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					{
						Score += 0x8000;
					}

					if(Score > BestScore)
					{
						BestScore = Score;
						physical_device = Pd;
					}
				}
			}
			
		}

		if(physical_device == VK_NULL_HANDLE)
		{
			std::cerr << "Physical devices:\n";
			for (std::string const &Name : PhysicalDeviceNames) 
			{
				std::cerr << "    " << Name << "\n";
			}
			std::cerr.flush();

			if (!configuration.physical_device_name.empty()) 
			{
				throw std::runtime_error("No physical device with name '" + configuration.physical_device_name + "'.");
			} 
			else 
			{
				throw std::runtime_error("No suitable GPU found.");
			}
		}

		// report device name:
		{
			VkPhysicalDeviceProperties Properties;
			vkGetPhysicalDeviceProperties(physical_device, &Properties);
			std::cout << "Selected physical device '" << Properties.deviceName << "'." << std::endl;
		}
	}

	// select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
	if(configuration.headless)
	{
		//in headless mode, just use the first requested format:
		if (configuration.surface_formats.empty()) 
		{
			throw std::runtime_error("No surface formats requested.");
		}
		surface_format = configuration.surface_formats[0];

		//headless mode will always use VK_PRESENT_MODE_FIFO_KHR, so make sure that's an option:
		bool HaveFifo = false;
		for (auto const &Mode : configuration.present_modes) 
		{
			if (Mode == VK_PRESENT_MODE_FIFO_KHR) 
			{
				HaveFifo = true;
				break;
			}
		}
		if (!HaveFifo) 
		{
			throw std::runtime_error("Configured present modes do not contain VK_PRESENT_MODE_FIFO_KHR.");
		}
		present_mode = VK_PRESENT_MODE_FIFO_KHR;

		present_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else
	{
		std::vector< VkSurfaceFormatKHR > Formats;
		std::vector< VkPresentModeKHR > PresentModes;

		{
			uint32_t Count = 0;
			VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &Count, nullptr));
			Formats.resize(Count);
			VK(  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &Count, Formats.data()));
		}

		{
			uint32_t Count = 0;
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &Count, nullptr) );
			PresentModes.resize(Count);
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &Count, PresentModes.data()) );
		}

		
		// std::cout << "Supproted Surface Formats (" << Formats.size() << "):\n";

		// for (size_t i = 0; i < Formats.size(); ++i)
		// {
		// 	const auto& f = Formats[i];
		// 	std::cout
		// 		<< "  [" << i << "] "
		// 		<< "format = " << string_VkFormat(f.format)
		// 		<< ", colorSpace = " << string_VkColorSpaceKHR(f.colorSpace)
		// 		<< '\n';
		// }

		// std::cout << "Supproted Present Modes (" << PresentModes.size() << "):\n";

		// for (size_t i = 0; i < PresentModes.size(); ++i)
		// {
		// 	std::cout
		// 		<< "  [" << i << "] "
		// 		<< string_VkPresentModeKHR(PresentModes[i])
		// 		<< '\n';
		// }

		// find first available surface format matching config:
		surface_format = [&]()
		{
			for (auto const &ConfigFormat : configuration.surface_formats) 
			{
				for (auto const &Format : Formats) 
				{
					if (ConfigFormat.format == Format.format && ConfigFormat.colorSpace == Format.colorSpace) 
					{
						return Format;
					}
				}
			}
			throw std::runtime_error("No format matching requested format(s) found.");
		}();

		// find first available present mode matching config:
		present_mode = [&](){
			for (auto const &ConfigMode : configuration.present_modes) 
			{
				for (auto const &Mode : PresentModes) 
				{
					if (ConfigMode == Mode) 
					{
						return Mode;
					}
				}
			}
			throw std::runtime_error("No present mode matching requested mode(s) found.");
		}();

		present_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	// create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
	{
		// look up queue indices:
		{
			uint32_t Count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &Count, nullptr);
			std::vector< VkQueueFamilyProperties > QueueFamilies(Count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &Count, QueueFamilies.data());

			for (auto const &QueueFamily : QueueFamilies)
			{
				uint32_t i = uint32_t(&QueueFamily - &QueueFamilies[0]);

				// if it does graphics, set the graphics queue family:
				if(QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					if(!graphics_queue_family)
					{
						graphics_queue_family = i;
					}
				}

				// if it has present support, set the present queue family:
				if(!configuration.headless)
				{
					VkBool32 PresentSupport = VK_FALSE;
					VK( vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, & PresentSupport));
					if(PresentSupport == VK_TRUE)
					{
						if(!present_queue_family)
						{
							present_queue_family = i;
						}
					}
				}
			}

			// in headless mode, "present" (copy-to-host) on the graphics queue:
			if (configuration.headless) 
			{
				present_queue_family = graphics_queue_family;
			}
			if(!graphics_queue_family)
			{
				throw std::runtime_error("No queue with graphics support.");
			}
			if(!present_queue_family)
			{
				throw std::runtime_error("No queue with present support.");
			}
		}

		// select device extensions:
		std::vector< const char * > DeviceExtensions;
		#if defined(__APPLE__)
		DeviceExtensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		#endif
		// Add the swapchain extension:
		if (!configuration.headless) 
		{
			DeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		// create the logical device:
		{
			std::vector< VkDeviceQueueCreateInfo > QueueCreateInfos;
			std::set< uint32_t > UniqueQueueFamilies
			{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			float QueueProperties[1] = {1.0f};
			for (uint32_t QueueFamily : UniqueQueueFamilies) 
			{
				QueueCreateInfos.emplace_back(VkDeviceQueueCreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = QueueFamily,
					.queueCount = 1,
					.pQueuePriorities = QueueProperties,
				});
			}

			VkDeviceCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = uint32_t(QueueCreateInfos.size()),
				.pQueueCreateInfos = QueueCreateInfos.data(),

				// device layers are depreciated; spec suggests passing instance_layers or nullptr:
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,

				.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size()),
				.ppEnabledExtensionNames = DeviceExtensions.data(),

				// pass a pointer to a VkPhysicalDeviceFeatures to request specific features: (e.g., thick lines)
				.pEnabledFeatures = nullptr,
			};

			VK( vkCreateDevice(physical_device, &CreateInfo, nullptr, &device));

			vkGetDeviceQueue(device, graphics_queue_family.value(), 0, &graphics_queue);
			vkGetDeviceQueue(device, present_queue_family.value(), 0, &present_queue);
		}
	}

	//run any resource creation required by Helpers structure:
	helpers.create();

	//create initial swapchain:
	recreate_swapchain();

	//create workspace resources:
	workspaces.resize(configuration.workspaces);
	for (auto &workspace : workspaces) 
	{
		// Create workspace fences:
		{
			VkFenceCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT,	// start signaled, because all workspaces are available to start
			};

			VK( vkCreateFence(device, &CreateInfo, nullptr, &workspace.workspace_available));
		}
		
		// Create workspace semaphores:
		{
			VkSemaphoreCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VK( vkCreateSemaphore(device, &CreateInfo, nullptr, &workspace.image_available));
		}
	}


}

RTG::~RTG() {
	//don't destroy until device is idle:
	if (device != VK_NULL_HANDLE) {
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	//destroy workspace resources:
	for (auto &workspace : workspaces) 
	{
		if (workspace.workspace_available != VK_NULL_HANDLE) 
		{
			vkDestroyFence(device, workspace.workspace_available, nullptr);
			workspace.workspace_available = VK_NULL_HANDLE;
		}
		if (workspace.image_available != VK_NULL_HANDLE) 
		{
			vkDestroySemaphore(device, workspace.image_available, nullptr);
			workspace.image_available = VK_NULL_HANDLE;
		}
	}
	workspaces.clear();

	// destroy the swapchain:
	destroy_swapchain();

	// destroy Helpers structure resources:
	helpers.destroy();

	if(device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if(surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if(window != nullptr)
	{
		glfwDestroyWindow(window);
		window = nullptr;
	}

	if(debug_messenger != VK_NULL_HANDLE)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if(vkDestroyDebugUtilsMessengerEXT)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			debug_messenger = VK_NULL_HANDLE;
		}
	}

	if(instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}

}

void RTG::recreate_swapchain() 
{
	// clean up swapchain if it already exists
	if(!swapchain_images.empty())
	{
		destroy_swapchain();
	}

	if(configuration.headless)
	{
		assert(surface == VK_NULL_HANDLE);	// headless, so must not have a surface

		//make a fake swapchain:

		// set extent from configuration
		swapchain_extent = configuration.surface_extent;

		// set number of images to 3
		uint32_t RequestedCount = 3;	// enough for FIFO-style presentation

		// create command pool for the headless image copy command buffers:
		{
			assert(HeadlessCommandPool == VK_NULL_HANDLE);
			VkCommandPoolCreateInfo CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = 0,
				.queueFamilyIndex = graphics_queue_family.value(),
			};
			VK( vkCreateCommandPool(device, &CreateInfo, nullptr, &HeadlessCommandPool));
		}

		// create headless_swapchain
		assert(headlessSwapchain.empty());

		headlessSwapchain.reserve(RequestedCount);
		for (uint32_t i = 0; i < RequestedCount; i++)
		{
			// add a headless "swapchain" image:
			HeadlessSwapchainImage &h =  headlessSwapchain.emplace_back();

			// allocate image data: (on-GPU, will be rendered to)
			h.Image = helpers.create_image
			(
				swapchain_extent,
				surface_format.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);

			// allocate buffer data: (on-CPU, will be copied to)
			h.Buffer = helpers.create_buffer
			(
				swapchain_extent.width * swapchain_extent.height * 
				vkuFormatTexelBlockSize(surface_format.format) / vkuFormatTexelsPerBlock(surface_format.format),
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped
			);

			// Create and record copy command:
			{
				VkCommandBufferAllocateInfo AllocInfo
				{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = HeadlessCommandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				VK( vkAllocateCommandBuffers(device, &AllocInfo, &h.CopyCommand));

				// Record:
				VkCommandBufferBeginInfo BeginInfo
				{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = 0,	
				};
				VK( vkBeginCommandBuffer(h.CopyCommand, &BeginInfo));

				VkBufferImageCopy Region
				{
					.bufferOffset = 0,
					.bufferRowLength = swapchain_extent.width,
					.bufferImageHeight = swapchain_extent.height,
					.imageSubresource
					{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
					.imageOffset{ .x = 0, .y = 0, .z = 0},
					.imageExtent
					{
						.width = swapchain_extent.width,
						.height = swapchain_extent.height,
						.depth = 1
					},
				};
				vkCmdCopyImageToBuffer(h.CopyCommand, h.Image.handle, 
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, h.Buffer.handle, 1, &Region);
				
					VK( vkEndCommandBuffer(h.CopyCommand));
			}

			// create fence to signal when image is done being "presented" (copied to host memory):
			{
				VkFenceCreateInfo CreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT,	// start signaled, because all images are available to start with
				};
				VK( vkCreateFence(device, &CreateInfo, nullptr, &h.ImagePresented));
			}
		}
		
		// copy image references into swapchain_images:
		assert(swapchain_images.empty());
		swapchain_images.assign(RequestedCount, VK_NULL_HANDLE);
		for (uint32_t i = 0; i < RequestedCount; i++)
		{
			swapchain_images[i] = headlessSwapchain[i].Image.handle;
		}
	}
	else
	{
		assert(surface != VK_NULL_HANDLE); // not headless, so must have a surface

		// request a swapchain from the windowing system:

		// determine size, image count, and transform for swapchain
		VkSurfaceCapabilitiesKHR Capabilities;
		VK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &Capabilities));

		swapchain_extent = Capabilities.currentExtent;

		uint32_t RequestCount = Capabilities.minImageCount + 1;
		if(Capabilities.maxImageCount != 0)
		{
			RequestCount = std::min(Capabilities.maxImageCount, RequestCount);
		}

		// create the swapchain
		{
			VkSwapchainCreateInfoKHR CreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = surface,
				.minImageCount = RequestCount,
				.imageFormat = surface_format.format,
				.imageColorSpace = surface_format.colorSpace,
				.imageExtent = swapchain_extent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.preTransform = Capabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = present_mode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE	//NOTE: could be more efficient by passing old swapchain handle here instead of destroying it
			};

			std::vector< uint32_t > QueueFamilyIndices
			{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			if(QueueFamilyIndices[0] != QueueFamilyIndices[1])
			{
				// if images will be presented on a different queue, make sure they are shared:
				CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				CreateInfo.queueFamilyIndexCount = uint32_t(QueueFamilyIndices.size());
				CreateInfo.pQueueFamilyIndices = QueueFamilyIndices.data();
			}
			else
			{
				CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			}

			VK( vkCreateSwapchainKHR(device, &CreateInfo, nullptr, &swapchain));
		}

		// get the swapchain images
		{
			uint32_t Count = 0;
			VK( vkGetSwapchainImagesKHR(device, swapchain, &Count, nullptr));
			swapchain_images.resize(Count);
			VK( vkGetSwapchainImagesKHR(device, swapchain, &Count, swapchain_images.data()));
		}
	}

	// create views for swapchain images:
	swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain_images.size(); i++)
	{
		VkImageViewCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surface_format.format,
			.components
			{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(device, &CreateInfo, nullptr, &swapchain_image_views[i]));
	}

	// create semaphores for waiting on each image to be done:
	{
		VkSemaphoreCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		swapchain_image_dones.assign(swapchain_images.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < swapchain_images.size(); i++)
		{
			VK( vkCreateSemaphore(device, &CreateInfo, nullptr, &swapchain_image_dones[i]));
		}
	}
	if (configuration.debug) 
	{
		std::cout << "Swapchain is now " << swapchain_images.size() << " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}


void RTG::destroy_swapchain() 
{
	VK( vkDeviceWaitIdle(device));	// wait for any rendering to old swapchain to finish

	// Clean up Semaphores used for waiting on the swapchain
	for (auto & Semaphore : swapchain_image_dones)
	{
		vkDestroySemaphore(device, Semaphore, nullptr);
		Semaphore = VK_NULL_HANDLE;
	}
	swapchain_image_dones.clear();

	// clean up image views referencing the swapchain:
	for (auto &ImageView : swapchain_image_views)
	{
		vkDestroyImageView(device, ImageView, nullptr);
		ImageView = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	// forget handles to swapchain images (will destroy by deallocating the swapchain itself):
	swapchain_images.clear();

	if(configuration.headless)
	{
		// destroy headless_swapchain
		for( auto &h : headlessSwapchain)
		{
			helpers.destroy_image(std::move(h.Image));
			helpers.destroy_buffer(std::move(h.Buffer));
			h.CopyCommand = VK_NULL_HANDLE;	// pool deallocated below
			vkDestroyFence(device, h.ImagePresented, nullptr);
			h.ImagePresented = VK_NULL_HANDLE;
		}
		headlessSwapchain.clear();

		// free all of the copy command buffers by destroying the pool from which they were allocated:
		vkDestroyCommandPool(device, HeadlessCommandPool, nullptr);
		HeadlessCommandPool = VK_NULL_HANDLE;
	}
	else
	{
		// deallocate the swapchain and thus its images
		if(swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, swapchain, nullptr);
			swapchain = VK_NULL_HANDLE;
		}
	}

}

void RTG::HeadlessSwapchainImage::Save() const 
{
	if (SaveTo == "") return;

	if (Image.format == VK_FORMAT_B8G8R8A8_SRGB) 
	{
		//get a pointer to the image data copied to the buffer:
		char const *bgra = reinterpret_cast< char const * >(Buffer.allocation.data());

		// convert bgra -> rgb data
		std::vector< char > rgb(Image.extent.height * Image.extent.width * 3);
		for (uint32_t y = 0; y < Image.extent.height; ++y) 
		{
			for (uint32_t x = 0; x < Image.extent.width; ++x) 
			{
				rgb[(y * Image.extent.width + x) * 3 + 0] = bgra[(y * Image.extent.width + x) * 4 + 2];
				rgb[(y * Image.extent.width + x) * 3 + 1] = bgra[(y * Image.extent.width + x) * 4 + 1];
				rgb[(y * Image.extent.width + x) * 3 + 2] = bgra[(y * Image.extent.width + x) * 4 + 0];
			}
		}
		// write ppm file
		std::ofstream ppm(SaveTo, std::ios::binary);
		ppm << "P6\n"; //magic number + newline
		ppm << Image.extent.width << " " << Image.extent.height << "\n"; //image size + newline
		ppm << "255\n"; //max color value + newline
		ppm.write(rgb.data(), rgb.size()); //rgb data in row-major order, starting from the top left
	} 
	else 
	{
		std::cerr << "WARNING: saving format " << string_VkFormat(Image.format) << " not supported." << std::endl;
	}
}

static void CursorPosCallback(GLFWwindow *window, double PosX, double PosY)
{
	std::vector< InputEvent > *EventQueue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!EventQueue)
	{
		return;
	}

	InputEvent Event;
	std::memset(&Event, '\0', sizeof(Event));

	Event.type = InputEvent::MouseMotion;
	Event.motion.x = float(PosX);
	Event.motion.y = float(PosY);
	Event.motion.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) 
	{
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) 
		{
			Event.motion.state |= (1 << b);
		}
	}

	EventQueue->emplace_back(Event);
}

static void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
	std::vector< InputEvent > *EventQueue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if(!EventQueue)
	{
		return;
	}

	InputEvent Event;
	std::memset(&Event, '\0', sizeof(Event));

	if(action == GLFW_PRESS)
	{
		Event.type = InputEvent::MouseButtonDown;
	}
	else if(action == GLFW_RELEASE)
	{
		Event.type = InputEvent::MouseButtonUp;
	}
	else
	{
		std::cerr << "Strange: unknown mouse button action." << std::endl;
		return;
	}

	double PosX, PosY;
	glfwGetCursorPos(window, &PosX, &PosY);
	Event.button.x = float(PosX);
	Event.button.y = float(PosY);
	Event.button.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b)
	{
		if(glfwGetMouseButton(window, b) == GLFW_PRESS)
		{
			Event.button.state |= (1 << b);
		}
	}
	Event.button.button = uint8_t(button);
	Event.button.mods = uint8_t(mods);

	EventQueue->emplace_back(Event);
}

static void ScrollCallback(GLFWwindow *window, double offsetX, double offsetY)
{
	std::vector< InputEvent > *EventQueue = reinterpret_cast< std::vector< InputEvent > *>(glfwGetWindowUserPointer(window));
	if(!EventQueue)
	{
		return;
	}

	InputEvent Event;
	std::memset(&Event, '\0', sizeof(Event));

	Event.type = InputEvent::MouseWheel;
	Event.wheel.x = float(offsetX);
	Event.wheel.y = float(offsetY);

	EventQueue->emplace_back(Event);
}

static void KeyCallback(GLFWwindow *window, int Key, int Scanmode, int action, int mods)
{
	std::vector< InputEvent > *EventQueue = reinterpret_cast< std::vector< InputEvent > *>(glfwGetWindowUserPointer(window));
	if(!EventQueue)
	{
		return;
	}

	InputEvent Event;
	std::memset(&Event, '\0', sizeof(Event));

	if(action == GLFW_PRESS)
	{
		Event.type = InputEvent::KeyDown;
	}
	else if(action == GLFW_RELEASE)
	{
		Event.type = InputEvent::KeyUp;
	}
	else if(action == GLFW_REPEAT)
	{
		// ignore repeats
		return;
	}
	else
	{
		std::cerr << "Strange: got unknown keyboard action." << std::endl;
	}

	Event.key.key = Key;
	Event.key.mods = mods;

	EventQueue->emplace_back(Event);
}

void RTG::run(Application &application) 
{
	auto OnSwapchain = [&,this]()
	{
		application.on_swapchain(*this, SwapchainEvent
		{
			.extent = swapchain_extent,
			.images = swapchain_images,
			.image_views = swapchain_image_views,
		});
	};
	OnSwapchain();

	
	// setup event handling:
	std::vector< InputEvent > EventQueue;
	if(!configuration.headless)
	{
		glfwSetWindowUserPointer(window, &EventQueue);

		glfwSetCursorPosCallback(window, CursorPosCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwSetScrollCallback(window, ScrollCallback);
		glfwSetKeyCallback(window, KeyCallback);
	}
	

	uint32_t HeadlessNextImage = 0;

	// setup event handling
	std::chrono::high_resolution_clock::time_point Before = std::chrono::high_resolution_clock::now();

	while (configuration.headless || !glfwWindowShouldClose(window)) 
	{
		float HeadlessDt = 0.0f;
		std::string HeadlessSave = "";

		// event handling:
		if(configuration.headless)
		{
			// read events from stdin
			std::string Line;
			while (std::getline(std::cin, Line)) 
			{
				// parse event from line
				try 
				{
					std::istringstream iss(Line);
					iss.imbue(std::locale::classic()); //ensure floating point numbers always parse with '.' as the separator

					// read type
					std::string type;
					if (!(iss >> type)) throw std::runtime_error("failed to read event type");

					// type-specific parsing
					if (type == "AVAILABLE") 
					{  //AVAILABLE dt [save.ppm]

						// read dt
						if (!(iss >> HeadlessDt)) throw std::runtime_error("failed to read dt");
						if (HeadlessDt < 0.0f) throw std::runtime_error("dt less than zero");

						// check for save file name
						if (iss >> HeadlessSave) 
						{
							if (!HeadlessSave.ends_with(".ppm"))
							{
								throw std::runtime_error("output filename ("" + headless_save + "") must end with .ppm");
							}
						}

						// check for trailing junk
						char junk;
						if (iss >> junk) throw std::runtime_error("trailing junk in event line");

						// stop parsing events so a frame can draw
						break;
					} 
					else 
					{
						throw std::runtime_error("unrecognized type");
					}

				} 
				catch (std::exception &e) 
				{
					std::cerr << "WARNING: failed to parse event (" << e.what() << ") from: "" << line << ""; ignoring it." << std::endl;
				}
			}
			//if we've run out of events, stop running the main loop:
			if (!std::cin)
			{
				break;
			}
		}
		else
		{
			glfwPollEvents();
		}

		// deliver all input events to application:
		for (InputEvent const &input : EventQueue) 
		{
			application.on_input(input);
		}
		EventQueue.clear();

		
		// elapsed time handling:
		{
			std::chrono::high_resolution_clock::time_point After = std::chrono::high_resolution_clock::now();
			float dt = float(std::chrono::duration< double >(After - Before).count());
			Before = After;

			dt = std::min(dt, 0.1f); // lag if frame rate dips too low
			//in headless mode, override dt:
			if (configuration.headless)
			{
				dt = HeadlessDt;
			}
			application.update(dt);
		}

		
		// acquire a workspace
		uint32_t WorkspaceIndex;
		{
			assert(next_workspace < workspaces.size());
			WorkspaceIndex = next_workspace;
			next_workspace = (next_workspace + 1) % workspaces.size();

			// wait until the workspace is not being used:
			VK( vkWaitForFences(device, 1, &workspaces[WorkspaceIndex].workspace_available, VK_TRUE, UINT64_MAX));

			// mark the workspace as in use:
			VK( vkResetFences(device, 1, &workspaces[WorkspaceIndex].workspace_available));
		}

		// acquire an image (resize swapchain if needed)
		uint32_t ImageIndex = -1U;
		if(configuration.headless)
		{
			assert(swapchain == VK_NULL_HANDLE);

			// acquire the least-recently-used headless swapchain image:
			assert(HeadlessNextImage< uint32_t(headlessSwapchain.size()));
			ImageIndex = HeadlessNextImage;
			HeadlessNextImage = (HeadlessNextImage + 1) % uint32_t(headlessSwapchain.size());

			// wait for image to be done copying to buffer
			VK( vkWaitForFences(device, 1, &headlessSwapchain[ImageIndex].ImagePresented, VK_TRUE, UINT64_MAX));

			// save buffer, if needed
			if(headlessSwapchain[ImageIndex].SaveTo != "")
			{
				headlessSwapchain[ImageIndex].Save();
				headlessSwapchain[ImageIndex].SaveTo = "";
			}
			// remember if next frame should be saved:
			headlessSwapchain[ImageIndex].SaveTo = HeadlessSave;

			// mark next copy as pending
			VK( vkResetFences(device, 1, &headlessSwapchain[ImageIndex].ImagePresented));

			// signal GPU that image is "available for rendering to"
			VkSubmitInfo SubmitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &workspaces[WorkspaceIndex].image_available
			};
			VK( vkQueueSubmit(graphics_queue, 1, &SubmitInfo, nullptr));
		}
		else
		{
retry:	
			// Ask the swapchain for the next image index -- note careful return handling:
			if(VkResult Result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, workspaces[WorkspaceIndex].image_available,
				VK_NULL_HANDLE, &ImageIndex);
				Result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				// if the swapchain is out-of-date, recreate it and run the loop again:
				std::cerr << "Recreating swapchain because vkAcquireNextImageKHR returned " << string_VkResult(Result) << "." << std::endl;

				recreate_swapchain();
				OnSwapchain();

				goto retry;
			}
			else if(Result == VK_SUBOPTIMAL_KHR)
			{
				// if the swapchain is suboptimal, render to it and recreate it later:
				std::cerr << "Suboptimal swapchain format -- ignoring for the moment." << std::endl;
			}
			else if(Result != VK_SUCCESS)
			{
				// other non-success results are genuine errors:
				throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(Result)) + ")!");
			}
		}


		// call render function:
		application.render(*this, RenderParams
		{
			.workspace_index = WorkspaceIndex,
			.image_index = ImageIndex,
			.image_available = workspaces[WorkspaceIndex].image_available,
			.image_done = swapchain_image_dones[ImageIndex],
			.workspace_available = workspaces[WorkspaceIndex].workspace_available,
		});

		// queue the work for presentation:
		if(configuration.headless)
		{
			//in headless mode, submit the copy command we recorded previously:

			// will wait in the transfer stage for image_done to be signaled
			VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo SubmitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &swapchain_image_dones[ImageIndex],
				.pWaitDstStageMask = &WaitStage,
				.commandBufferCount = 1,
				.pCommandBuffers = &headlessSwapchain[ImageIndex].CopyCommand,
			};

			VK( vkQueueSubmit(graphics_queue, 1 , &SubmitInfo, headlessSwapchain[ImageIndex].ImagePresented));
		}
		else
		{
			VkPresentInfoKHR PresentInfo
			{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &swapchain_image_dones[ImageIndex],
				.swapchainCount = 1,
				.pSwapchains = &swapchain,
				.pImageIndices = &ImageIndex,
			};

			assert(present_queue);

			// note, again, the careful return handling:
			if(VkResult Result = vkQueuePresentKHR(present_queue, &PresentInfo); 
				Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
			{
				std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(Result) << "." << std::endl;
				recreate_swapchain();
				OnSwapchain();
			}
			else if(Result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(Result)) + ")!");
			}
		}
	}

	if (configuration.headless) {
		for (size_t i = 0; i < headlessSwapchain.size(); ++i) 
		{
			uint32_t ImageIndex = HeadlessNextImage;
			HeadlessNextImage = (HeadlessNextImage + 1) % uint32_t(headlessSwapchain.size());

			//block until the image is finished being "presented" (copied-to-host):
			VK( vkWaitForFences(device, 1, &headlessSwapchain[ImageIndex].ImagePresented, VK_TRUE, UINT64_MAX) );

			//save if requested:
			if (headlessSwapchain[ImageIndex].SaveTo != "") 
			{
				headlessSwapchain[ImageIndex].Save();
				headlessSwapchain[ImageIndex].SaveTo = "";
			}
		}
	}
	// tear down event handling
	if(!configuration.headless)
	{
		glfwSetWindowUserPointer(window, nullptr);

		glfwSetMouseButtonCallback(window, nullptr);
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetScrollCallback(window, nullptr);
		glfwSetKeyCallback(window, nullptr);
	}



}