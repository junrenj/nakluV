#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"

void Tutorial::BackgroundPipeline::Create(RTG &rtg, VkRenderPass RenderPass, uint32_t Subpass)
{
    VkShaderModule Vert_Module = VK_NULL_HANDLE;
    VkShaderModule Frag_Module = VK_NULL_HANDLE;

    refsol::BackgroundPipeline_create(rtg, RenderPass, Subpass, Vert_Module, Frag_Module, &layout, &handle);
}

void Tutorial::BackgroundPipeline::Destroy(RTG &rtg)
{
    refsol::BackgroundPipeline_destroy(rtg, &layout, &handle);
}