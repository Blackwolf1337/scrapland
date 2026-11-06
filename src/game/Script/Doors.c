/*
 * The Doors, Elevator and Teleport modules in C (scripts/missions/doors.py, elevator.py and teleport.py, issue #2
 * step 8a). Doors.Init makes a door at every DM_Door_<class>_<extra> dummy of the level, with its action class
 * Door<class> (closed, opened, open, close) and the initializers of DoorInfo. Elevators and teleporters are doors with
 * more actions: an elevator carries the character linked to its platform up or down to another elevator (or to
 * another level), a teleporter does the same with its effect.
 *
 * The tables and the state stay in the modules (Doors.DoorInfo and DefDoorInfo, Elevator.ELEVATORS, ElevatorStart,
 * LinkedElevator, OutMapList, the MOVE_ and ELEVATOR_ constants, the hooks OnElevatorReachDown and OnElevatorReachUp),
 * because the level scripts and other modules read and change them; the functions read them as their globals.
 * DoorInfo is filled from the table below as the script's InitDoorInfo did, with the functions of the modules.
 */
#include <limits.h>
#include <string.h>

#include "scrap/script.h"

/* ---- Doors ---- */

/* StdInitializator(Class), StdObjInitializator(obj, ExtraParam): nothing */
static PyObject *Doors_StdInitializator(PyObject *self, PyObject *args) {
	PyObject *a;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &a)) {
		return NULL;
	}
	return Py_ReturnNone();
}

static PyObject *Doors_StdObjInitializator(PyObject *self, PyObject *args) {
	PyObject *a, *b;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &a, &b)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* UsrObjInitializator(obj, ExtraParam): a door only the player opens */
static PyObject *Doors_UsrObjInitializator(PyObject *self, PyObject *args) {
	PyObject *obj, *extra;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &obj, &extra)) {
		return NULL;
	}
	Sf_SetAttrS(&f, obj, "OnObjSlot", "!AutoDoorUsrSlot");
	return Sf_None(&f);
}

/* SAct.SetAct(var, value) and SAct.SetCls(var, value) */
static void Door_SetAct(scriptFrame_t *f, const char *var, PyObject *v) {
	Sf_Ask(f, "SAct", "SetAct", "(sO)", var, v);
}

static void Door_SetActI(scriptFrame_t *f, const char *var, long i) {
	Sf_Ask(f, "SAct", "SetAct", "(sl)", var, i);
}

static void Door_SetActS(scriptFrame_t *f, const char *var, const char *s) {
	Sf_Ask(f, "SAct", "SetAct", "(ss)", var, s);
}

/* SAct.CreateAction(name) with the animation Models/Doors/<cls>/Anm/<anm>.M3D, InterpTime 0 and isLoop (in the
 * order of the script: loopFirst for the Close action) */
static void Door_Action(scriptFrame_t *f, PyObject *cls, const char *name, const char *anm, int loop, byte loopFirst) {
	Sf_Ask(f, "SAct", "CreateAction", "(s)", name);
	Door_SetAct(f, "AnmFilePath", Sf_Cat(f, "sOsss", "Models/Doors/", cls, "/Anm/", anm, ".M3D"));
	if (loopFirst) {
		Door_SetActI(f, "isLoop", loop);
		Door_SetActI(f, "InterpTime", 0);
	} else {
		Door_SetActI(f, "InterpTime", 0);
		Door_SetActI(f, "isLoop", loop);
	}
}

/* AddEventAtTime of the end of the current action (SAct.GetAct("AnmTotalTime")) */
static void Door_EventAtEnd(scriptFrame_t *f) {
	Door_SetAct(f, "AddEventAtTime", Sf_Ask(f, "SAct", "GetAct", "(s)", "AnmTotalTime"));
}

/* CreateDoorClass(DoorClass): the action class Door<class> unless it is there (1 when it was made) */
static PyObject *Doors_CreateDoorClass(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	PyObject *name = Sf_Cat(&f, "sO", "Door", cls);
	int there = Sf_True(&f, Sf_Ask(&f, "SAct", "GetClass", "(O)", name));
	if (there != 0) {
		return Sf_Done(&f, there > 0 ? PyInt_FromLong(0) : NULL);
	}
	PyObject *info = Sf_Global(&f, "Doors", "DoorInfo");
	PyObject *dinfo = Sf_True(&f, Sf_Method(&f, info, "has_key", "(O)", cls)) > 0 ? Sf_GetItem(&f, info, cls) :
		Sf_Global(&f, "Doors", "DefDoorInfo");
	int events = Sf_Compare(&f, Sf_Item(&f, dinfo, 3), "!=", Sf_Int(&f, 2));
	Sf_Ask(&f, "SAct", "CreateClass", "(O)", Sf_Cat(&f, "sO", "Door", cls));
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", Sf_Cat(&f, "sOsOs", "Models/Doors/", cls, "/", cls, ".m3d"));
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "WRadius", Sf_Item(&f, dinfo, 0));
	Door_Action(&f, cls, "Closed", "Closed", 1, 0);
	Door_Action(&f, cls, "Opened", "Opened", 1, 0);
	Door_Action(&f, cls, "Open", "Open", 0, 0);
	if (events > 0) {
		Door_EventAtEnd(&f);
		Door_SetActS(&f, "EventName", "off");
		Door_SetActI(&f, "EventDiscardable", 0);
		Door_SetActS(&f, "EventFunc", "!ChangeDoorState");
	}
	Door_Action(&f, cls, "Close", "Close", 0, 1);
	if (events > 0) {
		Door_SetActI(&f, "AddEventAtTime", 0);
		Door_SetActS(&f, "EventName", "on");
		Door_SetActS(&f, "EventFunc", "!ChangeDoorState");
		Door_SetActI(&f, "EventDiscardable", 0);
	}
	int open = Sf_EqI(&f, Sf_Item(&f, dinfo, 3), 1);
	if (open >= 0) {
		Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", open ? "Opened" : "Closed");
	}
	Sf_Call(&f, Sf_Item(&f, dinfo, 1), "(O)", cls);
	return Sf_Done(&f, PyInt_FromLong(1));
}

/* the doors of DoorInfo: radius, the class and object initializers (module and function), 0 closed 1 open 2 without
 * events, closed for the AI */
typedef struct doorInfo_s {
	const char *name;
	long radius;
	const char *initMod, *init;
	const char *objMod, *obj;
	long state, closedAI;
} doorInfo_t;

