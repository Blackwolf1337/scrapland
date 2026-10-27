/*
 * Access to Scrap.exe: build check, addresses and global data.
 *
 * Addresses are RVAs (Ghidra address - 0x400000). The exe has ASLR (DYNAMIC_BASE), so nothing may use absolute
 * addresses: Game_Ptr adds the load address. Functions of the exe are called through the thunks of
 * src/imports.inc, global data through the GAME_DATA macros below.
 */
#ifndef SCRAP_GAME_H
#define SCRAP_GAME_H

#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/nto1links.h"
#include "scrap/timer.h"

#define GAME_TIMESTAMP 0x61b304cc	/* PE link time of Scrap.exe 1.6.1 (2021-12-10 07:42:04 UTC) */
#define GAME_SIZEOFIMAGE 0x55e000

extern uintptr_t game_base;	/* load address of Scrap.exe, 0 until Game_Init found the right build */

static inline void *Game_Ptr(uint32_t rva) {
	return (void *)(game_base + rva);
}

/* lvalue of a global of the exe */
#define GAME_DATA(type, rva) (*(type *)Game_Ptr(rva))

int Game_Init(void);

/* weapon and ammo descriptors of the world tables (defined in include/scrap/world.h) */
typedef struct cWeapInfo cWeapInfo;
typedef struct cAmmoInfo cAmmoInfo;

/* Global/Table.h: fixed size array made once with Init (new T[count]) */
#define TABLE(type) \
	struct { \
		int count; \
		type *items; \
	}

/* the inlined index check of Table<T>::operator[]: the original stops on int3, then goes on with the bad slot */
#define TABLE_AT(tab, i) \
	({ \
		typeof((tab).items) items_ = (tab).items; \
		uint32_t i_ = (uint32_t)(i); \
		if (!items_ || i_ >= (uint32_t)(tab).count) { \
			__asm__ volatile("int3"); \
		} \
		items_ + i_; \
	})

/* an item description for the item list (Scrap.AddItem): a string object and a value */
typedef struct cItemDesc {
	char *name;			/* 0x0 string object (cString_ctor/dtor, cString_Set) */
	int value;			/* 0x4 */
} cItemDesc;

/* the item list embedded in cWorld at 0x20e0, only seen through its vtable */
typedef struct cItemList cItemList;
typedef struct cItemListVtbl {
	void *vf00;
	void(__thiscall *AddLife)(cItemList *self, cItemDesc *item);			/* 0x04 */
	/* the walk over the life items: iter is the cursor (one pointer), NULL after the last */
	cItemDesc *(__thiscall *FirstLife)(cItemList *self, void **iter);		/* 0x08 */
	cItemDesc *(__thiscall *NextLife)(cItemList *self, void **iter);		/* 0x0c */
	void(__thiscall *AddAmmo)(cItemList *self, int kind, cItemDesc *item);		/* 0x10 */
	cItemDesc *(__thiscall *FirstAmmo)(cItemList *self, int kind, void **iter);	/* 0x14 */
	cItemDesc *(__thiscall *NextAmmo)(cItemList *self, int kind, void **iter);	/* 0x18 */
} cItemListVtbl;
SD_OFFSET(cItemListVtbl, AddAmmo, 0x10);

struct cItemList {
	const cItemListVtbl *vftable;
};

