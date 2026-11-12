/*
 * The MakeChar module of the game in C (issue #2, step 6b): the helpers the character scripts build the action
 * classes of the characters with (relax, walk, fall, talk, morph, break loop, sit and turret actions, the class
 * defaults). They work on SAct's current class and action as the script did, with the same variable sets.
 * MaxSpeed, MinSpeed, RelaxRotLimit, DazedChars and the morph lists stay variables of the module: the character
 * scripts change them.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values and call the engine's action
 * classes in C. The module variables are read where the port read them (as numbers, Script_GetFloat, or by the
 * membership of a name, errors dropped) and written through at once (the character scripts, Scene and
 * CharAct read and write them); DazedChars is changed in place. Every function has a native entry that
 * parses its arguments with the formats of the port (Mode_Parse; the keyword functions MODE_KWT with the script's
 * names).
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
		Native_ErrClear();
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

/* the same for an argument of an entry (NULL: not given, def) */
static double MC_ArgNum(const nativeVal_t *v, double def) {
	return v ? MC_Num(v) : def;
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

MODE_KWT(MakeChar, CreateRelaxAction, "sO|ssi", "CharName", "turnspeed", "Name", "Anm", "Loop") {
	const char *ch, *name, *anm;
	const nativeVal_t *turn;
	int loop;
	Mode_KwDefS(v, given, 2, "Relax");
	Mode_KwDefS(v, given, 3, "Relax");
	Mode_KwDefI(v, given, 4, 0);
	if (!Mode_Parse(v, 5, "sOssi", &ch, &turn, &name, &anm, &loop)) {
		return 0;
	}
	MC_RelaxAction(ch, MC_ArgNum(turn, 0.0), name, anm, loop);
	return 1;
}

MODE_KWT(MakeChar, AddRelaxActions, "sO|i", "CharName", "turnspeed", "nExtraRlx") {
	const char *ch;
	char name[0x20];
	const nativeVal_t *turnObj;
	int extra;
	Mode_KwDefI(v, given, 2, 2);
	if (!Mode_Parse(v, 3, "sOi", &ch, &turnObj, &extra)) {
		return 0;
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
	return 1;
}

/* DiscoverInit and Discover: the character saw the player and calls the police */
static byte MakeChar_AddDiscover_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch;
	(void)out;
	if (!Mode_Parse(a, n, "s", &ch)) {
		return 0;
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
	return 1;
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

static byte MakeChar_AddMove_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch, *action, *anm;
	const nativeVal_t *turn, *disp;
	int loop;
	(void)out;
	if (!Mode_Parse(a, n, "sssOOi", &ch, &action, &anm, &turn, &disp, &loop)) {
		return 0;
	}
	MC_Move(ch, action, anm, MC_ArgNum(turn, 0.0), MC_ArgNum(disp, 0.0), loop);
	return 1;
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
			Native_ErrClear();
		} else {
			Str_Copy(act, def[i], sizeof(act));
		}
		MC_Move(ch, act, act, turn, disp, loop);
		if (end) {
			MC_SetNum("MinSpeed", MC_Var("MaxSpeed", 1.5));
		}
	}
}

/* the walk functions (CharName, turnspeed=360, isRun=1, UseDisp=1, ActList=None) on their slots */
static byte MC_Walk(nativeVal_t *v, const byte *given, const char *const *def, const char *run, int loop, byte end) {
	const char *ch;
	const nativeVal_t *turn, *disp, *acts;
	int isRun;
	nativeVal_t list;
	Mode_KwDefI(v, given, 2, 1);
	if (!Mode_Parse(v, 5, "sOiOO", &ch, &turn, &isRun, &disp, &acts)) {
		return 0;
	}
	/* the list is made native in place (Mode_Elems): a copy */
	if (given[4]) {
		Native_Copy(&list, acts);
	}
	MC_WalkList(ch, MC_ArgNum(given[1] ? turn : NULL, 360), isRun, MC_ArgNum(given[3] ? disp : NULL, 1), given[4] ? &list : NULL, def,
		run, loop, end);
	if (given[4]) {
		Native_Free(&list);
	}
	return 1;
}

