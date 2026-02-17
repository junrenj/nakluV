#include "DebugView.hpp"
#include "../../mat4.hpp"
#include <iomanip>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm/gtx/string_cast.hpp"
#include "glm/glm/gtc/type_ptr.hpp"

// Debug Scene Class
void UDebugScene::Update(const std::vector< UCamera* >& Cameras, uint8_t ActiveIdx, const std::vector< UNode * >& Nodes)
{
    UpdateBBoxVertices(Nodes);
    UpdateFrustumVertices(Cameras, ActiveIdx);
}

void UDebugScene::GenerateBBoxVertices(const FAABB& BBox, const glm::mat4& Transform, const bool bCanSee)
{
    float x0 = BBox.Min.x, y0 = BBox.Min.y, z0 = BBox.Min.z;
    float x1 = BBox.Max.x, y1 = BBox.Max.y, z1 = BBox.Max.z;
    vec4 v[8] = 
    {
        {x0, y0, z0, 1}, {x1, y0, z0, 1}, {x1, y1, z0, 1}, {x0, y1, z0, 1}, // Bottom
        {x0, y0, z1, 1}, {x1, y0, z1, 1}, {x1, y1, z1, 1}, {x0, y1, z1, 1} 	// Top
    };

    uint32_t Indices[] = 
    {
        0, 1, 1, 2, 2, 3, 3, 0, // bottom 4 edges
        4, 5, 5, 6, 6, 7, 7, 4, // top 4 edges
        0, 4, 1, 5, 2, 6, 3, 7  // vertical 4 edges
    };

    for (uint32_t Idx : Indices) 
    {
        FDebugColVertex Vertex;
        vec4 WorldPos = Transform * v[Idx];
        Vertex.Position = { WorldPos.x, WorldPos.y, WorldPos.z };
        const uint8_t (&Color)[4] = bCanSee ? BBoxColorVisible : BBoxColorCull;
        Vertex.Color.r = Color[0];
        Vertex.Color.g = Color[1];
        Vertex.Color.b = Color[2];
        Vertex.Color.a = Color[3];
        BBoxVertices.emplace_back(Vertex);
    }
}

void UDebugScene::GenerateFrustumVertices(const UCamera& Camera, bool bIsActive)
{
    // TODO: replace all Mat4 with mat4
    mat4 Projection = Perspective
		(
			Camera.Projection.Vfov,
			Camera.Projection.Aspect,
			Camera.Projection.Near,
			Camera.Projection.Far
		);
	mat4 View = glm::inverse(UNode::GetLocal2WorldMatrix(Camera.BindingNode));
	mat4 ClipFromWorld = Projection * View;
	mat4 WorldFromClip = glm::inverse(ClipFromWorld);

	std::vector<glm::vec4> NDCCorners = 
	{
		{-1.0f, -1.0f,  0.0f, 1.0f}, { 1.0f, -1.0f,  0.0f, 1.0f},
		{ 1.0f,  1.0f,  0.0f, 1.0f}, {-1.0f,  1.0f,  0.0f, 1.0f},
		{-1.0f, -1.0f,  1.0f, 1.0f}, { 1.0f, -1.0f,  1.0f, 1.0f},
		{ 1.0f,  1.0f,  1.0f, 1.0f}, {-1.0f,  1.0f,  1.0f, 1.0f}
	};

	std::vector<glm::vec3> WorldCorners;
	for (const auto& pt : NDCCorners) 
	{
		glm::vec4 WorldPt = WorldFromClip * pt;
    	WorldCorners.push_back(glm::vec3(WorldPt) / WorldPt.w);
	}

	uint32_t Indices[] = 
	{
        0, 1, 1, 2, 2, 3, 3, 0, // bottom 4 edges
        4, 5, 5, 6, 6, 7, 7, 4, // top 4 edges
        0, 4, 1, 5, 2, 6, 3, 7  // vertical 4 edges
    };

	for (uint32_t i : Indices) 
	{
        FDebugColVertex vertex;
        vertex.Position = { WorldCorners[i].x, WorldCorners[i].y, WorldCorners[i].z };
        const uint8_t (&Color)[4] = bIsActive ? FrustumColorActive : FrustumColorInActive;
        vertex.Color.r = Color[0];
		vertex.Color.g = Color[1];
		vertex.Color.b = Color[2];
		vertex.Color.a = Color[3];
        FrustumVertices.emplace_back(vertex);
    }
}

