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
#define COBJENTITY_SIZE sizeof(cObjEntity)
#define OBJ_MIN_DT 1e-6f			/* no update for a smaller step */

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
	void(*Reset)(cNetController *self);
	void(*Read)(cNetController *self, void *msg);
	void(*Write)(cNetController *self, int kind, cEntity *e, void *msg, int full);
	byte(*Changed)(cNetController *self, byte flags, cEntity *e);
	void(*Apply)(cNetController *self, cEntity *e, float dt, byte move);
} cNetControllerVtbl;

/* calls through the slots of cNetControllerVtbl */
static inline void NetCtl_Apply(void *self, cEntity *e, float dt, byte move) {
	VT(cNetControllerVtbl, self)->Apply(self, e, dt, move);
}

static inline byte NetCtl_Changed(void *self, byte flags, cEntity *e) {
	return VT(cNetControllerVtbl, self)->Changed(self, flags, e);
}

static inline void NetCtl_Read(void *self, void *msg) {
	VT(cNetControllerVtbl, self)->Read(self, msg);
}

static inline void NetCtl_Reset(void *self) {
	VT(cNetControllerVtbl, self)->Reset(self);
}

static inline void NetCtl_Write(void *self, int kind, cEntity *e, void *msg, int full) {
	VT(cNetControllerVtbl, self)->Write(self, kind, e, msg, full);
}

struct cNetController {
	const cNetControllerVtbl *vftable;
	float age;			/* 0.5 when a state came, then counted down by Apply */
	vec3_t pos;
	vec3_t ang;
	vec3_t vel;
	vec3_t rotVel;
	byte flags;			/* NETC_ of the last state */
	byte pad_39[3];
	/* the sends left of each part after a change (pos, ang, ang z, vel, rotVel, rotVel z), 5 at the start */
	int left[6];
	byte pending[6];		/* 1 after Reset: the part may count down once */
	byte pad_5a[2];
};

typedef struct cObjEntity {
	cEntity base;
	vec3_t slotAng;			/* the state of the last slot, the render lerp starts there */
	vec3_t slotPos;
	vec3_t renderAng;		/* of the model */
	vec3_t renderPos;
	vec3_t ang;
	CGameModel model;
	vec3_t vel;
	vec3_t rotVel;
	byte netRes;			/* "NetRes": the index of the network resource */
	byte proxy;			/* a watcher's copy of a replicated entity: no slot ticks (include/scrap/replicate.h) */
	byte netStep;			/* a player's entity of the network: its actions and slot ticks run by tick
					   (NetPl_Step, include/scrap/play.h), not by the frame */
	byte pad_ef;
	cEntity *cam;			/* the follow camera of the object ("CamName") */
	cNetController *net;		/* only in a network game */
	vec3_t camOff;			/* "CamTgt" (0, 50, -50) */
	float lastTime;			/* the world time of the last timer step, -1 */
	int radarShown;			/* the RadarShow radarTime belongs to */
	float radarTime;
	cTimer timer;
	byte realTime;			/* "UseRealTime": the world speed does not scale the timer */
	byte pad_139[3];
	int radarShow;			/* "RadarShow" */
	cSlotController *slot;		/* every 0.05 s */
	cSndInterface snd;
	cPy_Function onObjSlot;		/* "OnObjSlot" (entity, interval) */
	cObjMove move;
	cModMove mod;
	struct cEmbeddedFX *fx;
} cObjEntity;

/* the sound interface of an object (the weapon and effect sounds by index) */

/* the slots of cObjEntity::vftable after the 23 of cEntity */
typedef struct cObjEntityVtbl {
	cEntityVtbl base;
	void(*Tick)(cEntity *self, float dt);					/* per slot */
	void(*UpdateModel)(cEntity *self);
	int(*GetNodeSlot)(cEntity *self);
	vec3_t *(*GetRenderPos)(cEntity *self, vec3_t *out);
	vec3_t *(*GetRenderAng)(cEntity *self, vec3_t *out);
	vec3_t *(*GetSndAng)(cEntity *self, vec3_t *out);
	vec3_t *(*GetViewPos)(cEntity *self, vec3_t *out);
	vec3_t *(*GetViewAng)(cEntity *self, vec3_t *out);
	vec3_t *(*GetSndPos)(cEntity *self, vec3_t *out);
	vec3_t *(*GetAng)(cEntity *self, vec3_t *out);
	void(*SetAng)(cEntity *self, const vec3_t *ang);
	vec3_t *(*GetVel)(cEntity *self, vec3_t *out);
	void(*SetVel)(cEntity *self, const vec3_t *vel);
	vec3_t *(*GetRotVel)(cEntity *self, vec3_t *out);
	void(*SetRotVel)(cEntity *self, const vec3_t *v);
	CGameModel *(*GetModel)(cEntity *self);
	void(*GetCamera)(cEntity *self, vec3_t *pos, vec3_t *ang);
	float(*GetRadius)(cEntity *self);
	/* the network copy of the object (vehicles, missiles, items; nothing in cObjEntity) */
	void(*NetWrite)(cEntity *self, float dist2, void *msg, int full);
	byte(*NetNeedsSend)(cEntity *self, float dist2);
	void(*NetRead)(cEntity *self, void *msg, int isNew);
	void(*SetEffect)(cEntity *self, uint32_t fx);
	void(*SetEffectParam)(cEntity *self, int i, float v);
	void(*Render)(cEntity *self);
} cObjEntityVtbl;

