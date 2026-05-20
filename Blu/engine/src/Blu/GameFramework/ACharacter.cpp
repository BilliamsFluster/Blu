#include "Blupch.h"
#include "ACharacter.h"

// CharacterControllerComponent integration lives here once that component is added.
// These are intentional stubs — each method will be wired when the component exists.

namespace Blu
{
	void ACharacter::BeginPlay()
	{
		APawn::BeginPlay();
		// Future: auto-add CharacterControllerComponent if absent.
	}

	void ACharacter::Move(glm::vec3 /*worldDirection*/)
	{
		// Future: set velocity on CharacterControllerComponent.
	}

	void ACharacter::Jump()
	{
		// Future: trigger jump on CharacterControllerComponent.
	}

	bool ACharacter::IsGrounded() const
	{
		return false; // Future: query CharacterControllerComponent.
	}

	float ACharacter::GetMoveSpeed() const
	{
		return 5.0f; // Future: read from CharacterControllerComponent.
	}

	void ACharacter::SetMoveSpeed(float /*speed*/)
	{
		// Future: write to CharacterControllerComponent.
	}

	glm::vec3 ACharacter::GetVelocity() const
	{
		return glm::vec3(0.0f); // Future: read from CharacterControllerComponent.
	}
}
