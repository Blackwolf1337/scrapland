/*
 * The Chars module of the game in C (issue #2, step 6a): the sound events of the animations (the character scripts
 * call ActSound and its kin a few thousand times while a level loads), the resources of the characters of a level
 * and the creation of the player. The module code (default values, the Players list) stays in the script.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scheduler.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

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

static PyObject *Chars_ResList(void) {
	PyObject *m = PyImport_ImportModule("Chars");
	PyObject *l = m ? PyObject_GetAttrString(m, "CharRes") : NULL;
	Py_XDECREF(m);
	if (!l) {
		PyErr_Clear();
	}
	return l;
}

/* [cls, discardable] is in the list: its index, -1 when not */
static int Chars_ResFind(PyObject *list, PyObject *cls, int discardable) {
	PyObject *e = Py_BuildValue("[Oi]", cls, discardable);
	int n = list ? PySequence_Length(list) : 0;
	int found = -1;
	for (int i = 0; e && i < n && found < 0; i++) {
		PyObject *x = PySequence_GetItem(list, i);
		if (x && PyObject_Compare(x, e) == 0) {
			found = i;
		}
		Py_XDECREF(x);
	}
	PyErr_Clear();
	Py_XDECREF(e);
	return found;
}

/* AddNewCharRes(cls[, discardable]): 1 when the class goes into the list */
static int Chars_AddRes(PyObject *cls, int discardable) {
	PyObject *list = Chars_ResList();
	int r = 0;
	if (!list) {
		return 0;
	}
	if (Chars_ResFind(list, cls, 0) >= 0) {
		/* a lasting one is there */
	} else if (Chars_ResFind(list, cls, 1) >= 0 && discardable) {
		/* the discardable one is there */
	} else {
		int i = Chars_ResFind(list, cls, 1);
		if (i >= 0) {
			PySequence_DelItem(list, i);
		}
		PyObject *e = Py_BuildValue("[Oi]", cls, discardable);
		PyList_Append(list, e);
		Py_XDECREF(e);
		r = 1;
	}
	Py_DECREF(list);
	return r;
}

static PyObject *Chars_AddNewCharRes(PyObject *self, PyObject *args) {
	PyObject *cls;
	int discardable = 1;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|i", &cls, &discardable)) {
		return NULL;
	}
	return PyInt_FromLong(Chars_AddRes(cls, discardable));
}

/* LocalLoadRes([cls, discardable]): the character script of the class initialises it */
static PyObject *Chars_LocalLoadRes(PyObject *self, PyObject *args) {
	PyObject *res;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &res)) {
		return NULL;
	}
	PyObject *cls = PySequence_GetItem(res, 0), *disc = PySequence_GetItem(res, 1);
	if (!cls || !disc || !PyString_Check(cls)) {
		Py_XDECREF(cls);
		Py_XDECREF(disc);
		PyErr_SetString(PyExc_TypeError, "LocalLoadRes: [class, discardable]");
		return NULL;
	}
	/* exec("import X; X.Init()") of the script: an error goes to the caller */
	PyObject *mod = PyImport_ImportModule(PyString_AsString(cls));
	PyObject *init = mod ? PyObject_GetAttrString(mod, "Init") : NULL;
	PyObject *r = init ? Script_Invoke(init, PyTuple_New(0)) : NULL;
	Py_XDECREF(init);
	Py_XDECREF(mod);
	byte ok = r != NULL;
	Py_XDECREF(r);
	if (!ok) {
		Py_DECREF(cls);
		Py_DECREF(disc);
		return NULL;
	}
	if (ok) {
		g_pAuxActClass = cActClassList_Get(g_pActClassList, PyString_AsString(cls));
		if (g_pAuxActClass) {
			double d = PyInt_Check(disc) ? (double)PyInt_AsLong(disc) : PyFloat_AsDouble(disc);
			int i = (int)d;
			cActClass_Var(g_pAuxActClass, "Discardable", 1, NULL, &d, &i);
		} else {
			Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		}
	}
	Py_DECREF(cls);
	Py_DECREF(disc);
	return Py_ReturnNone();
}

