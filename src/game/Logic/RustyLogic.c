/*
 * Logic/RustyLogic.cpp of the game: Rusty, the scrap worker of the ship editor (cRustyLocalLogic, a
 * cGenCharLocalLogic, include/scrap/logic.h). It walks to a random point of the list JunkPointsListName (state 7,
 * never the same twice), moves onto it and works there (8, the action "Action") for NumWorkingLoops loops, counted
 * by the native at the end of each loop, then walks to the next point. State 9 is the pissed Rusty at the platform
 * "DM_Door_EditShip_00": he runs to his point when the player goes off the platform.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/weapons.h"

#define CRUSTYLOCALLOGIC_VFTABLE GAME_VFTABLE(cRustyLocalLogic_vftable, 0x3c0d58)
#define g_JunkPointsList GAME_DATA(cEntityListData *, 0x44d844)
#define g_JunkPointsListName GAME_DATA(char *, 0x44d85c)	/* cString "JunkPointsListName" */
#define g_RustyLoops GAME_DATA(int, 0x44d848)			/* the working loops done at this point */
#define g_NumWorkingLoops GAME_DATA(int, 0x44d89c)
#define g_PlatformRadiusSqr GAME_DATA(int, 0x44d874)
#define s_RustyLastJunkPoint GAME_DATA(int, 0x428bcc)		/* -1 at the start */

void *__thiscall cRustyLocalLogic_ctor(void *mem, cEntity *parent) {
	cRustyLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 1);
	self->base.base.vftable = CRUSTYLOCALLOGIC_VFTABLE;
	if (!g_JunkPointsList) {
		g_JunkPointsList = (cEntityListData *)cWorld_GetList(g_pWorld, g_JunkPointsListName);
		CHECK(g_JunkPointsList && "Lista de puntos de chatarra no inicializada");
	}
	return self;
}

void __fastcall cRustyLocalLogic_dtor(cRustyLocalLogic *self) {
	self->base.base.vftable = CRUSTYLOCALLOGIC_VFTABLE;
	g_JunkPointsList = NULL;
	cGenCharLocalLogic_dtor(&self->base);
}

