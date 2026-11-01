/*
 * World/World.cpp of the game: cWorld, the world of the game entities (g_pWorld), its weapon and ammo tables, the
 * entity queries along lines and spheres on the entity grid, the cameras of the views and the per frame update
 * (src/game/World/World.c). The struct is in include/scrap/game.h, the older declarations in include/scrap/scrap.h.
 */
#ifndef SCRAP_WORLD_H
#define SCRAP_WORLD_H

#include "scrap/collisiongrid.h"
#include "scrap/entity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

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
SD_OFFSET(cWeapInfo, next, 0x90);
SD_OFFSET(cWeapInfo, minAmmo, 0xac);
SD_SIZE(cWeapInfo, 0xb0);

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
SD_OFFSET(cAmmoInfo, onHit, 0x48);
SD_SIZE(cAmmoInfo, 0x5c);

/* the destructors of the table items (rva), for Delete_Objects */
#define CAMMOINFO_DTOR 0x19fa40
#define CWEAPINFO_DTOR 0x19fa70
#define CAMMOINFO_CTOR 0x206360
#define CWEAPINFO_CTOR 0x2063f0

#define CWORLD_VFTABLE GAME_VFTABLE(cWorld_vftable)
#define CWORLD_SIZE 0x225c

/* the fields of entities the world reads */
#define Entity_Mask(e) (((cEntity *)(e))->mask)		/* the entity lists it is in, a bit each */
#define CamEntity_Cam(e) (&((cCamControl *)(e))->cam.pos)		/* the camera of a "Cam", its position first */
#define CamEntity_Orient(e) (&((cCamControl *)(e))->cam.rot)
#define CamEntity_Slot(e) (((cCamControl *)(e))->slot)		/* the view it shows, -1 for none */

int MapLevel_NearestUp(const MapLevel *self, const vec3_t *p);
int MapLevel_CompareY(const void *a, const void *b);

/* the objects embedded in cWorld, only known by their methods */
void *Elements_ctor(void *elements);
void Elements_dtor(void *elements);
void Elements_Update(void *elements, float dt);
/* World/MapElements.cpp: the first element of the model name within maxDist of pos, NULL */
struct cMapElement *Elements_Find(void *elements, const char *name, vec3_t pos, float maxDist);
/* an element of a saved game: name, position, radius of the search, on or off */
void Elements_Load(cAbstractFile *f);
void *cStrMoney_ctor(void *obj);
void cStrMoney_Update(void *obj, float dt);
void *Logic_ctor(void *logic);
void Logic_dtor(void *logic);
void Logic_Reset(void *logic);
void Logic_Update(void *logic, float dt);
void cScheduledFuncStack_dtor(void *stack);

cDebrisSys *cDebrisSys_ctor(void *mem);
void cDebrisSys_dtor(cDebrisSys *sys);
void cDebrisSys_SetName(cDebrisSys *sys, const char *name);
/* the debris of the list at 0x0 (the free ones, the count cleared) and of the list at 0x4 (the dropped ones)
 * deleted; the names are older than the knowledge of the lists */
void cDebrisSys_FreeActive(cDebrisSys *sys);
void cDebrisSys_FreeIdle(cDebrisSys *sys);
void cDebrisSys_Update(cDebrisSys *sys, float dt);
/* World/ObjPool.cpp, the rest (src/game/World/ObjPool.c; more in fxmisc.h and missileentity.h) */
byte cDebris_SetFX(int type, cDebris *d);
byte cDebris_IsAlive(cDebris *d, cDebrisSys *sys);
void cDebrisSys_SetMaxIdle(cDebrisSys *sys, int n);
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