static const doorInfo_t door_infos[] = {
	{ "CityDoorA", 10000, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 1, 1 },
	{ "Elevator", 700, "Elevator", "DoorInitializator", "Elevator", "ObjInitializatorNang", 0, 1 },
	{ "TempleGBD", 1200, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "Bankinquad", 1200, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PressSlidingDoor", 1000, "Doors", "StdInitializator", "Doors", "UsrObjInitializator", 0, 1 },
	{ "PressIndustrialDoor", 800, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PressStairsDoor", 1200, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PressSegurityDoor", 250, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "Bureau", 1, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 2, 1 },
	{ "GamblinDenChupito", 400, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PoliceJail", 550, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PoliceTunel", 200, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "PoliceSegurity", 550, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 2, 1 },
	{ "TownhallDoorA", 800, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
	{ "ElevatorOrbit", 1000, "Elevator", "DoorInitializator", "Elevator", "ObjInitializator", 0, 1 },
	{ "ElevatorOrbitB", 1000, "Elevator", "DoorInitializator", "Elevator", "ObjInitializator", 0, 1 },
	{ "Metro", 2150, "Metro", "DoorInitializator", "Metro", "ObjInitializator", 0, 0 },
	{ "MetroExt", 2150, "Metro", "OutDoorInitializator", "Elevator", "ObjInitializator", 2, 1 },
	{ "Teleporter", 600, "Teleport", "DoorInitializator", "Teleport", "ObjInitializator", 0, 1 },
	{ "TempleTeleporter", 600, "Teleport", "DoorInitializator", "Teleport", "ObjInitializator", 0, 1 },
	{ "GamblinDenCrazy", 400, "Doors", "StdInitializator", "Doors", "StdObjInitializator", 0, 0 },
};

static const doorInfo_t door_outmap = { "Outmap", 10000, "Parking", "DoorInitializator", "Doors", "UsrObjInitializator", 0, 1 };
static const doorInfo_t door_datatmaker = { "DataTMaker", 500, "Messenger", "DoorInitializator", "Messenger", "ObjInitializator", 2, 1 };
static const doorInfo_t door_editship = { "EditShip", 1400, "ShipEdit", "DoorInitializator", "ShipEdit", "ObjInitializator", 0, 1 };

/* a function of the table: a global of Doors as its functions read it, else an attribute of the imported module */
static PyObject *Door_Func(scriptFrame_t *f, const char *module, const char *name) {
	return strcmp(module, "Doors") ? Sf_Mod(f, module, name) : Sf_Global(f, "Doors", name);
}

/* DoorInfo[name] = radius, init, obj, state, closedAI */
static void Door_Info(scriptFrame_t *f, const doorInfo_t *d) {
	int mark = f->n;
	PyObject *init = Door_Func(f, d->initMod, d->init);
	PyObject *obj = init ? Door_Func(f, d->objMod, d->obj) : NULL;
	PyObject *v = obj ? Sf_Build(f, "(lOOll)", d->radius, init, obj, d->state, d->closedAI) : NULL;
	PyObject *info = Sf_Global(f, "Doors", "DoorInfo");
	Sf_SetItem(f, info, Sf_Str(f, d->name), v);
	Sf_Release(f, mark);
}

/* InitDoorInfo(): the door types, outdoors the parking, indoors the data maker, the ship editor where it is */
static PyObject *Doors_InitDoorInfo(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Elevator", "Teleport", "Init", "Metro" };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
		Sf_Import(&f, mods[i]);
	}
	for (size_t i = 0; i < sizeof(door_infos) / sizeof(door_infos[0]); i++) {
		Door_Info(&f, &door_infos[i]);
	}
	PyObject *inSkip = Sf_Mod(&f, "Init", "InMapSkip");
	if (Sf_True(&f, inSkip) == 0) {
		Sf_Import(&f, "Parking");
		Door_Info(&f, &door_outmap);
	}
	if (Sf_True(&f, Sf_Mod(&f, "Init", "OutMapSkip")) == 0) {
		Sf_Import(&f, "Messenger");
		Door_Info(&f, &door_datatmaker);
	}
	int skip = Sf_True(&f, Sf_Mod(&f, "Init", "InMapSkip"));
	if (skip == 0) {
		skip = Sf_True(&f, Sf_Mod(&f, "Init", "OutMapSkip"));
	}
	int edit = skip == 0;
	if (skip > 0) {
		edit = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetNetFlags", "()"), "!=", Sf_Build(&f, "(iii)", 0, 0, 0));
	}
	if (edit > 0) {
		Sf_Import(&f, "ShipEdit");
		Door_Info(&f, &door_editship);
	}
	return Sf_None(&f);
}

/* Init(ElevatorsOnly=0): a door at every DM_Door_ dummy (with ElevatorsOnly only the doors closed for the AI, and
 * those without their open state), then Elevator.Init() */
static PyObject *Doors_Init(PyObject *self, PyObject *args) {
	PyObject *only = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &only)) {
		return NULL;
	}
	if (!only) {
		only = Sf_Int(&f, 0);
	}
	Sf_Call(&f, Sf_Global(&f, "Doors", "InitDoorInfo"), "()");
	Sf_Import(&f, "string");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Door_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *d0 = Sf_Item(&f, dummy, 0);
		PyObject *names = Sf_Ask(&f, "string", "splitfields", "(Os)", d0, "_");
		if (Sf_True(&f, only) > 0) {
			PyObject *info = Sf_Global(&f, "Doors", "DoorInfo");
			PyObject *n2 = Sf_Item(&f, names, 2);
			if (Sf_True(&f, Sf_Method(&f, info, "has_key", "(O)", n2)) == 0) {
				Sf_Release(&f, mark);
				continue;
			}
			info = Sf_Global(&f, "Doors", "DoorInfo");
			PyObject *a = Sf_GetItem(&f, info, Sf_Item(&f, names, 2));
			if (Sf_EqI(&f, Sf_Item(&f, a, 4), 0) > 0) {
				Sf_Release(&f, mark);
				continue;
			}
			PyObject *a0 = Sf_Item(&f, a, 0), *a1 = Sf_Item(&f, a, 1), *a2 = Sf_Item(&f, a, 2);
			PyObject *v = Sf_Build(&f, "(OOOiO)", a0, a1, a2, 0, Sf_Item(&f, a, 4));
			info = Sf_Global(&f, "Doors", "DoorInfo");
			Sf_SetItem(&f, info, Sf_Item(&f, names, 2), v);
		}
		if (Sf_True(&f, Sf_Call(&f, Sf_Global(&f, "Doors", "CreateDoorClass"), "(O)", Sf_Item(&f, names, 2))) > 0) {
			Sf_Import(&f, "DoorSound");
			Sf_Ask(&f, "DoorSound", "SetSlide", "(O)", Sf_Cat(&f, "sO", "Door", Sf_Item(&f, names, 2)));
		}
		PyObject *d1 = Sf_Item(&f, dummy, 1);
		PyObject *x = Sf_Item(&f, d1, 0), *y = Sf_Item(&f, d1, 1), *z = Sf_Item(&f, d1, 2);
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", d0, x, y, z, "WithLife");
		PyObject *d2 = Sf_Item(&f, dummy, 2);
		PyObject *ax = Sf_Item(&f, d2, 0), *ay = Sf_Item(&f, d2, 1);
		Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(OOi)", ax, ay, 0));
		Sf_SetAttr(&f, l, "ActCtrl", Sf_Cat(&f, "sO", "Door", Sf_Item(&f, names, 2)));
		Sf_SetAttrS(&f, l, "OnObjSlot", "!AutoDoorSlot");
		Sf_SetAttrI(&f, l, "ActiveCMask", 0);
		Sf_SetAttrI(&f, l, "MapCollision", 1);
		Sf_SetAttrI(&f, l, "UseRealTime", 1);
		/* len(names) >= 2 always holds: a dummy without the extra part raises the IndexError of the script */
		int len = Sf_Len(&f, names);
		PyObject *extra = len >= 2 ? Sf_Item(&f, names, 3) : len >= 0 ? Py_None : NULL;
		PyObject *info = Sf_Global(&f, "Doors", "DoorInfo");
		PyObject *n2 = Sf_Item(&f, names, 2);
		int has = Sf_True(&f, Sf_Method(&f, info, "has_key", "(O)", n2));
		if (has > 0) {
			info = Sf_Global(&f, "Doors", "DoorInfo");
			PyObject *di = Sf_GetItem(&f, info, Sf_Item(&f, names, 2));
			Sf_Call(&f, Sf_Item(&f, di, 2), "(OO)", l, extra);
			if (Sf_EqI(&f, Sf_Item(&f, di, 3), 1) > 0) {
				Sf_SetAttrI(&f, l, "MapCollision", 0);
			}
		} else if (has == 0) {
			Sf_Call(&f, Sf_Item(&f, Sf_Global(&f, "Doors", "DefDoorInfo"), 2), "(OO)", l, extra);
		}
		Sf_Release(&f, mark);
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "Init", "()");
	return Sf_None(&f);
}

