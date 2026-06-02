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
		if (!HasComponent<CapsuleCollider3DComponent>())
		{
			auto& capsule = AddComponent<CapsuleCollider3DComponent>();
			capsule.Radius = 0.3f;
			capsule.HalfHeight = 0.55f;
		}
		if (!HasComponent<VisualOffsetComponent>() && HasComponent<MeshComponent>())
		{
			auto& mesh = GetComponent<MeshComponent>();
			if (mesh.Primitive == MeshComponent::PrimitiveType::Cube && mesh.FilePath.empty() && !mesh.ModelAsset)
			{
				auto& capsule = GetComponent<CapsuleCollider3DComponent>();
				auto& visual = AddComponent<VisualOffsetComponent>();
				visual.Translation = capsule.Offset + glm::vec3(0.0f, capsule.HalfHeight + capsule.Radius, 0.0f);
				visual.Scale = glm::vec3(capsule.Radius * 2.0f, (capsule.HalfHeight + capsule.Radius) * 2.0f, capsule.Radius * 2.0f);
			}
		}
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
