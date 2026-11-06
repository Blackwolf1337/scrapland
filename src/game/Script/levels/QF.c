/*
 * The scripts of the level QF in C (levels/qf/scripts of data03.packed, issue #2): Mission_BankersCombat, the team
 * combat against the bankers with the photos of the blueprints of the banker ship. Its other functions, Map, MapSnd
 * and the super deals are data (the files in data/levels/qf). The module variables (plansNames, numPlansWithPhoto,
 * SuperCrazyDealHasFinished) stay in the script.
 */
#include "scrap/script.h"
#include "Levels.h"

#define BC "Mission_BankersCombat"

/* Speech.AddRemoteCom(key, "PoliceBoss", fn, ()) with fn the global of the mission (NULL: no arguments after it) */
static void Qf_RemoteCom(scriptFrame_t *f, const char *key, const char *fn, byte params) {
	int mark = f->n;
	PyObject *add = Sf_Mod(f, "Speech", "AddRemoteCom"), *func = Sf_Global(f, BC, fn);
	if (params) {
		Sf_Call(f, add, "(ssO())", key, "PoliceBoss", func);
	} else {
		Sf_Call(f, add, "(ssO)", key, "PoliceBoss", func);
	}
	Sf_Release(f, mark);
}

/* CreatePlans(): the bankers attack the player, the two blueprints to photograph at their dummies */
static PyObject *Bc_CreatePlans(PyObject *self, PyObject *args) {
	static const char *const plans[] = { "PlanoMBanker1", "PlanoSBanker1" };
	scriptFrame_t f = { 0 };
	char name[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Qf_RemoteCom(&f, "Mission_GetBankerShipPlans_RemoteMessage4", "AllBankersAgainstPlayer", 1);
	Sf_Import(&f, "CharAct");
	Sf_SetMod(&f, "CharAct", "TakenPhoto", Sf_Global(&f, BC, "TakePlanPhoto"));
	for (int i = 1; i <= 2 && !f.failed; i++) {
		int mark = f.n;
		Str_Printf(name, sizeof(name), "DM_Mission_BankerPlan%d", i);
		PyObject *dummy = Lv_Dummy(&f, name, 1);
		int ok = Sf_True(&f, dummy);
		if (ok > 0) {
			PyObject *p = Sf_Item(&f, dummy, 1), *a = Sf_Item(&f, dummy, 2);
			PyObject *pos = Sf_Build(&f, "(OOO)", Sf_Item(&f, p, 0), Sf_Item(&f, p, 1), Sf_Item(&f, p, 2));
			PyObject *ang = Sf_Build(&f, "(OOi)", Sf_Item(&f, a, 0), Sf_Item(&f, a, 1), 0);
			PyObject *pln = Sf_Ask(&f, "MissionsFuncs", "CreatePhotoObject", "(sOO)", plans[i - 1], pos, ang);
			Sf_Method(&f, Sf_Global(&f, BC, "plansNames"), "append", "(O)", Sf_Attr(&f, pln, "Name"));
		} else if (ok == 0) {
			Sf_RaiseText(&f, "Uno de los dummies de los planos de la nave de los banqueros no existe");
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* TakePlanPhoto(planName): a photo of one of the blueprints; the engines or the ship into the hangar lists; 0 when it
 * is no blueprint of the mission */
static PyObject *Bc_TakePlanPhoto(PyObject *self, PyObject *args) {
	PyObject *plan;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &plan)) {
		return NULL;
	}
	Sf_Import(&f, "CharAct");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "string");
	int in = Sf_In(&f, plan, Sf_Global(&f, BC, "plansNames"));
	if (in == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	Sf_Run(&f, "MissionsFuncs", "UnsetPhotoObject", "(O)", Sf_Entity(&f, plan));
	PyObject *pos = Sf_Ask(&f, "string", "find", "(Os)", plan, "M");
	int engines = Sf_Compare(&f, pos, "!=", Sf_Int(&f, -1));
	if (engines >= 0) {
		const char *var = engines ? "Hangar.availableEnginesList" : "Hangar.shipsToEditList";
		const char *item = engines ? "MBANKER1" : "SBanker1";
		PyObject *lst = Lv_SaveList(&f, var, BC);
		if (Sf_In(&f, Sf_Str(&f, item), lst) == 0) {
			Lv_SaveAppend(&f, var, lst, Sf_Str(&f, item));
			Sf_Run(&f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
		}
		PyObject *text = Sf_Lang(&f, Sf_Str(&f, engines ? "Mission_GetBankerShipPlans_SystemMessage2" :
			"Mission_GetBankerShipPlans_SystemMessage1"));
		Sf_Run(&f, "RaceMaker", "ShowSystemMessageDelay", "(Od)", text, 0.25);
	}
	Sf_SetGlobal(&f, BC, "numPlansWithPhoto", Sf_OpI(&f, Sf_Global(&f, BC, "numPlansWithPhoto"), '+', 1));
	int both = Sf_EqI(&f, Sf_Global(&f, BC, "numPlansWithPhoto"), 2);
	if (both > 0) {
		Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
		int over = Sf_True(&f, Sf_Global(&f, BC, "SuperCrazyDealHasFinished"));
		if (over > 0) {
			Sf_Import(&f, "TeamDeathMatch");
			Sf_Run(&f, "TeamDeathMatch", "SetInvulnerableStatus", "(O)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
			Qf_RemoteCom(&f, "Mission_GetBankerShipPlans_RemoteMessage5", "SetNextMission", 1);
		} else if (over == 0) {
			Sf_Run(&f, "Speech", "ForceRemoteCom", "(ss)", "Mission_GetBankerShipPlans_RemoteMessage6", "PoliceBoss");
		}
	} else if (both == 0) {
		Sf_Run(&f, "Speech", "ForceRemoteCom", "(ss)", "Mission_GetBankerShipPlans_RemoteMessage3", "PoliceBoss");
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* OnEndOfSuperCrazyDeal(): the end of the combat (TeamDeathMatch.KeepPlaying); the next mission when both photos are
 * taken, else the blueprints */
static PyObject *Bc_OnEndOfSuperCrazyDeal(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, BC, "SuperCrazyDealHasFinished", Sf_Int(&f, 1));
	int both = Sf_EqI(&f, Sf_Global(&f, BC, "numPlansWithPhoto"), 2);
	if (both > 0) {
		Sf_Import(&f, "TeamDeathMatch");
		Sf_Run(&f, "TeamDeathMatch", "SetInvulnerableStatus", "(O)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
		PyObject *params = Sf_Build(&f, "(ssO())", "Mission_GetBankerShipPlans_RemoteMessage5", "PoliceBoss",
			Sf_Global(&f, BC, "SetNextMission"));
		Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Mod(&f, "Speech", "AddRemoteCom"), params, NULL);
	} else if (both == 0) {
		Sf_Import(&f, "MissionsFuncs");
		Sf_Run(&f, "MissionsFuncs", "SetInvulnerableShipsInList", "(si)", "Enemies", 0);
		Sf_Run(&f, "MissionsFuncs", "SetInvulnerableShipsInList", "(si)", "Friends", 0);
		Qf_RemoteCom(&f, "Mission_GetBankerShipPlans_RemoteMessage2", "CreatePlans", 0);
	}
	return Sf_None(&f);
}

/* BankersAttackPlayer(): the first banker ignores its friends (attacks the player); again in 3 s while there is none */
static PyObject *Bc_BankersAttackPlayer(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Enemies", "First"));
	int ok = Sf_True(&f, e);
	if (ok > 0) {
		Sf_SetAttrI(&f, e, "LLogicIgnoreFriends", 1);
	} else if (ok == 0) {
		Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 3), Sf_Global(&f, BC, "BankersAttackPlayer"), Sf_Build(&f, "()"),
			NULL);
	}
	return Sf_None(&f);
}

/* SetNextMission(): the police boss leaves the characters of the conversor, fade out, SetNextMission2 in 2 s */
static PyObject *Bc_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *chars = Lv_SaveList(&f, "Conversor.AvailableChars", BC);
	Sf_Method(&f, chars, "remove", "(s)", "PoliceBoss");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Conversor.AvailableChars", Sf_Repr(&f, chars));
	Lv_Fade(&f, lv_clear, lv_black, 2.0);
	Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 2), Sf_Global(&f, BC, "SetNextMission2"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

PyMethodDef g_QfBankersCombatMethods[] = {
	{ "CreatePlans", Bc_CreatePlans, 1, NULL },
	{ "TakePlanPhoto", Bc_TakePlanPhoto, 1, NULL },
	{ "OnEndOfSuperCrazyDeal", Bc_OnEndOfSuperCrazyDeal, 1, NULL },
	{ "BankersAttackPlayer", Bc_BankersAttackPlayer, 1, NULL },
	{ "SetNextMission", Bc_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
