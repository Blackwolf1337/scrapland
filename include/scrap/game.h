/*
 * The game's globals, functions, strings and imports.
 */
#ifndef SCRAP_GAME_H
#define SCRAP_GAME_H

#include "scrap/alarm.h"
#include "scrap/global.h"
#include "scrap/mapfloors.h"
#include "scrap/mathlib.h"
#include "scrap/nto1links.h"
#include "scrap/timer.h"
#include "scrap/worldlogic.h"
#include "scrap/worldparts.h"

/*
 * The globals and tables of src/game/Global/Globals*.c, declared inside the macro, so no header lists them. Each
 * define that uses them is named like the object; the names are pasted, never expanded.
 *	GAME_GLOBAL(type, name)			the object name of type (an lvalue)
 *	GAME_ARRAY(type, name)			the first element of the array name of type (type *)
 *	GAME_GLOBAL_IN(type, owner, off)	a view of type at off in the object owner: a field of a struct, an
 *						element of a table, another type at the same place (an lvalue)
 *	GAME_ARRAY_IN(type, owner, off)		the same as a pointer (type *)
 *	GAME_PTR(owner, off)			the address off in the object owner (void *)
 */
#define SD_REF_(id, label) \
	({ \
		extern char id[] __asm__(label); \
		(void *)id; \
	})
/* a cGVar or cGVarFunction object by its name, without a declaration */
#define GAME_GVAR(name) ((cGVar *)SD_REF_(sd_##name, SD_CPFX #name))
#define GAME_GVARFUNC(name) ((cGVarFunction *)SD_REF_(sd_##name, SD_CPFX #name))
#define GAME_GLOBAL(type, name) \
	(*({ \
		extern typeof(type) sd_obj_##name __asm__(SD_CPFX #name); \
		&sd_obj_##name; \
	}))
#define GAME_ARRAY(type, name) ((typeof(type) *)SD_REF_(sd_##name, SD_CPFX #name))
#define GAME_GLOBAL_IN(type, owner, off) (*(typeof(type) *)((char *)SD_REF_(sd_##owner, SD_CPFX #owner) + (off)))
#define GAME_ARRAY_IN(type, owner, off) ((typeof(type) *)((char *)SD_REF_(sd_##owner, SD_CPFX #owner) + (off)))
#define GAME_PTR(owner, off) ((void *)((char *)SD_REF_(sd_##owner, SD_CPFX #owner) + (off)))


/* the one "" of the game: char * fields hold it for "no string", the code compares with it by address (never
 * written) */
extern char str_Empty[];


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

/* the world: created by cWorld's constructor, owns the game state of the running level */
typedef struct cWorldVtbl {
	struct cWorld *(*Delete)(struct cWorld *self, byte flags);	/* 0x00 */
} cWorldVtbl;

