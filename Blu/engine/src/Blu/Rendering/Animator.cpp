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
