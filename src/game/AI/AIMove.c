/*
 * AI/AIMove.cpp of the game: cAIMove, the base of the character and the vehicle AIs (a cAI). It holds the link to the
 * target entity, the move of the state (the first 0x90 bytes of cAIChar, include/scrap/ai.h, the same in
 * cAIVehicle) and stops an AI that should move but stands still for 10 seconds (the event 7). The prototypes are in
 * include/scrap/aivehicle.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aivehicle.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/nodesworld2d.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/walkcharentity.h"

#define g_AIMoveRandScale GAME_GLOBAL(double, g_FxRandScale, 0x3a3d60)	/* 1/32767 */
#define AIMOVE_STOP_TIME 10.0f				/* rva 0x3a5d08 */
#define AIMOVE_EV_STOPPED 7

static inline cAIChar *AIMove(void *self) {
	return self;
}

static void AIMove_InitTarget(cAIChar *self) {
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
}

void *__thiscall cAIMove_ctor(void *mem, cEntity *e) {
	cAIChar *self = mem;
	byte fly;

	cAI_ctor(mem, e);
	self->vftable = CAIMOVE_VFTABLE;
	AIMove_InitTarget(self);
	self->target.data = self;
	self->flying = 0;
	cEntity *o = self->owner;
	if (o->vftable->IsType(o, "WalkChar")) {
		fly = ((cActCtrl *)WithLife_Vtbl(o)->GetActCtrl(o))->cls->canFly;
	} else {
		fly = o->vftable->IsType(o, "Car");
	}
	if (fly) {
		self->flying = 1;
	}
	self->pathB = 0;
	self->stopped = 0;
	self->paused = 0;
	self->arrived = 0;
	self->repos = 0;
	self->reposing = 0;
	self->unused_88 = 0;
	self->dest.y = 0.0f;
	self->dest.z = 0.0f;
	self->dest.x = 0.0f;
	self->center.x = 0.0f;
	self->center.y = self->dest.y;
	self->center.z = self->dest.z;
	self->dist = 0.0f;
	self->radius = 0.0f;
	self->moveMode = 0;
	self->speed = 0.0f;
	self->dir.x = -1.0f;
	self->dir.y = -1.0f;
	self->dir.z = -1.0f;
	self->pathA = 0;
	self->stopped = 0;
	self->inPath = 1;
	self->stopTime = 0.0f;
	return self;
}

void *__fastcall cAIMove_ctorDefault(void *mem) {
	cAIChar *self = mem;

	cAI_ctorDefault(mem);
	self->vftable = CAIMOVE_VFTABLE;
	AIMove_InitTarget(self);
	return self;
}

void __fastcall cAIMove_dtor(void *mem) {
	cAIChar *self = mem;

	self->vftable = CAIMOVE_VFTABLE;
	Nto1Node_Done(&self->target);
	cAI_dtor(mem);
}

void *__thiscall cAIMove_Delete(void *self, byte flags) {
	cAIMove_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIMOVE_SIZE);
	}
	return self;
}

void __thiscall cAIMove_CheckStopped(void *mem, float dt) {
	cAIChar *self = mem;
	vec3_t buf;

	if (self->vftable->IsMoving(self)) {
		const vec3_t *v = Obj_Vtbl(self->owner)->GetVel(self->owner, &buf);
		if (v->x * v->x + v->y * v->y + v->z * v->z == 0.0f) {
			float t = dt + self->stopTime;
			self->stopTime = t;
			if (t > AIMOVE_STOP_TIME) {
				self->stopTime = 0.0f;
				self->stopped = 1;
				self->vftable->Event(self, AIMOVE_EV_STOPPED);
				return;
			}
			self->stopped = 0;
			return;
		}
	}
	self->stopTime = 0.0f;
}

void __thiscall cAIMove_Draw(void *self) {
	cAI_Draw(self);
}

void __thiscall cAIMove_Update(void *self, float dt) {
	cAI_Update(self, dt);
	AIMove(self)->vftable->CheckStopped(self, dt);
}

