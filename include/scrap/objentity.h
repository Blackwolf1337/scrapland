/*
 * cObjEntity ("Obj"), the base class of every object of the levels with a model (items, doors, missiles, the entities
 * with life, vehicles and characters): src/game/Entities/ObjEntity.c, the original code sits between ModMove.cpp and
 * the auto aim of the vehicles and has no CHECK of its own. The object has angles, velocities, its own timer
 * ("TimeSpeed"), a slot controller that fires every 0.05 s, the sounds (cSndInterface) and the two movers of
 * include/scrap/modmove.h. Between two slots the model is drawn at a position and angles interpolated from the state
 * of the last slot (renderPos, renderAng). In a network game the cNetController holds the state the server sent.
 */
#ifndef SCRAP_OBJENTITY_H
#define SCRAP_OBJENTITY_H

#include "scrap/gvar.h"
#include "scrap/modmove.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"
#include "scrap/slotcontroller.h"
#include "scrap/sound.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define TYEN_OBJECT "Obj"
#define COBJENTITY_VFTABLE GAME_VFTABLE(cObjEntity_vftable)
#define CNETCONTROLLER_VFTABLE GAME_VFTABLE(cNetController_vftable)
#define COBJENTITY_SIZE 0x228

/* the flags of the network state */
#define NETC_FULL 0x01			/* full precision (floats), else packed */
#define NETC_POS 0x02
#define NETC_ANG 0x04			/* x and y */
#define NETC_ANGZ 0x08
#define NETC_VEL 0x10
#define NETC_ROTVEL 0x20		/* x and y */
#define NETC_ROTVELZ 0x40

typedef struct cNetController cNetController;
typedef struct cNetControllerVtbl {
	void(*Reset)(cNetController *self);					/* 0x00 */
	void(*Read)(cNetController *self, void *msg);			/* 0x04 */
	void(*Write)(cNetController *self, int kind, cEntity *e, void *msg, int full);	/* 0x08 */
	byte(*Changed)(cNetController *self, byte flags, cEntity *e);	/* 0x0c */
	void(*Apply)(cNetController *self, cEntity *e, float dt, byte move);	/* 0x10 */
} cNetControllerVtbl;

struct cNetController {
	const cNetControllerVtbl *vftable;	/* 0x00 */
	float age;			/* 0x04 0.5 when a state came, then counted down by Apply */
	vec3_t pos;			/* 0x08 */
	vec3_t ang;			/* 0x14 */
	vec3_t vel;			/* 0x20 */
	vec3_t rotVel;			/* 0x2c */
	byte flags;			/* 0x38 NETC_ of the last state */
	byte pad_39[3];
	/* the sends left of each part after a change (pos, ang, ang z, vel, rotVel, rotVel z), 5 at the start */
	int left[6];			/* 0x3c */
	byte pending[6];		/* 0x54 1 after Reset: the part may count down once */
	byte pad_5a[2];
};
SD_OFFSET(cNetController, flags, 0x38);
SD_OFFSET(cNetController, left, 0x3c);
SD_OFFSET(cNetController, pending, 0x54);
SD_SIZE(cNetController, 0x5c);

