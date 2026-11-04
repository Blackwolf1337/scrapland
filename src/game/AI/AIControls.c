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
#include "scrap/staticinit.h"

#define CAICONTROLBANKDIRECTOR_VFTABLE (&cAIControlBankDirector_vftable.base)
#define CAICONTROLBANKMASTER_VFTABLE (&cAIControlBankMaster_vftable.base)
#define CAICONTROLBISHOP_VFTABLE (&cAIControlBishop_vftable.base)
#define CAICONTROLGEAR_VFTABLE (&cAIControlGear_vftable.base)
#define CAICONTROLMESSENGER_VFTABLE (&cAIControlMessenger_vftable.base)
#define CAICONTROLNURSE_VFTABLE (&cAIControlNurse_vftable.base)
#define CAICONTROLSENTINEL_VFTABLE (&cAIControlSentinel_vftable.base)
#define g_TimeStrafeUnderAttackBishopAI (GVar_TimeStrafeUnderAttackBishopAI.value.f)
#define g_TimeStrafeUnderAttackBishopAIDef (GVar_TimeStrafeUnderAttackBishopAI.def.f)
#define g_AngleStrafeUnderAttackBishopAI (GVar_AngleStrafeUnderAttackBishopAI.value.f)
#define g_AngleStrafeUnderAttackBishopAIDef (GVar_AngleStrafeUnderAttackBishopAI.def.f)
#define g_TimeStrafeUnderAttackGearAI (GVar_TimeStrafeUnderAttackGearAI.value.f)
#define g_TimeStrafeUnderAttackGearAIDef (GVar_TimeStrafeUnderAttackGearAI.def.f)
#define g_AngleStrafeUnderAttackGearAI (GVar_AngleStrafeUnderAttackGearAI.value.f)
#define g_AngleStrafeUnderAttackGearAIDef (GVar_AngleStrafeUnderAttackGearAI.def.f)
#define g_TimeStrafeUnderAttackSentinelAI (GVar_TimeStrafeUnderAttackSentinelAI.value.f)
#define g_AngleStrafeUnderAttackSentinelAI (GVar_AngleStrafeUnderAttackSentinelAI.value.f)
#define g_VelRotDynamicMessengerAI (GVar_VelRotDynamicMessengerAI.value.f)
#define g_LimVelRotDynamicMessengerAI (GVar_LimVelRotDynamicMessengerAI.value.f)
static const cAICharControl3DVtbl cAIControlSentinel_vftable;
static const cAICharControl3DVtbl cAIControlNurse_vftable;
static const cAICharControl3DVtbl cAIControlMessenger_vftable;
static const cAICharControl2DVtbl cAIControlGear_vftable;
static const cAICharControl2DVtbl cAIControlBishop_vftable;
static const cAICharControl2DVtbl cAIControlBankMaster_vftable;
static const cAICharControl2DVtbl cAIControlBankDirector_vftable;
static cGVar GVar_VelRotDynamicMessengerAI;
static cGVar GVar_TimeStrafeUnderAttackSentinelAI;
static cGVar GVar_TimeStrafeUnderAttackGearAI;
static cGVar GVar_TimeStrafeUnderAttackBishopAI;
static cGVar GVar_LimVelRotDynamicMessengerAI;
static cGVar GVar_AngleStrafeUnderAttackSentinelAI;
static cGVar GVar_AngleStrafeUnderAttackGearAI;
static cGVar GVar_AngleStrafeUnderAttackBishopAI;

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
	float r = Obj_GetRadius(me);
	const vec3_t *tp = AICC_EntPos(e, &b1);

	return AICC_TestSides(self, AICC_EntPos(me, &b2), tp, r);
}

static int ACtl_TargetActing(cAICharControl *self) {
	return AIChar_ActionIs(AICC_TargetEnt(self), "Action") || AIChar_ActionIs(AICC_TargetEnt(self), "ActionInit") ||
		AIChar_ActionIs(AICC_TargetEnt(self), "ActionEnd");
}