void UDebugScene::UpdateBBoxVertices(const std::vector< UNode * >& Nodes)
{
    BBoxVertices.clear();
    for (const UNode* Node : Nodes)
    {
        const FAABB& BBox = Node->BoundingBox;
        if(Node->Mesh && BBox.Max != vec3(-FLT_MAX) && BBox.Min != vec3(FLT_MAX))
        {
            const FMeshRenderProxy* Proxy = Node->Mesh->RenderProxy;
			const glm::mat4 Transform = UNode::GetLocal2WorldMatrix(Node);
            GenerateBBoxVertices(BBox, Transform, Proxy->bCanSee);
        }
    }
}

void UDebugScene::UpdateFrustumVertices(const std::vector< UCamera* >& Cameras, uint8_t ActiveIdx)
{
    FrustumVertices.clear();
    for (uint8_t i = 0; i < Cameras.size(); i++)
    {
        GenerateFrustumVertices(*Cameras[i], ActiveIdx == i);
    }
}

void UDebugScene::GetAllVerticesData(std::vector<FDebugColVertex>& LineVertices)
{
    LineVertices.reserve(BBoxVertices.size() + FrustumVertices.size());
    LineVertices.insert(LineVertices.end(), BBoxVertices.begin(), BBoxVertices.end());
    LineVertices.insert(LineVertices.end(), FrustumVertices.begin(), FrustumVertices.end());
}

// Debug Message Class
void UDebugMessage::PrintRenderSceneMesh(const URenderScene& Scene)
{
    std::cout << "======= Render Scene Mesh Debug Info =======" << std::endl;
    
    int MeshIndex = 0;
    for (const auto& [NodePtr, Mesh] : Scene.Nodes2RenderMeshes) 
    {
        std::cout << "\n[Mesh #" << MeshIndex++ << "]" << std::endl;
        
        
        std::cout << "Bounding Box Min: " << glm::to_string(Mesh->BoundingBox.Min) << std::endl;
        std::cout << "Bounding Box Max: " << glm::to_string(Mesh->BoundingBox.Max) << std::endl;
        
        
        std::cout << "Vertex Count: " << Mesh->VertexCount << std::endl;
        
        
        const uint8_t* DataPtr = Mesh->VertexData.data();
        
        for (uint32_t i = 0; i < Mesh->VertexCount; ++i) 
        {
            size_t Offset = i * Mesh->VertexStride;
            
            const auto* Vertex = reinterpret_cast<const URenderMesh::FVertex*>(DataPtr + Offset);
            
            if (i < 5 || i == Mesh->VertexCount - 1) { 
                std::cout << "  Vertex [" << i << "] Position: " 
                          << glm::to_string(Vertex->Position) << std::endl;
            } else if (i == 5) {
                std::cout << "  ... (skipping intermediate vertices) ..." << std::endl;
            }
        }
    }
    std::cout << "============================================" << std::endl;
}

void UDebugMessage::PrintRenderProxies(const URenderScene& Scene)
{
   std::cout << "=== Render Scene Proxies (Count: " << Scene.MeshProxyInstances.size() << ") ===" << std::endl;
    for (size_t i = 0; i < Scene.MeshProxyInstances.size(); ++i) 
    {
        const auto& p = Scene.MeshProxyInstances[i];
        std::cout << "Proxy [" << i << "]:" << std::endl;
        std::cout << "  - Vertex: StartIdx=" << p->FirstVertexIdx << ", Count=" << p->VertexNum << std::endl;
        std::cout << "  - Texture ID: " << p->Texture << std::endl;
        
        PrintMatrix("WORLD_FROM_LOCAL", p->Transform.WORLD_FROM_LOCAL);
        std::cout << "-----------------------------------------------" << std::endl;
    }
}

void UDebugMessage::PrintMatrix(const std::string& Name, const glm::mat4& Mat)
{
    std::cout << "    " << Name << ":" << std::endl;
    for (int i = 0; i < 4; ++i) 
    {
        std::cout << "      [ ";
        for (int j = 0; j < 4; ++j) 
        {
            std::cout << std::setw(8) << std::fixed << std::setprecision(2) << Mat[j][i] << " ";
        }
        std::cout << "]" << std::endl;
    }
}

std::string UDebugMessage::FormatTexIdx(uint32_t idx)
{
    if (idx == INVALID_TEXTURE)
    {
        return "[None]";
    }
    return std::to_string(idx);
}

std::string UDebugMessage::MaterialToString(EMaterialType Type)
{
    switch (Type) 
    {
        case EMaterialType::PBR:         return "PBR";
        case EMaterialType::Lambertian:  return "Lambertian";
        case EMaterialType::Mirror:      return "Mirror";
        case EMaterialType::Environment: return "Environment";
        default:                         return "Unknown";
    }
}

