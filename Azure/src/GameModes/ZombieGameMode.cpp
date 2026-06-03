#include "ZombieGameMode.h"

namespace Azure
{
	void ZombieGameMode::OnGameStart()
	{
		ZombiesKilled = 0;
	}

	void ZombieGameMode::OnActorKilled(Blu::AActor*, Blu::AActor*)
	{
		++ZombiesKilled;
	}

	void ZombieGameMode::OnPlayerDeath(Blu::AActor*)
	{
		--PlayerLives;
	}

	void ZombieGameMode::Tick(float)
	{
	}
}
