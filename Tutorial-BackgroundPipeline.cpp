#include "Tutorial.hpp"

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

    refsol::BackgroundPipeline_create(rtg, RenderPass, Subpass, Vert_Module, Frag_Module, &layout, &handle);
}

void Tutorial::BackgroundPipeline::Destroy(RTG &rtg)
{
    refsol::BackgroundPipeline_destroy(rtg, &layout, &handle);
}