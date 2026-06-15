#pragma once
#include "Blu/GameFramework/AActor.h"
#include <cstdint>

namespace Azure
{
	// Populates the level with GPU-instanced foliage scattered over the terrain surface.
	// At BeginPlay it builds procedural grass-tuft and tree models, scatters thousands of
	// instances conforming to the procedural terrain height (flat play area left clear of
	// trees), and spawns FoliageComponent entities that the renderer draws instanced.
	class EnvironmentActor : public Blu::AActor
	{
	public:
		void BeginPlay() override;

	private:
		float Rand01();                // deterministic LCG
		uint32_t m_Seed = 0x9E3779B9u;
	};
}