/* the name of the action class of the target */
static const char *ACtl_TargetClass(cAICharControl *self) {
	cEntity *t = AICC_TargetEnt(self);

	return ((cActCtrl *)WithLife_GetActCtrl(t))->cls->name;
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

static cAICharControl *ACtl_ShootInit(cAICharControl *self, const cAICharControlVtbl *vftable) {
	cAIControlShoot *s = (cAIControlShoot *)self;

	self->vftable = vftable;
	cSlotController_ctor(&s->slotSide, 0.2f);
	s->side = 0;
	s->time = 0.0f;
	s->other = 1;
	return self;
}

/* ---- the bank director ---- */

cAICharControl *cAIControlBankDirector_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl2D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLBANKDIRECTOR_VFTABLE;
	return self;
}

static void cAIControlBankDirector_Free(cAIControlBankDirector *self) {
	cAICharControl2D_dtor(self);
	operator_delete(self);
}

/* the Walk of the bank director and the master: forward beyond farDist, back within nearDist when the way back is
 * free */
static void cAIControlBank_Walk(cAICharControl *self) {
	cEntity *te = AICC_TargetEnt(self);
	vec3_t b1, b2;
	float f;

	float px = AICC_Pos(self, &b1)->x;
	float pz = AICC_Pos(self, &b2)->z;
	float tx = AICC_EntPos(te, &b1)->x;
	const vec3_t *t = AICC_EntPos(te, &b2);
	float d = ACtl_FlatDist(tx - px, t->z - pz);
	cSlotController *slot = &self->slotWalk;
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

cAICharControl *cAIControlBankMaster_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl2D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLBANKMASTER_VFTABLE;
	return self;
}

static void cAIControlBankMaster_Free(cAIControlBankMaster *self) {
	cAICharControl2D_dtor(self);
	operator_delete(self);
}

/* the Orbit of the control, but not while the action button is down or its ActionEnd plays */
static void cAIControlBankMaster_Orbit(cAIControlBankMaster *self) {
	vec3_t d, w, out, b1, b2;
	vec3_t fwd = {0.0f, 0.0f, -1.0f};
	float f, v;

	AICC_Strafe2(self);
	if (self->side || self->blocked) {
		return;
	}
	cEntity *t = AICC_TargetEnt(self);
	const vec3_t *tp = AICC_EntPos(t, &b1);
	float dist = AICC_Dist(self, AICC_Pos(self, &b2), tp);
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
		AICC_StartOrbit(self, dist - 4.0f);
		self->orbitStart = 0;
		self->orbitDir = WChar(AICC_TargetEnt(self))->view;
		const vec3_t *tv = Vec3_Rotate(&out, &fwd, &self->orbitDir);
		self->orbitDir.x = -tv->x;
		self->orbitDir.y = -tv->y;
		self->orbitDir.z = -tv->z;
		self->orbitDir.y = 0.0f;
	}
	cSlotController *slot = &self->slotOrbit;
	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			if (AICC_SideBlocked(self, self->orbitSide)) {
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
					AICC_SetTurnSpeed(self, self->speed);
				}
			} else if (self->walkDir == 1 || self->walkDir == 2) {
				AICC_SetTurnSpeed(self, self->speed);
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
	if (!WithLife_GetActCtrl(t)) {
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
		cSlotController *s = &self->slotSide;
		if (cSlotController_Tick(s, c->dt) && cSlotController_FireReset(s, &f)) {
			do {
				if (AICC_SideBlocked(c, self->side)) {
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
	AICC_Strafe(c);
	return c->side == 0 && !c->blocked && !c->avoidFound && !c->link.target;
}

/* the 2D Walk without a strafe of its own (the gear too) */
static void cAIControlShoot_Walk(cAICharControl *elem) {
	cAIControlShoot *self = (cAIControlShoot *)elem;
	if (self->side == 0) {
		cAICharControl2D_Walk(&self->base);
		return;
	}
	self->base.ctrl->walk = 0.0f;
}

/* ---- the bishop ---- */

cAICharControl *cAIControlBishop_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl2D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLBISHOP_VFTABLE);
}

static void cAIControlBishop_Free(cAICharControl *self) {
	cAICharControl2D_dtor(self);
	operator_delete(self);
}

/* CheckWait, but not while the target plays its ActionEnd */
static byte cAIControlBishop_CheckWait(cAICharControl *self) {
	byte r = cAICharControl_CheckWait(self);

	if (r && AIChar_ActionIs(AICC_TargetEnt(self), "ActionEnd")) {
		self->waitStart = 0;
		return 0;
	}
	return r;
}

static void cAIControlBishop_Strafe(cAIControlBishop *self);
static void cAIControlBishop_Strafe2(cAICharControl *elem) {
	cAIControlBishop *self = (cAIControlBishop *)elem;
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlBishop_Strafe(self);
	}
}

static void cAIControlBishop_Strafe(cAIControlBishop *self) {
	ACtl_ShootStrafe(self, 0);
}

/* Nop of the control: the strafe parameters by the target */
static void cAIControlBishop_SetParams(cAICharControl *elem) {
	cAIControlBishop *self = (cAIControlBishop *)elem;
	/* the target may have died since the link was set: then the link is there but empty */
	if (!self->base.target || !self->base.target->target) {
		return;
	}
	cEntity *t = self->base.target->target;
	if (Entity_GetAI(t)) {
		self->strafeTime = g_TimeStrafeUnderAttackBishopAIDef;
		self->strafeAng = g_AngleStrafeUnderAttackBishopAIDef;
		return;
	}
	self->strafeTime = g_TimeStrafeUnderAttackBishopAI;
	self->strafeAng = g_AngleStrafeUnderAttackBishopAI;
}

/* ---- the gear ---- */

cAICharControl *cAIControlGear_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl2D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLGEAR_VFTABLE);
}

