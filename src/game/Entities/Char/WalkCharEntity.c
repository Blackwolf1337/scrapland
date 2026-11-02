/*
 * Entities/Char/WalkCharEntity.cpp of the game: the walking character (include/scrap/walkcharentity.h).
 */
#include "scrap/chartype.h"
#include "scrap/actions.h"
#include "scrap/arrowindicator.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/fxelements.h"
#include "scrap/missileentity.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

static inline cActClass *WCE_Class(const cWalkCharEntity *w) {
	return w->wl.actCtrl->cls;
}

static inline cAction *WCE_Action0(const cWalkCharEntity *w) {
	return w->wl.actCtrl->layers[0].action;
}

/* the turn of the head of the controller towards ang (x, y) in dt and the camera of the class
 * */
void cActCtrl_TurnHead(void *ctrl, const float *ang, float dt);
void cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e);

cEntity *cWalkCharEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cEntity *self = mem;
	cWalkCharEntity *w = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->vftable = CWALKCHARENTITY_VFTABLE;
	Nto1Node_Init(&w->collide);
	w->sphereBox.flags = 0;
	w->sphereBox.wait = 0;
	w->sphereBox.id = 0;
	cPy_Function_ctor(&w->onEnergyFull);
	Nto1Node_Init(&w->reposHead);
	cWalkCharControl_ctor(&w->control);
	Nto1Node_Init(&w->listen);
	Nto1Node_Init(&w->victim);
	cPy_Function_ctor(&w->outSelect);
	cPy_Function_ctor(&w->abortSelect);
	w->ownerSlot = -1;
	w->view = (vec3_t){0.0f, 0.0f, 0.0f};
	w->viewVel = (vec3_t){0.0f, 0.0f, 0.0f};
	w->slotView = (vec3_t){0.0f, 0.0f, 0.0f};
	w->renderView = (vec3_t){0.0f, 0.0f, 0.0f};
	float r = FxElem_Rand(4.0, 2.0);
	self->mask |= 0x10;
	w->relaxTime = r;
	w->floorDist = 128.0f;
	w->falling = 0;
	w->walking = 0;
	w->displAng = 0.0f;
	w->unused_46c = 0.0f;
	w->lastMove = 0.0f;
	w->lastDisplAng = 0.0f;
	w->walkTime = 0.0f;
	w->walkEnd = NULL;
	w->fallVel = (vec3_t){0.0f, 0.0f, 0.0f};
	w->disableControl = 0;
	w->listen.data = self;
	w->victim.data = self;
	w->listenOn = 0;
	w->yModelDisp = 0.0f;
	w->move = 0.0f;
	w->energyUsed = 0.0f;
	w->wl.mass = 5000.0f;
	w->money = 0;
	w->moneyReward = 0;
	w->moved2 = 1000.0f;
	w->pushOn = 0;
	w->push = (vec3_t){0.0f, 0.0f, 0.0f};
	w->wl.prevLife = w->wl.life;
	w->energyBarDecr = 0.200000003f;
	w->energyTime = 0.0f;
	w->collide.data = self;
	w->abortTime = 0.0f;
	w->abortOpt = 0;
	w->headPitch = 0.0f;
	w->reposHead.target = self;
	w->viewLock = 1;
	w->viewLockF = 1.0f;
	w->hitFlash = 0.0f;
	w->slideTime = 0.0f;
	return self;
}

void cWalkCharEntity_dtor(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	self->vftable = CWALKCHARENTITY_VFTABLE;
	if (w->wl.actCtrl) {
		void *ai = self->ai;
		if (ai) {
			/* the deleting destructor of the AI */
			(*(void(**)(void *, uint32_t))*(void **)ai)(ai, 1);
		}
		self->ai = NULL;
	}
	cPy_Function_dtor(&w->abortSelect);
	cPy_Function_dtor(&w->outSelect);
	Nto1Node_Done(&w->victim);
	Nto1Node_Done(&w->listen);
	Nto1Node_Done(&w->control.target);
	Nto1Node_Done(&w->reposHead);
	cPy_Function_dtor(&w->onEnergyFull);
	Nto1Node_Done(&w->collide);
	cWithLifeEntity_dtor(self);
}

cEntity *cWalkCharEntity_Delete(cEntity *self, uint32_t flags) {
	cWalkCharEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CWALKCHARENTITY_SIZE);
	}
	return self;
}

void cWalkCharEntity_ActionTick(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cActCtrl *ctrl = w->wl.actCtrl;

	if (!ctrl) {
		return;
	}
	int slot = w->ownerSlot;
	if ((g_EnergyBarActive || slot < 0 || slot >= 4) && w->control.prevAct != w->control.act) {
		cActClass *cls = ctrl->cls;
		if (w->control.act) {
			cPy_Function_AddParamS(&cls->actionPress, self->name);
			cPy_Function_Execute(&WCE_Class(w)->actionPress, 0);
		} else {
			cPy_Function_AddParamS(&cls->actionRelease, self->name);
			cPy_Function_Execute(&WCE_Class(w)->actionRelease, 0);
		}
	}
	if (!w->control.act3) {
		if (!w->control.act && w->control.act2 && !w->control.prevAct2 && cWalkCharEntity_CanPossess(self, 0)) {
			cPy_Function_AddParamS(&WCE_Class(w)->possessionPress, self->name);
			cActClass *cls = WCE_Class(w);
			cPy_Function_AddParamS(&cls->possessionPress, Nto1Node_TargetName(&w->victim));
			cPy_Function_Execute(&WCE_Class(w)->possessionPress, 0);
		}
		if (!w->control.act3) {
			goto energy;
		}
	}
	if (!w->control.prevAct3 && !ChType_ActPriority(w->wl.actCtrl)) {
		cEntity *t;
		byte ok;
		if (w->listenOn && (t = WChar_Listen(self)) != NULL) {
			ok = t->vftable->IsType(t, TYEN_WITH_LIFE);
		} else {
			t = WChar_Victim(self);
			if (!t || !t->vftable->IsType(t, TYEN_WITH_LIFE)) {
				goto energy;
			}
			ok = WLife(t)->arrow.alwaysUse;
		}
		if (ok) {
			if (WLife(t)->arrow.canPhoto) {
				cPy_Function_AddParamS(&WCE_Class(w)->photoPress, self->name);
				cPy_Function_Execute(&WCE_Class(w)->photoPress, 0);
			} else {
				cLocalLogic *l = t->lLogic;
				if (!l || l->vftable->Vf02(l)) {
					cArrowIndicator_Execute(&WLife(t)->arrow, self);
				}
			}
		}
	}
energy:
	if (!g_EnergyBarActive) {
		w->energyUsed = 1.0f;
		return;
	}
	float time = w->energyTime;
	if (time > 0.0f) {
		float used = dt / time + w->energyUsed;
		w->energyUsed = used;
		if (used >= 1.0f) {
			w->energyUsed = 1.0f;
			if (cPy_Function_IsValid(&w->onEnergyFull)) {
				cPy_Function_AddParamS(&w->onEnergyFull, self->name);
				cPy_Function_Execute(&w->onEnergyFull, 0);
			}
		}
	} else {
		float used = w->energyUsed;
		int i = (int)(used - 0.999899983f);
		int lo = 0;
		if (i <= 0) {
			lo = i;
		}
		used = used - dt / g_EnergyRechargeTime;
		w->energyUsed = used;
		if ((float)lo >= used) {
			w->energyUsed = (float)lo;
		}
	}
}

void cWalkCharEntity_UpdateModel(cEntity *self) {
	vec3_t n, d, pos;
	float head[2];
	cWalkCharEntity *w = WChar(self);
	cObjEntity *o = &w->wl.obj;

	vec3_t ang = o->renderAng;
	d.x = w->renderView.x - o->renderAng.x;
	d.y = w->renderView.y - o->renderAng.y;
	d.z = w->renderView.z - o->renderAng.z;
	Vec3_NormAng(&n, &d);
	float dt = cTimer_GetDelta(&o->timer);
	if (!(0.00000099999999747f >= dt)) {
		CGameModel_SetRot(&o->model, &ang);
		pos.x = o->renderPos.x + 0.0f;
		pos.y = o->renderPos.y + w->yModelDisp;
		pos.z = o->renderPos.z + 0.0f;
		CGameModel_SetPos(&o->model, &pos);
		cActCtrl *ctrl = w->wl.actCtrl;
		if (ctrl) {
			head[0] = n.x;
			head[1] = w->viewLockF * n.y;
			cActCtrl_TurnHead(ctrl, head, dt);
			if (o->cam) {
				cActClassCam_Update(&WCE_Class(w)->cam, o->cam, self);
			}
		}
		struct cEmbeddedFX *fx = o->fx;
		if (fx) {
			fx->vftable->Tick(fx);
		}
	}
}

void cWalkCharEntity_UpdateViewLock(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cActCtrl *ctrl = w->wl.actCtrl;

	if (ctrl && ctrl->cls) {
		byte b = ctrl->cls->strafeChar;
		byte old = w->viewLock;
		w->viewLock = b;
		if (b && (g_WalkCameraCenter == 0.0f || w->ownerSlot == -1 || w->control.lock)) {
			b = 1;
		} else {
			b = 0;
		}
		w->viewLock = b;
		if (!old && b) {
			w->walking = 1;
		}
	}
	float target = 0.0f;
	if (w->viewLock) {
		target = 1.0f;
	}
	Float_Approach(&w->viewLockF, target, dt + dt);
}

