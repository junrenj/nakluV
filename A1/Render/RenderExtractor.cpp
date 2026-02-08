#include "RenderExtractor.hpp"
#include "glm/glm/glm.hpp"
#include <iostream>
#include <fstream>

void URenderExtractor::BuildRenderScene(std::string S72Path, URenderScene& RenderScene)
{
    CurrentS72 = S72::load(S72Path);

    // 1. Build UNode Tree
    BuildUNodeTree(RenderScene);
    // 2. Build UNode BBox
    BuildUNodesBBoxIterate(RenderScene.RootNode);
}

//BEGIN: UNode Data Extract
void URenderExtractor::BuildUNodeTree(URenderScene& RenderScene)
{
    // Build a single unique root node for all nodes data
    UNode* Root = new UNode();
    RenderScene.Nodes.push_back(Root);
    RenderScene.RootNode = Root;
    for (uint32_t i = 0; i < CurrentS72.scene.roots.size(); i++)
    {
        if(CurrentS72.scene.roots[i])
        {
            const S72::Node& InS72Node = *(CurrentS72.scene.roots[i]);
            Root->Children.push_back(BuildUNodeTreeIterate(InS72Node, RenderScene));
        }
    }
}

UNode* URenderExtractor::BuildUNodeTreeIterate(const S72::Node& InS72Node, URenderScene& RenderScene)
{
    UNode* NewNode = new UNode();

    // 1. Transform
    NewNode->Transform.Translation = glm::vec3(InS72Node.translation.x, InS72Node.translation.y, InS72Node.translation.z);
    NewNode->Transform.Rotation = glm::vec4(InS72Node.rotation.x, InS72Node.rotation.y, InS72Node.rotation.z, InS72Node.rotation.w);
    NewNode->Transform.Scale = glm::vec3(InS72Node.scale.x, InS72Node.scale.y, InS72Node.scale.z);

    // 2. Other Data
    if(InS72Node.mesh)
    {
        const S72::Mesh& S72Mesh = *InS72Node.mesh;
        URenderMesh* NewMesh = new URenderMesh();
        CloneRenderMeshFromS72Mesh(S72Mesh, *NewMesh);
        NewNode->Mesh = NewMesh;
        RenderScene.RenderMeshes[NewNode] = NewMesh;
    }

    if(InS72Node.camera)
    {
        // const S72::Camera& S72Camera = *InS72Node.camera;
        UCamera* NewCamera = new UCamera();
        // TODO: Camera data
        NewNode->camera = NewCamera;
        RenderScene.Cameras[NewNode] = NewCamera;
    }

    if(InS72Node.environment)
    {
        // const S72::Environment& S72Environment = *InS72Node.environment;
        UEnvironment* NewEnvironment = new UEnvironment();
        // TODO: Environment data
        NewNode->environment = NewEnvironment;
        RenderScene.Environment = NewEnvironment;
    }

    if(InS72Node.light)
    {
        // const S72::Light& S72Light = *InS72Node.light;
        ULight* NewLight = new ULight();
        // TODO: Light data
        NewNode->light = NewLight;
        RenderScene.Lights[NewNode] = NewLight;
    }

    // 3. Iterate and bind children
    if(InS72Node.children.size() >= 1)
    {
        for (size_t i = 0; i < InS72Node.children.size(); i++)
        {
            const S72::Node& Child = *InS72Node.children[i];
            NewNode->Children.push_back(BuildUNodeTreeIterate(Child, RenderScene));
        }
    }

    RenderScene.Nodes.push_back(NewNode);
    return NewNode;
}

void URenderExtractor::BuildUNodesBBoxIterate(UNode* InNode)
{
    UBoundingBox NewBBox;
    NewBBox.Min = glm::vec3(FLT_MAX);
    NewBBox.Max = glm::vec3(-FLT_MAX);
    // if has mesh first use mesh's bounding box
    if(InNode->Mesh)
    {
        NewBBox = InNode->Mesh->BoundingBox;
    }

    // Iterate Child
    for (UNode* Child : InNode->Children)
    {
        BuildUNodesBBoxIterate(Child);
        NewBBox.Min = glm::min(NewBBox.Min, Child->BoundingBox.Min);
        NewBBox.Max = glm::max(NewBBox.Max, Child->BoundingBox.Max);
    }

    InNode->BoundingBox = NewBBox;
}

