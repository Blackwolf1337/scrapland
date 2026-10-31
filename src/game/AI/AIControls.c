/*
 * The move controls of the character classes, original sources AI/AIControlBankDirector.cpp .. AIControlSentinel.cpp
 * (include/scrap/aicharcontrol.h): cAICharControl2D (bank director, bank master, bishop, gear) or cAICharControl3D
 * (messenger, nurse, sentinel) with some slots of their own. The bishop, the gear and the sentinel strafe to a side
 * while a Bishop or a Gear (the bishop and the gear: a Dtritus too) aims at them, the sentinel also when it was hit;
 * their walk stops during the strafe.
 */
#include <float.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/aichar.h"
#include "scrap/aicharcontrol.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"

/* ---- helpers ---- */

static inline float ACtl_FlatDist(float dx, float dz) {
	return (float)__builtin_sqrt((double)(dz * dz + dx * dx));
}

static inline float ACtl_Dist(float dx, float dy, float dz) {
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* the walk value of walkDir, none while the control is blocked, stuck (or strafes around the target) */
static void ACtl_SetWalk(cAICharControl *self, int orbit) {
	switch (self->walkDir) {
	case 0:
		self->ctrl->walk = 0.0f;
		break;
	case 1:
		self->ctrl->walk = self->speed;
		break;
	case 2:
		self->ctrl->walk = -self->speed;
		break;
	}
	if (self->blocked || self->stuck || (orbit && self->orbitSide)) {
		self->ctrl->walk = 0.0f;
	}
}

/* the way from the character back from its target free (TestSides, slot 0x40) */
static byte ACtl_BackFree(cAICharControl *self) {
	vec3_t b1, b2;
	cEntity *e = AICC_TargetEnt(self);
	cEntity *me = self->ent;
	float r = Obj_Vtbl(me)->GetRadius(me);
	const vec3_t *tp = AICC_EntPos(e, &b1);

	return AICC_Vtbl(self)->TestSides(self, AICC_EntPos(me, &b2), tp, r);
}

static int ACtl_TargetActing(cAICharControl *self) {
	return AIChar_ActionIs(AICC_TargetEnt(self), "Action") || AIChar_ActionIs(AICC_TargetEnt(self), "ActionInit") ||
		AIChar_ActionIs(AICC_TargetEnt(self), "ActionEnd");
}

/* the name of the action class of the target */
static const char *ACtl_TargetClass(cAICharControl *self) {
	cEntity *t = AICC_TargetEnt(self);

	return ((cActCtrl *)WithLife_Vtbl(t)->GetActCtrl(t))->cls->name;
}

/* the angle between the way from the target to the character (d) and the forward of the view of the target on the
 * floor */
static double ACtl_AngleToView(cAICharControl *self, const vec3_t *d) {
	vec3_t fwd = {0.0f, 0.0f, -1.0f};
	vec3_t out;
	vec3_t ang = WChar(AICC_TargetEnt(self))->view;
	const vec3_t *tv = Vec3_Rotate(&out, &fwd, &ang);
	vec3_t w = {tv->x, 0.0f, tv->z};

	return Vec3_Angle(&w, d);
}

/* the side of the strafe at random (rand() % 2: 0 the side 2), the time from 0 */
static void ACtl_RandomSide(cAIControlShoot *self) {
	int r = crt_rand() % 2;

	if (r == 0) {
		self->side = 2;
	} else if (r == 1) {
		self->side = 1;
	}
	self->time = 0.0f;
}

static cAICharControl *ACtl_ShootInit(cAICharControl *self, void *vftable) {
	cAIControlShoot *s = (cAIControlShoot *)self;

	self->vftable = vftable;
	cSlotController_ctor(AICC_Slot(&s->slotSide), 0.2f);
	s->side = 0;
	s->time = 0.0f;
	s->other = 1;
	return self;
}

/* ---- the bank director ---- */

cAICharControl *__thiscall cAIControlBankDirector_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl2D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLBANKDIRECTOR_VFTABLE;
	return self;
}

