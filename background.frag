#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 position;

layout(push_constant) uniform Push
{
    float time;
};

void RippleMask(vec2 uv, float radiusMax, float radiusMin, float fadeInner, float fadeOuter, float thickness, inout float finalAlpha, inout vec2 seed, vec2 offsetRange, float duration, float timeSpeed)
{
    seed = fract(seed * 782.109);
    float randomOffsetX = mix(offsetRange.x, offsetRange.y, seed.x);
    float randomOffsetY = mix(offsetRange.x, offsetRange.y, seed.y);
    vec2 randomOffset = vec2(randomOffsetX, randomOffsetY);

    float cycle = duration + fract(randomOffset.x);
    float pulse = fract(time / cycle);

    float radius = radiusMin + pulse * (radiusMax - radiusMin);

    vec2 offset = (uv - 0.5) - randomOffset;

    float dist = length(offset);

    float radiusLimit = radiusMin + (seed.y) * (radiusMax - radiusMin);

    float alpha = clamp(smoothstep(radius - fadeInner, radius + fadeInner, dist),
                        0.0,
                        1.0);
    
    alpha *= clamp(1.0 - smoothstep(radiusLimit - fadeOuter, radiusLimit + fadeOuter, dist),
                        0.0,
                        1.0);

    if(dist >= radius - thickness &&
         dist <= radius + thickness)
    {
        finalAlpha += alpha;
    }
}

void RainLine(vec2 uv, float lineWidth, float thickness, inout float finalAlpha, inout vec2 seed, vec2 offsetRange, float widthRange, float duration, float timeSpeed)
{
    seed = fract(seed * 217.345);
    float randomOffsetX = mix(offsetRange.x, offsetRange.y, seed.x);
    float randomOffsetY = mix(offsetRange.x, offsetRange.y, seed.y);
    vec2 randomOffset = vec2(randomOffsetX, randomOffsetY);

    float cycle = duration + fract(randomOffset.x);
    float pulse = fract(time / cycle);

    vec2 offset = (uv - 0.5) - randomOffset;

    float calAlpha = smoothstep(offset.x - lineWidth, offset.x, offset.y) - smoothstep(offset.x, offset.x + lineWidth, offset.y);

    float timeFrac = fract(time);
    if(uv.y <= timeFrac + widthRange && uv.y >= timeFrac - widthRange)
    {
        finalAlpha += clamp(calAlpha - seed.x, 0, 1) * pulse;
    }

}

vec4 RainBubble()
{
    // Final Var
    vec4 result = vec4(0,0,0,0);
    float finalAlpha = 0.0;

    // Color Var
    vec3 colorBackground = vec3(0.427, 0.823, 0.859);
    vec3 colorRipple = vec3(1, 1, 1);

    // Ripple Var
    float radius = 0.05;
    float radiusMin = 0.01;
    float radiusMax = 0.2;
    float thickness = 0.003;
    float fadeInner = 0.003;
    float fadeOuter = 0.01;
    float duration = 1.0;
    float dropsNum = 200;
    float timeSpeed = 0.1;

    vec2 centerPos = vec2(0.5, 0.5);
    vec2 uvOffset = position - centerPos;

    vec2 seed = vec2(137.284, 789.012);
    vec2 offsetRange = vec2(-1, 1);

    // Ripple
    for(int i = 0; i < dropsNum; i++)
    {
        RippleMask(uvOffset, radiusMax, radiusMin, fadeInner, fadeOuter, thickness, finalAlpha, seed, offsetRange, duration, timeSpeed);
    }

    // RainLine
    float rainLineNum = 50;
    float lineWidth = 0.005;
    float widthRange = 0.01;
    for(int i = 0; i < rainLineNum; i++)
    {
        RainLine(uvOffset, lineWidth, thickness, finalAlpha, seed, offsetRange, widthRange, duration, 1);
    }

    vec3 colorFinal = mix(colorBackground, colorRipple, finalAlpha);

    result = vec4(colorFinal, finalAlpha);

    return clamp(result, 0, 1);
}


void main() 
{
    vec4 Color = RainBubble();
    outColor = Color;
}
