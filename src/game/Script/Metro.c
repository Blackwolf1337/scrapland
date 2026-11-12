/*
 * The metro in C (issue #2, the mission script missions/metro.py): the station doors of the levels (SetAsMetro,
 * DoorInitializator, ObjInitializator) and the map the player picks the next station on (OnUse); the trip itself is
 * an elevator ride (Elevator.UseElevator with "MoveLevel <station>") while the player walks into the train. The
 * layout of the map is data/menus/metro.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. The module variables (MissionMap, OutOfMetro, WalkCharSpeed; PlayerName and CallParams, which
 * the script creates on the first use) are mirrored from the module dict where the script reads them and written
 * through where it writes them (OBJLIB 5). The calls of Elevator, Menu and Chars go through their refs (stage 3);
 * Init.Path and Menu.BackButtonYStart stay by name (OBJLIB 5).
 */
#include <limits.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/pyentity.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static const char metro_def[] = {
#embed "../../../data/menus/metro.def"
	, 0 };

#define METRO_FILE "menus/metro.def"

static struct {
	nativeVal_t missionMap;		/* MissionMap: the station of the main mission */
	nativeVal_t outOfMetro;		/* OutOfMetro: maps without a station and the station outside */
	nativeVal_t walkCharSpeed;	/* WalkCharSpeed: the walk of each character class into the train */
	nativeVal_t playerName;		/* PlayerName: the character that walks in */
	nativeVal_t callParams;		/* CallParams: the arguments of OnUse, for the ride */
} metro;

/* OBJLIB 5: the mirror in the module dict, read where the script reads each variable as a global */
static const nativeVar_t metro_vars[] = {
	{ "MissionMap", &metro.missionMap, 0, NV_ANY },
	{ "OutOfMetro", &metro.outOfMetro, 0, NV_ANY },
	{ "WalkCharSpeed", &metro.walkCharSpeed, 0, NV_ANY },
	{ "PlayerName", &metro.playerName, 0, NV_ANY },
	{ "CallParams", &metro.callParams, 0, NV_ANY },
	{ NULL, NULL, 0, 0 },
};
enum { MV_MAP, MV_OUT, MV_SPEED, MV_PLAYER, MV_PARAMS };

/* the variable k from the module: 0 with the NameError of the script's global */
static byte Metro_Read(int k) {
	const nativeVar_t one[] = { metro_vars[k], { NULL, NULL, 0, 0 } };
	return Native_ImportGlobals("Metro", one);
}

/* `global name; name = v` of the script for the variable k (v taken over): 0 with the error set */
static byte Metro_Write(int k, nativeVal_t *v) {
	Native_Free(metro_vars[k].values);
	*metro_vars[k].values = *v;
	Native_SetNone(v);
	return Native_Put("Metro", &metro_vars[k], 0);
}

/* d.has_key(key) of the script: 1 with *has, else 0 with the AttributeError of a value that is no dict */
static byte Metro_HasKey(const nativeVal_t *d, const nativeVal_t *key, int *has) {
	if (d->type == NV_DICT) {
		*has = Native_DictGet(d, key) != NULL;
		return 1;
	}
	/* a list finds no method of that name, the other types have no attributes */
	return d->type == NV_LIST ? Native_Raise(NE_ATTRIBUTE, "has_key") :
		Native_Raise(NE_ATTRIBUTE, "'%s' object has no attribute 'has_key'", Native_TypeName(d));
}

/* CalculateMissionMap(EntityName): the station of the main mission (its map in capitals, or the one outside the
 * metro of OutOfMetro) */
static byte Metro_MissionMap(void) {
	static const char *const mods[] = { "string", NULL };
	if (!Script_Import(mods)) {
		return 0;
	}
	/* string.upper(Scrap.GetSaveVar("Mission.Map")) */
	const char *sv = SaveVar_Get("Mission.Map");
	nativeVal_t v, up;
	if (sv) {
		Native_SetText(&v, sv, (int)strlen(sv));
	} else {
		Native_SetNone(&v);
	}
	byte ok = Native_Upper(&v, &up);
	Native_Free(&v);
	if (!ok || !Metro_Write(MV_MAP, &up) || !Metro_Read(MV_OUT) || !Metro_Read(MV_MAP)) {
		return 0;
	}
	int has;
	if (!Metro_HasKey(&metro.outOfMetro, &metro.missionMap, &has)) {
		return 0;
	}
	if (has) {
		if (!Metro_Read(MV_MAP)) {
			return 0;
		}
		nativeVal_t out;
		Native_Copy(&out, Native_DictGet(&metro.outOfMetro, &metro.missionMap));
		return Metro_Write(MV_MAP, &out);
	}
	return 1;
}

