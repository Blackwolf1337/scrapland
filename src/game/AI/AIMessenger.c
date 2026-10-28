/*
 * AI/AIMessenger.cpp of the game: the AI of the messenger (cAIMessenger, a cAIChar with cAIControlMessenger). It
 * shoots at a target in the cone of its view while its action lets it walk, holding the button for TimeShoot. See
 * include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"

#define CAIMESSENGER_SIZE 0x390

void *__thiscall cAIMessenger_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIMESSENGER_VFTABLE;
	cAIChar_NewControl(self, CAICHARCONTROL_SIZE, cAIControlMessenger_ctor, e, arg);
	self->canTalk = 0;
	return self;
}

void __fastcall cAIMessenger_dtor(cAIChar *self) {
	self->vftable = CAIMESSENGER_VFTABLE;
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIMessenger_Delete(cAIChar *self, byte flags) {
	self->vftable = CAIMESSENGER_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIMESSENGER_SIZE);
	}
	return self;
}

void __thiscall cAIMessenger_CheckActDist(cAIMessenger *self) {
	cAIChar *c = &self->base;
	byte act = 0;
	vec3_t a, b, d;

	cEntity *tgt = c->target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	d.x = tp->x - op->x;
	d.y = tp->y - op->y;
	d.z = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(d.x * d.x + 0.0f + d.z * d.z));
	if (!AIChar_Action0(c->ent)->walk || !c->vftable->GetDirect2(c) || AIChar_Action0(c->ent)->priority > 0x65 ||
		!(dist > c->actMin) || !(c->actMax > dist)) {
		goto done;
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
	if (!((double)self->shootAng + half > ang)) {
		goto done;
	}
	if (c->ctrl->act) {
		c->ctrl->act = !(c->actTime > self->shootTime);
		return;
	}
	if ((self->shootFactor == 0 || crt_rand() % self->shootFactor == 0) && !WChar(c->ent)->falling &&
		AIChar_Action0(c->ent)->priority == 0) {
		c->actTime = 0.0f;
		act = 1;
	}
done:
	c->ctrl->act = act;
}

void __thiscall cAIMessenger_SetDists(cAIMessenger *self) {
	cAIChar *c = &self->base;

	if (c->state == 2) {
		c->actMin = 0.0f;
		c->actMax = self->shootDist;
	} else if (c->state != 3) {
		cAIChar_SetDists(c);
		return;
	}
	c->chaseFar = 400.0f;
	c->chaseNear = 129.0f;
}

void __thiscall cAIMessenger_SetShoot(cAIMessenger *self) {
	self->shootFactor = g_ShootMessengerAI;
	self->shootAng = g_OffsetAngShootMessengerAI;
	self->shootTime = g_TimeShootMessengerAI;
	self->shootDist = g_DistMaxShootMessengerAI;
	self->base.vftable->SetDists(&self->base);
}
