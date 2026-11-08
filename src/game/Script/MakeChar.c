/*
 * The MakeChar module of the game in C (issue #2, step 6b): the helpers the character scripts build the action
 * classes of the characters with (relax, walk, fall, talk, morph, break loop, sit and turret actions, the class
 * defaults). They work on SAct's current class and action as the script did, with the same variable sets.
 * MaxSpeed, MinSpeed, RelaxRotLimit, DazedChars and the morph lists stay variables of the module: the character
 * scripts change them.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values and call the engine's action
 * classes in C. The module variables are read where the port read them (as numbers, Script_GetFloat, or by the
 * membership of a name, errors dropped) and written through at once (OBJLIB 5: the character scripts, Scene and
 * CharAct read and write them in the dict); DazedChars is changed in place. The adapters parse the arguments as
 * before (PyArg_ParseTuple and the keyword functions with the script's names).
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

/* ---- SAct of the script ---- */

static void MC_SetNum(const char *name, double d) {
	if (!g_pAuxAction) {
		Con_Debugf("Caution: SAct.SetAct(): AuxActClass not defined.\n");
		return;
	}
	int i = (int)d;
	cAction_Var(g_pAuxAction, name, 1, NULL, &d, &i);
}

static void MC_SetStr(const char *name, const char *text) {
	if (!g_pAuxAction) {
		Con_Debugf("Caution: SAct.SetAct(): AuxActClass not defined.\n");
		return;
	}
	char *s = (char *)text;
	cAction_Var(g_pAuxAction, name, 1, &s, NULL, NULL);
}

/* SAct.GetAct(name) as a number (0 when it is no number) */
static double MC_Get(const char *name) {
	char *s;
	double d = 0.0;
	int i = 0;
	if (!g_pAuxAction) {
		Con_Debugf("Caution: SAct.GetAct('%s'): AuxActClass not defined.\n", name);
		return 0.0;
	}
	switch (cAction_Var(g_pAuxAction, name, 0, &s, &d, &i)) {
	case ACTVAR_DOUBLE:
		return d;
	case ACTVAR_INT:
		return i;
	}
	Con_Debugf("Caution: SAct.GetAct('%s'): element not found.\n", name);
	return 0.0;
}

/* an event at a time with a name, a function and maybe discardable (-1 leaves it) */
static void MC_Event(double t, const char *name, int discardable, const char *func) {
	MC_SetNum("AddEventAtTime", t);
	MC_SetStr("EventName", name);
	if (discardable >= 0) {
		MC_SetNum("EventDiscardable", discardable);
	}
	MC_SetStr("EventFunc", func);
}

static void MC_ClsNum(const char *name, double d) {
	if (!g_pAuxActClass) {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		return;
	}
	int i = (int)d;
	cActClass_Var(g_pAuxActClass, name, 1, NULL, &d, &i);
}

static void MC_ClsStr(const char *name, const char *text) {
	if (!g_pAuxActClass) {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		return;
	}
	char *s = (char *)text;
	cActClass_Var(g_pAuxActClass, name, 1, &s, NULL, NULL);
}

static void MC_GetClass(const char *name) {
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, name);
}

static void MC_Create(const char *name) {
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.CreateAction(): AuxActClass not defined.\n");
		return;
	}
	g_pAuxAction = cActClass_CreateAction(g_pAuxActClass, name);
}

/* "Models/Chars/<char>/Anm/<char>_<anm>.M3D" */
static void MC_Anm(const char *ch, const char *anm) {
	char path[0x200];
	Str_Printf(path, sizeof(path), "Models/Chars/%s/Anm/%s_%s.M3D", ch, ch, anm);
	MC_SetStr("AnmFilePath", path);
}

/* a number variable of the module (MaxSpeed and the others the character scripts set) */
static double MC_Var(const char *name, double def) {
	return Mode_ModGetN("MakeChar", name, def);
}

/* the name is in a variable of module (a tuple or list of names: `name in var`, an error counts as not) */
static byte MC_InOf(const char *module, const char *list, const char *name) {
	nativeVal_t l = { 0 }, s;
	int r = -1;
	if (LvN_ModGet(module, list, &l)) {
		Native_SetText(&s, name, (int)strlen(name));
		r = Mode_In(&l, &s);
		Native_Free(&s);
	}
	if (r < 0) {
		Mode_DropError();
	}
	Native_Free(&l);
	return r > 0;
}

