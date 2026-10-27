/*
 * cObjEntity ("Obj") and its network state cNetController, see include/scrap/objentity.h.
 */
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/fxmisc.h"
#include "scrap/net.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"

#define OBJ_CLASSNAME ((const char *)Game_Ptr(0x3acc50))	/* "Obj" */
#define EXE_EMPTY ((char *)Game_Ptr(0x3a3228))			/* "" of the exe */
#define OBJ_ONENDMOVE_DESC ((char *)Game_Ptr(0x3b3814))		/* "<ObjMove.OnEnd> function" */
#define OBJ_ONOBJSLOT_DESC ((char *)Game_Ptr(0x3b384c))		/* "<OnObjSlot> function" */

#define OBJ_MIN_DT 1e-6f			/* rva 0x3b0830: no update for a smaller step */
#define OBJ_SLOT_INTERVAL 0.05f
#define OBJ_SLOT_JUMP2 9000000.0f		/* rva 0x3b38f8: a jump of more than 3000 since the slot */

/* the smoothing of the position of a network client: no smoothing nearer than min or farther than max */
#define g_NetSmoothMin GAME_DATA(float, 0x4479f0)
#define g_NetSmoothMax GAME_DATA(float, 0x4479d8)
#define g_NetSmoothFactor GAME_DATA(float, 0x447a38)
/* the origin of the packed positions of the network messages (32 units a step) */
#define g_NetOrigin GAME_DATA(vec3_t, 0x463ef4)

#define NET_POS_STEP 32.0f			/* rva 0x3b1f74 */
#define NET_ANG_SHORT 10185.916357881302	/* rva 0x3b38d8, 32000 / pi */
#define NET_ANG_BYTE 40.42535554534142		/* rva 0x3b38c8, 127 / pi */
#define NET_ROT_SHORT 6.702064327658225		/* rva 0x3b38b8 */
#define NET_ROT_BYTE 0.10471975511965977	/* rva 0x3b38b0 */
#define NET_POS_SCALE 0.03125f			/* rva 0x3b38ac */
#define NET_POS_MAX 32640.0f			/* rva 0x3b38f4 */
#define NET_VEL_MAX 30000.0f			/* rva 0x3b38f0 */
#define NET_ROT_SHORT_MAX 214466.0584850632	/* rva 0x3b38e8, 32000 * NET_ROT_SHORT */
#define NET_ROT_BYTE_MAX 13.29940890019679	/* rva 0x3b38c0, 127 * NET_ROT_BYTE */

/* the slots of the follow camera of an object and of the AI the object calls */
typedef struct cObjCamVtbl {
	void *vf00_24[25];
	void(__thiscall *Lerp)(cEntity *cam, float f);			/* 0x64 */
	void(__thiscall *Slot)(cEntity *cam, float dt);			/* 0x68 */
	void *vf27;
	void(__thiscall *UpdateOff)(cEntity *cam, float dt);		/* 0x70 */
} cObjCamVtbl;
SD_OFFSET(cObjCamVtbl, UpdateOff, 0x70);

typedef struct cObjAIVtbl {
	void *vf00;
	void(__thiscall *Tick)(void *ai, float dt);			/* 0x04 */
	void(__thiscall *UpdateOff)(void *ai, float dt);		/* 0x08 */
} cObjAIVtbl;

/* the variable methods of the sounds (cSndInterface::vftable from slot 1) */
typedef struct cObjSndVtbl {
	void *vf00;
	byte(__thiscall *VarI)(cSndInterface *self, const char *name, int *i, int set);
	byte(__thiscall *VarF)(cSndInterface *self, const char *name, double *d, int set);
	byte(__thiscall *VarS)(cSndInterface *self, const char *name, char **s, int set);
	byte(__thiscall *VarV)(cSndInterface *self, const char *name, double *x, double *y, double *z, int set);
} cObjSndVtbl;

static inline cObjEntity *Obj(cEntity *e) {
	return (cObjEntity *)e;
}

static inline const cObjCamVtbl *Cam_Vtbl(cEntity *cam) {
	return (const cObjCamVtbl *)cam->vftable;
}

static inline const cObjAIVtbl *AI_Vtbl(void *ai) {
	return *(const cObjAIVtbl **)ai;
}

static inline const cObjSndVtbl *Snd_Vtbl(cSndInterface *snd) {
	return (const cObjSndVtbl *)snd->vftable;
}

static inline uint32_t Float_Bits(float f) {
	uint32_t u;

	memcpy(&u, &f, 4);
	return u;
}

static inline float Bits_Float(uint32_t u) {
	float f;

	memcpy(&f, &u, 4);
	return f;
}

static inline float Deg2Rad(double v) {
	return (float)((v * M_PI_D) / 180.0);
}

static inline double Rad2Deg(float v) {
	return ((double)v * 180.0) / M_PI_D;
}

cEntity *__thiscall cObjEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cObjEntity *self = mem;

	cEntity_ctor(mem, name, x, y, z);
	self->base.vftable = Game_Ptr(COBJENTITY_VFTABLE);
	CGameModel_ctor(&self->model, NULL);
	cTimer_ctor(&self->timer);
	cSndInterface_ctor(&self->snd, 1);
	cPy_Function_ctor(&self->onObjSlot);
	cObjMove_ctor(&self->move);
	cModMove_ctor(&self->mod);
	self->ang.x = 0.0f;
	self->ang.y = 0.0f;
	self->ang.z = 0.0f;
	self->vel.x = 0.0f;
	self->vel.y = 0.0f;
	self->vel.z = 0.0f;
	self->rotVel.x = 0.0f;
	self->rotVel.y = 0.0f;
	self->rotVel.z = 0.0f;
	self->cam = NULL;
	self->netRes = 0;
	self->slotAng = self->ang;
	self->slotPos = self->base.pos;
	self->renderAng = self->ang;
	self->renderPos = self->base.pos;
	cSlotController *slot = operator_new(CSLOTCONTROLLER_SIZE);
	self->slot = slot ? cSlotController_ctor(slot, OBJ_SLOT_INTERVAL) : NULL;
	if (!g_pApp->flags) {
		self->net = NULL;
	} else {
		cNetController *net = operator_new(sizeof(cNetController));
		self->net = net ? cNetController_ctor(net) : NULL;
	}
	self->camOff.x = 0.0f;
	self->camOff.y = 50.0f;
	self->camOff.z = -50.0f;
	self->snd.owner = &self->base;
	self->radarShow = 0;
	self->radarShown = 0;
	self->radarTime = 0.0f;
	cModMove_SetParent(&self->mod, &self->base);
	self->lastTime = -1.0f;
	self->fx = NULL;
	self->realTime = 0;
	return &self->base;
}

