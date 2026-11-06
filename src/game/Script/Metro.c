/*
 * The metro in C (issue #2, the mission script missions/metro.py): the station doors of the levels (SetAsMetro,
 * DoorInitializator, ObjInitializator) and the map the player picks the next station on (OnUse); the trip itself is
 * an elevator ride (Elevator.UseElevator with "MoveLevel <station>") while the player walks into the train.
 *
 * The module variables stay in the module (MissionMap, OutOfMetro, WalkCharSpeed; PlayerName and CallParams, which
 * the script creates on the first use). The layout of the map is data/menus/metro.def.
 */
#include <limits.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

static const char metro_def[] = {
#embed "../../../data/menus/metro.def"
	, 0 };

#define METRO_FILE "menus/metro.def"

/* CalculateMissionMap(EntityName): the station of the main mission (its map in capitals, or the one outside the
 * metro of OutOfMetro) */
static byte Metro_MissionMap(void) {
	static const char *const mods[] = { "string", NULL };
	PyObject *sv = Script_Import(mods) ? Script_SaveVar("Mission.Map") : NULL;
	PyObject *up = sv ? Script_Ask("string", "upper", "(O)", sv) : NULL;
	Py_XDECREF(sv);
	if (!up || !Script_SetGlobal("Metro", "MissionMap", up)) {
		return 0;
	}
	scriptRefs_t r = { 0 };
	PyObject *out = Script_Keep(&r, Script_Global("Metro", "OutOfMetro"));
	PyObject *map = out ? Script_Keep(&r, Script_Global("Metro", "MissionMap")) : NULL;
	int t = Script_True(map ? Script_Keep(&r, Script_Method(out, "has_key", "(O)", map)) : NULL);
	if (t > 0) {
		map = Script_Keep(&r, Script_Global("Metro", "MissionMap"));
		PyObject *v = map ? PyObject_GetItem(out, map) : NULL;
		t = v && Script_SetGlobal("Metro", "MissionMap", v) ? 1 : -1;
	}
	Script_Done(&r, NULL);
	return t >= 0;
}

static PyObject *Metro_CalculateMissionMap(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Metro_MissionMap() ? Py_ReturnNone() : NULL;
}

/* WrongOpt(id, control), DoNothing(id, control): a station the player cannot go to */
static PyObject *Metro_WrongOpt(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* RecoverUsr(EntityName, EventName, Time): the event near the end of the door going down gives the player back */
static PyObject *Metro_RecoverUsr(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	PyObject *player = Script_Global("Metro", "PlayerName");
	PyObject *e = Script_Entity(player);
	byte ok = e && Script_HSet(e, "UsrControl", PyInt_FromLong(0));
	Py_XDECREF(e);
	Py_XDECREF(player);
	return ok ? Py_ReturnNone() : NULL;
}

/* OutDoorInitializator(DoorClass, val=0): the door of Elevator with the event Recover 0.1 s before the end of Down */
static byte Metro_OutDoor(PyObject *door, PyObject *val) {
	static const char *const mods[] = { "Elevator", NULL };
	if (!Script_Import(mods) || !Script_Try("Elevator", "DoorInitializator", "(OO)", door, val) ||
		!Script_Try("SAct", "GetAction", "(s)", "Down")) {
		return 0;
	}
	PyObject *total = Script_Ask("SAct", "GetAct", "(s)", "AnmTotalTime");
	PyObject *tenth = total ? PyFloat_FromDouble(0.1) : NULL;
	PyObject *at = tenth ? PyNumber_Subtract(total, tenth) : NULL;
	byte ok = at && Script_Try("SAct", "SetAct", "(sO)", "AddEventAtTime", at) &&
		Script_Try("SAct", "SetAct", "(ss)", "EventName", "Recover") && Script_Try("SAct", "SetAct", "(ss)", "EventFunc", "Metro.RecoverUsr");
	Py_XDECREF(at);
	Py_XDECREF(tenth);
	Py_XDECREF(total);
	return ok;
}

static PyObject *Metro_OutDoorInitializator(PyObject *self, PyObject *args) {
	PyObject *door, *val = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &door, &val)) {
		return NULL;
	}
	PyObject *zero = val ? NULL : PyInt_FromLong(0);
	byte ok = Metro_OutDoor(door, val ? val : zero);
	Py_XDECREF(zero);
	return ok ? Py_ReturnNone() : NULL;
}

