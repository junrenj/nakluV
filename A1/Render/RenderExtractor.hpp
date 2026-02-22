// The struct is to translate the data from s72 to data used at CPU
#pragma once

#include "s72Loader/S72.hpp"
#include "RenderScene.hpp"

class UAnimInstance;

class URenderExtractor
{
public:
    static void BuildRenderScene(std::string S72Path, URenderScene& RenderScene);

    // New Node Tree
    static void BuildUNodeTree(URenderScene& RenderScene, 
        std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, 
        std::unordered_map< const S72::Material* , FMaterial* >& S72Mat2UMat, 
        std::unordered_map< const S72::Node* , UNode* >& S72Node2UNode,
        std::unordered_map< const S72::Mesh* , URenderMesh* >& S72Mesh2UMesh);
    static UNode* BuildUNodeTreeIterate(const S72::Node& InS72Nodes, URenderScene& RenderScene, UNode* Parent,
        std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, 
        std::unordered_map< const S72::Material* , FMaterial* >& S72Mat2UMat, 
        std::unordered_map< const S72::Node* , UNode* >& S72Node2UNode,
        std::unordered_map< const S72::Mesh* , URenderMesh* >& S72Mesh2UMesh);
    // Bounding Box
    static void BuildUNodesBBoxIterate(UNode* InNode, 
        const std::unordered_map<UNode*, URenderMesh*>& Nodes2RenderMeshes);
    // New Material Structure
    static void BuildUMaterialData(std::unordered_map< const S72::Material* , FMaterial* >& S72Mat2UMat, 
        std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex , URenderScene& Scene);
    static FMaterial* CloneUMaterialFromS72Material(const S72::Material& InS72Mat, 
        std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, URenderScene& Scene);
    // New Texture Structure
    static void BuildUTextureData(std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, URenderScene& Scene);
    static UTexture* ReadBulkDataFromImage(const S72::Texture& InTexture);

    // New RenderMesh Structure
    static void BuildUMeshData(URenderScene& Scene,
                                std::unordered_map< const S72::Mesh* , URenderMesh* >& S72Mesh2UMesh, 
                                std::unordered_map< const S72::Material* , FMaterial* >& S72Mat2UMat);
    static void CloneRenderMeshFromS72Mesh(const S72::Mesh& InMesh, URenderMesh& OutMesh, 
        std::unordered_map< const S72::Material* , FMaterial* >& S72Mat2UMat);

    // New Animation Structure
    static void BuildAnimData(std::unordered_map< const S72::Node* , UNode* >& S72Node2UNode);
    static void CloneAnimFromS72Anim(const S72::Driver& Driver, UAnimInstance& AnimInstance);
private:
    static std::vector<uint8_t> ReadBinaryFile(const std::string& path);
private:
    inline static S72 CurrentS72;
};
