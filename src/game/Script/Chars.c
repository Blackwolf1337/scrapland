/*
 * The Chars module of the game in C (issue #2, step 6a): the sound events of the animations (the character scripts
 * call ActSound and its kin a few thousand times while a level loads), the resources of the characters of a level
 * and the creation of the player. The module code (default values, the Players list) stays in the script.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values. The sound functions set the
 * events of SAct's current class and action in C. CharRes (the [class, discardable] pairs other scripts fill too),
 * CanDriveList and the hooks of the missions (OnCreateMainChar, OnMainCharDeath) stay variables of the module: they
 * are read where the port read them and lists are changed in place (OBJLIB 5); Init walks the CharRes object itself,
 * as the script's loop, while the character scripts it starts append to it. The characters are handle values
 * (OBJLIB 7). The engine functions the port called by name go through their seams (OBJLIB 7: GetEntity,
 * CreateEntity, GetNetFlags, the dummy search, the scheduler, SAct.GetClass/SetCls/DelClass, SInput.SetActionSet,
 * SLogic.SetOnFloor); the other ports (CharsNPC, CharScorer, MissionsFuncs) and the functions of the module the
 * script calls through its globals are called through their refs (PORT_CALL, PORT_OWN: the port seam, OBJLIB 5), the
 * function given to the scheduler by name (OBJLIB 4). The exec of a character's Init and the eval of the saved
 * batteries keep the readers of the game's texts (OBJLIB 6).
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define CH "Chars"

/*
 * SAct.GetClass(cls) and, when there is one, SAct.GetAction(action) as IntSetChar calls them: 1 when both exist, 0
 * when not, -1 with the error set. A cls or action that is no text raises the TypeError of SAct's parse, the action
 * only when the class exists (the script's "and").
 */
static int Ch_Select(const nativeVal_t *clsV, const nativeVal_t *actionV) {
	const char *cls, *action;
	if (!Native_ArgStr(clsV, 1, &cls)) {
		return -1;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, cls);
	if (!g_pAuxActClass) {
		return 0;
	}
	if (!Native_ArgStr(actionV, 1, &action)) {
		return -1;
	}
	g_pAuxAction = cActClass_GetAction(g_pAuxActClass, action);
	return g_pAuxAction != NULL;
}

/* SAct.SetAct(name, text) */
static void Ch_SetActStr(const char *name, const char *text) {
	char *s = (char *)text;
	cAction_Var(g_pAuxAction, name, 1, &s, NULL, NULL);
}

/* the event of an animation sound at a frame: SAct.SetAct("AddEventAtTime", SAct.GetAct("TimeOfFrameN")), its name
 * and its function. 0 when the class or the action is not there, -1 with the error set */
static int Ch_AddEvent(const nativeVal_t *cls, const nativeVal_t *action, const nativeVal_t *frame, const char *name,
	const char *func) {
	char var[0x40], *s;
	double d;
	int i;
	int sel = Ch_Select(cls, action);
	if (sel <= 0) {
		return sel;
	}
	nativeVal_t r;
	Mode_Repr(frame, &r);
	Str_Printf(var, sizeof(var), "TimeOfFrame%s", Native_Text(&r));
	Native_Free(&r);
	switch (cAction_Var(g_pAuxAction, var, 0, &s, &d, &i)) {
	case ACTVAR_STRING:
		cAction_Var(g_pAuxAction, "AddEventAtTime", 1, &s, NULL, NULL);
		break;
	case ACTVAR_DOUBLE:
		i = (int)d;
		cAction_Var(g_pAuxAction, "AddEventAtTime", 1, NULL, &d, &i);
		break;
	case ACTVAR_INT:
		d = i;
		cAction_Var(g_pAuxAction, "AddEventAtTime", 1, NULL, &d, &i);
		break;
	default:
		/* the script set None: SAct.SetAct refused it */
		Con_Debugf("Caution: SAct.GetAct('%s'): element not found.\n", var);
		Con_Debugf("Caution: SAct.SetAct(): AuxActClass not defined.\n");
		break;
	}
	Ch_SetActStr("EventName", name);
	Ch_SetActStr("EventFunc", func);
	return 1;
}

/* "a:b:c" from three values, as the script joined them (the text of a, or its repr for reprA, and the reprs of the
 * others): 0 with the TypeError of a that is no text */
