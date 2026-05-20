#pragma once
#include <glm/glm.hpp>

namespace Blu
{
    struct FrustumPlane
    {
        glm::vec3 Normal;
        float     Distance;  // such that dot(Normal, point) + Distance < 0 is outside
    };

    class Frustum
    {
    public:
        void ExtractFromVP(const glm::mat4& vp)
        {
            // Left   (column 3 + column 0)
            m_Planes[0].Normal   = glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]);
            m_Planes[0].Distance = vp[3][3] + vp[3][0];
            // Right  (column 3 - column 0)
            m_Planes[1].Normal   = glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]);
            m_Planes[1].Distance = vp[3][3] - vp[3][0];
            // Bottom (column 3 + column 1)
            m_Planes[2].Normal   = glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]);
            m_Planes[2].Distance = vp[3][3] + vp[3][1];
            // Top    (column 3 - column 1)
            m_Planes[3].Normal   = glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]);
            m_Planes[3].Distance = vp[3][3] - vp[3][1];
            // Near   (column 3 + column 2) — for RH_ZO, clip space z=0 is near
            m_Planes[4].Normal   = glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]);
            m_Planes[4].Distance = vp[3][3] + vp[3][2];
            // Far    (column 3 - column 2)
            m_Planes[5].Normal   = glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]);
            m_Planes[5].Distance = vp[3][3] - vp[3][2];

            // Normalise all 6 planes
            for (int i = 0; i < 6; ++i)
            {
                float len = glm::length(m_Planes[i].Normal);
                if (len > 0.0f)
                {
                    m_Planes[i].Normal   /= len;
                    m_Planes[i].Distance /= len;
                }
            }
        }

        // Returns true if the sphere is *inside or intersecting* the frustum.
        bool TestSphere(const glm::vec3& center, float radius) const
        {
            for (int i = 0; i < 6; ++i)
            {
                float dist = glm::dot(m_Planes[i].Normal, center) + m_Planes[i].Distance;
                if (dist < -radius)
                    return false; // completely outside this plane
            }
            return true;
        }

    private:
        FrustumPlane m_Planes[6];
    };
}
