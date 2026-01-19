#include "Tutorial.hpp"
#include "VK.hpp"

#include "Helpers.hpp"

static uint32_t vert_code[] =
#include "spv/lines.vert.inl"
;

static uint32_t frag_code[] =
#include "spv/lines.frag.inl"
;


void Tutorial::LinesPipeline::Create(RTG &rtg, VkRenderPass RenderPass, uint32_t Subpass)
{
    VkShaderModule Vert_Module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule Frag_Module = rtg.helpers.create_shader_module(frag_code);

    // refsol::BackgroundPipeline_create(rtg, RenderPass, Subpass, Vert_Module, Frag_Module, &layout, &handle);

    {

        VkPipelineLayoutCreateInfo CreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        VK( vkCreatePipelineLayout(rtg.device, &CreateInfo, nullptr, &Layout));
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

        // This pipeline will draw lines:
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        // This pipeline will render to one viewport and scissor rectangle:
        VkPipelineViewportStateCreateInfo ViewportState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        // The rasterizer will cull back faces and fill polygons:
        VkPipelineRasterizationStateCreateInfo RasterizationState
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

        // Multisampling will be disabled (one sample per pixel):
        VkPipelineMultisampleStateCreateInfo MultisampleState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
        };

        // Depth test will be less, and stencil test will be disabled:
        VkPipelineDepthStencilStateCreateInfo DepthStencilState
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };

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
            .pVertexInputState = &Vertex::ArrayInputState,
            .pInputAssemblyState = &InputAssemblyState,
			.pViewportState = &ViewportState,
            .pRasterizationState = &RasterizationState,
			.pMultisampleState = &MultisampleState,
			.pDepthStencilState = &DepthStencilState,
			.pColorBlendState = &ColorBlendState,
			.pDynamicState = &DynamicState,
			.layout = Layout,
			.renderPass = RenderPass,
			.subpass = Subpass,
        };

        VK( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &CreateInfo, nullptr, &Handle) );
    }
}

void Tutorial::LinesPipeline::Destroy(RTG &rtg)
{
    if(Layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(rtg.device, Layout, nullptr);
        Layout = VK_NULL_HANDLE;
    }

    if(Handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(rtg.device, Handle, nullptr);
        Handle = VK_NULL_HANDLE;
    }
}