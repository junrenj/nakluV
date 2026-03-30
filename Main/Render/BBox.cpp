#include "BBox.hpp"

void FAABB::UpdateBBoxWithTransform(FAABB& OutBox, const FAABB& LocalBox, const glm::mat4& Transform)
{
    vec3 Corners[8] = 
    {
        LocalBox.Min,
        vec3(LocalBox.Min.x, LocalBox.Min.y, LocalBox.Max.z),
        vec3(LocalBox.Min.x, LocalBox.Max.y, LocalBox.Min.z),
        vec3(LocalBox.Min.x, LocalBox.Max.y, LocalBox.Max.z),
        vec3(LocalBox.Max.x, LocalBox.Min.y, LocalBox.Min.z),
        vec3(LocalBox.Max.x, LocalBox.Min.y, LocalBox.Max.z),
        vec3(LocalBox.Max.x, LocalBox.Max.y, LocalBox.Min.z),
        LocalBox.Max
    };

    for (int i = 0; i < 8; i++)
    {
        glm::vec4 Position = Transform * glm::vec4(Corners[i], 1.0f);
        OutBox.Min = glm::min(OutBox.Min, vec3(Position));
        OutBox.Max = glm::max(OutBox.Max, vec3(Position));
    }
}