/* LoadRes(cls): a discardable class, loaded when it is new */
static PyObject *Chars_LoadRes(PyObject *self, PyObject *args) {
	PyObject *cls;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	if (Chars_AddRes(cls, 1)) {
		PyObject *m = PyImport_ImportModule("Chars");
		PyObject *f = m ? PyObject_GetAttrString(m, "LocalLoadRes") : NULL;
		PyObject *r = f ? Script_Invoke(f, Py_BuildValue("([Oi])", cls, 1)) : NULL;
		Py_XDECREF(f);
		Py_XDECREF(m);
		if (!r) {
			return NULL;
		}
		Py_DECREF(r);
	}
	return Py_ReturnNone();
}

/* DiscardRes(cls): a discardable class goes after this frame; 1 when it was one */
static PyObject *Chars_DiscardRes(PyObject *self, PyObject *args) {
	PyObject *cls;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	PyObject *list = Chars_ResList();
	byte found = list && Chars_ResFind(list, cls, 1) >= 0;
	Py_XDECREF(list);
	if (found) {
		PyObject *m = PyImport_ImportModule("Chars");
		PyObject *f = m ? PyObject_GetAttrString(m, "TryToDiscardThis") : NULL;
		PyObject *a = Py_BuildValue("([Oi])", cls, 1);
		if (f && a) {
			Scheduler_Add(&g_pWorld->scheduler, Timer_GetTime(&g_pWorld->timer), f, a, "");
		}
		PyErr_Clear();
		Py_XDECREF(a);
		Py_XDECREF(f);
		Py_XDECREF(m);
	}
	return PyInt_FromLong(found);
}

/* TryToDiscardThis([cls, discardable]): the class is deleted when nothing uses it, then it leaves the list */
static PyObject *Chars_TryToDiscardThis(PyObject *self, PyObject *args) {
	PyObject *res;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &res)) {
		return NULL;
	}
	PyObject *cls = PySequence_GetItem(res, 0);
	if (!cls || !PyString_Check(cls)) {
		Py_XDECREF(cls);
		PyErr_SetString(PyExc_TypeError, "TryToDiscardThis: [class, discardable]");
		return NULL;
	}
	cActClassList_Del(g_pActClassList, PyString_AsString(cls));
	g_pAuxActClass = cActClassList_Get(g_pActClassList, PyString_AsString(cls));
	if (!g_pAuxActClass) {
		PyObject *list = Chars_ResList();
		int i = -1, n = list ? PySequence_Length(list) : 0;
		for (int k = 0; k < n && i < 0; k++) {
			PyObject *x = PySequence_GetItem(list, k);
			if (x && PyObject_Compare(x, res) == 0) {
				i = k;
			}
			Py_XDECREF(x);
		}
		if (i >= 0) {
			PySequence_DelItem(list, i);
		} else if (list) {
			/* list.remove of the script */
			PyErr_SetString(PyExc_ValueError, "list.remove(x): x not in list");
			Py_DECREF(list);
			Py_DECREF(cls);
			return NULL;
		}
		Py_XDECREF(list);
	}
	Py_DECREF(cls);
	return Py_ReturnNone();
}

/* ---- the player ---- */

/* CanDrive(char): the character can drive a ship */
static PyObject *Chars_CanDrive(PyObject *self, PyObject *args) {
	PyObject *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	PyObject *act = PyObject_GetAttrString(ch, "ActCtrl");
	PyObject *m = PyImport_ImportModule("Chars");
	PyObject *list = m ? PyObject_GetAttrString(m, "CanDriveList") : NULL;
	int r = act && list ? PySequence_Contains(list, act) : -1;
	Py_XDECREF(list);
	Py_XDECREF(m);
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
	PyObject *name = PyObject_GetAttrString(ch, "Name");
	if (!name) {
		return NULL;
	}
	Script_CallKw("SLogic", "SetOnFloor", Py_BuildValue("(O)", name), NULL);
	Py_DECREF(name);
	return Py_ReturnNone();
}