/* engine variables (Gvars) */
#define g_MaxRadObj GAME_GLOBAL_IN(float, GVar_Max_Rad_Obj, 16)		/* Max_Rad_Obj, the margin of the grid queries */
#define g_GridScaleX GAME_GLOBAL_IN(float, GVar_GridScaleX, 16)
#define g_GridScaleY GAME_GLOBAL_IN(float, GVar_GridScaleY, 16)
#define g_GridScaleZ GAME_GLOBAL_IN(float, GVar_GridScaleZ, 16)
#define g_SplitScreenGame GAME_GLOBAL_IN(int, GVar_SplitScreen, 16)		/* SplitScreen */
#define g_GameSkill GAME_GLOBAL_IN(int, GVar_GameSkill, 16)
#define g_GameSkillDef GAME_GLOBAL_IN(int, GVar_GameSkill, 20)
#define g_VehicleEditingMode GAME_GLOBAL_IN(int, GVar_VehicleEditingMode, 16)
#define g_MaxChasingShips GAME_GLOBAL_IN(int, GVar_MaxChasingShips, 16)
#define g_MaxChasingShipsDef GAME_GLOBAL_IN(int, GVar_MaxChasingShips, 20)
#define g_PitchScale GAME_GLOBAL_IN(float, GVar_S_PitchScale, 16)		/* S_PitchScale */
#define g_FreeSpaceMagnet GAME_GLOBAL_IN(int, GVar_FreeSpaceMagnet, 16)
#define g_FreeSpaceMagnetDef GAME_GLOBAL_IN(int, GVar_FreeSpaceMagnet, 20)
#define g_FXVehicleFloatingFactor GAME_GLOBAL_IN(float, GVar_FXVehicleFloatingFactor, 16)
#define g_FXVehicleFloatingFactorDef GAME_GLOBAL_IN(float, GVar_FXVehicleFloatingFactor, 20)
#define g_SceneMotionBlurValue GAME_GLOBAL_IN(float, GVar_R_SceneMotionBlurValue, 16)	/* R_SceneMotionBlurValue */
#define g_NotShootAI GAME_GLOBAL_IN(int, GVar_NotShootAI, 16)
#define g_NotShootAIDef GAME_GLOBAL_IN(int, GVar_NotShootAI, 20)
#define g_RemainTalkTime GAME_GLOBAL_IN(float, GVar_RemainTalkTime, 16)
#define g_PriorityWeapon (GAME_ARRAY_IN(cString, GVar_PriorityWeapon, 16))	/* PriorityWeapon, its default at +4 */
#define g_SplitScreenCamera GAME_GLOBAL_IN(int, GVar_R_SplitScreenCamera, 16)		/* R_SplitScreenCamera */
#define g_SplitScreenUpdate GAME_GLOBAL_IN(int, GVar_R_SplitScreenUpdate, 16)		/* R_SplitScreenUpdate */
extern char g_UniqueName[128];		/* 0x80 bytes, the result of cWorld_UniqueName */
/* script callbacks (cPy_Function of the GVAR_SCRIPT_FUNCTION variables) */
#define g_OnSleep (GAME_ARRAY_IN(cPy_Function, GVar_OnSleep, 16))
#define g_OnLifeItemTaken (GAME_ARRAY_IN(cPy_Function, GVar_OnLifeItemTaken, 16))
#define g_OnGeneralDamage (GAME_ARRAY_IN(cPy_Function, GVar_OnGeneralDamage, 16))
#define g_MainMissionEvent (GAME_ARRAY_IN(cPy_Function, GVar_MainMissionEvent, 16))
#define g_EscapeEvent (GAME_ARRAY_IN(cPy_Function, GVar_EscapeEvent, 16))
#define g_ChatEvent (GAME_ARRAY_IN(cPy_Function, GVar_ChatEvent, 16))
#define g_QChatEvent1 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent1, 16))
#define g_QChatEvent2 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent2, 16))
#define g_QChatEvent3 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent3, 16))
#define g_QChatEvent4 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent4, 16))
#define g_QChatEvent5 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent5, 16))
#define g_QChatEvent6 (GAME_ARRAY_IN(cPy_Function, GVar_QChatEvent6, 16))

/* World/World.cpp (the rest is in scrap.h, entity.h and game.h) */
float Vec3_DotViewDir(const vec3_t *v, float ax, float ay);
cEntity *Entity_FindInView(cEntity *self, float range, float *outDot, float *outDist, int mode, float minDot);
byte Team_IsFriend(int a, int b);
byte Team_IsEnemy(int a, int b);
cAmmoInfo *cAmmoInfo_ctor(cAmmoInfo *self);
cWeapInfo *cWeapInfo_ctor(cWeapInfo *self);
void cAmmoInfo_dtor(cAmmoInfo *self);
void cWeapInfo_dtor(cWeapInfo *self);
cAmmoInfo *cAmmoInfo_Delete(cAmmoInfo *self, uint32_t flags);
cWeapInfo *cWeapInfo_Delete(cWeapInfo *self, uint32_t flags);
cWorld *cWorld_ctor(cWorld *self);
void AmmoTable_Free(void *table);
void WeapTable_Free(void *table);
void cWorld_dtor(cWorld *self);
cWorld *cWorld_Destroy(cWorld *self, byte flags);
void cWorld_AddIgnore(cWorld *self, cEntity *e);
byte cWorld_IsIgnored(cWorld *self, cEntity *e);
byte cWorld_LineCallF(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	cPy_Function *func);
byte cWorld_LineCall(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	const char *func);
byte cWorld_LineFirst(cWorld *self, cEntity *exclude, uint32_t mask, const vec3_t *from, const vec3_t *to,
	float radius, cEntity **target);
void World_InputEvents(void);
void cWorld_Clear(cWorld *self);
void cWorld_DeleteLists(cWorld *self);
float cWorld_GetMinCamDist(cWorld *self, const vec3_t *pos);
void *cWorld_GetObjData(cWorld *self, const char *name);
char *cWorld_UniqueName(cWorld *self, const char *name);
void cWorld_ResetLogic(cWorld *self);
byte World_IsEasySingle(void);
byte World_IsHardSingle(void);
void cWorld_ShowDebrisInfo(cWorld *self, int x, int y);
void cWorld_UpdateListeners(cWorld *self, float dt);
void cWorld_Update(cWorld *self, float dt);
byte cWorld_SphereCallF(cWorld *self, uint32_t mask, const vec3_t *center, float radius,
	cPy_Function *func, int noWall);

#endif
