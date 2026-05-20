#include "PlayerCharacter.h"
#include "Blu/GameFramework/ActorRegistry.h"
#include "Blu/Core/Log.h"

BLU_REGISTER_ACTOR(PlayerCharacter, Azure::PlayerCharacter);

namespace Azure
{
	void PlayerCharacter::BeginPlay()
	{
		ACharacter::BeginPlay();
		SetPlayerControlled(true);
		BLU_CORE_INFO("PlayerCharacter::BeginPlay — actor is live!");
	}

	void PlayerCharacter::SetupPlayerInput(Blu::InputMap& /*input*/)
	{
		// Bind axes here once InputMap axis-binding API is wired.
	}

	void PlayerCharacter::Tick(float dt)
	{
		m_TickAccum += dt;
		if (m_TickAccum >= 1.0f)
		{
			BLU_CORE_INFO("PlayerCharacter::Tick — running ({0:.1f}s elapsed)", m_TotalTime);
			m_TickAccum = 0.0f;
		}
		m_TotalTime += dt;
	}
}