byte cWalkCharEntity_CanPossess(cEntity *self, byte checkClass) {
	cWalkCharEntity *w = WChar(self);

	if (!(0.0f >= w->energyUsed)) {
		return 0;
	}
	cEntity *v = WChar_Victim(self);
	if (!v || !w->wl.actCtrl || ChType_ActPriority(w->wl.actCtrl)) {
		return 0;
	}
	if (!v->vftable->IsType(v, TYEN_WALKCHAR)) {
		return 0;
	}
	if (checkClass) {
		cActCtrl *vctrl = ChType_ActCtrl(v);
		if (!vctrl->cls->canBePossessed) {
			return 0;
		}
	}
	return 1;
}

void cWalkCharEntity_UnlinkCollide(cEntity *self) {
	Nto1Node_UnlinkChecked(&WChar(self)->collide);
}

/* ---- the rest of WalkCharEntity.cpp ---- */

#define WCE_WALKCHAR "WalkChar"	/* "WalkChar", the class name */
#define WCE_RELAX "Relax"
#define WCE_RELAXALARM "RelaxAlarm"

#define WCE_MIN_DT 9.99999997e-07f
#define WCE_MIN_MOVE2 0.00249999994f		/* rva 0x3b0834: a smaller walk step is undone */
#define WCE_MIN_INPUT2 0.0400000028f
#define WCE_MAX_SPEED 14000.0f			/* rva 0x3b0880: faster is an error of the walk */
#define WCE_KMH 28.0f				/* rva 0x3b0878, 0x3b0870 (double) */
#define WCE_PITCH_FLY 1.57079637f
#define WCE_PITCH_WALK 1.04719758f
#define WCE_MAX_FALL 10000.0f			/* rva 0x3a5ac0, 0x3b0898 */

/* the WithLife entity hit test of the walk */
#define WCE_HIT_LISTS 0x11u

/* the name of the action of layer 0 starts with s (n characters) */
static inline byte WCE_Action0Is(const cWalkCharEntity *w, const char *s, size_t n) {
	return !crt_strncmp(cActCtrl_GetActionName(w->wl.actCtrl, 0), s, n);
}

/* the alarm flags of the world the alarm actions follow */
static inline byte WCE_AlarmOn(void) {
	return g_pWorld->alarm.hasGuaranteed || g_pWorld->alarm.on;
}

/* World.h, inlined: the entity of player slot 0 */
static inline cEntity *WCE_Usr0(void) {
	return cWorld_GetUsrEntity(g_pWorld, 0);
}

/* the push of an AI moves it, once */
static inline void WCE_ApplyPush(cWalkCharEntity *w, float dt) {
	vec3_t *pos = &w->wl.obj.base.pos;

	if (w->ownerSlot == -1 && w->pushOn) {
		pos->y = w->push.y * dt + pos->y;
		pos->z = w->push.z * dt + pos->z;
		pos->x = pos->x + w->push.x * dt;
		w->push.x = 0.0f;
		w->push.y = 0.0f;
		w->push.z = 0.0f;
	}
}

/* the friction of the class over dt, the stop below 2 and the speed limit (the walk and the flight) */
static inline void WCE_Friction(cWalkCharEntity *w, float dt, float maxSpeed) {
	vec3_t *v = &w->wl.obj.vel;
	float f = WCE_Class(w)->flyFric;

	v->x = v->x - dt * v->x * f;
	v->y = v->y - v->y * dt * f;
	v->z = v->z - v->z * dt * f;
	float s = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	if (2.0f > s) {
		v->x = 0.0f;
		v->y = 0.0f;
		v->z = 0.0f;
	}
	if (s > maxSpeed) {
		float k = maxSpeed / s;
		v->x = v->x * k;
		v->y = k * v->y;
		v->z = k * v->z;
	}
}

void Nto1Node_UnlinkChecked(Nto1Node *node) {
	CHECK(node->data);
	Nto1Node_Unlink(node);
}

void cWalkCharEntity_Touch(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal) {
	cWalkCharEntity *w = WChar(self);
	vec3_t p, epos;

	(void)point;
	(void)normal;
	if (!w->wl.actCtrl || !(e->mask & 0x11)) {
		return;
	}
	CHECK(e->vftable->IsType(e, TYEN_WITH_LIFE));
	p = self->pos;
	byte xz = !w->falling && !WCE_Class(w)->canFly;
	const vec3_t *from = e->vftable->GetPos(e, &epos);
	cPrimitive_UnelasticDisp(&w->wl.prim, &p, from, &WChar(e)->wl.prim, xz);
	w->wl.obj.vel.x = w->wl.obj.vel.x + (p.x - self->pos.x);
	w->wl.obj.vel.y = (p.y - self->pos.y) + w->wl.obj.vel.y;
	w->wl.obj.vel.z = (p.z - self->pos.z) + w->wl.obj.vel.z;
}

/* the push of a WalkChar the character ran into: dir normalized times CharUsrPushSpeed */
static void WCE_PushHit(cEntity *hit, const vec3_t *dir) {
	vec3_t n, p;
	const vec3_t *r = Vec3_NormalizeTo(&n, dir);

	p.x = r->x * g_CharUsrPushSpeed;
	p.y = r->y * g_CharUsrPushSpeed;
	p.z = r->z * g_CharUsrPushSpeed;
	cWalkCharEntity_SetPush(hit, &p);
}

void cWalkCharEntity_Walk(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t old, t, a, b;
	cEntity *hit;

	if (!w->wl.actCtrl) {
		return;
	}
	byte fly = WCE_Class(w)->canFly;
	WChar_Vtbl(self)->Turn(self, dt);
	old = *pos;
	WCE_ApplyPush(w, dt);
	WChar_Vtbl(self)->Idle(self, dt);
	if (fly) {
		WChar_Vtbl(self)->Fly(self, dt);
	} else {
		WChar_Vtbl(self)->Fall(self, dt);
	}
	WChar_Vtbl(self)->WalkAnim(self, dt);
	float dx = old.x - pos->x;
	float dy = old.y - pos->y;
	float dz = old.z - pos->z;
	float r = WCE_Class(w)->wRadius;
	float d2 = dx * dx + dy * dy + dz * dz;
	w->moved2 = d2;
	if (WCE_MIN_MOVE2 > d2) {
		Nto1Node_UnlinkChecked(&w->collide);
		*pos = old;
		goto speed;
	}
	if (fly) {
		Collision_Slide(&old, pos, r, &t, &w->sphereBox);
		*pos = t;
	} else if (w->falling) {
		byte slid = Collision_Slide(&old, pos, r, &t, &w->sphereBox);
		if (slid) {
			w->slideTime = w->slideTime + dt;
		} else {
			w->slideTime = w->slideTime - dt / 1.5f;
		}
		*pos = t;
	} else {
		w->slideTime = 0.0f;
		if (pos->y == old.y) {
			t = old;
		} else {
			b.x = old.x;
			b.y = pos->y;
			b.z = old.z;
			Collision_Slide(&old, &b, r, &t, NULL);
		}
		a = t;
		pos->y = t.y;
		Collision_Slide2D(&a, pos, r, &t, &w->sphereBox);
		*pos = t;
	}
	w->wl.obj.vel.x = 0.0f;
	w->wl.obj.vel.y = 0.0f;
	w->wl.obj.vel.z = 0.0f;
	hit = NULL;
	if (!cWorld_TestSphereEntities(g_pWorld, self, WCE_HIT_LISTS, &old, w->wl.prim.radius, &hit, 0, pos)) {
		Nto1Node_UnlinkChecked(&w->collide);
		goto speed;
	}
	cEntity_AddLink(hit, &w->collide);
	if (hit->vftable->IsType(hit, WCE_WALKCHAR)) {
		if (w->ownerSlot != -1) {
			a.x = -w->wl.obj.vel.x;
			a.y = -w->wl.obj.vel.y;
			a.z = -w->wl.obj.vel.z;
			WCE_PushHit(hit, &a);
		} else if (w->pushOn) {
			vec3_t sp, hp;
			const vec3_t *s = self->vftable->GetPos(self, &sp);
			const vec3_t *h = hit->vftable->GetPos(hit, &hp);
			a.x = h->x - s->x;
			a.y = h->y - s->y;
			a.z = h->z - s->z;
			WCE_PushHit(hit, &a);
		}
	}
	pos->x = w->wl.obj.vel.x + pos->x;
	pos->y = w->wl.obj.vel.y + pos->y;
	pos->z = w->wl.obj.vel.z + pos->z;
	if (Engine_TestSphere(pos, pos, r, NULL, NULL)) {
		*pos = old;
	} else if (cWorld_TestSphereEntities(g_pWorld, self, WCE_HIT_LISTS, pos, w->wl.prim.radius, &hit, 1, 0)) {
		*pos = old;
		cEntity_AddLink(hit, &w->collide);
	}
speed:;
	float inv = 1.0f / dt;
	w->wl.obj.vel.y = (pos->y - old.y) * inv;
	w->wl.obj.vel.z = (pos->z - old.z) * inv;
	w->wl.obj.vel.x = (pos->x - old.x) * inv;
	vec3_t *v = &w->wl.obj.vel;
	float s = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	if (s > WCE_MAX_SPEED) {
		double prev = (double)(float)((__builtin_sqrt((double)w->moved2) / (double)dt) / (double)WCE_KMH);
		Con_Verbosef("Character %s, was trying to run at %f Km/h.\n Previous speed %f Km/h.\n", self->name,
			(double)(s / WCE_KMH), prev);
		*pos = old;
		v->x = 0.0f;
		v->y = 0.0f;
		v->z = 0.0f;
	}
	WChar_Vtbl(self)->ActionTick(self, dt);
	cWalkCharControl_Latch(&w->control, self, dt);
	w->pushOn = 0;
}

