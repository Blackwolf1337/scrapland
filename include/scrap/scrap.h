/*
 * Engine functions behind the Scrap script module that have no better home yet: save variables, languages,
 * packs, loading screens, Steam/GOG, the global call builder and the world objects only known by their methods.
 */
#ifndef SCRAP_SCRAP_H
#define SCRAP_SCRAP_H

#include "scrap/cstring.h"
#include "scrap/entity.h"
#include "scrap/filepak.h"
#include "scrap/mathlib.h"
#include "scrap/pyentity.h"
#include "scrap/python.h"

#include "scrap/scorer.h"

/* the function of Scrap.SetCallFunc with its parameters (AddParamX) for Scrap.Execute (rva 0x466a04) */
#define g_CallFunc ((cPy_Function *)Game_Ptr(0x466a04))
#define g_PackName ((cString *)Game_Ptr(0x44ab6c))	/* string object, the open pack */
#define g_IsSteam GAME_DATA(byte, 0x467b9c)

void __thiscall Scheduler_Add(void *scheduler, float time, PyObject *func, PyObject *params, const char *name);
void __thiscall Scheduler_Delete(void *scheduler, const char *name);
void __thiscall Elements_Call(void *elements, const char *name);
void __thiscall Elements_Create(void *elements);
int __thiscall Floor_Get(void *floor, float y);
int __thiscall Alarm_GetValue(void *alarm);
byte __thiscall Alarm_IsLocked(void *alarm);

void *__thiscall cWorld_NewEntityList(cWorld *world, const char *name);
const char *__thiscall cWorld_GetCamName(cWorld *world, int i);
cEntity *__thiscall cWorld_GetFirstEntity(cWorld *world);

/* engine global variables (gvars, Scrap.Get/Set/Def/Des) */
enum { GVAR_STRING = 1, GVAR_INT = 2, GVAR_FLOAT = 4, GVAR_FUNCTION = 5, GVAR_SCRIPT_FUNCTION = 6 };

/*
 * an engine variable (Global/Gvars.cpp). The pointers in g_GVars point 4 bytes into the object: a GVAR_FUNCTION
 * variable has a vtable before it (cGVarListVtbl) that saves and loads its lines of the config file.
 */
typedef struct cGVar {
	struct cGVar *next;		/* 0x00 g_GVars */
	char *name;			/* 0x04 */
	char *description;		/* 0x08 */
	byte flags;			/* 0x0c bit 0: scripts may not write it */
	byte type;			/* 0x0d GVAR_ */
	byte pad_e[2];
	union {				/* 0x10 current value */
		char *s;		/* GVAR_STRING: first member of a string object, cString_Set writes it */
		int i;
		float f;
	} value;
	union {				/* 0x14 default value */
		char *s;
		int i;
		float f;
	} def;
} cGVar;
SD_OFFSET(cGVar, def, 0x14);
/* GVAR_SCRIPT_FUNCTION keeps a cPy_Function at +0x10 instead of value/def */
#define GVar_Function(v) ((cPy_Function *)&(v)->value)
SD_OFFSET(cGVar, type, 0x0d);
SD_OFFSET(cGVar, value, 0x10);
cGVar *GVar_Find(const char *name);

typedef struct cGVarListVtbl {
	void(__thiscall *Save)(void *self, void *f);	/* 0x0 */
	void(__thiscall *Load)(void *self, void *f);	/* 0x4 */
} cGVarListVtbl;
#define GVar_ListObj(v) ((void *)((byte *)(v) - 4))
#define GVar_ListVtbl(v) (*(const cGVarListVtbl **)GVar_ListObj(v))

#define g_GVars GAME_DATA(cGVar *, 0x44b5f8)		/* every variable, newest first */
#define g_GVarTable GAME_DATA(HashTable *, 0x44b5fc)	/* by name after GVars_Init */

void Achievement_Unlock(int n);
void Steam_SetAchievement(int n, int on);
const char *Steam_GetID(void);
const char *Steam_GetTicket(void);
const char *Gog_GetID(void);

byte SaveVar_Create(const char *name, const char *value);
void SaveVars_Delete(const char *prefix);
const char *SaveVar_Get(const char *name);
int SaveVar_Inc(const char *name, int n);
PyObject *SaveGames_GetList(const char *path);

PyObject *Lang_GetEntries(const char *name, int n);
const char *Lang_GetStr(const char *name);
const char *Lang_GetLanguage(void);

/* Global/StaticBuffer.h: a buffer that grows and is locked while in use */
typedef struct cStaticBuffer {
	void *data;			/* 0x0 */
	uint32_t size;			/* 0x4 */
	byte lock;			/* 0x8 */
} cStaticBuffer;
SD_OFFSET(cStaticBuffer, lock, 0x8);
/* at least size bytes, locks the buffer (rva 0x18c050) */
void *__thiscall cStaticBuffer_Get(cStaticBuffer *self, uint32_t size);
/* the path of the game into buf (rva 0x20d100) */
void App_GetPath(char *buf, int size);
float __thiscall cWorld_GetMinCamDistSq(cWorld *world, const vec3_t *pos);
void __thiscall Grid_Set(void *grid, const vec3_t *size);
void __thiscall Parking_AddZone(void *parking, int flag, const vec3_t *pos, const char *name);
/* nearest (farest != 0: farest) parked vehicle from pos; its position into out */
const char *__thiscall Parking_Find(void *parking, int flag, const vec3_t *pos, vec3_t *out, int farest);
/* like cActClass_Var on an entity list: returns 1 string, 2 double, 3 int, 0 */
int __thiscall cEntityList_Var(void *list, const char *name, int set, char **s, double *d, int *i);
int Video_GetModes(int i, char *buf);

