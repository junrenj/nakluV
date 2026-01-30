#include "RTG.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
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
		} else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	configuration = configuration_;

	//fill in flags/extensions/layers information:

	//create the `instance` (main handle to Vulkan library):
	refsol::RTG_constructor_create_instance(
		configuration.application_info,
		configuration.debug,
		&instance,
		&debug_messenger
	);

	//create the `window` and `surface` (where things get drawn):
	refsol::RTG_constructor_create_surface(
		configuration.application_info,
		configuration.debug,
		configuration.surface_extent,
		instance,
		&window,
		&surface
	);

	//select the `physical_device` -- the gpu that will be used to draw:
	refsol::RTG_constructor_select_physical_device(
		configuration.debug,
		configuration.physical_device_name,
		instance,
		&physical_device
	);

	//select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
	refsol::RTG_constructor_select_format_and_mode(
		configuration.debug,
		configuration.surface_formats,
		configuration.present_modes,
		physical_device,
		surface,
		&surface_format,
		&present_mode
	);

	//create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
	refsol::RTG_constructor_create_device(
		configuration.debug,
		physical_device,
		surface,
		&device,
		&graphics_queue_family,
		&graphics_queue,
		&present_queue_family,
		&present_queue
	);

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

			VK( vkCreateSemaphore(device, &CreateInfo, nullptr, &workspace.workspace_available));
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

	//destroy the swapchain:
	destroy_swapchain();

	//destroy Helpers structure resources:
	helpers.destroy();

	//destroy the rest of the resources:
	refsol::RTG_destructor( &device, &surface, &window, &debug_messenger, &instance );

}


void RTG::recreate_swapchain() 
{
	// clean up swapchain if it already exists
	if(!swapchain_images.empty())
	{
		destroy_swapchain();
	}

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
	uint32_t Count = 0;
	VK( vkGetSwapchainImagesKHR(device, swapchain, &Count, nullptr));
	swapchain_images.resize(Count);
	VK( vkGetSwapchainImagesKHR(device, swapchain, &Count, swapchain_images.data()));

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
		Semaphore = nullptr;
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

	// deallocate the swapchain and (thus) its images:
	if(swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
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
	std::memset(&event, '\0', sizeof(Event));

	event.type = InputEvent::MouseMotion;
	event.motion.x = float(PosX);
	event.motion.y = float(PosY);
	event.motion.state = 0;
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
		application.on_swapchain(*thism SwapchainEvent
		{
			.extent = swapchain_extent,
			.images = swapchain_images,
			.image_views = swapchain_image_views,
		});
	};
	OnSwapchain();

	// setup event handling
	std::chrono::high_resolution_clock::time_point Before = std::chrono::high_resolution_clock::now();

	// setup time handling
	std::vector< InputEvent > EventQueue;
	glfwSetWindowUserPointer(window, &EventQueue);

	glfwSetCursorPosCallback(window, CursorPosCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetKeyCallback(window, KeyCallback);

	// tear down event handling
	glfwSetMouseButtonCallback(window, nullptr);
	glfwSetCursorPosCallback(window, nullptr);
	glfwSetScrollCallback(window, nullptr);
	glfwSetKeyCallback(window, nullptr);

	glfwSetWindowUserPointer(window, nullptr);

	while (!glfwWindowShouldClose(window)) 
	{
		// event handling:
		glfwPollEvents();

		// deliver all input events to application:
		for (InputEvent const &input : event_queue) 
		{
			application.on_input(input);
		}
		event_queue.clear();

		// elapsed time handling:
		std::chrono::high_resolution_clock::time_point After = std::chrono::high_resolution_clock::now();
		float dt = float(std::chrono::duration< double >(After - Before).count());
		Before = After;

		dt = std::min(dt, 0.1f); // lag if frame rate dips too low

		application.update(dt);

		// setup event handling:

		
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
			throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(result)) + ")!");
		}

		// queue rendering work
		application.render(*this, RenderParams
		{
			.workspace_index = WorkspaceIndex,
			.image_index = ImageIndex,
			.image_available = workspaces[WorkspaceIndex].image_available,
			.image_done = swapchain_image_dones[ImageIndex],
			.workspace_available = workspaces[WorkspaceIndex].workspace_available,
		});

		// present image (resize swapchain if needed)
		{
			VkPresentInfoKHR PresentInfo
			{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &swapchain_image_dones[image_index],
				.swapchainCount = 1,
				.pSwapchains = &swapchain,
				.pImageIndices = &image_index,
			};

			assert(present_queue);

			// note, again, the careful return handling:
			if(VkResult Result = vkQueuePresentKHR(present_queue, &PresentInfo));
				result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
				{
					std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(result) << "." << std::endl;
					recreate_swapchain();
					on_swapchain();
				}
			else if(result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(result)) + ")!");
			}
		}
	}

	//TODO: tear down event handling
}
