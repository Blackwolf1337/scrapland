/*
 * World/World.cpp of the game: cWorld, the world of the game entities (g_pWorld), its weapon and ammo tables, the
 * entity queries along lines and spheres on the entity grid, the cameras of the views and the per frame update
 * (src/game/World/World.c). The struct is in include/scrap/game.h, the older declarations in include/scrap/scrap.h.
 */
#ifndef SCRAP_WORLD_H
#define SCRAP_WORLD_H

#include "scrap/gvar.h"
#include "scrap/collisiongrid.h"
#include "scrap/entity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

extern cGVarFunction GVar_OnGeneralDamage;
extern cGVarFunction GVar_OnLifeItemTaken;
extern cGVarFunction GVar_OnSleep;

/* a weapon of g_pWorld->weapons (SWeap.InitWeap) */
struct cWeapInfo {
	char *name;			/* 0x00 string object */
	cPy_Function onPress;		/* 0x04 */
	cPy_Function onRelease;		/* 0x18 */
	cPy_Function onFire;		/* 0x2c */
	cPy_Function onHit;		/* 0x40 */
	cPy_Function onStart;		/* 0x54 */
	cPy_Function onEnd;		/* 0x68 */
	cPy_Function setFx;		/* 0x7c */
	int next;			/* 0x90 iNext, the weapon order of FillWeapPriority */
	int prev;			/* 0x94 iPrev */
	int ammo;			/* 0x98 iAmmo */
	byte upgrade[16];		/* 0x9c "UpgrdNN" reads byte 0x9b + NN */
	float minAmmo;			/* 0xac */
};

/* an ammo type of g_pWorld->ammo (SWeap.InitAmmo) */
struct cAmmoInfo {
	char *name;			/* 0x00 string object */
	float max;			/* 0x04 */
	float charger;			/* 0x08 */
	cPy_Function onPress;		/* 0x0c */
	cPy_Function onRelease;		/* 0x20 */
	cPy_Function onFire;		/* 0x34 */
	cPy_Function onHit;		/* 0x48 */
};

/* the destructors of the table items (rva), for Delete_Objects */
#define CAMMOINFO_DTOR 0x19fa40
#define CWEAPINFO_DTOR 0x19fa70
#define CAMMOINFO_CTOR 0x206360
#define CWEAPINFO_CTOR 0x2063f0

#define CWORLD_SIZE sizeof(cWorld)

/* the fields of entities the world reads */
#define Entity_Mask(e) (((cEntity *)(e))->mask)		/* the entity lists it is in, a bit each */
#define CamEntity_Cam(e) (&((cCamControl *)(e))->cam.pos)		/* the camera of a "Cam", its position first */
#define CamEntity_Orient(e) (&((cCamControl *)(e))->cam.rot)
#define CamEntity_Slot(e) (((cCamControl *)(e))->slot)		/* the view it shows, -1 for none */

int MapLevel_NearestUp(const MapLevel *self, const vec3_t *p);

/* the objects embedded in cWorld, only known by their methods */
void *Elements_ctor(void *elements);
void Elements_dtor(void *elements);
void Elements_Update(void *elements, float dt);
void *cStrMoney_ctor(void *obj);
void cStrMoney_Update(void *obj, float dt);
void *Logic_ctor(void *logic);
void Logic_dtor(void *logic);
void Logic_Reset(void *logic);
void LogicControl_Update(void *logic, float dt);
void cScheduledFuncStack_dtor(void *stack);

cDebrisSys *cDebrisSys_ctor(void *mem);
void cDebrisSys_dtor(cDebrisSys *sys);
void cDebrisSys_SetName(cDebrisSys *sys, const char *name);
/* the debris of the list at 0x0 (the free ones, the count cleared) and of the list at 0x4 (the dropped ones)
 * deleted; the names are older than the knowledge of the lists */
void cDebrisSys_FreeActive(cDebrisSys *sys);
void cDebrisSys_FreeIdle(cDebrisSys *sys);
void cDebrisSys_Update(cDebrisSys *sys, float dt);
/* the debug text of the object pool (O_Show) at x, y; *line goes on to the next line */
void cDebrisSys_ShowInfo(cDebrisSys *sys, int x, int y, float *line);

