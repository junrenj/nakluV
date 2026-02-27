#version 450

struct Light
{
	uint TYPE;
	vec4 POSITION;
	vec4 DIRECTION;
	vec4 COLOR;
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

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;

layout(location=0) out vec4 outColor;


vec4 Mirror()
{
	vec3 n = normalize(normal);
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, n);
	vec3 env = texture(ENV_TEX, r).rgb;
	return vec4(env, 1.0);
}

vec4 Environment()
{
	vec3 n = normalize(normal);
	vec3 env = texture(ENV_TEX, n).rgb;
	return vec4(env, 1.0);
}

vec4 PBR()
{
	// Basic Info
	vec3 albedo = texture(ALBEDO_TEX, texcoord).rgb;
	float rough = texture(ROUGHNESS_TEX, texcoord).r;
	float metal = texture(METALNESS_TEX, texcoord).r;
	float displace = texture(DISPLACEMENT_TEX, texcoord).r;
	vec3 nor = texture(NORMAL_TEX, texcoord).rgb;

	// Dir
	vec3 n = normalize(normal);
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, n);

	// Test
	vec3 irradiance = texture(IRRADIANCE_TEX, n).rgb;
	vec3 diffuse = albedo * (1.0 - metal); 

	return vec4(1,1,1,1);	// TODO: finish it
}

vec4 Lambertian()
{
	// Basic Info
	vec3 albedo = texture(ALBEDO_TEX, texcoord).rgb;
	vec3 nor = texture(NORMAL_TEX, texcoord).rgb;
	float displace = texture(DISPLACEMENT_TEX, texcoord).r;
	// Dir
	vec3 n = normalize(normal);
	vec3 v = normalize(EYE - position);
	vec3 r = reflect(-v, n);

	vec3 irradiance = texture(IRRADIANCE_TEX, n).rgb;

	// hemisphere sky + directional sun:
	vec3 e = SKY_ENERGY * (dot(n, SKY_DIRECTION) * 0.5 + 0.5)
		+ SUN_ENERGY * max(dot(n, SUN_DIRECTION), 0.0);

	return vec4(albedo / 3.1415926 * e + irradiance * albedo , 1.0);
}

void main() 
{
	vec4 computeColor = vec4(0.0);
	if(materialType == 0)	// PBR
	{
		computeColor = PBR();
	}
	else if(materialType == 1) // Lambertian
	{
		computeColor = Lambertian();
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
}