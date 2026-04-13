#version 450

layout(set = 0, binding = 0) uniform sampler2D GBuffer0Tex;
layout(set = 0, binding = 1) uniform sampler2D GBuffer1Tex;
layout(set = 0, binding = 2) uniform sampler2D GBuffer2Tex;

layout(set = 1, binding = 0, std140) uniform World
{
    vec4 VIEW_POS;
    vec4 AJUST_VAR;
} WORLD;

struct Light
{
    vec4 POSITION_TYPE;
    vec4 COLOR_FALLOFF;
    vec4 DIRECTION_LIMIT;
    vec4 SPECIALPARAMS;
    mat4 LIGHT_FROM_WORLD;
};

layout(set = 2, binding = 0, std430) readonly buffer Lights
{
    Light LIGHTS[];
};

layout(push_constant) uniform Push
{
    int LightsCount;
    float Padding0;
    float Padding1;
    float Padding2;
} PushConstant;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 g0 = texture(GBuffer0Tex, uv);
    vec4 g1 = texture(GBuffer1Tex, uv);
    vec4 g2 = texture(GBuffer2Tex, uv);

    vec3 N = normalize(g0.xyz * 2.0 - 1.0);
    float roughness = g0.w;

    vec3 albedo = g1.xyz;
    float metalness = g1.w;

    vec3 worldPos = g2.xyz;

    vec3 V = normalize(WORLD.VIEW_POS.xyz - worldPos);

    vec3 color = vec3(0.0);

    for (int i = 0; i < PushConstant.LightsCount; ++i)
    {
        vec3 lightPos = LIGHTS[i].POSITION_TYPE.xyz;
        vec3 lightColor = LIGHTS[i].COLOR_FALLOFF.xyz;

        vec3 L = lightPos - worldPos;
        float dist2 = max(dot(L, L), 1e-4);
        float dist = sqrt(dist2);
        L /= dist;

        float NdotL = max(dot(N, L), 0.0);

        vec3 diffuse = albedo * lightColor * NdotL / dist2;

        color += diffuse;
    }

    outColor = vec4(color, 1.0);
}