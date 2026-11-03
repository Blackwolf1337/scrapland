/*
 * Logic/RustyLogic.cpp of the game: Rusty, the scrap worker of the ship editor (cRustyLocalLogic, a
 * cGenCharLocalLogic, include/scrap/logic.h). It walks to a random point of the list JunkPointsListName (state 7,
 * never the same twice), moves onto it and works there (8, the action "Action") for NumWorkingLoops loops, counted
 * by the native at the end of each loop, then walks to the next point. State 9 is the pissed Rusty at the platform
 * "DM_Door_EditShip_00": he runs to his point when the player goes off the platform.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

/* the states Rusty adds to the GENCHAR_ ones */
enum {
	RUSTY_GO_TO_JUNK = 7, RUSTY_WORK = 8, RUSTY_PISSED = 9
};

static const cGenCharLocalLogicVtbl cRustyLocalLogic_vftable;
#define CRUSTYLOCALLOGIC_VFTABLE (&cRustyLocalLogic_vftable.base)
static cGVar GVar_JunkPointsListName;
#define g_JunkPointsListName (GVar_JunkPointsListName.value.s)	/* cString "JunkPointsListName" */
static cGVar GVar_NumWorkingLoops;
#define g_NumWorkingLoops (GVar_NumWorkingLoops.value.i)
static cGVar GVar_PlatformRadiusSqr;
#define g_PlatformRadiusSqr (GVar_PlatformRadiusSqr.value.i)

static cEntityListData *g_JunkPointsList;

void *cRustyLocalLogic_ctor(void *mem, cEntity *parent) {
	cRustyLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 1);
	self->base.base.vftable = CRUSTYLOCALLOGIC_VFTABLE;
	if (!g_JunkPointsList) {
		g_JunkPointsList = (cEntityListData *)cWorld_GetList(g_pWorld, g_JunkPointsListName);
		CHECK(g_JunkPointsList && "Lista de puntos de chatarra no inicializada");
	}
	return self;
}

static cLocalLogic *cRustyLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cRustyLocalLogic *self = (cRustyLocalLogic *)elem;
	self->base.base.vftable = CRUSTYLOCALLOGIC_VFTABLE;
	g_JunkPointsList = NULL;
	cGenCharLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

static int g_RustyLoops;

/* the native at the end of a working loop of the character of that name: after the last one it relaxes */
static void Rusty_LoopDone(cAbstractFile *params) {
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
static byte cRustyLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
	cRustyLocalLogic *self = (cRustyLocalLogic *)elem;
	return cBaseLocalLogic_VarI(&self->base.base.logic, name, i, set);
}

static byte cRustyLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set) {
	cRustyLocalLogic *self = (cRustyLocalLogic *)elem;
	return cBaseLocalLogic_VarS(&self->base.base.logic, name, s, set);
}

