/*
 * cWithLifeEntity ("WithLife"), see include/scrap/withlifeentity.h. The destructor of cVehicleAttack sits in the same
 * code.
 */
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/fxscene.h"
#include "scrap/fxvehicle.h"
#include "scrap/mathlib.h"
#include "scrap/missileentity.h"
#include "scrap/net.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weaps2.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#define WL_CLASSNAME "WithLife"
#define WL_NOCTRL_WARN "WARNING: '%s' action controler doesn´t exist.\n"	/* "WARNING: '%s' action controler doesn't exist.\n" */
#define WL_MIN_DT 1e-6f
#define WL_FADE_MIN 0.001f					/* rva 0x3a3d58: no fade for a shorter time */
#define WL_SND_FIRE 4						/* the fire sound of lifeSounds */

/* the float argument the original passes where cSndInterface_StopAll takes an unused int */
static inline int WL_FloatBits(float f) {
	int i;

	memcpy(&i, &f, 4);
	return i;
}

cEntity *cWithLifeEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cWithLifeEntity *self = mem;
	cEntity *e = &self->obj.base;

	cObjEntity_ctor(mem, name, x, y, z);
	e->vftable = CWITHLIFEENTITY_VFTABLE;
	Nto1Node_Init(&self->burner);
	cString_ctor(&self->usrName);
	Nto1Node_Init(&self->targets);
	cPy_Function_ctor(&self->onDeath);
	cPy_Function_ctor(&self->onDamage);
	cVehicleAttack_ctor(&self->attack);
	cArrowIndicator_ctor(&self->arrow);
	cPrimitive_ctor(&self->prim);
	cSndInterface_ctor(&self->hitSounds, 1);
	cSndInterface_ctor(&self->lifeSounds, 1);
	cString_ctor(&self->headModel);
	e->mask = 8;
	self->prevLife = 0;
	self->life = g_StdMaxLife;
	self->mass = 100.0f;
	self->fireDelay = 0.0f;
	self->fireRemain = 0.0f;
	self->burnAcc = 0.0f;
	self->burner.data = self;
	self->attack.owner = e;
	self->fireAtt = 0.0f;
	cPrimitive_SetOwner(&self->prim, e);
	self->hitSounds.owner = e;
	cSndInterface_SetId(&self->hitSounds, "sdd");
	self->lifeSounds.owner = e;
	cSndInterface_SetId(&self->lifeSounds, "sdl");
	self->actCtrl = NULL;
	cArrowIndicator_SetParent(&self->arrow, e);
	self->headAnim = 0;
	self->targets.target = self;
	self->god = 0;
	return e;
}

void cWithLifeEntity_dtor(cEntity *self) {
	cWithLifeEntity *w = WLife(self);

	self->vftable = CWITHLIFEENTITY_VFTABLE;
	if (w->actCtrl) {
		ActCtrl_Vtbl(w->actCtrl)->Delete(w->actCtrl, 1);
	}
	cString_dtor(&w->headModel);
	cSndInterface_dtor(&w->lifeSounds);
	cSndInterface_dtor(&w->hitSounds);
	cArrowIndicator_dtor(&w->arrow);
	cVehicleAttack_dtor(&w->attack);
	cPy_Function_dtor(&w->onDamage);
	cPy_Function_dtor(&w->onDeath);
	Nto1Node_Done(&w->targets);
	cString_dtor(&w->usrName);
	Nto1Node_Done(&w->burner);
	cObjEntity_dtor(self);
}

/* VehicleAttack.h, inlined into WithLifeEntity.cpp */
void cVehicleAttack_dtor(cVehicleAttack *self) {
	Nto1Node_Done(&self->hitBy);
	Nto1Node_Done(&self->last);
	Nto1Node_Done(&self->sent);
	Nto1Node_Done(&self->target);
}

cEntity *cWithLifeEntity_Delete(cEntity *self, uint32_t flags) {
	cWithLifeEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CWITHLIFEENTITY_SIZE);
	}
	return self;
}

void cWithLifeEntity_SetSndTime(cEntity *self, float t, int i) {
	cSndItem *s = PtrArray_Get(&WLife(self)->lifeSounds, i);
	if (s) {
		s->time = -t;
	}
}

