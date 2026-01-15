#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 position;

layout(push_constant) uniform Push
{
    float time;
};

vec4 RainBubble()
{
    vec4 result = vec4(0,0,0,0);
    vec2 centerPos = vec2(0.5, 0.5);
    vec2 uvOffset = position - centerPos;
    float radius = 0.05;

    float dist = length(uvOffset);

    return result;
}

void main() 
{
    // vec4 Color = RainBubble();
    // outColor = ;
    outColor = vec4(fract(position.x + time), position.y, 0.0, 1.0);
}
