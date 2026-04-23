#version 450

layout(set = 0, binding = 0) uniform sampler2D GBuffer0Tex; // normal (world-space)
layout(set = 0, binding = 1) uniform sampler2D GBuffer1Tex; // albedo
layout(set = 0, binding = 2) uniform sampler2D GBuffer2Tex; // position (world-space)

layout(set = 0, binding = 3, std140) uniform SSDOParams
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

bool IsValidUV(vec2 coord)
{
    return coord.x >= 0.0 && coord.x <= 1.0 &&
           coord.y >= 0.0 && coord.y <= 1.0;
}

vec3 DecodeNormal(vec3 InN)
{
    return normalize(InN * 2.0 - 1.0);
}

void main()
{
    vec3 P = texture(GBuffer2Tex, uv).xyz;
    vec3 N = DecodeNormal(texture(GBuffer0Tex, uv).rgb);

    if (length(N) < 1e-4)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 randomVec = RandomVec(uv);
    vec3 T = normalize(randomVec - N * dot(randomVec, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    vec3 indirect = vec3(0.0);
    float validSamples = 0.0;

    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        vec3 sampleDir = normalize(TBN * SampleKernel[i]);

        float scale = float(i + 1) / float(KERNEL_SIZE);
        scale = mix(0.1, 1.0, scale * scale);

        vec3 targetPos = P + sampleDir * (UBO.Radius * scale);

        vec4 clipPos = UBO.Projection * UBO.ViewFromWorld * vec4(targetPos, 1.0);

        if (abs(clipPos.w) < 1e-4)
            continue;

        vec3 ndc = clipPos.xyz / clipPos.w;
        vec2 sampleUV = ndc.xy * 0.5 + 0.5;

        if (!IsValidUV(sampleUV))
            continue;

        vec3 sampleWorldPos = texture(GBuffer2Tex, sampleUV).xyz;
        vec3 sampleNormal   = DecodeNormal(texture(GBuffer0Tex, sampleUV).xyz);
        vec3 sampleAlbedo   = texture(GBuffer1Tex, sampleUV).rgb;

        vec3 diffToTarget = sampleWorldPos - targetPos;
        float hitDist = length(diffToTarget);

        float visibility = (hitDist < UBO.Bias) ? 1.0 : 0.0;

        if (visibility <= 1e-4)
            continue;

        vec3 L = sampleWorldPos - P;
        float dist = length(L);

        if (dist < 1e-4)
            continue;

        if (dist > UBO.Radius)
            continue;

        float normalAgree = dot(N, sampleNormal);
        if (normalAgree < 0.3)
            continue;

        vec3 wi = L / dist;

        float NdotL = max(dot(N, wi), 0.0);

        float sampleFacing = max(dot(sampleNormal, -wi), 0.0);

        float attenuation = 1.0 - clamp(dist / UBO.Radius, 0.0, 1.0);
        attenuation *= attenuation;

        vec3 bounce = sampleAlbedo * NdotL * sampleFacing * attenuation * visibility;

        indirect += bounce;
        validSamples += 1.0;
    }

    if (validSamples > 0.0)
    {
        indirect /= validSamples;
    }

    indirect *= UBO.Power;

    outColor = vec4(indirect, 1.0);
}