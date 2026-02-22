#include "RenderScene.hpp"
#include "CullingUtils.hpp"
#include "glm/glm/gtc/matrix_transform.hpp"

void URenderScene::GenerateWholeVertexBuffer()
{
    // Create Vertex Buffer CPU Data

    AllVertexData.clear();
    AllVertexData.reserve(TotalBytes);

    for (const auto& Mesh : AllMeshes)
    {
        AllVertexData.insert(
            AllVertexData.end(),
            Mesh->VertexData.begin(),
            Mesh->VertexData.end()
        );
    }
}

void URenderScene::GenerateMeshProxy()
{
    TotalBytes = 0;
    uint32_t Index = 0;
    const uint32_t BytePerVertex = static_cast<uint32_t>(sizeof(URenderMesh::FVertex));

    for (URenderMesh* Mesh : AllMeshes)
    {
        const uint32_t MeshBytesCount = static_cast<uint32_t>(Mesh->VertexData.size());

        TotalBytes += MeshBytesCount;
        Mesh->FirstVertexIdx = Index;
        Index += MeshBytesCount / BytePerVertex;
    }

    for (auto& Node : Nodes)
    {
        auto it = Nodes2RenderMeshes.find(Node);
        if (it != Nodes2RenderMeshes.end())
        {
            URenderMesh* RenderMesh = it->second;
            FMeshRenderProxy* ProxyInstance = new FMeshRenderProxy();
            // Vertices
            ProxyInstance->FirstVertexIdx = RenderMesh->FirstVertexIdx;
            ProxyInstance->VertexNum = static_cast<uint32_t>(RenderMesh->VertexData.size()) / BytePerVertex;
            // Transform
            ProxyInstance->Transform.WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(Node);
            ProxyInstance->Transform.WORLD_FROM_LOCAL_NORMAL = UNode::GetLocal2WorldMatrix(Node);
            // Material
            ProxyInstance->MaterialIdx = GetMaterialIdx(RenderMesh->Material);

            Node->RenderProxy = ProxyInstance;
            MeshProxyInstances.push_back(ProxyInstance);
        }
    }
}

void URenderScene::GenerateLightProxy()
{
    for (auto const& [Light, Node] : Lights)
    {
        FLightRenderProxy* LightProxy = new FLightRenderProxy();
        switch (Light->LightType)
        {
            case ELightType::Sun:
            {
                const ULight_Sun* Sun = static_cast<ULight_Sun*>(Light);
                LightProxy->Type = static_cast<uint32_t>(ELightType::Sun);
                LightProxy->Position = vec3(0,0,0);
                LightProxy->Color = vec3(Sun->Tint.r * Sun->Strength, Sun->Tint.g * Sun->Strength, Sun->Tint.b * Sun->Strength);

                const vec4 LocalDir = vec4(0.0f, 0.0f, 1.0f, 0.0f);
                vec4 WorldDir = UNode::GetLocal2WorldMatrix(Node) * LocalDir;
                LightProxy->Direction = vec3(WorldDir);
                SunProxy = LightProxy;
                break;
            }
            case ELightType::Sphere:
            {
                // FLightRenderProxy* LightProxy = new FLightRenderProxy();
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
    }

    if(SunProxy == nullptr)
    {
        // give a default - dark sun
        FLightRenderProxy* LightProxy = new FLightRenderProxy();
        SunProxy = LightProxy;
        SunProxy->Type = static_cast<uint32_t>(ELightType::Sun);
        SunProxy->Position = vec3(0,0,0);
        SunProxy->Color = vec3(0,0,0);
        const vec4 Dir = vec4(0.0f, 0.0f, 0.0f, 0.0f);
        SunProxy->Direction = vec3(Dir);
    }
}

void URenderScene::UpdateVisibleMesh(uint8_t ActiveIdx, bool bEnableCulling)
{
    if(bEnableCulling)
    {
        const UCamera& ActiveCamera = *Cameras[ActiveIdx];
        const mat4 VIEW_FROM_WORLD = glm::inverse(UNode::GetLocal2WorldMatrix(ActiveCamera.BindingNode));
        float TanFov = tan(0.5f * ActiveCamera.Projection.Vfov);
        FCullingFrustum Frustum = 
        {
            .NearRight = ActiveCamera.Projection.Aspect * ActiveCamera.Projection.Near * TanFov,
            .NearTop = ActiveCamera.Projection.Near * TanFov,
            .NearPlane = -ActiveCamera.Projection.Near,
            .FarPlane = -ActiveCamera.Projection.Far,
        };

        for (uint32_t i = 0; i < Nodes.size(); i++)
        {
            UNode* Node = Nodes[i];
            FMeshRenderProxy* RenderProxy = Node->RenderProxy;
            if(RenderProxy)
            {
                const URenderMesh* Mesh = Nodes2RenderMeshes[Node];
                const FAABB& AABB = Mesh->BoundingBox;
                const mat4 Transform = VIEW_FROM_WORLD * UNode::GetLocal2WorldMatrix(Node);
            
                const bool bCanSee = UCullingUtils::SATVisibilityTest(Frustum, Transform, AABB);
                RenderProxy->bCanSee = bCanSee;
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < Nodes.size(); i++)
        {
            UNode* Node = Nodes[i];
            FMeshRenderProxy* RenderProxy = Node->RenderProxy;
            if(RenderProxy)
            {
                RenderProxy->bCanSee = true;
            }
        }
    }
}

uint32_t URenderScene::GetMaterialIdx(const FMaterial* InMaterial) const
{
    auto It = std::find(Materials.begin(), Materials.end(), InMaterial);

    if (It != Materials.end()) 
    {
        return static_cast<uint32_t>(std::distance(Materials.begin(), It));
    } 
    else 
    {
        return 0;
    }
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

void URenderScene::Update(uint8_t ActiveIdx, bool bEnableCull)
{
    UpdateTransform();
    UpdateVisibleMesh(ActiveIdx, bEnableCull);
}

void URenderScene::UpdateTransform()
{
    for (UNode* Node : Nodes)
    {
        if(Node->bIsDirty)
        {
            if(Node->RenderProxy)
            {
                Node->RenderProxy->Transform.WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(Node);
            }
        }
    }
}

void UNode::SetDirty()
{
    bIsDirty = true;
    for (UNode* Child : Children)
    {
        Child->SetDirty();
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