void cWalkCharEntity_Turn(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return;
	}
	float t = w->control.hRot + w->control.hRot;
	w->viewVel.y = t;
	w->view.y = t * dt + w->view.y;
	t = w->control.vRot + w->control.vRot;
	w->viewVel.x = t;
	float x = t * dt + w->view.x;
	w->view.x = x;
	if (WCE_Class(w)->canFly) {
		if (x > WCE_PITCH_FLY) {
			w->view.x = WCE_PITCH_FLY;
		}
		if (-WCE_PITCH_FLY > w->view.x) {
			w->view.x = -WCE_PITCH_FLY;
		}
	} else {
		if (x > WCE_PITCH_WALK) {
			w->view.x = WCE_PITCH_WALK;
		}
		if (-WCE_PITCH_WALK > w->view.x) {
			w->view.x = -WCE_PITCH_WALK;
		}
	}
}

void cWalkCharEntity_PlayWalk(cEntity *self, const char *init, const char *loop, const char *end) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl || cActCtrl_IsActive(w->wl.actCtrl, init) || cActCtrl_IsActive(w->wl.actCtrl, loop)) {
		return;
	}
	cActCtrl_SetDefLoop(w->wl.actCtrl, loop, 0);
	if (!*init || !cActCtrl_PlayNew(w->wl.actCtrl, init, 0, 1.0f)) {
		cActCtrl_PlayNew(w->wl.actCtrl, loop, 0, 1.0f);
	}
	w->walkEnd = cActClass_GetAction((cActClass *)WCE_Class(w), end);
}

void cWalkCharEntity_UpdateHitFlash(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (self->active && w->wl.life > 0 && w->wl.prevLife > w->wl.life) {
		w->hitFlash = 1.5f;
	}
	if (!(w->hitFlash > 0.0f)) {
		return;
	}
	uint32_t fx = CGameModel_GetEffect(&w->wl.obj.model, NULL);
	if (fx != 0 && fx != 6) {
		return;
	}
	float a;
	float hf = w->hitFlash;
	if (!(hf >= 1.125f)) {
		float f = hf / 1.125f;
		a = (3.0f - (f + f)) * (f * f);
	} else {
		a = 1.0f;
	}
	Obj_Vtbl(self)->SetEffect(self, 6);
	Obj_Vtbl(self)->SetEffectParam(self, 0, a);
	float t = w->hitFlash - dt;
	w->hitFlash = t;
	if (!(0.0f < t)) {
		Obj_Vtbl(self)->SetEffect(self, 0);
	}
}

/* the AI of each class name, cAIChar for the others */
typedef void *(*wceAICtor_t)(void *mem, cEntity *e, int *arg);
static const struct {
	const char *cls;
	uint32_t size;
	wceAICtor_t ctor;
} wce_ais[] = {
	{"Sentinel", sizeof(cAISentinel), cAISentinel_ctor},
	{"Gear", sizeof(cAIGear), cAIGear_ctor},
	{"Nurse", sizeof(cAINurse), cAINurse_ctor},
	{"BankDirector", sizeof(cAIBankDirector), cAIBankDirector_ctor},
	{"Mayor", sizeof(cAIMayor), cAIMayor_ctor},
	{"Police", sizeof(cAIPolice), cAIPolice_ctor},
	{"Bishop", sizeof(cAIBishop), cAIBishop_ctor},
	{"Messenger", sizeof(cAIMessenger), cAIMessenger_ctor},
	{"BankMaster", sizeof(cAIBankMaster), cAIBankMaster_ctor},
	{"Desktop", sizeof(cAIDesktop), cAIDesktop_ctor},
	{"Human", sizeof(cAIHuman), cAIHuman_ctor},
	{NULL, sizeof(cAIChar), cAIChar_ctor}
};

void cWalkCharEntity_EnableAI(cEntity *self, byte on, int *arg) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return;
	}
	if (!on) {
		if (self->ai) {
			/* the deleting destructor of the AI */
			(*(void(**)(void *, uint32_t))*(void **)self->ai)(self->ai, 1);
		}
		self->ai = NULL;
		return;
	}
	if (self->ai) {
		return;
	}
	const char *name = WCE_Class(w)->name;
	int i = 0;
	while (wce_ais[i].cls && strcmp(name, wce_ais[i].cls)) {
		i++;
	}
	void *mem = operator_new(wce_ais[i].size);
	self->ai = mem ? wce_ais[i].ctor(mem, self, arg) : NULL;
}

static inline byte WCE_VarFloat(float *f, double *d, byte set) {
	if (set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
	return 1;
}

byte cWalkCharEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	cWalkCharEntity *w = WChar(self);
	byte s = (byte)set;

	if (!crt_strncmp("InCtrl_", name, 7) && cWalkCharControl_VarF(&w->control, name + 7, d, s)) {
		return 1;
	}
	if (!s && !strcmp(name, "SlidingAndFallingTime")) {
		*d = w->slideTime;
		return 1;
	}
	if (!strcmp(name, "EnergyBarDecr")) {
		if (!s) {
			*d = w->energyBarDecr;
		} else {
			float f = (float)*d;
			w->energyBarDecr = f;
			CHECK(f >= 0.0f && f <= 1.0f);
		}
		return 1;
	}
	if (!strcmp(name, "EnergyBarConstantDecr")) {
		if (!s) {
			*d = w->energyTime;
		} else {
			w->energyTime = (float)*d;
			CHECK(w->energyBarDecr >= 0.0f);
		}
		return 1;
	}
	if (!strcmp(name, "TimeToExtraRelax")) {
		return WCE_VarFloat(&w->relaxTime, d, s);
	}
	if (!strcmp(name, "EnergyToRecharge")) {
		if (s) {
			cWalkCharEntity_SetEnergyUsed(self, (float)*d);
		} else {
			*d = w->energyUsed;
		}
		return 1;
	}
	if (!s && !strcmp(name, "Battery")) {
		*d = cWalkCharEntity_GetEnergyFrac(self);
		return 1;
	}
	if (!strcmp(name, "Energy")) {
		if (s) {
			cWalkCharEntity_SetEnergy(self, (float)*d);
		} else {
			*d = 1.0f - w->energyUsed;
		}
		return 1;
	}
	if (!strcmp(name, "FloorDist")) {
		return WCE_VarFloat(&w->floorDist, d, s);
	}
	if (!strcmp(name, "DisplAng")) {
		if (s) {
			w->displAng = (float)((*d * M_PI_D) / DEG_PER_PI);
		} else {
			*d = ((double)w->displAng * DEG_PER_PI) / M_PI_D;
		}
		return 1;
	}
	if (!strcmp(name, "YModelDisp")) {
		if (s) {
			WChar_Vtbl(self)->SetYModelDisp(self, (float)*d);
		} else {
			*d = w->yModelDisp;
		}
		return 1;
	}
	return cWithLifeEntity_VarF(self, name, d, set);
}

void cWalkCharEntity_Fall(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t a, b;

	if (!w->wl.actCtrl) {
		return;
	}
	cActClass *cls = WCE_Class(w);
	byte falling = w->falling;
	float h = falling ? 0.0f : cls->minFallDist;
	float vy = w->wl.obj.vel.y;
	h = h - vy * dt;
	a.x = pos->x - 0.0f;
	a.y = pos->y - ((w->floorDist + h) - cls->sRadius);
	a.z = pos->z - 0.0f;
	b = a;
	if (WCE_MIN_MOVE2 > w->moved2 && !falling) {
		b.y = a.y + h;
	} else if (0.0f > vy || !falling) {
		byte r = Engine_TestSphere(pos, &a, cls->sRadius, &b, NULL);
		cWalkCharEntity_SetFalling(self, r == 0);
	}
	if (!w->falling) {
		float d = (b.y - a.y) - h;
		if (1.5 > __builtin_fabs((double)d)) {
			d = 0.0f;
		}
		w->wl.obj.vel.y = d / dt;
		if (WCE_Action0(w)->slideAsFly) {
			WChar_Vtbl(self)->Accel(self, dt);
		} else {
			w->wl.obj.vel.x = 0.0f;
			w->wl.obj.vel.z = 0.0f;
		}
	} else {
		float v = w->wl.obj.vel.y - g_CharGrav * dt;
		w->wl.obj.vel.y = v;
		if (-WCE_MAX_FALL > v) {
			w->wl.obj.vel.y = -WCE_MAX_FALL;
		} else if (WCE_Class(w)->canJump) {
			if (v > WCE_MAX_FALL) {
				w->wl.obj.vel.y = WCE_MAX_FALL;
			}
		} else if (v > 0.0f) {
			w->wl.obj.vel.y = 0.0f;
		}
		w->fallVel = w->wl.obj.vel;
	}
	pos->x = pos->x + w->wl.obj.vel.x * dt;
	pos->y = pos->y + w->wl.obj.vel.y * dt;
	pos->z = pos->z + w->wl.obj.vel.z * dt;
	WChar_Vtbl(self)->WalkSound(self, dt);
}

