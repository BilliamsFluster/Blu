#include "Blupch.h"
#include "ACharacter.h"
#include "Blu/Scene/Component.h"

namespace Blu
{
	void ACharacter::BeginPlay()
	{
		APawn::BeginPlay();
		if (!HasComponent<CharacterControllerComponent>())
			AddComponent<CharacterControllerComponent>();
	}

	void ACharacter::Move(glm::vec3 worldDirection)
	{
		if (!HasComponent<CharacterControllerComponent>()) return;
		auto& ccc = GetComponent<CharacterControllerComponent>();
		ccc._PendingMoveInput += worldDirection * ccc.MoveSpeed;
	}

	void ACharacter::Jump()
	{
		if (!HasComponent<CharacterControllerComponent>()) return;
		auto& ccc = GetComponent<CharacterControllerComponent>();
		if (ccc.IsGrounded)
			ccc._PendingJump = true;
	}

	bool ACharacter::IsGrounded()
	{
		if (!HasComponent<CharacterControllerComponent>()) return false;
		return GetComponent<CharacterControllerComponent>().IsGrounded;
	}

	float ACharacter::GetMoveSpeed()
	{
		if (!HasComponent<CharacterControllerComponent>()) return 5.0f;
		return GetComponent<CharacterControllerComponent>().MoveSpeed;
	}

	void ACharacter::SetMoveSpeed(float speed)
	{
		if (!HasComponent<CharacterControllerComponent>()) return;
		GetComponent<CharacterControllerComponent>().MoveSpeed = speed;
	}

	glm::vec3 ACharacter::GetVelocity()
	{
		if (!HasComponent<CharacterControllerComponent>()) return glm::vec3(0.0f);
		return GetComponent<CharacterControllerComponent>().Velocity;
	}
}
