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
 *
 * The three modules are native C (docs/objlib-plan.md, stage 2): their functions work on C values, read the tables
 * and the state where the script reads them and change the dicts of the modules in place (OBJLIB 5). The SAct calls
 * of the classes (the scene cases record them), Scrap.Set, UsrEntity, GetEntity, CreateEntity, GetNetFlags,
 * DeleteScheduledFuncs, the dummy search, the scheduler, SLogic.SetOnFloor, SScorer.SetMsgText and Fade and
 * SInput.SetActionSet go through their seams, and the doors, elevators and riders are handle values (OBJLIB 7). The
 * other ports (Menu, CharScorer, Chars, SaveGame, DoorSound, Metro, Teleport, CharConversor, PhoneCab, DTritusDesktop)
 * and Scrap.SetCallFunc, AddParams, AddParami and Execute stay by name (OBJLIB 3, 7); the functions of DoorInfo, the
 * hooks and the module's own functions are called as the values the script calls (a mission may replace them). An
 * option of ConnectElevators whose text does not paste between quotes runs the exec line of the script (OBJLIB 6).
 */
#include <limits.h>
#include <string.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

/* ---- Doors ---- */

/* a global of a module as its functions read it (out None before) */
static byte Dr_G(const char *module, const char *name, nativeVal_t *out) {
	return Mode_G(module, name, out);
}

/* UsrObjInitializator(obj, ExtraParam): a door only the player opens */
static byte Dr_UsrObjInitializator(const nativeVal_t *obj) {
	return LvN_SetS(obj, "OnObjSlot", "!AutoDoorUsrSlot");
}

/* SAct.SetAct(var, i), SAct.SetAct(var, "text") through their seams */
static byte Dr_SetActI(const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return LvN_SetAct(var, &v);
}

static byte Dr_SetActS(const char *var, const char *s) {
	nativeVal_t v;
	LvN_Str(&v, s);
	byte ok = LvN_SetAct(var, &v);
	Native_Free(&v);
	return ok;
}

/* SAct.CreateAction(name) with the animation Models/Doors/<cls>/Anm/<anm>.M3D, InterpTime 0 and isLoop (in the
 * order of the script: loopFirst for the Close action) */
static byte Dr_Action(const nativeVal_t *cls, const char *name, const char *anm, long loop, byte loopFirst) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *path = Mode_V(&t);
	byte ok = LvN_CreateAction(name, r) &&
		Mode_Cat(&t, path, 5, Mode_S(&t, "Models/Doors/"), cls, Mode_S(&t, "/Anm/"), Mode_S(&t, anm), Mode_S(&t, ".M3D")) &&
		LvN_SetAct("AnmFilePath", path) && (loopFirst ? Dr_SetActI("isLoop", loop) && Dr_SetActI("InterpTime", 0) :
		Dr_SetActI("InterpTime", 0) && Dr_SetActI("isLoop", loop));
	return Mode_Done(&t, ok);
}

/* AddEventAtTime of the end of the current action (SAct.GetAct("AnmTotalTime")) */
static byte Dr_EventAtEnd(void) {
	nativeVal_t v = { 0 };
	byte ok = LvN_GetAct("AnmTotalTime", &v) && LvN_SetAct("AddEventAtTime", &v);
	Native_Free(&v);
	return ok;
}

/* the text of a value as an SAct function's "s" reads it, the call of fn (GetClass, CreateClass) on it */
static byte Dr_ClassCall(byte (*fn)(const char *, nativeVal_t *), const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	return Native_ArgStr(name, 1, &s) && fn(s, out);
}

/* the info of a door class: DoorInfo[cls] when it has the key, else DefDoorInfo */
static byte Dr_Info(const nativeVal_t *cls, nativeVal_t *out) {
	nativeVal_t info = { 0 };
	int has = Mode_HasKey("Doors", "DoorInfo", 1, cls);
	byte ok = has < 0 ? 0 : has ? Dr_G("Doors", "DoorInfo", &info) && Mode_Sub(&info, cls, out) :
		Dr_G("Doors", "DefDoorInfo", out);
	Native_Free(&info);
	return ok;
}

/* CreateDoorClass(DoorClass): the action class Door<class> unless it is there (1 when it was made) */
static byte Dr_CreateDoorClass(const nativeVal_t *cls, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *there = Mode_V(&t), *dinfo = Mode_V(&t), *st = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *path = Mode_V(&t), *rad = Mode_V(&t), *st2 = Mode_V(&t), *init = Mode_V(&t);
	if (!Mode_Cat(&t, name, 2, Mode_S(&t, "Door"), cls) || !Dr_ClassCall(LvN_GetClass, name, there)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(there)) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, 1);
	}
	int events = Dr_Info(cls, dinfo) && LvN_Item(dinfo, 3, st) ? Mode_CmpI(st, "!=", 2) : -1;
	Native_Free(name);
	if (events < 0 || !Mode_Cat(&t, name, 2, Mode_S(&t, "Door"), cls) || !Dr_ClassCall(LvN_CreateClass, name, r) ||
		!Mode_Cat(&t, path, 5, Mode_S(&t, "Models/Doors/"), cls, Mode_S(&t, "/"), cls, Mode_S(&t, ".m3d")) ||
		!LvN_SetCls("ModelFilePath", path) || !LvN_Item(dinfo, 0, rad) || !LvN_SetCls("WRadius", rad) ||
		!Dr_Action(cls, "Closed", "Closed", 1, 0) || !Dr_Action(cls, "Opened", "Opened", 1, 0) ||
		!Dr_Action(cls, "Open", "Open", 0, 0) || (events && (!Dr_EventAtEnd() || !Dr_SetActS("EventName", "off") ||
		!Dr_SetActI("EventDiscardable", 0) || !Dr_SetActS("EventFunc", "!ChangeDoorState"))) ||
		!Dr_Action(cls, "Close", "Close", 0, 1) || (events && (!Dr_SetActI("AddEventAtTime", 0) ||
		!Dr_SetActS("EventName", "on") || !Dr_SetActS("EventFunc", "!ChangeDoorState") ||
		!Dr_SetActI("EventDiscardable", 0))) || !LvN_Item(dinfo, 3, st2)) {
		return Mode_Done(&t, 0);
	}
	byte open = LvN_EqI(st2, 1);
	if (!LvN_SetCls("DefAction", Mode_S(&t, open ? "Opened" : "Closed")) || !LvN_Item(dinfo, 1, init) ||
		!Mode_CallArgs(init, NULL, cls, 1)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, 1);
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

/* a function of the table as a value: a global of Doors as its functions read it, else an attribute of the imported
 * module (OBJLIB 3: the function objects) */
static byte Dr_Func(const char *module, const char *name, nativeVal_t *out) {
	return strcmp(module, "Doors") ? LvN_ModGet(module, name, out) : LvN_GObj("Doors", name, out);
}

/* DoorInfo[name] = radius, init, obj, state, closedAI (the dict changed in place) */
static byte Dr_PutInfo(const doorInfo_t *d) {
	nativeVal_t v, init = { 0 }, obj = { 0 }, key;
	if (!Dr_Func(d->initMod, d->init, &init) || !Dr_Func(d->objMod, d->obj, &obj)) {
		Native_Free(&init);
		Native_Free(&obj);
		return 0;
	}
	Native_SetEmpty(&v, NV_TUPLE);
	Native_SetInt(Native_Append(&v), d->radius);
	*Native_Append(&v) = init;
	*Native_Append(&v) = obj;
	Native_SetInt(Native_Append(&v), d->state);
	Native_SetInt(Native_Append(&v), d->closedAI);
	LvN_Str(&key, d->name);
	byte ok = Mode_DictPut("Doors", "DoorInfo", NULL, &key, &v);
	Native_Free(&key);
	Native_Free(&v);
	return ok;
}

