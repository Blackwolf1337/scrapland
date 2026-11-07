/*
 * The scripts of the level QF in C (levels/qf/scripts of data03.packed, issue #2): Mission_BankersCombat, the team
 * combat against the bankers with the photos of the blueprints of the banker ship. Its other functions, Map, MapSnd
 * and the super deals are data (the files in data/levels/qf). The module variables (plansNames, numPlansWithPhoto,
 * SuperCrazyDealHasFinished) stay in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of the
 * mission are read where the script reads them and written through at once, plansNames is changed in place on the
 * object of the module (OBJLIB 5); the photo objects are handle values (OBJLIB 7); MissionsFuncs, Speech, RaceMaker,
 * TeamDeathMatch, string.find and the functions of the mission go by name (OBJLIB 3).
 */
#include "scrap/scrap.h"
#include "Levels.h"

#define BC "Mission_BankersCombat"

/* CreatePlans(): the bankers attack the player, the two blueprints to photograph at their dummies */
static byte Bc_CreatePlans(void) {
	static const char *const plans[] = { "PlanoMBanker1", "PlanoSBanker1" };
	char name[0x40];
	if (!LvN_Com("Mission_GetBankerShipPlans_RemoteMessage4", "PoliceBoss", BC, "AllBankersAgainstPlayer", LvN_NoArgs()) ||
		!Mode_Import("CharAct") || !LvN_ModSetGlobal("CharAct", "TakenPhoto", BC, "TakePlanPhoto")) {
		return 0;
	}
	byte ok = 1;
	for (int i = 1; i <= 2 && ok; i++) {
		nativeVal_t dummy, pos, ang, plan, pln, n;
		Native_SetNone(&pos);
		Native_SetNone(&ang);
		Native_SetNone(&pln);
		Native_SetNone(&n);
		Str_Printf(name, sizeof(name), "DM_Mission_BankerPlan%d", i);
		LvN_Dummy(name, 1, &dummy);
		LvN_Str(&plan, plans[i - 1]);
		if (Native_True(&dummy)) {
			ok = LvN_DummyPosAng(&dummy, &pos, &ang) && LV_CALL("MissionsFuncs", "CreatePhotoObject", &pln, "vvv", &plan, &pos, &ang) &&
				Mode_VGet(&pln, "Name", &n) && Mode_Append(BC, "plansNames", &n);
		} else {
			ok = LvN_RaiseText("Uno de los dummies de los planos de la nave de los banqueros no existe");
		}
		Native_Free(&dummy);
		Native_Free(&pos);
		Native_Free(&ang);
		Native_Free(&plan);
		Native_Free(&pln);
		Native_Free(&n);
	}
	return ok;
}

/* the blueprint of the photo into its hangar list and the message (engines: the M of the banker's engine was found) */
static byte Bc_Blueprint(byte engines) {
	const char *var = engines ? "Hangar.availableEnginesList" : "Hangar.shipsToEditList";
	nativeVal_t lst, item, text;
	LvN_Str(&item, engines ? "MBANKER1" : "SBanker1");
	Native_SetNone(&text);
	byte ok = LvN_SaveList(var, BC, &lst);
	int in = ok ? Native_Contains(&lst, &item) : -1;
	ok = in >= 0;
	if (in == 0) {
		ok = LvN_SaveAppend(var, &lst, &item);
		if (ok) {
			SaveVar_Inc("NewBluePrintAvaliable", 1);
		}
	}
	if (ok) {
		Mode_Lang(engines ? "Mission_GetBankerShipPlans_SystemMessage2" : "Mission_GetBankerShipPlans_SystemMessage1", &text);
		ok = LV_CALL("RaceMaker", "ShowSystemMessageDelay", NULL, "vd", &text, 0.25);
	}
	Native_Free(&lst);
	Native_Free(&item);
	Native_Free(&text);
	return ok;
}

/* TeamDeathMatch.SetInvulnerableStatus(Scrap.UsrEntity(0)) */
static byte Bc_Invulnerable(void) {
	nativeVal_t usr;
	byte ok = Mode_Import("TeamDeathMatch") && Mode_UsrHandle(&usr);
	if (ok) {
		ok = LV_CALL("TeamDeathMatch", "SetInvulnerableStatus", NULL, "v", &usr);
		Native_Free(&usr);
	}
	return ok;
}

/* TakePlanPhoto(planName): a photo of one of the blueprints; the engines or the ship into the hangar lists; 0 when it
 * is no blueprint of the mission */
