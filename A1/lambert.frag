#version 450

struct Light
{
	uint TYPE;
	vec4 POSITION;
	vec4 DIRECTION;
	vec4 COLOR;
};

layout(set=1,binding=0,std140) uniform World 
{
	vec3 SKY_DIRECTION;
	vec3 SKY_ENERGY; 	// energy supplied by sky to a surface patch with normal = SKY_DIRECTION
	vec3 SUN_DIRECTION;
	vec3 SUN_ENERGY; 	// energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set=4,binding=0,std140) readonly buffer Lights
{
	Light[] LIGHTS;
};

layout(set=1,binding=1,std140) uniform Camera
{
	vec3 EYE;
};

layout(set=3,binding=0) uniform sampler2D ALBEDO_TEX;
layout(set=3,binding=1) uniform sampler2D ROUGHNESS_TEX;
layout(set=3,binding=2) uniform sampler2D METALNESS_TEX;
layout(set=3,binding=3) uniform sampler2D NORMAL_TEX;
layout(set=3,binding=4) uniform sampler2D DISPLACEMENT_TEX;

layout(set=5,binding=0) uniform sampler2D ENV_TEX;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;

layout(location=0) out vec4 outColor;


void main() 
{
	// Basic Info
	vec3 albedo = texture(ALBEDO_TEX, texcoord).rgb;
	vec3 nor = texture(NORMAL_TEX, texcoord).rgb;
	float rough = texture(ROUGHNESS_TEX, texcoord).r;
	float metal = texture(METALNESS_TEX, texcoord).r;
	vec3 env = texture(ENV_TEX, texcoord).rgb;
	// Dir
	vec3 n = normalize(normal);
	//vec3 v = normalize(position - EYE);

	// hemisphere sky + directional sun:
	vec3 e = SKY_ENERGY * (dot(n, SKY_DIRECTION) * 0.5 + 0.5)
        	+ SUN_ENERGY * max(dot(n, SUN_DIRECTION), 0.0);
    outColor = vec4(rough, rough, rough, 1.0);
}