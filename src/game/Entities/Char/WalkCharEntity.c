/*
 * Entities/Char/WalkCharEntity.cpp of the game: the walking character (include/scrap/walkcharentity.h).
 */
#include "scrap/chartype.h"
#include "scrap/actions.h"
#include "scrap/arrowindicator.h"
#include "scrap/fxelements.h"
#include "scrap/missileentity.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"

#define WC_F(e, o) (*(float *)((byte *)(e) + (o)))
#define WC_I(e, o) (*(int *)((byte *)(e) + (o)))
#define WC_B(e, o) (*(byte *)((byte *)(e) + (o)))
#define WC_P(e, o) (*(void **)((byte *)(e) + (o)))
#define WC_NODE(e, o) ((Nto1Node *)((byte *)(e) + (o)))
#define WC_FUNC(e, o) ((cPy_Function *)((byte *)(e) + (o)))

/* the action controller of the character and the callbacks of its class (cActClass) */
#define WChar_ActCtrlPtr(e) (*(void **)((byte *)(e) + 0x234))
#define WChar_OnEnergyFull(e) WC_FUNC(e, 0x49c)		/* the energy used up while an action takes it */
#define WChar_EnergyTime(e) WC_F(e, 0x4b4)		/* the seconds an action takes the energy in, 0 recharges */
#define WChar_ViewLockF(e) WC_F(e, 0x4d8)		/* the blend of the view lock, 0..1 */
#define ActClass_OnPress(c) WC_FUNC(c, 0x190)
#define ActClass_OnRelease(c) WC_FUNC(c, 0x1a4)
#define ActClass_OnTalk(c) WC_FUNC(c, 0x1b8)
#define ActClass_OnVictim(c) WC_FUNC(c, 0x1cc)

/* the turn of the head of the controller towards ang (x, y) in dt (rva 0x62dc0) and the camera of the class
 * (rva 0x616e0) */
void __thiscall cActCtrl_TurnHead(void *ctrl, const float *ang, float dt);
void __thiscall cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e);

cEntity *__thiscall cWalkCharEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->vftable = Game_Ptr(CWALKCHARENTITY_VFTABLE);
	Nto1Node_Init(WC_NODE(self, 0x3e8));
	WC_I(self, 0x404) = 0;
	WC_I(self, 0x408) = 0;
	cPy_Function_ctor(WChar_OnEnergyFull(self));
	Nto1Node_Init(WC_NODE(self, 0x4bc));
	cWalkCharControl_ctor(WChar_Control(self));
	Nto1Node_Init(WChar_ListenNode(self));
	Nto1Node_Init(WC_NODE(self, 0x534));
	cPy_Function_ctor(WC_FUNC(self, 0x544));
	cPy_Function_ctor(WC_FUNC(self, 0x558));
	WChar_OwnerSlot(self) = -1;
	for (int o = 0x428; o < 0x458; o += 4) {
		WC_I(self, o) = 0;
	}
	float r = FxElem_Rand(4.0, 2.0);
	self->mask |= 0x10;
	WC_F(self, 0x458) = r;
	WC_F(self, 0x45c) = 128.0f;
	*(uint16_t *)((byte *)self + 0x464) = 0;
	for (int o = 0x468; o <= 0x478; o += 4) {
		WC_I(self, o) = 0;
	}
	WC_I(self, 0x490) = 0;
	WC_I(self, 0x480) = 0;
	WC_I(self, 0x484) = 0;
	WC_I(self, 0x488) = 0;
	WC_B(self, 0x48c) = 0;
	WChar_ListenNode(self)->data = self;
	WC_NODE(self, 0x534)->data = self;
	WChar_ListenOn(self) = 0;
	WC_I(self, 0x494) = 0;
	WC_I(self, 0x47c) = 0;
	WC_I(self, 0x498) = 0;
	WC_F(self, 0x230) = 5000.0f;
	WChar_Money(self) = 0;
	WChar_MoneyReward(self) = 0;
	WC_F(self, 0x3f8) = 1000.0f;
	WC_B(self, 0x4dc) = 0;
	WC_I(self, 0x4e0) = 0;
	WC_I(self, 0x4e4) = 0;
	WC_I(self, 0x4e8) = 0;
	WChar_MaxLife(self) = WChar_Life(self);
	WChar_EnergyBarDecr(self) = 0.200000003f;
	WChar_EnergyTime(self) = 0.0f;
	WC_NODE(self, 0x3e8)->data = self;
	WC_I(self, 0x56c) = 0;
	WC_I(self, 0x570) = 0;
	WC_I(self, 0x3fc) = 0;
	WC_NODE(self, 0x4bc)->target = self;
	WChar_ViewLock(self) = 1;
	WChar_ViewLockF(self) = 1.0f;
	WC_I(self, 0x4b8) = 0;
	WC_I(self, 0x460) = 0;
	return self;
}

void __fastcall cWalkCharEntity_dtor(cEntity *self) {
	self->vftable = Game_Ptr(CWALKCHARENTITY_VFTABLE);
	if (WChar_ActCtrlPtr(self)) {
		void *ai = self->ai;
		if (ai) {
			/* the deleting destructor of the AI */
			(*(void(__thiscall **)(void *, uint32_t))*(void **)ai)(ai, 1);
		}
		self->ai = NULL;
	}
	cPy_Function_dtor(WC_FUNC(self, 0x558));
	cPy_Function_dtor(WC_FUNC(self, 0x544));
	Nto1Node_Done(WC_NODE(self, 0x534));
	Nto1Node_Done(WChar_ListenNode(self));
	Nto1Node_Done(&WChar_Control(self)->target);
	Nto1Node_Done(WC_NODE(self, 0x4bc));
	cPy_Function_dtor(WChar_OnEnergyFull(self));
	Nto1Node_Done(WC_NODE(self, 0x3e8));
	cWithLifeEntity_dtor(self);
}

cEntity *__thiscall cWalkCharEntity_Delete(cEntity *self, uint32_t flags) {
	cWalkCharEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CWALKCHARENTITY_SIZE);
	}
	return self;
}