void __fastcall cAIControlBankDirector_dtor(cAIControlBankDirector *self) {
	self->vftable = CAICONTROLBANKDIRECTOR_VFTABLE;
	cAICharControl2D_dtor(self);
}

cAIControlBankDirector *__thiscall cAIControlBankDirector_Delete(cAIControlBankDirector *self, byte flags) {
	self->vftable = CAICONTROLBANKDIRECTOR_VFTABLE;
	cAICharControl2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

void __thiscall cAIControlBank_Walk(cAICharControl *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t b1, b2;
	float f;

	float px = AICC_Pos(self, &b1)->x;
	float pz = AICC_Pos(self, &b2)->z;
	float tx = AICC_EntPos(te, &b1)->x;
	const vec3_t *t = AICC_EntPos(te, &b2);
	float d = ACtl_FlatDist(tx - px, t->z - pz);
	cSlotController *slot = AICC_Slot(&self->slotWalk);
	switch (self->walkDir) {
	case 0:
		if (d > self->farDist) {
			self->walkDir = 1;
			break;
		}
		if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &f)) {
			break;
		}
		do {
			if (self->nearDist > d && ACtl_BackFree(self)) {
				self->walkDir = 2;
			}
		} while (cSlotController_FireReset(slot, &f));
		break;
	case 1:
		if (self->farDist > d) {
			self->walkDir = 0;
		}
		break;
	case 2:
		if (d > self->nearDist) {
			self->walkDir = 0;
			break;
		}
		if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &f)) {
			break;
		}
		do {
			if (!ACtl_BackFree(self)) {
				self->walkDir = 0;
			}
		} while (cSlotController_FireReset(slot, &f));
		break;
	}
	ACtl_SetWalk(self, 1);
}

/* ---- the bank master ---- */

cAICharControl *__thiscall cAIControlBankMaster_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl2D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLBANKMASTER_VFTABLE;
	return self;
}

void __fastcall cAIControlBankMaster_dtor(cAIControlBankMaster *self) {
	self->vftable = CAICONTROLBANKMASTER_VFTABLE;
	cAICharControl2D_dtor(self);
}

