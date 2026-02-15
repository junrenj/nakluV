#include "RenderExtractor.hpp"
#include "glm/glm/glm.hpp"
#include "../Animation/AnimationPlayer.hpp"
#include <iostream>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

void URenderExtractor::BuildRenderScene(std::string S72Path, URenderScene& RenderScene)
{
    CurrentS72 = S72::load(S72Path);

    std::unordered_map< const S72::Texture* , UTexture* > S72Tex2UTex;
    std::unordered_map< const S72::Material* , UMaterial* > S72Mat2UMat;
    std::unordered_map< const S72::Node* , UNode* > S72Node2UNode;

    // 0. Get Dependency ready
    // get Texture Resources Ready
    BuildUTextureData(S72Tex2UTex, RenderScene);
    // get Material Resources Ready
    BuildUMaterialData(S72Mat2UMat, S72Tex2UTex, RenderScene);

    // 1. Build UNode Tree
    BuildUNodeTree(RenderScene, S72Tex2UTex, S72Mat2UMat, S72Node2UNode);
    // 2. Build UNode BBox
    BuildUNodesBBoxIterate(RenderScene.RootNode);
    // 3. Generate RenderProxy
    RenderScene.GenerateMeshProxy();
    RenderScene.GenerateLightProxy();
    RenderScene.GenerateWholeVertexBuffer();
    // 4. Get Animation Sequence
    BuildAnimData(S72Node2UNode);
}

//BEGIN: UNode Data Extract
void URenderExtractor::BuildUNodeTree(URenderScene& RenderScene,
    std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex,
    std::unordered_map< const S72::Material* , UMaterial* >& S72Mat2UMat,
    std::unordered_map< const S72::Node*, UNode* >& S72Node2UNode)
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
            Root->Children.push_back(BuildUNodeTreeIterate(InS72Node, RenderScene, Root, S72Tex2UTex, S72Mat2UMat, S72Node2UNode));
        }
    }
}

