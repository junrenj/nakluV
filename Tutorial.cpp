#include "Tutorial.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

const Tutorial::Vec2 Tutorial::Vec2::Zero{0.0f, 0.0f};
const Tutorial::Vec2 Tutorial::Vec2::One{1.0f, 1.0f};

Tutorial::Tutorial(RTG &rtg_) : rtg(rtg_) 
{
	refsol::Tutorial_constructor(rtg, &depth_format, &render_pass, &command_pool);

	BackgroundPipeline.Create(rtg, render_pass, 0);
	LinesPipeline.Create(rtg, render_pass, 0);

	// create descriptor pool:
	{
		uint32_t PerWorkspace = uint32_t(rtg.workspaces.size());	// for easier-to-read counting

		std::array< VkDescriptorPoolSize, 1> PoolSizes
		{
			// we only need uniform buffer descriptors for the moment:
			VkDescriptorPoolSize
			{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1 * PerWorkspace, 	// one descriptor per set, one set per workspace
			},
		};

		VkDescriptorPoolCreateInfo CreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, // because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 1 * PerWorkspace, // one set per workspace
			.poolSizeCount = uint32_t(PoolSizes.size()),
			.pPoolSizes = PoolSizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &CreateInfo, nullptr, &DescriptorPool));
	}

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) 
	{
		refsol::Tutorial_constructor_workspace(rtg, command_pool, &workspace.command_buffer);

		workspace.Camera_Src = rtg.helpers.create_buffer
		(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// going to have GPU copy from this memory
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	// host-visible memory, coherent (no special sync needed)
			Helpers::Mapped 						// get a pointer to the memory
		);

		workspace.Camera = rtg.helpers.create_buffer
		(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT, 	// going to use as a uniform buffer, also going to have GPU copy into this memory
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 	// GPU-local memory
			Helpers::Unmapped 						// don't get a pointer to the memory
		);
		
		// allocate descriptor set for Camera descriptor
		{
			VkDescriptorSetAllocateInfo AllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &LinesPipeline.Set0_Camera,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &AllocInfo, &workspace.CameraDescriptors) );
		}

		 // point descriptor to Camera buffer:
		{
			VkDescriptorBufferInfo CameraInfo
			{
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size,
			};

			std::array< VkWriteDescriptorSet, 1 > Writes
			{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.CameraDescriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &CameraInfo,
				},
			};

			vkUpdateDescriptorSets
			(
				rtg.device, 				// device
				uint32_t(Writes.size()), 	// descriptorWriteCount
				Writes.data(), 				// pDescriptorWrites
				0, 							// descriptorCopyCount
				nullptr 					// pDescriptorCopies
			);
		}
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

		if(workspace.Camera_Src.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Camera_Src));
		}
		if(workspace.Camera.handle != VK_NULL_HANDLE)
		{
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}
	}
	workspaces.clear();

	BackgroundPipeline.Destroy(rtg);
	LinesPipeline.Destroy(rtg);

	if(DescriptorPool)
	{
		vkDestroyDescriptorPool(rtg.device, DescriptorPool, nullptr);
		DescriptorPool = nullptr;
		// (this also frees the descriptor sets allocated from the pool)
	}

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

				workspace.LinesVerticesSrc = rtg.helpers.create_buffer
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

	// upload camera info:
	{ 
		LinesPipeline::Camera Camera
		{
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD
		};
		assert(workspace.Camera_Src.size == sizeof(Camera));

		// host-side copy into Camera_src:
		memcpy(workspace.Camera_Src.allocation.data(), &Camera, sizeof(Camera));

		// add device-side copy from Camera_src -> Camera:
		assert(workspace.Camera_Src.size == workspace.Camera.size);
		VkBufferCopy CopyRegion
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.Camera_Src.size,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.Camera_Src.handle, workspace.Camera.handle, 1, &CopyRegion);
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
			VkClearValue{ .color{ .float32{.0f, .0f, 0.f, 1.0f}}},
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

			// bind Camera descriptor set:
			{
				std::array< VkDescriptorSet, 1 > DescriptorSets
				{
					workspace.CameraDescriptors,
				};
				vkCmdBindDescriptorSets
				(
					workspace.command_buffer, 			// command buffer
					VK_PIPELINE_BIND_POINT_GRAPHICS, 	// pipeline bind point
					LinesPipeline.Layout, 				// pipeline layout
					0, 									// first set
					uint32_t(DescriptorSets.size()),
					DescriptorSets.data(), 				// descriptor sets count, ptr
					0, nullptr 							// dynamic offsets count, ptr
				);
			}
			
			// Push time here
			{
				LinesPipeline::Push push
				{
					.time = time,
				};
				vkCmdPushConstants(workspace.command_buffer, LinesPipeline.Layout, 
									VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
			}
			// Draw Lines vertices
			 vkCmdDraw(workspace.command_buffer, uint32_t(LinesVertices.size()), 1, 0, 0);
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

	// camera orbiting the origin:
	{
		float Angle = float(M_PI) * 2.0f * 2.0f * (time / 60.0f);
		CLIP_FROM_WORLD = Perspective
		(
			60.0f * float(M_PI) / 180.0f, // vfov
			rtg.swapchain_extent.width / float(rtg.swapchain_extent.height), // aspect
			0.1f, // near
			10000.0f // far
		) * Look_at
		(
			3.0f * std::cos(Angle), 3.0f * std::sin(Angle), 1.8f, // eye
			0.0f, 0.0f, 0.5f, // target
			0.0f, 0.0f, 1.0f // up
		);
	}

 	// Test for fun
	// MakePatternX();
	// MakePatternGrid();
	MakePatternBlackHole();
}