/* the world: created by cWorld's constructor (rva 0x206db0), owns the game state of the running level */
typedef struct cWorld {
	void *vftable;			/* 0x0000 cWorld::vftable */
	HashTable entities;		/* 0x0004 every entity by name (cEntity *) */
	Nto1Node entityList;		/* 0x0010 head of the entities (their node at +8), target the world */
	Nto1Node deadList;		/* 0x0020 entities to delete after the frame */
	int nameCount;			/* 0x0030 suffix of the next generated name */
	byte scheduler[0x0050 - 0x0034]; /* 0x0034 scheduled Python functions (Scrap.AddScheduledFunc) */
	int curCam;			/* 0x0050 index into camPos */
	char camName[4][0x80];		/* 0x0054 camera entity of each view, "" for none */
	vec3_t camPos[4];		/* 0x0254 camera positions (Scrap.GetNearestParked) */
	byte testing;			/* 0x0284 set by the entity queries */
	byte pad_0285[3];
	struct cEntity *owners[4];	/* 0x0288 the entity of each player slot, found every frame */
	HashTable debris;		/* 0x0298 debris systems by name */
	struct cDebrisSys *debrisList;	/* 0x02a4 the same, newest first */
	struct cEntity *ignore[4];	/* 0x02a8 skipped by the next line and sphere test */
	HashTable lists;		/* 0x02b8 entity lists by name */
	byte checkInput;		/* 0x02c4 the input events are called every frame */
	byte pad_02c5[3];
	cTimer timer;			/* 0x02c8 world time (Scrap.GetTime, GetTimeSpeed) */
	byte floor[0x1bb4 - 0x02f0];	/* 0x02f0 Scrap.GetFloor */
	byte elements[0x1bb8 - 0x1bb4]; /* 0x1bb4 map elements (Scrap.CreateElements, CallElements) */
	TABLE(cWeapInfo) weapons;	/* 0x1bb8 SWeap.InitWeap, GetNWeap */
	int firstWeapon;		/* 0x1bc0 SWeap.GetFirstiWeap */
	TABLE(cAmmoInfo) ammo;		/* 0x1bc4 SWeap.InitAmmo, GetNAmmo */
	byte grid[0x1c68 - 0x1bcc];	/* 0x1bcc Scrap.SetGrid */
	byte alarm[0x1c6c - 0x1c68];	/* 0x1c68 alarm object (Scrap.GetAlarm, GetLockAlarm) */
	float alarmLevel;		/* 0x1c6c */
	byte alarmOn;			/* 0x1c70 */
	char alarmChars[0x1e71 - 0x1c71]; /* 0x1c71 Scrap.GetAlarmChars */
	char alarmChars2[0x2078 - 0x1e71]; /* 0x1e71 */
	byte strMoney[0x2090 - 0x2078];	/* 0x2078 the digits of the money (cStrMoney, logic.h), its int is money */
	int money;			/* 0x2090 Scrap.GetMoney */
	/* embedded objects of unknown size, only their addresses are used so far */
	byte taunts[0x20ac - 0x2094];	/* 0x2094 SLogic.UpdateTauntEndTime */
	byte parking[0x20e0 - 0x20ac];	/* 0x20ac parking zones (Scrap.AddParkingZone, GetNearestParked) */
	cItemList items;		/* 0x20e0 Scrap.AddItem */
	byte pad_20e4[0x2144 - 0x20e4];
	byte shipRegen[0x21c8 - 0x2144]; /* 0x2144 SLogic.SetShipToRegenerate */
	byte aiCtrl[4];			/* 0x21c8 sentinels and gears (SLogic) */
} cWorld;
SD_OFFSET(cWorld, entities, 0x0004);
SD_OFFSET(cWorld, scheduler, 0x0034);
SD_OFFSET(cWorld, entityList, 0x0010);
SD_OFFSET(cWorld, nameCount, 0x0030);
SD_OFFSET(cWorld, curCam, 0x0050);
SD_OFFSET(cWorld, camName, 0x0054);
SD_OFFSET(cWorld, camPos, 0x0254);
SD_OFFSET(cWorld, testing, 0x0284);
SD_OFFSET(cWorld, debris, 0x0298);
SD_OFFSET(cWorld, ignore, 0x02a8);
SD_OFFSET(cWorld, lists, 0x02b8);
SD_OFFSET(cWorld, checkInput, 0x02c4);
SD_OFFSET(cWorld, strMoney, 0x2078);
SD_OFFSET(cWorld, timer, 0x02c8);
SD_OFFSET(cWorld, floor, 0x02f0);
SD_OFFSET(cWorld, elements, 0x1bb4);
SD_OFFSET(cWorld, alarm, 0x1c68);
SD_OFFSET(cWorld, alarmChars, 0x1c71);
SD_OFFSET(cWorld, alarmChars2, 0x1e71);
SD_OFFSET(cWorld, weapons, 0x1bb8);
SD_OFFSET(cWorld, firstWeapon, 0x1bc0);
SD_OFFSET(cWorld, ammo, 0x1bc4);
SD_OFFSET(cWorld, money, 0x2090);
SD_OFFSET(cWorld, taunts, 0x2094);
SD_OFFSET(cWorld, parking, 0x20ac);
SD_OFFSET(cWorld, items, 0x20e0);
SD_OFFSET(cWorld, grid, 0x1bcc);
SD_OFFSET(cWorld, shipRegen, 0x2144);
SD_OFFSET(cWorld, aiCtrl, 0x21c8);

