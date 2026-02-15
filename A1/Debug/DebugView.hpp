#pragma once
#include "../Render/RenderScene.hpp"
#include "glm/glm/glm.hpp"
#include "DebugColVertex.hpp"

// This is the place to store all Debug Data
class UDebugScene
{
private:
    std::vector< FDebugColVertex > FrustumVertices;
    std::vector< FDebugColVertex > BBoxVertices;
    uint8_t FrustumColorActive[4] = {255, 255 ,0 ,255};	// Yellow
    uint8_t FrustumColorInActive[4] = {64, 64 ,64 ,255};	// Dark Grey
    uint8_t BBoxColorVisible[4] = {0, 255, 0, 255};		    // Green
    uint8_t BBoxColorCull[4] = {255, 0, 0, 255};            // Red

public:
    void Update(const std::vector< UCamera* >& Cameras, uint8_t ActiveIdx, const std::vector< UNode * >& Nodes);
    void UpdateFrustumVertices(const std::vector< UCamera* >& Cameras, uint8_t ActiveIdx);
    void UpdateBBoxVertices(const std::vector< UNode * >& Nodes);
    void GetAllVerticesData(std::vector<FDebugColVertex>& LineVertices);
private:
    void GenerateBBoxVertices(const FAABB& BBox, const glm::mat4& Transform, const bool bCanSee);
    void GenerateFrustumVertices(const UCamera& Camera, bool bIsActive);
};

// This is the class that print message
class UDebugMessage
{
public:
    static void PrintRenderSceneMesh(const URenderScene& Scene);
    static void PrintRenderProxies(const URenderScene& Scene);
    static void PrintMatrix(const std::string& Name, const glm::mat4& Mat);
    static std::string FormatTexIdx(uint32_t Idx);
    static std::string MaterialToString(EMaterialType Type);
    static void PrintMaterial(const URenderScene& Scene);
    static void PrintTextureSizes(const URenderScene& Scene);
    static void PrintLightProxy(const URenderScene& Scene);
};