PyMethodDef g_DoorsMethods[] = {
	{ "StdInitializator", Doors_StdInitializator, 1, NULL },
	{ "StdObjInitializator", Doors_StdObjInitializator, 1, NULL },
	{ "UsrObjInitializator", Doors_UsrObjInitializator, 1, NULL },
	{ "CreateDoorClass", Doors_CreateDoorClass, 1, NULL },
	{ "InitDoorInfo", Doors_InitDoorInfo, 1, NULL },
	{ "Init", Doors_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Elevator ---- */

/* isOutMap(MapName): an outdoor level of the single player game */
static PyObject *Elevator_isOutMap(PyObject *self, PyObject *args) {
	PyObject *map;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &map)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *up = Sf_Ask(&f, "string", "upper", "(O)", map);
	int in = Sf_In(&f, up, Sf_Global(&f, "Elevator", "OutMapList"));
	return Sf_Done(&f, in >= 0 ? PyInt_FromLong(in) : NULL);
}

/* the entity of camera 0 */
static PyObject *Elv_Cam(scriptFrame_t *f) {
	return Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
}

/* ElevatorCamera(ElevatorName): the camera rides with the elevator, the HUD of a movie */
static PyObject *Elevator_ElevatorCamera(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_SetAttr(&f, Elv_Cam(&f), "EntityLink", name);
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	return Sf_None(&f);
}

/* ElevatorBackCamera(): the camera back on the player, the HUD on foot */
static PyObject *Elevator_ElevatorBackCamera(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetAttrS(&f, Elv_Cam(&f), "EntityLink", "");
	Sf_Import(&f, "CharScorer");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "DrawNextFrame", 0);
	return Sf_None(&f);
}

/* exec("elv.ArrOpt" + `i` + "    = '" + idcad + ":'+fullcad") in ConnectElevators: elv.ArrOpt<i> = idcad + ":" +
 * fullcad made directly when idcad pasted between the quotes is itself again */
static void Elv_ExecOpt(scriptFrame_t *f, PyObject *locals, PyObject *elv, long i, PyObject *idcad, PyObject *fullcad) {
	if (!f->failed && Script_IsQuotable(idcad)) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "ArrOpt%ld", i);
		PyObject *lit = Sf_Cat(f, "Os", idcad, ":");
		PyObject *text = lit ? Sf_Keep(f, Script_StrConst(PyString_AsString(lit), PyString_Size(lit))) : NULL;
		Sf_SetAttr(f, elv, name, Sf_Op(f, text, '+', fullcad));
		return;
	}
	PyObject *code = Sf_Cat(f, "sOsOs", "elv.ArrOpt", Sf_ReprI(f, i), "    = '", idcad, ":'+fullcad");
	Sf_Check(f, !code || PyDict_SetItemString(locals, "idcad", idcad) == 0);
	Sf_Check(f, !code || PyDict_SetItemString(locals, "fullcad", fullcad) == 0);
	Sf_Exec(f, code, "Elevator", locals);
}

/* the language text of key, else the one of "Teleport" */
static PyObject *Elv_Text(scriptFrame_t *f, PyObject *key) {
	PyObject *s = Sf_Lang(f, key);
	return Sf_True(f, s) == 0 ? Sf_Lang(f, Sf_Str(f, "Teleport")) : s;
}

/* ConnectElevators(List, MapNames=(), UseUpDown=1): every elevator of the list gets the others as options (down to
 * those before it in the list, up to those after it, or teleport), then the levels of MapNames */