static byte MC_In(const char *list, const char *name) {
	return MC_InOf("MakeChar", list, name);
}

/* a number argument as the port read it: an int or a float, 0 for any other value */
static double MC_Num(const nativeVal_t *v) {
	return v->type == NV_INT ? (double)v->i : v->type == NV_FLOAT ? v->f : 0.0;
}

/* the same for an argument of an adapter (NULL: not given, def) */
static double MC_ArgNum(PyObject *o, double def) {
	nativeVal_t v;
	if (!o) {
		return def;
	}
	Native_Arg(o, &v);
	double d = MC_Num(&v);
	Native_Free(&v);
	return d;
}

/* ---- the helpers ---- */

static void MC_RelaxAction(const char *ch, double turn, const char *name, const char *anm, int loop) {
	MC_Create(name);
	MC_Anm(ch, anm);
	MC_SetNum("isLoop", loop);
	MC_SetNum("RotSpeed", turn);
	MC_SetNum("AngRotLimit", MC_Var("RelaxRotLimit", 90));
	MC_SetNum("UseDisp", 0);
	MC_SetNum("NodeModifier", -1);
	MC_SetNum("AngleModifierSpeed", 5.0);
}

static PyObject *MakeChar_CreateRelaxAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "turnspeed", "Name", "Anm", "Loop", NULL };
	char *ch, *name = "Relax", *anm = "Relax";
	PyObject *turn;
	int loop = 0;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|ssi", names, &ch, &turn, &name, &anm, &loop)) {
		return NULL;
	}
	MC_RelaxAction(ch, MC_ArgNum(turn, 0.0), name, anm, loop);
	return Py_ReturnNone();
}

static PyObject *MakeChar_AddRelaxActions(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "turnspeed", "nExtraRlx", NULL };
	char *ch, name[0x20];
	PyObject *turnObj;
	int extra = 2;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|i", names, &ch, &turnObj, &extra)) {
		return NULL;
	}
	double turn = MC_ArgNum(turnObj, 0.0);
	MC_GetClass(ch);
	MC_RelaxAction(ch, turn, "Relax", "Relax", 0);
	MC_SetNum("isLoop", 1);
	MC_SetNum("NodeModifier", 0);
	MC_SetNum("AngleModifierSpeed", 1.0);
	MC_ClsStr("DefAction", "Relax");
	for (int i = 0; i < extra; i++) {
		Str_Printf(name, sizeof(name), "Relax%d", i);
		MC_RelaxAction(ch, turn, name, name, 0);
	}
	if (turn == 0) {
		MC_RelaxAction(ch, 180, "Turn", "Turn", 0);
		MC_SetNum("NodeModifier", 0);
	}
	MC_Create("FaceToTalk");
	MC_Anm(ch, turn == 0 ? "Turn" : "Relax");
	MC_SetNum("isLoop", 0);
	MC_SetNum("PriorityLevel", 190);
	MC_SetNum("RotSpeed", 720);
	MC_SetNum("AngRotLimit", 720);
	MC_SetNum("UseDisp", 0);
	MC_SetNum("UseFlyDisp", 0);
	MC_SetNum("UseFlyFall", 0);
	MC_SetNum("NodeModifier", -1);
	MC_SetNum("AngleModifierSpeed", 5.0);
	MC_SetNum("AutoFaceSpeed", 10);
	MC_SetNum("AutoFaceOffsetH", 0);
	MC_SetNum("AutoFaceOffsetV", 0);
	MC_ClsStr("PhotoPress", "CharAct.ActionPhoto");
	if (!Mode_ModGetI("Init", "Outdoor", 0)) {
		MC_ClsStr("PossessionPress", "CharConversor.Possession");
	}
	return Py_ReturnNone();
}

/* DiscoverInit and Discover: the character saw the player and calls the police */
static PyObject *MakeChar_AddDiscover(PyObject *self, PyObject *args) {
	char *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &ch)) {
		return NULL;
	}
	for (int k = 0; k < 2; k++) {
		MC_Create(k ? "Discover" : "DiscoverInit");
		MC_Anm(ch, k ? "Discover" : "DiscoverInit");
		MC_SetNum("isLoop", k);
		MC_SetNum("RotSpeed", 360);
		MC_SetNum("AngRotLimit", 90);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("UseFlyDisp", 0);
		MC_SetNum("MaxSpeed", 1.0);
		MC_SetNum("MinSpeed", 1.0);
		MC_SetNum("NodeModifier", 0);
		MC_SetNum("AngleModifierSpeed", 5.0);
		MC_SetNum("UseFlyFall", 0);
		if (!k) {
			MC_Event(0, "0", -1, "!AnmSetDisplAng");
			MC_Event(0, "InitAcuse", -1, "!WarningPolice");
		} else {
			MC_Event(0, "Acuse", -1, "!WarningPolice");
		}
	}
	return Py_ReturnNone();
}

