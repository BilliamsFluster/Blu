#pragma once
#include "UObject.h"

namespace Blu
{
	class AActor;

	// AGameMode owns the rules of the game.
	// It has no transform and is not an ECS entity — it lives as a plain C++ object
	// owned by the Scene or the application layer.
	class AGameMode : public UObject
	{
	public:
		virtual void OnGameStart()                                 {}
		virtual void OnPlayerDeath(AActor* player)                 {}
		virtual void OnActorKilled(AActor* victim, AActor* killer) {}
		virtual void Tick(float deltaTime)                         {}
	};
}