/* MainCharDeath(name, param): the hook of a mission, else the end of the game */
static PyObject *Chars_MainCharDeath(PyObject *self, PyObject *args) {
	PyObject *name, *param;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &param)) {
		return NULL;
	}
	PyObject *m = PyImport_ImportModule("Chars");
	PyObject *hook = m ? PyObject_GetAttrString(m, "OnMainCharDeath") : NULL;
	PyErr_Clear();
	if (hook && PyObject_IsTrue(hook) > 0) {
		PyObject *r = Script_Invoke(hook, Py_BuildValue("(OO)", name, param));
		if (!r) {
			Py_DECREF(hook);
			Py_XDECREF(m);
			return NULL;
		}
		Py_DECREF(r);
	} else {
		Script_Call("MissionsFuncs", "FinishGame", "(i)", 0);
	}
	Py_XDECREF(hook);
	Py_XDECREF(m);
	return Py_ReturnNone();
}

static PyObject *Chars_CreatePlayersList(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Script_Call("Scrap", "EntityListSet", "(sss)", "Players", "OnDeath", "Chars.MainCharDeath");
	return Py_ReturnNone();
}

/* Init(): the characters of the level: the NPC spawn points, Sputnik outdoors and in the junkyard, the resources
 * so far (from now on lasting), then the class of the player */
static PyObject *Chars_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Script_Call("Chars", "CreatePlayersList", "()");
	if (!Script_GetInt("Init", "OutMapSkip", 0)) {
		Script_Call("CharsNPC", "BrowseNPC", "()");
	}
	if (Script_GetInt("Init", "Outdoor", 0) || Script_GetInt("Init", "isJunkYard", 0)) {
		PyObject *s = PyString_FromString("Sputnik");
		Chars_AddRes(s, 0);
		Py_XDECREF(s);
	}
	PyObject *list = Chars_ResList();
	/* the list may grow while the scripts load: the length of each turn, as the for loop of the script */
	for (int i = 0; list && i < PySequence_Length(list); i++) {
		PyObject *cr = PySequence_GetItem(list, i);
		if (cr) {
			Script_CallKw("Chars", "LocalLoadRes", Py_BuildValue("(O)", cr), NULL);
			PyObject *zero = PyInt_FromLong(0);
			PySequence_SetItem(cr, 1, zero);
			Py_XDECREF(zero);
		}
		PyErr_Clear();
		Py_XDECREF(cr);
	}
	Py_XDECREF(list);
	const char *ch = SaveVar_Get("Char");
	Script_Call("Chars", "LoadRes", "(s)", ch ? ch : "");
	Con_Printf("Character system initialized.\n");
	return Py_ReturnNone();
}

/* module.func(args): the result (a new reference, NULL with the error printed) */
static PyObject *Chars_Ask(const char *module, const char *func, PyObject *args) {
	PyObject *m = PyImport_ImportModule((char *)module);
	PyObject *f = m ? PyObject_GetAttrString(m, (char *)func) : NULL;
	PyObject *r = f && args ? PyEval_CallObject(f, args) : NULL;
	if (!r) {
		PyErr_Print();
	}
	Py_XDECREF(args);
	Py_XDECREF(f);
	Py_XDECREF(m);
	return r;
}

static void Chars_Set(PyObject *o, const char *name, PyObject *v) {
	if (o && v && PyObject_SetAttrString(o, (char *)name, v) < 0) {
		PyErr_Print();
	}
	Py_XDECREF(v);
}

/* CreateMainChar(name, type, pos, ang): the character of the player (made when it is not there), the camera on
 * it, the controls and the HUD on foot */
