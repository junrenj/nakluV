#include "Tutorial.hpp"
#include "VK.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"

static uint32_t vert_code[] =
#include "spv/background.vert.inl"
;

static uint32_t frag_code[] =
#include "spv/background.frag.inl"
;


void Tutorial::BackgroundPipeline::Create(RTG &rtg, VkRenderPass RenderPass, uint32_t Subpass)
{
    VkShaderModule Vert_Module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule Frag_Module = rtg.helpers.create_shader_module(frag_code);

    // refsol::BackgroundPipeline_create(rtg, RenderPass, Subpass, Vert_Module, Frag_Module, &layout, &handle);

    {
        // Create pipeline layout:
        VkPushConstantRange Range
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        VkPipelineLayoutCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &Range,
        };

        VK( vkCreatePipelineLayout(rtg.device, &CreateInfo, nullptr, &layout));
    }

    {
        // Create Pipeline

        // Shader code for verttex and fragment pipeline stages:
        std::array< VkPipelineShaderStageCreateInfo, 2 > Stages
        {
            VkPipelineShaderStageCreateInfo
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = Vert_Module,
                .pName = "main",
            },
            VkPipelineShaderStageCreateInfo
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = Frag_Module,
                .pName = "main",
            },
        };

        // The viewport and scissor state will be set at runtime for the pipeline:
        std::vector< VkDynamicState> DynamicStates
        {
            VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo DynamicState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(DynamicStates.size()),
            .pDynamicStates = DynamicStates.data(),
        };

        // TODO: draw triangles

        // TODO: one viewport and scissor rectangle:

        // TODO: the rasterizer will cull back faces and fill polygons:

        // TODO: multisampling will be disabled (one sample per pixel):

        // TODO: //depth and stencil tests will be disabled:

        // There will be one color attachment with blending disabled:
        std::array< VkPipelineColorBlendAttachmentState, 1 > AttachmentStates
        {
            VkPipelineColorBlendAttachmentState
            {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        };
        VkPipelineColorBlendStateCreateInfo ColorBlendState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = uint32_t(AttachmentStates.size()),
            .pAttachments = AttachmentStates.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.0f},
        };

        // All of the above structures get bundled together into one very large create_info:
        VkGraphicsPipelineCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(Stages.size()),
            .pStages = Stages.data(),
            .pVertexInputState = &,
            .pInputAssemblyState = &input_assembly_state,
            .pRasterizationState = &rasterization_state,
			.pMultisampleState = &multisample_state,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blend_state,
			.pDynamicState = &dynamic_state,
			.layout = layout,
			.renderPass = render_pass,
			.subpass = subpass,
        };

        VK( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &handle) );
    }
}

void Tutorial::BackgroundPipeline::Destroy(RTG &rtg)
{
    refsol::BackgroundPipeline_destroy(rtg, &layout, &handle);
}