void cWalkCharEntity_Fly(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t *v = &w->wl.obj.vel;
	vec3_t in, tmp, a, hit;
	byte move;

	if (!w->wl.actCtrl) {
		goto stop;
	}
	cAction *act = WCE_Action0(w);
	if (act->useFlyDisp) {
		move = w->control.strafe * w->control.strafe + w->control.jump * w->control.jump +
			w->control.walk * w->control.walk > WCE_MIN_INPUT2;
	} else if (act->useFlyFall) {
		move = 0;
	} else {
		goto stop;
	}
	cActClass *cls = WCE_Class(w);
	float maxSpeed = cls->flySpeed;
	float f = cls->flyAccel * dt;
	in.x = -(w->control.strafe * f);
	in.y = w->control.walk * f;
	in.z = w->control.jump * f;
	const vec3_t *r = Vec3_RotateXY(&tmp, &in, w->view.x - WCE_PITCH_FLY, w->view.y);
	vec3_t add = *r;
	if (move) {
		v->x = v->x + add.x;
		v->y = v->y + add.y;
		v->z = v->z + add.z;
	}
	WCE_Friction(w, dt, maxSpeed);
	if (!move && WCE_Action0(w)->useFlyFall && CGameModel_IsVisible(Obj_Vtbl(self)->GetModel(self))) {
		cls = WCE_Class(w);
		a = *pos;
		a.y = pos->y - ((cls->minFallDist + w->floorDist) - cls->sRadius);
		Engine_TestSphere(pos, &a, WCE_Class(w)->sRadius, &hit, NULL);
		cls = WCE_Class(w);
		float k = (((pos->y - hit.y) - w->floorDist) + cls->sRadius) / cls->minFallDist;
		if (k > 0.0f) {
			v->y = v->y - cls->flyFallAccel * k * dt;
		}
	}
	goto end;
stop:
	v->x = 0.0f;
	v->y = 0.0f;
	v->z = 0.0f;
end:
	pos->y = v->y * dt + pos->y;
	pos->z = v->z * dt + pos->z;
	pos->x = pos->x + v->x * dt;
	WChar_Vtbl(self)->FlySound(self, dt);
}

cActCtrl *cWithLifeEntity_GetActCtrl(cEntity *self) {
	return WChar(self)->wl.actCtrl;
}

float cWalkCharEntity_GetEnergyFrac(cEntity *e) {
	float f = 0.999899983f - WChar(e)->energyUsed;

	return f - (float)(int)f;
}

vec3_t *cWalkCharEntity_GetRenderAng(cEntity *self, vec3_t *out) {
	*out = WChar(self)->renderView;
	return out;
}

vec3_t *cWalkCharEntity_GetSndAng(cEntity *self, vec3_t *out) {
	vec3_t t1, t2;
	float x = Obj_Vtbl(self)->GetRenderAng(self, &t1)->x;
	float y = WChar(self)->headPitch;
	float z = Obj_Vtbl(self)->GetRenderAng(self, &t2)->z;

	out->x = x;
	out->y = y;
	out->z = z;
	return out;
}

/* the class has alarm actions (GetActCtrl twice, like the inline of the original) and the alarm is on */
static inline byte WCE_Alarm(cEntity *self) {
	if (!WithLife_Vtbl(self)->GetActCtrl(self)) {
		return 0;
	}
	cActCtrl *ctrl = WithLife_Vtbl(self)->GetActCtrl(self);
	return ctrl->cls->relaxAlarm && WCE_AlarmOn();
}

const char *cWalkCharEntity_RelaxName(cEntity *self) {
	return WCE_Alarm(self) ? WCE_RELAXALARM : WCE_RELAX;
}

byte cWalkCharEntity_IsAlarm(cEntity *self) {
	return WCE_Alarm(self);
}

void cWalkCharEntity_GetCamera(cEntity *self, vec3_t *pos, vec3_t *ang) {
	cWalkCharEntity *w = WChar(self);

	cObjEntity_GetCamera(self, pos, ang);
	if (!w->wl.obj.cam) {
		ang->x = -w->renderView.x;
		ang->y = w->renderView.y + M_PI_F;
		ang->z = w->renderView.z;
	}
}

const char *cWalkCharEntity_GetClassName(cEntity *self) {
	(void)self;
	return WCE_WALKCHAR;
}

int cWalkCharEntity_GetOwnerSlot(cEntity *e) {
	return WChar(e)->ownerSlot;
}

float cWalkCharEntity_GetRadius(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (w->wl.actCtrl) {
		return WCE_Class(w)->wRadius;
	}
	return 150.0f;
}

int cWalkCharEntity_GetBatteries(cEntity *e) {
	return (int)(0.999899983f - WChar(e)->energyUsed);
}

static inline byte WCE_VarFlag(byte *b, int *i, byte set) {
	if (set) {
		*b = *i != 0;
	} else {
		*i = *b != 0;
	}
	return 1;
}

byte cWalkCharEntity_VarI(cEntity *self, const char *name, int *i, int set) {
	cWalkCharEntity *w = WChar(self);
	byte s = (byte)set;

	if (!s && !strcmp(name, "CanFly")) {
		*i = WCE_Class(w)->canFly != 0;
		return 1;
	}
	if (!strcmp(name, "Money")) {
		if (s) {
			w->money = *i;
		} else {
			*i = w->money;
		}
		return 1;
	}
	if (!s && !strcmp(name, "nBatteries")) {
		*i = cWalkCharEntity_GetBatteries(self);
		return 1;
	}
	if (!strcmp(name, "MoneyReward")) {
		if (s) {
			w->moneyReward = *i;
		} else {
			*i = w->moneyReward;
		}
		return 1;
	}
	if (!s) {
		if (self->ai && !strcmp(name, "AIState")) {
			cAIChar *ai = self->ai;
			*i = ai->vftable->GetState(&ai->base.base);
			return 1;
		}
		if (!strcmp(name, "ActionPriority")) {
			*i = WCE_Action0(w)->priority;
			return 1;
		}
	}
	if (!strcmp(name, "IgnoreUsrControl")) {
		if (s) {
			cWalkCharControl_Reset(&w->control, *i != 0);
		} else {
			*i = w->control.disabled != 0;
		}
		return 1;
	}
	if (!strcmp(name, "UsrControl")) {
		if (s) {
			cWalkCharEntity_SetUsrControl(self, *i);
		} else {
			*i = w->ownerSlot;
		}
		return 1;
	}
	if (!strcmp(name, "DisableControl")) {
		return WCE_VarFlag(&w->disableControl, i, s);
	}
	if (!strcmp(name, "AutoSelect")) {
		return WCE_VarFlag(&w->listenOn, i, s);
	}
	if (!strcmp(name, "isFalling")) {
		if (s) {
			cWalkCharEntity_SetFalling(self, *i != 0);
		} else {
			*i = w->falling != 0;
		}
		return 1;
	}
	if (!s) {
		if (!strcmp(name, "isWalking")) {
			*i = w->walking != 0;
			return 1;
		}
		if (!strcmp(name, "ActionPress")) {
			*i = w->control.act != 0;
			return 1;
		}
		if (!strcmp(name, "UsePress")) {
			*i = w->control.act3 != 0;
			return 1;
		}
	}
	return cWithLifeEntity_VarI(self, name, i, set);
}

byte cWalkCharEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_WALKCHAR)) {
		return 1;
	}
	return cWithLifeEntity_IsType(self, type);
}

byte cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit) {
	return cPrimitive_TestSegment(&WChar(self)->wl.prim, from, hit);
}

byte cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
	const vec3_t *prev) {
	return cPrimitive_TestSphere(&WChar(self)->wl.prim, exclude, center, radius, prev);
}

void cObjEntity_Vf41(cEntity *self, int a, int b, int c) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
}

byte cObjEntity_Vf42(cEntity *self, int a) {
	(void)self;
	(void)a;
	return 1;
}

void cObjEntity_Vf43(cEntity *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

void cWalkCharEntity_Tick(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!g_ActiveExtrapolation || w->wl.obj.cam) {
		w->slotView = w->view;
	} else {
		float iv = w->wl.obj.slot->interval;
		w->slotView.x = w->viewVel.x * iv + w->view.x;
		w->slotView.y = w->view.y + w->viewVel.y * iv;
		w->slotView.z = w->view.z + w->viewVel.z * iv;
	}
	if (!w->wl.obj.move.active) {
		WChar_Vtbl(self)->UpdateSelect(self, dt);
		if (w->disableControl) {
			WChar_Vtbl(self)->Idle(self, dt);
		} else {
			cWalkCharEntity_Walk(self, dt);
		}
	} else {
		if (w->wl.obj.move.viewAng) {
			goto tick;
		}
		WChar_Vtbl(self)->UpdateSelect(self, dt);
		WChar_Vtbl(self)->Idle(self, dt);
	}
	Grid_Add(&g_pWorld->grid, self);
tick:
	cWithLifeEntity_Tick(self, dt);
	w->abortOpt = w->wl.arrow.iOpt;
}

void cWalkCharEntity_UnlinkListen(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	Nto1Node_UnlinkChecked(&w->listen);
	w->abortTime = 0.0f;
}

