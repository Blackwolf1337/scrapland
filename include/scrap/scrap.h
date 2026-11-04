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

#define g_PackName (((cString *)&GVar_PathInPackFile.value))	/* string object, the open pack */
extern cGVar GVar_SuperDealAvaliable;
#define g_SuperDealAvaliable (GVar_SuperDealAvaliable.value.i)	/* SuperDealAvaliable, all three crazy deals done */

void Elements_Call(void *elements, const char *name);
void Elements_Create(void *elements);

void *cWorld_NewEntityList(cWorld *world, const char *name);
const char *cWorld_GetCamName(cWorld *world, int i);
cEntity *cWorld_GetFirstEntity(cWorld *world);

cGVar *GVar_Find(const char *name);
/* the number of an int or float variable (def: its default), 0 for other types; 0 and a message for an unknown name */
double GVar_GetNum(const char *name, byte def);
/* as Scrap.Set: 1 set, 0 read only, -1 unknown name or wrong type (each with its message) */
int GVar_SetNum(const char *name, double d);
/* a string or script function variable */
int GVar_SetStr(const char *name, const char *s);
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

/* cFuncGvar: the abstract base of the GVAR_FUNCTION variables (cFuncGvar::vftable Save and
 * Load pure) */
typedef struct cFuncGvar {
	cGvar gvar;			/* g_GVars points here */
} cFuncGvar;
#define GVar_ListObj(v) ((void *)((byte *)(v) - offsetof(cFuncGvar, gvar)))

/* the object of a GVAR_SCRIPT_FUNCTION variable (INIT_GVAR_FUNCTION of scrap/staticinit.h): its function in place of value
 * and def */
typedef struct cGVarFunction {
	union {
		cGVar var;
		struct {
			cGvar head;
			cPy_Function func;
		};
	};
} cGVarFunction;

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
	void *data;
	uint32_t size;
	byte lock;
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
	cDebris *idle;			/* the free debris */
	cDebris *active;		/* the dropped ones */
	int idleCount;
	char *name;			/* string object, the key in g_pWorld->debris; the model */
	float time;			/* the time left of the 0.05 s steps */
	float radiusSq;			/* the debris further from the cameras are not moved, 0 for all */
	float b;			/* the friction of the move */
	float c;			/* the scale of the model */
	float d;			/* the gravity */
	byte flag;			/* collisions with the floor */
	byte pad_25[3];
	DebrisHit_t onHit;
	int maxIdle;			/* 0x100 */
	int fxType;			/* the cObjElementFX of each debris (cDebris_SetFX), 0 for none */
	struct cDebrisSys *next;	/* g_pWorld->debrisList */
} cDebrisSys;

struct cDebris {
	CGameModel Mod;			/* first: Debris_Model casts the debris */
	vec3_t drop[4];			/* copy of g_DebrisDrop */
	float life;			/* the time left, counts down by 0.05 a step and ends at 0 */
	cDebris *next;			/* in the idle or active list */
	struct cObjElementFX *fx;
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
/* the move of a debris of the system, 0 when it is gone */
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
	char *name;			/* cString */
	char *value;			/* cString */
	struct cSaveVar *next;		/* g_SaveVars */
} cSaveVar;

void cFuncGvarDevices_Load(struct cFuncGvar *elem, void *f);
void cFuncGvarDevices_Save(struct cFuncGvar *elem, void *f);

#endif