UNode* URenderExtractor::BuildUNodeTreeIterate(const S72::Node& InS72Node, 
    URenderScene& RenderScene, 
    UNode* Parent,
    std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex,
    std::unordered_map< const S72::Material* , UMaterial* >& S72Mat2UMat,
    std::unordered_map< const S72::Node*, UNode* >& S72Node2UNode)
{
    UNode* NewNode = new UNode();
    NewNode->Parent = Parent;
    S72Node2UNode[&InS72Node] = NewNode;

    // 1. Transform
    NewNode->Transform.Translation = glm::vec3(InS72Node.translation.x, InS72Node.translation.y, InS72Node.translation.z);
    NewNode->Transform.Rotation = glm::quat(InS72Node.rotation.w, InS72Node.rotation.x, InS72Node.rotation.y, InS72Node.rotation.z);
    NewNode->Transform.Scale = glm::vec3(InS72Node.scale.x, InS72Node.scale.y, InS72Node.scale.z);

    // 2. Other Data
    if(InS72Node.mesh)
    {
        const S72::Mesh& S72Mesh = *InS72Node.mesh;
        URenderMesh* NewMesh = new URenderMesh();
        CloneRenderMeshFromS72Mesh(S72Mesh, *NewMesh, S72Mat2UMat);
        NewNode->Mesh = NewMesh;
        RenderScene.Nodes2RenderMeshes[NewNode] = NewMesh;
        RenderScene.RenderMeshes2Nodes[NewMesh] = NewNode;
        RenderScene.AllMeshes.push_back(NewMesh);
    }

    if(InS72Node.camera)
    {
        const S72::Camera& S72Camera = *InS72Node.camera;
        UCamera* NewCamera = new UCamera();
        // Camera data
        std::visit([&](auto const& S72Camera)
        {
            using T = std::decay_t<decltype(S72Camera)>;
            if constexpr (std::is_same_v<T, S72::Camera::Perspective>)
            {
                UCamera::FPerspective Projection;
                Projection.Aspect = S72Camera.aspect;
                Projection.Vfov = S72Camera.vfov;
                Projection.Near = S72Camera.near;
                Projection.Far = S72Camera.far;

                NewCamera->Projection = Projection;
            }

        }, S72Camera.projection);

        NewCamera->BindingNode = NewNode;
        RenderScene.Cameras.push_back(NewCamera);
    }

    if(InS72Node.environment)
    {
        // const S72::Environment& S72Environment = *InS72Node.environment;
        UEnvironment* NewEnvironment = new UEnvironment();
        //  Environment data
        if(InS72Node.environment->radiance)
        {
            auto it = S72Tex2UTex.find(InS72Node.environment->radiance);
            if (it != S72Tex2UTex.end())
            {
                NewEnvironment->EnvTexture = RenderScene.GetTextureIdx(it->second);
            }
        }

        NewNode->environment = NewEnvironment;
        RenderScene.Environments[NewNode] = NewEnvironment;
    }

    if(InS72Node.light)
    {
        const S72::Light& S72Light = *InS72Node.light;
        ULight* NewLight = nullptr;

        std::visit([&](auto const& S72Light)
        {
            using T = std::decay_t<decltype(S72Light)>;

            // -------- Sun --------
            if constexpr (std::is_same_v<T, S72::Light::Sun>)
            {
                ULight_Sun* Sun = new ULight_Sun();
                Sun->Angle = S72Light.angle;
                Sun->Strength = S72Light.strength;

                NewLight = Sun;
            }

            // -------- Sphere --------
            else if constexpr (std::is_same_v<T, S72::Light::Sphere>)
            {
                ULight_Sphere* Sphere = new ULight_Sphere();
                Sphere->Radius = S72Light.radius;
                Sphere->Power  = S72Light.power;
                Sphere->Limit  = S72Light.limit;

                NewLight = Sphere;
            }

            // -------- Spot --------
            else if constexpr (std::is_same_v<T, S72::Light::Spot>)
            {
                ULight_Spot* Spot = new ULight_Spot();
                Spot->Radius = S72Light.radius;
                Spot->Power  = S72Light.power;
                Spot->Limit  = S72Light.limit;
                Spot->Fov    = S72Light.fov;
                Spot->Blend  = S72Light.blend;

                NewLight = Spot;
            }

        }, S72Light.source);

        if(NewLight)
        {
            // Light data
            NewLight->Tint.r = S72Light.tint.r;
            NewLight->Tint.g = S72Light.tint.g;
            NewLight->Tint.b = S72Light.tint.b;
            NewLight->shadow = S72Light.shadow;
            NewNode->light = NewLight;
            RenderScene.Lights[NewLight] = NewNode;
        }
    }

    // 3. Iterate and bind children
    if(InS72Node.children.size() >= 1)
    {
        for (size_t i = 0; i < InS72Node.children.size(); i++)
        {
            const S72::Node& Child = *InS72Node.children[i];
            NewNode->Children.push_back(BuildUNodeTreeIterate(Child, RenderScene, NewNode, S72Tex2UTex, S72Mat2UMat, S72Node2UNode));
        }
    }
    RenderScene.Nodes.push_back(NewNode);
    return NewNode;
}

void URenderExtractor::BuildUNodesBBoxIterate(UNode* InNode)
{
    FAABB BBox_World;
    BBox_World.Min = glm::vec3(FLT_MAX);
    BBox_World.Max = glm::vec3(-FLT_MAX);
    // if has mesh first use mesh's bounding box
    if(InNode->Mesh)
    {
        // glm::mat4 Local2World = UNode::GetLocal2WorldMatrix(InNode);
        BBox_World = InNode->Mesh->BoundingBox;
        // FAABB::UpdateBBoxWithTransform(BBox_World, BBox_Local, Local2World);
    }

    // Iterate Child
    for (UNode* Child : InNode->Children)
    {
        BuildUNodesBBoxIterate(Child);
        // BBox_World.Min = glm::min(BBox_World.Min, Child->BoundingBox.Min);
        // BBox_World.Max = glm::max(BBox_World.Max, Child->BoundingBox.Max);
    }

    InNode->BoundingBox = BBox_World;
}
//END:  UNode Data Extract