static PyObject *Chars_CreateMainChar(PyObject *self, PyObject *args) {
	char *name, *type;
	PyObject *pos, *ang;
	(void)self;
	if (!PyArg_ParseTuple(args, "ssOO", &name, &type, &pos, &ang)) {
		return NULL;
	}
	PyObject *c = Chars_Ask("Scrap", "GetEntity", Py_BuildValue("(s)", name));
	if (!c || PyObject_IsTrue(c) <= 0) {
		Py_XDECREF(c);
		PyObject *x = PySequence_GetItem(pos, 0), *y = PySequence_GetItem(pos, 1), *z = PySequence_GetItem(pos, 2);
		c = x && y && z ? Chars_Ask("Scrap", "CreateEntity", Py_BuildValue("(sOOOs)", name, x, y, z, "WalkChar")) : NULL;
		Py_XDECREF(x);
		Py_XDECREF(y);
		Py_XDECREF(z);
		if (!c) {
			return NULL;
		}
		PyObject *yaw = PySequence_GetItem(ang, 1);
		Chars_Set(c, "ActCtrl", PyString_FromString(type));
		Chars_Set(c, "UsrControl", PyInt_FromLong(0));
		Chars_Set(c, "AutoSelect", PyInt_FromLong(1));
		Chars_Set(c, "Ang", Py_BuildValue("(iOi)", 0, yaw ? yaw : Py_None, 0));
		Py_INCREF(ang);
		Chars_Set(c, "ViewAng", ang);
		Chars_Set(c, "TgType", PyString_FromString("Green"));
		Chars_Set(c, "EntityList", PyString_FromString("Players"));
		Chars_Set(c, "Energy", PyInt_FromLong(2));
		Py_XDECREF(yaw);
		PyErr_Clear();
		PyObject *m = PyImport_ImportModule("Chars");
		PyObject *hook = m ? PyObject_GetAttrString(m, "OnCreateMainChar") : NULL;
		PyErr_Clear();
		if (hook && PyObject_IsTrue(hook) > 0) {
			PyObject *r = Script_Invoke(hook, Py_BuildValue("(O)", c));
			if (!r) {
				PyErr_Print();
			}
			Py_XDECREF(r);
		}
		Py_XDECREF(hook);
		Py_XDECREF(m);
	} else {
		PyObject *r = PyObject_CallMethod(c, "IsType", "(s)", "WalkChar");
		PyObject *act = PyObject_GetAttrString(c, "ActCtrl");
		byte walk = r && PyObject_IsTrue(r) > 0;
		byte same = act && PyString_Check(act) && !strcmp(PyString_AsString(act), type);
		Py_XDECREF(r);
		Py_XDECREF(act);
		PyErr_Clear();
		if (!walk || !same) {
			Con_Printf("Invalid '%s' character created.\n", name);
			Py_DECREF(c);
			return Py_ReturnNone();
		}
	}
	PyObject *camName = Chars_Ask("Scrap", "GetCam", Py_BuildValue("(i)", 0));
	PyObject *cam = camName ? Chars_Ask("Scrap", "GetEntity", Py_BuildValue("(O)", camName)) : NULL;
	PyObject *cname = PyObject_GetAttrString(c, "Name");
	PyErr_Clear();
	if (cam && cname) {
		Py_INCREF(cname);
		Chars_Set(cam, "MainTarget", cname);
		Py_INCREF(cname);
		Chars_Set(cam, "Target", cname);
		Chars_Set(cam, "FreeCam", PyInt_FromLong(0));
	}
	Py_XDECREF(cname);
	Py_XDECREF(cam);
	Py_XDECREF(camName);
	Input_SelectActionSetNamed("Walk");
	Script_Call("CharScorer", "Create", "(i)", 0);
	return c;
}

/* CreateStartupPlayer(): offline, the camera and the character of the player at the spawn point of the save game
 * (or the first deathmatch spawn), with the batteries of the save game */
