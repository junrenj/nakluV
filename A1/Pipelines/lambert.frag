#version 450

const int DISPLACE_MAX_STEP = 32;
const float MAX_GGX_LOD = 5.0;

struct Light
{
	vec4 Position_Type;
	vec4 Direction_Radius;
	vec4 Color_Falloff;
	vec4 SpecialParams;
};

layout(push_constant) uniform PushConsts 
{
    int materialType;
};

layout(set=1,binding=0,std140) uniform World 
{
	vec3 SKY_DIRECTION;
	vec3 SKY_ENERGY; 	// energy supplied by sky to a surface patch with normal = SKY_DIRECTION
	vec3 SUN_DIRECTION;
	vec3 SUN_ENERGY; 	// energy supplied by sun to a surface patch with normal = SUN_DIRECTION
	vec3 EYE;
	vec2 AJUST_VAR;
};

layout(set=4,binding=0,std140) readonly buffer Lights
{
	Light[] LIGHTS;
};

layout(set=3,binding=0) uniform sampler2D ALBEDO_TEX;
layout(set=3,binding=1) uniform sampler2D ROUGHNESS_TEX;
layout(set=3,binding=2) uniform sampler2D METALNESS_TEX;
layout(set=3,binding=3) uniform sampler2D NORMAL_TEX;
layout(set=3,binding=4) uniform sampler2D DISPLACEMENT_TEX;

layout(set=5,binding=0) uniform samplerCube ENV_TEX;
layout(set=5,binding=1) uniform samplerCube IRRADIANCE_TEX;
layout(set=5,binding=2) uniform sampler2D LUT_TEX;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in mat3 TBN;

layout(location=0) out vec4 outColor;

//~BEGIN function for feature
vec4 Displacement()
{
	vec2 ddx = dFdx(texcoord);
    vec2 ddy = dFdy(texcoord);
	vec3 worldV = normalize(EYE - position);
	vec3 vTangent = transpose(TBN) * worldV;
	vec2 UVDist = vec2(vTangent.x, vTangent.y) / vTangent.z * 0.05;
	float rayHeight = 1.0;
	float stepSize = 1.0 / DISPLACE_MAX_STEP;
	vec2 outputUV;

	int i = 0;
	float oldRayDepth = 1;
	float oldTexDepth = texture(DISPLACEMENT_TEX, texcoord).r;
	float currentTexDepth;
	float yIntersect;
	vec2 uvOffset = vec2(0.0);
	while(i < DISPLACE_MAX_STEP + 2)
	{
		currentTexDepth = textureGrad(DISPLACEMENT_TEX, texcoord + uvOffset, ddx, ddy).r;

		if(rayHeight < currentTexDepth)
		{
			float xIntersect = (oldRayDepth - oldTexDepth) + (currentTexDepth - rayHeight);
			xIntersect = (currentTexDepth - rayHeight) / xIntersect;
			yIntersect = (oldRayDepth * xIntersect) + (rayHeight * (1.0 - xIntersect));
			uvOffset -= (xIntersect * UVDist);
			break;
		}

		oldRayDepth = rayHeight;
		rayHeight -= stepSize;
		uvOffset += UVDist * stepSize;
		oldTexDepth = currentTexDepth;

		i++;
	}

	vec4 outputValue;
	outputValue.xy = uvOffset + texcoord;
	outputValue.z = yIntersect;
	outputValue.w = 1;

	return outputValue;
}

vec3 NormalFromTexture()
{
	vec3 nTangent = texture(NORMAL_TEX, texcoord).rgb;
	
	nTangent = nTangent * 2.0 - 1.0;

	vec3 nWorld = normalize(TBN * nTangent);
	return nWorld;
}
//~END function for feature

//~BEGIN function for lights
float GetDistanceAttenuation(float dist, float lightRadius) 
{
    float divide = dist * dist + 1.0;
    float falloff = pow(max(0.0, 1.0 - pow(dist / lightRadius, 4.0)), 2.0);
    return falloff / divide;
}

