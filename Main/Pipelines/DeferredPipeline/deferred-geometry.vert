#version 450


struct Transform
{
	mat4 CLIP_FROM_LOCAL;
	mat4 WORLD_FROM_LOCAL;
	mat4 WORLD_FROM_LOCAL_NORMAL;
};

layout(set=0, binding=0, std140) uniform Camera
{
	mat4 CLIP_FROM_WORLD;
};

layout(set=2, binding=0, std140) readonly buffer Transforms
{
	Transform[] TRANSFORMS;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec4 Tangent;
layout(location = 3) in vec2 Texcoord;

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 texcoord;
layout(location = 3) out mat3 TBN;

void main()
{
    Transform transform = TRANSFORMS[gl_InstanceIndex];

    vec4 worldPos = transform.WORLD_FROM_LOCAL * vec4(Position, 1.0);
    vec3 worldNormal = normalize((transform.WORLD_FROM_LOCAL_NORMAL * vec4(Normal, 0.0)).xyz);
    vec3 worldTangent = normalize((transform.WORLD_FROM_LOCAL_NORMAL * vec4(Tangent.xyz, 0.0)).xyz);

    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent)) * Tangent.w;

    gl_Position = CLIP_FROM_WORLD * worldPos;

    position = worldPos.xyz;
    normal = worldNormal;
    texcoord = Texcoord;
    TBN = mat3(worldTangent, worldBitangent, worldNormal);
}