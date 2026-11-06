/*
 * The Chars module of the game in C (issue #2, step 6a): the sound events of the animations (the character scripts
 * call ActSound and its kin a few thousand times while a level loads), the resources of the characters of a level
 * and the creation of the player. The module code (default values, the Players list) stays in the script.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scheduler.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

/* print text as the script's print statement does (sys.stdout, not the console log): 0 with the error set */
static byte Chars_PrintLine(const char *fmt, ...) {
	char buf[0x200];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	PyObject *t = PyString_FromString(buf);
	byte ok = t && Script_Print(&t, 1);
	Py_XDECREF(t);
	return ok;
}


/* SAct.GetClass(cls) and, when there is one, SAct.GetAction(action): 1 when both exist */
static byte Chars_Select(const char *cls, const char *action) {
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, cls);
	if (!g_pAuxActClass) {
		return 0;
	}
	g_pAuxAction = cActClass_GetAction(g_pAuxActClass, action);
	return g_pAuxAction != NULL;
}

/* SAct.SetAct(name, text) */
static void Chars_SetActStr(const char *name, const char *text) {
	char *s = (char *)text;
	cAction_Var(g_pAuxAction, name, 1, &s, NULL, NULL);
}

/* the event of an animation sound at a frame: SAct.SetAct("AddEventAtTime", SAct.GetAct("TimeOfFrameN")), its name
 * and its function. 0 when the class or the action is not there */
static byte Chars_AddEvent(const char *cls, const char *action, PyObject *frame, const char *name, const char *func) {
	char var[0x40], *s;
	double d;
	int i;
	if (!Chars_Select(cls, action)) {
		return 0;
	}
	PyObject *r = PyObject_Repr(frame);
	Str_Printf(var, sizeof(var), "TimeOfFrame%s", r ? PyString_AsString(r) : "");
	Py_XDECREF(r);
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
	Chars_SetActStr("EventName", name);
	Chars_SetActStr("EventFunc", func);
	return 1;
}

/* "a:b:c" from three values, as the script joined them (the text of a, the repr of the others) */
static byte Chars_Join(PyObject *a, PyObject *b, PyObject *c, byte reprA, char *out, int size) {
	PyObject *ra = reprA ? PyObject_Repr(a) : NULL, *rb = PyObject_Repr(b), *rc = PyObject_Repr(c);
	byte ok = (reprA ? ra != NULL : PyString_Check(a)) && rb && rc;
	if (ok) {
		Str_Printf(out, size, "%s:%s:%s", reprA ? PyString_AsString(ra) : PyString_AsString(a), PyString_AsString(rb),
			PyString_AsString(rc));
	} else if (!PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
	}
	Py_XDECREF(ra);
	Py_XDECREF(rb);
	Py_XDECREF(rc);
	return ok;
}

/* ActSound(cls, action, frame, sound, vol[, pitch]): a sound of the animation */
static PyObject *Chars_ActSound(PyObject *self, PyObject *args) {
	char *cls, *action, name[0x200];
	PyObject *frame, *sound, *vol, *pitch = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssOOO|O", &cls, &action, &frame, &sound, &vol, &pitch)) {
		return NULL;
	}
	if (!Chars_Select(cls, action)) {
		return Py_ReturnNone();
	}
	PyObject *zero = pitch ? NULL : PyInt_FromLong(0);
	byte ok = Chars_Join(sound, vol, pitch ? pitch : zero, 0, name, sizeof(name));
	Py_XDECREF(zero);
	if (!ok) {
		return NULL;
	}
	Chars_AddEvent(cls, action, frame, name, "!AnmSoundEvent");
	return Py_ReturnNone();
}

/* ActSoundLoop(cls, action, frame, soundId, vol[, pitch]) */
static PyObject *Chars_ActSoundLoop(PyObject *self, PyObject *args) {
	char *cls, *action, name[0x200];
	PyObject *frame, *sound, *vol, *pitch = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssOOO|O", &cls, &action, &frame, &sound, &vol, &pitch)) {
		return NULL;
	}
	if (!Chars_Select(cls, action)) {
		return Py_ReturnNone();
	}
	PyObject *zero = pitch ? NULL : PyInt_FromLong(0);
	byte ok = Chars_Join(sound, vol, pitch ? pitch : zero, 1, name, sizeof(name));
	Py_XDECREF(zero);
	if (!ok) {
		return NULL;
	}
	Chars_AddEvent(cls, action, frame, name, "!AnmSoundLoopEvent");
	return Py_ReturnNone();
}