/* SetUsrParams(name, Ang=0): the player walks into the train by itself at the walking speed of its character */
static PyObject *Metro_SetUsrParams(PyObject *self, PyObject *args) {
	PyObject *name, *ang = NULL;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &name, &ang) || !Script_SetGlobal("Metro", "PlayerName", Script_Ref(name))) {
		return NULL;
	}
	PyObject *ply = Script_Keep(&r, Script_Entity(name));
	PyObject *speeds = ply && Script_HSet(ply, "UsrControl", PyInt_FromLong(-1)) ? Script_Keep(&r, Script_Global("Metro", "WalkCharSpeed")) : NULL;
	PyObject *act = speeds ? Script_Keep(&r, PyObject_GetAttrString(ply, "ActCtrl")) : NULL;
	int t = Script_True(act ? Script_Keep(&r, Script_Method(speeds, "has_key", "(O)", act)) : NULL);
	PyObject *walk = NULL;
	if (t > 0) {
		speeds = Script_Keep(&r, Script_Global("Metro", "WalkCharSpeed"));
		act = speeds ? Script_Keep(&r, PyObject_GetAttrString(ply, "ActCtrl")) : NULL;
		walk = act ? PyObject_GetItem(speeds, act) : NULL;
	} else if (t == 0) {
		walk = PyFloat_FromDouble(0.0);
	}
	if (!walk || !Script_HSet(ply, "InCtrl_Walk", walk)) {
		return Script_Done(&r, NULL);
	}
	t = ang ? Script_True(ang) : 0;
	if (t > 0) {
		PyObject *cur = Script_Keep(&r, PyObject_GetAttrString(ply, "Ang"));
		PyObject *y = cur ? Script_Keep(&r, Script_Index(cur, 1)) : NULL;
		PyObject *ny = y ? Script_Keep(&r, PyNumber_Add(y, ang)) : NULL;
		t = ny && Script_HSet(ply, "Ang", Py_BuildValue("(iOi)", 0, ny, 0)) ? 1 : -1;
	}
	return Script_Done(&r, t >= 0 ? Py_ReturnNone() : NULL);
}

/* UseMetro(id, control): the station picked (control is Station_<name>): back to the game, the elevator ride, the
 * player walks in, the other characters hide */
static PyObject *Metro_UseMetro(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Elevator", "Menu", NULL };
	PyObject *id, *control;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control) || !Script_Import(mods) ||
		!Script_Try("Menu", "BackToGame", "(OO)", id, control)) {
		return NULL;
	}
	PyObject *cp[4];
	for (int i = 0; i < 4; i++) {
		cp[i] = NULL;
		if (i == 2) {
			PyObject *tail = Script_Keep(&r, PySequence_GetSlice(control, 8, INT_MAX));
			cp[i] = tail ? Script_Keep(&r, Script_SPlus("MoveLevel ", tail)) : NULL;
		} else {
			PyObject *params = Script_Keep(&r, Script_Global("Metro", "CallParams"));
			cp[i] = params ? Script_Keep(&r, Script_Index(params, i)) : NULL;
		}
		if (!cp[i]) {
			return Script_Done(&r, NULL);
		}
	}
	if (!Script_Try("Elevator", "UseElevator", "(OOOO)", cp[0], cp[1], cp[2], cp[3])) {
		return Script_Done(&r, NULL);
	}
	PyObject *params = Script_Keep(&r, Script_Global("Metro", "CallParams"));
	PyObject *first = params ? Script_Keep(&r, Script_Index(params, 0)) : NULL;
	PyObject *a = first ? Script_Keep(&r, Py_BuildValue("(O)", first)) : NULL;
	PyObject *cls = a && Script_Keep(&r, Metro_SetUsrParams(NULL, a)) ? Script_Keep(&r, Script_Ask("SAct", "GetClass", "(s)", "DoorMetro")) : NULL;
	int t = Script_True(cls);
	if (t > 0 && !Script_Try("SAct", "SetCls", "(si)", "SoundDispZ", 2800)) {
		t = -1;
	}
	PyObject *e = t >= 0 ? Script_Ask("Scrap", "GetFirst", "()") : NULL;
	while ((t = Script_True(e)) > 0) {
		PyObject *walk = Script_Method(e, "IsType", "(s)", "WalkChar");
		int hide = Script_True(walk);
		Py_XDECREF(walk);
		if (hide > 0) {
			PyObject *name = PyObject_GetAttrString(e, "Name");
			PyObject *player = name ? Script_Global("Metro", "PlayerName") : NULL;
			hide = player ? Script_Eq(name, player) : -1;
			hide = hide < 0 ? -1 : !hide;
			Py_XDECREF(player);
			Py_XDECREF(name);
		}
		PyObject *next = hide >= 0 && (hide == 0 || Script_HSet(e, "Active", PyInt_FromLong(0))) ? PyObject_GetAttrString(e, "NextInSlot") : NULL;
		Py_DECREF(e);
		e = Script_Entity(next);
		Py_XDECREF(next);
	}
	Py_XDECREF(e);
	return Script_Done(&r, t >= 0 ? Py_ReturnNone() : NULL);
}