void __fastcall cObjEntity_ReleaseFX(cEntity *self) {
	cObjEntity *o = Obj(self);

	if (o->fx) {
		o->fx->vftable->Release(o->fx);
		if (o->fx) {
			o->fx->vftable->Delete(o->fx, 1);
			o->fx = NULL;
		}
	}
}

void __fastcall cObjEntity_dtor(cEntity *self) {
	cObjEntity *o = Obj(self);

	self->vftable = Game_Ptr(COBJENTITY_VFTABLE);
	if (o->cam) {
		cCamEntity_SetTarget(o->cam, NULL);
	}
	operator_delete_sized(o->slot, CSLOTCONTROLLER_SIZE);
	if (o->net) {
		operator_delete_sized(o->net, sizeof(cNetController));
	}
	cObjEntity_ReleaseFX(self);
	cModMove_dtor(&o->mod);
	cObjMove_dtor(&o->move);
	cPy_Function_dtor(&o->onObjSlot);
	cSndInterface_dtor(&o->snd);
	CGameModel_Reset(&o->model);
	cEntity_dtor(self);
}

cEntity *__thiscall cObjEntity_Delete(cEntity *self, uint32_t flags) {
	cObjEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, COBJENTITY_SIZE);
	}
	return self;
}

void __thiscall cObjEntity_UpdateModel(cEntity *self) {
	cObjEntity *o = Obj(self);

	if (CGameModel_IsLoaded(&o->model)) {
		CGameModel_SetRot(&o->model, &o->renderAng);
		CGameModel_SetPos(&o->model, &o->renderPos);
	}
	if (o->fx) {
		o->fx->vftable->Tick(o->fx);
	}
}

void __thiscall cObjEntity_UpdateTimer(cEntity *self) {
	cObjEntity *o = Obj(self);
	cTimer *wt = &g_pWorld->timer;
	float dt;

	if (Timer_GetTime(wt) != o->lastTime) {
		if (o->realTime && cTimer_GetSpeed(wt) > 0.01f) {
			float d = cTimer_GetDelta(wt);
			dt = d / cTimer_GetSpeed(wt);
		} else {
			dt = cTimer_GetDelta(wt);
		}
		cTimer_Advance(&o->timer, dt);
		o->lastTime = Timer_GetTime(wt);
	}
}

void __fastcall cObjEntity_NewSlot(cEntity *self) {
	cSlotController *slot = operator_new(CSLOTCONTROLLER_SIZE);

	Obj(self)->slot = slot ? cSlotController_ctor(slot, OBJ_SLOT_INTERVAL) : NULL;
}

void __thiscall cObjEntity_RadarTick(cEntity *self, float dt) {
	cObjEntity *o = Obj(self);

	if (o->radarShown != o->radarShow) {
		o->radarTime = g_RadarShowTime;
		o->radarShown = o->radarShow;
	}
	o->radarTime = o->radarTime - dt;
}

byte __thiscall cObjEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	cObjEntity *o = Obj(self);

	if (!*name) {
		return 0;
	}
	if (cModMove_VarF(&o->mod, name, d, set) || Snd_Vtbl(&o->snd)->VarF(&o->snd, name, d, set)) {
		return 1;
	}
	if ((byte)set && !strcmp(name, "MoveTime")) {
		cObjMove_SetTime(&o->move, (float)*d);
		return 1;
	}
	if (!strcmp(name, "TimeSpeed")) {
		if ((byte)set) {
			cTimer_SetSpeedChecked(&o->timer, (float)*d);
		} else {
			*d = cTimer_GetSpeed(&o->timer);
		}
		return 1;
	}
	if (!strcmp(name, "EndTimeSpeed")) {
		if ((byte)set) {
			cTimer_SetSpeedTo(&o->timer, (float)*d);
		} else {
			*d = cTimer_GetSpeedTo(&o->timer);
		}
		return 1;
	}
	if (!strcmp(name, "FadeTimeSpeed")) {
		if ((byte)set) {
			cTimer_SetBlend(&o->timer, (float)*d);
		} else {
			*d = cTimer_GetBlendTime(&o->timer);
		}
		return 1;
	}
	if (!strcmp(name, "RenderEffectParam0")) {
		if ((byte)set) {
			Obj_Vtbl(self)->SetEffectParam(self, 0, (float)*d);
		} else {
			/* the original reads into the stack slot of set */
			uint32_t v = (uint32_t)set;
			CGameModel_GetEffectParam(&o->model, 0, &v);
			*d = Bits_Float(v);
		}
		return 1;
	}
	return cEntity_VarF(self, name, d, set);
}

vec3_t *__thiscall cObjEntity_GetAng(cEntity *self, vec3_t *out) {
	*out = Obj(self)->ang;
	return out;
}

vec3_t *__thiscall cObjEntity_GetRotVel(cEntity *self, vec3_t *out) {
	*out = Obj(self)->rotVel;
	return out;
}

/* vf27 and vf30 */
vec3_t *__thiscall cObjEntity_GetRenderAng(cEntity *self, vec3_t *out) {
	*out = Obj(self)->renderAng;
	return out;
}

/* vf26 and vf29 */
vec3_t *__thiscall cObjEntity_GetRenderPos(cEntity *self, vec3_t *out) {
	*out = Obj(self)->renderPos;
	return out;
}

vec3_t *__thiscall cObjEntity_GetSndAng(cEntity *self, vec3_t *out) {
	Obj_Vtbl(self)->GetRenderAng(self, out);
	return out;
}