void cWithLifeEntity_Burn(cEntity *self, float dt) {
	cWithLifeEntity *w = WLife(self);

	if (!(w->fireRemain > 0.0f) || !(w->fireDelay > 0.0f) || !(w->fireAtt > -1.0f)) {
		return;
	}
	float k = w->fireAtt + 1.0f;
	cSndItem *s = PtrArray_Get(&w->lifeSounds, WL_SND_FIRE);
	if (s) {
		s->time = -0.3f;
	}
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	w->burnAcc = w->burnAcc - dt;
	cEntity *burner = w->burner.target;
	cVehicleAttack_HitBy(&w->attack, burner ? burner->name : ACT_EMPTY);
	w->fireRemain = w->fireRemain - k * dt;
	if (w->god || !(0.0f > w->burnAcc)) {
		return;
	}
	int life = w->life;
	float step = w->fireDelay * k;
	do {
		life--;
		w->burnAcc = w->burnAcc + step;
	} while (0.0f > w->burnAcc);
	w->life = life;
}

byte cWithLifeEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	cWithLifeEntity *w = WLife(self);

	if (!*name) {
		return 0;
	}
	if (cArrowIndicator_VarF(&w->arrow, name, d, set)) {
		return 1;
	}
	if (w->actCtrl && ActCtrl_Vtbl(w->actCtrl)->VarF(w->actCtrl, name, d, set)) {
		return 1;
	}
	if (cPrimitive_VarF(&w->prim, name, d, set)) {
		return 1;
	}
	if (!strcmp(name, "Mass")) {
		if ((byte)set) {
			WithLife_Vtbl(self)->SetMass(self, (float)*d);
		} else {
			*d = w->mass;
		}
		return 1;
	}
	if (!strcmp(name, "FireRemainTime")) {
		if ((byte)set) {
			w->fireRemain = (float)*d;
		} else {
			*d = w->fireRemain;
		}
		return 1;
	}
	if (!strcmp(name, "FireAtenuationFactor")) {
		if ((byte)set) {
			w->fireAtt = (float)*d;
		} else {
			*d = w->fireAtt;
		}
		return 1;
	}
	if (!strcmp(name, "FireDelayTime")) {
		if ((byte)set) {
			w->fireDelay = (float)*d;
		} else {
			*d = w->fireDelay;
		}
		return 1;
	}
	if (Snd_Vtbl(&w->hitSounds)->VarF(&w->hitSounds, name, d, set) ||
		Snd_Vtbl(&w->lifeSounds)->VarF(&w->lifeSounds, name, d, set)) {
		return 1;
	}
	return cObjEntity_VarF(self, name, d, set);
}

cAction *cWithLifeEntity_GetAction(cEntity *self, const char *name) {
	cActCtrl *c = WithLife_Vtbl(self)->GetActCtrl(self);
	if (c && c->cls) {
		return cActClass_GetAction(c->cls, name);
	}
	return NULL;
}

int cWithLifeEntity_GetNodeSlot(cEntity *self) {
	cActCtrl *c = WLife(self)->actCtrl;
	if (!c) {
		return cObjEntity_GetNodeSlot(self);
	}
	return c->cls->pretransformChan;
}

cEntity *cWithLifeEntity_GetBurner(cEntity *self) {
	return WLife(self)->burner.target;
}

vec3_t *cWithLifeEntity_GetSndPos(cEntity *self, vec3_t *out) {
	cActCtrl *c = WLife(self)->actCtrl;

	if (c) {
		cActClass *cls = c->cls;
		if (cls->soundDisp.x != 0.0f || cls->soundDisp.y != 0.0f || cls->soundDisp.z != 0.0f) {
			vec3_t ang, rot, pos;
			const vec3_t *r = Vec3_Rotate(&rot, &cls->soundDisp, Obj_Vtbl(self)->GetViewAng(self, &ang));
			const vec3_t *p = cObjEntity_GetSndPos(self, &pos);
			out->x = p->x + r->x;
			out->y = p->y + r->y;
			out->z = p->z + r->z;
			return out;
		}
	}
	cObjEntity_GetSndPos(self, out);
	return out;
}

const char *cWithLifeEntity_GetClassName(cEntity *self) {
	(void)self;
	return WL_CLASSNAME;
}

float cWithLifeEntity_GetRadius(cEntity *self) {
	return WLife(self)->prim.radius;
}

void cWithLifeEntity_UpdateOff(cEntity *self) {
	cWithLifeEntity *w = WLife(self);

	cObjEntity_UpdateTimer(self);
	float dt = cTimer_GetDelta(&w->obj.timer);
	if (WL_MIN_DT >= dt) {
		return;
	}
	cSndInterface_StopAll(&w->hitSounds, WL_FloatBits(dt));
	cSndInterface_StopAll(&w->lifeSounds, WL_FloatBits(dt));
	w->fireRemain = 0.0f;
	w->fireDelay = 0.0f;
	w->fireAtt = -1.0f;
	cWithLifeEntity_Burn(self, dt);
	WithLife_Vtbl(self)->CheckLife(self);
	cObjEntity_UpdateOff(self);
}

