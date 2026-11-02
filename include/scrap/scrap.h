/*
 * Engine functions behind the Scrap script module that have no better home yet: save variables, languages,
 * packs, loading screens, the global call builder and the world objects only known by their methods.
 */
#ifndef SCRAP_SCRAP_H
#define SCRAP_SCRAP_H

#include "scrap/cstring.h"
#include "scrap/entity.h"
#include "scrap/filepak.h"
#include "scrap/gvar.h"
#include "scrap/mathlib.h"
#include "scrap/pyentity.h"
#include "scrap/python.h"

#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrapmodel.h"

/* the function of Scrap.SetCallFunc with its parameters (AddParamX) for Scrap.Execute */
extern cPy_Function g_CallFunc;
#define g_PackName (((cString *)&GVar_PathInPackFile.value))	/* string object, the open pack */
extern cGVar GVar_SuperDealAvaliable;
#define g_SuperDealAvaliable (GVar_SuperDealAvaliable.value.i)	/* SuperDealAvaliable, all three crazy deals done */

void Elements_Call(void *elements, const char *name);
void Elements_Create(void *elements);

void *cWorld_NewEntityList(cWorld *world, const char *name);
const char *cWorld_GetCamName(cWorld *world, int i);
cEntity *cWorld_GetFirstEntity(cWorld *world);

cGVar *GVar_Find(const char *name);
/* the base constructor (src/game/Global/Gvars.c): in front of g_GVars, the derived class sets the type */
cGVar *cGVar_ctor(cGVar *self, const char *name, byte flags, const char *description);
/* an int variable with its default, as the static initializers make it */
cGVar *GVar_InitInt(cGVar *v, const char *name, byte flags, const char *description, int value);
cGVar *GVar_InitFloat(cGVar *v, const char *name, byte flags, const char *description, float value);
/* a string variable: value and default are cString, the destructor frees both */
cGVar *GVar_InitString(cGVar *v, const char *name, byte flags, const char *description, const char *value);
void GVar_FiniString(cGVar *v);
/* a script function variable bound to func ("Module.function", "!native", empty or NULL for none) */
cGVar *GVar_InitFunction(cGVar *v, const char *name, byte flags, const char *description, const char *func);
/* "name = value # comment" of Scrap.cfg: cuts the comment, splits at '=' and trims both; 0 without '=' */
byte Cfg_ParseLine(char *line, char **value);

struct cFuncGvar;

typedef struct cGVarListVtbl {
	void(*Save)(struct cFuncGvar *self, void *f);	/* 0x0 */
	void(*Load)(struct cFuncGvar *self, void *f);	/* 0x4 */
} cGVarListVtbl;

/* cFuncGvar, 0x14 bytes: the abstract base of the GVAR_FUNCTION variables (cFuncGvar::vftable rva 0x3bc2c8, Save and
 * Load pure) */
typedef struct cFuncGvar {
	const cGVarListVtbl *vftable;	/* 0x00 */
	cGvar gvar;			/* 0x04 g_GVars points here */
} cFuncGvar;
#define GVar_ListObj(v) ((void *)((byte *)(v) - offsetof(cFuncGvar, gvar)))
#define GVar_ListVtbl(v) (*(const cGVarListVtbl **)GVar_ListObj(v))

/* the object of a GVAR_SCRIPT_FUNCTION variable (INIT_GVAR_FUNCTION of scrap/staticinit.h): its function in place of value
 * and def */
typedef struct cGVarFunction {
	union {
		cGVar var;
		struct {
			cGvar head;
			cPy_Function func;	/* 0x10 */
		};
	};
} cGVarFunction;

extern cGVar *g_GVars;		/* every variable, newest first */
extern HashTable *g_GVarTable;	/* by name after GVars_Init */

byte SaveVar_Create(const char *name, const char *value);
void SaveVars_Delete(const char *prefix);
const char *SaveVar_Get(const char *name);
int SaveVar_Inc(const char *name, int n);
PyObject *SaveGames_GetList(const char *path);

PyObject *Lang_GetEntries(const char *name, int n);
const char *Lang_GetStr(const char *name);
const char *Lang_GetLanguage(void);
/* the string table of the language (src/game/Global/Language.c) */
void LangTable_Clear(HashTable *self);

/* Global/StaticBuffer.h: a buffer that grows and is locked while in use */
typedef struct cStaticBuffer {
	void *data;			/* 0x0 */
	uint32_t size;			/* 0x4 */
	byte lock;			/* 0x8 */
} cStaticBuffer;
/* at least size bytes, locks the buffer */
void *cStaticBuffer_Get(cStaticBuffer *self, uint32_t size);
/* the end of the use of the buffer (CHECK lock); inlined in most places */
void cStaticBuffer_Unlock(cStaticBuffer *self);
cStaticBuffer *cStaticBuffer_ctor(cStaticBuffer *self);
void cStaticBuffer_dtor(cStaticBuffer *self);
float cWorld_GetMinCamDistSq(cWorld *world, const vec3_t *pos);
void Parking_AddZone(void *parking, int flag, const vec3_t *pos, const char *name);
/* nearest (farest != 0: farest) parked vehicle from pos; its position into out */
const char *Parking_Find(void *parking, int flag, const vec3_t *pos, vec3_t *out, int farest);
/* like cActClass_Var on an entity list: returns 1 string, 2 double, 3 int, 0 */
int cEntityList_Var(void *list, const char *name, int set, char **s, double *d, int *i);
int Video_GetModes(int i, char *buf);

