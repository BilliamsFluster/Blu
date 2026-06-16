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

        // Per-bone linear blend of two pose matrix sets (out = lerp(a, b, t)). t is clamped to
        // [0,1]. Matrix-level lerp (not slerp) — fast and visually fine for short crossfades.
        static void BlendClips(float t,
                               const std::vector<glm::mat4>& a,
                               const std::vector<glm::mat4>& b,
                               std::vector<glm::mat4>& out);

        // Crossfade: advance both clips (fromTime/toTime by deltaTime), advance blendElapsed,
        // and write the blended pose. Returns true once blendElapsed >= blendDuration, at which
        // point the caller should make `toClip` the sole active clip. See AnimatorComponent::PlayClip.
        static bool UpdateWithBlending(float deltaTime,
                                       float& fromTime, float& toTime,
                                       float& blendElapsed, float blendDuration,
                                       bool loop, float speedScale,
                                       const AnimationClip& fromClip,
                                       const AnimationClip& toClip,
                                       const Skeleton& skeleton,
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
