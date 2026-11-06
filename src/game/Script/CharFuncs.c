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

/* Speech.PlayerName, Speech.NPCName: who talks in the speech that runs */
static void Char_Speakers(const char *usr, const char *ch) {
	Script_SetVar("Speech", "PlayerName", PyString_FromString(usr));
	Script_SetVar("Speech", "NPCName", PyString_FromString(ch));
}

/* `if Speech.OnSpch(msg, ans):` of a script: 1 or 0, -1 with the error left set */
static int Char_Spch(const char *msg, const char *ans) {
	PyObject *r = Script_Ask("Speech", "OnSpch", "(ss)", msg, ans);
	int ok = r ? PyObject_IsTrue(r) > 0 : -1;
	Py_XDECREF(r);
	return ok;
}

/* Scrap.GetEntity(name): the handle (a new reference, None when there is no such entity) */
static PyObject *Char_Handle(const char *name) {
	PyObject *e = name ? PyEntity_New(name) : NULL;
	if (!e) {
		Py_INCREF(Py_None);
		e = Py_None;
	}
	return e;
}

/* the name of the entity the player controls (NULL when there is none) */
static const char *Char_Usr0(void) {
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	return usr ? usr->name : NULL;
}

/* Scrap.SetCallFunc(func); Scrap.AddParams(a); Scrap.AddParams(b); Scrap.Execute(): an effect between two entities */
static void Char_Effect(const char *func, const char *a, const char *b) {
	cPy_Function f;
	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, func);
	cPy_Function_AddParamS(&f, a);
	cPy_Function_AddParamS(&f, b);
	cPy_Function_Execute(&f, 0);
	cPy_Function_dtor(&f);
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
	if (!Script_Try("SScorer", "SetMsgText", "(sd)", Script_Lang("INSPECT_CHAR_CantTalk"),
			(double)Timer_GetTime(&g_pWorld->timer) + 2.0)) {
		return NULL;
	}
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
	const char *usr = Char_Usr0();
	if (!usr) {
		/* the script reads Scrap.UsrEntity(0).isFalling */
		PyErr_SetString(PyExc_AttributeError, "isFalling");
		return NULL;
	}
	if (!Script_EntNum(usr, "isFalling", 0) && Script_EntNum(usr, "ActionPriority", -1) == 0) {
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
	Char_Speakers(usr, ch);
	if (!strcmp(opt, "Talk")) {
		if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_Boss_InitMsg1", "Generics_Boss_InitAns1")) {
			return NULL;
		}
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
	PyObject *shipName = ship ? Script_GetAttr(ship, "Name") : NULL;
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

/* Speech.StdAnsLst(CharsNPC.optionsList): 0 with the error set */
static byte CrazyGambler_Options(void) {
	PyObject *list = Script_Attr("CharsNPC", "optionsList");
	byte ok = list && Script_Try("Speech", "StdAnsLst", "(O)", list);
	Py_XDECREF(list);
	return ok;
}

static PyObject *CrazyGambler_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	char *usr, *ch, *opt, msg[0x80], ans[0x80];
	PyObject *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	if (!strcmp(opt, "Talk")) {
		if (!Script_Try("CharsNPC", "GenericNPCSpeech", "(sssO)", usr, ch, opt, full)) {
			return NULL;
		}
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
			byte ok = Script_Try("Speech", "StdAnsLst", "(O)", list);
			const char *lives = SaveVar_Get("Player.InfiniteLives");
			if (ok && lives && !strcmp(lives, "0")) {
				PyObject *e = Char_Handle(ch);
				ok = Script_Try("Challenge", "AddChallenge", "(Osi)", e, usr, PySequence_Length(list));
				Py_DECREF(e);
			}
			Py_DECREF(list);
			if (!ok) {
				return NULL;
			}
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
		if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_CrazyGambler_MegaDeal_No_Msg", ans)) {
			return NULL;
		}
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Yes_")) {
		const char *lib = SaveVar_Get("SuperDeal.Library");
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%s_Yes_Ans", lib ? lib : "");
		if (!Script_Try("Speech", "OnSpch", "(ss)", msg, ans) ||
			!Script_Try("Speech", "StdAnsLst", "([ss])", "Generics_CrazyGambler_MegaDeal_Start_",
				"Generics_CrazyGambler_MegaDeal_Abort_")) {
			return NULL;
		}
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Start_")) {
		if (!Script_Try("Menu", "MovieScorer", "(i)", 0)) {
			return NULL;
		}
		Input_SelectActionSetNamed("Inactive");
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if (!Script_Try("Speech", "OnSpch", "(ss)", msg, ans)) {
			return NULL;
		}
		Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("CrazyGambler", "StartSuperDeal"));
	} else if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Abort_")) {
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if (!Script_Try("Speech", "OnSpch", "(ss)", msg, ans) || !CrazyGambler_Options()) {
			return NULL;
		}
		CrazyGambler_FirstOption(ch);
	} else if (!Script_Try("CharsNPC", "GenericNPCSpeech", "(sssO)", usr, ch, opt, full)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* the end of the speech that starts the mega deal */
static PyObject *CrazyGambler_StartSuperDeal(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Script_Try("MissionsFuncs", "StartActualSuperDeal", "()") ? Py_ReturnNone() : NULL;
}

PyMethodDef g_CrazyGamblerFuncs[] = {
	{ "CalculateStatus", CrazyGambler_CalculateStatus, 1, NULL },
	{ "CrazyGamblerSpeech", CrazyGambler_CrazyGamblerSpeech, 1, NULL },
	{ "StartSuperDeal", CrazyGambler_StartSuperDeal, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Gear and Police: the siren on channel 1 (its actions are in their files) ---- */

/* the event at the end of SirenInit (Gear) */
static PyObject *Gear_StartSirenLoop(PyObject *self, PyObject *args) {
	char *name;
	PyObject *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "sOO", &name, &event, &time)) {
		return NULL;
	}
	Char_SetText(name, "Action", "Siren");
	return Py_ReturnNone();
}

/* an event: the siren ends when it runs */
static PyObject *Char_AbortSiren(PyObject *self, PyObject *args) {
	char *name;
	PyObject *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "sOO", &name, &event, &time)) {
		return NULL;
	}
	if (Script_EntTrue(name, "ActChan1")) {
		Char_SetText(name, "Action", "SirenEnd");
	}
	return Py_ReturnNone();
}

