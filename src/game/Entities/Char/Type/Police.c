/*
 * Entities/Char/Type/Police.cpp of the game: the police, who make the characters listen (PoliceListen) and take
 * their money ("show me the money"), call the gears on the ones who do not pay and turn into a gear
 * (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/actions.h"
#include "scrap/modmove.h"
#include "scrap/staticinit.h"

#define g_FXPoliceGearConversionFile (GVar_FXPoliceGearConversionFile.value.s)
#define g_FXPoliceStealFile (GVar_FXPoliceStealFile.value.s)
#define g_PoliceAlarmStepSTM (GVar_PoliceAlarmStepSTM.value.f)
static cGVar GVar_PoliceAlarmStepSTM;
static cGVar GVar_FXPoliceStealFile;
static cGVar GVar_FXPoliceGearConversionFile;

/* params: the police, an event, a float: the effect of the conversion into a gear for 5 seconds */
static void ChType_FXPoliceGearConversion(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	cObjElementFXPoliceGearConversion *el = FX_DropNew(g_FXPoliceGearConversionFile, 5.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXPoliceGearConversion_Start(el, v);
	}
}

/* params: the attacker and the attacked (both WalkChars): the coins of the steal */
static void FX_PoliceSteal(cEntity *attacker, cEntity *attacked);
static void ChType_FXPoliceSteal(cAbstractFile *params) {
	char a[0x80], b[0x80];

	cAbstractFile_ReadString(params, a, 0x80);
	cAbstractFile_ReadString(params, b, 0x80);
	cEntity *attacker = World_FindEntity(a);
	CHECK(attacker);
	CHECK(Entity_IsType(attacker, TYEN_WALKCHAR));
	cEntity *attacked = World_FindEntity(b);
	CHECK(attacked);
	CHECK(Entity_IsType(attacked, TYEN_WALKCHAR));
	FX_PoliceSteal(attacker, attacked);
}

/* the coins from attacked to attacker for 1.35 s when the attacker is in sight */
static void FX_PoliceSteal(cEntity *attacker, cEntity *attacked) {
	vec3_t tmp;

	CHECK(attacker);
	CHECK(Entity_IsType(attacker, TYEN_WALKCHAR));
	CHECK(attacked);
	CHECK(Entity_IsType(attacked, TYEN_WALKCHAR));
	const vec3_t *p = Entity_GetPos(attacker, &tmp);
	if (!FX_IsVisible(p->x, p->y, p->z, 2500.0f, 0.0f, NULL, 0)) {
		return;
	}
	cObjElementFXPoliceSteal *el = FX_DropNew(g_FXPoliceStealFile, 1.35000002f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, attacker);
		FXCharActions_SetEntity8(el, attacked);
	}
}

/* the actions of a police while it talks: Action, ActionNPC, ActionInit, ActionInitNPC */
static int Police_Talks(const char *a) {
	if (crt_strncmp(a, "Action", 6)) {
		return 0;
	}
	a += 6;
	return !*a || !strcmp(a, "NPC") || !strcmp(a, "InitNPC") || !strcmp(a, "Init");
}

/* params: the listener, an event, a float. It stops listening (its logic goes on, Relax) once its police does not
 * talk any more */
static void ChType_ListeningPolice(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *me = World_FindEntity(name);
	CHECK(me);
	CHECK(Entity_IsType(me, TYEN_WALKCHAR));
	void *acm = WithLife_GetAction(me, "PoliceListen");
	cLocalLogic *ll = Logic_GetLocalLogic(me);
	CHECK(ll && acm);
	if (!ChLogic_Idle(ll) && !ChLogic_AlwaysLock(ll)) {
		Logic_SetIdle(ll, 1);
	} else {
		cEntity *police = WChar_Listen(me);
		if (police && Entity_IsType(police, TYEN_WALKCHAR) &&
			Police_Talks(cActCtrl_GetActionName(ChType_ActCtrl(police), 0))) {
			return;
		}
	}
	Nto1Node *n = &WChar(me)->listen;
	CHECK(n->data);
	Nto1Node_Unlink(n);
	Logic_SetIdle(ll, 0);
	cActCtrl_Play(ChType_ActCtrl(me), "Relax", 1, 1.0f);
}

/* the character v (not falling) talks to: the player its listener while it still talks, an AI the target of its
 * logic. 1 when that one can listen (its class has PoliceListen in *action): an AI only when awake (0x381), the
 * player not while a police of the player still runs an Action. usr: v is a player. */
