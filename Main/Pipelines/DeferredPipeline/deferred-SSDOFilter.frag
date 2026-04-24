#version 450

layout(set = 0, binding = 0) uniform sampler2D SSDOTex;
layout(set = 0, binding = 1) uniform sampler2D GBuffer_NormalWS;
layout(set = 0, binding = 2) uniform sampler2D GBuffer_PositionWS;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 DecodeNormal(vec3 n)
{
    return normalize(n * 2.0 - 1.0);
}

bool IsValidUV(vec2 coord)
{
    return coord.x >= 0.0 && coord.x <= 1.0 &&
           coord.y >= 0.0 && coord.y <= 1.0;
}

void main()
{
    ivec2 texSize = textureSize(SSDOTex, 0);

    vec3 centerIndirect = texture(SSDOTex, uv).rgb;
    vec3 centerN = DecodeNormal(texture(GBuffer_NormalWS, uv).rgb);
    vec3 centerP = texture(GBuffer_PositionWS, uv).rgb;

    vec3 sum = vec3(0.0);
    float wsum = 0.0;

    const int R = 4;

    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            vec2 offset = vec2(x, y) / vec2(texSize);
            vec2 sampleUV = uv + offset;

            if (!IsValidUV(sampleUV)) continue;

            vec3 sampleIndirect = texture(SSDOTex, sampleUV).rgb;
            vec3 sampleN = DecodeNormal(texture(GBuffer_NormalWS, sampleUV).rgb);
            vec3 sampleP = texture(GBuffer_PositionWS, sampleUV).rgb;

            float spatial2 = float(x * x + y * y);
            float spatialWeight = exp(-spatial2 / 8.0);

            float normalWeight = max(dot(centerN, sampleN), 0.0);

            float posDist = length(sampleP - centerP);
            float positionWeight = exp(-(posDist * posDist) / 1.0);

            float w = spatialWeight * normalWeight * positionWeight;

            sum += sampleIndirect * w;
            wsum += w;
        }
    }

    vec3 filtered = (wsum > 0.0) ? (sum / wsum) : centerIndirect;
    outColor = vec4(filtered, 1.0);
}