static void MC_Move(const char *ch, const char *action, const char *anm, double turn, double disp, int loop) {
	MC_Create(action);
	MC_Anm(ch, anm);
	MC_SetNum("isLoop", loop);
	MC_SetNum("RotSpeed", turn);
	MC_SetNum("AngRotLimit", MC_Var("RelaxRotLimit", 90));
	MC_SetNum("UseDisp", disp);
	MC_SetNum("MaxSpeed", MC_Var("MaxSpeed", 1.5));
	MC_SetNum("MinSpeed", MC_Var("MinSpeed", 0.8));
	MC_SetNum("NodeModifier", 0);
	MC_SetNum("AngleModifierSpeed", 5.0);
	MC_SetNum("UseFlyFall", 0);
}

static PyObject *MakeChar_AddMove(PyObject *self, PyObject *args) {
	char *ch, *action, *anm;
	PyObject *turn, *disp;
	int loop;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssOOi", &ch, &action, &anm, &turn, &disp, &loop)) {
		return NULL;
	}
	MC_Move(ch, action, anm, MC_ArgNum(turn, 0.0), MC_ArgNum(disp, 0.0), loop);
	return Py_ReturnNone();
}

/* the moves of a list (AddWalkActions and its Init and End kin), list NULL for the four of def; the list's length as
 * PySequence_Length takes it (its TypeError left set, as the port went on), items that are no text "" */
static void MC_WalkList(const char *ch, double turn, int isRun, double disp, nativeVal_t *list, const char *const *def,
	const char *run, int loop, byte end) {
	int n = 4;
	MC_GetClass(ch);
	if (list) {
		Mode_Elems(list);
		if (!Native_Len(list, &n)) {
			n = -1;
		}
	}
	for (int i = 0; i < n + (isRun ? 1 : 0); i++) {
		char act[0x40];
		if (i == n) {
			Str_Copy(act, run, sizeof(act));
		} else if (list) {
			nativeVal_t tmp;
			const nativeVal_t *e = Native_GetItem(list, i, 0, &tmp);
			Str_Copy(act, e && e->type == NV_STR ? Native_Text(e) : "", sizeof(act));
			if (e == &tmp) {
				Native_Free(&tmp);
			}
			Mode_DropError();
		} else {
			Str_Copy(act, def[i], sizeof(act));
		}
		MC_Move(ch, act, act, turn, disp, loop);
		if (end) {
			MC_SetNum("MinSpeed", MC_Var("MaxSpeed", 1.5));
		}
	}
}

static PyObject *MC_Walk(PyObject *args, PyObject *kw, const char *const *def, const char *run, int loop, byte end) {
	static char *names[] = { "CharName", "turnspeed", "isRun", "UseDisp", "ActList", NULL };
	char *ch;
	PyObject *turnObj = NULL, *dispObj = NULL, *listObj = NULL;
	int isRun = 1;
	nativeVal_t list;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|OiOO", names, &ch, &turnObj, &isRun, &dispObj, &listObj)) {
		return NULL;
	}
	if (listObj) {
		Native_Arg(listObj, &list);
	}
	MC_WalkList(ch, MC_ArgNum(turnObj, 360), isRun, MC_ArgNum(dispObj, 1), listObj ? &list : NULL, def, run, loop, end);
	if (listObj) {
		Native_Free(&list);
	}
	return Py_ReturnNone();
}

static PyObject *MakeChar_AddWalkActions(PyObject *self, PyObject *args, PyObject *kw) {
	static const char *const def[] = { "Walk", "Back", "StrLeft", "StrRight" };
	(void)self;
	return MC_Walk(args, kw, def, "Run", 1, 0);
}

static PyObject *MakeChar_AddInitWalkActions(PyObject *self, PyObject *args, PyObject *kw) {
	static const char *const def[] = { "WalkInit", "BackInit", "StrLeftInit", "StrRightInit" };
	(void)self;
	return MC_Walk(args, kw, def, "RunInit", 0, 0);
}

