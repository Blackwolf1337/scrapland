/*
 * The gameplay functions of the character scripts in C (issue #2, step 6e): action presses, the special effects of a
 * character and its speech routines. Their Init and callbacks are data (Characters.c, data/chars/); these are logic
 * on entity variables and module state, so they are C. Each table goes into its module next to the one of
 * Characters.c (script_modules in Script.c). Module variables other scripts may read or change (Functionary.PressOk,
 * CrazyGambler.CDcompl, speedFactor and the like) stay variables of the module.
 *
 * The functions are native C (docs/objlib-plan.md, stage 2): they work on C texts and values and on the entity
 * variables in C. The module variables are read where the port read them (Script_GetInt, Script_GetFloat and
 * Script_GetString forms, errors dropped) and written through at once with the port's dropped errors (OBJLIB 5).
 * The lists of CharsNPC that its functions hand over (optionsList, the result of ClassifyOptions) keep their identity
 * (OBJLIB 5). The other ports (Speech, CharsNPC, Challenge, Menu, MissionsFuncs, SaveGame) are called by name
 * (OBJLIB 3); the engine functions the port called by name go through their seams (OBJLIB 7: SetMsgText,
 * CreateEntity, Scrap.Set, SetMoney, SetAlarm) or to the engine in C where no case replaces them (GetMoney, Get).
 * The adapters parse the arguments as before.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"
#include "game/Script/levels/Levels.h"

/* ent.var = v as the port set it (Script_EntSet: the console line of the handle, no error) */
static void Char_Set(const char *ent, const char *var, const nativeVal_t *v) {
	if (!Native_EntSet(World_FindEntity(ent), ent, var, v)) {
		Mode_DropError();
	}
}

static void Char_SetText(const char *ent, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	Char_Set(ent, var, &v);
	Native_Free(&v);
}

static void Char_SetInt(const char *ent, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	Char_Set(ent, var, &v);
}

static void Char_SetNum(const char *ent, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	Char_Set(ent, var, &v);
}

/* module.var = a text as Script_SetVar sets it */
static void Char_ModText(const char *module, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	Mode_ModPut(module, var, &v);
	Native_Free(&v);
}

/* module.var = src.name (the attribute as it is, a function of the module: nothing when it is not there) */
static void Char_ModAttr(const char *module, const char *var, const char *src, const char *name) {
	nativeVal_t v = { 0 };
	if (LvN_ModGet(src, name, &v)) {
		Mode_ModPut(module, var, &v);
	} else {
		Mode_DropError();
	}
	Native_Free(&v);
}

/* Speech.PlayerName, Speech.NPCName: who talks in the speech that runs */
static void Char_Speakers(const char *usr, const char *ch) {
	Char_ModText("Speech", "PlayerName", usr);
	Char_ModText("Speech", "NPCName", ch);
}

/* Speech.OnEndOfSpeechParams = (name,) */
static void Char_EndParams(const nativeVal_t *v) {
	nativeVal_t t;
	Native_SetEmpty(&t, NV_TUPLE);
	Native_Copy(Native_Append(&t), v);
	Mode_ModPut("Speech", "OnEndOfSpeechParams", &t);
	Native_Free(&t);
}

static void Char_EndParamsS(const char *name) {
	nativeVal_t v;
	Native_SetText(&v, name, (int)strlen(name));
	Char_EndParams(&v);
	Native_Free(&v);
}

/* Speech.OnSpch(msg, ans) as a line of a script, and `if Speech.OnSpch(msg, ans):` (1 or 0, -1 with the error) */
static byte Char_OnSpch(const char *msg, const char *ans) {
	return Native_CallMod("Speech", "OnSpch", NULL, "ss", msg, ans);
}

static int Char_Spch(const char *msg, const char *ans) {
	nativeVal_t r = { 0 };
	int ok = Native_CallMod("Speech", "OnSpch", &r, "ss", msg, ans) ? Native_True(&r) : -1;
	Native_Free(&r);
	return ok;
}

/* CharsNPC.GenericNPCSpeech(usr, ch, opt, full) */
static byte Char_Generic(const char *usr, const char *ch, const char *opt, const nativeVal_t *full) {
	return Native_CallMod("CharsNPC", "GenericNPCSpeech", NULL, "sssv", usr, ch, opt, full);
}

/* Scrap.GetEntity(name) of a C name: the handle value, None when there is no such entity */
static void Char_Handle(const char *name, nativeVal_t *out) {
	Native_SetNone(out);
	Mode_EntHandle(World_FindEntity(name), out);
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

/* the adapter of a function on the entity name (PyArg "s", the further n arguments any values the C does not read) */
#define CHAR_FN(pre, name, fmt, call) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		char *ent; \
		PyObject *x1, *x2; \
		(void)self; \
		if (!PyArg_ParseTuple(args, fmt, &ent, &x1, &x2)) { \
			return NULL; \
		} \
		return Mode_Result(call); \
	}

/* the adapter of a speech routine (usr, ch, opt, full): "sssO" */
#define CHAR_SPEECH(pre, name, core) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		char *usr, *ch, *opt; \
		PyObject *o; \
		nativeVal_t full; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "sssO", &usr, &ch, &opt, &o)) { \
			return NULL; \
		} \
		Native_Arg(o, &full); \
		byte ok = core(usr, ch, opt, &full); \
		Native_Free(&full); \
		return Mode_Result(ok); \
	}

/* SScorer.SetMsgText(Scrap.GetLangStr("INSPECT_CHAR_CantTalk"), Scrap.GetTime() + 2): the speech of a character that
 * does not talk */
