#version 450

const float MAX_GGX_LOD = 5.0;
const float PI = 3.1415926;
const int MAX_SPOT_SHADOWS = 32;
const int MAX_SPHERE_SHADOWS = 32;

layout(push_constant) uniform Push
{
    int SSMode;         // 0 -> None 1 -> SSAO 2 -> SSDO
    float Padding0;
    float Padding1;
    float Padding2;
} PushConstant;

layout(set = 0, binding = 0) uniform sampler2D GBuffer0Tex;
layout(set = 0, binding = 1) uniform sampler2D GBuffer1Tex;
layout(set = 0, binding = 2) uniform sampler2D GBuffer2Tex;
layout(set = 0, binding = 3) uniform sampler2D DirectLightTex;

layout(set = 1, binding = 0, std140) uniform World
{
    vec4 VIEW_POS;
    vec4 AJUST_VAR;
} WORLD;

layout(set=2,binding=0) uniform samplerCube ENV_TEX;
layout(set=2,binding=1) uniform samplerCube IRRADIANCE_TEX;
layout(set=2,binding=2) uniform sampler2D LUT_TEX;

layout(set=3, binding=0) uniform sampler2D SSAOTex;
layout(set=3, binding=1) uniform sampler2D SSDOTex;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;


//~BEGIN calculate method
float Saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec3 SafeNormalize(vec3 v)
{
    float len2 = dot(v, v);
    if (len2 < 1e-8)
    {
        return vec3(0.0, 0.0, 1.0);
    }
    return v * inversesqrt(len2);
}

float GetSpotConeFactor(vec3 Ld, vec3 spotDir, float cosInner, float cosOuter)
{
    float theta = dot(Ld, -spotDir); // point->light dir against spotlight forward
    return Saturate((theta - cosOuter) / max(cosInner - cosOuter, 1e-4));
}

vec3 ClampDirToCone(vec3 u, vec3 axis, float cosOuter)
{
    float d = dot(u, axis);
    if (d >= cosOuter) return u;

    vec3 tangent = u - axis * d;
    float lenT = length(tangent);

    if (lenT < 1e-6)
    {
        tangent = normalize(abs(axis.z) < 0.999
            ? cross(axis, vec3(0.0, 0.0, 1.0))
            : cross(axis, vec3(0.0, 1.0, 0.0)));
    }
    else
    {
        tangent /= lenT;
    }

    float sinOuter = sqrt(max(1.0 - cosOuter * cosOuter, 0.0));
    return normalize(axis * cosOuter + tangent * sinOuter);
}
//~END calculate method

//~BEGIN PostProcessing
vec4 ACESFilm(vec4 inColor)
{
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((inColor * (a * inColor + b)) / (inColor * (c * inColor + d) + e), vec4(0.0),vec4(1.0));
}
//~END PostProcessing

void main()
{
    vec4 g0 = texture(GBuffer0Tex, uv);
    vec4 g1 = texture(GBuffer1Tex, uv);
    vec4 g2 = texture(GBuffer2Tex, uv);

    float ssao = texture(SSAOTex, uv).r;
    vec3 ssdo = texture(SSDOTex, uv).rgb;

    vec3 direct = texture(DirectLightTex, uv).rgb;

    vec3 worldPos = g2.xyz;
    vec3 N = normalize(g0.xyz * 2.0 - 1.0);
    float rough = g0.w;

    vec3 albedo = g1.rgb;
    float metal = g1.a;

    int materialType = int(g2.w + 0.5);

    vec3 V = normalize(WORLD.VIEW_POS.xyz - worldPos);
    vec3 R = reflect(-V, N);

    vec3 color = vec3(0.0);

    if (materialType == 0) // PBR
    {
        vec3 F0 = mix(vec3(0.04), albedo, metal);
        float NdotV = max(dot(N, V), 0.0);

        vec3 irradiance = textureLod(IRRADIANCE_TEX, N, 0.0).rgb;
        vec3 diffuseIBL = irradiance * albedo / PI;

        float mipLevel = rough * MAX_GGX_LOD;
        vec3 prefilteredColor = textureLod(ENV_TEX, R, mipLevel).rgb;
        vec2 brdf = texture(LUT_TEX, vec2(NdotV, rough)).rg;

        vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

        vec3 kS = F0;
        vec3 kD = (1.0 - kS) * (1.0 - metal);

        vec3 ibl = kD * diffuseIBL + specularIBL;

        if (PushConstant.SSMode == 0)          // None
        {
            color = direct + ibl;
        }
        else if (PushConstant.SSMode == 1)     // SSAO
        {
            color = (direct + ibl) * ssao;
        }
        else if (PushConstant.SSMode == 2)     // SSDO
        {
            color = direct + ibl + ssdo;
        }
        else
        {
            color = direct + ibl;
        }
    }
    else if (materialType == 1) // Lambertian
    {
        vec3 irradiance = texture(IRRADIANCE_TEX, N).rgb;
        vec3 ibl = irradiance * albedo / PI;

        if (PushConstant.SSMode == 0)          // None
        {
            color = direct + ibl;
        }
        else if (PushConstant.SSMode == 1)     // SSAO
        {
            color = direct + ibl * ssao;
        }
        else if (PushConstant.SSMode == 2)     // SSDO
        {
            color = direct + ibl + ssdo;
        }
        else
        {
            color = direct + ibl;
        }
    }
    else if (materialType == 2) // Mirror
    {
        color = textureLod(ENV_TEX, R, 0.0).rgb;
    }
    else if (materialType == 3) // Environment
    {
        color = textureLod(ENV_TEX, N, 0.0).rgb;
    }

    float exposure = pow(2.0, WORLD.AJUST_VAR.x);
    vec4 computeColor = vec4(color * exposure, 1.0);

    if (WORLD.AJUST_VAR.y == 0.0)
    {
        outColor = computeColor;
    }
    else if (WORLD.AJUST_VAR.y == 1.0)
    {
        outColor = pow(computeColor, vec4(1.0 / 2.2));
    }
    else if (WORLD.AJUST_VAR.y == 2.0)
    {
        outColor = computeColor / (computeColor + vec4(1.0));
    }
    else if (WORLD.AJUST_VAR.y == 3.0)
    {
        outColor = ACESFilm(computeColor);
    }
    else
    {
        outColor = computeColor;
    }
}