/* StrSound(cls, action, frame, sound[, vol[, dist]]): a streamed voice */
static PyObject *Chars_StrSound(PyObject *self, PyObject *args) {
	char *cls, *action, name[0x200];
	PyObject *frame, *sound, *vol = NULL, *dist = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssOO|OO", &cls, &action, &frame, &sound, &vol, &dist)) {
		return NULL;
	}
	if (!Chars_Select(cls, action)) {
		return Py_ReturnNone();
	}
	PyObject *v = vol ? NULL : PyFloat_FromDouble(1.0), *dd = dist ? NULL : PyInt_FromLong(3000);
	byte ok = Chars_Join(sound, vol ? vol : v, dist ? dist : dd, 0, name, sizeof(name));
	Py_XDECREF(v);
	Py_XDECREF(dd);
	if (!ok) {
		return NULL;
	}
	Chars_AddEvent(cls, action, frame, name, "!StreamingVoice");
	return Py_ReturnNone();
}

/* StrSoundStop(cls, action, frame): the streamed voice stops, also when a new action jumps over the frame */
static PyObject *Chars_StrSoundStop(PyObject *self, PyObject *args) {
	char *cls, *action;
	PyObject *frame;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssO", &cls, &action, &frame)) {
		return NULL;
	}
	if (Chars_AddEvent(cls, action, frame, "Stop", "!StopStreamingVoice")) {
		double d = 0.0;
		int i = 0;
		cAction_Var(g_pAuxAction, "EventDiscardable", 1, NULL, &d, &i);
	}
	return Py_ReturnNone();
}

/* IntSetChar(cls, action): 1 when the class or the action is missing (the name of the script) */
static PyObject *Chars_IntSetChar(PyObject *self, PyObject *args) {
	char *cls, *action;
	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &cls, &action)) {
		return NULL;
	}
	return PyInt_FromLong(!Chars_Select(cls, action));
}

/* ---- the resources: Chars.CharRes, a list of [class, discardable] that other scripts fill too ---- */

/* the global CharRes of the module (NameError when it is missing, as the script reads it) */
static PyObject *Chars_ResList(void) {
	return Script_Global("Chars", "CharRes");
}

/*
 * AddNewCharRes(cr, Discardable): [cr, Discardable] into Chars.CharRes unless [cr, 0] is there, or [cr, 1] is there
 * and Discardable is true (a lasting one replaces the discardable one): 1 when it goes in, 0 when not, -1 with the
 * error set
 */
static int Chars_AddRes(PyObject *cls, PyObject *disc) {
	scriptFrame_t f = { 0 };
	int add = 0;
	int in0 = Sf_In(&f, Sf_Build(&f, "[Oi]", cls, 0), Sf_Global(&f, "Chars", "CharRes"));
	if (in0 == 0) {
		int in1 = Sf_In(&f, Sf_Build(&f, "[Oi]", cls, 1), Sf_Global(&f, "Chars", "CharRes"));
		if (in1 > 0 && Sf_True(&f, disc) == 0) {
			Sf_Method(&f, Sf_Global(&f, "Chars", "CharRes"), "remove", "(O)", Sf_Build(&f, "[Oi]", cls, 1));
			add = 1;
		} else if (in1 == 0) {
			add = 1;
		}
	}
	if (add) {
		Sf_Method(&f, Sf_Global(&f, "Chars", "CharRes"), "append", "(O)", Sf_Build(&f, "[OO]", cls, disc));
	}
	byte failed = f.failed;
	Sf_Done(&f, NULL);
	return failed ? -1 : add;
}

