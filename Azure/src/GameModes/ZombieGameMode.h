#pragma once
#include "Blu/GameFramework/AGameMode.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Azure
{
	// Wave-based survival rules: escalating waves of zombies spawn around the arena; clear a
	// wave to trigger the next after a short breather. Win by clearing all waves, lose when
	// the player runs out of lives.
	class ZombieGameMode : public Blu::AGameMode
	{
	public:
		int ZombiesKilled = 0;
		int PlayerLives   = 3;
		int CurrentWave   = 0;
		int TotalWaves    = 5;

		void OnGameStart() override;
		void OnActorKilled(Blu::AActor* victim, Blu::AActor* killer) override;
		void OnPlayerDeath(Blu::AActor* player) override;
		void Tick(float dt) override;

		bool IsGameOver() const { return PlayerLives <= 0; }
		bool IsVictory()  const { return CurrentWave > TotalWaves; }

	private:
		void StartWave(int wave);
		void SpawnZombie(const glm::vec3& pos);
		int  CountAliveZombies();
		float Rand01();

		bool     m_WaveActive    = false;
		float    m_WaveGrace     = 0.0f;  // ignore clear-checks briefly after spawning
		float    m_NextWaveTimer = 0.0f;  // delay before the next wave
		uint32_t m_Seed          = 0x1337beefu;
	};
}
