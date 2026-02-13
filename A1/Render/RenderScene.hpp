// URenderScene is a Object to contain all datas in the scene.
#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include "s72Loader/S72.hpp"
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/quaternion.hpp"
#include "Texture.hpp"
#include "Material.hpp"
#include "Light.hpp"


// forward declarations so we can write the scene's objects in the same order as in the spec:
class URenderMesh;
class UEnvironment;
class ULight;
class UCamera;
class UNode;

using vec3 = glm::vec3;
using quat = glm::quat;
using mat4 = glm::mat4;
using vec4 = glm::vec4;
using vec2 = glm::vec2;

class UBoundingBox
{
public:
    vec3 Min;
    vec3 Max;
};

class UNode
{
public:
    struct FTransform
    {
        vec3 Translation = vec3(0.0f, 0.0f, 0.0f);
        quat Rotation = quat(0.0f, 0.0f , 0.0f, 1.0f);
        vec3 Scale = vec3(1.0f, 1.0f, 1.0f);
    }Transform;

    UNode* Parent = nullptr;    // If a node no Parent, which means it is root
    std::vector< UNode * > Children;
    URenderMesh* Mesh = nullptr;
	UEnvironment* environment = nullptr;
	ULight* light = nullptr;

    UBoundingBox BoundingBox;

    static glm::mat4 GetLocal2WorldMatrix(const UNode* InNode);
};

struct FMeshRenderProxy
{
public: 
    uint32_t FirstVertexIdx = 0;
    uint32_t VertexNum = 0;
    struct FTransform
    {
        glm::mat4 CLIP_FROM_LOCAL;
        glm::mat4 WORLD_FROM_LOCAL;
        glm::mat4 WORLD_FROM_LOCAL_NORMAL;
    }Transform;
    static_assert(sizeof(FTransform) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");
   
    uint32_t Texture = 0;
};

class URenderScene
{
public:

    // Nodes Tree
    UNode* RootNode;
    std::vector<UNode*> Nodes;

    // Mutual Dictionary for Node and Mesh
    std::unordered_map<UNode*, URenderMesh*> Nodes2RenderMeshes;
    std::unordered_map<URenderMesh*, UNode*> RenderMeshes2Nodes;

    // Mesh Data
    std::vector<URenderMesh*> AllMeshes;
    std::unordered_map<UNode*, URenderMesh*> VisibleRenderMeshes;

    // Other Data
    std::vector<UTexture*> Textures;
    std::vector<UMaterial*> Materials;
    std::vector<UCamera*> Cameras;
    std::unordered_map<ULight*, UNode*> Lights;
    std::unordered_map<UNode*, UEnvironment*> Environments;

    // VertexBuffer
    uint32_t TotalBytes = 0;
    std::vector<uint8_t> AllVertexData;

    // ProxyData
    std::vector<FMeshRenderProxy*> MeshProxyInstances;
    std::vector<FLightRenderProxy*> LightProxyInstances;

    void GenerateWholeVertexBuffer();
    void GenerateMeshProxy();
    void GenerateLightProxy();
    void UpdateVisibleMesh();

    uint32_t GetTextureIdx(UTexture* Texture) const;

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
        vec3 Position;
        vec3 Normal;
        vec4 Tangent;
        vec2 UV;
    };

    UMaterial* Material;

    std::vector<uint8_t> VertexData;    // Include data of POSITION,NORMAL,TANGENT,UV
    FMeshRenderProxy* RenderProxy;
};

class UCamera
{
public:
    std::string Name;
    UNode* BindingNode;
    struct FPerspective
    {
        float Aspect;
		float Vfov;
		float Near;
		float Far = std::numeric_limits< float >::infinity();
    }Projection;
};

class UEnvironment
{
public:
    uint32_t EnvTexture = INVALID_TEXTURE;
};
