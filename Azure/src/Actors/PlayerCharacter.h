#pragma once
#include "Blu/GameFramework/ACharacter.h"
#include "Blu/Core/InputMap.h"

namespace Azure
{
	// The player-controlled character.
	// Reads WASD + mouse from InputMap and drives ACharacter locomotion.
	class PlayerCharacter : public Blu::ACharacter
	{
	public:
		void BeginPlay() override;
		void Tick(float dt) override;
		void SetupPlayerInput(Blu::InputMap& input) override;
		void OnPossessed() override;
		void OnUnPossessed() override;

	private:
		void ResetMouseLookState();
		void FaceMovementDirection(const glm::vec3& moveDir, float dt);

		float m_Yaw       = 0.0f;
		float m_Pitch     = 0.0f;
		float m_PrevMouseX = 0.0f;
		float m_PrevMouseY = 0.0f;
		bool  m_FirstMouse = true;
		float m_TickAccum  = 0.0f;
		float m_TotalTime  = 0.0f;
	};
}