/* the relax time again: lo .. hi at random */
static inline void WCE_RelaxTime(cWalkCharEntity *w, float lo, float hi) {
	int r = crt_rand();

	w->relaxTime = (float)((double)(r & 0x7fff) * 3.051850947599719e-05 * (double)(hi - lo) + (double)lo);
}

void cWalkCharEntity_Idle(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t ang, t1;

	if (!w->wl.actCtrl) {
		return;
	}
	cAction *act = WCE_Action0(w);
	float maxTurn = w->viewLock ? act->angRotLimit : M_2PI_F;
	float *body = &w->wl.obj.ang.y;
	float step = act->rotSpeed * dt;
	float a = NormAng((w->view.y - *body) + w->displAng);
	float old = *body;
	Float_Approach(body, old + a, step);
	float d = NormAng((w->view.y - *body) + w->displAng);
	if (d > maxTurn) {
		*body = (w->view.y + w->displAng) - maxTurn;
	} else if (-maxTurn > d) {
		*body = (w->view.y + w->displAng) + maxTurn;
	}
	w->wl.obj.rotVel.y = (old - *body) / dt;
	if (w->control.lockStart) {
		w->control.lockStart = 0;
		cWalkCharControl_SetTarget(&w->control, WChar_Victim(self));
		ang.x = 0.0f;
		ang.y = Obj_Vtbl(self)->GetAng(self, &t1)->y;
		ang.z = 0.0f;
		WChar_Vtbl(self)->SetViewAng(self, &ang);
		if (w->wl.obj.cam) {
			((cFollowCamEntity *)w->wl.obj.cam)->vSpeed = 1.10000002f;
			((cFollowCamEntity *)w->wl.obj.cam)->hSpeed = 1.10000002f;
		}
	}
	if (w->control.lock && !w->control.target.target) {
		cWalkCharControl_SetTarget(&w->control, WChar_Victim(self));
	}
	if (w->wl.obj.cam) {
		((cFollowCamEntity *)w->wl.obj.cam)->tiltTo = w->viewLock ? 0.0f : w->control.strafe;
	}
	byte turns;
	if (g_WalkCameraCenter == 0.0f || w->ownerSlot == -1 || w->control.lock) {
		turns = 1;
		if (__builtin_fabs((double)d) > (double)WCE_Class(w)->turnAngLimit && WCE_Action0Is(w, "Relax", 5)) {
			cActCtrl_Play(w->wl.actCtrl, WCE_Alarm(self) ? "TurnAlarm" : "Turn", 0, 1.0f);
		}
	} else {
		turns = 0;
	}
	byte relaxing = WCE_Action0Is(w, "Relax", 5);
	if (WCE_Alarm(self) && relaxing) {
		if (strcmp(cActCtrl_GetDefLoopName(w->wl.actCtrl), "RelaxAlarm")) {
			cActCtrl_SetDefLoop(w->wl.actCtrl, WCE_RELAXALARM, 0);
		}
		float t = w->relaxTime - dt;
		w->relaxTime = t;
		if (0.0f < t) {
			goto look;
		}
		cActCtrl_PlayNew(w->wl.actCtrl, "RelaxAlarm0", 0, 1.0f);
		float hi = WCE_Class(w)->minTimeToExtraRelax;
		WCE_RelaxTime(w, hi * 0.5f, hi);
		goto look;
	}
	if (!WCE_Alarm(self) && relaxing && cActCtrl_IsActive(w->wl.actCtrl, WCE_RELAXALARM)) {
		cActCtrl_PlayNew(w->wl.actCtrl, WCE_RELAX, 0, 2.0f);
		cActCtrl_SetDefLoop(w->wl.actCtrl, WCE_RELAX, 0);
		goto look;
	}
	if (cActCtrl_IsActive(w->wl.actCtrl, cWalkCharEntity_RelaxName(self))) {
		float t = w->relaxTime - dt;
		w->relaxTime = t;
		if (0.0f < t) {
			goto look;
		}
		char name[7] = "Relax0";
		if (turns && __builtin_fabs((double)d) > 0.10000000149011612) {
			cActCtrl_Play(w->wl.actCtrl, WCE_Alarm(self) ? "TurnAlarm" : "Turn", 0, 1.0f);
		} else {
			do {
				name[5] = (char)(crt_rand() % 5 + '0');
			} while (!cActCtrl_Play(w->wl.actCtrl, name, 0, 1.0f) && name[5] > '0');
		}
		WCE_RelaxTime(w, WCE_Class(w)->minTimeToExtraRelax, WCE_Class(w)->maxTimeToExtraRelax);
	} else if (!relaxing) {
		if (w->ownerSlot >= 0) {
			WCE_RelaxTime(w, WCE_Class(w)->minTimeToExtraRelax, WCE_Class(w)->maxTimeToExtraRelax);
		} else {
			w->relaxTime = WCE_Class(w)->minTimeToExtraRelax;
		}
	}
look:;
	float look = WCE_Action0(w)->autoFaceSpeed;
	cEntity *l = WChar_Listen(self);
	if (look > 0.0f && l) {
		vec3_t lp, mp, dir, n;
		const vec3_t *p = l->vftable->GetPos(l, &lp);
		const vec3_t *m = self->vftable->GetPos(self, &mp);
		dir.x = m->x - p->x;
		dir.y = m->y - p->y;
		dir.z = m->z - p->z;
		Vec3_GetRotAng(&ang, &dir);
		act = WCE_Action0(w);
		dir.x = (ang.x - w->view.x) - act->autoFaceOffsetV;
		dir.y = (ang.y - w->view.y) - act->autoFaceOffsetH;
		dir.z = (ang.z - w->view.z) - 0.0f;
		const vec3_t *r = Vec3_NormAng(&n, &dir);
		float inv = 1.0f / dt;
		w->viewVel.y = r->y * look * inv;
		w->viewVel.z = r->z * look * inv;
		w->viewVel.x = r->x * look * inv;
		w->view.x = w->view.x + dt * w->viewVel.x;
		w->view.y = w->view.y + w->viewVel.y * dt;
		w->view.z = w->view.z + w->viewVel.z * dt;
	}
}

void cWalkCharEntity_UpdateSelect(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return;
	}
	if (w->ownerSlot != -1) {
		cEntity *sel = ArrowIndicator_FindSel(self, w->viewLock ? w->view.y : w->wl.obj.ang.y);
		if (sel) {
			cEntity_AddLink(sel, &w->victim);
		} else {
			Nto1Node_UnlinkChecked(&w->victim);
		}
		if (w->listenOn) {
			Nto1Node_LinkAs(&w->listen, &w->victim);
		} else {
			ArrowIndicator_ClearSel();
		}
	}
	cEntity *l;
	if (cPy_Function_IsValid(&w->outSelect) && (l = WChar_Listen(self)) != NULL) {
		if (l->vftable->IsType(l, TYEN_WITH_LIFE)) {
			if (WChar(l)->wl.arrow.canUse &&
				ArrowIndicator_Visible(self, l, w->view.y, g_ArrowDeactivationRadius, NULL, 0, 0)) {
				goto abort;
			}
			cPy_Function_AddParamS(&w->outSelect, self->name);
			cPy_Function_AddParamS(&w->outSelect, Nto1Node_TargetName(&w->listen));
		} else {
			cPy_Function_AddParamS(&w->outSelect, self->name);
			cPy_Function_AddParamS(&w->outSelect, str_Empty);
		}
		cPy_Function_Execute(&w->outSelect, 1);
	}
abort:
	if (!cPy_Function_IsValid(&w->abortSelect)) {
		return;
	}
	w->abortTime = w->abortTime - dt;
	if (w->abortOpt != w->wl.arrow.iOpt || !cActCtrl_IsActive(w->wl.actCtrl, cWalkCharEntity_RelaxName(self))) {
		w->abortTime = g_TimeToAbortSelectChar;
	}
	cEntity *usr = WCE_Usr0();
	if (w->wl.arrow.canUse && !(0.0f >= w->abortTime) && usr) {
		vec3_t sp, up;
		const vec3_t *s = self->vftable->GetPos(self, &sp);
		const vec3_t *u = usr->vftable->GetPos(usr, &up);
		float dx = u->x - s->x;
		float dy = u->y - s->y;
		float dz = u->z - s->z;
		if (!(dx * dx + dy * dy + dz * dz > g_AbortSelectDistance * g_AbortSelectDistance)) {
			return;
		}
	}
	cPy_Function_AddParamS(&w->abortSelect, self->name);
	cPy_Function_Execute(&w->abortSelect, 1);
}

void cWalkCharEntity_SetActive(cEntity *self, byte on) {
	cWalkCharEntity *w = WChar(self);

	cWithLifeEntity_SetActive(self, on);
	if (w->ownerSlot != -1 && self->active) {
		g_pWorld->owners[w->ownerSlot] = self;
	}
	cWalkCharEntity_UpdateViewLock(self, 0.0f);
}

void cWalkCharEntity_SetAng(cEntity *self, const vec3_t *ang) {
	WChar_Vtbl(self)->SetViewAng(self, ang);
	cObjEntity_SetAng(self, ang);
}

void cWalkCharEntity_SetEnergy(cEntity *self, float energy) {
	if (0.0f > energy) {
		WChar(self)->energyUsed = 1.0f;
		return;
	}
	WChar(self)->energyUsed = 1.0f - energy;
}