static PyObject *MakeChar_AddEndWalkActions(PyObject *self, PyObject *args, PyObject *kw) {
	static const char *const def[] = { "WalkEnd", "BackEnd", "StrLeftEnd", "StrRightEnd" };
	(void)self;
	return MC_Walk(args, kw, def, "RunEnd", 0, 1);
}

/* Fall, then FallInit and FallEnd when asked; only the first loops */
static PyObject *MakeChar_AddFallAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "turnspeed", "InitAction", "EndAction", NULL };
	char *ch;
	PyObject *turnObj = NULL;
	int init = 0, end = 0;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|Oii", names, &ch, &turnObj, &init, &end)) {
		return NULL;
	}
	const char *acts[3] = { "Fall", NULL, NULL };
	int n = 1;
	if (init) {
		acts[n++] = "FallInit";
	}
	if (end) {
		acts[n++] = "FallEnd";
	}
	MC_GetClass(ch);
	for (int i = 0; i < n; i++) {
		MC_Create(acts[i]);
		MC_Anm(ch, acts[i]);
		MC_SetNum("isLoop", i == 0);
		MC_SetNum("RotSpeed", MC_ArgNum(turnObj, 45));
		MC_SetNum("NodeModifier", -1);
		MC_SetNum("AngleModifierSpeed", 5.0);
	}
	return Py_ReturnNone();
}

/* the morph of the lips while the character talks */
static void MC_TalkMorph(const char *morph) {
	if (morph && *morph) {
		MC_Event(0, morph, -1, "!AnmSetAction");
		MC_Event(MC_Get("AnmTotalTime"), "3", 0, "!AnmStopChan");
	}
}

static void MC_TalkAct(const char *ch, const char *anm, double nodeMod, int loop, const char *extra, const char *morph) {
	char name[0x80], file[0x80];
	MC_GetClass(ch);
	Str_Printf(name, sizeof(name), "%s%s", anm, extra);
	Str_Printf(file, sizeof(file), "%s%s", anm, extra);
	for (int npc = 0; npc < 2; npc++) {
		if (npc) {
			Str_Printf(name, sizeof(name), "%sNPC%s", anm, extra);
		}
		MC_Create(name);
		MC_Anm(ch, file);
		MC_SetNum("isLoop", loop);
		MC_SetNum("RotSpeed", npc ? 180 : 0);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("UseFlyDisp", 0);
		MC_SetNum("UseFlyFall", 0);
		MC_SetNum("PriorityLevel", 190);
		MC_SetNum("AutoFaceSpeed", npc ? 30 : 0);
		MC_SetNum("AutoFaceOffsetH", 0);
		MC_SetNum("AutoFaceOffsetV", 0);
		MC_SetNum("NodeModifier", npc ? nodeMod : -1);
		MC_SetNum("AngleModifierSpeed", npc ? 1.0 : 1000.0);
		MC_TalkMorph(morph);
	}
}

static PyObject *MakeChar_AddTalkAct(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "AnmName", "NodeModifier", "isLoop", "ExtraAction", "MorphAnm", NULL };
	char *ch, *anm, *extra = "";
	PyObject *nodeMod = NULL, *morph = NULL;
	int loop = 1;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "ss|OisO", names, &ch, &anm, &nodeMod, &loop, &extra, &morph)) {
		return NULL;
	}
	nativeVal_t m;
	Native_SetNone(&m);
	if (morph) {
		Native_Arg(morph, &m);
	}
	MC_TalkAct(ch, anm, MC_ArgNum(nodeMod, 0), loop, extra, m.type == NV_STR ? Native_Text(&m) : NULL);
	Native_Free(&m);
	return Py_ReturnNone();
}