/* calls through the slots of cObjEntityVtbl */
static inline vec3_t *Obj_GetAng(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetAng(self, out);
}

static inline void Obj_GetCamera(void *self, vec3_t *pos, vec3_t *ang) {
	VT(cObjEntityVtbl, self)->GetCamera(self, pos, ang);
}

static inline CGameModel *Obj_GetModel(void *self) {
	return VT(cObjEntityVtbl, self)->GetModel(self);
}

static inline int Obj_GetNodeSlot(void *self) {
	return VT(cObjEntityVtbl, self)->GetNodeSlot(self);
}

static inline float Obj_GetRadius(void *self) {
	return VT(cObjEntityVtbl, self)->GetRadius(self);
}

static inline vec3_t *Obj_GetRenderAng(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetRenderAng(self, out);
}

static inline vec3_t *Obj_GetRenderPos(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetRenderPos(self, out);
}

static inline vec3_t *Obj_GetSndAng(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetSndAng(self, out);
}

static inline vec3_t *Obj_GetSndPos(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetSndPos(self, out);
}

static inline vec3_t *Obj_GetVel(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetVel(self, out);
}

static inline vec3_t *Obj_GetViewAng(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetViewAng(self, out);
}

static inline vec3_t *Obj_GetViewPos(void *self, vec3_t *out) {
	return VT(cObjEntityVtbl, self)->GetViewPos(self, out);
}

static inline byte Obj_NetNeedsSend(void *self, float dist2) {
	return VT(cObjEntityVtbl, self)->NetNeedsSend(self, dist2);
}

static inline void Obj_NetRead(void *self, void *msg, int isNew) {
	VT(cObjEntityVtbl, self)->NetRead(self, msg, isNew);
}

static inline void Obj_NetWrite(void *self, float dist2, void *msg, int full) {
	VT(cObjEntityVtbl, self)->NetWrite(self, dist2, msg, full);
}

static inline void Obj_Render(void *self) {
	VT(cObjEntityVtbl, self)->Render(self);
}

static inline void Obj_SetAng(void *self, const vec3_t *ang) {
	VT(cObjEntityVtbl, self)->SetAng(self, ang);
}

static inline void Obj_SetEffect(void *self, uint32_t fx) {
	VT(cObjEntityVtbl, self)->SetEffect(self, fx);
}

static inline void Obj_SetEffectParam(void *self, int i, float v) {
	VT(cObjEntityVtbl, self)->SetEffectParam(self, i, v);
}

static inline void Obj_SetVel(void *self, const vec3_t *vel) {
	VT(cObjEntityVtbl, self)->SetVel(self, vel);
}

static inline void Obj_Tick(void *self, float dt) {
	VT(cObjEntityVtbl, self)->Tick(self, dt);
}

static inline void Obj_UpdateModel(void *self) {
	VT(cObjEntityVtbl, self)->UpdateModel(self);
}

static inline cObjEntity *Obj(const void *e) {
	return (cObjEntity *)e;
}

/* 1 while a missile updates: cObjEntity_Update then moves the render state on by the velocity of one slot */
extern cGVar GVar_ActiveExtrapolation;
#define g_MissileUpdating (GVar_ActiveExtrapolation.value.i)

/* the cObjEntity functions take a cEntity like the callers of the derived classes */
void cObjEntity_dtor(cEntity *self);
void cObjEntity_Update(cEntity *self);
void cObjEntity_UpdateOff(cEntity *self);
byte cObjEntity_IsType(cEntity *self, const char *type);
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
void cObjEntity_SetEffect(cEntity *self, uint32_t fx);
void cObjEntity_SetEffectParam(cEntity *self, int i, float v);
void cObjEntity_Render(cEntity *self);
/* the entity timer from the world timer */
void cObjEntity_UpdateTimer(cEntity *self);
/* the collision of the model ("MapCollision"); without a model only a warning */
void cObjEntity_Switch(cEntity *self, byte on);
/* "NetRes": the index of the resource of that name, kept when there is none */
void cEntity_SetNetResource(cEntity *e, const char *name);

cNetController *cNetController_ctor(cNetController *self);
void cNetController_Reset(cNetController *self);
void cNetController_Read(cNetController *self, void *msg);
/* flags (only the changed parts unless force) and the packed (or full) state of e into msg */
void cNetController_Write(cNetController *self, int flags, cEntity *e, void *m, int force);
/* the parts of flags that changed enough to be sent (the others count down) */
byte cNetController_Changed(cNetController *self, byte flags, cEntity *e);
/* the received state onto e; move: the client moves e by its velocities while no new state came */
void cNetController_Apply(cNetController *self, cEntity *e, float dt, byte move);

#endif