/* World/: thiscall methods; Table<T>::Init per element type */
void __thiscall WeapTable_Init(void *table, int num);
void __thiscall AmmoTable_Init(void *table, int num);
void __thiscall cWorld_FillWeapPriority(cWorld *world);
int __thiscall cWeapInfo_GetF(cWeapInfo *weap, const char *var, float *value);
int __thiscall cWeapInfo_GetS(cWeapInfo *weap, const char *var, char **value);
int __thiscall cWeapInfo_SetF(cWeapInfo *weap, const char *var, float value);
int __thiscall cWeapInfo_SetS(cWeapInfo *weap, const char *var, const char *value);
int __thiscall cAmmoInfo_GetF(cAmmoInfo *ammo, const char *var, float *value);
int __thiscall cAmmoInfo_GetS(cAmmoInfo *ammo, const char *var, char **value);
int __thiscall cAmmoInfo_SetF(cAmmoInfo *ammo, const char *var, float value);
int __thiscall cAmmoInfo_SetS(cAmmoInfo *ammo, const char *var, const char *value);

/*
 * the application (App/, include/scrap/app.h): CApp (0x48 bytes, vftable rva 0x3cf190) is the game part, CWinApp
 * (0x90 bytes, vftable rva 0x3cf7d4) adds the Win32 window. Allocated and run by WinMain (rva 0x20d8c0). The slots
 * are the vf numbers of both classes.
 */
typedef struct CWinApp CWinApp;
typedef struct CWinAppVtbl {
	void(__thiscall *DoneGame)(CWinApp *self);					/* 0x00 vf00 */
	void(__thiscall *InitGame)(CWinApp *self, const char *level);			/* 0x04 vf01 */
	void(__thiscall *InitDebug)(CWinApp *self);					/* 0x08 vf02 */
	CWinApp *(__thiscall *Delete)(CWinApp *self, byte flags);			/* 0x0c vf03 */
	byte(__thiscall *Init)(CWinApp *self);						/* 0x10 vf04 */
	byte(__thiscall *Frame)(CWinApp *self);						/* 0x14 vf05 */
	void(__thiscall *Shutdown)(CWinApp *self);					/* 0x18 vf06 */
	void(__thiscall *LoadLevelNow)(CWinApp *self, const char *level);		/* 0x1c vf07 */
	void(__thiscall *InitDedicated)(CWinApp *self);					/* 0x20 vf08 */
	void(__thiscall *Exit)(CWinApp *self);						/* 0x24 vf09 */
	void(__thiscall *Update)(CWinApp *self, float dt);				/* 0x28 vf10 */
	void(__thiscall *Render)(CWinApp *self);					/* 0x2c vf11 */
	byte(__thiscall *ParseParam)(CWinApp *self, const char *param, const char *ext);	/* 0x30 vf12 */
	void(__thiscall *ResetParams)(CWinApp *self);					/* 0x34 vf13 */
	double(__thiscall *GetTime)(CWinApp *self);					/* 0x38 vf14 */
	double(__thiscall *GetGameTime)(CWinApp *self);					/* 0x3c vf15 */
	void(__thiscall *ShowConsole)(CWinApp *self, byte show);			/* 0x40 vf16 */
	void(__thiscall *ConsoleAdd)(CWinApp *self, const char *text, uint32_t color);	/* 0x44 vf17 */
	const char *(__thiscall *DebugInput)(CWinApp *self, const char *text);		/* 0x48 vf18 */
	byte(__thiscall *RunApp)(CWinApp *self, const char *exe, const char *cmdLine, int arg);	/* 0x4c vf19 */
	byte(__thiscall *IsDedicatedWin)(CWinApp *self);				/* 0x50 vf20 */
} CWinAppVtbl;
SD_OFFSET(CWinAppVtbl, Exit, 0x24);
SD_OFFSET(CWinAppVtbl, DebugInput, 0x48);
SD_OFFSET(CWinAppVtbl, IsDedicatedWin, 0x50);

