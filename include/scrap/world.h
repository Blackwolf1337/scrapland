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

#define CWORLD_VFTABLE 0x3cec84
#define CWORLD_SIZE 0x225c

/* the slots of the entity vtables the world calls; entity.h has the first ones by their script use */
typedef struct cWorldEntityVtbl {
	void(__thiscall *Update)(cEntity *self);			/* 0x00 every frame while flag_4c is set */
	void *vf01;
	void(__thiscall *UpdateOff)(cEntity *self);			/* 0x08 every frame otherwise */
	void(__thiscall *Delete)(cEntity *self, int flags);		/* 0x0c deleting destructor */
	void *vf04;
	byte(__thiscall *IsType)(cEntity *self, const char *type);	/* 0x14 */
	vec3_t *(__thiscall *GetPos)(cEntity *self, vec3_t *out);	/* 0x18 */
	void *vf07_08[2];
	/* the segment from -> *hit against the entity; on a hit *hit is moved to it */
	byte(__thiscall *TestLine)(cEntity *self, const vec3_t *from, vec3_t *hit);			/* 0x24 */
	byte(__thiscall *TestSphere)(cEntity *self, cEntity *exclude, const vec3_t *center, float radius, int arg); /* 0x28 */
	void *vf0b_15[11];
	void *(__thiscall *GetAI)(cEntity *self);			/* 0x58 */
	void *vf17_1a[4];
	void(__thiscall *SetTarget)(cEntity *self, const char *name);	/* 0x6c cameras */
	void *vf1c;
	vec3_t *(__thiscall *GetCenter)(cEntity *self, vec3_t *out);	/* 0x74 the vector at +0xa0 of an object */
	void *vf1e_21[4];
	vec3_t *(__thiscall *GetVel)(cEntity *self, vec3_t *out);	/* 0x88 the vector at +0xd4 of an object */
	void *vf23_25[3];
	void *(__thiscall *GetObjData)(cEntity *self);			/* 0x98 "Obj": the data at +0xb8 */
	void(__thiscall *GetView)(cEntity *self, vec3_t *pos, vec3_t *ang);	/* 0x9c eye position and angles */
} cWorldEntityVtbl;
SD_OFFSET(cWorldEntityVtbl, Delete, 0x0c);
SD_OFFSET(cWorldEntityVtbl, TestSphere, 0x28);
SD_OFFSET(cWorldEntityVtbl, GetAI, 0x58);
SD_OFFSET(cWorldEntityVtbl, SetTarget, 0x6c);
SD_OFFSET(cWorldEntityVtbl, GetCenter, 0x74);
SD_OFFSET(cWorldEntityVtbl, GetVel, 0x88);
SD_OFFSET(cWorldEntityVtbl, GetView, 0x9c);

static inline const cWorldEntityVtbl *WEnt_Vtbl(cEntity *e) {
	return (const cWorldEntityVtbl *)e->vftable;
}

/* the fields of entities the world reads */
#define Entity_Mask(e) (*(uint32_t *)((byte *)(e) + 0x48))		/* the entity lists it is in, a bit each */
#define Entity_Team(e) (*(int *)((byte *)(e) + 0x388))		/* characters: 3 neutral, 4 against all */
#define CamEntity_Cam(e) ((vec3_t *)((byte *)(e) + 0x7c))		/* the camera of a "Cam", its position first */
#define CamEntity_Orient(e) ((vec3_t *)((byte *)(e) + 0x88))
#define CamEntity_Slot(e) (*(int *)((byte *)(e) + 0xb8))		/* the view it shows, -1 for none */
#define CamEntity_NoFollow(e) (*(byte *)((byte *)(e) + 0xbc))

/* World/MapFloors.cpp: the floors of the 2D map (cWorld floor) */
#define MAX_2D_MAPS 16
#define MAX_2D_LINKS 8
typedef struct MapLevel {
	float y;			/* 0x000 the height where the floor starts, the first one -FLT_MAX */
	int nUp;			/* 0x004 */
	vec3_t up[MAX_2D_LINKS];	/* 0x008 the ways to the next floor up, y + 1 */
	float upAng[MAX_2D_LINKS];	/* 0x068 the angle y of the dummy */
	int nDown;			/* 0x088 */
	vec3_t down[MAX_2D_LINKS];	/* 0x08c y - 1 */
	float downAng[MAX_2D_LINKS];	/* 0x0ec */
	char name[0x80];		/* 0x10c behind DM_2DMapSwitch */
} MapLevel;
SD_OFFSET(MapLevel, nDown, 0x88);
SD_OFFSET(MapLevel, name, 0x10c);
SD_SIZE(MapLevel, 0x18c);

typedef struct MapFloors {
	int count;			/* 0x000 */
	MapLevel levels[MAX_2D_MAPS];	/* 0x004 sorted by y */
} MapFloors;
SD_SIZE(MapFloors, 0x18c4);