PyMethodDef g_GearFuncs[] = {
	{ "StartSirenLoop", Gear_StartSirenLoop, 1, NULL },
	{ "AbortSiren", Char_AbortSiren, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Police: his speech, the bribe and the way to the main mission ---- */

static PyObject *Police_GoToMission(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Script_EntSet(name, "LLogicGotoMainMission", PyInt_FromLong(1));
	return Py_ReturnNone();
}

/* Speech.StdAnsLst(CharsNPC.optionsList [+ ["Generics_PoliceWereIs"]]); its length, -1 with the error set */
static int Police_Options(byte wereIs) {
	PyObject *list = Script_Attr("CharsNPC", "optionsList");
	PyObject *more = list && wereIs ? Py_BuildValue("[s]", "Generics_PoliceWereIs") : NULL;
	PyObject *all = more ? PySequence_Concat(list, more) : list;
	Py_XDECREF(more);
	int n = all ? PySequence_Length(all) : -1;
	if (all && !Script_Try("Speech", "StdAnsLst", "(O)", all)) {
		n = -1;
	}
	if (all != list) {
		Py_XDECREF(all);
	}
	Py_XDECREF(list);
	return n;
}

/* Challenge.AddChallenge(ch, usr, n) (when the player has no infinite lives unless always): 0 with the error set */
static byte Police_Challenge(const char *ch, const char *usr, int n, byte always) {
	const char *lives = SaveVar_Get("Player.InfiniteLives");
	byte ok = 1;
	if (always || (lives && !strcmp(lives, "0"))) {
		PyObject *e = Char_Handle(ch);
		ok = Script_Try("Challenge", "AddChallenge", "(Osi)", e, usr, n);
		Py_DECREF(e);
	}
	return ok;
}

/* ResState and the actions while the speech of the bribe ends */
static void Police_Wait(const char *ch, int state) {
	Script_SetInt("Police", "ResState", state);
	Char_SetText(ch, "ForceAction", "FaceToTalk");
	Char_SetText(ch, "ForceDefLoop", "TalkListenNPC");
	Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("Police", "OnEndOfSpeech"));
}

/* the bribe is paid or refused for good: the police goes back to idle */
static void Police_Calm(const char *ch) {
	Script_EntSet(ch, "LLogicState", PyInt_FromLong(0));
	Char_SetText(ch, "LLogicTarget", "");
	Char_SetText(ch, "ForceAction", "Relax");
	Char_SetText(ch, "ForceDefLoop", "Relax");
}

/* Challenge.challengeType, foeType, SetChallengeOptions(ch): 0 with the error set */
static byte Police_StartChallenge(const char *type, const char *ch) {
	Script_SetVar("Challenge", "challengeType", PyString_FromString(type));
	Script_SetVar("Challenge", "foeType", PyString_FromString("Police"));
	PyObject *e = Char_Handle(ch);
	byte ok = Script_Try("Challenge", "SetChallengeOptions", "(O)", e);
	Py_DECREF(e);
	return ok;
}

static PyObject *Police_PoliceSpeech(PyObject *self, PyObject *args) {
	char *usr, *ch, *opt, msg[0x80], ans[0x80];
	PyObject *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Char_Speakers(usr, ch);
	PyObject *list = Script_Attr("CharsNPC", "optionsList");
	PyObject *key = PyString_FromString(opt);
	int listed = list && key ? PySequence_In(list, key) : 0;
	int index = listed > 0 ? PySequence_Index(list, key) + 1 : 0;
	Py_XDECREF(key);
	Py_XDECREF(list);
	PyErr_Clear();
	int r;
	if (!strcmp(opt, "Talk")) {
		if (!Script_Try("CharsNPC", "CheckActualEntity", "(s)", ch)) {
			return NULL;
		}
		/* ch.LLogicState of the script: an AttributeError when there is no such character */
		PyObject *e = Char_Handle(ch), *state = Script_GetAttr(e, "LLogicState");
		int ten = state ? Script_NumCmp(state, 10) == 0 : -1;
		Py_DECREF(e);
		Py_XDECREF(state);
		if (ten < 0) {
			return NULL;
		}
		if (ten) {
			if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_PoliceWereIsGoInitMsg1", "Generics_PoliceWereIsGoInitAns1")) {
				return NULL;
			}
		} else {
			Str_Printf(ans, sizeof(ans), "Generics_Police_InitAns%d", (int)Scrap_RandRange(1.0, 3.9));
			if ((r = Char_Spch("Generics_Police_InitMsg1", ans)) < 0) {
				return NULL;
			}
			if (r) {
				PyObject *lists = Script_Ask("CharsNPC", "ClassifyOptions", "(s)", "Police");
				PyObject *a = lists ? PySequence_GetItem(lists, 0) : NULL, *b = lists ? PySequence_GetItem(lists, 1) : NULL;
				Py_XDECREF(lists);
				if (!a || !b) {
					Py_XDECREF(a);
					Py_XDECREF(b);
					return NULL;
				}
				Script_SetVar("CharsNPC", "optionsList", a);
				Script_SetVar("CharsNPC", "subOptionsList", b);
				int n = Police_Options(1);
				if (n < 0 || !Police_Challenge(ch, usr, n, 0)) {
					return NULL;
				}
			}
		}
	} else if (listed > 0) {
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if ((r = Char_Spch(msg, ans)) < 0) {
			return NULL;
		}
		if (r) {
			if (!Script_Try("CharsNPC", "IncludeSubOptions", "(s)", opt)) {
				return NULL;
			}
			int n = Police_Options(1);
			if (n < 0) {
				return NULL;
			}
			Script_EntSet(ch, "ArriOpt", PyInt_FromLong(index));
			if (!Police_Challenge(ch, usr, n, 0)) {
				return NULL;
			}
		}
	} else if (!strcmp(opt, "Generics_PoliceWereIs")) {
		Script_EntSet(ch, "LLogicGotoMainMission", PyInt_FromLong(1));
		Script_EntSet(ch, "LLogicState", PyInt_FromLong(0));
		Script_EntSet(ch, "LLogicIdle", PyInt_FromLong(1));
		if (Script_EntNum(ch, "LLogicPoliceResult", 0) == 0) {
			if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_PoliceWereIsMsg", "Generics_PoliceWereIsNo")) {
				return NULL;
			}
			Script_EntSet(ch, "ArriOpt", PyInt_FromLong(0));
		} else {
			Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("Police", "GoToMission"));
			Script_SetVar("Speech", "OnEndOfSpeechParams", Py_BuildValue("(s)", Script_EntStr(ch, "Name", ch)));
			if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_PoliceWereIsMsg", "Generics_PoliceWereIsYes") ||
				!Script_Try("Speech", "RestoreState", "(s)", ch)) {
				return NULL;
			}
		}
	} else if (!strcmp(opt, "ChallengeRaceStdOpt0") || !strcmp(opt, "ChallengeCombatStdOpt0")) {
		const char *type = opt[9] == 'R' ? "Race" : "Combat";
		Str_Printf(msg, sizeof(msg), "Challenge%sPoliceOpt1", type);
		Str_Printf(ans, sizeof(ans), "Challenge%sPoliceMsg1", type);
		if ((r = Char_Spch(msg, ans)) < 0 || (r && !Police_StartChallenge(type, ch))) {
			return NULL;
		}
	} else if (!strncmp(opt, "ChallengeRaceStdOpt", 19) && opt[19] >= '1' && opt[19] <= '3' && !opt[20]) {
		PyObject *e = Char_Handle(ch);
		byte ok = Script_Try("Challenge", "OnChallengeOpt", "(iO)", opt[19] - '0', e);
		Py_DECREF(e);
		if (!ok) {
			return NULL;
		}
	} else if (!strcmp(opt, "ChallengeRaceStdOpt4")) {
		if ((r = Char_Spch("ChallengeRaceStdOpt2", "ChallengeRacePoliceMsg3")) < 0) {
			return NULL;
		}
		if (r) {
			int n = Police_Options(0);
			if (n < 0 || !Police_Challenge(ch, usr, n, 1)) {
				return NULL;
			}
		}
	} else if (!strcmp(opt, "GiveWhy")) {
		if ((r = Char_Spch("Generics_PoliceBribeGiveWhyAns", "Generics_PoliceBribeGiveWhyMsg")) < 0) {
			return NULL;
		}
		if (r) {
			Police_Wait(ch, 0);
		}
	} else if (!strcmp(opt, "GiveYes")) {
		PyObject *money = Script_Ask("Scrap", "GetMoney", "()");
		PyObject *price = money ? Script_Ask("Scrap", "Get", "(s)", "PoliceStealQuanto") : NULL;
		int rich = price ? PyObject_Compare(money, price) >= 0 : 0;
		PyObject *rest = price && rich ? PyNumber_Subtract(money, price) : NULL;
		Py_XDECREF(money);
		Py_XDECREF(price);
		if (!price || (rich && !rest)) {
			return NULL;
		}
		if (rich) {
			byte ok = Script_Try("Scrap", "SetMoney", "(O)", rest) && Script_Try("Scrap", "SetAlarm", "(i)", 0);
			Py_DECREF(rest);
			if (!ok) {
				return NULL;
			}
			Script_SetInt("Police", "ResState", 1);
			Police_Calm(ch);
			if ((r = Char_Spch("Generics_PoliceBribeGiveYesAns", "Generics_PoliceBribeGiveYesMsg")) < 0) {
				return NULL;
			}
			if (r) {
				Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("Police", "OnEndOfSpeech"));
			}
		} else {
			if ((r = Char_Spch("Generics_PoliceBribeCantDoAns", "Generics_PoliceBribeCantDoMsg")) < 0) {
				return NULL;
			}
			if (r) {
				Police_Wait(ch, 0);
			}
		}
	} else if (!strcmp(opt, "GiveNo")) {
		if (Scrap_RandRange(0, 100) < 50) {
			char npc[0x80], player[0x80];
			Script_GetString("Speech", "NPCName", npc, sizeof(npc));
			Script_GetString("Speech", "PlayerName", player, sizeof(player));
			PyObject *me = Char_Handle(player), *e = Char_Handle(npc);
			PyObject *end = Script_Attr("Police", "OnEndOfSpeech");
			PyObject *res = end ? Script_Ask("Speech", "PlayerAns", "(OOsO)", me, e, "Generics_PoliceBribeGiveNoAns", end) : NULL;
			Py_DECREF(me);
			Py_DECREF(e);
			if (!res) {
				Py_XDECREF(end);
				return NULL;
			}
			if (PyObject_IsTrue(res) > 0) {
				Script_SetInt("Police", "ResState", -1);
				Script_SetVar("Speech", "OnEndOfSpeech", end);
				end = NULL;
			}
			Py_DECREF(res);
			Py_XDECREF(end);
		} else {
			Police_Calm(ch);
			if (!Script_Try("Speech", "OnSpch", "(ss)", "Generics_PoliceBribeGiveNoAns", "Generics_PoliceBribeGiveNoMsg")) {
				return NULL;
			}
		}
	} else {
		PyObject *text = PyObject_Str(full);
		Con_Printf("Error PoliceSpeech( %s %s %s %s )\n", usr, ch, opt, text ? PyString_AsString(text) : "");
		Py_XDECREF(text);
		PyErr_Clear();
		PyObject *again = Py_BuildValue("(sssO)", usr, ch, "Talk", full);
		PyObject *r = again ? Police_PoliceSpeech(self, again) : NULL;
		Py_XDECREF(again);
		return r;
	}
	return Py_ReturnNone();
}

