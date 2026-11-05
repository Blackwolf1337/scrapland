/*
 * The gameplay functions of the character scripts in C (issue #2, step 6e): action presses, the special effects of a
 * character and its speech routines. Their Init and callbacks are data (Characters.c, data/chars/); these are logic
 * on entity variables and module state, so they are C. Each table goes into its module next to the one of
 * Characters.c (script_modules in Script.c). Module variables other scripts may read or change (Functionary.PressOk,
 * CrazyGambler.CDcompl, speedFactor and the like) stay variables of the module.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static void Char_SetText(const char *ent, const char *var, const char *s) {
	Script_EntSet(ent, var, PyString_FromString(s));
}

/* SAct.GetClass(cls); SAct.SetCls(var, d) */
static void Char_SetCls(const char *cls, const char *var, double d) {
	int i = (int)d;
	g_pAuxActClass = cActClassList_Get(g_pActClassList, cls);
	if (g_pAuxActClass) {
		cActClass_Var(g_pAuxActClass, var, 1, NULL, &d, &i);
	} else {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
	}
}

/* SScorer.SetMsgText(Scrap.GetLangStr("INSPECT_CHAR_CantTalk"), Scrap.GetTime() + 2): the speech of a character that
 * does not talk */
static PyObject *Char_CantTalk(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Script_Call("SScorer", "SetMsgText", "(sd)", Script_Lang("INSPECT_CHAR_CantTalk"),
		(double)Timer_GetTime(&g_pWorld->timer) + 2.0);
	return Py_ReturnNone();
}

/* ---- Functionary: the slow motion of his special action, while the energy bar empties ---- */

static void Functionary_Enter(const char *name) {
	Script_EntSet(name, "TimeSpeed", Script_Attr("Functionary", "charSpeed"));
	PyErr_Clear();
	Timer_SetSpeed(&g_pWorld->timer, (float)Script_GetFloat("Functionary", "worldSpeed", 1.0));
	if (GVar_GetNum("R_SceneMotionBlurRTarget", 0) == 1) {
		GVar_SetNum("MotionBlurFactorBegin", 0.0);
		GVar_SetNum("MotionBlurFactorEnd", 0.75);
		GVar_SetNum("MotionBlurTime", 0.0);
		GVar_SetNum("MotionBlurEndTime", 0.5);
		GVar_SetNum("MotionBlurOffVar", 0.05);
	} else {
		GVar_SetNum("NoiseFactorBegin", 128);
		GVar_SetNum("NoiseFactor", 24);
		GVar_SetNum("NoiseTime", 0);
		GVar_SetNum("NoiseEndTime", 0.25);
	}
	Char_SetCls("Functionary", "FOVInterpTime", 0.5);
	GVar_SetNum("S_PitchScale", 0.25);
}

static void Functionary_Exit(const char *name) {
	double speed = Script_GetFloat("Functionary", "speedFactor", 1.0);
	Script_EntSet(name, "EnergyBarConstantDecr", PyFloat_FromDouble(0.0));
	Char_SetText(name, "OnEnergyBarFull", "");
	Char_SetCls("Functionary", "FOVInterpTime", 0.5 * speed);
	Script_EntSet(name, "TimeSpeed", PyInt_FromLong(1));
	Timer_SetSpeed(&g_pWorld->timer, 1.0f);
	if (GVar_GetNum("R_SceneMotionBlurRTarget", 0) == 1) {
		GVar_SetNum("MotionBlurFactorBegin", 0.75);
		GVar_SetNum("MotionBlurFactorEnd", 0.0);
		GVar_SetNum("MotionBlurTime", 0.0);
		GVar_SetNum("MotionBlurEndTime", 0.5 * speed);
		GVar_SetNum("MotionBlurOffVar", 0.0);
	} else {
		GVar_SetNum("NoiseFactorBegin", 24);
		GVar_SetNum("NoiseFactor", 0);
		GVar_SetNum("NoiseTime", 0);
		GVar_SetNum("NoiseEndTime", 0.25 / speed);
	}
	GVar_SetNum("S_PitchScale", 1.0);
}

