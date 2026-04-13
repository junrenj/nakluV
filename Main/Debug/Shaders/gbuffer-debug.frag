#version 450

layout(set = 0, binding = 0) uniform sampler2D GBuffer0Tex;
layout(set = 0, binding = 1) uniform sampler2D GBuffer1Tex;
layout(set = 0, binding = 2) uniform sampler2D GBuffer2Tex;

layout(push_constant) uniform Push
{
    int Mode;
    float Padding0;
    float Padding1;
    float Padding2;
} PushConstant;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    if (PushConstant.Mode == 1) // albedo
    {
        vec3 albedo = texture(GBuffer1Tex, uv).rgb;
        outColor = vec4(albedo, 1.0);
    }
    else if (PushConstant.Mode == 2) // normal
    {
        vec3 n = texture(GBuffer0Tex, uv).rgb;
        outColor = vec4(n, 1.0);
    }
    else if (PushConstant.Mode == 3) // position
    {
        vec3 p = texture(GBuffer2Tex, uv).rgb;
        outColor = vec4(fract(p * 0.05), 1.0);
    }
    else if( PushConstant.Mode == 4) // roughness
    {
        float r = texture(GBuffer0Tex, uv).a;
        outColor = vec4(r, r , r, 1.0);
    }
    else if( PushConstant.Mode == 5) // metalness
    {
        float m = texture(GBuffer1Tex, uv).a;
        outColor = vec4(m, m , m, 1.0);
    }
    else
    {
        outColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
}