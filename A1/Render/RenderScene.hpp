#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include "s72Loader/S72.hpp"
#include "glm/glm/glm.hpp"


// forward declarations so we can write the scene's objects in the same order as in the spec:
class URenderMesh;
class UMaterial;
class UEnvironment;
class ULight;
class UCamera;
class UNode;


class UBoundingBox
{
public:
    glm::vec3 Min;
    glm::vec3 Max;
};

class UNode
{
public:
    struct FTransform
    {
        glm::vec3 Translation;
        glm::vec4 Rotation;
        glm::vec3 Scale;
    }Transform;

    std::vector< UNode * > Children;
    URenderMesh* Mesh = nullptr;
	UCamera* camera = nullptr;
	UEnvironment* environment = nullptr;
	ULight* light = nullptr;

    UBoundingBox BoundingBox;
};

class URenderScene
{
public:

    std::unordered_map<UNode*, URenderMesh*> RenderMeshes;
    std::unordered_map<UNode*, URenderMesh*> VisibleRenderMeshes;

    // Nodes Tree
    UNode* RootNode;
    std::vector<UNode*> Nodes;

    std::unordered_map<UNode*, UCamera*> Cameras;
    std::unordered_map<UNode*, UMaterial*> Materials;
    std::unordered_map<UNode*, ULight*> Lights;
    UEnvironment* Environment;
};

class URenderMesh
{
public:
    VkPrimitiveTopology topology;

    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    uint32_t VertexStride = 0;

    uint32_t PositionOffset = UINT32_MAX;
    uint32_t NormalOffset   = UINT32_MAX;
    uint32_t TangentOffset  = UINT32_MAX;
    uint32_t UVOffset       = UINT32_MAX;

    UBoundingBox BoundingBox;

    struct FVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent;
        glm::vec2 UV;
    };

    UMaterial* Material;

    std::vector<uint8_t> VertexData;    // Include data of POSITION,NORMAL,TANGENT,UV
};

class UMaterial
{

};

class UCamera
{

};

class UEnvironment
{

};

class ULight
{

};