/* Scrap.GetEntity(name).var = v of the script: the TypeError of GetEntity for a name that is no text, the error of
 * the attribute set on the None it gives for a missing entity */
static byte Metro_EntSet(const nativeVal_t *name, const char *var, const nativeVal_t *v) {
	cEntity *e;
	if (!Native_GetEntity(name, &e)) {
		return 0;
	}
	if (!e) {
		return Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	return Native_EntSet(e, Native_Text(name), var, v);
}

/* RecoverUsr(EntityName, EventName, Time): the event near the end of the door going down gives the player back */
static byte Metro_RecoverUsr(void) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Metro_Read(MV_PLAYER) && Metro_EntSet(&metro.playerName, "UsrControl", &zero);
}

/* OutDoorInitializator(DoorClass, val=0): the door of Elevator with the event Recover 0.1 s before the end of Down;
 * door and val as the caller gave them (they go to Elevator as they are) */
static byte Metro_OutDoor(const nativeVal_t *door, const nativeVal_t *val) {
	if (!Mode_Import("Elevator") || !PORT_CALL(Elevator, DoorInitializator, NULL, "vv", door, val)) {
		return 0;
	}
	Native_ActAction("Down");
	/* SAct.GetAct("AnmTotalTime") - 0.1 */
	nativeVal_t total, tenth, v;
	Native_ActVar("AnmTotalTime", &total);
	Native_SetFloat(&tenth, 0.1);
	byte ok = Native_Arith('-', &total, &tenth, &v);
	Native_Free(&total);
	if (!ok) {
		return 0;
	}
	Native_ActSet("AddEventAtTime", &v);
	Native_Free(&v);
	Native_SetStr(&v, "Recover");
	Native_ActSet("EventName", &v);
	Native_SetStr(&v, "Metro.RecoverUsr");
	Native_ActSet("EventFunc", &v);
	return 1;
}