/* debris systems (named) and the values SetDebrisValue sets for the next DropDebris: World/ObjPool.cpp, a pool of
 * debris (models with a move, or the carrier of an effect cObjElementFX) */
typedef struct cDebris cDebris;
/* the hit of a debris on the floor, instead of the bounce */
typedef void (*DebrisHit_t)(cDebris *debris, void *tri);

typedef struct cDebrisSys {
	cDebris *idle;			/* 0x00 the free debris */
	cDebris *active;		/* 0x04 the dropped ones */
	int idleCount;			/* 0x08 */
	char *name;			/* 0x0c string object, the key in g_pWorld->debris; the model */
	float time;			/* 0x10 the time left of the 0.05 s steps */
	float radiusSq;			/* 0x14 the debris further from the cameras are not moved, 0 for all */
	float b;			/* 0x18 the friction of the move */
	float c;			/* 0x1c the scale of the model */
	float d;			/* 0x20 the gravity */
	byte flag;			/* 0x24 collisions with the floor */
	byte pad_25[3];
	DebrisHit_t onHit;		/* 0x28 */
	int maxIdle;			/* 0x2c 0x100 */
	int fxType;			/* 0x30 the cObjElementFX of each debris (cDebris_SetFX), 0 for none */
	struct cDebrisSys *next;	/* 0x34 g_pWorld->debrisList */
} cDebrisSys;

struct cDebris {
	CGameModel Mod;			/* 0x00 first: Debris_Model casts the debris */
	vec3_t drop[4];			/* 0x1c copy of g_DebrisDrop */
	float life;			/* 0x4c the time left, counts down by 0.05 a step and ends at 0 */
	cDebris *next;			/* 0x50 in the idle or active list */
	struct cObjElementFX *fx;	/* 0x54 */
};
#define Debris_Mod(d) (&(d)->Mod)

extern vec3_t g_DebrisDrop[4];	/* [4]: position, angles (rad), velocity, angle speed (rad) */
extern cGVar GVar_Gravity;
#define g_DebrisDefault (GVar_Gravity.value.f)

cDebrisSys *cWorld_GetDebrisSys(cWorld *world, const char *name);
byte cWorld_DeleteDebrisSys(cWorld *world, const char *name);
void cDebrisSys_Init(cDebrisSys *sys, int n, int e);
cDebris *cDebrisSys_NewElement(cDebrisSys *sys);
void cDebrisSys_Drop(cDebrisSys *sys, cDebris *debris);
/* rva 0x204ea0: the move of a debris of the system, 0 when it is gone */
byte cDebris_Update(cDebris *self, cDebrisSys *sys);

/* entity list by name for the collision tests */
uint32_t EntityLists_Get(const char *name);
byte cWorld_TestLineEntities(cWorld *world, cEntity *exclude, uint32_t mask, const vec3_t *from, const vec3_t *to,
	vec3_t *hit, cEntity **target, int flag);
byte cWorld_TestSphereEntities(cWorld *world, cEntity *exclude, uint32_t mask, const vec3_t *center,
	float radius, cEntity **target, int a, const vec3_t *prev);
byte cWorld_SphereCall(cWorld *world, uint32_t mask, const vec3_t *center, float radius, const char *func,
	int flag);
byte DummySearch_Next(char *buf, int size, vec3_t *pos, float *ang, byte *flag);
void Object3D_Preload(const char *name, const vec3_t *scale);
cEntity *cWorld_CreateEntity(cWorld *world, const char *className, float x, float y, float z,
	const char *name);
/* printf into a string object */
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
byte Video_Play(const char *name, const char *cmd, float arg);
/* Video_Play "play" with a call when the video ends (Scrap.VideoPlayer with a callback) */
byte Video_PlayThen(const char *name, void (*done)(int result));
byte Music_Play(const char *name, const char *cmd, float volume);
void Model_Info(const char *name, int arg);
void GVars_Show(const char *filter);
void DummySearch_Start(const char *name, int flag);
void MissionArrows_Switch(const char *a, const char *b);
byte Lang_Set(const char *lang, byte force);
const char *Version_Get(void);
int cWorld_SetCam(cWorld *world, int i, const char *name);
cEntity *cWorld_GetUsrEntity(cWorld *world, int player);

int Video_GetCurrentMode(void);
void Models_List(void);

typedef struct cSaveVar {
	char *name;			/* 0x0 cString */
	char *value;			/* 0x4 cString */
	struct cSaveVar *next;		/* 0x8 g_SaveVars */
} cSaveVar;

#endif