/* CreateStation(X, Y, Name, SpriteIndex=6): a station button of the map (metro.def sections): 0 with the error set */
static byte Metro_Station(PyObject *x, PyObject *y, PyObject *name, PyObject *sprite) {
	static const char *const mods[] = { "string", "Chars", "Elevator", "Init", NULL };
	scriptRefs_t r = { 0 };
	if (!Script_Import(mods)) {
		return 0;
	}
	PyObject *up = Script_Keep(&r, Script_Ask("string", "upper", "(O)", name));
	PyObject *map = up ? Script_Keep(&r, Script_Global("Metro", "MissionMap")) : NULL;
	int mission = map ? Script_Eq(up, map) : -1;
	if (mission < 0) {
		Script_Done(&r, NULL);
		return 0;
	}
	PyObject *vars = Script_Keep(&r, Py_BuildValue("{s:O,s:O,s:O,s:O,s:i}", "x", x, "y", y, "name", name, "sprite", sprite,
		"mission", mission));
	if (!vars) {
		Script_Done(&r, NULL);
		return 0;
	}
	if (mission) {
		Script_RunFile(0, METRO_FILE, "mission", metro_def, NULL, vars);
	}
	PyObject *path = Script_Keep(&r, Script_Attr("Init", "Path"));
	PyObject *tail = path ? Script_Keep(&r, PySequence_GetSlice(path, 7, INT_MAX)) : NULL;
	PyObject *here = tail ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", tail)) : NULL;
	int usrhere = here ? Script_Eq(up, here) : -1;
	if (usrhere < 0) {
		Script_Done(&r, NULL);
		return 0;
	}
	Script_RunFile(0, METRO_FILE, "station", metro_def, NULL, vars);
	const char *section = "here";
	if (!usrhere) {
		int cantgo = Script_True(Script_Keep(&r, Script_Ask("Elevator", "isOutMap", "(O)", name)));
		if (cantgo > 0) {
			PyObject *usr = Script_Keep(&r, Script_Usr());
			int drive = Script_True(usr ? Script_Keep(&r, Script_Ask("Chars", "CanDrive", "(O)", usr)) : NULL);
			cantgo = drive < 0 ? -1 : !drive;
		}
		if (cantgo < 0) {
			Script_Done(&r, NULL);
		return 0;
		}
		section = cantgo ? "cantgo" : "go";
	}
	Script_RunFile(0, METRO_FILE, section, metro_def, NULL, vars);
	if (usrhere) {
		Script_RunFile(0, METRO_FILE, "usrhere", metro_def, NULL, vars);
	}
	Script_Done(&r, NULL);
	return 1;
}

static PyObject *Metro_CreateStation(PyObject *self, PyObject *args) {
	PyObject *x, *y, *name, *sprite = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO|O", &x, &y, &name, &sprite)) {
		return NULL;
	}
	PyObject *six = sprite ? NULL : PyInt_FromLong(6);
	byte ok = Metro_Station(x, y, name, sprite ? sprite : six);
	Py_XDECREF(six);
	return ok ? Py_ReturnNone() : NULL;
}

/* ShowHint(id, control, lostfocus), HideHint: the name of the station under the cursor */
static PyObject *Metro_ShowHint(PyObject *self, PyObject *args) {
	PyObject *id, *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &id, &control, &lost)) {
		return NULL;
	}
	PyObject *text = Script_Ask("Scrap", "GetLangStr", "(O)", control);
	byte ok = text && Script_Try("SScorer", "Set", "(OssO)", id, "StationName", "Text", text) &&
		Script_Try("SScorer", "Set", "(Ossi)", id, "StationName", "Visible", 1);
	Py_XDECREF(text);
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *Metro_HideHint(PyObject *self, PyObject *args) {
	PyObject *id, *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &id, &control, &lost)) {
		return NULL;
	}
	return Script_Try("SScorer", "Set", "(Ossi)", id, "StationName", "Visible", 0) ? Py_ReturnNone() : NULL;
}

/* the stations of the map: the centre on the map, the name, the sprite */
static const struct {
	int x, y;
	const char *name;
	int sprite;
} metro_stations[] = {
	{ 50, 228, "DownTown", 5 },
	{ 98, 134, "TownHall", 8 },
	{ 216, 214, "Press", 7 },
	{ 436, 114, "Outskirts", 5 },
	{ 333, 104, "Police", 6 },
	{ 436, 28, "Bank", 9 },
	{ 33, 71, "Commercial", 5 },
	{ 170, 41, "Temple", 10 },
	{ 468, 208, "Scrapyard", 5 },
};

/* the corner of the city map on the screen (320 - 512 / 2 - 2, the description tab + 2) */
#define METRO_MAPX 62
#define METRO_MAPY 46