static PyObject *Elevator_ConnectElevators(PyObject *self, PyObject *args) {
	PyObject *list, *maps = NULL, *updown = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|OO", &list, &maps, &updown)) {
		return NULL;
	}
	if (!maps) {
		maps = Sf_Keep(&f, PyTuple_New(0));
	}
	if (!updown) {
		updown = Sf_Int(&f, 1);
	}
	for (int e = 0;; e++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, list, e);
		if (!name) {
			break;
		}
		PyObject *elv = Sf_Entity(&f, name);
		Sf_SetAttrI(&f, elv, "ArrowCanUse", 1);
		Sf_SetAttrI(&f, elv, "ArrnOpt", 0);
		PyObject *locals = Sf_Build(&f, "{sOsO}", "elv", elv, "ElevName", name);
		long i = 0;
		int ud = Sf_True(&f, updown);
		PyObject *key = ud > 0 ? Sf_Global(&f, "Elevator", "MOVE_DOWN") : Sf_Global(&f, "Elevator", "MOVE_TELE");
		for (int t = 0;; t++) {
			int tmark = f.n;
			PyObject *tgt = Sf_Next(&f, list, t);
			if (!tgt) {
				break;
			}
			int other = Sf_Compare(&f, tgt, "!=", name);
			if (other > 0) {
				PyObject *idcad = Sf_Op(&f, key, '+', tgt);
				PyObject *fullcad = Elv_Text(&f, tgt);
				Elv_ExecOpt(&f, locals, elv, i, idcad, fullcad);
				i++;
			} else if (other == 0 && Sf_True(&f, updown) > 0) {
				key = Sf_Global(&f, "Elevator", "MOVE_UP");
				tmark = f.n;
			}
			Sf_Release(&f, tmark);
		}
		for (int m = 0;; m++) {
			int mmark = f.n;
			PyObject *map = Sf_Next(&f, maps, m);
			if (!map) {
				break;
			}
			PyObject *idcad = Sf_Op(&f, Sf_Global(&f, "Elevator", "MOVE_LEVEL"), '+', map);
			PyObject *fullcad = Elv_Text(&f, Sf_Cat(&f, "Os", map, "Goto"));
			Elv_ExecOpt(&f, locals, elv, i, idcad, fullcad);
			i++;
			Sf_Release(&f, mmark);
		}
		Sf_SetAttrS(&f, elv, "UseFunc", "Elevator.UseElevator");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* an action of the elevator class: the animation Models/Doors/<cls>/Anm/<anm>.M3D */
static void Elv_Action(scriptFrame_t *f, PyObject *cls, const char *name, int loop, byte loopFirst) {
	Door_Action(f, cls, name, name, loop, loopFirst);
}

/* DoorInitializator(DoorClass, UseDefaultDoorFunc=0): the moving actions of an elevator class (on the current
 * class of SAct) and the events of its arrival, its closing and its opening */
static PyObject *Elevator_DoorInitializator(PyObject *self, PyObject *args) {
	PyObject *cls, *def = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &cls, &def)) {
		return NULL;
	}
	if (!def) {
		def = Sf_Int(&f, 0);
	}
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "VRadius", 100);
	Elv_Action(&f, cls, "Upstairs", 1, 0);
	Elv_Action(&f, cls, "Up", 0, 0);
	Door_EventAtEnd(&f);
	Door_SetActS(&f, "EventName", "ReachUp");
	Door_SetActS(&f, "EventFunc", "Elevator.ElevatorReachUp");
	Elv_Action(&f, cls, "Down", 0, 1);
	Door_EventAtEnd(&f);
	Door_SetActS(&f, "EventName", "ReachDown");
	Door_SetActS(&f, "EventFunc", "Elevator.ElevatorReachDown");
	Sf_Ask(&f, "SAct", "GetAction", "(s)", "Close");
	Door_EventAtEnd(&f);
	int d = Sf_True(&f, def);
	if (d >= 0) {
		Door_SetActS(&f, "EventName", d ? "!AutoDoorSlot" : "!Usr0AutoDoorSlot");
	}
	Door_SetActS(&f, "EventFunc", "Elevator.ElevatorHaveClose");
	Sf_Ask(&f, "SAct", "GetAction", "(s)", "Open");
	Door_EventAtEnd(&f);
	Door_SetActS(&f, "EventName", "!AutoDoorSlot");
	Door_SetActS(&f, "EventFunc", "Elevator.ElevatorHaveOpen");
	return Sf_None(&f);
}

static void Elv_ObjInit(scriptFrame_t *f, PyObject *elv) {
	Sf_SetAttrS(f, elv, "OnObjSlot", "!Usr0AutoDoorSlot");
	Sf_SetAttrI(f, elv, "AlwaysUse", 1);
}

/* ObjInitializator(elv, ExtraInfo): the elevator opens for the player only and is always usable */
static PyObject *Elevator_ObjInitializator(PyObject *self, PyObject *args) {
	PyObject *elv, *extra;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra)) {
		return NULL;
	}
	Elv_ObjInit(&f, elv);
	return Sf_None(&f);
}

/* ObjInitializatorNang(elv, ExtraInfo): the same, selected by the absolute angle */
static PyObject *Elevator_ObjInitializatorNang(PyObject *self, PyObject *args) {
	PyObject *elv, *extra;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra)) {
		return NULL;
	}
	Sf_Call(&f, Sf_Global(&f, "Elevator", "ObjInitializator"), "(OO)", elv, extra);
	Sf_SetAttrI(&f, elv, "UseAngSel", 0);
	Sf_SetAttrI(&f, elv, "UseAbsAngSel", 1);
	return Sf_None(&f);
}

/* the elevator entity name gets action and loop (OnObjSlot emptied first) */
static void Elv_Move(scriptFrame_t *f, PyObject *name, const char *action, const char *loop) {
	PyObject *el = Sf_Entity(f, name);
	Sf_SetAttrS(f, el, "OnObjSlot", "");
	Sf_SetAttrS(f, el, "Action", action);
	Sf_SetAttrS(f, el, "DefLoop", loop);
}

/* optId[0:len(prefix)] == prefix with the prefix a global of the module (1, 0 or -1) and the prefix */
static int Elv_Prefix(scriptFrame_t *f, PyObject *opt, const char *var, PyObject **prefix) {
	*prefix = Sf_Global(f, "Elevator", var);
	int n = Sf_Len(f, *prefix);
	return n >= 0 ? Sf_Compare(f, Sf_Slice(f, opt, 0, n), "==", *prefix) : -1;
}

/* optId[len(prefix):] */
static PyObject *Elv_Rest(scriptFrame_t *f, PyObject *opt, PyObject *prefix) {
	int n = Sf_Len(f, prefix);
	return n >= 0 ? Sf_Slice(f, opt, n, INT_MAX) : NULL;
}

/* UseElevator(UsrName, ElevatorName, optId, FullOption): the option of the elevator: to another level (fade out,
 * the doors close and it goes up), up or down to another elevator, or to another teleporter */
