#include "RenderScene.hpp"
#include "glm/glm/gtc/matrix_transform.hpp"

void URenderScene::GenerateWholeVertexBuffer()
{
    TotalBytes = 0;
    uint32_t Index = 0;
    const uint32_t BytePerVertex = static_cast<uint32_t>(sizeof(URenderMesh::FVertex));
    for (auto& Mesh : AllMeshes)
    {
        URenderProxy* ProxyInstance = new URenderProxy();
        const UNode* BindingNode = RenderMeshes2Nodes.find(Mesh)->second;
        const uint32_t MeshBytesCount = static_cast<uint32_t>(Mesh->VertexData.size());
        // Data Collect
        ProxyInstance->FirstVertexIdx = Index;
        ProxyInstance->VertexNum = MeshBytesCount / BytePerVertex;
        ProxyInstance->Transform.WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(BindingNode);
        // TODO: Add more Textures and also separate logic
        ProxyInstance->Texture = 0;//Mesh->Material->AlbedoTex;

        Mesh->RenderProxy = ProxyInstance;
        ProxyInstances.push_back(ProxyInstance);

        TotalBytes += MeshBytesCount;
        Index += MeshBytesCount / BytePerVertex;
    }

    // Create Vertex Buffer CPU Data
    AllVertexData.reserve(TotalBytes);
    uint32_t BytesOffset = 0;
    for (const auto& Mesh : AllMeshes) 
    {
        std::memcpy(
            AllVertexData.data() + BytesOffset, 
            Mesh->VertexData.data(), 
            Mesh->VertexData.size()
        );
        BytesOffset += static_cast<uint32_t>(Mesh->VertexData.size());
    }
}

void URenderScene::UpdateVisibleMesh()
{

}

uint32_t URenderScene::GetTextureIdx(UTexture* Texture) const
{
    if(!Texture)
    {
        return INVALID_TEXTURE;
    }
    auto It = std::find(Textures.begin(), Textures.end(), Texture);

    if(It != Textures.end())
    {
        // find it
        return static_cast<uint32_t>(std::distance(Textures.begin(), It));
    }
    else
    {
        return INVALID_TEXTURE;
    }
}

glm::mat4 UNode::GetLocal2WorldMatrix(const UNode* InNode)
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), InNode->Transform.Translation);
    glm::mat4 R = glm::mat4_cast(InNode->Transform.Rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), InNode->Transform.Scale);
    
    glm::mat4 LocalMatrix = T * R * S;

    if(InNode->Parent == nullptr)
    {
        return LocalMatrix;
    }
    else
    {
        return GetLocal2WorldMatrix(InNode->Parent) * LocalMatrix;
    }
}