byte ChType_PoliceFindListener(cEntity *v, cEntity **victim, byte *usr, void **action) {
	if (WChar(v)->falling) {
		return 0;
	}
	*usr = cWalkCharEntity_GetOwnerSlot(v) != -1;
	if (*usr) {
		if (WChar_Listen(v) && WChar(v)->listenOn) {
			*victim = WChar_Listen(v);
		}
	} else {
		cLocalLogic *l = Logic_GetLocalLogic(v);
		if (l) {
			*victim = ChLogic_Target(l);
			ChLogic_PoliceResult(l) = 0;
		}
	}
	if (!*victim || !Entity_IsType(*victim, TYEN_WALKCHAR)) {
		return 0;
	}
	void *ctrl = ChType_ActCtrl(*victim);
	if (!ctrl) {
		return 0;
	}
	cActClass *cls = ChType_ActClass(ctrl);
	if (!cls) {
		return 0;
	}
	*action = cActClass_GetAction(cls, "PoliceListen");
	if (!*action) {
		return 0;
	}
	if (cWalkCharEntity_GetOwnerSlot(*victim) == -1) {
		return WChar(*victim)->wl.arrow.canUse ? 1 : 0;
	}
	if (!strcmp(cls->name, "Police") &&
		!crt_strncmp(cActCtrl_GetActionName(ctrl, 0), "Action", 6)) {
		return 0;
	}
	return 1;
}

/* params: the police. It talks to its listener (turned to it, ActionInit, ActionInitNPC for an AI), which listens
 * when it is an AI and awake; the player calls the sentinels */
static void ChType_PoliceActionPress(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2;
	cEntity *victim = NULL;
	byte usr;
	void *action;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if (!ChType_PoliceFindListener(v, &victim, &usr, &action)) {
		return;
	}
	cEntity_AddLink(victim, &WChar(v)->listen);
	if (!cActCtrl_Play(ChType_ActCtrl(v), usr ? "ActionInit" : "ActionInitNPC", 0, 1.0f)) {
		return;
	}
	cObjMove *move = &Obj(v)->move;
	const vec3_t *tp = Entity_GetPos(victim, &t1);
	const vec3_t *vp = Entity_GetPos(v, &t2);
	float at = (float)atan2((double)(vp->x - tp->x), (double)(vp->z - tp->z));
	move->moveAng.x = 0.0f;
	move->moveAng.y = at;
	move->moveAng.z = 0.0f;
	move->movePos = *Entity_GetPos(v, &t2);
	cObjMove_SetTime(move, 0.200000003f);
	move->viewAng = 0;
	cActCtrl_SetDefLoop(ChType_ActCtrl(v), usr ? "Action" : "ActionNPC", 0);
	if (usr) {
		WChar(v)->listenOn = 0;
		AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, Entity_GetPos(v, &t2));
	}
	cLocalLogic *l = Logic_GetLocalLogic(victim);
	if (l && WChar(victim)->wl.arrow.canUse && cActCtrl_PlayAction(ChType_ActCtrl(victim), action, 0, 1, 1.0f)) {
		Logic_SetIdle(l, 0);
		Logic_SetIdle(l, 1);
		cEntity_AddLink(v, &WChar(victim)->listen);
	}
}

/* params: the police, an event, a float. The end of the talk: the money of the listener (PoliceStealQuanto) when
 * it pays, the alarm for the player, the gears on an AI that does not pay; the police lets it go */
static void ChType_PoliceEndActionMoney(cAbstractFile *params) {
	char name[0x80], event[0x80];
	byte usr = 0;

	ChType_ReadParams(params, name, event);
	cEntity *me = World_FindEntity(name);
	CHECK(me);
	CHECK(Entity_IsType(me, TYEN_WALKCHAR));
	const char *act = "ActionBad";
	cEntity *victim = WChar_Listen(me);
	cLocalLogic *ll = Logic_GetLocalLogic(me);
	if (victim) {
		usr = cWalkCharEntity_GetOwnerSlot(victim) != -1;
		if (usr) {
			if (ChLogic_PoliceResult(ll) == -1) {
				act = "ActionBad";
				Alarm_Set(&g_pWorld->alarm, 1.0f);
			} else {
				if (WChar(me)->control.act) {
					return;
				}
				act = "Relax";
				Alarm_Set(&g_pWorld->alarm, g_pWorld->alarm.value + g_PoliceAlarmStepSTM);
			}
		} else {
			cLocalLogic *vl = Logic_GetLocalLogic(victim);
			if (vl) {
				const char *cls = ChType_ActClass(ChType_ActCtrl(victim))->name;
				if (me == cWorld_GetUsrEntity(g_pWorld, 0) &&
					(!strcmp(cls, "Police") || !strcmp(cls, "Sentinel") || !strcmp(cls, "Gear"))) {
					act = "Relax";
				}
				if (cActCtrl_IsActive(ChType_ActCtrl(victim), "PoliceListen") &&
					Logic_GivesMoney(vl, me) &&
					ChType_Steal(&me, &victim, g_PoliceStealAmount, "Stats.Police", 1.0f)) {
					act = "ActionOK";
					FX_PoliceSteal(me, victim);
				}
			}
		}
	}
	if (ll && strcmp(act, "Relax")) {
		Logic_RestartState(ll);
	}
	if (cWalkCharEntity_GetOwnerSlot(me) != -1) {
		WChar(me)->listenOn = 1;
	} else if (victim && !strcmp(act, "ActionBad")) {
		act = "Relax";
		if (!usr) {
			AICtrl_SendSentinelToNPC(&g_pWorld->logic.aiCtrl, victim);
			AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, victim, 0);
			if (!strcmp(cActCtrl_GetActionName(ChType_ActCtrl(me), 0), "Fusion")) {
				return;
			}
			Logic_SetState(ll, 7, victim);
		}
	}
	Nto1Node *n = &WChar(me)->listen;
	CHECK(n->data);
	Nto1Node_Unlink(n);
	cActCtrl_Play(ChType_ActCtrl(me), act, 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(me), "Relax", 1);
}

