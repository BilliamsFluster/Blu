#pragma once
#include <string>
#include <glm/glm.hpp>
#include "Blu/Scene/SceneCamera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Blu/Rendering/ParticleSystem.h"
#include "Blu/Core/UUID.h"
#include "Blu/Rendering/Asset.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Rendering/Animation.h"
#include "Blu/Rendering/Animator.h"
#include "Blu/Rendering/Terrain.h"
#include "Blu/GameFramework/NativeClass.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
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
		enum class PrimitiveType
		{
			None = 0,
			Cube,
			Quad
		};

		Shared<class Mesh> MeshData;
		Shared<class Material> MaterialInstance;
		Shared<Model> ModelAsset;       // resolved runtime cache (rendered)
		AssetHandle ModelHandle = AssetHandle(0); // stable source reference; resolves ModelAsset
		std::string FilePath;           // deprecated fallback / human-readable source path
		PrimitiveType Primitive = PrimitiveType::None;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
	};

	struct TerrainComponent
	{
		TerrainSpec Spec;

		TerrainComponent() = default;
		TerrainComponent(const TerrainComponent&) = default;
	};

	struct VisualOffsetComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		VisualOffsetComponent() = default;
		VisualOffsetComponent(const VisualOffsetComponent&) = default;

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	// ── Level of Detail ───────────────────────────────────────────────────────
	// Each LOD entry pairs a loaded model with a maximum camera distance at which
	// it should be rendered. Levels are checked nearest-first; the last entry's
	// model is used for any distance beyond it (the "lowest quality" fallback).
	struct LODEntry
	{
		Shared<Model> ModelAsset;                  // resolved runtime cache (rendered)
		AssetHandle   ModelHandle = AssetHandle(0); // stable source reference
		float         MaxDistance = 100.0f;
		std::string   FilePath;                    // deprecated fallback
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

	// Editor-only organisational grouping for the Outliner. Path is a folder label
	// (e.g. "Lights"); empty / no component = the entity sits at the Outliner root.
	// Purely a display grouping — no transform parenting.
	struct FolderComponent
	{
		std::string Path;
		FolderComponent() = default;
		FolderComponent(const FolderComponent&) = default;
		FolderComponent(const std::string& path) : Path(path) {}
	};

	struct CameraComponent 
	{
		Blu::SceneCamera Camera;
		bool Primary = false;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
		
	};
	struct ActorComponent
	{
		ActorClassID ClassID;
		PropertyOverrideMap Overrides;

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

	struct MeshCollider3DComponent
	{
		bool Enabled          = true;
		bool DoubleSided      = true;
		float Friction        = 0.5f;
		float Restitution     = 0.0f;

		uint32_t RuntimeTriangleCount = 0;
		bool RuntimeBodyCreated       = false;
		std::string RuntimeStatus;

		MeshCollider3DComponent() = default;
		MeshCollider3DComponent(const MeshCollider3DComponent& other)
			: Enabled(other.Enabled), DoubleSided(other.DoubleSided),
			  Friction(other.Friction), Restitution(other.Restitution),
			  RuntimeTriangleCount(0), RuntimeBodyCreated(false), RuntimeStatus()
		{}
	};

	// ─── Character Controller (Jolt CharacterVirtual) ─────────────────────────
	// Attach to an entity that will be player/AI controlled.
	// Scene creates the JPH::CharacterVirtual at runtime; ACharacter::Move/Jump
	// write into _PendingMoveInput / _PendingJump which Scene consumes each frame.
	struct CharacterControllerComponent
	{
		float     MoveSpeed   = 5.0f;
		float     JumpImpulse = 7.0f;
		float     StepHeight  = 0.35f;
		float     SlopeLimit  = 45.0f;  // degrees

		// Written by Scene each frame — read by ACharacter
		bool      IsGrounded  = false;
		glm::vec3 Velocity    = { 0.0f, 0.0f, 0.0f };

		// Set by ACharacter::Move / Jump, consumed by Scene each physics tick
		glm::vec3 _PendingMoveInput = { 0.0f, 0.0f, 0.0f };
		bool      _PendingJump      = false;

		// Runtime pointer — owned by Scene, never serialized
		void* _RuntimeCharacter = nullptr;

		CharacterControllerComponent() = default;
		CharacterControllerComponent(const CharacterControllerComponent& o)
			: MoveSpeed(o.MoveSpeed), JumpImpulse(o.JumpImpulse),
			  StepHeight(o.StepHeight), SlopeLimit(o.SlopeLimit),
			  IsGrounded(false), Velocity(0.0f),
			  _PendingMoveInput(0.0f), _PendingJump(false),
			  _RuntimeCharacter(nullptr) {}
	};

	struct InteractableComponent
	{
		enum class InteractionType { Pickup = 0, Trigger, Usable };

		bool Enabled = true;
		std::string DisplayName = "Interactable";
		float InteractionRadius = 2.0f;
		InteractionType Type = InteractionType::Pickup;

		InteractableComponent() = default;
		InteractableComponent(const InteractableComponent&) = default;
	};

	struct PickupComponent
	{
		enum class PickupType { Health = 0, Stamina, GenericItem };

		PickupType Type = PickupType::Health;
		float Amount = 25.0f;
		int Count = 1;
		bool ConsumeOnPickup = true;

		PickupComponent() = default;
		PickupComponent(const PickupComponent&) = default;
	};

	struct PlayerStatsComponent
	{
		float Health = 100.0f;
		float MaxHealth = 100.0f;
		float Stamina = 100.0f;
		float MaxStamina = 100.0f;
		float StaminaRegenRate = 20.0f;
		float SprintStaminaDrain = 25.0f;

		PlayerStatsComponent() = default;
		PlayerStatsComponent(const PlayerStatsComponent&) = default;
	};

	struct UIRootComponent
	{
		std::string DocumentPath = "assets/ui/GameplayHUD.bluui";
		bool Visible = true;
		float Scale = 1.0f;

		UIRootComponent() = default;
		UIRootComponent(const UIRootComponent&) = default;
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

	// A localized fog region. Composited screen-space in the post-process pass by
	// integrating density along the view ray through the box/sphere (see
	// PostProcess_FogVolume.hlsl). Box is treated axis-aligned (rotation ignored for now);
	// sphere uses Radius. Costs nothing when no FogVolumeComponent exists in the scene.
	struct FogVolumeComponent
	{
		enum class Shape { Box = 0, Sphere = 1 };
		Shape     VolumeShape = Shape::Box;
		glm::vec3 Extents     = glm::vec3(5.0f);               // box half-extents (world units)
		float     Radius      = 5.0f;                          // sphere radius (world units)
		glm::vec3 Color       = glm::vec3(0.60f, 0.65f, 0.72f);
		float     Density     = 0.25f;                         // fog accumulated per world unit through the volume
		float     Falloff     = 1.0f;                          // 0..1 edge softness (fraction of extent that fades in)

		FogVolumeComponent() = default;
		FogVolumeComponent(const FogVolumeComponent&) = default;
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
		// If true, Yaw is relative to the target entity yaw; if false, Yaw is world/control yaw.
		bool      InheritYaw        = true;
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
		Shared<Model>              ModelAsset;     // resolved runtime cache (rendered)
		AssetHandle                ModelHandle = AssetHandle(0); // stable source reference
		std::string                FilePath;       // deprecated fallback
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
		std::string FilePath;           // Path to .wav / .mp3 / .ogg / .flac (deprecated fallback)
		AssetHandle AudioHandle = AssetHandle(0); // stable source reference
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
			: FilePath(o.FilePath), AudioHandle(o.AudioHandle), Volume(o.Volume), Pitch(o.Pitch),
			  Loop(o.Loop), PlayOnStart(o.PlayOnStart),
			  Spatial(o.Spatial), MinDistance(o.MinDistance), MaxDistance(o.MaxDistance),
			  _RuntimeHandle(kInvalidSound) // never copy runtime handle
		{}
	};

	// ── Health ───────────────────────────────────────────────────────────────
	// Damageable entities (zombies, destructibles). Runtime gameplay state — added
	// at BeginPlay by enemy actors, not serialized into scenes.
	struct HealthComponent
	{
		float Health    = 100.0f;
		float MaxHealth  = 100.0f;

		HealthComponent() = default;
		HealthComponent(const HealthComponent&) = default;
		explicit HealthComponent(float hp) : Health(hp), MaxHealth(hp) {}
	};

	// ── Projectile ───────────────────────────────────────────────────────────
	// A bullet/pellet in flight. Spawned by the weapon, stepped each frame, removed
	// on impact or when its life expires. Runtime gameplay state — not serialized.
	struct ProjectileComponent
	{
		glm::vec3 Velocity  = { 0.0f, 0.0f, 0.0f }; // world units / second
		float     Damage    = 34.0f;
		float     Life      = 2.0f;                 // seconds before despawn
		float     HitRadius = 0.9f;                 // proximity radius for a hit

		ProjectileComponent() = default;
		ProjectileComponent(const ProjectileComponent&) = default;
	};

	// ── Ammo / weapon HUD state ──────────────────────────────────────────────
	// Mirror of the possessed pawn's weapon counters so the runtime HUD can read them
	// without reaching into the actor. Synced by the player actor each frame. HitFlash is
	// the remaining lifetime (seconds) of the on-screen hitmarker. Runtime state — not serialized.
	struct AmmoComponent
	{
		int   InMag     = 0;
		int   Reserve   = 0;
		int   MagSize   = 0;
		bool  Reloading = false;
		float HitFlash  = 0.0f;

		AmmoComponent() = default;
		AmmoComponent(const AmmoComponent&) = default;
	};

	template<typename... Component>
	struct Components
	{

	};
	using AllComponents =
		Components<TransformComponent, ParticleSystemComponent, SpriteRendererComponent, CircleRendererComponent,
		CircleCollider2DComponent, BoxCollider2DComponent, CameraComponent,
		ActorComponent, Rigidbody2DComponent,
		PointLightComponent, DirectionalLightComponent, SpotLightComponent, FogVolumeComponent, MeshComponent, VisualOffsetComponent, MeshLODComponent,
		TerrainComponent, SpringArmComponent, AudioSourceComponent, FoliageComponent, AnimatorComponent,
		Rigidbody3DComponent, BoxCollider3DComponent, SphereCollider3DComponent, CapsuleCollider3DComponent,
		MeshCollider3DComponent, CharacterControllerComponent,
		InteractableComponent, PickupComponent, PlayerStatsComponent, FolderComponent>;




}
