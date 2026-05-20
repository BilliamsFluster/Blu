#pragma once
#include "AActor.h"

namespace Blu
{
	class InputMap;

	// APawn is an Actor that can be "possessed" by a player or AI controller.
	// Override SetupPlayerInput() to bind axes/actions to movement logic.
	class APawn : public AActor
	{
	public:
		virtual void SetupPlayerInput(InputMap& input) {}

		bool IsPlayerControlled() const        { return m_IsPlayerControlled; }
		void SetPlayerControlled(bool value)   { m_IsPlayerControlled = value; }

	private:
		bool m_IsPlayerControlled = false;
	};
}