void Tutorial::on_input(InputEvent const &) 
{
	
}

// Make Pattern Function (test)
void Tutorial::MakePatternX()
{
	// Make an 'x'
	LinesVertices.clear();
	LinesVertices.reserve(4);

	LinesVertices.emplace_back(PosColVertex
	{
		.Position{ .x = -1.0f, .y = -1.0f, .z = 0.0f },
		.Color{ .r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff }
	});
	LinesVertices.emplace_back(PosColVertex
	{
		.Position{ .x = 1.0f, .y = 1.0f, .z = 0.0f },
		.Color{ .r = 0xff, .g = 0x00, .b = 0x00, .a = 0xff }
	});
	LinesVertices.emplace_back(PosColVertex
	{
		.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
		.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
	});
	LinesVertices.emplace_back(PosColVertex
	{
		.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
		.Color{ .r = 0x00, .g = 0x00, .b = 0xff, .a = 0xff }
	});
	
	assert(LinesVertices.size() == 4);
}

void Tutorial::MakePatternGrid()
{
	LinesVertices.clear();
	constexpr size_t count = 2 * 30 + 2 * 30;
	LinesVertices.reserve(count);
	// horizontal lines at z = 0.5f:
	for (uint32_t i = 0; i < 30; ++i) 
	{
		float y = (i + 0.5f) / 30.0f * 2.0f - 1.0f;
		LinesVertices.emplace_back(PosColVertex
		{
			.Position{.x = -1.0f, .y = y, .z = 0.5f},
			.Color{ .r = 0xff, .g = 0xff, .b = 0x00, .a = 0xff},
		});
		LinesVertices.emplace_back(PosColVertex
		{
			.Position{.x = 1.0f, .y = y, .z = 0.5f},
			.Color{ .r = 0xff, .g = 0xff, .b = 0x00, .a = 0xff},
		});
	}
	// vertical lines at z = 0.0f (near) through 1.0f (far):
	for (uint32_t i = 0; i < 30; ++i) 
	{
		float x = (i + 0.5f) / 30.0f * 2.0f - 1.0f;
		float z = (i + 0.5f) / 30.0f;
		LinesVertices.emplace_back(PosColVertex
		{
			.Position{.x = x, .y =-1.0f, .z = z},
			.Color{ .r = 0x44, .g = 0x00, .b = 0xff, .a = 0xff},
		});
		LinesVertices.emplace_back(PosColVertex
		{
			.Position{.x = x, .y = 1.0f, .z = z},
			.Color{ .r = 0x44, .g = 0x00, .b = 0xff, .a = 0xff},
		});
	}
}

void Tutorial::MakePatternBlackHole()
{
	LinesVertices.clear();

	const size_t GoldenRatioIterate = 16;
	const size_t VerticesNumsPerArcBegin = 32;
	const size_t LinesNum = 46;
	const float RadiusBegin = 0.5f;
	
	size_t VerticesNumNow = VerticesNumsPerArcBegin;
	float RadiusNow = RadiusBegin;
	float Angle = 0.0f;
	float Radians = 0.0f;
	float PosZ = 0.0f;

	// float RandomOffset = 138.985f;

	Vec2 CircleCenter = Vec2::Zero;

	
	constexpr size_t count = GoldenRatioIterate * 48 * LinesNum;
	LinesVertices.reserve(count);

	for (uint32_t k = 0; k < LinesNum; k++)
	{
		for (uint32_t i = 0; i < GoldenRatioIterate; ++i) 
		{
			for (uint32_t j = 0; j < VerticesNumNow; ++j)
			{
				// RandomOffset *= RandomOffset;
				// float intPart;
				// float ColorOffset = std::modf(RandomOffset, &intPart);
				// ColorOffset = ColorOffset >= 0.05f ? 0.05f : ColorOffset;
				// uint8_t ColorValue = 0xff / (uint8_t)VerticesNumsPerArcBegin * (uint8_t)j + uint8_t(ColorOffset * 255);

				Radians = Angle * 3.1415926f / 180.0f;
				Vec2 PointPos = CircleCenter + Vec2(cosf(Radians) * RadiusNow ,sinf(Radians) * RadiusNow);
				LinesVertices.emplace_back(PosColVertex
				{
					.Position{.x = PointPos.x, .y = PointPos.y, .z = PosZ},
					.Color{ .r = 0xff, .g = 0xff, .b = 0xff, .a = 0xff},
				});

				Angle += 90.0f / VerticesNumNow;
				Angle = fmodf(Angle, 360.0f);
				PosZ += 0.002f;
			}
			// Offset for the circle center
			Radians = (Angle - 90.0f) * 3.1415926f / 180.0f;
			float offsetX = -sinf(Radians) * RadiusNow;
			float offsetY = cosf(Radians) * RadiusNow;
			CircleCenter = Vec2(CircleCenter.x - offsetX , CircleCenter.y - offsetY);
			// Golden Ratio
			RadiusNow *= 2.0f;

			VerticesNumNow += 1;
		}
		RadiusNow = RadiusBegin;
		Angle = fmodf(k * 8.0f, 360.0f);
		PosZ = 0.0f;
		CircleCenter = Vec2::Zero;
		VerticesNumNow = VerticesNumsPerArcBegin;
	}
	
	
}