//BEGIN: Material Data Extract
void URenderExtractor::BuildUMaterialData(
    std::unordered_map< const S72::Material* , UMaterial* >& S72Mat2UMat, 
    std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, 
    URenderScene& Scene)
{
    for (const auto& [Name, Mat] : CurrentS72.materials)
    {
        UMaterial* NewMaterial = CloneUMaterialFromS72Material(Mat, S72Tex2UTex, Scene);
        S72Mat2UMat[&Mat] = NewMaterial;
        Scene.Materials.push_back(NewMaterial);
    }
}

UMaterial* URenderExtractor::CloneUMaterialFromS72Material(
    const S72::Material& InS72Mat, 
    std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, 
    const URenderScene& Scene)
{
    UMaterial* NewMat = new UMaterial();

    // 1. Get general Tex Idx
    if(InS72Mat.normal_map)
    {
        const S72::Texture& S72Tex = *InS72Mat.normal_map;
        UTexture* Tex = S72Tex2UTex[&S72Tex];
        NewMat->NormalTexIdx = Scene.GetTextureIdx(Tex);
    }

    if(InS72Mat.displacement_map)
    {
        const S72::Texture& S72Tex = *InS72Mat.displacement_map;
        UTexture* Tex = S72Tex2UTex[&S72Tex];
        NewMat->DisplacementIdx = Scene.GetTextureIdx(Tex);
    }

    // 2. deal with variant
    std::visit([&](auto&& arg) 
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, S72::Material::PBR>) 
        {
            NewMat->Type = EMaterialType::PBR;
            
            // Albedo
            if (auto* col = std::get_if<S72::color>(&arg.albedo)) 
            {
                NewMat->Albedo = glm::vec4(col->r, col->g, col->b, 1.0f);
            } 
            else 
            {
                auto* Albedo = std::get<S72::Texture*>(arg.albedo);
                if(Albedo)
                {
                    const S72::Texture& S72Tex = *Albedo;
                    UTexture* Tex = S72Tex2UTex[&S72Tex];
                    NewMat->AlbedoTex = Scene.GetTextureIdx(Tex);
                }
                NewMat->Albedo = glm::vec4(1.0f);
            }

            // Roughness
            if (auto* val = std::get_if<float>(&arg.roughness)) 
            {
                NewMat->Roughness = *val;
            } 
            else 
            {
                auto* Roughness = std::get<S72::Texture*>(arg.roughness);
                if(Roughness)
                {
                    const S72::Texture& S72Tex = *Roughness;
                    UTexture* Tex = S72Tex2UTex[&S72Tex];
                    NewMat->RoughnessTex = Scene.GetTextureIdx(Tex);
                }
                NewMat->Roughness = 1.0f;
            }

            // Metalness
            if (auto* val = std::get_if<float>(&arg.metalness)) 
            {
                NewMat->Metalness = *val;
            } else 
            {
                auto* Metalness = std::get<S72::Texture*>(arg.metalness);
                if(Metalness)
                {
                    const S72::Texture& S72Tex = *Metalness;
                    UTexture* Tex = S72Tex2UTex[&S72Tex];
                    NewMat->MetalnessTex = Scene.GetTextureIdx(Tex);
                }
                NewMat->Metalness = 1.0f;
            }
        }
        else if constexpr (std::is_same_v<T, S72::Material::Lambertian>) 
        {
            NewMat->Type = EMaterialType::Lambertian;
            // Lambertian 
            if (auto* col = std::get_if<S72::color>(&arg.albedo)) 
            {
                NewMat->Albedo = glm::vec4(col->r, col->g, col->b, 1.0f);
            } 
            else 
            {
                auto* Albedo = std::get<S72::Texture*>(arg.albedo);
                if(Albedo)
                {
                    const S72::Texture& S72Tex = *Albedo;
                    UTexture* Tex = S72Tex2UTex[&S72Tex];
                    NewMat->AlbedoTex = Scene.GetTextureIdx(Tex);
                }
            }
            NewMat->Roughness = 1.0f;
            NewMat->Metalness = 0.0f;
        }
        else if constexpr (std::is_same_v<T, S72::Material::Mirror>) 
        {
            NewMat->Type = EMaterialType::Mirror;
            NewMat->Albedo = glm::vec4(1.0f);
            NewMat->Roughness = 0.0f;
            NewMat->Metalness = 1.0f;
        }
        else if constexpr (std::is_same_v<T, S72::Material::Environment>) 
        {
            NewMat->Type = EMaterialType::Environment;
        }
    }, InS72Mat.brdf);

    return NewMat;
}