void __thiscall cWalkCharEntity_ActionTick(cEntity *self, float dt) {
	void *ctrl = WChar_ActCtrlPtr(self);

	if (!ctrl) {
		return;
	}
	int slot = WChar_OwnerSlot(self);
	if ((g_EnergyBarActive || slot < 0 || slot >= 4) && WC_B(self, 0x505) != WChar_ActionPress(self)) {
		byte *cls = ChType_ActClass(ctrl);
		if (WChar_ActionPress(self)) {
			cPy_Function_AddParamS(ActClass_OnPress(cls), self->name);
			cPy_Function_Execute(ActClass_OnPress(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		} else {
			cPy_Function_AddParamS(ActClass_OnRelease(cls), self->name);
			cPy_Function_Execute(ActClass_OnRelease(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		}
	}
	if (!WC_B(self, 0x508)) {
		if (!WChar_ActionPress(self) && WC_B(self, 0x506) && !WC_B(self, 0x507) &&
			cWalkCharEntity_CanActOnVictim(self, 0)) {
			cPy_Function_AddParamS(ActClass_OnVictim(ChType_ActClass(WChar_ActCtrlPtr(self))), self->name);
			byte *cls = ChType_ActClass(WChar_ActCtrlPtr(self));
			cPy_Function_AddParamS(ActClass_OnVictim(cls), Nto1Node_TargetName(WC_NODE(self, 0x534)));
			cPy_Function_Execute(ActClass_OnVictim(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		}
		if (!WC_B(self, 0x508)) {
			goto energy;
		}
	}
	if (!WC_B(self, 0x509) && !ChType_ActPriority(WChar_ActCtrlPtr(self))) {
		cEntity *t;
		byte ok;
		if (WChar_ListenOn(self) && (t = WChar_Listen(self)) != NULL) {
			ok = t->vftable->IsType(t, TYEN_WITH_LIFE);
		} else {
			t = WChar_Victim(self);
			if (!t || !t->vftable->IsType(t, TYEN_WITH_LIFE)) {
				goto energy;
			}
			ok = WC_B(t, 0x384);
		}
		if (ok) {
			if (WC_B(t, 0x398)) {
				cPy_Function_AddParamS(ActClass_OnTalk(ChType_ActClass(WChar_ActCtrlPtr(self))), self->name);
				cPy_Function_Execute(ActClass_OnTalk(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
			} else {
				void *l = t->lLogic;
				if (!l || (*(byte(__thiscall **)(void *))(*(byte **)l + 8))(l)) {
					cArrowIndicator_Execute(Ent_Arrow(t), self);
				}
			}
		}
	}
energy:
	if (!g_EnergyBarActive) {
		WChar_EnergyUsed(self) = 1.0f;
		return;
	}
	float time = WChar_EnergyTime(self);
	if (time > 0.0f) {
		float used = dt / time + WChar_EnergyUsed(self);
		WChar_EnergyUsed(self) = used;
		if (used >= 1.0f) {
			WChar_EnergyUsed(self) = 1.0f;
			if (cPy_Function_IsValid(WChar_OnEnergyFull(self))) {
				cPy_Function_AddParamS(WChar_OnEnergyFull(self), self->name);
				cPy_Function_Execute(WChar_OnEnergyFull(self), 0);
			}
		}
	} else {
		float used = WChar_EnergyUsed(self);
		int i = (int)(used - 0.999899983f);
		int lo = 0;
		if (i <= 0) {
			lo = i;
		}
		used = used - dt / g_EnergyRechargeTime;
		WChar_EnergyUsed(self) = used;
		if ((float)lo >= used) {
			WChar_EnergyUsed(self) = (float)lo;
		}
	}
}

void __thiscall cWalkCharEntity_UpdateModel(cEntity *self) {
	vec3_t n, d, pos;
	float head[2];
	cObjEntity *o = (cObjEntity *)self;

	vec3_t ang = o->renderAng;
	d.x = WC_F(self, 0x44c) - o->renderAng.x;
	d.y = WC_F(self, 0x450) - o->renderAng.y;
	d.z = WC_F(self, 0x454) - o->renderAng.z;
	Vec3_NormAng(&n, &d);
	float dt = cTimer_GetDelta(&o->timer);
	if (!(0.00000099999999747f >= dt)) {
		CGameModel_SetRot(&o->model, &ang);
		pos.x = o->renderPos.x + 0.0f;
		pos.y = o->renderPos.y + WC_F(self, 0x494);
		pos.z = o->renderPos.z + 0.0f;
		CGameModel_SetPos(&o->model, &pos);
		void *ctrl = WChar_ActCtrlPtr(self);
		if (ctrl) {
			head[0] = n.x;
			head[1] = WChar_ViewLockF(self) * n.y;
			cActCtrl_TurnHead(ctrl, head, dt);
			if (WChar_Cam(self)) {
				cActClassCam_Update(ChType_ActClass(WChar_ActCtrlPtr(self)) + 0x110, WChar_Cam(self), self);
			}
		}
		void **fx = WChar_FX(self);
		if (fx) {
			(*(void(__thiscall **)(void *))(*(byte **)fx + 0x10))(fx);
		}
	}
}

void __thiscall cWalkCharEntity_UpdateViewLock(cEntity *self, float dt) {
	void *ctrl = WChar_ActCtrlPtr(self);

	if (ctrl && ChType_ActClass(ctrl)) {
		byte b = ChType_ActClass(ctrl)[0x188];
		byte old = WChar_ViewLock(self);
		WChar_ViewLock(self) = b;
		if (b && (g_WalkCameraCenter == 0.0f || WChar_OwnerSlot(self) == -1 || WC_B(self, 0x50c))) {
			b = 1;
		} else {
			b = 0;
		}
		WChar_ViewLock(self) = b;
		if (!old && b) {
			WC_B(self, 0x465) = 1;
		}
	}
	float target = 0.0f;
	if (WChar_ViewLock(self)) {
		target = 1.0f;
	}
	Float_Approach(&WChar_ViewLockF(self), target, dt + dt);
}

byte __thiscall cWalkCharEntity_CanActOnVictim(cEntity *self, byte checkClass) {
	if (!(0.0f >= WChar_EnergyUsed(self))) {
		return 0;
	}
	cEntity *v = WChar_Victim(self);
	if (!v || !WChar_ActCtrlPtr(self) || ChType_ActPriority(WChar_ActCtrlPtr(self))) {
		return 0;
	}
	if (!v->vftable->IsType(v, TYEN_WALKCHAR)) {
		return 0;
	}
	if (checkClass && !ChType_ActClass(ChType_ActCtrl(v))[0x184]) {
		return 0;
	}
	return 1;
}

void __fastcall cWalkCharEntity_Unlink3e8(cEntity *self) {
	Nto1Node_UnlinkChecked(&WChar(self)->collide);
}

/* ---- the rest of WalkCharEntity.cpp ---- */

#define WCE_WALKCHAR ((const char *)Game_Ptr(0x3a38b4))	/* "WalkChar", the class name */
#define WCE_RELAX ((const char *)Game_Ptr(0x3ad2f4))		/* "Relax" */
#define WCE_RELAXALARM ((const char *)Game_Ptr(0x3a3248))	/* "RelaxAlarm" */
#define WCE_EMPTY ((const char *)Game_Ptr(0x3a3228))		/* "" */

#define WCE_MIN_DT 9.99999997e-07f		/* rva 0x3b0830 */
#define WCE_MIN_MOVE2 0.00249999994f		/* rva 0x3b0834: a smaller walk step is undone */
#define WCE_MIN_INPUT2 0.0400000028f		/* rva 0x3b0838 */
#define WCE_MAX_SPEED 14000.0f			/* rva 0x3b0880: faster is an error of the walk */
#define WCE_KMH 28.0f				/* rva 0x3b0878, 0x3b0870 (double) */
#define WCE_PITCH_FLY 1.57079637f		/* rva 0x3ad098 */
#define WCE_PITCH_WALK 1.04719758f		/* rva 0x3b084c */
#define WCE_MAX_FALL 10000.0f			/* rva 0x3a5ac0, 0x3b0898 */

/* the WithLife entity hit test of the walk */
#define WCE_HIT_LISTS ((void *)0x11)

static inline cActClassChar *WCE_Class(const cWalkCharEntity *w) {
	return ((cActCtrlChar *)w->actCtrl)->cls;
}

static inline cActionChar *WCE_Action0(const cWalkCharEntity *w) {
	return ((cActCtrlChar *)w->actCtrl)->action0;
}

/* the name of the action of layer 0 starts with s (n characters) */
static inline byte WCE_Action0Is(const cWalkCharEntity *w, const char *s, size_t n) {
	return !crt_strncmp(cActCtrl_GetActionName(w->actCtrl, 0), s, n);
}

/* the alarm flags of the world the alarm actions follow */
static inline byte WCE_AlarmOn(void) {
	return ((byte *)g_pWorld)[0x2074] || g_pWorld->alarmOn;
}

/* World.h, inlined: the entity of player slot 0 */
static inline cEntity *WCE_Usr0(void) {
	return cWorld_GetUsrEntity(g_pWorld, 0);
}

/* the push of an AI moves it, once */
static inline void WCE_ApplyPush(cWalkCharEntity *w, float dt) {
	vec3_t *pos = &w->obj.base.pos;

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
	vec3_t *v = &w->obj.vel;
	float f = WCE_Class(w)->friction;

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

void __fastcall Nto1Node_UnlinkChecked(Nto1Node *node) {
	CHECK(node->data);
	Nto1Node_Unlink(node);
}

void __thiscall cWalkCharEntity_Touch(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal) {
	cWalkCharEntity *w = WChar(self);
	vec3_t p, epos;

	(void)point;
	(void)normal;
	if (!w->actCtrl || !(e->mask & 0x11)) {
		return;
	}
	CHECK(e->vftable->IsType(e, TYEN_WITH_LIFE));
	p = self->pos;
	byte xz = !w->falling && !WCE_Class(w)->canFly;
	const vec3_t *from = e->vftable->GetPos(e, &epos);
	cPrimitive_UnelasticDisp(&w->prim, &p, from, &WChar(e)->prim, xz);
	w->obj.vel.x = w->obj.vel.x + (p.x - self->pos.x);
	w->obj.vel.y = (p.y - self->pos.y) + w->obj.vel.y;
	w->obj.vel.z = (p.z - self->pos.z) + w->obj.vel.z;
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

void __thiscall cWalkCharEntity_Walk(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t old, t, a, b;
	cEntity *hit;

	if (!w->actCtrl) {
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
	float r = WCE_Class(w)->radius;
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
	w->obj.vel.x = 0.0f;
	w->obj.vel.y = 0.0f;
	w->obj.vel.z = 0.0f;
	hit = NULL;
	if (!cWorld_TestSphereEntities(g_pWorld, self, WCE_HIT_LISTS, &old, w->prim.radius, &hit, 0, (int)pos)) {
		Nto1Node_UnlinkChecked(&w->collide);
		goto speed;
	}
	cEntity_AddLink(hit, &w->collide);
	if (hit->vftable->IsType(hit, WCE_WALKCHAR)) {
		if (w->ownerSlot != -1) {
			a.x = -w->obj.vel.x;
			a.y = -w->obj.vel.y;
			a.z = -w->obj.vel.z;
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
	pos->x = w->obj.vel.x + pos->x;
	pos->y = w->obj.vel.y + pos->y;
	pos->z = w->obj.vel.z + pos->z;
	if (Engine_TestSphere(pos, pos, r, NULL, NULL)) {
		*pos = old;
	} else if (cWorld_TestSphereEntities(g_pWorld, self, WCE_HIT_LISTS, pos, w->prim.radius, &hit, 1, 0)) {
		*pos = old;
		cEntity_AddLink(hit, &w->collide);
	}
speed:;
	float inv = 1.0f / dt;
	w->obj.vel.y = (pos->y - old.y) * inv;
	w->obj.vel.z = (pos->z - old.z) * inv;
	w->obj.vel.x = (pos->x - old.x) * inv;
	vec3_t *v = &w->obj.vel;
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

void __thiscall cWalkCharEntity_Turn(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
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

void __thiscall cWalkCharEntity_PlayWalk(cEntity *self, const char *init, const char *loop, const char *end) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl || cActCtrl_IsActive(w->actCtrl, init) || cActCtrl_IsActive(w->actCtrl, loop)) {
		return;
	}
	cActCtrl_SetDefLoop(w->actCtrl, loop, 0);
	if (!*init || !cActCtrl_PlayNew(w->actCtrl, init, 0, 1.0f)) {
		cActCtrl_PlayNew(w->actCtrl, loop, 0, 1.0f);
	}
	w->walkEnd = cActClass_GetAction((cActClass *)WCE_Class(w), end);
}

void __thiscall cWalkCharEntity_UpdateHitFlash(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (self->flag_4c && w->life > 0 && w->maxLife > w->life) {
		w->hitFlash = 1.5f;
	}
	if (!(w->hitFlash > 0.0f)) {
		return;
	}
	uint32_t fx = CGameModel_GetEffect(&w->obj.model, NULL);
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
typedef void *(__thiscall *wceAICtor_t)(void *mem, cEntity *e, int *arg);
static const struct {
	const char *cls;
	uint32_t size;
	wceAICtor_t ctor;
} wce_ais[] = {
	{"Sentinel", 0x380, cAISentinel_ctor},
	{"Gear", 0x390, cAIGear_ctor},
	{"Nurse", 0x384, cAINurse_ctor},
	{"BankDirector", 0x384, cAIBankDirector_ctor},
	{"Mayor", 0x380, cAIMayor_ctor},
	{"Police", 0x380, cAIPolice_ctor},
	{"Bishop", 0x398, cAIBishop_ctor},
	{"Messenger", 0x390, cAIMessenger_ctor},
	{"BankMaster", 0x384, cAIBankMaster_ctor},
	{"Desktop", 0x380, cAIDesktop_ctor},
	{"Human", 0x380, cAIHuman_ctor},
	{NULL, 0x380, cAIChar_ctor}
};

void __thiscall cWalkCharEntity_EnableAI(cEntity *self, byte on, int *arg) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
		return;
	}
	if (!on) {
		if (self->ai) {
			/* the deleting destructor of the AI */
			(*(void(__thiscall **)(void *, uint32_t))*(void **)self->ai)(self->ai, 1);
		}
		self->ai = NULL;
		return;
	}
	if (self->ai) {
		return;
	}
	const char *name = *(const char **)((byte *)WCE_Class(w) + 4);
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

byte __thiscall cWalkCharEntity_VarF(cEntity *self, const char *name, double *d, int set) {
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

void __thiscall cWalkCharEntity_Fall(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t a, b;

	if (!w->actCtrl) {
		return;
	}
	cActClassChar *cls = WCE_Class(w);
	byte falling = w->falling;
	float h = falling ? 0.0f : cls->stepHeight;
	float vy = w->obj.vel.y;
	h = h - vy * dt;
	a.x = pos->x - 0.0f;
	a.y = pos->y - ((w->floorDist + h) - cls->floorRadius);
	a.z = pos->z - 0.0f;
	b = a;
	if (WCE_MIN_MOVE2 > w->moved2 && !falling) {
		b.y = a.y + h;
	} else if (0.0f > vy || !falling) {
		byte r = Engine_TestSphere(pos, &a, cls->floorRadius, &b, NULL);
		cWalkCharEntity_SetFalling(self, r == 0);
	}
	if (!w->falling) {
		float d = (b.y - a.y) - h;
		if (1.5 > __builtin_fabs((double)d)) {
			d = 0.0f;
		}
		w->obj.vel.y = d / dt;
		if (WCE_Action0(w)->moveType) {
			WChar_Vtbl(self)->Accel(self, dt);
		} else {
			w->obj.vel.x = 0.0f;
			w->obj.vel.z = 0.0f;
		}
	} else {
		float v = w->obj.vel.y - g_CharGrav * dt;
		w->obj.vel.y = v;
		if (-WCE_MAX_FALL > v) {
			w->obj.vel.y = -WCE_MAX_FALL;
		} else if (WCE_Class(w)->fallUp) {
			if (v > WCE_MAX_FALL) {
				w->obj.vel.y = WCE_MAX_FALL;
			}
		} else if (v > 0.0f) {
			w->obj.vel.y = 0.0f;
		}
		w->fallVel = w->obj.vel;
	}
	pos->x = pos->x + w->obj.vel.x * dt;
	pos->y = pos->y + w->obj.vel.y * dt;
	pos->z = pos->z + w->obj.vel.z * dt;
	WChar_Vtbl(self)->WalkSound(self, dt);
}

void __thiscall cWalkCharEntity_Fly(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *pos = &self->pos;
	vec3_t *v = &w->obj.vel;
	vec3_t in, tmp, a, hit;
	byte move;

	if (!w->actCtrl) {
		goto stop;
	}
	cActionChar *act = WCE_Action0(w);
	if (act->walk) {
		move = w->control.strafe * w->control.strafe + w->control.jump * w->control.jump +
			w->control.walk * w->control.walk > WCE_MIN_INPUT2;
	} else if (act->hover) {
		move = 0;
	} else {
		goto stop;
	}
	cActClassChar *cls = WCE_Class(w);
	float maxSpeed = cls->maxSpeed;
	float f = cls->airAccel * dt;
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
	if (!move && WCE_Action0(w)->hover && CGameModel_IsVisible(Obj_Vtbl(self)->GetModel(self))) {
		cls = WCE_Class(w);
		a = *pos;
		a.y = pos->y - ((cls->stepHeight + w->floorDist) - cls->floorRadius);
		Engine_TestSphere(pos, &a, WCE_Class(w)->floorRadius, &hit, NULL);
		cls = WCE_Class(w);
		float k = (((pos->y - hit.y) - w->floorDist) + cls->floorRadius) / cls->stepHeight;
		if (k > 0.0f) {
			v->y = v->y - cls->accel * k * dt;
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

void *__thiscall cWithLifeEntity_GetActCtrl(cEntity *self) {
	return WChar(self)->actCtrl;
}

float __thiscall cWalkCharEntity_GetEnergyFrac(cEntity *e) {
	float f = 0.999899983f - WChar(e)->energyUsed;

	return f - (float)(int)f;
}

vec3_t *__thiscall cWalkCharEntity_GetRenderAng(cEntity *self, vec3_t *out) {
	*out = WChar(self)->renderView;
	return out;
}

vec3_t *__thiscall cWalkCharEntity_GetSndAng(cEntity *self, vec3_t *out) {
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
	cActCtrlChar *ctrl = WithLife_Vtbl(self)->GetActCtrl(self);
	return ctrl->cls->alarmAnims && WCE_AlarmOn();
}

const char *__fastcall cWalkCharEntity_RelaxName(cEntity *self) {
	return WCE_Alarm(self) ? WCE_RELAXALARM : WCE_RELAX;
}

byte __fastcall cWalkCharEntity_IsAlarm(cEntity *self) {
	return WCE_Alarm(self);
}

void __thiscall cWalkCharEntity_GetCamera(cEntity *self, vec3_t *pos, vec3_t *ang) {
	cWalkCharEntity *w = WChar(self);

	cObjEntity_GetCamera(self, pos, ang);
	if (!w->obj.cam) {
		ang->x = -w->renderView.x;
		ang->y = w->renderView.y + M_PI_F;
		ang->z = w->renderView.z;
	}
}

const char *__thiscall cWalkCharEntity_GetClassName(cEntity *self) {
	(void)self;
	return WCE_WALKCHAR;
}

int __thiscall cWalkCharEntity_GetOwnerSlot(cEntity *e) {
	return WChar(e)->ownerSlot;
}

float __thiscall cWalkCharEntity_GetRadius(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (w->actCtrl) {
		return WCE_Class(w)->radius;
	}
	return 150.0f;
}

int __thiscall cWalkCharEntity_GetBatteries(cEntity *e) {
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

byte __thiscall cWalkCharEntity_VarI(cEntity *self, const char *name, int *i, int set) {
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
			*i = ai->vftable->GetState(ai);
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

byte __thiscall cWalkCharEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_WALKCHAR)) {
		return 1;
	}
	return cWithLifeEntity_IsType(self, type);
}

byte __thiscall cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit) {
	return cPrimitive_TestSegment(&WChar(self)->prim, from, hit);
}

byte __thiscall cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
	const vec3_t *prev) {
	return cPrimitive_TestSphere(&WChar(self)->prim, exclude, center, radius, prev);
}

void __thiscall cObjEntity_Vf41(cEntity *self, int a, int b, int c) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
}

byte __thiscall cObjEntity_Vf42(cEntity *self, int a) {
	(void)self;
	(void)a;
	return 1;
}

void __thiscall cObjEntity_Vf43(cEntity *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

void __thiscall cWalkCharEntity_Tick(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!g_ActiveExtrapolation || w->obj.cam) {
		w->slotView = w->view;
	} else {
		float iv = w->obj.slot->interval;
		w->slotView.x = w->viewVel.x * iv + w->view.x;
		w->slotView.y = w->view.y + w->viewVel.y * iv;
		w->slotView.z = w->view.z + w->viewVel.z * iv;
	}
	if (!w->obj.move.active) {
		WChar_Vtbl(self)->UpdateSelect(self, dt);
		if (w->disableControl) {
			WChar_Vtbl(self)->Idle(self, dt);
		} else {
			cWalkCharEntity_Walk(self, dt);
		}
	} else {
		if (w->obj.move.viewAng) {
			goto tick;
		}
		WChar_Vtbl(self)->UpdateSelect(self, dt);
		WChar_Vtbl(self)->Idle(self, dt);
	}
	Grid_Add(g_pWorld->grid, self);
tick:
	cWithLifeEntity_Tick(self, dt);
	w->abortOpt = w->arrow.iOpt;
}

void __thiscall cWalkCharEntity_UnlinkListen(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	Nto1Node_UnlinkChecked(&w->listen);
	w->abortTime = 0.0f;
}

/* the relax time again: lo .. hi at random */
static inline void WCE_RelaxTime(cWalkCharEntity *w, float lo, float hi) {
	int r = crt_rand();

	w->relaxTime = (float)((double)(r & 0x7fff) * 3.051850947599719e-05 * (double)(hi - lo) + (double)lo);
}

void __thiscall cWalkCharEntity_Idle(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t ang, t1;

	if (!w->actCtrl) {
		return;
	}
	cActionChar *act = WCE_Action0(w);
	float maxTurn = w->viewLock ? act->maxTurn : M_2PI_F;
	float *body = &w->obj.ang.y;
	float step = act->turnSpeed * dt;
	float a = NormAng((w->view.y - *body) + w->displAng);
	float old = *body;
	Float_Approach(body, old + a, step);
	float d = NormAng((w->view.y - *body) + w->displAng);
	if (d > maxTurn) {
		*body = (w->view.y + w->displAng) - maxTurn;
	} else if (-maxTurn > d) {
		*body = (w->view.y + w->displAng) + maxTurn;
	}
	w->obj.rotVel.y = (old - *body) / dt;
	if (w->control.lockStart) {
		w->control.lockStart = 0;
		cWalkCharControl_SetTarget(&w->control, WChar_Victim(self));
		ang.x = 0.0f;
		ang.y = Obj_Vtbl(self)->GetAng(self, &t1)->y;
		ang.z = 0.0f;
		WChar_Vtbl(self)->SetViewAng(self, &ang);
		if (w->obj.cam) {
			((cFollowCamEntity *)w->obj.cam)->vSpeed = 1.10000002f;
			((cFollowCamEntity *)w->obj.cam)->hSpeed = 1.10000002f;
		}
	}
	if (w->control.lock && !w->control.target.target) {
		cWalkCharControl_SetTarget(&w->control, WChar_Victim(self));
	}
	if (w->obj.cam) {
		((cFollowCamEntity *)w->obj.cam)->tiltTo = w->viewLock ? 0.0f : w->control.strafe;
	}
	byte turns;
	if (g_WalkCameraCenter == 0.0f || w->ownerSlot == -1 || w->control.lock) {
		turns = 1;
		if (__builtin_fabs((double)d) > (double)WCE_Class(w)->turnAng && WCE_Action0Is(w, "Relax", 5)) {
			cActCtrl_Play(w->actCtrl, WCE_Alarm(self) ? "TurnAlarm" : "Turn", 0, 1.0f);
		}
	} else {
		turns = 0;
	}
	byte relaxing = WCE_Action0Is(w, "Relax", 5);
	if (WCE_Alarm(self) && relaxing) {
		if (strcmp(cActCtrl_GetDefLoopName(w->actCtrl), "RelaxAlarm")) {
			cActCtrl_SetDefLoop(w->actCtrl, WCE_RELAXALARM, 0);
		}
		float t = w->relaxTime - dt;
		w->relaxTime = t;
		if (0.0f < t) {
			goto look;
		}
		cActCtrl_PlayNew(w->actCtrl, "RelaxAlarm0", 0, 1.0f);
		float hi = WCE_Class(w)->relaxMin;
		WCE_RelaxTime(w, hi * 0.5f, hi);
		goto look;
	}
	if (!WCE_Alarm(self) && relaxing && cActCtrl_IsActive(w->actCtrl, WCE_RELAXALARM)) {
		cActCtrl_PlayNew(w->actCtrl, WCE_RELAX, 0, 2.0f);
		cActCtrl_SetDefLoop(w->actCtrl, WCE_RELAX, 0);
		goto look;
	}
	if (cActCtrl_IsActive(w->actCtrl, cWalkCharEntity_RelaxName(self))) {
		float t = w->relaxTime - dt;
		w->relaxTime = t;
		if (0.0f < t) {
			goto look;
		}
		char name[7] = "Relax0";
		if (turns && __builtin_fabs((double)d) > 0.10000000149011612) {
			cActCtrl_Play(w->actCtrl, WCE_Alarm(self) ? "TurnAlarm" : "Turn", 0, 1.0f);
		} else {
			do {
				name[5] = (char)(crt_rand() % 5 + '0');
			} while (!cActCtrl_Play(w->actCtrl, name, 0, 1.0f) && name[5] > '0');
		}
		WCE_RelaxTime(w, WCE_Class(w)->relaxMin, WCE_Class(w)->relaxMax);
	} else if (!relaxing) {
		if (w->ownerSlot >= 0) {
			WCE_RelaxTime(w, WCE_Class(w)->relaxMin, WCE_Class(w)->relaxMax);
		} else {
			w->relaxTime = WCE_Class(w)->relaxMin;
		}
	}
look:;
	float look = WCE_Action0(w)->lookAt;
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
		dir.x = (ang.x - w->view.x) - act->lookX;
		dir.y = (ang.y - w->view.y) - act->lookY;
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

void __thiscall cWalkCharEntity_UpdateSelect(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
		return;
	}
	if (w->ownerSlot != -1) {
		cEntity *sel = ArrowIndicator_FindSel(self, w->viewLock ? w->view.y : w->obj.ang.y);
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
			if (WChar(l)->arrow.canUse &&
				ArrowIndicator_Visible(self, l, w->view.y, g_ArrowDeactivationRadius, NULL, 0, 0)) {
				goto abort;
			}
			cPy_Function_AddParamS(&w->outSelect, self->name);
			cPy_Function_AddParamS(&w->outSelect, Nto1Node_TargetName(&w->listen));
		} else {
			cPy_Function_AddParamS(&w->outSelect, self->name);
			cPy_Function_AddParamS(&w->outSelect, WCE_EMPTY);
		}
		cPy_Function_Execute(&w->outSelect, 1);
	}
abort:
	if (!cPy_Function_IsValid(&w->abortSelect)) {
		return;
	}
	w->abortTime = w->abortTime - dt;
	if (w->abortOpt != w->arrow.iOpt || !cActCtrl_IsActive(w->actCtrl, cWalkCharEntity_RelaxName(self))) {
		w->abortTime = g_TimeToAbortSelectChar;
	}
	cEntity *usr = WCE_Usr0();
	if (w->arrow.canUse && !(0.0f >= w->abortTime) && usr) {
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

void __thiscall cWalkCharEntity_SetActive(cEntity *self, byte on) {
	cWalkCharEntity *w = WChar(self);

	cWithLifeEntity_SetActive(self, on);
	if (w->ownerSlot != -1 && self->flag_4c) {
		g_pWorld->owners[w->ownerSlot] = self;
	}
	cWalkCharEntity_UpdateViewLock(self, 0.0f);
}

void __thiscall cWalkCharEntity_SetAng(cEntity *self, const vec3_t *ang) {
	WChar_Vtbl(self)->SetViewAng(self, ang);
	cObjEntity_SetAng(self, ang);
}

void __thiscall cWalkCharEntity_SetEnergy(cEntity *self, float energy) {
	if (0.0f > energy) {
		WChar(self)->energyUsed = 1.0f;
		return;
	}
	WChar(self)->energyUsed = 1.0f - energy;
}

void __thiscall cWalkCharEntity_SetEnergyUsed(cEntity *self, float used) {
	if (used > 1.0f) {
		WChar(self)->energyUsed = 1.0f;
		return;
	}
	WChar(self)->energyUsed = used;
}

void __thiscall cWalkCharEntity_SetEmbeddedFX(cEntity *self, const char *fxType) {
	FX_Character(self, fxType);
	cWithLifeEntity_SetEmbeddedFX(self, fxType);
}

void __thiscall cWalkCharEntity_SetFalling(cEntity *self, byte on) {
	cWalkCharEntity *w = WChar(self);
	void *ctrl = w->actCtrl;

	if (!ctrl || w->falling == on || WCE_Class(w)->canFly) {
		return;
	}
	w->falling = on;
	if (on) {
		cActCtrl_SetDefLoop(ctrl, "Fall", 0);
		if (!cActCtrl_Play(w->actCtrl, "FallInit", 0, 1.0f)) {
			cActCtrl_Play(w->actCtrl, "Fall", 0, 1.0f);
		}
		if (w->walking && w->obj.vel.y > 0.0f) {
			w->obj.vel.y = 0.0f;
			w->walking = 0;
			return;
		}
	} else {
		cActCtrl_SetDefLoop(w->actCtrl, cWalkCharEntity_RelaxName(self), 0);
		if (!cActCtrl_Play(w->actCtrl, "FallEnd", 0, 1.0f)) {
			cActCtrl_Play(w->actCtrl, cWalkCharEntity_RelaxName(self), 0, 1.0f);
		}
		if (-g_FallSoundVel > w->fallVel.y) {
			cActClassChar *cls = WCE_Class(w);
			float vol = cls->fallSoundVol;
			const char *snd = cls->fallSound;
			if (vol > 0.0f && snd && *snd) {
				int v = Snd_Volume(vol);
				g_pSound->vftable->PlayAt(g_pSound, snd, &w->obj.renderPos, v);
			}
		}
	}
	w->walking = 0;
}

void __thiscall cWithLifeEntity_SetMass(cEntity *self, float mass) {
	CHECK(mass > 0.0f);
	WChar(self)->mass = mass;
}

void __thiscall cWalkCharEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag) {
	cWalkCharEntity *w = WChar(self);
	vec3_t root, floor;

	if (!file || !*file) {
		cObjEntity_SetModel(self, file, sx, sy, sz, flag);
		return;
	}
	w->yModelDisp = 0.0f;
	cObjEntity_SetModel(self, file, sx, sy, sz, flag);
	CGameModel *m = &w->obj.model;
	CGameModel_NodoPos(m, &root, CGameModel_FindNodoR(m, "DC_Root"), 0);
	CGameModel_NodoPos(m, &floor, CGameModel_FindNodoR(m, "DC_Floor"), 0);
	w->camNode = CGameModel_FindNodoR(m, "DC_Camera");
	w->floorDist = root.y - floor.y;
	WChar_Vtbl(self)->SetYModelDisp(self, w->yModelDisp);
}

void __thiscall cWalkCharEntity_UpdateSlotRate(cEntity *self, float dt) {
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
	Float_Approach(&w->obj.slot->minDiff, target, dt / 10.0f);
}

byte __thiscall cWalkCharEntity_SetPush(cEntity *self, const vec3_t *push) {
	cWalkCharEntity *w = WChar(self);

	if (w->ownerSlot != -1 || w->pushOn) {
		return 0;
	}
	w->pushOn = 1;
	w->push = *push;
	return 1;
}

void __thiscall cWalkCharEntity_ApplyPush(cEntity *self, float dt) {
	WCE_ApplyPush(WChar(self), dt);
}

void __thiscall cWalkCharEntity_SetUsrControl(cEntity *self, int slot) {
	cWalkCharEntity *w = WChar(self);

	if (w->ownerSlot >= 0 && w->ownerSlot < 4) {
		cWalkCharControl_Clear(&w->control);
	}
	int old = w->ownerSlot;
	w->ownerSlot = slot;
	if (slot == -1) {
		w->obj.slot->minDiff = 0.0500000007f;
		if ((uint32_t)old <= 3 && cWorld_GetUsrEntity(g_pWorld, old) == self) {
			g_pWorld->owners[old] = NULL;
			ArrowIndicator_ClearSel();
		}
	} else {
		w->obj.slot->minDiff = 0.0166666675f;
		g_pWorld->owners[w->ownerSlot] = self;
	}
}

void __thiscall cWalkCharEntity_SetVel(cEntity *self, const vec3_t *vel) {
	cWalkCharEntity *w = WChar(self);

	if (w->obj.move.active) {
		return;
	}
	cObjEntity_SetVel(self, vel);
	if (w->obj.vel.y > 0.0f) {
		cWalkCharEntity_SetFalling(self, 1);
	}
}

void __thiscall cWalkCharEntity_SetViewAng(cEntity *self, const vec3_t *ang) {
	cWalkCharEntity *w = WChar(self);

	w->slotView = *ang;
	w->view = w->slotView;
	w->renderView = w->view;
	w->viewVel.x = 0.0f;
	w->viewVel.y = 0.0f;
	w->viewVel.z = 0.0f;
	cWalkCharControl_RestartCenter(&w->control);
}

void __thiscall cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos) {
	vec3_t t, d, ang;
	const vec3_t *p = self->vftable->GetPos(self, &t);

	d.x = p->x - pos->x;
	d.y = p->y - pos->y;
	d.z = p->z - pos->z;
	Vec3_GetRotAng(&ang, &d);
	WChar_Vtbl(self)->SetViewAng(self, &ang);
}

void __thiscall cWalkCharEntity_SetYModelDisp(cEntity *self, float y) {
	cWalkCharEntity *w = WChar(self);
	float d = w->yModelDisp - y;

	w->obj.camOff.y = w->obj.camOff.y - d;
	w->yModelDisp = w->yModelDisp - d;
	w->floorDist = d + w->floorDist;
}

void __thiscall cWalkCharEntity_Accel(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	vec3_t *v = &w->obj.vel;
	vec3_t in, tmp;

	if (!w->actCtrl) {
		return;
	}
	cActClassChar *cls = WCE_Class(w);
	float maxSpeed = cls->maxSpeed;
	float f = cls->accel * dt;
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

void __thiscall cWalkCharEntity_Update(cEntity *self) {
	cWalkCharEntity *w = WChar(self);
	vec3_t base, d, n, t;
	matrix_t m;

	if (!w->actCtrl) {
		return;
	}
	float dt = cTimer_GetDelta(&w->obj.timer);
	cWalkCharEntity_UpdateSlotRate(self, dt);
	cWalkCharEntity_UpdateHitFlash(self, dt);
	cWithLifeEntity_Update(self);
	if (WCE_MIN_DT >= dt) {
		return;
	}
	float iv = w->obj.slot->interval;
	float k = w->obj.slot->time / iv;
	base = w->view;
	if (g_ActiveExtrapolation && !w->obj.cam) {
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
	if (WCE_Action0(w)->headNode) {
		CGameModel *model = Obj_Vtbl(self)->GetModel(self);
		const matrix_t *mat = CGameModel_NodoMatrix(model, &m, w->camNode, 1, NULL, 0);
		w->headPitch = Matrix_GetRotAng(mat, &t)->y + M_PI_F;
	} else {
		w->headPitch = Obj_Vtbl(self)->GetRenderAng(self, &t)->y;
	}
}

/* the pitch or volume of the loop sound between its first two values by k (inv = 1 - k) */
static inline float WCE_SndMix(const float *v, float inv, float k) {
	return v[0] * inv + v[1] * k;
}

void __thiscall cWalkCharEntity_FlySound(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cSndItem *s = PtrArray_Get(&w->obj.snd, 0);
	vec3_t *v = &w->obj.vel;

	(void)dt;
	if (!w->actCtrl || !s) {
		return;
	}
	float sp = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	float k = sp / WCE_Class(w)->maxSpeed;
	float c = 0.0f;
	if (!(0.0f > k)) {
		c = Float_MinSS(1.0f, k);
	}
	float inv = 1.0f - c;
	s->pitch = WCE_SndMix(s->pitches, inv, c);
	s->volume = WCE_SndMix(s->vols, inv, c);
}

void __thiscall cWalkCharEntity_WalkSound(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cSndItem *s = PtrArray_Get(&w->obj.snd, 0);

	if (!s || s->ints[0] != 1) {
		return;
	}
	Float_Approach(&s->times[0], w->move, dt / s->times[1]);
	float k = s->times[0];
	float inv = 1.0f - k;
	s->pitch = WCE_SndMix(s->pitches, inv, k);
	float mul = w->obj.cam ? 1.0f : g_SndLoopCharNPCMul;
	s->volume = WCE_SndMix(s->vols, inv, k) * mul;
}

byte __thiscall cWalkCharEntity_VarS(cEntity *self, const char *name, char **s, int set) {
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
			cWalkCharEntity_Unlink3e8(self);
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

byte __thiscall cWalkCharEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
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
static inline const char *WCE_Init(cEntity *self, byte strafeInit, byte(__fastcall *running)(cEntity *),
	const char *init) {
	return strafeInit && running(self) ? WCE_EMPTY : init;
}

static byte __fastcall WCE_StrafeOrMove(cEntity *self) {
	return cWalkCharEntity_IsStrafeAnim(self) || cWalkCharEntity_IsMoveAnim(self);
}

void __thiscall cWalkCharEntity_WalkAnim(cEntity *self, float dt) {
	cWalkCharEntity *w = WChar(self);
	cActCtrlChar *ctrl = w->actCtrl;
	vec3_t *v = &w->obj.vel;
	vec3_t *pos = &self->pos;
	float in[2], out[2], mx, mz;

	if (!ctrl) {
		return;
	}
	byte wasWalking = w->walking;
	byte strafeInit = ctrl->cls->strafeInit;
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
		float f = ctrl->cls->airAccel * dt;
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
			if (!strcmp(cActCtrl_GetActionName(w->actCtrl, 0), loop)) {
				cActCtrl_PlayAction(w->actCtrl, w->walkEnd, 0, 1, 1.0f);
			} else {
				cActCtrl_PlayAction(w->actCtrl, w->walkEnd, 0, 1, WCE_Class(w)->endSpeed);
			}
		} else {
			cActCtrl_Play(w->actCtrl, cWalkCharEntity_RelaxName(self), 0, 1.0f);
		}
		cActCtrl_SetDefLoop(w->actCtrl, cWalkCharEntity_RelaxName(self), 0);
		WChar_Ctrl(w)->walkSpeed = 0.0f;
		w->walking = 0;
		goto still;
	}
	if (ctrl->action0->moveType == 4) {
		w->displAng = 0.0f;
	} else {
		float a = (float)Game_Atan2((double)w->control.walk, (double)w->control.strafe);
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
			switch (WCE_Action0(w)->moveType) {
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
			switch (WCE_Action0(w)->moveType) {
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
		if (WCE_Action0(w)->moveType != 4) {
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
	ctrl = w->actCtrl;
	if (ctrl->action0->rootMove && !w->falling) {
		in[0] = ctrl->rootMove.x;
		in[1] = ctrl->rootMove.z;
	}
	ctrl->root[0] = 0.0f;
	ctrl->root[1] = 0.0f;
	ctrl->root[2] = 0.0f;
	ctrl->root[3] = 0.0f;
	if (w->falling) {
		const float *r = Vec2_Rotate(out, in, -w->view.y);
		float vx = v->x * dt;
		float vz = v->z * dt;
		float k = WCE_Class(w)->friction;
		mz = r[1] - k * vz;
		mx = r[0] - k * vx;
	} else {
		const float *r = Vec2_Rotate(out, in, -w->obj.ang.y);
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
		float max = WCE_Class(w)->maxSpeed;
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
			w->walkTime = WCE_Class(w)->stopTime;
		}
	}
}

byte __fastcall cWalkCharEntity_IsMoveAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "Run", 3) || WCE_Action0Is(w, "Walk", 4) || WCE_Action0Is(w, "Sprint", 6);
}

byte __fastcall cWalkCharEntity_IsBackOrMoveAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "Back", 4) || cWalkCharEntity_IsMoveAnim(self);
}

byte __fastcall cWalkCharEntity_IsStrafeAnim(cEntity *self) {
	cWalkCharEntity *w = WChar(self);

	if (!w->actCtrl) {
		return 0;
	}
	return WCE_Action0Is(w, "StrRight", 8) || WCE_Action0Is(w, "StrLeft", 7);
}
