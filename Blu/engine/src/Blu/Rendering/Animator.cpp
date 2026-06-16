#include "Blupch.h"
#include "Animator.h"

namespace Blu
{
    void Animator::Update(float deltaTime,
                          float& currentTime, bool loop, float speedScale,
                          const AnimationClip& clip,
                          const Skeleton& skeleton,
                          std::vector<glm::mat4>& finalBoneMatrices)
    {
        finalBoneMatrices.assign(kMaxBones, glm::mat4(1.0f));

        if (clip.Duration <= 0.0f || skeleton.NumBones == 0) return;

        float ticksPerSec = clip.TicksPerSec > 0.0f ? clip.TicksPerSec : 25.0f;
        currentTime += deltaTime * ticksPerSec * speedScale;

        if (loop)
        {
            currentTime = std::fmod(currentTime, clip.Duration);
            if (currentTime < 0.0f) currentTime += clip.Duration;
        }
        else
        {
            currentTime = std::min(currentTime, clip.Duration);
        }

        ComputeNodeTransforms(skeleton.RootNode, skeleton.GlobalInverseTransform,
                              currentTime, clip, skeleton, finalBoneMatrices);
    }

    void Animator::ComputeBindPose(const Skeleton& skeleton,
                                   std::vector<glm::mat4>& finalBoneMatrices)
    {
        finalBoneMatrices.assign(kMaxBones, glm::mat4(1.0f));
        if (skeleton.NumBones == 0) return;

        // An empty clip has no channels, so ComputeNodeTransforms falls back to each
        // node's LocalTransform at every bone — exactly the rest/bind pose.
        static const AnimationClip kEmptyClip{};
        ComputeNodeTransforms(skeleton.RootNode, skeleton.GlobalInverseTransform,
                              0.0f, kEmptyClip, skeleton, finalBoneMatrices);
    }

    void Animator::BlendClips(float t,
                              const std::vector<glm::mat4>& a,
                              const std::vector<glm::mat4>& b,
                              std::vector<glm::mat4>& out)
    {
        t = glm::clamp(t, 0.0f, 1.0f);
        const size_t n = std::min(a.size(), b.size());
        out.assign(n, glm::mat4(1.0f));
        for (size_t i = 0; i < n; ++i)
            out[i] = a[i] * (1.0f - t) + b[i] * t;
    }

    bool Animator::UpdateWithBlending(float deltaTime,
                                      float& fromTime, float& toTime,
                                      float& blendElapsed, float blendDuration,
                                      bool loop, float speedScale,
                                      const AnimationClip& fromClip,
                                      const AnimationClip& toClip,
                                      const Skeleton& skeleton,
                                      std::vector<glm::mat4>& finalBoneMatrices)
    {
        std::vector<glm::mat4> poseFrom, poseTo;
        Update(deltaTime, fromTime, loop, speedScale, fromClip, skeleton, poseFrom);
        Update(deltaTime, toTime,   loop, speedScale, toClip,   skeleton, poseTo);

        blendElapsed += deltaTime;
        const float t = blendDuration > 0.0f ? glm::clamp(blendElapsed / blendDuration, 0.0f, 1.0f) : 1.0f;
        BlendClips(t, poseFrom, poseTo, finalBoneMatrices);
        return blendElapsed >= blendDuration;
    }

    void Animator::ComputeNodeTransforms(const BoneNode& node,
                                         const glm::mat4& parentTransform,
                                         float animTimeTicks,
                                         const AnimationClip& clip,
                                         const Skeleton& skeleton,
                                         std::vector<glm::mat4>& finalBoneMatrices)
    {
        glm::mat4 nodeTransform = node.LocalTransform;

        // If this node has an animation channel, use it
        auto chanIt = clip.ChannelMap.find(node.Name);
        if (chanIt != clip.ChannelMap.end())
        {
            const BoneChannel& chan = clip.Channels[chanIt->second];
            nodeTransform = chan.Evaluate(animTimeTicks);
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        // If this node is a bone, write its final matrix
        auto boneIt = skeleton.BoneInfoMap.find(node.Name);
        if (boneIt != skeleton.BoneInfoMap.end())
        {
            int boneID = boneIt->second.ID;
            if (boneID >= 0 && boneID < kMaxBones)
                finalBoneMatrices[boneID] = globalTransform * boneIt->second.OffsetMatrix;
        }

        for (const auto& child : node.Children)
            ComputeNodeTransforms(child, globalTransform, animTimeTicks, clip, skeleton, finalBoneMatrices);
    }
}