/* `if module.var:` of another module: 1, 0, -1 with the error */
static int Dr_ModTrue(const char *module, const char *var) {
	nativeVal_t v;
	int r = LvN_ModGet(module, var, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* InitDoorInfo(): the door types, outdoors the parking, indoors the data maker, the ship editor where it is */
static byte Dr_InitDoorInfo(void) {
	static const char *const mods[] = { "Elevator", "Teleport", "Init", "Metro" };
	for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	for (size_t i = 0; i < sizeof(door_infos) / sizeof(door_infos[0]); i++) {
		if (!Dr_PutInfo(&door_infos[i])) {
			return 0;
		}
	}
	int skip = Dr_ModTrue("Init", "InMapSkip");
	if (skip < 0 || (!skip && (!Mode_Import("Parking") || !Dr_PutInfo(&door_outmap)))) {
		return 0;
	}
	skip = Dr_ModTrue("Init", "OutMapSkip");
	if (skip < 0 || (!skip && (!Mode_Import("Messenger") || !Dr_PutInfo(&door_datatmaker)))) {
		return 0;
	}
	skip = Dr_ModTrue("Init", "InMapSkip");
	if (skip == 0) {
		skip = Dr_ModTrue("Init", "OutMapSkip");
	}
	int edit = skip == 0;
	if (skip > 0) {
		nativeVal_t net = { 0 }, zero;
		Mode_Ints3(&zero, 0, 0, 0);
		edit = Mode_NetFlags(&net) ? Mode_Cmp(&net, "!=", &zero) : -1;
		Native_Free(&net);
		Native_Free(&zero);
	}
	return edit >= 0 && (!edit || (Mode_Import("ShipEdit") && Dr_PutInfo(&door_editship)));
}

/* the door of one DM_Door_<class>_<extra> dummy (only: the ElevatorsOnly of Init); 0 with the error */
static byte Dr_Door(const nativeVal_t *dummy, byte only) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *d0 = Mode_V(&t), *names = Mode_V(&t), *n2 = Mode_V(&t), *info = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *a4 = Mode_V(&t), *v = Mode_V(&t), *made = Mode_V(&t), *cls = Mode_V(&t), *d1 = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *l = Mode_V(&t), *d2 = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *ra = Mode_V(&t), *di = Mode_V(&t), *fn = Mode_V(&t), *st = Mode_V(&t);
	if (!LvN_Item(dummy, 0, d0) || !Native_Split(d0, "_", names) || !LvN_Item(names, 2, n2)) {
		return Mode_Done(&t, 0);
	}
	if (only) {
		int has = Mode_HasKey("Doors", "DoorInfo", 1, n2);
		if (has <= 0) {
			return Mode_Done(&t, has == 0);
		}
		if (!Dr_G("Doors", "DoorInfo", info) || !Mode_Sub(info, n2, a) || !LvN_Item(a, 4, a4)) {
			return Mode_Done(&t, 0);
		}
		if (LvN_EqI(a4, 0)) {
			return Mode_Done(&t, 1);
		}
		Native_SetEmpty(v, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			if (!LvN_Item(a, i, Native_Append(v))) {
				return Mode_Done(&t, 0);
			}
		}
		Native_SetInt(Native_Append(v), 0);
		Native_Copy(Native_Append(v), a4);
		if (!Mode_DictPut("Doors", "DoorInfo", NULL, n2, v)) {
			return Mode_Done(&t, 0);
		}
	}
	/* OBJLIB 3: the module's CreateDoorClass through its global, DoorSound by name */
	if (!Native_CallGlobal("Doors", "CreateDoorClass", made, "v", n2) ||
		(Native_True(made) && (!Mode_Import("DoorSound") || !Mode_Cat(&t, cls, 2, Mode_S(&t, "Door"), n2) ||
		!Native_CallMod("DoorSound", "SetSlide", NULL, "v", cls)))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(cls);
	if (!LvN_Item(dummy, 1, d1) || !LvN_Item(d1, 0, x) || !LvN_Item(d1, 1, y) || !LvN_Item(d1, 2, z) ||
		!LvN_Create(d0, x, y, z, "WithLife", l) || !LvN_Item(dummy, 2, d2)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(ang, NV_TUPLE);
	if (!LvN_Item(d2, 0, Native_Append(ang)) || !LvN_Item(d2, 1, Native_Append(ang))) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(Native_Append(ang), 0);
	if (!Mode_VSet(l, "Ang", ang) || !Mode_Cat(&t, cls, 2, Mode_S(&t, "Door"), n2) || !Mode_VSet(l, "ActCtrl", cls) ||
		!LvN_SetS(l, "OnObjSlot", "!AutoDoorSlot") || !LvN_SetI(l, "ActiveCMask", 0) || !LvN_SetI(l, "MapCollision", 1) ||
		!LvN_SetI(l, "UseRealTime", 1)) {
		return Mode_Done(&t, 0);
	}
	/* len(names) >= 2 always holds: a dummy without the extra part raises the IndexError of the script */
	nativeVal_t *args = Mode_V(&t), *extra = Mode_V(&t);
	Native_Copy(args, l);
	int len = Native_Size(names);
	if (len >= 2 && !LvN_Item(names, 3, extra)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey("Doors", "DoorInfo", 1, n2);
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	Native_Free(info);
	if (!has) {
		return Mode_Done(&t, Dr_G("Doors", "DefDoorInfo", ra) && LvN_Item(ra, 2, fn) && Mode_CallArgs(fn, NULL, args, 2));
	}
	return Mode_Done(&t, Dr_G("Doors", "DoorInfo", info) && Mode_Sub(info, n2, di) && LvN_Item(di, 2, fn) &&
		Mode_CallArgs(fn, NULL, args, 2) && LvN_Item(di, 3, st) && (!LvN_EqI(st, 1) || LvN_SetI(l, "MapCollision", 0)));
}

/* Init(ElevatorsOnly=0): a door at every DM_Door_ dummy (with ElevatorsOnly only the doors closed for the AI, and
 * those without their open state), then Elevator.Init() */
static byte Dr_Init(const nativeVal_t *onlyArg) {
	byte only = onlyArg && Native_True(onlyArg);
	if (!Native_CallGlobal("Doors", "InitDoorInfo", NULL, "") || !Mode_Import("string") ||
		!Mode_DummyStartSeam("DM_Door_*", 1)) {
		return 0;
	}
	for (;;) {
		nativeVal_t dummy = { 0 }, none;
		Native_SetNone(&none);
		int more = Mode_DummyNextSeam(&dummy) ? Mode_Cmp(&dummy, "!=", &none) : -1;
		byte ok = more > 0 && Dr_Door(&dummy, only);
		Native_Free(&dummy);
		if (more <= 0) {
			if (more < 0) {
				return 0;
			}
			break;
		}
		if (!ok) {
			return 0;
		}
	}
	return Mode_Import("Elevator") && Native_CallMod("Elevator", "Init", NULL, "");
}

/* the adapters (the arity errors of the script's functions) */
#define DR_FN0(pre, name, call) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		return PyArg_ParseTuple(args, "") ? Mode_Result(call) : NULL; \
	}

/* name(a[, b]) with the optional argument b NULL when it is not given */
#define DR_FNOPT(pre, name, fmt, n, call) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		PyObject *o[5] = { NULL, NULL, NULL, NULL, NULL }; \
		nativeVal_t a[5]; \
		(void)self; \
		if (!PyArg_ParseTuple(args, fmt, &o[0], &o[1], &o[2], &o[3], &o[4])) { \
			return NULL; \
		} \
		for (int i = 0; i < n; i++) { \
			Native_SetNone(&a[i]); \
			if (o[i]) { \
				Native_Arg(o[i], &a[i]); \
			} \
		} \
		byte ok = call; \
		for (int i = 0; i < n; i++) { \
			Native_Free(&a[i]); \
		} \
		return Mode_Result(ok); \
	}

MODE_FN(Doors, StdInitializator, 1, 1)
MODE_FN(Doors, StdObjInitializator, 2, 1)
MODE_FN(Doors, UsrObjInitializator, 2, Dr_UsrObjInitializator(&a[0]))
MODE_FN(Doors, CreateDoorClass, 1, Dr_CreateDoorClass(&a[0], out))
DR_FN0(Doors, InitDoorInfo, Dr_InitDoorInfo())
DR_FNOPT(Doors, Init, "|O", 1, Dr_Init(o[0] ? &a[0] : NULL))

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

#define ELV "Elevator"

/* isOutMap(MapName): an outdoor level of the single player game */
static byte Elv_isOutMap(const nativeVal_t *map, nativeVal_t *out) {
	nativeVal_t up = { 0 }, list = { 0 };
	int in = Mode_Import("string") && Native_Upper(map, &up) && Dr_G(ELV, "OutMapList", &list) ? Native_Contains(&list, &up) : -1;
	Native_Free(&up);
	Native_Free(&list);
	if (in < 0) {
		return 0;
	}
	Native_SetInt(out, in);
	return 1;
}

/* the entity of camera 0 with EntityLink = link */
static byte Elv_CamLink(const nativeVal_t *link) {
	nativeVal_t cam, e = { 0 };
	Mode_CamName(0, &cam);
	byte ok = Mode_EntitySeam(&cam, &e) && Mode_VSet(&e, "EntityLink", link);
	Native_Free(&cam);
	Native_Free(&e);
	return ok;
}

/* ElevatorCamera(ElevatorName): the camera rides with the elevator, the HUD of a movie (OBJLIB 3: Menu by name) */
static byte Elv_ElevatorCamera(const nativeVal_t *name) {
	return Elv_CamLink(name) && Mode_Import("Menu") && Native_CallMod("Menu", "MovieScorer", NULL, "i", 0L);
}

/* ElevatorBackCamera(): the camera back on the player, the HUD on foot (OBJLIB 3: CharScorer by name) */
static byte Elv_ElevatorBackCamera(void) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	return Elv_CamLink(&empty) && Mode_Import("CharScorer") && Native_CallMod("CharScorer", "Create", NULL, "i", 0L) &&
		Mode_GVarSetI("DrawNextFrame", 0);
}

/* exec("elv.ArrOpt" + `i` + "    = '" + idcad + ":'+fullcad") in ConnectElevators: elv.ArrOpt<i> = idcad + ":" +
 * fullcad made directly when idcad pasted between the quotes is itself again, else the exec line (OBJLIB 6) */
static byte Elv_ExecOpt(const nativeVal_t *elv, const nativeVal_t *name, long i, const nativeVal_t *idcad,
	const nativeVal_t *fullcad) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lit = Mode_V(&t), *text = Mode_V(&t), *n = Mode_V(&t), *rep = Mode_V(&t), *code = Mode_V(&t);
	PyObject *id = Native_ToPy(idcad);
	byte quotable = id && Script_IsQuotable(id);
	Py_XDECREF(id);
	if (quotable) {
		char var[0x20];
		Str_Printf(var, sizeof(var), "ArrOpt%ld", i);
		return Mode_Done(&t, Mode_Cat(&t, lit, 2, idcad, Mode_S(&t, ":")) && Native_Arith('+', lit, fullcad, text) &&
			Mode_VSet(elv, var, text));
	}
	Native_SetInt(n, i);
	Mode_Repr(n, rep);
	if (!Mode_Cat(&t, code, 5, Mode_S(&t, "elv.ArrOpt"), rep, Mode_S(&t, "    = '"), idcad, Mode_S(&t, ":'+fullcad"))) {
		return Mode_Done(&t, 0);
	}
	scriptFrame_t f = { 0 };
	PyObject *e = Native_ToPy(elv), *nm = e ? Native_ToPy(name) : NULL, *ic = nm ? Native_ToPy(idcad) : NULL;
	PyObject *fc = ic ? Native_ToPy(fullcad) : NULL, *c = fc ? Native_ToPy(code) : NULL;
	PyObject *locals = c ? Py_BuildValue("{sOsOsOsO}", "elv", e, "ElevName", nm, "idcad", ic, "fullcad", fc) : NULL;
	byte ok = locals && Sf_Exec(&f, c, ELV, locals);
	Sf_Done(&f, NULL);
	Py_XDECREF(locals);
	Py_XDECREF(c);
	Py_XDECREF(fc);
	Py_XDECREF(ic);
	Py_XDECREF(nm);
	Py_XDECREF(e);
	return Mode_Done(&t, ok);
}