static PyObject *Chars_AddNewCharRes(PyObject *self, PyObject *args) {
	PyObject *cls, *disc = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &cls, &disc)) {
		return NULL;
	}
	PyObject *one = disc ? NULL : PyInt_FromLong(1);
	int r = Chars_AddRes(cls, disc ? disc : one);
	Py_XDECREF(one);
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* LocalLoadRes(cr): exec("import "+cr[0]+";"+cr[0]+".Init()"), then the class gets Discardable cr[1] (SAct.SetCls
 * without the class fails without an exception: ceval's SystemError, as in the script) */
static PyObject *Chars_LocalLoadRes(PyObject *self, PyObject *args) {
	PyObject *cr;
	SF_ARGS("O", &cr);
	PyObject *loc = Sf_Build(&f, "{s:O}", "cr", cr);
	PyObject *a = Sf_Cat(&f, "sO", "import ", Sf_Item(&f, cr, 0));
	Sf_Exec(&f, Sf_Cat(&f, "OsOs", a, ";", Sf_Item(&f, cr, 0), ".Init()"), "Chars", loc);
	Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Item(&f, cr, 0));
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "Discardable", Sf_Item(&f, cr, 1));
	return Sf_None(&f);
}

/* LoadRes(cls): a discardable class, loaded when it is new */
static PyObject *Chars_LoadRes(PyObject *self, PyObject *args) {
	PyObject *cls;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	PyObject *one = PyInt_FromLong(1);
	int a = one ? Chars_AddRes(cls, one) : -1;
	Py_XDECREF(one);
	if (a < 0) {
		return NULL;
	}
	if (a) {
		PyObject *f = Script_Global("Chars", "LocalLoadRes");
		PyObject *r = f ? Script_Invoke(f, Py_BuildValue("([Oi])", cls, 1)) : NULL;
		Py_XDECREF(f);
		if (!r) {
			return NULL;
		}
		Py_DECREF(r);
	}
	return Py_ReturnNone();
}

/* DiscardRes(cr): [cr, 1] in CharRes; when it is, TryToDiscardThis([cr, 1]) is scheduled now */
static PyObject *Chars_DiscardRes(PyObject *self, PyObject *args) {
	PyObject *cr;
	SF_ARGS("O", &cr);
	int in = Sf_In(&f, Sf_Build(&f, "[Oi]", cr, 1), Sf_Global(&f, "Chars", "CharRes"));
	if (in > 0) {
		Sf_Schedule(&f, Sf_Time(&f), Sf_Global(&f, "Chars", "TryToDiscardThis"), Sf_Build(&f, "([Oi])", cr, 1), NULL);
	}
	return Sf_Return(&f, Sf_Int(&f, in));
}

/* TryToDiscardThis(res): SAct.DelClass(res[0]); the class leaves CharRes when it is gone (SAct.GetClass(res[0]) == 0) */
static PyObject *Chars_TryToDiscardThis(PyObject *self, PyObject *args) {
	PyObject *res;
	SF_ARGS("O", &res);
	Sf_Ask(&f, "SAct", "DelClass", "(O)", Sf_Item(&f, res, 0));
	if (Sf_EqI(&f, Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Item(&f, res, 0)), 0) > 0) {
		Sf_Method(&f, Sf_Global(&f, "Chars", "CharRes"), "remove", "(O)", res);
	}
	return Sf_None(&f);
}

/* ---- the player ---- */

/* CanDrive(char): the character can drive a ship */
static PyObject *Chars_CanDrive(PyObject *self, PyObject *args) {
	PyObject *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	PyObject *act = Script_GetAttr(ch, "ActCtrl");
	PyObject *list = act ? Script_Global("Chars", "CanDriveList") : NULL;
	int r = list ? PySequence_Contains(list, act) : -1;
	Py_XDECREF(list);
	Py_XDECREF(act);
	if (r < 0) {
		return NULL;
	}
	return PyInt_FromLong(r);
}

/* SetOnFloor(char) */
static PyObject *Chars_SetOnFloor(PyObject *self, PyObject *args) {
	PyObject *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	PyObject *name = Script_GetAttr(ch, "Name");
	byte ok = name && Script_Try("SLogic", "SetOnFloor", "(O)", name);
	Py_XDECREF(name);
	return ok ? Py_ReturnNone() : NULL;
}

