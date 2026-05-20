#include "Blupch.h"
#include "Physics3D.h"
#include "Blu/Core/Core.h"
#include "Blu/Core/Log.h"

#include <thread>
#include <cstdarg>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>

JPH_SUPPRESS_WARNINGS

namespace Blu
{
    // ──────────────────────────────────────────────────────────────────────────
    // Jolt process-level init / shutdown
    // ──────────────────────────────────────────────────────────────────────────
    static bool  s_JoltInitialized = false;
    static int   s_JoltRefCount    = 0;

    void Physics3DWorld::EnsureJoltInitialized()
    {
        ++s_JoltRefCount;
        if (s_JoltInitialized)
            return;

        JPH::RegisterDefaultAllocator();
        JPH::Trace = [](const char* inFMT, ...) {
            va_list list;
            va_start(list, inFMT);
            char buf[1024];
            vsnprintf(buf, sizeof(buf), inFMT, list);
            va_end(list);
            BLU_CORE_TRACE("{0}", buf);
        };
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        s_JoltInitialized = true;
    }

    void Physics3DWorld::ReleaseJolt()
    {
        --s_JoltRefCount;
        if (s_JoltRefCount > 0)
            return;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
        s_JoltInitialized = false;
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Physics3DWorld
    // ──────────────────────────────────────────────────────────────────────────
    Physics3DWorld::~Physics3DWorld()
    {
        if (m_PhysicsSystem)
        {
            delete m_PhysicsSystem;
            m_PhysicsSystem = nullptr;
        }
        if (m_TempAllocator)
        {
            delete m_TempAllocator;
            m_TempAllocator = nullptr;
        }
        if (m_JobSystem)
        {
            delete m_JobSystem;
            m_JobSystem = nullptr;
        }

        ReleaseJolt();
    }

    void Physics3DWorld::Init(const glm::vec3& gravity)
    {
        EnsureJoltInitialized();

        const JPH::uint cMaxBodies             = 1024;
        const JPH::uint cNumBodyMutexes        = 0;      // 0 = auto
        const JPH::uint cMaxBodyPairs          = 65536;
        const JPH::uint cMaxContactConstraints = 10240;

        m_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024); // 10 MB
        m_JobSystem     = new JPH::JobSystemThreadPool(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            (int)std::thread::hardware_concurrency() - 1);

        m_PhysicsSystem = new JPH::PhysicsSystem();
        m_PhysicsSystem->Init(
            cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            m_BPLayerInterface, m_OBPLayerFilter, m_OLPairFilter);

        m_PhysicsSystem->SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }

