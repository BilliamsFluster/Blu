#pragma once
#include "Blu/GameFramework/ACharacter.h"

namespace Azure
{
	class ZombieTestActor : public Blu::ACharacter
	{
	public:
		float DetectRange = 18.0f;
		float AttackRange = 1.35f;
		float AttackDamage = 10.0f;
		float AttackCooldownSeconds = 1.0f;
		float MaxHealth = 100.0f;

		void BeginPlay() override;
		void Tick(float dt) override;

	private:
		float m_AttackCooldown = 0.0f;
		bool  m_Dead = false;
	};
}
