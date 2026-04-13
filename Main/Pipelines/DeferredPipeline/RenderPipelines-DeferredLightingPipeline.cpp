#include "RenderPipelines-DeferredLightingPipeline.hpp"

#include "../../RenderPipelines.hpp"
#include "../../../RTG.hpp"
#include "../../../VK.hpp"

static uint32_t vert_code[] =
#include "../../../spv/Main/Pipelines/DeferredPipeline/deferred-lighting.vert.inl"
;

static uint32_t frag_code[] =
#include "../../../spv/Main/Pipelines/DeferredPipeline/deferred-lighting.frag.inl"
;

void FDeferredLightingPipeline::Create(
    RTG &rtg, 
    VkRenderPass RenderPass, uint32_t Subpass, 
    VkDescriptorSetLayout WorldLayout, VkDescriptorSetLayout LightsLayout, 
    VkDescriptorSetLayout EnvTexLayout, VkDescriptorSetLayout ShadowLayout)
{
    VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

    Set1_World = WorldLayout;
    Set2_Lights = LightsLayout;
    Set3_EnvTex = EnvTexLayout;
    Set4_Shadowmap = ShadowLayout;
    
    // set 0: gbuffer textures
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings
        {
            VkDescriptorSetLayoutBinding
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &Set0_GBuffer));
    }

    {
        std::array<VkDescriptorSetLayout, 5> layouts 
        {
            Set0_GBuffer,
            Set1_World,
            Set2_Lights,
            Set3_EnvTex,
            Set4_Shadowmap
        };

        VkPushConstantRange pushRange
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(FConstant),
        };

        VkPipelineLayoutCreateInfo create_info 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
        };

        VK ( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &Layout));
    }

    {
        std::array <VkPipelineShaderStageCreateInfo, 2> stages 
        {
            VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main",
            },
            VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main",
            },
        };

        std::vector<VkDynamicState> dynamic_states 
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = (uint32_t) (dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineViewportStateCreateInfo viewport_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };
        // 3 GBuffer output
        std::array<VkPipelineColorBlendAttachmentState, 3> blend_attachments
        {
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT,
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT,
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT,
            },
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = uint32_t(blend_attachments.size()),
            .pAttachments = blend_attachments.data(),
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        VkGraphicsPipelineCreateInfo create_info 
        {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = (uint32_t) (stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &URenderPipelines::FLambertPipeline::FVertex::ArrayInputState,
            .pInputAssemblyState = &input_assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = Layout,
            .renderPass = RenderPass,
            .subpass = Subpass,
        };

        VK ( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &Handle));
    }   
}

void FDeferredLightingPipeline::Destroy(RTG &rtg)
{
    if (Handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(rtg.device, Handle, nullptr);
        Handle = VK_NULL_HANDLE;
    }
    if (Layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(rtg.device, Layout, nullptr);
        Layout = VK_NULL_HANDLE;
    }
    if (Set0_GBuffer != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, Set0_GBuffer, nullptr);
        Set0_GBuffer = VK_NULL_HANDLE;
    }
    if (Set1_World != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, Set1_World, nullptr);
        Set1_World = VK_NULL_HANDLE;
    }
    if (Set2_Lights != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, Set2_Lights, nullptr);
        Set2_Lights = VK_NULL_HANDLE;
    }
    if (Set3_EnvTex != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, Set3_EnvTex, nullptr);
        Set3_EnvTex = VK_NULL_HANDLE;
    }
    if (Set4_Shadowmap != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(rtg.device, Set4_Shadowmap, nullptr);
        Set4_Shadowmap = VK_NULL_HANDLE;
    }
    
}