void cWalkCharEntity_SetEnergyUsed(cEntity *self, float used) {
	if (used > 1.0f) {
		WChar(self)->energyUsed = 1.0f;
		return;
	}
	WChar(self)->energyUsed = used;
}

void cWalkCharEntity_SetEmbeddedFX(cEntity *self, const char *fxType) {
	FX_Character(self, fxType);
	cWithLifeEntity_SetEmbeddedFX(self, fxType);
}

void cWalkCharEntity_SetFalling(cEntity *self, byte on) {
	cWalkCharEntity *w = WChar(self);
	void *ctrl = w->wl.actCtrl;

	if (!ctrl || w->falling == on || WCE_Class(w)->canFly) {
		return;
	}
	w->falling = on;
	if (on) {
		cActCtrl_SetDefLoop(ctrl, "Fall", 0);
		if (!cActCtrl_Play(w->wl.actCtrl, "FallInit", 0, 1.0f)) {
			cActCtrl_Play(w->wl.actCtrl, "Fall", 0, 1.0f);
		}
		if (w->walking && w->wl.obj.vel.y > 0.0f) {
			w->wl.obj.vel.y = 0.0f;
			w->walking = 0;
			return;
		}
	} else {
		cActCtrl_SetDefLoop(w->wl.actCtrl, cWalkCharEntity_RelaxName(self), 0);
		if (!cActCtrl_Play(w->wl.actCtrl, "FallEnd", 0, 1.0f)) {
			cActCtrl_Play(w->wl.actCtrl, cWalkCharEntity_RelaxName(self), 0, 1.0f);
		}
		if (-g_FallSoundVel > w->fallVel.y) {
			cActClass *cls = WCE_Class(w);
			float vol = cls->fallSoundVol;
			const char *snd = cls->fallSound;
			if (vol > 0.0f && snd && *snd) {
				int v = Snd_Volume(vol);
				g_pSound->vftable->PlayAt(g_pSound, snd, &w->wl.obj.renderPos, v);
			}
		}
	}
	w->walking = 0;
}

void cWithLifeEntity_SetMass(cEntity *self, float mass) {
	CHECK(mass > 0.0f);
	WChar(self)->wl.mass = mass;
}

void cWalkCharEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag) {
	cWalkCharEntity *w = WChar(self);
	vec3_t root, floor;

	if (!file || !*file) {
		cObjEntity_SetModel(self, file, sx, sy, sz, flag);
		return;
	}
	w->yModelDisp = 0.0f;
	cObjEntity_SetModel(self, file, sx, sy, sz, flag);
	CGameModel *m = &w->wl.obj.model;
	CGameModel_NodePos(m, &root, CGameModel_FindRenderNode(m, "DC_Root"), 0);
	CGameModel_NodePos(m, &floor, CGameModel_FindRenderNode(m, "DC_Floor"), 0);
	w->camNode = CGameModel_FindRenderNode(m, "DC_Camera");
	w->floorDist = root.y - floor.y;
	WChar_Vtbl(self)->SetYModelDisp(self, w->yModelDisp);
}

void cWalkCharEntity_UpdateSlotRate(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	float target;

	if (w->ownerSlot != -1) {
		return;
	}
	if (CGameModel_IsVisible(Obj_Vtbl(self)->GetModel(self))) {
		target = 0.0500000007f;
	} else {
		float d = cWorld_GetMinCamDistSq(g_pWorld, &self->pos);
		target = g_CharProcessReduction * g_CharProcessReduction > d ? 0.0500000007f : 0.100000001f;
	}
	Float_Approach(&w->wl.obj.slot->minDiff, target, dt / 10.0f);
}

byte cWalkCharEntity_SetPush(cEntity *self, const vec3_t *push) {
	cWalkCharEntity *w = WChar(self);

	if (w->ownerSlot != -1 || w->pushOn) {
		return 0;
	}
	w->pushOn = 1;
	w->push = *push;
	return 1;
}

void cWalkCharEntity_ApplyPush(cEntity *self, float dt) {
	WCE_ApplyPush(WChar(self), dt);
}

void cWalkCharEntity_SetUsrControl(cEntity *self, int slot) {
	cWalkCharEntity *w = WChar(self);

	if (w->ownerSlot >= 0 && w->ownerSlot < 4) {
		cWalkCharControl_Clear(&w->control);
	}
	int old = w->ownerSlot;
	w->ownerSlot = slot;
	if (slot == -1) {
		w->wl.obj.slot->minDiff = 0.0500000007f;
		if ((uint32_t)old <= 3 && cWorld_GetUsrEntity(g_pWorld, old) == self) {
			g_pWorld->owners[old] = NULL;
			ArrowIndicator_ClearSel();
		}
	} else {
		w->wl.obj.slot->minDiff = 0.0166666675f;
		g_pWorld->owners[w->ownerSlot] = self;
	}
}

void cWalkCharEntity_SetVel(cEntity *self, const vec3_t *vel) {
	cWalkCharEntity *w = WChar(self);

	if (w->wl.obj.move.active) {
		return;
	}
	cObjEntity_SetVel(self, vel);
	if (w->wl.obj.vel.y > 0.0f) {
		cWalkCharEntity_SetFalling(self, 1);
	}
}

void cWalkCharEntity_SetViewAng(cEntity *self, const vec3_t *ang) {
	cWalkCharEntity *w = WChar(self);

	w->slotView = *ang;
	w->view = w->slotView;
	w->renderView = w->view;
	w->viewVel.x = 0.0f;
	w->viewVel.y = 0.0f;
	w->viewVel.z = 0.0f;
	cWalkCharControl_RestartCenter(&w->control);
}

void cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos) {
	vec3_t t, d, ang;
	const vec3_t *p = self->vftable->GetPos(self, &t);

	d.x = p->x - pos->x;
	d.y = p->y - pos->y;
	d.z = p->z - pos->z;
	Vec3_GetRotAng(&ang, &d);
	WChar_Vtbl(self)->SetViewAng(self, &ang);
}

void cWalkCharEntity_SetYModelDisp(cEntity *self, float y) {
	cWalkCharEntity *w = WChar(self);
	float d = w->yModelDisp - y;

	w->wl.obj.camOff.y = w->wl.obj.camOff.y - d;
	w->yModelDisp = w->yModelDisp - d;
	w->floorDist = d + w->floorDist;
}

void cWalkCharEntity_Accel(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *v = &w->wl.obj.vel;
	vec3_t in, tmp;

	if (!w->wl.actCtrl) {
		return;
	}
	cActClass *cls = WCE_Class(w);
	float maxSpeed = cls->flySpeed;
	float f = cls->flyFallAccel * dt;
	in.x = -(f * w->control.strafe);
	in.y = 0.0f;
	in.z = -(w->control.walk * f);
	const vec3_t *r = Vec3_RotateXY(&tmp, &in, 0.0f, w->view.y);
	vec3_t add = *r;
	if (w->control.walk * w->control.walk + w->control.strafe * w->control.strafe > WCE_MIN_INPUT2) {
		v->x = v->x + add.x;
		v->y = v->y + add.y;
		v->z = v->z + add.z;
	}
	WCE_Friction(w, dt, maxSpeed);
}

void cWalkCharEntity_Update(cEntity *self) {
	cWalkCharEntity *w = WChar(self);
	vec3_t base, d, n, t;
	matrix_t m;

	if (!w->wl.actCtrl) {
		return;
	}
	float dt = cTimer_GetDelta(&w->wl.obj.timer);
	cWalkCharEntity_UpdateSlotRate(self, dt);
	cWalkCharEntity_UpdateHitFlash(self, dt);
	cWithLifeEntity_Update(self);
	if (WCE_MIN_DT >= dt) {
		return;
	}
	float iv = w->wl.obj.slot->interval;
	float k = w->wl.obj.slot->time / iv;
	base = w->view;
	if (g_ActiveExtrapolation && !w->wl.obj.cam) {
		base.y = base.y + w->viewVel.y * iv;
		base.x = base.x + w->viewVel.x * iv;
		base.z = base.z + w->viewVel.z * iv;
	}
	d.x = w->slotView.x - base.x;
	d.y = w->slotView.y - base.y;
	d.z = w->slotView.z - base.z;
	const vec3_t *r = Vec3_NormAng(&n, &d);
	vec3_t a = *r;
	w->renderView.z = a.z * k + base.z;
	w->renderView.x = a.x * k + base.x;
	w->renderView.y = a.y * k + base.y;
	cWalkCharEntity_UpdateViewLock(self, dt);
	if (w->ownerSlot >= 0 && w->ownerSlot < 4) {
		cWalkCharControl_Update(&w->control, self, dt);
	}
	if (WCE_Action0(w)->anmConeView) {
		CGameModel *model = Obj_Vtbl(self)->GetModel(self);
		const matrix_t *mat = CGameModel_NodeMatrix(model, &m, w->camNode, 1, NULL, 0);
		w->headPitch = Matrix_GetRotAng(mat, &t)->y + M_PI_F;
	} else {
		w->headPitch = Obj_Vtbl(self)->GetRenderAng(self, &t)->y;
	}
}

/* the pitch or volume of the loop sound between its first two values by k (inv = 1 - k) */
static inline float WCE_SndMix(const float *v, float inv, float k) {
	return v[0] * inv + v[1] * k;
}