/* MainCharDeath(name, param): the hook of a mission, else the end of the game */
static PyObject *Chars_MainCharDeath(PyObject *self, PyObject *args) {
	PyObject *name, *param;
	SF_ARGS("OO", &name, &param);
	PyObject *hook = Sf_Global(&f, "Chars", "OnMainCharDeath");
	int t = Sf_True(&f, hook);
	if (t > 0) {
		Sf_Call(&f, hook, "(OO)", name, param);
	} else if (t == 0) {
		Sf_Import(&f, "MissionsFuncs");
		Sf_Ask(&f, "MissionsFuncs", "FinishGame", "(i)", 0);
	}
	return Sf_None(&f);
}

static PyObject *Chars_CreatePlayersList(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Players", "OnDeath", "Chars.MainCharDeath");
	return Sf_None(&f);
}

/* module.func(args), args stolen: the result (a new reference, NULL with the error left set for the caller) */
static PyObject *Chars_Try(const char *module, const char *func, PyObject *args) {
	PyObject *m = PyImport_ImportModule((char *)module);
	/* a function of this module as the script calls it: its global (NameError when missing) */
	PyObject *f = !m ? NULL : !strcmp(module, "Chars") ? Script_Global(module, func) : PyObject_GetAttrString(m, (char *)func);
	PyObject *r = f && args ? Script_CallObject(f, args, NULL) : NULL;
	Py_XDECREF(args);
	Py_XDECREF(f);
	Py_XDECREF(m);
	return r;
}

/* Init(): the characters of the level: the NPC spawn points, Sputnik outdoors and in the junkyard, the resources
 * so far (from now on lasting), then the class of the player */
static PyObject *Chars_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	/* an error stops it and goes to the caller, as in the script (GDB: the save game's character is not loaded) */
	PyObject *r = Chars_Try("Chars", "CreatePlayersList", PyTuple_New(0));
	if (r && !Script_GetInt("Init", "OutMapSkip", 0)) {
		Py_DECREF(r);
		r = Chars_Try("CharsNPC", "BrowseNPC", PyTuple_New(0));
	}
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	if (Script_GetInt("Init", "Outdoor", 0) || Script_GetInt("Init", "isJunkYard", 0)) {
		PyObject *s = PyString_FromString("Sputnik"), *zero = PyInt_FromLong(0);
		int a = s && zero ? Chars_AddRes(s, zero) : -1;
		Py_XDECREF(s);
		Py_XDECREF(zero);
		if (a < 0) {
			return NULL;
		}
	}
	PyObject *list = Chars_ResList();
	if (!list) {
		return NULL;
	}
	/* the list may grow while the scripts load: the length of each turn, as the for loop of the script */
	for (int i = 0; i < PySequence_Length(list); i++) {
		PyObject *cr = PySequence_GetItem(list, i);
		r = cr ? Chars_Try("Chars", "LocalLoadRes", Py_BuildValue("(O)", cr)) : NULL;
		PyObject *zero = r ? PyInt_FromLong(0) : NULL;
		if (!zero || PySequence_SetItem(cr, 1, zero) < 0) {
			Py_XDECREF(zero);
			Py_XDECREF(r);
			Py_XDECREF(cr);
			Py_DECREF(list);
			return NULL;
		}
		Py_DECREF(zero);
		Py_DECREF(r);
		Py_DECREF(cr);
	}
	Py_DECREF(list);
	const char *ch = SaveVar_Get("Char");
	r = Chars_Try("Chars", "LoadRes", Py_BuildValue("(s)", ch ? ch : ""));
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	if (!Chars_PrintLine("Character system initialized.")) {
		return NULL;
	}
	return Py_ReturnNone();
}


/* CreateMainChar(CharName, CharType, Pos, Ang): the character of the player (made when it is not there), the camera
 * on it, the controls and the HUD on foot; None after the print of a character of another kind */