static PyObject *Elevator_UseElevator(PyObject *self, PyObject *args) {
	PyObject *usrName, *elvName, *opt, *full, *prefix;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &elvName, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "RemainTalkTime", 0);
	int is = Elv_Prefix(&f, opt, "MOVE_LEVEL", &prefix);
	PyObject *idcall = NULL, *tgt = NULL;
	if (is > 0) {
		idcall = Sf_Global(&f, "Elevator", "ELEVATOR_NEXTLEVEL");
		tgt = Elv_Rest(&f, opt, Sf_Global(&f, "Elevator", "MOVE_LEVEL"));
		PyObject *usr = Sf_Entity(&f, usrName);
		int out = Sf_True(&f, Sf_Call(&f, Sf_Global(&f, "Elevator", "isOutMap"), "(O)", tgt));
		if (out > 0) {
			out = Sf_True(&f, Sf_Ask(&f, "Chars", "CanDrive", "(O)", usr)) == 0;
		}
		if (out > 0) {
			PyObject *msg = Sf_Lang(&f, Sf_Str(&f, "CantDrive"));
			Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", msg, Sf_OpI(&f, Sf_Time(&f), '+', 3));
			return Sf_None(&f);
		}
		PyObject *el = Sf_Entity(&f, elvName);
		Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Attr(&f, el, "ActCtrl"));
		Sf_Ask(&f, "SAct", "GetAction", "(s)", "Close");
		PyObject *timmy = Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime");
		Sf_Ask(&f, "SAct", "GetAction", "(s)", "Up");
		timmy = Sf_Op(&f, timmy, '+', Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime"));
		Sf_Ask(&f, "SScorer", "Fade", "(i(iiii)(iiii)O)", 0, 0, 0, 0, 0, 0, 0, 0, 255, timmy);
		Sf_Call(&f, Sf_Global(&f, "Elevator", "ElevatorCloseToGo"), "(O)", elvName);
		Sf_Call(&f, Sf_Global(&f, "Elevator", "LinkToElevator"), "(OO(OO)i)", usrName, elvName, idcall, tgt, 1);
		Sf_SetAttrI(&f, usr, "Invulnerable", 1);
		return Sf_None(&f);
	}
	static const char *const moves[][2] = {
		{ "MOVE_UP", "ELEVATOR_UP" }, { "MOVE_DOWN", "ELEVATOR_DOWN" }, { "MOVE_TELE", "TELEPORTER_UP" },
	};
	for (int i = 0; is == 0 && i < 3; i++) {
		is = Elv_Prefix(&f, opt, moves[i][0], &prefix);
		if (is > 0) {
			tgt = Elv_Rest(&f, opt, Sf_Global(&f, "Elevator", moves[i][0]));
			idcall = Sf_Global(&f, "Elevator", moves[i][1]);
		}
	}
	if (is == 0) {
		Sf_Print(&f, "sOsOsOsOs", "Unknow command at Elevator.UseElevator(", usrName, ",", elvName, ",", opt, ",", full, ")");
		return Sf_None(&f);
	}
	Sf_SetAttrS(&f, Sf_Entity(&f, tgt), "OnObjSlot", "");
	Sf_Call(&f, Sf_Global(&f, "Elevator", "ElevatorCloseToGo"), "(O)", elvName);
	PyObject *up = Sf_Global(&f, "Elevator", "TELEPORTER_UP");
	PyObject *down = Sf_Global(&f, "Elevator", "ELEVATOR_DOWN");
	if (Sf_In(&f, idcall, Sf_Build(&f, "(OO)", up, down)) > 0) {
		Sf_Call(&f, Sf_Global(&f, "Elevator", "ElevatorActiveToGet"), "(O)", tgt);
	}
	Sf_Call(&f, Sf_Global(&f, "Elevator", "LinkToElevator"), "(OO(OO)i)", usrName, elvName, idcall, tgt, 1);
	return Sf_None(&f);
}

/* ElevatorActiveToGet(ElevatorName): the target elevator comes up */
static PyObject *Elevator_ElevatorActiveToGet(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Elv_Move(&f, name, "Up", "Upstairs");
	return Sf_None(&f);
}

/* ElevatorCloseToGo(ElevatorName): the doors close */
static PyObject *Elevator_ElevatorCloseToGo(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Elv_Move(&f, name, "Close", "Closed");
	return Sf_None(&f);
}

/* LinkedElevator.has_key(name) and LinkedElevator[name] (lel NULL when not there) */
static int Elv_Linked(scriptFrame_t *f, PyObject *name, PyObject **lel) {
	PyObject *linked = Sf_Global(f, "Elevator", "LinkedElevator");
	int has = Sf_True(f, Sf_Method(f, linked, "has_key", "(O)", name));
	*lel = has > 0 ? Sf_GetItem(f, Sf_Global(f, "Elevator", "LinkedElevator"), name) : NULL;
	return has;
}

/* lel[1][i] */
static PyObject *Elv_Opt(scriptFrame_t *f, PyObject *lel, int i) {
	return Sf_Item(f, Sf_Item(f, lel, 1), i);
}

/* lel[1][0] == <global var> */
static int Elv_OptIs(scriptFrame_t *f, PyObject *lel, const char *var) {
	return Sf_Compare(f, Elv_Opt(f, lel, 0), "==", Sf_Global(f, "Elevator", var));
}

/* the rider of lel goes to the elevator lel[1][1] with (reach, None), which gets action and loop */
static void Elv_Pass(scriptFrame_t *f, PyObject *name, PyObject *lel, const char *reach, const char *action, const char *loop) {
	Sf_Call(f, Sf_Global(f, "Elevator", "UnlinkFromElevator"), "(O)", name);
	PyObject *elv = Sf_Entity(f, Elv_Opt(f, lel, 1));
	PyObject *link = Sf_Global(f, "Elevator", "LinkToElevator");
	PyObject *rider = Sf_Item(f, lel, 0), *tgt = Elv_Opt(f, lel, 1);
	Sf_Call(f, link, "(OO(OO))", rider, tgt, Sf_Global(f, "Elevator", reach), Py_None);
	Sf_SetAttrS(f, elv, "Action", action);
	Sf_SetAttrS(f, elv, "DefLoop", loop);
}

/* ElevatorHaveClose(EntityName, EventName, Time): the doors are closed; going down the rider passes to the target,
 * else the elevator goes up */
static PyObject *Elevator_ElevatorHaveClose(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time, *lel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	if (Elv_Linked(&f, name, &lel) > 0) {
		int down = Elv_OptIs(&f, lel, "ELEVATOR_DOWN");
		if (down > 0) {
			Elv_Pass(&f, name, lel, "ELEVATOR_DOWN_REACH", "Down", "Closed");
		} else if (down == 0) {
			PyObject *el = Sf_Entity(&f, name);
			Sf_SetAttrS(&f, el, "Action", "Up");
			Sf_SetAttrS(&f, el, "DefLoop", "Upstairs");
			return Sf_None(&f);
		}
	}
	Sf_SetAttr(&f, Sf_Entity(&f, name), "OnObjSlot", event);
	return Sf_None(&f);
}