#define MC_WALKFN(name, pre, loop, end) \
	MODE_KWT(MakeChar, name, "s|OiOO", "CharName", "turnspeed", "isRun", "UseDisp", "ActList") { \
		static const char *const def[] = { "Walk" pre, "Back" pre, "StrLeft" pre, "StrRight" pre }; \
		return MC_Walk(v, given, def, "Run" pre, loop, end); \
	}
MC_WALKFN(AddWalkActions, "", 1, 0)
MC_WALKFN(AddInitWalkActions, "Init", 0, 0)
MC_WALKFN(AddEndWalkActions, "End", 0, 1)

/* Fall, then FallInit and FallEnd when asked; only the first loops */
MODE_KWT(MakeChar, AddFallAction, "s|Oii", "CharName", "turnspeed", "InitAction", "EndAction") {
	const char *ch;
	const nativeVal_t *turnObj;
	int init, end;
	Mode_KwDefI(v, given, 2, 0);
	Mode_KwDefI(v, given, 3, 0);
	if (!Mode_Parse(v, 4, "sOii", &ch, &turnObj, &init, &end)) {
		return 0;
	}
	double turn = MC_ArgNum(given[1] ? turnObj : NULL, 45);
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
		MC_SetNum("RotSpeed", turn);
		MC_SetNum("NodeModifier", -1);
		MC_SetNum("AngleModifierSpeed", 5.0);
	}
	return 1;
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

MODE_KWT(MakeChar, AddTalkAct, "ss|OisO", "CharName", "AnmName", "NodeModifier", "isLoop", "ExtraAction", "MorphAnm") {
	const char *ch, *anm, *extra;
	const nativeVal_t *nodeMod, *morph;
	int loop;
	Mode_KwDefI(v, given, 3, 1);
	Mode_KwDefS(v, given, 4, "");
	if (!Mode_Parse(v, 6, "ssOisO", &ch, &anm, &nodeMod, &loop, &extra, &morph)) {
		return 0;
	}
	/* a morph not given is None: no text */
	MC_TalkAct(ch, anm, MC_ArgNum(given[2] ? nodeMod : NULL, 0), loop, extra, morph->type == NV_STR ? Native_Text(morph) : NULL);
	return 1;
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

MODE_KWT(MakeChar, AddTalkAction, "s|Oi", "CharName", "NodeModifier", "nExtraActions") {
	const char *ch;
	const nativeVal_t *nodeMod;
	int extra;
	Mode_KwDefI(v, given, 2, 0);
	if (!Mode_Parse(v, 3, "sOi", &ch, &nodeMod, &extra)) {
		return 0;
	}
	MC_TalkAction(ch, MC_ArgNum(given[1] ? nodeMod : NULL, 0), extra);
	return 1;
}

static byte MC_AddTalkMorph(const nativeVal_t *m) {
	MC_TalkMorph(Native_True(m) && m->type == NV_STR ? Native_Text(m) : NULL);
	return 1;
}

MODE_FN(MakeChar, AddTalkMorph, 1, MC_AddTalkMorph(&a[0]))

static void MC_Morph(const char *ch, const char *act, const char *file, int chan) {
	char path[0x200];
	MC_Create(act);
	Str_Printf(path, sizeof(path), "Models/Chars/%s/Anm/%s", ch, file);
	MC_SetStr("AnmFilePath", path);
	MC_SetNum("isLoop", 1);
	MC_SetNum("Chanel", chan);
}

static byte MakeChar_AddMorphAction_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch, *act, *file;
	int chan;
	(void)out;
	if (!Mode_Parse(a, n, "sssi", &ch, &act, &file, &chan)) {
		return 0;
	}
	MC_Morph(ch, act, file, chan);
	return 1;
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

static byte MakeChar_AddDefMorphActions_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch;
	(void)out;
	if (!Mode_Parse(a, n, "s", &ch)) {
		return 0;
	}
	MC_DefMorphs(ch);
	return 1;
}

