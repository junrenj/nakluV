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

layout(set=3,binding=0) uniform sampler2D TEXTURE;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;

layout(location=0) out vec4 outColor;


void main() 
{
	vec3 n = normalize(normal);
	vec3 albedo = texture(TEXTURE, texcoord).rgb;

	// hemisphere sky + directional sun:
	vec3 e = SKY_ENERGY * (dot(n, SKY_DIRECTION) * 0.5 + 0.5)
        	+ SUN_ENERGY * max(dot(n, SUN_DIRECTION), 0.0);
    outColor = vec4(albedo / 3.1415926 * e, 1.0);
}