static void cAIControlGear_Free(cAICharControl *self) {
	cAICharControl2D_dtor(self);
	operator_delete(self);
}

/* CheckWait, but not while the target plays its ActionInit or ActionEnd */
static byte cAIControlGear_CheckWait(cAICharControl *self) {
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

/* the turns stop during the PostFusion action of the gear */
static void cAIControlGear_TurnFollow(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl_TurnFollow(self);
	}
}

static void cAIControlGear_Turn(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl2D_Turn(self);
	}
}

static void cAIControlGear_DirTurn(cAICharControl *self) {
	if (!ACtl_GearFusion(self)) {
		cAICharControl_DirTurn(self);
	}
}

static void cAIControlGear_Strafe(cAIControlGear *self);
static void cAIControlGear_Strafe2(cAICharControl *elem) {
	cAIControlGear *self = (cAIControlGear *)elem;
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlGear_Strafe(self);
	}
}

static void cAIControlGear_Strafe(cAIControlGear *self) {
	ACtl_ShootStrafe(self, 1);
}

/* the AI of the gear saw the way to the target blocked */
static byte cAIControlGear_AIBlocked(cAICharControl *self) {
	return cAIGear_IsBlocked((cAIGear *)self->ai);
}

static void cAIControlGear_SetParams(cAICharControl *elem) {
	cAIControlGear *self = (cAIControlGear *)elem;
	if (!self->base.target || !self->base.target->target) {
		return;
	}
	cEntity *t = self->base.target->target;
	if (Entity_GetAI(t)) {
		self->strafeTime = g_TimeStrafeUnderAttackGearAIDef;
		self->strafeAng = g_AngleStrafeUnderAttackGearAIDef;
		return;
	}
	self->strafeTime = g_TimeStrafeUnderAttackGearAI;
	self->strafeAng = g_AngleStrafeUnderAttackGearAI;
}

static void cAIControlGear_SetTurnSpeed(cAICharControl *self, float speed) {
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
	self->viewGain = 10.0f;
	self->viewStep = 1.0f;
}

/* ---- the messenger ---- */

cAICharControl *cAIControlMessenger_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl3D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLMESSENGER_VFTABLE;
	return self;
}

static void cAIControlMessenger_Free(cAIControlMessenger *self) {
	cAICharControl3D_dtor(self);
	operator_delete(self);
}