//END: Material Data Extract

//BEGIN: Texture Data Extract
void URenderExtractor::BuildUTextureData(
    std::unordered_map< const S72::Texture* , UTexture* >& S72Tex2UTex, 
    URenderScene& Scene)
{
    for (const auto& [Name, Tex] : CurrentS72.textures)
    {
        UTexture* Texture = ReadBulkDataFromImage(Tex);
        S72Tex2UTex[&Tex] = Texture;
        Scene.Textures.push_back(Texture);
    }
}

UTexture* URenderExtractor::ReadBulkDataFromImage(const S72::Texture& InTexture)
{
    stbi_set_flip_vertically_on_load(true);
    UTexture* NewTexture = new UTexture();
    NewTexture->Type = InTexture.type == S72::Texture::Type::cube ? UTexture::EType::Cube : UTexture::EType::Flat;

    switch (InTexture.format)
    {
        case S72::Texture::Format::srgb:
            NewTexture->Format = UTexture::EFormat::SRGB;
            break;
        case S72::Texture::Format::rgbe:
            default:
            NewTexture->Format = UTexture::EFormat::Linear;
            break;
    }

    int Width, Height, Channels;
    uint8_t* Pixels = stbi_load(InTexture.path.c_str(), &Width, &Height, &Channels, STBI_rgb_alpha);

    if (!Pixels) 
    {
        return nullptr; 
    }

    // 3. Mipmap Structure
    UTexture::FTextureMipMap* Mip0 = new UTexture::FTextureMipMap();
    Mip0->SizeX = static_cast<uint32_t>(Width);
    Mip0->SizeY = static_cast<uint32_t>(Height);
    
    size_t ImageSize = Width * Height * 4; // 4 
    Mip0->BulkData.resize(ImageSize);
    std::memcpy(Mip0->BulkData.data(), Pixels, ImageSize);

    NewTexture->MipmapsData.push_back(std::move(Mip0));

    // 4. release data
    stbi_image_free(Pixels);

    return NewTexture;
}
//END: Texture Data Extract

//BEGIN: URenerMesh Data Extract
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

