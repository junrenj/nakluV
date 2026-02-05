#pragma once

#include "RTG.hpp"

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
    struct FAssignmentOneBackgroundPipeline
    {

    }BackgroundPipeline;

    struct FAssignmentOneLambertPipeline
    {
        // Descriptor set Layouts:
		VkDescriptorSetLayout Set0_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set1_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout Set2_TEXTURE = VK_NULL_HANDLE;

        // types for descriptors:
		struct World
		{
			struct { float x, y, z, padding_; } SKY_DIRECTION;
			struct { float r, g, b, padding_; } SKY_ENERGY;
			struct { float x, y, z, padding_; } SUN_DIRECTION;
			struct { float r, g, b, padding_; } SUN_ENERGY;

			void DirectionNormalize()
			{ 
				float Length2 = SUN_DIRECTION.x * SUN_DIRECTION.x +
								SUN_DIRECTION.y * SUN_DIRECTION.y +
								SUN_DIRECTION.z * SUN_DIRECTION.z;


				if (Length2 > 0.0f)
				{
					float LenInv = 1.0f / std::sqrt(Length2);
					SUN_DIRECTION.x *= LenInv;
					SUN_DIRECTION.y *= LenInv;
					SUN_DIRECTION.z *= LenInv;
				}
			}
		};
         static_assert(sizeof(World) == 4*4 + 4*4 + 4*4 + 4*4, "World is the expected size.");

            struct Transform
            {
                Mat4 CLIP_FROM_LOCAL;
                Mat4 WORLD_FROM_LOCAL;
                Mat4 WORLD_FROM_LOCAL_NORMAL;
            };
            static_assert(sizeof(Transform) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");
        }LambertPipeline;

   
    

};