static byte Ch_Join(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c, byte reprA, char *out, int size) {
	nativeVal_t ra, rb, rc;
	if (!reprA && a->type != NV_STR) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	Native_SetNone(&ra);
	if (reprA) {
		Mode_Repr(a, &ra);
	}
	Mode_Repr(b, &rb);
	Mode_Repr(c, &rc);
	Str_Printf(out, size, "%s:%s:%s", Native_Text(reprA ? &ra : a), Native_Text(&rb), Native_Text(&rc));
	Native_Free(&ra);
	Native_Free(&rb);
	Native_Free(&rc);
	return 1;
}

/* a sound of an animation (ActSound, ActSoundLoop, StrSound): the event named "a:b:c" with the function func */
static byte Ch_Sound(const nativeVal_t *cls, const nativeVal_t *action, const nativeVal_t *frame, const nativeVal_t *a,
	const nativeVal_t *b, const nativeVal_t *c, byte reprA, const char *func) {
	char name[0x200];
	int sel = Ch_Select(cls, action);
	if (sel <= 0) {
		return sel == 0;
	}
	if (!Ch_Join(a, b, c, reprA, name, sizeof(name))) {
		return 0;
	}
	Ch_AddEvent(cls, action, frame, name, func);
	return 1;
}

/* the arguments of an adapter in a PyArg_ParseTuple format of O's (up to 6, optional ones after '|') as native values
 * (None when not given): the number given, -1 with the arity error */
static int Ch_Args(PyObject *args, const char *fmt, nativeVal_t *a, int n) {
	PyObject *o[6] = { NULL };
	int given = 0;
	for (int i = 0; i < n; i++) {
		Native_SetNone(&a[i]);
	}
	if (!PyArg_ParseTuple(args, (char *)fmt, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5])) {
		return -1;
	}
	for (int i = 0; i < n && o[i]; i++) {
		Native_Arg(o[i], &a[i]);
		given++;
	}
	return given;
}

/* the result of such an adapter: the n arguments freed, None for 1, NULL for 0 */
static PyObject *Ch_Out(nativeVal_t *a, int n, byte ok) {
	return Mode_KwOut(a, n, ok, NULL);
}

/* ActSound(cls, action, frame, sound, vol[, pitch]): a sound of the animation */
static PyObject *Chars_ActSound(PyObject *self, PyObject *args) {
	nativeVal_t a[6];
	(void)self;
	int n = Ch_Args(args, "OOOOO|O", a, 6);
	if (n == 5) {
		Native_SetInt(&a[5], 0);
	}
	return Ch_Out(a, 6, n >= 0 && Ch_Sound(&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], 0, "!AnmSoundEvent"));
}

/* ActSoundLoop(cls, action, frame, soundId, vol[, pitch]) */
static PyObject *Chars_ActSoundLoop(PyObject *self, PyObject *args) {
	nativeVal_t a[6];
	(void)self;
	int n = Ch_Args(args, "OOOOO|O", a, 6);
	if (n == 5) {
		Native_SetInt(&a[5], 0);
	}
	return Ch_Out(a, 6, n >= 0 && Ch_Sound(&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], 1, "!AnmSoundLoopEvent"));
}

/* StrSound(cls, action, frame, sound[, vol[, dist]]): a streamed voice */
static PyObject *Chars_StrSound(PyObject *self, PyObject *args) {
	nativeVal_t a[6];
	(void)self;
	int n = Ch_Args(args, "OOOO|OO", a, 6);
	if (n >= 0 && n < 5) {
		Native_SetFloat(&a[4], 1.0);
	}
	if (n >= 0 && n < 6) {
		Native_SetInt(&a[5], 3000);
	}
	return Ch_Out(a, 6, n >= 0 && Ch_Sound(&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], 0, "!StreamingVoice"));
}

/* StrSoundStop(cls, action, frame): the streamed voice stops, also when a new action jumps over the frame */
static byte Ch_StrSoundStop(const nativeVal_t *cls, const nativeVal_t *action, const nativeVal_t *frame) {
	int sel = Ch_AddEvent(cls, action, frame, "Stop", "!StopStreamingVoice");
	if (sel > 0) {
		double d = 0.0;
		int i = 0;
		cAction_Var(g_pAuxAction, "EventDiscardable", 1, NULL, &d, &i);
	}
	return sel >= 0;
}

