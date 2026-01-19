#include "PosColVertex.hpp"

#include <array>

static std::array< VkVertexInputBindingDescription, 1 > Bindings
{
    VkVertexInputBindingDescription
    {
        .binding = 0,
        .stride = sizeof(PosColVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }
};

static std::array< VkVertexInputAttributeDescription, 2 > Attributes
{
    VkVertexInputAttributeDescription
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(PosColVertex, Position),
    },
    VkVertexInputAttributeDescription
    {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .offset = offsetof(PosColVertex, Color),
    },
};

const VkPipelineVertexInputStateCreateInfo PosColVertex::ArrayInputState
{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = uint32_t(Bindings.size()),
    .pVertexBindingDescriptions = Bindings.data(),
    .vertexAttributeDescriptionCount = uint32_t(Attributes.size()),
    .pVertexAttributeDescriptions = Attributes.data(),
};