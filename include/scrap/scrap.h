/*
 * Engine functions behind the Scrap script module that have no better home yet: save variables, languages,
 * packs, loading screens, Steam/GOG, the global call builder and the world objects only known by their methods.
 */
#ifndef SCRAP_SCRAP_H
#define SCRAP_SCRAP_H

#include "scrap/entity.h"
#include "scrap/python.h"

#include "scrap/scorer.h"

/* the function of Scrap.SetCallFunc with its parameters (AddParamX) for Scrap.Execute (rva 0x466a04) */
#define g_CallFunc ((cPy_Function *)Game_Ptr(0x466a04))
#define g_PackName ((void *)Game_Ptr(0x44ab6c))	/* string object, the open pack */
#define g_IsSteam GAME_DATA(byte, 0x467b9c)

void __thiscall cPy_Function_AddParamF(cPy_Function *f, float v);
void __thiscall cPy_Function_AddParamI(cPy_Function *f, int v);
void __thiscall cPy_Function_AddParamS(cPy_Function *f, const char *v);
void __thiscall cPy_Function_Execute(cPy_Function *f, int arg);
byte __thiscall cPy_Function_IsValid(cPy_Function *f);

void __thiscall Scheduler_Add(void *scheduler, float time, PyObject *func, PyObject *params, const char *name);
void __thiscall Scheduler_Delete(void *scheduler, const char *name);
void __thiscall Elements_Call(void *elements, const char *name);
void __thiscall Elements_Create(void *elements);
int __thiscall Floor_Get(void *floor, float y);
float __thiscall Timer_GetTime(void *timer);
int __thiscall Alarm_GetValue(void *alarm);
byte __thiscall Alarm_IsLocked(void *alarm);

void *__thiscall cWorld_NewEntityList(cWorld *world, const char *name);
const char *__thiscall cWorld_GetCamName(cWorld *world, int i);
cEntity *__thiscall cWorld_GetFirstEntity(cWorld *world);
/* the Python handle (cPyEntity) of the entity with that name */
PyObject *PyEntity_New(const char *name);

/* engine global variables (gvars): description at +8 */
typedef struct cGVar {
	byte pad_0[0x8];
	char *description;		/* 0x8 */
} cGVar;
cGVar *GVar_Find(const char *name);

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

int Pack_Extract(const char *name);
/* printf into a string object (rva 0x134080) */
void cString_Printf(void *str, const char *fmt, ...);
void Pack_PreloadLibrary(const char *a, const char *b);
void Anm_Preload(const char *a, const char *b);
void DVF_Process(const char *name, int arg);
void Python_CompileAll(void);
void Config_Save(void);
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
byte Lang_Set(const char *lang, int flag);
const char *Version_Get(void);
void __thiscall CWinApp_LoadLevel(CWinApp *app, const char *level);
int __thiscall cWorld_SetCam(cWorld *world, int i, const char *name);
cEntity *__thiscall cWorld_GetUsrEntity(cWorld *world, int player);
void __thiscall Alarm_Set(void *alarm, float level);
void __thiscall Alarm_SetChar(void *alarm, const char *name);
void __thiscall Alarm_SetGrow(void *alarm, int grow);
void __thiscall Alarm_SetLock(void *alarm, int lock);
void __thiscall Timer_SetTime(void *timer, float t);
void __thiscall Timer_SetSpeed(void *timer, float speed);

/* steam_api: SteamInternal_ContextInit through the import table of the exe, context data at rva 0x428a30 */
typedef void *(__cdecl *SteamContextInit_t)(void *ctx);
#define g_SteamInternal_ContextInit GAME_DATA(SteamContextInit_t, 0x3a15a4)
#define g_SteamFriendsContext ((void *)Game_Ptr(0x428a30))
void Font_Build(const char *name, int size);
void Loading_Init(void);
void Loading_End(void);
int Video_GetCurrentMode(void);
void Models_List(void);

/* console printf of warnings (rva 0xbc8b0) */
void Con_Warnf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
