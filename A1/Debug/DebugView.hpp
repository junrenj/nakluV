#pragma once
#include "../Render/RenderScene.hpp"
#include "glm/glm/glm.hpp"
#include "DebugColVertex.hpp"

// This is the place to store all Debug Data
class UDebugScene
{
    std::vector< FDebugColVertex > FrustumVertices;
    std::vector< FDebugColVertex > BBoxVertices;
    uint8_t DebugFrustumLineColor[4] = {255, 0 ,0 ,255};	// Red
    uint8_t DebugBBoxLineColor[4] = {255, 255, 255, 255};		// White

    void GenerateBBoxVertices(const UBoundingBox& BBox);
    void GenerateFrustumVertices(const UCamera& Camera);
};

// This is the class that print message
class UDebugMessage
{
    static void PrintRenderSceneMesh(const URenderScene& Scene);
    static void PrintRenderProxies(const URenderScene& Scene);
    static void PrintMatrix(const std::string& Name, const glm::mat4& Mat);
    static std::string FormatTexIdx(uint32_t Idx);
    static std::string MaterialToString(EMaterialType Type);
    static void PrintMaterial(const URenderScene& Scene);
    static void PrintTextureSizes(const URenderScene& Scene);
    static void PrintLightProxy(const URenderScene& Scene);
};
