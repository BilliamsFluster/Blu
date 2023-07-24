#pragma once
#include "Blu/Core/Core.h"

namespace Blu
{

	// Input class declaration, this is a static class (has only static methods and attributes)
	class BLU_API Input
	{
	public:
		// Checks if a specific key, identified by a keycode, is being pressed
		static bool IsKeyPressed(int keycode);

		// Checks if a specific mouse button, identified by a button code, is being pressed
		static bool IsMouseButtonPressed(int button);

		// Gets the current position of the mouse on the screen
		// Returns a pair of floats, where the first element is the X position and the second element is the Y position
		static std::pair<float, float> GetMousePosition();

		// Gets the current X position of the mouse on the screen
		static float GetMouseX();

		// Gets the current Y position of the mouse on the screen
		static float GetMouseY();

		// Returns the keycode of the key that was pressed
		static int GetKeyCode() { return m_KeyCode; }

		// Returns the code of the mouse button that was pressed
		static int GetMouseCode() { return m_MouseCode; }
	private:
		// Static attributes to keep the keycode and mouse button code that was pressed
		// As this is a static class, these attributes will be shared among all instances of this class
		static int m_KeyCode, m_MouseCode;
	};
	
}