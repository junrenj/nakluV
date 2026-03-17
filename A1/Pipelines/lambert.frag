#version 450

const int DISPLACE_MAX_STEP = 32;
const float MAX_GGX_LOD = 5.0;

struct Light
{
	vec4 POSITION_TYPE;
	vec4 DIRECTION_RADIUS;
	vec4 COLOR_FALLOFF;
	vec4 SPECIALPARAMS;
};

layout(push_constant) uniform PushConsts 
{
    int materialType;
	int lightsCount;
};

layout(set=1,binding=0,std140) uniform World 
{
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
	// No normal map case
	if(nTangent == vec3(0, 0, 0))
	{
		return normal;
	}
	
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

vec3 PBRLighting(vec3 N, vec3 V, vec3 L, vec3 lightColor, vec3 albedo, float roughness, float metalness)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L),0.0);
    float NdotV = max(dot(N, V),0.0);
    float NdotH = max(dot(N, H),0.0);
    float VdotH = max(dot(V, H),0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    // Fresnel
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // GGX Distribution
    float a = roughness * roughness;
    float a2 = a * a;

    float denom = (NdotH * NdotH) * (a2 - 1.0)+1.0;
    float D = a2/(3.1415926 * denom * denom);

    // Geometry
    float k = (roughness + 1.0);
    k = k * k / 8.0;

    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float Gl = NdotL / (NdotL * (1.0 - k) + k );

    float G = Gv*Gl;

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.001;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 diffuse = kD * albedo / 3.1415926;

    return (diffuse + specular) * lightColor * NdotL;
}

vec3 CalSunLight(Light sun, vec3 N, vec3 V, vec3 albedo, float rough, float metal)
{
	vec3 L = normalize(-sun.DIRECTION_RADIUS.xyz);
	vec3 color = sun.COLOR_FALLOFF.xyz;

	return PBRLighting(N, V, L, color, albedo, rough, metal);
}

vec3 CalSphereLight(Light sphere, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float rough, float metal)
{
    vec3 lightPos = sphere.POSITION_TYPE.xyz;

    vec3 L = lightPos - worldPos;
    float dist = length(L);
    L /= dist;

    float attenuation = 1.0 / (dist * dist);

    vec3 color = sphere.COLOR_FALLOFF.xyz * attenuation;

    return PBRLighting(N, V, L, color, albedo, rough, metal);
}

vec3 CalSpotLight(Light spot, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float rough, float metal)
{
    vec3 lightPos = spot.POSITION_TYPE.xyz;
    vec3 dir = normalize(spot.DIRECTION_RADIUS.xyz);

    vec3 L = lightPos - worldPos;
    float dist = length(L);
    L /= dist;

    float attenuation = 1.0/(dist * dist);

    float theta = dot(L, -dir);

    float cosInner = spot.SPECIALPARAMS.x;
    float cosOuter = spot.SPECIALPARAMS.y;

    float result = clamp((theta - cosOuter)/(cosInner - cosOuter), 0.0, 1.0);

    vec3 color = spot.COLOR_FALLOFF.xyz * attenuation * result;

    return PBRLighting(N, V, L, color, albedo, rough, metal);
}

vec3 CalDirectLightings(vec3 position, vec3 nWorld, vec3 v, vec3 albedo, float rough, float metal)
{
	vec3 result = vec3(0);
	for(int i = 0 ; i < lightsCount ; i++)
	{
		Light light = LIGHTS[i];

		int type = int(light.POSITION_TYPE.w);

		if(type == 0)
		{
			result += CalSunLight(light, nWorld, v, albedo, rough, metal);
		}
		else if(type == 1)
		{
			result += CalSphereLight(light, position, nWorld, v, albedo, rough, metal);
		}
		else if(type == 2)
		{
			result += CalSpotLight(light, position, nWorld, v, albedo, rough, metal);
		}
	}

	return result;
}

vec3 LambertLighting(vec3 N, vec3 L, vec3 lightColor, vec3 albedo)
{
    float NdotL = max(dot(N,L),0.0);

    vec3 diffuse = albedo / 3.1415926;

    return diffuse * lightColor * NdotL;
}

vec3 CalSun_Lambert(Light sun, vec3 N, vec3 albedo)
{
    vec3 L = normalize(sun.DIRECTION_RADIUS.xyz);

    vec3 color = sun.COLOR_FALLOFF.xyz;

    return LambertLighting(N,L,color,albedo);
}

vec3 CalSphere_Lambert(Light sphere, vec3 worldPos, vec3 N, vec3 albedo)
{
    vec3 lightPos = sphere.POSITION_TYPE.xyz;

    vec3 L = lightPos - worldPos;
    float dist = length(L);
    L /= dist;

    float attenuation = 1.0 / (dist * dist);

    vec3 color = sphere.COLOR_FALLOFF.xyz * attenuation;

    return LambertLighting(N,L,color,albedo);
}

vec3 CalSpot_Lambert(Light spot, vec3 worldPos, vec3 N, vec3 albedo)
{
    vec3 lightPos = spot.POSITION_TYPE.xyz;
    vec3 dir = normalize(spot.DIRECTION_RADIUS.xyz);

    vec3 L = lightPos - worldPos;

    float dist = length(L);
    L /= dist;

    float attenuation = 1.0/(dist*dist);

    float theta = dot(L,-dir);

    float cosInner = spot.SPECIALPARAMS.x;
    float cosOuter = spot.SPECIALPARAMS.y;

    float result = clamp((theta - cosOuter)/(cosInner - cosOuter),0.0,1.0);

    vec3 color = spot.COLOR_FALLOFF.xyz * attenuation * result;

    return LambertLighting(N, L, color, albedo);
}

vec3 CalDirectLightings_Lambert(vec3 pos, vec3 nWorld, vec3 albedo)
{
	vec3 result = vec3(0, 0, 0);

    for(int i = 0 ; i < lightsCount ; i++)
    {
        Light light = LIGHTS[i];

        int type = int(light.POSITION_TYPE.w);

        if(type == 0)
		{
            result += CalSun_Lambert(light,nWorld,albedo);
		}
        else if(type == 1)
		{
			result += CalSphere_Lambert(light,pos,nWorld,albedo);
		}
        else if(type == 2)
		{
			result += CalSpot_Lambert(light,pos,nWorld,albedo);
		}
    }

	return result;
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

	vec3 direct = CalDirectLightings_Lambert(position, nWorld, albedo);

	vec3 ibl = irradiance * albedo;

	vec3 color = direct + ibl; 

	return vec4(direct , 1.0);
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