/* the hook var of the module, called with arg (the entity of that name with entity) when it is set */
static void Elv_Hook(scriptFrame_t *f, const char *var, PyObject *arg, byte entity) {
	PyObject *hook = Sf_Global(f, "Elevator", var);
	if (Sf_True(f, hook) > 0) {
		Sf_Call(f, hook, "(O)", entity ? Sf_Entity(f, arg) : arg);
	}
}

/* ElevatorReachUp(EntityName, EventName, Time): up: to the next level (way point and start), the rider passes to
 * the upper elevator, or teleports to the target; then the hook OnElevatorReachUp */
static PyObject *Elevator_ElevatorReachUp(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time, *lel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	if (Elv_Linked(&f, name, &lel) > 0) {
		PyObject *el = Sf_Entity(&f, name);
		Sf_SetAttrS(&f, el, "Action", "Down");
		Sf_SetAttrS(&f, el, "DefLoop", "Closed");
		lel = Sf_GetItem(&f, Sf_Global(&f, "Elevator", "LinkedElevator"), name);
		int next = Elv_OptIs(&f, lel, "ELEVATOR_NEXTLEVEL");
		if (next > 0) {
			Sf_Call(&f, Sf_Global(&f, "Elevator", "UnlinkFromElevator"), "(O)", name);
			Sf_Import(&f, "SaveGame");
			Sf_Import(&f, "Init");
			PyObject *level = Sf_Cat(&f, "sO", "Levels/", Elv_Opt(&f, lel, 1));
			PyObject *spawn = Sf_Cat(&f, "sO", "DM_Player_Spawn_", Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, INT_MAX));
			Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(OOO)", level, spawn, Sf_Attr(&f, el, "ActCtrl"));
			Sf_CallKw(&f, Sf_Mod(&f, "SaveGame", "StartGame"), PyTuple_New(0), Sf_Build(&f, "{si}", "CleanRevenge", 1));
		} else if (next == 0 && Elv_OptIs(&f, lel, "ELEVATOR_UP") > 0) {
			Elv_Pass(&f, name, lel, "ELEVATOR_UP_REACH", "Open", "Opened");
		}
		if (Elv_OptIs(&f, lel, "TELEPORTER_UP") > 0) {
			Elv_Pass(&f, name, lel, "TELEPORTER_UP_REACH", "Down", "Closed");
		}
	}
	Elv_Hook(&f, "OnElevatorReachUp", name, 1);
	return Sf_None(&f);
}

/* ElevatorReachDown(EntityName, EventName, Time): down: a rider that arrives gets the doors opened, else the
 * elevator opens for the player again */
static PyObject *Elevator_ElevatorReachDown(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time, *lel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	PyObject *el = Sf_Entity(&f, name);
	if (Elv_Linked(&f, name, &lel) > 0) {
		PyObject *reach = Sf_Build(&f, "(OO)", Sf_Global(&f, "Elevator", "ELEVATOR_DOWN_REACH"),
			Sf_Global(&f, "Elevator", "TELEPORTER_UP_REACH"));
		if (Sf_In(&f, Elv_Opt(&f, lel, 0), reach) > 0) {
			Sf_SetAttrS(&f, el, "Action", "Open");
			Sf_SetAttrS(&f, el, "DefLoop", "Opened");
			return Sf_None(&f);
		}
	}
	Sf_SetAttrS(&f, el, "OnObjSlot", "!Usr0AutoDoorSlot");
	return Sf_None(&f);
}

/* ElevatorHaveOpen(EntityName, EventName, Time): the doors are open, the rider is free; the hook
 * OnElevatorReachDown */
static PyObject *Elevator_ElevatorHaveOpen(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time, *lel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	PyObject *el = Sf_Entity(&f, name);
	Sf_SetAttr(&f, el, "OnObjSlot", event);
	if (Elv_Linked(&f, name, &lel) > 0) {
		Sf_Call(&f, Sf_Global(&f, "Elevator", "UnlinkFromElevator"), "(O)", name);
	}
	Elv_Hook(&f, "OnElevatorReachDown", el, 0);
	return Sf_None(&f);
}

/* LinkToElevator(UsrName, ElevatorName, optId, Instant=0): the character rides on the platform of the elevator
 * (LinkedElevator keeps the rider, the option and its UsrControl); the player gets the elevator camera, the
 * teleporter effect and no controls */
