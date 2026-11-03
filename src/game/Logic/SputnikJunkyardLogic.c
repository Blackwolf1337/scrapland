/*
 * Logic/SputnikJunkyardLogic.cpp of the game: Sputnik in the junkyard of the ship editor (a cGenCharLocalLogic,
 * include/scrap/logic.h). It walks around (state 7) while the camera "EditCamera" exists; after
 * SputnikJunyardTimeToPissPlayer seconds the first Sputnik goes in front of the camera and laughs at the player
 * (state 8) until the native of the end of the laugh sends it back to state 7.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/fxmisc.h"
#include "scrap/logic.h"
#include "scrap/staticinit.h"

static const cGenCharLocalLogicVtbl cSputnikJunkyardLocalLogic_vftable;
#define CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE (&cSputnikJunkyardLocalLogic_vftable.base)
static cGVar GVar_SputnikJunyardTimeToPissPlayer;
#define g_SputnikJunyardTimeToPissPlayer (GVar_SputnikJunyardTimeToPissPlayer.value.f)
/* the angles of the camera entity */
#define Sputnik_CamAng(cam) ((const vec3_t *)&((cCamControl *)(cam))->cam.rot)

static void Sputnik_Walk(cGenCharLocalLogic *self) {
	cEntity *p = self->base.parent;
	cAIChar *ai = Entity_GetAI(p);

	AIChar_SetState(ai, 1, (double)g_GenCharRelaxSpeed, 0);
}

static float g_SputnikTime;

cSputnikJunkyardLocalLogic *cSputnikJunkyardLocalLogic_ctor(cSputnikJunkyardLocalLogic *self,
	cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 1);
	self->base.vftable = CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE;
	g_SputnikTime = 0.0f;
	if ((byte)cBaseLocalLogic_SetState(&self->base.logic, 7, NULL)) {
		Sputnik_Walk(self);
	}
	return self;
}

static cEntity *g_SputnikPisser;

/* SputnikJunkyardLogic.cpp: a cGenCharLocalLogic, states 7 the walk, 8 the laugh in front of the camera */
static cLocalLogic *cSputnikJunkyardLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cSputnikJunkyardLocalLogic *self = (cSputnikJunkyardLocalLogic *)base;
	self->base.vftable = CSPUTNIKJUNKYARDLOCALLOGIC_VFTABLE;
	g_SputnikPisser = NULL;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the native at the end of the laugh of the character of that name: back to the walk */
static void Sputnik_LaughDone(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cLocalLogic *l = Logic_GetLocalLogic(World_FindEntity(name));
	g_SputnikPisser = NULL;
	g_SputnikTime = 0.0f;
	Logic_SetState(l, 7, NULL);
}

/* vf17 */
static void cSputnikJunkyardLocalLogic_Update(cLocalLogic *elem, float dt) {
	cSputnikJunkyardLocalLogic *self = (cSputnikJunkyardLocalLogic *)elem;
	cEntity *p = self->base.parent;

	Entity_GetAI(p);
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
			Logic_SetState(self, 8, NULL);
		}
		return;
	}
	if (self->base.state != 8) {
		cGenCharLocalLogic_Update(&self->base.logic, dt);
		return;
	}
	cAIChar *ai = Entity_GetAI(p);
	void *ctrl = ChType_ActCtrl(p);
	int st = AIChar_GetState(ai);
	if (st) {
		return;
	}
	if (strcmp(cActCtrl_GetActionName(ctrl, st), "Laugh")) {
		cActCtrl_Play(ctrl, "Laugh", 1, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Walk", 1);
	}
}

/* vf19: 7 the walk, 8 to the point 200 in front of the camera, facing it; always 1 */
static byte cSputnikJunkyardLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cSputnikJunkyardLocalLogic *self = (cSputnikJunkyardLocalLogic *)elem;
	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&self->base.logic, 7, target)) {
			Sputnik_Walk(self);
		}
		return 1;
	}
	if (state != 8) {
		return cGenCharLocalLogic_SetState(&self->base.logic, state, target);
	}
	if (!(byte)cBaseLocalLogic_SetState(&self->base.logic, 8, target)) {
		return 1;
	}
	cEntity *p = self->base.parent;
	cAIChar *ai = Entity_GetAI(p);
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
	AIChar_SetState(ai, 4, pos, dir, 100.0, 0.25);
	return 1;
}

/* the vtable of cSputnikJunkyardLocalLogic */
static const cGenCharLocalLogicVtbl cSputnikJunkyardLocalLogic_vftable = {
	.base.base.Delete = cSputnikJunkyardLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cSputnikJunkyardLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cSputnikJunkyardLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cGenCharLocalLogic_ToRelax,
	.ToAlert = cGenCharLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cLocalLogic_FindTargetNull,
	.TargetDone = cLocalLogic_TargetDoneTrue,
	.Work = cLocalLogic_WorkNop,
	.Vf33 = cLocalLogic_Vf33Nop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};

static cPy_NativeFunc NativeFunc_EndOfLaugh;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_SputnikJunyardTimeToPissPlayer, &GVar_SputnikJunyardTimeToPissPlayer, "SputnikJunyardTimeToPissPlayer", 0, "SputnikJunyard time to piss player", 60.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_EndOfLaugh, &NativeFunc_EndOfLaugh, "EndOfLaugh", Sputnik_LaughDone)