/* EventAnimCommand(time, name[, discard]): an animation command; a negative time is the end of the animation */
MODE_KWT(MakeChar, EventAnimCommand, "Os|i", "Time", "Name", "Discard") {
	const nativeVal_t *t;
	const char *name;
	int discard;
	Mode_KwDefI(v, given, 2, 1);
	if (!Mode_Parse(v, 3, "Osi", &t, &name, &discard)) {
		return 0;
	}
	double time = MC_ArgNum(t, 0);
	MC_Event(time < 0 ? MC_Get("AnmTotalTime") : time, name, discard, "!AnmCommand");
	return 1;
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

MODE_KWT(MakeChar, ListenAction, "sss|s", "CharName", "ActionName", "EventFunc", "AnmName") {
	const char *ch, *act, *func, *anm;
	Mode_KwDefS(v, given, 3, "Relax");
	if (!Mode_Parse(v, 4, "ssss", &ch, &act, &func, &anm)) {
		return 0;
	}
	MC_Listen(ch, act, func, anm);
	return 1;
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

static byte MakeChar_AddBreakLoopAction_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch, *name;
	int priority;
	(void)out;
	if (!Mode_Parse(a, n, "ssi", &ch, &name, &priority)) {
		return 0;
	}
	MC_BreakLoop(ch, name, priority);
	return 1;
}

/* flashed by the camera: the end of each part leads to the next; the police listen */
MODE_KWT(MakeChar, AddFlashedAction, "s|i", "CharName", "ListenPolice") {
	const char *ch;
	int listen;
	Mode_KwDefI(v, given, 1, 1);
	if (!Mode_Parse(v, 2, "si", &ch, &listen)) {
		return 0;
	}
	MC_BreakLoop(ch, "Flashed", 201);
	Native_ActAction("FlashedInit");
	MC_Event(MC_Get("AnmTotalTime"), "Flashed", -1, "!AnmSetAction");
	Native_ActAction("Flashed");
	MC_Event(MC_Get("AnmTotalTime"), "FlashedEnd", -1, "!AnmSetAction");
	if (listen) {
		MC_Listen(ch, "PoliceListen", "!ListeningPolice", "Relax");
	}
	return 1;
}

static byte MakeChar_AddCrazyAction_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch;
	(void)out;
	if (!Mode_Parse(a, n, "s", &ch)) {
		return 0;
	}
	MC_BreakLoop(ch, "Crazy", 1);
	return 1;
}

/* dazed by the mayor: a coin falls a quarter into the start; the character goes into DazedChars */
MODE_KWT(MakeChar, AddDazeAction, "s|i", "CharName", "ListenMayor") {
	const char *ch;
	int listen;
	Mode_KwDefI(v, given, 1, 1);
	if (!Mode_Parse(v, 2, "si", &ch, &listen)) {
		return 0;
	}
	nativeVal_t s;
	Native_SetText(&s, ch, (int)strlen(ch));
	if (!Mode_ListAddG("MakeChar", "DazedChars", &s, 0)) {
		Native_ErrClear();
	}
	Native_Free(&s);
	MC_BreakLoop(ch, "Dazed", 202);
	Native_ActAction("DazedInit");
	MC_Event(MC_Get("AnmTotalTime") / 4.0, "DazedMoney", -1, "MisItems.DazedCoin");
	if (listen) {
		MC_Listen(ch, "MayorListen", "!ListeningMayor", "Relax");
	}
	return 1;
}

/* sitting at a desk: SitDown, GetUp, Work and WorkRelax */
static byte MakeChar_SetSitActions_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const struct {
		const char *name;
		int loop, priority;
	} acts[] = { { "SitDown", 0, 0 }, { "GetUp", 0, 1 }, { "Work", 1, 0 }, { "WorkRelax", 0, 0 } };
	const char *ch;
	(void)out;
	if (!Mode_Parse(a, n, "s", &ch)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		MC_Create(acts[i].name);
		MC_Anm(ch, acts[i].name);
		MC_SetNum("isLoop", acts[i].loop);
		MC_SetNum("RotSpeed", 0);
		MC_SetNum("UseDisp", 0);
		MC_SetNum("PriorityLevel", acts[i].priority);
	}
	return 1;
}

