#pragma once
#include "Blu/GameFramework/ACharacter.h"

namespace Azure
{
	// Basic zombie enemy.
	// Chases the player and deals damage on contact.
	// AI logic will be wired once AISystem exists.
	class ZombieCharacter : public Blu::ACharacter
	{
	public:
		float DetectRange  = 20.0f;
		float AttackRange  = 1.5f;
		float AttackDamage = 15.0f;

		void BeginPlay() override;
		void Tick(float dt) override;

	private:
		float m_AttackCooldown = 0.0f;
	};
}