cAIControlBankMaster *__thiscall cAIControlBankMaster_Delete(cAIControlBankMaster *self, byte flags) {
	self->vftable = CAICONTROLBANKMASTER_VFTABLE;
	cAICharControl2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

void __thiscall cAIControlBankMaster_Orbit(cAIControlBankMaster *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t d, w, out, b1, b2;
	vec3_t fwd = {0.0f, 0.0f, -1.0f};
	float f, v;

	vt->Strafe2(self);
	if (self->side || self->blocked) {
		return;
	}
	cEntity *t = AICC_TargetEnt(self);
	const vec3_t *tp = AICC_EntPos(t, &b1);
	float dist = vt->Dist(self, AICC_Pos(self, &b2), tp);
	if (self->orbitSide) {
		if (!self->direct || dist > self->farDist + self->farDist || self->ctrl->act ||
			AIChar_ActionIs(self->ent, "ActionEnd")) {
			self->orbitSide = 0;
		} else {
			tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
			const vec3_t *s = AICC_Pos(self, &b2);
			d.x = s->x - tp->x;
			d.y = 0.0f;
			d.z = s->z - tp->z;
			if ((double)g_AngAttackBackCharAI > Vec3_Angle(&d, &self->orbitDir)) {
				self->orbitSide = 0;
			} else {
				vec3_t ang = WChar(AICC_TargetEnt(self))->view;
				const vec3_t *tv = Vec3_Rotate(&out, &fwd, &ang);
				w.x = -tv->x;
				w.z = -tv->z;
				w.y = 0.0f;
				if ((double)g_AngAttackBackCharAI > Vec3_Angle(&d, &w)) {
					self->orbitSide = 0;
				}
			}
		}
	}
	if (!self->orbitStart && (!self->direct || dist > self->farDist * 1.1f) && !self->ctrl->act) {
		self->orbitStart = 1;
	}
	if (self->walkDir == 0 && self->orbitSide == 0 && self->orbitStart) {
		vt->StartOrbit(self, dist - 4.0f);
		self->orbitStart = 0;
		self->orbitDir = WChar(AICC_TargetEnt(self))->view;
		const vec3_t *tv = Vec3_Rotate(&out, &fwd, &self->orbitDir);
		self->orbitDir.x = -tv->x;
		self->orbitDir.y = -tv->y;
		self->orbitDir.z = -tv->z;
		self->orbitDir.y = 0.0f;
	}
	cSlotController *slot = AICC_Slot(&self->slotOrbit);
	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			if (vt->SideBlocked(self, self->orbitSide)) {
				self->orbitSide = 0;
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	/* orbitSide is 0, 1 or 2 (the original strafes by a stale local for another value) */
	if (self->orbitSide == 0) {
		v = 0.0f;
	} else {
		v = self->orbitSide == 1 ? self->speed : -self->speed;
		if (v != 0.0f) {
			if (self->walkDir == 0) {
				if (Vec3_Equal(&self->lastTargetPos, AICC_EntPos(AICC_TargetEnt(self), &b1))) {
					self->turnGain = 25.0f;
					self->turnStep = 2.0f;
				} else {
					vt->SetTurnSpeed(self, self->speed);
				}
			} else if (self->walkDir == 1 || self->walkDir == 2) {
				vt->SetTurnSpeed(self, self->speed);
			}
		}
	}
	self->ctrl->strafe = v;
	self->lastTargetPos = *AICC_EntPos(AICC_TargetEnt(self), &b1);
}

/* ---- the strafe of the bishop and the gear ---- */

static void ACtl_ShootStrafe(cAIControlShoot *self, int gear) {
	cAICharControl *c = &self->base;
	vec3_t b1, b2, d;
	float f;

	self->time = c->dt + self->time;
	cEntity *t = AICC_TargetEnt(c);
	if (!WithLife_Vtbl(t)->GetActCtrl(t)) {
		self->time = self->strafeTime;
		self->side = 0;
		goto move;
	}
	const char *cls = ACtl_TargetClass(c);
	if (strcmp(cls, "Bishop") && strcmp(cls, "Gear") && strcmp(cls, "Dtritus")) {
		goto move;
	}
	const vec3_t *tp = AICC_EntPos(AICC_TargetEnt(c), &b1);
	const vec3_t *sp = AICC_Pos(c, &b2);
	d.y = 0.0f;
	d.x = sp->x - tp->x;
	d.z = sp->z - tp->z;
	if (ACtl_TargetActing(c)) {
		if (self->other) {
			self->side = (self->side != 2) + 1;
			self->time = self->strafeTime;
		}
	} else if (c->walkDir != 0) {
		goto reset;
	}
	if ((gear && AIChar_ActionIs(c->ent, "ActionInit")) || AIChar_ActionIs(c->ent, "ActionEnd")) {
		goto reset;
	}
	if (!(self->time >= self->strafeTime)) {
		goto slot;
	}
	if (!((double)self->strafeAng > ACtl_AngleToView(c, &d))) {
		goto reset;
	}
	ACtl_RandomSide(self);
	goto slot;
reset:
	self->side = 0;
	self->time = self->strafeTime;
slot:
	if (self->side != 0) {
		cSlotController *s = AICC_Slot(&self->slotSide);
		if (cSlotController_Tick(s, c->dt) && cSlotController_FireReset(s, &f)) {
			do {
				if (AICC_Vtbl(c)->SideBlocked(c, self->side)) {
					self->other = 1;
					self->time = self->strafeTime * 0.1f + self->time;
				} else {
					self->other = 0;
				}
			} while (cSlotController_FireReset(s, &f));
		}
	}
move:
	if (self->other || self->side == 0) {
		c->ctrl->strafe = 0.0f;
		return;
	}
	if (self->side == 1) {
		c->ctrl->strafe = ACtl_TargetActing(c) ? 1.0f : c->speed;
	} else if (self->side == 2) {
		c->ctrl->strafe = ACtl_TargetActing(c) ? -1.0f : -c->speed;
	}
}

/* Strafe2: the strafe around the characters in the way first, then the one of the class */
static int ACtl_FreeToStrafe(cAICharControl *c) {
	AICC_Vtbl(c)->Strafe(c);
	return c->side == 0 && !c->blocked && !c->avoidFound && !c->link.target;
}

void __thiscall cAIControlShoot_Walk(cAIControlShoot *self) {
	if (self->side == 0) {
		cAICharControl2D_Walk(&self->base);
		return;
	}
	self->base.ctrl->walk = 0.0f;
}

/* ---- the bishop ---- */

cAICharControl *__thiscall cAIControlBishop_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl2D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLBISHOP_VFTABLE);
}

