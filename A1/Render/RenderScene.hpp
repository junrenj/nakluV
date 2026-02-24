// URenderScene is a Object to contain all datas in the scene.
#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/quaternion.hpp"
#include "Texture.hpp"
#include "Material.hpp"
#include "Light.hpp"
#include "BBox.hpp"


// forward declarations so we can write the scene's objects in the same order as in the spec:
class URenderMesh;
class UEnvironment;
class ULight;
class UCamera;
class UNode;
struct FMeshRenderProxy;

using quat = glm::quat;
using mat4 = glm::mat4;
using vec4 = glm::vec4;
using vec2 = glm::vec2;

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
	UEnvironment* Environment = nullptr;
	ULight* Light = nullptr;
    FMeshRenderProxy* RenderProxy = nullptr;
    
    FAABB BoundingBox;      // Always Object-Coordinate


    static glm::mat4 GetLocal2WorldMatrix(const UNode* InNode);
    
    // Animation
    bool bIsDirty = false;
    void SetDirty();
};

struct FMeshRenderProxy
{
public: 
    bool bCanSee = true;
    uint32_t FirstVertexIdx = 0;
    uint32_t VertexNum = 0;
    struct FTransform
    {
        glm::mat4 CLIP_FROM_LOCAL;
        glm::mat4 WORLD_FROM_LOCAL;
        glm::mat4 WORLD_FROM_LOCAL_NORMAL;
    }Transform;
    static_assert(sizeof(FTransform) == 16*4 + 16*4 + 16*4, "Transform is the expected size.");
   
    uint32_t MaterialIdx = 0;
};

class URenderScene
{
public:

    // Nodes Tree
    UNode* RootNode;
    std::vector<UNode*> Nodes;

    // Mutual Dictionary for Node and Mesh
    std::unordered_map<UNode*, URenderMesh*> Nodes2RenderMeshes;

    // Mesh Data
    std::vector<URenderMesh*> AllMeshes;

    // Other Data
    std::vector<UTexture*> Textures;
    std::vector<FMaterial*> Materials;
    FMaterial* EnvMaterial;     // unique env in the scene
    std::vector<UCamera*> Cameras;

    // Light Data
    std::unordered_map<ULight*, UNode*> Lights;
    FLightRenderProxy* SunProxy = nullptr;
    std::unordered_map<UNode*, UEnvironment*> Environments;

    // VertexBuffer
    uint32_t TotalBytes = 0;
    std::vector<uint8_t> AllVertexData;

    // ProxyData
    std::vector<FMeshRenderProxy*> MeshProxyInstances;
    std::vector<FLightRenderProxy*> LightProxyInstances;

    void Update(uint8_t ActiveIdx, bool bEnableCull = true);

    void GenerateWholeVertexBuffer();
    void GenerateMeshProxy();
    void GenerateLightProxy();
    void UpdateTransform();
    void UpdateVisibleMesh(uint8_t ActiveIdx, bool bEnableCulling = true);

    uint32_t GetMaterialIdx(const FMaterial* InMaterial) const;

    uint32_t GetTextureIdx(UTexture* Texture) const;
    uint32_t GetDefaultWhiteTexIdx()const {return 0;}   // white -> 0
    uint32_t GetDefaultBlackTexIdx()const {return 1;}   // black -> 1
    uint32_t GetDefaultNormalTexIdx()const {return 2;}  // normal ->2
    uint32_t GetDefaultMatColTexIdx()const {return 3;}  // Magenta ->3

    uint32_t TestIdx = 0;

    URenderScene()
    {
        // Default Textures
        UTexture* DefaultWhiteTex = UTexture::GetDefaultWhiteTex();
        Textures.push_back(DefaultWhiteTex);            // white -> 0
        UTexture* DefaultBlackTex = UTexture::GetDefaultBlackTex();
        Textures.push_back(DefaultBlackTex);            // black -> 1
        UTexture* DefaultNormalTex = UTexture::GetDefaultNormalTex();
        Textures.push_back(DefaultNormalTex);           // normal ->2
        UTexture* DefaultMagentaTex = UTexture::GetDefualtFallbackColorTex();
        Textures.push_back(DefaultMagentaTex);           // Magenta ->3

        // Default Material
        Materials.push_back(FMaterial::GetDefaultMaterial());
    }
private:
};

class URenderMesh
{
public:
    VkPrimitiveTopology topology;

    uint32_t FirstVertexIdx = 0;

    uint32_t VertexCount = 0;
    uint32_t VertexStride = 0;

    uint32_t PositionOffset = UINT32_MAX;
    uint32_t NormalOffset   = UINT32_MAX;
    uint32_t TangentOffset  = UINT32_MAX;
    uint32_t UVOffset       = UINT32_MAX;

    FAABB BoundingBox;      // Object-coordinate

    struct FVertex
    {
        vec3 Position;
        vec3 Normal;
        vec4 Tangent;
        vec2 UV;
    };

    FMaterial* Material;

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
