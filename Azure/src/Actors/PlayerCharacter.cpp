#include "PlayerCharacter.h"
#include "Blu/GameFramework/ActorRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Scene.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

BLU_REGISTER_ACTOR(PlayerCharacter, Azure::PlayerCharacter);

namespace Azure
{
	void PlayerCharacter::BeginPlay()
	{
		ACharacter::BeginPlay();  // auto-adds CharacterControllerComponent
		if (!HasComponent<Blu::PlayerStatsComponent>())
			AddComponent<Blu::PlayerStatsComponent>();
		if (HasComponent<Blu::SpringArmComponent>())
			GetComponent<Blu::SpringArmComponent>().InheritYaw = false;
		SetPlayerControlled(true);
		SetupPlayerInput(Blu::InputMap::Get());
		ResetMouseLookState();
		BLU_CORE_INFO("PlayerCharacter::BeginPlay — actor live, input wired");
	}

	void PlayerCharacter::OnPossessed()
	{
		if (HasComponent<Blu::SpringArmComponent>())
		{
			auto& arm = GetComponent<Blu::SpringArmComponent>();
			m_Yaw = arm.Yaw;
			m_Pitch = arm.Pitch;
		}
		else
		{
			m_Yaw = glm::degrees(GetTransform().Rotation.y);
			m_Pitch = glm::degrees(GetTransform().Rotation.x);
		}
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
	}

	void PlayerCharacter::ResetMouseLookState()
	{
		auto [mouseX, mouseY] = Blu::Input::GetMousePosition();
		m_PrevMouseX = mouseX;
		m_PrevMouseY = mouseY;
		m_FirstMouse = true;
	}

	void PlayerCharacter::FaceMovementDirection(const glm::vec3& moveDir, float dt)
	{
		if (glm::length(moveDir) <= 0.001f)
			return;

		auto& transform = GetTransform();
		float targetYaw = std::atan2(moveDir.x, -moveDir.z);
		float currentYaw = transform.Rotation.y;
		float yawDelta = std::remainder(targetYaw - currentYaw, glm::two_pi<float>());
		float alpha = dt > 0.0f ? 1.0f - std::exp(-12.0f * dt) : 1.0f;
		transform.Rotation.y = std::remainder(currentYaw + yawDelta * alpha, glm::two_pi<float>());
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

		// ── Mouse look ────────────────────────────────────────────────────────
		const float kSens = 0.15f;
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

		if (HasComponent<Blu::SpringArmComponent>())
		{
			auto& arm = GetComponent<Blu::SpringArmComponent>();
			m_Yaw = arm.Yaw;
			m_Pitch = arm.Pitch;
		}

		m_Yaw   += dx * kSens;
		m_Pitch  = glm::clamp(m_Pitch - dy * kSens, -80.0f, 20.0f);

		if (HasComponent<Blu::SpringArmComponent>())
		{
			auto& arm  = GetComponent<Blu::SpringArmComponent>();
			arm.Yaw   = m_Yaw;
			arm.Pitch = m_Pitch;
		}

		// ── WASD movement (camera-relative) ───────────────────────────────────
		float fwd   = Blu::InputMap::Get().GetAxis("MoveForward");
		float right = Blu::InputMap::Get().GetAxis("MoveRight");
		const bool isMoving = std::abs(fwd) > 0.001f || std::abs(right) > 0.001f;
		float speedScale = 1.0f;
		UpdateStats(dt, Blu::InputMap::Get().IsActionPressed("Sprint"), isMoving, speedScale);

		if (isMoving)
		{
			float controlYaw = m_Yaw;
			if (HasComponent<Blu::SpringArmComponent>())
			{
				auto& arm = GetComponent<Blu::SpringArmComponent>();
				if (arm.InheritYaw)
					controlYaw += glm::degrees(GetTransform().Rotation.y);
			}

			// Match the engine camera convention: yaw 0 looks down world -Z.
			float yawRad  = glm::radians(controlYaw);
			glm::vec3 camFwd   = { std::sin(yawRad), 0.0f, -std::cos(yawRad) };
			glm::vec3 camRight = { std::cos(yawRad), 0.0f,  std::sin(yawRad) };

			glm::vec3 moveDir = camFwd * fwd + camRight * right;
			if (glm::length(moveDir) > 0.001f)
			{
				moveDir = glm::normalize(moveDir);

				if (speedScale != 1.0f && HasComponent<Blu::CharacterControllerComponent>())
				{
					auto& ccc = GetComponent<Blu::CharacterControllerComponent>();
					float saved  = ccc.MoveSpeed;
					ccc.MoveSpeed *= speedScale;
					Move(moveDir);
					ccc.MoveSpeed = saved;
				}
				else
				{
					Move(moveDir);
				}

				FaceMovementDirection(moveDir, dt);
			}
		}

		// ── Jump ──────────────────────────────────────────────────────────────
		if (Blu::InputMap::Get().IsActionJustPressed("Jump"))
			Jump();

		TryPickupOverlap();

		const bool interactPressed = Blu::InputMap::Get().IsActionJustPressed("Interact")
			|| (Blu::Input::IsKeyPressed(BLU_KEY_E) && !m_InteractHeld);
		m_InteractHeld = Blu::Input::IsKeyPressed(BLU_KEY_E);
		if (interactPressed)
			TryInteract();
	}
}