/* SetUsrParams(name, Ang=0): the player walks into the train by itself at the walking speed of its character */
static byte Metro_SetUsrParams(const nativeVal_t *name, const nativeVal_t *ang) {
	nativeVal_t v;
	Native_Copy(&v, name);
	cEntity *ply;
	if (!Metro_Write(MV_PLAYER, &v) || !Native_GetEntity(name, &ply)) {
		return 0;
	}
	if (!ply) {
		return Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	const char *ref = Native_Text(name);
	Native_SetInt(&v, -1);
	nativeVal_t act;
	int has;
	if (!Native_EntSet(ply, ref, "UsrControl", &v) || !Metro_Read(MV_SPEED) || !Native_EntVar(ply, ref, "ActCtrl", &act)) {
		return 0;
	}
	byte ok = Metro_HasKey(&metro.walkCharSpeed, &act, &has);
	Native_Free(&act);
	if (!ok) {
		return 0;
	}
	nativeVal_t walk;
	if (has) {
		if (!Metro_Read(MV_SPEED) || !Native_EntVar(ply, ref, "ActCtrl", &act)) {
			return 0;
		}
		const nativeVal_t *w = Native_DictGet(&metro.walkCharSpeed, &act);
		ok = w || Native_RaiseKey(&act);
		Native_Free(&act);
		if (!ok) {
			return 0;
		}
		Native_Copy(&walk, w);
	} else {
		Native_SetFloat(&walk, 0.0);
	}
	ok = Native_EntSet(ply, ref, "InCtrl_Walk", &walk);
	Native_Free(&walk);
	if (!ok || !Native_True(ang)) {
		return ok;
	}
	/* ply.Ang = (0, ply.Ang[1] + Ang, 0) */
	nativeVal_t cur, tmp, ny = { 0 };
	if (!Native_EntVar(ply, ref, "Ang", &cur)) {
		return 0;
	}
	const nativeVal_t *cy = Native_GetItem(&cur, 1, 1, &tmp);
	ok = cy && Native_Arith('+', cy, ang, &ny);
	if (cy == &tmp) {
		Native_Free(&tmp);
	}
	Native_Free(&cur);
	if (ok) {
		/* (0, ny, 0) */
		Native_SetEmpty(&v, NV_TUPLE);
		Native_SetInt(Native_Append(&v), 0);
		*Native_Append(&v) = ny;
		Native_SetInt(Native_Append(&v), 0);
		ok = Native_EntSet(ply, ref, "Ang", &v);
		Native_Free(&v);
	}
	return ok;
}

/* UseMetro(id, control): the station picked (control is Station_<name>): back to the game, the elevator ride, the
 * player walks in, the other characters hide */
static byte Metro_UseMetro(const nativeVal_t *id, const nativeVal_t *control) {
	static const char *const mods[] = { "Elevator", "Menu", NULL };
	if (!Script_Import(mods) || !PORT_CALL(Menu, BackToGame, NULL, "vv", id, control)) {
		return 0;
	}
	nativeVal_t cp[4] = { 0 };
	byte ok = 1;
	for (int i = 0; ok && i < 4; i++) {
		nativeVal_t tmp;
		if (i == 2) {
			/* "MoveLevel " + control[8:] */
			nativeVal_t head, tail;
			Native_SetStr(&head, "MoveLevel ");
			ok = Native_Slice(control, 8, INT_MAX, &tail);
			if (ok) {
				ok = Native_Arith('+', &head, &tail, &cp[i]);
				Native_Free(&tail);
			}
		} else {
			const nativeVal_t *p = Metro_Read(MV_PARAMS) ? Native_GetItem(&metro.callParams, i, 1, &tmp) : NULL;
			ok = p != NULL;
			if (p) {
				Native_Copy(&cp[i], p);
			}
			if (p == &tmp) {
				Native_Free(&tmp);
			}
		}
	}
	ok = ok && PORT_CALL(Elevator, UseElevator, NULL, "vvvv", &cp[0], &cp[1], &cp[2], &cp[3]);
	for (int i = 0; i < 4; i++) {
		Native_Free(&cp[i]);
	}
	if (!ok || !Metro_Read(MV_PARAMS)) {
		return 0;
	}
	nativeVal_t tmp, zero, first;
	const nativeVal_t *p = Native_GetItem(&metro.callParams, 0, 1, &tmp);
	if (!p) {
		return 0;
	}
	Native_Copy(&first, p);
	if (p == &tmp) {
		Native_Free(&tmp);
	}
	Native_SetInt(&zero, 0);
	ok = Metro_SetUsrParams(&first, &zero);
	Native_Free(&first);
	if (!ok) {
		return 0;
	}
	if (Native_ActClass("DoorMetro")) {
		nativeVal_t v;
		Native_SetInt(&v, 2800);
		if (!Native_ClsSet("SoundDispZ", &v)) {
			return 0;
		}
	}
	/* the walking characters but the player hide: Scrap.GetFirst() and each NextInSlot (the handle of the script
	 * finds an entity by its name) */
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	e = e ? World_FindEntity(e->name) : NULL;
	while (e) {
		char ref[0x80];
		nativeVal_t next;
		Str_Copy(ref, e->name, sizeof(ref));
		if (Entity_IsType(e, "WalkChar")) {
			nativeVal_t name;
			if (!Native_EntVar(e, ref, "Name", &name)) {
				return 0;
			}
			ok = Metro_Read(MV_PLAYER);
			byte hide = ok && !Native_Equal(&name, &metro.playerName);
			Native_Free(&name);
			if (!ok) {
				return 0;
			}
			Native_SetInt(&zero, 0);
			if (hide && !Native_EntSet(e, ref, "Active", &zero)) {
				return 0;
			}
		}
		if (!Native_EntVar(e, ref, "NextInSlot", &next)) {
			return 0;
		}
		ok = Native_GetEntity(&next, &e);
		Native_Free(&next);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* string.upper(v[from:]) of the script */
static byte Metro_UpperTail(const nativeVal_t *v, int from, nativeVal_t *out) {
	nativeVal_t tail;
	if (!Native_Slice(v, from, INT_MAX, &tail)) {
		return 0;
	}
	byte ok = Native_Upper(&tail, out);
	Native_Free(&tail);
	return ok;
}

/* CreateStation(X, Y, Name, SpriteIndex=6): a station button of the map (metro.def sections): 0 with the error set */
static byte Metro_Station(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *name, const nativeVal_t *sprite) {
	static const char *const mods[] = { "string", "Chars", "Elevator", "Init", NULL };
	nativeVal_t up, here = { 0 };
	if (!Script_Import(mods) || !Native_Upper(name, &up)) {
		return 0;
	}
	byte ok = Metro_Read(MV_MAP);
	int mission = ok && Native_Equal(&up, &metro.missionMap);
	/* one dict of $names for the sections of a station */
	nativeVal_t mv, vars;
	if (!ok) {
		Native_Free(&up);
		return 0;
	}
	Mode_DefVars(&vars, 5, "x", x, "y", y, "name", name, "sprite", sprite, "mission", Mode_IntVal(&mv, mission));
	if (mission) {
		Script_RunFile(0, METRO_FILE, "mission", metro_def, NULL, &vars);
	}
	nativeVal_t p;
	ok = Mode_ModGet("Init", "Path", &p);
	if (ok) {
		ok = Metro_UpperTail(&p, 7, &here);
		Native_Free(&p);
	}
	int usrhere = ok && Native_Equal(&up, &here);
	Native_Free(&up);
	Native_Free(&here);
	if (ok) {
		Script_RunFile(0, METRO_FILE, "station", metro_def, NULL, &vars);
	}
	const char *section = "here";
	if (ok && !usrhere) {
		/* Elevator.isOutMap(Name) and Chars.CanDrive(Scrap.UsrEntity(0)) (the handle goes to Chars) */
		nativeVal_t r = { 0 }, usr = { 0 };
		int cantgo = PORT_CALL(Elevator, isOutMap, &r, "v", name) ? Mode_Truth(&r) : -1;
		Native_Free(&r);
		if (cantgo > 0) {
			int d = PORT_CALL(Scrap, UsrEntity, &usr, "i", 0L) && PORT_CALL(Chars, CanDrive, &r, "v", &usr) ? Mode_Truth(&r) : -1;
			cantgo = d < 0 ? -1 : !d;
			Native_Free(&r);
			Native_Free(&usr);
		}
		ok = cantgo >= 0;
		section = cantgo ? "cantgo" : "go";
	}
	if (ok) {
		Script_RunFile(0, METRO_FILE, section, metro_def, NULL, &vars);
		if (usrhere) {
			Script_RunFile(0, METRO_FILE, "usrhere", metro_def, NULL, &vars);
		}
	}
	Native_Free(&vars);
	return ok;
}

/* ShowHint(id, control, lostfocus): the name of the station under the cursor */
static byte Metro_ShowHint(const nativeVal_t *id, const nativeVal_t *control) {
	const char *key;
	int player;
	nativeVal_t v;
	/* SScorer.Set(id, "StationName", "Text", Scrap.GetLangStr(control)), then Visible 1 */
	if (!Native_ArgStr(control, 1, &key)) {
		return 0;
	}
	Native_SetStr(&v, Script_Lang(key));
	if (!Native_ArgInt(id, &player) || !Native_ScorerSet(player, "StationName", "Text", &v)) {
		return 0;
	}
	Native_SetInt(&v, 1);
	return Native_ScorerSet(player, "StationName", "Visible", &v);
}

/* HideHint(id, control, lostfocus) */
static byte Metro_HideHint(const nativeVal_t *id) {
	int player;
	nativeVal_t v;
	Native_SetInt(&v, 0);
	return Native_ArgInt(id, &player) && Native_ScorerSet(player, "StationName", "Visible", &v);
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

/* OnUse(UsrName, ElevatorName, optId, FullOption): the use of a station door opens the map, the game time stops;
 * params the tuple of the four arguments (taken over) */
static byte Metro_OnUse(nativeVal_t *params) {
	static const char *const mods[] = { "Menu", "Init", NULL };
	if (!Script_Import(mods) || !Metro_Write(MV_PARAMS, params)) {
		return 0;
	}
	/* OBJLIB 5: from Menu import BackButtonYStart */
	nativeVal_t back;
	if (!Mode_FromV("Menu", "BackButtonYStart", &back)) {
		return 0;
	}
	/* SScorer.SetOnCancel(0, "Menu.BackToGame") */
	byte ok = Metro_MissionMap() && PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) &&
		PORT_CALL(Menu, StartNewMenu, NULL, "i", 0L);
	if (ok) {
		cScorer *sc = Scorer_Get(0);
		if (sc) {
			cPy_Function_SetFunc(&sc->onCancel, "Menu.BackToGame");
		}
	}
	for (size_t i = 0; ok && i < sizeof(metro_stations) / sizeof(metro_stations[0]); i++) {
		nativeVal_t x, y, name, sprite;
		Native_SetInt(&x, metro_stations[i].x + METRO_MAPX);
		Native_SetInt(&y, metro_stations[i].y + METRO_MAPY);
		Native_SetStr(&name, metro_stations[i].name);
		Native_SetInt(&sprite, metro_stations[i].sprite);
		ok = Metro_Station(&x, &y, &name, &sprite);
	}
	if (ok) {
		Mode_DefRun(0, METRO_FILE, "menu", metro_def, NULL, 0, 1, "back", &back);
	}
	Native_Free(&back);
	return ok;
}

/* SetAsMetro(EntityName): the door of the level is a metro station; name as the caller gave it (it goes to
 * Elevator as it is) */
static byte Metro_SetAsMetro(const nativeVal_t *name) {
	static const char *const mods[] = { "Elevator", "Init", NULL };
	if (!Script_Import(mods)) {
		return 0;
	}
	/* Elevator.ConnectElevators([EntityName], [Init.Path[7:]]) (OBJLIB 5: Init.Path is a variable of Init) */
	nativeVal_t path, names, maps;
	Native_SetEmpty(&names, NV_LIST);
	Native_SetEmpty(&maps, NV_LIST);
	byte ok = Mode_ModGet("Init", "Path", &path) && Mode_Slice(&path, 7, INT_MAX, Native_Append(&maps));
	Native_Copy(Native_Append(&names), name);
	ok = ok && PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &names, &maps);
	Native_Free(&maps);
	Native_Free(&names);
	Native_Free(&path);
	nativeVal_t v;
	const nativeVal_t *n = name;
	cEntity *elv = NULL;
	ok = ok && Native_GetEntity(n, &elv);
	if (ok && !elv) {
		ok = Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	const char *ref = Native_Text(n);
	Native_SetInt(&v, 0);
	ok = ok && Native_EntSet(elv, ref, "ArrnOpt", &v);
	if (ok) {
		/* "Travel:" + Scrap.GetLangStr("Station_Hint0") */
		nativeBuf_t b;
		Native_BufInit(&b);
		Native_BufAdd(&b, "Travel:", -1);
		Native_BufAdd(&b, Script_Lang("Station_Hint0"), -1);
		Native_SetText(&v, b.p, b.len);
		Native_BufFree(&b);
		ok = Native_EntSet(elv, ref, "ArrOpt0", &v);
		Native_Free(&v);
	}
	Native_SetStr(&v, "Metro.OnUse");
	return ok && Native_EntSet(elv, ref, "UseFunc", &v);
}

/* ---- the functions of the module table (MODE_FN: the arguments as native values, the arity errors of
 * PyArg_ParseTuple) ---- */

MODE_FN(Metro, CalculateMissionMap, 1, Metro_MissionMap())
MODE_FN(Metro, RecoverUsrPy, 3, Metro_RecoverUsr())

/* OutDoorInitializator(DoorClass, val=0) */
static byte Metro_OutDoorArgs(const nativeVal_t *a, int n) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Metro_OutDoor(&a[0], n > 1 ? &a[1] : &zero);
}

MODE_FNOPT(Metro, OutDoorInitializator, 1, 2, Metro_OutDoorArgs(a, n))

/* SetUsrParams(name, ang=0) */
static byte Metro_SetUsrParamsArgs(const nativeVal_t *a, int n) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Metro_SetUsrParams(&a[0], n > 1 ? &a[1] : &zero);
}

MODE_FNOPT(Metro, SetUsrParamsPy, 1, 2, Metro_SetUsrParamsArgs(a, n))
MODE_FN(Metro, UseMetroPy, 2, Metro_UseMetro(&a[0], &a[1]))

/* CreateStation(X, Y, Name, SpriteIndex=6) */
static byte Metro_StationArgs(const nativeVal_t *a, int n) {
	nativeVal_t six;
	Native_SetInt(&six, 6);
	return Metro_Station(&a[0], &a[1], &a[2], n > 3 ? &a[3] : &six);
}

MODE_FNOPT(Metro, CreateStation, 3, 4, Metro_StationArgs(a, n))
MODE_FN(Metro, ShowHintPy, 3, Metro_ShowHint(&a[0], &a[1]))
MODE_FN(Metro, HideHintPy, 3, Metro_HideHint(&a[0]))

/* OnUse(a, b, c, d): the four arguments as the tuple CallParams */
static byte Metro_OnUseArgs(const nativeVal_t *a) {
	nativeVal_t params;
	Native_SetEmpty(&params, NV_TUPLE);
	for (int i = 0; i < 4; i++) {
		Native_Copy(Native_Append(&params), &a[i]);
	}
	byte ok = Metro_OnUse(&params);
	Native_Free(&params);
	return ok;
}

MODE_FN(Metro, OnUsePy, 4, Metro_OnUseArgs(a))
MODE_FN(Metro, SetAsMetroPy, 1, Metro_SetAsMetro(&a[0]))

/* DoorInitializator(DoorClass): the station door, its sound heard farther */
static byte Metro_Door(const nativeVal_t *door) {
	nativeVal_t v;
	Native_SetInt(&v, 1);
	if (!Metro_OutDoor(door, &v)) {
		return 0;
	}
	Native_SetInt(&v, 1800);
	return Native_ClsSet("SoundDispZ", &v);
}

MODE_FN(Metro, DoorInitializator, 1, Metro_Door(&a[0]))

/* ObjInitializator(elv, ExtraInfo): the door of Elevator that opens by itself */
static PyObject *Metro_ObjInitializator(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Elevator", NULL };
	PyObject *elv, *extra;
	nativeVal_t v;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra) || !Script_Import(mods) ||
		!PORT_CALL(Elevator, ObjInitializator, NULL, "OO", elv, extra)) {
		return NULL;
	}
	if (!PyEntity_Check(elv)) {
		/* OBJLIB 7: the attribute set of another value, with its error */
		return Mode_Result(Script_HSet(elv, "OnObjSlot", PyString_FromString("!AutoDoorSlot")));
	}
	const cEntityRef *ref = ((cPyEntity *)elv)->ref;
	Native_SetStr(&v, "!AutoDoorSlot");
	return Mode_Result(Native_EntSet(ref->link.target, ref->name, "OnObjSlot", &v));
}

PyMethodDef g_MetroMethods[] = {
	{ "CalculateMissionMap", Metro_CalculateMissionMap, 1, NULL },
	{ "WrongOpt", Mode_PyNone2, 1, NULL },
	{ "RecoverUsr", Metro_RecoverUsrPy, 1, NULL },
	{ "OutDoorInitializator", Metro_OutDoorInitializator, 1, NULL },
	{ "SetUsrParams", Metro_SetUsrParamsPy, 1, NULL },
	{ "UseMetro", Metro_UseMetroPy, 1, NULL },
	{ "CreateStation", Metro_CreateStation, 1, NULL },
	{ "DoNothing", Mode_PyNone2, 1, NULL },
	{ "ShowHint", Metro_ShowHintPy, 1, NULL },
	{ "HideHint", Metro_HideHintPy, 1, NULL },
	{ "OnUse", Metro_OnUsePy, 1, NULL },
	{ "SetAsMetro", Metro_SetAsMetroPy, 1, NULL },
	{ "DoorInitializator", Metro_DoorInitializator, 1, NULL },
	{ "ObjInitializator", Metro_ObjInitializator, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Metro)