/* OnUse(UsrName, ElevatorName, optId, FullOption): the use of a station door opens the map, the game time stops */
static PyObject *Metro_OnUse(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Menu", "Init", NULL };
	PyObject *usr, *elv, *opt, *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &elv, &opt, &full) || !Script_Import(mods) ||
		!Script_SetGlobal("Metro", "CallParams", Py_BuildValue("(OOOO)", usr, elv, opt, full))) {
		return NULL;
	}
	PyObject *back = Script_Attr("Menu", "BackButtonYStart");
	if (!back) {
		PyErr_Clear();
		PyErr_SetString(PyExc_ImportError, "cannot import name BackButtonYStart");
		return NULL;
	}
	byte ok = Metro_MissionMap() && Script_Try("Menu", "InitMenuSys", "(i)", 0) && Script_Try("Menu", "StartNewMenu", "(i)", 0) &&
		Script_Try("SScorer", "SetOnCancel", "(is)", 0, "Menu.BackToGame");
	for (size_t i = 0; ok && i < sizeof(metro_stations) / sizeof(metro_stations[0]); i++) {
		PyObject *x = PyInt_FromLong(metro_stations[i].x + METRO_MAPX), *y = PyInt_FromLong(metro_stations[i].y + METRO_MAPY);
		PyObject *name = PyString_FromString(metro_stations[i].name), *sprite = PyInt_FromLong(metro_stations[i].sprite);
		ok = x && y && name && sprite && Metro_Station(x, y, name, sprite);
		Py_XDECREF(sprite);
		Py_XDECREF(name);
		Py_XDECREF(y);
		Py_XDECREF(x);
	}
	if (ok) {
		PyObject *vars = Py_BuildValue("{s:O}", "back", back);
		Script_RunFile(0, METRO_FILE, "menu", metro_def, NULL, vars);
		Py_XDECREF(vars);
	}
	Py_DECREF(back);
	return ok ? Py_ReturnNone() : NULL;
}

/* SetAsMetro(EntityName): the door of the level is a metro station */
static PyObject *Metro_SetAsMetro(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Elevator", "Init", NULL };
	PyObject *name;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *path = Script_Keep(&r, Script_Attr("Init", "Path"));
	PyObject *tail = path ? Script_Keep(&r, PySequence_GetSlice(path, 7, INT_MAX)) : NULL;
	PyObject *elv = tail && Script_Try("Elevator", "ConnectElevators", "([O][O])", name, tail) ? Script_Keep(&r, Script_Entity(name)) : NULL;
	PyObject *hint = elv && Script_HSet(elv, "ArrnOpt", PyInt_FromLong(0)) ?
		Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "Station_Hint0")) : NULL;
	byte ok = hint && Script_HSet(elv, "ArrOpt0", Script_SPlus("Travel:", hint)) &&
		Script_HSet(elv, "UseFunc", PyString_FromString("Metro.OnUse"));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* DoorInitializator(DoorClass): the station door, its sound heard farther */
static PyObject *Metro_DoorInitializator(PyObject *self, PyObject *args) {
	PyObject *door;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &door)) {
		return NULL;
	}
	PyObject *one = PyInt_FromLong(1);
	byte ok = one && Metro_OutDoor(door, one) && Script_Try("SAct", "SetCls", "(si)", "SoundDispZ", 1800);
	Py_XDECREF(one);
	return ok ? Py_ReturnNone() : NULL;
}

/* ObjInitializator(elv, ExtraInfo): the door of Elevator that opens by itself */
static PyObject *Metro_ObjInitializator(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Elevator", NULL };
	PyObject *elv, *extra;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra) || !Script_Import(mods)) {
		return NULL;
	}
	byte ok = Script_Try("Elevator", "ObjInitializator", "(OO)", elv, extra) &&
		Script_HSet(elv, "OnObjSlot", PyString_FromString("!AutoDoorSlot"));
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_MetroMethods[] = {
	{ "CalculateMissionMap", Metro_CalculateMissionMap, 1, NULL },
	{ "WrongOpt", Metro_WrongOpt, 1, NULL },
	{ "RecoverUsr", Metro_RecoverUsr, 1, NULL },
	{ "OutDoorInitializator", Metro_OutDoorInitializator, 1, NULL },
	{ "SetUsrParams", Metro_SetUsrParams, 1, NULL },
	{ "UseMetro", Metro_UseMetro, 1, NULL },
	{ "CreateStation", Metro_CreateStation, 1, NULL },
	{ "DoNothing", Metro_WrongOpt, 1, NULL },
	{ "ShowHint", Metro_ShowHint, 1, NULL },
	{ "HideHint", Metro_HideHint, 1, NULL },
	{ "OnUse", Metro_OnUse, 1, NULL },
	{ "SetAsMetro", Metro_SetAsMetro, 1, NULL },
	{ "DoorInitializator", Metro_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Metro_ObjInitializator, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
