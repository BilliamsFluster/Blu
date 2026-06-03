#include "ZombieCharacter.h"

namespace Azure
{
	void ZombieCharacter::BeginPlay()
	{
		ACharacter::BeginPlay();
		SetMoveSpeed(2.5f);
	}

	void ZombieCharacter::Tick(float dt)
	{
		if (m_AttackCooldown > 0.0f)
			m_AttackCooldown -= dt;

		// AI chasing / attack logic will go here once AISystem is implemented.
	}
}