CGameModel *__thiscall cObjEntity_GetModel(cEntity *self) {
	return &Obj(self)->model;
}

vec3_t *__thiscall cObjEntity_GetPos(cEntity *self, vec3_t *out) {
	cEntity_GetPos(self, out);
	return out;
}

int __thiscall cObjEntity_GetNodeSlot(cEntity *self) {
	(void)self;
	return 0;
}

vec3_t *__thiscall cObjEntity_GetSndPos(cEntity *self, vec3_t *out) {
	Obj_Vtbl(self)->GetViewPos(self, out);
	return out;
}

byte __fastcall cObjEntity_GetCollision(cEntity *self) {
	CGameModel *m = &Obj(self)->model;

	if (CGameModel_IsLoaded(m)) {
		return CGameModel_CollideOn(m);
	}
	return 0;
}

void __thiscall cObjEntity_GetTargetAngle(cEntity *self, float *ang, float dist, cEntity **target) {
	vec3_t pos, cang, dir, r1, r2, end, hit, d;

	Obj_Vtbl(self)->GetCamera(self, &pos, &cang);
	dir.x = 0.0f;
	dir.y = 0.0f;
	dir.z = dist;
	const vec3_t *a = Vec3_RotateXY(&r1, &dir, ang[0], ang[1]);
	const vec3_t *b = Vec3_RotateXY(&r2, a, cang.x, cang.y);
	end.x = b->x + pos.x;
	end.y = b->y + pos.y;
	end.z = b->z + pos.z;
	Engine_TestLine(&pos, &end, &hit, NULL);
	end = hit;
	cWorld_TestLineEntities(g_pWorld, (int)self, (void *)8, &pos, &end, &hit, target, 1);
	d.x = self->pos.x - hit.x;
	d.y = self->pos.y - hit.y;
	d.z = self->pos.z - hit.z;
	const vec3_t *r = Vec3_GetRotAng(&r2, &d);
	ang[0] = r->x;
	ang[1] = r->y;
	ang[2] = r->z;
}

void __thiscall cObjEntity_GetCamera(cEntity *self, vec3_t *pos, vec3_t *ang) {
	cObjEntity *o = Obj(self);
	cEntity *cam = o->cam;
	vec3_t rot, off, r, tmp;

	if (cam) {
		const vec3_t *orient = CamEntity_Orient(cam);
		ang->x = orient->x;
		rot.x = -ang->x;
		ang->y = orient->y;
		rot.y = ang->y + M_PI_F;
		ang->z = orient->z;
		rot.z = ang->z;
		float s = (float)Game_Sin((double)-*(float *)((byte *)cam + 0x218));
		s = s * *(float *)((byte *)o->cam + 0x1f0);
		off.x = o->camOff.x + 0.0f;
		off.y = o->camOff.y + s;
		off.z = o->camOff.z + 0.0f;
		const vec3_t *p = Vec3_Rotate(&r, &off, &rot);
		const vec3_t *v = Obj_Vtbl(self)->GetViewPos(self, &tmp);
		pos->x = v->x + p->x;
		pos->y = v->y + p->y;
		pos->z = v->z + p->z;
		ang->z = 0.0f;
		return;
	}
	*pos = *Obj_Vtbl(self)->GetViewPos(self, &tmp);
	const vec3_t *a = Obj_Vtbl(self)->GetAng(self, &tmp);
	ang->x = -a->x;
	ang->y = a->y + M_PI_F;
	ang->z = 0.0f;
}

const char *__thiscall cObjEntity_GetClassName(cEntity *self) {
	(void)self;
	return OBJ_CLASSNAME;
}

vec3_t *__thiscall cObjEntity_GetVel(cEntity *self, vec3_t *out) {
	*out = Obj(self)->vel;
	return out;
}

float __thiscall cObjEntity_GetRadius(cEntity *self) {
	(void)self;
	return 0.0f;
}

void __thiscall cObjEntity_UpdateOff(cEntity *self) {
	cObjEntity *o = Obj(self);

	cObjEntity_UpdateTimer(self);
	float dt = cTimer_GetDelta(&o->timer);
	if (OBJ_MIN_DT >= dt) {
		return;
	}
	if (self->ai) {
		AI_Vtbl(self->ai)->UpdateOff(self->ai, dt);
	}
	/* the original passes dt as the unused argument */
	cSndInterface_StopAll(&o->snd, (int)Float_Bits(dt));
	cEntity_UpdateOff(self);
	if (o->cam) {
		Cam_Vtbl(o->cam)->UpdateOff(o->cam, dt);
	}
}

byte __thiscall cObjEntity_VarI(cEntity *self, const char *name, int *i, int set) {
	cObjEntity *o = Obj(self);

	if (!*name) {
		return 0;
	}
	if (cModMove_VarI(&o->mod, name, i, set) || Snd_Vtbl(&o->snd)->VarI(&o->snd, name, i, set)) {
		return 1;
	}
	if (!strcmp(name, "RadarShow")) {
		if ((byte)set) {
			o->radarShow = *i;
		} else {
			*i = o->radarShow;
		}
		return 1;
	}
	if (!strcmp(name, "MoveFade")) {
		if ((byte)set) {
			o->move.fade = *i;
		} else {
			*i = o->move.fade;
		}
		return 1;
	}
	if (!strcmp(name, "UseRealTime")) {
		if ((byte)set) {
			o->realTime = *i != 0;
		} else {
			*i = o->realTime != 0;
		}
		return 1;
	}
	if (!strcmp(name, "MoveViewAng")) {
		if ((byte)set) {
			o->move.viewAng = *i != 0;
		} else {
			*i = o->move.viewAng != 0;
		}
		return 1;
	}
	if (!strcmp(name, "MapCollision")) {
		if ((byte)set) {
			cObjEntity_Switch(self, *i != 0);
		} else {
			*i = cObjEntity_GetCollision(self) != 0;
		}
		return 1;
	}
	if (!strcmp(name, "RenderEffect")) {
		if ((byte)set) {
			Obj_Vtbl(self)->SetEffect(self, (uint32_t)*i);
		} else {
			*i = (int)CGameModel_GetEffect(&o->model, NULL);
		}
		return 1;
	}
	return cEntity_VarI(self, name, i, set);
}