void cWalkCharEntity_FlySound(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cSndItem *s = PtrArray_Get(&w->wl.obj.snd, 0);
	vec3_t *v = &w->wl.obj.vel;

	(void)dt;
	if (!w->wl.actCtrl || !s) {
		return;
	}
	float sp = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	float k = sp / WCE_Class(w)->flySpeed;
	float c = 0.0f;
	if (!(0.0f > k)) {
		c = Float_MinSS(1.0f, k);
	}
	float inv = 1.0f - c;
	s->pitch = WCE_SndMix(s->pitches, inv, c);
	s->volume = WCE_SndMix(s->vols, inv, c);
}

void cWalkCharEntity_WalkSound(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cSndItem *s = PtrArray_Get(&w->wl.obj.snd, 0);

	if (!s || s->ints[0] != 1) {
		return;
	}
	Float_Approach(&s->times[0], w->move, dt / s->times[1]);
	float k = s->times[0];
	float inv = 1.0f - k;
	s->pitch = WCE_SndMix(s->pitches, inv, k);
	float mul = w->wl.obj.cam ? 1.0f : g_SndLoopCharNPCMul;
	s->volume = WCE_SndMix(s->vols, inv, k) * mul;
}

byte cWalkCharEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	cWalkCharEntity *w = WChar(self);
	byte st = (byte)set;

	if (st && !strcmp(name, "OnEnergyBarFull")) {
		cPy_Function_SetFunc(&w->onEnergyFull, *s);
		return 1;
	}
	if (!strcmp(name, "CollideEntity")) {
		if (!st) {
			*s = (char *)Nto1Node_TargetName(&w->collide);
			return 1;
		}
		if (!**s) {
			cWalkCharEntity_UnlinkCollide(self);
			return 1;
		}
		return 0;
	}
	if (!st) {
		if (!strcmp(name, "AutoSelectedEntity")) {
			*s = (char *)Nto1Node_TargetName(&w->victim);
			return 1;
		}
	} else {
		if (!strcmp(name, "AbortSelectFunc")) {
			cPy_Function_SetFunc(&w->abortSelect, *s);
			return 1;
		}
		if (!strcmp(name, "OutSelectFunc")) {
			cPy_Function_SetFunc(&w->outSelect, *s);
			return 1;
		}
	}
	if (!strcmp(name, "SelectedEntity")) {
		if (st) {
			Nto1Node_LinkByName(&w->listen, *s);
		} else {
			*s = (char *)Nto1Node_TargetName(&w->listen);
		}
		return 1;
	}
	return cWithLifeEntity_VarS(self, name, s, set);
}

byte cWalkCharEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	cWalkCharEntity *w = WChar(self);
	vec3_t t, d, ang;

	if ((byte)set && !strcmp(name, "ViewPos")) {
		float px = (float)*x;
		float py = (float)*y;
		float pz = (float)*z;
		const vec3_t *p = self->vftable->GetPos(self, &t);
		d.x = p->x - px;
		d.y = p->y - py;
		d.z = p->z - pz;
		Vec3_GetRotAng(&ang, &d);
		WChar_Vtbl(self)->SetViewAng(self, &ang);
		return 1;
	}
	if (!strcmp(name, "ViewAng")) {
		if (!(byte)set) {
			*x = ((double)w->view.x * DEG_PER_PI) / M_PI_D;
			*y = ((double)w->view.y * DEG_PER_PI) / M_PI_D;
			*z = ((double)w->view.z * DEG_PER_PI) / M_PI_D;
			return 1;
		}
		ang.x = (float)((*x * M_PI_D) / DEG_PER_PI);
		ang.y = (float)((*y * M_PI_D) / DEG_PER_PI);
		ang.z = (float)((*z * M_PI_D) / DEG_PER_PI);
		WChar_Vtbl(self)->SetViewAng(self, &ang);
		return 1;
	}
	return cWithLifeEntity_VarV(self, name, x, y, z, set);
}

/* the init action of a walk, "" when the class needs none (strafeInit) and one of the walk actions runs already */
static inline const char *WCE_Init(cEntity *self, byte strafeInit, byte(*running)(cEntity *),
	const char *init) {
	return strafeInit && running(self) ? str_Empty : init;
}

static byte WCE_StrafeOrMove(cEntity *self) {
	return cWalkCharEntity_IsStrafeAnim(self) || cWalkCharEntity_IsMoveAnim(self);
}

void cWalkCharEntity_WalkAnim(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cActCtrl *ctrl = w->wl.actCtrl;
	vec3_t *v = &w->wl.obj.vel;
	vec3_t *pos = &self->pos;
	float in[2], out[2], mx, mz;

	if (!ctrl) {
		return;
	}
	byte wasWalking = w->walking;
	byte strafeInit = ctrl->cls->skipInitOnChange;
	float s2 = w->control.strafe * w->control.strafe + w->control.walk * w->control.walk;
	float s = (float)__builtin_sqrt((double)s2);
	w->move = s;
	if (0.200000003f > s) {
		w->move = 0.0f;
	} else {
		float f = (s - 0.200000003f) / 0.800000012f;
		w->move = f;
		if (f > 1.0f) {
			w->move = 1.0f;
		}
	}
	if (w->falling) {
		float f = ctrl->cls->flyAccel * dt;
		in[0] = -(w->control.strafe * f);
		in[1] = -(w->control.walk * f);
		goto move;
	}
	float mv = w->move;
	byte stopping = w->walkTime > 0.0f && mv == 0.0f;
	float x;
	if (mv == 0.0f && !stopping) {
		if (!wasWalking) {
			goto still;
		}
		if (w->viewLock) {
			w->displAng = 0.0f;
		}
		if (w->walkEnd) {
			const char *loop = cActCtrl_GetDefLoopName(ctrl);
			if (!strcmp(cActCtrl_GetActionName(w->wl.actCtrl, 0), loop)) {
				cActCtrl_PlayAction(w->wl.actCtrl, w->walkEnd, 0, 1, 1.0f);
			} else {
				cActCtrl_PlayAction(w->wl.actCtrl, w->walkEnd, 0, 1, WCE_Class(w)->initEndWalkInterpFactor);
			}
		} else {
			cActCtrl_Play(w->wl.actCtrl, cWalkCharEntity_RelaxName(self), 0, 1.0f);
		}
		cActCtrl_SetDefLoop(w->wl.actCtrl, cWalkCharEntity_RelaxName(self), 0);
		WChar_Ctrl(w)->walkSpeed = 0.0f;
		w->walking = 0;
		goto still;
	}
	if (ctrl->layers[0].action->slideAsFly == 4) {
		w->displAng = 0.0f;
	} else {
		float a = (float)atan2((double)w->control.walk, (double)w->control.strafe);
		w->displAng = NormAng(WCE_PITCH_FLY - a);
	}
	if (stopping) {
		mv = w->lastMove;
		w->displAng = w->lastDisplAng;
	} else {
		w->lastDisplAng = w->displAng;
		w->lastMove = mv;
	}
	byte lock = w->viewLock;
	if (lock) {
		float da = w->displAng;
		if (da > 0.785398185f && 2.356194490192345 > (double)da) {
			switch (WCE_Action0(w)->slideAsFly) {
			case 1:
				w->displAng = WCE_PITCH_FLY;
				break;
			case 2:
				w->displAng = 0.785398185f;
				break;
			case 3:
			case 4:
				w->displAng = 0.0f;
				break;
			default:
				cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, cWalkCharEntity_IsBackOrMoveAnim,
					"StrRightInit"), "StrRight", "StrRightEnd");
				w->displAng = 0.0f;
			}
			x = mv;
			goto anim;
		}
	}
	if (lock) {
		float da = w->displAng;
		if (-0.785398185f > da && (double)da > -2.356194490192345) {
			switch (WCE_Action0(w)->slideAsFly) {
			case 1:
				w->displAng = -WCE_PITCH_FLY;
				break;
			case 2:
				w->displAng = -0.785398185f;
				break;
			case 3:
			case 4:
				w->displAng = 0.0f;
				break;
			default:
				cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, cWalkCharEntity_IsBackOrMoveAnim,
					"StrLeftInit"), "StrLeft", "StrLeftEnd");
				w->displAng = 0.0f;
			}
			x = mv;
			goto anim;
		}
	}
	if (lock && __builtin_fabs((double)w->displAng) > 1.5707963705062866) {
		cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, cWalkCharEntity_IsStrafeAnim, "BackInit"), "Back",
			"BackEnd");
		if (WCE_Action0(w)->slideAsFly != 4) {
			w->displAng = w->displAng + M_PI_F;
		}
		x = mv;
		goto anim;
	}
	if (!cActClass_GetAction((cActClass *)WCE_Class(w), "Run")) {
		cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, WCE_StrafeOrMove, "WalkInit"), "Walk", "WalkEnd");
		x = mv;
	} else if (mv > 0.699999988f) {
		if (cWalkCharEntity_IsAlarm(self)) {
			cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, WCE_StrafeOrMove, "SprintInit"), "Sprint",
				"SprintEnd");
		} else {
			cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, WCE_StrafeOrMove, "RunInit"), "Run",
				"RunEnd");
		}
		x = (mv - 0.699999988f) / 0.300000012f;
	} else {
		if (cWalkCharEntity_IsAlarm(self)) {
			cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, WCE_StrafeOrMove, "RunInit"), "Run",
				"RunEnd");
		} else {
			cWalkCharEntity_PlayWalk(self, WCE_Init(self, strafeInit, WCE_StrafeOrMove, "WalkInit"), "Walk",
				"WalkEnd");
		}
		x = mv / 0.699999988f;
	}
