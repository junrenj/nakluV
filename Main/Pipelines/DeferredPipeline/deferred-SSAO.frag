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
    int SampleCount;
} UBO;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

const int MAX_KERNEL_SIZE = 64;

const vec3 SampleKernel[MAX_KERNEL_SIZE] = vec3[]
(
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
    vec3(-0.4776,  0.2847,  0.0271),

    vec3( 0.2454,  0.7374,  0.2271),
    vec3(-0.3207,  0.4621,  0.6345),
    vec3( 0.6098,  0.4209,  0.1937),
    vec3(-0.7232,  0.2081,  0.3088),
    vec3( 0.2133, -0.8212,  0.3214),
    vec3(-0.1547,  0.8121,  0.1289),
    vec3( 0.7811, -0.2764,  0.4102),
    vec3(-0.5782, -0.6031,  0.2935),
    vec3( 0.0341,  0.3589,  0.8793),
    vec3(-0.2422, -0.1744,  0.7628),
    vec3( 0.4948, -0.0981,  0.6812),
    vec3(-0.6815,  0.0187,  0.5123),
    vec3( 0.1789,  0.6215,  0.5418),
    vec3(-0.4319,  0.3854,  0.4562),
    vec3( 0.3541, -0.5213,  0.6027),
    vec3(-0.1032, -0.7428,  0.4826),

    vec3( 0.8123,  0.1214,  0.1172),
    vec3(-0.8051,  0.1432,  0.1449),
    vec3( 0.1426,  0.9043,  0.2178),
    vec3(-0.1728, -0.8876,  0.2567),
    vec3( 0.5674,  0.6018,  0.0814),
    vec3(-0.6257,  0.5562,  0.0971),
    vec3( 0.6412, -0.4891,  0.1883),
    vec3(-0.5219, -0.6124,  0.1746),
    vec3( 0.2761,  0.1189,  0.9351),
    vec3(-0.3014,  0.2092,  0.8824),
    vec3( 0.1876, -0.3285,  0.8392),
    vec3(-0.4213, -0.2611,  0.7815),
    vec3( 0.7221,  0.3427,  0.3493),
    vec3(-0.7192,  0.2765,  0.4028),
    vec3( 0.4389, -0.6983,  0.3162),
    vec3(-0.3187, -0.7462,  0.3315),

    vec3( 0.0912,  0.0718,  0.9932),
    vec3(-0.0873,  0.0921,  0.9891),
    vec3( 0.0735, -0.1119,  0.9844),
    vec3(-0.1184, -0.0845,  0.9723),
    vec3( 0.9311,  0.2414,  0.1378),
    vec3(-0.9182,  0.2851,  0.1225),
    vec3( 0.2381,  0.9314,  0.1439),
    vec3(-0.2745, -0.9203,  0.1517),
    vec3( 0.5932,  0.2134,  0.6891),
    vec3(-0.5571,  0.2922,  0.6435),
    vec3( 0.2844, -0.5821,  0.6388),
    vec3(-0.3341, -0.5192,  0.7014),
    vec3( 0.7582, -0.0813,  0.5121),
    vec3(-0.7634, -0.0662,  0.4812),
    vec3( 0.0642,  0.7641,  0.4937),
    vec3(-0.0721, -0.7315,  0.5321)
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

vec3 DecodeNormal(vec3 InN)
{
    return normalize(InN * 2.0 - 1.0);
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

    for (int i = 0; i < UBO.SampleCount; ++i)
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

    float ao = 1.0 - (occlusion / float(UBO.SampleCount));
    ao = pow(clamp(ao, 0.0, 1.0), UBO.Power);
    return ao;
}

void main()
{
    float ao = ComputeSSAO(uv);
    outColor = vec4(ao, ao, ao, 1.0);
}