/*
 * The move control of the AI of a character, the end of AI/AICharControl.cpp (include/scrap/aicharcontrol.h): the
 * change of the move (SetMove) and the setup of each kind, the strafe around a character in the way and the small
 * accessors. The constructor and the first part of the file (rva 0x06cba0..0x072340) are still the exe's.
 */
#include <float.h>

#include "scrap/aicharcontrol.h"
#include "scrap/fxmisc.h"

vec3_t *Vec3_NormalizeTo(vec3_t *out, const vec3_t *in) {
	float len = (float)__builtin_sqrt((double)(in->x * in->x + in->y * in->y + in->z * in->z));
	vec3_t r;

	if (len == 0.0f) {
		r = *in;
	} else {
		float inv = 1.0f / len;
		r.x = in->x * inv;
		r.y = in->y * inv;
		r.z = in->z * inv;
	}
	*out = r;
	return out;
}

void __thiscall cAICharControl_ClearAvoid(cAICharControl *self) {
	self->avoidA.x = -FLT_MAX;
	self->avoidA.y = -FLT_MAX;
	self->avoidA.z = -FLT_MAX;
	self->avoidB.x = -FLT_MAX;
	self->avoidB.y = -FLT_MAX;
	self->avoidB.z = -FLT_MAX;
}

byte __thiscall cAICharControl_Avoid(cAICharControl *self, cEntity *e, byte *found) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t toE, toGoal;
	byte res = 0;
	int side;

	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	*found = 0;
	float keep = vt->KeepDist(self, e);
	if (vt->InWay(self, self->dest, e)) {
		if (self->mode != AICC_FOLLOW && self->mode != AICC_FOLLOW_PATH) {
			self->side = 0;
			self->blocked = 1;
			self->bd5 = 1;
			return 1;
		}
		return res;
	}
	void *eai = e->vftable->GetAI(e);
	if (!vt->Facing(self, e, &toE, &toGoal)) {
		return res;
	}
	*found = 1;
	if (self->walkDir == 2) {
		goto blocked;
	}
	if (eai) {
		cAICharControl *other = cAIChar_GetControl(eai);
		if (AICC_Vtbl(other)->Busy(other)) {
			goto blocked;
		}
	}
	toE.y = 0.0f;
	toGoal.y = 0.0f;
	float cross = toGoal.x * toE.z - toE.x * toGoal.z;
	if (cross > 0.0f) {
		side = 1;
	} else if (0.0f >= cross) {
		side = 2;
	} else {
		/* NaN: the original keeps the register with e */
		side = (int)(uintptr_t)e;
	}
	float sd = vt->StrafeDist(self, e);
	if (!vt->TryStrafe(self, side, e, 1, keep, 1, sd, 1)) {
		side = side == 1 ? 2 : 1;
		if (!vt->TryStrafe(self, side, e, 1, keep, 1, sd, 0)) {
			return res;
		}
		self->avoidDir.x = -toE.x;
		self->avoidDir.y = -toE.y;
		self->avoidDir.z = -toE.z;
	}
	self->side = side;
	self->blocked = 0;
	return 1;

blocked:
	self->side = 0;
	self->blocked = 1;
	return 1;
}

/* the common part of Follow and FollowPath after the target is set */
static byte AICC_FollowStart(cAICharControl *self, Nto1Node *target) {
	vec3_t t;

	self->target = target;
	self->targetEnt = target->target;
	float r = Obj_Vtbl(target->target)->GetRadius(target->target);
	self->arriveDist = r;
	self->arriveR = Float_MinSS(self->radius, r);
	cEntity *te = AICC_TargetEnt(self);
	byte ok = AICC_Vtbl(self)->FindPath(self, Obj_Vtbl(te)->base.GetPos(te, &t));
	if (!ok) {
		return ok;
	}
	if (self->goal.x == self->dest.x && self->goal.y == self->dest.y && self->goal.z == self->dest.z) {
		self->direct = 1;
		return 1;
	}
	self->direct = 0;
	return 1;
}

byte __thiscall cAICharControl_FollowPath(cAICharControl *self, Nto1Node *target, float near, float far, float speed) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	self->nearDist = near;
	self->farDist = far;
	AICC_Vtbl(self)->SetSpeed(self, speed);
	if (self->mode == self->prevMode && self->targetEnt == target->target) {
		return 1;
	}
	self->b88 = 1;
	return AICC_FollowStart(self, target);
}

byte __thiscall cAICharControl_Follow(cAICharControl *self, Nto1Node *target, float near, float far, float speed) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	self->nearDist = near;
	self->farDist = far;
	AICC_Vtbl(self)->SetSpeed(self, speed);
	if (self->mode == self->prevMode && self->targetEnt == target->target) {
		return 1;
	}
	return AICC_FollowStart(self, target);
}