int __thiscall cObjEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_OBJECT)) {
		return 1;
	}
	return cEntity_IsType(self, type);
}

void __thiscall cObjEntity_Tick(cEntity *self, float dt) {
	cObjMove_Tick(&Obj(self)->move, self, dt);
	cObjEntity_RadarTick(self, dt);
	if (self->ai) {
		AI_Vtbl(self->ai)->Tick(self->ai, dt);
	}
}

void __thiscall cObjEntity_Render(cEntity *self) {
	CGameModel *m = &Obj(self)->model;

	if (CGameModel_IsLoaded(m)) {
		CGameModel_RenderNow(m, 1);
	}
}

void __thiscall cObjEntity_SetActive(cEntity *self, byte on) {
	cObjEntity *o = Obj(self);

	cEntity_SetActive(self, on);
	if (CGameModel_IsLoaded(&o->model)) {
		if (self->flag_4c) {
			CGameModel_Enable(&o->model);
			cModMove_Update(&o->mod, 0.0f);
			Obj_Vtbl(self)->UpdateModel(self);
		} else {
			CGameModel_Disable(&o->model);
		}
	}
	if (o->fx) {
		o->fx->vftable->Stop(o->fx, on);
	}
}

void __thiscall cObjEntity_SetAng(cEntity *self, const vec3_t *ang) {
	cObjEntity *o = Obj(self);

	o->ang.x = ang->x;
	o->ang.y = ang->y;
	o->ang.z = ang->z;
	o->slotAng = o->ang;
	o->renderAng = o->ang;
	Obj_Vtbl(self)->UpdateModel(self);
	if (o->cam) {
		cCamEntity_SetAng(o->cam, o->ang);
	}
}

void __thiscall cObjEntity_SetRotVel(cEntity *self, const vec3_t *v) {
	Obj(self)->rotVel.x = v->x;
	Obj(self)->rotVel.y = v->y;
	Obj(self)->rotVel.z = v->z;
}

void __thiscall cObjEntity_SetEmbeddedFX(cEntity *self, const char *fxType) {
	cObjEntity *o = Obj(self);

	if (!self->deadNode.target) {
		EmbeddedFXMisc_Create(self, CGameModel_Name(&o->model), &o->fx);
		FX_Sputnik(self, fxType);
	}
}

void __thiscall cObjEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag) {
	cObjEntity *o = Obj(self);
	vec3_t scale, cam;

	(void)flag;
	if (!file || !*file) {
		cObjEntity_ReleaseFX(self);
		CGameModel_LoadName(&o->model, NULL);
		return;
	}
	cObjEntity_ReleaseFX(self);
	scale.x = sx;
	scale.y = sy;
	scale.z = sz;
	CGameModel_LoadScaled(&o->model, file, &scale);
	CGameModel_SetPivotNodo(&o->model, "DC_Root");
	CGameModel_SetPos(&o->model, &self->pos);
	CGameModel_SetRot(&o->model, &o->ang);
	int ic = CGameModel_FindNodoR(&o->model, "DC_Camera");
	int ir = CGameModel_FindNodoR(&o->model, "DC_Root");
	if (ic != -1 && ir != -1) {
		const vec3_t *root = CGameModel_NodoPos(&o->model, &scale, (uint32_t)ir, 0);
		const vec3_t *c = CGameModel_NodoPos(&o->model, &cam, (uint32_t)ic, 0);
		o->camOff.x = c->x - root->x;
		o->camOff.y = c->y - root->y;
		o->camOff.z = c->z - root->z;
	}
	if (!self->flag_4c) {
		CGameModel_Disable(&o->model);
	}
	if (self->vftable->IsType(self, "item")) {
		EmbeddedFXMisc_Create(self, CGameModel_Name(&o->model), &o->fx);
	}
}

void __thiscall cEntity_SetNetResource(cEntity *e, const char *name) {
	int r = Net_AddResource(name);

	if (r != -1) {
		Obj(e)->netRes = (byte)r;
	}
}

void __thiscall cObjEntity_SetPos(cEntity *self, const vec3_t *pos) {
	cObjEntity *o = Obj(self);

	cEntity_SetPos(self, pos);
	o->slotPos = self->pos;
	o->renderPos = self->pos;
	Obj_Vtbl(self)->UpdateModel(self);
}

void __thiscall cObjEntity_SetEffect(cEntity *self, uint32_t fx) {
	CGameModel_SetEffect(&Obj(self)->model, fx);
}

void __thiscall cObjEntity_SetEffectParam(cEntity *self, int i, float v) {
	CGameModel_SetEffectParam(&Obj(self)->model, i, Float_Bits(v));
}

void __thiscall cObjEntity_Switch(cEntity *self, byte on) {
	if (CGameModel_IsLoaded(&Obj(self)->model)) {
		CGameModel_EnableCollision(Obj_Vtbl(self)->GetModel(self), on, 1);
		return;
	}
	Con_Warnf("SetStaticMapCollision(%d) Entity '%s' has no model\n", on != 0, self->name);
}

void __thiscall cObjEntity_SetVel(cEntity *self, const vec3_t *vel) {
	cObjEntity *o = Obj(self);

	if (!o->move.active) {
		o->vel.x = vel->x;
		o->vel.y = vel->y;
		o->vel.z = vel->z;
	}
}

/* the render state is ahead of the slot by the velocity of one slot while a missile updates */
static inline byte Obj_Extrapolates(const cObjEntity *o) {
	return g_MissileUpdating && !o->cam;
}