void UDebugMessage::PrintMaterial(const URenderScene& Scene)
{
    const std::vector<UMaterial*>& Materials = Scene.Materials;
    std::cout << "\n========== Render Scene Materials (Count: " << Materials.size() << ") ==========\n";
    
    for (size_t i = 0; i < Materials.size(); ++i) 
    {
        const UMaterial* Mat = Materials[i];
        if (!Mat) continue;

        std::cout << "Material [" << i << "] (" << MaterialToString(Mat->Type) << "):\n";
        
        std::cout << "  > Common Maps: \n";
        std::cout << "    NormalMap: " << FormatTexIdx(Mat->NormalTexIdx) 
                  << " | Displacement: " << FormatTexIdx(Mat->DisplacementIdx) << "\n";

        if (Mat->Type == EMaterialType::PBR || Mat->Type == EMaterialType::Lambertian) {
            std::cout << "  > Surface Attributes:\n";
            std::cout << "    Albedo:    [" << Mat->Albedo.r << ", " << Mat->Albedo.g << ", " << Mat->Albedo.b << "]\n";
            
            if (Mat->Type == EMaterialType::PBR) {
                std::cout << "    Roughness: " << Mat->Roughness << " | Metalness: " << Mat->Metalness << "\n";
            }
            
            std::cout << "  > Surface Maps:\n";
            std::cout << "    AlbedoTex:    " << FormatTexIdx(Mat->AlbedoTex) << "\n";
            if (Mat->Type == EMaterialType::PBR) {
                std::cout << "    RoughnessTex: " << FormatTexIdx(Mat->RoughnessTex) << "\n";
                std::cout << "    MetalnessTex: " << FormatTexIdx(Mat->MetalnessTex) << "\n";
            }
        } else if (Mat->Type == EMaterialType::Mirror) {
            std::cout << "  > Attributes: Perfect specular reflection (Roughness=0, Metal=1)\n";
        }

        std::cout << "------------------------------------------------------------------\n";
    }
}

void UDebugMessage::PrintTextureSizes(const URenderScene& Scene)
{
    const std::vector<UTexture*>& Textures_Data = Scene.Textures;
    for (size_t i = 0; i < Textures_Data.size(); ++i) 
    {
        const auto& Tex = Textures_Data[i];
        
        if (!Tex->MipmapsData.empty() && Tex->MipmapsData[0] != nullptr) {
            uint32_t Width  = Tex->MipmapsData[0]->SizeX;
            uint32_t Height = Tex->MipmapsData[0]->SizeY;
            
            std::cout << "Texture [" << i << "] - " 
                      << "Level 0 Size: " << Width << " x " << Height
                      << (Tex->Type == UTexture::EType::Cube ? " (Cube)" : " (Flat)")
                      << std::endl;
        } else {
            std::cerr << "Texture [" << i << "] - Warning: No Mipmap data found!" << std::endl;
        }
    }
}

void UDebugMessage::PrintLightProxy(const URenderScene& Scene)
{
    const std::vector<FLightRenderProxy*>& LightProxies = Scene.LightProxyInstances;
	std::cout << "\n==================== GPU Light Proxies (" << LightProxies.size() << ") ====================\n";
    std::cout << std::left << std::setw(6) << "Idx" 
              << std::setw(18) << "Type" 
              << std::setw(25) << "Color (RGB)" << "\n";
    std::cout << "----------------------------------------------------------------------\n";

    for (size_t i = 0; i < LightProxies.size(); ++i) {
        const auto& lp = LightProxies[i];


        std::cout << std::left << "[" << i << "] " 
                  << std::setw(18) << lp->Type
                  << "[" << lp->Color.r << ", " << lp->Color.g << ", " << lp->Color.b << "]\n";


        if (lp->Type == 0) { // Sun
            std::cout << "      Dir: [" << lp->Direction.x << ", " << lp->Direction.y << ", " << lp->Direction.z << "]\n";
        } 
        else if (lp->Type == 1 || lp->Type == 2) {
            std::cout << "      Pos: [" << lp->Position.x << ", " << lp->Position.y << ", " << lp->Position.z << "]\n";
            if (lp->Type == 2) {
                std::cout << "      Dir: [" << lp->Direction.x << ", " << lp->Direction.y << ", " << lp->Direction.z << "]\n";
            }
        }
        std::cout << "----------------------------------------------------------------------\n";
    }
}