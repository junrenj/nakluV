#version 450 //GLSL version 4.5

layout(location=0) in vec3 Position;

struct Transform
{
    mat4 CLIP_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL_NORMAL;
};

layout(set = 0, binding = 0) readonly buffer TransformsBuffer
{
    Transform transforms[];
};

layout(push_constant) uniform Push
{
    mat4 SHADOW_CLIP_FROM_WORLD;
};

void main()
{
    mat4 world_from_local = transforms[gl_InstanceIndex].WORLD_FROM_LOCAL;
    vec4 worldPos = world_from_local * vec4(Position, 1.0);
    gl_Position = SHADOW_CLIP_FROM_WORLD * worldPos;
}