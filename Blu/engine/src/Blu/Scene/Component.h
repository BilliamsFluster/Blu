#pragma once
#include <string>
#include <glm/glm.hpp>
#include "Blu/Scene/SceneCamera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Blu/Rendering/ParticleSystem.h"
#include "Blu/Core/UUID.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Rendering/Animation.h"
#include "Blu/Rendering/Animator.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
	class AActor;
	class Entity;
	
	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(UUID& uuid) : ID(uuid) {}

	};
	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			:Translation(translation) {}

		glm::mat4 GetTransform() const 
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
		
	};

	struct ParticleSystemComponent
	{
		std::function<void()> CurrentParticleSystem; // function will call the correct particle system template
		Particle ParticleAttributes;
		Entity* AttachedEntity = nullptr;
		ParticleProps ParticleSystemProps;
		ParticleSystem PSystem;
		ParticleSystemComponent() = default;
		ParticleSystemComponent(const ParticleSystemComponent&) = default;
		ParticleSystemComponent(const glm::mat4& attachTransform) {}
		bool operator==(const ParticleSystemComponent& other) const {
			
			return ParticleAttributes.ColorBegin == other.ParticleAttributes.ColorBegin;
		}
		void Update(float deltaTime)
		{
			if (CurrentParticleSystem)
			{
				CurrentParticleSystem();
			}


			PSystem.OnUpdate(deltaTime);
			PSystem.OnRender();
		}
			
	};

	struct SpriteRendererComponent 
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		
		Shared<class Material> MaterialInstance;  

		SpriteRendererComponent()
		{
			MaterialInstance = Material::Create();

		}
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			:Color(color) 
		{
			MaterialInstance = Material::Create();

		}
		
	};

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Radius = 0.5f;
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
		CircleRendererComponent(const glm::vec4& color)
			:Color(color) {}

	};

	struct MeshComponent
	{
		Shared<class Mesh> MeshData;
		Shared<class Material> MaterialInstance;
		Shared<Model> ModelAsset;
		std::string FilePath;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
	};

	// ── Level of Detail ───────────────────────────────────────────────────────
	// Each LOD entry pairs a loaded model with a maximum camera distance at which
	// it should be rendered. Levels are checked nearest-first; the last entry's
	// model is used for any distance beyond it (the "lowest quality" fallback).
	struct LODEntry
	{
		Shared<Model> ModelAsset;
		float         MaxDistance = 100.0f;
		std::string   FilePath;
	};

	struct MeshLODComponent
	{
		std::vector<LODEntry> Levels;   // sorted by MaxDistance ascending
		bool Active = true;

		// Returns the model to render at the given camera distance.
		// Falls back to the MeshComponent's own model when Levels is empty.
		Shared<Model> SelectLOD(float dist) const
		{
			if (!Active || Levels.empty()) return nullptr;
			for (const auto& lv : Levels)
				if (dist <= lv.MaxDistance) return lv.ModelAsset;
			return Levels.back().ModelAsset;
		}
	};

	struct TagComponent 
	{
		std::string Tag;
		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			:Tag(tag) {}

	};

	struct CameraComponent 
	{
		Blu::SceneCamera Camera;
		bool Primary = false;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
		
	};
	struct NativeScriptComponent
	{
		AActor*     Instance          = nullptr;
		std::string ClassName;                     // set from editor or Bind<T>()

		AActor*(*InstantiateScript)()              = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		// Programmatic bind — T must derive from AActor.
		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<AActor*>(new T()); };
			DestroyScript     = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	struct Rigidbody2DComponent
	{
		enum class BodyType {Static = 0, Dynamic, Kinematic};

		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent& other) = default;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 1.0f, 1.0f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.1f;
		float RestitutionThreshold = 0.5f;
		bool ShowCollision = false;
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent& other) = default;
		
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 1.0f;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.1f;
		float RestitutionThreshold = 0.5f;
		bool ShowCollision = false;

		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent& other) = default;
	};

	// ─── 3D Physics Components ────────────────────────────────────────────────

	struct Rigidbody3DComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type           = BodyType::Static;
		bool FixedRotationX     = false;
		bool FixedRotationY     = false;
		bool FixedRotationZ     = false;
		float GravityScale      = 1.0f;
		float LinearDamping     = 0.05f;
		float AngularDamping    = 0.05f;
		uint32_t RuntimeBodyID  = UINT32_MAX; // Jolt BodyID packed as uint32

		Rigidbody3DComponent() = default;
		Rigidbody3DComponent(const Rigidbody3DComponent&) = default;
	};

	struct BoxCollider3DComponent
	{
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset      = { 0.0f, 0.0f, 0.0f };
		float Friction        = 0.5f;
		float Restitution     = 0.0f;
		float Density         = 1000.0f;

		BoxCollider3DComponent() = default;
		BoxCollider3DComponent(const BoxCollider3DComponent&) = default;
	};

	struct SphereCollider3DComponent
	{
		float Radius          = 0.5f;
		glm::vec3 Offset      = { 0.0f, 0.0f, 0.0f };
		float Friction        = 0.5f;
		float Restitution     = 0.0f;
		float Density         = 1000.0f;

		SphereCollider3DComponent() = default;
		SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
	};

	struct CapsuleCollider3DComponent
	{
		float Radius          = 0.5f;
		float HalfHeight      = 1.0f;
		glm::vec3 Offset      = { 0.0f, 0.0f, 0.0f };
		float Friction        = 0.5f;
		float Restitution     = 0.0f;
		float Density         = 1000.0f;

		CapsuleCollider3DComponent() = default;
		CapsuleCollider3DComponent(const CapsuleCollider3DComponent&) = default;
	};

	// ─── Point Light ──────────────────────────────────────────────────────────
	// Position is taken from the entity's TransformComponent::Translation.
	// Attenuation formula: att = 1 / (Constant + Linear*d + Quadratic*d²)
	// Typical presets (d = range in world units):
	//   Range 7    → Constant 1.0 / Linear 0.7  / Quadratic 1.8
	//   Range 50   → Constant 1.0 / Linear 0.09 / Quadratic 0.032
	//   Range 200  → Constant 1.0 / Linear 0.022/ Quadratic 0.0019
	struct PointLightComponent
	{
		glm::vec3 Ambient   = glm::vec3(0.05f, 0.05f, 0.05f);
		glm::vec3 Diffuse   = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Specular  = glm::vec3(1.00f, 1.00f, 1.00f);
		float     Intensity    = 1.0f;
		float     Range        = 20.0f;
		// Attenuation coefficients
		float     AttConstant  = 1.0f;
		float     AttLinear    = 0.09f;
		float     AttQuadratic = 0.032f;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
	};

	// ─── Directional Light ────────────────────────────────────────────────────
	// Infinite-distance source (sun / sky).  No position, no attenuation.
	struct DirectionalLightComponent
	{
		glm::vec3 Direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
		glm::vec3 Ambient   = glm::vec3(0.10f, 0.10f, 0.10f);
		glm::vec3 Diffuse   = glm::vec3(0.80f, 0.80f, 0.80f);
		glm::vec3 Specular  = glm::vec3(0.50f, 0.50f, 0.50f);
		float     Intensity = 1.0f;

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// ─── Spot Light ───────────────────────────────────────────────────────────
	// Position comes from TransformComponent::Translation.
	// InnerConeAngle / OuterConeAngle in degrees; shader pre-computes cos values.
	// Smooth falloff between inner (full intensity) and outer (zero) edges.
	struct SpotLightComponent
	{
		glm::vec3 Ambient        = glm::vec3(0.00f, 0.00f, 0.00f);
		glm::vec3 Diffuse        = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Specular       = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Direction      = glm::vec3(0.0f, -1.0f, 0.0f);
		float     Intensity      = 1.0f;
		float     Range          = 30.0f;
		float     InnerConeAngle = 12.5f;   // degrees
		float     OuterConeAngle = 17.5f;   // degrees
		float     AttConstant    = 1.0f;
		float     AttLinear      = 0.09f;
		float     AttQuadratic   = 0.032f;

		SpotLightComponent() = default;
		SpotLightComponent(const SpotLightComponent&) = default;
	};

	// ── Spring-Arm (Camera Boom) ─────────────────────────────────────────────
	// Attach to an entity to position a child camera at ArmLength behind/above it.
	// The arm extends in the direction opposite to TargetOffset (pivot → arm end).
	// During OnUpdateEditor/Runtime the Scene drives the attached CameraComponent
	// entity's TransformComponent each frame using the lag-smoothed world position.
	struct SpringArmComponent
	{
		// Arm length: world-space distance from the pivot to the camera socket.
		float     ArmLength         = 5.0f;
		// Socket offset relative to the end of the arm (e.g. raise camera above)
		glm::vec3 SocketOffset      = { 0.0f, 0.5f, 0.0f };
		// Pivot offset: where on the target entity the arm originates (e.g. head)
		glm::vec3 PivotOffset       = { 0.0f, 1.7f, 0.0f };
		// Pitch / yaw of the arm (degrees).  Yaw is added to the entity's own yaw.
		float     Pitch             = -15.0f;
		float     Yaw               = 0.0f;
		// Lag: how quickly the camera follows (0=instant, 1=very slow)
		float     PositionLagSpeed  = 10.0f;
		bool      EnableLag         = true;
		// Camera entity driven by this arm (set in editor via SceneHierarchyPanel)
		UUID      TargetCameraUUID  = 0;

		SpringArmComponent() = default;
		SpringArmComponent(const SpringArmComponent&) = default;
	};

	// ── Skeletal Animation ───────────────────────────────────────────────────
	// Attach to an entity that also has a MeshComponent whose Model has bone data.
	// The Scene updates FinalBoneMatrices each frame and uploads them to the GPU.
	struct AnimatorComponent
	{
		// Clips are shared from the Model's SkeletonData; this component owns the
		// current playback state so multiple entities can animate independently.
		Shared<SkeletonData>       SkelData;        // set from Model::SkelData on start
		int                        CurrentClipIndex = 0;
		float                      CurrentTime      = 0.0f; // ticks
		bool                       Playing          = true;
		bool                       Loop             = true;
		float                      SpeedScale       = 1.0f;

		// Output: filled each frame by Animator::Update — uploaded to BoneData cbuffer
		std::vector<glm::mat4>     FinalBoneMatrices;

		AnimatorComponent()
		{
			FinalBoneMatrices.assign(Animator::kMaxBones, glm::mat4(1.0f));
		}
		AnimatorComponent(const AnimatorComponent&) = default;

		const std::string& CurrentClipName() const
		{
			if (SkelData && CurrentClipIndex < (int)SkelData->Clips.size())
				return SkelData->Clips[CurrentClipIndex].Name;
			static const std::string none = "<none>";
			return none;
		}
	};

	// ── Foliage (GPU-instanced vegetation) ───────────────────────────────────
	// Attach to an entity to scatter N copies of a mesh in world space.
	// Each instance is positioned individually; the GPU draws them in one call.
	// Use the Scatter button in the editor to procedurally fill transforms,
	// or add them manually.
	struct FoliageComponent
	{
		Shared<Model>              ModelAsset;
		std::string                FilePath;
		std::vector<glm::mat4>     Transforms;     // world-space per-instance transforms

		// Wind animation parameters (applied in the vertex shader)
		bool      WindEnabled     = true;
		float     WindStrength    = 0.05f;
		float     WindFrequency   = 1.5f;
		glm::vec3 WindDirection   = { 1.0f, 0.0f, 0.0f };

		FoliageComponent() = default;
		FoliageComponent(const FoliageComponent&) = default;
	};

	// ── Audio Source ─────────────────────────────────────────────────────────
	// Attach to any entity to give it a sound.  The Scene initialises the
	// runtime handle on OnRuntimeStart and stops/frees it on OnRuntimeStop.
	struct AudioSourceComponent
	{
		std::string FilePath;           // Path to .wav / .mp3 / .ogg / .flac
		float       Volume      = 1.0f; // [0, 1]
		float       Pitch       = 1.0f; // 1.0 = normal speed
		bool        Loop        = false;
		bool        PlayOnStart = true;
		// 3-D spatial audio
		bool        Spatial     = false;
		float       MinDistance = 1.0f;
		float       MaxDistance = 50.0f;

		// --- Runtime state (not serialized) ---
		SoundHandle _RuntimeHandle = kInvalidSound;

		AudioSourceComponent() = default;
		AudioSourceComponent(const AudioSourceComponent& o)
			: FilePath(o.FilePath), Volume(o.Volume), Pitch(o.Pitch),
			  Loop(o.Loop), PlayOnStart(o.PlayOnStart),
			  Spatial(o.Spatial), MinDistance(o.MinDistance), MaxDistance(o.MaxDistance),
			  _RuntimeHandle(kInvalidSound) // never copy runtime handle
		{}
	};

	template<typename... Component>
	struct Components
	{

	};
	using AllComponents =
		Components<TransformComponent, ParticleSystemComponent, SpriteRendererComponent, CircleRendererComponent,
		CircleCollider2DComponent, BoxCollider2DComponent, CameraComponent,
		NativeScriptComponent, Rigidbody2DComponent,
		PointLightComponent, DirectionalLightComponent, SpotLightComponent, MeshComponent, MeshLODComponent,
		SpringArmComponent, AudioSourceComponent, FoliageComponent, AnimatorComponent,
		Rigidbody3DComponent, BoxCollider3DComponent, SphereCollider3DComponent, CapsuleCollider3DComponent>;




}