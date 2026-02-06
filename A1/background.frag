#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 position;

layout(push_constant) uniform Push
{
    float time;
};

void main() 
{
    float value = time;
    outColor = vec4(time, time, time, 1);
}