/* InitChar(name): a new class with the defaults of a character, MaxSpeed and MinSpeed back to theirs */
static byte MakeChar_InitChar_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *ch;
	char text[0x100], path[0x200];
	(void)out;
	if (!Mode_Parse(a, n, "s", &ch)) {
		return 0;
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
	return 1;
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
MODE_KWT(MakeChar, TurretBasicActions, "s|Oi", "CharName", "RotLimit", "nExtraRlx") {
	const char *ch;
	char name[0x20];
	const nativeVal_t *limitObj;
	int extra;
	Mode_KwDefI(v, given, 2, 2);
	if (!Mode_Parse(v, 3, "sOi", &ch, &limitObj, &extra)) {
		return 0;
	}
	double limit = MC_ArgNum(given[1] ? limitObj : NULL, 720);
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
	return 1;
}

/* the turret turns only its head while it talks */
static void MC_NoTurn(const char *act) {
	Native_ActAction(act);
	MC_SetNum("RotSpeed", 0);
	MC_SetNum("AngRotLimit", 360);
}

MODE_KWT(MakeChar, AddTurretTalkAction, "s|O", "CharName", "NodeModifier") {
	const char *ch;
	const nativeVal_t *nodeMod;
	if (!Mode_Parse(v, 2, "sO", &ch, &nodeMod)) {
		return 0;
	}
	MC_TalkAction(ch, MC_ArgNum(given[1] ? nodeMod : NULL, 0), 0);
	MC_NoTurn("Talk");
	MC_NoTurn("TalkNPC");
	MC_NoTurn("TalkListen");
	return 1;
}

MODE_KWT(MakeChar, AddTurretTalkAct, "ss|O", "CharName", "ActName", "NodeModifier") {
	const char *ch, *act;
	char npc[0x48];
	const nativeVal_t *nodeMod;
	if (!Mode_Parse(v, 3, "ssO", &ch, &act, &nodeMod)) {
		return 0;
	}
	MC_TalkAct(ch, act, MC_ArgNum(given[2] ? nodeMod : NULL, 0), 1, "", NULL);
	MC_NoTurn(act);
	Str_Printf(npc, sizeof(npc), "%sNPC", act);
	MC_NoTurn(npc);
	return 1;
}

const portFn_t g_MakeCharMethods[] = {
	PORT_FNK(CreateRelaxAction, MakeChar_CreateRelaxAction),
	PORT_FNK(AddRelaxActions, MakeChar_AddRelaxActions),
	PORT_FN(AddDiscover, MakeChar_AddDiscover),
	PORT_FN(AddMove, MakeChar_AddMove),
	PORT_FNK(AddWalkActions, MakeChar_AddWalkActions),
	PORT_FNK(AddInitWalkActions, MakeChar_AddInitWalkActions),
	PORT_FNK(AddEndWalkActions, MakeChar_AddEndWalkActions),
	PORT_FNK(AddFallAction, MakeChar_AddFallAction),
	PORT_FNK(AddTalkAction, MakeChar_AddTalkAction),
	PORT_FN(AddTalkMorph, MakeChar_AddTalkMorph),
	PORT_FN(AddMorphAction, MakeChar_AddMorphAction),
	PORT_FN(AddDefMorphActions, MakeChar_AddDefMorphActions),
	PORT_FNK(EventAnimCommand, MakeChar_EventAnimCommand),
	PORT_FNK(AddTalkAct, MakeChar_AddTalkAct),
	PORT_FNK(AddFlashedAction, MakeChar_AddFlashedAction),
	PORT_FN(AddCrazyAction, MakeChar_AddCrazyAction),
	PORT_FNK(AddDazeAction, MakeChar_AddDazeAction),
	PORT_FNK(ListenAction, MakeChar_ListenAction),
	PORT_FN(AddBreakLoopAction, MakeChar_AddBreakLoopAction),
	PORT_FN(SetSitActions, MakeChar_SetSitActions),
	PORT_FN(InitChar, MakeChar_InitChar),
	PORT_FN(SetNodeRotation, MakeChar_SetNodeRotation),
	PORT_FNK(TurretBasicActions, MakeChar_TurretBasicActions),
	PORT_FNK(AddTurretTalkAction, MakeChar_AddTurretTalkAction),
	PORT_FNK(AddTurretTalkAct, MakeChar_AddTurretTalkAct),
	PORT_FNEND
};

PORTS(MakeChar)
