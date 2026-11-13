/*
 * Entities/Char/Type/BankMaster.cpp of the game: the bank master, whose baton sucks the money out of the characters
 * in front of it (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fx.h"
#include "scrap/staticinit.h"

#define g_FXBankMasterAttackFile (GVar_FXBankMasterAttackFile.value.s)
#define g_BankMasterAmmoCost (GVar_BankMasterAmmoCost.value.f)
#define g_FXBankMasterSuctionFile (GVar_FXBankMasterSuctionFile.value.s)
static cGVar GVar_FXBankMasterSuctionFile;
static cGVar GVar_FXBankMasterAttackFile;
static cGVar GVar_BankMasterAmmoCost;
#define BANKMASTER_SUCTION_ANG 1.0471975511965976	/* 60 degrees */

void ChType_ActionSound(cEntity *v, byte loop, byte stop) {
	cSndItem *s = PtrArray_Get(&WChar(v)->wl.obj.snd, 0);

	if (s) {
		s->volume = 0.0f;
		s->sound.desc.loop = loop;
		if (stop) {
			cSound_Stop(&s->sound);
		}
	}
}

/* params: the bank master, an event, a float */
static void ChType_BankMasterActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 0, 1);
	if ((cWalkCharEntity_GetOwnerSlot(v) || !(WChar(v)->energyBarDecr > 1.0f - WChar(v)->energyUsed)) &&
		WChar(v)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 1);
	}
}

static void ChType_BankMasterActionInit(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 1, 0);
}

/* BankMasterActionPress, BertoActionPress and BishopActionPress: for a character that does not fall and runs no
 * other action, Action as the loop and ActionInit */
static void ChType_ActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (v && Entity_IsType(v, TYEN_WALKCHAR) && !WChar(v)->falling && !ChType_ActPriority(ChType_ActCtrl(v))) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 0);
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 0, 1.0f);
	}
}

void ChType_Attack(cAbstractFile *params, float cost) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	float energy = 1.0f - WChar(v)->energyUsed;
	if ((!cWalkCharEntity_GetOwnerSlot(v) && g_GearLowWeaponEnergy > energy) || !WChar(v)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
	}
	cWalkCharEntity_SetEnergy(v, energy - cost);
}

static void ChType_BankMasterAttack(cAbstractFile *params) {
	ChType_Attack(params, g_BankMasterAmmoCost);
}

/* the characters of the running actions (the "MeEnt" CHECKs) */
static cEntity *g_BankMasterEnt;

/* the sphere test of the hit: params the character in the sphere. Within 60 degrees in front of the bank master its
 * money goes over, with the effect, and its logic is told */
static void ChType_BankMasterSuction(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, t4, d, diff, n;
	cEntity *victim;

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_BankMasterEnt);
	CHECK(Entity_IsType(g_BankMasterEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	victim = World_FindEntity(name);
	if (!victim || g_BankMasterEnt == victim || !Entity_IsType(victim, TYEN_WALKCHAR) ||
		WChar(victim)->wl.god || WChar(victim)->wl.life <= 0) {
		return;
	}
	const vec3_t *p = Entity_GetPos(g_BankMasterEnt, &t1);
	const vec3_t *q = Entity_GetPos(victim, &t2);
	d.x = q->x - p->x;
	d.y = q->y - p->y;
	d.z = q->z - p->z;
	float ax = WChar(g_BankMasterEnt)->view.x + 0.0f;
	float ay = Obj_GetAng(g_BankMasterEnt, &t3)->y + M_PI_F;
	const vec3_t *r = Vec3_GetRotAng(&t4, &d);
	diff.y = ay - r->y;
	diff.z = 0.0f - r->z;
	diff.x = ax - r->x;
	Vec3_NormAng(&n, &diff);
	float nx = (float)__builtin_fabs((double)n.x);
	float ny = (float)__builtin_fabs((double)n.y);
	if (!(BANKMASTER_SUCTION_ANG > (double)nx) || !(BANKMASTER_SUCTION_ANG > (double)ny)) {
		return;
	}
	if (ChType_Steal(&g_BankMasterEnt, &victim, g_BankerStealAmount, "Stats.BankMaster", 0.25f)) {
		FX_BankMasterSuction(g_BankMasterEnt, victim);
	}
	cLocalLogic *l = Logic_GetLocalLogic(victim);
	if (l) {
		Logic_OnAttack(l, g_BankMasterEnt, 100.0f);
	}
}

void FX_BankMasterSuction(cEntity *master, cEntity *victim) {
	cObjElementFXBankMasterSuction *el = FX_DropNew(g_FXBankMasterSuctionFile, 0.8f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, master);
		FXCharActions_SetEntity8(el, victim);
		cObjElementFXBankMasterSuction_RandomPoints(el);
	}
}