typedef struct cObjEntity {
	cEntity base;			/* 0x000 */
	vec3_t slotAng;			/* 0x07c the state of the last slot, the render lerp starts there */
	vec3_t slotPos;			/* 0x088 */
	vec3_t renderAng;		/* 0x094 of the model */
	vec3_t renderPos;		/* 0x0a0 */
	vec3_t ang;			/* 0x0ac */
	CGameModel model;		/* 0x0b8 */
	vec3_t vel;			/* 0x0d4 */
	vec3_t rotVel;			/* 0x0e0 */
	byte netRes;			/* 0x0ec "NetRes": the index of the network resource */
	byte pad_ed[3];
	cEntity *cam;			/* 0x0f0 the follow camera of the object ("CamName") */
	cNetController *net;		/* 0x0f4 only in a network game */
	vec3_t camOff;			/* 0x0f8 "CamTgt" (0, 50, -50) */
	float lastTime;			/* 0x104 the world time of the last timer step, -1 */
	int radarShown;			/* 0x108 the RadarShow radarTime belongs to */
	float radarTime;		/* 0x10c */
	cTimer timer;			/* 0x110 */
	byte realTime;			/* 0x138 "UseRealTime": the world speed does not scale the timer */
	byte pad_139[3];
	int radarShow;			/* 0x13c "RadarShow" */
	cSlotController *slot;		/* 0x140 every 0.05 s */
	cSndInterface snd;		/* 0x144 */
	cPy_Function onObjSlot;		/* 0x160 "OnObjSlot" (entity, interval) */
	cObjMove move;			/* 0x174 */
	cModMove mod;			/* 0x1e4 */
	struct cEmbeddedFX *fx;		/* 0x224 */
} cObjEntity;
SD_OFFSET(cObjEntity, slotAng, 0x7c);
SD_OFFSET(cObjEntity, renderPos, 0xa0);
SD_OFFSET(cObjEntity, ang, 0xac);
SD_OFFSET(cObjEntity, model, 0xb8);
SD_OFFSET(cObjEntity, vel, 0xd4);
SD_OFFSET(cObjEntity, cam, 0xf0);
SD_OFFSET(cObjEntity, camOff, 0xf8);
SD_OFFSET(cObjEntity, timer, 0x110);
SD_OFFSET(cObjEntity, slot, 0x140);
SD_OFFSET(cObjEntity, snd, 0x144);
SD_OFFSET(cObjEntity, onObjSlot, 0x160);
SD_OFFSET(cObjEntity, move, 0x174);
SD_OFFSET(cObjEntity, mod, 0x1e4);
SD_OFFSET(cObjEntity, fx, 0x224);
SD_SIZE(cObjEntity, COBJENTITY_SIZE);

/* the sound interface of an object (the weapon and effect sounds by index) */

/* the slots of cObjEntity::vftable after the 23 of cEntity */
typedef struct cObjEntityVtbl {
	cEntityVtbl base;
	void(*Tick)(cEntity *self, float dt);					/* 0x5c vf23 per slot */
	void(*UpdateModel)(cEntity *self);						/* 0x60 */
	int(*GetNodeSlot)(cEntity *self);						/* 0x64 */
	vec3_t *(*GetRenderPos)(cEntity *self, vec3_t *out);			/* 0x68 */
	vec3_t *(*GetRenderAng)(cEntity *self, vec3_t *out);			/* 0x6c */
	vec3_t *(*GetSndAng)(cEntity *self, vec3_t *out);				/* 0x70 */
	vec3_t *(*GetViewPos)(cEntity *self, vec3_t *out);				/* 0x74 */
	vec3_t *(*GetViewAng)(cEntity *self, vec3_t *out);				/* 0x78 */
	vec3_t *(*GetSndPos)(cEntity *self, vec3_t *out);				/* 0x7c */
	vec3_t *(*GetAng)(cEntity *self, vec3_t *out);				/* 0x80 */
	void(*SetAng)(cEntity *self, const vec3_t *ang);				/* 0x84 */
	vec3_t *(*GetVel)(cEntity *self, vec3_t *out);				/* 0x88 */
	void(*SetVel)(cEntity *self, const vec3_t *vel);				/* 0x8c */
	vec3_t *(*GetRotVel)(cEntity *self, vec3_t *out);				/* 0x90 */
	void(*SetRotVel)(cEntity *self, const vec3_t *v);				/* 0x94 */
	CGameModel *(*GetModel)(cEntity *self);					/* 0x98 */
	void(*GetCamera)(cEntity *self, vec3_t *pos, vec3_t *ang);			/* 0x9c */
	float(*GetRadius)(cEntity *self);						/* 0xa0 */
	/* the network copy of the object (vehicles, missiles, items; nothing in cObjEntity) */
	void(*NetWrite)(cEntity *self, float dist2, void *msg, int full);		/* 0xa4 vf41 */
	byte(*NetNeedsSend)(cEntity *self, float dist2);				/* 0xa8 vf42 */
	void(*NetRead)(cEntity *self, void *msg, int isNew);			/* 0xac vf43 */
	void(*SetEffect)(cEntity *self, uint32_t fx);				/* 0xb0 */
	void(*SetEffectParam)(cEntity *self, int i, float v);			/* 0xb4 */
	void(*Render)(cEntity *self);						/* 0xb8 */
} cObjEntityVtbl;
SD_OFFSET(cObjEntityVtbl, Tick, 0x5c);
SD_OFFSET(cObjEntityVtbl, GetAng, 0x80);
SD_OFFSET(cObjEntityVtbl, GetModel, 0x98);
SD_OFFSET(cObjEntityVtbl, SetEffect, 0xb0);
SD_OFFSET(cObjEntityVtbl, Render, 0xb8);

