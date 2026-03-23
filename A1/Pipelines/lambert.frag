#version 450

const int DISPLACE_MAX_STEP = 32;
const float MAX_GGX_LOD = 5.0;
const float PI = 3.1415926;
const int MAX_SPOT_SHADOWS = 64;

struct Light
{
	// 0. Lighting
	vec4 POSITION_TYPE;
	vec4 DIRECTION_LIMIT;
	vec4 COLOR_FALLOFF;
	/*
        Sun[Angle, 0, 0, 0]
        Sphere[SourceRadius, 0, 0, 0]
        Spot[SourceRadius, cosInner, cosOuter, 0]
    */
	vec4 SPECIALPARAMS;
	// 1. Shadow
	mat4 LIGHT_FROM_WORLD;
	vec4 SHADOW_INFO;	// x -> shadowmapIdx
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

layout(set=6, binding=0) uniform sampler2DShadow SpotShadowMaps[MAX_SPOT_SHADOWS];

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in mat3 TBN;

layout(location=0) out vec4 outColor;

//~BEGIN calculate method
float Saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}
//~END calculate method

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
float GetDistanceAttenuation(float dist, float limit) 
{
    float x = dist / max(limit, 1e-4);
    float falloff = Saturate(1.0 - x * x * x * x);
	falloff *= falloff;

    return falloff / (dist * dist + 1.0);
}

float SampleSpotShadowPCF(Light light, vec3 N)
{
    if (light.SHADOW_INFO.y == 0)
    {
        return 1.0;
    }

    int shadowMapIndex = int(light.SHADOW_INFO.x);
    if (shadowMapIndex < 0)
    {
        return 1.0;
    }

    vec3 L = normalize(light.POSITION_TYPE.xyz - position);
    float NDotL = max(dot(N, L), 0.0);

    float depthBias = max(0.0005 * (1.0 - NDotL), 0.0001);
    float normalBias = 0.001;

    vec3 biasedWorldPos = position + N * normalBias;

    vec4 shadowClip = light.LIGHT_FROM_WORLD * vec4(biasedWorldPos, 1.0);
    if (shadowClip.w <= 0.0)
    {
        return 1.0;
    }

    vec3 ndc = shadowClip.xyz / shadowClip.w;

    vec2 uv = ndc.xy * 0.5 + 0.5;
    float z = ndc.z - depthBias;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }
    if (z < 0.0 || z > 1.0)
    {
        return 1.0;
    } 

    vec2 texelSize = 1.0 / vec2(textureSize(SpotShadowMaps[shadowMapIndex], 0));

    float visibility = 0.0;
    visibility += texture(SpotShadowMaps[shadowMapIndex], vec3(uv + vec2(-0.5, -0.5) * texelSize, z));
    visibility += texture(SpotShadowMaps[shadowMapIndex], vec3(uv + vec2( 0.5, -0.5) * texelSize, z));
    visibility += texture(SpotShadowMaps[shadowMapIndex], vec3(uv + vec2(-0.5,  0.5) * texelSize, z));
    visibility += texture(SpotShadowMaps[shadowMapIndex], vec3(uv + vec2( 0.5,  0.5) * texelSize, z));

    return visibility * 0.25;
}

vec3 GGXSpecularRepresentative(
    vec3 N, vec3 V, vec3 L,
    vec3 lightColor,
    vec3 albedo,
    float rough,
    float metalness,
    float areaNormalization
)
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    float alpha = rough * rough;
    float alpha2 = alpha * alpha;

    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (PI * denom * denom);

    // keep original GGX, only multiply the normalization factor
    D *= areaNormalization;

    // Schlick-GGX geometry, keep original roughness/alpha
    float k = (rough + 1.0);
    k = (k * k) / 8.0;

    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float Gl = NdotL / (NdotL * (1.0 - k) + k);
    float G = Gv * Gl;

    vec3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 1e-4);

    return specular * lightColor * NdotL;
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

vec3 LambertLighting(vec3 N, vec3 L, vec3 lightColor, vec3 albedo)
{
    float NdotL = max(dot(N, L),0.0);

    vec3 diffuse = albedo / 3.1415926;

    return diffuse * lightColor * NdotL;
}

vec3 CalSunLight_PBR(Light sun, vec3 N, vec3 V, vec3 albedo, float rough, float metal)
{
	vec3 L = normalize(sun.DIRECTION_LIMIT.xyz);
	vec3 color = sun.COLOR_FALLOFF.xyz;

	vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L),0.0);
    float NdotV = max(dot(N, V),0.0);
    float NdotH = max(dot(N, H),0.0);
    float VdotH = max(dot(V, H),0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metal);

    // Fresnel
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // GGX Distribution
    float a = rough * rough;
    float a2 = a * a;

    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    float D = a2/(3.1415926 * denom * denom);

    // Geometry
    float k = (rough + 1.0);
    k = k * k / 8.0;

    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float Gl = NdotL / (NdotL * (1.0 - k) + k );

    float G = Gv * Gl;

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.001;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metal);

    vec3 diffuse = kD * albedo / 3.1415926;

    return (diffuse + specular) * color * NdotL;
}