void __thiscall cObjEntity_Update(cEntity *self) {
	cObjEntity *o = Obj(self);

	cObjEntity_UpdateTimer(self);
	float dt = cTimer_GetDelta(&o->timer);
	if (OBJ_MIN_DT >= dt) {
		return;
	}
	cSndInterface_Tick(&o->snd, dt);
	byte ex = Obj_Extrapolates(o);
	float step = o->slot->interval;
	float f = o->slot->time / step;
	if (self->pos.x != o->slotPos.x || self->pos.y != o->slotPos.y || self->pos.z != o->slotPos.z || ex) {
		float cx = self->pos.x, cy = self->pos.y, cz = self->pos.z;
		if (ex) {
			cy = cy + o->vel.y * step;
			cz = cz + o->vel.z * step;
			cx = cx + o->vel.x * step;
		}
		o->renderPos.x = (o->slotPos.x - cx) * f + cx;
		o->renderPos.y = (o->slotPos.y - cy) * f + cy;
		o->renderPos.z = (o->slotPos.z - cz) * f + cz;
	} else {
		o->renderPos = self->pos;
	}
	if (o->ang.x != o->slotAng.x || o->ang.y != o->slotAng.y || o->ang.z != o->slotAng.z || ex) {
		float ax = o->ang.x, ay = o->ang.y, az = o->ang.z;
		if (ex) {
			float s = o->slot->interval;
			ay = ay + o->rotVel.y * s;
			az = az + o->rotVel.z * s;
			ax = ax + o->rotVel.x * s;
		}
		float d0 = o->slotAng.x - ax;
		float d1 = o->slotAng.y - ay;
		float d2 = o->slotAng.z - az;
		d0 = NormAng(d0);
		d1 = NormAng(d1);
		d2 = NormAng(d2);
		o->renderAng.x = d0 * f + ax;
		o->renderAng.y = d1 * f + ay;
		o->renderAng.z = d2 * f + az;
	} else {
		o->renderAng = o->ang;
	}
	cModMove_Update(&o->mod, dt);
	Obj_Vtbl(self)->UpdateModel(self);
	if (o->cam) {
		Cam_Vtbl(o->cam)->Lerp(o->cam, f);
	}
	if ((byte)cSlotController_Tick(o->slot, dt)) {
		float iv;
		while (cSlotController_Fire(o->slot, &iv)) {
			if (Obj_Extrapolates(o)) {
				float s = o->slot->interval;
				o->slotPos.x = o->vel.x * s + self->pos.x;
				o->slotPos.y = self->pos.y + o->vel.y * s;
				o->slotPos.z = self->pos.z + o->vel.z * s;
				s = o->slot->interval;
				o->slotAng.x = o->ang.x + o->rotVel.x * s;
				o->slotAng.y = o->ang.y + o->rotVel.y * s;
				o->slotAng.z = o->ang.z + o->rotVel.z * s;
			} else {
				o->slotPos = self->pos;
				o->slotAng = o->ang;
			}
			if (o->net) {
				o->net->vftable->Reset(o->net);
			}
			if ((byte)cPy_Function_IsValid(&o->onObjSlot)) {
				cPy_Function_AddParamS(&o->onObjSlot, self->name);
				cPy_Function_AddParamF(&o->onObjSlot, iv);
				cPy_Function_Execute(&o->onObjSlot, 0);
			}
			Obj_Vtbl(self)->Tick(self, iv);
			if (o->cam) {
				Cam_Vtbl(o->cam)->Slot(o->cam, iv);
			}
		}
		float cx = self->pos.x, cy = self->pos.y, cz = self->pos.z;
		if (Obj_Extrapolates(o)) {
			float s = o->slot->interval;
			cy = cy + o->vel.y * s;
			cx = cx + s * o->vel.x;
			cz = cz + o->vel.z * s;
		}
		float dy = cy - o->slotPos.y;
		float dx = cx - o->slotPos.x;
		float dz = cz - o->slotPos.z;
		if (dy * dy + dx * dx + dz * dz > OBJ_SLOT_JUMP2) {
			o->slotPos.x = cx;
			o->slotPos.y = cy;
			o->slotPos.z = cz;
		}
	}
	if (o->fx) {
		o->fx->vftable->Update(o->fx, dt);
	}
	cEntity_Update(self);
}

byte __thiscall cObjEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	cObjEntity *o = Obj(self);

	if (!*name) {
		return 0;
	}
	if (cModMove_VarS(&o->mod, name, s, set)) {
		return 1;
	}
	if (!strcmp(name, "OnEndMove")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&o->move.onEnd, *s);
		} else {
			*s = OBJ_ONENDMOVE_DESC;
		}
		return 1;
	}
	if (!(byte)set && !strcmp(name, "CamName")) {
		*s = o->cam ? o->cam->name : EXE_EMPTY;
		return 1;
	}
	if (!strcmp(name, "NetRes")) {
		if ((byte)set) {
			cEntity_SetNetResource(self, *s);
		} else {
			*s = (char *)Net_GetResourceName(o->netRes);
		}
		return 1;
	}
	if (!strcmp(name, "OnObjSlot")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&o->onObjSlot, *s);
		} else {
			*s = OBJ_ONOBJSLOT_DESC;
		}
		return 1;
	}
	if (Snd_Vtbl(&o->snd)->VarS(&o->snd, name, s, set)) {
		return 1;
	}
	return cEntity_VarS(self, name, s, set);
}

/* x, y, z of the translation of the node matrix */
static void Obj_NodePos(const matrix_t *m, double *x, double *y, double *z) {
	*x = m->m[12];
	*y = m->m[13];
	*z = m->m[14];
}

