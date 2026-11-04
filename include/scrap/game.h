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

typedef struct cWorld {
	HashTable entities;		/* every entity by name (cEntity *) */
	Nto1Node entityList;		/* head of the entities (their node at +8), target the world */
	Nto1Node deadList;		/* entities to delete after the frame */
	int nameCount;			/* suffix of the next generated name */
	cScheduledFuncStack scheduler;	/* scheduled Python functions (Scrap.AddScheduledFunc) */
	int curCam;			/* index into camPos */
	char camName[4][0x80];		/* camera entity of each view, "" for none */
	vec3_t camPos[4];		/* camera positions (Scrap.GetNearestParked) */
	byte testing;			/* set by the entity queries */
	byte pad_0285[3];
	struct cEntity *owners[4];	/* the entity of each player slot, found every frame */
	HashTable debris;		/* debris systems by name */
	struct cDebrisSys *debrisList;	/* the same, newest first */
	struct cEntity *ignore[4];	/* skipped by the next line and sphere test */
	HashTable lists;		/* entity lists by name */
	byte checkInput;		/* the input events are called every frame */
	byte pad_02c5[3];
	cTimer timer;			/* world time (Scrap.GetTime, GetTimeSpeed) */
	MapFloors floor;		/* Scrap.GetFloor */
	struct cMapElement *elements;	/* map elements, newest first (Scrap.CreateElements, CallElements) */
	TABLE(cWeapInfo) weapons;	/* SWeap.InitWeap, GetNWeap */
	int firstWeapon;		/* SWeap.GetFirstiWeap */
	TABLE(cAmmoInfo) ammo;		/* SWeap.InitAmmo, GetNAmmo */
	cCollisionGrid grid;		/* Scrap.SetGrid */
	cAlarm alarm;			/* Scrap.GetAlarm, GetLockAlarm */
	byte strMoney[0x2090 - 0x2078];	/* the digits of the money (cStrMoney, logic.h), its int is money */
	int money;			/* Scrap.GetMoney */
	cLogicControl logic;		/* the taunts, parking zones, items and the mode objects (SLogic, Scrap.AddItem) */
} cWorld;

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
 * the application (App/, include/scrap/app.h): CApp (vftable) is the game part, CWinApp
 * (vftable) adds the Win32 window. Allocated and run by WinMain. The slots
 * are the vf numbers of both classes.
 */
typedef struct CApp CApp;
typedef struct CAppVtbl {
	void(*DoneGame)(CApp *self);
	void(*InitGame)(CApp *self, const char *level);
	void(*InitDebug)(CApp *self);
	void (*Free)(CApp *self);
	byte(*Init)(CApp *self);
	byte(*Frame)(CApp *self);
	void(*Shutdown)(CApp *self);
	void(*LoadLevelNow)(CApp *self, const char *level);
	void(*InitDedicated)(CApp *self);
	void(*Exit)(CApp *self);
	void(*Update)(CApp *self, float dt);
	void(*Render)(CApp *self);
	byte(*ParseParam)(CApp *self, const char *param, const char *ext);
	void(*ResetParams)(CApp *self);
	double(*GetTime)(CApp *self);
	double(*GetGameTime)(CApp *self);
	void(*ShowConsole)(CApp *self, byte show);
	void(*ConsoleAdd)(CApp *self, const char *text, uint32_t color);
	const char *(*DebugInput)(CApp *self, const char *text);
	byte(*RunApp)(CApp *self, const char *exe, const char *cmdLine, int arg);
	byte(*IsDedicatedWin)(CApp *self);
} CAppVtbl;

/* calls through the slots of CAppVtbl */
static inline const char *App_DebugInput(void *self, const char *text) {
	return VT(CAppVtbl, self)->DebugInput(self, text);
}

static inline void App_Free(void *self) {
	VT(CAppVtbl, self)->Free(self);
}

static inline void App_DoneGame(void *self) {
	VT(CAppVtbl, self)->DoneGame(self);
}