void __fastcall cAIControlBishop_dtor(cAICharControl *self) {
	self->vftable = CAICONTROLBISHOP_VFTABLE;
	cAICharControl2D_dtor(self);
}

cAICharControl *__thiscall cAIControlBishop_Delete(cAICharControl *self, byte flags) {
	self->vftable = CAICONTROLBISHOP_VFTABLE;
	cAICharControl2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICONTROLSHOOT_SIZE);
	}
	return self;
}

byte __thiscall cAIControlBishop_CheckWait(cAICharControl *self) {
	byte r = cAICharControl_CheckWait(self);

	if (r && AIChar_ActionIs(AICC_TargetEnt(self), "ActionEnd")) {
		self->waitStart = 0;
		return 0;
	}
	return r;
}

void __thiscall cAIControlBishop_Strafe2(cAIControlBishop *self) {
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlBishop_Strafe(self);
	}
}

void __thiscall cAIControlBishop_Strafe(cAIControlBishop *self) {
	ACtl_ShootStrafe(self, 0);
}

void __thiscall cAIControlBishop_SetParams(cAIControlBishop *self) {
	if (!self->base.target) {
		return;
	}
	cEntity *t = self->base.target->target;
	if (t->vftable->GetAI(t)) {
		self->strafeTime = g_TimeStrafeUnderAttackBishopAIDef;
		self->strafeAng = g_AngleStrafeUnderAttackBishopAIDef;
		return;
	}
	self->strafeTime = g_TimeStrafeUnderAttackBishopAI;
	self->strafeAng = g_AngleStrafeUnderAttackBishopAI;
}

/* ---- the gear ---- */

cAICharControl *__thiscall cAIControlGear_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl2D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLGEAR_VFTABLE);
}

void __fastcall cAIControlGear_dtor(cAICharControl *self) {
	self->vftable = CAICONTROLGEAR_VFTABLE;
	cAICharControl2D_dtor(self);
}

cAICharControl *__thiscall cAIControlGear_Delete(cAICharControl *self, byte flags) {
	self->vftable = CAICONTROLGEAR_VFTABLE;
	cAICharControl2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICONTROLSHOOT_SIZE);
	}
	return self;
}

byte __thiscall cAIControlGear_CheckWait(cAICharControl *self) {
	byte r = cAICharControl_CheckWait(self);

	if (!r) {
		return r;
	}
	if (!AIChar_ActionIs(AICC_TargetEnt(self), "ActionInit") && !AIChar_ActionIs(AICC_TargetEnt(self), "ActionEnd")) {
		return r;
	}
	self->waitStart = 0;
	return 0;
}

/* the turn stopped during the PostFusion action of the gear; 0 then */
static int ACtl_GearFusion(cAICharControl *self) {
	if (!AIChar_ActionIs(self->ent, "PostFusion")) {
		return 0;
	}
	self->ctrl->vRot = 0.0f;
	self->ctrl->hRot = 0.0f;
	return 1;
}

void __thiscall cAIControlGear_TurnFollow(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl_TurnFollow(self);
	}
}

void __thiscall cAIControlGear_Turn(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl2D_Turn(self);
	}
}

void __thiscall cAIControlGear_DirTurn(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl_DirTurn(self);
	}
}