static void MC_TalkAction(const char *ch, double nodeMod, int extra) {
	const char *morph = MC_In("MorphLipsList", ch) ? "MorphLips" : NULL;
	MC_GetClass(ch);
	for (int k = 0; k < 2; k++) {
		MC_Create(k ? "TalkListenNPC" : "TalkListen");
		MC_Anm(ch, "Relax");
		MC_SetNum("isLoop", 1);
		MC_SetNum("RotSpeed", 0);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("UseFlyDisp", 0);
		MC_SetNum("UseFlyFall", 0);
		MC_SetNum("NodeModifier", -1);
		MC_SetNum("AngleModifierSpeed", 1.0);
		MC_SetNum("InterpTime", 0.4);
	}
	MC_SetNum("PriorityLevel", 1);
	MC_TalkAct(ch, "Talk", nodeMod, 1, "", morph);
	if (extra > 0) {
		/* the loop changes at its end to one of the extra animations */
		for (int k = 0; k < 2; k++) {
			char name[0x40];
			const char *ev = k ? "TalkNPC" : "Talk";
			Native_ActAction(ev);
			Str_Printf(name, sizeof(name), "%s:%d", ev, extra);
			MC_Event(MC_Get("AnmTotalTime"), name, -1, "!AnmChangeRandomAnm");
		}
		for (int i = 0; i < extra; i++) {
			char n[0x10];
			Str_Printf(n, sizeof(n), "%d", i);
			MC_TalkAct(ch, "Talk", nodeMod, 0, n, morph);
		}
	}
}

static PyObject *MakeChar_AddTalkAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "NodeModifier", "nExtraActions", NULL };
	char *ch;
	PyObject *nodeMod = NULL;
	int extra = 0;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|Oi", names, &ch, &nodeMod, &extra)) {
		return NULL;
	}
	MC_TalkAction(ch, MC_ArgNum(nodeMod, 0), extra);
	return Py_ReturnNone();
}

static PyObject *MakeChar_AddTalkMorph(PyObject *self, PyObject *args) {
	PyObject *morph;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &morph)) {
		return NULL;
	}
	nativeVal_t m;
	Native_Arg(morph, &m);
	MC_TalkMorph(Native_True(&m) && m.type == NV_STR ? Native_Text(&m) : NULL);
	Native_Free(&m);
	return Py_ReturnNone();
}

static void MC_Morph(const char *ch, const char *act, const char *file, int chan) {
	char path[0x200];
	MC_Create(act);
	Str_Printf(path, sizeof(path), "Models/Chars/%s/Anm/%s", ch, file);
	MC_SetStr("AnmFilePath", path);
	MC_SetNum("isLoop", 1);
	MC_SetNum("Chanel", chan);
}

static PyObject *MakeChar_AddMorphAction(PyObject *self, PyObject *args) {
	char *ch, *act, *file;
	int chan;
	(void)self;
	if (!PyArg_ParseTuple(args, "sssi", &ch, &act, &file, &chan)) {
		return NULL;
	}
	MC_Morph(ch, act, file, chan);
	return Py_ReturnNone();
}

/* the morphs of the eyes and the lips of the characters of the lists */
static void MC_DefMorphs(const char *ch) {
	char file[0x80];
	if (MC_In("MorphEyesList", ch)) {
		Str_Printf(file, sizeof(file), "%s_Morph_Eyes.M3D", ch);
		MC_Morph(ch, "MorphEyes", file, 2);
	}
	if (MC_In("MorphLipsList", ch)) {
		Str_Printf(file, sizeof(file), "%s_Morph_Lips.M3D", ch);
		MC_Morph(ch, "MorphLips", file, 3);
	}
}

static PyObject *MakeChar_AddDefMorphActions(PyObject *self, PyObject *args) {
	char *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &ch)) {
		return NULL;
	}
	MC_DefMorphs(ch);
	return Py_ReturnNone();
}

/* EventAnimCommand(time, name[, discard]): an animation command; a negative time is the end of the animation */
static PyObject *MakeChar_EventAnimCommand(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Time", "Name", "Discard", NULL };
	PyObject *t;
	char *name;
	int discard = 1;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "Os|i", names, &t, &name, &discard)) {
		return NULL;
	}
	double time = MC_ArgNum(t, 0);
	MC_Event(time < 0 ? MC_Get("AnmTotalTime") : time, name, discard, "!AnmCommand");
	return Py_ReturnNone();
}

/* a listening action: the event function at regular times, about twice a second */
static void MC_Listen(const char *ch, const char *act, const char *func, const char *anm) {
	MC_Create(act);
	MC_Anm(ch, anm);
	MC_SetNum("PriorityLevel", 1);
	MC_SetNum("isLoop", 1);
	MC_SetNum("RotSpeed", 720);
	MC_SetNum("AngRotLimit", 720);
	MC_SetNum("UseDisp", 0);
	MC_SetNum("UseFlyDisp", 0);
	MC_SetNum("UseFlyFall", 0);
	MC_SetNum("NodeModifier", -1);
	MC_SetNum("AngleModifierSpeed", 5.0);
	MC_SetNum("AutoFaceSpeed", 10);
	MC_SetNum("AutoFaceOffsetH", 0);
	MC_SetNum("AutoFaceOffsetV", 0);
	double total = MC_Get("AnmTotalTime");
	int n = (int)(total * 2) + 1;
	for (int i = 0; i < n; i++) {
		MC_Event(total * (i + 1) / n, func, -1, func);
	}
}