/* the end of the bribe speech, by ResState: paid (the steal effect), refused (the police attacks), or waiting */
static PyObject *Police_OnEndOfSpeech(PyObject *self, PyObject *args) {
	char ch[0x80], me[0x80];
	(void)self;
	(void)args;
	Script_GetString("Speech", "NPCName", ch, sizeof(ch));
	Script_GetString("Speech", "PlayerName", me, sizeof(me));
	byte hasCh = World_FindEntity(ch) != NULL;
	int state = Script_GetInt("Police", "ResState", 0);
	if (state == 1) {
		if (hasCh) {
			Char_SetText(ch, "ForceAction", "ActionOK");
		}
		Char_Effect("!FXPoliceSteal", ch, Script_EntStr(me, "Name", me));
	} else if (state == -1) {
		if (hasCh) {
			Script_EntSet(ch, "LLogicPoliceResult", PyInt_FromLong(-1));
		}
		Char_SetText(me, "ForceAction", "Relax");
		Char_SetText(me, "ForceDefLoop", "Relax");
		Script_EntSet(me, "AutoSelect", PyInt_FromLong(1));
	} else if (hasCh) {
		if (!strncmp(Script_EntStr(ch, "Action", ""), "Relax", 5)) {
			Script_EntSet(ch, "LLogicIdle", PyInt_FromLong(state));
			Script_EntSet(ch, "LLogicPoliceResult", PyInt_FromLong(state));
			Char_SetText(ch, "ForceAction", "ActionNPC");
			Char_SetText(ch, "ForceDefLoop", "ActionNPC");
		}
		const char *usr = Char_Usr0();
		if (!usr) {
			PyErr_SetString(PyExc_AttributeError, "Name");
			return NULL;
		}
		Char_SetText(ch, "SelectedEntity", usr);
	}
	return Py_ReturnNone();
}