void *__thiscall Floor_ctor(void *floor);
void __thiscall Floor_dtor(void *floor);
float __thiscall Floor_Dist(void *floor, vec3_t a, vec3_t b);
int __thiscall MapLevel_NearestDown(const MapLevel *self, const vec3_t *p);
int __thiscall MapLevel_NearestUp(const MapLevel *self, const vec3_t *p);
int MapLevel_CompareY(const void *a, const void *b);

/* the objects embedded in cWorld, only known by their methods */
void *__thiscall Elements_ctor(void *elements);
void __thiscall Elements_dtor(void *elements);
void __thiscall Elements_Update(void *elements, float dt);
/* World/MapElements.cpp: the first element of the model name within maxDist of pos, NULL */
struct cMapElement *__thiscall Elements_Find(void *elements, const char *name, vec3_t pos, float maxDist);
/* an element of a saved game: name, position, radius of the search, on or off */
void Elements_Load(cAbstractFile *f);
void *__thiscall Alarm_ctor(void *alarm);
void __thiscall Alarm_Update(void *alarm, float dt);
void *__thiscall cStrMoney_ctor(void *obj);
void __thiscall cStrMoney_Update(void *obj, float dt);
void *__thiscall Logic_ctor(void *logic);
void __thiscall Logic_dtor(void *logic);
void __thiscall Logic_Reset(void *logic);
void __thiscall Logic_Update(void *logic, float dt);
void __thiscall cScheduledFuncStack_dtor(void *stack);

cDebrisSys *__thiscall cDebrisSys_ctor(void *mem);
void __thiscall cDebrisSys_dtor(cDebrisSys *sys);
void __thiscall cDebrisSys_SetName(cDebrisSys *sys, const char *name);
/* the debris of the list at 0x0 (the free ones, the count cleared) and of the list at 0x4 (the dropped ones)
 * deleted; the names are older than the knowledge of the lists */
void __thiscall cDebrisSys_FreeActive(cDebrisSys *sys);
void __thiscall cDebrisSys_FreeIdle(cDebrisSys *sys);
void __thiscall cDebrisSys_Update(cDebrisSys *sys, float dt);
/* World/ObjPool.cpp, the rest (src/game/World/ObjPool.c; more in fxmisc.h and missileentity.h) */
byte cDebris_SetFX(int type, cDebris *d);
byte __thiscall cDebris_IsAlive(cDebris *d, cDebrisSys *sys);
void __thiscall cDebrisSys_SetMaxIdle(cDebrisSys *sys, int n);
/* the debug text of the object pool (O_Show) at x, y; *line goes on to the next line */
void __thiscall cDebrisSys_ShowInfo(cDebrisSys *sys, int x, int y, float *line);

/* the entity classes of cWorld_CreateEntity: constructors on the memory from operator_new, return it */
cEntity *__thiscall cEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cCamPathEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cWalkCharEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cVehicleEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cCamEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cFollowCamEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cMissileEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cItemEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cWithLifeEntity_ctor(void *mem, const char *name, float x, float y, float z);
cEntity *__thiscall cObjEntity_ctor(void *mem, const char *name, float x, float y, float z);
/* the entity after e in the world list or NULL */
cEntity *__fastcall cEntity_Next(cEntity *e);
/* the entity a follow camera follows */
cEntity *__fastcall cCamEntity_GetTarget(cEntity *cam);
/* the entity the camera follows (its +0xf0 points back), and the angles of the camera (x and y) */
void __thiscall cCamEntity_SetTarget(cEntity *cam, cEntity *target);
void __thiscall cCamEntity_SetAng(cEntity *cam, vec3_t ang);

/* an entity list of g_pWorld->lists (0x44 bytes: name, two functions, the entities at 0x30) */
void *__thiscall cEntityList_ctor(void *mem, const char *name);
void __thiscall EntityDList_dtor(void *list);

/* the engine3d side: the camera of the view (NULL for none) and the box of the level */
/* frees the model list at rva 0x4479a8 */
void GameModelList_Free(void);
void Sound_Update(float dt);
void Sound_UpdatePaused(void);
/* the action went over 0.5 for the player in this frame */
/* RemainTalkTime > 0: a talk text is shown */
int Talk_IsShown(void);
void __thiscall cPy_Function_Clear(cPy_Function *self);
void Str_TrimBlanks(char *s);

