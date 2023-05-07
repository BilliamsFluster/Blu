#pragma once
#include "Blupch.h"



namespace Blu
{
	using KeyCode = uint16_t;

	namespace Key
	{
		enum : KeyCode
		{
			// From glfw3.h
			Space = 32,
			Apostrophe = 39, /* ' */
			Comma = 44, /* , */
			Minus = 45, /* - */
			Period = 46, /* . */
			Slash = 47, /* / */

			D0 = 48, /* 0 */
			D1 = 49, /* 1 */
			D2 = 50, /* 2 */
			D3 = 51, /* 3 */
			D4 = 52, /* 4 */
			D5 = 53, /* 5 */
			D6 = 54, /* 6 */
			D7 = 55, /* 7 */
			D8 = 56, /* 8 */
			D9 = 57, /* 9 */

			Semicolon = 59, /* ; */
			Equal = 61, /* = */

			A = 65,
			B = 66,
			C = 67,
			D = 68,
			E = 69,
			F = 70,
			G = 71,
			H = 72,
			I = 73,
			J = 74,
			K = 75,
			L = 76,
			M = 77,
			N = 78,
			O = 79,
			P = 80,
			Q = 81,
			R = 82,
			S = 83,
			T = 84,
			U = 85,
			V = 86,
			W = 87,
			X = 88,
			Y = 89,
			Z = 90,

			LeftBracket = 91,  /* [ */
			Backslash = 92,  /* \ */
			RightBracket = 93,  /* ] */
			GraveAccent = 96,  /* ` */

			World1 = 161, /* non-US #1 */
			World2 = 162, /* non-US #2 */

			/* Function keys */
			Escape = 256,
			Enter = 257,
			Tab = 258,
			Backspace = 259,
			Insert = 260,
			Delete = 261,
			Right = 262,
			Left = 263,
			Down = 264,
			Up = 265,
			PageUp = 266,
			PageDown = 267,
			Home = 268,
			End = 269,
			CapsLock = 280,
			ScrollLock = 281,
			NumLock = 282,
			PrintScreen = 283,
			Pause = 284,
			F1 = 290,
			F2 = 291,
			F3 = 292,
			F4 = 293,
			F5 = 294,
			F6 = 295,
			F7 = 296,
			F8 = 297,
			F9 = 298,
			F10 = 299,
			F11 = 300,
			F12 = 301,
			F13 = 302,
			F14 = 303,
			F15 = 304,
			F16 = 305,
			F17 = 306,
			F18 = 307,
			F19 = 308,
			F20 = 309,
			F21 = 310,
			F22 = 311,
			F23 = 312,
			F24 = 313,
			F25 = 314,

			/* Keypad */
			KP0 = 320,
			KP1 = 321,
			KP2 = 322,
			KP3 = 323,
			KP4 = 324,
			KP5 = 325,
			KP6 = 326,
			KP7 = 327,
			KP8 = 328,
			KP9 = 329,
			KPDecimal = 330,
			KPDivide = 331,
			KPMultiply = 332,
			KPSubtract = 333,
			KPAdd = 334,
			KPEnter = 335,
			KPEqual = 336,