vec3 CalSphereLight_PBR(Light sphere, vec3 N, vec3 V, vec3 R, vec3 albedo, float rough, float metal)
{
    vec3 lightPos = sphere.POSITION_TYPE.xyz;

    float sourceRadius = sphere.SPECIALPARAMS.x; // emitter size
    float limit        = sphere.DIRECTION_LIMIT.w; // falloff radius / influence limit

    vec3 L = lightPos - position;
    float dist = length(L);
    vec3 Ld = L / max(dist, 1e-4);

    // Representative point for specular
    float LdotR = dot(L, R);
    vec3 centerToRay = LdotR * R - L;
    float distToRay = length(centerToRay);

    float t = clamp(sourceRadius / max(distToRay, 1e-4), 0.0, 1.0);
    vec3 closestPos = L + centerToRay * t;
    vec3 Ls = normalize(closestPos);

    // Falloff uses LIMIT, not source radius
    float attenuation = GetDistanceAttenuation(dist, limit);
    vec3 lightColor = sphere.COLOR_FALLOFF.xyz * attenuation;

    // Diffuse: point light at center
    vec3 H = normalize(V + Ld);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metal);

    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metal);

    vec3 diffuse = kD * albedo / PI * lightColor * max(dot(N, Ld), 0.0);

    // Specular normalization from Epic representative-point method
    float alpha = rough * rough;
    float alphaPrime = clamp(alpha + sourceRadius / max(2.0 * dist, 1e-4), alpha, 1.0);
    float areaNormalization = (alpha / alphaPrime);
    areaNormalization *= areaNormalization;

    vec3 specular = GGXSpecularRepresentative(
        N, V, Ls,
        lightColor,
        albedo,
        rough,
        metal,
        areaNormalization
    );

    return diffuse + specular;
}

vec3 CalSpotLight_PBR(Light spot, vec3 N, vec3 V, vec3 R, vec3 albedo, float rough, float metal)
{
    vec3 lightPos = spot.POSITION_TYPE.xyz;
    vec3 dir = SafeNormalize(spot.DIRECTION_LIMIT.xyz);   // spotlight forward
    vec3 coneAxis = -dir;                                 // emitting axis

    float sourceRadius = spot.SPECIALPARAMS.x;
    float cosInner = spot.SPECIALPARAMS.y;
    float cosOuter = spot.SPECIALPARAMS.z;
    float limit = spot.DIRECTION_LIMIT.w;

    // from shading point to light center
    vec3 L = lightPos - position;
    float dist = length(L);
    vec3 Ld = (dist > 1e-4) ? (L / dist) : coneAxis;

    // diffuse / overall visibility cone factor
    float spotFactor = GetSpotConeFactor(Ld, dir, cosInner, cosOuter);

    // if outside cone, no contribution
    if (spotFactor <= 0.0)
    {
        return vec3(0.0);
    } 

    float attenuation = GetDistanceAttenuation(dist, limit);
    vec3 lightColor = spot.COLOR_FALLOFF.xyz * attenuation * spotFactor;

    // diffuse: use center point
    vec3 H = normalize(V + Ld);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metal);

    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metal);

    vec3 diffuse = kD * albedo / PI * lightColor * max(dot(N, Ld), 0.0);

    // ----- specular representative point -----
    // 1) sphere-style closest point candidate
    float LdotR = dot(L, R);
    vec3 centerToRay = LdotR * R - L;
    float distToRay = length(centerToRay);

    float t = clamp(sourceRadius / max(distToRay, 1e-4), 0.0, 1.0);
    vec3 closestPosLocal = L + centerToRay * t;   // vector from shading point to candidate point
    vec3 candidateDirFromLight = SafeNormalize((position + closestPosLocal) - lightPos);

    // 2) clamp candidate direction to spotlight cone
    vec3 clampedDirFromLight = ClampDirToCone(candidateDirFromLight, coneAxis, cosOuter);

    // 3) rebuild representative point on emitter surface
    vec3 repPoint = lightPos + clampedDirFromLight * sourceRadius;
    vec3 Ls = SafeNormalize(repPoint - position);

    // optional: specular cone factor can be recomputed with representative direction
    float specTheta = dot(Ls, coneAxis);
    float specSpotFactor = Saturate((specTheta - cosOuter) / max(cosInner - cosOuter, 1e-4));

    if (specSpotFactor <= 0.0)
    {
        return diffuse;
    }

    vec3 specLightColor = spot.COLOR_FALLOFF.xyz * attenuation * specSpotFactor;

    // Epic-style normalization, still based on subtended size
    float alpha = rough * rough;
    float alphaPrime = clamp(alpha + sourceRadius / max(2.0 * dist, 1e-4), alpha, 1.0);
    float areaNormalization = alpha / alphaPrime;
    areaNormalization *= areaNormalization;

    vec3 specular = GGXSpecularRepresentative(
        N, V, Ls,
        specLightColor,
        albedo,
        rough,
        metal,
        areaNormalization
    );

    vec3 result = diffuse + specular;

    if (any(isnan(result)) || any(isinf(result)))
        return vec3(0.0);
    
    float shadowVis = SampleSpotShadowPCF(spot, N);

    return result * shadowVis;
}

