#pragma once

#include "PosColVertex.hpp"
#include "PosNorTexVertex.hpp"
#include "mat4.hpp"

#include "RTG.hpp"

struct Tutorial : RTG::Application {

	Tutorial(RTG &);
	Tutorial(Tutorial const &) = delete; //you shouldn't be copying this object
	~Tutorial();

	//kept for use in destructor:
	RTG &rtg;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat depth_format{};
	//Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;

	// Background Pipelines:
	struct BackgroundPipeline
	{
		VkPipelineLayout layout = VK_NULL_HANDLE;

		VkPipeline handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass RenderPass, uint32_t subpass);
		void Destroy(RTG &);

		struct Push
		{
			float time;
		};
		
	} BackgroundPipeline;

	// Lines Pipeline
	struct LinesPipeline
	{
		VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;

		struct Push
		{
			float time;
		};

		struct Camera
		{
			Mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(Camera) == 16*4, "camera buffer structure is packed");
		

		// No push constants

		VkPipelineLayout Layout = VK_NULL_HANDLE;

		using Vertex = PosColVertex;

		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
		void Destroy(RTG &);
	} LinesPipeline;

	// Objects Pipeline
	struct ObjectsPipeline
	{
		// Descriptor set Layouts:
		// VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set1Transforms = VK_NULL_HANDLE;

		// types for descriptors:
		struct Transfrom
		{
			Mat4 CLIP_FROM_LOCAL;
			Mat4 WORLD_FROM_LOCAL;
			Mat4 WORLD_FROM_LOCAL_NORMAL;
		};
		static_assert(sizeof(Transfrom) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");

		using Vertex = PosNorTexVertex;

		// no push constants

		VkPipelineLayout Layout = VK_NULL_HANDLE;
		
		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass Render_pass, uint32_t Subpass);
		void Destroy(RTG &);
	} ObjectsPipeline;

	enum PatternType
	{	
		None,
		X,
		Grid,
		BlackHole,
	} PatternType = Grid;
	

	// Pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

	//workspaces hold per-render resources:
	struct Workspace 
	{
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

		// Location for lines data:( streamed to GPU per-frame)
		Helpers::AllocatedBuffer LinesVerticesSrc;	// host coherent; mapped
		Helpers::AllocatedBuffer LinesVertices;		// device-local

		// location for LinesPipeline::Camera data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer Camera_Src;	// host coherent; mapped
		Helpers::AllocatedBuffer Camera;		// device-local
		VkDescriptorSet CameraDescriptors;		// references Camera

		// location for ObjectsPipeline::Transforms data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer TransformsSrc;	// host coherent; mapped
		Helpers::AllocatedBuffer Transforms;	// device-local
		VkDescriptorSet TransformDescriptors;	// references Transforms
	};
	std::vector< Workspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:
	Helpers::AllocatedBuffer ObjectVertices;
	struct ObjectVerticesInfo
	{
		uint32_t first = 0;
		uint32_t count = 0;
	};
	ObjectVerticesInfo PlaneVertices;
	ObjectVerticesInfo TorusVertices;

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

	Helpers::AllocatedImage swapchain_depth_image;
	VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
	std::vector< VkFramebuffer > swapchain_framebuffers;
	//used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
	void destroy_framebuffers();

	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;

	Mat4 CLIP_FROM_WORLD;

	std::vector< LinesPipeline::Vertex > LinesVertices;

	struct ObjectInstance
	{
		ObjectVerticesInfo Vertices;
		ObjectsPipeline::Transfrom Transform;
	};

	std::vector<ObjectInstance> ObjectInstances;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
	void RenderCustom(Workspace &workspace);
	void RenderBackgroundPipeline(Workspace &workspace);
	void RenderLinesPipeline(Workspace &workspace);
	void RenderObjectsPipeline(Workspace &workspace);

// Pattern function just for fun and test
	void MakePatternX();
	void MakePatternGrid();
	void MakePatternBlackHole();
	struct Vec2
	{
		float x;
		float y;

		Vec2 operator*(float Input) const
        {
            return Vec2{x * Input, y * Input};
        }
		Vec2 operator*(Vec2 Input) const
        {
            return Vec2{x * Input.x, y * Input.y};
        }
		Vec2 operator*=(Vec2 Input) const
        {
            return Vec2{x * Input.x, y * Input.y};
        }
		Vec2 operator*=(float Input) const
        {
            return Vec2{x * Input, y * Input};
        }

		Vec2 operator+(Vec2 Input) const
        {
            return Vec2{x + Input.x, y + Input.y};
        }
		Vec2 operator+=(Vec2 Input) const
        {
            return Vec2{x + Input.x, y + Input.y};
        }
		Vec2 operator-(Vec2 Input) const
        {
            return Vec2{x - Input.x, y - Input.y};
        }
		Vec2 operator-=(Vec2 Input) const
        {
            return Vec2{x - Input.x, y - Input.y};
        }

		static void PrintVec2(const Vec2& v)
		{
			printf("(%.3f, %.3f)\n", v.x, v.y);
		}

		static const Vec2 Zero;
		static const Vec2 One;
	};

// Different Mesh Vertices Instantialize
	void InstantializePlane(std::vector< PosNorTexVertex > &Vertices);
	void InstantializeTorus(std::vector< PosNorTexVertex > &Vertices);
};
