#include "PlayerCharacter.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Core/MouseCodes.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Rendering/GpuParticleSystem.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Utils/AssetPath.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Azure
{
	static Blu::Shared<Blu::Model> BuildViewModelModel(); // defined below; built once in BeginPlay

	void PlayerCharacter::BeginPlay()
	{
		ACharacter::BeginPlay();  // auto-adds CharacterControllerComponent
		if (!HasComponent<Blu::PlayerStatsComponent>())
			AddComponent<Blu::PlayerStatsComponent>();
		SetPlayerControlled(true);
		SetupPlayerInput(Blu::InputMap::Get());

		// Drop any third-person spring arm so Scene::UpdateSpringArmCameras doesn't fight
		// the first-person camera we drive below.
		if (HasComponent<Blu::SpringArmComponent>())
			RemoveComponent<Blu::SpringArmComponent>();

		// HUD-readable mirror of the weapon's ammo counters (synced each Tick).
		if (!HasComponent<Blu::AmmoComponent>())
			AddComponent<Blu::AmmoComponent>();


		// First-person view-model (arms + weapon) — spawned once, re-anchored to the camera
		// every frame in UpdateViewModel().
		if (Blu::Scene* scene = GetScene())
		{
			Blu::Entity vm = scene->CreateEntity("ViewModel");
			if (!vm.HasComponent<Blu::TransformComponent>())
				vm.AddComponent<Blu::TransformComponent>();
			auto& vmc = vm.AddComponent<Blu::MeshComponent>();
			vmc.ModelAsset       = BuildViewModelModel();
			vmc.MaterialInstance = Blu::Material::Create();
			vmc.MaterialInstance->AlbedoColor      = glm::vec4(0.20f, 0.19f, 0.18f, 1.0f);
			vmc.MaterialInstance->Metallic         = 0.55f;
			vmc.MaterialInstance->Roughness        = 0.50f;
			vmc.MaterialInstance->EmissiveColor    = glm::vec3(0.05f, 0.05f, 0.055f);
			vmc.MaterialInstance->EmissiveStrength = 1.2f;
			m_ViewModelUUID = vm.GetUUID();
		}

		// First-person camera: take ownership of the scene's primary camera and drive it
		// from the pawn each frame (eye height + yaw/pitch). No third-person spring arm.
		m_Yaw   = glm::degrees(GetTransform().Rotation.y);
		m_Pitch = 0.0f;
		if (Blu::Scene* scene = GetScene())
		{
			Blu::Entity cam = scene->EnsurePrimaryCamera();
			if (cam)
				m_CameraUUID = cam.GetUUID();
		}
		ResetMouseLookState();
		BLU_CORE_INFO("PlayerCharacter::BeginPlay — first-person, input wired");
	}

	void PlayerCharacter::OnPossessed()
	{
		m_Yaw = glm::degrees(GetTransform().Rotation.y);
		m_Pitch = 0.0f;
		ResetMouseLookState();
	}

	void PlayerCharacter::OnUnPossessed()
	{
		ResetMouseLookState();
	}

	void PlayerCharacter::SetupPlayerInput(Blu::InputMap& input)
	{
		input.AddAxis("MoveForward", BLU_KEY_W, BLU_KEY_S);
		input.AddAxis("MoveRight",   BLU_KEY_D, BLU_KEY_A);
		input.AddAction("Jump",   BLU_KEY_SPACE);
		input.AddAction("Sprint", BLU_KEY_LEFT_SHIFT);
		input.AddAction("Interact", BLU_KEY_E);
		input.AddAction("Reload", BLU_KEY_R);
		input.AddMouseAction("Fire", BLU_MOUSE_BUTTON_LEFT);
	}

	void PlayerCharacter::ResetMouseLookState()
	{
		auto [mouseX, mouseY] = Blu::Input::GetMousePosition();
		m_PrevMouseX = mouseX;
		m_PrevMouseY = mouseY;
		m_FirstMouse = true;
	}

	// Build a chunky procedural first-person view-model: a boxy rifle plus two gloved
	// forearms, all in local space with -Z pointing "forward" (matching the camera basis,
	// where the entity's local -Z maps to the look direction). One shared cube VAO drives
	// every box via its per-submesh LocalTransform.
	static Blu::Shared<Blu::Model> BuildViewModelModel()
	{
		auto cube  = Blu::Mesh::CreateCube();
		auto model = std::make_shared<Blu::Model>();

		// Two materials: 0 = dark gunmetal, 1 = tactical glove/arm (lighter, matte) so the
		// arms read distinctly from the weapon. Faint emissive keeps them visible at night.
		auto gun = Blu::Material::Create();
		gun->AlbedoColor      = glm::vec4(0.17f, 0.17f, 0.19f, 1.0f);
		gun->Metallic         = 0.65f;
		gun->Roughness        = 0.42f;
		gun->EmissiveColor    = glm::vec3(0.04f, 0.04f, 0.05f);
		gun->EmissiveStrength = 1.0f;
		auto glove = Blu::Material::Create();
		glove->AlbedoColor      = glm::vec4(0.34f, 0.30f, 0.26f, 1.0f);
		glove->Metallic         = 0.05f;
		glove->Roughness        = 0.85f;
		glove->EmissiveColor    = glm::vec3(0.05f, 0.045f, 0.04f);
		glove->EmissiveStrength = 1.2f;
		model->Materials = { gun, glove };

		auto addBox = [&](const glm::vec3& pos, const glm::vec3& size, float pitchDeg, int matIdx)
		{
			Blu::SubMesh sm;
			sm.VAO          = cube->GetVertexArray();
			sm.IndexCount   = cube->GetIndexCount();
			sm.MaterialIndex = matIdx;
			glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
			if (pitchDeg != 0.0f)
				m = glm::rotate(m, glm::radians(pitchDeg), glm::vec3(1.0f, 0.0f, 0.0f));
			m = glm::scale(m, size);
			sm.LocalTransform  = m;
			sm.BoundingCenter  = cube->GetBoundingCenter();
			sm.BoundingRadius  = cube->GetBoundingRadius();
			model->Meshes.push_back(std::move(sm));
		};

		// Rifle (−Z = muzzle direction) — material 0
		addBox({ 0.0f,  0.000f, -0.16f }, { 0.075f, 0.100f, 0.34f }, 0.0f, 0);  // receiver
		addBox({ 0.0f,  0.020f, -0.44f }, { 0.040f, 0.045f, 0.30f }, 0.0f, 0);  // barrel
		addBox({ 0.0f, -0.005f, -0.30f }, { 0.060f, 0.060f, 0.18f }, 0.0f, 0);  // handguard
		addBox({ 0.0f, -0.010f,  0.07f }, { 0.065f, 0.090f, 0.16f }, 0.0f, 0);  // stock
		addBox({ 0.0f,  0.085f, -0.26f }, { 0.018f, 0.050f, 0.022f }, 0.0f, 0); // front sight
		addBox({ 0.0f, -0.100f, -0.01f }, { 0.050f, 0.140f, 0.06f }, 10.0f, 0); // pistol grip
		addBox({ 0.0f, -0.130f, -0.13f }, { 0.045f, 0.150f, 0.05f }, 0.0f, 0);  // magazine
		// Gloved forearms + fists — material 1
		addBox({  0.070f, -0.160f,  0.07f }, { 0.055f, 0.055f, 0.26f }, -22.0f, 1); // right forearm
		addBox({  0.020f, -0.105f, -0.02f }, { 0.075f, 0.085f, 0.085f }, 0.0f, 1);  // right fist (grip)
		addBox({ -0.070f, -0.155f, -0.18f }, { 0.050f, 0.050f, 0.22f }, -16.0f, 1); // left forearm
		addBox({ -0.020f, -0.070f, -0.30f }, { 0.075f, 0.075f, 0.075f }, 0.0f, 1);  // left fist (handguard)

		return model;
	}

	glm::vec3 PlayerCharacter::LookForward() const
	{
		// Forward matching the engine convention (yaw 0 looks down world -Z).
		const float yawRad   = glm::radians(m_Yaw);
		const float pitchRad = glm::radians(m_Pitch);
		return glm::normalize(glm::vec3(
			std::cos(pitchRad) * std::sin(yawRad),
			std::sin(pitchRad),
			-std::cos(pitchRad) * std::cos(yawRad)));
	}

	void PlayerCharacter::UpdateFirstPersonCamera()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		Blu::Entity cam = (Blu::UUID)m_CameraUUID != 0 ? scene->GetEntityByUUID(m_CameraUUID) : Blu::Entity{};
		if (!cam)
		{
			cam = scene->EnsurePrimaryCamera();
			if (cam) m_CameraUUID = cam.GetUUID();
		}
		if (!cam || !cam.HasComponent<Blu::TransformComponent>())
			return;

		glm::vec3 forward = LookForward();
		glm::vec3 up = (std::abs(forward.y) > 0.98f) ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		auto& camXform = cam.GetComponent<Blu::TransformComponent>();
		camXform.Translation = GetTransform().Translation + glm::vec3(0.0f, m_EyeHeight, 0.0f);
		camXform.Rotation = glm::eulerAngles(glm::quatLookAtRH(forward, up));

		UpdateViewModel();
	}

	void PlayerCharacter::UpdateViewModel()
	{
		Blu::Scene* scene = GetScene();
		if (!scene || (Blu::UUID)m_ViewModelUUID == 0)
			return;
		Blu::Entity vm = scene->GetEntityByUUID(m_ViewModelUUID);
		if (!vm || !vm.HasComponent<Blu::TransformComponent>())
			return;

		const glm::vec3 forward = LookForward();
		const glm::vec3 worldUp = (std::abs(forward.y) > 0.98f) ? glm::vec3(0.0f, 0.0f, -1.0f)
		                                                        : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::quat q     = glm::quatLookAtRH(forward, worldUp);
		const glm::vec3 right = q * glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 vmUp  = q * glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 eye   = GetTransform().Translation + glm::vec3(0.0f, m_EyeHeight, 0.0f);

		// Offset the weapon to the lower-right of the view and a little forward.
		const glm::vec3 pos = eye + right * 0.15f + vmUp * (-0.21f) + forward * 0.32f;

		auto& t       = vm.GetComponent<Blu::TransformComponent>();
		t.Translation = pos;
		t.Rotation    = glm::eulerAngles(q);  // same basis as the camera; local -Z → forward
		t.Scale       = glm::vec3(1.0f);
	}

	void PlayerCharacter::FireWeapon()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		const glm::vec3 forward = LookForward();
		const glm::vec3 muzzle = GetTransform().Translation + glm::vec3(0.0f, m_EyeHeight, 0.0f) + forward * 0.6f;

		Blu::Entity proj = scene->CreateEntity("Projectile");
		auto& t = proj.HasComponent<Blu::TransformComponent>()
			? proj.GetComponent<Blu::TransformComponent>()
			: proj.AddComponent<Blu::TransformComponent>();
		t.Translation = muzzle;
		t.Scale = glm::vec3(0.12f);

		auto& mc = proj.AddComponent<Blu::MeshComponent>();
		mc.MeshData = Blu::Mesh::CreateCube();
		mc.Primitive = Blu::MeshComponent::PrimitiveType::Cube;
		mc.MaterialInstance = Blu::Material::Create();
		mc.MaterialInstance->AlbedoColor = glm::vec4(1.0f, 0.85f, 0.30f, 1.0f);
		mc.MaterialInstance->EmissiveColor = glm::vec3(1.0f, 0.75f, 0.20f);
		mc.MaterialInstance->EmissiveStrength = 4.0f;

		auto& pc = proj.AddComponent<Blu::ProjectileComponent>();
		pc.Velocity  = forward * m_ProjectileSpeed;
		pc.Damage    = m_WeaponDamage;
		pc.Life      = 2.0f;
		pc.HitRadius = 0.9f;

		// Muzzle flash: a bright, short-range transient point light at the muzzle. Lasts a
		// single frame (cleared next runtime tick), so continuous fire reads as a flicker.
		Blu::Renderer3D::AddDynamicLight(muzzle, glm::vec3(1.0f, 0.78f, 0.40f), 12.0f, 7.0f);

		// Transient muzzle-flash sparks — a brief forward burst, only when firing (not a
		// persistent emitter). Short life so it pops and clears with the shot cadence.
		Blu::GpuParticleSystem::Get().Emit(muzzle, 12, forward * 3.0f, 2.2f, 0.12f, 0.06f, 0.0f);

		// Gunshot SFX (one handle re-triggered per shot — Play seeks to frame 0).
		if (m_GunshotSound != Blu::kInvalidSound)
		{
			Blu::AudioEngine::Get().SetVolume(m_GunshotSound, 0.85f);
			Blu::AudioEngine::Get().Play(m_GunshotSound);
		}
	}

	void PlayerCharacter::UpdateWeapon(float dt)
	{
		// Lazy-load weapon SFX once — the AudioEngine isn't initialized during BeginPlay,
		// but it is by the first runtime tick. (Audio isn't captured headlessly; the log
		// confirms the load succeeded, i.e. handles != 0.)
		if (!m_TriedAudio)
		{
			m_TriedAudio   = true;
			m_GunshotSound = Blu::AudioEngine::Get().LoadSound(
				Blu::AssetPath::ResolvePath("assets/audio/gunshot.wav").string());
			m_ImpactSound  = Blu::AudioEngine::Get().LoadSound(
				Blu::AssetPath::ResolvePath("assets/audio/impact.wav").string());
			BLU_CORE_INFO("PlayerCharacter: weapon SFX loaded (gunshot={0}, impact={1})",
			              m_GunshotSound, m_ImpactSound);
		}

		auto& input = Blu::InputMap::Get();
		if (m_FireCooldown > 0.0f)
			m_FireCooldown -= dt;

		if (m_Reloading)
		{
			m_ReloadTimer -= dt;
			if (m_ReloadTimer <= 0.0f)
			{
				const int need = m_MagSize - m_AmmoInMag;
				const int take = std::min(need, m_AmmoReserve);
				m_AmmoInMag   += take;
				m_AmmoReserve -= take;
				m_Reloading = false;
			}
			return;
		}

		if (input.IsActionJustPressed("Reload") && m_AmmoInMag < m_MagSize && m_AmmoReserve > 0)
		{
			m_Reloading   = true;
			m_ReloadTimer = m_ReloadDuration;
			return;
		}

		if (input.IsActionPressed("Fire") && m_FireCooldown <= 0.0f)
		{
			if (m_AmmoInMag > 0)
			{
				FireWeapon();
				--m_AmmoInMag;
				m_FireCooldown = m_FireInterval;
			}
			else if (m_AmmoReserve > 0)
			{
				m_Reloading   = true;
				m_ReloadTimer = m_ReloadDuration;
			}
		}
	}

	void PlayerCharacter::UpdateProjectiles(float dt)
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		std::vector<Blu::Entity> toDestroy;
		auto projectiles = scene->GetAllEntitiesWith<Blu::TransformComponent, Blu::ProjectileComponent>();
		for (auto e : projectiles)
		{
			auto&& [t, proj] = projectiles.get<Blu::TransformComponent, Blu::ProjectileComponent>(e);
			t.Translation += proj.Velocity * dt;
			proj.Life -= dt;

			bool hit = false;
			auto targets = scene->GetAllEntitiesWith<Blu::TransformComponent, Blu::HealthComponent>();
			for (auto z : targets)
			{
				auto&& [zt, zh] = targets.get<Blu::TransformComponent, Blu::HealthComponent>(z);
				if (zh.Health <= 0.0f)
					continue;
				if (glm::length2(zt.Translation - t.Translation) <= proj.HitRadius * proj.HitRadius)
				{
					zh.Health -= proj.Damage;
					hit = true;
					// Impact spark burst + a brief warm flash light at the hit point.
					Blu::GpuParticleSystem::Get().Emit(t.Translation, 16, glm::vec3(0.0f, 1.5f, 0.0f), 4.0f, 0.5f, 0.09f, 0.0f);
					Blu::Renderer3D::AddDynamicLight(t.Translation, glm::vec3(1.0f, 0.65f, 0.25f), 6.0f, 4.5f);
					if (HasComponent<Blu::AmmoComponent>())
						GetComponent<Blu::AmmoComponent>().HitFlash = 0.15f; // flash the hitmarker
					if (m_ImpactSound != Blu::kInvalidSound)
						Blu::AudioEngine::Get().Play(m_ImpactSound);
					break;
				}
			}

			if (hit || proj.Life <= 0.0f)
				toDestroy.push_back(Blu::Entity{ e, scene });
		}

		for (auto& e : toDestroy)
			scene->DestroyEntity(e);
	}

	void PlayerCharacter::UpdateStats(float dt, bool wantsSprint, bool isMoving, float& outSpeedScale)
	{
		outSpeedScale = 1.0f;
		if (!HasComponent<Blu::PlayerStatsComponent>())
			return;

		auto& stats = GetComponent<Blu::PlayerStatsComponent>();
		stats.MaxHealth = std::max(stats.MaxHealth, 1.0f);
		stats.MaxStamina = std::max(stats.MaxStamina, 1.0f);
		stats.Health = glm::clamp(stats.Health, 0.0f, stats.MaxHealth);
		stats.Stamina = glm::clamp(stats.Stamina, 0.0f, stats.MaxStamina);

		const bool canSprint = wantsSprint && isMoving && stats.Stamina > 0.1f;
		if (canSprint)
		{
			stats.Stamina = std::max(0.0f, stats.Stamina - stats.SprintStaminaDrain * dt);
			outSpeedScale = stats.Stamina > 0.0f ? 2.0f : 1.0f;
		}
		else
		{
			stats.Stamina = std::min(stats.MaxStamina, stats.Stamina + stats.StaminaRegenRate * dt);
		}
	}

	void PlayerCharacter::TryInteract()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		const glm::vec3 playerPos = GetTransform().Translation;
		Blu::Entity bestEntity;
		float bestDistSq = std::numeric_limits<float>::max();

		auto view = scene->GetAllEntitiesWith<Blu::TransformComponent, Blu::InteractableComponent>();
		for (auto e : view)
		{
			auto&& [transform, interactable] = view.get<Blu::TransformComponent, Blu::InteractableComponent>(e);
			if (!interactable.Enabled)
				continue;

			float distSq = glm::length2(transform.Translation - playerPos);
			float radiusSq = interactable.InteractionRadius * interactable.InteractionRadius;
			if (distSq <= radiusSq && distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestEntity = Blu::Entity{ e, scene };
			}
		}

		if (!bestEntity)
			return;

		auto& interactable = bestEntity.GetComponent<Blu::InteractableComponent>();
		if (interactable.Type == Blu::InteractableComponent::InteractionType::Pickup && bestEntity.HasComponent<Blu::PickupComponent>())
			ApplyPickup(bestEntity);
	}

	bool PlayerCharacter::TryPickupOverlap()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return false;

		const glm::vec3 playerPos = GetTransform().Translation;
		Blu::Entity bestEntity;
		float bestDistSq = std::numeric_limits<float>::max();

		auto view = scene->GetAllEntitiesWith<Blu::TransformComponent, Blu::InteractableComponent, Blu::PickupComponent>();
		for (auto e : view)
		{
			auto&& [transform, interactable, pickup] = view.get<Blu::TransformComponent, Blu::InteractableComponent, Blu::PickupComponent>(e);
			if (!interactable.Enabled || interactable.Type != Blu::InteractableComponent::InteractionType::Pickup)
				continue;
			if (!pickup.ConsumeOnPickup)
				continue;

			float distSq = glm::length2(transform.Translation - playerPos);
			float radiusSq = interactable.InteractionRadius * interactable.InteractionRadius;
			if (distSq <= radiusSq && distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestEntity = Blu::Entity{ e, scene };
			}
		}

		return ApplyPickup(bestEntity);
	}

	bool PlayerCharacter::ApplyPickup(Blu::Entity pickupEntity)
	{
		if (!pickupEntity || !pickupEntity.HasComponent<Blu::InteractableComponent>() || !pickupEntity.HasComponent<Blu::PickupComponent>())
			return false;

		auto& interactable = pickupEntity.GetComponent<Blu::InteractableComponent>();
		auto& pickup = pickupEntity.GetComponent<Blu::PickupComponent>();
		if (!interactable.Enabled)
			return false;

		if (HasComponent<Blu::PlayerStatsComponent>())
		{
			auto& stats = GetComponent<Blu::PlayerStatsComponent>();
			switch (pickup.Type)
			{
				case Blu::PickupComponent::PickupType::Health:
					stats.Health = std::min(stats.MaxHealth, stats.Health + pickup.Amount);
					break;
				case Blu::PickupComponent::PickupType::Stamina:
					stats.Stamina = std::min(stats.MaxStamina, stats.Stamina + pickup.Amount);
					break;
				case Blu::PickupComponent::PickupType::GenericItem:
					break;
			}
		}

		BLU_CORE_INFO("PlayerCharacter: picked up {0}", interactable.DisplayName);
		if (pickup.ConsumeOnPickup)
		{
			interactable.Enabled = false;
			if (pickupEntity.HasComponent<Blu::TransformComponent>())
				pickupEntity.GetComponent<Blu::TransformComponent>().Scale = glm::vec3(0.0f);
		}
		return true;
	}

	void PlayerCharacter::Tick(float dt)
	{
		if (!IsPlayerControlled() || (GetScene() && !GetScene()->IsPlayerInputEnabled()))
		{
			ResetMouseLookState();
			return;
		}

		// ── Mouse look (first-person) ─────────────────────────────────────────
		const float kSens = 0.12f;
		auto [mouseX, mouseY] = Blu::Input::GetMousePosition();
		if (m_FirstMouse)
		{
			m_PrevMouseX = mouseX;
			m_PrevMouseY = mouseY;
			m_FirstMouse = false;
		}
		float dx = mouseX - m_PrevMouseX;
		float dy = mouseY - m_PrevMouseY;
		m_PrevMouseX = mouseX;
		m_PrevMouseY = mouseY;

		m_Yaw  += dx * kSens;
		m_Pitch = glm::clamp(m_Pitch - dy * kSens, -89.0f, 89.0f);

		// Body yaw follows the look direction; pitch tilts only the camera.
		GetTransform().Rotation.y = glm::radians(m_Yaw);

		// ── WASD movement (view-relative) ─────────────────────────────────────
		float fwd   = Blu::InputMap::Get().GetAxis("MoveForward");
		float right = Blu::InputMap::Get().GetAxis("MoveRight");
		const bool isMoving = std::abs(fwd) > 0.001f || std::abs(right) > 0.001f;
		float speedScale = 1.0f;
		UpdateStats(dt, Blu::InputMap::Get().IsActionPressed("Sprint"), isMoving, speedScale);

		if (isMoving)
		{
			const float yawRad = glm::radians(m_Yaw);
			glm::vec3 camFwd   = { std::sin(yawRad), 0.0f, -std::cos(yawRad) };
			glm::vec3 camRight = { std::cos(yawRad), 0.0f,  std::sin(yawRad) };
			glm::vec3 moveDir = camFwd * fwd + camRight * right;
			if (glm::length(moveDir) > 0.001f)
			{
				moveDir = glm::normalize(moveDir);
				if (speedScale != 1.0f && HasComponent<Blu::CharacterControllerComponent>())
				{
					auto& ccc = GetComponent<Blu::CharacterControllerComponent>();
					float saved = ccc.MoveSpeed;
					ccc.MoveSpeed *= speedScale;
					Move(moveDir);
					ccc.MoveSpeed = saved;
				}
				else
				{
					Move(moveDir);
				}
			}
		}

		// ── Jump ──────────────────────────────────────────────────────────────
		if (Blu::InputMap::Get().IsActionJustPressed("Jump"))
			Jump();

		// Drive the first-person camera after movement so it tracks this frame's position.
		UpdateFirstPersonCamera();

		// Weapon: fire/reload, then advance live projectiles and resolve hits.
		UpdateWeapon(dt);
		UpdateProjectiles(dt);

		// Mirror weapon state into the HUD-readable AmmoComponent + age the hitmarker.
		if (HasComponent<Blu::AmmoComponent>())
		{
			auto& ammo = GetComponent<Blu::AmmoComponent>();
			ammo.InMag     = m_AmmoInMag;
			ammo.Reserve   = m_AmmoReserve;
			ammo.MagSize   = m_MagSize;
			ammo.Reloading = m_Reloading;
			ammo.HitFlash  = std::max(0.0f, ammo.HitFlash - dt);
		}

		TryPickupOverlap();
		if (Blu::InputMap::Get().IsActionJustPressed("Interact"))
			TryInteract();
	}
}