byte cWithLifeEntity_VarI(cEntity *self, const char *name, int *i, int set) {
	cWithLifeEntity *w = WLife(self);

	if (!*name) {
		return 0;
	}
	if (cArrowIndicator_VarI(&w->arrow, name, i, set)) {
		return 1;
	}
	if (w->actCtrl && ActCtrl_Vtbl(w->actCtrl)->VarI(w->actCtrl, name, i, set)) {
		return 1;
	}
	if (!crt_strncmp("Hit_", name, 4) && cVehicleAttack_VarI(&w->attack, name + 4, i, set)) {
		return 1;
	}
	if (!strcmp(name, "HeadAnimIndex")) {
		if ((byte)set) {
			w->headAnim = *i;
		} else {
			*i = w->headAnim;
		}
		return 1;
	}
	if (!strcmp(name, "Invulnerable")) {
		if ((byte)set) {
			w->god = *i != 0;
		} else {
			*i = w->god != 0;
		}
		return 1;
	}
	if (!strcmp(name, "Life")) {
		if (!(byte)set) {
			*i = w->life;
		} else if (!w->god) {
			w->life = *i;
		}
		return 1;
	}
	if (!strcmp(name, "PrevLife")) {
		if (!(byte)set) {
			*i = w->prevLife;
		} else if (!w->god) {
			w->prevLife = *i;
		}
		return 1;
	}
	if (Snd_Vtbl(&w->hitSounds)->VarI(&w->hitSounds, name, i, set) ||
		Snd_Vtbl(&w->lifeSounds)->VarI(&w->lifeSounds, name, i, set)) {
		return 1;
	}
	return cObjEntity_VarI(self, name, i, set);
}

int cWithLifeEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, WL_CLASSNAME)) {
		return 1;
	}
	return cObjEntity_IsType(self, type);
}

void cWithLifeEntity_CheckLife(cEntity *self) {
	cWithLifeEntity *w = WLife(self);

	if (w->prevLife > 0 && w->prevLife > w->life) {
		if (cPy_Function_IsValid(g_OnGeneralDamage)) {
			cPy_Function_AddParamS(g_OnGeneralDamage, self->name);
			cPy_Function_AddParamS(g_OnGeneralDamage, Nto1Node_TargetName(&w->attack.hitBy));
			cPy_Function_AddParamI(g_OnGeneralDamage, w->prevLife - w->life);
			cPy_Function_Execute(g_OnGeneralDamage, 0);
		}
		WithLife_Vtbl(self)->OnDamage(self, w->prevLife - w->life);
		if (w->life <= 0) {
			w->life = 0;
			WithLife_Vtbl(self)->OnDeath(self);
		}
	}
	w->prevLife = w->life;
}

void cWithLifeEntity_FadeSounds(cEntity *self, float dt) {
	cWithLifeEntity *w = WLife(self);

	for (int i = 0; i < w->lifeSounds.nsnd; i++) {
		cSndItem *s = PtrArray_Get(&w->lifeSounds, i);
		if (!s) {
			continue;
		}
		float t = -s->time;
		float vol;
		s->sound.desc.loop = 1;
		if (0.0f >= t) {
			s->time = 0.0f;
			vol = 0.0f;
		} else {
			s->pitch = s->pitches[0];
			float in = s->times[0];
			if (WL_FADE_MIN >= in) {
				vol = s->vols[0];
			} else {
				float out = s->times[1];
				if (t > out) {
					float max = s->vols[0];
					float v = max * dt / in + s->volume;
					vol = v > max ? max : v;
				} else {
					float v = s->volume - s->vols[0] * dt / out;
					vol = 0.0f >= v ? 0.0f : v;
				}
			}
		}
		cSndItem_SetVolume(s, self, vol);
	}
}

void cWithLifeEntity_Tick(cEntity *self, float dt) {
	WithLife_Vtbl(self)->CheckLife(self);
	cWithLifeEntity_Burn(self, dt);
	cWithLifeEntity_FadeSounds(self, dt);
	Grid_Add(&g_pWorld->grid, self);
	cObjEntity_Tick(self, dt);
}

