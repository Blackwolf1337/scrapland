/*
 * Logic/SputnikJunkyardLogic.cpp of the game: Sputnik in the junkyard of the ship editor (a cGenCharLocalLogic,
 * include/scrap/logic.h). It walks around (state 7) while the camera "EditCamera" exists; after
 * SputnikJunyardTimeToPissPlayer seconds the first Sputnik goes in front of the camera and laughs at the player
 * (state 8) until the native of the end of the laugh sends it back to state 7.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxmisc.h"
#include "scrap/logic.h"

#define CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE GAME_VFTABLE(cSputnikJunkyardLocalLogic_vftable, 0x3c169c)
#define g_SputnikJunyardTimeToPissPlayer GAME_GLOBAL_IN(float, GVar_SputnikJunyardTimeToPissPlayer, 16, 0x44db88)
#define g_SputnikPisser GAME_GLOBAL(cEntity *, g_SputnikPisser, 0x44db70)	/* the Sputnik that laughs at the player */
#define g_SputnikTime GAME_GLOBAL(float, g_SputnikTime, 0x44db74)		/* since the camera exists */
/* the angles of the camera entity */
#define Sputnik_CamAng(cam) ((const vec3_t *)&((cCamControl *)(cam))->cam.rot)

#define SPUTNIK_VT(self) ((self)->base.vftable)

static void Sputnik_Walk(cGenCharLocalLogic *self) {
	cEntity *p = self->base.parent;
	cAIChar *ai = p->vftable->GetAI(p);

	ai->vftable->SetState(ai, 1, (double)g_GenCharRelaxSpeed, 0);
}

cSputnikJunkyardLocalLogic *__thiscall cSputnikJunkyardLocalLogic_ctor(cSputnikJunkyardLocalLogic *self,
	cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 1);
	SPUTNIK_VT(self) = CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE;
	g_SputnikTime = 0.0f;
	if ((byte)cBaseLocalLogic_SetState(&self->base, 7, NULL)) {
		Sputnik_Walk(self);
	}
	return self;
}

void __fastcall cSputnikJunkyardLocalLogic_dtor(cSputnikJunkyardLocalLogic *self) {
	SPUTNIK_VT(self) = CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE;
	g_SputnikPisser = NULL;
	cGenCharLocalLogic_dtor(self);
}

cSputnikJunkyardLocalLogic *__thiscall cSputnikJunkyardLocalLogic_Delete(cSputnikJunkyardLocalLogic *self, byte flags) {
	SPUTNIK_VT(self) = CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE;
	g_SputnikPisser = NULL;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the native at the end of the laugh of the character of that name: back to the walk */
void Sputnik_LaughDone(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cLocalLogic *l = Logic_GetLocalLogic(World_FindEntity(name));
	g_SputnikPisser = NULL;
	g_SputnikTime = 0.0f;
	l->vftable->SetState(l, 7, NULL);
}

/* vf17 */
void __thiscall cSputnikJunkyardLocalLogic_Update(cSputnikJunkyardLocalLogic *self, float dt) {
	cEntity *p = self->base.parent;

	p->vftable->GetAI(p);
	if (self->base.state == 7) {
		if (!World_FindEntity("EditCamera")) {
			g_SputnikTime = 0.0f;
			return;
		}
		if (g_SputnikPisser) {
			return;
		}
		g_SputnikTime = g_SputnikTime + dt;
		if (g_SputnikTime >= g_SputnikJunyardTimeToPissPlayer) {
			g_SputnikPisser = p;
			SPUTNIK_VT(self)->SetState(self, 8, NULL);
		}
		return;
	}
	if (self->base.state != 8) {
		cGenCharLocalLogic_Update(self, dt);
		return;
	}
	cAIChar *ai = p->vftable->GetAI(p);
	void *ctrl = ChType_ActCtrl(p);
	int st = ai->vftable->GetState(ai);
	if (st) {
		return;
	}
	if (strcmp(cActCtrl_GetActionName(ctrl, st), "Laugh")) {
		cActCtrl_Play(ctrl, "Laugh", 1, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Walk", 1);
	}
}

/* vf19: 7 the walk, 8 to the point 200 in front of the camera, facing it; always 1 */
byte __thiscall cSputnikJunkyardLocalLogic_SetState(cSputnikJunkyardLocalLogic *self, int state, cEntity *target) {
	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&self->base, 7, target)) {
			Sputnik_Walk(self);
		}
		return 1;
	}
	if (state != 8) {
		return cGenCharLocalLogic_SetState(self, state, target);
	}
	if (!(byte)cBaseLocalLogic_SetState(&self->base, 8, target)) {
		return 1;
	}
	cEntity *p = self->base.parent;
	cAIChar *ai = p->vftable->GetAI(p);
	cEntity *cam = World_FindEntity("EditCamera");
	if (!cam) {
		return 1;
	}
	vec3_t fwd = { 0.0f, 0.0f, 1.0f };
	vec3_t cp = g_pWorld->camPos[g_pWorld->curCam];
	vec3_t r, d;
	Vec3_NormalizeTo(&d, Vec3_Rotate(&r, &fwd, Sputnik_CamAng(cam)));
	vec3_t pos, dir;
	pos.x = cp.x + d.x * 200.0f;
	pos.y = cp.y + d.y * 200.0f;
	pos.z = cp.z + d.z * 200.0f;
	dir.x = -d.x;
	dir.y = -d.y;
	dir.z = -d.z;
	ai->vftable->SetState(ai, 4, pos, dir, 100.0, 0.25);
	return 1;
}
