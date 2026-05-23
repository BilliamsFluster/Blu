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
		virtual void OnPossessed() {}
		virtual void OnUnPossessed() {}

		bool IsPlayerControlled() const        { return m_IsPlayerControlled; }
		void SetPlayerControlled(bool value)
		{
			if (m_IsPlayerControlled == value)
				return;

			m_IsPlayerControlled = value;
			if (m_IsPlayerControlled)
				OnPossessed();
			else
				OnUnPossessed();
		}

	private:
		bool m_IsPlayerControlled = false;
	};
}