/* IntSetChar(cls, action): 1 when the class or the action is missing (the name of the script) */
static byte Ch_IntSetChar(const nativeVal_t *cls, const nativeVal_t *action, nativeVal_t *out) {
	int sel = Ch_Select(cls, action);
	if (sel < 0) {
		return 0;
	}
	Native_SetInt(out, !sel);
	return 1;
}

/* ---- the resources: Chars.CharRes, a list of [class, discardable] that other scripts fill too ---- */

/* [cls, flag] */
static void Ch_Pair(nativeVal_t *out, const nativeVal_t *cls, const nativeVal_t *flag) {
	Native_SetEmpty(out, NV_LIST);
	Native_Copy(Native_Append(out), cls);
	Native_Copy(Native_Append(out), flag);
}

/* [cls, i] in Chars.CharRes (the global read at each test, its NameError): 1 or 0, -1 with the error set */
static int Ch_ResIn(const nativeVal_t *cls, long i) {
	nativeVal_t flag, x, list = { 0 };
	Native_SetInt(&flag, i);
	Ch_Pair(&x, cls, &flag);
	int r = Mode_G(CH, "CharRes", &list) ? Mode_In(&list, &x) : -1;
	Native_Free(&list);
	Native_Free(&x);
	return r;
}

/*
 * AddNewCharRes(cr, Discardable): [cr, Discardable] into Chars.CharRes unless [cr, 0] is there, or [cr, 1] is there
 * and Discardable is true (a lasting one replaces the discardable one): 1 when it goes in, 0 when not, -1 with the
 * error set
 */
static int Ch_AddRes(const nativeVal_t *cls, const nativeVal_t *disc) {
	int add = 0;
	int in0 = Ch_ResIn(cls, 0);
	if (in0 < 0) {
		return -1;
	}
	if (in0 == 0) {
		int in1 = Ch_ResIn(cls, 1);
		if (in1 < 0) {
			return -1;
		}
		if (in1 > 0 && !Native_True(disc)) {
			nativeVal_t one, x;
			Native_SetInt(&one, 1);
			Ch_Pair(&x, cls, &one);
			byte ok = Mode_Remove(CH, "CharRes", &x);
			Native_Free(&x);
			if (!ok) {
				return -1;
			}
			add = 1;
		} else if (in1 == 0) {
			add = 1;
		}
	}
	if (add) {
		nativeVal_t x;
		Ch_Pair(&x, cls, disc);
		byte ok = Mode_Append(CH, "CharRes", &x);
		Native_Free(&x);
		if (!ok) {
			return -1;
		}
	}
	return add;
}

/* AddNewCharRes(cls, disc=1): the result of Ch_AddRes */
static byte Ch_AddResArgs(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t one;
	Native_SetInt(&one, 1);
	int r = Ch_AddRes(&a[0], n > 1 ? &a[1] : &one);
	Native_SetInt(out, r);
	return r >= 0;
}

MODE_FNOPT(Chars, AddNewCharRes, 1, 2, Ch_AddResArgs(a, n, out))

/* the AddNewCharRes of a C text */
static int Ch_AddResS(const char *cls, long disc) {
	nativeVal_t c, d;
	Native_SetText(&c, cls, (int)strlen(cls));
	Native_SetInt(&d, disc);
	int r = Ch_AddRes(&c, &d);
	Native_Free(&c);
	return r;
}

/* OBJLIB 6: exec(code) in the module with the locals {"cr": cr}, as the script's exec of a character's Init (the
 * reader of the lines the game execs) */
static byte Ch_Exec(const nativeVal_t *code, const nativeVal_t *cr) {
	scriptFrame_t f = { 0 };
	PyObject *c = Sf_Keep(&f, Native_ToPy(code)), *r = c ? Sf_Keep(&f, Native_ToPy(cr)) : NULL;
	Sf_Exec(&f, c, CH, Sf_Build(&f, "{s:O}", "cr", r));
	byte failed = f.failed;
	Sf_Done(&f, NULL);
	return !failed;
}