/* the character moves onto the junk point in t seconds and plays its work */
static void Rusty_StartWork(cEntity *p, cEntity *junk, float t) {
	vec3_t tmp;
	cObjMove *m = &Obj(p)->move;

	m->moveAng = *Obj_GetAng(junk, &tmp);
	m->movePos = *Entity_GetPos(p, &tmp);
	cObjMove_SetTime(m, t);
	m->fade = OBJMOVE_SMOOTH;
	cActCtrl_Play(ChType_ActCtrl(p), "Action", 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Action", 1);
}

/* the AI walks to the junk point, faced like it (speed of the GenChar logic or 1) */
static void Rusty_GoToJunk(cRustyLocalLogic *self, cAIChar *ai, double speed) {
	vec3_t ang, fwd, dir;
	cEntity *junk = self->junk;

	Obj_GetAng(junk, &ang);
	fwd.x = 0.0f;
	fwd.y = 0.0f;
	fwd.z = -1.0f;
	Vec3_Rotate(&dir, &fwd, &ang);
	const vec3_t *pos = Entity_GetPos(self->junk, &fwd);
	AIChar_SetState(ai, AIC_GO_TO, *pos, dir, 100.0, speed);
}

/* vf17 */
static void cRustyLocalLogic_NextJunk(cRustyLocalLogic *self);
static void cRustyLocalLogic_Update(cLocalLogic *elem, float dt) {
	cRustyLocalLogic *self = (cRustyLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base.base;
	int st = b->state;
	cEntity *p = b->parent;
	vec3_t t1, t2;

	if (st == GENCHAR_RELAX || st == GENCHAR_STAY) {
		cRustyLocalLogic_NextJunk(self);
		Logic_SetState(self, RUSTY_GO_TO_JUNK, NULL);
	}
	if (st == RUSTY_GO_TO_JUNK) {
		cAIChar *ai = Entity_GetAI(p);
		if (AIChar_GetState(ai) || !AIMove_GetArrived(ai)) {
			return;
		}
		if (strcmp(Entity_GetClassName(p), TYEN_WALKCHAR)) {
			return;
		}
		vec3_t jp;
		/* the original uses the buffer, not the returned pointer */
		Entity_GetPos(self->junk, &jp);
		jp.y = Entity_GetPos(p, &t2)->y;
		if (cWorld_TestSphereEntities(g_pWorld, p, 0x10, &jp, WLife(p)->prim.radius, NULL, 1, 0)) {
			return;
		}
		Rusty_StartWork(p, self->junk, 0.25f);
		Logic_SetState(self, RUSTY_WORK, NULL);
		return;
	}
	if (st == RUSTY_WORK) {
		if (b->idle || b->alwaysLock) {
			if (!Obj(p)->move.active) {
				return;
			}
			cObjMove_SetTime(&Obj(p)->move, 0.0f);
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
		cObjMove_SetTime(&Obj(p)->move, 0.0f);
		cActCtrl_Play(ChType_ActCtrl(p), "Walk", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Walk", 1);
		cRustyLocalLogic_NextJunk(self);
		Logic_SetState(self, RUSTY_GO_TO_JUNK, NULL);
		return;
	}
	if (st != RUSTY_PISSED) {
		cGenCharLocalLogic_Update(&self->base.base.logic, dt);
		return;
	}
	Entity_GetAI(p);
	if (strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Run")) {
		Rusty_GoToJunk(self, Entity_GetAI(b->parent), 1.0);
		return;
	}
	cEntity *plat = World_FindEntity("DM_Door_EditShip_00");
	CHECK(plat && "La plataforma no existe");
	const vec3_t *pp = Entity_GetPos(plat, &t1);
	const vec3_t *mp = Entity_GetPos(p, &t2);
	float dx = mp->x - pp->x;
	float dy = mp->y - pp->y;
	float dz = mp->z - pp->z;
	if (dy * dy + dx * dx + dz * dz > (float)(g_PlatformRadiusSqr + 90000)) {
		Logic_SetState(self, RUSTY_GO_TO_JUNK, NULL);
	}
}

static int s_RustyLastJunkPoint = -1;

/* a random junk point other than the last one */
static void cRustyLocalLogic_NextJunk(cRustyLocalLogic *self) {
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
static byte cRustyLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cRustyLocalLogic *self = (cRustyLocalLogic *)elem;
	if (state == RUSTY_GO_TO_JUNK) {
		if ((byte)cBaseLocalLogic_SetState(&self->base.base.logic, RUSTY_GO_TO_JUNK, target)) {
			cEntity *p = self->base.base.parent;
			Rusty_GoToJunk(self, Entity_GetAI(p), (double)g_GenCharRelaxSpeed);
		}
		return 1;
	}
	if (state != RUSTY_PISSED) {
		return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
	}
	cEntity *p = self->base.base.parent;
	cActCtrl_Play(ChType_ActCtrl(p), "Pissed", 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
	return 1;
}

/* the vtable of cRustyLocalLogic */
static const cGenCharLocalLogicVtbl cRustyLocalLogic_vftable = {
	.base.base.Delete = cRustyLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cRustyLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cRustyLocalLogic_VarS,
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
	.base.Update = cRustyLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cRustyLocalLogic_SetState,
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

static cPy_NativeFunc NativeFunc_IncrementHits;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_JunkPointsListName, &GVar_JunkPointsListName)
INIT_GVAR_STRING(Init_GVar_JunkPointsListName, Fini_GVar_JunkPointsListName, &GVar_JunkPointsListName, "JunkPointsListName", 1, "Junk points list name", "JunkPoints")
INIT_GVAR_INT(Init_GVar_NumWorkingLoops, &GVar_NumWorkingLoops, "NumWorkingLoops", 0, "Num working loops", 3)
INIT_GVAR_INT(Init_GVar_PlatformRadiusSqr, &GVar_PlatformRadiusSqr, "PlatformRadiusSqr", 0, "Platform radius squared", 1575025)
INIT_NATIVE_FUNC(Init_NativeFunc_IncrementHits, &NativeFunc_IncrementHits, "IncrementHits", Rusty_LoopDone)
