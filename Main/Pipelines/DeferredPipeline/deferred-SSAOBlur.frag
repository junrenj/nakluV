#version 450

layout(set = 0, binding = 0) uniform sampler2D SSAOTex; // Original SSAO
layout(set = 0, binding = 1) uniform sampler2D GBuffer_NormalWS; // Normal
layout(set = 0, binding = 2) uniform sampler2D GBuffer_PostionWS; // Position

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 DecodeNormal(vec3 InN)
{
    return normalize(InN * 2.0 - 1.0);
}

void main()
{
    vec2 texSize = vec2(textureSize(SSAOTex, 0));
    vec2 texel = 1.0 / texSize;

    float centerAO = texture(SSAOTex, uv).r;
    vec3 centerNormal = DecodeNormal(texture(GBuffer_NormalWS, uv).rgb);
    vec3 centerPos = texture(GBuffer_PostionWS, uv).rgb;

    if (length(centerPos) < 1e-4)
    {
        outColor = vec4(centerAO, centerAO, centerAO, 1.0);
        return;
    }

    float sum = 0.0;
    float weightSum = 0.0;

    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offset = vec2(x, y) * texel;
            vec2 sampleUV = uv + offset;

            float sampleAO = texture(SSAOTex, sampleUV).r;
            vec3 sampleNormal = DecodeNormal(texture(GBuffer_NormalWS, sampleUV).rgb);
            vec3 samplePos = texture(GBuffer_PostionWS, sampleUV).rgb;

            if (length(samplePos) < 1e-4)
                continue;

            float spatialDist2 = float(x * x + y * y);
            float spatialWeight = exp(-spatialDist2 / 8.0);

            float normalDot = max(dot(centerNormal, sampleNormal), 0.0);
            float normalWeight = pow(normalDot, 8.0);

            float posDist = length(samplePos - centerPos);
            float positionWeight = exp(-posDist * 2.0);

            float weight = spatialWeight * normalWeight * positionWeight;

            sum += sampleAO * weight;
            weightSum += weight;
        }
    }

    float ao = centerAO;
    if (weightSum > 1e-5)
        ao = sum / weightSum;

    outColor = vec4(vec3(ao), 1.0);
}