static PyObject *MakeChar_ListenAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "ActionName", "EventFunc", "AnmName", NULL };
	char *ch, *act, *func, *anm = "Relax";
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "sss|s", names, &ch, &act, &func, &anm)) {
		return NULL;
	}
	MC_Listen(ch, act, func, anm);
	return Py_ReturnNone();
}

/* <name>Init, <name> (loops) and <name>End */
static void MC_BreakLoop(const char *ch, const char *name, int priority) {
	char act[0x40];
	MC_GetClass(ch);
	for (int k = 0; k < 3; k++) {
		Str_Printf(act, sizeof(act), "%s%s", name, k == 0 ? "Init" : k == 1 ? "" : "End");
		MC_Create(act);
		MC_Anm(ch, act);
		MC_SetNum("isLoop", k == 1);
		MC_SetNum("RotSpeed", 0);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("UseFlyDisp", 0);
		MC_SetNum("PriorityLevel", priority);
		if (k == 1 && !strcmp(name, "Dazed")) {
			MC_Event(MC_Get("AnmTotalTime"), "FXCharacterDazed", -1, "!FXCharacterDazed");
		}
	}
}

static PyObject *MakeChar_AddBreakLoopAction(PyObject *self, PyObject *args) {
	char *ch, *name;
	int priority;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssi", &ch, &name, &priority)) {
		return NULL;
	}
	MC_BreakLoop(ch, name, priority);
	return Py_ReturnNone();
}

/* flashed by the camera: the end of each part leads to the next; the police listen */
static PyObject *MakeChar_AddFlashedAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "ListenPolice", NULL };
	char *ch;
	int listen = 1;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|i", names, &ch, &listen)) {
		return NULL;
	}
	MC_BreakLoop(ch, "Flashed", 201);
	Native_ActAction("FlashedInit");
	MC_Event(MC_Get("AnmTotalTime"), "Flashed", -1, "!AnmSetAction");
	Native_ActAction("Flashed");
	MC_Event(MC_Get("AnmTotalTime"), "FlashedEnd", -1, "!AnmSetAction");
	if (listen) {
		MC_Listen(ch, "PoliceListen", "!ListeningPolice", "Relax");
	}
	return Py_ReturnNone();
}

static PyObject *MakeChar_AddCrazyAction(PyObject *self, PyObject *args) {
	char *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &ch)) {
		return NULL;
	}
	MC_BreakLoop(ch, "Crazy", 1);
	return Py_ReturnNone();
}

/* dazed by the mayor: a coin falls a quarter into the start; the character goes into DazedChars */
static PyObject *MakeChar_AddDazeAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "ListenMayor", NULL };
	char *ch;
	int listen = 1;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|i", names, &ch, &listen)) {
		return NULL;
	}
	nativeVal_t s;
	Native_SetText(&s, ch, (int)strlen(ch));
	if (!Mode_ListAddG("MakeChar", "DazedChars", &s, 0)) {
		Mode_DropError();
	}
	Native_Free(&s);
	MC_BreakLoop(ch, "Dazed", 202);
	Native_ActAction("DazedInit");
	MC_Event(MC_Get("AnmTotalTime") / 4.0, "DazedMoney", -1, "MisItems.DazedCoin");
	if (listen) {
		MC_Listen(ch, "MayorListen", "!ListeningMayor", "Relax");
	}
	return Py_ReturnNone();
}

/* sitting at a desk: SitDown, GetUp, Work and WorkRelax */
static PyObject *MakeChar_SetSitActions(PyObject *self, PyObject *args) {
	static const struct {
		const char *name;
		int loop, priority;
	} acts[] = { { "SitDown", 0, 0 }, { "GetUp", 0, 1 }, { "Work", 1, 0 }, { "WorkRelax", 0, 0 } };
	char *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &ch)) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		MC_Create(acts[i].name);
		MC_Anm(ch, acts[i].name);
		MC_SetNum("isLoop", acts[i].loop);
		MC_SetNum("RotSpeed", 0);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("PriorityLevel", acts[i].priority);
	}
	return Py_ReturnNone();
}