/* the OnDamage or OnDeath function of the entity, then of its entity list: name, attacker (and damage) */
static void WL_RunFunc(cEntity *self, cPy_Function *own, size_t listOff, const int *damage) {
	cWithLifeEntity *w = WLife(self);

	cPy_Function_AddParamS(own, self->name);
	cPy_Function_AddParamS(own, Nto1Node_TargetName(&w->attack.hitBy));
	if (damage) {
		cPy_Function_AddParamI(own, *damage);
	}
	cPy_Function_Execute(own, 0);
	byte *list = self->listNode.node.target;
	if (list) {
		cPy_Function *f = (cPy_Function *)(list + listOff);
		cPy_Function_AddParamS(f, self->name);
		cPy_Function_AddParamS(f, Nto1Node_TargetName(&w->attack.hitBy));
		if (damage) {
			cPy_Function_AddParamI(f, *damage);
		}
		cPy_Function_Execute(f, 0);
	}
}

void cWithLifeEntity_OnDamage(cEntity *self, int damage) {
	WL_RunFunc(self, &WLife(self)->onDamage, offsetof(cEntityListData, onDamage), &damage);
}

void cWithLifeEntity_OnDeath(cEntity *self) {
	WL_RunFunc(self, &WLife(self)->onDeath, offsetof(cEntityListData, onDeath), NULL);
}

/* the arrow model is drawn again on top, without its glow */
void cWithLifeEntity_RenderNow(cEntity *self) {
	cWithLifeEntity *w = WLife(self);

	cObjEntity_Render(self);
	CGameModel *m = w->arrow.mission;
	if (!self->active || !m || !CGameModel_IsLoaded(m) || !CGameModel_IsEnabled(w->arrow.mission)) {
		return;
	}
	cArrowGlow *glow = w->arrow.glow;
	if (glow) {
		cArrowGlow_SetVisible(glow, 0);
	}
	CGameModel_RenderNow(w->arrow.mission, 1);
	if (glow) {
		cArrowGlow_SetVisible(glow, 1);
	}
}

void cWithLifeEntity_SetActCtrl(cEntity *self, const char *cls) {
	cWithLifeEntity *w = WLife(self);
	byte created = 0;

	if (w->actCtrl) {
		ActCtrl_Vtbl(w->actCtrl)->Delete(w->actCtrl, 1);
		w->actCtrl = NULL;
	}
	if (!cls || !*cls) {
		return;
	}
	CHECK(g_pActClassList);
	cActClass *c = cActClassList_Get(g_pActClassList, cls);
	if (!c) {
		Con_Warnf(WL_NOCTRL_WARN, cls);
	} else {
		cActCtrl *mem = operator_new(sizeof(cActCtrl));
		w->actCtrl = mem ? cActCtrl_ctor(mem, self, c) : NULL;
		created = 1;
	}
	if (w->actCtrl && !Entity_IsType(self, TYEN_CAR)) {
		cPrimitive_SetSize(&w->prim, w->actCtrl->cls->vRadius);
	}
	if (created) {
		cPy_Function_AddParamS(&w->actCtrl->cls->onCreate, self->name);
		cPy_Function_Execute(&w->actCtrl->cls->onCreate, 0);
	}
}

void cWithLifeEntity_SetActive(cEntity *self, byte on) {
	cWithLifeEntity *w = WLife(self);
	byte was = self->active;

	cArrowIndicator_SetModels(&w->arrow, on);
	cObjEntity_SetActive(self, on);
	if (was != on && on && w->actCtrl) {
		cActCtrl_Tick(w->actCtrl, 0.005f);
	}
}

void cWithLifeEntity_SetUsrName(cEntity *e, const char *name) {
	cString_Set(&WLife(e)->usrName, name == ACT_EMPTY ? NULL : name);
}

/* not for an entity that is gone (in the dead list) */
void cWithLifeEntity_SetEmbeddedFX(cEntity *self, const char *fxType) {
	if (self->deadNode.target) {
		return;
	}
	EmbeddedFXChar_Create(self, fxType, &WLife(self)->obj.fx);
	FXScene_Create(self, fxType);
	cObjEntity_SetEmbeddedFX(self, fxType);
}

void cWithLifeEntity_SetPos(cEntity *self, const vec3_t *pos) {
	cObjEntity_SetPos(self, pos);
	Grid_Add(&g_pWorld->grid, self);
}

/* the burner node (CHECK line 0x4e of Nto1Links.h) linked to the attacker */
void cWithLifeEntity_SetBurner(cEntity *e, cEntity *attacker) {
	Nto1Node *node = &WLife(e)->burner;

	CHECK(node->data);
	Nto1Node_Unlink(node);
	Nto1Node_LinkTo(&attacker->links, node);
}