/* the entity classes of cWorld_CreateEntity: constructors on the memory from operator_new, return it */
cEntity *cEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cCamPathEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cWalkCharEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cVehicleEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cCamEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cFollowCamEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cMissileEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cItemEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cWithLifeEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *cObjEntity_ctor(void *mem, const char *name, float x, float y, float z);
/* the entity after e in the world list or NULL */
cEntity *cEntity_Next(cEntity *e);
/* the entity a follow camera follows */
cEntity *cCamEntity_GetTarget(cEntity *cam);
/* the entity the camera follows (its +0xf0 points back), and the angles of the camera (x and y) */
void cCamEntity_SetTarget(cEntity *cam, cEntity *target);
void cCamEntity_SetAng(cEntity *cam, vec3_t ang);

/* an entity list of g_pWorld->lists (0x44 bytes: name, two functions, the entities at 0x30) */
void *cEntityList_ctor(void *mem, const char *name);
void EntityDList_dtor(void *list);

/* the engine3d side: the camera of the view (NULL for none) and the box of the level */
/* frees the model list at rva 0x4479a8 */
void GameModelList_Free(void);
/* the action went over 0.5 for the player in this frame */
/* RemainTalkTime > 0: a talk text is shown */
int Talk_IsShown(void);
void cPy_Function_Clear(cPy_Function *self);
void Str_TrimBlanks(char *s);

extern cGVar GVar_SplitScreen;
#define g_SplitScreenGame (GVar_SplitScreen.value.i)		/* SplitScreen */
extern cGVar GVar_VehicleEditingMode;
#define g_VehicleEditingMode (GVar_VehicleEditingMode.value.i)
extern cGVar GVar_MaxChasingShips;
#define g_MaxChasingShips (GVar_MaxChasingShips.value.i)
#define g_MaxChasingShipsDef (GVar_MaxChasingShips.def.i)
extern cGVar GVar_S_PitchScale;
#define g_PitchScale (GVar_S_PitchScale.value.f)		/* S_PitchScale */
extern cGVar GVar_FreeSpaceMagnet;
#define g_FreeSpaceMagnet (GVar_FreeSpaceMagnet.value.i)
#define g_FreeSpaceMagnetDef (GVar_FreeSpaceMagnet.def.i)
extern cGVar GVar_FXVehicleFloatingFactor;
#define g_FXVehicleFloatingFactor (GVar_FXVehicleFloatingFactor.value.f)
#define g_FXVehicleFloatingFactorDef (GVar_FXVehicleFloatingFactor.def.f)
extern cGVar GVar_R_SceneMotionBlurValue;
#define g_SceneMotionBlurValue (GVar_R_SceneMotionBlurValue.value.f)	/* R_SceneMotionBlurValue */
extern cGVar GVar_NotShootAI;
#define g_NotShootAI (GVar_NotShootAI.value.i)
#define g_NotShootAIDef (GVar_NotShootAI.def.i)
extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)
extern cGVar GVar_R_SplitScreenCamera;
#define g_SplitScreenCamera (GVar_R_SplitScreenCamera.value.i)		/* R_SplitScreenCamera */
extern cGVar GVar_R_SplitScreenUpdate;
#define g_SplitScreenUpdate (GVar_R_SplitScreenUpdate.value.i)		/* R_SplitScreenUpdate */
/* script callbacks (cPy_Function of the GVAR_SCRIPT_FUNCTION variables) */
#define g_OnSleep (((cPy_Function *)&GVar_OnSleep.func))
#define g_OnLifeItemTaken (((cPy_Function *)&GVar_OnLifeItemTaken.func))
#define g_OnGeneralDamage (((cPy_Function *)&GVar_OnGeneralDamage.func))

cEntity *Entity_FindInView(cEntity *self, float range, float *outDot, float *outDist, int mode, float minDot);
byte Team_IsFriend(int a, int b);
byte Team_IsEnemy(int a, int b);
cWorld *cWorld_ctor(cWorld *self);
void cWorld_AddIgnore(cWorld *self, cEntity *e);
byte cWorld_LineCall(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	const char *func);
void cWorld_Clear(cWorld *self);
void *cWorld_GetObjData(cWorld *self, const char *name);
void cWorld_ResetLogic(cWorld *self);
byte World_IsEasySingle(void);
byte World_IsHardSingle(void);
void cWorld_ShowDebrisInfo(cWorld *self, int x, int y);
void cWorld_Update(cWorld *self, float dt);
byte cWorld_SphereCallF(cWorld *self, uint32_t mask, const vec3_t *center, float radius,
	cPy_Function *func, int noWall);

#endif
