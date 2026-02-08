// The struct is to translate the data from s72 to data used at CPU
#pragma once

#include "s72Loader/S72.hpp"
#include "RenderScene.hpp"

class URenderExtractor
{
public:
    static void BuildRenderScene(std::string S72Path, URenderScene& RenderScene);

    // New Node Tree
    static void BuildUNodeTree(URenderScene& RenderScene);
    static UNode* BuildUNodeTreeIterate(const S72::Node& InS72Nodes, URenderScene& RenderScene);
    static void BuildUNodesBBoxIterate(UNode* InNode);
    // New RenderMesh Structure
    static void CloneRenderMeshFromS72Mesh(const S72::Mesh& InMesh, URenderMesh& OutMesh);

private:
    static std::vector<uint8_t> ReadBinaryFile(const std::string& path);

private:
    inline static S72 CurrentS72;
};
