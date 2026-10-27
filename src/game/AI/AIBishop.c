/*
 * AI/AIBishop.cpp of the game: the AI of the bishop (cAIBishop, a cAIChar) that shoots at its target when it is in
 * the cone of its view and the way is free, with the angle between two vectors (Vec3_Angle) the AIs share. See
 * include/scrap/ai.h.
 */
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"

#define CAIBISHOP_SIZE 0x398
#define CAICONTROLBISHOP_SIZE 0x384
#define BISHOP_BLOCK_TIME 7.5f		/* the way to the target is tested again after it (rva 0x3a46d0) */

void *__thiscall cAIBishop_ctor(void *mem, cEntity *e, int *arg) {
	cAIBishop *self = mem;
	cAICharView *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	*(const void **)self = Game_Ptr(CAIBISHOP_VFTABLE);
	if (c->control) {
		((void *(__thiscall *)(void *, byte))AI_VSlot(c->control, 0))(c->control, 1);
	}
	void *ctl = operator_new(CAICONTROLBISHOP_SIZE);
	c->control = ctl ? cAIControlBishop_ctor(ctl, e, self, arg) : NULL;
	CHECK(c->control);
	self->targetLife = 0;
	self->blocked = 0;
	return self;
}

void __fastcall cAIBishop_dtor(void *self) {
	*(const void **)self = Game_Ptr(CAIBISHOP_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
}

void *__thiscall cAIBishop_Delete(void *self, byte flags) {
	*(const void **)self = Game_Ptr(CAIBISHOP_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
	if (flags & 1) {
		operator_delete_sized(self, CAIBISHOP_SIZE);
	}
	return self;
}

/* v normalized into out (a copy for the zero vector) */
static void Vec3_NormCopy(vec3_t *out, const vec3_t *v) {
	float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));

	if (len == 0.0f) {
		*out = *v;
		return;
	}
	float inv = 1.0f / len;
	out->x = v->x * inv;
	out->y = v->y * inv;
	out->z = v->z * inv;
}

double Vec3_Angle(const vec3_t *a, const vec3_t *b) {
	vec3_t na, nb;

	Vec3_NormCopy(&na, a);
	Vec3_NormCopy(&nb, b);
	float dot = na.x * nb.x + na.y * nb.y + na.z * nb.z;
	return Game_Acos((double)Float_MaxSS(-1.0f, Float_MinSS(1.0f, dot)));
}

static int Layer0_Priority(cEntity *e) {
	return ((cActCtrl *)WithLife_Vtbl(e)->GetActCtrl(e))->layers[0].action->priority;
}

static int Ent_Life(cEntity *e) {
	return WChar(e)->life;
}

void __thiscall cAIBishop_CheckAttack(cAIBishop *self) {
	cAICharView *c = &self->base;
	float f = c->blockTime;
	vec3_t a, b, d;

	self->blocked = 0;
	if (1.0f > f) {
		self->targetLife = Ent_Life(c->target);
	} else if (f > BISHOP_BLOCK_TIME && self->targetLife <= Ent_Life(c->target)) {
		/* hit since the last check: is the way to the target free */
		cEntity *tgt = c->target;
		void *test = AI_VSlot(c->control, 0x38);
		cEntity *own = c->own;
		float r = Obj_Vtbl(own)->GetRadius(own);
		const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
		const vec3_t *op = own->vftable->GetPos(own, &a);
		if (!((byte(__thiscall *)(void *, const vec3_t *, const vec3_t *, float))test)(c->control, op, tp, r)) {
			self->blocked = 1;
			c->attack[0x18] = 0;
			return;
		}
		self->blocked = 0;
		c->blockTime = 0.0f;
	}
	cEntity *tgt = c->target;
	const vec3_t *op = c->own->vftable->GetPos(c->own, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	d.y = tp->y - op->y;
	d.x = tp->x - op->x;
	d.z = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(d.x * d.x + 0.0f + d.z * d.z));
	if (!AICharView_Vtbl(self)->CanShoot(self) || Layer0_Priority(c->own) > 0x65 || !(dist > c->attackMin) ||
		!(c->attackMax > dist)) {
		c->blockTime = 0.0f;
		c->attack[0x18] = 0;
		return;
	}
	/* the half angle of the target at its distance, the angle of the view (down z) to it */
	float tr = Obj_Vtbl(c->target)->GetRadius(c->target);
	double half = Game_Atan2((double)tr, (double)dist);
	vec3_t fwd = { 0.0f, 0.0f, -1.0f };
	vec3_t view = WChar(c->own)->view;
	vec3_t dir;
	Vec3_Rotate(&dir, &fwd, &view);
	double ang = Vec3_Angle(&dir, &d);
	if (!((double)self->shootAng + half > ang)) {
		c->attack[0x18] = 0;
		return;
	}
	if (c->attack[0x18]) {
		c->attack[0x18] = !(c->attackTime > self->shootTime);
		return;
	}
	if ((self->shootFactor == 0 || crt_rand() % self->shootFactor == 0) && !WChar(c->own)->falling &&
		Layer0_Priority(c->own) == 0) {
		c->attackTime = 0.0f;
		c->attack[0x18] = 1;
		return;
	}
	c->attack[0x18] = 0;
}

void __thiscall cAIBishop_SetRanges(cAIBishop *self) {
	cAICharView *c = &self->base;

	if (c->state != 2) {
		cAIChar_SetDists((cAIChar *)self);
		return;
	}
	c->attackMin = 0.0f;
	float mx = self->shootDist;
	c->rangeMin = 400.0f;
	c->rangeMax = 1250.0f;
	c->attackMax = mx;
}

void __thiscall cAIBishop_SetShoot(cAIBishop *self) {
	cEntity *t = self->base.target;

	if (t->vftable->GetAI(t)) {
		self->shootFactor = g_ShootBishopAIDef;
		self->shootAng = g_OffsetAngShootBishopAIDef;
		self->shootTime = g_TimeShootBishopAIDef;
		self->shootDist = g_DistMaxShootBishopAIDef;
	} else {
		self->shootFactor = g_ShootBishopAI;
		self->shootAng = g_OffsetAngShootBishopAI;
		self->shootTime = g_TimeShootBishopAI;
		self->shootDist = g_DistMaxShootBishopAI;
	}
	AICharView_Vtbl(self)->SetRanges(self);
}