static byte cAIControlMessenger_UpdateFollowMode(cAIControlMessenger *self, float dt) {

	(void)dt;
	if (self->path.count <= 0) {
		return 1;
	}
	if (!AICC_UpdateFollow(self)) {
		return 0;
	}
	if (!self->direct) {
		if (!AICC_CheckFollow(self)) {
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
		AICC_Strafe2(self);
		AICC_TurnFollow(self);
		AICC_Walk(self);
		return 1;
	}
	AICC_Strafe(self);
	AICC_Turn(self);
	AICC_Walk2(self);
	return 1;
}

static void cAIControlMessenger_SetTurnSpeed(cAIControlMessenger *self, float speed) {
	if (0.55f >= speed) {
		self->turnGain = 5.0f;
		self->turnStep = 1.0f;
	} else {
		self->turnGain = 30.0f;
		self->turnStep = 20.0f;
	}
	self->viewGain = g_VelRotDynamicMessengerAI;
	self->viewStep = g_LimVelRotDynamicMessengerAI;
}

/* ---- the nurse ---- */

cAICharControl *cAIControlNurse_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	cAICharControl3D_ctor(mem, e, ai, arg);
	self->vftable = CAICONTROLNURSE_VFTABLE;
	return self;
}

static void cAIControlNurse_Free(cAIControlNurse *self) {
	cAICharControl3D_dtor(self);
	operator_delete(self);
}

