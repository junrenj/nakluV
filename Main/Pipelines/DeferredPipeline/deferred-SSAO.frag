#version 450

layout(set = 0, binding = 0) uniform sampler2D GBuffer0Tex; // normal (world-space)
layout(set = 0, binding = 1) uniform sampler2D GBuffer1Tex; // optional / unused
layout(set = 0, binding = 2) uniform sampler2D GBuffer2Tex; // position (world-space)

layout(set = 0, binding = 3, std140) uniform SSAOParams
{
    mat4 ViewFromWorld;
    mat4 Projection;
    mat4 InvProjection;
    float Radius;
    float Bias;
    float Power;
    float Padding0;
} UBO;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

const int KERNEL_SIZE = 16;

const vec3 SampleKernel[KERNEL_SIZE] = vec3[](
    vec3( 0.5381,  0.1856,  0.4319),
    vec3( 0.1379,  0.2486,  0.4430),
    vec3( 0.3371,  0.5679,  0.0057),
    vec3(-0.6999, -0.0451,  0.0019),
    vec3( 0.0689, -0.1598,  0.8547),
    vec3( 0.0560,  0.0069,  0.1843),
    vec3(-0.0146,  0.1402,  0.0762),
    vec3( 0.0100, -0.1924,  0.0344),
    vec3(-0.3577, -0.5301,  0.4358),
    vec3(-0.3169,  0.1063,  0.0158),
    vec3( 0.0103, -0.5869,  0.0046),
    vec3(-0.0897, -0.4940,  0.3287),
    vec3( 0.7119, -0.0154,  0.0918),
    vec3(-0.0533,  0.0596,  0.5411),
    vec3( 0.0352, -0.0631,  0.5460),
    vec3(-0.4776,  0.2847,  0.0271)
);

float Hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 RandomVec(vec2 texcoord)
{
    float x = Hash12(texcoord * 123.34) * 2.0 - 1.0;
    float y = Hash12(texcoord * 456.21) * 2.0 - 1.0;
    float z = Hash12(texcoord * 789.54) * 2.0 - 1.0;
    return normalize(vec3(x, y, z));
}

vec3 DecodeNormal(vec3 enc)
{
    return normalize(enc * 2.0 - 1.0);
}

float ComputeSSAO(vec2 texcoord)
{
    vec3 fragPosWS = texture(GBuffer2Tex, texcoord).rgb;
    vec3 normalWS  = DecodeNormal(texture(GBuffer0Tex, texcoord).rgb);

    // world -> view
    vec3 fragPosVS = (UBO.ViewFromWorld * vec4(fragPosWS, 1.0)).xyz;
    vec3 normalVS  = normalize(mat3(UBO.ViewFromWorld) * normalWS);

    if (length(normalVS) < 1e-5)
        return 1.0;

    vec3 randomVec = RandomVec(texcoord * vec2(textureSize(GBuffer0Tex, 0)));

    vec3 tangent   = normalize(randomVec - normalVS * dot(randomVec, normalVS));
    vec3 bitangent = cross(normalVS, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normalVS);

    float occlusion = 0.0;

    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        vec3 sampleDirVS = TBN * SampleKernel[i];
        vec3 samplePosVS = fragPosVS + sampleDirVS * UBO.Radius;

        vec4 clip = UBO.Projection * vec4(samplePosVS, 1.0);
        clip.xyz /= max(clip.w, 1e-5);

        vec2 sampleUV = clip.xy * 0.5 + 0.5;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            continue;
        }

        vec3 visiblePosWS = texture(GBuffer2Tex, sampleUV).rgb;
        vec3 visiblePosVS = (UBO.ViewFromWorld * vec4(visiblePosWS, 1.0)).xyz;

        float dz = abs(fragPosVS.z - visiblePosVS.z);
        float rangeWeight = smoothstep(0.0, 1.0, UBO.Radius / max(dz, 1e-4));

        float blocked = (visiblePosVS.z >= samplePosVS.z + UBO.Bias) ? 1.0 : 0.0;

        occlusion += blocked * rangeWeight;
    }

    float ao = 1.0 - (occlusion / float(KERNEL_SIZE));
    ao = pow(clamp(ao, 0.0, 1.0), UBO.Power);
    return ao;
}

void main()
{
    float ao = ComputeSSAO(uv);
    outColor = vec4(ao, ao, ao, 1.0);
}