/* InitChar(name): a new class with the defaults of a character, MaxSpeed and MinSpeed back to theirs */
static PyObject *MakeChar_InitChar(PyObject *self, PyObject *args) {
	char *ch, text[0x100], path[0x200];
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &ch)) {
		return NULL;
	}
	nativeVal_t v;
	Native_SetFloat(&v, 1.5);
	Mode_ModPut("MakeChar", "MaxSpeed", &v);
	Native_SetFloat(&v, 0.8);
	Mode_ModPut("MakeChar", "MinSpeed", &v);
	Str_Printf(text, sizeof(text), "Loading %s ...\n", ch);
	Con_Print(CON_COLOR_DEBUG, text, 0);
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Create(g_pActClassList, ch);
	Str_Printf(path, sizeof(path), "Models/Chars/%s/%s.M3D", ch, ch);
	MC_ClsStr("ModelFilePath", path);
	MC_ClsNum("TurnAngLimit", 45);
	MC_ClsNum("MinTimeToExtraRelax", 5);
	MC_ClsNum("MaxTimeToExtraRelax", 10);
	MC_ClsNum("MinTimeWalking", 0.3);
	MC_ClsNum("MinFallDist", 30);
	MC_ClsNum("FlyAccel", 600);
	MC_ClsNum("WRadius", 60);
	MC_ClsNum("VRadius", 60);
	MC_ClsNum("SRadius", 12);
	if (MC_Var("RelaxRotLimit", 90) > 90) {
		MC_ClsNum("StrafeChar", 0);
	}
	MC_ClsNum("FOV", 90);
	MC_ClsNum("CamDist", 200);
	MC_ClsNum("CamAng", 0);
	MC_ClsNum("CamRot", 0);
	if (!Mode_ModGetI("Init", "Outdoor", 0) && MC_InOf("CharConversor", "AvailableChars", ch)) {
		MC_ClsNum("CanBePossessed", 1);
	}
	MC_DefMorphs(ch);
	return Py_ReturnNone();
}

/* SetNodeRotation(set, node, x, y): the angle pair of a node in hundredths */
static void MC_NodeRotation(const nativeVal_t *set, const nativeVal_t *node, const nativeVal_t *x, const nativeVal_t *y) {
	nativeVal_t rs, rn;
	char name[0x40];
	Mode_Repr(set, &rs);
	Mode_Repr(node, &rn);
	Str_Printf(name, sizeof(name), "Node%s_%sx", Native_Text(&rn), Native_Text(&rs));
	MC_ClsNum(name, MC_Num(x) / 100.0);
	name[strlen(name) - 1] = 'y';
	MC_ClsNum(name, MC_Num(y) / 100.0);
	Native_Free(&rs);
	Native_Free(&rn);
}

MODE_FN(MakeChar, SetNodeRotation, 4, (MC_NodeRotation(&a[0], &a[1], &a[2], &a[3]), 1))

/* the basic actions of a turret (a character that does not walk) */
static PyObject *MakeChar_TurretBasicActions(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "RotLimit", "nExtraRlx", NULL };
	char *ch, name[0x20];
	PyObject *limitObj = NULL;
	int extra = 2;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|Oi", names, &ch, &limitObj, &extra)) {
		return NULL;
	}
	double limit = MC_ArgNum(limitObj, 720);
	MC_GetClass(ch);
	MC_Create("Relax");
	MC_Anm(ch, "Relax");
	MC_SetNum("isLoop", 1);
	MC_SetNum("RotSpeed", 0);
	MC_SetNum("AngRotLimit", limit);
	MC_SetNum("UseDisp", 0);
	MC_SetNum("NodeModifier", 0);
	MC_SetNum("AngleModifierSpeed", 1.0);
	MC_ClsStr("DefAction", "Relax");
	for (int i = 0; i < extra; i++) {
		Str_Printf(name, sizeof(name), "Relax%d", i);
		MC_Create(name);
		MC_Anm(ch, name);
		MC_SetNum("isLoop", 0);
		MC_SetNum("RotSpeed", 0);
		MC_SetNum("AngRotLimit", limit);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("NodeModifier", -1);
		MC_SetNum("AngleModifierSpeed", 5.0);
	}
	MC_Create("FaceToTalk");
	MC_Anm(ch, "Relax");
	MC_SetNum("PriorityLevel", 190);
	MC_SetNum("isLoop", 1);
	MC_SetNum("RotSpeed", 0);
	MC_SetNum("AngRotLimit", limit);
	MC_SetNum("UseDisp", 0);
	MC_SetNum("UseFlyDisp", 0);
	MC_SetNum("UseFlyFall", 0);
	MC_SetNum("NodeModifier", 0);
	MC_SetNum("AngleModifierSpeed", 5.0);
	MC_SetNum("AutoFaceSpeed", 10);
	MC_SetNum("AutoFaceOffsetH", 0);
	MC_SetNum("AutoFaceOffsetV", 0);
	return Py_ReturnNone();
}