void __thiscall cAIMove_UpdateOff(void *self, float dt) {
	cAI_UpdateOff(self, dt);
}

byte __thiscall cAIMove_VarF(void *self, const char *name, double *d, byte set) {
	if (!*name) {
		return 0;
	}
	return cAI_VarF(self, name, d, set);
}

byte __thiscall cAIMove_VarI(void *self, const char *name, int *i, byte set) {
	if (!*name) {
		return 0;
	}
	return cAI_VarI(self, name, i, set);
}

byte __thiscall cAIMove_VarS(void *self, const char *name, char **s, byte set) {
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Target")) {
		if (set) {
			return 0;
		}
		cEntity *t = AIMove(self)->target.target;
		*s = t ? t->name : ACT_EMPTY;
		return 1;
	}
	return cAI_VarS(self, name, s, set);
}

byte __thiscall cAIMove_VarV(void *self, const char *name, double *x, double *y, double *z, byte set) {
	if (!*name) {
		return 0;
	}
	return cAI_VarV(self, name, x, y, z, set);
}

byte __thiscall cAIMove_CanJump(void *self) {
	return AIMove(self)->flying;
}

cEntity *__thiscall cAIMove_GetTargetEnt(void *self) {
	return AIMove(self)->target.target;
}

Nto1Node *__thiscall cAIMove_GetTarget(void *self) {
	return &AIMove(self)->target;
}

byte __thiscall cAIMove_GetStopped(void *self) {
	return AIMove(self)->stopped;
}

byte __thiscall cAIMove_GetArrived(void *self) {
	return AIMove(self)->arrived;
}

byte __thiscall cAIMove_IsPaused(void *self) {
	return AIMove(self)->paused;
}

byte __thiscall cAIMove_Event(void *self, int ev) {
	(void)self;
	(void)ev;
	return 1;
}

byte __thiscall cAIMove_CanTarget(void *self, cEntity *e) {
	(void)self;
	if (!e) {
		Con_Warnf("Objetivo din\xe1mico = NULL\n");
		return 0;
	}
	if (!e->flag_4c) {
		Con_Warnf("Objetivo din\xe1mico: %s Inactivo\n", e->name);
		return 0;
	}
	return 1;
}

/* rand() of the exe in [lo, lo + span] */
static inline float AIMove_Rand(double span, double lo) {
	return (float)((double)(crt_rand() & 0x7fff) * g_AIMoveRandScale * span + lo);
}

vec3_t *__thiscall cAIMove_RandomPoint(void *mem, vec3_t *out, const vec3_t *p, float r) {
	cAIChar *self = mem;
	vec3_t q;

	*out = *p;
	for (int i = 0; i < 3; i++) {
		float nr = -r;
		double span = (double)(r - nr);
		double lo = (double)nr;
		byte ok;
		if (!self->flying) {
			float ox = AIMove_Rand(span, lo);
			float oy = AIMove_Rand(span, lo);
			q.x = p->x + ox;
			q.y = p->y + oy;
			q.z = p->z + 0.0f;
			cEntity *o = self->owner;
			cActCtrl *ac = WithLife_Vtbl(o)->GetActCtrl(o);
			o = self->owner;
			float h = WChar(o)->floorDist;
			float h2 = ac->cls->sRadius;
			float r1 = Obj_Vtbl(o)->GetRadius(o);
			float r2 = Obj_Vtbl(o)->GetRadius(o);
			ok = CNodesWorld2D_TestPath(g_NodesWorld2D, p, &q, r2, r1, h, h2, NULL);
		} else {
			float ox = AIMove_Rand(span, lo);
			float oy = AIMove_Rand(span, lo);
			float oz = AIMove_Rand(span, lo);
			q.x = p->x + ox;
			q.y = p->y + oy;
			q.z = p->z + oz;
			cEntity *o = self->owner;
			ok = !Engine_TestSphere(p, &q, Obj_Vtbl(o)->GetRadius(o), NULL, NULL);
		}
		if (ok) {
			*out = q;
			return out;
		}
	}
	return out;
}