static PyObject *Elevator_LinkToElevator(PyObject *self, PyObject *args) {
	PyObject *usrName, *elvName, *opt, *instant = NULL, *tr[3];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO|O", &usrName, &elvName, &opt, &instant)) {
		return NULL;
	}
	if (!instant) {
		instant = Sf_Int(&f, 0);
	}
	Sf_Import(&f, "Chars");
	PyObject *el = Sf_Entity(&f, elvName);
	PyObject *elevs = Sf_Global(&f, "Elevator", "ELEVATORS");
	PyObject *values;
	if (Sf_True(&f, Sf_Method(&f, elevs, "has_key", "(O)", Sf_Attr(&f, el, "ActCtrl"))) > 0) {
		values = Sf_GetItem(&f, Sf_Global(&f, "Elevator", "ELEVATORS"), Sf_Attr(&f, el, "ActCtrl"));
	} else {
		PyObject *h = Sf_Global(&f, "Elevator", "TranspHeight");
		PyObject *r = Sf_Global(&f, "Elevator", "TranspCamRadius");
		PyObject *a = Sf_Global(&f, "Elevator", "TrapspCamAngle");
		values = Sf_Build(&f, "(OOO)", h, r, a);
	}
	Sf_Unpack(&f, values, 3, tr);
	PyObject *ch = Sf_Entity(&f, usrName);
	Sf_SetAttrI(&f, el, "ArrowCanUse", 0);
	PyObject *ctrl = Sf_Attr(&f, ch, "UsrControl");
	Sf_SetItem(&f, Sf_Global(&f, "Elevator", "LinkedElevator"), elvName, Sf_Build(&f, "(OOO)", usrName, opt, ctrl));
	if (Sf_EqI(&f, Sf_Attr(&f, ch, "UsrControl"), 0) > 0) {
		if (Sf_EqS(&f, Sf_Attr(&f, el, "ActCtrl"), "DoorTeleporter") > 0) {
			Sf_Import(&f, "Teleport");
			int one = Sf_EqI(&f, instant, 1);
			if (one >= 0) {
				Sf_Ask(&f, "Teleport", "FXAction", "(OOi)", usrName, elvName, one ? 1 : 2);
			}
		}
		Sf_Call(&f, Sf_Global(&f, "Elevator", "ElevatorCamera"), "(O)", elvName);
		instant = Sf_Int(&f, 0);
	}
	int inst = Sf_True(&f, instant);
	if (inst > 0) {
		Sf_SetAttr(&f, ch, "DeltaOffMul", Sf_Float(&f, 2.0));
	} else if (inst == 0) {
		PyObject *node = Sf_Attr(&f, el, "NT_NodePos_DC_Platform");
		PyObject *off = Sf_Build(&f, "(iOi)", 0, Sf_Attr(&f, ch, "FloorDist"), 0);
		Sf_SetAttr(&f, ch, "Pos", Sf_Ask(&f, "SVec", "Add", "(OO)", node, off));
		PyObject *ang = Sf_Attr(&f, el, "Ang");
		Sf_SetAttr(&f, ch, "Ang", Sf_Ask(&f, "SVec", "Add", "(OO)", ang, Sf_Build(&f, "(iOi)", 0, tr[2], 0)));
		Sf_SetAttr(&f, ch, "DeltaOffMul", Sf_Float(&f, 0.0));
		Sf_SetAttr(&f, ch, "OffMul", Sf_Float(&f, 1.0));
	}
	Sf_SetAttr(&f, ch, "LinkModel", Sf_Cat(&f, "Oss", Sf_Attr(&f, el, "Name"), ",", "DC_Platform"));
	Sf_SetAttr(&f, ch, "OffModPos", Sf_Build(&f, "(iOi)", 0, Sf_Attr(&f, ch, "FloorDist"), 0));
	if (Sf_EqI(&f, Sf_Attr(&f, ch, "UsrControl"), 0) > 0) {
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	}
	Sf_SetAttrS(&f, ch, "ForceAction", "Relax");
	Sf_SetAttrS(&f, ch, "ForceDefLoop", "Relax");
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, ch, "Name"));
	return Sf_None(&f);
}

/* UnlinkFromElevator(EntityName): the rider is free again (the walk controls and the camera for the player) */
static PyObject *Elevator_UnlinkFromElevator(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *rg = Sf_GetItem(&f, Sf_Global(&f, "Elevator", "LinkedElevator"), name);
	PyObject *ch = Sf_Entity(&f, Sf_Item(&f, rg, 0));
	Sf_SetAttrS(&f, ch, "LinkModel", ",");
	Sf_SetAttr(&f, ch, "DeltaOffMul", Sf_Float(&f, 0.0));
	Sf_SetAttr(&f, ch, "OffMul", Sf_Float(&f, 0.0));
	Sf_SetAttr(&f, ch, "OffModPos", Sf_Build(&f, "(iii)", 0, 0, 0));
	if (Sf_EqI(&f, Sf_Item(&f, rg, 2), 0) > 0) {
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	}
	Sf_SetAttrI(&f, Sf_Entity(&f, name), "ArrowCanUse", 1);
	Sf_DelItem(&f, Sf_Global(&f, "Elevator", "LinkedElevator"), name);
	if (Sf_EqI(&f, Sf_Attr(&f, ch, "UsrControl"), 0) > 0) {
		Sf_Call(&f, Sf_Global(&f, "Elevator", "ElevatorBackCamera"), "()");
	}
	return Sf_None(&f);
}

/* Init(): the heights and cameras of the elevator classes */
static PyObject *Elevator_Init(PyObject *self, PyObject *args) {
	static const struct {
		const char *cls;
		int h, r, a;
	} elevs[] = {
		{ "DoorMetro", 500, 2000, 180 },
		{ "DoorMetroExt", 500, 2000, 180 },
		{ "DoorTeleporter", 250, 600, 0 },
		{ "DoorElevatorOrbit", 50, 500, 0 },
		{ "DoorEditShip", 500, 1000, 180 },
	};
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (size_t i = 0; i < sizeof(elevs) / sizeof(elevs[0]); i++) {
		PyObject *v = Sf_Build(&f, "(iii)", elevs[i].h, elevs[i].r, elevs[i].a);
		Sf_SetItem(&f, Sf_Global(&f, "Elevator", "ELEVATORS"), Sf_Str(&f, elevs[i].cls), v);
	}
	return Sf_None(&f);
}

/* SetDef(Type, Name): the elevator a player coming from a door of that type arrives in */
static PyObject *Elevator_SetDef(PyObject *self, PyObject *args) {
	PyObject *type, *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &type, &name)) {
		return NULL;
	}
	Sf_SetItem(&f, Sf_Global(&f, "Elevator", "ElevatorStart"), type, name);
	return Sf_None(&f);
}

/* TryToElevatorLaunchFirst(): the hook OnElevatorReachDown with None */
static PyObject *Elevator_TryToElevatorLaunchFirst(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Elv_Hook(&f, "OnElevatorReachDown", Py_None, 0);
	return Sf_None(&f);
}

/* PlayerCreatedElevator(): the player arrives where the save variable ComeFrom says: down in the elevator of that
 * door type (with the metro parameters), at a conversion point, a phone cabin or Dtritus' desktop */