static PyObject *Functionary_EnterFastEffect(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Functionary_Enter(name);
	return Py_ReturnNone();
}

/* also the OnEnergyBarFull of the entity while the action runs */
static PyObject *Functionary_ExitFastEffect(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Functionary_Exit(name);
	return Py_ReturnNone();
}

/* the player presses the action: slow motion when there is energy enough, the bar empties in 60 s */
static PyObject *Functionary_ActionPress(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Script_SetInt("Functionary", "PressOk", 0);
	if (Script_EntNum(name, "UsrControl", -1) == 0 && Script_EntNum(name, "ActionPriority", -1) == 0 &&
		Script_EntNum(name, "Energy", 0) >= GVar_GetNum("FunctionarySpecialActionLow", 0)) {
		double speed = Script_GetFloat("Functionary", "speedFactor", 1.0);
		Script_EntSet(name, "EnergyBarConstantDecr", PyFloat_FromDouble(60.0 * speed));
		Char_SetText(name, "OnEnergyBarFull", "Functionary.ExitFastEffect");
		Functionary_Enter(Script_EntStr(name, "Name", name));
		Script_SetInt("Functionary", "PressOk", 1);
	}
	return Py_ReturnNone();
}

static PyObject *Functionary_ActionRelease(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (Script_EntNum(name, "UsrControl", -1) == 0) {
		Script_EntSet(name, "EnergyBarConstantDecr", PyFloat_FromDouble(0.0));
		Char_SetText(name, "OnEnergyBarFull", "");
		if (Script_EntNum(name, "Energy", 0) != 0.0 && Script_GetInt("Functionary", "PressOk", 0)) {
			Functionary_Exit(Script_EntStr(name, "Name", name));
		}
	}
	return Py_ReturnNone();
}