static PyObject *Chars_CreateStartupPlayer(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (g_pApp->flags & 7) {
		return Py_ReturnNone();
	}
	const char *spawn = SaveVar_Get("Spawn");
	PyObject *dummy = Chars_Ask("Scrap", "StartDummySearch", Py_BuildValue("(si)", spawn ? spawn : "", 1));
	Py_XDECREF(dummy);
	dummy = Chars_Ask("Scrap", "NextDummySearch", PyTuple_New(0));
	if (!dummy || PyObject_IsTrue(dummy) <= 0) {
		Py_XDECREF(dummy);
		Con_Printf("%s Not found!!!!\n", spawn ? spawn : "");
		PyObject *r = Chars_Ask("Scrap", "StartDummySearch", Py_BuildValue("(si)", "DM_Player_Spawn*", 1));
		Py_XDECREF(r);
		dummy = Chars_Ask("Scrap", "NextDummySearch", PyTuple_New(0));
		/* the script read dummy[0] here before it checked the dummy */
		PyObject *n = dummy ? PySequence_GetItem(dummy, 0) : NULL;
		if (n && PyString_Check(n) && !strcmp(PyString_AsString(n), "DM_Player_Spawn_Junkyard")) {
			Py_DECREF(dummy);
			dummy = Chars_Ask("Scrap", "NextDummySearch", PyTuple_New(0));
		}
		Py_XDECREF(n);
		PyErr_Clear();
		if (!dummy || PyObject_IsTrue(dummy) <= 0) {
			Py_XDECREF(dummy);
			Con_Printf("Cannot create a main char!\n");
			return Py_ReturnNone();
		}
		PyObject *dn = PySequence_GetItem(dummy, 0);
		Con_Printf("Using %s indeed.\n", dn && PyString_Check(dn) ? PyString_AsString(dn) : "");
		Py_XDECREF(dn);
	}
	PyObject *dname = PySequence_GetItem(dummy, 0), *dpos = PySequence_GetItem(dummy, 1), *dang = PySequence_GetItem(dummy, 2);
	PyObject *c = NULL;
	if (dname && dpos && dang) {
		PyObject *x = PySequence_GetItem(dpos, 0), *y = PySequence_GetItem(dpos, 1), *z = PySequence_GetItem(dpos, 2);
		PyObject *cam = Chars_Ask("Scrap", "CreateEntity", Py_BuildValue("(sOOOs)", "MainCam", x, y, z, "FCam"));
		PyObject *camName = cam ? PyObject_GetAttrString(cam, "Name") : NULL;
		if (camName) {
			Script_CallKw("Scrap", "SetCam", Py_BuildValue("(iO)", 0, camName), NULL);
		}
		PyObject *yaw = PySequence_GetItem(dang, 1);
		PyObject *half = PyInt_FromLong(180);
		PyObject *ang = yaw && half ? PyNumber_Add(yaw, half) : NULL;
		Py_XDECREF(half);
		const char *ch = SaveVar_Get("Char");
		c = ang ? Chars_Ask("Chars", "CreateMainChar", Py_BuildValue("(ssO(iOi))", "Player0", ch ? ch : "", dpos, 0, ang, 0)) : NULL;
		if (c && c != Py_None) {
			const char *bat = SaveVar_Get("AcBatteries");
			PyObject *energy = Script_Eval(bat ? bat : "");
			double e = energy && PyFloat_Check(energy) ? PyFloat_AsDouble(energy) : energy && PyInt_Check(energy) ? PyInt_AsLong(energy) : 0;
			if (e < 2.0) {
				Py_XDECREF(energy);
				energy = PyInt_FromLong(2);
				SaveVar_Set("AcBatteries", "2");
			}
			Chars_Set(c, "Energy", energy);
			if (strncmp(PyString_AsString(dname), "DM_Ship_Spawn_", 14)) {
				PyObject *cn = PyObject_GetAttrString(c, "Name");
				if (cn) {
					Script_CallKw("SLogic", "SetOnFloor", Py_BuildValue("(O)", cn), NULL);
				}
				Py_XDECREF(cn);
			}
		}
		PyErr_Clear();
		Py_XDECREF(ang);
		Py_XDECREF(yaw);
		Py_XDECREF(camName);
		Py_XDECREF(cam);
		Py_XDECREF(x);
		Py_XDECREF(y);
		Py_XDECREF(z);
	}
	Py_XDECREF(dname);
	Py_XDECREF(dpos);
	Py_XDECREF(dang);
	Py_DECREF(dummy);
	if (!c) {
		return Py_ReturnNone();
	}
	return c;
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