static inline void App_Exit(void *self) {
	VT(CAppVtbl, self)->Exit(self);
}

static inline byte App_Frame(void *self) {
	return VT(CAppVtbl, self)->Frame(self);
}

static inline double App_GetGameTime(void *self) {
	return VT(CAppVtbl, self)->GetGameTime(self);
}

static inline double App_GetTime(void *self) {
	return VT(CAppVtbl, self)->GetTime(self);
}

static inline byte App_Init(void *self) {
	return VT(CAppVtbl, self)->Init(self);
}

static inline void App_InitDebug(void *self) {
	VT(CAppVtbl, self)->InitDebug(self);
}

static inline void App_InitDedicated(void *self) {
	VT(CAppVtbl, self)->InitDedicated(self);
}

static inline void App_InitGame(void *self, const char *level) {
	VT(CAppVtbl, self)->InitGame(self, level);
}

static inline byte App_IsDedicatedWin(void *self) {
	return VT(CAppVtbl, self)->IsDedicatedWin(self);
}

static inline void App_LoadLevelNow(void *self, const char *level) {
	VT(CAppVtbl, self)->LoadLevelNow(self, level);
}

static inline byte App_ParseParam(void *self, const char *param, const char *ext) {
	return VT(CAppVtbl, self)->ParseParam(self, param, ext);
}

static inline void App_Render(void *self) {
	VT(CAppVtbl, self)->Render(self);
}

static inline void App_ResetParams(void *self) {
	VT(CAppVtbl, self)->ResetParams(self);
}

static inline byte App_RunApp(void *self, const char *exe, const char *cmdLine, int arg) {
	return VT(CAppVtbl, self)->RunApp(self, exe, cmdLine, arg);
}

static inline void App_ShowConsole(void *self, byte show) {
	VT(CAppVtbl, self)->ShowConsole(self, show);
}

static inline void App_Shutdown(void *self) {
	VT(CAppVtbl, self)->Shutdown(self);
}

static inline void App_Update(void *self, float dt) {
	VT(CAppVtbl, self)->Update(self, dt);
}

/* WNDCLASSA */
typedef struct exe_wndclass {
	uint32_t style;
	void *wndProc;
	int clsExtra;
	int wndExtra;
	void *hInstance;
	void *hIcon;
	void *hCursor;
	void *hbrBackground;
	const char *menuName;
	const char *className;
} exe_wndclass;

#define APPF_CLIENT 1
#define APPF_SERVER 2
#define APPF_DEDICATED 4

struct CApp {
	const CAppVtbl *vftable;
	byte pad_04[4];
	char *cmdLine;			/* copy (Mem_Alloc) */
	byte pad_0c[4];
	double time;			/* GetTime of this frame */
	double lastFrame;
	double gameTime;		/* the sum of the frame times */
	int build;			/* -build[:mode]: Pack_Build of Data */
	byte active;			/* WM_ACTIVATEAPP */
	byte dedicated;			/* -dedicated */
	byte pad_2e[2];
	uint32_t flags;			/* APPF_ (SNet.IsDedicated) */
	char *levelPath;		/* Scrap.GetLevelPath */
	char *newLevelPath;		/* Scrap.GetNewLevelPath */
	byte loadPending;		/* newLevelPath is loaded at the next frame */
	byte pad_3d[3];
	void (*onLevelLoaded)(void);	/* called once after LoadLevelNow */
	byte pad_44[4];
};

typedef struct CWinApp {
	CApp base;
	void *hInstance;
	void *hPrevInstance;
	int nCmdShow;
	exe_wndclass wc;		/* "ScrapClass" */
	void *hwnd;
	byte console;			/* -console: the console window */
	byte dedicatedWin;		/* -dedicated: small window */
	byte pad_82[2];
	byte unused_84[0xc];
} CWinApp;

extern cWorld *g_pWorld;
extern CApp *g_pApp;
/* the application is always a CWinApp */
#define g_pWinApp ((CWinApp *)g_pApp)

#endif
