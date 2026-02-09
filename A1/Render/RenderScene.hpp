#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include "s72Loader/S72.hpp"
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/quaternion.hpp"
#include "Texture.hpp"


// forward declarations so we can write the scene's objects in the same order as in the spec:
class URenderMesh;
class UMaterial;
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
	UCamera* camera = nullptr;
	UEnvironment* environment = nullptr;
	ULight* light = nullptr;

    UBoundingBox BoundingBox;

    static glm::mat4 GetLocal2WorldMatrix(const UNode* InNode);
};

class URenderProxy
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

    std::unordered_map<UNode*, URenderMesh*> Nodes2RenderMeshes;
    std::unordered_map<URenderMesh*, UNode*> RenderMeshes2Nodes;
    std::vector<URenderMesh*> AllMeshes;
    std::unordered_map<UNode*, URenderMesh*> VisibleRenderMeshes;

    // Nodes Tree
    UNode* RootNode;
    std::vector<UNode*> Nodes;

    std::unordered_map<UNode*, UCamera*> Cameras;
    std::unordered_map<UNode*, UMaterial*> Materials;
    std::unordered_map<UNode*, ULight*> Lights;
    std::unordered_map<UNode*, UEnvironment*> Environments;

    // VertexBuffer
    uint32_t TotalBytes = 0;
    std::vector<uint8_t> VertexBuffer;
    std::vector<uint8_t> StagingData;

    // ProxyData
    std::vector<URenderProxy*> ProxyInstances;

    void GenerateWholeVertexBuffer();
    void UpdateVisibleMesh();
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
    URenderProxy* RenderProxy;
};

class UMaterial
{
    std::vector<UTexture*> TextureSet;
};

class UCamera
{
public:
    struct FPerspective
    {
        float Aspect;
		float Vfov;
		float Near;
		float Far = std::numeric_limits< float >::infinity();
    };

    std::variant< FPerspective > Projection;
};

class UEnvironment
{
    UTexture* EnvTexture;
};

class ULight
{
public:
    uint32_t shadow = 0; //optional, if not set will be '0'
    struct FColor
    {
        float r, g, b = 1.0f;
    }Tint;
    
    //light has exactly one of these sources:
    struct FSun {
        float Angle;
        float Strength;
    };
    struct FSphere {
        float Radius;
        float Power;
        float Limit = std::numeric_limits< float >::infinity(); //optional, will be infinity if not specified
    };
    struct FSpot {
        float Radius;
        float Power;
        float Limit = std::numeric_limits< float >::infinity(); //optional, will be infinity if not specified
        float Fov;
        float Blend;
    };
    std::variant< FSun, FSphere, FSpot > Source;
};