byte __thiscall cObjEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	cObjEntity *o = Obj(self);
	matrix_t mat;
	vec3_t v;

	if (!*name) {
		return 0;
	}
	if (cModMove_VarV(&o->mod, name, x, y, z, set)) {
		return 1;
	}
	if (!(byte)set) {
		if (!crt_strncmp("NT_NodePos_", name, 11)) {
			int i = CGameModel_FindNodoR(&o->model, name + 11);
			if (i) {
				Obj_NodePos(CGameModel_NodoMatrix(&o->model, &mat, (uint32_t)i, 0, NULL, 0), x, y, z);
				return 1;
			}
		}
		if (!crt_strncmp("NodePos_", name, 8)) {
			int i = CGameModel_FindNodoR(&o->model, name + 8);
			if (i) {
				uint32_t slot = (uint32_t)Obj_Vtbl(self)->GetNodeSlot(self);
				Obj_NodePos(CGameModel_NodoMatrix(&o->model, &mat, (uint32_t)i, 1, NULL, slot), x, y, z);
				return 1;
			}
		}
	}
	if (!strcmp(name, "MoveAng")) {
		if ((byte)set) {
			o->move.moveAng.x = Deg2Rad(*x);
			o->move.moveAng.y = Deg2Rad(*y);
			o->move.moveAng.z = Deg2Rad(*z);
		} else {
			*x = Rad2Deg(o->move.moveAng.x);
			*y = Rad2Deg(o->move.moveAng.y);
			*z = Rad2Deg(o->move.moveAng.z);
		}
		return 1;
	}
	if (!strcmp(name, "MovePos")) {
		if ((byte)set) {
			o->move.movePos.x = (float)*x;
			o->move.movePos.y = (float)*y;
			o->move.movePos.z = (float)*z;
		} else {
			*x = o->move.movePos.x;
			*y = o->move.movePos.y;
			*z = o->move.movePos.z;
		}
		return 1;
	}
	if (!(byte)set && !strcmp(name, "SndPos")) {
		Obj_Vtbl(self)->GetSndPos(self, &v);
		*x = v.x;
		*y = v.y;
		*z = v.z;
		return 1;
	}
	if (!strcmp(name, "CamTgt")) {
		if ((byte)set) {
			o->camOff.x = (float)*x;
			o->camOff.y = (float)*y;
			o->camOff.z = (float)*z;
		} else {
			*x = o->camOff.x;
			*y = o->camOff.y;
			*z = o->camOff.z;
		}
		return 1;
	}
	if (!strcmp(name, "Ang")) {
		if ((byte)set) {
			v.x = Deg2Rad(*x);
			v.y = Deg2Rad(*y);
			v.z = Deg2Rad(*z);
			Obj_Vtbl(self)->SetAng(self, &v);
		} else {
			Obj_Vtbl(self)->GetAng(self, &v);
			*x = Rad2Deg(v.x);
			*y = Rad2Deg(v.y);
			*z = Rad2Deg(v.z);
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "PvRot")) {
		v.x = Deg2Rad(*x);
		v.y = Deg2Rad(*y);
		v.z = Deg2Rad(*z);
		CGameModel_SetRot2(&o->model, &v);
		return 1;
	}
	if ((byte)set && !strcmp(name, "PvPos")) {
		v.x = (float)*x;
		v.y = (float)*y;
		v.z = (float)*z;
		CGameModel_SetPivot(&o->model, &v, 1, 0);
		return 1;
	}
	if (Snd_Vtbl(&o->snd)->VarV(&o->snd, name, x, y, z, set)) {
		return 1;
	}
	return cEntity_VarV(self, name, x, y, z, set);
}

/* cNetController */

cNetController *__fastcall cNetController_ctor(cNetController *self) {
	self->vftable = Game_Ptr(CNETCONTROLLER_VFTABLE);
	self->age = 0.0f;
	self->rotVel.x = 0.0f;
	self->rotVel.y = 0.0f;
	self->rotVel.z = 0.0f;
	self->vel = self->rotVel;
	self->ang = self->vel;
	self->pos = self->ang;
	self->flags = 0;
	for (int i = 0; i < 6; i++) {
		self->left[i] = 5;
	}
	cNetController_Reset(self);
	return self;
}

void __thiscall cNetController_Reset(cNetController *self) {
	memset(self->pending, 1, sizeof(self->pending));
}

/* one part of Changed: a small change counts down its sends (once per Reset), a big one starts them again */
static inline byte Net_Part(cNetController *self, int part, byte small, byte flags, byte bit) {
	if (!small) {
		self->left[part] = 5;
		return flags;
	}
	if (self->left[part] <= 0) {
		return flags & ~bit;
	}
	if (self->pending[part]) {
		self->left[part]--;
		self->pending[part] = 0;
	}
	return flags;
}

byte __thiscall cNetController_Changed(cNetController *self, byte flags, cEntity *e) {
	cObjEntity *o = Obj(e);

	if (flags & NETC_POS) {
		float dy = e->pos.y - self->pos.y;
		float dx = e->pos.x - self->pos.x;
		float dz = e->pos.z - self->pos.z;
		byte small = 100.0f > dy * dy + dx * dx + dz * dz;
		flags = Net_Part(self, 0, small, flags, NETC_POS);
		if (!small) {
			self->pos = e->pos;
		}
	}
	if (flags & NETC_VEL) {
		float dy = o->vel.y - self->vel.y;
		float dx = o->vel.x - self->vel.x;
		float dz = o->vel.z - self->vel.z;
		byte small = 10.0f > dy * dy + dx * dx + dz * dz;
		flags = Net_Part(self, 3, small, flags, NETC_VEL);
		if (!small) {
			self->vel = o->vel;
		}
	}
	if (flags & NETC_ANG) {
		byte small = (double)0.05f > __builtin_fabs((double)(o->ang.x - self->ang.x)) &&
			(double)0.05f > __builtin_fabs((double)(o->ang.y - self->ang.y));
		flags = Net_Part(self, 1, small, flags, NETC_ANG);
		if (!small) {
			self->ang.x = o->ang.x;
			self->ang.y = o->ang.y;
		}
	}
	if (flags & NETC_ANGZ) {
		byte small = (double)0.05f > __builtin_fabs((double)(o->ang.z - self->ang.z));
		flags = Net_Part(self, 2, small, flags, NETC_ANGZ);
		if (!small) {
			self->ang.z = o->ang.z;
		}
	}
	if (flags & NETC_ROTVEL) {
		byte small = (double)0.1f > __builtin_fabs((double)(o->rotVel.x - self->rotVel.x)) &&
			(double)0.1f > __builtin_fabs((double)(o->rotVel.y - self->rotVel.y));
		flags = Net_Part(self, 4, small, flags, NETC_ROTVEL);
		if (!small) {
			self->rotVel.x = o->rotVel.x;
			self->rotVel.y = o->rotVel.y;
		}
	}
	if (flags & NETC_ROTVELZ) {
		byte small = (double)0.1f > __builtin_fabs((double)(o->rotVel.z - self->rotVel.z));
		flags = Net_Part(self, 5, small, flags, NETC_ROTVELZ);
		if (!small) {
			self->rotVel.z = o->rotVel.z;
		}
	}
	self->flags = flags;
	return flags;
}

