#pragma once
#include "Animation.h"
#include <glm/glm.hpp>
#include <vector>

namespace Blu
{
    // Computes the final bone matrices for the current animation time.
    // Results are uploaded to the GPU each frame via a cbuffer.
    class Animator
    {
    public:
        static constexpr int kMaxBones = 128;

        // Advance the animation by deltaTime seconds and compute FinalBoneMatrices.
        // Call this once per frame before rendering.
        static void Update(float deltaTime,
                           float& currentTime, bool loop, float speedScale,
                           const AnimationClip& clip,
                           const Skeleton& skeleton,
                           std::vector<glm::mat4>& finalBoneMatrices);

        // Compute the rest/bind pose (no animation). Use when a skinned mesh has no
        // clip to play — leaving the matrices identity collapses the mesh because its
        // vertices are authored in bone space and need globalBind * inverseBind applied.
        static void ComputeBindPose(const Skeleton& skeleton,
                                    std::vector<glm::mat4>& finalBoneMatrices);

    private:
        static void ComputeNodeTransforms(const BoneNode& node,
                                          const glm::mat4& parentTransform,
                                          float animTimeTicks,
                                          const AnimationClip& clip,
                                          const Skeleton& skeleton,
                                          std::vector<glm::mat4>& finalBoneMatrices);
    };
}