static inline cObjEntity *Obj(const void *e) {
	return (cObjEntity *)e;
}

static inline const cObjEntityVtbl *Obj_Vtbl(const cEntity *e) {
	return (const cObjEntityVtbl *)e->vftable;
}

/* 1 while a missile updates: cObjEntity_Update then moves the render state on by the velocity of one slot */
extern cGVar GVar_ActiveExtrapolation;
#define g_MissileUpdating (GVar_ActiveExtrapolation.value.i)
extern cGVar GVar_MssnRadarBlinkTime;
#define g_RadarShowTime (GVar_MssnRadarBlinkTime.value.f)	/* radarTime after a change of RadarShow */

/* the cObjEntity functions take a cEntity like the callers of the derived classes */
void cObjEntity_dtor(cEntity *self);
cEntity *cObjEntity_Delete(cEntity *self, uint32_t flags);
void cObjEntity_Update(cEntity *self);
void cObjEntity_UpdateOff(cEntity *self);
const char *cObjEntity_GetClassName(cEntity *self);
int cObjEntity_IsType(cEntity *self, const char *type);
vec3_t *cObjEntity_GetPos(cEntity *self, vec3_t *out);
void cObjEntity_SetPos(cEntity *self, const vec3_t *pos);
void cObjEntity_SetActive(cEntity *self, byte on);
void cObjEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag);
void cObjEntity_GetTargetAngle(cEntity *self, float *ang, float dist, cEntity **target);
void cObjEntity_SetEmbeddedFX(cEntity *self, const char *fxType);
byte cObjEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte cObjEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte cObjEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte cObjEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void cObjEntity_Tick(cEntity *self, float dt);
void cObjEntity_UpdateModel(cEntity *self);
int cObjEntity_GetNodeSlot(cEntity *self);
vec3_t *cObjEntity_GetRenderPos(cEntity *self, vec3_t *out);
vec3_t *cObjEntity_GetRenderAng(cEntity *self, vec3_t *out);
vec3_t *cObjEntity_GetSndAng(cEntity *self, vec3_t *out);
vec3_t *cObjEntity_GetSndPos(cEntity *self, vec3_t *out);
vec3_t *cObjEntity_GetAng(cEntity *self, vec3_t *out);
void cObjEntity_SetAng(cEntity *self, const vec3_t *ang);
vec3_t *cObjEntity_GetVel(cEntity *self, vec3_t *out);
void cObjEntity_SetVel(cEntity *self, const vec3_t *vel);
vec3_t *cObjEntity_GetRotVel(cEntity *self, vec3_t *out);
void cObjEntity_SetRotVel(cEntity *self, const vec3_t *v);
CGameModel *cObjEntity_GetModel(cEntity *self);
void cObjEntity_GetCamera(cEntity *self, vec3_t *pos, vec3_t *ang);
float cObjEntity_GetRadius(cEntity *self);
void cObjEntity_SetEffect(cEntity *self, uint32_t fx);
void cObjEntity_SetEffectParam(cEntity *self, int i, float v);
void cObjEntity_Render(cEntity *self);
/* the entity timer from the world timer */
void cObjEntity_UpdateTimer(cEntity *self);
/* a new slot controller (the old one is not freed) */
void cObjEntity_NewSlot(cEntity *self);
/* the RadarShow time of Tick on its own */
void cObjEntity_RadarTick(cEntity *self, float dt);
void cObjEntity_ReleaseFX(cEntity *self);
/* the collision of the model ("MapCollision"); without a model only a warning */
void cObjEntity_Switch(cEntity *self, byte on);
byte cObjEntity_GetCollision(cEntity *self);
/* "NetRes": the index of the resource of that name, kept when there is none */
void cEntity_SetNetResource(cEntity *e, const char *name);

cNetController *cNetController_ctor(cNetController *self);
void cNetController_Reset(cNetController *self);
void cNetController_Read(cNetController *self, void *msg);
/* flags (only the changed parts unless force) and the packed (or full) state of e into msg */
void cNetController_Write(cNetController *self, byte flags, cEntity *e, void *msg, byte force);
/* the parts of flags that changed enough to be sent (the others count down) */
byte cNetController_Changed(cNetController *self, byte flags, cEntity *e);
/* the received state onto e; move: the client moves e by its velocities while no new state came */
void cNetController_Apply(cNetController *self, cEntity *e, float dt, byte move);

#endif
