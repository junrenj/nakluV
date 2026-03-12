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
            const glm::mat4 WORLD_FROM_LOCAL = UNode::GetLocal2WorldMatrix(Node);
            ProxyInstance->Transform.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL;
            ProxyInstance->Transform.WORLD_FROM_LOCAL_NORMAL = glm::transpose(glm::inverse(WORLD_FROM_LOCAL));
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

                const vec4 LocalDir = vec4(0.0f, 0.0f, 1.0f, 0.0f);
                const vec4 WorldDir = UNode::GetLocal2WorldMatrix(Node) * LocalDir;
                const vec3 Color = Sun->Tint * Sun->Strength;

                LightProxy->Position_Type = vec4(0, 0, 0, static_cast<float>(ELightType::Sun));
                LightProxy->Color_Falloff = vec4(Color.x, Color.y, Color.z, 0);
                LightProxy->Direction_Limit = vec4(WorldDir.x, WorldDir.y, WorldDir.z, 0.0f);

                LightProxy->SpecialParams.x = Sun->Angle;

                SunProxy = LightProxy;
                break;
            }
            case ELightType::Sphere:
            {
                const ULight_Sphere* Sphere = static_cast<ULight_Sphere*>(Light);

                const vec4 LocalPos = vec4(0.0f);
                const vec4 WorldPos = UNode::GetLocal2WorldMatrix(Node) * LocalPos;
                const vec3 Color = Sphere->Tint * Sphere->Power;
                
                LightProxy->Position_Type = vec4(WorldPos.x, WorldPos.y, WorldPos.z, static_cast<float>(ELightType::Sphere));
                LightProxy->Color_Falloff = vec4(Color.x, Color.y, Color.z, 0);
                LightProxy->Direction_Limit = vec4(0.0f, 0.0f, 0.0f, Sphere->Limit);

                const float SourceRadius = Sphere->Radius;
                LightProxy->SpecialParams.x = SourceRadius;

                LightProxyInstances.push_back(LightProxy);
                break;

            }
            case ELightType::Spot:
            {
                const ULight_Spot* Spot = static_cast<ULight_Spot*>(Light);

                const vec4 LocalPos = vec4(0.0f);
                const vec4 WorldPos = UNode::GetLocal2WorldMatrix(Node) * LocalPos;
                const vec3 Color = Spot->Tint * Spot->Power;
                
                LightProxy->Position_Type = vec4(WorldPos.x, WorldPos.y, WorldPos.z, static_cast<float>(ELightType::Spot));
                LightProxy->Color_Falloff = vec4(Color.x, Color.y, Color.z, 0);
                LightProxy->Direction_Limit = vec4(0.0f, 0.0f, 0.0f, Spot->Limit);

                const float cosInner = cos(Spot->Fov * (1.0f - Spot->Blend));
                const float cosOuter = cos(Spot->Fov);
                LightProxy->SpecialParams.x = cosInner;
                LightProxy->SpecialParams.y = cosOuter;

                LightProxyInstances.push_back(LightProxy);
                break;
            }
        }
    }

    if(SunProxy == nullptr)
    {
        // give a default - dark sun
        FLightRenderProxy* LightProxy = new FLightRenderProxy();
        SunProxy = LightProxy;
        const float Type = static_cast<uint32_t>(ELightType::Sun);

        SunProxy->Position_Type = vec4(0,0,0, Type);
        SunProxy->Color_Falloff = vec4(1,1,1,0);
        SunProxy->Direction_Limit = vec4(0,0.7,0.7,0);
    }
}

void URenderScene::GenerateFallbackResource()
{
    // 0. Give a fallback env
    if(Environments.size() < 1)
    {
        UEnvironment* Env = new UEnvironment();
        Env->EnvTexture = GetDefaultBlackTexIdx();
        Environments.push_back(Env);
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