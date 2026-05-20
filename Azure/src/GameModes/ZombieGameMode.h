#pragma once
#include "Blu/GameFramework/AGameMode.h"

namespace Azure
{
	class ZombieGameMode : public Blu::AGameMode
	{
	public:
		int TotalZombies    = 10;
		int ZombiesKilled   = 0;
		int PlayerLives     = 3;

		void OnGameStart() override;
		void OnActorKilled(Blu::AActor* victim, Blu::AActor* killer) override;
		void OnPlayerDeath(Blu::AActor* player) override;
		void Tick(float dt) override;

		bool IsGameOver() const { return PlayerLives <= 0; }
		bool IsVictory()  const { return ZombiesKilled >= TotalZombies; }
	};
}
