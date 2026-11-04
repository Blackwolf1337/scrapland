/*
 * AI/AIMove.cpp of the game: cAIMove, the base of the character and the vehicle AIs (a cAI). It holds the link to the
 * target entity and the move of the state, and stops an AI that should move but stands still for 10 seconds (the
 * event 7). See include/scrap/ai.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/nodesworld2d.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/walkcharentity.h"

#define CAIMOVE_VFTABLE (&cAIMove_vftable)
static const cAIMoveVtbl cAIMove_vftable;
#define AIMOVE_STOP_TIME 10.0f
#define AIMOVE_EV_STOPPED 7

static void AIMove_InitTarget(cAIMove *self) {
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
}

cAIMove *cAIMove_ctor(cAIMove *self, cEntity *e) {
	byte fly;

	cAI_ctor(&self->base, e);
	self->vftable = CAIMOVE_VFTABLE;
	AIMove_InitTarget(self);
	self->target.data = self;
	self->flying = 0;
	cEntity *o = self->base.entity;
	if (Entity_IsType(o, "WalkChar")) {
		fly = ((cActCtrl *)WithLife_GetActCtrl(o))->cls->canFly;
	} else {
		fly = Entity_IsType(o, "Car");
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
	self->trafficPlaced = 0;
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

void cAIMove_dtor(cAIMove *self) {
	Nto1Node_Done(&self->target);
	cAI_dtor(&self->base);
}

static void cAIMove_Free(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	cAIMove_dtor(self);
	operator_delete(self);
}

void cAIMove_CheckStopped(cAI *elem, float dt) {
	cAIMove *self = (cAIMove *)elem;
	vec3_t buf;

	if (AIMove_IsMoving(self)) {
		const vec3_t *v = Obj_GetVel(self->base.entity, &buf);
		if (v->x * v->x + v->y * v->y + v->z * v->z == 0.0f) {
			float t = dt + self->stopTime;
			self->stopTime = t;
			if (t > AIMOVE_STOP_TIME) {
				self->stopTime = 0.0f;
				self->stopped = 1;
				AIMove_Event(self, AIMOVE_EV_STOPPED);
				return;
			}
			self->stopped = 0;
			return;
		}
	}
	self->stopTime = 0.0f;
}

void cAIMove_Draw(cAI *elem) {
	cAIMove *self = (cAIMove *)elem;
	cAI_Draw(&self->base);
}

void cAIMove_Update(cAI *elem, float dt) {
	cAIMove *self = (cAIMove *)elem;
	cAI_Update(&self->base, dt);
	AIMove_CheckStopped(self, dt);
}

void cAIMove_UpdateOff(cAI *elem, float dt) {
	cAIMove *self = (cAIMove *)elem;
	cAI_UpdateOff(&self->base, dt);
}

byte cAIMove_VarF(cAI *elem, const char *name, double *d, byte set) {
	cAIMove *self = (cAIMove *)elem;
	if (!*name) {
		return 0;
	}
	return cAI_VarF(&self->base, name, d, set);
}

byte cAIMove_VarI(cAI *elem, const char *name, int *i, byte set) {
	cAIMove *self = (cAIMove *)elem;
	if (!*name) {
		return 0;
	}
	return cAI_VarI(&self->base, name, i, set);
}

byte cAIMove_VarS(cAI *elem, const char *name, char **s, byte set) {
	cAIMove *self = (cAIMove *)elem;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Target")) {
		if (set) {
			return 0;
		}
		cEntity *t = self->target.target;
		*s = t ? t->name : str_Empty;
		return 1;
	}
	return cAI_VarS(&self->base, name, s, set);
}

byte cAIMove_VarV(cAI *elem, const char *name, double *x, double *y, double *z, byte set) {
	cAIMove *self = (cAIMove *)elem;
	if (!*name) {
		return 0;
	}
	return cAI_VarV(&self->base, name, x, y, z, set);
}

byte cAIMove_CanJump(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return self->flying;
}

cEntity *cAIMove_GetTargetEnt(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return self->target.target;
}

Nto1Node *cAIMove_GetTarget(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return &self->target;
}

byte cAIMove_GetStopped(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return self->stopped;
}

byte cAIMove_GetArrived(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return self->arrived;
}

byte cAIMove_IsPaused(cAI *base) {
	cAIMove *self = (cAIMove *)base;
	return self->paused;
}

byte cAIMove_CanTarget(cAI *base, cEntity *e) {
	cAIMove *self = (cAIMove *)base;
	(void)self;
	if (!e) {
		Con_Warnf("Objetivo din\xe1mico = NULL\n");
		return 0;
	}
	if (!e->active) {
		Con_Warnf("Objetivo din\xe1mico: %s Inactivo\n", e->name);
		return 0;
	}
	return 1;
}

/* rand() of the exe in [lo, lo + span] */
static inline float AIMove_Rand(double span, double lo) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * span + lo);
}

vec3_t *cAIMove_RandomPoint(cAI *elem, vec3_t *out, const vec3_t *p, float r) {
	cAIMove *self = (cAIMove *)elem;
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
			cEntity *o = self->base.entity;
			cActCtrl *ac = WithLife_GetActCtrl(o);
			o = self->base.entity;
			float h = WChar(o)->floorDist;
			float h2 = ac->cls->sRadius;
			float r1 = Obj_GetRadius(o);
			float r2 = Obj_GetRadius(o);
			ok = CNodesWorld2D_TestPath(g_NodesWorld2D, p, &q, r2, r1, h, h2, NULL);
		} else {
			float ox = AIMove_Rand(span, lo);
			float oy = AIMove_Rand(span, lo);
			float oz = AIMove_Rand(span, lo);
			q.x = p->x + ox;
			q.y = p->y + oy;
			q.z = p->z + oz;
			cEntity *o = self->base.entity;
			ok = !Engine_TestSphere(p, &q, Obj_GetRadius(o), NULL, NULL);
		}
		if (ok) {
			*out = q;
			return out;
		}
	}
	return out;
}

/* the vtable of cAIMove */
static const cAIMoveVtbl cAIMove_vftable = {
	.base.Free = cAIMove_Free,
	.base.Update = cAIMove_Update,
	.base.UpdateOff = cAIMove_UpdateOff,
	.base.Draw = cAIMove_Draw,
	.base.GetChanged = cAI_GetChanged,
	.base.IsVisible = cAI_IsVisible,
	.base.Debugf = (void *)crt_purecall,
	.base.VarI = cAIMove_VarI,
	.base.VarF = cAIMove_VarF,
	.base.VarS = cAIMove_VarS,
	.base.VarV = cAIMove_VarV,
	.base.Print = cAI_Print,
	.base.GetEventName = cAI_GetEventName,
	.base.OnEvent = cAI_OnEvent,
	.GetTarget = cAIMove_GetTarget,
	.CanJump = cAIMove_CanJump,
	.GetStopped = cAIMove_GetStopped,
	.IsPaused = cAIMove_IsPaused,
	.GetTargetEnt = cAIMove_GetTargetEnt,
	.GetArrived = cAIMove_GetArrived,
	.RandomPoint = cAIMove_RandomPoint,
	.CheckStopped = cAIMove_CheckStopped,
	.Event = cAI_EventTrue,
	.IsMoving = (void *)crt_purecall,
	.CanTarget = cAIMove_CanTarget,
};
