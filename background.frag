#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 position;

layout(push_constant) uniform Push
{
    float time;
};

void BubbleColor(vec2 uv, float radius, float fadeInner, float thickness, inout vec4 result, inout vec2 seed, vec2 offsetRange)
{
    seed = fract(seed * 172.351);
    float randomOffsetX = mix(offsetRange.x, offsetRange.y, seed.x);
    float randomOffsetY = mix(offsetRange.x, offsetRange.y, seed.y);
    vec2 randomOffset = vec2(randomOffsetX, randomOffsetY);

    vec2 offset = (uv - 0.5) - randomOffset;
    float dist = length(offset);

    float alpha = clamp(smoothstep(radius - fadeInner, radius + fadeInner, dist),
                        0.0,
                        1.0);

    if(dist >= radius - thickness &&
         dist <= radius + thickness)
    {
        result += alpha;
    }
}

vec4 RainBubble()
{
    vec4 result = vec4(0,0,0,0);
    vec2 centerPos = vec2(0.5, 0.5);
    vec2 uvOffset = position - centerPos;

    float radius = 0.05;
    float radiusMax = 0.1;
    float thickness = 0.005;
    float fadeInner = 0.005;

    vec2 seed = vec2(198.235, 537.129);
    vec2 offsetRange = vec2(-1, 1);

    float drops = 100;

    for(int i = 0; i < drops; i++)
    {
        BubbleColor(position, radius, fadeInner, thickness, result, seed, offsetRange);
    }

    return result;
}


void main() 
{
    vec4 Color = RainBubble();
    outColor = vec4(fract(position.x + time), position.y, 0.0, 1.0);
    outColor = Color;
}