typedef struct cWorld {
	const cWorldVtbl *vftable;			/* 0x0000 cWorld::vftable */
	HashTable entities;		/* 0x0004 every entity by name (cEntity *) */
	Nto1Node entityList;		/* 0x0010 head of the entities (their node at +8), target the world */
	Nto1Node deadList;		/* 0x0020 entities to delete after the frame */
	int nameCount;			/* 0x0030 suffix of the next generated name */
	cScheduledFuncStack scheduler;	/* 0x0034 scheduled Python functions (Scrap.AddScheduledFunc) */
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
	MapFloors floor;		/* 0x02f0 Scrap.GetFloor */
	struct cMapElement *elements;	/* 0x1bb4 map elements, newest first (Scrap.CreateElements, CallElements) */
	TABLE(cWeapInfo) weapons;	/* 0x1bb8 SWeap.InitWeap, GetNWeap */
	int firstWeapon;		/* 0x1bc0 SWeap.GetFirstiWeap */
	TABLE(cAmmoInfo) ammo;		/* 0x1bc4 SWeap.InitAmmo, GetNAmmo */
	cCollisionGrid grid;		/* 0x1bcc Scrap.SetGrid */
	cAlarm alarm;			/* 0x1c68 Scrap.GetAlarm, GetLockAlarm */
	byte strMoney[0x2090 - 0x2078];	/* 0x2078 the digits of the money (cStrMoney, logic.h), its int is money */
	int money;			/* 0x2090 Scrap.GetMoney */
	cLogicControl logic;		/* 0x2094 the taunts, parking zones, items and the mode objects (SLogic, Scrap.AddItem) */
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
SD_OFFSET(cWorld, weapons, 0x1bb8);
SD_OFFSET(cWorld, firstWeapon, 0x1bc0);
SD_OFFSET(cWorld, ammo, 0x1bc4);
SD_OFFSET(cWorld, money, 0x2090);
SD_OFFSET(cWorld, grid, 0x1bcc);
SD_OFFSET(cWorld, logic, 0x2094);
SD_SIZE(cWorld, 0x225c);

/* World/: methods; Table<T>::Init per element type */
void WeapTable_Init(void *table, int num);
void AmmoTable_Init(void *table, int num);
void cWorld_FillWeapPriority(cWorld *world);
int cWeapInfo_GetF(cWeapInfo *weap, const char *var, float *value);
int cWeapInfo_GetS(cWeapInfo *weap, const char *var, char **value);
int cWeapInfo_SetF(cWeapInfo *weap, const char *var, float value);
int cWeapInfo_SetS(cWeapInfo *weap, const char *var, const char *value);
int cAmmoInfo_GetF(cAmmoInfo *ammo, const char *var, float *value);
int cAmmoInfo_GetS(cAmmoInfo *ammo, const char *var, char **value);
int cAmmoInfo_SetF(cAmmoInfo *ammo, const char *var, float value);
int cAmmoInfo_SetS(cAmmoInfo *ammo, const char *var, const char *value);

/*
 * the application (App/, include/scrap/app.h): CApp (0x48 bytes, vftable rva 0x3cf190) is the game part, CWinApp
 * (0x90 bytes, vftable rva 0x3cf7d4) adds the Win32 window. Allocated and run by WinMain. The slots
 * are the vf numbers of both classes.
 */
typedef struct CApp CApp;
typedef struct CAppVtbl {
	void(*DoneGame)(CApp *self);					/* 0x00 vf00 */
	void(*InitGame)(CApp *self, const char *level);			/* 0x04 vf01 */
	void(*InitDebug)(CApp *self);					/* 0x08 vf02 */
	CApp *(*Delete)(CApp *self, byte flags);			/* 0x0c vf03 */
	byte(*Init)(CApp *self);						/* 0x10 vf04 */
	byte(*Frame)(CApp *self);						/* 0x14 vf05 */
	void(*Shutdown)(CApp *self);					/* 0x18 vf06 */
	void(*LoadLevelNow)(CApp *self, const char *level);		/* 0x1c vf07 */
	void(*InitDedicated)(CApp *self);					/* 0x20 vf08 */
	void(*Exit)(CApp *self);						/* 0x24 vf09 */
	void(*Update)(CApp *self, float dt);				/* 0x28 vf10 */
	void(*Render)(CApp *self);					/* 0x2c vf11 */
	byte(*ParseParam)(CApp *self, const char *param, const char *ext);	/* 0x30 vf12 */
	void(*ResetParams)(CApp *self);					/* 0x34 vf13 */
	double(*GetTime)(CApp *self);					/* 0x38 vf14 */
	double(*GetGameTime)(CApp *self);					/* 0x3c vf15 */
	void(*ShowConsole)(CApp *self, byte show);			/* 0x40 vf16 */
	void(*ConsoleAdd)(CApp *self, const char *text, uint32_t color);	/* 0x44 vf17 */
	const char *(*DebugInput)(CApp *self, const char *text);		/* 0x48 vf18 */
	byte(*RunApp)(CApp *self, const char *exe, const char *cmdLine, int arg);	/* 0x4c vf19 */
	byte(*IsDedicatedWin)(CApp *self);				/* 0x50 vf20 */
} CAppVtbl;
SD_OFFSET(CAppVtbl, Exit, 0x24);
SD_OFFSET(CAppVtbl, DebugInput, 0x48);
SD_OFFSET(CAppVtbl, IsDedicatedWin, 0x50);

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

#define APPF_CLIENT 1
#define APPF_SERVER 2
#define APPF_DEDICATED 4

struct CApp {
	const CAppVtbl *vftable;	/* 0x00 */
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
};
SD_OFFSET(CApp, cmdLine, 0x08);
SD_OFFSET(CApp, gameTime, 0x20);
SD_OFFSET(CApp, flags, 0x30);
SD_OFFSET(CApp, levelPath, 0x34);
SD_OFFSET(CApp, onLevelLoaded, 0x40);
SD_SIZE(CApp, 0x48);

typedef struct CWinApp {
	CApp base;			/* 0x00 */
	void *hInstance;		/* 0x48 */
	void *hPrevInstance;		/* 0x4c */
	int nCmdShow;			/* 0x50 */
	exe_wndclass wc;		/* 0x54 "ScrapClass" */
	void *hwnd;			/* 0x7c */
	byte console;			/* 0x80 -console: the console window */
	byte dedicatedWin;		/* 0x81 -dedicated: small window */
	byte pad_82[2];
	byte unused_84[0xc];		/* 0x84 */
} CWinApp;
SD_OFFSET(CWinApp, wc, 0x54);
SD_OFFSET(CWinApp, hwnd, 0x7c);
SD_OFFSET(CWinApp, unused_84, 0x84);
SD_SIZE(CWinApp, 0x90);

extern cWorld *g_pWorld;
extern CApp *g_pApp;
/* the application is always a CWinApp */
#define g_pWinApp ((CWinApp *)g_pApp)

#endif