/* the language text of key, else the one of "Teleport" */
static byte Elv_Text(const nativeVal_t *key, nativeVal_t *out) {
	if (!Mode_LangV(key, out)) {
		return 0;
	}
	if (!Native_True(out)) {
		Native_Free(out);
		Mode_Lang("Teleport", out);
	}
	return 1;
}

/* the options of one elevator name of ConnectElevators */
static byte Elv_Connect(const nativeVal_t *name, const nativeVal_t *list, const nativeVal_t *maps, const nativeVal_t *updown) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *elv = Mode_V(&t), *key = Mode_V(&t), *ml = Mode_V(&t), *mv = Mode_V(&t);
	long i = 0;
	if (!Mode_EntitySeam(name, elv) || !LvN_SetI(elv, "ArrowCanUse", 1) || !LvN_SetI(elv, "ArrnOpt", 0) ||
		!Dr_G(ELV, Native_True(updown) ? "MOVE_DOWN" : "MOVE_TELE", key)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(list);
	for (int k = 0; k < n; k++) {
		nativeVal_t tgt = { 0 }, idcad = { 0 }, fullcad = { 0 };
		int other = Native_SeqItem(list, k, &tgt) ? Mode_Cmp(&tgt, "!=", name) : -1;
		byte ok = other >= 0;
		if (other > 0) {
			ok = Mode_Arith('+', key, &tgt, &idcad) && Elv_Text(&tgt, &fullcad) && Elv_ExecOpt(elv, name, i, &idcad, &fullcad);
			i++;
		} else if (other == 0 && Native_True(updown)) {
			Native_Free(key);
			ok = Dr_G(ELV, "MOVE_UP", key);
		}
		Native_Free(&tgt);
		Native_Free(&idcad);
		Native_Free(&fullcad);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Copy(ml, maps);
	int m = Mode_Loop(ml);
	for (int k = 0; k < m; k++) {
		nativeVal_t map = { 0 }, idcad = { 0 }, goto_ = { 0 }, fullcad = { 0 };
		byte ok = Native_SeqItem(ml, k, &map) && Dr_G(ELV, "MOVE_LEVEL", mv) && Mode_Arith('+', mv, &map, &idcad) &&
			Mode_Cat(&t, &goto_, 2, &map, Mode_S(&t, "Goto")) && Elv_Text(&goto_, &fullcad) &&
			Elv_ExecOpt(elv, name, i, &idcad, &fullcad);
		i++;
		Native_Free(mv);
		Native_Free(&map);
		Native_Free(&idcad);
		Native_Free(&goto_);
		Native_Free(&fullcad);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, m >= 0 && LvN_SetS(elv, "UseFunc", "Elevator.UseElevator"));
}

/* ConnectElevators(List, MapNames=(), UseUpDown=1): every elevator of the list gets the others as options (down to
 * those before it in the list, up to those after it, or teleport), then the levels of MapNames */
static byte Elv_ConnectElevators(const nativeVal_t *listArg, const nativeVal_t *mapsArg, const nativeVal_t *updownArg) {
	nativeVal_t list, maps, updown;
	Native_Copy(&list, listArg);
	if (mapsArg) {
		Native_Copy(&maps, mapsArg);
	} else {
		Native_SetEmpty(&maps, NV_TUPLE);
	}
	if (updownArg) {
		Native_Copy(&updown, updownArg);
	} else {
		Native_SetInt(&updown, 1);
	}
	int n = Mode_Loop(&list);
	byte ok = n >= 0;
	for (int e = 0; ok && e < n; e++) {
		nativeVal_t name = { 0 };
		ok = Native_SeqItem(&list, e, &name) && Elv_Connect(&name, &list, &maps, &updown);
		Native_Free(&name);
	}
	Native_Free(&list);
	Native_Free(&maps);
	Native_Free(&updown);
	return ok;
}

/* DoorInitializator(DoorClass, UseDefaultDoorFunc=0): the moving actions of an elevator class (on the current
 * class of SAct) and the events of its arrival, its closing and its opening */
static byte Elv_DoorInitializator(const nativeVal_t *cls, const nativeVal_t *def) {
	nativeVal_t r = { 0 }, v;
	Native_SetInt(&v, 100);
	byte ok = LvN_SetCls("VRadius", &v) && Dr_Action(cls, "Upstairs", "Upstairs", 1, 0) && Dr_Action(cls, "Up", "Up", 0, 0) &&
		Dr_EventAtEnd() && Dr_SetActS("EventName", "ReachUp") && Dr_SetActS("EventFunc", "Elevator.ElevatorReachUp") &&
		Dr_Action(cls, "Down", "Down", 0, 1) && Dr_EventAtEnd() && Dr_SetActS("EventName", "ReachDown") &&
		Dr_SetActS("EventFunc", "Elevator.ElevatorReachDown") && LvN_GetAction("Close", &r) && Dr_EventAtEnd() &&
		Dr_SetActS("EventName", def && Native_True(def) ? "!AutoDoorSlot" : "!Usr0AutoDoorSlot") &&
		Dr_SetActS("EventFunc", "Elevator.ElevatorHaveClose") && (Native_Free(&r), LvN_GetAction("Open", &r)) &&
		Dr_EventAtEnd() && Dr_SetActS("EventName", "!AutoDoorSlot") && Dr_SetActS("EventFunc", "Elevator.ElevatorHaveOpen");
	Native_Free(&r);
	return ok;
}

/* ObjInitializator(elv, ExtraInfo): the elevator opens for the player only and is always usable */
static byte Elv_ObjInitializator(const nativeVal_t *elv) {
	return LvN_SetS(elv, "OnObjSlot", "!Usr0AutoDoorSlot") && LvN_SetI(elv, "AlwaysUse", 1);
}

/* ObjInitializatorNang(elv, ExtraInfo): the same, selected by the absolute angle */
static byte Elv_ObjInitializatorNang(const nativeVal_t *elv, const nativeVal_t *extra) {
	return Native_CallGlobal(ELV, "ObjInitializator", NULL, "vv", elv, extra) && LvN_SetI(elv, "UseAngSel", 0) &&
		LvN_SetI(elv, "UseAbsAngSel", 1);
}

/* e.Action = action; e.DefLoop = loop */
static byte Elv_Act(const nativeVal_t *e, const char *action, const char *loop) {
	return LvN_SetS(e, "Action", action) && LvN_SetS(e, "DefLoop", loop);
}

/* the elevator entity name gets action and loop (OnObjSlot emptied first) */
static byte Elv_Move(const nativeVal_t *name, const char *action, const char *loop) {
	nativeVal_t el = { 0 };
	byte ok = Mode_EntitySeam(name, &el) && LvN_SetS(&el, "OnObjSlot", "") && Elv_Act(&el, action, loop);
	Native_Free(&el);
	return ok;
}

/* optId[0:len(prefix)] == prefix with the prefix a global of the module: 1, 0 or -1 */
static int Elv_Prefix(const nativeVal_t *opt, const char *var) {
	nativeVal_t prefix = { 0 }, head = { 0 };
	int n = Dr_G(ELV, var, &prefix) ? Native_Size(&prefix) : -1;
	int r = n >= 0 && Native_Slice(opt, 0, n, &head) ? Mode_Cmp(&head, "==", &prefix) : -1;
	Native_Free(&prefix);
	Native_Free(&head);
	return r;
}

/* optId[len(prefix):] with the prefix a global of the module */
static byte Elv_Rest(const nativeVal_t *opt, const char *var, nativeVal_t *out) {
	nativeVal_t prefix = { 0 };
	int n = Dr_G(ELV, var, &prefix) ? Native_Size(&prefix) : -1;
	Native_Free(&prefix);
	return n >= 0 && Native_Slice(opt, n, INT_MAX, out);
}

/* the module's LinkToElevator(usr, elv, (idcall, tgt)[, 1]) through its global */
static byte Elv_Link(const nativeVal_t *usr, const nativeVal_t *elv, const nativeVal_t *idcall, const nativeVal_t *tgt,
	byte instant) {
	nativeVal_t opt, args[4];
	Native_SetEmpty(&opt, NV_TUPLE);
	Native_Copy(Native_Append(&opt), idcall);
	Native_Copy(Native_Append(&opt), tgt);
	args[0] = *usr;
	args[1] = *elv;
	args[2] = opt;
	Native_SetInt(&args[3], 1);
	PyObject *fn = Mode_GlobalPy(ELV, "LinkToElevator");
	byte ok = fn && Native_Call(fn, NULL, instant ? "vvvv" : "vvv", &args[0], &args[1], &args[2], &args[3]);
	Py_XDECREF(fn);
	Native_Free(&opt);
	return ok;
}

/* the level option of UseElevator: fade out, the doors close and it goes up (OBJLIB 3: Chars by name) */
static byte Elv_UseLevel(const nativeVal_t *usrName, const nativeVal_t *elvName, const nativeVal_t *opt) {
	static const int clear[4] = { 0, 0, 0, 0 }, black[4] = { 0, 0, 0, 255 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *idcall = Mode_V(&t), *tgt = Mode_V(&t), *usr = Mode_V(&t), *out = Mode_V(&t), *can = Mode_V(&t);
	nativeVal_t *msg = Mode_V(&t), *el = Mode_V(&t), *act = Mode_V(&t), *r = Mode_V(&t), *t1 = Mode_V(&t);
	nativeVal_t *t2 = Mode_V(&t), *timmy = Mode_V(&t);
	if (!Dr_G(ELV, "ELEVATOR_NEXTLEVEL", idcall) || !Elv_Rest(opt, "MOVE_LEVEL", tgt) || !Mode_EntitySeam(usrName, usr) ||
		!Native_CallGlobal(ELV, "isOutMap", out, "v", tgt)) {
		return Mode_Done(&t, 0);
	}
	int no = Native_True(out);
	if (no) {
		no = Native_CallMod("Chars", "CanDrive", can, "v", usr) ? !Native_True(can) : -1;
	}
	if (no) {
		Mode_Lang("CantDrive", msg);
		return Mode_Done(&t, no > 0 && Mode_MsgText(msg, Mode_Time() + 3));
	}
	const char *a;
	return Mode_Done(&t, Mode_EntitySeam(elvName, el) && Mode_VObj(el, "ActCtrl", act) && Native_ArgStr(act, 1, &a) &&
		LvN_GetClass(a, r) && LvN_GetAction("Close", r) && LvN_GetAct("AnmTotalTime", t1) && LvN_GetAction("Up", r) &&
		LvN_GetAct("AnmTotalTime", t2) && Mode_Arith('+', t1, t2, timmy) && Mode_FadeV(0, clear, black, timmy) &&
		Native_CallGlobal(ELV, "ElevatorCloseToGo", NULL, "v", elvName) && Elv_Link(usrName, elvName, idcall, tgt, 1) &&
		LvN_SetI(usr, "Invulnerable", 1));
}

/* UseElevator(UsrName, ElevatorName, optId, FullOption): the option of the elevator: to another level (fade out,
 * the doors close and it goes up), up or down to another elevator, or to another teleporter */
static byte Elv_UseElevator(const nativeVal_t *usrName, const nativeVal_t *elvName, const nativeVal_t *opt,
	const nativeVal_t *full) {
	static const char *const moves[][2] = {
		{ "MOVE_UP", "ELEVATOR_UP" }, { "MOVE_DOWN", "ELEVATOR_DOWN" }, { "MOVE_TELE", "TELEPORTER_UP" },
	};
	modeVals_t t = { .n = 0 };
	nativeVal_t *tgt = Mode_V(&t), *idcall = Mode_V(&t), *e = Mode_V(&t), *up = Mode_V(&t), *down = Mode_V(&t);
	nativeVal_t *pair = Mode_V(&t);
	if (!Mode_Import("Chars") || !Mode_GVarSetI("RemainTalkTime", 0)) {
		return Mode_Done(&t, 0);
	}
	int is = Elv_Prefix(opt, "MOVE_LEVEL");
	if (is) {
		return Mode_Done(&t, is > 0 && Elv_UseLevel(usrName, elvName, opt));
	}
	for (int i = 0; is == 0 && i < 3; i++) {
		is = Elv_Prefix(opt, moves[i][0]);
		if (is > 0 && (!Elv_Rest(opt, moves[i][0], tgt) || !Dr_G(ELV, moves[i][1], idcall))) {
			return Mode_Done(&t, 0);
		}
	}
	if (is == 0) {
		const nativeVal_t *items[9] = { Mode_S(&t, "Unknow command at Elevator.UseElevator("), usrName, Mode_S(&t, ","),
			elvName, Mode_S(&t, ","), opt, Mode_S(&t, ","), full, Mode_S(&t, ")") };
		return Mode_Done(&t, Mode_Print(items, 9));
	}
	if (is < 0 || !Mode_EntitySeam(tgt, e) || !LvN_SetS(e, "OnObjSlot", "") ||
		!Native_CallGlobal(ELV, "ElevatorCloseToGo", NULL, "v", elvName) || !Dr_G(ELV, "TELEPORTER_UP", up) ||
		!Dr_G(ELV, "ELEVATOR_DOWN", down)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(pair, NV_TUPLE);
	Native_Copy(Native_Append(pair), up);
	Native_Copy(Native_Append(pair), down);
	int in = Native_Contains(pair, idcall);
	return Mode_Done(&t, in >= 0 && (!in || Native_CallGlobal(ELV, "ElevatorActiveToGet", NULL, "v", tgt)) &&
		Elv_Link(usrName, elvName, idcall, tgt, 1));
}

/* LinkedElevator.has_key(name) and LinkedElevator[name] into lel when it has it: 1, 0, -1 */
static int Elv_Linked(const nativeVal_t *name, nativeVal_t *lel) {
	nativeVal_t linked = { 0 };
	int has = Mode_HasKey(ELV, "LinkedElevator", 1, name);
	if (has > 0 && (!Dr_G(ELV, "LinkedElevator", &linked) || !Mode_Sub(&linked, name, lel))) {
		has = -1;
	}
	Native_Free(&linked);
	return has;
}

/* lel[1][i] */
static byte Elv_Opt(const nativeVal_t *lel, long i, nativeVal_t *out) {
	nativeVal_t o = { 0 };
	byte ok = LvN_Item(lel, 1, &o) && LvN_Item(&o, i, out);
	Native_Free(&o);
	return ok;
}

/* lel[1][0] == <global var>: 1, 0, -1 */
static int Elv_OptIs(const nativeVal_t *lel, const char *var) {
	nativeVal_t o = { 0 }, g = { 0 };
	int r = Elv_Opt(lel, 0, &o) && Dr_G(ELV, var, &g) ? Mode_Cmp(&o, "==", &g) : -1;
	Native_Free(&o);
	Native_Free(&g);
	return r;
}

/* the rider of lel goes to the elevator lel[1][1] with (reach, None), which gets action and loop */
static byte Elv_Pass(const nativeVal_t *name, const nativeVal_t *lel, const char *reach, const char *action, const char *loop) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *o1 = Mode_V(&t), *elv = Mode_V(&t), *rider = Mode_V(&t), *tgt = Mode_V(&t), *opt = Mode_V(&t);
	if (!Native_CallGlobal(ELV, "UnlinkFromElevator", NULL, "v", name) || !Elv_Opt(lel, 1, o1) || !Mode_EntitySeam(o1, elv)) {
		return Mode_Done(&t, 0);
	}
	PyObject *link = Mode_GlobalPy(ELV, "LinkToElevator");
	Native_SetEmpty(opt, NV_TUPLE);
	byte ok = link && LvN_Item(lel, 0, rider) && Elv_Opt(lel, 1, tgt) && Dr_G(ELV, reach, Native_Append(opt)) &&
		(Native_SetNone(Native_Append(opt)), Native_Call(link, NULL, "vvv", rider, tgt, opt)) && Elv_Act(elv, action, loop);
	Py_XDECREF(link);
	return Mode_Done(&t, ok);
}

/* ElevatorHaveClose(EntityName, EventName, Time): the doors are closed; going down the rider passes to the target,
 * else the elevator goes up */
static byte Elv_ElevatorHaveClose(const nativeVal_t *name, const nativeVal_t *event) {
	nativeVal_t lel = { 0 }, el = { 0 };
	int linked = Elv_Linked(name, &lel);
	byte ok = linked >= 0;
	if (linked > 0) {
		int down = Elv_OptIs(&lel, "ELEVATOR_DOWN");
		if (down > 0) {
			ok = Elv_Pass(name, &lel, "ELEVATOR_DOWN_REACH", "Down", "Closed");
		} else if (down == 0) {
			ok = Mode_EntitySeam(name, &el) && Elv_Act(&el, "Up", "Upstairs");
			Native_Free(&lel);
			Native_Free(&el);
			return ok;
		} else {
			ok = 0;
		}
	}
	ok = ok && Mode_EntitySeam(name, &el) && Mode_VSet(&el, "OnObjSlot", event);
	Native_Free(&lel);
	Native_Free(&el);
	return ok;
}

/* the hook var of the module, called with arg (the entity of that name with entity) when it is set */
static byte Elv_Hook(const char *var, const nativeVal_t *arg, byte entity) {
	nativeVal_t hook = { 0 }, e = { 0 };
	byte ok = LvN_GObj(ELV, var, &hook) && (!Native_True(&hook) ||
		(entity ? Mode_EntitySeam(arg, &e) && Mode_CallArgs(&hook, NULL, &e, 1) : Mode_CallArgs(&hook, NULL, arg, 1)));
	Native_Free(&hook);
	Native_Free(&e);
	return ok;
}

/* up to the next level: the way point and the start (OBJLIB 3: SaveGame by name with its keyword) */
static byte Elv_NextLevel(const nativeVal_t *name, const nativeVal_t *el, const nativeVal_t *lel) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *map = Mode_V(&t), *level = Mode_V(&t), *path = Mode_V(&t), *tail = Mode_V(&t), *spawn = Mode_V(&t);
	nativeVal_t *act = Mode_V(&t);
	return Mode_Done(&t, Native_CallGlobal(ELV, "UnlinkFromElevator", NULL, "v", name) && Mode_Import("SaveGame") &&
		Mode_Import("Init") && Elv_Opt(lel, 1, map) && Mode_Cat(&t, level, 2, Mode_S(&t, "Levels/"), map) &&
		LvN_ModGet("Init", "Path", path) && Native_Slice(path, 7, INT_MAX, tail) &&
		Mode_Cat(&t, spawn, 2, Mode_S(&t, "DM_Player_Spawn_"), tail) && Mode_VObj(el, "ActCtrl", act) &&
		Native_CallMod("SaveGame", "MarkWayPoint", NULL, "vvv", level, spawn, act) &&
		Mode_CallKw("SaveGame", "StartGame", NULL, NULL, 0, Py_BuildValue("{s:i}", "CleanRevenge", 1)));
}

/* ElevatorReachUp(EntityName, EventName, Time): up: to the next level (way point and start), the rider passes to
 * the upper elevator, or teleports to the target; then the hook OnElevatorReachUp */
static byte Elv_ElevatorReachUp(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lel = Mode_V(&t), *el = Mode_V(&t), *linked = Mode_V(&t);
	int has = Elv_Linked(name, lel);
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	if (has) {
		Native_Free(lel);
		if (!Mode_EntitySeam(name, el) || !Elv_Act(el, "Down", "Closed") || !Dr_G(ELV, "LinkedElevator", linked) ||
			!Mode_Sub(linked, name, lel)) {
			return Mode_Done(&t, 0);
		}
		int next = Elv_OptIs(lel, "ELEVATOR_NEXTLEVEL");
		byte ok = next >= 0;
		if (next > 0) {
			ok = Elv_NextLevel(name, el, lel);
		} else if (next == 0) {
			int up = Elv_OptIs(lel, "ELEVATOR_UP");
			ok = up >= 0 && (!up || Elv_Pass(name, lel, "ELEVATOR_UP_REACH", "Open", "Opened"));
		}
		int tele = ok ? Elv_OptIs(lel, "TELEPORTER_UP") : -1;
		if (tele < 0 || (tele && !Elv_Pass(name, lel, "TELEPORTER_UP_REACH", "Down", "Closed"))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Elv_Hook("OnElevatorReachUp", name, 1));
}

/* ElevatorReachDown(EntityName, EventName, Time): down: a rider that arrives gets the doors opened, else the
 * elevator opens for the player again */
static byte Elv_ElevatorReachDown(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *el = Mode_V(&t), *lel = Mode_V(&t), *reach = Mode_V(&t), *o = Mode_V(&t);
	if (!Mode_EntitySeam(name, el)) {
		return Mode_Done(&t, 0);
	}
	int has = Elv_Linked(name, lel);
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	if (has) {
		Native_SetEmpty(reach, NV_TUPLE);
		int in = Dr_G(ELV, "ELEVATOR_DOWN_REACH", Native_Append(reach)) && Dr_G(ELV, "TELEPORTER_UP_REACH", Native_Append(reach)) &&
			Elv_Opt(lel, 0, o) ? Native_Contains(reach, o) : -1;
		if (in) {
			return Mode_Done(&t, in > 0 && Elv_Act(el, "Open", "Opened"));
		}
	}
	return Mode_Done(&t, LvN_SetS(el, "OnObjSlot", "!Usr0AutoDoorSlot"));
}

/* ElevatorHaveOpen(EntityName, EventName, Time): the doors are open, the rider is free; the hook
 * OnElevatorReachDown */
static byte Elv_ElevatorHaveOpen(const nativeVal_t *name, const nativeVal_t *event) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *el = Mode_V(&t), *lel = Mode_V(&t);
	if (!Mode_EntitySeam(name, el) || !Mode_VSet(el, "OnObjSlot", event)) {
		return Mode_Done(&t, 0);
	}
	int has = Elv_Linked(name, lel);
	return Mode_Done(&t, has >= 0 && (!has || Native_CallGlobal(ELV, "UnlinkFromElevator", NULL, "v", name)) &&
		Elv_Hook("OnElevatorReachDown", el, 0));
}

/* (0, v, 0) with v a value */
static void Elv_Mid(const nativeVal_t *v, nativeVal_t *out) {
	Mode_Ints3(out, 0, 0, 0);
	Native_Free(&out->items[1]);
	Native_Copy(&out->items[1], v);
}

/* LinkToElevator(UsrName, ElevatorName, optId, Instant=0): the character rides on the platform of the elevator
 * (LinkedElevator keeps the rider, the option and its UsrControl); the player gets the elevator camera, the
 * teleporter effect and no controls (OBJLIB 3: Chars and Teleport by name) */
static byte Elv_LinkToElevator(const nativeVal_t *usrName, const nativeVal_t *elvName, const nativeVal_t *opt,
	const nativeVal_t *instantArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *instant = Mode_V(&t), *el = Mode_V(&t), *act = Mode_V(&t), *values = Mode_V(&t), *elevs = Mode_V(&t);
	nativeVal_t *ch = Mode_V(&t), *ctrl = Mode_V(&t), *rec = Mode_V(&t), *c2 = Mode_V(&t), *act2 = Mode_V(&t);
	nativeVal_t *node = Mode_V(&t), *fd = Mode_V(&t), *off = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *off2 = Mode_V(&t), *ang2 = Mode_V(&t), *ename = Mode_V(&t), *lm = Mode_V(&t), *fd2 = Mode_V(&t);
	nativeVal_t *omp = Mode_V(&t), *c3 = Mode_V(&t), *chn = Mode_V(&t), *tr = Mode_V(&t);
	/* tr and the two values after it: the three items of the unpack */
	Mode_V(&t);
	Mode_V(&t);
	if (instantArg) {
		Native_Copy(instant, instantArg);
	} else {
		Native_SetInt(instant, 0);
	}
	if (!Mode_Import("Chars") || !Mode_EntitySeam(elvName, el) || !Mode_VObj(el, "ActCtrl", act)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(ELV, "ELEVATORS", 1, act);
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	if (has) {
		Native_Free(act);
		if (!Mode_VObj(el, "ActCtrl", act) || !Dr_G(ELV, "ELEVATORS", elevs) || !Mode_Sub(elevs, act, values)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetEmpty(values, NV_TUPLE);
		if (!Dr_G(ELV, "TranspHeight", Native_Append(values)) || !Dr_G(ELV, "TranspCamRadius", Native_Append(values)) ||
			!Dr_G(ELV, "TrapspCamAngle", Native_Append(values))) {
			return Mode_Done(&t, 0);
		}
	}
	/* tr[0], tr[1], tr[2] */
	if (!Mode_Unpack(values, 3, tr) || !Mode_EntitySeam(usrName, ch) || !LvN_SetI(el, "ArrowCanUse", 0) ||
		!Mode_VObj(ch, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(rec, NV_TUPLE);
	Native_Copy(Native_Append(rec), usrName);
	Native_Copy(Native_Append(rec), opt);
	Native_Copy(Native_Append(rec), ctrl);
	if (!Mode_DictPut(ELV, "LinkedElevator", NULL, elvName, rec) || !Mode_VObj(ch, "UsrControl", c2)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(c2, 0)) {
		Native_Free(act2);
		if (!Mode_VObj(el, "ActCtrl", act2)) {
			return Mode_Done(&t, 0);
		}
		if (LvN_EqS(act2, "DoorTeleporter") && (!Mode_Import("Teleport") ||
			!Native_CallMod("Teleport", "FXAction", NULL, "vvi", usrName, elvName, LvN_EqI(instant, 1) ? 1L : 2L))) {
			return Mode_Done(&t, 0);
		}
		if (!Native_CallGlobal(ELV, "ElevatorCamera", NULL, "v", elvName)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(instant);
		Native_SetInt(instant, 0);
	}
	byte ok;
	if (Native_True(instant)) {
		ok = LvN_SetF(ch, "DeltaOffMul", 2.0);
	} else {
		ok = Mode_VObj(el, "NT_NodePos_DC_Platform", node) && Mode_VObj(ch, "FloorDist", fd) && (Elv_Mid(fd, off), 1) &&
			Mode_VecAdd(node, off, pos) && Mode_VSet(ch, "Pos", pos) && Mode_VObj(el, "Ang", ang) &&
			(Elv_Mid(&tr[2], off2), 1) && Mode_VecAdd(ang, off2, ang2) && Mode_VSet(ch, "Ang", ang2) &&
			LvN_SetF(ch, "DeltaOffMul", 0.0) && LvN_SetF(ch, "OffMul", 1.0);
	}
	if (!ok || !Mode_VObj(el, "Name", ename) || !Mode_Cat(&t, lm, 3, ename, Mode_S(&t, ","), Mode_S(&t, "DC_Platform")) ||
		!Mode_VSet(ch, "LinkModel", lm) || !Mode_VObj(ch, "FloorDist", fd2) || (Elv_Mid(fd2, omp), 0) ||
		!Mode_VSet(ch, "OffModPos", omp) || !Mode_VObj(ch, "UsrControl", c3) ||
		(LvN_EqI(c3, 0) && !Mode_ActionSet("Inactive")) || !LvN_SetS(ch, "ForceAction", "Relax") ||
		!LvN_SetS(ch, "ForceDefLoop", "Relax") || !Mode_VObj(ch, "Name", chn)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, LvN_SetOnFloor(chn));
}

/* UnlinkFromElevator(EntityName): the rider is free again (the walk controls and the camera for the player) */
static byte Elv_UnlinkFromElevator(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *linked = Mode_V(&t), *rg = Mode_V(&t), *rider = Mode_V(&t), *ch = Mode_V(&t), *zero = Mode_V(&t);
	nativeVal_t *c2 = Mode_V(&t), *el = Mode_V(&t), *ctrl = Mode_V(&t);
	if (!Dr_G(ELV, "LinkedElevator", linked) || !Mode_Sub(linked, name, rg) || !LvN_Item(rg, 0, rider) ||
		!Mode_EntitySeam(rider, ch) || !LvN_SetS(ch, "LinkModel", ",") || !LvN_SetF(ch, "DeltaOffMul", 0.0) ||
		!LvN_SetF(ch, "OffMul", 0.0) || (Mode_Ints3(zero, 0, 0, 0), !Mode_VSet(ch, "OffModPos", zero)) ||
		!LvN_Item(rg, 2, c2) || (LvN_EqI(c2, 0) && !Mode_ActionSet("Walk")) || !Mode_EntitySeam(name, el) ||
		!LvN_SetI(el, "ArrowCanUse", 1) || !Mode_DictDel(ELV, "LinkedElevator", NULL, name) ||
		!Mode_VObj(ch, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !LvN_EqI(ctrl, 0) || Native_CallGlobal(ELV, "ElevatorBackCamera", NULL, ""));
}

/* Init(): the heights and cameras of the elevator classes */
static byte Elv_Init(void) {
	static const struct {
		const char *cls;
		long h, r, a;
	} elevs[] = {
		{ "DoorMetro", 500, 2000, 180 },
		{ "DoorMetroExt", 500, 2000, 180 },
		{ "DoorTeleporter", 250, 600, 0 },
		{ "DoorElevatorOrbit", 50, 500, 0 },
		{ "DoorEditShip", 500, 1000, 180 },
	};
	for (size_t i = 0; i < sizeof(elevs) / sizeof(elevs[0]); i++) {
		nativeVal_t v, k;
		Mode_Ints3(&v, elevs[i].h, elevs[i].r, elevs[i].a);
		LvN_Str(&k, elevs[i].cls);
		byte ok = Mode_DictPut(ELV, "ELEVATORS", NULL, &k, &v);
		Native_Free(&v);
		Native_Free(&k);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* SetDef(Type, Name): the elevator a player coming from a door of that type arrives in */
static byte Elv_SetDef(const nativeVal_t *type, const nativeVal_t *name) {
	return Mode_DictPut(ELV, "ElevatorStart", NULL, type, name);
}

/* from[lo:hi] == text: 1, 0, -1 */
static int Elv_Head(const nativeVal_t *from, long lo, long hi, const char *text) {
	nativeVal_t h = { 0 };
	int r = Native_Slice(from, lo, hi, &h) ? LvN_EqS(&h, text) : -1;
	Native_Free(&h);
	return r;
}

/* TryToElevatorLaunchFirst(): the hook OnElevatorReachDown with None */
static byte Elv_TryFirst(void) {
	nativeVal_t none;
	Native_SetNone(&none);
	return Elv_Hook("OnElevatorReachDown", &none, 0);
}

/* Scrap.AddScheduledFunc(0, func, args, name) with func an object (OBJLIB 4) */
static byte Elv_Later0(PyObject *func, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t zero, nm;
	Native_SetInt(&zero, 0);
	LvN_Str(&nm, name);
	byte ok = func && Mode_Schedule(&zero, func, args, n, &nm);
	Native_Free(&nm);
	return ok;
}

/* the player arrives down in the elevator of the door type of ComeFrom (with the metro parameters) */
static byte Elv_FromDoor(const nativeVal_t *from) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *start = Mode_V(&t), *key = Mode_V(&t), *tn = Mode_V(&t), *tgt = Mode_V(&t), *act = Mode_V(&t);
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *reach = Mode_V(&t), *opt = Mode_V(&t), *tname = Mode_V(&t);
	nativeVal_t *ac = Mode_V(&t), *ac2 = Mode_V(&t);
	if (!Mode_DelSchedS("TELF") || !Dr_G(ELV, "ElevatorStart", start) || !Native_Slice(from, 4, INT_MAX, key) ||
		!Mode_Sub(start, key, tn) || !Mode_EntitySeam(tn, tgt) || !Mode_VObj(tgt, "Action", act)) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_EqS(act, "Closed")) {
		return Mode_Done(&t, 1);
	}
	if (!LvN_SetS(tgt, "OnObjSlot", "") || !Elv_Act(tgt, "Down", "Closed") || !Mode_UsrHandle(usr) ||
		!Mode_VObj(usr, "Name", un) || !Dr_G(ELV, "ELEVATOR_DOWN_REACH", reach) || !Mode_VObj(tgt, "Name", tname)) {
		return Mode_Done(&t, 0);
	}
	PyObject *link = Mode_GlobalPy(ELV, "LinkToElevator");
	Native_SetEmpty(opt, NV_TUPLE);
	Native_Copy(Native_Append(opt), reach);
	Native_SetNone(Native_Append(opt));
	byte ok = link && Native_Call(link, NULL, "vvv", un, tname, opt);
	Py_XDECREF(link);
	if (!ok || !Mode_VObj(tgt, "ActCtrl", ac)) {
		return Mode_Done(&t, 0);
	}
	int metro = LvN_EqS(ac, "DoorMetro"), ext = 0;
	if (!metro) {
		ext = Mode_VObj(tgt, "ActCtrl", ac2) ? LvN_EqS(ac2, "DoorMetroExt") : -1;
	}
	if (ext < 0) {
		return Mode_Done(&t, 0);
	}
	if (!metro && !ext) {
		return Mode_Done(&t, 1);
	}
	/* OBJLIB 3, 4: Metro.SetUsrParams by name, scheduled */
	nativeVal_t args[2];
	args[0] = *un;
	Native_SetInt(&args[1], metro ? 0 : 180);
	PyObject *fn = Mode_Import("Metro") ? Script_Attr("Metro", "SetUsrParams") : NULL;
	ok = Elv_Later0(fn, args, 2, "MetroSetUsrParams");
	Py_XDECREF(fn);
	return Mode_Done(&t, ok);
}

/* port.SpawnChar(Scrap.UsrEntity(0)[, e]) of the port that places the player (OBJLIB 3) */
static byte Elv_Spawn(const char *port, const nativeVal_t *e) {
	nativeVal_t usr = { 0 };
	byte ok = Mode_Import(port) && Mode_UsrHandle(&usr) &&
		(e ? Native_CallMod(port, "SpawnChar", NULL, "vv", &usr, e) : Native_CallMod(port, "SpawnChar", NULL, "v", &usr));
	Native_Free(&usr);
	return ok;
}

/* PlayerCreatedElevator(): the player arrives where the save variable ComeFrom says: down in the elevator of that
 * door type (with the metro parameters), at a conversion point, a phone cabin or Dtritus' desktop */
static byte Elv_PlayerCreatedElevator(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *from = Mode_V(&t), *key = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("Init")) {
		return Mode_Done(&t, 0);
	}
	PyObject *fn = Mode_GlobalPy(ELV, "TryToElevatorLaunchFirst");
	byte ok = Elv_Later0(fn, NULL, 0, "TELF");
	Py_XDECREF(fn);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("ComeFrom", from);
	if (LvN_EqS(from, "")) {
		return Mode_Done(&t, 1);
	}
	const nativeVal_t *items[2] = { Mode_S(&t, "ExtraComeFrom = "), from };
	if (!Mode_Print(items, 2)) {
		return Mode_Done(&t, 0);
	}
	int door = Elv_Head(from, 0, 4, "Door");
	if (door > 0) {
		door = Native_Slice(from, 4, INT_MAX, key) ? Mode_HasKey(ELV, "ElevatorStart", 1, key) : -1;
	}
	if (door) {
		return Mode_Done(&t, door > 0 && Elv_FromDoor(from));
	}
	int conv = Elv_Head(from, 0, 19, "DM_Conversion_Point");
	if (conv) {
		return Mode_Done(&t, conv > 0 && Mode_EntitySeam(from, e) && (!Native_True(e) || Elv_Spawn("CharConversor", e)));
	}
	int phone = Elv_Head(from, 0, 9, "DM_Phone_");
	if (phone > 0) {
		int out = Dr_ModTrue("Init", "Outdoor");
		phone = out;
	}
	if (phone) {
		return Mode_Done(&t, phone > 0 && Mode_Import("PhoneCab") && Mode_EntitySeam(from, e) &&
			(!Native_True(e) || Elv_Spawn("PhoneCab", e)));
	}
	int desk = Elv_Head(from, 0, 17, "DM_DTritusDesktop");
	if (desk > 0) {
		int out = Dr_ModTrue("Init", "Outdoor");
		desk = out < 0 ? -1 : !out;
	}
	return Mode_Done(&t, desk >= 0 && (!desk || Elv_Spawn("DTritusDesktop", NULL)));
}

MODE_FN(Elevator, isOutMap, 1, Elv_isOutMap(&a[0], out))
MODE_FN(Elevator, ElevatorCamera, 1, Elv_ElevatorCamera(&a[0]))
DR_FN0(Elevator, ElevatorBackCamera, Elv_ElevatorBackCamera())
DR_FNOPT(Elevator, ConnectElevators, "O|OO", 3, Elv_ConnectElevators(&a[0], o[1] ? &a[1] : NULL, o[2] ? &a[2] : NULL))
DR_FNOPT(Elevator, DoorInitializator, "O|O", 2, Elv_DoorInitializator(&a[0], o[1] ? &a[1] : NULL))
MODE_FN(Elevator, ObjInitializator, 2, Elv_ObjInitializator(&a[0]))
MODE_FN(Elevator, ObjInitializatorNang, 2, Elv_ObjInitializatorNang(&a[0], &a[1]))
MODE_FN(Elevator, UseElevator, 4, Elv_UseElevator(&a[0], &a[1], &a[2], &a[3]))
MODE_FN(Elevator, ElevatorActiveToGet, 1, Elv_Move(&a[0], "Up", "Upstairs"))
MODE_FN(Elevator, ElevatorCloseToGo, 1, Elv_Move(&a[0], "Close", "Closed"))
MODE_FN(Elevator, ElevatorHaveClose, 3, Elv_ElevatorHaveClose(&a[0], &a[1]))
MODE_FN(Elevator, ElevatorReachUp, 3, Elv_ElevatorReachUp(&a[0]))
MODE_FN(Elevator, ElevatorReachDown, 3, Elv_ElevatorReachDown(&a[0]))
MODE_FN(Elevator, ElevatorHaveOpen, 3, Elv_ElevatorHaveOpen(&a[0], &a[1]))
DR_FNOPT(Elevator, LinkToElevator, "OOO|O", 4, Elv_LinkToElevator(&a[0], &a[1], &a[2], o[3] ? &a[3] : NULL))
MODE_FN(Elevator, UnlinkFromElevator, 1, Elv_UnlinkFromElevator(&a[0]))
DR_FN0(Elevator, Init, Elv_Init())
MODE_FN(Elevator, SetDef, 2, Elv_SetDef(&a[0], &a[1]))
DR_FN0(Elevator, TryToElevatorLaunchFirst, Elv_TryFirst())
DR_FN0(Elevator, PlayerCreatedElevator, Elv_PlayerCreatedElevator())

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

/* DoorInitializator(DoorClass): an elevator class whose closing opens for the player only (OBJLIB 3: Elevator by
 * name) */
static byte Tp_DoorInitializator(const nativeVal_t *cls) {
	return Mode_Import(ELV) && Native_CallMod(ELV, "DoorInitializator", NULL, "vi", cls, 0L);
}

/* ObjInitializator(elv, ExtraInfo): an elevator that opens for everyone, the camera above it */
static byte Tp_ObjInitializator(const nativeVal_t *elv, const nativeVal_t *extra) {
	nativeVal_t tgt;
	Mode_Ints3(&tgt, 0, 150, 0);
	byte ok = Mode_Import(ELV) && Native_CallMod(ELV, "ObjInitializator", NULL, "vv", elv, extra) &&
		LvN_SetI(elv, "UseAngSel", 0) && LvN_SetS(elv, "OnObjSlot", "!AutoDoorSlot") && Mode_VSet(elv, "CamTgt", &tgt);
	Native_Free(&tgt);
	return ok;
}

/* Link(Teleport1, Teleport2): two teleporters that lead to each other */
static byte Tp_Link(const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t pair, none;
	Native_SetEmpty(&pair, NV_TUPLE);
	Native_Copy(Native_Append(&pair), a);
	Native_Copy(Native_Append(&pair), b);
	Native_SetEmpty(&none, NV_LIST);
	byte ok = Mode_Import(ELV) && Native_CallMod(ELV, "ConnectElevators", NULL, "vvi", &pair, &none, 0L);
	Native_Free(&pair);
	Native_Free(&none);
	return ok;
}

/* FXAction(CharName, ElevatorName, Phase): the teleport effect of the engine (phase 1 in, 2 out); OBJLIB 7: the call
 * function of Scrap by name (the cases record SetCallFunc, AddParams and Execute) */
static byte Tp_FXAction(const nativeVal_t *ch, const nativeVal_t *elv, const nativeVal_t *phase) {
	return Native_CallMod("Scrap", "SetCallFunc", NULL, "s", "!FXCharacterTeleport") &&
		Native_CallMod("Scrap", "AddParams", NULL, "v", ch) && Native_CallMod("Scrap", "AddParams", NULL, "v", elv) &&
		Native_CallMod("Scrap", "AddParami", NULL, "v", phase) && Native_CallMod("Scrap", "Execute", NULL, "");
}

MODE_FN(Teleport, DoorInitializator, 1, Tp_DoorInitializator(&a[0]))
MODE_FN(Teleport, ObjInitializator, 2, Tp_ObjInitializator(&a[0], &a[1]))
MODE_FN(Teleport, Link, 2, Tp_Link(&a[0], &a[1]))
MODE_FN(Teleport, FXAction, 3, Tp_FXAction(&a[0], &a[1], &a[2]))

PyMethodDef g_TeleportMethods[] = {
	{ "DoorInitializator", Teleport_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Teleport_ObjInitializator, 1, NULL },
	{ "Link", Teleport_Link, 1, NULL },
	{ "FXAction", Teleport_FXAction, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
