/*
 * Logic/MayorLogic.cpp of the game: cMayorLocalLogic (include/scrap/logic.h), the mayor (a GenChar logic). Its
 * special attack goes for the nearest dazed character in its view within MayorBlabliblaRadius; the character that
 * talks to it (the listener) makes its AI listen (state 9) during the special action.
 */
#include <string.h>

#include "scrap/chartype.h"

#define MAYOR_VT(self) ((self)->base.base.vftable)
#define MAYOR_SPHEREFUNC 0x152e60

void *__thiscall cMayorLocalLogic_ctor(void *mem, cEntity *parent) {
	cMayorLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 0);
	MAYOR_VT(self) = CMAYORLOCALLOGIC_VFTABLE;
	self->listener[0] = 0;
	return self;
}

cMayorLocalLogic *__thiscall cMayorLocalLogic_Delete(cMayorLocalLogic *self, byte flags) {
	MAYOR_VT(self) = CMAYORLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf31: the special action is over after GenCharSpecialActionTime */
byte __thiscall cMayorLocalLogic_TargetDone(cMayorLocalLogic *self) {
	return self->base.specialTime > g_GenCharSpecialActionTime;
}

/* vf30: at the search time the nearest dazed character in view in the sphere of MayorBlabliblaRadius */
cEntity *__thiscall cMayorLocalLogic_FindTarget(cMayorLocalLogic *self) {
	vec3_t t;

	if (!(0.0f > self->base.searchTime)) {
		return NULL;
	}
	g_MayorTargetDist = 3.4028235e38f;
	g_MayorTarget = NULL;
	g_MayorSearchEnt = self->base.base.parent;
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)Game_Ptr(MAYOR_SPHEREFUNC));
	float radius = g_MayorBlabliblaRadius;
	cEntity *m = g_MayorSearchEnt;
	cWorld_SphereCallF(g_pWorld, 0x10, m->vftable->GetPos(m, &t), radius, g_ChTypeSphereFunc, 1);
	return g_MayorTarget;
}

/* vf28 and vf27: the AI walks in alert or relaxed */
void __thiscall cMayorLocalLogic_ToAlert(cMayorLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharAlertSpeed, 0);
}

void __thiscall cMayorLocalLogic_ToRelax(cMayorLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharRelaxSpeed, 0);
}

/* vf17: during the special action the AI listens to the listener once */
void __thiscall cMayorLocalLogic_Update(cMayorLocalLogic *self, float dt) {
	if (self->base.specialOn) {
		self->base.specialTime = dt + self->base.specialTime;
		if (self->listener[0]) {
			cEntity *p = self->base.base.parent;
			cAIChar *ai = p->vftable->GetAI(p);
			cEntity *e = World_FindEntity(self->listener);
			if (e) {
				ai->vftable->SetState(ai, 9, e, 0);
			}
			self->listener[0] = 0;
		}
	}
	cGenCharLocalLogic_Update(&self->base, dt);
}

/* the C function of the sphere: a dazed character (not the player, not the mayor) in the view of the mayor, the
 * nearest is kept */
void Mayor_SphereFunc(cAbstractFile *params) {
	char name[0x80];
	vec3_t pos, t;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr || usr == e || g_MayorSearchEnt == e) {
		return;
	}
	e->vftable->GetPos(e, &pos);
	if (!ChType_Vtbl(e)->FindAction(e, "Dazed") ||
		!Logic_InView(g_MayorSearchEnt, &pos, g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		return;
	}
	cEntity *m = g_MayorSearchEnt;
	const vec3_t *mp = m->vftable->GetPos(m, &t);
	float dy = pos.y - mp->y, dx = pos.x - mp->x, dz = pos.z - mp->z;
	float d = dy * dy + dx * dx + dz * dz;
	if (g_MayorTargetDist > d) {
		g_MayorTargetDist = d;
		g_MayorTarget = e;
	}
}

void __thiscall cMayorLocalLogic_SetListener(cLocalLogic *l, cEntity *e) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)l;

	if (e) {
		Str_Copy(self->listener, e->name, sizeof(self->listener));
		return;
	}
	self->listener[0] = 0;
}
