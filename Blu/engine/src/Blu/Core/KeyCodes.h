#pragma once
#include "Blupch.h"






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