static PyObject *Chars_CreateMainChar(PyObject *self, PyObject *args) {
	PyObject *name, *type, *pos, *ang;
	SF_ARGS("OOOO", &name, &type, &pos, &ang);
	Sf_Import(&f, "CharScorer");
	PyObject *c = Sf_Entity(&f, name);
	int t = Sf_True(&f, c);
	if (t == 0) {
		PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
		c = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "WalkChar");
		Sf_SetAttr(&f, c, "ActCtrl", type);
		Sf_SetAttrI(&f, c, "UsrControl", 0);
		Sf_SetAttrI(&f, c, "AutoSelect", 1);
		Sf_SetAttr(&f, c, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, ang, 1), 0));
		Sf_SetAttr(&f, c, "ViewAng", ang);
		Sf_SetAttrS(&f, c, "TgType", "Green");
		Sf_SetAttrS(&f, c, "EntityList", "Players");
		Sf_SetAttrI(&f, c, "Energy", 2);
		PyObject *hook = Sf_Global(&f, "Chars", "OnCreateMainChar");
		if (Sf_True(&f, hook) > 0) {
			Sf_Call(&f, hook, "(O)", c);
		}
	} else if (t > 0) {
		int walk = Sf_True(&f, Sf_Method(&f, c, "IsType", "(s)", "WalkChar"));
		int other = walk == 0 ? 1 : walk > 0 ? Sf_Compare(&f, Sf_Attr(&f, c, "ActCtrl"), "!=", type) : -1;
		if (other > 0) {
			Sf_Print(&f, "O", Sf_Cat(&f, "sOs", "Invalid '", name, "' character created."));
			return Sf_None(&f);
		}
	}
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	PyObject *cname = Sf_Attr(&f, c, "Name");
	Sf_SetAttr(&f, cam, "MainTarget", cname);
	Sf_SetAttr(&f, cam, "Target", cname);
	Sf_SetAttrI(&f, cam, "FreeCam", 0);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	return Sf_Return(&f, c);
}

/* CreateStartupPlayer(): offline, the camera and the character of the player at the spawn point of the save game
 * (or the first deathmatch spawn), with the batteries of the save game */
static PyObject *Chars_CreateStartupPlayer(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetNetFlags", "()"), "==", Sf_Build(&f, "(iii)", 0, 0, 0)) <= 0) {
		return Sf_None(&f);
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(Oi)", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn"), 1);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	if (Sf_True(&f, dummy) == 0) {
		Sf_Print(&f, "O", Sf_Cat(&f, "Os", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn"), " Not found!!!!"));
		Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Player_Spawn*", 1);
		dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_EqS(&f, Sf_Item(&f, dummy, 0), "DM_Player_Spawn_Junkyard") > 0) {
			dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		}
		if (Sf_True(&f, dummy) == 0) {
			Sf_Print(&f, "s", "Cannot create a main char!");
			return Sf_None(&f);
		}
		Sf_Print(&f, "O", Sf_Cat(&f, "sOs", "Using ", Sf_Item(&f, dummy, 0), " indeed."));
	}
	PyObject *at = Sf_Item(&f, dummy, 1);
	PyObject *x = Sf_Item(&f, at, 0), *y = Sf_Item(&f, at, 1), *z = Sf_Item(&f, at, 2);
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "MainCam", x, y, z, "FCam");
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cam, "Name"));
	PyObject *fn = Sf_Global(&f, "Chars", "CreateMainChar");
	PyObject *ch = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char");
	PyObject *yaw = Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180);
	PyObject *c = Sf_Call(&f, fn, "(sOO(iOi))", "Player0", ch, Sf_Item(&f, dummy, 1), 0, yaw, 0);
	PyObject *energy = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "AcBatteries"), "Chars");
	if (Sf_Compare(&f, energy, "<", Sf_Float(&f, 2.0)) > 0) {
		energy = Sf_Int(&f, 2);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "AcBatteries", "2");
	}
	Sf_SetAttr(&f, c, "Energy", energy);
	if (Sf_Compare(&f, Sf_Slice(&f, Sf_Item(&f, dummy, 0), 0, 14), "!=", Sf_Str(&f, "DM_Ship_Spawn_")) > 0) {
		Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, c, "Name"));
	}
	return Sf_Return(&f, c);
}

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