/* LocalLoadRes(cr): exec("import "+cr[0]+";"+cr[0]+".Init()"), then the class gets Discardable cr[1] (SAct.SetCls
 * without the class fails without an exception: ceval's SystemError, as in the script) */
static byte Ch_LocalLoadRes(const nativeVal_t *cr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *head = Mode_V(&t), *code = Mode_V(&t), *r = Mode_V(&t), *disc = Mode_V(&t);
	if (!LvN_Item(cr, 0, name) || !Mode_Cat(&t, head, 2, Mode_S(&t, "import "), name)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(name);
	if (!LvN_Item(cr, 0, name) || !Mode_Cat(&t, code, 4, head, Mode_S(&t, ";"), name, Mode_S(&t, ".Init()")) ||
		!Ch_Exec(code, cr)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(name);
	return Mode_Done(&t, LvN_Item(cr, 0, name) && Mode_SActV("GetClass", name, r) && LvN_Item(cr, 1, disc) &&
		LvN_SetCls("Discardable", disc));
}

/* LoadRes(cls): a discardable class, loaded when it is new (LocalLoadRes through the global of the module) */
static byte Ch_LoadRes(const nativeVal_t *cls) {
	nativeVal_t one, pair;
	Native_SetInt(&one, 1);
	int a = Ch_AddRes(cls, &one);
	if (a <= 0) {
		return a == 0;
	}
	Ch_Pair(&pair, cls, &one);
	byte ok = PORT_OWN(Chars, LocalLoadRes, NULL, "v", &pair);
	Native_Free(&pair);
	return ok;
}

/* DiscardRes(cr): [cr, 1] in CharRes; when it is, TryToDiscardThis([cr, 1]) is scheduled now */
static byte Ch_DiscardRes(const nativeVal_t *cr, nativeVal_t *out) {
	int in = Ch_ResIn(cr, 1);
	if (in < 0) {
		return 0;
	}
	if (in > 0) {
		nativeVal_t time, one, pair;
		Native_SetFloat(&time, Mode_Time());
		Native_SetInt(&one, 1);
		Ch_Pair(&pair, cr, &one);
		/* OBJLIB 4: the scheduler takes the global TryToDiscardThis by name */
		byte ok = Mode_AtG(&time, CH, "TryToDiscardThis", &pair, 1);
		Native_Free(&pair);
		if (!ok) {
			return 0;
		}
	}
	Native_SetInt(out, in);
	return 1;
}

/* TryToDiscardThis(res): SAct.DelClass(res[0]); the class leaves CharRes when it is gone (SAct.GetClass(res[0]) == 0) */
static byte Ch_TryToDiscardThis(const nativeVal_t *res) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *r = Mode_V(&t);
	if (!LvN_Item(res, 0, name) || !Mode_SActV("DelClass", name, r)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(name);
	Native_Free(r);
	if (!LvN_Item(res, 0, name) || !Mode_SActV("GetClass", name, r)) {
		return Mode_Done(&t, 0);
	}
	int gone = Mode_CmpI(r, "==", 0);
	return Mode_Done(&t, gone >= 0 && (!gone || Mode_Remove(CH, "CharRes", res)));
}

/* ---- the player ---- */

/* CanDrive(char): the character can drive a ship */
static byte Ch_CanDrive(const nativeVal_t *ch, nativeVal_t *out) {
	nativeVal_t act = { 0 }, list = { 0 };
	int r = Mode_VObj(ch, "ActCtrl", &act) && Mode_G(CH, "CanDriveList", &list) ? Mode_In(&list, &act) : -1;
	Native_Free(&list);
	Native_Free(&act);
	if (r < 0) {
		return 0;
	}
	Native_SetInt(out, r);
	return 1;
}

/* MainCharDeath(name, param): the hook of a mission, else the end of the game */
static byte Ch_MainCharDeath(const nativeVal_t *a) {
	nativeVal_t hook = { 0 };
	byte ok = Mode_G(CH, "OnMainCharDeath", &hook);
	if (ok) {
		ok = Native_True(&hook) ? Mode_CallArgs(&hook, NULL, a, 2) :
			Mode_Import("MissionsFuncs") && PORT_CALL(MissionsFuncs, FinishGame, NULL, "i", 0L);
	}
	Native_Free(&hook);
	return ok;
}

static byte Ch_CreatePlayersList(void) {
	return Mode_ListSet("Players", "OnDeath", "Chars.MainCharDeath");
}

/* OBJLIB 5: the CharRes object Init's loop walks (the character scripts it starts append to it): its length as the
 * for loop takes it each turn (-1 with the error left set, as the port went on), and cr[1] = 0 on the item object
 * itself */
static int Ch_ResCount(PyObject *list) {
	return PySequence_Length(list);
}

static byte Ch_ResLasting(PyObject *cr) {
	PyObject *zero = PyInt_FromLong(0);
	int r = zero ? PySequence_SetItem(cr, 1, zero) : -1;
	Py_XDECREF(zero);
	return r == 0;
}

/* Init(): the characters of the level: the NPC spawn points, Sputnik outdoors and in the junkyard, the resources
 * so far (from now on lasting), then the class of the player */
static byte Ch_Init(void) {
	/* an error stops it and goes to the caller, as in the script (GDB: the save game's character is not loaded) */
	if (!PORT_OWN(Chars, CreatePlayersList, NULL, "")) {
		return 0;
	}
	if (!Mode_ModGetI("Init", "OutMapSkip", 0) && !PORT_CALL(CharsNPC, BrowseNPC, NULL, "")) {
		return 0;
	}
	if ((Mode_ModGetI("Init", "Outdoor", 0) || Mode_ModGetI("Init", "isJunkYard", 0)) && Ch_AddResS("Sputnik", 0) < 0) {
		return 0;
	}
	PyObject *list = Mode_GlobalPy(CH, "CharRes");
	if (!list) {
		return 0;
	}
	/* the list may grow while the scripts load: the length of each turn, as the for loop of the script */
	for (int i = 0; i < Ch_ResCount(list); i++) {
		PyObject *item = PySequence_GetItem(list, i);
		nativeVal_t cr = { 0 };
		if (item) {
			Native_Arg(item, &cr);
		}
		byte ok = item && PORT_OWN(Chars, LocalLoadRes, NULL, "v", &cr) && Ch_ResLasting(item);
		Native_Free(&cr);
		Py_XDECREF(item);
		if (!ok) {
			Py_DECREF(list);
			return 0;
		}
	}
	Py_DECREF(list);
	const char *ch = SaveVar_Get("Char");
	return PORT_OWN(Chars, LoadRes, NULL, "s", ch ? ch : "") && Mode_PrintS("Character system initialized.");
}

/* CreateMainChar(CharName, CharType, Pos, Ang): the character of the player (made when it is not there), the camera
 * on it, the controls and the HUD on foot; None after the print of a character of another kind */
static byte Ch_CreateMainChar(const nativeVal_t *a, nativeVal_t *out) {
	const nativeVal_t *name = &a[0], *type = &a[1], *pos = &a[2], *ang = &a[3];
	modeVals_t t = { .n = 0 };
	nativeVal_t *c = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *v = Mode_V(&t), *hook = Mode_V(&t);
	nativeVal_t *cam = Mode_V(&t), *cname = Mode_V(&t), *text = Mode_V(&t);
	if (!Mode_Import("CharScorer") || !Mode_EntitySeam(name, c)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(c)) {
		Native_Free(c);
		if (!LvN_Item(pos, 0, x) || !LvN_Item(pos, 1, y) || !LvN_Item(pos, 2, z) || !LvN_Create(name, x, y, z, "WalkChar", c) ||
			!Mode_VSet(c, "ActCtrl", type) || !LvN_SetI(c, "UsrControl", 0) || !LvN_SetI(c, "AutoSelect", 1) ||
			!LvN_Item(ang, 1, x)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(v, NV_TUPLE);
		Native_SetInt(Native_Append(v), 0);
		Native_Copy(Native_Append(v), x);
		Native_SetInt(Native_Append(v), 0);
		if (!Mode_VSet(c, "Ang", v) || !Mode_VSet(c, "ViewAng", ang) || !LvN_SetS(c, "TgType", "Green") ||
			!LvN_SetS(c, "EntityList", "Players") || !LvN_SetI(c, "Energy", 2) || !Mode_G(CH, "OnCreateMainChar", hook) ||
			(Native_True(hook) && !Mode_CallArgs(hook, NULL, c, 1))) {
			return Mode_Done(&t, 0);
		}
	} else {
		int walk = Mode_VIsType(c, "WalkChar");
		int other = walk == 0 ? 1 : walk > 0 && Mode_VObj(c, "ActCtrl", v) ? Mode_Cmp(v, "!=", type) : -1;
		if (other < 0) {
			return Mode_Done(&t, 0);
		}
		if (other) {
			return Mode_Done(&t, Mode_Cat(&t, text, 3, Mode_S(&t, "Invalid '"), name, Mode_S(&t, "' character created.")) &&
				Mode_PrintV(text));
		}
	}
	Mode_CamName(0, cname);
	if (!Mode_EntitySeam(cname, cam) || !Mode_VObj(c, "Name", v) || !Mode_VSet(cam, "MainTarget", v) ||
		!Mode_VSet(cam, "Target", v) || !LvN_SetI(cam, "FreeCam", 0) || !Mode_ActionSet("Walk") ||
		!PORT_CALL(CharScorer, Create, NULL, "i", 0L)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, c);
	return Mode_Done(&t, 1);
}

/* CreateStartupPlayer(): offline, the camera and the character of the player at the spawn point of the save game
 * (or the first deathmatch spawn), with the batteries of the save game */
static byte Ch_CreateStartupPlayer(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spawn = Mode_V(&t), *dummy = Mode_V(&t), *text = Mode_V(&t), *head = Mode_V(&t), *at = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *cam = Mode_V(&t), *cname = Mode_V(&t);
	nativeVal_t *fn = Mode_V(&t), *c = Mode_V(&t), *energy = Mode_V(&t), *rot = Mode_V(&t);
	nativeVal_t *yaw = Mode_V(&t), *two = Mode_V(&t);
	const char *s;
	int off = Native_NetOff();
	if (off <= 0) {
		return Mode_Done(&t, off == 0);
	}
	Mode_SaveVar("Spawn", spawn);
	if (!Mode_DummyStartV(spawn, 1) || !Mode_DummyNextSeam(dummy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(dummy)) {
		Native_Free(spawn);
		Mode_SaveVar("Spawn", spawn);
		if (!Mode_Cat(&t, text, 2, spawn, Mode_S(&t, " Not found!!!!")) || !Mode_PrintV(text) ||
			!Mode_DummyStartSeam("DM_Player_Spawn*", 1)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(dummy);
		if (!Mode_DummyNextSeam(dummy) || !LvN_Item(dummy, 0, head)) {
			return Mode_Done(&t, 0);
		}
		if (LvN_EqS(head, "DM_Player_Spawn_Junkyard")) {
			Native_Free(dummy);
			if (!Mode_DummyNextSeam(dummy)) {
				return Mode_Done(&t, 0);
			}
		}
		if (!Native_True(dummy)) {
			return Mode_Done(&t, Mode_PrintS("Cannot create a main char!"));
		}
		Native_Free(head);
		Native_Free(text);
		if (!LvN_Item(dummy, 0, head) || !Mode_Cat(&t, text, 3, Mode_S(&t, "Using "), head, Mode_S(&t, " indeed.")) ||
			!Mode_PrintV(text)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!LvN_Item(dummy, 1, at) || !LvN_Item(at, 0, x) || !LvN_Item(at, 1, y) || !LvN_Item(at, 2, z) ||
		!LvN_Create(Mode_S(&t, "MainCam"), x, y, z, "FCam", cam) || !Mode_VObj(cam, "Name", cname) ||
		!Native_ArgStr(cname, 2, &s)) {
		return Mode_Done(&t, 0);
	}
	cWorld_SetCam(g_pWorld, 0, s);
	Native_Free(head);
	Native_Free(at);
	if (!Mode_G(CH, "CreateMainChar", fn) || !LvN_Item(dummy, 2, rot) || !LvN_Item(rot, 1, head) ||
		!Mode_Arith('+', head, Mode_I(&t, 180), yaw) || !LvN_Item(dummy, 1, at)) {
		return Mode_Done(&t, 0);
	}
	/* fn("Player0", Scrap.GetSaveVar("Char"), dummy[1], (0, yaw, 0)) */
	nativeVal_t call[4];
	LvN_Str(&call[0], "Player0");
	Mode_SaveVar("Char", &call[1]);
	Native_Copy(&call[2], at);
	Native_SetEmpty(&call[3], NV_TUPLE);
	Native_SetInt(Native_Append(&call[3]), 0);
	Native_Copy(Native_Append(&call[3]), yaw);
	Native_SetInt(Native_Append(&call[3]), 0);
	byte ok = Mode_CallArgs(fn, c, call, 4);
	for (int i = 0; i < 4; i++) {
		Native_Free(&call[i]);
	}
	Native_Free(text);
	Mode_SaveVar("AcBatteries", text);
	if (!ok || !Mode_EvalIn(CH, text, energy)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(two, 2.0);
	int low = Mode_Cmp(energy, "<", two);
	if (low < 0) {
		return Mode_Done(&t, 0);
	}
	if (low) {
		Native_Free(energy);
		Native_SetInt(energy, 2);
		SaveVar_Set("AcBatteries", "2");
	}
	Native_Free(head);
	Native_Free(text);
	if (!Mode_VSet(c, "Energy", energy) || !LvN_Item(dummy, 0, head) || !Native_Slice(head, 0, 14, text)) {
		return Mode_Done(&t, 0);
	}
	int floor = Mode_Cmp(text, "!=", Mode_S(&t, "DM_Ship_Spawn_"));
	Native_Free(cname);
	if (floor < 0 || (floor && (!Mode_VObj(c, "Name", cname) || !LvN_SetOnFloor(cname)))) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, c);
	return Mode_Done(&t, 1);
}

MODE_FN(Chars, StrSoundStop, 3, Ch_StrSoundStop(&a[0], &a[1], &a[2]))
MODE_FN(Chars, IntSetChar, 2, Ch_IntSetChar(&a[0], &a[1], out))
MODE_FN(Chars, LocalLoadRes, 1, Ch_LocalLoadRes(&a[0]))
MODE_FN(Chars, LoadRes, 1, Ch_LoadRes(&a[0]))
MODE_FN(Chars, DiscardRes, 1, Ch_DiscardRes(&a[0], out))
MODE_FN(Chars, TryToDiscardThis, 1, Ch_TryToDiscardThis(&a[0]))
MODE_FN(Chars, CanDrive, 1, Ch_CanDrive(&a[0], out))
MODE_FN(Chars, SetOnFloor, 1, LvN_SetOnFloorOf(&a[0]))
MODE_FN(Chars, MainCharDeath, 2, Ch_MainCharDeath(a))
MODE_FN(Chars, CreatePlayersList, 0, Ch_CreatePlayersList())
MODE_FN(Chars, CreateMainChar, 4, Ch_CreateMainChar(a, out))
MODE_FN(Chars, CreateStartupPlayer, 0, Ch_CreateStartupPlayer(out))

/* Init takes any arguments (the port did not parse them) */
MODE_FNANY(Chars, Init, Ch_Init())

PyMethodDef g_CharsMethods[] = {
	{ "CanDrive", Chars_CanDrive, 1, NULL },
	{ "CreateMainChar", Chars_CreateMainChar, 1, NULL },
	{ "SetOnFloor", Chars_SetOnFloor, 1, NULL },
	{ "IntSetChar", Chars_IntSetChar, 1, NULL },
	{ "ActSound", Chars_ActSound, 1, NULL },
	{ "StrSound", Chars_StrSound, 1, NULL },
	{ "StrSoundStop", Chars_StrSoundStop, 1, NULL },
	{ "ActSoundLoop", Chars_ActSoundLoop, 1, NULL },
	{ "DiscardRes", Chars_DiscardRes, 1, NULL },
	{ "TryToDiscardThis", Chars_TryToDiscardThis, 1, NULL },
	{ "LoadRes", Chars_LoadRes, 1, NULL },
	{ "AddNewCharRes", Chars_AddNewCharRes, 1, NULL },
	{ "LocalLoadRes", Chars_LocalLoadRes, 1, NULL },
	{ "MainCharDeath", Chars_MainCharDeath, 1, NULL },
	{ "CreatePlayersList", Chars_CreatePlayersList, 1, NULL },
	{ "Init", Chars_Init, 1, NULL },
	{ "CreateStartupPlayer", Chars_CreateStartupPlayer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Chars)