static byte Char_CantTalkC(void) {
	nativeVal_t text;
	Mode_Lang("INSPECT_CHAR_CantTalk", &text);
	byte ok = Mode_MsgText(&text, (double)Timer_GetTime(&g_pWorld->timer) + 2.0);
	Native_Free(&text);
	return ok;
}

MODE_FN(Char, CantTalk, 4, Char_CantTalkC())

/* ---- Functionary: the slow motion of his special action, while the energy bar empties ---- */

static void Functionary_Enter(const char *name) {
	nativeVal_t v = { 0 };
	if (LvN_ModGet("Functionary", "charSpeed", &v)) {
		Char_Set(name, "TimeSpeed", &v);
	}
	Mode_DropError();
	Native_Free(&v);
	Timer_SetSpeed(&g_pWorld->timer, (float)Mode_ModGetN("Functionary", "worldSpeed", 1.0));
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
	double speed = Mode_ModGetN("Functionary", "speedFactor", 1.0);
	Char_SetNum(name, "EnergyBarConstantDecr", 0.0);
	Char_SetText(name, "OnEnergyBarFull", "");
	Char_SetCls("Functionary", "FOVInterpTime", 0.5 * speed);
	Char_SetInt(name, "TimeSpeed", 1);
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

/* the player presses the action: slow motion when there is energy enough, the bar empties in 60 s */
static byte Functionary_Press(const char *name) {
	Mode_ModPutI("Functionary", "PressOk", 0);
	if (Script_EntNum(name, "UsrControl", -1) == 0 && Script_EntNum(name, "ActionPriority", -1) == 0 &&
		Script_EntNum(name, "Energy", 0) >= GVar_GetNum("FunctionarySpecialActionLow", 0)) {
		double speed = Mode_ModGetN("Functionary", "speedFactor", 1.0);
		Char_SetNum(name, "EnergyBarConstantDecr", 60.0 * speed);
		Char_SetText(name, "OnEnergyBarFull", "Functionary.ExitFastEffect");
		Functionary_Enter(Script_EntStr(name, "Name", name));
		Mode_ModPutI("Functionary", "PressOk", 1);
	}
	return 1;
}

static byte Functionary_Release(const char *name) {
	if (Script_EntNum(name, "UsrControl", -1) == 0) {
		Char_SetNum(name, "EnergyBarConstantDecr", 0.0);
		Char_SetText(name, "OnEnergyBarFull", "");
		if (Script_EntNum(name, "Energy", 0) != 0.0 && Mode_ModGetI("Functionary", "PressOk", 0)) {
			Functionary_Exit(Script_EntStr(name, "Name", name));
		}
	}
	return 1;
}

CHAR_FN(Functionary, EnterFastEffect, "s", (Functionary_Enter(ent), 1))
/* also the OnEnergyBarFull of the entity while the action runs */
CHAR_FN(Functionary, ExitFastEffect, "s", (Functionary_Exit(ent), 1))
CHAR_FN(Functionary, ActionPress, "s", Functionary_Press(ent))
CHAR_FN(Functionary, ActionRelease, "s", Functionary_Release(ent))

PyMethodDef g_FunctionaryFuncs[] = {
	{ "EnterFastEffect", Functionary_EnterFastEffect, 1, NULL },
	{ "ExitFastEffect", Functionary_ExitFastEffect, 1, NULL },
	{ "ActionPress", Functionary_ActionPress, 1, NULL },
	{ "ActionRelease", Functionary_ActionRelease, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Rusty: his action presses do nothing (the class names them) ---- */

PyMethodDef g_RustyFuncs[] = {
	{ "ActionPress", Mode_PyNone, 1, NULL },
	{ "ActionRelease", Mode_PyNone, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Boss ---- */

/* his action, unless the player falls or does another */
static byte Boss_Press(const char *name) {
	const char *usr = Char_Usr0();
	if (!usr) {
		/* the script reads Scrap.UsrEntity(0).isFalling */
		return Native_Raise(NE_ATTRIBUTE, "isFalling");
	}
	if (!Script_EntNum(usr, "isFalling", 0) && Script_EntNum(usr, "ActionPriority", -1) == 0) {
		Char_SetText(name, "Action", "Action");
	}
	return 1;
}

static byte Boss_Speech(const char *usr, const char *ch, const char *opt, const nativeVal_t *full) {
	(void)full;
	Char_Speakers(usr, ch);
	if (!strcmp(opt, "Talk")) {
		if (!Char_OnSpch("Generics_Boss_InitMsg1", "Generics_Boss_InitAns1")) {
			return 0;
		}
		Char_ModAttr("Speech", "OnEndOfSpeech", "Speech", "RestoreState");
		Char_EndParamsS(ch);
	}
	return 1;
}

CHAR_FN(Boss, ActionPress, "s", Boss_Press(ent))
CHAR_SPEECH(Boss, BossSpeech, Boss_Speech)

PyMethodDef g_BossFuncs[] = {
	{ "ActionPress", Boss_ActionPress, 1, NULL },
	{ "BossSpeech", Boss_BossSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mayor: the action loops while the key is held ---- */

static byte Mayor_Press(const char *name) {
	Char_SetText(name, "Action", "ActionInit");
	if (!strcmp(Script_EntStr(name, "Action", ""), "ActionInit")) {
		Char_SetText(name, "DefLoop", "Action");
	}
	return 1;
}

/* an event of his action: again while the key is held */
static byte Mayor_ReCheck(const char *name) {
	if (Script_EntNum(name, "ActionPress", 0)) {
		Char_SetText(name, "ForceAction", "ActionInit");
		Char_SetText(name, "ForceDefLoop", "Action");
	}
	return 1;
}

CHAR_FN(Mayor, MayorActionPress, "s", Mayor_Press(ent))
CHAR_FN(Mayor, MayorReCheckAction, "sOO", Mayor_ReCheck(ent))

PyMethodDef g_MayorFuncs[] = {
	{ "MayorActionPress", Mayor_MayorActionPress, 1, NULL },
	{ "MayorReCheckAction", Mayor_MayorReCheckAction, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Maintenance, SputnikJunkyard: they do not talk ---- */

/* the event of his work: the effect between him and the ship of the hangar */
static byte Maintenance_WorkFX(const char *name) {
	nativeVal_t ship = { 0 }, shipName = { 0 };
	byte ok = LvN_ModGet("HangarTab", "prevShip", &ship) && Mode_VObj(&ship, "Name", &shipName) &&
		(shipName.type == NV_STR || Native_Raise(NE_TYPE, "HangarTab.prevShip.Name"));
	if (ok) {
		cPy_Function f;
		cPy_Function_ctor(&f);
		cPy_Function_SetFunc(&f, "!FXMaintenanceWork");
		cPy_Function_AddParamS(&f, name);
		cPy_Function_AddParamS(&f, Native_Text(&shipName));
		cPy_Function_AddParamF(&f, (float)Scrap_RandRange(0.5, 0.9));
		cPy_Function_Execute(&f, 0);
		cPy_Function_dtor(&f);
	}
	Native_Free(&shipName);
	Native_Free(&ship);
	return ok;
}

CHAR_FN(Maintenance, MaintenanceWorkFX, "sOO", Maintenance_WorkFX(ent))

PyMethodDef g_MaintenanceFuncs[] = {
	{ "MaintenanceTalk", Char_CantTalk, 1, NULL },
	{ "MaintenanceWorkFX", Maintenance_MaintenanceWorkFX, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_SputnikJunkyardFuncs[] = {
	{ "SputnikJunkyardTalk", Char_CantTalk, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- CharsNPC.optionsList: the options of the talk that runs (OBJLIB 5: the list object itself) ---- */

/* CharsNPC.optionsList as a value (its AttributeError when it is not there) */
static byte Char_Options(nativeVal_t *out) {
	return LvN_ModGet("CharsNPC", "optionsList", out);
}

/* CharsNPC.optionsList.insert(0, text) as the port did it (PyList_Insert: the SystemError of a value that is no list) */
static byte Char_OptionsFirst(const char *text) {
	PyObject *list = Script_Attr("CharsNPC", "optionsList"), *item = list ? PyString_FromString(text) : NULL;
	int r = item ? PyList_Insert(list, 0, item) : -1;
	Py_XDECREF(item);
	Py_XDECREF(list);
	return r == 0;
}

/* CharsNPC.optionsList, CharsNPC.subOptionsList = lists[0], lists[1] of a result of CharsNPC.ClassifyOptions (the
 * objects themselves, Script_SetVar's dropped errors): 0 with the error of an item */
static byte Char_SetOptionLists(PyObject *lists) {
	PyObject *a = PySequence_GetItem(lists, 0), *b = PySequence_GetItem(lists, 1);
	byte ok = a && b;
	if (ok) {
		if (!Mode_SetModPy("CharsNPC", "optionsList", a) || !Mode_SetModPy("CharsNPC", "subOptionsList", b)) {
			Mode_DropError();
		}
	}
	Py_XDECREF(a);
	Py_XDECREF(b);
	return ok;
}

/* Speech.StdAnsLst(CharsNPC.optionsList): 0 with the error set */
static byte Char_StdOptions(void) {
	nativeVal_t list = { 0 };
	byte ok = Char_Options(&list) && Native_CallMod("Speech", "StdAnsLst", NULL, "v", &list);
	Native_Free(&list);
	return ok;
}

/* Challenge.AddChallenge(ch, usr, n) (when the player has no infinite lives unless always): 0 with the error set */
static byte Char_Challenge(const char *ch, const char *usr, int n, byte always) {
	const char *lives = SaveVar_Get("Player.InfiniteLives");
	byte ok = 1;
	if (always || (lives && !strcmp(lives, "0"))) {
		nativeVal_t e;
		Char_Handle(ch, &e);
		ok = Native_CallMod("Challenge", "AddChallenge", NULL, "vsi", &e, usr, (long)n);
		Native_Free(&e);
	}
	return ok;
}

/* ---- CrazyGambler: the mega deal once the three crazy deals are done ---- */

/* CDcompl: how many of the three crazy deals are complete (MissionsFuncs.CrazyDealStatus(i): [.., done, needed]) */
static int CrazyGambler_Status(void) {
	int done = 0;
	Mode_ModPutI("CrazyGambler", "CDcompl", 0);
	for (int i = 1; i <= 3; i++) {
		nativeVal_t cd = { 0 }, ta, tb;
		const nativeVal_t *have = NULL, *need = NULL;
		if (Native_CallMod("MissionsFuncs", "CrazyDealStatus", &cd, "i", (long)i)) {
			Mode_Elems(&cd);
			have = Native_GetItem(&cd, 1, 0, &ta);
			need = Native_GetItem(&cd, 2, 0, &tb);
		}
		byte ok = have && need;
		if (ok && Native_Compare(have, need) >= 0) {
			Mode_ModPutI("CrazyGambler", "CDcompl", ++done);
		}
		if (have == &ta) {
			Native_Free(&ta);
		}
		if (need == &tb) {
			Native_Free(&tb);
		}
		Native_Free(&cd);
		if (!ok) {
			return -1;
		}
	}
	return done;
}

/* ArrnOpt > 1: ArriOpt = 1 (the arrow on the first option) */
static void CrazyGambler_FirstOption(const char *ch) {
	if (Script_EntNum(ch, "ArrnOpt", 0) > 1) {
		Char_SetInt(ch, "ArriOpt", 1);
	}
}

/* the talk of the mega deal when the jack in the box is active: the option of the deal first, then the options and
 * the challenge */
static byte CrazyGambler_MegaOptions(const char *usr, const char *ch) {
	int done = CrazyGambler_Status();
	nativeVal_t list = { 0 };
	if (done < 0 || !Char_OptionsFirst(done == 3 ? "Generics_CrazyGambler_MegaDeal_Yes_" : "Generics_CrazyGambler_MegaDeal_No_") ||
		!Char_Options(&list)) {
		return 0;
	}
	byte ok = Native_CallMod("Speech", "StdAnsLst", NULL, "v", &list);
	if (ok) {
		int n = 0;
		Mode_Elems(&list);
		Native_Len(&list, &n);
		ok = Char_Challenge(ch, usr, n, 0);
	}
	Native_Free(&list);
	return ok;
}

static byte CrazyGambler_Speech(const char *usr, const char *ch, const char *opt, const nativeVal_t *full) {
	char msg[0x80], ans[0x80];
	if (!strcmp(opt, "Talk")) {
		if (!Char_Generic(usr, ch, opt, full)) {
			return 0;
		}
		const char *active = SaveVar_Get("JackInTheBox.Active");
		return !active || strcmp(active, "1") || CrazyGambler_MegaOptions(usr, ch);
	}
	if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_No_")) {
		nativeVal_t n = { 0 }, r;
		CrazyGambler_FirstOption(ch);
		if (!LvN_ModGet("CrazyGambler", "CDcompl", &n)) {
			return 0;
		}
		Mode_Repr(&n, &r);
		Str_Printf(ans, sizeof(ans), "Generics_CrazyGambler_MegaDeal_No%s_Ans", Native_Text(&r));
		Native_Free(&r);
		Native_Free(&n);
		return Char_OnSpch("Generics_CrazyGambler_MegaDeal_No_Msg", ans);
	}
	if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Yes_")) {
		static const char *const opts[] = { "Generics_CrazyGambler_MegaDeal_Start_", "Generics_CrazyGambler_MegaDeal_Abort_" };
		const char *lib = SaveVar_Get("SuperDeal.Library");
		nativeVal_t list;
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%s_Yes_Ans", lib ? lib : "");
		if (!Char_OnSpch(msg, ans)) {
			return 0;
		}
		Mode_Texts(&list, opts, 2);
		byte ok = Native_CallMod("Speech", "StdAnsLst", NULL, "v", &list);
		Native_Free(&list);
		return ok;
	}
	if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Start_")) {
		if (!Native_CallMod("Menu", "MovieScorer", NULL, "i", 0L)) {
			return 0;
		}
		Input_SelectActionSetNamed("Inactive");
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if (!Char_OnSpch(msg, ans)) {
			return 0;
		}
		Char_ModAttr("Speech", "OnEndOfSpeech", "CrazyGambler", "StartSuperDeal");
		return 1;
	}
	if (!strcmp(opt, "Generics_CrazyGambler_MegaDeal_Abort_")) {
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if (!Char_OnSpch(msg, ans) || !Char_StdOptions()) {
			return 0;
		}
		CrazyGambler_FirstOption(ch);
		return 1;
	}
	return Char_Generic(usr, ch, opt, full);
}

/* the end of the speech that starts the mega deal */
static PyObject *CrazyGambler_StartSuperDeal(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Native_CallMod("MissionsFuncs", "StartActualSuperDeal", NULL, ""));
}

static PyObject *CrazyGambler_CalculateStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(CrazyGambler_Status() >= 0);
}

CHAR_SPEECH(CrazyGambler, CrazyGamblerSpeech, CrazyGambler_Speech)

PyMethodDef g_CrazyGamblerFuncs[] = {
	{ "CalculateStatus", CrazyGambler_CalculateStatus, 1, NULL },
	{ "CrazyGamblerSpeech", CrazyGambler_CrazyGamblerSpeech, 1, NULL },
	{ "StartSuperDeal", CrazyGambler_StartSuperDeal, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Gear and Police: the siren on channel 1 (its actions are in their files) ---- */

/* an event: the siren ends when it runs */
static byte Char_AbortSirenC(const char *name) {
	if (Script_EntTrue(name, "ActChan1")) {
		Char_SetText(name, "Action", "SirenEnd");
	}
	return 1;
}

/* the event at the end of SirenInit (Gear) */
CHAR_FN(Gear, StartSirenLoop, "sOO", (Char_SetText(ent, "Action", "Siren"), 1))
CHAR_FN(Char, AbortSiren, "sOO", Char_AbortSirenC(ent))

PyMethodDef g_GearFuncs[] = {
	{ "StartSirenLoop", Gear_StartSirenLoop, 1, NULL },
	{ "AbortSiren", Char_AbortSiren, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Police: his speech, the bribe and the way to the main mission ---- */

/* Speech.StdAnsLst(CharsNPC.optionsList [+ ["Generics_PoliceWereIs"]]) (PySequence_Concat: a value that is no
 * sequence can't be concatenated); its length, -1 with the error set */
static int Police_Options(byte wereIs) {
	nativeVal_t list = { 0 }, all = { 0 };
	int n = -1;
	if (Char_Options(&list)) {
		Mode_Elems(&list);
		byte ok = 1;
		if (wereIs) {
			static const char *const more[] = { "Generics_PoliceWereIs" };
			nativeVal_t m;
			byte t = list.type;
			Mode_Texts(&m, more, 1);
			ok = t == NV_LIST || t == NV_TUPLE || t == NV_STR || t == NV_VEC ? Native_Arith('+', &list, &m, &all) :
				Native_Raise(NE_TYPE, "object can't be concatenated");
			Native_Free(&m);
		} else {
			Native_Copy(&all, &list);
		}
		/* the port called StdAnsLst also after a length that failed (its error left set) */
		if (ok) {
			byte len = Native_Len(&all, &n);
			if (!Native_CallMod("Speech", "StdAnsLst", NULL, "v", &all) || !len) {
				n = -1;
			}
		}
	}
	Native_Free(&all);
	Native_Free(&list);
	return n;
}

/* ResState and the actions while the speech of the bribe ends */
static void Police_Wait(const char *ch, int state) {
	Mode_ModPutI("Police", "ResState", state);
	Char_SetText(ch, "ForceAction", "FaceToTalk");
	Char_SetText(ch, "ForceDefLoop", "TalkListenNPC");
	Char_ModAttr("Speech", "OnEndOfSpeech", "Police", "OnEndOfSpeech");
}

/* the bribe is paid or refused for good: the police goes back to idle */
static void Police_Calm(const char *ch) {
	Char_SetInt(ch, "LLogicState", 0);
	Char_SetText(ch, "LLogicTarget", "");
	Char_SetText(ch, "ForceAction", "Relax");
	Char_SetText(ch, "ForceDefLoop", "Relax");
}

/* Challenge.challengeType, foeType, SetChallengeOptions(ch): 0 with the error set */
static byte Police_StartChallenge(const char *type, const char *ch) {
	nativeVal_t e;
	Char_ModText("Challenge", "challengeType", type);
	Char_ModText("Challenge", "foeType", "Police");
	Char_Handle(ch, &e);
	byte ok = Native_CallMod("Challenge", "SetChallengeOptions", NULL, "v", &e);
	Native_Free(&e);
	return ok;
}

/* the place of opt in CharsNPC.optionsList from 1 (PySequence_In and PySequence_Index), 0 when it is not there or
 * the list can not be searched (no error) */
static int Police_OptionIndex(const char *opt) {
	nativeVal_t list = { 0 }, key;
	int index = 0;
	Native_SetText(&key, opt, (int)strlen(opt));
	int in = Char_Options(&list) ? Mode_In(&list, &key) : -1;
	if (in > 0) {
		Mode_Elems(&list);
		for (int i = 0; !index && i < Native_ForCount(&list); i++) {
			nativeVal_t tmp;
			const nativeVal_t *x = Native_GetItem(&list, i, 0, &tmp);
			if (x && !Native_Compare(x, &key)) {
				index = i + 1;
			}
			if (x == &tmp) {
				Native_Free(&tmp);
			}
		}
	}
	Mode_DropError();
	Native_Free(&key);
	Native_Free(&list);
	return index;
}

/* the first talk: where the main mission is, or the greeting and the options (ClassifyOptions) */
static byte Police_Talk(const char *usr, const char *ch) {
	char ans[0x80];
	nativeVal_t e, state = { 0 };
	int r;
	if (!Native_CallMod("CharsNPC", "CheckActualEntity", NULL, "s", ch)) {
		return 0;
	}
	/* ch.LLogicState of the script: an AttributeError when there is no such character */
	Char_Handle(ch, &e);
	byte got = Mode_VObj(&e, "LLogicState", &state);
	nativeVal_t ten;
	Native_SetInt(&ten, 10);
	byte isTen = got && !Native_Compare(&state, &ten);
	Native_Free(&state);
	Native_Free(&e);
	if (!got) {
		return 0;
	}
	if (isTen) {
		return Char_OnSpch("Generics_PoliceWereIsGoInitMsg1", "Generics_PoliceWereIsGoInitAns1");
	}
	Str_Printf(ans, sizeof(ans), "Generics_Police_InitAns%d", (int)Scrap_RandRange(1.0, 3.9));
	if ((r = Char_Spch("Generics_Police_InitMsg1", ans)) <= 0) {
		return r == 0;
	}
	PyObject *f = Script_Attr("CharsNPC", "ClassifyOptions");
	PyObject *lists = f ? Native_CallObj(f, "s", "Police") : NULL;
	Py_XDECREF(f);
	byte ok = lists && Char_SetOptionLists(lists);
	Py_XDECREF(lists);
	if (!ok) {
		return 0;
	}
	int n = Police_Options(1);
	return n >= 0 && Char_Challenge(ch, usr, n, 0);
}

/* the bribe is paid when the player has the money */
static byte Police_GiveYes(const char *ch) {
	nativeVal_t money, price = { 0 }, rest = { 0 };
	int r;
	Native_SetInt(&money, Mode_GetMoney());
	if (!Native_GVarGet("PoliceStealQuanto", &price)) {
		return 0;
	}
	byte rich = Native_Compare(&money, &price) >= 0;
	if (rich) {
		nativeVal_t zero;
		Native_SetInt(&zero, 0);
		byte ok = Mode_Arith('-', &money, &price, &rest) && Mode_SetMoneySeam(&rest) && Mode_SetAlarm(&zero);
		Native_Free(&rest);
		Native_Free(&price);
		if (!ok) {
			return 0;
		}
		Mode_ModPutI("Police", "ResState", 1);
		Police_Calm(ch);
		if ((r = Char_Spch("Generics_PoliceBribeGiveYesAns", "Generics_PoliceBribeGiveYesMsg")) < 0) {
			return 0;
		}
		if (r) {
			Char_ModAttr("Speech", "OnEndOfSpeech", "Police", "OnEndOfSpeech");
		}
		return 1;
	}
	Native_Free(&price);
	if ((r = Char_Spch("Generics_PoliceBribeCantDoAns", "Generics_PoliceBribeCantDoMsg")) < 0) {
		return 0;
	}
	if (r) {
		Police_Wait(ch, 0);
	}
	return 1;
}

/* the bribe is refused: half the time the player answers and the police attacks */
static byte Police_GiveNo(const char *ch) {
	if (Scrap_RandRange(0, 100) < 50) {
		char npc[0x80], player[0x80];
		nativeVal_t me, e, end = { 0 }, res = { 0 };
		Mode_ModGetS("Speech", "NPCName", npc, sizeof(npc));
		Mode_ModGetS("Speech", "PlayerName", player, sizeof(player));
		Char_Handle(player, &me);
		Char_Handle(npc, &e);
		byte ok = LvN_ModGet("Police", "OnEndOfSpeech", &end) &&
			Native_CallMod("Speech", "PlayerAns", &res, "vvsv", &me, &e, "Generics_PoliceBribeGiveNoAns", &end);
		if (ok && Native_True(&res)) {
			Mode_ModPutI("Police", "ResState", -1);
			Mode_ModPut("Speech", "OnEndOfSpeech", &end);
		}
		Native_Free(&res);
		Native_Free(&end);
		Native_Free(&e);
		Native_Free(&me);
		return ok;
	}
	Police_Calm(ch);
	return Char_OnSpch("Generics_PoliceBribeGiveNoAns", "Generics_PoliceBribeGiveNoMsg");
}

static byte Police_Speech(const char *usr, const char *ch, const char *opt, const nativeVal_t *full) {
	char msg[0x80], ans[0x80];
	int r;
	Char_Speakers(usr, ch);
	int index = Police_OptionIndex(opt);
	if (!strcmp(opt, "Talk")) {
		return Police_Talk(usr, ch);
	}
	if (index > 0) {
		Str_Printf(msg, sizeof(msg), "%sMsg", opt);
		Str_Printf(ans, sizeof(ans), "%sAns", opt);
		if ((r = Char_Spch(msg, ans)) <= 0) {
			return r == 0;
		}
		if (!Native_CallMod("CharsNPC", "IncludeSubOptions", NULL, "s", opt)) {
			return 0;
		}
		int n = Police_Options(1);
		if (n < 0) {
			return 0;
		}
		Char_SetInt(ch, "ArriOpt", index);
		return Char_Challenge(ch, usr, n, 0);
	}
	if (!strcmp(opt, "Generics_PoliceWereIs")) {
		Char_SetInt(ch, "LLogicGotoMainMission", 1);
		Char_SetInt(ch, "LLogicState", 0);
		Char_SetInt(ch, "LLogicIdle", 1);
		if (Script_EntNum(ch, "LLogicPoliceResult", 0) == 0) {
			if (!Char_OnSpch("Generics_PoliceWereIsMsg", "Generics_PoliceWereIsNo")) {
				return 0;
			}
			Char_SetInt(ch, "ArriOpt", 0);
			return 1;
		}
		Char_ModAttr("Speech", "OnEndOfSpeech", "Police", "GoToMission");
		Char_EndParamsS(Script_EntStr(ch, "Name", ch));
		return Char_OnSpch("Generics_PoliceWereIsMsg", "Generics_PoliceWereIsYes") &&
			Native_CallMod("Speech", "RestoreState", NULL, "s", ch);
	}
	if (!strcmp(opt, "ChallengeRaceStdOpt0") || !strcmp(opt, "ChallengeCombatStdOpt0")) {
		const char *type = opt[9] == 'R' ? "Race" : "Combat";
		Str_Printf(msg, sizeof(msg), "Challenge%sPoliceOpt1", type);
		Str_Printf(ans, sizeof(ans), "Challenge%sPoliceMsg1", type);
		return (r = Char_Spch(msg, ans)) >= 0 && (!r || Police_StartChallenge(type, ch));
	}
	if (!strncmp(opt, "ChallengeRaceStdOpt", 19) && opt[19] >= '1' && opt[19] <= '3' && !opt[20]) {
		nativeVal_t e;
		Char_Handle(ch, &e);
		byte ok = Native_CallMod("Challenge", "OnChallengeOpt", NULL, "iv", (long)(opt[19] - '0'), &e);
		Native_Free(&e);
		return ok;
	}
	if (!strcmp(opt, "ChallengeRaceStdOpt4")) {
		if ((r = Char_Spch("ChallengeRaceStdOpt2", "ChallengeRacePoliceMsg3")) <= 0) {
			return r == 0;
		}
		int n = Police_Options(0);
		return n >= 0 && Char_Challenge(ch, usr, n, 1);
	}
	if (!strcmp(opt, "GiveWhy")) {
		if ((r = Char_Spch("Generics_PoliceBribeGiveWhyAns", "Generics_PoliceBribeGiveWhyMsg")) < 0) {
			return 0;
		}
		if (r) {
			Police_Wait(ch, 0);
		}
		return 1;
	}
	if (!strcmp(opt, "GiveYes")) {
		return Police_GiveYes(ch);
	}
	if (!strcmp(opt, "GiveNo")) {
		return Police_GiveNo(ch);
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, full, NR_STR);
	Con_Printf("Error PoliceSpeech( %s %s %s %s )\n", usr, ch, opt, b.p);
	Native_BufFree(&b);
	return Police_Speech(usr, ch, "Talk", full);
}

/* the end of the bribe speech, by ResState: paid (the steal effect), refused (the police attacks), or waiting */
static byte Police_EndOfSpeech(void) {
	char ch[0x80], me[0x80];
	Mode_ModGetS("Speech", "NPCName", ch, sizeof(ch));
	Mode_ModGetS("Speech", "PlayerName", me, sizeof(me));
	byte hasCh = World_FindEntity(ch) != NULL;
	long state = Mode_ModGetI("Police", "ResState", 0);
	if (state == 1) {
		if (hasCh) {
			Char_SetText(ch, "ForceAction", "ActionOK");
		}
		Char_Effect("!FXPoliceSteal", ch, Script_EntStr(me, "Name", me));
	} else if (state == -1) {
		if (hasCh) {
			Char_SetInt(ch, "LLogicPoliceResult", -1);
		}
		Char_SetText(me, "ForceAction", "Relax");
		Char_SetText(me, "ForceDefLoop", "Relax");
		Char_SetInt(me, "AutoSelect", 1);
	} else if (hasCh) {
		if (!strncmp(Script_EntStr(ch, "Action", ""), "Relax", 5)) {
			Char_SetInt(ch, "LLogicIdle", state);
			Char_SetInt(ch, "LLogicPoliceResult", state);
			Char_SetText(ch, "ForceAction", "ActionNPC");
			Char_SetText(ch, "ForceDefLoop", "ActionNPC");
		}
		const char *usr = Char_Usr0();
		if (!usr) {
			return Native_Raise(NE_ATTRIBUTE, "Name");
		}
		Char_SetText(ch, "SelectedEntity", usr);
	}
	return 1;
}

CHAR_FN(Police, GoToMission, "s", (Char_SetInt(ent, "LLogicGotoMainMission", 1), 1))
CHAR_SPEECH(Police, PoliceSpeech, Police_Speech)

static PyObject *Police_OnEndOfSpeech(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Police_EndOfSpeech());
}

PyMethodDef g_PoliceFuncs[] = {
	{ "AbortSiren", Char_AbortSiren, 1, NULL },
	{ "GoToMission", Police_GoToMission, 1, NULL },
	{ "PoliceSpeech", Police_PoliceSpeech, 1, NULL },
	{ "OnEndOfSpeech", Police_OnEndOfSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Bishop: he sells three lives for BishopLivePrice ---- */

/* int(Scrap.GetSaveVar("Player.NumLives")) (the text "" when there is none; the ValueError of int()) */
static byte Bishop_Lives(nativeVal_t *out) {
	const char *saved = SaveVar_Get("Player.NumLives");
	nativeVal_t text;
	Native_SetText(&text, saved ? saved : "", saved ? (int)strlen(saved) : 0);
	byte ok = Native_ToInt(&text, out);
	Native_Free(&text);
	return ok;
}

/* three lives more (up to MaxPlayerLives) for the price, and a quick save */
static byte Bishop_AddLifes(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lives = Mode_V(&t), *n = Mode_V(&t), *max = Mode_V(&t), *money = Mode_V(&t), *price = Mode_V(&t);
	nativeVal_t *rest = Mode_V(&t);
	if (!Bishop_Lives(lives) || !Mode_Arith('+', lives, Mode_I(&t, 3), n) || !Native_GVarGet("MaxPlayerLives", max)) {
		return Mode_Done(&t, 0);
	}
	LvN_SaveRepr("Player.NumLives", Native_Compare(n, max) > 0 ? max : n);
	Native_SetInt(money, Mode_GetMoney());
	return Mode_Done(&t, Native_GVarGet("BishopLivePrice", price) && Mode_Arith('-', money, price, rest) &&
		Mode_SetMoneySeam(rest) && Native_CallMod("SaveGame", "QWayPoint", NULL, ""));
}

/* the answer of the option of the lives: none left to buy, no money, or the sale after the speech */
static byte Bishop_Option(const char *ch, const char **ans) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lives = Mode_V(&t), *max = Mode_V(&t), *money = Mode_V(&t), *price = Mode_V(&t), *e = Mode_V(&t);
	*ans = NULL;
	if (!Char_Usr0()) {
		/* the script used ans without setting it */
		return Native_Raise(NE_NAME, "ans");
	}
	if (!Bishop_Lives(lives) || !Native_GVarGet("MaxPlayerLives", max)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(money, Mode_GetMoney());
	if (!Native_GVarGet("BishopLivePrice", price)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(lives, max) == 0) {
		*ans = "Generics_SpecialBishop_3Ans";
	} else if (Native_Compare(money, price) < 0) {
		*ans = "Generics_SpecialBishop_2Ans";
	} else {
		*ans = "Generics_SpecialBishop_1Ans";
		Char_ModAttr("Speech", "OnEndOfSpeech", "Bishop", "StartSellingLife");
		Char_Handle(ch, e);
		Char_EndParams(e);
	}
	return Mode_Done(&t, 1);
}

/* the option of the lives shows the price: "<n1>" in ArrOpt00 becomes `Scrap.Get("BishopLivePrice")` */
static byte Bishop_Price(const char *ch) {
	const char *opt0 = Script_EntStr(ch, "ArrOpt00", "");
	if (strncmp(opt0, "Generics_SpecialBishop_1:", 25)) {
		return 1;
	}
	nativeVal_t price = { 0 }, r;
	if (!Native_GVarGet("BishopLivePrice", &price)) {
		return 0;
	}
	Mode_Repr(&price, &r);
	Native_Free(&price);
	char out[0x200];
	int n = 0;
	for (const char *p = opt0; *p && n < (int)sizeof(out) - 1;) {
		if (!strncmp(p, "<n1>", 4)) {
			for (const char *q = Native_Text(&r); *q && n < (int)sizeof(out) - 1; q++) {
				out[n++] = *q;
			}
			p += 4;
		} else {
			out[n++] = *p++;
		}
	}
	out[n] = 0;
	Native_Free(&r);
	Char_SetText(ch, "ArrOpt00", out);
	return 1;
}

static byte Bishop_Speech(const char *usr, const char *ch, const char *opt, const nativeVal_t *full) {
	char msg[0x80];
	nativeVal_t e;
	Char_Handle(ch, &e);
	Char_Speakers(usr, ch);
	byte ok;
	if (!strcmp(opt, "Generics_SpecialBishop_1")) {
		const char *ans;
		ok = Bishop_Option(ch, &ans);
		if (ok) {
			Str_Printf(msg, sizeof(msg), "%sMsg", opt);
			ok = Native_CallMod("CharsNPC", "PlayOption", NULL, "ssssv", opt, msg, ans, usr, &e);
		}
	} else {
		ok = Char_Generic(usr, ch, opt, full);
	}
	Native_Free(&e);
	return ok && Bishop_Price(ch);
}

/* the end of the speech: he walks to the player to sell (e the entity handle). A Name that is no text gives NULL
 * without an exception, as the port (the caller's SystemError; OBJLIB 7) */
static byte Bishop_StartSelling(const nativeVal_t *e) {
	nativeVal_t name = { 0 };
	char ch[0x80];
	if (!Mode_VObj(e, "Name", &name) || name.type != NV_STR) {
		Native_Free(&name);
		return 0;
	}
	Str_Copy(ch, Native_Text(&name), sizeof(ch));
	Native_Free(&name);
	Char_SetText(ch, "Action", "SellLife");
	Char_SetText(ch, "DefLoop", "Relax");
	const char *usr = Char_Usr0();
	if (!usr) {
		return Native_Raise(NE_ATTRIBUTE, "Name");
	}
	Char_SetText(ch, "SelectedEntity", usr);
	return 1;
}

static PyObject *Bishop_AddLifesPy(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Bishop_AddLifes());
}

CHAR_SPEECH(Bishop, BishopSpeech, Bishop_Speech)
MODE_FN(Bishop, StartSellingLife, 1, Bishop_StartSelling(&a[0]))
/* SellLife(EntityName, EventName, Time): the event of the sale (the script does not use the entity name: any value) */
MODE_FN(Bishop, SellLife, 3, !Char_Usr0() || Bishop_AddLifes())

static byte Bishop_SellFX(const char *name) {
	const char *usr = Char_Usr0();
	if (usr) {
		Char_Effect("!FXBishopSellLife", name, usr);
	}
	return 1;
}

CHAR_FN(Bishop, FXBishopSellLife, "sOO", Bishop_SellFX(ent))

PyMethodDef g_BishopFuncs[] = {
	{ "AddLifes", Bishop_AddLifesPy, 1, NULL },
	{ "BishopSpeech", Bishop_BishopSpeech, 1, NULL },
	{ "StartSellingLife", Bishop_StartSellingLife, 1, NULL },
	{ "SellLife", Bishop_SellLife, 1, NULL },
	{ "FXBishopSellLife", Bishop_FXBishopSellLife, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Messenger: the data pack he carries (Messenger.MDP, MakeMDP stay module variables) ---- */

static byte Messenger_MakePack(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mdp = Mode_V(&t), *name = Mode_V(&t), *zero = Mode_V(&t);
	Native_SetInt(zero, 0);
	if (!LvN_Create(Mode_S(&t, "MessengerDataPack"), zero, zero, zero, "Item", mdp)) {
		return Mode_Done(&t, 0);
	}
	Mode_ModPut("Messenger", "MDP", mdp);
	return Mode_Done(&t, Mode_VModel(mdp, Mode_S(&t, "Models/Chars/Messenger/DataPack.M3D")) && LvN_SetI(mdp, "Radius", 250) &&
		Mode_VObj(mdp, "Name", name) && Native_GVarSet("MessengerDataPack", name));
}

/* the data pack at the node of its elevator, or none is made */
static byte Messenger_ObjInit(const nativeVal_t *elv) {
	nativeVal_t mdp = { 0 }, pos = { 0 };
	if (!LvN_ModGet("Messenger", "MDP", &mdp)) {
		return 0;
	}
	byte ok = 1;
	if (Native_True(&mdp)) {
		ok = Mode_VObj(elv, "NT_NodePos_DC_DataPack", &pos) && Mode_VSet(&mdp, "Pos", &pos);
	} else {
		Mode_ModPutI("Messenger", "MakeMDP", 0);
	}
	Native_Free(&pos);
	Native_Free(&mdp);
	return ok;
}

static PyObject *Messenger_MakeDataPack(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Messenger_MakePack());
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

MODE_FN(Messenger, ObjInitializator, 2, Messenger_ObjInit(&a[0]))

PyMethodDef g_MessengerFuncs[] = {
	{ "MakeDataPack", Messenger_MakeDataPack, 1, NULL },
	{ "DoorInitializator", Messenger_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Messenger_ObjInitializator, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
