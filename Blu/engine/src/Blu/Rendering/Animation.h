#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include "Blu/Core/Core.h"

namespace Blu
{
    // ── Key-frame data ────────────────────────────────────────────────────────
    struct KeyPosition { glm::vec3 Value; float Time; };
    struct KeyRotation { glm::quat Value; float Time; };
    struct KeyScale    { glm::vec3 Value; float Time; };

    // ── Per-bone animation channel ────────────────────────────────────────────
    struct BoneChannel
    {
        std::string             Name;
        int                     BoneID = -1;
        std::vector<KeyPosition> Positions;
        std::vector<KeyRotation> Rotations;
        std::vector<KeyScale>    Scales;

        // Returns the local-space transform for this bone at the given animation time.
        glm::mat4 Evaluate(float animTime) const;

    private:
        template<typename T>
        static int FindIndex(const std::vector<T>& keys, float t)
        {
            for (int i = 0; i + 1 < (int)keys.size(); ++i)
                if (t < keys[i + 1].Time) return i;
            return (int)keys.size() - 2; // clamp to last valid interval
        }

        static float Factor(float a, float b, float t)
        {
            return (b - a) < 1e-6f ? 0.0f : (t - a) / (b - a);
        }
    };

    // ── Bone hierarchy node ───────────────────────────────────────────────────
    struct BoneNode
    {
        std::string          Name;
        glm::mat4            LocalTransform; // node default (rest pose)
        std::vector<BoneNode> Children;
    };

    // ── Per-bone binding info ─────────────────────────────────────────────────
    struct BoneInfo
    {
        int       ID           = -1;
        glm::mat4 OffsetMatrix = glm::mat4(1.0f); // model-space bind-pose inverse
    };

    // ── Skeleton ──────────────────────────────────────────────────────────────
    struct Skeleton
    {
        BoneNode RootNode;
        glm::mat4 GlobalInverseTransform = glm::mat4(1.0f);
        std::unordered_map<std::string, BoneInfo> BoneInfoMap;
        int NumBones = 0;
    };

    // ── Animation clip ────────────────────────────────────────────────────────
    struct AnimationClip
    {
        std::string              Name;
        float                    Duration     = 0.0f; // in ticks
        float                    TicksPerSec  = 25.0f;
        std::vector<BoneChannel> Channels;

        // Map from bone name → channel index, built after loading.
        std::unordered_map<std::string, int> ChannelMap;

        void BuildChannelMap()
        {
            ChannelMap.clear();
            for (int i = 0; i < (int)Channels.size(); ++i)
                ChannelMap[Channels[i].Name] = i;
        }

        float DurationSeconds() const
        {
            return TicksPerSec > 0.0f ? Duration / TicksPerSec : 0.0f;
        }
    };

    // ── Skinned model (extends Model with skeleton + animations) ───────────────
    // Stored separately so static meshes (no skin) aren't bloated.
    struct SkeletonData
    {
        Shared<Skeleton>              Skel;
        std::vector<AnimationClip>    Clips;
    };
}
