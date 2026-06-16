#pragma once
#include "UObject.h"

namespace Blu
{
	class AActor;
	class Scene;
	struct SceneDiagnostics;

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

		// Contribute gameplay stats (wave/score/lives/…) to the HUD diagnostics each frame.
		// Default no-op so the engine stays game-agnostic; game modes (e.g. ZombieGameMode)
		// override this to surface their state to RuntimeUI bindings.
		virtual void PopulateHUD(SceneDiagnostics& diagnostics) const {}

		// The scene that owns this game mode (set by Scene before OnGameStart). Lets the
		// game mode spawn entities / query actors for wave logic, scoring, etc.
		Scene* GetScene() const { return m_Scene; }

	private:
		Scene* m_Scene = nullptr;
		friend class Scene;
	};
}