static PyObject *Elevator_PlayerCreatedElevator(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Schedule(&f, Sf_Int(&f, 0), Sf_Global(&f, "Elevator", "TryToElevatorLaunchFirst"), Sf_Keep(&f, PyTuple_New(0)), "TELF");
	PyObject *from = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "ComeFrom");
	if (Sf_EqS(&f, from, "") != 0) {
		return Sf_None(&f);
	}
	Sf_Print(&f, "sO", "ExtraComeFrom = ", from);
	int door = Sf_EqS(&f, Sf_Slice(&f, from, 0, 4), "Door");
	if (door > 0) {
		PyObject *start = Sf_Global(&f, "Elevator", "ElevatorStart");
		door = Sf_True(&f, Sf_Method(&f, start, "has_key", "(O)", Sf_Slice(&f, from, 4, INT_MAX)));
	}
	if (door > 0) {
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "TELF");
		PyObject *start = Sf_Global(&f, "Elevator", "ElevatorStart");
		PyObject *tgt = Sf_Entity(&f, Sf_GetItem(&f, start, Sf_Slice(&f, from, 4, INT_MAX)));
		if (Sf_EqS(&f, Sf_Attr(&f, tgt, "Action"), "Closed") > 0) {
			Sf_SetAttrS(&f, tgt, "OnObjSlot", "");
			Sf_SetAttrS(&f, tgt, "Action", "Down");
			Sf_SetAttrS(&f, tgt, "DefLoop", "Closed");
			PyObject *usrName = Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Name");
			PyObject *reach = Sf_Global(&f, "Elevator", "ELEVATOR_DOWN_REACH");
			Sf_Call(&f, Sf_Global(&f, "Elevator", "LinkToElevator"), "(OO(OO))", usrName, Sf_Attr(&f, tgt, "Name"), reach, Py_None);
			int metro = Sf_EqS(&f, Sf_Attr(&f, tgt, "ActCtrl"), "DoorMetro");
			int ext = metro == 0 ? Sf_EqS(&f, Sf_Attr(&f, tgt, "ActCtrl"), "DoorMetroExt") : 0;
			if (metro > 0 || ext > 0) {
				Sf_Import(&f, "Metro");
				PyObject *fn = Sf_Mod(&f, "Metro", "SetUsrParams");
				Sf_Schedule(&f, Sf_Int(&f, 0), fn, Sf_Build(&f, "(Oi)", usrName, metro > 0 ? 0 : 180), "MetroSetUsrParams");
			}
		}
		return Sf_None(&f);
	}
	if (door < 0) {
		return Sf_None(&f);
	}
	if (Sf_EqS(&f, Sf_Slice(&f, from, 0, 19), "DM_Conversion_Point") > 0) {
		PyObject *conv = Sf_Entity(&f, from);
		if (Sf_True(&f, conv) > 0) {
			Sf_Import(&f, "CharConversor");
			Sf_Ask(&f, "CharConversor", "SpawnChar", "(OO)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), conv);
		}
		return Sf_None(&f);
	}
	int phone = Sf_EqS(&f, Sf_Slice(&f, from, 0, 9), "DM_Phone_");
	if (phone > 0) {
		phone = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	}
	if (phone > 0) {
		Sf_Import(&f, "PhoneCab");
		PyObject *cab = Sf_Entity(&f, from);
		if (Sf_True(&f, cab) > 0) {
			Sf_Ask(&f, "PhoneCab", "SpawnChar", "(OO)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), cab);
		}
		return Sf_None(&f);
	}
	int desk = phone == 0 ? Sf_EqS(&f, Sf_Slice(&f, from, 0, 17), "DM_DTritusDesktop") : -1;
	if (desk > 0) {
		desk = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor")) == 0;
	}
	if (desk > 0) {
		Sf_Import(&f, "DTritusDesktop");
		Sf_Ask(&f, "DTritusDesktop", "SpawnChar", "(O)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
	}
	return Sf_None(&f);
}

PyMethodDef g_ElevatorMethods[] = {
	{ "isOutMap", Elevator_isOutMap, 1, NULL },
	{ "ElevatorCamera", Elevator_ElevatorCamera, 1, NULL },
	{ "ElevatorBackCamera", Elevator_ElevatorBackCamera, 1, NULL },
	{ "ConnectElevators", Elevator_ConnectElevators, 1, NULL },
	{ "DoorInitializator", Elevator_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Elevator_ObjInitializator, 1, NULL },
	{ "ObjInitializatorNang", Elevator_ObjInitializatorNang, 1, NULL },
	{ "UseElevator", Elevator_UseElevator, 1, NULL },
	{ "ElevatorActiveToGet", Elevator_ElevatorActiveToGet, 1, NULL },
	{ "ElevatorCloseToGo", Elevator_ElevatorCloseToGo, 1, NULL },
	{ "ElevatorHaveClose", Elevator_ElevatorHaveClose, 1, NULL },
	{ "ElevatorReachUp", Elevator_ElevatorReachUp, 1, NULL },
	{ "ElevatorReachDown", Elevator_ElevatorReachDown, 1, NULL },
	{ "ElevatorHaveOpen", Elevator_ElevatorHaveOpen, 1, NULL },
	{ "LinkToElevator", Elevator_LinkToElevator, 1, NULL },
	{ "UnlinkFromElevator", Elevator_UnlinkFromElevator, 1, NULL },
	{ "Init", Elevator_Init, 1, NULL },
	{ "SetDef", Elevator_SetDef, 1, NULL },
	{ "TryToElevatorLaunchFirst", Elevator_TryToElevatorLaunchFirst, 1, NULL },
	{ "PlayerCreatedElevator", Elevator_PlayerCreatedElevator, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Teleport ---- */

/* DoorInitializator(DoorClass): an elevator class whose closing opens for the player only */
static PyObject *Teleport_DoorInitializator(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "DoorInitializator", "(Oi)", cls, 0);
	return Sf_None(&f);
}

/* ObjInitializator(elv, ExtraInfo): an elevator that opens for everyone, the camera above it */
static PyObject *Teleport_ObjInitializator(PyObject *self, PyObject *args) {
	PyObject *elv, *extra;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "ObjInitializator", "(OO)", elv, extra);
	Sf_SetAttrI(&f, elv, "UseAngSel", 0);
	Sf_SetAttrS(&f, elv, "OnObjSlot", "!AutoDoorSlot");
	Sf_SetAttr(&f, elv, "CamTgt", Sf_Build(&f, "(iii)", 0, 150, 0));
	return Sf_None(&f);
}

/* Link(Teleport1, Teleport2): two teleporters that lead to each other */
static PyObject *Teleport_Link(PyObject *self, PyObject *args) {
	PyObject *a, *b;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &a, &b)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "((OO)[]i)", a, b, 0);
	return Sf_None(&f);
}

/* FXAction(CharName, ElevatorName, Phase): the teleport effect of the engine (phase 1 in, 2 out) */
static PyObject *Teleport_FXAction(PyObject *self, PyObject *args) {
	PyObject *ch, *elv, *phase;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ch, &elv, &phase)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetCallFunc", "(s)", "!FXCharacterTeleport");
	Sf_Ask(&f, "Scrap", "AddParams", "(O)", ch);
	Sf_Ask(&f, "Scrap", "AddParams", "(O)", elv);
	Sf_Ask(&f, "Scrap", "AddParami", "(O)", phase);
	Sf_Ask(&f, "Scrap", "Execute", "()");
	return Sf_None(&f);
}

PyMethodDef g_TeleportMethods[] = {
	{ "DoorInitializator", Teleport_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Teleport_ObjInitializator, 1, NULL },
	{ "Link", Teleport_Link, 1, NULL },
	{ "FXAction", Teleport_FXAction, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