PyMethodDef g_PoliceFuncs[] = {
	{ "AbortSiren", Char_AbortSiren, 1, NULL },
	{ "GoToMission", Police_GoToMission, 1, NULL },
	{ "PoliceSpeech", Police_PoliceSpeech, 1, NULL },
	{ "OnEndOfSpeech", Police_OnEndOfSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Bishop: he sells three lives for BishopLivePrice ---- */

/* three lives more (up to MaxPlayerLives) for the price, and a quick save */
static int Bishop_AddLifes(void) {
	const char *saved = SaveVar_Get("Player.NumLives");
	PyObject *text = PyString_FromString(saved ? saved : "");
	PyObject *lives = text ? PyNumber_Int(text) : NULL;
	Py_XDECREF(text);
	PyObject *three = PyInt_FromLong(3);
	PyObject *n = lives ? PyNumber_Add(lives, three) : NULL;
	Py_XDECREF(three);
	Py_XDECREF(lives);
	PyObject *max = n ? Script_Ask("Scrap", "Get", "(s)", "MaxPlayerLives") : NULL;
	if (!max) {
		Py_XDECREF(n);
		return -1;
	}
	if (PyObject_Compare(n, max) > 0) {
		Py_DECREF(n);
		Py_INCREF(max);
		n = max;
	}
	Py_DECREF(max);
	PyObject *r = PyObject_Repr(n);
	Py_DECREF(n);
	if (!r) {
		return -1;
	}
	SaveVar_Set("Player.NumLives", PyString_AsString(r));
	Py_DECREF(r);
	PyObject *money = Script_Ask("Scrap", "GetMoney", "()");
	PyObject *price = money ? Script_Ask("Scrap", "Get", "(s)", "BishopLivePrice") : NULL;
	PyObject *rest = price ? PyNumber_Subtract(money, price) : NULL;
	Py_XDECREF(money);
	Py_XDECREF(price);
	if (!rest) {
		return -1;
	}
	byte ok = Script_Try("Scrap", "SetMoney", "(O)", rest);
	Py_DECREF(rest);
	return ok && Script_Try("SaveGame", "QWayPoint", "()") ? 0 : -1;
}

static PyObject *Bishop_AddLifesPy(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Bishop_AddLifes() < 0 ? NULL : Py_ReturnNone();
}

static PyObject *Bishop_BishopSpeech(PyObject *self, PyObject *args) {
	char *usr, *ch, *opt, msg[0x80];
	PyObject *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	PyObject *e = Char_Handle(ch);
	Char_Speakers(usr, ch);
	if (!strcmp(opt, "Generics_SpecialBishop_1")) {
		const char *ans = NULL;
		if (Char_Usr0()) {
			const char *saved = SaveVar_Get("Player.NumLives");
			PyObject *text = PyString_FromString(saved ? saved : "");
			PyObject *lives = text ? PyNumber_Int(text) : NULL;
			PyObject *max = lives ? Script_Ask("Scrap", "Get", "(s)", "MaxPlayerLives") : NULL;
			PyObject *money = max ? Script_Ask("Scrap", "GetMoney", "()") : NULL;
			PyObject *price = money ? Script_Ask("Scrap", "Get", "(s)", "BishopLivePrice") : NULL;
			if (price) {
				if (PyObject_Compare(lives, max) == 0) {
					ans = "Generics_SpecialBishop_3Ans";
				} else if (PyObject_Compare(money, price) < 0) {
					ans = "Generics_SpecialBishop_2Ans";
				} else {
					ans = "Generics_SpecialBishop_1Ans";
					Script_SetVar("Speech", "OnEndOfSpeech", Script_Attr("Bishop", "StartSellingLife"));
					Script_SetVar("Speech", "OnEndOfSpeechParams", Py_BuildValue("(O)", e));
				}
			}
			Py_XDECREF(text);
			Py_XDECREF(lives);
			Py_XDECREF(max);
			Py_XDECREF(money);
			Py_XDECREF(price);
			if (!ans) {
				Py_DECREF(e);
				return NULL;
			}
		} else {
			/* the script used ans without setting it */
			PyErr_SetString(PyExc_NameError, "ans");
			Py_DECREF(e);
			return NULL;
		}
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		byte ok = Script_Try("CharsNPC", "PlayOption", "(ssssO)", opt, msg, ans, usr, e);
		Py_DECREF(e);
		if (!ok) {
			return NULL;
		}
	} else {
		Py_DECREF(e);
		if (!Script_Try("CharsNPC", "GenericNPCSpeech", "(sssO)", usr, ch, opt, full)) {
			return NULL;
		}
	}
	/* the option of the lives shows the price */
	const char *opt0 = Script_EntStr(ch, "ArrOpt00", "");
	if (!strncmp(opt0, "Generics_SpecialBishop_1:", 25)) {
		PyObject *price = Script_Ask("Scrap", "Get", "(s)", "BishopLivePrice");
		PyObject *r = price ? PyObject_Repr(price) : NULL;
		Py_XDECREF(price);
		if (!r) {
			return NULL;
		}
		char out[0x200];
		int n = 0;
		for (const char *p = opt0; *p && n < (int)sizeof(out) - 1;) {
			if (!strncmp(p, "<n1>", 4)) {
				for (const char *q = PyString_AsString(r); *q && n < (int)sizeof(out) - 1; q++) {
					out[n++] = *q;
				}
				p += 4;
			} else {
				out[n++] = *p++;
			}
		}
		out[n] = 0;
		Py_DECREF(r);
		Char_SetText(ch, "ArrOpt00", out);
	}
	return Py_ReturnNone();
}

/* the end of the speech: he walks to the player to sell (ch the entity handle) */
static PyObject *Bishop_StartSellingLife(PyObject *self, PyObject *args) {
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	PyObject *name = Script_GetAttr(e, "Name");
	if (!name || !PyString_Check(name)) {
		Py_XDECREF(name);
		return NULL;
	}
	char ch[0x80];
	Str_Copy(ch, PyString_AsString(name), sizeof(ch));
	Py_DECREF(name);
	Char_SetText(ch, "Action", "SellLife");
	Char_SetText(ch, "DefLoop", "Relax");
	const char *usr = Char_Usr0();
	if (!usr) {
		PyErr_SetString(PyExc_AttributeError, "Name");
		return NULL;
	}
	Char_SetText(ch, "SelectedEntity", usr);
	return Py_ReturnNone();
}

/* SellLife(EntityName, EventName, Time): the event of the sale (the script does not use the entity name: any value) */
static PyObject *Bishop_SellLife(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	if (Char_Usr0() && Bishop_AddLifes() < 0) {
		return NULL;
	}
	return Py_ReturnNone();
}

static PyObject *Bishop_FXBishopSellLife(PyObject *self, PyObject *args) {
	char *name;
	PyObject *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "sOO", &name, &event, &time)) {
		return NULL;
	}
	const char *usr = Char_Usr0();
	if (usr) {
		Char_Effect("!FXBishopSellLife", name, usr);
	}
	return Py_ReturnNone();
}

