#include "Tutorial.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

Tutorial::Tutorial(RTG &rtg_) : rtg(rtg_) {
	refsol::Tutorial_constructor(rtg, &depth_format, &render_pass, &command_pool);

	BackgroundPipeline.Create(rtg, render_pass, 0);
	LinesPipeline.Create(rtg, render_pass, 0);

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		refsol::Tutorial_constructor_workspace(rtg, command_pool, &workspace.command_buffer);
	}
}

Tutorial::~Tutorial() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces) 
	{
		refsol::Tutorial_destructor_workspace(rtg, command_pool, &workspace.command_buffer);
		
		if(workspace.LinesVerticesSrc.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.LinesVerticesSrc));
		}
		if(workspace.LinesVertices.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.LinesVertices));
		}
	}
	workspaces.clear();

	BackgroundPipeline.Destroy(rtg);
	LinesPipeline.Destroy(rtg);

	refsol::Tutorial_destructor(rtg, &render_pass, &command_pool);
}

void Tutorial::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//[re]create framebuffers:
	refsol::Tutorial_on_swapchain(rtg, swapchain, depth_format, render_pass, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}

void Tutorial::destroy_framebuffers() {
	refsol::Tutorial_destroy_framebuffers(rtg, &swapchain_depth_image, &swapchain_depth_image_view, &swapchain_framebuffers);
}


void Tutorial::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	// //record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	// refsol::Tutorial_render_record_blank_frame(rtg, render_pass, framebuffer, &workspace.command_buffer);

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
	
	// GPU commands here:
	// Line Render Pass
	{
		// Upload line vertices
		if(!LinesVertices.empty())
		{
			// [re-]allocate lines buffers if needed:
			size_t NeededBytes = LinesVertices.size() * sizeof(LinesVertices[0]);
			if(workspace.LinesVerticesSrc.handle == VK_NULL_HANDLE ||
				workspace.LinesVerticesSrc.size < NeededBytes)
			{
				size_t NewBytes = ((NeededBytes + 4096) / 4096) * 4096;
				if(workspace.LinesVerticesSrc.handle)
				{
					rtg.helpers.destroy_buffer(std::move(workspace.LinesVerticesSrc));
				}
				if(workspace.LinesVertices.handle)
				{
					rtg.helpers.destroy_buffer(std::move(workspace.LinesVertices));
				}

				workspace.LinesVertices = rtg.helpers.create_buffer
				(
					NewBytes,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 											// going to have GPU copy from this memory
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host-visible memory, coherent (no special sync needed)
					Helpers::Mapped 															// get a pointer to the memory
				);
				workspace.LinesVertices = rtg.helpers.create_buffer
				(
					NewBytes,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 	// going to use as vertex buffer, also going to have GPU into this memory
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 									// GPU-local memory
					Helpers::Unmapped 														// don't get a pointer to the memory
				);

				std::cout << "Re-allocated lines buffers to " << NewBytes << " bytes." << std::endl;
			}

			assert(workspace.LinesVerticesSrc.size == workspace.LinesVertices.size);
			assert(workspace.LinesVerticesSrc.size >= NeededBytes);

			// Host-side copy into LinesVerticesSrc:
			assert(workspace.LinesVerticesSrc.allocation.mapped);
			std::memcpy(workspace.LinesVerticesSrc.allocation.data(), LinesVertices.data(), NeededBytes);

			// Device-side copy from LinesVerticesSrc -> LineVertices
			VkBufferCopy CopyRegion
			{
				.srcOffset = 0,
				.dstOffset = 0,
				.size = NeededBytes,
			};
			vkCmdCopyBuffer(workspace.command_buffer, workspace.LinesVerticesSrc.handle, 
							workspace.LinesVertices.handle, 1, &CopyRegion);
		}
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

	// Background Render Pass
	{
		// render pass
		std::array<VkClearValue, 2> clear_values
		{
			VkClearValue{ .color{ .float32{.5f, .1f, .3f, 1.0f}}},
			VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0}},
		};

		VkRenderPassBeginInfo begin_info
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_pass,
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

		// draw with the background pipeline:
		{
			
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, BackgroundPipeline.handle);
			
			// Push time here
			{
				BackgroundPipeline::Push push
				{
					.time = time,
				};
				vkCmdPushConstants(workspace.command_buffer, BackgroundPipeline.layout, 
									VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			}
			vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		}

		// Draw with the lines pipeline:
		{
			
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
							LinesPipeline.Handle);
			{
				// Use LinesVertices (offset 0) as vertex buffer binding 0:
				std::array< VkBuffer, 1 > VertexBuffers{ workspace.LinesVertices.handle };
				std::array< VkDeviceSize, 1 > Offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(VertexBuffers.size()),
										VertexBuffers.data(), Offsets.data());
			}

			// Draw Lines vertices
			 vkCmdDraw(workspace.command_buffer, uint32_t(LinesVertices.size()), 1, 0, 0);
		}

		vkCmdEndRenderPass(workspace.command_buffer);
	}

	// end recording:
	VK(vkEndCommandBuffer(workspace.command_buffer));

	//submit `workspace.command buffer` for the GPU to run:
	refsol::Tutorial_render_submit(rtg, render_params, workspace.command_buffer);
}


void Tutorial::update(float dt) 
{
	time = std::fmod(time + dt, 60.0f);

	// Make an 'x'
	LinesVertices.clear();
	LinesVertices.reserve(4);

	LinesVertices.emplace_back(
	PosColVertex
	{
		.Position{ .x = -1.0f, .y = -1.0f, .z = 0.0f },
		.Color{ .r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff }
	});
	LinesVertices.emplace_back(
	PosColVertex
	{
		.Position{ .x = 1.0f, .y = 1.0f, .z = 0.0f },
		.Color{ .r = 0xff, .g = 0x00, .b = 0x00, .a = 0xff }
	});
	LinesVertices.emplace_back(
	PosColVertex
	{
		.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
		.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
	});
	LinesVertices.emplace_back(
	PosColVertex
	{
		.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
		.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
	});

	assert(LinesVertices.size() == 4);
}


void Tutorial::on_input(InputEvent const &) 
{

}

