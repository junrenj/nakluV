#include "CullingUtils.hpp"

using vec3 = glm::vec3;
using vec4 = glm::vec4;

bool UCullingUtils::SATVisibilityTest(const FCullingFrustum& Frustum, const mat4& Transform, const FAABB& AABB)
{
    /*
    Referrence : https://bruop.github.io/improved_frustum_culling/
    */
    float ZNear = Frustum.NearPlane;
    float ZFar = Frustum.FarPlane;
    float XNear = Frustum.NearRight;
    float YNear = Frustum.NearTop;

    vec3 Corners[] = 
    {
        {AABB.Min.x, AABB.Min.y, AABB.Min.z},
        {AABB.Max.x, AABB.Min.y, AABB.Min.z},
        {AABB.Min.x, AABB.Max.y, AABB.Min.z},
        {AABB.Min.x, AABB.Min.y, AABB.Max.z},
    };

    for (size_t CornerIdx = 0; CornerIdx < 4; CornerIdx++)
    {
        Corners[CornerIdx] = vec3(Transform * vec4(Corners[CornerIdx], 1.0f));
    }

    FOBB OBB = 
    {
        .Axes = 
        {
            Corners[1] - Corners[0],
            Corners[2] - Corners[0],
            Corners[3] - Corners[0]
        },
    };

    OBB.Center = Corners[0] + 0.5f * (OBB.Axes[0] + OBB.Axes[1] + OBB.Axes[2]);
    OBB.Extents = vec3{glm::length((OBB.Axes[0])), glm::length(OBB.Axes[1]), glm::length(OBB.Axes[2])};
    OBB.Axes[0] = OBB.Axes[0] / OBB.Extents.x;
    OBB.Axes[1] = OBB.Axes[1] / OBB.Extents.y;
    OBB.Axes[2] = OBB.Axes[2] / OBB.Extents.z;
    OBB.Extents *= 0.5f;

    {
        // Projected center of our OBB
        float MoC = OBB.Center.z;
        // Projected size of OBB
        float Radius = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            Radius += fabsf(OBB.Axes[i].z) * OBB.Extents[i];
        }
        float OBBMin = MoC - Radius;
        float OBBMax = MoC + Radius;

        float Tau0 = ZFar;
        float Tau1 = ZNear;
        
        if(OBBMin > Tau1 || OBBMax < Tau0)
        {
            return false;
        }
    }

    {
        const vec3 M[] = 
        {
            {ZNear, 0.0f, XNear},   // Left Plane
            {-ZNear, 0.0f, XNear},  // Right Plane
            {0.0f, -ZNear, YNear},  // Top Plane
            {0.0f, ZNear, YNear},   // Bottom Plane
        };
        for (uint32_t m = 0; m < std::size(M); m++)
        {
            float MoX = fabsf(M[m].x);
            float MoY = fabsf(M[m].y);
            float MoZ = M[m].z;
            float MoC = glm::dot(M[m], OBB.Center);

            float OBBRadius = 0.0f;
            for (int i = 0; i < 3; i++)
            {
                OBBRadius += fabsf(glm::dot(M[m], OBB.Axes[i])) * OBB.Extents[i];
            }

            float OBBMin = MoC - OBBRadius;
            float OBBMax = MoC + OBBRadius;

            float P = XNear * MoX + YNear * MoY;

            float Tau0 = ZNear * MoZ - P;
            float Tau1 = ZNear * MoZ + P;

            if(Tau0 < 0.0f)
            {
                Tau0 *= ZFar / ZNear;
            }
            if(Tau1 > 0.0f)
            {
                Tau1 *= ZFar / ZNear;
            }

            if(OBBMin > Tau1 || OBBMax < Tau0)
            {
                return false;
            }
        }
    }

    // OBB Axes
    {
        for (uint32_t m = 0; m < std::size(OBB.Axes); m++)
        {
            const vec3& M = OBB.Axes[m];
            float MoX = fabsf(M.x);
            float MoY = fabsf(M.y);
            float MoZ = M.z;
            float MoC = dot(M, OBB.Center);

            float OBBRadius = OBB.Extents[m];

            float OBBMin = MoC - OBBRadius;
            float OBBMax = MoC + OBBRadius;

            // Frustum projection
            float P = XNear * MoX + YNear * MoY;
            float Tau0 = ZNear * MoZ - P;
            float Tau1 = ZNear * MoZ + P;
            if (Tau0 < 0.0f) 
            {
                Tau0 *= ZFar / ZNear;
            }
            if (Tau1 > 0.0f) 
            {
                Tau1 *= ZFar / ZNear;
            }

            if (OBBMin > Tau1 || OBBMax < Tau0) 
            {
                return false;
            }
        } 
    }

    // Now let's perform each of the cross products between the edges
    // First R x A_i
    {
        for (uint32_t m = 0; m < std::size(OBB.Axes); m++)
        {
            const vec3 M = {0.0f, -OBB.Axes[m].z, OBB.Axes[m].y};
            float MoX = 0.0f;
            float MoY = fabsf(M.y);
            float MoZ = M.z;
            float MoC = M.y * OBB.Center.y + M.z * OBB.Center.z;

            float OBBRadius = 0.0f;
            for (uint32_t i = 0; i < 3; i++)
            {
                OBBRadius += fabsf(glm::dot(M, OBB.Axes[i])) * OBB.Extents[i];
            }

            float OBBMin = MoC - OBBRadius;
            float OBBMax = MoC + OBBRadius;

            // Frustum Projection
            float P = XNear * MoX + YNear * MoY;
            float Tau0 = ZNear * MoZ - P;
            float Tau1 = ZNear * MoZ + P;
            if(Tau0 < 0.0f)
            {
                Tau0 *= ZFar / ZNear;
            }
            if(Tau1 > 0.0f)
            {
                Tau1 *= ZFar / ZNear;
            }

            if(OBBMin > Tau1 || OBBMax < Tau0)
            {
                return false;
            }
        }
    }

    // U x A_i
    {
        for (uint32_t m = 0; m < std::size(OBB.Axes); m++) 
        {
            const vec3 M = { OBB.Axes[m].z, 0.0f, -OBB.Axes[m].x };
            float MoX = fabsf(M.x);
            float MoY = 0.0f;
            float MoZ = M.z;
            float MoC = M.x * OBB.Center.x + M.z * OBB.Center.z;

            float OBBRadius = 0.0f;
            for (int i = 0; i < 3; i++) 
            {
                OBBRadius += fabsf(dot(M, OBB.Axes[i])) * OBB.Extents[i];
            }

            float OBBMin = MoC - OBBRadius;
            float OBBMax = MoC + OBBRadius;

            // Frustum projection
            float P = XNear * MoX + YNear * MoY;
            float Tau0 = ZNear * MoZ - P;
            float Tau1 = ZNear * MoZ + P;
            if (Tau0 < 0.0f) 
            {
                Tau0 *= ZFar / ZNear;
            }
            if (Tau1 > 0.0f) 
            {
                Tau1 *= ZFar / ZNear;
            }

            if (OBBMin > Tau1 || OBBMax < Tau0) 
            {
                return false;
            }
        }
    }

    // Frustum Edges X Ai
    {
        for (uint32_t EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++) 
        {
            const vec3 M[] = 
            {
                cross({-XNear, 0.0f, ZNear}, OBB.Axes[EdgeIdx]), // Left Plane
                cross({ XNear, 0.0f, ZNear }, OBB.Axes[EdgeIdx]), // Right plane
                cross({ 0.0f, YNear, ZNear }, OBB.Axes[EdgeIdx]), // Top plane
                cross({ 0.0, -YNear, ZNear }, OBB.Axes[EdgeIdx]) // Bottom plane
            };

            for (size_t m = 0; m < std::size(M); m++) 
            {
                float MoX = fabsf(M[m].x);
                float MoY = fabsf(M[m].y);
                float MoZ = M[m].z;

                constexpr float Epsilon = 1e-4f;
                if (MoX < Epsilon && MoY < Epsilon && fabsf(MoZ) < Epsilon)
                {
                    continue;
                }

                float MoC = dot(M[m], OBB.Center);

                float OBBRadius = 0.0f;
                for (int i = 0; i < 3; i++) 
                {
                    OBBRadius += fabsf(dot(M[m], OBB.Axes[i])) * OBB.Extents[i];
                }

                float OBBMin = MoC - OBBRadius;
                float OBBMax = MoC + OBBRadius;

                // Frustum projection
                float P = XNear * MoX + YNear * MoY;
                float Tau0 = ZNear * MoZ - P;
                float Tau1 = ZNear * MoZ + P;
                if (Tau0 < 0.0f) 
                {
                    Tau0 *= ZFar / ZNear;
                }
                if (Tau1 > 0.0f) 
                {
                    Tau1 *= ZFar / ZNear;
                }

                if (OBBMin > Tau1 || OBBMax < Tau0) 
                {
                    return false;
                }
            }
        }
    }
    
    return true;
}
