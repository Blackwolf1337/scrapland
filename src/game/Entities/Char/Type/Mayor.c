/*
 * Entities/Char/Type/Mayor.cpp of the game: the mayor, whose bla-bla makes the characters around him listen (the
 * action MayorListen of their class, their logic paused) or fall asleep (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxelements.h"
#include "scrap/scrapcollision.h"
#include "scrap/staticinit.h"

#define g_MayorListenPercent (GVar_MayorListenPercent.value.f)
#define g_MayorSleepPercent (GVar_MayorSleepPercent.value.f)
static cGVar GVar_MayorSleepPercent;
static cGVar GVar_MayorListenPercent;

/* the characters of the running actions (the "MeEnt" CHECKs), the nurse by name and the character it hits */
static cEntity *g_MayorEnt;

/* params: the listener, an event, a float. It stops listening (its logic goes on, Relax) unless its mayor still
 * talks near it and in sight */
static void ChType_ListeningMayor(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t t1, t2;

	ChType_ReadParams(params, name, event);
	g_MayorEnt = World_FindEntity(name);
	CHECK(g_MayorEnt);
	CHECK(Entity_IsType(g_MayorEnt, TYEN_WALKCHAR));
	void *acm = WithLife_Vtbl(g_MayorEnt)->GetAction(g_MayorEnt, "MayorListen");
	cLocalLogic *ll = Logic_GetLocalLogic(g_MayorEnt);
	CHECK(ll && acm);
	if (!ChLogic_Idle(ll) && !ChLogic_AlwaysLock(ll)) {
		Logic_Vtbl(ll)->SetIdle(ll, 1);
	} else {
		cEntity *mayor = WChar_Listen(g_MayorEnt);
		if (mayor && Entity_IsType(mayor, TYEN_WALKCHAR)) {
			const vec3_t *mp = g_MayorEnt->vftable->GetPos(g_MayorEnt, &t1);
			const vec3_t *tp = mayor->vftable->GetPos(mayor, &t2);
			float dy = tp->y - mp->y;
			float dx = tp->x - mp->x;
			float dz = tp->z - mp->z;
			if (!(dy * dy + dx * dx + dz * dz > g_MayorBlabliblaRadius * g_MayorBlabliblaRadius) &&
				!crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(mayor), 0), "Action", 6)) {
				const vec3_t *to = g_MayorEnt->vftable->GetPos(g_MayorEnt, &t1);
				const vec3_t *from = mayor->vftable->GetPos(mayor, &t2);
				if (!Engine_TestLine(from, to, NULL, NULL)) {
					return;
				}
			}
		}
	}
	Nto1Node *n = &WChar(g_MayorEnt)->listen;
	CHECK(n->data);
	Nto1Node_Unlink(n);
	Logic_Vtbl(ll)->SetIdle(ll, 0);
	cActCtrl_Play(ChType_ActCtrl(g_MayorEnt), "Relax", 1, 1.0f);
}

/* params: the mayor, an event, a float: ChType_MayorSphereHit for the characters in his radius, Relax again once
 * the button is up */
static void ChType_MayorSphereHit(cAbstractFile *params);
static void ChType_MayorEndAction(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_MayorEnt = World_FindEntity(name);
	CHECK(g_MayorEnt);
	CHECK(Entity_IsType(g_MayorEnt, TYEN_WALKCHAR));
	cPy_Function_SetNative(&g_ChTypeSphereFunc, (nativefunc_t)ChType_MayorSphereHit);
	cWorld_SphereCallF(g_pWorld, 0x10, g_MayorEnt->vftable->GetPos(g_MayorEnt, &tmp), g_MayorBlabliblaRadius,
		&g_ChTypeSphereFunc, 0);
	if (!WChar(g_MayorEnt)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(g_MayorEnt), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(g_MayorEnt), "Relax", 1);
	}
}

/* the sphere test of MayorEndAction: params a character. One whose logic listens to the mayor starts MayorListen
 * (always for the player, else by MayorListenPercent and not when busy), with the alarm on or for a sentinel of the
 * player it falls asleep (by MayorSleepPercent for an AI mayor) */
static void ChType_MayorSphereHit(cAbstractFile *params) {
	char name[0x80];

	CHECK(g_MayorEnt);
	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *e = World_FindEntity(name);
	byte alarm = g_pWorld->alarm.on;
	CHECK(g_MayorEnt);
	CHECK(Entity_IsType(g_MayorEnt, TYEN_WALKCHAR));
	if (!e) {
		return;
	}
	void *acm = WithLife_Vtbl(e)->GetAction(e, "MayorListen");
	cLocalLogic *ll = Logic_GetLocalLogic(e);
	if (!ll || !acm || !Logic_Vtbl(ll)->Listens(ll, g_MayorEnt)) {
		return;
	}
	byte usr = cWalkCharEntity_GetOwnerSlot(g_MayorEnt) != -1;
	if (!alarm && (!usr || strcmp(ChType_ActClass(ChType_ActCtrl(e))->name, "Sentinel"))) {
		if (ChType_Action0(ChType_ActCtrl(e)) == acm) {
			goto sleep;
		}
		if (cAIChar_TestFlags(ll) &&
			(!usr || crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(e), 0), "Relax", 5))) {
			return;
		}
		if (usr || (!Obj(e)->radarShow && g_MayorListenPercent > FxElem_Rand(100.0, 0.0))) {
			cActCtrl_PlayAction(ChType_ActCtrl(e), acm, 0, 1, 1.0f);
			if (ChType_Action0(ChType_ActCtrl(e)) == acm) {
				Logic_Vtbl(ll)->SetIdle(ll, 1);
				cEntity_AddLink(g_MayorEnt, &WChar(e)->listen);
				cLocalLogic *ml = Logic_GetLocalLogic(g_MayorEnt);
				if (ml) {
					cMayorLocalLogic_SetListener(ml, e);
				}
			}
		}
		return;
	}
sleep:
	if (usr) {
		if (!cActCtrl_IsActive(ChType_ActCtrl(e), "Dazed")) {
			SaveVar_Inc("Stats.Mayor", 1);
		}
	} else if (!(g_MayorSleepPercent > FxElem_Rand(100.0, 0.0))) {
		return;
	}
	cActCtrl_Play(ChType_ActCtrl(e), "Dazed", 1, 10.0f);
}

static cGVar GVar_MayorListenPercent;
static cGVar GVar_MayorSleepPercent;
cGVar GVar_MayorBlabliblaRadius;
static cPy_NativeFunc NativeFunc_MayorEndAction;
static cPy_NativeFunc NativeFunc_ListeningMayor;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MayorBlabliblaRadius, &GVar_MayorBlabliblaRadius, "MayorBlabliblaRadius", 0, "Radius of Mayor´s Bla-Blas.", 1000.0f)
INIT_GVAR_FLOAT(Init_GVar_MayorListenPercent, &GVar_MayorListenPercent, "MayorListenPercent", 0, "Listen percent probability", 50.0f)
INIT_GVAR_FLOAT(Init_GVar_MayorSleepPercent, &GVar_MayorSleepPercent, "MayorSleepPercent", 0, "Sleep when listen probability", 20.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_MayorEndAction, &NativeFunc_MayorEndAction, "MayorEndAction", ChType_MayorEndAction)
INIT_NATIVE_FUNC(Init_NativeFunc_ListeningMayor, &NativeFunc_ListeningMayor, "ListeningMayor", ChType_ListeningMayor)