anim:
	w->walking = 1;
	WChar_Ctrl(w)->walkSpeed = Float_MinSS(1.0f, x);
still:
	in[0] = 0.0f;
	in[1] = 0.0f;
move:
	ctrl = w->wl.actCtrl;
	if (ctrl->layers[0].action->useDisp && !w->falling) {
		in[0] = ctrl->rootMove.x;
		in[1] = ctrl->rootMove.z;
	}
	ctrl->rootSum.x = 0.0f;
	ctrl->rootSum.y = 0.0f;
	ctrl->rootSum.z = 0.0f;
	ctrl->rootTime = 0.0f;
	if (w->falling) {
		const float *r = Vec2_Rotate(out, in, -w->view.y);
		float vx = v->x * dt;
		float vz = v->z * dt;
		float k = WCE_Class(w)->flyFric;
		mz = r[1] - k * vz;
		mx = r[0] - k * vx;
	} else {
		const float *r = Vec2_Rotate(out, in, -w->wl.obj.ang.y);
		mx = r[0];
		mz = r[1];
	}
	v->x = v->x + mx;
	v->y = v->y + 0.0f;
	v->z = v->z + mz;
	if (w->falling) {
		float vx = v->x;
		float vz = v->z;
		float sp = (float)__builtin_sqrt((double)(vz * vz + vx * vx));
		float max = WCE_Class(w)->flySpeed;
		if (sp > max) {
			float q = sp / max;
			v->x = vx / q;
			v->z = vz / q;
		}
	}
	pos->x = pos->x + mx * dt;
	pos->y = pos->y + dt * 0.0f;
	pos->z = pos->z + mz * dt;
	if (w->falling) {
		w->walkTime = 0.0f;
		return;
	}
	if (w->walking) {
		if (wasWalking) {
			w->walkTime = w->walkTime - dt;
		} else {
			w->walkTime = WCE_Class(w)->minTimeWalking;
		}
	}
}

byte cWalkCharEntity_IsMoveAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "Run", 3) || WCE_Action0Is(w, "Walk", 4) || WCE_Action0Is(w, "Sprint", 6);
}

byte cWalkCharEntity_IsBackOrMoveAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "Back", 4) || cWalkCharEntity_IsMoveAnim(self);
}

byte cWalkCharEntity_IsStrafeAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->wl.actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "StrRight", 8) || WCE_Action0Is(w, "StrLeft", 7);
}

/* the vtable of cWalkCharEntity */
const cWalkCharVtbl cWalkCharEntity_vftable = {
	.base.base.base.Update = cWalkCharEntity_Update,
	.base.base.base.Render = cEntity_Render,
	.base.base.base.UpdateOff = cWithLifeEntity_UpdateOff,
	.base.base.base.Delete = cWalkCharEntity_Delete,
	.base.base.base.GetClassName = cWalkCharEntity_GetClassName,
	.base.base.base.IsType = cWalkCharEntity_IsType,
	.base.base.base.GetPos = cObjEntity_GetPos,
	.base.base.base.SetPos = cWithLifeEntity_SetPos,
	.base.base.base.SetActive = cWalkCharEntity_SetActive,
	.base.base.base.TestLine = cWithLifeEntity_TestLine,
	.base.base.base.TestSphere = cWithLifeEntity_TestSphere,
	.base.base.base.Touch = cWalkCharEntity_Touch,
	.base.base.base.SetModel = cWalkCharEntity_SetModel,
	.base.base.base.GetTargetAngle = cObjEntity_GetTargetAngle,
	.base.base.base.AimWeapon = cEntity_AimWeapon,
	.base.base.base.SetEmbeddedFX = cWalkCharEntity_SetEmbeddedFX,
	.base.base.base.VarI = cWalkCharEntity_VarI,
	.base.base.base.VarF = cWalkCharEntity_VarF,
	.base.base.base.VarS = cWalkCharEntity_VarS,
	.base.base.base.VarV = cWalkCharEntity_VarV,
	.base.base.base.Vf20 = cEntity_Vf20,
	.base.base.base.Vf21 = cEntity_Vf21,
	.base.base.base.GetAI = cEntity_GetAI,
	.base.base.Tick = cWalkCharEntity_Tick,
	.base.base.UpdateModel = cWalkCharEntity_UpdateModel,
	.base.base.GetNodeSlot = cWithLifeEntity_GetNodeSlot,
	.base.base.GetRenderPos = cObjEntity_GetRenderPos,
	.base.base.GetRenderAng = cWalkCharEntity_GetRenderAng,
	.base.base.GetSndAng = cWalkCharEntity_GetSndAng,
	.base.base.GetViewPos = cObjEntity_GetRenderPos,
	.base.base.GetViewAng = cObjEntity_GetRenderAng,
	.base.base.GetSndPos = cWithLifeEntity_GetSndPos,
	.base.base.GetAng = cObjEntity_GetAng,
	.base.base.SetAng = cWalkCharEntity_SetAng,
	.base.base.GetVel = cObjEntity_GetVel,
	.base.base.SetVel = cWalkCharEntity_SetVel,
	.base.base.GetRotVel = cObjEntity_GetRotVel,
	.base.base.SetRotVel = cObjEntity_SetRotVel,
	.base.base.GetModel = cObjEntity_GetModel,
	.base.base.GetCamera = cWalkCharEntity_GetCamera,
	.base.base.GetRadius = cWalkCharEntity_GetRadius,
	.base.base.NetWrite = cEntity_NetWriteNop,
	.base.base.NetNeedsSend = cEntity_NetNeedsSendTrue,
	.base.base.NetRead = cEntity_NetReadNop,
	.base.base.SetEffect = cObjEntity_SetEffect,
	.base.base.SetEffectParam = cObjEntity_SetEffectParam,
	.base.base.Render = cWithLifeEntity_RenderNow,
	.base.CheckLife = cWithLifeEntity_CheckLife,
	.base.UnlinkListen = cWalkCharEntity_UnlinkListen,
	.base.OnDeath = cWithLifeEntity_OnDeath,
	.base.OnDamage = cWithLifeEntity_OnDamage,
	.base.SetMass = cWithLifeEntity_SetMass,
	.base.GetAction = cWithLifeEntity_GetAction,
	.base.GetActCtrl = cWithLifeEntity_GetActCtrl,
	.base.SetActCtrl = cWithLifeEntity_SetActCtrl,
	.base.IsEMIed = cWithLifeEntity_IsEMIed,
	.EnableAI = cWalkCharEntity_EnableAI,
	.UpdateSelect = cWalkCharEntity_UpdateSelect,
	.Turn = cWalkCharEntity_Turn,
	.Idle = cWalkCharEntity_Idle,
	.Fall = cWalkCharEntity_Fall,
	.Accel = cWalkCharEntity_Accel,
	.WalkAnim = cWalkCharEntity_WalkAnim,
	.ActionTick = cWalkCharEntity_ActionTick,
	.Fly = cWalkCharEntity_Fly,
	.FlySound = cWalkCharEntity_FlySound,
	.WalkSound = cWalkCharEntity_WalkSound,
	.SetViewAng = cWalkCharEntity_SetViewAng,
	.SetYModelDisp = cWalkCharEntity_SetYModelDisp,
};

cGVar GVar_EnergyRechargeTime;
cGVar GVar_AbortSelectDistance;
cGVar GVar_EnergyBarActive;
cGVar GVar_SndLoopCharNPCMul;
cGVar GVar_CharUsrPushSpeed;
cGVar GVar_CharGrav;
cGVar GVar_TimeToAbortSelectChar;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_CharGrav, &GVar_CharGrav, "CharGrav", 0, "Characters Gravity.", 1960.0f)
INIT_GVAR_FLOAT(Init_GVar_EnergyRechargeTime, &GVar_EnergyRechargeTime, "EnergyRechargeTime", 0, "Time to recharge the full energy...", 15.0f)
INIT_GVAR_FLOAT(Init_GVar_AbortSelectDistance, &GVar_AbortSelectDistance, "AbortSelectDistance", 0, "At this distance (from the center), the selection must be reseted.", 1500.0f)
INIT_GVAR_FLOAT(Init_GVar_CharUsrPushSpeed, &GVar_CharUsrPushSpeed, "CharUsrPushSpeed", 0, "Speed in cm/seg used to push characters....", 100.0f)
INIT_GVAR_INT(Init_GVar_EnergyBarActive, &GVar_EnergyBarActive, "EnergyBarActive", 0, "if 0, the energy bar action is disable.", 1)
INIT_GVAR_FLOAT(Init_GVar_TimeToAbortSelectChar, &GVar_TimeToAbortSelectChar, "TimeToAbortSelectChar", 0, "Time to call AbortSelectFunc in chars if inactive.", 30.0f)
INIT_GVAR_FLOAT(Init_GVar_SndLoopCharNPCMul, &GVar_SndLoopCharNPCMul, "SndLoopCharNPCMul", 0, "Multiplicador de volumen para los loops de los npcs", 1.0f)