void __thiscall cAIControlGear_Strafe2(cAIControlGear *self) {
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlGear_Strafe(self);
	}
}

void __thiscall cAIControlGear_Strafe(cAIControlGear *self) {
	ACtl_ShootStrafe(self, 1);
}

byte __thiscall cAIControlGear_Vf89(cAICharControl *self) {
	return cAIGear_IsBlocked((cAIGear *)self->ai);
}

void __thiscall cAIControlGear_SetParams(cAIControlGear *self) {
	if (!self->base.target || !self->base.target->target) {
		return;
	}
	cEntity *t = self->base.target->target;
	if (t->vftable->GetAI(t)) {
		self->strafeTime = g_TimeStrafeUnderAttackGearAIDef;
		self->strafeAng = g_AngleStrafeUnderAttackGearAIDef;
		return;
	}
	self->strafeTime = g_TimeStrafeUnderAttackGearAI;
	self->strafeAng = g_AngleStrafeUnderAttackGearAI;
}

void __thiscall cAIControlGear_SetTurnSpeed(cAICharControl *self, float speed) {
	if (0.55f >= speed) {
		self->turnGain = 5.0f;
		self->turnStep = 1.0f;
	} else {
		float t = speed - 0.5f;
		float a = t * 9.0f;
		float b = t * 2.5f;
		self->turnStep = a + a + 1.0f;
		self->turnGain = b + b + 5.0f;
	}
	self->f204 = 10.0f;
	self->f208 = 1.0f;
}

/* ---- the messenger ---- */

cAICharControl *__thiscall cAIControlMessenger_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl3D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLMESSENGER_VFTABLE;
	return self;
}

void __fastcall cAIControlMessenger_dtor(cAIControlMessenger *self) {
	self->vftable = CAICONTROLMESSENGER_VFTABLE;
	cAICharControl3D_dtor(self);
}

cAIControlMessenger *__thiscall cAIControlMessenger_Delete(cAIControlMessenger *self, byte flags) {
	self->vftable = CAICONTROLMESSENGER_VFTABLE;
	cAICharControl3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

byte __thiscall cAIControlMessenger_UpdateFollowMode(cAIControlMessenger *self, float dt) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	(void)dt;
	if (self->path.count <= 0) {
		return 1;
	}
	if (!vt->UpdateFollow(self)) {
		return 0;
	}
	if (!self->direct) {
		if (!vt->CheckFollow(self)) {
			return 0;
		}
	} else {
		self->avoidA.x = -FLT_MAX;
		self->avoidA.y = -FLT_MAX;
		self->avoidA.z = -FLT_MAX;
		self->avoidB.x = -FLT_MAX;
		self->avoidB.y = -FLT_MAX;
		self->avoidB.z = -FLT_MAX;
	}
	if (self->direct) {
		vt->Strafe2(self);
		vt->TurnFollow(self);
		vt->Walk(self);
		return 1;
	}
	vt->Strafe(self);
	vt->Turn(self);
	vt->Walk2(self);
	return 1;
}

void __thiscall cAIControlMessenger_SetTurnSpeed(cAIControlMessenger *self, float speed) {
	if (0.55f >= speed) {
		self->turnGain = 5.0f;
		self->turnStep = 1.0f;
	} else {
		self->turnGain = 30.0f;
		self->turnStep = 20.0f;
	}
	self->f204 = g_VelRotDynamicMessengerAI;
	self->f208 = g_LimVelRotDynamicMessengerAI;
}

/* ---- the nurse ---- */

cAICharControl *__thiscall cAIControlNurse_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl3D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLNURSE_VFTABLE;
	return self;
}

void __fastcall cAIControlNurse_dtor(cAIControlNurse *self) {
	self->vftable = CAICONTROLNURSE_VFTABLE;
	cAICharControl3D_dtor(self);
}

