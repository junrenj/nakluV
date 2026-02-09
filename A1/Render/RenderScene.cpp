#include "RenderScene.hpp"
#include "glm/glm/gtc/matrix_transform.hpp"

void URenderScene::GenerateWholeVertexBuffer()
{
    TotalBytes = 0;
    int Index = 0;
    const uint32_t BytePerVertex = static_cast<uint32_t>(sizeof(URenderMesh::FVertex));
    for (auto& Mesh : AllMeshes)
    {
        URenderProxy* ProxyInstance = new URenderProxy();
        const uint32_t MeshBytesCount = static_cast<uint32_t>(Mesh->VertexData.size());
        ProxyInstance->FirstVertexIdx = Index;
        ProxyInstance->VertexNum = MeshBytesCount / BytePerVertex;
        const UNode* BindingNode = RenderMeshes2Nodes.find(Mesh)->second;
        ProxyInstance->Transform.WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(BindingNode);

        Mesh->RenderProxy = ProxyInstance;
        ProxyInstances.push_back(ProxyInstance);

        TotalBytes += MeshBytesCount;
        Index += MeshBytesCount / BytePerVertex;
    }

    // Create Vertex Buffer CPU Data
    StagingData.reserve(TotalBytes);
    uint32_t BytesOffset = 0;
    for (const auto& Mesh : AllMeshes) 
    {
        std::memcpy(
            StagingData.data() + BytesOffset, 
            Mesh->VertexData.data(), 
            Mesh->VertexData.size()
        );
        BytesOffset += static_cast<uint32_t>(Mesh->VertexData.size());
    }
}

void URenderScene::UpdateVisibleMesh()
{

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