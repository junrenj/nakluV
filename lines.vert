#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec4 Color;

layout(location=0) out vec4 color;

layout(push_constant) uniform Push
{
    float time;
};

layout(set=0, binding=0, std140) uniform Camera
{
	mat4 CLIP_FROM_WORLD;
};

void main() 
{
	float scale = clamp((1 - 0.1f * time), 0.000028, 1);
	gl_Position = CLIP_FROM_WORLD * vec4(Position * scale, 1.0);
	color = Color;
}
