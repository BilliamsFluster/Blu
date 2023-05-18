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



/* The unknown key */
#define BLU_KEY_UNKNOWN            -1

/* Printable keys */
#define BLU_KEY_SPACE              32
#define BLU_KEY_APOSTROPHE         39  /* ' */
#define BLU_KEY_COMMA              44  /* , */
#define BLU_KEY_MINUS              45  /* - */
#define BLU_KEY_PERIOD             46  /* . */
#define BLU_KEY_SLASH              47  /* / */
#define BLU_KEY_0                  48
#define BLU_KEY_1                  49
#define BLU_KEY_2                  50
#define BLU_KEY_3                  51
#define BLU_KEY_4                  52
#define BLU_KEY_5                  53
#define BLU_KEY_6                  54
#define BLU_KEY_7                  55
#define BLU_KEY_8                  56
#define BLU_KEY_9                  57
#define BLU_KEY_SEMICOLON          59  /* ; */
#define BLU_KEY_EQUAL              61  /* = */
#define BLU_KEY_A                  65
#define BLU_KEY_B                  66
#define BLU_KEY_C                  67
#define BLU_KEY_D                  68
#define BLU_KEY_E                  69
#define BLU_KEY_F                  70
#define BLU_KEY_G                  71
#define BLU_KEY_H                  72
#define BLU_KEY_I                  73
#define BLU_KEY_J                  74
#define BLU_KEY_K                  75
#define BLU_KEY_L                  76
#define BLU_KEY_M                  77
#define BLU_KEY_N                  78
#define BLU_KEY_O                  79
#define BLU_KEY_P                  80
#define BLU_KEY_Q                  81
#define BLU_KEY_R                  82
#define BLU_KEY_S                  83
#define BLU_KEY_T                  84
#define BLU_KEY_U                  85
#define BLU_KEY_V                  86
#define BLU_KEY_W                  87
#define BLU_KEY_X                  88
#define BLU_KEY_Y                  89
#define BLU_KEY_Z                  90
#define BLU_KEY_LEFT_BRACKET       91  /* [ */
#define BLU_KEY_BACKSLASH          92  /* \ */
#define BLU_KEY_RIGHT_BRACKET      93  /* ] */
#define BLU_KEY_GRAVE_ACCENT       96  /* ` */
#define BLU_KEY_WORLD_1            161 /* non-US #1 */
#define BLU_KEY_WORLD_2            162 /* non-US #2 */
		
/* Function keys */
#define BLU_KEY_ESCAPE             256
#define BLU_KEY_ENTER              257
#define BLU_KEY_TAB                258
#define BLU_KEY_BACKSPACE          259
#define BLU_KEY_INSERT             260
#define BLU_KEY_DELETE             261
#define BLU_KEY_RIGHT              262
#define BLU_KEY_LEFT               263
#define BLU_KEY_DOWN               264
#define BLU_KEY_UP                 265
#define BLU_KEY_PAGE_UP            266
#define BLU_KEY_PAGE_DOWN          267
#define BLU_KEY_HOME               268
#define BLU_KEY_END                269
#define BLU_KEY_CAPS_LOCK          280
#define BLU_KEY_SCROLL_LOCK        281
#define BLU_KEY_NUM_LOCK           282
#define BLU_KEY_PRINT_SCREEN       283
#define BLU_KEY_PAUSE              284
#define BLU_KEY_F1                 290
#define BLU_KEY_F2                 291
#define BLU_KEY_F3                 292
#define BLU_KEY_F4                 293
#define BLU_KEY_F5                 294
#define BLU_KEY_F6                 295
#define BLU_KEY_F7                 296
#define BLU_KEY_F8                 297
#define BLU_KEY_F9                 298
#define BLU_KEY_F10                299
#define BLU_KEY_F11                300
#define BLU_KEY_F12                301
#define BLU_KEY_F13                302
#define BLU_KEY_F14                303
#define BLU_KEY_F15                304
#define BLU_KEY_F16                305
#define BLU_KEY_F17                306
#define BLU_KEY_F18                307
#define BLU_KEY_F19                308
#define BLU_KEY_F20                309
#define BLU_KEY_F21                310
#define BLU_KEY_F22                311
#define BLU_KEY_F23                312
#define BLU_KEY_F24                313
#define BLU_KEY_F25                314
#define BLU_KEY_KP_0               320
#define BLU_KEY_KP_1               321
#define BLU_KEY_KP_2               322
#define BLU_KEY_KP_3               323
#define BLU_KEY_KP_4               324
#define BLU_KEY_KP_5               325
#define BLU_KEY_KP_6               326
#define BLU_KEY_KP_7               327
#define BLU_KEY_KP_8               328
#define BLU_KEY_KP_9               329
#define BLU_KEY_KP_DECIMAL         330
#define BLU_KEY_KP_DIVIDE          331
#define BLU_KEY_KP_MULTIPLY        332
#define BLU_KEY_KP_SUBTRACT        333
#define BLU_KEY_KP_ADD             334
#define BLU_KEY_KP_ENTER           335
#define BLU_KEY_KP_EQUAL           336
#define BLU_KEY_LEFT_SHIFT         340
#define BLU_KEY_LEFT_CONTROL       341
#define BLU_KEY_LEFT_ALT           342
#define BLU_KEY_LEFT_SUPER         343
#define BLU_KEY_RIGHT_SHIFT        344
#define BLU_KEY_RIGHT_CONTROL      345
#define BLU_KEY_RIGHT_ALT          346
#define BLU_KEY_RIGHT_SUPER        347
#define BLU_KEY_MENU               348
		
#define BLU_KEY_LAST               BLU_KEY_MENU