/* debris systems (named) and the values SetDebrisValue sets for the next DropDebris */
typedef struct cDebrisSys {
	byte pad_00[0x14];
	float radiusSq;			/* 0x14 */
	float b;			/* 0x18 */
	float c;			/* 0x1c */
	float d;			/* 0x20 */
	byte flag;			/* 0x24 */
} cDebrisSys;
SD_OFFSET(cDebrisSys, flag, 0x24);

typedef struct cDebris {
	byte pad_00[0x1c];
	vec3_t drop[4];			/* 0x1c copy of g_DebrisDrop */
	float speed;			/* 0x4c */
} cDebris;
SD_OFFSET(cDebris, speed, 0x4c);

#define g_DebrisDrop ((vec3_t *)Game_Ptr(0x467408))	/* [4]: position, angles (rad), velocity, angle speed (rad) */
#define g_DebrisDefault GAME_DATA(float, 0x4674f4)

cDebrisSys *__thiscall cWorld_GetDebrisSys(cWorld *world, const char *name);
void __thiscall cWorld_DeleteDebrisSys(cWorld *world, const char *name);
void __thiscall cDebrisSys_Init(cDebrisSys *sys, int n, int e);
cDebris *__thiscall cDebrisSys_NewElement(cDebrisSys *sys);
void __thiscall cDebrisSys_Drop(cDebrisSys *sys, cDebris *debris);

/* entity list by name for the collision tests */
void *EntityLists_Get(const char *name);
byte Engine_TestLine(const vec3_t *from, const vec3_t *to, vec3_t *hit, int arg);
byte Engine_TestSphere(const vec3_t *center, const vec3_t *center2, float radius, int a, int b);
void __thiscall cWorld_TestLineEntities(cWorld *world, int arg, void *list, const vec3_t *from, const vec3_t *to,
	vec3_t *hit, cEntity **target, int flag);
void __thiscall cWorld_TestSphereEntities(cWorld *world, cEntity *exclude, void *list, const vec3_t *center,
	float radius, cEntity **target, int a, int b);
byte __thiscall cWorld_SphereCall(cWorld *world, void *list, const vec3_t *center, float radius, const char *func,
	int flag);
byte DummySearch_Next(char *buf, int size, vec3_t *pos, float *ang, byte *flag);
void Object3D_Preload(const char *name, const vec3_t *scale);
cEntity *__thiscall cWorld_CreateEntity(cWorld *world, const char *className, float x, float y, float z,
	const char *name);
/* printf into a string object (rva 0x134080) */
/* imports the compiled module file of the open pack as name (src/game/PyInterface/PythonInit.c) */
void Pack_PreloadLibrary(const char *name, const char *file);
void Anm_Preload(const char *a, const char *b);
void DVF_Process(const char *name, int arg);
void Python_CompileAll(void);
void Config_Save(void);
/* reads the config file name (in the config directory) */
void Config_Load(const char *name);
byte SaveGame_Save(const char *name, const char *title);
byte SaveGame_Load(const char *name);
byte SaveVar_Set(const char *name, const char *value);
void Screenshot_Take(const char *name);
void Video_SetCurrentMode(int mode);
void Video_Play(const char *name, const char *cmd, float arg);
void Music_Play(const char *name, const char *cmd, float volume);
void Model_Info(const char *name, int arg);
void GVars_Show(const char *filter);
void DummySearch_Start(const char *name, int flag);
void MissionArrows_Switch(const char *a, const char *b);
byte Lang_Set(const char *lang, byte force);
const char *Version_Get(void);
void __thiscall CWinApp_LoadLevel(CWinApp *app, const char *level);
int __thiscall cWorld_SetCam(cWorld *world, int i, const char *name);
cEntity *__thiscall cWorld_GetUsrEntity(cWorld *world, int player);
void __thiscall Alarm_Set(void *alarm, float level);
void __thiscall Alarm_SetChar(void *alarm, const char *name);
void __thiscall Alarm_SetGrow(void *alarm, int grow);
void __thiscall Alarm_SetLock(void *alarm, int lock);

/* steam_api: SteamInternal_ContextInit through the import table of the exe, context data at rva 0x428a30 */
typedef void *(__cdecl *SteamContextInit_t)(void *ctx);
#define g_SteamInternal_ContextInit GAME_DATA(SteamContextInit_t, 0x3a15a4)
#define g_SteamFriendsContext ((void *)Game_Ptr(0x428a30))
void Font_Build(const char *name, int size);
void Loading_Init(void);
void Loading_End(void);
int Video_GetCurrentMode(void);
void Models_List(void);

#endif
