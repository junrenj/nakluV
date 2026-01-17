#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 position;

layout(push_constant) uniform Push
{
    float time;
};

void RippleCalculate(vec2 uv, float radiusMax, float radiusMin, float fadeInner, float fadeOuter, float thickness, inout vec4 finalRGBA, inout vec2 seed, vec2 offsetRange, float duration, float timeSpeed, vec3 colorRipple_1, vec3 colorRipple_2)
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
        finalRGBA += vec4(0, 0, 0, alpha);
        vec3 color = mix(colorRipple_1, colorRipple_2, sin(seed.y));
        finalRGBA = vec4(color, finalRGBA.w);
    }
}

void RainLine(vec2 uv, float lineWidth, float thickness, inout vec4 finalRGBA, inout vec2 seed, vec2 offsetRange, float widthRange, float duration, float timeSpeed, vec3 colorRainline_1, vec3 colorRainline_2)
{
    seed = fract(seed * 217.345);
    float randomOffsetX = mix(offsetRange.x, offsetRange.y, seed.x);
    float randomOffsetY = mix(offsetRange.x, offsetRange.y, seed.y);
    vec2 randomOffset = vec2(randomOffsetX, randomOffsetY);

    float cycle = duration + fract(randomOffset.x);
    float pulse = fract(sin(time / cycle));

    vec2 offset = (uv - 0.5) - randomOffset;

    float randomWidthRange = widthRange + 0.01 * sin(seed.y);

    float calAlpha = smoothstep(offset.x - lineWidth, offset.x, offset.y) - smoothstep(offset.x, offset.x + lineWidth, offset.y);
    float timeFrac = fract(timeSpeed * time + sin(seed.x)) - 0.5;

    if(uv.y <= timeFrac + randomWidthRange && uv.y >= timeFrac - randomWidthRange)
    {
        float finalAlpha = clamp(calAlpha - seed.x, 0, 1);
        finalRGBA += vec4(0, 0, 0, finalAlpha);
        vec3 color = mix(colorRainline_1, colorRainline_2, sin(seed.x));
        finalRGBA = vec4(color, finalRGBA.w);
    }
}

vec4 RainBubble()
{
    // Final Var
    vec4 finalRGBA = vec4(0,0,0,0);

    // Color Var
    vec3 colorBackground = vec3(103, 139, 159) / 255;
    vec3 colorRipple_1 = vec3(1, 1, 1);
    vec3 colorRipple_2 = vec3(1, 1, 1);

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
        RippleCalculate(uvOffset, radiusMax, radiusMin, fadeInner, fadeOuter, thickness, finalRGBA, seed, offsetRange, duration, timeSpeed, colorRipple_1, colorRipple_2);
    }

    // RainLine
    float rainLineNum = 300;
    float lineWidth = 0.005;
    float widthRange = 0.01;
    vec3 colorRainline_1 = vec3(1, 1, 1);
    vec3 colorRainline_2 = vec3(1, 1, 1);

    for(int i = 0; i < rainLineNum; i++)
    {
        RainLine(uvOffset, lineWidth, thickness, finalRGBA, seed, offsetRange, widthRange, duration, 1, colorRainline_1, colorRainline_2);
    }
    finalRGBA = clamp(finalRGBA, 0, 1);
    vec3 colorFinal = pow(mix(colorBackground, finalRGBA.xyz, finalRGBA.w), vec3(2.2));
    finalRGBA = vec4(colorFinal, finalRGBA.w);
    return clamp(finalRGBA, 0, 1);
}


void main() 
{
    vec4 Color = RainBubble();
    outColor = Color;
}