//BEGIN: Mesh Data Extract
std::vector<uint8_t> URenderExtractor::ReadBinaryFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

void URenderExtractor::CloneRenderMeshFromS72Mesh(const S72::Mesh& InMesh, URenderMesh& OutMesh)
{
    // 1. type of mesh(can be changed if debug mode on)
    OutMesh.topology = InMesh.topology;

    // 2. Vertex Data Layout
    OutMesh.VertexStride = sizeof(URenderMesh::FVertex);
    OutMesh.PositionOffset = offsetof(URenderMesh::FVertex, Position);
    OutMesh.NormalOffset = offsetof(URenderMesh::FVertex, Normal);
    OutMesh.TangentOffset = offsetof(URenderMesh::FVertex, Tangent);
    OutMesh.UVOffset = offsetof(URenderMesh::FVertex, UV);

    // 3. Vertex Count
    OutMesh.VertexCount = InMesh.count;
    OutMesh.IndexCount = 0;

    OutMesh.VertexData.resize(OutMesh.VertexCount * OutMesh.VertexStride);

    // 4. Bounding box
    OutMesh.BoundingBox.Min = glm::vec3(FLT_MAX);
    OutMesh.BoundingBox.Max = glm::vec3(-FLT_MAX);

    std::unordered_map<std::string, std::vector<uint8_t>> LoadedBinary;

    for (auto& [_, df] : CurrentS72.data_files) 
    {
        LoadedBinary[df.path] = ReadBinaryFile(df.path);
    }

    // 5. Get POSITION to Calculate BBox
    {
        const auto& Attribute = InMesh.attributes.at("POSITION");
        const S72::DataFile& DataFile = Attribute.src;
        
        const uint8_t* SrcPtr = LoadedBinary.at(DataFile.path).data() + Attribute.offset;
        for (uint32_t i = 0; i < OutMesh.VertexCount; i++)
        {
            glm::vec3 Position;
            std::memcpy(&Position, SrcPtr + i * Attribute.stride, sizeof(glm::vec3));

            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.PositionOffset,
                &Position,
                sizeof(glm::vec3)
            );

            OutMesh.BoundingBox.Min = glm::min(OutMesh.BoundingBox.Min, Position);
            OutMesh.BoundingBox.Max = glm::max(OutMesh.BoundingBox.Max, Position);
        }
    }

    // 6. Get NORMAL
    if (auto it = InMesh.attributes.find("NORMAL"); it != InMesh.attributes.end()) 
    {
        auto& Attribute = it->second;
        const S72::DataFile& DataFile = Attribute.src;

        const uint8_t* SrcPtr =
            LoadedBinary.at(DataFile.path).data() + Attribute.offset;

        for (uint32_t i = 0; i < OutMesh.VertexCount; ++i) {
            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.NormalOffset,
                SrcPtr + i * Attribute.stride,
                sizeof(glm::vec3)
            );
        }
    }

    // 7. Get TANGENT
    if (auto it = InMesh.attributes.find("TANGENT"); it != InMesh.attributes.end()) 
    {
        auto& Attribute = it->second;
        const S72::DataFile& DataFile = Attribute.src;

        const uint8_t* SrcPtr =
            LoadedBinary.at(DataFile.path).data() + Attribute.offset;

        for (uint32_t i = 0; i < OutMesh.VertexCount; ++i) {
            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.TangentOffset,
                SrcPtr + i * Attribute.stride,
                sizeof(glm::vec4)
            );
        }
    }

    // 8. Get TEXCOORD
    if (auto it = InMesh.attributes.find("TEXCOORD"); it != InMesh.attributes.end()) 
    {
        auto& Attribute = it->second;
        const S72::DataFile& DataFile = Attribute.src;

        const uint8_t* SrcPtr =
            LoadedBinary.at(DataFile.path).data() + Attribute.offset;

        for (uint32_t i = 0; i < OutMesh.VertexCount; ++i) {
            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.UVOffset,
                SrcPtr + i * Attribute.stride,
                sizeof(glm::vec2)
            );
        }
    }
}
//END: URenerMesh Data Extract
