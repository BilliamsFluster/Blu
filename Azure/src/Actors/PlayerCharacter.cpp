#include "PlayerCharacter.h"
#include "Blu/GameFramework/ActorRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Scene.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

BLU_REGISTER_ACTOR(PlayerCharacter, Azure::PlayerCharacter);

namespace Azure
{
	void PlayerCharacter::BeginPlay()
	{
		ACharacter::BeginPlay();  // auto-adds CharacterControllerComponent
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
		float speedScale = Blu::InputMap::Get().IsActionPressed("Sprint") ? 2.0f : 1.0f;

		if (std::abs(fwd) > 0.001f || std::abs(right) > 0.001f)
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
	}
}