cAIControlNurse *__thiscall cAIControlNurse_Delete(cAIControlNurse *self, byte flags) {
	self->vftable = CAICONTROLNURSE_VFTABLE;
	cAICharControl3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

void __thiscall cAIControlNurse_Walk(cAIControlNurse *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t b1, b2;

	const vec3_t *p = AICC_Pos(self, &b1);
	const vec3_t *t = AICC_EntPos(te, &b2);
	float d = ACtl_Dist(t->x - p->x, t->y - p->y, t->z - p->z);
	if (self->walkDir == 0) {
		if (d > self->farDist) {
			self->walkDir = 1;
		}
	} else if (self->walkDir == 1) {
		if (self->farDist > d) {
			self->walkDir = 0;
		}
	}
	ACtl_SetWalk(self, 0);
}

/* ---- the sentinel ---- */

cAICharControl *__thiscall cAIControlSentinel_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl3D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLSENTINEL_VFTABLE);
}

void __fastcall cAIControlSentinel_dtor(cAICharControl *self) {
	self->vftable = CAICONTROLSENTINEL_VFTABLE;
	cAICharControl3D_dtor(self);
}

cAICharControl *__thiscall cAIControlSentinel_Delete(cAICharControl *self, byte flags) {
	self->vftable = CAICONTROLSENTINEL_VFTABLE;
	cAICharControl3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICONTROLSENTINEL_SIZE);
	}
	return self;
}

void __thiscall cAIControlSentinel_Strafe2(cAIControlShoot *self) {
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlSentinel_Strafe(self);
	}
}

void __thiscall cAIControlSentinel_Strafe(cAIControlShoot *self) {
	cAICharControl *c = &self->base;
	vec3_t b1, b2, d;
	float f;

	self->time = c->dt + self->time;
	cEntity *t = AICC_TargetEnt(c);
	if (!WithLife_Vtbl(t)->GetActCtrl(t)) {
		goto reset;
	}
	const char *cls = ACtl_TargetClass(c);
	if (strcmp(cls, "Bishop") && strcmp(cls, "Gear")) {
		if (!(self->time >= g_TimeStrafeUnderAttackSentinelAI)) {
			goto hit;
		}
		goto off;
	}
	const vec3_t *tp = AICC_EntPos(AICC_TargetEnt(c), &b1);
	const vec3_t *sp = AICC_Pos(c, &b2);
	d.y = 0.0f;
	d.x = sp->x - tp->x;
	d.z = sp->z - tp->z;
	if (!(g_TimeStrafeUnderAttackSentinelAI > self->time) && !ACtl_TargetActing(c)) {
		goto reset;
	}
	if (!(self->time >= g_TimeStrafeUnderAttackSentinelAI)) {
		goto hit;
	}
	if (!((double)g_AngleStrafeUnderAttackSentinelAI > ACtl_AngleToView(c, &d))) {
		goto reset;
	}
	ACtl_RandomSide(self);
	goto hit;
reset:
	self->time = g_TimeStrafeUnderAttackSentinelAI;
off:
	self->side = 0;
hit:
	/* hit since the last frame */
	if (self->side == 0 && c->lifeMark > WChar(c->ent)->wl.life) {
		ACtl_RandomSide(self);
	}
	int side = self->side;
	c->lifeMark = WChar(c->ent)->wl.life;
	if (side != 0) {
		cSlotController *s = AICC_Slot(&self->slotSide);
		if (cSlotController_Tick(s, c->dt) && cSlotController_FireReset(s, &f)) {
			do {
				if (AICC_Vtbl(c)->SideBlocked(c, self->side)) {
					self->other = 1;
					self->side = (self->side != 2) + 1;
				} else {
					self->other = 0;
				}
			} while (cSlotController_FireReset(s, &f));
		}
	}
	if (self->other || self->side == 0) {
		c->ctrl->strafe = 0.0f;
		return;
	}
	if (self->side == 1) {
		c->ctrl->strafe = 1.0f;
	} else if (self->side == 2) {
		c->ctrl->strafe = -1.0f;
	}
}

void __thiscall cAIControlSentinel_Walk(cAIControlShoot *self) {
	if (self->side == 0) {
		cAICharControl3D_Walk(&self->base);
		return;
	}
	self->base.ctrl->walk = 0.0f;
}