void __thiscall cNetController_Apply(cNetController *self, cEntity *e, float dt, byte move) {
	cObjEntity *o = Obj(e);

	if (self->age != 0.5f) {
		if (move) {
			e->pos.x = e->pos.x + o->vel.x * dt;
			e->pos.y = o->vel.y * dt + e->pos.y;
			e->pos.z = o->vel.z * dt + e->pos.z;
			o->ang.x = o->ang.x + dt * o->rotVel.x;
			o->ang.y = o->ang.y + o->rotVel.y * dt;
			o->ang.z = o->ang.z + o->rotVel.z * dt;
		}
		if (self->age > 0.0f) {
			self->age = self->age - dt;
			return;
		}
		if (move) {
			memset(&o->vel, 0, sizeof(o->vel));
			memset(&o->rotVel, 0, sizeof(o->rotVel));
		}
		return;
	}
	if (g_pApp->flags & APPF_SERVER) {
		if (self->flags & NETC_POS) {
			e->pos = self->pos;
		}
		if (self->flags & NETC_ANG) {
			o->ang.x = self->ang.x;
			o->ang.y = self->ang.y;
		}
		if (self->flags & NETC_ANGZ) {
			o->ang.z = self->ang.z;
		}
		if (self->flags & NETC_VEL) {
			o->vel = self->vel;
		}
		if (self->flags & NETC_ROTVEL) {
			o->rotVel.x = self->rotVel.x;
			o->rotVel.y = self->rotVel.y;
		}
		if (self->flags & NETC_ROTVELZ) {
			o->rotVel.z = self->rotVel.z;
		}
		self->age = self->age - dt;
		return;
	}
	/* a client: the position is pulled towards the state, the rest of the way goes into the velocity */
	float vx = 0.0f, vy = 0.0f, vz = 0.0f, rx = 0.0f, ry = 0.0f;
	if (self->flags & NETC_POS) {
		float dy = self->pos.y - e->pos.y;
		float dx = self->pos.x - e->pos.x;
		float dz = self->pos.z - e->pos.z;
		float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		float lo = g_NetSmoothMin;
		if (lo > dist || dist > g_NetSmoothMax) {
			e->pos = self->pos;
		} else {
			float t = (dist - lo) / (g_NetSmoothMax - lo);
			float k = g_NetSmoothFactor;
			float m;
			if (0.0f > k) {
				m = 0.0f;
			} else if (k > 1.0f) {
				m = 1.0f;
			} else {
				m = k * t;
			}
			float g = 1.0f - m;
			float nx = (self->pos.x - e->pos.x) * m + e->pos.x;
			float ny = e->pos.y + (self->pos.y - e->pos.y) * m;
			float nz = e->pos.z + (self->pos.z - e->pos.z) * m;
			e->pos.x = nx;
			e->pos.y = ny;
			e->pos.z = nz;
			vx = dx * g * 2.0f;
			vy = dy * g * 2.0f;
			vz = dz * g * 2.0f;
			o->vel.x = vx;
			o->vel.y = vy;
			o->vel.z = vz;
		}
	}
	if (self->flags & NETC_ANG) {
		rx = NormAng(self->ang.x - o->ang.x) * 2.0f;
		ry = NormAng(self->ang.y - o->ang.y) * 2.0f;
		o->ang.x = self->ang.x;
		o->ang.y = self->ang.y;
	}
	if (self->flags & NETC_ANGZ) {
		o->ang.z = self->ang.z;
	}
	if (self->flags & NETC_VEL) {
		o->vel.x = vx + self->vel.x;
		o->vel.y = self->vel.y + vy;
		o->vel.z = self->vel.z + vz;
	}
	if (self->flags & NETC_ROTVEL) {
		o->rotVel.x = self->rotVel.x + rx;
		o->rotVel.y = self->rotVel.y + ry;
	}
	if (self->flags & NETC_ROTVELZ) {
		o->rotVel.z = self->rotVel.z;
	}
	self->age = self->age - dt;
}

static inline int16_t Net_ReadShort(cAbstractFile *msg) {
	int16_t v;

	msg->vftable->Read(msg, &v, 2);
	return v;
}

static inline int8_t Net_ReadByte(cAbstractFile *msg) {
	int8_t v;

	msg->vftable->Read(msg, &v, 1);
	return v;
}

/* a packed angle part: short / (32000 / pi) or byte / (127 / pi) */
static inline float Net_ReadAng(cAbstractFile *msg, byte full) {
	if (full) {
		return (float)((double)Net_ReadShort(msg) / NET_ANG_SHORT);
	}
	return (float)((double)Net_ReadByte(msg) / NET_ANG_BYTE);
}

static inline float Net_ReadRot(cAbstractFile *msg, byte full) {
	if (full) {
		return (float)((double)Net_ReadShort(msg) * NET_ROT_SHORT);
	}
	return (float)((double)Net_ReadByte(msg) * NET_ROT_BYTE);
}

static inline void Net_WriteShort(cAbstractFile *msg, int v) {
	uint32_t t = (uint16_t)v;

	msg->vftable->Write(msg, &t, 2);
}

static inline void Net_WriteByte(cAbstractFile *msg, int v) {
	byte b = (byte)v;

	msg->vftable->Write(msg, &b, 1);
}

static inline void Net_WriteFloat(cAbstractFile *msg, float f) {
	msg->vftable->Write(msg, &f, 4);
}

/* a packed angle: the x87 product of the normalized angle (in the precision of the game) truncated */
static inline void Net_WriteAng(cAbstractFile *msg, float ang, byte full) {
	float a = NormAng(ang);

	if (full) {
		Net_WriteShort(msg, (int)X87_Mul(a, NET_ANG_SHORT));
	} else {
		Net_WriteByte(msg, (int)X87_Mul(a, NET_ANG_BYTE));
	}
}

