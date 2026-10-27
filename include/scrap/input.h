/*
 * Input: action sets, key definitions and the per player input state (original source: Input/). Up to four
 * local players (split screen), each with an input slot of 0xb4 bytes and a pad of 0x20 bytes.
 */
#ifndef SCRAP_INPUT_H
#define SCRAP_INPUT_H

#include "scrap/cstring.h"
#include "scrap/game.h"

#define MAX_LOCAL_PLAYERS 4

/* an input action of an action set; value[player] is its current state (SScorer.GetMenuAccept) */
typedef struct cInputAction {
	byte pad_00[0x18];
	float value[MAX_LOCAL_PLAYERS];	/* 0x18 */
	byte pad_28[0x7c - 0x28];
} cInputAction;
SD_SIZE(cInputAction, 0x7c);

typedef struct cActionSet {
	char *name;			/* 0x0 */
	TABLE(cInputAction) actions;	/* 0x4 */
} cActionSet;
SD_SIZE(cActionSet, 0xc);

typedef struct cPlayerInput {
	byte pad_00[0x08];
	char text[0x8c - 0x08];		/* 0x08 SInput.GetString */
	float cursorX;			/* 0x8c SInput.GetCursorChar */
	float cursorY;			/* 0x90 */
	byte pad_94[0xb4 - 0x94];
} cPlayerInput;
SD_SIZE(cPlayerInput, 0xb4);
SD_OFFSET(cPlayerInput, cursorX, 0x8c);

typedef struct cPad {
	byte data[0x20];
} cPad;

typedef struct cVirtualKeyboard {
	char text[0x100];		/* 0x000 */
	int a;				/* 0x100 SInput.GetVirtualKeyboard */
	int b;				/* 0x104 */
} cVirtualKeyboard;
SD_OFFSET(cVirtualKeyboard, a, 0x100);

#define g_ActionSets GAME_DATA(TABLE(cActionSet), 0x44c128)
#define g_PlayerInput ((cPlayerInput *)Game_Ptr(0x44c130))	/* [MAX_LOCAL_PLAYERS] */
#define g_VirtualKeyboard GAME_DATA(cVirtualKeyboard, 0x44c400)
#define g_Pads ((cPad *)Game_Ptr(0x44c510))		/* [MAX_LOCAL_PLAYERS] */

/* rva 0x133a30 is cString_EqStr on the name */
static inline byte ActionSet_IsNamed(cActionSet *set, const char *name) {
	return cString_EqStr(&set->name, name);
}
void Input_SelectActionSet(int index);
uint32_t Input_GetActionSet(void);
int Input_AssignEntry(const char *a, const char *b, int c, const char *d, const char *e);
int Input_GetEntry(const char *a, const char *b, const char *c, int *index, char **name);
int Input_ListenToDefine(int player, const char *a, const char *b, const char *c);
void Input_AbortListenToDefine(void);
char *Input_GetDefinedEntry(int player, const char *a, const char *b, const char *c, int flag);
int Input_GetDefinedList(int player, const char *a, const char *b, char *buf, int size, int flag);
int Input_ClearDefinedList(int player, const char *a, const char *b, int flag);
void Input_ResetToDefault(void);
int Input_Bind(int player, const char *a, const char *b, const char *c);
void __thiscall cPlayerInput_SetString(cPlayerInput *in, const char *text);
void __thiscall cPlayerInput_SetInputFunc(cPlayerInput *in, const char *func);
char __thiscall cPlayerInput_GetCursorChar(cPlayerInput *in);
byte __thiscall cVirtualKeyboard_Set(cVirtualKeyboard *vk, const char *text);
void __thiscall cPad_Rumble(cPad *pad, float a, float b, float c);

/* rva 0x13a1b0: whether the action is pressed for the player */
byte __thiscall cInputAction_Pressed(void *action, int player);

#endif
