/*
 * AI/AIGear.cpp of the game: the AI of the gear (cAIGear, a cAIChar with cAIControlGear). It shoots at its target
 * like the bishop (AIBishop.c) when the target is in the cone of its view and the way is free, and presses the
 * action button when it is blocked by a paused AI it ran into. See include/scrap/ai.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"

#define CAIGEAR_SIZE 0x390
#define GEAR_BLOCK_TIME 7.5f		/* the way to the target is tested again after it (rva 0x3a46d0) */

void *__thiscall cAIGear_ctor(void *mem, cEntity *e, int *arg) {
	cAIGear *self = mem;
	cAIChar *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	c->vftable = Game_Ptr(CAIGEAR_VFTABLE);
	cAIChar_NewControl(c, CAICONTROLSHOOT_SIZE, cAIControlGear_ctor, e, arg);
	self->targetLife = 0;
	self->blocked = 0;
	c->canTalk = 0;
	return self;
}

void __fastcall cAIGear_dtor(cAIChar *self) {
	self->vftable = Game_Ptr(CAIGEAR_VFTABLE);
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIGear_Delete(cAIChar *self, byte flags) {
	self->vftable = Game_Ptr(CAIGEAR_VFTABLE);
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIGEAR_SIZE);
	}
	return self;
}

void __thiscall cAIGear_UpdateAction(cAIGear *self) {
	cAIChar *c = &self->base;

	if (c->vftable->CanRepos(c, c->state) || !AICC_Vtbl(c->control)->IsBlocked(c->control)) {
		return;
	}
	const char *name = Nto1Node_TargetName(&WChar(c->ent)->collide);
	if (!strcmp(name, "") || !strcmp(name, "GateKeeper") || !strcmp(name, "Sebastian")) {
		return;
	}
	cEntity *o = World_FindEntity(name);
	if (o && o->vftable->GetAI(o)) {
		cAIChar *ai = o->vftable->GetAI(o);
		if (ai->vftable->IsPaused(ai)) {
			c->ctrl->act = 1;
		}
	}
}

void __thiscall cAIGear_CheckActDist(cAIGear *self) {
	cAIChar *c = &self->base;
	float f = c->standTime;
	vec3_t a, b, d;

	self->blocked = 0;
	if (1.0f > f) {
		self->targetLife = WChar(c->target.target)->life;
	} else if (f > GEAR_BLOCK_TIME && self->targetLife <= WChar(c->target.target)->life) {
		/* hit since the last check: is the way to the target free */
		cEntity *tgt = c->target.target;
		cAICharControl *ctl = c->control;
		cEntity *own = c->ent;
		float r = Obj_Vtbl(own)->GetRadius(own);
		const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
		const vec3_t *op = own->vftable->GetPos(own, &a);
		if (!AICC_Vtbl(ctl)->TestPath(ctl, op, tp, r)) {
			self->blocked = 1;
			c->ctrl->act = 0;
			return;
		}
		self->blocked = 0;
		c->standTime = 0.0f;
	}
	cEntity *tgt = c->target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	d.y = tp->y - op->y;
	d.x = tp->x - op->x;
	d.z = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(d.x * d.x + 0.0f + d.z * d.z));
	if (!c->vftable->GetDirect2(c) || !(dist > c->actMin) || !(c->actMax > dist)) {
		c->standTime = 0.0f;
		c->ctrl->act = 0;
		return;
	}
	if (AIChar_Action0(c->ent)->priority > 0x65) {
		c->ctrl->act = 0;
		return;
	}
	/* the half angle of the target at its distance, the angle of the view (down z) to it */
	cEntity *t = c->target.target;
	float tr = Obj_Vtbl(t)->GetRadius(t);
	double half = Game_Atan2((double)tr, (double)dist);
	vec3_t fwd = { 0.0f, 0.0f, -1.0f };
	vec3_t view = WChar(c->ent)->view;
	vec3_t dir;
	Vec3_Rotate(&dir, &fwd, &view);
	double ang = Vec3_Angle(&dir, &d);
	if (!((double)self->shootAng + half > ang) || (self->shootFactor != 0 && crt_rand() % self->shootFactor != 0)) {
		c->ctrl->act = 0;
		return;
	}
	c->ctrl->act = 1;
}

void __thiscall cAIGear_SetDists(cAIGear *self) {
	cAIChar *c = &self->base;

	if (c->state == 2) {
		c->chaseFar = 1250.0f;
		c->actMin = 0.0f;
		c->actMax = 10000.0f;
		c->chaseNear = 400.0f;
		return;
	}
	if (c->state != 3) {
		cAIChar_SetDists(c);
		return;
	}
	c->chaseFar = 1000.0f;
	c->chaseNear = 400.0f;
}

void __thiscall cAIGear_SetShoot(cAIGear *self) {
	cEntity *t = self->base.target.target;

	if (t->vftable->GetAI(t)) {
		self->shootFactor = g_ShootGearAIDef;
		self->shootAng = g_OffsetAngShootGearAIDef;
	} else {
		self->shootFactor = g_ShootGearAI;
		self->shootAng = g_OffsetAngShootGearAI;
	}
	self->base.vftable->SetDists(&self->base);
}

byte __fastcall cAIGear_IsBlocked(cAIGear *self) {
	return self->blocked;
}