/* a packed rotation speed clamped to +-32000 or +-127 */
static inline void Net_WriteRot(cAbstractFile *msg, float v, byte full) {
	double d = v;

	if (full) {
		double r = NET_ROT_SHORT_MAX > d ? (d > -NET_ROT_SHORT_MAX ? d / NET_ROT_SHORT : -32000.0) : 32000.0;
		Net_WriteShort(msg, (int)r);
	} else {
		double r = NET_ROT_BYTE_MAX > d ? (d > -NET_ROT_BYTE_MAX ? d / NET_ROT_BYTE : -127.0) : 127.0;
		Net_WriteByte(msg, (int)r);
	}
}

/* a packed coordinate: clamped below to -NET_POS_MAX, above by minss */
static inline float Net_PosClamp(float v) {
	return -NET_POS_MAX >= v ? -NET_POS_MAX : Float_MinSS(NET_POS_MAX, v);
}

void __thiscall cNetController_Write(cNetController *self, byte flags, cEntity *e, void *m, byte force) {
	cAbstractFile *msg = m;
	cObjEntity *o = Obj(e);
	byte f;

	if (force) {
		self->flags = flags;
		f = flags;
	} else {
		f = self->vftable->Changed(self, flags, e);
	}
	Net_WriteByte(msg, f);
	byte full = f & NETC_FULL;
	if (f & NETC_POS) {
		if (full) {
			Net_WriteFloat(msg, e->pos.x);
			Net_WriteFloat(msg, e->pos.y);
			Net_WriteFloat(msg, e->pos.z);
		} else {
			float x = (e->pos.x - g_NetOrigin.x) * NET_POS_SCALE;
			float y = (e->pos.y - g_NetOrigin.y) * NET_POS_SCALE;
			float z = (e->pos.z - g_NetOrigin.z) * NET_POS_SCALE;
			float cx = Net_PosClamp(x);
			float cy = Net_PosClamp(y);
			float cz = Net_PosClamp(z);
			Net_WriteShort(msg, (int)cx);
			Net_WriteShort(msg, (int)cy);
			Net_WriteShort(msg, (int)cz);
		}
	}
	if (f & NETC_ANG) {
		Net_WriteAng(msg, o->ang.x, full);
		Net_WriteAng(msg, o->ang.y, full);
	}
	if (f & NETC_ANGZ) {
		Net_WriteAng(msg, o->ang.z, full);
	}
	if (f & NETC_VEL) {
		vec3_t v = o->vel;
		if (full) {
			Net_WriteFloat(msg, v.x);
			Net_WriteFloat(msg, v.y);
			Net_WriteFloat(msg, v.z);
		} else {
			float l = (float)__builtin_sqrt((double)(v.y * v.y + v.x * v.x + v.z * v.z));
			float vx = v.x, vy = v.y, vz = v.z;
			if (l > NET_VEL_MAX) {
				float k = NET_VEL_MAX / l;
				vz = k * v.z;
				vx = v.x * k;
				vy = k * v.y;
			}
			Net_WriteShort(msg, (int)vx);
			Net_WriteShort(msg, (int)vy);
			Net_WriteShort(msg, (int)vz);
		}
	}
	/* the z speed also under the x and y bit, as Read takes it */
	if (f & NETC_ROTVEL) {
		Net_WriteRot(msg, o->rotVel.x, full);
		Net_WriteRot(msg, o->rotVel.y, full);
		Net_WriteRot(msg, o->rotVel.z, full);
	}
}

void __thiscall cNetController_Read(cNetController *self, void *m) {
	cAbstractFile *msg = m;
	vec3_t clamped;

	msg->vftable->Read(msg, &self->flags, 1);
	if (self->flags & NETC_POS) {
		if (self->flags & NETC_FULL) {
			msg->vftable->Read(msg, &self->pos.x, 4);
			msg->vftable->Read(msg, &self->pos.y, 4);
			msg->vftable->Read(msg, &self->pos.z, 4);
		} else {
			self->pos.x = (float)Net_ReadShort(msg);
			self->pos.y = (float)Net_ReadShort(msg);
			self->pos.z = (float)Net_ReadShort(msg);
			float px = self->pos.x * NET_POS_STEP;
			float py = self->pos.y * NET_POS_STEP;
			float pz = self->pos.z * NET_POS_STEP;
			self->pos.x = g_NetOrigin.x + px;
			self->pos.y = g_NetOrigin.y + py;
			self->pos.z = g_NetOrigin.z + pz;
		}
		self->pos = *Grid_ClampPos(g_pWorld->grid, &clamped, &self->pos);
	}
	if (self->flags & NETC_ANG) {
		byte full = self->flags & NETC_FULL;
		self->ang.x = Net_ReadAng(msg, full);
		self->ang.y = Net_ReadAng(msg, full);
	}
	if (self->flags & NETC_ANGZ) {
		self->ang.z = Net_ReadAng(msg, self->flags & NETC_FULL);
	}
	if (self->flags & NETC_VEL) {
		if (self->flags & NETC_FULL) {
			msg->vftable->Read(msg, &self->vel.x, 4);
			msg->vftable->Read(msg, &self->vel.y, 4);
			msg->vftable->Read(msg, &self->vel.z, 4);
		} else {
			self->vel.x = (float)Net_ReadShort(msg);
			self->vel.y = (float)Net_ReadShort(msg);
			self->vel.z = (float)Net_ReadShort(msg);
		}
	}
	if (self->flags & NETC_ROTVEL) {
		byte full = self->flags & NETC_FULL;
		self->rotVel.x = Net_ReadRot(msg, full);
		self->rotVel.y = Net_ReadRot(msg, full);
	}
	/* the original tests the x and y bit again for z */
	if (self->flags & NETC_ROTVEL) {
		self->rotVel.z = Net_ReadRot(msg, self->flags & NETC_FULL);
	}
	self->age = 0.5f;
}