byte cAICharControl_SetMove(cAICharControl *self, int mode, ...) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	va_list ap;
	byte r = 1;

	va_start(ap, mode);
	self->prevMode = self->mode;
	self->mode = mode;
	self->walkDir = 0;
	self->side = 0;
	self->i9c = 0;
	self->i20c = 0;
	self->hold = 0;
	self->ic0 = 0;
	self->stuck = 0;
	if (self->link.target) {
		CHECK(self->link.data);
		Nto1Node_Unlink(&self->link);
	}
	self->direct = 0;
	self->b1d1 = 0;
	self->i1d4 = 0;
	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	self->lifeMark = WChar(self->ent)->life;
	switch (self->mode) {
	case AICC_STOP:
		vt->Stop2(self);
		break;
	case AICC_GO_TO: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		r = vt->GoTo(self, &p, (float)dist, (float)speed);
		break;
	}
	case AICC_GO_TO_DIRECT: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		r = vt->GoToDirect(self, &p, (float)dist, (float)speed);
		break;
	}
	case AICC_FOLLOW:
	case AICC_FOLLOW_PATH: {
		Nto1Node *t = AICC_OwnerVtbl(self->ai)->GetTarget(self->ai);
		double near = va_arg(ap, double);
		double far = va_arg(ap, double);
		double speed = va_arg(ap, double);
		self->flying = (byte)va_arg(ap, int);
		if (self->mode == AICC_FOLLOW) {
			r = vt->Follow(self, t, (float)near, (float)far, (float)speed);
		} else {
			r = vt->FollowPath(self, t, (float)near, (float)far, (float)speed);
		}
		break;
	}
	case AICC_DIR: {
		vec3_t p = va_arg(ap, vec3_t);
		vt->SetDir(self, &p);
		break;
	}
	case AICC_AWAY: {
		self->direct = 1;
		Nto1Node *t = AICC_OwnerVtbl(self->ai)->GetTarget(self->ai);
		vt->Away(self, t, (byte)va_arg(ap, int));
		break;
	}
	case AICC_ROUTE: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		int a = va_arg(ap, int);
		int b = va_arg(ap, int);
		r = vt->Route(self, &p, (float)dist, (float)speed, (byte)a, (byte)b);
		break;
	}
	}
	va_end(ap);
	return r;
}

void __thiscall cAICharControl_Away(cAICharControl *self, Nto1Node *target, byte flag) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	AICC_Vtbl(self)->SetSpeed(self, 0.0f);
	self->target = target;
	self->awayFlag = flag;
	float near = WChar(target->target)->prim.radius * 1.5f + WChar(self->ent)->prim.radius;
	self->speed = 1.0f;
	self->nearDist = near;
	self->farDist = near * 3.0f;
}

byte __thiscall cAICharControl_Route(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b) {
	self->arriveDist = dist;
	self->arriveR = Float_MinSS(self->radius, dist);
	AICC_Vtbl(self)->SetSpeed(self, speed);
	self->routeA = a;
	self->routeB = b;
	self->routePoint = *p;
	return AICC_Vtbl(self)->FindRoute(self);
}

void __thiscall cAICharControl_SetDir(cAICharControl *self, const vec3_t *dir) {
	vec3_t n, t, pos;

	self->dir = *dir;
	AICC_Vtbl(self)->SetSpeed(self, 0.0f);
	Vec3_NormalizeTo(&n, dir);
	t.x = n.x * 10000.0f;
	t.y = n.y * 10000.0f;
	t.z = n.z * 10000.0f;
	const vec3_t *p = Obj_Vtbl(self->ent)->base.GetPos(self->ent, &pos);
	self->goal.x = p->x + t.x;
	self->goal.y = p->y + t.y;
	self->goal.z = p->z + t.z;
}

void __thiscall cAICharControl_Nop(cAICharControl *self) {
	(void)self;
}

/* the arrive distance and the speed of GoTo and GoToDirect; 1 when the move to p runs already */
static int AICC_GoToSame(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	self->arriveDist = dist;
	self->arriveR = Float_MinSS(self->radius, dist);
	AICC_Vtbl(self)->SetSpeed(self, speed);
	return self->mode == self->prevMode && self->dest.x == p->x && self->dest.y == p->y && self->dest.z == p->z;
}

byte __thiscall cAICharControl_GoTo(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	if (AICC_GoToSame(self, p, dist, speed)) {
		return 1;
	}
	byte r = AICC_Vtbl(self)->FindPath(self, p);
	if (r && self->pathRes == 2) {
		AICC_OwnerVtbl(self->ai)->Event(self->ai, 1);
	}
	return r;
}

byte __thiscall cAICharControl_GoToDirect(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	if (AICC_GoToSame(self, p, dist, speed)) {
		return 1;
	}
	return AICC_Vtbl(self)->DirectPath(self, p);
}

void __thiscall cAICharControl_SetSpeed(cAICharControl *self, float speed) {
	self->speed = speed;
	AICC_Vtbl(self)->SetTurnSpeed(self, speed);
}

void __thiscall cAICharControl_SetTurnSpeed(cAICharControl *self, float speed) {
	if (0.55f >= speed) {
		self->turnGain = 5.0f;
		self->turnStep = 1.0f;
	} else {
		float t = speed - 0.5f;
		float a = t * 9.0f;
		float b = t * 2.5f;
		a = a + a;
		b = b + b;
		self->turnStep = a + 1.0f;
		self->turnGain = b + 5.0f;
	}
	self->f204 = 15.0f;
	self->f208 = 1.0f;
}

void __thiscall cAICharControl_Stop2(cAICharControl *self) {
	AICC_Vtbl(self)->Reset(self);
}

void __thiscall cAICharControl_Reset(cAICharControl *self) {
	AICC_Vtbl(self)->Stop(self);
	self->dest.x = -FLT_MAX;
	self->dest.y = -FLT_MAX;
	self->dest.z = -FLT_MAX;
	self->goal.x = -FLT_MAX;
	self->goal.y = -FLT_MAX;
	self->goal.z = -FLT_MAX;
}

byte __thiscall cAICharControl_IsBlocked(cAICharControl *self) {
	return self->blocked;
}

byte __thiscall cAICharControl_GetE4(cAICharControl *self) {
	return self->be4;
}

void __thiscall cAICharControl_SetHold(cAICharControl *self, byte on) {
	self->hold = on;
	if (on) {
		AICC_Vtbl(self)->Stop(self);
	}
}
