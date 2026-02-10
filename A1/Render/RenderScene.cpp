#include "RenderScene.hpp"
#include "glm/glm/gtc/matrix_transform.hpp"

void URenderScene::GenerateWholeVertexBuffer()
{
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

void URenderScene::GenerateMeshProxy()
{
    TotalBytes = 0;
    uint32_t Index = 0;
    const uint32_t BytePerVertex = static_cast<uint32_t>(sizeof(URenderMesh::FVertex));
    for (auto& Mesh : AllMeshes)
    {
        UMeshRenderProxy* ProxyInstance = new UMeshRenderProxy();
        const UNode* BindingNode = RenderMeshes2Nodes.find(Mesh)->second;
        const uint32_t MeshBytesCount = static_cast<uint32_t>(Mesh->VertexData.size());
        // Data Collect
        ProxyInstance->FirstVertexIdx = Index;
        ProxyInstance->VertexNum = MeshBytesCount / BytePerVertex;
        ProxyInstance->Transform.WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(BindingNode);
        ProxyInstance->Transform.WORLD_FROM_LOCAL_NORMAL = UNode::GetLocal2WorldMatrix(BindingNode);
        // TODO: Add more Textures and also separate logic
        if(Mesh->Material->Type == EMaterialType::Environment)
        {
            ProxyInstance->Texture = 0; //TODO: Solve Env
        }
        else
        {
            if(Mesh->Material->AlbedoTex == INVALID_TEXTURE)
            {
                // for god sake, just make a 1x1 pixel texture stand for albedo
                UTexture* NewTexture = new UTexture();
                NewTexture->Get1x1PixelTexture(Mesh->Material->Albedo.r, Mesh->Material->Albedo.g, Mesh->Material->Albedo.b);
                Textures.push_back(NewTexture);
                ProxyInstance->Texture = static_cast<uint32_t>(Textures.size() - 1);
            }
            else
            {
                ProxyInstance->Texture = Mesh->Material->AlbedoTex;
            }
        }

        Mesh->RenderProxy = ProxyInstance;
        MeshProxyInstances.push_back(ProxyInstance);

        TotalBytes += MeshBytesCount;
        Index += MeshBytesCount / BytePerVertex;
    }
}

void URenderScene::GenerateLightProxy()
{
    //TODO: Finish other kind of light
    for (auto const& [Light, Node] : Lights)
    {
        ULightRenderProxy* LightProxy = new ULightRenderProxy();
        switch (Light->LightType)
        {
            case ELightType::Sun:
            {
                const ULight_Sun* Sun = static_cast<ULight_Sun*>(Light); 
                LightProxy->Type = static_cast<uint32_t>(ELightType::Sun);
                LightProxy->Position = glm::vec3(0,0,0);
                LightProxy->Color = glm::vec3(Sun->Tint.r * Sun->Strength, Sun->Tint.g * Sun->Strength, Sun->Tint.b * Sun->Strength);

                const glm::vec4 LocalDir = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
                glm::vec4 WorldDir = UNode::GetLocal2WorldMatrix(Node) * LocalDir;
                LightProxy->Direction = glm::vec3(WorldDir);
                break;
            }
            case ELightType::Sphere:
            {
                // const ULight_Sphere* Sphere = static_cast<ULight_Sphere*>(Light); 
                // LightProxy->Type = static_cast<uint32_t>(ELightType::Sphere);
                break;

            }
            case ELightType::Spot:
            {
                // LightProxy->Type = static_cast<uint32_t>(ELightType::Spot);
                break;
            }
        }
        LightProxyInstances.push_back(LightProxy);
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