#include "AssignmentOne-Vertex.hpp"

#include <array>

static std::array< VkVertexInputBindingDescription, 1 > Bindings
{
    VkVertexInputBindingDescription
    {
        .binding = 0,
        .stride = sizeof(FVertexDataSet),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }
};

static std::array< VkVertexInputAttributeDescription, 4 > Attributes
{
    VkVertexInputAttributeDescription
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(FVertexDataSet, Position),
    },
    VkVertexInputAttributeDescription
    {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(FVertexDataSet, Normal),
    },
    VkVertexInputAttributeDescription
    {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(FVertexDataSet, Tangent),
    },
    VkVertexInputAttributeDescription
    {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(FVertexDataSet, Texcoord),
    },
};

const VkPipelineVertexInputStateCreateInfo FVertexDataSet::ArrayInputState
{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = uint32_t(Bindings.size()),
    .pVertexBindingDescriptions = Bindings.data(),
    .vertexAttributeDescriptionCount = uint32_t(Attributes.size()),
    .pVertexAttributeDescriptions = Attributes.data(),
};