/* forward beyond farDist only (3D) */
static void cAIControlNurse_Walk(cAIControlNurse *self) {
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

cAICharControl *cAIControlSentinel_ctor(void *mem, cEntity *e, cAIChar *ai, int *arg) {
	cAICharControl3D_ctor(mem, e, ai, arg);
	return ACtl_ShootInit(mem, CAICONTROLSENTINEL_VFTABLE);
}

static void cAIControlSentinel_Free(cAICharControl *self) {
	cAICharControl3D_dtor(self);
	operator_delete(self);
}

static void cAIControlSentinel_Strafe(cAIControlShoot *self);
static void cAIControlSentinel_Strafe2(cAICharControl *elem) {
	cAIControlShoot *self = (cAIControlShoot *)elem;
	if (ACtl_FreeToStrafe(&self->base)) {
		cAIControlSentinel_Strafe(self);
	}
}

static void cAIControlSentinel_Strafe(cAIControlShoot *self) {
	cAICharControl *c = &self->base;
	vec3_t b1, b2, d;
	float f;

	self->time = c->dt + self->time;
	cEntity *t = AICC_TargetEnt(c);
	if (!WithLife_GetActCtrl(t)) {
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
		cSlotController *s = &self->slotSide;
		if (cSlotController_Tick(s, c->dt) && cSlotController_FireReset(s, &f)) {
			do {
				if (AICC_SideBlocked(c, self->side)) {
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

/* the 3D Walk without a strafe of its own */
static void cAIControlSentinel_Walk(cAICharControl *elem) {
	cAIControlShoot *self = (cAIControlShoot *)elem;
	if (self->side == 0) {
		cAICharControl3D_Walk(&self->base);
		return;
	}
	self->base.ctrl->walk = 0.0f;
}

/* the vtable of cAIControlBankDirector */
static const cAICharControl2DVtbl cAIControlBankDirector_vftable = {
	.base.Free = cAIControlBankDirector_Free,
	.base.Draw = cAICharControl2D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl2D_TestPath,
	.base.TestLine = cAICharControl2D_TestLine,
	.base.TestSides = cAICharControl2D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl2D_StrafeBack,
	.base.StrafeFwd = cAICharControl2D_StrafeFwd,
	.base.TryStrafe = cAICharControl2D_TryStrafe,
	.base.Facing = cAICharControl2D_Facing,
	.base.InWay = cAICharControl2D_InWay,
	.base.Near = cAICharControl2D_Near,
	.base.KeepDist = cAICharControl2D_KeepDist,
	.base.StrafeDist = cAICharControl2D_StrafeDist,
	.base.OnFloor = cAICharControl2D_OnFloor,
	.base.GoalAhead = cAICharControl2D_GoalAhead,
	.base.TestPathEnd = cAICharControl2D_TestPathEnd,
	.base.Dist = cAICharControl2D_Dist,
	.base.DistSq = cAICharControl2D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl2D_Behind,
	.base.InFront = cAICharControl2D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl2D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl2D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl2D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl2D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl2D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl2D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAICharControl_Strafe2,
	.base.Walk = cAIControlBank_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl2D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.FloorAhead = cAICharControl2D_FloorAhead,
	.FloorAt = cAICharControl2D_FloorAt,
	.TestStep = cAICharControl2D_TestStep,
};

/* the vtable of cAIControlBankMaster */
static const cAICharControl2DVtbl cAIControlBankMaster_vftable = {
	.base.Free = cAIControlBankMaster_Free,
	.base.Draw = cAICharControl2D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl2D_TestPath,
	.base.TestLine = cAICharControl2D_TestLine,
	.base.TestSides = cAICharControl2D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl2D_StrafeBack,
	.base.StrafeFwd = cAICharControl2D_StrafeFwd,
	.base.TryStrafe = cAICharControl2D_TryStrafe,
	.base.Facing = cAICharControl2D_Facing,
	.base.InWay = cAICharControl2D_InWay,
	.base.Near = cAICharControl2D_Near,
	.base.KeepDist = cAICharControl2D_KeepDist,
	.base.StrafeDist = cAICharControl2D_StrafeDist,
	.base.OnFloor = cAICharControl2D_OnFloor,
	.base.GoalAhead = cAICharControl2D_GoalAhead,
	.base.TestPathEnd = cAICharControl2D_TestPathEnd,
	.base.Dist = cAICharControl2D_Dist,
	.base.DistSq = cAICharControl2D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl2D_Behind,
	.base.InFront = cAICharControl2D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl2D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl2D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl2D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl2D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl2D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl2D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAICharControl_Strafe2,
	.base.Walk = cAIControlBank_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl2D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAIControlBankMaster_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.FloorAhead = cAICharControl2D_FloorAhead,
	.FloorAt = cAICharControl2D_FloorAt,
	.TestStep = cAICharControl2D_TestStep,
};

/* the vtable of cAIControlBishop */
static const cAICharControl2DVtbl cAIControlBishop_vftable = {
	.base.Free = cAIControlBishop_Free,
	.base.Draw = cAICharControl2D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl2D_TestPath,
	.base.TestLine = cAICharControl2D_TestLine,
	.base.TestSides = cAICharControl2D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAIControlBishop_SetParams,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl2D_StrafeBack,
	.base.StrafeFwd = cAICharControl2D_StrafeFwd,
	.base.TryStrafe = cAICharControl2D_TryStrafe,
	.base.Facing = cAICharControl2D_Facing,
	.base.InWay = cAICharControl2D_InWay,
	.base.Near = cAICharControl2D_Near,
	.base.KeepDist = cAICharControl2D_KeepDist,
	.base.StrafeDist = cAICharControl2D_StrafeDist,
	.base.OnFloor = cAICharControl2D_OnFloor,
	.base.GoalAhead = cAICharControl2D_GoalAhead,
	.base.TestPathEnd = cAICharControl2D_TestPathEnd,
	.base.Dist = cAICharControl2D_Dist,
	.base.DistSq = cAICharControl2D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl2D_Behind,
	.base.InFront = cAICharControl2D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl2D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl2D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl2D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl2D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl2D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl2D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAIControlBishop_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAIControlBishop_Strafe2,
	.base.Walk = cAIControlShoot_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl2D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.FloorAhead = cAICharControl2D_FloorAhead,
	.FloorAt = cAICharControl2D_FloorAt,
	.TestStep = cAICharControl2D_TestStep,
};

/* the vtable of cAIControlGear */
static const cAICharControl2DVtbl cAIControlGear_vftable = {
	.base.Free = cAIControlGear_Free,
	.base.Draw = cAICharControl2D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl2D_TestPath,
	.base.TestLine = cAICharControl2D_TestLine,
	.base.TestSides = cAICharControl2D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAIControlGear_SetParams,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAIControlGear_SetTurnSpeed,
	.base.StrafeBack = cAICharControl2D_StrafeBack,
	.base.StrafeFwd = cAICharControl2D_StrafeFwd,
	.base.TryStrafe = cAICharControl2D_TryStrafe,
	.base.Facing = cAICharControl2D_Facing,
	.base.InWay = cAICharControl2D_InWay,
	.base.Near = cAICharControl2D_Near,
	.base.KeepDist = cAICharControl2D_KeepDist,
	.base.StrafeDist = cAICharControl2D_StrafeDist,
	.base.OnFloor = cAICharControl2D_OnFloor,
	.base.GoalAhead = cAICharControl2D_GoalAhead,
	.base.TestPathEnd = cAICharControl2D_TestPathEnd,
	.base.Dist = cAICharControl2D_Dist,
	.base.DistSq = cAICharControl2D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl2D_Behind,
	.base.InFront = cAICharControl2D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAIControlGear_DirTurn,
	.base.CheckArrived = cAICharControl2D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl2D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl2D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAIControlGear_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl2D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl2D_FindRoute,
	.base.TurnFollow = cAIControlGear_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAIControlGear_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAIControlGear_AIBlocked,
	.base.Strafe2 = cAIControlGear_Strafe2,
	.base.Walk = cAIControlShoot_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl2D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.FloorAhead = cAICharControl2D_FloorAhead,
	.FloorAt = cAICharControl2D_FloorAt,
	.TestStep = cAICharControl2D_TestStep,
};

/* the vtable of cAIControlMessenger */
static const cAICharControl3DVtbl cAIControlMessenger_vftable = {
	.base.Free = cAIControlMessenger_Free,
	.base.Draw = cAICharControl3D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl3D_TestPath,
	.base.TestLine = cAICharControl3D_TestLine,
	.base.TestSides = cAICharControl3D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAIControlMessenger_SetTurnSpeed,
	.base.StrafeBack = cAICharControl3D_StrafeBack,
	.base.StrafeFwd = cAICharControl3D_StrafeFwd,
	.base.TryStrafe = cAICharControl3D_TryStrafe,
	.base.Facing = cAICharControl3D_Facing,
	.base.InWay = cAICharControl3D_InWay,
	.base.Near = cAICharControl3D_Near,
	.base.KeepDist = cAICharControl3D_KeepDist,
	.base.StrafeDist = cAICharControl3D_StrafeDist,
	.base.OnFloor = cAICharControl3D_OnFloor,
	.base.GoalAhead = cAICharControl3D_GoalAhead,
	.base.TestPathEnd = cAICharControl3D_TestPathEnd,
	.base.Dist = cAICharControl3D_Dist,
	.base.DistSq = cAICharControl3D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl3D_Behind,
	.base.InFront = cAICharControl3D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl3D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl3D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl3D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl3D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl3D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl3D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAIControlMessenger_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAICharControl_Strafe2,
	.base.Walk = cAICharControl3D_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl3D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.TestStep = cAICharControl3D_TestStep,
};

/* the vtable of cAIControlNurse */
static const cAICharControl3DVtbl cAIControlNurse_vftable = {
	.base.Free = cAIControlNurse_Free,
	.base.Draw = cAICharControl3D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl3D_TestPath,
	.base.TestLine = cAICharControl3D_TestLine,
	.base.TestSides = cAICharControl3D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl3D_StrafeBack,
	.base.StrafeFwd = cAICharControl3D_StrafeFwd,
	.base.TryStrafe = cAICharControl3D_TryStrafe,
	.base.Facing = cAICharControl3D_Facing,
	.base.InWay = cAICharControl3D_InWay,
	.base.Near = cAICharControl3D_Near,
	.base.KeepDist = cAICharControl3D_KeepDist,
	.base.StrafeDist = cAICharControl3D_StrafeDist,
	.base.OnFloor = cAICharControl3D_OnFloor,
	.base.GoalAhead = cAICharControl3D_GoalAhead,
	.base.TestPathEnd = cAICharControl3D_TestPathEnd,
	.base.Dist = cAICharControl3D_Dist,
	.base.DistSq = cAICharControl3D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl3D_Behind,
	.base.InFront = cAICharControl3D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl3D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl3D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl3D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl3D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl3D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl3D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAICharControl_Strafe2,
	.base.Walk = cAIControlNurse_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl3D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.TestStep = cAICharControl3D_TestStep,
};

/* the vtable of cAIControlSentinel */
static const cAICharControl3DVtbl cAIControlSentinel_vftable = {
	.base.Free = cAIControlSentinel_Free,
	.base.Draw = cAICharControl3D_Draw,
	.base.GetWalkDir = cAICharControl_GetWalkDir,
	.base.Busy = cAICharControl_Busy,
	.base.GetOrbitSide = cAICharControl_GetOrbitSide,
	.base.Update = cAICharControl_Update,
	.base.SetMove = cAICharControl_SetMove,
	.base.Reset = cAICharControl_Reset,
	.base.SetHold = cAICharControl_SetHold,
	.base.Stop = cAICharControl_Stop,
	.base.IsDirect = cAICharControl_IsDirect,
	.base.IsDirect2 = cAICharControl_IsDirect,
	.base.GetGoal = cAICharControl_GetGoal,
	.base.GetDest = cAICharControl_GetDest,
	.base.TestPath = cAICharControl3D_TestPath,
	.base.TestLine = cAICharControl3D_TestLine,
	.base.TestSides = cAICharControl3D_TestSides,
	.base.GetAvoidA = cAICharControl_GetAvoidA,
	.base.GetAvoidB = cAICharControl_GetAvoidB,
	.base.ClearAvoid = cAICharControl_ClearAvoid,
	.base.SetSpeed = cAICharControl_SetSpeed,
	.base.RestartSlots = cAICharControl_RestartSlots,
	.base.IsBlocked = cAICharControl_IsBlocked,
	.base.GetE4 = cAICharControl_GetAvoidFound,
	.base.GetAvoidEnt = cAICharControl_GetAvoidEnt,
	.base.Nop = cAICharControl_Nop,
	.base.AtPathEnd = cAICharControl_AtPathEnd,
	.base.GoalAtStart = cAICharControl_GoalAtStart,
	.base.CheckTarget = cAICharControl_CheckTarget,
	.base.SetTurnSpeed = cAICharControl_SetTurnSpeed,
	.base.StrafeBack = cAICharControl3D_StrafeBack,
	.base.StrafeFwd = cAICharControl3D_StrafeFwd,
	.base.TryStrafe = cAICharControl3D_TryStrafe,
	.base.Facing = cAICharControl3D_Facing,
	.base.InWay = cAICharControl3D_InWay,
	.base.Near = cAICharControl3D_Near,
	.base.KeepDist = cAICharControl3D_KeepDist,
	.base.StrafeDist = cAICharControl3D_StrafeDist,
	.base.OnFloor = cAICharControl3D_OnFloor,
	.base.GoalAhead = cAICharControl3D_GoalAhead,
	.base.TestPathEnd = cAICharControl3D_TestPathEnd,
	.base.Dist = cAICharControl3D_Dist,
	.base.DistSq = cAICharControl3D_DistSq,
	.base.Away = cAICharControl_Away,
	.base.UpdateAway = cAICharControl_UpdateAway,
	.base.AwayCheck = cAICharControl_AwayCheck,
	.base.AwayWalk = cAICharControl_AwayWalk,
	.base.Behind = cAICharControl3D_Behind,
	.base.InFront = cAICharControl3D_InFront,
	.base.Route = cAICharControl_Route,
	.base.SetDir = cAICharControl_SetDir,
	.base.UpdateDir = cAICharControl_UpdateDir,
	.base.DirCheck = cAICharControl_DirCheck,
	.base.DirTurn = cAICharControl_DirTurn,
	.base.CheckArrived = cAICharControl3D_CheckArrived,
	.base.CanMove = cAICharControl_CanMove,
	.base.GoTo = cAICharControl_GoTo,
	.base.GoToDirect = cAICharControl_GoToDirect,
	.base.FindPath = cAICharControl3D_FindPath,
	.base.DirectPath = cAICharControl_DirectPath,
	.base.StepGoal = cAICharControl3D_StepGoal,
	.base.UpdateGoTo = cAICharControl_UpdateGoTo,
	.base.UpdateRoute = cAICharControl_UpdateRoute,
	.base.UpdateGoToDirect = cAICharControl_UpdateGoToDirect,
	.base.GoalOnLine = cAICharControl_GoalOnLine,
	.base.NextGoal = cAICharControl_NextGoal,
	.base.NextGoalPath = cAICharControl_NextGoalPath,
	.base.UpdatePath = cAICharControl_UpdatePath,
	.base.CheckPathEnd = cAICharControl_CheckPathEnd,
	.base.UpdateRoutePath = cAICharControl_UpdateRoutePath,
	.base.Turn = cAICharControl3D_Turn,
	.base.Strafe = cAICharControl_Strafe,
	.base.FindAvoid = cAICharControl_FindAvoid,
	.base.Avoid = cAICharControl_Avoid,
	.base.UpdateAvoid = cAICharControl_UpdateAvoid,
	.base.CharsNear = cAICharControl_CharsNear,
	.base.SideBlocked = cAICharControl3D_SideBlocked,
	.base.SideDir = cAICharControl_SideDir,
	.base.WalkFwd = cAICharControl_WalkFwd,
	.base.CheckGoTo = cAICharControl_CheckGoTo,
	.base.CheckRoute = cAICharControl_CheckRoute,
	.base.CheckFollow = cAICharControl_CheckFollow,
	.base.FindRoute = cAICharControl3D_FindRoute,
	.base.TurnFollow = cAICharControl_TurnFollow,
	.base.Follow = cAICharControl_Follow,
	.base.UpdateFollowMode = cAICharControl_UpdateFollowMode,
	.base.Wait = cAICharControl_Wait,
	.base.CheckWait = cAICharControl_CheckWait,
	.base.StrafeWalk = cAICharControl_StrafeWalk,
	.base.AIBlocked = cAICharControl_AIBlocked,
	.base.Strafe2 = cAIControlSentinel_Strafe2,
	.base.Walk = cAIControlSentinel_Walk,
	.base.Walk2 = cAICharControl_Walk2,
	.base.UpdateFollow = cAICharControl3D_UpdateFollow,
	.base.UpdateFollowB = cAICharControl_UpdateFollowB,
	.base.FollowPath = cAICharControl_FollowPath,
	.base.UpdateFollowPath = cAICharControl_UpdateFollowPath,
	.base.Orbit = cAICharControl_Orbit,
	.base.StartOrbit = cAICharControl_StartOrbit,
	.base.Stop2 = cAICharControl_Stop2,
	.TestStep = cAICharControl3D_TestStep,
};

/* the strafe parameters by the target (with an AI or not) */
static cGVar GVar_TimeStrafeUnderAttackBishopAI;
static cGVar GVar_AngleStrafeUnderAttackBishopAI;
static cGVar GVar_TimeStrafeUnderAttackGearAI;
static cGVar GVar_AngleStrafeUnderAttackGearAI;
static cGVar GVar_LimVelRotDynamicMessengerAI;
static cGVar GVar_VelRotDynamicMessengerAI;
static cGVar GVar_TimeStrafeUnderAttackSentinelAI;
static cGVar GVar_AngleStrafeUnderAttackSentinelAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_AngleStrafeUnderAttackBishopAI, &GVar_AngleStrafeUnderAttackBishopAI, "AngleStrafeUnderAttackBishopAI", 4, "Angle of the opponent to strafe under attack.\n", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_TimeStrafeUnderAttackBishopAI, &GVar_TimeStrafeUnderAttackBishopAI, "TimeStrafeUnderAttackBishopAI", 4, "Angle of the opponent to strafe under attack.\n", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_AngleStrafeUnderAttackGearAI, &GVar_AngleStrafeUnderAttackGearAI, "AngleStrafeUnderAttackGearAI", 4, "Angle of the opponent to strafe under attack.\n", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_TimeStrafeUnderAttackGearAI, &GVar_TimeStrafeUnderAttackGearAI, "TimeStrafeUnderAttackGearAI", 4, "Angle of the opponent to strafe under attack.\n", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_VelRotDynamicMessengerAI, &GVar_VelRotDynamicMessengerAI, "VelRotDinamicMessengerAI", 4, "Vel. Rot. Dinamic Messenger.\n", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_LimVelRotDynamicMessengerAI, &GVar_LimVelRotDynamicMessengerAI, "LimVelRotDinamicMessengerAI", 4, "Lim. Vel. Rot. Dinamic Messenger.\n", 0.3f)
INIT_GVAR_FLOAT(Init_GVar_TimeStrafeUnderAttackSentinelAI, &GVar_TimeStrafeUnderAttackSentinelAI, "TimeStrafeUnderAttackSentinelAI", 4, "Angle of the opponent to strafe under attack sentinel.\n", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_AngleStrafeUnderAttackSentinelAI, &GVar_AngleStrafeUnderAttackSentinelAI, "AngleStrafeUnderAttackSentinelAI", 4, "Angle of the opponent to strafe under attack.\n", 1.5707964f)