static byte Bc_TakePlanPhoto(const nativeVal_t *plan, nativeVal_t *out) {
	nativeVal_t names, h, pos, minus;
	Native_SetNone(&h);
	Native_SetNone(&pos);
	if (!Mode_Import("CharAct") || !Mode_Import("RaceMaker") || !Mode_Import("string") || !Mode_G(BC, "plansNames", &names)) {
		return 0;
	}
	int in = Native_Contains(&names, plan);
	Native_Free(&names);
	if (in <= 0) {
		Native_SetInt(out, 0);
		return in == 0;
	}
	byte ok = Mode_EntityH(plan, &h) && LV_CALL("MissionsFuncs", "UnsetPhotoObject", NULL, "v", &h) &&
		LV_CALL("string", "find", &pos, "vs", plan, "M");
	Native_SetInt(&minus, -1);
	ok = ok && Bc_Blueprint(Native_Compare(&pos, &minus) != 0) && LvN_GAdd(BC, "numPlansWithPhoto", 1, NULL);
	Native_Free(&h);
	Native_Free(&pos);
	int both = ok ? LvN_GEqI(BC, "numPlansWithPhoto", 2) : -1;
	if (both > 0) {
		int over = LvN_ModNone("CharAct", "TakenPhoto") ? LvN_GTrue(BC, "SuperCrazyDealHasFinished") : -1;
		if (over > 0) {
			ok = Bc_Invulnerable() && LvN_Com("Mission_GetBankerShipPlans_RemoteMessage5", "PoliceBoss", BC, "SetNextMission", LvN_NoArgs());
		} else {
			ok = over == 0 && LV_CALL("Speech", "ForceRemoteCom", NULL, "ss", "Mission_GetBankerShipPlans_RemoteMessage6", "PoliceBoss");
		}
	} else {
		ok = both == 0 && LV_CALL("Speech", "ForceRemoteCom", NULL, "ss", "Mission_GetBankerShipPlans_RemoteMessage3", "PoliceBoss");
	}
	Native_SetInt(out, 1);
	return ok;
}

/* OnEndOfSuperCrazyDeal(): the end of the combat (TeamDeathMatch.KeepPlaying); the next mission when both photos are
 * taken, else the blueprints */
static byte Bc_OnEndOfSuperCrazyDeal(void) {
	int both = LvN_PutI(BC, "SuperCrazyDealHasFinished", 1) ? LvN_GEqI(BC, "numPlansWithPhoto", 2) : -1;
	if (both > 0) {
		nativeVal_t params[4];
		LvN_Str(&params[0], "Mission_GetBankerShipPlans_RemoteMessage5");
		LvN_Str(&params[1], "PoliceBoss");
		Native_SetNone(&params[2]);
		Native_SetEmpty(&params[3], NV_TUPLE);
		byte ok = Bc_Invulnerable() && LvN_GObj(BC, "SetNextMission", &params[2]) &&
			Mode_LaterIn(1.0, "Speech", "AddRemoteCom", params, 4, NULL);
		for (int i = 0; i < 4; i++) {
			Native_Free(&params[i]);
		}
		return ok;
	}
	return both == 0 && Mode_Import("MissionsFuncs") &&
		LV_CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "si", "Enemies", 0L) &&
		LV_CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "si", "Friends", 0L) &&
		LvN_Com("Mission_GetBankerShipPlans_RemoteMessage2", "PoliceBoss", BC, "CreatePlans", NULL);
}

/* BankersAttackPlayer(): the first banker ignores its friends (attacks the player); again in 3 s while there is none */
static byte Bc_BankersAttackPlayer(void) {
	nativeVal_t name, e;
	Native_SetNone(&name);
	Native_SetNone(&e);
	byte ok = Mode_ListGet("Enemies", "First", &name) && Mode_EntityH(&name, &e);
	if (ok) {
		ok = Native_True(&e) ? LvN_SetI(&e, "LLogicIgnoreFriends", 1) : Mode_Later(3, BC, "BankersAttackPlayer", NULL, 0, NULL);
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* SetNextMission(): the police boss leaves the characters of the conversor, fade out, SetNextMission2 in 2 s */
static byte Bc_SetNextMission(void) {
	nativeVal_t chars, boss;
	LvN_Str(&boss, "PoliceBoss");
	byte ok = LvN_SaveList("Conversor.AvailableChars", BC, &chars) && LvN_ListRemove(&chars, &boss);
	if (ok) {
		LvN_SaveRepr("Conversor.AvailableChars", &chars);
		ok = Mode_Fade(0, lv_clear, lv_black, 2.0) && Mode_Later(2, BC, "SetNextMission2", NULL, 0, NULL);
	}
	Native_Free(&chars);
	Native_Free(&boss);
	return ok;
}

LV_CORE0(Bc_CreatePlans)
LV_CORE0(Bc_OnEndOfSuperCrazyDeal)
LV_CORE0(Bc_BankersAttackPlayer)
LV_CORE0(Bc_SetNextMission)

static byte Bc_TakePlanPhoto_C(const nativeVal_t *a, nativeVal_t *out) {
	return Bc_TakePlanPhoto(&a[0], out);
}

LV_FN(Bc, CreatePlansPy, 0, Bc_CreatePlans_C)
LV_FN(Bc, TakePlanPhotoPy, 1, Bc_TakePlanPhoto_C)
LV_FN(Bc, OnEndOfSuperCrazyDealPy, 0, Bc_OnEndOfSuperCrazyDeal_C)
LV_FN(Bc, BankersAttackPlayerPy, 0, Bc_BankersAttackPlayer_C)
LV_FN(Bc, SetNextMissionPy, 0, Bc_SetNextMission_C)

PyMethodDef g_QfBankersCombatMethods[] = {
	{ "CreatePlans", Bc_CreatePlansPy, 1, NULL },
	{ "TakePlanPhoto", Bc_TakePlanPhotoPy, 1, NULL },
	{ "OnEndOfSuperCrazyDeal", Bc_OnEndOfSuperCrazyDealPy, 1, NULL },
	{ "BankersAttackPlayer", Bc_BankersAttackPlayerPy, 1, NULL },
	{ "SetNextMission", Bc_SetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