vec3 CalDirectLightings(vec3 nWorld, vec3 v, vec3 r, vec3 albedo, float rough, float metal)
{
	vec3 result = vec3(0);
	 if (lightsCount <= 0) return vec3(1, 0, 1); 
	for(int i = 0 ; i < lightsCount ; i++)
	{
		Light light = LIGHTS[i];

		int type = int(light.POSITION_TYPE.w);

		if(type == 0)
		{
			result += CalSunLight_PBR(light, nWorld, v, albedo, rough, metal);
		}
		else if(type == 1)
		{
			result += CalSphereLight_PBR(light, nWorld, v, r, albedo, rough, metal);
		}
		else if(type == 2)
		{
			result += CalSpotLight_PBR(light, nWorld, v, r, albedo, rough, metal);
		}
	}

	return result;
}

vec3 CalSun_Lambert(Light sun, vec3 N, vec3 albedo)
{
    vec3 L = normalize(sun.DIRECTION_LIMIT.xyz);

    vec3 color = sun.COLOR_FALLOFF.xyz;

    return LambertLighting(N, L, color, albedo);
}

vec3 CalSphere_Lambert(Light sphere, vec3 N, vec3 albedo)
{
    vec3 lightPos = sphere.POSITION_TYPE.xyz;
	float lightRadius = sphere.SPECIALPARAMS.x;
	float lightLimit = sphere.DIRECTION_LIMIT.w;

    vec3 toLight = lightPos - position;
    float dist = length(toLight);
    vec3 L = toLight / max(dist, 1e-5);

	float surfaceDist = max(dist - lightRadius, 0.0);
    float physicalAttenuation = 1.0 / max(surfaceDist * surfaceDist, 1e-4);

	float x = surfaceDist / lightLimit;
	float limitAttenuation = GetDistanceAttenuation(dist, lightLimit);

    vec3 color = sphere.COLOR_FALLOFF.xyz * limitAttenuation;

    return LambertLighting(N, L, color, albedo);
}

vec3 CalSpot_Lambert(Light spot, vec3 N, vec3 albedo)
{
    vec3 lightPos = spot.POSITION_TYPE.xyz;
    vec3 dir = normalize(spot.DIRECTION_LIMIT.xyz);

    vec3 L = lightPos - position;

    float dist = length(L);
    L /= dist;

    float attenuation = GetDistanceAttenuation(dist, spot.DIRECTION_LIMIT.w);

    float theta = dot(L, -dir);

    float cosInner = spot.SPECIALPARAMS.y;
    float cosOuter = spot.SPECIALPARAMS.z;

    float result = clamp((theta - cosOuter)/(cosInner - cosOuter), 0.0, 1.0);

    float shadowVis = SampleSpotShadowPCF(spot, N);
    vec3 color = spot.COLOR_FALLOFF.xyz * attenuation * result;

    return LambertLighting(N, L, color, albedo) * shadowVis;
}

vec3 CalDirectLightings_Lambert(vec3 nWorld, vec3 albedo)
{
	vec3 result = vec3(0, 0, 0);

    for(int i = 0 ; i < lightsCount ; i++)
    {
        Light light = LIGHTS[i];

        int type = int(light.POSITION_TYPE.w);

        if(type == 0)
		{
            result += CalSun_Lambert(light, nWorld, albedo);
		}
        else if(type == 1)
		{
			result += CalSphere_Lambert(light, nWorld, albedo);
		}
        else if(type == 2)
		{
			result += CalSpot_Lambert(light, nWorld, albedo);
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
	vec3 direct = CalDirectLightings(nWorld, v, r, albedo, rough, metal);

	// 4. Diffuse IBL
    vec3 irradiance = textureLod(IRRADIANCE_TEX, nWorld, 0.0).rgb;
    vec3 diffuseIBL = irradiance * albedo / PI;

    // 4. Specular IBL
    float mipLevel = rough * MAX_GGX_LOD;
    vec3 prefilteredColor = textureLod(ENV_TEX, r, mipLevel).rgb;
    vec2 brdf = texture(LUT_TEX, vec2(NdotV, rough)).rg;

    vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);

    // 5. Energy Conservation
    vec3 kS = F0;
    vec3 kD = (1.0 - kS) * (1.0 - metal);

	// Combine IBL
    vec3 ibl = kD * diffuseIBL + specular;

	vec3 color = direct + ibl;

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

	vec3 direct = CalDirectLightings_Lambert(nWorld, albedo);

	vec3 ibl = irradiance * albedo / PI;

	vec3 color = direct + ibl; 

	return vec4(color , 1.0);
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