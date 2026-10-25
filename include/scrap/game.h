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

/* weapon and ammo descriptors of the world tables; methods get/set fields by name ("MinAmmo", "OnFire", ...) */
typedef struct cWeapInfo {
	byte data[0xb0];
} cWeapInfo;
SD_SIZE(cWeapInfo, 0xb0);

typedef struct cAmmoInfo {
	byte data[0x5c];
} cAmmoInfo;
SD_SIZE(cAmmoInfo, 0x5c);

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
	void *vf02_03[2];
	void(__thiscall *AddAmmo)(cItemList *self, int kind, cItemDesc *item);		/* 0x10 */
} cItemListVtbl;
SD_OFFSET(cItemListVtbl, AddAmmo, 0x10);

struct cItemList {
	const cItemListVtbl *vftable;
};

/* the world: created by cWorld's constructor (rva 0x206db0), owns the game state of the running level */
typedef struct cWorld {
	void *vftable;			/* 0x0000 cWorld::vftable */
	HashTable entities;		/* 0x0004 every entity by name (cEntity *) */
	byte pad_0010[0x0034 - 0x0010];
	byte scheduler[0x0050 - 0x0034]; /* 0x0034 scheduled Python functions (Scrap.AddScheduledFunc) */
	int curCam;			/* 0x0050 index into camPos */
	byte pad_0054[0x0254 - 0x0054];
	vec3_t camPos[9];		/* 0x0254 camera positions (Scrap.GetNearestParked) */
	byte pad_02c0[0x02c8 - 0x02c0];
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
	char alarmChars2[0x2090 - 0x1e71]; /* 0x1e71 */
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
SD_OFFSET(cWorld, curCam, 0x0050);
SD_OFFSET(cWorld, camPos, 0x0254);
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

/* the application: allocated (0x90 bytes) and run by the WinMain message loop at rva 0x20d8c0 */
typedef struct CWinApp CWinApp;
typedef struct CWinAppVtbl {
	void *vf00_08[9];
	void(__thiscall *Exit)(CWinApp *self);				/* 0x24 */
	void *vf0a_11[8];
	const char *(__thiscall *DebugInput)(CWinApp *self, const char *text);	/* 0x48 */
	void(__thiscall *RunApp)(CWinApp *self, const char *exe, const char *cmdLine, int arg);	/* 0x4c */
} CWinAppVtbl;
SD_OFFSET(CWinAppVtbl, Exit, 0x24);
SD_OFFSET(CWinAppVtbl, DebugInput, 0x48);

struct CWinApp {
	const CWinAppVtbl *vftable;	/* 0x00 CWinApp::vftable */
	byte pad_04[0x30 - 0x04];
	uint32_t flags;			/* 0x30 bit 2: dedicated server (SNet.IsDedicated) */
	char *levelPath;		/* 0x34 Scrap.GetLevelPath */
	char *newLevelPath;		/* 0x38 Scrap.GetNewLevelPath */
	byte pad_3c[0x90 - 0x3c];
};
SD_OFFSET(CWinApp, levelPath, 0x34);
SD_SIZE(CWinApp, 0x90);

#define g_pWorld GAME_DATA(cWorld *, 0x467450)
#define g_pApp GAME_DATA(CWinApp *, 0x467ba0)

#endif