PyMethodDef g_FunctionaryFuncs[] = {
	{ "EnterFastEffect", Functionary_EnterFastEffect, 1, NULL },
	{ "ExitFastEffect", Functionary_ExitFastEffect, 1, NULL },
	{ "ActionPress", Functionary_ActionPress, 1, NULL },
	{ "ActionRelease", Functionary_ActionRelease, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Rusty: his action presses do nothing (the class names them) ---- */

static PyObject *Char_Nothing(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

PyMethodDef g_RustyFuncs[] = {
	{ "ActionPress", Char_Nothing, 1, NULL },
	{ "ActionRelease", Char_Nothing, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Boss ---- */

/* his action, unless the player falls or does another */
static PyObject *Boss_ActionPress(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr) {
		/* the script reads Scrap.UsrEntity(0).isFalling */
		PyErr_SetString(PyExc_AttributeError, "isFalling");
		return NULL;
	}
	if (!Script_EntNum(usr->name, "isFalling", 0) && Script_EntNum(usr->name, "ActionPriority", -1) == 0) {
		Char_SetText(name, "Action", "Action");
	}
	return Py_ReturnNone();
}

static PyObject *Boss_BossSpeech(PyObject *self, PyObject *args) {
	char *usr, *ch, *opt;
	PyObject *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Script_SetVar("Speech", "PlayerName", PyString_FromString(usr));
	Script_SetVar("Speech", "NPCName", PyString_FromString(ch));
	if (!strcmp(opt, "Talk")) {
		Script_Call("Speech", "OnSpch", "(ss)", "Generics_Boss_InitMsg1", "Generics_Boss_InitAns1");
		Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("Speech", "RestoreState"));
		Script_SetVar("Speech", "OnEndOfSpeechParams", Py_BuildValue("(s)", ch));
	}
	return Py_ReturnNone();
}

PyMethodDef g_BossFuncs[] = {
	{ "ActionPress", Boss_ActionPress, 1, NULL },
	{ "BossSpeech", Boss_BossSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mayor: the action loops while the key is held ---- */

static PyObject *Mayor_MayorActionPress(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Char_SetText(name, "Action", "ActionInit");
	if (!strcmp(Script_EntStr(name, "Action", ""), "ActionInit")) {
		Char_SetText(name, "DefLoop", "Action");
	}
	return Py_ReturnNone();
}

/* an event of his action: again while the key is held */
static PyObject *Mayor_MayorReCheckAction(PyObject *self, PyObject *args) {
	char *name;
	PyObject *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "sOO", &name, &event, &time)) {
		return NULL;
	}
	if (Script_EntNum(name, "ActionPress", 0)) {
		Char_SetText(name, "ForceAction", "ActionInit");
		Char_SetText(name, "ForceDefLoop", "Action");
	}
	return Py_ReturnNone();
}

PyMethodDef g_MayorFuncs[] = {
	{ "MayorActionPress", Mayor_MayorActionPress, 1, NULL },
	{ "MayorReCheckAction", Mayor_MayorReCheckAction, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Maintenance, SputnikJunkyard: they do not talk ---- */

/* the event of his work: the effect between him and the ship of the hangar */
static PyObject *Maintenance_MaintenanceWorkFX(PyObject *self, PyObject *args) {
	char *name;
	PyObject *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "sOO", &name, &event, &time)) {
		return NULL;
	}
	PyObject *ship = Script_Attr("HangarTab", "prevShip");
	PyObject *shipName = ship ? PyObject_GetAttrString(ship, "Name") : NULL;
	Py_XDECREF(ship);
	if (!shipName || !PyString_Check(shipName)) {
		Py_XDECREF(shipName);
		if (!PyErr_Occurred()) {
			PyErr_SetString(PyExc_TypeError, "HangarTab.prevShip.Name");
		}
		return NULL;
	}
	cPy_Function f;
	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, "!FXMaintenanceWork");
	cPy_Function_AddParamS(&f, name);
	cPy_Function_AddParamS(&f, PyString_AsString(shipName));
	cPy_Function_AddParamF(&f, (float)Scrap_RandRange(0.5, 0.9));
	cPy_Function_Execute(&f, 0);
	cPy_Function_dtor(&f);
	Py_DECREF(shipName);
	return Py_ReturnNone();
}

PyMethodDef g_MaintenanceFuncs[] = {
	{ "MaintenanceTalk", Char_CantTalk, 1, NULL },
	{ "MaintenanceWorkFX", Maintenance_MaintenanceWorkFX, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_SputnikJunkyardFuncs[] = {
	{ "SputnikJunkyardTalk", Char_CantTalk, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- CrazyGambler: the mega deal once the three crazy deals are done ---- */

/* CDcompl: how many of the three crazy deals are complete (MissionsFuncs.CrazyDealStatus(i): [.., done, needed]) */
static int CrazyGambler_Status(void) {
	int done = 0;
	Script_SetInt("CrazyGambler", "CDcompl", 0);
	for (int i = 1; i <= 3; i++) {
		PyObject *f = Script_Attr("MissionsFuncs", "CrazyDealStatus");
		PyObject *cd = f ? Script_Invoke(f, Py_BuildValue("(i)", i)) : NULL;
		Py_XDECREF(f);
		PyObject *have = cd ? PySequence_GetItem(cd, 1) : NULL, *need = cd ? PySequence_GetItem(cd, 2) : NULL;
		if (!have || !need) {
			Py_XDECREF(have);
			Py_XDECREF(need);
			Py_XDECREF(cd);
			return -1;
		}
		if (PyObject_Compare(have, need) >= 0) {
			Script_SetInt("CrazyGambler", "CDcompl", ++done);
		}
		Py_DECREF(have);
		Py_DECREF(need);
		Py_DECREF(cd);
	}
	return done;
}

static PyObject *CrazyGambler_CalculateStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return CrazyGambler_Status() < 0 ? NULL : Py_ReturnNone();
}

/* ArrnOpt > 1: ArriOpt = 1 (the arrow on the first option) */
static void CrazyGambler_FirstOption(const char *ch) {
	if (Script_EntNum(ch, "ArrnOpt", 0) > 1) {
		Script_EntSet(ch, "ArriOpt", PyInt_FromLong(1));
	}
}

/* Speech.StdAnsLst(CharsNPC.optionsList) */
static void CrazyGambler_Options(void) {
	PyObject *list = Script_Attr("CharsNPC", "optionsList");
	if (list) {
		Script_Call("Speech", "StdAnsLst", "(O)", list);
		Py_DECREF(list);
	} else {
		PyErr_Print();
	}
}

static PyObject *CrazyGambler_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	char *usr, *ch, *opt, msg[0x80], ans[0x80];
	PyObject *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	if (!strcmp(opt, "Talk")) {
		Script_Call("CharsNPC", "GenericNPCSpeech", "(sssO)", usr, ch, opt, full);
		const char *active = SaveVar_Get("JackInTheBox.Active");
		if (active && !strcmp(active, "1")) {
			int done = CrazyGambler_Status();
			if (done < 0) {
				return NULL;
			}
			PyObject *list = Script_Attr("CharsNPC", "optionsList");
			PyObject *item = PyString_FromString(done == 3 ? "Generics_CrazyGambler_MegaDeal_Yes_" :
				"Generics_CrazyGambler_MegaDeal_No_");
			if (!list || !item || PyList_Insert(list, 0, item) < 0) {
				Py_XDECREF(item);
				Py_XDECREF(list);
				return NULL;
			}
			Py_DECREF(item);
			Script_Call("Speech", "StdAnsLst", "(O)", list);
			const char *lives = SaveVar_Get("Player.InfiniteLives");
			if (lives && !strcmp(lives, "0")) {
				PyObject *e = PyEntity_New(ch);
				Script_Call("Challenge", "AddChallenge", "(Osi)", e ? e : Py_None, usr, PySequence_Length(list));
				Py_XDECREF(e);
			}
			Py_DECREF(list);
		}
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_No_")) {
		CrazyGambler_FirstOption(ch);
		PyObject *n = Script_Attr("CrazyGambler", "CDcompl");
		PyObject *r = n ? PyObject_Repr(n) : NULL;
		Py_XDECREF(n);
		if (!r) {
			return NULL;
		}
		Str_Printf(ans, sizeof(ans), "Generics_CrazyGambler_MegaDeal_No%s_Ans", PyString_AsString(r));
		Py_DECREF(r);
		Script_Call("Speech", "OnSpch", "(ss)", "Generics_CrazyGambler_MegaDeal_No_Msg", ans);
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Yes_")) {
		const char *lib = SaveVar_Get("SuperDeal.Library");
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%s_Yes_Ans", lib ? lib : "");
		Script_Call("Speech", "OnSpch", "(ss)", msg, ans);
		Script_Call("Speech", "StdAnsLst", "([ss])", "Generics_CrazyGambler_MegaDeal_Start_",
			"Generics_CrazyGambler_MegaDeal_Abort_");
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Start_")) {
		Script_Call("Menu", "MovieScorer", "(i)", 0);
		Input_SelectActionSetNamed("Inactive");
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		Script_Call("Speech", "OnSpch", "(ss)", msg, ans);
		Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("CrazyGambler", "StartSuperDeal"));
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Abort_")) {
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		Script_Call("Speech", "OnSpch", "(ss)", msg, ans);
		CrazyGambler_Options();
		CrazyGambler_FirstOption(ch);
	} else {
		Script_Call("CharsNPC", "GenericNPCSpeech", "(sssO)", usr, ch, opt, full);
	}
	return Py_ReturnNone();
}

/* the end of the speech that starts the mega deal */
static PyObject *CrazyGambler_StartSuperDeal(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Script_Call("MissionsFuncs", "StartActualSuperDeal", "()");
	return Py_ReturnNone();
}

PyMethodDef g_CrazyGamblerFuncs[] = {
	{ "CalculateStatus", CrazyGambler_CalculateStatus, 1, NULL },
	{ "CrazyGamblerSpeech", CrazyGambler_CrazyGamblerSpeech, 1, NULL },
	{ "StartSuperDeal", CrazyGambler_StartSuperDeal, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
