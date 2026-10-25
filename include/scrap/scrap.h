/*
 * Engine functions behind the Scrap script module that have no better home yet: save variables, languages,
 * packs, loading screens, Steam/GOG, the global call builder and the world objects only known by their methods.
 */
#ifndef SCRAP_SCRAP_H
#define SCRAP_SCRAP_H

#include "scrap/entity.h"
#include "scrap/python.h"

/* the global call builder of Scrap.SetCallFunc, AddParamX and Execute (rva 0x466a04) */
#define g_CallFunc ((void *)Game_Ptr(0x466a04))
#define g_PackName ((void *)Game_Ptr(0x44ab6c))	/* string object, the open pack */
#define g_IsSteam GAME_DATA(byte, 0x467b9c)

void __thiscall CallFunc_AddParamF(void *call, float v);
void __thiscall CallFunc_AddParamI(void *call, int v);
void __thiscall CallFunc_AddParamS(void *call, const char *v);
void __thiscall CallFunc_Execute(void *call, int arg);

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
void Font_Build(const char *name, int size);
void Loading_Init(void);
void Loading_End(void);
int Video_GetCurrentMode(void);
void Models_List(void);

/* console printf of warnings (rva 0xbc8b0) */
void Con_Warnf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