			LeftShift = 340,
			LeftControl = 341,
			LeftAlt = 342,
			LeftSuper = 343,
			RightShift = 344,
			RightControl = 345,
			RightAlt = 346,
			RightSuper = 347,
			Menu = 348
		};
		static std::map<KeyCode, std::string> KeyToString =
		{
			{Space, "Space"},
			{Apostrophe, "Apostrophe"},
			{Comma, "Comma"},
			{Minus, "Minus"},
			{Period, "Period"},
			{Slash, "Slash"},
			{D0, "D0"},
			{D1, "D1"},
			{D2, "D2"},
			{D3, "D3"},
			{D4, "D4"},
			{D5, "D5"},
			{D6, "D6"},
			{D7, "D7"},
			{D8, "D8"},
			{D9, "D9"},
			{Semicolon, "Semicolon"},
			{Equal, "Equal"},
			{A, "A"},
			{B, "B"},
			{C, "C"},
			{D, "D"},
			{E, "E"},
			{F, "F"},
			{G, "G"},
			{H, "H"},
			{I, "I"},
			{J, "J"},
			{K, "K"},
			{L, "L"},
			{M, "M"},
			{N, "N"},
			{O, "O"},
			{P, "P"},
			{Q, "Q"},
			{R, "R"},
			{S, "S"},
			{T, "T"},
			{U, "U"},
			{V, "V"},
			{W, "W"},
			{X, "X"},
			{Y, "Y"},
			{Z, "Z"},
			{LeftBracket, "LeftBracket"},
			{Backslash, "Backslash"},
			{RightBracket, "RightBracket"},
			{GraveAccent, "GraveAccent"},
			{World1, "World1"},
			{World2, "World2"},
			{Escape, "Escape"},
			{Enter, "Enter"},
			{Tab, "Tab"},
			{Backspace, "Backspace"},
			{Insert, "Insert"},
			{Delete, "Delete"},
			{Right, "Right"},
			{Left, "Left"},
			{Down, "Down"},
			{Up, "Up"},
			{PageUp, "PageUp"},
			{PageDown, "PageDown"},
			{Home, "Home"},
			{End, "End"},
			{CapsLock, "CapsLock"},
			{ScrollLock, "ScrollLock"},
			{NumLock, "NumLock"},
			{PrintScreen, "PrintScreen"},
			{Pause, "Pause"},
			{F1, "F1"},
			{F2, "F2"},
			{F3, "F3"},
			{F4, "F4"},
			{F5, "F5"},
			{F6, "F6"},
			{F7, "F7"},
			{F8, "F8"},
			{F9, "F9"},
			{F10, "F10"},
			{F11, "F11"},
			{F12, "F12"},
			{F13, "F13"},
			{F14, "F14"},
			{F15, "F15"},
			 {F16, "F16"},
			{F17, "F17"},
			{F18, "F18"},
			{F19, "F19"},
			{F20, "F20"},
			{F21, "F21"},
			{F22, "F22"},
			{F23, "F23"},
			{F24, "F24"},
			{F25, "F25"},
	// Keypad
			{KP0, "KP0"},
			{KP1, "KP1"},
			{KP2, "KP2"}, 
			{KP3, "KP3"},
			{KP4, "KP4"},
			{KP5, "KP5"},
			{KP6, "KP6"},
			{KP7, "KP7"},
			{KP8, "KP8"},
			{KP9, "KP9"},
			{KPDecimal, "KPDecimal"},
			{KPDivide, "KPDivide"},
			{KPMultiply, "KPMultiply"},
			{KPSubtract, "KPSubtract"},
			{KPAdd, "KPAdd"},
			{KPEnter, "KPEnter"},
			{KPEqual, "KPEqual"},
	// Other keys
			{LeftShift, "LeftShift"},
			{LeftControl, "LeftControl"},
			{LeftAlt, "LeftAlt"},
			{LeftSuper, "LeftSuper"},
			{RightShift, "RightShift"}, 
			{RightControl, "RightControl"},
			{RightAlt, "RightAlt"},
			{RightSuper, "RightSuper"},
			{Menu, "Menu"}
		};
		
		KeyCode FromValue(int value)
		{
			for (KeyCode code = 0; code <= 348; code++)
			{
				if ((int)code == value)
				{
					std::cout << code << std::endl;
					
				}
					
				
			}

			return 0;
		}

		KeyCode KeyString(int value)
		{
			for (auto it  = KeyToString.begin(); it != KeyToString.end(); it++)
			{
				if (it->first == value)
				{
					std::cout << it->second + " Pressed" << std::endl;
					return 1;
				}


			}

			return 0;
		}

	}
}