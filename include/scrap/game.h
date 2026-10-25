/*
 * Access to Scrap.exe: build check, addresses and global data.
 *
 * Addresses are RVAs (Ghidra address - 0x400000). The exe has ASLR (DYNAMIC_BASE), so nothing may use absolute
 * addresses: Game_Ptr adds the load address. Functions of the exe are called through the thunks of
 * src/imports.inc, global data through the GAME_DATA macros below.
 */
#ifndef SCRAP_GAME_H
#define SCRAP_GAME_H

#include "scrap/shared.h"

#define GAME_TIMESTAMP 0x61b304cc	/* PE link time of Scrap.exe 1.6.1 (2021-12-10 07:42:04 UTC) */
#define GAME_SIZEOFIMAGE 0x55e000

extern uintptr_t game_base;	/* load address of Scrap.exe, 0 until Game_Init found the right build */

static inline void *Game_Ptr(uint32_t rva) {
	return (void *)(game_base + rva);
}

/* lvalue of a global of the exe */
#define GAME_DATA(type, rva) (*(type *)Game_Ptr(rva))

int Game_Init(void);

/* the world: created by cWorld's constructor (rva 0x206db0), owns the game state of the running level */
typedef struct cWorld {
	void *vftable;			/* 0x0000 cWorld::vftable */
	byte pad_0004[0x1bb8 - 0x0004];
	int numWeapons;			/* 0x1bb8 SWeap.GetNWeap */
	byte pad_1bbc[0x2090 - 0x1bbc];
	int money;			/* 0x2090 Scrap.GetMoney */
} cWorld;
SD_OFFSET(cWorld, numWeapons, 0x1bb8);
SD_OFFSET(cWorld, money, 0x2090);

/* the application: allocated (0x90 bytes) and run by the WinMain message loop at rva 0x20d8c0 */
typedef struct CWinApp {
	void *vftable;			/* 0x00 CWinApp::vftable */
	byte pad_04[0x34 - 0x04];
	char *levelPath;		/* 0x34 Scrap.GetLevelPath */
	byte pad_38[0x90 - 0x38];
} CWinApp;
SD_OFFSET(CWinApp, levelPath, 0x34);
SD_SIZE(CWinApp, 0x90);

#define g_pWorld GAME_DATA(cWorld *, 0x467450)
#define g_pApp GAME_DATA(CWinApp *, 0x467ba0)

#endif
