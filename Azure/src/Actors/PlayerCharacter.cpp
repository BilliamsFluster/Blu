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
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Azure
{
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
	}

	void PlayerCharacter::UpdateWeapon(float dt)
	{
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

		// Ambient embers drifting ahead of the player — atmosphere + a live particle source.
		{
			glm::vec3 fwd = LookForward();
			glm::vec3 base = GetTransform().Translation + glm::vec3(0.0f, 1.0f, 0.0f) + fwd * 4.0f;
			Blu::GpuParticleSystem::Get().Emit(base, 2, glm::vec3(0.0f, 0.4f, 0.0f), 1.2f, 2.5f, 0.06f, 0.0f);
		}

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