/* WNDCLASSA */
typedef struct exe_wndclass {
	uint32_t style;			/* 0x00 */
	void *wndProc;			/* 0x04 */
	int clsExtra;			/* 0x08 */
	int wndExtra;			/* 0x0c */
	void *hInstance;		/* 0x10 */
	void *hIcon;			/* 0x14 */
	void *hCursor;			/* 0x18 */
	void *hbrBackground;		/* 0x1c */
	const char *menuName;		/* 0x20 */
	const char *className;		/* 0x24 */
} exe_wndclass;
SD_SIZE(exe_wndclass, 0x28);

/* the GOG Galaxy listeners of CWinApp: a vector of objects deleted through their slot 0 */
typedef struct cGogListeners {
	void **begin;			/* 0x0 */
	void **end;			/* 0x4 */
	void **cap;			/* 0x8 */
} cGogListeners;

#define APPF_CLIENT 1
#define APPF_SERVER 2
#define APPF_DEDICATED 4

struct CWinApp {
	const CWinAppVtbl *vftable;	/* 0x00 */
	byte pad_04[4];
	char *cmdLine;			/* 0x08 copy (Mem_Alloc) */
	byte pad_0c[4];
	double time;			/* 0x10 GetTime of this frame */
	double lastFrame;		/* 0x18 */
	double gameTime;		/* 0x20 the sum of the frame times */
	int build;			/* 0x28 -build[:mode]: Pack_Build of Data */
	byte active;			/* 0x2c WM_ACTIVATEAPP */
	byte dedicated;			/* 0x2d -dedicated */
	byte pad_2e[2];
	uint32_t flags;			/* 0x30 APPF_ (SNet.IsDedicated) */
	char *levelPath;		/* 0x34 Scrap.GetLevelPath */
	char *newLevelPath;		/* 0x38 Scrap.GetNewLevelPath */
	byte loadPending;		/* 0x3c newLevelPath is loaded at the next frame */
	byte pad_3d[3];
	void (*onLevelLoaded)(void);	/* 0x40 called once after LoadLevelNow */
	byte pad_44[4];
	/* CWinApp */
	void *hInstance;		/* 0x48 */
	void *hPrevInstance;		/* 0x4c */
	int nCmdShow;			/* 0x50 */
	exe_wndclass wc;		/* 0x54 "ScrapClass" */
	void *hwnd;			/* 0x7c */
	byte console;			/* 0x80 -console: the console window */
	byte dedicatedWin;		/* 0x81 -dedicated: small window */
	byte pad_82[2];
	cGogListeners gog;		/* 0x84 */
};
SD_OFFSET(CWinApp, cmdLine, 0x08);
SD_OFFSET(CWinApp, gameTime, 0x20);
SD_OFFSET(CWinApp, flags, 0x30);
SD_OFFSET(CWinApp, levelPath, 0x34);
SD_OFFSET(CWinApp, onLevelLoaded, 0x40);
SD_OFFSET(CWinApp, wc, 0x54);
SD_OFFSET(CWinApp, hwnd, 0x7c);
SD_OFFSET(CWinApp, gog, 0x84);
SD_SIZE(CWinApp, 0x90);

#define g_pWorld GAME_DATA(cWorld *, 0x467450)
#define g_pApp GAME_DATA(CWinApp *, 0x467ba0)

#endif
