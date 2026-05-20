#include "Blupch.h"
#include "Animation.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
    glm::mat4 BoneChannel::Evaluate(float animTime) const
    {
        // ---------- Translation ----------
        glm::vec3 pos = glm::vec3(0.0f);
        if (!Positions.empty())
        {
            if (Positions.size() == 1)
            {
                pos = Positions[0].Value;
            }
            else
            {
                int i = FindIndex(Positions, animTime);
                if (i < 0) i = 0;
                float f = Factor(Positions[i].Time, Positions[i + 1].Time, animTime);
                pos = glm::mix(Positions[i].Value, Positions[i + 1].Value, f);
            }
        }

        // ---------- Rotation ----------
        glm::quat rot = glm::quat(1, 0, 0, 0);
        if (!Rotations.empty())
        {
            if (Rotations.size() == 1)
            {
                rot = glm::normalize(Rotations[0].Value);
            }
            else
            {
                int i = FindIndex(Rotations, animTime);
                if (i < 0) i = 0;
                float f = Factor(Rotations[i].Time, Rotations[i + 1].Time, animTime);
                rot = glm::normalize(glm::slerp(Rotations[i].Value, Rotations[i + 1].Value, f));
            }
        }

        // ---------- Scale ----------
        glm::vec3 scl = glm::vec3(1.0f);
        if (!Scales.empty())
        {
            if (Scales.size() == 1)
            {
                scl = Scales[0].Value;
            }
            else
            {
                int i = FindIndex(Scales, animTime);
                if (i < 0) i = 0;
                float f = Factor(Scales[i].Time, Scales[i + 1].Time, animTime);
                scl = glm::mix(Scales[i].Value, Scales[i + 1].Value, f);
            }
        }

        return glm::translate(glm::mat4(1.0f), pos)
             * glm::toMat4(rot)
             * glm::scale(glm::mat4(1.0f), scl);
    }
}