float GetAngleAttenuation(vec3 L, vec3 lightDir, float innerAngle, float outerAngle)
{
	float cosTheta = dot(L, -lightDir);
	float scale = 1.0 / max(0.001, cosTheta - outerAngle);
	float offset = -outerAngle * scale;
	return clamp(cosTheta * scale + offset, 0.0, 1.0);
}
//~END function for lights


// function for different material calculation
vec4 Mirror()
{
	vec3 nWorld = NormalFromTexture();
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, nWorld);
	vec3 env = textureLod(ENV_TEX, r, 0.0).rgb;
	return vec4(env, 1.0);
}

vec4 Environment()
{
	vec3 nWorld = NormalFromTexture();
	vec3 env = textureLod(ENV_TEX, nWorld, 0.0).rgb;
	return vec4(env, 1.0);
}

vec4 PBR(vec2 uv)
{
	// 0. Basic Info
	vec3 albedo = texture(ALBEDO_TEX, uv).rgb;
	float rough = texture(ROUGHNESS_TEX, uv).r;
	float metal = texture(METALNESS_TEX, uv).r;

	// 1. Dir
	vec3 nWorld = NormalFromTexture();
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, nWorld);

	float NdotV = max(dot(nWorld, v), 0.0);

	// 2. F0
    vec3 F0 = mix(vec3(0.04), albedo, metal);

    // 3. Diffuse
    vec3 irradiance = textureLod(IRRADIANCE_TEX, nWorld, 0.0).rgb;
    vec3 diffAlbedo = albedo * (1.0 - metal);
    vec3 diffuse = irradiance * diffAlbedo;

    // 4. Specular
    float maxMip = MAX_GGX_LOD;
    float mipLevel = rough * maxMip;

    vec3 prefilteredColor = textureLod(ENV_TEX, r, mipLevel).rgb;

    vec2 brdf = texture(LUT_TEX, vec2(NdotV, rough)).rg;

    vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);

    // Energy Conservation
    vec3 kS = F0;
    vec3 kD = (1.0 - kS) * (1.0 - metal);

    vec3 color = kD * diffuse + specular;

    return vec4(color, 1.0);
}

vec4 Lambertian(vec2 uv)
{
	// Basic Info
	vec3 albedo = texture(ALBEDO_TEX, uv).rgb;
	// Dir
	vec3 nWorld = NormalFromTexture();
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, nWorld);

	vec3 irradiance = texture(IRRADIANCE_TEX, nWorld).rgb;

	// hemisphere sky + directional sun:
	vec3 e = SKY_ENERGY * (dot(nWorld, SKY_DIRECTION) * 0.5 + 0.5)
		+ SUN_ENERGY * max(dot(nWorld, SUN_DIRECTION), 0.0);

	return vec4(albedo / 3.1415926 * e + irradiance * albedo , 1.0);
}

vec4 ACESFilm(vec4 inColor)
{
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((inColor * (a * inColor + b)) / (inColor * (c * inColor + d) + e), vec4(0.0),vec4(1.0));
}

void main() 
{
	vec4 computeColor = vec4(0.0);
	if(materialType == 0)	// PBR
	{
		// vec2 uv = Displacement().xy;
		computeColor = PBR(texcoord);
	}
	else if(materialType == 1) // Lambertian
	{
		// vec2 uv = Displacement().xy;
		computeColor = Lambertian(texcoord);
	}
	else if(materialType == 2)	// Mirror
	{
		computeColor = Mirror();
	}
	else if(materialType == 3)	// Environment
	{
		computeColor = Environment();
	}

	// exposure
	float exposure = pow(2, AJUST_VAR.x);
	computeColor *= exposure;

	// Tonemapping
	if(AJUST_VAR.y == 0)
	{
		// 0. linear
		outColor = computeColor;
	}
	else if(AJUST_VAR.y == 1)
	{
		// 1. simple gamma
		outColor = pow(computeColor, vec4(1.0/2.2));
	}
	else if(AJUST_VAR.y == 2)
	{
		// 2. reinhard
		outColor = computeColor / (computeColor + vec4(1.0));
	}
	else if(AJUST_VAR.y == 3)
	{
		// 3. ACES
		outColor = ACESFilm(computeColor);
	}
}