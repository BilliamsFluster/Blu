#pragma once
#include "APawn.h"
#include <glm/glm.hpp>

namespace Blu
{
	// ACharacter is a Pawn with built-in character locomotion.
	// Movement is backed by CharacterControllerComponent (added in BeginPlay if absent).
	// Call Move() / Jump() from Tick() to drive the character each frame.
	class ACharacter : public APawn
	{
	public:
		void BeginPlay() override;

		// Drive character locomotion — call from Tick().
		void      Move(glm::vec3 worldDirection);
		void      Jump();
		bool      IsGrounded();
		float     GetMoveSpeed();
		void      SetMoveSpeed(float speed);
		glm::vec3 GetVelocity();
	};
}