/* engine variables (Gvars) */
#define g_MaxRadObj GAME_DATA(float, 0x467758)		/* Max_Rad_Obj, the margin of the grid queries */
#define g_GridScaleX GAME_DATA(float, 0x467704)
#define g_GridScaleY GAME_DATA(float, 0x4675f0)
#define g_GridScaleZ GAME_DATA(float, 0x46762c)
#define g_SplitScreenGame GAME_DATA(int, 0x467584)		/* SplitScreen */
#define g_GameSkill GAME_DATA(int, 0x467674)
#define g_GameSkillDef GAME_DATA(int, 0x467678)
#define g_VehicleEditingMode GAME_DATA(int, 0x447ea8)
#define g_MaxChasingShips GAME_DATA(int, 0x44dc04)
#define g_MaxChasingShipsDef GAME_DATA(int, 0x44dc08)
#define g_PitchScale GAME_DATA(float, 0x466d48)		/* S_PitchScale */
#define g_FreeSpaceMagnet GAME_DATA(int, 0x447f08)
#define g_FreeSpaceMagnetDef GAME_DATA(int, 0x447f0c)
#define g_FXVehicleFloatingFactor GAME_DATA(float, 0x447ff8)
#define g_FXVehicleFloatingFactorDef GAME_DATA(float, 0x447ffc)
#define g_SceneMotionBlurValue GAME_DATA(float, 0x4a8c44)	/* R_SceneMotionBlurValue */
#define g_NotShootAI GAME_DATA(int, 0x4441a4)
#define g_NotShootAIDef GAME_DATA(int, 0x4441a8)
#define g_RemainTalkTime GAME_DATA(float, 0x462148)
#define g_PriorityWeapon ((cString *)Game_Ptr(0x467448))	/* PriorityWeapon, its default at +4 */
#define g_SplitScreenCamera GAME_DATA(int, 0x4a8a8c)		/* R_SplitScreenCamera */
#define g_SplitScreenUpdate GAME_DATA(int, 0x4a8c74)		/* R_SplitScreenUpdate */
#define g_UniqueName ((char *)Game_Ptr(0x4678f8))		/* 0x80 bytes, the result of cWorld_UniqueName */
/* script callbacks (cPy_Function of the GVAR_SCRIPT_FUNCTION variables) */
#define g_OnSleep ((cPy_Function *)Game_Ptr(0x446e74))
#define g_OnLifeItemTaken ((cPy_Function *)Game_Ptr(0x447ae0))
#define g_OnGeneralDamage ((cPy_Function *)Game_Ptr(0x44ab10))
#define g_MainMissionEvent ((cPy_Function *)Game_Ptr(0x467560))
#define g_EscapeEvent ((cPy_Function *)Game_Ptr(0x4674b8))
#define g_ChatEvent ((cPy_Function *)Game_Ptr(0x467824))
#define g_QChatEvent1 ((cPy_Function *)Game_Ptr(0x467734))
#define g_QChatEvent2 ((cPy_Function *)Game_Ptr(0x467494))
#define g_QChatEvent3 ((cPy_Function *)Game_Ptr(0x46750c))
#define g_QChatEvent4 ((cPy_Function *)Game_Ptr(0x467800))
#define g_QChatEvent5 ((cPy_Function *)Game_Ptr(0x467608))
#define g_QChatEvent6 ((cPy_Function *)Game_Ptr(0x4675b4))

/* World/World.cpp (the rest is in scrap.h, entity.h and game.h) */
float Vec3_DotViewDir(const vec3_t *v, float ax, float ay);
cEntity *Entity_FindInView(cEntity *self, float range, float *outDot, float *outDist, int mode, float minDot);
byte Team_IsFriend(int a, int b);
byte Team_IsEnemy(int a, int b);
cAmmoInfo *__thiscall cAmmoInfo_ctor(cAmmoInfo *self);
cWeapInfo *__thiscall cWeapInfo_ctor(cWeapInfo *self);
cWorld *__thiscall cWorld_ctor(cWorld *self);
void __fastcall AmmoTable_Free(void *table);
void __fastcall WeapTable_Free(void *table);
void __fastcall cWorld_dtor(cWorld *self);
cWorld *__thiscall cWorld_Destroy(cWorld *self, byte flags);
void __thiscall cWorld_AddIgnore(cWorld *self, cEntity *e);
byte __thiscall cWorld_IsIgnored(cWorld *self, cEntity *e);
byte __thiscall cWorld_LineCallF(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	cPy_Function *func);
byte __thiscall cWorld_LineCall(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	const char *func);
byte __thiscall cWorld_LineFirst(cWorld *self, cEntity *exclude, uint32_t mask, const vec3_t *from, const vec3_t *to,
	float radius, cEntity **target);
void World_InputEvents(void);
void __fastcall cWorld_Clear(cWorld *self);
void __fastcall cWorld_DeleteLists(cWorld *self);
float __thiscall cWorld_GetMinCamDist(cWorld *self, const vec3_t *pos);
void *__thiscall cWorld_GetObjData(cWorld *self, const char *name);
char *__thiscall cWorld_UniqueName(cWorld *self, const char *name);
void __fastcall cWorld_ResetLogic(cWorld *self);
byte World_IsEasySingle(void);
byte World_IsHardSingle(void);
void __thiscall cWorld_ShowDebrisInfo(cWorld *self, int x, int y);
void __thiscall cWorld_UpdateListeners(cWorld *self, float dt);
void __thiscall cWorld_Update(cWorld *self, float dt);
byte __thiscall cWorld_SphereCallF(cWorld *self, uint32_t mask, const vec3_t *center, float radius,
	cPy_Function *func, int noWall);

#endif