/* params: the police, an event, a float: the turn of its move ends */
static void ChType_PoliceRestoreFreeTurn(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	cObjMove_SetTime(&Obj(v)->move, 0.0f);
}

static cGVar GVar_FXPoliceStealFile;
static cGVar GVar_FXPoliceGearConversionFile;
static cGVar GVar_PoliceAlarmStepSTM;
cGVar GVar_PoliceStealAmount;
static cPy_NativeFunc NativeFunc_PoliceActionPress;
static cPy_NativeFunc NativeFunc_PoliceRestoreFreeTurn;
static cPy_NativeFunc NativeFunc_PoliceEndActionMoney;
static cPy_NativeFunc NativeFunc_ListeningPolice;
static cPy_NativeFunc NativeFunc_FXPoliceSteal;
static cPy_NativeFunc NativeFunc_FXPoliceGearConversion;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_PoliceStealAmount, &GVar_PoliceStealAmount, "PoliceStealQuanto", 0, "Money stealed by the banker to the main player", 10)
INIT_GVAR_FLOAT(Init_GVar_PoliceAlarmStepSTM, &GVar_PoliceAlarmStepSTM, "PoliceAlarmStepSTM", 0, "Police Alarm Step in any Show Me The Money Action.", 0.1f)
FINI_GVAR_STRING(Fini_GVar_FXPoliceStealFile, &GVar_FXPoliceStealFile)
INIT_GVAR_STRING(Init_GVar_FXPoliceStealFile, Fini_GVar_FXPoliceStealFile, &GVar_FXPoliceStealFile, "FXPoliceStealFile", 0, "Default Model for Police Steal Effect", "Models/GFX/Police/PoliceSteal.M3D")
FINI_GVAR_STRING(Fini_GVar_FXPoliceGearConversionFile, &GVar_FXPoliceGearConversionFile)
INIT_GVAR_STRING(Init_GVar_FXPoliceGearConversionFile, Fini_GVar_FXPoliceGearConversionFile, &GVar_FXPoliceGearConversionFile, "FXPoliceGearConversionFile", 0, "Default Model for Police Gear Conversion Effect", "Models/GFX/Police/Conversion.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_PoliceActionPress, &NativeFunc_PoliceActionPress, "PoliceActionPress", ChType_PoliceActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_PoliceRestoreFreeTurn, &NativeFunc_PoliceRestoreFreeTurn, "PoliceRestoreFreeTurn", ChType_PoliceRestoreFreeTurn)
INIT_NATIVE_FUNC(Init_NativeFunc_PoliceEndActionMoney, &NativeFunc_PoliceEndActionMoney, "PoliceEndActionMoney", ChType_PoliceEndActionMoney)
INIT_NATIVE_FUNC(Init_NativeFunc_ListeningPolice, &NativeFunc_ListeningPolice, "ListeningPolice", ChType_ListeningPolice)
INIT_NATIVE_FUNC(Init_NativeFunc_FXPoliceSteal, &NativeFunc_FXPoliceSteal, "FXPoliceSteal", ChType_FXPoliceSteal)
INIT_NATIVE_FUNC(Init_NativeFunc_FXPoliceGearConversion, &NativeFunc_FXPoliceGearConversion, "FXPoliceGearConversion", ChType_FXPoliceGearConversion)