void cWithLifeEntity_Update(cEntity *self) {
	cWithLifeEntity *w = WLife(self);

	cObjEntity_UpdateTimer(self);
	float dt = cTimer_GetDelta(&w->obj.timer);
	if (WL_MIN_DT >= dt) {
		return;
	}
	cSndInterface_Tick(&w->hitSounds, dt);
	cSndInterface_Tick(&w->lifeSounds, dt);
	if (w->actCtrl) {
		cActCtrl_Tick(w->actCtrl, dt);
	}
	cObjEntity_Update(self);
	cArrowIndicator_Update(&w->arrow);
}

byte cWithLifeEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	cWithLifeEntity *w = WLife(self);

	if (!*name) {
		return 0;
	}
	if (w->actCtrl && ActCtrl_Vtbl(w->actCtrl)->VarS(w->actCtrl, name, s, set)) {
		return 1;
	}
	if (cArrowIndicator_VarS(&w->arrow, name, s, set)) {
		return 1;
	}
	if (!strcmp(name, "Descriptor")) {
		if ((byte)set) {
			cWithLifeEntity_SetUsrName(self, *s);
		} else {
			*s = w->usrName ? w->usrName : ACT_EMPTY;
		}
		return 1;
	}
	if (!(byte)set && !strcmp(name, "Attacker")) {
		*s = (char *)Nto1Node_TargetName(&w->attack.hitBy);
		return 1;
	}
	if (!crt_strncmp("Hit_", name, 4) && cVehicleAttack_VarS(&w->attack, name + 4, s, set)) {
		return 1;
	}
	if (!strcmp(name, "ActCtrl")) {
		if ((byte)set) {
			WithLife_Vtbl(self)->SetActCtrl(self, *s);
		} else {
			*s = w->actCtrl ? w->actCtrl->cls->name : ACT_EMPTY;
		}
		return 1;
	}
	if (!strcmp(name, "HeadModel")) {
		if ((byte)set) {
			cString_Set(&w->headModel, *s);
		} else {
			*s = w->headModel;
		}
		return 1;
	}
	if (!strcmp(name, "Pyromaniac")) {
		if ((byte)set) {
			cWithLifeEntity_SetBurner(self, World_FindEntity(*s));
		} else {
			cEntity *b = w->burner.target;
			*s = b ? b->name : ACT_EMPTY;
		}
		return 1;
	}
	if ((byte)set) {
		if (!strcmp(name, "OnDeath")) {
			cPy_Function_SetFunc(&w->onDeath, *s);
			return 1;
		}
		if (!strcmp(name, "OnDamage")) {
			cPy_Function_SetFunc(&w->onDamage, *s);
			return 1;
		}
	}
	if (Snd_Vtbl(&w->hitSounds)->VarS(&w->hitSounds, name, s, set) ||
		Snd_Vtbl(&w->lifeSounds)->VarS(&w->lifeSounds, name, s, set)) {
		return 1;
	}
	return cObjEntity_VarS(self, name, s, set);
}

byte cWithLifeEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	cWithLifeEntity *w = WLife(self);

	if (!*name) {
		return 0;
	}
	if (w->actCtrl && ActCtrl_Vtbl(w->actCtrl)->VarV(w->actCtrl, name, x, y, z, set)) {
		return 1;
	}
	if (!crt_strncmp("Hit_", name, 4) && cVehicleAttack_VarV(&w->attack, name + 4, x, y, z, set)) {
		return 1;
	}
	if (!strcmp(name, "Vel")) {
		if ((byte)set) {
			vec3_t v;
			v.x = (float)*x;
			v.y = (float)*y;
			v.z = (float)*z;
			Obj_Vtbl(self)->SetVel(self, &v);
		} else {
			*x = w->obj.vel.x;
			*y = w->obj.vel.y;
			*z = w->obj.vel.z;
		}
		return 1;
	}
	if (Snd_Vtbl(&w->hitSounds)->VarV(&w->hitSounds, name, x, y, z, set) ||
		Snd_Vtbl(&w->lifeSounds)->VarV(&w->lifeSounds, name, x, y, z, set)) {
		return 1;
	}
	return cObjEntity_VarV(self, name, x, y, z, set);
}

byte cWithLifeEntity_IsEMIed(cEntity *self) {
	(void)self;
	return 0;
}

void cWithLifeEntity_Vf48(cEntity *self) {
	(void)self;
}