/* the turret turns only its head while it talks */
static void MC_NoTurn(const char *act) {
	Native_ActAction(act);
	MC_SetNum("RotSpeed", 0);
	MC_SetNum("AngRotLimit", 360);
}

static PyObject *MakeChar_AddTurretTalkAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "NodeModifier", NULL };
	char *ch;
	PyObject *nodeMod = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|O", names, &ch, &nodeMod)) {
		return NULL;
	}
	MC_TalkAction(ch, MC_ArgNum(nodeMod, 0), 0);
	MC_NoTurn("Talk");
	MC_NoTurn("TalkNPC");
	MC_NoTurn("TalkListen");
	return Py_ReturnNone();
}

static PyObject *MakeChar_AddTurretTalkAct(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "ActName", "NodeModifier", NULL };
	char *ch, *act, npc[0x48];
	PyObject *nodeMod = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "ss|O", names, &ch, &act, &nodeMod)) {
		return NULL;
	}
	MC_TalkAct(ch, act, MC_ArgNum(nodeMod, 0), 1, "", NULL);
	MC_NoTurn(act);
	Str_Printf(npc, sizeof(npc), "%sNPC", act);
	MC_NoTurn(npc);
	return Py_ReturnNone();
}

#define MC_KW(f) (PyCFunction)(void (*)(void))f, 3

PyMethodDef g_MakeCharMethods[] = {
	{ "CreateRelaxAction", MC_KW(MakeChar_CreateRelaxAction), NULL },
	{ "AddRelaxActions", MC_KW(MakeChar_AddRelaxActions), NULL },
	{ "AddDiscover", MakeChar_AddDiscover, 1, NULL },
	{ "AddMove", MakeChar_AddMove, 1, NULL },
	{ "AddWalkActions", MC_KW(MakeChar_AddWalkActions), NULL },
	{ "AddInitWalkActions", MC_KW(MakeChar_AddInitWalkActions), NULL },
	{ "AddEndWalkActions", MC_KW(MakeChar_AddEndWalkActions), NULL },
	{ "AddFallAction", MC_KW(MakeChar_AddFallAction), NULL },
	{ "AddTalkAction", MC_KW(MakeChar_AddTalkAction), NULL },
	{ "AddTalkMorph", MakeChar_AddTalkMorph, 1, NULL },
	{ "AddMorphAction", MakeChar_AddMorphAction, 1, NULL },
	{ "AddDefMorphActions", MakeChar_AddDefMorphActions, 1, NULL },
	{ "EventAnimCommand", MC_KW(MakeChar_EventAnimCommand), NULL },
	{ "AddTalkAct", MC_KW(MakeChar_AddTalkAct), NULL },
	{ "AddFlashedAction", MC_KW(MakeChar_AddFlashedAction), NULL },
	{ "AddCrazyAction", MakeChar_AddCrazyAction, 1, NULL },
	{ "AddDazeAction", MC_KW(MakeChar_AddDazeAction), NULL },
	{ "ListenAction", MC_KW(MakeChar_ListenAction), NULL },
	{ "AddBreakLoopAction", MakeChar_AddBreakLoopAction, 1, NULL },
	{ "SetSitActions", MakeChar_SetSitActions, 1, NULL },
	{ "InitChar", MakeChar_InitChar, 1, NULL },
	{ "SetNodeRotation", MakeChar_SetNodeRotation, 1, NULL },
	{ "TurretBasicActions", MC_KW(MakeChar_TurretBasicActions), NULL },
	{ "AddTurretTalkAction", MC_KW(MakeChar_AddTurretTalkAction), NULL },
	{ "AddTurretTalkAct", MC_KW(MakeChar_AddTurretTalkAct), NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MakeChar)