void URenderExtractor::CloneRenderMeshFromS72Mesh(
    const S72::Mesh& S72Mesh, 
    URenderMesh& OutMesh,
    std::unordered_map< const S72::Material* , UMaterial* >& S72Mat2UMat)
{
    // 1. type of mesh(can be changed if debug mode on)
    OutMesh.topology = S72Mesh.topology;

    // 2. Vertex Data Layout
    OutMesh.VertexStride = sizeof(URenderMesh::FVertex);
    OutMesh.PositionOffset = offsetof(URenderMesh::FVertex, Position);
    OutMesh.NormalOffset = offsetof(URenderMesh::FVertex, Normal);
    OutMesh.TangentOffset = offsetof(URenderMesh::FVertex, Tangent);
    OutMesh.UVOffset = offsetof(URenderMesh::FVertex, UV);

    // 3. Vertex Count
    OutMesh.VertexCount = S72Mesh.count;
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
        const auto& Attribute = S72Mesh.attributes.at("POSITION");
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
    if (auto it = S72Mesh.attributes.find("NORMAL"); it != S72Mesh.attributes.end()) 
    {
        auto& Attribute = it->second;
        const S72::DataFile& DataFile = Attribute.src;

        const uint8_t* SrcPtr =
            LoadedBinary.at(DataFile.path).data() + Attribute.offset;

        for (uint32_t i = 0; i < OutMesh.VertexCount; ++i) 
        {
            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.NormalOffset,
                SrcPtr + i * Attribute.stride,
                sizeof(glm::vec3)
            );
        }
    }

    // 7. Get TANGENT
    if (auto it = S72Mesh.attributes.find("TANGENT"); it != S72Mesh.attributes.end()) 
    {
        auto& Attribute = it->second;
        const S72::DataFile& DataFile = Attribute.src;

        const uint8_t* SrcPtr =
            LoadedBinary.at(DataFile.path).data() + Attribute.offset;

        for (uint32_t i = 0; i < OutMesh.VertexCount; ++i) 
        {
            std::memcpy(
                OutMesh.VertexData.data() + i * OutMesh.VertexStride + OutMesh.TangentOffset,
                SrcPtr + i * Attribute.stride,
                sizeof(glm::vec4)
            );
        }
    }

    // 8. Get TEXCOORD
    if (auto it = S72Mesh.attributes.find("TEXCOORD"); it != S72Mesh.attributes.end()) 
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

    // 9. Material
    auto it = S72Mat2UMat.find(S72Mesh.material);
    if (it != S72Mat2UMat.end())
    {
        OutMesh.Material = it->second;
    }
}
//END: URenerMesh Data Extract

//BEGIN: Animation
void URenderExtractor::BuildAnimData(std::unordered_map< const S72::Node* , UNode* >& S72Node2UNode)
{
    for (const S72::Driver& Driver : CurrentS72.drivers)
    {
        UAnimInstance* AnimInstance = new UAnimInstance();
        auto it = S72Node2UNode.find(&Driver.node);
        if (it != S72Node2UNode.end())
        {
            AnimInstance->Node = it->second;
            CloneAnimFromS72Anim(Driver, *AnimInstance);
        }
        UAnimPlayer::AnimInstances.push_back(AnimInstance);
    }
}

void URenderExtractor::CloneAnimFromS72Anim(const S72::Driver& Driver, UAnimInstance& AnimInstance)
{
    switch (Driver.channel)
    {
        case S72::Driver::Channel::translation:
            AnimInstance.Channel = UAnimInstance::EChannel::Translation;
            break;
        case S72::Driver::Channel::scale:
            AnimInstance.Channel = UAnimInstance::EChannel::Scale;
            break;
        case S72::Driver::Channel::rotation:
            AnimInstance.Channel = UAnimInstance::EChannel::Rotation;
            break;
    }

    switch (Driver.interpolation)
    {
        case S72::Driver::Interpolation::STEP:
            AnimInstance.Interpolation = UAnimInstance::EInterpolation::STEP;
            break;
        case S72::Driver::Interpolation::LINEAR:
            AnimInstance.Interpolation = UAnimInstance::EInterpolation::LINEAR;
            break;
        case S72::Driver::Interpolation::SLERP:
            AnimInstance.Interpolation = UAnimInstance::EInterpolation::SLERP;
            break;
    }

    AnimInstance.Times.reserve(Driver.times.size());
    AnimInstance.Times.insert(AnimInstance.Times.end(), Driver.times.begin(), Driver.times.end());

    AnimInstance.Values.reserve(Driver.values.size());
    AnimInstance.Values.insert(AnimInstance.Values.end(), Driver.values.begin(), Driver.values.end());
}
//END: Animation
