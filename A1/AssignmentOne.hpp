#pragma once
#include "AssignmentOne-Vertex.hpp"
#include "../RTG.hpp"
#include "../mat4.hpp"
#include "../PosColVertex.hpp"
#include "Render/RenderScene.hpp"


struct UAssignmentOne : RTG::Application
{
    UAssignmentOne(RTG &);
	UAssignmentOne(UAssignmentOne const &) = delete; //you shouldn't be copying this object
	~UAssignmentOne();

    // Kept for use in destructor:
    RTG &rtg;

    //--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat DepthFormat{};
	//Render passes describe how pipelines write to images:
	VkRenderPass RenderPass = VK_NULL_HANDLE;

    // Background Pipeline
    struct FBackgroundPipeline
    {
		VkPipelineLayout Layout = VK_NULL_HANDLE;

		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass RenderPass, uint32_t subpass);
		void Destroy(RTG &);

		struct FPush
		{
			float time;
		};
    }BackgroundPipeline;

	// Linepipeline used for Debug
	struct FLinesPipeline
	{
		VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;
		struct Camera
		{
			Mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(Camera) == 16*4, "camera buffer structure is packed");
	
		VkPipelineLayout Layout = VK_NULL_HANDLE;

		using Vertex = PosColVertex;

		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
		void Destroy(RTG &);
	}LinesPipeline;

	std::vector< FLinesPipeline::Vertex > LinesVertices;

    struct FLambertPipeline
    {
        // Descriptor set Layouts:
		VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set2_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set3_TEXTURE = VK_NULL_HANDLE;

        // types for descriptors:
		struct FWorld
		{
			struct { float x, y, z, padding_; } SKY_DIRECTION;
			struct { float r, g, b, padding_; } SKY_ENERGY;
			struct { float x, y, z, padding_; } SUN_DIRECTION;
			struct { float r, g, b, padding_; } SUN_ENERGY;
		};
        static_assert(sizeof(FWorld) == 4*4 + 4*4 + 4*4 + 4*4, "World is the expected size.");

		struct FCamera
		{
			Mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(FCamera) == 16*4, "camera buffer structure is packed");

		struct FTransform
		{
			Mat4 CLIP_FROM_LOCAL;
			Mat4 WORLD_FROM_LOCAL;
			Mat4 WORLD_FROM_LOCAL_NORMAL;
		};
        static_assert(sizeof(FTransform) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");
		
		using FVertex = FVertexDataSet;

		VkPipelineLayout Layout = VK_NULL_HANDLE;
		
		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass Render_pass, uint32_t Subpass);
		void Destroy(RTG &);
		
		}LambertPipeline;

		VkCommandPool CommandPool = VK_NULL_HANDLE;
		VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    
		//workspaces hold per-render resources:
		struct FWorkspace 
		{
			VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

			//location for ObjectsPipeline::World data: (streamed to GPU per-frame)
			Helpers::AllocatedBuffer WorldSrc; 	// host coherent; mapped
			Helpers::AllocatedBuffer World; 	// device-local
			VkDescriptorSet WorldDescriptors; 	// references World

			// Location for lines data:( streamed to GPU per-frame)
			Helpers::AllocatedBuffer LinesVerticesSrc;	// host coherent; mapped
			Helpers::AllocatedBuffer LinesVertices;		// device-local

			// location for LinesPipeline::Camera data: (streamed to GPU per-frame)
			Helpers::AllocatedBuffer CameraSrc;	// host coherent; mapped
			Helpers::AllocatedBuffer Camera;		// device-local
			VkDescriptorSet CameraDescriptors;		// references Camera

			// location for ObjectsPipeline::Transforms data: (streamed to GPU per-frame)
			Helpers::AllocatedBuffer TransformsSrc;	// host coherent; mapped
			Helpers::AllocatedBuffer Transforms;	// device-local
			VkDescriptorSet TransformDescriptors;	// references Transforms
		};
		std::vector< FWorkspace > workspaces;

		//-------------------------------------------------------------------
		//static scene resources:
		Helpers::AllocatedBuffer ObjectVertices;

		std::vector< Helpers::AllocatedImage > Textures;
		std::vector< VkImageView > TextureViews;
		VkSampler TextureSampler = VK_NULL_HANDLE;
		VkDescriptorPool TextureDescriptorPool = VK_NULL_HANDLE;
		std::vector< VkDescriptorSet > TextureDescriptors;

		//--------------------------------------------------------------------
		//Resources that change when the swapchain is resized:

		virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

		Helpers::AllocatedImage SwapchainDepthImage;
		VkImageView SwapchainDepthImageView = VK_NULL_HANDLE;
		std::vector< VkFramebuffer > SwapchainFramebuffer;
		//used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
		void DestroyFramebuffers();

		//--------------------------------------------------------------------
		//Resources that change when time passes or the user interacts:

		virtual void update(float dt) override;
		virtual void on_input(InputEvent const &) override;

		// Modal action, intercepts inputs
		std::function< void(InputEvent const &) > Action; 

		float Time = 0.0f;

		enum class ECameraMode
		{
			Scene = 0,
			Free = 1,
			Orbit = 2,
		} CameraMode = ECameraMode::Scene;

		// Used when cameraMode == cameraMode::Free
		struct FOrbitCamera
		{
			float TargetX = 0.0f, TargetY = 0.0f, TargetZ = 0.0f; // Where Camera Looking + Orbiting
			float Radius = 2.0f; 	// Distance from camera to target
			float Azimuth = 0.0f; 	// Counterclockwise angle around z axis between x axis and camera direction (radians)
			float Elevation = 0.25f * float(M_PI); // Angle up from xy plane to camera direction(radians)

			float FOV = 60.0f / 180.0f * float(M_PI);	// vertical field of view (radians)
			float Near = 0.1f;		// Near Clippping plane
			float Far = 1000.0f;	// Far Clipping plane 
		} FreeCamera;

		// Computed from the current camera (as set by camera_mode) during update():
		Mat4 CLIP_FROM_WORLD;

		FLambertPipeline::FWorld World;

		//--------------------------------------------------------------------
		//Rendering function, uses all the resources above to queue work to draw a frame:

		virtual void render(RTG &, RTG::RenderParams const &) override;

		void CustomViewPillarBoxing();

		// Load Scene
		URenderScene Scene;
		uint8_t ActiveCameraIdx = 0;
		void InitializeRenderScene();
		// TODO:delete functions below
		void PrintRenderSceneMesh();
		void PrintRenderProxies();
		void PrintMatrix(const std::string& name, const glm::mat4& m);
		std::string FormatTexIdx(uint32_t idx);
		std::string ToString(EMaterialType type);
		void PrintMaterial();
		void PrintTextureSizes();
		void PrintLightProxy();
		// Debug
		uint8_t DebugBBoxColor[4] = {255, 255, 255, 255};		// White
		uint8_t DebugCameraLineColor[4] = {255, 0 ,0 ,255};	// Red
		void RenderDebugLine();
		void GenerateBBoxVertices(const UBoundingBox& BBox);
		void GenerateFrustumVertices(const UCamera& Camera);
		
		void RenderBackgroundPipeline(FWorkspace &workspace);
		void RenderLinesPipeline(FWorkspace &workspace);
		void RenderLambertPipeline(FWorkspace &workspace);

		// Texture Loader
		void ReserveTextures();		// Reserve Texture to gpu

		// Camera Function
		void UpdateCamera();
};
