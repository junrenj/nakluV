#version 450

layout(set = 3, binding = 0) uniform sampler2D AlbedoTex;
layout(set = 3, binding = 1) uniform sampler2D RoughnessTex;
layout(set = 3, binding = 2) uniform sampler2D MetalnessTex;
layout(set = 3, binding = 3) uniform sampler2D NormalTex;
layout(set = 3, binding = 4) uniform sampler2D DisplacementTex;

layout(push_constant) uniform Push
{
    int MaterialType;
    float padding0;
    float padding1;
    float padding2;
} PushConstant;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 outGBuffer0; // normal.xyz, roughness
layout(location = 1) out vec4 outGBuffer1; // albedo.rgb, metalness
layout(location = 2) out vec4 outGBuffer2; // worldPos.xyz, materialType

vec3 CalNormal(vec3 n)
{
    return normalize(n * 2.0 - 1.0);
}

void main()
{
    vec3 albedo = texture(AlbedoTex, texcoord).rgb;
    float roughness = texture(RoughnessTex, texcoord).r;
    float metalness = texture(MetalnessTex, texcoord).r;

    vec3 worldNormal = normalize(normal);

    // normal map
    vec3 tangentNormal = texture(NormalTex, texcoord).xyz;
    tangentNormal = CalNormal(tangentNormal);
    worldNormal = normalize(TBN * tangentNormal);

    outGBuffer0 = vec4(worldNormal * 0.5 + 0.5, roughness);
    outGBuffer1 = vec4(albedo, metalness);
    outGBuffer2 = vec4(position, float(PushConstant.MaterialType));
}