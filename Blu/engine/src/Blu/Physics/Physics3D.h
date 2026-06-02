#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>

// Jolt headers — included here because Physics3D.h is an internal header
// (only included by Physics3D.cpp and Scene.cpp).
#include "Jolt/Jolt.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/NarrowPhaseQuery.h"
#include "Jolt/Physics/Collision/ShapeFilter.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Geometry/Triangle.h"

namespace Blu
{
    // ──────────────────────────────────────────────────────────────────────────
    // Object layers
    // ──────────────────────────────────────────────────────────────────────────
    namespace Physics3DLayers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING     = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Broad-phase layers
    // ──────────────────────────────────────────────────────────────────────────
    namespace Physics3DBPLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint            NUM_LAYERS(2);
    }

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            m_ObjectToBroadPhase[Physics3DLayers::NON_MOVING] = Physics3DBPLayers::NON_MOVING;
            m_ObjectToBroadPhase[Physics3DLayers::MOVING]     = Physics3DBPLayers::MOVING;
        }

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return Physics3DBPLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            return m_ObjectToBroadPhase[layer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
        {
            return (layer == Physics3DBPLayers::NON_MOVING) ? "NON_MOVING" : "MOVING";
        }
#endif

    private:
        JPH::BroadPhaseLayer m_ObjectToBroadPhase[Physics3DLayers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bpLayer) const override
        {
            switch (layer)
            {
                case Physics3DLayers::NON_MOVING: return bpLayer == Physics3DBPLayers::MOVING;
                case Physics3DLayers::MOVING:     return true;
                default:                          return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer obj1, JPH::ObjectLayer obj2) const override
        {
            switch (obj1)
            {
                case Physics3DLayers::NON_MOVING: return obj2 == Physics3DLayers::MOVING;
                case Physics3DLayers::MOVING:     return true;
                default:                          return false;
            }
        }
    };

    // ──────────────────────────────────────────────────────────────────────────
    // Collider shape tag — used when building bodies
    // ──────────────────────────────────────────────────────────────────────────
    enum class Physics3DShapeType
    {
        Box     = 0,
        Sphere,
        Capsule,
        Mesh
    };

    // All data needed to create a physics body (filled from ECS components)
    struct Physics3DBodySpec
    {
        Physics3DShapeType ShapeType    = Physics3DShapeType::Box;

        // Box
        glm::vec3  HalfExtents = { 0.5f, 0.5f, 0.5f };

        // Sphere
        float      Radius      = 0.5f;

        // Capsule
        float      HalfHeight  = 1.0f;
        // (Capsule radius re-uses Radius above)

        // Mesh
        std::vector<glm::vec3> MeshTriangleVertices;
        bool       MeshDoubleSided = true;

        // Shared
        glm::vec3  Offset      = { 0.0f, 0.0f, 0.0f };
        float      Friction    = 0.5f;
        float      Restitution = 0.0f;
        float      Density     = 1000.0f;
    };

    enum class Physics3DBodyType { Static = 0, Dynamic, Kinematic };

    // ──────────────────────────────────────────────────────────────────────────
    // Physics3DWorld — wraps a single Jolt PhysicsSystem
    // ──────────────────────────────────────────────────────────────────────────
    class Physics3DWorld
    {
    public:
        Physics3DWorld() = default;
        ~Physics3DWorld();

        // Must be called once before adding bodies. gravity is in m/s².
        void Init(const glm::vec3& gravity = { 0.0f, -9.81f, 0.0f });

        // Add a body. Returns the packed uint32 BodyID (store in RuntimeBodyID).
        // Pass the entity's world translation/rotation so the body starts there.
        uint32_t AddBody(
            const glm::vec3&    worldPosition,
            const glm::quat&    worldRotation,
            Physics3DBodyType   bodyType,
            const Physics3DBodySpec& spec);

        // Remove and destroy a single body (call on Stop).
        void RemoveBody(uint32_t bodyID);

        // Advance the simulation. Uses a fixed 1/60 s step with accumulation.
        void Step(float deltaTime);

        // Read the current position/rotation of a body back into GLM types.
        void GetTransform(uint32_t bodyID, glm::vec3& outPosition, glm::quat& outRotation) const;

        // Move a kinematic body to a new world-space transform.
        void MoveKinematic(uint32_t bodyID, const glm::vec3& position, const glm::quat& rotation, float deltaTime);

        bool CastRay(const glm::vec3& origin, const glm::vec3& direction, glm::vec3& outHitPosition) const;

        bool IsValid() const { return m_PhysicsSystem != nullptr; }

        JPH::PhysicsSystem*       GetPhysicsSystem()  const { return m_PhysicsSystem; }
        JPH::TempAllocatorImpl*   GetTempAllocator()  const { return m_TempAllocator; }

    private:
        // Jolt global init/shutdown reference counting
        static void EnsureJoltInitialized();
        static void ReleaseJolt();

        JPH::PhysicsSystem*         m_PhysicsSystem  = nullptr;
        JPH::TempAllocatorImpl*     m_TempAllocator  = nullptr;
        JPH::JobSystemThreadPool*   m_JobSystem      = nullptr;

        BPLayerInterfaceImpl                m_BPLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl   m_OBPLayerFilter;
        ObjectLayerPairFilterImpl           m_OLPairFilter;

        float m_Accumulator = 0.0f;
        static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
    };

} // namespace Blu