cRustyLocalLogic *__thiscall cRustyLocalLogic_Delete(cRustyLocalLogic *self, byte flags) {
	self->base.base.vftable = CRUSTYLOCALLOGIC_VFTABLE;
	g_JunkPointsList = NULL;
	cGenCharLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the native at the end of a working loop of the character of that name: after the last one it relaxes */
void Rusty_LoopDone(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	g_RustyLoops++;
	if (g_RustyLoops == g_NumWorkingLoops) {
		cActCtrl_Play(ChType_ActCtrl(e), "Relax", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(e), "Relax", 1);
	}
}

/* vf03 and vf05 */
byte __thiscall cRustyLocalLogic_VarI(cRustyLocalLogic *self, const char *name, int *i, byte set) {
	return cBaseLocalLogic_VarI(&self->base.base, name, i, set);
}

byte __thiscall cRustyLocalLogic_VarS(cRustyLocalLogic *self, const char *name, char **s, byte set) {
	return cBaseLocalLogic_VarS(&self->base.base, name, s, set);
}

/* the character moves onto the junk point in t seconds and plays its work */
static void Rusty_StartWork(cEntity *p, cEntity *junk, float t) {
	vec3_t tmp;
	cObjMove *m = &((cObjEntity *)p)->move;

	m->moveAng = *Obj_Vtbl(junk)->GetAng(junk, &tmp);
	m->movePos = *p->vftable->GetPos(p, &tmp);
	cObjMove_SetTime(m, t);
	m->fade = OBJMOVE_SMOOTH;
	cActCtrl_Play(ChType_ActCtrl(p), "Action", 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Action", 1);
}

/* the AI walks to the junk point, faced like it (speed of the GenChar logic or 1) */
static void Rusty_GoToJunk(cRustyLocalLogic *self, cAIChar *ai, double speed) {
	vec3_t ang, fwd, dir;
	cEntity *junk = self->junk;

	Obj_Vtbl(junk)->GetAng(junk, &ang);
	fwd.x = 0.0f;
	fwd.y = 0.0f;
	fwd.z = -1.0f;
	Vec3_Rotate(&dir, &fwd, &ang);
	const vec3_t *pos = self->junk->vftable->GetPos(self->junk, &fwd);
	ai->vftable->SetState(ai, 4, *pos, dir, 100.0, speed);
}

/* vf17 */
void __thiscall cRustyLocalLogic_Update(cRustyLocalLogic *self, float dt) {
	cBaseLocalLogic *b = &self->base.base;
	int st = b->state;
	cEntity *p = b->parent;
	vec3_t t1, t2;

	if (st == 0 || st == 6) {
		cRustyLocalLogic_NextJunk(self);
		b->vftable->SetState(self, 7, NULL);
	}
	if (st == 7) {
		cAIChar *ai = p->vftable->GetAI(p);
		if (ai->vftable->GetState(ai) || !Logic_CharAIVtbl(ai)->Stopped(ai)) {
			return;
		}
		if (strcmp(p->vftable->GetClassName(p), TYEN_WALKCHAR)) {
			return;
		}
		vec3_t jp;
		/* the original uses the buffer, not the returned pointer */
		self->junk->vftable->GetPos(self->junk, &jp);
		jp.y = p->vftable->GetPos(p, &t2)->y;
		if (cWorld_TestSphereEntities(g_pWorld, p, (void *)0x10, &jp, WChar_Radius(p), NULL, 1, 0)) {
			return;
		}
		Rusty_StartWork(p, self->junk, 0.25f);
		b->vftable->SetState(self, 8, NULL);
		return;
	}
	if (st == 8) {
		if (b->idle || b->alwaysLock) {
			if (!((cObjEntity *)p)->move.active) {
				return;
			}
			cObjMove_SetTime(&((cObjEntity *)p)->move, 0.0f);
			g_RustyLoops = 0;
			return;
		}
		if (strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Action")) {
			Rusty_StartWork(p, self->junk, 1.0f);
		}
		if (g_RustyLoops != g_NumWorkingLoops) {
			return;
		}
		g_RustyLoops = 0;
		cObjMove_SetTime(&((cObjEntity *)p)->move, 0.0f);
		cActCtrl_Play(ChType_ActCtrl(p), "Walk", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Walk", 1);
		cRustyLocalLogic_NextJunk(self);
		b->vftable->SetState(self, 7, NULL);
		return;
	}
	if (st != 9) {
		cGenCharLocalLogic_Update(&self->base, dt);
		return;
	}
	p->vftable->GetAI(p);
	if (strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Run")) {
		Rusty_GoToJunk(self, b->parent->vftable->GetAI(b->parent), 1.0);
		return;
	}
	cEntity *plat = World_FindEntity("DM_Door_EditShip_00");
	CHECK(plat && "La plataforma no existe");
	const vec3_t *pp = plat->vftable->GetPos(plat, &t1);
	const vec3_t *mp = p->vftable->GetPos(p, &t2);
	float dx = mp->x - pp->x;
	float dy = mp->y - pp->y;
	float dz = mp->z - pp->z;
	if (dy * dy + dx * dx + dz * dz > (float)(g_PlatformRadiusSqr + 90000)) {
		b->vftable->SetState(self, 7, NULL);
	}
}

/* a random junk point other than the last one */
void __fastcall cRustyLocalLogic_NextJunk(cRustyLocalLogic *self) {
	int i;

	do {
		int n = Logic_Count(g_JunkPointsList->numElems);
		i = (int)(float)(Weap_Rand01() * (double)((float)n - 0.05f) + 0.0);
	} while (i == s_RustyLastJunkPoint);
	self->junk = NULL;
	s_RustyLastJunkPoint = i;
	Nto1Node *n = Logic_ListFirst(g_JunkPointsList);
	for (int k = 0; n; k++) {
		self->junk = n->data;
		if (k == i) {
			break;
		}
		n = Logic_ListNext(n);
	}
	CHECK(self->junk && "currentJunkPoint && \"No se ha seleccionado ningún punto de chatarra\"");
}

/* vf19: 7 the walk to the junk point, 9 pissed; the others of the GenChar logic */
byte __thiscall cRustyLocalLogic_SetState(cRustyLocalLogic *self, int state, cEntity *target) {
	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&self->base.base, 7, target)) {
			cEntity *p = self->base.base.parent;
			Rusty_GoToJunk(self, p->vftable->GetAI(p), (double)g_GenCharRelaxSpeed);
		}
		return 1;
	}
	if (state != 9) {
		return cGenCharLocalLogic_SetState(&self->base, state, target);
	}
	cEntity *p = self->base.base.parent;
	cActCtrl_Play(ChType_ActCtrl(p), "Pissed", 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
	return 1;
}