PyMethodDef g_BishopFuncs[] = {
	{ "AddLifes", Bishop_AddLifesPy, 1, NULL },
	{ "BishopSpeech", Bishop_BishopSpeech, 1, NULL },
	{ "StartSellingLife", Bishop_StartSellingLife, 1, NULL },
	{ "SellLife", Bishop_SellLife, 1, NULL },
	{ "FXBishopSellLife", Bishop_FXBishopSellLife, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Messenger: the data pack he carries (Messenger.MDP, MakeMDP stay module variables) ---- */

static PyObject *Messenger_MakeDataPack(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *mdp = Script_Ask("Scrap", "CreateEntity", "(siiis)", "MessengerDataPack", 0, 0, 0, "Item");
	if (!mdp) {
		return NULL;
	}
	Py_INCREF(mdp);
	Script_SetVar("Messenger", "MDP", mdp);
	PyObject *r = Script_Method(mdp, "SetModel", "(s)", "Models/Chars/Messenger/DataPack.M3D");
	int ok = r && Script_HSet(mdp, "Radius", PyInt_FromLong(250));
	Py_XDECREF(r);
	PyObject *name = ok ? Script_GetAttr(mdp, "Name") : NULL;
	Py_DECREF(mdp);
	if (!name) {
		return NULL;
	}
	byte ok2 = Script_Try("Scrap", "Set", "(sO)", "MessengerDataPack", name);
	Py_DECREF(name);
	return ok2 ? Py_ReturnNone() : NULL;
}

/* the teleport script asks the doors and objects of the level */
static PyObject *Messenger_DoorInitializator(PyObject *self, PyObject *args) {
	PyObject *cls, *def = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &cls, &def)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* the data pack at the node of its elevator, or none is made */
static PyObject *Messenger_ObjInitializator(PyObject *self, PyObject *args) {
	PyObject *elv, *info;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &info)) {
		return NULL;
	}
	PyObject *mdp = Script_Attr("Messenger", "MDP");
	if (!mdp) {
		return NULL;
	}
	int ok = 1;
	if (PyObject_IsTrue(mdp) > 0) {
		PyObject *pos = Script_GetAttr(elv, "NT_NodePos_DC_DataPack");
		ok = pos && Script_HSet(mdp, "Pos", pos);
	} else {
		Script_SetInt("Messenger", "MakeMDP", 0);
	}
	Py_DECREF(mdp);
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_MessengerFuncs[] = {
	{ "MakeDataPack", Messenger_MakeDataPack, 1, NULL },
	{ "DoorInitializator", Messenger_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Messenger_ObjInitializator, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
