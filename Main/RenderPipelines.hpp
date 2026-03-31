#pragma once
#include "RenderPipelines-Vertex.hpp"
#include "../RTG.hpp"
#include "Render/RenderScene.hpp"
#include "Debug/DebugView.hpp"
#include "Pipelines/RenderPipelines-ShadowPipeline.hpp"
#include "Pipelines/DeferredPipeline/RenderPipelines-DeferredGeometryPipeline.hpp"

struct FShadowResource;
struct FCubeShadowResource;

struct URenderPipelines : RTG::Application
{
    URenderPipelines(RTG &);
	URenderPipelines(URenderPipelines const &) = delete; //you shouldn't be copying this object
	~URenderPipelines();

    // Kept for use in destructor:
    RTG &rtg;

    //--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat DepthFormat{};
	//Render passes describe how pipelines write to images:
	VkRenderPass RenderPass = VK_NULL_HANDLE;

//~BEGIN Shadow

	// data set
	FShadowDataSet ShadowData;
	// pipeline
	FShadowPipeline ShadowPipeline;

//~END Shadow

//~BEGIN GBuffer Pass
	
	// data set
	FGBufferDataSet GBufferData;
	// pipeline 
	FDeferredGeometryPipeline DeferredGeometryPipeline;

//~END GBuffer Pass

	// Sampler
	VkSampler TextureSamplerNearest = VK_NULL_HANDLE;
	VkSampler TextureSamplerLinear = VK_NULL_HANDLE;

//~BEGIN Forward Pass
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
			mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(Camera) == 16*4, "camera buffer structure is packed");
	
		VkPipelineLayout Layout = VK_NULL_HANDLE;

		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass RenderPass, uint32_t Subpass);
		void Destroy(RTG &);
	}LinesPipeline;
	std::vector< FDebugColVertex > LinesVertices;	// for debug

    struct FLambertPipeline
    {
        // Descriptor set Layouts:
		VkDescriptorSetLayout Set0_Camera = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set1_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set2_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set3_TEXTURE = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set4_Lights = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set5_EnvTex = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set6_Shadowmap = VK_NULL_HANDLE;

		struct FConstant
		{
			int MaterialType;	// Tag for materials
			int LightsCount;	// How many lights on the scene
		};

        // types for descriptors:
		struct FWorld
		{
			struct { float x, y, z, padding_; } VIEW_POS;
			struct { float exposure, tonemappingMode, padding_0, padding_1; } AJUST_VAR;
		};
        static_assert(sizeof(FWorld) == 4*4 + 4*4, "World is the expected size.");

		struct FCamera
		{
			mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(FCamera) == 16*4, "camera buffer structure is packed");

		struct FTransform
		{
			mat4 CLIP_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL_NORMAL;
		};
        static_assert(sizeof(FTransform) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");
		
		using FVertex = FVertexDataSet;

		VkPipelineLayout Layout = VK_NULL_HANDLE;
		
		VkPipeline Handle = VK_NULL_HANDLE;

		void Create(RTG &, VkRenderPass Render_pass, uint32_t Subpass);
		void Destroy(RTG &);
		
		const uint32_t MAX_SPOT_SHADOWS = 32;	//TODO: replace it
	}LambertPipeline;
//~END Forward Pass
	
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

		// Location for ObjectsPipeline::Lights data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer LightsSrc;		// host coherent; mapped
		Helpers::AllocatedBuffer Lights;		// device-local
		VkDescriptorSet LightsDescriptors;		// references Lights

		// location for ShadowPipeline::Transforms data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer ShadowTransformsSrc;	// host coherent; mapped
		Helpers::AllocatedBuffer ShadowTransforms;	// device-local
		VkDescriptorSet ShadowTransformDescriptors;	// references Transforms
	};
	std::vector< FWorkspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:
	Helpers::AllocatedBuffer ObjectVertices;

	std::vector< Helpers::AllocatedImage > Textures;
	std::vector< VkImageView > TextureViews;
	VkDescriptorPool TextureDescriptorPool = VK_NULL_HANDLE;
	std::vector< VkDescriptorSet > MaterialDescriptors;
	VkDescriptorPool EnvTexDescriptorPool = VK_NULL_HANDLE;
	std::vector< VkDescriptorSet > EnvTexDescriptors;

	VkDescriptorPool ShadowDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet ShadowDescriptors = VK_NULL_HANDLE;

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
	mat4 CLIP_FROM_WORLD;

	FLambertPipeline::FWorld World;
	
	struct { float x, y, z, padding_; } EYE;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;

	// Initial command line 
	void InitializeCommandLineSettings();

	// Load Scene
	URenderScene Scene;
	void InitializeRenderScene();
	
	// Pipeline Render
	void RenderBackgroundPipeline(FWorkspace &workspace);	// maybe abandoned, because it is no use now
	void RenderLinesPipeline(FWorkspace &workspace);		// for debug
	void RenderLambertPipeline(FWorkspace &workspace);		// the major Pipeline to pass
	void RenderShadowMaps(FWorkspace& Workspace);			// the pipeline for shadow map
	void RenderCubeShadowMaps(FWorkspace& Workspace);			// the pipeline for cube shadow map

	// Texture Loader
	void ReserveTextures();		// Reserve Texture to gpu

	// Viewport
	const float DefaultAspect = 16.0f / 9.0f;
	void ViewportPillarBoxing(FWorkspace &workspace);

	// Camera Function
	uint8_t ActiveCameraIdx = 0;
	void UpdateCamera();

	// Animation
	bool bIsPlay = true;

	// Shadow
	void GenerateShadowRes(const ULight* Light);
	void GenerateCubeShadowRes(const ULight* Light);

	// for debug
	UDebugScene DebugScene;
	void InitializeDebugRenderScene();
	void DrawDebugLines();
};