/* params: the bank master, an event, a float. The energy of a shot, then the suction in the sphere around it. */
static void ChType_BankMasterHitEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_BankMasterEnt = World_FindEntity(name);
	CHECK(g_BankMasterEnt);
	CHECK(Entity_IsType(g_BankMasterEnt, TYEN_WALKCHAR));
	if (WChar(g_BankMasterEnt)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], 0.0f, 0.5f, 0.35f);
	}
	cWalkCharEntity_SetEnergy(g_BankMasterEnt, (1.0f - WChar(g_BankMasterEnt)->energyUsed) - g_BankMasterAmmoCost);
	cPy_Function_SetNative(&g_ChTypeSphereFunc, (nativefunc_t)ChType_BankMasterSuction);
	float radius = g_BankMasterVacumDist;
	const vec3_t *pos = Entity_GetPos(g_BankMasterEnt, &tmp);
	cWorld_SphereCallF(g_pWorld, 0x10, pos, radius, &g_ChTypeSphereFunc, 0);
}

static void ChType_FXBankMasterAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	cObjElementFXBankMasterAttack *el = FX_DropNew(g_FXBankMasterAttackFile, 1.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXBankMasterAttack_Start(el, v);
	}
}

cGVar GVar_BankerStealAmount;
static cGVar GVar_FXBankMasterAttackFile;
static cGVar GVar_BankMasterAmmoCost;
static cGVar GVar_FXBankMasterSuctionFile;
static cPy_NativeFunc NativeFunc_BankMasterAttack;
static cPy_NativeFunc NativeFunc_BankMasterActionPress;
static cPy_NativeFunc NativeFunc_BankMasterHitEvent;
static cPy_NativeFunc NativeFunc_BankMasterActionInit;
static cPy_NativeFunc NativeFunc_BankMasterActionEnd;
static cPy_NativeFunc NativeFunc_FXBankMasterAttack;
static cPy_NativeFunc NativeFunc_BertoActionPress;
static cPy_NativeFunc NativeFunc_BishopActionPress;
static cPy_NativeFunc NativeFunc_MessengerActionPress;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_BankerStealAmount, &GVar_BankerStealAmount, "BankerStealQuanto", 0, "Money stealed by the banker to the main player", 30)
INIT_GVAR_FLOAT(Init_GVar_BankMasterAmmoCost, &GVar_BankMasterAmmoCost, "BankMasterAmmoCost", 0, "BankMaster AmmoCost Per shoot", 0.05f)
FINI_GVAR_STRING(Fini_GVar_FXBankMasterAttackFile, &GVar_FXBankMasterAttackFile)
INIT_GVAR_STRING(Init_GVar_FXBankMasterAttackFile, Fini_GVar_FXBankMasterAttackFile, &GVar_FXBankMasterAttackFile, "FXBankMasterAttackFile", 0, "Default Model for BankMaster Attack Effect", "Models/GFX/BankMaster/Attack.M3D")
FINI_GVAR_STRING(Fini_GVar_FXBankMasterSuctionFile, &GVar_FXBankMasterSuctionFile)
INIT_GVAR_STRING(Init_GVar_FXBankMasterSuctionFile, Fini_GVar_FXBankMasterSuctionFile, &GVar_FXBankMasterSuctionFile, "FXBankMasterSuctionFile", 0, "Default Model for BankMaster Suction Effect", "Models/GFX/BankMaster/Suction.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_BankMasterAttack, &NativeFunc_BankMasterAttack, "BankMasterAttack", ChType_BankMasterAttack)
INIT_NATIVE_FUNC(Init_NativeFunc_BankMasterActionPress, &NativeFunc_BankMasterActionPress, "BankMasterActionPress", ChType_ActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_BankMasterHitEvent, &NativeFunc_BankMasterHitEvent, "BankMasterHitEvent", ChType_BankMasterHitEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_BankMasterActionInit, &NativeFunc_BankMasterActionInit, "BankMasterActionInit", ChType_BankMasterActionInit)
INIT_NATIVE_FUNC(Init_NativeFunc_BankMasterActionEnd, &NativeFunc_BankMasterActionEnd, "BankMasterActionEnd", ChType_BankMasterActionEnd)
INIT_NATIVE_FUNC(Init_NativeFunc_FXBankMasterAttack, &NativeFunc_FXBankMasterAttack, "FXBankMasterAttack", ChType_FXBankMasterAttack)
INIT_NATIVE_FUNC(Init_NativeFunc_BertoActionPress, &NativeFunc_BertoActionPress, "BertoActionPress", ChType_ActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_BishopActionPress, &NativeFunc_BishopActionPress, "BishopActionPress", ChType_ActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_MessengerActionPress, &NativeFunc_MessengerActionPress, "MessengerActionPress", ChType_ActionPress)