    uint32_t Physics3DWorld::AddBody(
        const glm::vec3&         worldPosition,
        const glm::quat&         worldRotation,
        Physics3DBodyType        bodyType,
        const Physics3DBodySpec& spec)
    {
        BLU_CORE_ASSERT(m_PhysicsSystem, "Physics3DWorld not initialized!");

        JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();

        // Build shape
        JPH::ShapeRefC shape;
        switch (spec.ShapeType)
        {
            case Physics3DShapeType::Box:
            {
                JPH::BoxShapeSettings boxSettings(
                    JPH::Vec3(spec.HalfExtents.x, spec.HalfExtents.y, spec.HalfExtents.z));
                boxSettings.mDensity = spec.Density;
                auto result = boxSettings.Create();
                BLU_CORE_ASSERT(!result.HasError(), "Jolt BoxShape creation failed");
                shape = result.Get();
                break;
            }
            case Physics3DShapeType::Sphere:
            {
                JPH::SphereShapeSettings sphereSettings(spec.Radius);
                sphereSettings.mDensity = spec.Density;
                auto result = sphereSettings.Create();
                BLU_CORE_ASSERT(!result.HasError(), "Jolt SphereShape creation failed");
                shape = result.Get();
                break;
            }
            case Physics3DShapeType::Capsule:
            {
                JPH::CapsuleShapeSettings capsuleSettings(spec.HalfHeight, spec.Radius);
                capsuleSettings.mDensity = spec.Density;
                auto result = capsuleSettings.Create();
                BLU_CORE_ASSERT(!result.HasError(), "Jolt CapsuleShape creation failed");
                shape = result.Get();
                break;
            }
            default:
                BLU_CORE_ASSERT(false, "Unknown Physics3DShapeType");
                return UINT32_MAX;
        }

        // World position offset by collider offset
        glm::vec3 finalPos = worldPosition + spec.Offset;

        // Map body type to Jolt motion type and object layer
        JPH::EMotionType joltMotionType;
        JPH::ObjectLayer joltLayer;
        bool             activate;

        switch (bodyType)
        {
            case Physics3DBodyType::Static:
                joltMotionType = JPH::EMotionType::Static;
                joltLayer      = Physics3DLayers::NON_MOVING;
                activate       = false;
                break;
            case Physics3DBodyType::Dynamic:
                joltMotionType = JPH::EMotionType::Dynamic;
                joltLayer      = Physics3DLayers::MOVING;
                activate       = true;
                break;
            case Physics3DBodyType::Kinematic:
                joltMotionType = JPH::EMotionType::Kinematic;
                joltLayer      = Physics3DLayers::MOVING;
                activate       = true;
                break;
            default:
                BLU_CORE_ASSERT(false, "Unknown Physics3DBodyType");
                return UINT32_MAX;
        }

        JPH::BodyCreationSettings creationSettings(
            shape,
            JPH::RVec3(finalPos.x, finalPos.y, finalPos.z),
            JPH::Quat(worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w),
            joltMotionType,
            joltLayer);

        creationSettings.mFriction    = spec.Friction;
        creationSettings.mRestitution = spec.Restitution;

        JPH::BodyID bodyID = bi.CreateAndAddBody(
            creationSettings,
            activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        BLU_CORE_ASSERT(!bodyID.IsInvalid(), "Jolt failed to create body — max bodies exceeded?");

        return bodyID.GetIndexAndSequenceNumber();
    }

    void Physics3DWorld::RemoveBody(uint32_t packedID)
    {
        if (!m_PhysicsSystem || packedID == UINT32_MAX)
            return;

        JPH::BodyID bodyID(packedID);
        JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();

        if (bi.IsAdded(bodyID))
            bi.RemoveBody(bodyID);

        bi.DestroyBody(bodyID);
    }

    void Physics3DWorld::Step(float deltaTime)
    {
        if (!m_PhysicsSystem)
            return;

        m_Accumulator += deltaTime;
        while (m_Accumulator >= FIXED_TIMESTEP)
        {
            // collisionSteps = 1 is fine for 60 Hz
            m_PhysicsSystem->Update(FIXED_TIMESTEP, 1, m_TempAllocator, m_JobSystem);
            m_Accumulator -= FIXED_TIMESTEP;
        }
    }

    void Physics3DWorld::GetTransform(uint32_t packedID, glm::vec3& outPosition, glm::quat& outRotation) const
    {
        BLU_CORE_ASSERT(m_PhysicsSystem, "Physics3DWorld not initialized!");

        JPH::BodyID bodyID(packedID);
        const JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();

        JPH::RVec3 pos = bi.GetPosition(bodyID);
        JPH::Quat  rot = bi.GetRotation(bodyID);

        outPosition = glm::vec3(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ()));

        // GLM quat constructor: (w, x, y, z)
        outRotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
    }

    void Physics3DWorld::MoveKinematic(
        uint32_t         packedID,
        const glm::vec3& position,
        const glm::quat& rotation,
        float            deltaTime)
    {
        BLU_CORE_ASSERT(m_PhysicsSystem, "Physics3DWorld not initialized!");

        JPH::BodyID bodyID(packedID);
        JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();

        bi.MoveKinematic(
            bodyID,
            JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
            deltaTime);
    }

} // namespace Blu
