/*
 * The six game modes of the single player super deals in C (scripts/superdeals/<mode>.py, issue #2 round 4):
 * DeathMatch, TeamDeathMatch and Versus (combats in the CA and the QF), CTF (the flags in the FZ), Domination (the
 * zones and the baton in the OM) and MortalRace (the race with the baton in the FZ). The module code of init.py
 * preloads the module named by the save variable SuperDealType in a super deal map, Init.Init calls its InitStatus and
 * Init, and the crazy gambler gives the start when the player boards his ship (Parking.SwitchVehicleMissionCallback).
 * The missions of the crazy gambler (Mission_BankersCombat, Mission_IgorCombat, Mission_BankersCTF,
 * Mission_MortalRace) use the same modules. Functions the scripts copied from each other line by line are one C
 * function here, registered in each module; where only names differ, one helper takes them.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values and call the engine in C. The state
 * stays in the module dicts, because the missions, MissionsFuncs, the level files and the score tables read and set
 * it (DeathMatch.enemiesFrags, MortalRace.BayScorer, CTF.flags, Domination.zones, TeamDeathMatch.KeepPlaying, ...):
 * the C keeps the values in the struct of each mode and reads each where the script reads the global, a write goes
 * through at once, lists and dicts are changed in place on the object of the dict (OBJLIB 5). The functions call the
 * other functions of their module through its globals (a mission may replace one) and the other ports by name
 * (OBJLIB 3). Entities the script holds are handle values (NV_OBJ, OBJLIB 7): a handle whose entity is gone gives the
 * errors of the script. The engine functions the cases record or swap go through their seams (OBJLIB 7,
 * scrap/scriptmodes.h: Scrap.AddScheduledFunc, Scrap.UsrEntity, Scrap.Set, SSound.Play, SScorer.SetMsgText,
 * SScorer.Set, Scrap.Preload3DObject, SAI.GetStateVehicle); SAI.SetStateVehicle takes Python arguments in its state
 * handlers and is called by name (OBJLIB 7).
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/nodesworld.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define DM "DeathMatch"
#define TDM "TeamDeathMatch"
#define VS "Versus"
#define CTF "CTF"
#define DOM "Domination"
#define MR "MortalRace"
#define CG "CrazyGambler"

/* a function of the module through its global (a mission may replace it), another port by name (OBJLIB 3) */
#define OWN(mod, name, out, ...) Native_CallGlobal(mod, name, out, __VA_ARGS__)
#define CALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* ---- shared lines ---- */

/* Scrap.SetSaveVar(name, text) */
static void Sd_SetSave(const char *name, const char *text) {
	SaveVar_Set(name, text);
}

/* int(Scrap.GetSaveVar(name)) */
static byte Sd_SaveInt(const char *name, nativeVal_t *out) {
	nativeVal_t s;
	Mode_SaveVar(name, &s);
	byte ok = Native_ToInt(&s, out);
	Native_Free(&s);
	return ok;
}

/* the count of range(n) as its argument parser takes it ("l": an int, a float cut) */
static byte Sd_Range(const nativeVal_t *n, long *count) {
	int i;
	if (n->type == NV_LONG) {
		return Native_Raise(NE_OVERFLOW, "long int too long to convert");
	}
	if (!Native_ArgInt(n, &i)) {
		return 0;
	}
	*count = i;
	return 1;
}

/* h.var = i, h.var = "text", h.var = d of a handle value */
static byte Sd_SetI(const nativeVal_t *h, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_VSet(h, var, &v);
}

static byte Sd_SetS(const nativeVal_t *h, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Mode_VSet(h, var, &v);
	Native_Free(&v);
	return ok;
}


/* module.var of another module as a value (any object, a handle as NV_OBJ) */
static byte Sd_ModGet(const char *module, const char *var, nativeVal_t *out) {
	PyObject *o = Script_Attr(module, var);
	if (!o) {
		return 0;
	}
	Native_Arg(o, out);
	Py_DECREF(o);
	return 1;
}

/* module.var.attr = i (Parking.PlayerShip.Invulnerable = 0) */
static byte Sd_ModSetI(const char *module, const char *var, const char *attr, long i) {
	nativeVal_t h;
	if (!Sd_ModGet(module, var, &h)) {
		return 0;
	}
	byte ok = Sd_SetI(&h, attr, i);
	Native_Free(&h);
	return ok;
}

/* module.var = the global name of mod as the object it is (a function of the mode: the one a mission put there) */
static byte Sd_GlobalTo(const char *module, const char *var, const char *mod, const char *name) {
	PyObject *o = Mode_GlobalPy(mod, name);
	byte ok = o && Mode_SetModPy(module, var, o);
	Py_XDECREF(o);
	return ok;
}

/* module.var = src.name of another module as the object it is (Menu.MissionScorerCall = RacerScorer.MissionScorer) */
static byte Sd_ModTo(const char *module, const char *var, const char *src, const char *name) {
	PyObject *o = Script_Attr(src, name);
	byte ok = o && Mode_SetModPy(module, var, o);
	Py_XDECREF(o);
	return ok;
}

/* module.var = None */
static byte Sd_ModNone(const char *module, const char *var) {
	return Mode_SetModPy(module, var, Py_None);
}

/* `usr and usr.Name == name`, `usr and usr.IsType("Car")`, `cad and cad != ""` as conditions: 1, 0, -1 with the
 * error */
static int Sd_UsrIs(const nativeVal_t *usr, const nativeVal_t *name) {
	nativeVal_t n;
	if (!Native_True(usr)) {
		return 0;
	}
	Native_SetNone(&n);
	int r = Mode_VGet(usr, "Name", &n) ? !Native_Compare(&n, name) : -1;
	Native_Free(&n);
	return r;
}

static int Sd_UsrCar(const nativeVal_t *usr) {
	return Native_True(usr) ? Mode_VIsType(usr, "Car") : 0;
}

static int Sd_Text(const nativeVal_t *cad) {
	return Native_True(cad) && !Native_IsStr(cad, "");
}

/* a == b, a < b, a >= b of the script (Native_Compare can not fail on native values) */
static byte Sd_Eq(const nativeVal_t *a, const nativeVal_t *b) {
	return !Native_Compare(a, b);
}


/* name = name + add of a global of the module (slot its copy in C), the new value in the slot */
static byte Sd_Add(const char *mod, const char *name, nativeVal_t *slot, long add) {
	nativeVal_t n, r;
	Native_SetInt(&n, add);
	Native_SetNone(&r);
	byte ok = Mode_Read(mod, name, slot) && Native_Arith('+', slot, &n, &r) && Mode_Put(mod, name, slot, &r);
	Native_Free(&r);
	return ok;
}

/* a global of the module = an int (slot its copy in C) */
static byte Sd_PutI(const char *mod, const char *name, nativeVal_t *slot, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_Put(mod, name, slot, &v);
}



/* e.Ang = (0, ang, 0) */
static byte Sd_Ang(const nativeVal_t *e, const nativeVal_t *ang) {
	nativeVal_t a;
	Native_SetEmpty(&a, NV_TUPLE);
	Native_SetInt(Native_Append(&a), 0);
	Native_Copy(Native_Append(&a), ang);
	Native_SetInt(Native_Append(&a), 0);
	byte ok = Mode_VSet(e, "Ang", &a);
	Native_Free(&a);
	return ok;
}

/* x[i] of the script */
static byte Sd_Item(const nativeVal_t *x, long i, nativeVal_t *out) {
	nativeVal_t k;
	Native_SetInt(&k, i);
	return Native_Subscript(x, &k, out);
}

/* e = Scrap.CreateEntity(name, pos[0], pos[1], pos[2], cls) as the handle value */
static byte Sd_Create(const nativeVal_t *name, const nativeVal_t *pos, const char *cls, nativeVal_t *out) {
	cEntity *e;
	return Mode_CreateAt(name, pos, cls, &e) && Mode_EntHandle(e, out);
}

/* spawnParams = OutMap.GetSpawnPos(); e = Scrap.CreateEntity(name, spawnParams[0][0], ..., "Car") (spawnParams in
 * *sp for the angle) */
static byte Sd_SpawnCar(const nativeVal_t *name, nativeVal_t *sp, nativeVal_t *out) {
	nativeVal_t pos;
	Native_SetNone(&pos);
	byte ok = CALL("OutMap", "GetSpawnPos", sp, "") && Sd_Item(sp, 0, &pos) && Sd_Create(name, &pos, "Car", out);
	Native_Free(&pos);
	return ok;
}

/* the same with e.Ang = (0, spawnParams[1], 0) right after */
static byte Sd_SpawnCarAng(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t sp, ang;
	Native_SetNone(&sp);
	Native_SetNone(&ang);
	byte ok = Sd_SpawnCar(name, &sp, out) && Sd_Item(&sp, 1, &ang) && Sd_Ang(out, &ang);
	Native_Free(&sp);
	Native_Free(&ang);
	return ok;
}

/* prefix + `i` */
static void Sd_Numbered(const char *prefix, long i, nativeVal_t *out) {
	char name[0x40];
	Str_Printf(name, sizeof(name), "%s%ld", prefix, i);
	Native_SetStr(out, name);
}





/* SInput.GetActionSet() */
static void Sd_ActionSet(nativeVal_t *out) {
	Native_SetStr(out, TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
}

/* Menu.DrawScorer(0, SInput.GetActionSet()), with Menu.MissionScorerCall = None before it */
static byte Sd_Draw(void) {
	nativeVal_t set;
	Sd_ActionSet(&set);
	byte ok = CALL("Menu", "DrawScorer", NULL, "iv", 0L, &set);
	Native_Free(&set);
	return ok;
}

static byte Sd_ScorerOff(void) {
	return Sd_ModNone("Menu", "MissionScorerCall") && Sd_Draw();
}

/* RacerScorer.InitMissionScorer(a, b, c); Menu.MissionScorerCall = RacerScorer.MissionScorer;
 * RacerScorer.MissionScorer(0) */
static byte Sd_ScorerOn(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c) {
	return CALL("RacerScorer", "InitMissionScorer", NULL, "vvv", a, b, c) &&
		Sd_ModTo("Menu", "MissionScorerCall", "RacerScorer", "MissionScorer") &&
		CALL("RacerScorer", "MissionScorer", NULL, "i", 0L);
}

/* RacerScorer.UpdateMissionScorer(0, a, b) */
static byte Sd_Update(const nativeVal_t *a, const nativeVal_t *b) {
	return CALL("RacerScorer", "UpdateMissionScorer", NULL, "ivv", 0L, a, b);
}

/* the parameters (win, mode, retry) of MissionsFuncs.OnSuperDealFinish */
static void Sd_Params(long win, const char *mode, const char *retry, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), win);
	Native_SetStr(Native_Append(out), mode);
	Native_SetStr(Native_Append(out), retry);
}

/* Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params) (OBJLIB 3: a call with keywords) */
static byte Sd_Remote(const char *how, const nativeVal_t *cad, const nativeVal_t *who, PyObject *func,
	const nativeVal_t *params) {
	PyObject *fn = Script_Attr("Speech", how);
	if (!fn) {
		return 0;
	}
	PyObject *p = Native_ToPy(params), *kw = p ? Py_BuildValue("{sOsO}", "AfterRemoteComFunc", func, "Parameters", p) : NULL;
	PyObject *c = kw ? Native_ToPy(cad) : NULL, *w = c ? Native_ToPy(who) : NULL;
	PyObject *a = w ? Py_BuildValue("(OO)", c, w) : NULL;
	PyObject *r = a ? Script_CallObject(fn, a, kw) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(a);
	Py_XDECREF(w);
	Py_XDECREF(c);
	Py_XDECREF(kw);
	Py_XDECREF(p);
	Py_DECREF(fn);
	return r != NULL;
}

/* if (cad and cad != ""): Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params) else:
 * apply(func, params) */
static byte Sd_End(const char *how, const nativeVal_t *cad, const nativeVal_t *who, PyObject *func,
	const nativeVal_t *params) {
	if (Sd_Text(cad)) {
		return Sd_Remote(how, cad, who, func, params);
	}
	PyObject *p = Native_ToPy(params), *r = p ? Script_Invoke(func, p) : NULL;
	Py_XDECREF(r);
	return r != NULL;
}

/* the same with the crazy gambler speaking and func (MissionsFuncs.OnSuperDealFinish as the script read it) called
 * with (win, mode, retry) */
static byte Sd_FinishFn(const char *how, const nativeVal_t *cad, PyObject *func, long win, const char *mode,
	const char *retry) {
	nativeVal_t who, params;
	Native_SetStr(&who, CG);
	Sd_Params(win, mode, retry, &params);
	byte ok = Sd_End(how, cad, &who, func, &params);
	Native_Free(&params);
	return ok;
}

/* the same with MissionsFuncs.OnSuperDealFinish read here */
static byte Sd_Finish(const char *how, const nativeVal_t *cad, long win, const char *mode, const char *retry) {
	PyObject *func = Script_Attr("MissionsFuncs", "OnSuperDealFinish");
	byte ok = func && Sd_FinishFn(how, cad, func, win, mode, retry);
	Py_XDECREF(func);
	return ok;
}

/* usr = Scrap.UsrEntity(0); if usr: (if usr.IsType("Car"): usr.Life = usr.MaxLife); usr.Invulnerable = 1 */
static byte Sd_UsrSafe(void) {
	nativeVal_t usr, life;
	Native_SetNone(&life);
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	byte ok = 1;
	if (Native_True(&usr)) {
		int car = Mode_VIsType(&usr, "Car");
		ok = car >= 0 && (!car || (Mode_VGet(&usr, "MaxLife", &life) && Mode_VSet(&usr, "Life", &life))) &&
			Sd_SetI(&usr, "Invulnerable", 1);
	}
	Native_Free(&life);
	Native_Free(&usr);
	return ok;
}


/* if (Parking.PlayerShip): Parking.PlayerShip.Invulnerable = 1 */
static byte Sd_ShipSafe(void) {
	nativeVal_t ship;
	if (!Sd_ModGet("Parking", "PlayerShip", &ship)) {
		return 0;
	}
	byte ok = !Native_True(&ship) || Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 1);
	Native_Free(&ship);
	return ok;
}

/* MissionsFuncs.cameraData = MissionsFuncs.mapsCameraData[map] (OBJLIB 5: the object of the table, as the script
 * shares it) */
static byte Sd_Camera(const char *map) {
	PyObject *cams = Script_Attr("MissionsFuncs", "mapsCameraData");
	PyObject *key = cams ? PyString_FromString((char *)map) : NULL;
	PyObject *c = key ? PyObject_GetItem(cams, key) : NULL;
	byte ok = c && Mode_SetModPy("MissionsFuncs", "cameraData", c);
	Py_XDECREF(c);
	Py_XDECREF(key);
	Py_XDECREF(cams);
	return ok;
}

/* the same when Scrap.GetSaveVar("Mission.Library") is one of the libraries */
static byte Sd_CameraFor(const char *const *libraries, const char *map) {
	nativeVal_t lib;
	Mode_SaveVar("Mission.Library", &lib);
	int in = 0;
	for (; *libraries && !in; libraries++) {
		in = Native_IsStr(&lib, *libraries);
	}
	Native_Free(&lib);
	return !in || Sd_Camera(map);
}

/* cam = Scrap.GetEntity(Scrap.GetCam(0)); if (cam): cam.Target = winner; MissionsFuncs.ActivateTarget(
 * Scrap.GetEntity(winner), 0) */
static byte Sd_CamOnWinner(const nativeVal_t *winner) {
	nativeVal_t name, cam, w;
	const char *s = cWorld_GetCamName(g_pWorld, 0);
	if (s) {
		Native_SetStr(&name, s);
	} else {
		Native_SetNone(&name);
	}
	Native_SetNone(&cam);
	Native_SetNone(&w);
	byte ok = Mode_EntityH(&name, &cam);
	if (ok && Native_True(&cam)) {
		ok = Mode_VSet(&cam, "Target", winner) && Mode_EntityH(winner, &w) &&
			CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &w, 0L);
	}
	Native_Free(&name);
	Native_Free(&cam);
	Native_Free(&w);
	return ok;
}

/* import RaceMaker, VehiclesNPC; the AI profile of the ship from the type and profile save variables */
static byte Sd_Profile(const nativeVal_t *ship, const char *typeVar, const char *profVar) {
	nativeVal_t type, prof, s;
	if (!Mode_Import("RaceMaker") || !Mode_Import("VehiclesNPC")) {
		return 0;
	}
	Mode_SaveVar(typeVar, &type);
	Mode_SaveVar(profVar, &prof);
	Native_SetNone(&s);
	byte ok = CALL("RaceMaker", "InitShipIAProfile", NULL, "vv", &type, &prof) &&
		CALL("RaceMaker", "GetRandomShipIAProfile", &s, "vv", &type, &prof) &&
		CALL("VehiclesNPC", "SetIAProf", NULL, "vv", ship, &s);
	Native_Free(&type);
	Native_Free(&prof);
	Native_Free(&s);
	return ok;
}

/* `for name in names: e = Scrap.GetEntity(name); if (e): e.TgType = tg; e.EntityList = list` (tg NULL leaves it) */
static byte Sd_MarkShips(const nativeVal_t *names, const char *tg, const char *list) {
	int n = Native_ForCount(names);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name;
		cEntity *e;
		ok = Native_SeqItem(names, i, &name) && Mode_Ent(&name, &e);
		if (ok && e) {
			ok = (!tg || Mode_SetS(e, Native_Text(&name), "TgType", tg)) && Mode_SetS(e, Native_Text(&name), "EntityList", list);
		}
		Native_Free(&name);
	}
	return ok;
}


/* GetMenuDescription(): string.replace(the text of the deal, "<n1>", Scrap.GetSaveVar(var)) (string by name, OBJLIB
 * 3) */
static byte Sd_MenuDescriptionC(const char *key, const char *var, nativeVal_t *out) {
	nativeVal_t text, n;
	if (!Mode_Import("string")) {
		return 0;
	}
	Mode_Lang(key, &text);
	Mode_SaveVar(var, &n);
	byte ok = CALL("string", "replace", out, "vsv", &text, "<n1>", &n);
	Native_Free(&text);
	Native_Free(&n);
	return ok;
}

/* import Chars, Parking, Vehicles[, more]; the player and his ship invulnerable when created, no ship lost on death */
static byte Sd_StatusC(const char *mod, const char *more) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Mode_Import("Chars") && Mode_Import("Parking") && Mode_Import("Vehicles") && (!more || Mode_Import(more)) &&
		Sd_GlobalTo("Chars", "OnCreateMainChar", mod, "SetInvulnerableStatus") &&
		Sd_GlobalTo("Parking", "OnCreatePlayerShip", mod, "SetInvulnerableStatus") &&
		Mode_SetModVar("Vehicles", "DelPlayerShipOnDeath", &zero);
}

/* SetInvulnerableStatus(entity): the entity and the player's ship (TeamDeathMatch tests the entity first) */
static byte Sd_SetInvulnerableStatusC(const nativeVal_t *e, byte test) {
	return Mode_Import("Parking") && (test && !Native_True(e) ? 1 : Sd_SetI(e, "Invulnerable", 1)) && Sd_ShipSafe();
}

/* AbortRemoteMessages(): the queue of remote messages emptied (every mode) */
static byte Sd_AbortRemoteMessagesC(void) {
	nativeVal_t l;
	Native_SetEmpty(&l, NV_LIST);
	return Mode_Import("Speech") && Mode_SetModVar("Speech", "RemoteCom", &l);
}

/* SetVulnerable() (TeamDeathMatch: OnVehicle()): the player's ship can be hit again when he boards it */
static byte Sd_SetVulnerableC(void) {
	nativeVal_t usr;
	if (!Mode_Import("Parking") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	int car = Sd_UsrCar(&usr);
	byte ok = car >= 0 && (!car || Sd_SetI(&usr, "Invulnerable", 0)) && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0);
	Native_Free(&usr);
	return ok;
}

/* RespawnPlayer(LooserName) of DeathMatch and TeamDeathMatch: the player's ship again at a spawn point (one
 * GetSpawnPos for both) */
static byte Sd_RespawnPlayerC(const nativeVal_t *name) {
	nativeVal_t me, sp, v;
	if (!Mode_Import("Parking") || !Mode_Import("Vehicles") || !Mode_Import("OutMap") ||
		!CALL("Vehicles", "ReSpawnShip", NULL, "v", name)) {
		return 0;
	}
	Native_SetNone(&me);
	Native_SetNone(&sp);
	Native_SetNone(&v);
	byte ok = Mode_EntityH(name, &me) && CALL("OutMap", "GetSpawnPos", &sp, "") && Sd_Item(&sp, 0, &v) &&
		Mode_VSet(&me, "Pos", &v);
	Native_Free(&v);
	ok = ok && Sd_Item(&sp, 1, &v) && Sd_Ang(&me, &v);
	Native_Free(&me);
	Native_Free(&sp);
	Native_Free(&v);
	return ok;
}

/* RestartCTF(id, control) and RestartRace(id, control): the level again */
static byte Sd_RestartC(void) {
	return CALL("SaveGame", "Restart", NULL, "");
}

MODE_FN(Sd, SetInvulnerable, 1, Sd_SetInvulnerableStatusC(&a[0], 0))
MODE_FN(Sd, AbortRemoteMessages, 0, Sd_AbortRemoteMessagesC())
MODE_FN(Sd, SetVulnerable, 0, Sd_SetVulnerableC())
MODE_FN(Sd, RespawnPlayer, 1, Sd_RespawnPlayerC(&a[0]))
MODE_FN(Sd, Restart, 2, Sd_RestartC())

/* Scrap.GetEntity(name).Del() */
static byte Sd_Del(const nativeVal_t *name) {
	cEntity *e;
	return Mode_Ent(name, &e) && Mode_Del(e) >= 0;
}

/* the same with func an object (Speech.ForceRemoteCom, MissionsFuncs.OnSuperDealFinish) at Scrap.GetTime() + delay */
static byte Sd_LaterObj(double delay, PyObject *fn, const nativeVal_t *args, int n) {
	nativeVal_t t;
	Native_SetFloat(&t, Mode_Time() + delay);
	return Mode_Schedule(&t, fn, args, n, NULL);
}

static byte Sd_EqI(const nativeVal_t *a, long i) {
	nativeVal_t b;
	Native_SetInt(&b, i);
	return !Native_Compare(a, &b);
}

/* list.append(e.Name) of a global list of the module, in place */
static byte Sd_AppendName(const char *mod, const char *list, const nativeVal_t *e) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(e, "Name", &n) && Mode_Append(mod, list, &n);
	Native_Free(&n);
	return ok;
}

/* if x in list: list.remove(x), of a global list of the module (slot its copy in C), in place */
static byte Sd_RemoveIn(const char *mod, const char *list, nativeVal_t *slot, const nativeVal_t *x) {
	if (!Mode_Read(mod, list, slot)) {
		return 0;
	}
	int in = Native_Contains(slot, x);
	return in > 0 ? Mode_Remove(mod, list, x) && Mode_Read(mod, list, slot) : in == 0;
}

/* the ships of the entity list from the first by NextInList: fn(e, name, arg) for each (e the handle value, name the
 * one the loop read: EntityListGet or NextInList), 1 to go on, 0 to stop, -1 with the error */
typedef int (*sdEach_t)(const nativeVal_t *e, const nativeVal_t *name, void *arg);

static byte Sd_EachInList(const char *list, sdEach_t fn, void *arg) {
	nativeVal_t name, e;
	Native_SetNone(&name);
	Native_SetNone(&e);
	byte ok = Mode_ListGet(list, "First", &name) && Mode_EntityH(&name, &e);
	while (ok && Native_True(&e)) {
		int r = fn(&e, &name, arg);
		if (r <= 0) {
			ok = r == 0;
			break;
		}
		Native_Free(&name);
		ok = Mode_VGet(&e, "NextInList", &name);
		Native_Free(&e);
		ok = ok && Mode_EntityH(&name, &e);
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* usr = Scrap.UsrEntity(0); if usr: usr.Invulnerable = 1 */
static byte Sd_UsrInvulnerable(void) {
	nativeVal_t usr;
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	byte ok = !Native_True(&usr) || Sd_SetI(&usr, "Invulnerable", 1);
	Native_Free(&usr);
	return ok;
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func of the module, args) (Mode_Later for a number) with delay a
 * value of the script (a global of the module, the result of a function) */
static byte Sd_LaterV(const nativeVal_t *delay, const char *mod, const char *func, const nativeVal_t *args, int n) {
	nativeVal_t now, t;
	PyObject *fn = Mode_GlobalPy(mod, func);
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&t);
	byte ok = Native_Arith('+', &now, delay, &t) && Mode_Schedule(&t, fn, args, n, NULL);
	Native_Free(&t);
	Py_DECREF(fn);
	return ok;
}

static byte Sd_SetF(const nativeVal_t *h, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Mode_VSet(h, var, &v);
}

static byte Sd_SpawnAt(const nativeVal_t *usr) {
	nativeVal_t sp, v;
	Native_SetNone(&sp);
	Native_SetNone(&v);
	byte ok = CALL("OutMap", "GetSpawnPos", &sp, "") && Sd_Item(&sp, 0, &v) && Mode_VSet(usr, "Pos", &v);
	Native_Free(&sp);
	Native_Free(&v);
	ok = ok && CALL("OutMap", "GetSpawnPos", &sp, "") && Sd_Item(&sp, 1, &v) && Sd_Ang(usr, &v);
	Native_Free(&sp);
	Native_Free(&v);
	return ok;
}

/* the keyword argument of a function (one, optional) as a value: def when it is not given */
static byte Sd_KwArg(PyObject *args, PyObject *kw, char *name, long def, nativeVal_t *out) {
	char *names[] = { name, NULL };
	PyObject *o = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &o)) {
		return 0;
	}
	if (o) {
		Native_Arg(o, out);
	} else if (def < 0) {
		Native_SetNone(out);
	} else {
		Native_SetInt(out, def);
	}
	return 1;
}

/* the same for name + "CheckArrow" (the arrow of a baton) */
static byte Sd_DelArrow(const nativeVal_t *item) {
	nativeVal_t s, n;
	Native_SetStr(&s, "CheckArrow");
	Native_SetNone(&n);
	byte ok = Native_Arith('+', item, &s, &n) && Sd_Del(&n);
	Native_Free(&n);
	return ok;
}

/* ---- the lines of the modes still on Sf_ frames (until each mode is native) ---- */

/* Scrap.GetSaveVar(name), int(Scrap.GetSaveVar(name)), Scrap.SetSaveVar(name, "text") */
static PyObject *Sdf_Save(scriptFrame_t *f, const char *name) {
	return Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", name);
}

static PyObject *Sdf_SaveInt(scriptFrame_t *f, const char *name) {
	return Sf_ToInt(f, Sdf_Save(f, name));
}

/* Scrap.UsrEntity(0), Scrap.GetLangStr("key"), Scrap.Set(var, v), Scrap.Set(var, "text") */
static PyObject *Sdf_Usr(scriptFrame_t *f) {
	return Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
}

static PyObject *Sdf_Lang(scriptFrame_t *f, const char *key) {
	return Sf_Lang(f, Sf_Str(f, key));
}

static void Sdf_Set(scriptFrame_t *f, const char *var, PyObject *v) {
	if (v) {
		Sf_Ask(f, "Scrap", "Set", "(sO)", var, v);
	}
}

static void Sdf_SetS(scriptFrame_t *f, const char *var, const char *text) {
	Sf_Ask(f, "Scrap", "Set", "(ss)", var, text);
}

/* a function of a module as its functions call it, with Py_BuildValue arguments */
#define Sdf_Call(f, mod, func, ...) Sf_Call(f, Sf_Global(f, mod, func), __VA_ARGS__)

/* `usr and usr.Name == name`, `usr and usr.IsType("Car")`, `cad and cad != ""` as conditions */
static int Sdf_UsrIs(scriptFrame_t *f, PyObject *usr, PyObject *name) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_Compare(f, Sf_Attr(f, usr, "Name"), "==", name) : t;
}

static int Sdf_UsrCar(scriptFrame_t *f, PyObject *usr) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_True(f, Sf_Method(f, usr, "IsType", "(s)", "Car")) : t;
}

/* name = name + 1 (or - 1) of a module global, the new value */
static PyObject *Sdf_Add(scriptFrame_t *f, const char *mod, const char *name, long add) {
	PyObject *v = Sf_OpI(f, Sf_Global(f, mod, name), '+', add);
	Sf_SetGlobal(f, mod, name, v);
	return v;
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func of the module, args) */
static void Sdf_Later(scriptFrame_t *f, PyObject *delay, const char *mod, const char *func, PyObject *args) {
	Sf_Later(f, delay, Sf_Global(f, mod, func), args, NULL);
}

/* e.Ang = (0, ang, 0) */
static void Sdf_Ang(scriptFrame_t *f, PyObject *e, PyObject *ang) {
	Sf_SetAttr(f, e, "Ang", Sf_Build(f, "(iOi)", 0, ang, 0));
}

/* Scrap.CreateEntity(name, pos[0], pos[1], pos[2], cls) */
static PyObject *Sdf_Create(scriptFrame_t *f, PyObject *name, PyObject *pos, const char *cls) {
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
	return Sf_Ask(f, "Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, cls);
}

/* list.append(e.Name) of a module global list */
static void Sdf_AppendName(scriptFrame_t *f, const char *mod, const char *list, PyObject *e) {
	Sf_Method(f, Sf_Global(f, mod, list), "append", "(O)", Sf_Attr(f, e, "Name"));
}

/* if x in list: list.remove(x), of a module global list */
static void Sdf_RemoveIn(scriptFrame_t *f, const char *mod, const char *list, PyObject *x) {
	if (Sf_In(f, x, Sf_Global(f, mod, list)) > 0) {
		Sf_Method(f, Sf_Global(f, mod, list), "remove", "(O)", x);
	}
}

/* Scrap.GetEntity(name).Del() */
static void Sdf_Del(scriptFrame_t *f, PyObject *name) {
	Sf_Method(f, Sf_Entity(f, name), "Del", "()");
}

/* Menu.DrawScorer(0, SInput.GetActionSet()), with Menu.MissionScorerCall = None before it */
static void Sdf_Draw(scriptFrame_t *f) {
	Sf_Ask(f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(f, "SInput", "GetActionSet", "()"));
}

static void Sdf_ScorerOff(scriptFrame_t *f) {
	Sf_SetMod(f, "Menu", "MissionScorerCall", Py_None);
	Sdf_Draw(f);
}

/* RacerScorer.UpdateMissionScorer(0, a, b) */
static void Sdf_Update(scriptFrame_t *f, PyObject *a, PyObject *b) {
	Sf_Ask(f, "RacerScorer", "UpdateMissionScorer", "(iOO)", 0, a, b);
}

/* SSound.Play(Sound.<snd>, Sound.<vol>) */
static void Sdf_Sound(scriptFrame_t *f, const char *snd, const char *vol) {
	PyObject *s = Sf_Mod(f, "Sound", snd);
	Sf_Ask(f, "SSound", "Play", "(OO)", s, Sf_Mod(f, "Sound", vol));
}

/* Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params) */
static void Sdf_Remote(scriptFrame_t *f, const char *how, PyObject *cad, PyObject *who, PyObject *func,
	PyObject *params) {
	PyObject *fn = Sf_Mod(f, "Speech", how);
	PyObject *kw = Sf_Build(f, "{sOsO}", "AfterRemoteComFunc", func, "Parameters", params);
	Sf_CallKw(f, fn, kw ? Py_BuildValue("(OO)", cad, who) : NULL, kw);
}

/* usr = Scrap.UsrEntity(0); if usr: (if usr.IsType("Car"): usr.Life = usr.MaxLife); usr.Invulnerable = 1 */
static void Sdf_UsrSafe(scriptFrame_t *f) {
	PyObject *usr = Sdf_Usr(f);
	if (Sf_True(f, usr) > 0) {
		if (Sf_True(f, Sf_Method(f, usr, "IsType", "(s)", "Car")) > 0) {
			Sf_SetAttr(f, usr, "Life", Sf_Attr(f, usr, "MaxLife"));
		}
		Sf_SetAttrI(f, usr, "Invulnerable", 1);
	}
}

/* MissionsFuncs.cameraData = MissionsFuncs.mapsCameraData[map] */
static void Sdf_Camera(scriptFrame_t *f, const char *map) {
	PyObject *cams = Sf_Mod(f, "MissionsFuncs", "mapsCameraData");
	Sf_SetMod(f, "MissionsFuncs", "cameraData", Sf_GetItem(f, cams, Sf_Str(f, map)));
}

/* the same when Scrap.GetSaveVar("Mission.Library") is one of the libraries */
static void Sdf_CameraFor(scriptFrame_t *f, const char *const *libraries, const char *map) {
	if (Sf_InS(f, Sdf_Save(f, "Mission.Library"), libraries) > 0) {
		Sdf_Camera(f, map);
	}
}

/* GetMenuDescription(): the text of the deal with its number */
static PyObject *Sdf_MenuDescription(PyObject *args, const char *key, const char *var) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *text = Sdf_Lang(&f, key);
	return Sf_Return(&f, Sf_Ask(&f, "string", "replace", "(OsO)", text, "<n1>", Sdf_Save(&f, var)));
}

/* import Chars, Parking, Vehicles[, more]; the player and his ship invulnerable when created, no ship lost on death */
static void Sdf_Status(scriptFrame_t *f, const char *mod, const char *more) {
	Sf_Import(f, "Chars");
	Sf_Import(f, "Parking");
	Sf_Import(f, "Vehicles");
	if (more) {
		Sf_Import(f, more);
	}
	Sf_SetMod(f, "Chars", "OnCreateMainChar", Sf_Global(f, mod, "SetInvulnerableStatus"));
	Sf_SetMod(f, "Parking", "OnCreatePlayerShip", Sf_Global(f, mod, "SetInvulnerableStatus"));
	Sf_SetMod(f, "Vehicles", "DelPlayerShipOnDeath", Sf_Int(f, 0));
}

/* LostBatonMessage() of Domination and MortalRace: the message that the player lost the baton */
static PyObject *Sdf_LostBaton(PyObject *args, const char *key) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *cad = Sdf_Lang(&f, key);
	PyObject *t = Sf_Time(&f);
	t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad));
	Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, t);
	return Sf_None(&f);
}

/* the baton of Domination and MortalRace: l = the item "Baton" at pos with its model, radius and OnTake */
static PyObject *Sdf_Baton(scriptFrame_t *f, const char *mod, PyObject *pos, const char *onTake) {
	PyObject *l = Sdf_Create(f, Sf_Str(f, "Baton"), pos, "Item");
	Sf_Method(f, l, "SetModel", "(O)", Sf_Global(f, mod, "BatonModel"));
	Sf_SetAttr(f, l, "Radius", Sf_Global(f, mod, "BatonRadius"));
	Sf_SetAttrI(f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(f, l, "AutoFace", 0);
	Sf_SetAttrI(f, l, "OnlyUser", 0);
	Sf_SetAttr(f, l, "rTime", Sf_Float(f, 0.25));
	Sf_SetAttrS(f, l, "OnTake", onTake);
	return l;
}

/* l2 = Scrap.CreateEntity(l.Name + "CheckArrow", pos..., "WithLife"); l2.CamTgt = 0, 1000, 0 */
static PyObject *Sdf_BatonArrow(scriptFrame_t *f, PyObject *l, PyObject *pos) {
	PyObject *l2 = Sdf_Create(f, Sf_Cat(f, "Os", Sf_Attr(f, l, "Name"), "CheckArrow"), pos, "WithLife");
	Sf_SetAttr(f, l2, "CamTgt", Sf_Build(f, "(iii)", 0, 1000, 0));
	return l2;
}

/* the ships of the entity list from the first by NextInList: fn(f, e, name, arg) for each, 1 to go on; the name is
 * the one the loop read (EntityListGet or NextInList) */
static void Sdf_EachInList(scriptFrame_t *f, const char *list, int (*fn)(scriptFrame_t *f, PyObject *e, PyObject *name,
	void *arg), void *arg) {
	PyObject *name = Sf_Ask(f, "Scrap", "EntityListGet", "(ss)", list, "First");
	PyObject *e = Sf_Entity(f, name);
	int mark = f->n;
	while (Sf_True(f, e) > 0) {
		if (!fn(f, e, name, arg)) {
			break;
		}
		name = Sf_Attr(f, e, "NextInList");
		e = Sf_Entity(f, name);
		PyObject *keep[2] = { name, e };
		Sf_ReleaseKeep(f, mark, keep, 2);
	}
}

/* ---- DeathMatch: all against all in the CA, the first with CombatDeaths frags wins ---- */

static struct {
	nativeVal_t enemiesFrags;	/* the frags of each enemy name (a dict) */
	nativeVal_t playerFrags, keepPlaying;
} dm;

MODE_FN(Dm, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_DeathMatch_Description", "CombatDeaths", out))
MODE_FN(Dm, InitStatus, 1, Sd_StatusC(DM, NULL))

/* Init(LevelPath): the camera of the end, the death of the player, the enemies, the start when the player boards */
static byte Dm_InitC(void) {
	static const char *const third[] = { "SuperDeal_Third", NULL };
	return Mode_Import("Parking") && Mode_Import("Menu") && Sd_CameraFor(third, "CA") && Mode_Import("Chars") &&
		Sd_GlobalTo("Chars", "OnMainCharDeath", DM, "OnPlayerDeath") && OWN(DM, "CreateEnemies", NULL, "") &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", DM, "CrazyGamblerStartCombat");
}

MODE_FN(Dm, Init, 1, Dm_InitC())
/* SetProfile(enemy) (TeamDeathMatch: SetEnemyProfile): the AI of an enemy */
MODE_FN(Dm, SetProfile, 1, Sd_Profile(&a[0], "CombatEnemyType", "CombatEnemyProfile"))

/* CreateEnemies(): Enemy1 to EnemyN at spawn points, with their frags at 0 when the table is empty */
static byte Dm_CreateEnemiesC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *keys = Mode_V(&t);
	long n = 0;
	if (!Mode_Import("OutMap") || !Sd_SaveInt("CombatNumEnemies", num) || !Mode_Keys(DM, "enemiesFrags", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int size = Native_Size(keys);
	if (size < 0 || !Sd_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *en = Mode_V(&t);
		Sd_Numbered("Enemy", i, name);
		ok = Sd_SpawnCarAng(name, e) && OWN(DM, "SetProfile", NULL, "v", e) &&
			(size || (Mode_VGet(e, "Name", en) && Mode_DictPut(DM, "enemiesFrags", &dm.enemiesFrags, en, Mode_I(&t, 0))));
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Dm, CreateEnemies, 0, Dm_CreateEnemiesC())

/* Speech.AddRemoteCom(Scrap.GetSaveVar(cadVar), "CrazyGambler", mod.func) */
static byte Sd_GamblerSays(const char *cadVar, const char *mod, const char *func) {
	nativeVal_t cad;
	PyObject *fn = Mode_GlobalPy(mod, func);
	if (!fn) {
		return 0;
	}
	Mode_SaveVar(cadVar, &cad);
	byte ok = CALL("Speech", "AddRemoteCom", NULL, "vsO", &cad, CG, fn);
	Native_Free(&cad);
	Py_DECREF(fn);
	return ok;
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the combat */
static byte Dm_CrazyGamblerStartCombatC(void) {
	return Mode_Import("Parking") && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", DM, "SetVulnerable") &&
		Sd_GamblerSays("CombatStartTextCad", DM, "StartCombat");
}

MODE_FN(Dm, CrazyGamblerStartCombat, 0, Dm_CrazyGamblerStartCombatC())

/* StartCombat(): the deaths counted, the scorer, the enemies red in the list DeathMatch */
static byte Dm_StartCombatC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *deaths = Mode_V(&t), *keys = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") ||
		!Sd_GlobalTo("Vehicles", "OnDeathCallback", DM, "OnEnemyDeath")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("CombatDeaths", deaths);
	return Mode_Done(&t, Sd_ScorerOn(zero, zero, deaths) && Mode_Keys(DM, "enemiesFrags", 1, keys) &&
		Sd_MarkShips(keys, "red", "DeathMatch"));
}

MODE_FN(Dm, StartCombat, 0, Dm_StartCombatC())

/* GetMaxFragsEnemies(): the most frags of an enemy */
static byte Dm_GetMaxFragsEnemiesC(nativeVal_t *out) {
	nativeVal_t values;
	Native_SetInt(out, 0);
	if (!Mode_Values(DM, "enemiesFrags", 1, &values)) {
		return 0;
	}
	int n = Native_ForCount(&values);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t v;
		ok = Native_SeqItem(&values, i, &v);
		if (ok && Native_Compare(&v, out) > 0) {
			Native_Free(out);
			*out = v;
		} else if (ok) {
			Native_Free(&v);
		}
	}
	Native_Free(&values);
	return ok;
}

MODE_FN(Dm, GetMaxFragsEnemies, 0, Dm_GetMaxFragsEnemiesC(out))

/* the end of a deathmatch (and of a team deathmatch): the lines both share after the frag that ends it */
static byte Dm_Finish(void) {
	return Sd_ModNone("Chars", "OnMainCharDeath") && Sd_ModNone("Vehicles", "OnDeathCallback") &&
		Sd_ModNone("Parking", "SwitchCharMissionCallback");
}

/* the hook of a mission instead of the score table: KeepPlaying(enemiesFrags.keys()), after the message when there
 * is one (how is the Speech function) */
static byte Dm_Keep(const char *how, const nativeVal_t *cad) {
	nativeVal_t keys, params, who;
	PyObject *hook = Mode_GlobalPy(DM, "KeepPlaying");
	if (!hook) {
		return 0;
	}
	Native_SetNone(&keys);
	byte ok = Mode_Keys(DM, "enemiesFrags", 1, &keys);
	if (ok) {
		Native_SetEmpty(&params, NV_TUPLE);
		*Native_Append(&params) = keys;
		Native_SetNone(&keys);
		Native_SetStr(&who, CG);
		ok = Sd_End(how, cad, &who, hook, &params);
		Native_Free(&params);
	}
	Native_Free(&keys);
	Py_DECREF(hook);
	return ok;
}

/* the frags of a deathmatch end it: the player safe (safe), the ships invulnerable and relaxed, the save variable
 * of the result (player: he won), the scorer off; then the last message and the score table, or KeepPlaying of a
 * mission. how is the Speech function; cam the winner the camera looks at (NULL: none) */
static byte Dm_End(byte safe, byte player, const char *how, const nativeVal_t *cam) {
	nativeVal_t cad;
	PyObject *func = NULL;
	if ((safe && !Sd_UsrSafe()) || !Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") ||
		!Mode_Import("Parking") || !CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "s", "DeathMatch") ||
		!CALL("MissionsFuncs", "RelaxShipsInList", NULL, "s", "DeathMatch")) {
		return 0;
	}
	Sd_SetSave("PlayerWinsCombat", player ? "1" : "0");
	Mode_SaveVar(player ? "CombatFinishPlayerWinsTextCad" : "CombatFinishPlayerLoosesTextCad", &cad);
	byte ok = (cam || (func = Script_Attr("MissionsFuncs", "OnSuperDealFinish")) != NULL) && Sd_ScorerOff() &&
		Dm_Finish() && Mode_Read(DM, "KeepPlaying", &dm.keepPlaying);
	if (ok && !Native_True(&dm.keepPlaying)) {
		if (cam) {
			ok = Sd_CamOnWinner(cam) && Sd_Finish(how, &cad, 0, "DeathMatch", "DeathMatch.RestartCombat");
		} else {
			ok = Sd_FinishFn(how, &cad, func, player, "DeathMatch", player ? "" : "DeathMatch.RestartCombat");
		}
	} else if (ok) {
		ok = Dm_Keep(how, &cad);
	}
	Py_XDECREF(func);
	Native_Free(&cad);
	return ok;
}

/* the frag counted without the end: the scorer, the dead ship back after 5 s (func), inactive until then */
static byte Dm_Next(const nativeVal_t *loser, const char *func) {
	nativeVal_t max;
	cEntity *e;
	Native_SetNone(&max);
	byte ok = Mode_Read(DM, "playerFrags", &dm.playerFrags) && OWN(DM, "GetMaxFragsEnemies", &max, "") &&
		Sd_Update(&dm.playerFrags, &max) && Mode_Later(5.0, DM, func, loser, 1, NULL) && Mode_Ent(loser, &e) &&
		Mode_SetI(e, Native_Text(loser), "Active", 0);
	Native_Free(&max);
	return ok;
}

/* enemiesFrags[winner] = enemiesFrags[winner] + 1 */
static byte Dm_Frag(const nativeVal_t *winner) {
	nativeVal_t n, one, r;
	Native_SetInt(&one, 1);
	Native_SetNone(&n);
	Native_SetNone(&r);
	byte ok = Mode_Read(DM, "enemiesFrags", &dm.enemiesFrags) && Native_Subscript(&dm.enemiesFrags, winner, &n) &&
		Native_Arith('+', &n, &one, &r) && Mode_DictPut(DM, "enemiesFrags", &dm.enemiesFrags, winner, &r);
	Native_Free(&n);
	Native_Free(&r);
	return ok;
}

/* enemiesFrags[winner] compared with int(CombatDeaths): -1, 0, 1, or -2 with the error */
static int Dm_FragsCmp(const nativeVal_t *winner) {
	nativeVal_t frags, deaths;
	Native_SetNone(&frags);
	Native_SetNone(&deaths);
	int c = Mode_Read(DM, "enemiesFrags", &dm.enemiesFrags) && Native_Subscript(&dm.enemiesFrags, winner, &frags) &&
		Sd_SaveInt("CombatDeaths", &deaths) ? Native_Compare(&frags, &deaths) : -2;
	Native_Free(&frags);
	Native_Free(&deaths);
	return c;
}

/* winner in enemiesFrags.keys(): 1, 0, -1 with the error */
static int Dm_IsEnemy(const nativeVal_t *winner) {
	nativeVal_t keys;
	Native_SetNone(&keys);
	int in = Mode_Keys(DM, "enemiesFrags", 1, &keys) ? Native_Contains(&keys, winner) : -1;
	Native_Free(&keys);
	return in;
}

/* OnEnemyDeath(LooserName, WinnerName): a frag of the player or an enemy; the enemy comes back after 5 s unless the
 * frag ends the deathmatch */
static byte Dm_OnEnemyDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *deaths = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int u = Sd_UsrIs(usr, loser);
	if (u) {
		return Mode_Done(&t, u > 0);
	}
	int end = 0, player = 0;
	if (Native_True(winner)) {
		u = Sd_UsrIs(usr, winner);
		if (u > 0) {
			player = 1;
			end = Sd_Add(DM, "playerFrags", &dm.playerFrags, 1) && Sd_SaveInt("CombatDeaths", deaths) ?
				Sd_Eq(&dm.playerFrags, deaths) : -1;
		} else if (u == 0) {
			int in = Dm_IsEnemy(winner);
			if (in > 0) {
				int c = Dm_Frag(winner) ? Dm_FragsCmp(winner) : -2;
				end = c == -2 ? -1 : c == 0;
			}
			end = in < 0 ? -1 : end;
		} else {
			end = -1;
		}
	}
	if (end < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, end ? Dm_End(1, (byte)player, "ForceRemoteCom", NULL) : Dm_Next(loser, "RespawnEnemy"));
}

MODE_FN(Dm, OnEnemyDeath, 2, Dm_OnEnemyDeathC(&a[0], &a[1]))

/* RespawnEnemy(enemyName): the enemy again at a spawn point, unless the list is full */
static byte Dm_RespawnEnemyC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *max = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_ListGet("DeathMatch", "Num", num) || !Sd_SaveInt("CombatNumEnemies", max)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(num, max) >= 0) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Sd_SpawnCarAng(name, e) && Sd_SetS(e, "TgType", "red") && Sd_SetS(e, "EntityList", "DeathMatch") &&
		OWN(DM, "SetProfile", NULL, "v", e));
}

MODE_FN(Dm, RespawnEnemy, 1, Dm_RespawnEnemyC(&a[0]))

/* OnPlayerDeath(LooserName, WinnerName): a frag of an enemy; the player comes back after 5 s unless it ends the
 * deathmatch */
static byte Dm_OnPlayerDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	if (!Mode_Import("RacerScorer")) {
		return 0;
	}
	int in = Dm_IsEnemy(winner);
	if (in < 0 || (in > 0 && !Dm_Frag(winner))) {
		return 0;
	}
	int c = in > 0 ? Dm_FragsCmp(winner) : -1;
	if (c == -2) {
		return 0;
	}
	return c >= 0 ? Dm_End(0, 0, "AddRemoteCom", winner) : Dm_Next(loser, "RespawnPlayer");
}

MODE_FN(Dm, OnPlayerDeath, 2, Dm_OnPlayerDeathC(&a[0], &a[1]))

/* RestartCombat(id, control): the frags at 0 and the level again */
static byte Dm_RestartCombatC(void) {
	nativeVal_t d;
	Native_SetEmpty(&d, NV_DICT);
	byte ok = Mode_Put(DM, "enemiesFrags", &dm.enemiesFrags, &d) && Sd_PutI(DM, "playerFrags", &dm.playerFrags, 0) &&
		CALL("SaveGame", "Restart", NULL, "");
	Native_Free(&d);
	return ok;
}

MODE_FN(Dm, RestartCombat, 2, Dm_RestartCombatC())

PyMethodDef g_DeathMatchMethods[] = {
	{ "GetMenuDescription", Dm_GetMenuDescription, 1, NULL },
	{ "InitStatus", Dm_InitStatus, 1, NULL },
	{ "Init", Dm_Init, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "SetProfile", Dm_SetProfile, 1, NULL },
	{ "CreateEnemies", Dm_CreateEnemies, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartCombat", Dm_CrazyGamblerStartCombat, 1, NULL },
	{ "SetVulnerable", Sd_SetVulnerable, 1, NULL },
	{ "StartCombat", Dm_StartCombat, 1, NULL },
	{ "GetMaxFragsEnemies", Dm_GetMaxFragsEnemies, 1, NULL },
	{ "OnEnemyDeath", Dm_OnEnemyDeath, 1, NULL },
	{ "RespawnEnemy", Dm_RespawnEnemy, 1, NULL },
	{ "RespawnPlayer", Sd_RespawnPlayer, 1, NULL },
	{ "OnPlayerDeath", Dm_OnPlayerDeath, 1, NULL },
	{ "RestartCombat", Dm_RestartCombat, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- Versus: one enemy in the CA, the first with CombatDeaths deaths of the other wins ---- */

static struct {
	nativeVal_t combatDeaths, enemyName, combatEnemyType, combatEnemyProfile, charLangTrans;
} vs;

MODE_FN(Vs, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_Versus_Description", "CombatDeaths", out))
MODE_FN(Vs, InitStatus, 1, 1)

/* Init(LevelPath): the camera of the end, the deaths to reach, the enemy, the start when the player boards */
static byte Vs_InitC(void) {
	nativeVal_t zero, deaths;
	Native_SetInt(&zero, 0);
	Native_SetNone(&deaths);
	byte ok = Mode_Import("Parking") && Mode_Import("Menu") && Mode_Import("Vehicles") && Sd_Camera("CA") &&
		Mode_SetModVar("Vehicles", "DelPlayerShipOnDeath", &zero) && Sd_SaveInt("CombatDeaths", &deaths) &&
		Mode_Put(VS, "combatDeaths", &vs.combatDeaths, &deaths) && Mode_Import("Chars") &&
		Sd_GlobalTo("Chars", "OnMainCharDeath", VS, "OnPlayerDeath") && OWN(VS, "CreateEnemy", NULL, "") &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", VS, "CrazyGamblerStartCombat");
	Native_Free(&deaths);
	return ok;
}

MODE_FN(Vs, Init, 1, Vs_InitC())

/* CreateEnemy(markAsEnemy = 0): the enemy at a spawn point with the ship of its type and profile; marked as the
 * target, else invulnerable until the start */
static byte Vs_CreateEnemyC(const nativeVal_t *mark) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *pos = Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *e = Mode_V(&t), *v = Mode_V(&t), *ship = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("VehiclesNPC") || !Mode_Import("MissionsFuncs") || !Mode_Import("OutMap") ||
		!CALL("OutMap", "GetSpawnPos", sp, "") || !Mode_Unpack(sp, 2, pos) ||
		!Sd_Create(Mode_S(&t, "Enemy"), &pos[0], "Car", e) || !Sd_Ang(e, &pos[1]) || !Mode_VGet(e, "Name", v) ||
		!Mode_Put(VS, "enemyName", &vs.enemyName, v)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Mode_SaveVar("CombatEnemyType", v);
	if (!Mode_Put(VS, "combatEnemyType", &vs.combatEnemyType, v)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Mode_SaveVar("CombatEnemyProfile", v);
	if (!Mode_Put(VS, "combatEnemyProfile", &vs.combatEnemyProfile, v) || !Mode_Read(VS, "combatEnemyType", &vs.combatEnemyType) ||
		!Mode_Read(VS, "combatEnemyProfile", &vs.combatEnemyProfile) ||
		!CALL("RaceMaker", "InitShipIAProfile", NULL, "vv", &vs.combatEnemyType, &vs.combatEnemyProfile) ||
		!Mode_Read(VS, "combatEnemyType", &vs.combatEnemyType) || !Mode_Read(VS, "combatEnemyProfile", &vs.combatEnemyProfile) ||
		!Mode_Cat(&t, ship, 4, Mode_S(&t, "S"), &vs.combatEnemyType, Mode_S(&t, "1_"), &vs.combatEnemyProfile) ||
		!CALL("VehiclesNPC", "SetIAProf", NULL, "vv", e, ship)) {
		return Mode_Done(&t, 0);
	}
	if (Sd_EqI(mark, 1)) {
		return Mode_Done(&t, Sd_SetS(e, "TgType", "red") && Sd_SetS(e, "EntityList", "Enemies") &&
			CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 1L));
	}
	return Mode_Done(&t, Sd_SetI(e, "Invulnerable", 1));
}

static PyObject *Vs_CreateEnemy(PyObject *self, PyObject *args, PyObject *kw) {
	nativeVal_t mark;
	(void)self;
	if (!Sd_KwArg(args, kw, "markAsEnemy", 0, &mark)) {
		return NULL;
	}
	byte ok = Vs_CreateEnemyC(&mark);
	Native_Free(&mark);
	return Mode_Result(ok);
}

/* int(Scrap.GetSaveVar(a)) == 0 and int(Scrap.GetSaveVar(b)) == 0 (no round played): 1, 0, -1 with the error */
static int Sd_BothZero(const char *a, const char *b) {
	nativeVal_t n;
	Native_SetNone(&n);
	int r = Sd_SaveInt(a, &n) ? Sd_EqI(&n, 0) : -1;
	Native_Free(&n);
	if (r > 0) {
		r = Sd_SaveInt(b, &n) ? Sd_EqI(&n, 0) : -1;
		Native_Free(&n);
	}
	return r;
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first round, a later one
 * starts after 3 s */
static byte Vs_CrazyGamblerStartCombatC(void) {
	if (!Mode_Import("Parking") || !Sd_ModNone("Parking", "SwitchVehicleMissionCallback")) {
		return 0;
	}
	int first = Sd_BothZero("NumEnemyDeaths", "NumPlayerDeaths");
	return first > 0 ? Sd_GamblerSays("CombatStartTextCad", VS, "StartCombat") :
		first == 0 && Mode_Later(3.0, VS, "StartCombat", NULL, 0, NULL);
}

MODE_FN(Vs, CrazyGamblerStartCombat, 0, Vs_CrazyGamblerStartCombatC())

/* the text of the enemy's type (auxText of the script, which is never used): CharLangTrans[type] or the type */
static byte Vs_AuxText(void) {
	nativeVal_t x;
	if (!Mode_Read(VS, "combatEnemyType", &vs.combatEnemyType)) {
		return 0;
	}
	int has = Mode_HasKey(VS, "CharLangTrans", 1, &vs.combatEnemyType);
	if (has < 0) {
		return 0;
	}
	if (!has) {
		return Mode_Read(VS, "combatEnemyType", &vs.combatEnemyType);
	}
	Native_SetNone(&x);
	byte ok = Mode_Read(VS, "CharLangTrans", &vs.charLangTrans) && Mode_Read(VS, "combatEnemyType", &vs.combatEnemyType) &&
		Native_Subscript(&vs.charLangTrans, &vs.combatEnemyType, &x);
	Native_Free(&x);
	return ok;
}

/* `x` of the script as a text value */
static void Sd_Repr(const nativeVal_t *v, nativeVal_t *out) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, v, NR_REPR);
	Native_SetText(out, b.p, b.len);
	Native_BufFree(&b);
}

/* StartCombat(): the deaths counted, the scorer, the enemy as the red target */
static byte Vs_StartCombatC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *limit = Mode_V(&t), *type = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") || !Mode_Import("MissionsFuncs") ||
		!Mode_Read(VS, "enemyName", &vs.enemyName) || !Mode_EntityH(&vs.enemyName, e) ||
		!Sd_GlobalTo("Vehicles", "OnDeathCallback", VS, "OnEnemyDeath") || !Sd_SaveInt("NumEnemyDeaths", a) ||
		!Sd_SaveInt("NumPlayerDeaths", b) || !Mode_Read(VS, "combatDeaths", &vs.combatDeaths)) {
		return Mode_Done(&t, 0);
	}
	Sd_Repr(&vs.combatDeaths, limit);
	if (!Sd_ScorerOn(a, b, limit) || !Sd_SetS(e, "TgType", "red") || !Sd_SetS(e, "EntityList", "Enemies") ||
		!Sd_SetI(e, "Invulnerable", 0) || !CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 1L)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("CombatEnemyType", type);
	return Mode_Done(&t, Mode_Put(VS, "combatEnemyType", &vs.combatEnemyType, type) && Vs_AuxText());
}

MODE_FN(Vs, StartCombat, 0, Vs_StartCombatC())

/* save variable name = `int(Scrap.GetSaveVar(name)) + 1` */
static byte Vs_Count(const char *name) {
	nativeVal_t n, one, r, s;
	Native_SetNone(&n);
	Native_SetNone(&r);
	Native_SetNone(&s);
	Native_SetInt(&one, 1);
	byte ok = Sd_SaveInt(name, &n) && Native_Arith('+', &n, &one, &r);
	if (ok) {
		Sd_Repr(&r, &s);
		ok = Mode_SetSaveVar(name, &s);
	}
	Native_Free(&n);
	Native_Free(&r);
	Native_Free(&s);
	return ok;
}

/* RacerScorer.UpdateMissionScorer(0, int(NumEnemyDeaths), int(NumPlayerDeaths)) */
static byte Vs_Update(void) {
	nativeVal_t a, b;
	Native_SetNone(&a);
	Native_SetNone(&b);
	byte ok = Sd_SaveInt("NumEnemyDeaths", &a) && Sd_SaveInt("NumPlayerDeaths", &b) && Sd_Update(&a, &b);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* int(Scrap.GetSaveVar(name)) == combatDeaths: 1, 0, -1 with the error */
static int Vs_Reached(const char *name) {
	nativeVal_t n;
	Native_SetNone(&n);
	int r = Sd_SaveInt(name, &n) && Mode_Read(VS, "combatDeaths", &vs.combatDeaths) ? Sd_Eq(&n, &vs.combatDeaths) : -1;
	Native_Free(&n);
	return r;
}

/* the scorer off and the counts of the rounds gone (the end of a versus) */
static byte Vs_Over(void) {
	if (!Sd_ScorerOff()) {
		return 0;
	}
	SaveVars_Delete("NumEnemyDeaths");
	SaveVars_Delete("NumPlayerDeaths");
	return 1;
}

/* OnEnemyDeath(LooserName, WinnerName): the enemy died; the player wins at combatDeaths, else the enemy comes back
 * after 5 s */
static byte Vs_OnEnemyDeathC(const nativeVal_t *loser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cad = Mode_V(&t), *params = Mode_V(&t);
	if (!Mode_Read(VS, "enemyName", &vs.enemyName)) {
		return Mode_Done(&t, 0);
	}
	if (!Sd_Eq(loser, &vs.enemyName)) {
		return Mode_Done(&t, 1);
	}
	int end = Vs_Count("NumEnemyDeaths") ? Vs_Reached("NumEnemyDeaths") : -1;
	if (end == 0) {
		return Mode_Done(&t, Mode_Import("RacerScorer") && Vs_Update() && Mode_Later(5.0, VS, "RespawnEnemy", NULL, 0, NULL) &&
			Sd_Del(Mode_S(&t, "Enemy")));
	}
	if (end < 0 || !Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking") ||
		!Vs_Over()) {
		return Mode_Done(&t, 0);
	}
	Sd_SetSave("PlayerWinsCombat", "1");
	if (!Dm_Finish()) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("CombatFinishPlayerWinsTextCad", cad);
	if (!Sd_UsrSafe()) {
		return Mode_Done(&t, 0);
	}
	Sd_Params(1, "Versus", "", params);
	byte ok;
	if (Sd_Text(cad)) {
		/* Speech.ForceRemoteCom(cad, "CrazyGambler", 1, MissionsFuncs.OnSuperDealFinish, (1, "Versus", "")) after 2 s */
		PyObject *fn = Script_Attr("Speech", "ForceRemoteCom");
		PyObject *func = fn ? Script_Attr("MissionsFuncs", "OnSuperDealFinish") : NULL;
		ok = func != NULL;
		if (ok) {
			nativeVal_t args[5];
			Native_Copy(&args[0], cad);
			Native_SetStr(&args[1], CG);
			Native_SetInt(&args[2], 1);
			Native_Arg(func, &args[3]);
			args[4] = *params;
			Native_SetNone(params);
			ok = Sd_LaterObj(2.0, fn, args, 5);
			for (int i = 0; i < 5; i++) {
				Native_Free(&args[i]);
			}
		}
		Py_XDECREF(func);
		Py_XDECREF(fn);
	} else {
		PyObject *fn = Script_Attr("MissionsFuncs", "OnSuperDealFinish");
		ok = fn && Sd_LaterObj(5.0, fn, params->items, params->n);
		Py_XDECREF(fn);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Vs, OnEnemyDeath, 2, Vs_OnEnemyDeathC(&a[0]))

/* RespawnEnemy(): the enemy again, as the target, and the next round */
static byte Vs_RespawnEnemyC(void) {
	return OWN(VS, "CreateEnemy", NULL, "i", 1L) && OWN(VS, "StartCombat", NULL, "");
}

MODE_FN(Vs, RespawnEnemy, 0, Vs_RespawnEnemyC())

/* OnPlayerDeath(LooserName, WinnerName): the enemy relaxes; the enemy wins at combatDeaths, else the player comes
 * back after 5 s where he died */
static byte Vs_OnPlayerDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *enemy = Mode_V(&t), *name = Mode_V(&t), *cad = Mode_V(&t), *e = Mode_V(&t), *args = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	if (!Mode_Read(VS, "enemyName", &vs.enemyName) || !Mode_EntityH(&vs.enemyName, enemy)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(enemy) && (!Sd_SetS(enemy, "EntityList", "") || !Mode_VGet(enemy, "Name", name) ||
		!CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 0L))) {
		return Mode_Done(&t, 0);
	}
	int end = Vs_Count("NumPlayerDeaths") ? Vs_Reached("NumPlayerDeaths") : -1;
	if (end < 0) {
		return Mode_Done(&t, 0);
	}
	if (end == 0) {
		/* RespawnPlayer(loser, e.Pos, e.Ang) after 5 s */
		Native_Copy(&args[0], loser);
		byte ok = Mode_Import("RacerScorer") && Mode_EntityH(loser, e) && Mode_VGet(e, "Pos", &args[1]) &&
			Mode_VGet(e, "Ang", &args[2]) && Mode_Later(5.0, VS, "RespawnPlayer", args, 3, NULL) && Vs_AuxText() &&
			Vs_Update();
		return Mode_Done(&t, ok);
	}
	if (!Mode_Import("Menu") || !Vs_Over() || !Sd_CamOnWinner(winner)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("CombatFinishPlayerLoosesTextCad", cad);
	if (Sd_Text(cad)) {
		return Mode_Done(&t, Mode_Later(2.0, VS, "EnemyWins", cad, 1, NULL));
	}
	PyObject *fn = Script_Attr("MissionsFuncs", "OnSuperDealFinish");
	Sd_Params(0, "Versus", "Versus.RestartCombat", args);
	byte ok = fn && Sd_LaterObj(5.0, fn, args->items, args->n);
	Py_XDECREF(fn);
	return Mode_Done(&t, ok);
}

MODE_FN(Vs, OnPlayerDeath, 2, Vs_OnPlayerDeathC(&a[0], &a[1]))

/* EnemyWins(cad): the enemy's message, then the score table */
static byte Vs_EnemyWinsC(const nativeVal_t *cad) {
	nativeVal_t head, params;
	PyObject *fn;
	Mode_SaveVar("CombatEnemyTypeHead", &head);
	if (!(fn = Script_Attr("MissionsFuncs", "OnSuperDealFinish"))) {
		Native_Free(&head);
		return 0;
	}
	Sd_Params(0, "Versus", "Versus.RestartCombat", &params);
	byte ok = CALL("Speech", "AddRemoteCom", NULL, "vvOv", cad, &head, fn, &params);
	Native_Free(&params);
	Native_Free(&head);
	Py_DECREF(fn);
	return ok;
}

MODE_FN(Vs, EnemyWins, 1, Vs_EnemyWinsC(&a[0]))

/* RespawnPlayer(Usrname, pos, ang): the player's ship again where it died; the enemy attacks it */
static byte Vs_RespawnPlayerC(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *ang) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *enemy = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !Mode_EntityH(name, usr) || !Mode_VGet(usr, "Name", un) ||
		!CALL("Vehicles", "ReSpawnShip", NULL, "v", un) || !Mode_VSet(usr, "Pos", pos) || !Mode_VSet(usr, "Ang", ang) ||
		!Mode_Read(VS, "enemyName", &vs.enemyName) || !Mode_EntityH(&vs.enemyName, enemy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(enemy)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(un);
	return Mode_Done(&t, Sd_SetS(enemy, "TgType", "red") && Sd_SetS(enemy, "EntityList", "Enemies") &&
		Mode_Read(VS, "enemyName", &vs.enemyName) && Mode_VGet(usr, "Name", un) &&
		CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, &vs.enemyName, un));
}

MODE_FN(Vs, RespawnPlayer, 3, Vs_RespawnPlayerC(&a[0], &a[1], &a[2]))

/* RestartCombat(id, control): the deaths at 0 and the level again */
static byte Vs_RestartCombatC(void) {
	Sd_SetSave("NumEnemyDeaths", "0");
	Sd_SetSave("NumPlayerDeaths", "0");
	return CALL("SaveGame", "Restart", NULL, "");
}

MODE_FN(Vs, RestartCombat, 2, Vs_RestartCombatC())

PyMethodDef g_VersusMethods[] = {
	{ "GetMenuDescription", Vs_GetMenuDescription, 1, NULL },
	{ "InitStatus", Vs_InitStatus, 1, NULL },
	{ "Init", Vs_Init, 1, NULL },
	{ "CreateEnemy", (PyCFunction)(void (*)(void))Vs_CreateEnemy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartCombat", Vs_CrazyGamblerStartCombat, 1, NULL },
	{ "StartCombat", Vs_StartCombat, 1, NULL },
	{ "OnEnemyDeath", Vs_OnEnemyDeath, 1, NULL },
	{ "RespawnEnemy", Vs_RespawnEnemy, 1, NULL },
	{ "OnPlayerDeath", Vs_OnPlayerDeath, 1, NULL },
	{ "EnemyWins", Vs_EnemyWins, 1, NULL },
	{ "RespawnPlayer", Vs_RespawnPlayer, 1, NULL },
	{ "RestartCombat", Vs_RestartCombat, 1, NULL },
	{ NULL, NULL, 0, NULL }
};



/* ---- TeamDeathMatch: the player's team against the enemies in the QF, CombatDeaths frags of a team win ---- */

static struct {
	nativeVal_t enemiesFrags, friendsFrags, keepPlaying;
	nativeVal_t enemies, friends;	/* the ship names of each team (lists) */
} tdm;

MODE_FN(Tdm, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_TeamDeathMatch_Description", "CombatDeaths", out))
MODE_FN(Tdm, InitStatus, 1, Sd_StatusC(TDM, NULL))

/* Init(LevelPath): the camera of the end, the death of the player, both teams, the start when the player boards */
static byte Tdm_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Fourth", "SuperDeal_Eleventh", "Mission_BankersCombat", NULL };
	return Mode_Import("Parking") && Mode_Import("Menu") && Sd_CameraFor(libs, "QF") && Mode_Import("Chars") &&
		Sd_GlobalTo("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath") && OWN(TDM, "CreateFriends", NULL, "") &&
		OWN(TDM, "CreateEnemies", NULL, "") && Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", TDM, "CrazyGamblerStartCombat");
}

MODE_FN(Tdm, Init, 1, Tdm_InitC())
MODE_FN(Tdm, SetInvulnerableStatus, 1, Sd_SetInvulnerableStatusC(&a[0], 1))
/* SetFriendProfile(friend): the AI of a ship of the player's team */
MODE_FN(Tdm, SetFriendProfile, 1, Sd_Profile(&a[0], "CombatFriendType", "CombatFriendProfile"))

/* the list of a team (enemies or friends) and its slot */
static nativeVal_t *Tdm_List(byte enemy) {
	return enemy ? &tdm.enemies : &tdm.friends;
}

/* CreateFriends() and CreateEnemies(): <prefix>1 to <prefix>N of a team at spawn points, invulnerable until the
 * start, their frags at 0 */
static byte Tdm_CreateTeam(byte enemy) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t);
	long n = 0;
	if (!Mode_Import("OutMap") || !Sd_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", num) ||
		!Sd_PutI(TDM, enemy ? "enemiesFrags" : "friendsFrags", enemy ? &tdm.enemiesFrags : &tdm.friendsFrags, 0) ||
		!Sd_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Sd_SpawnCarAng(name, e) && OWN(TDM, enemy ? "SetEnemyProfile" : "SetFriendProfile", NULL, "v", e) &&
			Sd_AppendName(TDM, enemy ? "enemies" : "friends", e) && Sd_SetI(e, "Invulnerable", 1);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Tdm, CreateFriends, 0, Tdm_CreateTeam(0))
MODE_FN(Tdm, CreateEnemies, 0, Tdm_CreateTeam(1))

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first combat */
static byte Tdm_CrazyGamblerStartCombatC(void) {
	if (!Mode_Import("Parking") || !Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) ||
		!Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", TDM, "OnVehicle") ||
		!Mode_Read(TDM, "enemiesFrags", &tdm.enemiesFrags)) {
		return 0;
	}
	byte first = Sd_EqI(&tdm.enemiesFrags, 0);
	if (first) {
		if (!Mode_Read(TDM, "friendsFrags", &tdm.friendsFrags)) {
			return 0;
		}
		first = Sd_EqI(&tdm.friendsFrags, 0);
	}
	return first ? Sd_GamblerSays("CombatStartTextCad", TDM, "StartCombat") : OWN(TDM, "StartCombat", NULL, "");
}

MODE_FN(Tdm, CrazyGamblerStartCombat, 0, Tdm_CrazyGamblerStartCombatC())

/* `for name in names: e = Scrap.GetEntity(name); e.EntityList = list; e.Invulnerable = 0` */
static byte Tdm_Release(const nativeVal_t *names, const char *list) {
	int n = Native_ForCount(names);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name;
		cEntity *e;
		ok = Native_SeqItem(names, i, &name) && Mode_Ent(&name, &e) && Mode_SetS(e, Native_Text(&name), "EntityList", list) &&
			Mode_SetI(e, Native_Text(&name), "Invulnerable", 0);
		Native_Free(&name);
	}
	return ok;
}

/* StartCombat(): the deaths of both lists counted, the scorer, the teams in their lists */
static byte Tdm_StartCombatC(void) {
	nativeVal_t deaths, zero;
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") ||
		!Mode_ListSet("Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath") ||
		!Mode_ListSet("Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath")) {
		return 0;
	}
	Native_SetInt(&zero, 0);
	Mode_SaveVar("CombatDeaths", &deaths);
	byte ok = Sd_ScorerOn(&zero, &zero, &deaths) && Mode_Read(TDM, "enemies", &tdm.enemies) &&
		Tdm_Release(&tdm.enemies, "Enemies") && Mode_Read(TDM, "friends", &tdm.friends) && Tdm_Release(&tdm.friends, "Friends");
	Native_Free(&deaths);
	return ok;
}

MODE_FN(Tdm, StartCombat, 0, Tdm_StartCombatC())

/* the end with the hook of a mission: Speech.<how>(cad, ...) then mod.KeepPlaying(args), or KeepPlaying(args) at
 * once without a message */
static byte Sd_EndHook(const char *mod, const char *how, const nativeVal_t *cad, const nativeVal_t *params) {
	nativeVal_t who;
	PyObject *hook = Mode_GlobalPy(mod, "KeepPlaying");
	if (!hook) {
		return 0;
	}
	Native_SetStr(&who, CG);
	byte ok = Sd_End(how, cad, &who, hook, params);
	Py_DECREF(hook);
	return ok;
}

/* the end of the team combat after the frag that won it: OnEnemyDeath (the player's team wins) and OnFriendDeath */
static byte Tdm_End(byte win) {
	nativeVal_t cad, none;
	PyObject *func;
	if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking")) {
		return 0;
	}
	Sd_SetSave("PlayerWinsCombat", win ? "1" : "0");
	Mode_SaveVar(win ? "CombatFinishPlayerWinsTextCad" : "CombatFinishPlayerLoosesTextCad", &cad);
	Native_SetEmpty(&none, NV_TUPLE);
	byte ok = (func = Script_Attr("MissionsFuncs", "OnSuperDealFinish")) != NULL && Sd_ScorerOff() &&
		CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "s", "Enemies") &&
		CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "s", "Friends") &&
		Mode_ListSet("Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath2") &&
		Mode_ListSet("Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath2") &&
		Sd_GlobalTo("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath2") && Sd_ModNone("Parking", "SwitchCharMissionCallback") &&
		Mode_Read(TDM, "KeepPlaying", &tdm.keepPlaying);
	if (ok && !Native_True(&tdm.keepPlaying)) {
		ok = Sd_UsrSafe() && Sd_FinishFn("ForceRemoteCom", &cad, func, win, "TeamDeathMatch", win ? "" : "TeamDeathMatch.RestartCombat");
	} else if (ok) {
		ok = Sd_EndHook(TDM, "ForceRemoteCom", &cad, &none);
	}
	Py_XDECREF(func);
	Native_Free(&cad);
	return ok;
}

/* the dead ship of a team comes back after 5 s while the team is short (an enemy is deleted at once) */
static byte Tdm_Refill(byte enemy, const nativeVal_t *loser) {
	nativeVal_t num, len;
	nativeVal_t *list = Tdm_List(enemy);
	Native_SetNone(&num);
	if (!Mode_Read(TDM, enemy ? "enemies" : "friends", list)) {
		return 0;
	}
	int n = Native_Size(list);
	if (n < 0 || !Sd_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", &num)) {
		Native_Free(&num);
		return 0;
	}
	Native_SetInt(&len, n);
	byte ok = Native_Compare(&len, &num) >= 0 ||
		(Mode_Later(5.0, TDM, enemy ? "RespawnEnemy" : "RespawnFriend", loser, 1, NULL) && (!enemy || Sd_Del(loser)));
	Native_Free(&num);
	return ok;
}

/* OnEnemyDeath and OnFriendDeath(LooserName, WinnerName): a ship of a team died; a frag of the other team ends the
 * combat at CombatDeaths, else the score; a new ship comes after 5 s while the team is short */
static byte Tdm_Death(byte enemy, const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *deaths = Mode_V(&t), *e = Mode_V(&t), *list = Mode_V(&t);
	const char *frags = enemy ? "friendsFrags" : "enemiesFrags";
	nativeVal_t *slot = enemy ? &tdm.friendsFrags : &tdm.enemiesFrags;
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (enemy) {
		int u = Sd_UsrIs(usr, loser);
		if (u) {
			return Mode_Done(&t, u > 0);
		}
	}
	if (!Sd_RemoveIn(TDM, enemy ? "enemies" : "friends", Tdm_List(enemy), loser) || !Sd_SaveInt("CombatDeaths", deaths)) {
		return Mode_Done(&t, 0);
	}
	int end = 0;
	if (Native_True(winner)) {
		int u = Sd_UsrIs(usr, winner);
		if (u == 0) {
			u = Mode_EntityH(winner, e) && Mode_VGet(e, "EntityList", list) ? Native_IsStr(list, enemy ? "Friends" : "Enemies") : -1;
		}
		if (u > 0) {
			end = Sd_Add(TDM, frags, slot, 1) ? Sd_Eq(slot, deaths) : -1;
		} else if (u < 0) {
			end = -1;
		}
	}
	if (end < 0) {
		return Mode_Done(&t, 0);
	}
	if (end) {
		if (!Tdm_End(enemy)) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_Read(TDM, "friendsFrags", &tdm.friendsFrags) || !Mode_Read(TDM, "enemiesFrags", &tdm.enemiesFrags) ||
		!Sd_Update(&tdm.friendsFrags, &tdm.enemiesFrags)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Tdm_Refill(enemy, loser));
}

MODE_FN(Tdm, OnEnemyDeath, 2, Tdm_Death(1, &a[0], &a[1]))
MODE_FN(Tdm, OnFriendDeath, 2, Tdm_Death(0, &a[0], &a[1]))

/* OnEnemyDeath2 and OnFriendDeath2(LooserName, WinnerName): after the end, the dead ship only comes back */
static byte Tdm_Death2(byte enemy, const nativeVal_t *loser) {
	return Sd_RemoveIn(TDM, enemy ? "enemies" : "friends", Tdm_List(enemy), loser) && Tdm_Refill(enemy, loser);
}

MODE_FN(Tdm, OnEnemyDeath2, 2, Tdm_Death2(1, &a[0]))
MODE_FN(Tdm, OnFriendDeath2, 2, Tdm_Death2(0, &a[0]))

/* RespawnEnemy(enemyName) and RespawnFriend(friendName): the ship again at a spawn point in its team, unless the list
 * is full */
static byte Tdm_Respawn(byte enemy, const nativeVal_t *name) {
	const char *entityList = enemy ? "Enemies" : "Friends";
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *max = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_ListGet(entityList, "Num", num) ||
		!Sd_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", max)) {
		return Mode_Done(&t, 0);
	}
	if (Sd_Eq(num, max)) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Sd_SpawnCarAng(name, e) && Sd_SetS(e, "EntityList", entityList) &&
		OWN(TDM, enemy ? "SetEnemyProfile" : "SetFriendProfile", NULL, "v", e) && Sd_AppendName(TDM, enemy ? "enemies" : "friends", e));
}

MODE_FN(Tdm, RespawnEnemy, 1, Tdm_Respawn(1, &a[0]))
MODE_FN(Tdm, RespawnFriend, 1, Tdm_Respawn(0, &a[0]))

/* OnPlayerDeath(LooserName, WinnerName): a frag of the enemies ends the combat at CombatDeaths; the player comes back
 * after 5 s */
static byte Tdm_OnPlayerDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *deaths = Mode_V(&t), *cad = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_Read(TDM, "enemies", &tdm.enemies)) {
		return Mode_Done(&t, 0);
	}
	int in = Native_Contains(&tdm.enemies, winner);
	if (in < 0 || (in > 0 && !Sd_Add(TDM, "enemiesFrags", &tdm.enemiesFrags, 1))) {
		return Mode_Done(&t, 0);
	}
	int end = in;
	if (end > 0) {
		end = Mode_Read(TDM, "enemiesFrags", &tdm.enemiesFrags) && Sd_SaveInt("CombatDeaths", deaths) ?
			Native_Compare(&tdm.enemiesFrags, deaths) >= 0 : -1;
	}
	if (end < 0) {
		return Mode_Done(&t, 0);
	}
	if (end) {
		if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking")) {
			return Mode_Done(&t, 0);
		}
		Sd_SetSave("PlayerWinsCombat", "0");
		Mode_SaveVar("CombatFinishPlayerLoosesTextCad", cad);
		if (!Sd_ScorerOff() || !Sd_GlobalTo("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath2") ||
			!Sd_ModNone("Parking", "SwitchCharMissionCallback") || !Mode_Read(TDM, "KeepPlaying", &tdm.keepPlaying)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(none, NV_TUPLE);
		if (!(Native_True(&tdm.keepPlaying) ? Sd_EndHook(TDM, "AddRemoteCom", cad, none) : Sd_CamOnWinner(winner) &&
			Sd_Finish("AddRemoteCom", cad, 0, "TeamDeathMatch", "TeamDeathMatch.RestartCombat"))) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_Read(TDM, "friendsFrags", &tdm.friendsFrags) || !Mode_Read(TDM, "enemiesFrags", &tdm.enemiesFrags) ||
		!Sd_Update(&tdm.friendsFrags, &tdm.enemiesFrags)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_Later(5.0, TDM, "RespawnPlayer", loser, 1, NULL));
}

MODE_FN(Tdm, OnPlayerDeath, 2, Tdm_OnPlayerDeathC(&a[0], &a[1]))
/* OnPlayerDeath2(LooserName, WinnerName): after the end the player only comes back */
MODE_FN(Tdm, OnPlayerDeath2, 2, Mode_Later(5.0, TDM, "RespawnPlayer", &a[0], 1, NULL))

/* RestartCombat(id, control): the frags at 0 and the level again */
static byte Tdm_RestartCombatC(void) {
	return Sd_PutI(TDM, "enemiesFrags", &tdm.enemiesFrags, 0) && Sd_PutI(TDM, "friendsFrags", &tdm.friendsFrags, 0) &&
		CALL("SaveGame", "Restart", NULL, "");
}

MODE_FN(Tdm, RestartCombat, 2, Tdm_RestartCombatC())

PyMethodDef g_TeamDeathMatchMethods[] = {
	{ "GetMenuDescription", Tdm_GetMenuDescription, 1, NULL },
	{ "InitStatus", Tdm_InitStatus, 1, NULL },
	{ "Init", Tdm_Init, 1, NULL },
	{ "SetInvulnerableStatus", Tdm_SetInvulnerableStatus, 1, NULL },
	{ "SetEnemyProfile", Dm_SetProfile, 1, NULL },
	{ "SetFriendProfile", Tdm_SetFriendProfile, 1, NULL },
	{ "CreateFriends", Tdm_CreateFriends, 1, NULL },
	{ "CreateEnemies", Tdm_CreateEnemies, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartCombat", Tdm_CrazyGamblerStartCombat, 1, NULL },
	{ "OnVehicle", Sd_SetVulnerable, 1, NULL },
	{ "StartCombat", Tdm_StartCombat, 1, NULL },
	{ "OnEnemyDeath", Tdm_OnEnemyDeath, 1, NULL },
	{ "OnEnemyDeath2", Tdm_OnEnemyDeath2, 1, NULL },
	{ "RespawnEnemy", Tdm_RespawnEnemy, 1, NULL },
	{ "OnFriendDeath", Tdm_OnFriendDeath, 1, NULL },
	{ "OnFriendDeath2", Tdm_OnFriendDeath2, 1, NULL },
	{ "RespawnFriend", Tdm_RespawnFriend, 1, NULL },
	{ "RespawnPlayer", Sd_RespawnPlayer, 1, NULL },
	{ "OnPlayerDeath", Tdm_OnPlayerDeath, 1, NULL },
	{ "OnPlayerDeath2", Tdm_OnPlayerDeath2, 1, NULL },
	{ "RestartCombat", Tdm_RestartCombat, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- CTF: the flags in the FZ, the team of the player against the enemies' leader, flags flags win ---- */

static struct {
	nativeVal_t enemies, friends, flagsActive, flagsSpecial;	/* lists of names */
	nativeVal_t flags, maxFlags, onDeathSpawnTime, flagsEnemies, flagsFriends;
} ctf;

MODE_FN(Ctf, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_CTF_Description", "CTFFlags", out))
MODE_FN(Ctf, InitStatus, 1, Sd_StatusC(CTF, NULL))

/* raise "text" (a string exception of the script; OBJLIB 7: the Python exception until errors are a C state) */
static byte Sd_RaiseText(const char *text) {
	PyObject *s = PyString_FromString((char *)text);
	if (s) {
		PyErr_SetObject(s, Py_None);
		Py_DECREF(s);
	}
	return 0;
}

/* onDeathSpawnTime = float(Scrap.GetSaveVar(var)); the error of the script when it is under 1 s */
static byte Sd_SpawnTime(const char *mod, nativeVal_t *slot, const char *var) {
	nativeVal_t s, d, one;
	Mode_SaveVar(var, &s);
	Native_SetNone(&d);
	byte ok = Native_ToFloat(&s, &d) && Mode_Put(mod, "onDeathSpawnTime", slot, &d) && Mode_Read(mod, "onDeathSpawnTime", slot);
	Native_Free(&s);
	Native_Free(&d);
	Native_SetFloat(&one, 1.0);
	return ok && (Native_Compare(slot, &one) >= 0 || Sd_RaiseText("Demasiado poco tiempo de respawn"));
}

/* Init(LevelPath): the camera of the end, the flags to take, the respawn time, both teams, the start when the player
 * boards */
static byte Ctf_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Fifth", "SuperDeal_Ninth", NULL };
	nativeVal_t n, two, one, r, max;
	if (!Mode_Import("Parking") || !Mode_Import("Chars") || !Sd_CameraFor(libs, "FZ") ||
		!Sd_GlobalTo("Chars", "OnMainCharDeath", CTF, "OnPlayerDeath")) {
		return 0;
	}
	Native_SetNone(&n);
	Native_SetNone(&r);
	Native_SetNone(&max);
	Native_SetInt(&two, 2);
	Native_SetInt(&one, 1);
	byte ok = Sd_SaveInt("CTFFlags", &n) && Mode_Put(CTF, "flags", &ctf.flags, &n) && Mode_Read(CTF, "flags", &ctf.flags) &&
		Native_Arith('*', &ctf.flags, &two, &r) && Native_Arith('-', &r, &one, &max) &&
		Mode_Put(CTF, "maxFlags", &ctf.maxFlags, &max) && Sd_SpawnTime(CTF, &ctf.onDeathSpawnTime, "CTFOnDeathSpawnTime") &&
		OWN(CTF, "CreateEnemies", NULL, "") && OWN(CTF, "CreateFriends", NULL, "") &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", CTF, "CrazyGamblerStartCTF");
	Native_Free(&n);
	Native_Free(&r);
	Native_Free(&max);
	if (ok) {
		nativeVal_t flag;
		Native_SetStr(&flag, "models/Misc/Flag/Flag.m3d");
		ok = Mode_Preload(&flag);
	}
	return ok;
}

MODE_FN(Ctf, Init, 1, Ctf_InitC())

/* SetProfile(ship, isEnemy) of CTF and Domination: the AI of a ship of either team from the save variables of the
 * mode */
static byte Sd_SetProfileC(const char *mode, const nativeVal_t *ship, const nativeVal_t *enemy) {
	char type[64], prof[64];
	byte e = Sd_EqI(enemy, 1);
	Str_Printf(type, sizeof(type), "%s%sType", mode, e ? "Enemy" : "Friend");
	Str_Printf(prof, sizeof(prof), "%s%sProfile", mode, e ? "Enemy" : "Friend");
	return Sd_Profile(ship, type, prof);
}

MODE_FN(Ctf, SetProfile, 2, Sd_SetProfileC("CTF", &a[0], &a[1]))

/* CreateEnemies() (Enemy1 to EnemyN) and CreateFriends() (Friend1 to Friend<N-1>, the player is the last): the
 * ships of a team at spawn points (CTF) */
static byte Ctf_CreateTeam(byte enemy) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t);
	long n = 0;
	if (!Mode_Import("OutMap") || !Sd_SaveInt(enemy ? "CTFNumEnemies" : "CTFNumFriends", num) || !Sd_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n - (enemy ? 0 : 1); i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Sd_SpawnCarAng(name, e) && OWN(CTF, "SetProfile", NULL, "vi", e, (long)enemy) &&
			Sd_AppendName(CTF, enemy ? "enemies" : "friends", e);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Ctf, CreateEnemies, 0, Ctf_CreateTeam(1))
MODE_FN(Ctf, CreateFriends, 0, Ctf_CreateTeam(0))

/* CrazyGamblerStartCTF(): the player boarded; the crazy gambler's message starts the CTF */
static byte Ctf_CrazyGamblerStartCTFC(void) {
	return Mode_Import("Parking") && Mode_Import("Speech") && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", CTF, "SetVulnerable") &&
		Sd_GamblerSays("CTFStartTextCad", CTF, "StartCTF");
}

MODE_FN(Ctf, CrazyGamblerStartCTF, 0, Ctf_CrazyGamblerStartCTFC())

/* RaceMaker.ShowSystemMessage(Scrap.GetLangStr(key)) */
static byte Sd_SystemMessage(const char *key) {
	nativeVal_t text;
	Mode_Lang(key, &text);
	byte ok = CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

/* StartCTF(): the deaths counted, the scorer, the first flag, the enemies red with the first as the leader, the
 * friends green */
static byte Ctf_StartCTFC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *limit = Mode_V(&t), *first = Mode_V(&t), *e = Mode_V(&t), *usr = Mode_V(&t), *name = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("Vehicles") || !Mode_Import("Menu") || !Mode_Import("RacerScorer") || !Mode_Import("RaceMaker") ||
		!Sd_GlobalTo("Vehicles", "OnDeathCallback", CTF, "OnVehicleDeathCallback") || !Mode_Read(CTF, "flags", &ctf.flags)) {
		return Mode_Done(&t, 0);
	}
	Sd_Repr(&ctf.flags, limit);
	if (!Sd_ScorerOn(zero, zero, limit) || !OWN(CTF, "CreateFlag", NULL, "") || !Mode_Read(CTF, "enemies", &ctf.enemies) ||
		!Sd_MarkShips(&ctf.enemies, "red", "CTFEnemies") || !Mode_Read(CTF, "enemies", &ctf.enemies)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&ctf.enemies);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (n > 0) {
		if (!Mode_Read(CTF, "enemies", &ctf.enemies) || !Sd_Item(&ctf.enemies, 0, first) || !Mode_EntityH(first, e)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(e)) {
			int car;
			if (!Sd_SetI(e, "LLogicIsLeader", 1) || !CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 1L) ||
				!Mode_UsrHandle(usr) || (car = Sd_UsrCar(usr)) < 0 ||
				(car && (!Mode_VGet(e, "Name", name) || !Mode_VSet(usr, "Target", name)))) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, Mode_Read(CTF, "friends", &ctf.friends) && Sd_MarkShips(&ctf.friends, "green", "CTFFriends") &&
		Sd_SystemMessage("SuperDeal_CTF_SystemMessage"));
}

MODE_FN(Ctf, StartCTF, 0, Ctf_StartCTFC())

/* list.remove(x) of a global list of the module (slot its copy), in place */
static byte Sd_Remove(const char *mod, const char *list, nativeVal_t *slot, const nativeVal_t *x) {
	return Mode_Remove(mod, list, x) && Mode_Read(mod, list, slot);
}

/* RacerScorer.UpdateMissionScorer(0, flagsFriends, flagsEnemies) */
static byte Ctf_Update(void) {
	return Mode_Read(CTF, "flagsFriends", &ctf.flagsFriends) && Mode_Read(CTF, "flagsEnemies", &ctf.flagsEnemies) &&
		Sd_Update(&ctf.flagsFriends, &ctf.flagsEnemies);
}

/* CTF.SpawnFlags(pos of the handle, flags<side>); flags<side> = 0; the scorer */
static byte Ctf_Drop(const nativeVal_t *h, const char *var, nativeVal_t *slot) {
	nativeVal_t pos;
	Native_SetNone(&pos);
	byte ok = Mode_VGet(h, "Pos", &pos) && Mode_Read(CTF, var, slot) && OWN(CTF, "SpawnFlags", NULL, "vv", &pos, slot) &&
		Sd_PutI(CTF, var, slot, 0) && Ctf_Update();
	Native_Free(&pos);
	return ok;
}

/* OnVehicleDeathCallback(LooserName, WinnerName): the player or the enemies' leader drop the flags they carry; the
 * dead ship of a team comes back after onDeathSpawnTime */
static byte Ctf_OnVehicleDeathCallbackC(const nativeVal_t *loser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *e = Mode_V(&t), *lead = Mode_V(&t);
	const nativeVal_t *one = Mode_I(&t, 1);
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int u = Sd_UsrIs(usr, loser);
	if (u) {
		return Mode_Done(&t, u > 0 && Ctf_Drop(usr, "flagsFriends", &ctf.flagsFriends));
	}
	if (!Mode_Read(CTF, "enemies", &ctf.enemies)) {
		return Mode_Done(&t, 0);
	}
	int en = Native_Contains(&ctf.enemies, loser);
	if (en > 0) {
		if (!Sd_Remove(CTF, "enemies", &ctf.enemies, loser) || !Mode_EntityH(loser, e)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(e)) {
			return Mode_Done(&t, 1);
		}
		if (!Mode_VGet(e, "LLogicIsLeader", lead)) {
			return Mode_Done(&t, 0);
		}
		byte ok = Sd_EqI(lead, 1) ? Ctf_Drop(e, "flagsEnemies", &ctf.flagsEnemies) && Mode_Read(CTF, "onDeathSpawnTime", &ctf.onDeathSpawnTime) &&
			Sd_LaterV(&ctf.onDeathSpawnTime, CTF, "RespawnEnemy", one, 1) :
			Mode_Read(CTF, "onDeathSpawnTime", &ctf.onDeathSpawnTime) && Sd_LaterV(&ctf.onDeathSpawnTime, CTF, "RespawnEnemy", NULL, 0);
		return Mode_Done(&t, ok && Sd_Del(loser));
	}
	if (en < 0 || !Mode_Read(CTF, "friends", &ctf.friends)) {
		return Mode_Done(&t, 0);
	}
	int fr = Native_Contains(&ctf.friends, loser);
	return Mode_Done(&t, fr <= 0 ? fr == 0 : Sd_Remove(CTF, "friends", &ctf.friends, loser) &&
		Mode_Read(CTF, "onDeathSpawnTime", &ctf.onDeathSpawnTime) && Sd_LaterV(&ctf.onDeathSpawnTime, CTF, "RespawnFriend", NULL, 0));
}

MODE_FN(Ctf, OnVehicleDeathCallback, 2, Ctf_OnVehicleDeathCallbackC(&a[0]))

/* OnPlayerDeath(LooserName, WinnerName): the player comes back after onDeathSpawnTime (CTF and Domination's start) */
static byte Sd_RespawnLater(const char *mod, nativeVal_t *slot, const nativeVal_t *loser) {
	return Mode_Read(mod, "onDeathSpawnTime", slot) && Sd_LaterV(slot, mod, "RespawnPlayer", loser, 1);
}

MODE_FN(Ctf, OnPlayerDeath, 2, Sd_RespawnLater(CTF, &ctf.onDeathSpawnTime, &a[0]))

/* RemoveFlag(EntityName): the flag gone from the map and from the lists */
static byte Ctf_RemoveFlagC(const nativeVal_t *name) {
	nativeVal_t flag;
	if (!Mode_EntityH(name, &flag)) {
		return 0;
	}
	byte ok = !Native_True(&flag) || (Sd_SetS(&flag, "OnTake", "") && Mode_VDel(&flag) &&
		Sd_RemoveIn(CTF, "flagsActive", &ctf.flagsActive, name) && Sd_RemoveIn(CTF, "flagsSpecial", &ctf.flagsSpecial, name));
	Native_Free(&flag);
	return ok;
}

MODE_FN(Ctf, RemoveFlag, 1, Ctf_RemoveFlagC(&a[0]))

/* e.LLogicIsLeader == 1 of a handle value that is true: 1, 0, -1 with the error */
static int Sd_IsLeader(const nativeVal_t *e) {
	nativeVal_t v;
	if (!Native_True(e)) {
		return 0;
	}
	Native_SetNone(&v);
	int r = Mode_VGet(e, "LLogicIsLeader", &v) ? Sd_EqI(&v, 1) : -1;
	Native_Free(&v);
	return r;
}

/* EndOfCTF(): the leader not the target any more, the flags gone, the ships relax, then the last message */
static byte Ctf_EndOfCTFC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking") ||
		!Mode_Read(CTF, "enemies", &ctf.enemies)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(&ctf.enemies);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int lead;
		ok = Native_SeqItem(&ctf.enemies, i, name) && Mode_EntityH(name, e) && (lead = Sd_IsLeader(e)) >= 0;
		if (ok && lead) {
			ok = CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 0L);
			break;
		}
		Native_Free(name);
		Native_Free(e);
	}
	/* for name in flagsActive: CTF.RemoveFlag(name), on the list as RemoveFlag changes it (the global read again at
	 * each step: the loop of the script runs on the object of the module) */
	for (int i = 0; ok; i++) {
		Native_Free(name);
		ok = Mode_Read(CTF, "flagsActive", &ctf.flagsActive);
		int count = ok ? Native_ForCount(&ctf.flagsActive) : -1;
		if (count < 0) {
			ok = 0;
		} else if (i >= count) {
			break;
		}
		ok = ok && Native_SeqItem(&ctf.flagsActive, i, name) && OWN(CTF, "RemoveFlag", NULL, "v", name);
	}
	if (!ok || !Mode_GVarSetS("LeaderFlag", "") ||
		!CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "s", "CTFEnemies") ||
		!CALL("MissionsFuncs", "SetInvulnerableShipsInList", NULL, "s", "CTFFriends") ||
		!CALL("MissionsFuncs", "RelaxShipsInList", NULL, "s", "CTFEnemies") ||
		!CALL("MissionsFuncs", "RelaxShipsInList", NULL, "s", "CTFFriends") || !Sd_ScorerOff() ||
		!Sd_ModNone("Vehicles", "OnDeathCallback") || !Sd_ModNone("Parking", "SwitchCharMissionCallback") ||
		!Sd_ModNone("Parking", "SwitchVehicleMissionCallback") || !Mode_Read(CTF, "flagsFriends", &ctf.flagsFriends) ||
		!Mode_Read(CTF, "flags", &ctf.flags)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, OWN(CTF, Native_Compare(&ctf.flagsFriends, &ctf.flags) >= 0 ? "PreOnVictory" : "PreRestartMenu", NULL, ""));
}

MODE_FN(Ctf, EndOfCTF, 0, Ctf_EndOfCTFC())


/* RespawnEnemy(isLeader = 0): an enemy at a spawn point, the leader with the target */
static byte Ctf_RespawnEnemyC(const nativeVal_t *leader) {
	nativeVal_t e, name;
	Native_SetNone(&e);
	Native_SetStr(&name, "Enemy");
	byte ok = Mode_Import("OutMap") && Sd_SpawnCarAng(&name, &e) && Sd_SetS(&e, "TgType", "red") &&
		Sd_SetS(&e, "EntityList", "CTFEnemies") && OWN(CTF, "SetProfile", NULL, "vi", &e, 1L) &&
		(!Native_True(leader) || (Sd_SetI(&e, "LLogicIsLeader", 1) && CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L))) &&
		Sd_AppendName(CTF, "enemies", &e);
	Native_Free(&e);
	return ok;
}

static PyObject *Ctf_RespawnEnemy(PyObject *self, PyObject *args, PyObject *kw) {
	nativeVal_t leader;
	(void)self;
	if (!Sd_KwArg(args, kw, "isLeader", 0, &leader)) {
		return NULL;
	}
	byte ok = Ctf_RespawnEnemyC(&leader);
	Native_Free(&leader);
	return Mode_Result(ok);
}

/* RespawnFriend(): a ship of the player's team at a spawn point */
static byte Ctf_RespawnFriendC(void) {
	nativeVal_t e, name;
	Native_SetNone(&e);
	Native_SetStr(&name, "Friend");
	byte ok = Mode_Import("OutMap") && Sd_SpawnCarAng(&name, &e) && Sd_SetS(&e, "TgType", "green") &&
		Sd_SetS(&e, "EntityList", "CTFFriends") && OWN(CTF, "SetProfile", NULL, "vi", &e, 0L) && Sd_AppendName(CTF, "friends", &e);
	Native_Free(&e);
	return ok;
}

MODE_FN(Ctf, RespawnFriend, 0, Ctf_RespawnFriendC())

/* Scrap.Set(var, flagsActive[0]) (the global read again) */
static byte Ctf_SetFirst(const char *var) {
	nativeVal_t v;
	Native_SetNone(&v);
	byte ok = Mode_Read(CTF, "flagsActive", &ctf.flagsActive) && Sd_Item(&ctf.flagsActive, 0, &v) && Mode_GVarSet(var, &v);
	Native_Free(&v);
	return ok;
}

/* Scrap.Set("CurrentWaypointName", flagsActive[0]) and Scrap.Set("LeaderFlag", flagsActive[0]) while flags are
 * active, else Scrap.Set("LeaderFlag", "") */
static byte Ctf_Waypoint(void) {
	if (!Mode_Read(CTF, "flagsActive", &ctf.flagsActive)) {
		return 0;
	}
	int n = Native_Size(&ctf.flagsActive);
	if (n < 0) {
		return 0;
	}
	return n > 0 ? Ctf_SetFirst("CurrentWaypointName") && Ctf_SetFirst("LeaderFlag") : Mode_GVarSetS("LeaderFlag", "");
}

/* len(flagsActive) == 0 and len(flagsSpecial) == 0: 1, 0, -1 with the error */
static int Ctf_NoFlags(void) {
	if (!Mode_Read(CTF, "flagsActive", &ctf.flagsActive)) {
		return -1;
	}
	int n = Native_Size(&ctf.flagsActive);
	if (n != 0) {
		return n < 0 ? -1 : 0;
	}
	if (!Mode_Read(CTF, "flagsSpecial", &ctf.flagsSpecial)) {
		return -1;
	}
	n = Native_Size(&ctf.flagsSpecial);
	return n < 0 ? -1 : n == 0;
}

/* CreateFlag(pos = None): a flag at a spawn point, or thrown from pos by a dead ship (back after 60 s), while fewer
 * than maxFlags are carried; its name, else "" */
static byte Ctf_CreateFlagC(const nativeVal_t *pos, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *carried = Mode_V(&t), *sp = Mode_V(&t), *at = Mode_V(&t), *flag = Mode_V(&t), *dir = Mode_V(&t);
	nativeVal_t *norm = Mode_V(&t), *vel = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_Read(CTF, "flagsFriends", &ctf.flagsFriends) ||
		!Mode_Read(CTF, "flagsEnemies", &ctf.flagsEnemies) || !Native_Arith('+', &ctf.flagsFriends, &ctf.flagsEnemies, carried) ||
		!Mode_Read(CTF, "maxFlags", &ctf.maxFlags)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(carried, &ctf.maxFlags) >= 0) {
		Native_SetStr(out, "");
		return Mode_Done(&t, 1);
	}
	if (Native_True(pos)) {
		Native_Copy(at, pos);
	} else if (!CALL("OutMap", "GetSpawnPos", sp, "") || !Sd_Item(sp, 0, at)) {
		return Mode_Done(&t, 0);
	}
	if (!Sd_Create(Mode_S(&t, "Flag"), at, "Item", flag) || !Mode_VModel(flag, Mode_S(&t, "models/Misc/Flag/Flag.m3d")) ||
		!Sd_SetI(flag, "Radius", 1800) || !Sd_SetF(flag, "rTime", 0.25) || !Sd_SetS(flag, "OnTake", "CTF.TakeFlag") ||
		!Sd_SetI(flag, "RadarShow", 2)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(pos)) {
		/* a random direction of the engine's C random numbers, as the script's Scrap.Rand calls */
		double d[3];
		for (int i = 0; i < 3; i++) {
			d[i] = Scrap_RandRange(0.0, 2.0) - 1.0;
		}
		Native_SetEmpty(dir, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(dir), d[i]);
		}
		if (!Mode_VecNorm(dir, norm) || !Mode_VecProd(norm, Mode_I(&t, 300), vel) || !Mode_VSet(flag, "Vel", vel) ||
			!Sd_SetI(flag, "BackTime", 60) || !Sd_SetS(flag, "OnReset", "CTF.ResetFlag")) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Sd_AppendName(CTF, "flagsActive", flag) || !Mode_VGet(flag, "Name", name) ||
		!Mode_GVarSet("CurrentWaypointName", name) || !Ctf_SetFirst("LeaderFlag") || !Mode_VGet(flag, "Name", out)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, 1);
}

static PyObject *Ctf_CreateFlag(PyObject *self, PyObject *args, PyObject *kw) {
	nativeVal_t pos, out;
	(void)self;
	if (!Sd_KwArg(args, kw, "pos", -1, &pos)) {
		return NULL;
	}
	Native_SetNone(&out);
	byte ok = Ctf_CreateFlagC(&pos, &out);
	Native_Free(&pos);
	return Mode_Return(ok, &out);
}

/* ResetFlag(flagName): a thrown flag nobody took goes; a new one when none is left */
static byte Ctf_ResetFlagC(const nativeVal_t *name) {
	if (!OWN(CTF, "RemoveFlag", NULL, "v", name) || !Ctf_Waypoint()) {
		return 0;
	}
	int none = Ctf_NoFlags();
	return none > 0 ? OWN(CTF, "CreateFlag", NULL, "") : none == 0;
}

MODE_FN(Ctf, ResetFlag, 1, Ctf_ResetFlagC(&a[0]))

/* TakeFlag(ItemName, CharName): the player or the enemies' leader took a flag; the CTF ends at flags, else a new flag
 * when none is left */
static byte Ctf_TakeFlagC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *e = Mode_V(&t), *win = Mode_V(&t);
	cEntity *flag;
	if (!Mode_Import("RacerScorer") || !Mode_Import("Sound") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Ent(item, &flag) || !Mode_VGet(usr, "Name", un)) {
		return Mode_Done(&t, 0);
	}
	byte taken = 0;
	if (Sd_Eq(un, who)) {
		if (!Sd_Add(CTF, "flagsFriends", &ctf.flagsFriends, 1)) {
			return Mode_Done(&t, 0);
		}
		taken = 1;
		if (!Mode_Sound("WaySound", "WayVolume")) {
			return Mode_Done(&t, 0);
		}
	} else {
		int in = Mode_Read(CTF, "enemies", &ctf.enemies) ? Native_Contains(&ctf.enemies, who) : -1;
		int lead = in > 0 ? (Mode_EntityH(who, e) ? Sd_IsLeader(e) : -1) : in;
		if (lead < 0) {
			return Mode_Done(&t, 0);
		}
		if (lead) {
			if (!Sd_Add(CTF, "flagsEnemies", &ctf.flagsEnemies, 1)) {
				return Mode_Done(&t, 0);
			}
			taken = 1;
			if (!Mode_Sound("EneWaySound", "EneWayVolume")) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!taken) {
		return Mode_Done(&t, 1);
	}
	if (!OWN(CTF, "RemoveFlag", NULL, "v", item)) {
		return Mode_Done(&t, 0);
	}
	int fresh = Ctf_NoFlags();
	if (fresh < 0 || !Mode_Read(CTF, "flagsFriends", &ctf.flagsFriends) || !Mode_Read(CTF, "flagsEnemies", &ctf.flagsEnemies)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(win, Native_Compare(&ctf.flagsFriends, &ctf.flagsEnemies) > 0);
	if (!Mode_GVarSet("PlayerCTFTeamIsWinning", win) || !Ctf_Waypoint() || !Ctf_Update() ||
		!Mode_Read(CTF, "flagsFriends", &ctf.flagsFriends) || !Mode_Read(CTF, "flags", &ctf.flags)) {
		return Mode_Done(&t, 0);
	}
	byte end = Sd_Eq(&ctf.flagsFriends, &ctf.flags);
	if (!end) {
		if (!Mode_Read(CTF, "flagsEnemies", &ctf.flagsEnemies) || !Mode_Read(CTF, "flags", &ctf.flags)) {
			return Mode_Done(&t, 0);
		}
		end = Sd_Eq(&ctf.flagsEnemies, &ctf.flags);
	}
	return Mode_Done(&t, end ? OWN(CTF, "EndOfCTF", NULL, "") : !fresh || OWN(CTF, "CreateFlag", NULL, ""));
}

MODE_FN(Ctf, TakeFlag, 2, Ctf_TakeFlagC(&a[0], &a[1]))

/* SpawnFlags(pos, numFlags): the flags a dead ship carried, thrown from pos; the nearest active flag is the player's
 * way point */
static byte Ctf_SpawnFlagsC(const nativeVal_t *pos, const nativeVal_t *num) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *minName = Mode_V(&t), *minDist = Mode_V(&t);
	long n = 0;
	byte ok = Sd_Range(num, &n);
	for (long i = 0; ok && i < n; i++) {
		nativeVal_t name;
		Native_SetNone(&name);
		ok = OWN(CTF, "CreateFlag", &name, "v", pos) && Mode_Append(CTF, "flagsSpecial", &name);
		Native_Free(&name);
	}
	if (!ok || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, Mode_GVarSetS("CurrentWaypointName", ""));
	}
	if (!Mode_Read(CTF, "flagsActive", &ctf.flagsActive)) {
		return Mode_Done(&t, 0);
	}
	int count = Native_Size(&ctf.flagsActive);
	if (count <= 0) {
		return Mode_Done(&t, count == 0);
	}
	Native_SetStr(minName, "");
	Native_SetInt(minDist, 2000000);
	if (!Mode_Read(CTF, "flagsActive", &ctf.flagsActive) || (count = Native_ForCount(&ctf.flagsActive)) < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; ok && i < count; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *ep = Mode_V(&t), *up = Mode_V(&t), *d = Mode_V(&t);
		nativeVal_t *dist = Mode_V(&t);
		ok = Native_SeqItem(&ctf.flagsActive, i, name) && Mode_EntityH(name, e);
		if (ok && Native_True(e)) {
			ok = Mode_VGet(e, "Pos", ep) && Mode_VGet(usr, "Pos", up) && Mode_VecSub(ep, up, d) && Mode_VecMod(d, dist);
			if (ok && Native_Compare(dist, minDist) < 0) {
				Native_Free(minDist);
				Native_Copy(minDist, dist);
				Native_Free(minName);
				Native_Copy(minName, name);
			}
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok && Mode_GVarSet("CurrentWaypointName", minName));
}

MODE_FN(Ctf, SpawnFlags, 2, Ctf_SpawnFlagsC(&a[0], &a[1]))

/* RespawnPlayer(LooserName): the player's ship at a spawn point, the enemies' leader as its target */
static int Ctf_TargetLeader(const nativeVal_t *e, const nativeVal_t *name, void *usr) {
	nativeVal_t v;
	(void)name;
	Native_SetNone(&v);
	int lead = Mode_VGet(e, "LLogicIsLeader", &v) ? Sd_EqI(&v, 1) : -1;
	Native_Free(&v);
	if (lead > 0) {
		lead = Mode_VGet(e, "Name", &v) && Mode_VSet(usr, "Target", &v) ? 1 : -1;
		Native_Free(&v);
	}
	return lead < 0 ? -1 : !lead;
}

/* usr = Scrap.GetEntity(name); Vehicles.ReSpawnShip(name); the ship at a spawn point (CTF and Domination) */
static byte Sd_RespawnUsr(const nativeVal_t *name, nativeVal_t *usr) {
	return Mode_Import("Parking") && Mode_Import("Vehicles") && Mode_Import("OutMap") && Mode_EntityH(name, usr) &&
		CALL("Vehicles", "ReSpawnShip", NULL, "v", name) && Sd_SpawnAt(usr);
}

static byte Ctf_RespawnPlayerC(const nativeVal_t *name) {
	nativeVal_t usr;
	Native_SetNone(&usr);
	byte ok = Sd_RespawnUsr(name, &usr) && Sd_EachInList("CTFEnemies", Ctf_TargetLeader, &usr);
	Native_Free(&usr);
	return ok;
}

MODE_FN(Ctf, RespawnPlayer, 1, Ctf_RespawnPlayerC(&a[0]))

/* PreRestartMenu() and PreOnVictory() of CTF and Domination: the player safe and the last message of the deal, then
 * the score table (whoVar NULL: the crazy gambler speaks) */
static byte Sd_PreEnd(byte win, const char *cadVar, const char *whoVar, const char *mode, const char *retry) {
	nativeVal_t cad, who, params;
	if (!Mode_Import("Parking") || !Sd_ShipSafe() || !(win ? Sd_UsrSafe() : Sd_UsrInvulnerable())) {
		return 0;
	}
	Mode_SaveVar(cadVar, &cad);
	if (whoVar) {
		Mode_SaveVar(whoVar, &who);
	} else {
		Native_SetStr(&who, CG);
	}
	PyObject *func = Script_Attr("MissionsFuncs", "OnSuperDealFinish");
	Sd_Params(win, mode, retry, &params);
	byte ok = func && Sd_Remote("ForceRemoteCom", &cad, &who, func, &params);
	Py_XDECREF(func);
	Native_Free(&params);
	Native_Free(&cad);
	Native_Free(&who);
	return ok;
}

MODE_FN(Ctf, PreRestartMenu, 0, Sd_PreEnd(0, "CTFFinishPlayerLoosesTextCad", "CTFFriendHead", "CTF", "CTF.RestartCTF"))
MODE_FN(Ctf, PreOnVictory, 0, Sd_PreEnd(1, "CTFFinishPlayerWinsTextCad", "CTFFriendHead", "CTF", ""))

PyMethodDef g_CTFMethods[] = {
	{ "GetMenuDescription", Ctf_GetMenuDescription, 1, NULL },
	{ "InitStatus", Ctf_InitStatus, 1, NULL },
	{ "Init", Ctf_Init, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "SetProfile", Ctf_SetProfile, 1, NULL },
	{ "CreateEnemies", Ctf_CreateEnemies, 1, NULL },
	{ "CreateFriends", Ctf_CreateFriends, 1, NULL },
	{ "CrazyGamblerStartCTF", Ctf_CrazyGamblerStartCTF, 1, NULL },
	{ "SetVulnerable", Sd_SetVulnerable, 1, NULL },
	{ "StartCTF", Ctf_StartCTF, 1, NULL },
	{ "OnVehicleDeathCallback", Ctf_OnVehicleDeathCallback, 1, NULL },
	{ "OnPlayerDeath", Ctf_OnPlayerDeath, 1, NULL },
	{ "RemoveFlag", Ctf_RemoveFlag, 1, NULL },
	{ "EndOfCTF", Ctf_EndOfCTF, 1, NULL },
	{ "RespawnEnemy", (PyCFunction)(void (*)(void))Ctf_RespawnEnemy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "RespawnFriend", Ctf_RespawnFriend, 1, NULL },
	{ "CreateFlag", (PyCFunction)(void (*)(void))Ctf_CreateFlag, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "ResetFlag", Ctf_ResetFlag, 1, NULL },
	{ "TakeFlag", Ctf_TakeFlag, 1, NULL },
	{ "SpawnFlags", Ctf_SpawnFlags, 1, NULL },
	{ "RespawnPlayer", Ctf_RespawnPlayer, 1, NULL },
	{ "PreRestartMenu", Ctf_PreRestartMenu, 1, NULL },
	{ "RestartCTF", Sd_Restart, 1, NULL },
	{ "PreOnVictory", Ctf_PreOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- Domination: the zones of the OM, taken with the baton; all zones win a round, DominationRounds rounds the deal
 * ---- */

static struct {
	nativeVal_t zones, enemies, friends;	/* lists: the places (then the names) of the zones, the ship names */
	nativeVal_t zonesStates;		/* the state of each zone name: 0 neutral, 1 the player's, 2 the enemies' */
	nativeVal_t zonesCanBeTaken, playerZones, enemyZones, userWithBaton, shipWithBaton, onDeathSpawnTime;
	nativeVal_t startFirst, msgTime, model, batonRadius;
} dom;

/* GetMenuDescription(): the text of the deal with the number of zones */
static byte Dom_GetMenuDescriptionC(nativeVal_t *out) {
	nativeVal_t text, n;
	if (!Mode_Import("string")) {
		return 0;
	}
	Mode_Lang("SuperDeal_Domination_Description", &text);
	Native_SetNone(&n);
	int len = Mode_Read(DOM, "zones", &dom.zones) ? Native_Size(&dom.zones) : -1;
	byte ok = len >= 0;
	if (ok) {
		char s[0x20];
		Str_Printf(s, sizeof(s), "%d", len);
		Native_SetStr(&n, s);
		ok = CALL("string", "replace", out, "vsv", &text, "<n1>", &n);
	}
	Native_Free(&text);
	Native_Free(&n);
	return ok;
}

MODE_FN(Dom, GetMenuDescription, 0, Dom_GetMenuDescriptionC(out))

/* Scrap.Preload3DObject(name) of a global of the module */
static byte Sd_PreloadVar(const char *mod, const char *name, nativeVal_t *slot) {
	return Mode_Read(mod, name, slot) && Mode_Preload(slot);
}

/* InitStatus(LevelPath): the status of the modes and the zone models loaded */
static byte Dom_InitStatusC(void) {
	return Sd_StatusC(DOM, NULL) && Sd_PreloadVar(DOM, "ZoneModelNeutral", &dom.model) &&
		Sd_PreloadVar(DOM, "ZoneModelFriends", &dom.model) && Sd_PreloadVar(DOM, "ZoneModelEnemies", &dom.model);
}

MODE_FN(Dom, InitStatus, 1, Dom_InitStatusC())

/* Init(LevelPath): the camera of the end, the respawn time, both teams in their lists, the leader of the enemies,
 * the start when the player boards; the baton drops when he leaves the ship. The script's `zonesStates = {}` is a
 * local of the function (no global statement), so the table of the zones stays as it is. */
static byte Dom_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Sixth", "SuperDeal_Tenth", NULL };
	nativeVal_t first;
	Native_SetNone(&first);
	byte ok = Mode_Import("Parking") && Mode_Import("Chars") && Sd_CameraFor(libs, "OM") &&
		Sd_PreloadVar(DOM, "BatonModel", &dom.model) && Sd_GlobalTo("Chars", "OnMainCharDeath", DOM, "OnPlayerDeath") &&
		Sd_SpawnTime(DOM, &dom.onDeathSpawnTime, "DominationOnDeathSpawnTime") &&
		Sd_PutI(DOM, "zonesCanBeTaken", &dom.zonesCanBeTaken, 1) && OWN(DOM, "CreateEnemies", NULL, "") &&
		OWN(DOM, "CreateFriends", NULL, "") && Mode_GVarSetS("OnDominFriendDeath", "Domination.OnFriendDeath") &&
		Mode_GVarSetS("OnDominEnemyDeath", "Domination.OnEnemyDeath") && Mode_Read(DOM, "enemies", &dom.enemies) &&
		Sd_MarkShips(&dom.enemies, "red", "DominationEnemies") && Mode_Read(DOM, "enemies", &dom.enemies) &&
		Sd_Item(&dom.enemies, 0, &first) && Mode_GVarSet("LeaderName", &first) && Mode_Read(DOM, "friends", &dom.friends) &&
		Sd_MarkShips(&dom.friends, "green", "DominationFriends") &&
		Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", DOM, "CrazyGamblerStartDomination") &&
		Sd_GlobalTo("Parking", "SwitchCharMissionCallback", DOM, "ReleaseBaton");
	Native_Free(&first);
	return ok;
}

MODE_FN(Dom, Init, 1, Dom_InitC())

/* `for zoneName in zonesStates.keys(): e = Scrap.GetEntity(zoneName); if (e): e.RadarShow = 0` */
static byte Dom_HideZones(void) {
	nativeVal_t keys;
	if (!Mode_Keys(DOM, "zonesStates", 1, &keys)) {
		return 0;
	}
	int n = Native_ForCount(&keys);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t z;
		cEntity *e;
		ok = Native_SeqItem(&keys, i, &z) && Mode_Ent(&z, &e) && (!e || Mode_SetI(e, Native_Text(&z), "RadarShow", 0));
		Native_Free(&z);
	}
	Native_Free(&keys);
	return ok;
}

/* userWithBaton = 0; shipWithBaton = "" (the baton is free) */
static byte Sd_BatonFree(const char *mod, nativeVal_t *user, nativeVal_t *ship) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	return Sd_PutI(mod, "userWithBaton", user, 0) && Mode_Put(mod, "shipWithBaton", ship, &empty);
}

/* ReleaseBaton(): the player left his ship with the baton: it drops at a random place */
static byte Dom_ReleaseBatonC(void) {
	if (!Mode_Read(DOM, "userWithBaton", &dom.userWithBaton)) {
		return 0;
	}
	if (Native_True(&dom.userWithBaton) && (!Mode_Later(0.5, DOM, "LostBatonMessage", NULL, 0, NULL) ||
		!Sd_BatonFree(DOM, &dom.userWithBaton, &dom.shipWithBaton) || !OWN(DOM, "CreateBaton", NULL, ""))) {
		return 0;
	}
	return Dom_HideZones();
}

MODE_FN(Dom, ReleaseBaton, 0, Dom_ReleaseBatonC())

/* LostBatonMessage() of Domination and MortalRace: the message that the player lost the baton */
static byte Sd_LostBaton(const char *key) {
	nativeVal_t cad, now, len, t;
	double d;
	Mode_Lang(key, &cad);
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&len);
	Native_SetNone(&t);
	byte ok = CALL("Speech", "CalculateSpeechTime", &len, "v", &cad) && Native_Arith('+', &now, &len, &t) &&
		Native_ArgDouble(&t, &d) && Mode_MsgText(&cad, d);
	Native_Free(&cad);
	Native_Free(&len);
	Native_Free(&t);
	return ok;
}

MODE_FN(Dom, LostBatonMessage, 0, Sd_LostBaton("SuperDeal_DominationSystemMessage1"))
MODE_FN(Dom, SetProfile, 2, Sd_SetProfileC("Domination", &a[0], &a[1]))

/* CreateEnemies() and CreateFriends(): the ships of a team at the spawn dummies of the team; the first enemy (the
 * leader) carries no money */
static byte Dom_CreateTeam(byte enemy) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spawns = Mode_V(&t), *dummy = Mode_V(&t), *num = Mode_V(&t);
	long n = 0;
	Native_SetEmpty(spawns, NV_LIST);
	Mode_DummyStart(enemy ? "DM_Domination_Enemy_Spawn_*" : "DM_Domination_Friend_Spawn_*", 1);
	Mode_DummyNext(dummy);
	byte ok = 1;
	while (ok && Native_True(dummy)) {
		/* spawns.append((dummy[1], dummy[2][1])) */
		nativeVal_t a2, ang, pos;
		Native_SetNone(&a2);
		Native_SetNone(&ang);
		Native_SetNone(&pos);
		ok = Sd_Item(dummy, 2, &a2) && Sd_Item(&a2, 1, &ang) && Sd_Item(dummy, 1, &pos);
		if (ok) {
			nativeVal_t *s = Native_Append(spawns);
			Native_SetEmpty(s, NV_TUPLE);
			*Native_Append(s) = pos;
			*Native_Append(s) = ang;
		} else {
			Native_Free(&ang);
			Native_Free(&pos);
		}
		Native_Free(&a2);
		Native_Free(dummy);
		Mode_DummyNext(dummy);
	}
	if (ok && !enemy) {
		/* Scrap.UsrEntity(0), its value not used */
		ok = Mode_UsrHandle(num);
		Native_Free(num);
	}
	if (!ok || !Sd_SaveInt(enemy ? "DominationNumEnemies" : "DominationNumFriends", num) || !Sd_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *sp = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
		nativeVal_t *en = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Sd_Item(spawns, i, sp) && Sd_Item(sp, 0, pos) && Sd_Item(spawns, i, sp) && Sd_Item(sp, 1, ang) &&
			Sd_Create(name, pos, "Car", e) && Sd_SetS(e, "WeapList", "15,15,15,15,15,0") &&
			(!enemy || Sd_SetI(e, "LaunchTaunts", 0)) && Sd_Ang(e, ang) && (!enemy || i || Sd_SetI(e, "Money", -1)) &&
			OWN(DOM, "SetProfile", NULL, "vi", e, (long)enemy) && Sd_AppendName(DOM, enemy ? "enemies" : "friends", e) &&
			Mode_VGet(e, "Name", en) && CALL("SAI", "SetStateVehicle", NULL, "iv", 0L, en);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Dom, CreateEnemies, 0, Dom_CreateTeam(1))
MODE_FN(Dom, CreateFriends, 0, Dom_CreateTeam(0))

/* SAI.GetRandomVisibilityPoint(): a random point of the AI graph (None without points) */
static void Sd_RandomVisPoint(nativeVal_t *out) {
	int n = NodesWorld_NumVisPoints(g_NodesWorld);
	if (n <= 0) {
		Native_SetNone(out);
		return;
	}
	const vec3_t *p = NodesWorld_GetVisPoint(g_NodesWorld, crt_rand() % n);
	/* "(fff)" */
	Native_SetVec(out, (double)p->x, (double)p->y, (double)p->z);
}

/* SLogic.ChangeBatonState(pos, state) (other values by name for the errors of "(fff)i") */
static byte Sd_BatonState(const nativeVal_t *pos, long state) {
	double x[3];
	if (!Mode_VecOf(pos, x)) {
		return CALL("SLogic", "ChangeBatonState", NULL, "vi", pos, state);
	}
	vec3_t p = { (float)x[0], (float)x[1], (float)x[2] };
	Domination_SetBatonState(&p, (int)state);
	return 1;
}

/* the baton of Domination and MortalRace: l = the item "Baton" at pos with its model, radius and OnTake (the module's
 * BatonModel and BatonRadius) */
static byte Sd_Baton(const char *mod, const nativeVal_t *pos, const char *onTake, nativeVal_t *l) {
	nativeVal_t name, v;
	Native_SetStr(&name, "Baton");
	Native_SetNone(&v);
	byte ok = Sd_Create(&name, pos, "Item", l) && Mode_Read(mod, "BatonModel", &v) && Mode_VModel(l, &v);
	Native_Free(&v);
	ok = ok && Mode_Read(mod, "BatonRadius", &v) && Mode_VSet(l, "Radius", &v) && Sd_SetI(l, "AppearsIfFree", 0) &&
		Sd_SetI(l, "AutoFace", 0) && Sd_SetI(l, "OnlyUser", 0) && Sd_SetF(l, "rTime", 0.25) && Sd_SetS(l, "OnTake", onTake);
	Native_Free(&v);
	return ok;
}

/* l2 = Scrap.CreateEntity(l.Name + "CheckArrow", pos..., "WithLife"); l2.CamTgt = 0, 1000, 0 */
static byte Sd_BatonArrow(const nativeVal_t *l, const nativeVal_t *pos, nativeVal_t *l2) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *aname = Mode_V(&t), *tgt = Mode_V(&t);
	if (!Mode_VGet(l, "Name", name) || !Mode_Cat(&t, aname, 2, name, Mode_S(&t, "CheckArrow")) ||
		!Sd_Create(aname, pos, "WithLife", l2)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(tgt, NV_TUPLE);
	Native_SetInt(Native_Append(tgt), 0);
	Native_SetInt(Native_Append(tgt), 1000);
	Native_SetInt(Native_Append(tgt), 0);
	return Mode_Done(&t, Mode_VSet(l2, "CamTgt", tgt));
}

/* MissionsFuncs.SetMissionTarget(l2.Name); Scrap.Set("CurrentWaypointName", l2.Name) */
static byte Sd_BatonTarget(const nativeVal_t *l2) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(l2, "Name", &n) && CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", &n);
	Native_Free(&n);
	ok = ok && Mode_VGet(l2, "Name", &n) && Mode_GVarSet("CurrentWaypointName", &n);
	Native_Free(&n);
	return ok;
}

/* CreateBaton(pos = None): the baton at pos or at a random point of the AI graph, the mission target on its arrow;
 * the zones off the radar */
static byte Dom_CreateBatonC(const nativeVal_t *given) {
	nativeVal_t pos, l, l2;
	Native_SetNone(&l);
	Native_SetNone(&l2);
	if (Native_True(given)) {
		Native_Copy(&pos, given);
	} else {
		Sd_RandomVisPoint(&pos);
	}
	byte ok = Sd_Baton(DOM, &pos, "Domination.TakeBaton", &l) && Sd_SetI(&l, "RadarShow", 1) && Mode_Import("Items") &&
		CALL("Items", "SetSound", NULL, "vs", &l, "TakeBDataPack") && Sd_BatonArrow(&l, &pos, &l2) && Sd_BatonTarget(&l2) &&
		Sd_BatonState(&pos, 0) && Dom_HideZones();
	Native_Free(&pos);
	Native_Free(&l);
	Native_Free(&l2);
	return ok;
}

static PyObject *Dom_CreateBaton(PyObject *self, PyObject *args, PyObject *kw) {
	nativeVal_t pos;
	(void)self;
	if (!Sd_KwArg(args, kw, "pos", -1, &pos)) {
		return NULL;
	}
	byte ok = Dom_CreateBatonC(&pos);
	Native_Free(&pos);
	return Mode_Result(ok);
}

/* Speech.AddRemoteCom(key, "CrazyGambler", mod.func, ()) */
static byte Sd_GamblerKey(const char *key, const char *mod, const char *func) {
	nativeVal_t none;
	PyObject *fn = Mode_GlobalPy(mod, func);
	if (!fn) {
		return 0;
	}
	Native_SetEmpty(&none, NV_TUPLE);
	byte ok = CALL("Speech", "AddRemoteCom", NULL, "ssOv", key, CG, fn, &none);
	Py_DECREF(fn);
	return ok;
}

/* usr.Target = Scrap.Get("LeaderName") */
static byte Dom_AimLeader(const nativeVal_t *usr) {
	nativeVal_t leader;
	Native_SetNone(&leader);
	byte ok = Native_GVarGet("LeaderName", &leader) && Mode_VSet(usr, "Target", &leader);
	Native_Free(&leader);
	return ok;
}

/* CrazyGamblerStartDomination(): the player boarded and aims at the leader; the crazy gambler's message starts the
 * first round */
static byte Dom_CrazyGamblerStartDominationC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *v = Mode_V(&t), *fn = Mode_V(&t);
	if (!Mode_Import("Parking") || !Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) ||
		!Sd_GlobalTo("Parking", "SwitchVehicleMissionCallback", DOM, "OnVehicle") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int car = Sd_UsrCar(usr);
	if (car < 0 || (car && !Dom_AimLeader(usr)) || !Mode_Read(DOM, "CrazyGamblerStartDominationFirst", &dom.startFirst)) {
		return Mode_Done(&t, 0);
	}
	byte first = Native_True(&dom.startFirst);
	if (first) {
		Mode_SaveVar("PlayerRounds", v);
		first = Native_IsStr(v, "0");
	}
	if (first) {
		Native_Free(v);
		Mode_SaveVar("EnemyRounds", v);
		first = Native_IsStr(v, "0");
	}
	if (!first) {
		return Mode_Done(&t, OWN(DOM, "StartDomination", NULL, ""));
	}
	if (!Sd_PutI(DOM, "CrazyGamblerStartDominationFirst", &dom.startFirst, 0)) {
		return Mode_Done(&t, 0);
	}
	PyObject *f = Mode_GlobalPy(DOM, "StartDomination");
	Native_Free(v);
	Mode_SaveVar("DominationStartTextCad", v);
	Native_SetEmpty(fn, NV_TUPLE);
	byte ok = f && CALL("Speech", "AddRemoteCom", NULL, "vsOv", v, CG, f, fn);
	Py_XDECREF(f);
	return Mode_Done(&t, ok);
}

MODE_FN(Dom, CrazyGamblerStartDomination, 0, Dom_CrazyGamblerStartDominationC())
/* OnVehicle(): the player's ship can be hit again when he boards it */
MODE_FN(Dom, OnVehicle, 0, Mode_Import("Parking") && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0))

/* SLogic.SetDominationZones(zones): the WithLife entities of the zone names (the CHECKs of the engine function) */
static void Dom_SetZones(const nativeVal_t *zones) {
	CHECK(zones->type == NV_LIST && "El argumento pasado no es una lista");
	CHECK(zones->n > 0 && "El tamaño de la lista es inválido");
	for (int i = 0; i < zones->n; i++) {
		CHECK(zones->items[i].type == NV_STR && "El elemento no es un string");
		cEntity *e = World_FindEntity(Native_Text(&zones->items[i]));
		CHECK(e && Entity_IsType(e, TYEN_WITH_LIFE) && "La entidad no es un 'WithLifeEntity'");
		Domination_AddZone(e);
	}
}

/* StartDomination(): a round: no zones taken, the scorer, the zones at their places, the baton */
static byte Dom_StartDominationC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *rounds = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("Menu") || !Mode_Import("RacerScorer") || !Mode_Import("RaceMaker") ||
		!Sd_PutI(DOM, "playerZones", &dom.playerZones, 0) || !Sd_PutI(DOM, "enemyZones", &dom.enemyZones, 0) ||
		!Sd_SaveInt("PlayerRounds", a) || !Sd_SaveInt("EnemyRounds", b)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("DominationRounds", rounds);
	if (!CALL("RacerScorer", "InitMissionScorer", NULL, "vvv", a, b, rounds) ||
		!Sd_GlobalTo("Menu", "MissionScorerCall", DOM, "MissionScorer") || !OWN(DOM, "MissionScorer", NULL, "i", 0L) ||
		!Mode_Read(DOM, "zones", &dom.zones)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&dom.zones);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t pos;
		Native_SetNone(&pos);
		ok = Mode_Read(DOM, "zones", &dom.zones) && Sd_Item(&dom.zones, i, &pos) && OWN(DOM, "CreateZone", NULL, "iv", (long)i, &pos);
		Native_Free(&pos);
	}
	if (!ok || !Mode_Read(DOM, "zones", &dom.zones)) {
		return Mode_Done(&t, 0);
	}
	if (dom.zones.type == NV_LIST) {
		Dom_SetZones(&dom.zones);
	} else if (!CALL("SLogic", "SetDominationZones", NULL, "v", &dom.zones)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, OWN(DOM, "CreateBaton", NULL, ""));
}

MODE_FN(Dom, StartDomination, 0, Dom_StartDominationC())

/* zonesStates[z] == state (the global read again): 1, 0, -1 with the error */
static int Dom_StateIs(const nativeVal_t *z, long state) {
	nativeVal_t v;
	Native_SetNone(&v);
	int r = Mode_Read(DOM, "zonesStates", &dom.zonesStates) && Native_Subscript(&dom.zonesStates, z, &v) ? Sd_EqI(&v, state) : -1;
	Native_Free(&v);
	return r;
}

/* zonesStates[z] = state in place */
static byte Dom_SetState(const nativeVal_t *z, long state) {
	nativeVal_t v;
	Native_SetInt(&v, state);
	return Mode_DictPut(DOM, "zonesStates", &dom.zonesStates, z, &v);
}

/* the player or the leader took the baton: the baton and its arrow gone, SLogic.ChangeBatonState((0, 0, 0), state),
 * the holder, the crazy gambler's message and the sound */
static byte Dom_Took(long state, long user, const nativeVal_t *ship, const char *key,
	const char *snd, const char *vol) {
	nativeVal_t zero;
	Native_SetEmpty(&zero, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(&zero), 0);
	}
	byte ok = Sd_BatonState(&zero, state) && Sd_PutI(DOM, "userWithBaton", &dom.userWithBaton, user) &&
		Mode_Put(DOM, "shipWithBaton", &dom.shipWithBaton, ship) && CALL("Speech", "ForceRemoteCom", NULL, "ss", key, CG) &&
		Mode_Sound(snd, vol);
	Native_Free(&zero);
	return ok;
}

/* TakeBaton(ItemName, VehicleName): the leader or the player took the baton; with the player the zones he can take
 * show on the radar */
static byte Dom_TakeBatonC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *leader = Mode_V(&t), *usr = Mode_V(&t), *keys = Mode_V(&t), *z = Mode_V(&t), *un = Mode_V(&t);
	cEntity *e;
	if (!Native_GVarGet("LeaderName", leader) || !Mode_Import("Sound") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Sd_Eq(who, leader)) {
		if (!Sd_Del(item) || !Sd_DelArrow(item) || !CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", who) ||
			!Mode_GVarSet("CurrentWaypointName", who) ||
			!Dom_Took(1, 0, who, "SuperDeal_DominationRemoteMessage6", "EneWaySound", "EneWayVolume")) {
			return Mode_Done(&t, 0);
		}
	} else {
		int u = Sd_UsrIs(usr, who);
		if (u < 0) {
			return Mode_Done(&t, 0);
		}
		if (u) {
			if (!Mode_Keys(DOM, "zonesStates", 1, keys)) {
				return Mode_Done(&t, 0);
			}
			int n = Native_ForCount(keys);
			byte ok = n >= 0;
			for (int i = 0; ok && i < n; i++) {
				Native_Free(z);
				ok = Native_SeqItem(keys, i, z);
				int show = ok ? Dom_StateIs(z, 0) : -1;
				if (show == 0) {
					show = Dom_StateIs(z, 2);
				}
				ok = show >= 0 && (!show || (Mode_Ent(z, &e) && (!e || Mode_SetI(e, Native_Text(z), "RadarShow", 1))));
			}
			if (!ok || !Sd_Del(item) || !Sd_DelArrow(item) || !Mode_VGet(usr, "Name", un) ||
				!Dom_Took(2, 1, un, "SuperDeal_DominationRemoteMessage5", "WaySound", "WayVolume")) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!Mode_Ent(leader, &e)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !e || CALL("SAI", "SetStateVehicle", NULL, "iv", 0L, leader));
}

MODE_FN(Dom, TakeBaton, 2, Dom_TakeBatonC(&a[0], &a[1]))

/* CreateZone(i, pos): the neutral zone i at pos and the WithLife the wingmen guard, whose name replaces the place in
 * zones */
static byte Dom_CreateZoneC(const nativeVal_t *i, const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *name = Mode_V(&t), *l = Mode_V(&t), *ln = Mode_V(&t), *wname = Mode_V(&t);
	nativeVal_t *l2 = Mode_V(&t), *model = Mode_V(&t);
	Sd_Repr(i, rep);
	if (!Mode_Cat(&t, name, 2, Mode_S(&t, "Zone"), rep) || !Sd_Create(name, pos, "Item", l) ||
		!Mode_Read(DOM, "ZoneModelNeutral", model) || !Mode_VModel(l, model) || !Sd_SetI(l, "Radius", 3000) ||
		!Sd_SetS(l, "OnTake", "Domination.TakeZone") || !Sd_SetI(l, "RadarShow", 0) || !Mode_VGet(l, "Name", ln) ||
		!Mode_DictPut(DOM, "zonesStates", &dom.zonesStates, ln, Mode_I(&t, 0))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(ln);
	if (!Mode_VGet(l, "Name", ln) || !Mode_Cat(&t, wname, 2, ln, Mode_S(&t, "_WithLife")) || !Sd_Create(wname, pos, "WithLife", l2)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(ln);
	return Mode_Done(&t, Mode_VGet(l2, "Name", ln) && Mode_DictPut(DOM, "zones", &dom.zones, i, ln));
}

MODE_FN(Dom, CreateZone, 2, Dom_CreateZoneC(&a[0], &a[1]))

/* lang text + ": %d/%d" % (a, b) */
static byte Dom_Count(const char *key, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t text, fmt, args, s;
	Mode_Lang(key, &text);
	Native_SetStr(&fmt, ": %d/%d");
	Native_SetEmpty(&args, NV_TUPLE);
	Native_Copy(Native_Append(&args), a);
	Native_Copy(Native_Append(&args), b);
	Native_SetNone(&s);
	byte ok = Native_Format(&fmt, &args, &s) && Native_Arith('+', &text, &s, out);
	Native_Free(&text);
	Native_Free(&args);
	Native_Free(&s);
	return ok;
}

/* SScorer.Set(0, item, "Text", lang text + ": %d/%d" % (count, n)), n NULL for len(zones) */
static byte Dom_SetCount(const char *item, const char *key, const char *count, nativeVal_t *slot, const nativeVal_t *n) {
	nativeVal_t len, text, id, it;
	Native_SetNone(&text);
	if (!Mode_Read(DOM, count, slot)) {
		return 0;
	}
	if (!n) {
		if (!Mode_Read(DOM, "zones", &dom.zones)) {
			return 0;
		}
		int k = Native_Size(&dom.zones);
		if (k < 0) {
			return 0;
		}
		Native_SetInt(&len, k);
		n = &len;
	}
	Native_SetInt(&id, 0);
	Native_SetStr(&it, item);
	byte ok = Dom_Count(key, slot, n, &text) && Mode_ScorerSet(&id, &it, "Text", &text);
	Native_Free(&text);
	return ok;
}

/* the zone item taken by a side: the model of the side, the zone state of the engine, the sound, the count and the
 * table; then the round won by the side when it holds all zones, else the crazy gambler's message. 1 done, 0 when the
 * zone is the side's already (TakeZone returns at once), -1 with the error */
static int Dom_ZoneTaken(byte enemy, const nativeVal_t *item, const nativeVal_t *usr, const nativeVal_t *numZones) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *z = Mode_V(&t), *model = Mode_V(&t), *wname = Mode_V(&t), *idx = Mode_V(&t);
	nativeVal_t *pr = Mode_V(&t), *er = Mode_V(&t), *dr = Mode_V(&t), *one = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *fmt = Mode_V(&t), *args = Mode_V(&t), *text = Mode_V(&t), *id = Mode_V(&t), *it = Mode_V(&t);
	nativeVal_t *own = enemy ? &dom.enemyZones : &dom.playerZones, *other = enemy ? &dom.playerZones : &dom.enemyZones;
	const char *ownVar = enemy ? "enemyZones" : "playerZones", *otherVar = enemy ? "playerZones" : "enemyZones";
	int was = Dom_StateIs(item, enemy ? 2 : 1);
	if (was) {
		Mode_Done(&t, 0);
		return was > 0 ? 0 : -1;
	}
	was = Dom_StateIs(item, enemy ? 1 : 2);
	if (was < 0 || (was && (!Sd_Add(DOM, otherVar, other, -1) ||
		!Dom_SetCount(enemy ? "PlayerZonesText" : "EnemyZonesText", enemy ? "SuperDeal_DominationPlayerZones" :
		"SuperDeal_DominationEnemyZones", otherVar, other, NULL)))) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (!Mode_EntityH(item, z) || !Mode_Read(DOM, enemy ? "ZoneModelEnemies" : "ZoneModelFriends", model) ||
		!Mode_VModel(z, model) || (!enemy && !Sd_SetI(z, "RadarShow", 0)) || !Mode_Read(DOM, "zones", &dom.zones) ||
		!Mode_Cat(&t, wname, 2, item, Mode_S(&t, "_WithLife")) || !Mode_Index(&dom.zones, wname, idx)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	/* SLogic.ChangeZoneState(idx, state) ("ii") */
	Domination_SetZoneState((int)idx->i, enemy ? 2 : 1);
	if (!Mode_Sound(enemy ? "EneWaySound" : "WaySound", enemy ? "EneWayVolume" : "WayVolume") || !Sd_Add(DOM, ownVar, own, 1) ||
		!Dom_SetState(item, enemy ? 2 : 1) || !Dom_SetCount(enemy ? "EnemyZonesText" : "PlayerZonesText", enemy ?
		"SuperDeal_DominationEnemyZones" : "SuperDeal_DominationPlayerZones", ownVar, own, numZones) || !Sd_Draw() ||
		!Mode_Read(DOM, ownVar, own)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (Native_Compare(own, numZones) < 0) {
		return Mode_Done(&t, CALL("Speech", "ForceRemoteCom", NULL, "ss", enemy ? "SuperDeal_DominationRemoteMessage4" :
			"SuperDeal_DominationRemoteMessage3", CG)) ? 1 : -1;
	}
	/* the round won: the rounds of the side counted in its save variable */
	Native_SetInt(one, 1);
	nativeVal_t *won = enemy ? er : pr;
	if (!Sd_SaveInt("PlayerRounds", enemy ? pr : rep) || !Sd_SaveInt("EnemyRounds", enemy ? rep : er) ||
		!Native_Arith('+', rep, one, won) || !Sd_SaveInt("DominationRounds", dr)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	Native_Free(rep);
	Sd_Repr(won, rep);
	if (!Mode_SetSaveVar(enemy ? "EnemyRounds" : "PlayerRounds", rep) || !Sd_PutI(DOM, "zonesCanBeTaken", &dom.zonesCanBeTaken, 0)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (Native_Compare(won, dr) >= 0) {
		return Mode_Done(&t, OWN(DOM, "EndOfDomination", NULL, "i", enemy ? 0L : 1L)) ? 1 : -1;
	}
	if (enemy) {
		return Mode_Done(&t, Sd_Update(pr, er) && Sd_GamblerKey("SuperDeal_DominationRemoteMessage2", DOM, "RestartDomination")) ? 1 : -1;
	}
	/* SScorer.Set(0, "ZonesText", "Text", "%d/%d" % (playerZones, numZones)) */
	Native_SetStr(fmt, "%d/%d");
	Native_SetEmpty(args, NV_TUPLE);
	if (!Mode_Read(DOM, "playerZones", &dom.playerZones)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	Native_Copy(Native_Append(args), &dom.playerZones);
	Native_Copy(Native_Append(args), numZones);
	Native_SetInt(id, 0);
	Native_SetStr(it, "ZonesText");
	return Mode_Done(&t, Native_Format(fmt, args, text) && Mode_ScorerSet(id, it, "Text", text) && Sd_Update(pr, er) &&
		Sd_Draw() && Sd_GamblerKey("SuperDeal_DominationRemoteMessage1", DOM, "RestartDomination") &&
		Sd_SetI(usr, "Invulnerable", 1)) ? 1 : -1;
}

/* TakeZone(ItemName, CharName): an enemy with the baton marker or the player with the baton takes a zone; all zones
 * win the round (the deal at DominationRounds), else the next one starts after the message */
static byte Dom_TakeZoneC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *we = Mode_V(&t), *radar = Mode_V(&t), *num = Mode_V(&t);
	nativeVal_t *now = Mode_V(&t), *cad = Mode_V(&t), *len = Mode_V(&t), *mt = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_Import("Menu") || !Mode_Import("Sound") || !Mode_Import("Speech") ||
		!Mode_Read(DOM, "zonesCanBeTaken", &dom.zonesCanBeTaken)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(&dom.zonesCanBeTaken)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int isUser = Sd_UsrIs(usr, who);
	if (isUser > 0) {
		isUser = Mode_Read(DOM, "shipWithBaton", &dom.shipWithBaton) && Mode_VGet(usr, "Name", un) ?
			Sd_Eq(&dom.shipWithBaton, un) : -1;
		Native_Free(un);
	}
	if (isUser < 0 || !Mode_EntityH(who, we) || !Mode_VGet(we, "RadarShow", radar)) {
		return Mode_Done(&t, 0);
	}
	byte isEnemy = Sd_EqI(radar, 1);
	if (!isUser && !isEnemy) {
		/* the player without the baton at a zone that is not his: the system message, once per its time */
		int msg = Native_True(usr);
		if (msg) {
			msg = Mode_VGet(usr, "Name", un) ? Sd_Eq(who, un) : -1;
		}
		if (msg > 0) {
			msg = Dom_StateIs(item, 1);
			msg = msg < 0 ? -1 : !msg;
		}
		if (msg <= 0) {
			return Mode_Done(&t, msg == 0);
		}
		Native_SetFloat(now, Mode_Time());
		if (!Mode_Read(DOM, "msgTime", &dom.msgTime)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(&dom.msgTime, now) >= 0) {
			return Mode_Done(&t, 1);
		}
		double d;
		Mode_Lang("SuperDeal_Domination_SystemMessage", cad);
		return Mode_Done(&t, CALL("Speech", "CalculateSpeechTime", len, "v", cad) && Native_Arith('+', now, len, mt) &&
			Mode_Put(DOM, "msgTime", &dom.msgTime, mt) && Mode_Read(DOM, "msgTime", &dom.msgTime) &&
			Native_ArgDouble(&dom.msgTime, &d) && Mode_MsgText(cad, d));
	}
	cEntity *z;
	if (!Mode_Ent(item, &z) || !Mode_Read(DOM, "zones", &dom.zones)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&dom.zones);
	if (n < 0 || !Mode_Read(DOM, "zonesStates", &dom.zonesStates)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(num, n);
	int r = Dom_ZoneTaken(isEnemy, item, usr, num);
	if (r <= 0) {
		return Mode_Done(&t, r == 0);
	}
	cEntity *e;
	return Mode_Done(&t, Mode_Ent(item, &e) && Mode_SetS(e, Native_Text(item), "OnTake", "") &&
		Mode_Later(1.0, DOM, "ActivateZone", item, 1, NULL));
}

MODE_FN(Dom, TakeZone, 2, Dom_TakeZoneC(&a[0], &a[1]))

/* ActivateZone(ItemName): the zone can be taken again */
static byte Dom_ActivateZoneC(const nativeVal_t *item) {
	cEntity *e;
	return Mode_Ent(item, &e) && Mode_SetS(e, Native_Text(item), "OnTake", "Domination.TakeZone");
}

MODE_FN(Dom, ActivateZone, 1, Dom_ActivateZoneC(&a[0]))
/* RestartDomination(): the next round in the level again */
MODE_FN(Dom, RestartDomination, 0, Mode_Import("SaveGame") && CALL("SaveGame", "Restart", NULL, ""))

/* the baton drops at the dead ship: DOM.CreateBaton(dead.Pos) (and MortalRace) */
static byte Sd_BatonAt(const char *mod, const nativeVal_t *dead) {
	nativeVal_t pos;
	Native_SetNone(&pos);
	byte ok = Mode_VGet(dead, "Pos", &pos) && OWN(mod, "CreateBaton", NULL, "v", &pos);
	Native_Free(&pos);
	return ok;
}

/* OnPlayerDeath(LooserName, WinnerName): the baton drops where the player died; he comes back after
 * onDeathSpawnTime */
static byte Dom_OnPlayerDeathC(const nativeVal_t *loser) {
	nativeVal_t dead;
	Native_SetNone(&dead);
	byte ok = Mode_EntityH(loser, &dead) && Mode_Read(DOM, "shipWithBaton", &dom.shipWithBaton);
	if (ok && Sd_Eq(&dom.shipWithBaton, loser)) {
		ok = Sd_BatonFree(DOM, &dom.userWithBaton, &dom.shipWithBaton) && Sd_BatonAt(DOM, &dead);
	}
	Native_Free(&dead);
	return ok && Dom_HideZones() && Sd_RespawnLater(DOM, &dom.onDeathSpawnTime, loser);
}

MODE_FN(Dom, OnPlayerDeath, 2, Dom_OnPlayerDeathC(&a[0]))

/* Vehicles.OnDeath(loser, winner); dead = Scrap.GetEntity(loser); dead.EntityList = ""; if loser in list:
 * list.remove(loser) (the start of OnEnemyDeath and OnFriendDeath) */
static byte Dom_Dead(const nativeVal_t *loser, const nativeVal_t *winner, const char *list, nativeVal_t *slot,
	nativeVal_t *dead) {
	return Mode_Import("Vehicles") && CALL("Vehicles", "OnDeath", NULL, "vv", loser, winner) && Mode_EntityH(loser, dead) &&
		Sd_SetS(dead, "EntityList", "") && Sd_RemoveIn(DOM, list, slot, loser);
}

/* OnEnemyDeath(LooserName, WinnerName): an enemy died (the engine's OnDominEnemyDeath); a new one comes after
 * onDeathSpawnTime, the leader drops the baton */
static byte Dom_OnEnemyDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dead = Mode_V(&t), *leader = Mode_V(&t), *is = Mode_V(&t), *empty = Mode_V(&t);
	if (!Dom_Dead(loser, winner, "enemies", &dom.enemies, dead) || !Native_GVarGet("LeaderName", leader)) {
		return Mode_Done(&t, 0);
	}
	byte isLeader = Sd_Eq(leader, loser);
	Native_SetInt(is, isLeader);
	if (!Mode_Read(DOM, "onDeathSpawnTime", &dom.onDeathSpawnTime) || !Sd_LaterV(&dom.onDeathSpawnTime, DOM, "RespawnEnemy", is, 1) ||
		!Sd_Del(loser)) {
		return Mode_Done(&t, 0);
	}
	if (!isLeader) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Read(DOM, "shipWithBaton", &dom.shipWithBaton)) {
		return Mode_Done(&t, 0);
	}
	Native_SetStr(empty, "");
	if (Sd_Eq(&dom.shipWithBaton, leader) && (!Mode_GVarSetS("CurrentWaypointName", "") ||
		!Mode_Put(DOM, "shipWithBaton", &dom.shipWithBaton, empty) || !Sd_BatonAt(DOM, dead))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_GVarSetS("LeaderName", ""));
}

MODE_FN(Dom, OnEnemyDeath, 2, Dom_OnEnemyDeathC(&a[0], &a[1]))

/* OnFriendDeath(LooserName, WinnerName): a ship of the player's team died; a new one comes after onDeathSpawnTime */
static byte Dom_OnFriendDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	nativeVal_t dead;
	Native_SetNone(&dead);
	byte ok = Dom_Dead(loser, winner, "friends", &dom.friends, &dead) && Mode_Read(DOM, "onDeathSpawnTime", &dom.onDeathSpawnTime) &&
		Sd_LaterV(&dom.onDeathSpawnTime, DOM, "RespawnFriend", NULL, 0) && Sd_Del(loser);
	Native_Free(&dead);
	return ok;
}

MODE_FN(Dom, OnFriendDeath, 2, Dom_OnFriendDeathC(&a[0], &a[1]))

/* RespawnPlayer(LooserName): the player's ship at a spawn point, aiming at the leader */
static byte Dom_RespawnPlayerC(const nativeVal_t *name) {
	nativeVal_t usr;
	Native_SetNone(&usr);
	byte ok = Sd_RespawnUsr(name, &usr) && Dom_AimLeader(&usr);
	Native_Free(&usr);
	return ok;
}

MODE_FN(Dom, RespawnPlayer, 1, Dom_RespawnPlayerC(&a[0]))

/* RespawnEnemy(isLeader) and RespawnFriend(): a ship of a team at a spawn point (the new leader without money and as
 * LeaderName) */
static byte Dom_Respawn(byte enemy, const nativeVal_t *leader) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *e = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t);
	byte lead = enemy && Native_True(leader);
	if (!Mode_Import("OutMap") || !Sd_SpawnCar(Mode_S(&t, enemy ? "Enemy" : "Friend"), sp, e) ||
		!Sd_SetS(e, "WeapList", "15,15,15,15,15,0") || (enemy ? !Sd_SetI(e, "LaunchTaunts", 0) : !Sd_SetI(e, "Money", -1)) ||
		!Sd_Item(sp, 1, ang) || !Sd_Ang(e, ang) || !Sd_SetS(e, "TgType", enemy ? "red" : "green") ||
		!Sd_SetS(e, "EntityList", enemy ? "DominationEnemies" : "DominationFriends") || (lead && !Sd_SetI(e, "Money", -1)) ||
		!OWN(DOM, "SetProfile", NULL, "vi", e, (long)enemy) ||
		(enemy && Native_True(leader) && (!Mode_VGet(e, "Name", name) || !Mode_GVarSet("LeaderName", name)))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Sd_AppendName(DOM, enemy ? "enemies" : "friends", e));
}

MODE_FN(Dom, RespawnEnemy, 1, Dom_Respawn(1, &a[0]))
MODE_FN(Dom, RespawnFriend, 0, Dom_Respawn(0, NULL))

/* `for name in names: e = Scrap.GetEntity(name); if (e): e.EntityList = ""; e.Invulnerable = 1` */
static byte Dom_Freeze(const nativeVal_t *names) {
	int n = Native_ForCount(names);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name;
		cEntity *e;
		ok = Native_SeqItem(names, i, &name) && Mode_Ent(&name, &e) &&
			(!e || (Mode_SetS(e, Native_Text(&name), "EntityList", "") && Mode_SetI(e, Native_Text(&name), "Invulnerable", 1)));
		Native_Free(&name);
	}
	return ok;
}

/* EndOfDomination(playerWins): the ships out of their lists and safe, then the last message */
static byte Dom_EndOfDominationC(const nativeVal_t *wins) {
	return Mode_Import("Menu") && Mode_Import("Parking") && Mode_Read(DOM, "enemies", &dom.enemies) &&
		Dom_Freeze(&dom.enemies) && Mode_Read(DOM, "friends", &dom.friends) && Dom_Freeze(&dom.friends) && Sd_ScorerOff() &&
		Sd_ModNone("Parking", "SwitchCharMissionCallback") && Sd_ModNone("Parking", "SwitchVehicleMissionCallback") &&
		OWN(DOM, Native_True(wins) ? "PreOnVictory" : "PreRestartMenu", NULL, "");
}

MODE_FN(Dom, EndOfDomination, 1, Dom_EndOfDominationC(&a[0]))
MODE_FN(Dom, PreRestartMenu, 0, Sd_PreEnd(0, "DominationFinishPlayerLoosesTextCad", NULL, "Domination", "Domination.RestartDomin"))

/* RestartDomin(id, control): the rounds at 0 and the level again */
static byte Dom_RestartDominC(void) {
	Sd_SetSave("PlayerRounds", "0");
	Sd_SetSave("EnemyRounds", "0");
	return CALL("SaveGame", "Restart", NULL, "");
}

MODE_FN(Dom, RestartDomin, 2, Dom_RestartDominC())
MODE_FN(Dom, PreOnVictory, 0, Sd_PreEnd(1, "DominationFinishPlayerWinsTextCad", NULL, "Domination", ""))

/* a text of the mission scorer: the zones of a side (Y below the scorer's text, on the Xbox lower) */
static byte Dom_ScorerText(const nativeVal_t *id, const char *item, long yXbox, long yPc, const char *key,
	const char *count, nativeVal_t *slot) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *it = Mode_S(&t, item), *v = Mode_V(&t), *xbox = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *colors = Mode_V(&t);
	if (!Mode_Read(DOM, "zones", &dom.zones)) {
		return Mode_Done(&t, 0);
	}
	int len = Native_Size(&dom.zones);
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(n, len);
	if (!Mode_ScorerAdd(id, it, "Text") || !Mode_ModVar("Scorer", "MissionScorerText_X", v) || !Mode_ScorerSet(id, it, "X", v) ||
		!Native_GVarGet("isXbox", xbox)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	if (!Mode_ModVar("Scorer", "MissionScorerText_Y", v) ||
		!Native_Arith('+', v, Mode_I(&t, Native_True(xbox) ? yXbox : yPc), y) || !Mode_ScorerSet(id, it, "Y", y) ||
		!Mode_ScorerSet(id, it, "Font", Mode_S(&t, "Horatio")) || !Mode_ScorerSet(id, it, "Effect", Mode_S(&t, "Shadow")) ||
		!Mode_Read(DOM, count, slot) || !Dom_Count(key, slot, n, text) || !Mode_ScorerSet(id, it, "Text", text) ||
		!Mode_ScorerSet(id, it, "Align", Mode_S(&t, "Left"))) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		Native_Free(colors);
		Native_Free(v);
		if (!Mode_ModVar("Scorer", "MissionScorerText_Color", colors) || !Sd_Item(colors, i, v) || !Mode_ScorerSet(id, it, rgba[i], v)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* MissionScorer(id): the zones of both sides above the race scorer */
static byte Dom_MissionScorerC(const nativeVal_t *id) {
	return Mode_Import("RacerScorer") &&
		Dom_ScorerText(id, "PlayerZonesText", 80, -5, "SuperDeal_DominationPlayerZones", "playerZones", &dom.playerZones) &&
		Dom_ScorerText(id, "EnemyZonesText", 105, 20, "SuperDeal_DominationEnemyZones", "enemyZones", &dom.enemyZones) &&
		CALL("RacerScorer", "MissionScorer", NULL, "v", id);
}

MODE_FN(Dom, MissionScorer, 1, Dom_MissionScorerC(&a[0]))

PyMethodDef g_DominationMethods[] = {
	{ "GetMenuDescription", Dom_GetMenuDescription, 1, NULL },
	{ "InitStatus", Dom_InitStatus, 1, NULL },
	{ "Init", Dom_Init, 1, NULL },
	{ "ReleaseBaton", Dom_ReleaseBaton, 1, NULL },
	{ "LostBatonMessage", Dom_LostBatonMessage, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "SetProfile", Dom_SetProfile, 1, NULL },
	{ "CreateEnemies", Dom_CreateEnemies, 1, NULL },
	{ "CreateFriends", Dom_CreateFriends, 1, NULL },
	{ "CreateBaton", (PyCFunction)(void (*)(void))Dom_CreateBaton, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CrazyGamblerStartDomination", Dom_CrazyGamblerStartDomination, 1, NULL },
	{ "OnVehicle", Dom_OnVehicle, 1, NULL },
	{ "StartDomination", Dom_StartDomination, 1, NULL },
	{ "TakeBaton", Dom_TakeBaton, 1, NULL },
	{ "CreateZone", Dom_CreateZone, 1, NULL },
	{ "TakeZone", Dom_TakeZone, 1, NULL },
	{ "ActivateZone", Dom_ActivateZone, 1, NULL },
	{ "RestartDomination", Dom_RestartDomination, 1, NULL },
	{ "OnPlayerDeath", Dom_OnPlayerDeath, 1, NULL },
	{ "OnEnemyDeath", Dom_OnEnemyDeath, 1, NULL },
	{ "OnFriendDeath", Dom_OnFriendDeath, 1, NULL },
	{ "RespawnPlayer", Dom_RespawnPlayer, 1, NULL },
	{ "RespawnEnemy", Dom_RespawnEnemy, 1, NULL },
	{ "RespawnFriend", Dom_RespawnFriend, 1, NULL },
	{ "EndOfDomination", Dom_EndOfDomination, 1, NULL },
	{ "PreRestartMenu", Dom_PreRestartMenu, 1, NULL },
	{ "RestartDomin", Dom_RestartDomin, 1, NULL },
	{ "PreOnVictory", Dom_PreOnVictory, 1, NULL },
	{ "MissionScorer", Dom_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- MortalRace: the race of the FZ with the baton; only the ship with the baton counts the buoys, the others hunt
 * it ---- */

static PyObject *MortalRace_GetMenuDescription(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_MenuDescription(args, "SuperDeal_MortalRace_Description", "MortalRaceWaypoints");
}

/* InitStatus(LevelPath): no police and no traffic, the AI reports the places it reached */
static PyObject *MortalRace_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sdf_Status(&f, MR, "OutPolice");
	Sf_SetMod(&f, "OutPolice", "PoliceActive", Sf_Int(&f, 0));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "MaxFightingTrafficSize", 0);
	Sdf_SetS(&f, "OnItemTakenAI", "MortalRace.OnPositionReached");
	return Sf_None(&f);
}

/* Init(LevelPath): the race of the save variables (buoys, racers' profile, respawn time), the models, the start when
 * the player boards; the baton drops when he leaves the ship */
static PyObject *MortalRace_Init(PyObject *self, PyObject *args) {
	static const char *const libs[] = { "SuperDeal_Second", "SuperDeal_Seventh", "SuperDeal_Twelfth", NULL };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "RaceMaker");
	Sdf_CameraFor(&f, libs, "FZ");
	Sf_SetMod(&f, "MissionsFuncs", "cameraData", Sf_Build(&f, "()"));
	Sf_Ask(&f, "RaceMaker", "PreloadCnDnResources", "()");
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, MR, "BatonModel"));
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, MR, "WayPointModel"));
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, MR, "OnPlayerDeath"));
	Sf_SetMod(&f, "RaceMaker", "StartPos", Sf_Eval(&f, Sdf_Save(&f, "MortalRaceRace"), MR));
	Sf_SetGlobal(&f, MR, "numWaypoints", Sdf_SaveInt(&f, "MortalRaceWaypoints"));
	Sf_SetGlobal(&f, MR, "respawnTime", Sf_ToFloat(&f, Sdf_Save(&f, "MortalRaceRespawnTime")));
	Sf_SetGlobal(&f, MR, "profileType", Sdf_Save(&f, "MortalRaceRacersProfile"));
	Sf_SetGlobal(&f, MR, "FightingFreezeControlAim", Sdf_SaveInt(&f, "MortalRaceFightingFreezeControlAim"));
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, MR, "CrazyGamblerStartMortalRace"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Sf_Global(&f, MR, "ReleaseBaton"));
	return Sf_None(&f);
}

/* RaceMaker.StartPos[i] */
static PyObject *Mr_StartPos(scriptFrame_t *f, PyObject *i) {
	return Sf_GetItem(f, Sf_Mod(f, "RaceMaker", "StartPos"), i);
}

/* iStartPos - 1, at least 0 */
static PyObject *Mr_PrevPos(scriptFrame_t *f) {
	PyObject *i = Sf_OpI(f, Sf_Global(f, MR, "iStartPos"), '-', 1);
	return Sf_Compare(f, i, "<", Sf_Int(f, 0)) > 0 ? Sf_Int(f, 0) : i;
}

/* iStartPos = (iStartPos + 1) % len(RaceMaker.StartPos) */
static void Mr_NextPos(scriptFrame_t *f) {
	PyObject *i = Sf_OpI(f, Sf_Global(f, MR, "iStartPos"), '+', 1);
	PyObject *n = Sf_Int(f, Sf_Len(f, Sf_Mod(f, "RaceMaker", "StartPos")));
	Sf_SetGlobal(f, MR, "iStartPos", Sf_Op(f, i, '%', n));
}

/* sp = VehiclesNPC.cShipAI(); VehiclesNPC.SetAIProfile(sp, MortalRaceRacersProfile); the ship races to pos */
static void Mr_GoTo(scriptFrame_t *f, PyObject *name, PyObject *pos, PyObject *radius) {
	PyObject *sp = Sf_Ask(f, "VehiclesNPC", "cShipAI", "()");
	Sf_Ask(f, "VehiclesNPC", "SetAIProfile", "(OO)", sp, Sdf_Save(f, "MortalRaceRacersProfile"));
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
	Sf_Ask(f, "SAI", "SetStateVehicle", "(iOOOOOO)", 6, name, x, y, z, radius, Sf_Attr(f, sp, "ControlBoostRace"));
}

/* ReleaseBaton(): the player left his ship with the baton: it drops at the last buoy and every racer goes for it */
static PyObject *MortalRace_ReleaseBaton(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	if (Sf_True(&f, Sf_Global(&f, MR, "userWithBaton")) > 0) {
		Sdf_Later(&f, Sf_Float(&f, 0.3), MR, "LostBatonMessage", Sf_Build(&f, "()"));
		Sf_SetGlobal(&f, MR, "userWithBaton", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, MR, "shipWithBaton", Sf_Str(&f, ""));
		PyObject *i = Sf_OpI(&f, Sf_Global(&f, MR, "iStartPos"), '-', 1);
		if (Sf_Compare(&f, i, "<", Sf_Int(&f, 0)) > 0) {
			i = Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "RaceMaker", "StartPos")) - 1);
		}
		PyObject *pos = Sf_Item(&f, Mr_StartPos(&f, i), 0);
		Sdf_Call(&f, MR, "CreateBaton", "(O)", pos);
		Sdf_Later(&f, Sf_Float(&f, 0.2), MR, "MoveRacersToPos", Sf_Build(&f, "(OO)", pos, Sf_Global(&f, MR,
			"BatonRadius")));
	}
	return Sf_None(&f);
}

static PyObject *MortalRace_LostBatonMessage(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_LostBaton(args, "Mission_WinMortalRace_SystemMessage5");
}

/* CrazyGamblerStartMortalRace(): the player boarded: the racers line up, the crazy gambler's message puts the first
 * buoy */
static PyObject *MortalRace_CrazyGamblerStartMortalRace(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Speech");
	Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, MR, "SetVulnerable"));
	PyObject *cad = Sdf_Save(&f, "MortalRaceStartTextCad");
	Sf_Ask(&f, "Speech", "AddRemoteCom", "(OsO())", cad, CG, Sf_Global(&f, MR, "CreateFirstWaypoint"));
	Sdf_Call(&f, MR, "InitRace", "()");
	return Sf_None(&f);
}

/* CreateBaton(pos): the baton at pos, the mission target on its arrow */
static PyObject *MortalRace_CreateBaton(PyObject *self, PyObject *args) {
	PyObject *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &pos)) {
		return NULL;
	}
	PyObject *l = Sdf_Baton(&f, MR, pos, "MortalRace.TakeBaton");
	PyObject *l2 = Sdf_BatonArrow(&f, l, pos);
	Sf_SetGlobal(&f, MR, "baton", l);
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	Sdf_Set(&f, "CurrentWaypointName", Sf_Attr(&f, l2, "Name"));
	return Sf_None(&f);
}

/* TakeBaton(ItemName, VehicleName): a racer or the player took the baton: the buoy comes back, the others hunt him */
static PyObject *MortalRace_TakeBaton(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *usr = Sdf_Usr(&f);
	if (Sf_True(&f, Sf_Global(&f, MR, "baton")) <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, Sf_Global(&f, MR, "waypointName"));
	Sf_SetAttrI(&f, e, "Active", 1);
	Sf_SetAttrS(&f, e, "OnTake", "MortalRace.OnCheckPoint");
	Sdf_Del(&f, item);
	Sdf_Del(&f, Sf_Cat(&f, "Os", item, "CheckArrow"));
	Sf_SetGlobal(&f, MR, "baton", Py_None);
	PyObject *pos = Sf_Attr(&f, e, "Pos");
	int u = Sf_True(&f, usr);
	int racer = u == 0 ? 1 : u > 0 ? Sf_Compare(&f, who, "!=", Sf_Attr(&f, usr, "Name")) : -1;
	if (racer > 0) {
		if (Sf_True(&f, Sf_Global(&f, MR, "messageKillShipThrown")) == 0) {
			int show = u == 0 ? 1 : Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
			if (show > 0) {
				Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sdf_Lang(&f,
					"Mission_WinMortalRace_SystemMessage2"));
			}
			Sf_SetGlobal(&f, MR, "messageKillShipThrown", Sf_Int(&f, 1));
		}
		Sf_SetGlobal(&f, MR, "shipWithBaton", who);
		PyObject *v = Sf_Entity(&f, who);
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", v, 1);
		Mr_GoTo(&f, who, pos, Sf_Global(&f, MR, "BatonRadius"));
		Sdf_Call(&f, MR, "RacersAttackShip", "(O)", who);
		if (Sdf_UsrCar(&f, usr) > 0) {
			Sf_SetAttr(&f, usr, "Target", who);
		}
		Sdf_Set(&f, "CurrentWaypointName", who);
		Sf_SetAttrS(&f, v, "TgType", "red");
		Sdf_Sound(&f, "EneWaySound", "EneWayVolume");
	} else if (racer == 0 && Sdf_UsrIs(&f, usr, who) > 0) {
		Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sdf_Lang(&f, "Mission_WinMortalRace_SystemMessage1"));
		Sf_SetGlobal(&f, MR, "messageKillShipThrown", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, MR, "userWithBaton", Sf_Int(&f, 1));
		Sf_SetGlobal(&f, MR, "shipWithBaton", who);
		Sdf_Call(&f, MR, "RacersAttackShip", "(O)", who);
		Sdf_Set(&f, "CurrentWaypointName", Sf_Global(&f, MR, "waypointName"));
		Sdf_Sound(&f, "WaySound", "WayVolume");
	}
	return Sf_None(&f);
}

/* InitRace(createFirstWaypoint = 0): the racers of MortalRaceRacers in a circle around the first start position,
 * static until the start, with full ammo; the AI profile of the first one is kept for the fights */
static PyObject *MortalRace_InitRace(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "createFirstWaypoint", NULL };
	static const char *const mods[] = { "Alarm", "Speech", "Parking", "RaceMaker", "RacerScorer", "VehiclesNPC", "math",
		NULL };
	PyObject *first = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &first)) {
		return NULL;
	}
	if (!first) {
		first = Sf_Int(&f, 0);
	}
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetGlobal(&f, MR, "iStartPos", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, MR, "BayScorer", Sf_Keep(&f, PyDict_New()));
	Sf_SetGlobal(&f, MR, "YourScore", Sf_Int(&f, -1));
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDeath", "MortalRace.RacerKilled");
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "MortalRace.RacerDamaged");
	Sf_SetGlobal(&f, MR, "iStartPos", Sf_Int(&f, 0));
	PyObject *racers = Sf_Eval(&f, Sdf_Save(&f, "MortalRaceRacers"), MR);
	int n = Sf_Len(&f, racers);
	int profile = 1;
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *start = Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos"));
		PyObject *start2 = Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos"));
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)(iOi))", 0, 2200, 0, 0, Sf_Item(&f, start2, 1),
			90 + 360 * i / n);
		PyObject *spos = Sf_Ask(&f, "SVec", "Add", "(OO)", Sf_Item(&f, start, 0), rot);
		PyObject *foe = Sf_Item(&f, racers, i);
		PyObject *v = Sdf_Create(&f, Sf_Cat(&f, "sO", "MortalRace_Racer_", foe), spos, "Car");
		Sf_SetItem(&f, Sf_Global(&f, MR, "nameProfileDict"), Sf_Attr(&f, v, "Name"), foe);
		Sf_Ask(&f, "RaceMaker", "InitShipIAProfile", "(OO)", foe, Sf_Global(&f, MR, "profileType"));
		PyObject *ship = Sf_Ask(&f, "RaceMaker", "GetRandomShipIAProfile", "(OO)", foe, Sf_Global(&f, MR,
			"profileType"));
		if (profile && !f.failed) {
			profile = 0;
			Sf_SetGlobal(&f, MR, "prof", Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(O)", ship));
			Sf_SetGlobal(&f, MR, "prevCadenceShoot", Sf_Attr(&f, Sf_Global(&f, MR, "prof"), "CadenceShoot"));
			Sf_SetGlobal(&f, MR, "prevControlAim", Sf_Attr(&f, Sf_Global(&f, MR, "prof"), "ControlAim"));
		}
		Sf_Ask(&f, "VehiclesNPC", "SetIAProf", "(OO)", v, ship);
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, v, "Name"));
		Sf_SetAttrS(&f, v, "TgType", "none");
		Sf_SetAttrS(&f, v, "EntityList", "Race");
		Sdf_Ang(&f, v, Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 1));
		Sf_SetAttrI(&f, v, "Money", -1);
		Sf_SetAttr(&f, v, "MovePos", Sf_Attr(&f, v, "Pos"));
		Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, v, "Ang"));
		Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.001));
		Sf_SetAttrI(&f, v, "LaunchTaunts", 0);
		static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
		for (int a = 0; a < 3; a++) {
			Sf_SetAttr(&f, v, ammo[a], Sf_Ask(&f, "SWeap", "GetFAmmo", "(is)", a, "Max"));
		}
		Sf_SetItem(&f, Sf_Global(&f, MR, "BayScorer"), Sf_Attr(&f, v, "Name"), Sf_Int(&f, 0));
		Sdf_AppendName(&f, MR, "racers", v);
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, first) > 0) {
		Sdf_Call(&f, MR, "CreateFirstWaypoint", "()");
	}
	return Sf_None(&f);
}

/* CreateFirstWaypoint(): the buoy of the start at the first start position (once), the way point */
static PyObject *MortalRace_CreateFirstWaypoint(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Speech");
	Sdf_Call(&f, MR, "AbortRemoteMessages", "()");
	PyObject *l = Sf_Entity(&f, Sf_Global(&f, MR, "waypointName"));
	if (Sf_True(&f, l) == 0) {
		PyObject *pos = Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 0);
		l = Sdf_Create(&f, Sf_Str(&f, "CheckPoint"), pos, "Item");
		Sf_Method(&f, l, "SetModel", "(O)", Sf_Global(&f, MR, "WayPointModel"));
		Sf_SetAttrS(&f, l, "OnTake", "MortalRace.OnRaceStart");
		Sf_SetAttrI(&f, l, "RadarShow", 2);
		Sf_SetAttrI(&f, l, "Radius", 2400);
		Sdf_Ang(&f, l, Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 1));
		Sf_SetAttrI(&f, l, "FadeInEffect", 0);
		Sf_SetAttrI(&f, l, "FadeOutEffect", 0);
		Sf_SetGlobal(&f, MR, "waypointName", Sf_Attr(&f, l, "Name"));
		Sdf_Set(&f, "CurrentWaypointName", Sf_Global(&f, MR, "waypointName"));
	}
	return Sf_Return(&f, l);
}

/* OnRaceStart(ItemName, VehicleName): the player at the first buoy: the camera travels to him, the count down,
 * RaceGo at its end */
static PyObject *MortalRace_OnRaceStart(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	PyObject *user = Sdf_Usr(&f);
	if (Sdf_UsrIs(&f, user, who) > 0) {
		PyObject *t = Sf_Ask(&f, "RaceMaker", "StartCameraTravelling", "(OO)", item, who);
		Sdf_Later(&f, t, MR, "RaceGo", Sf_Build(&f, "(OO)", item, who));
		Sf_Ask(&f, "RaceMaker", "CreateRaceCountDown", "()");
	}
	return Sf_None(&f);
}

/* RaceGo(ItemName, VehicleName): the start: the first buoy counts, the scorer, the check of the hunters every 2 s */
static PyObject *MortalRace_RaceGo(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "RacerScorer");
	Sf_Ask(&f, "RaceMaker", "AbortCameraTravelling", "()");
	Sf_Ask(&f, "RaceMaker", "DestroyRaceCountDown", "()");
	Sdf_Call(&f, MR, "OnCheckPoint", "(OO)", item, who);
	Sf_Ask(&f, "RacerScorer", "InitMissionScorer", "(iiO)", 0, 0, Sf_Repr(&f, Sf_Global(&f, MR, "numWaypoints")));
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Sf_Mod(&f, "RacerScorer", "MissionScorer"));
	Sdf_Draw(&f);
	Sdf_Later(&f, Sf_Int(&f, 2), MR, "CheckShipsState", Sf_Build(&f, "()"));
	return Sf_None(&f);
}

/* the hunters of CheckShipsState: the attackers far from the ship with the baton and the waiting ones near it */
typedef struct {
	PyObject *ship, *attack, *freeze;
} mrHunt_t;

static int Mr_Hunter(scriptFrame_t *f, PyObject *e, PyObject *name, void *arg) {
	mrHunt_t *h = arg;
	(void)name;
	if (Sf_Compare(f, Sf_Attr(f, e, "Name"), "!=", Sf_Attr(f, h->ship, "Name")) > 0) {
		PyObject *state = Sf_Ask(f, "SAI", "GetStateVehicle", "(O)", Sf_Attr(f, e, "Name"));
		int st = Sf_EqI(f, state, 2) > 0 ? 2 : !f->failed && Sf_EqI(f, state, 8) > 0 ? 8 : 0;
		if (st) {
			PyObject *d = Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", Sf_Attr(f, h->ship,
				"Pos"), Sf_Attr(f, e, "Pos")));
			PyObject *lim = Sf_Global(f, MR, st == 2 ? "MIN_CHASE_DIST" : "MAX_FIGHTING_FREEZE_DIST");
			lim = Sf_Op(f, lim, '*', Sf_Global(f, MR, st == 2 ? "MIN_CHASE_DIST" : "MAX_FIGHTING_FREEZE_DIST"));
			if (Sf_Compare(f, d, st == 2 ? ">" : "<", lim) > 0) {
				Sf_Method(f, st == 2 ? h->attack : h->freeze, "append", "(O)", Sf_Attr(f, e, "Name"));
			}
		}
	}
	return !f->failed;
}

/* CheckShipsState(): one racer keeps attacking the ship with the baton, the other attackers wait some buoys ahead;
 * the waiting ones it reached attack again with their profile (every 2 s) */
static PyObject *MortalRace_CheckShipsState(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	mrHunt_t h = { .ship = Sf_Entity(&f, Sf_Global(&f, MR, "shipWithBaton")) };
	if (Sf_True(&f, h.ship) > 0) {
		h.attack = Sf_Keep(&f, PyList_New(0));
		h.freeze = Sf_Keep(&f, PyList_New(0));
		Sdf_EachInList(&f, "Race", Mr_Hunter, &h);
		int n = Sf_Len(&f, h.attack);
		if (n > 1) {
			Sf_Import(&f, "RaceMaker");
			for (int i = 1; i < n && !f.failed; i++) {
				int mark = f.n;
				PyObject *at = Sf_OpI(&f, Sf_Op(&f, Sf_Global(&f, MR, "iStartPos"), '+', Sf_Global(&f, MR,
					"NUM_WAYPOINTS_AHEAD")), '+', i - 1);
				PyObject *ip = Sf_Op(&f, at, '%', Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "RaceMaker", "StartPos"))));
				PyObject *pos = Sf_Item(&f, Mr_StartPos(&f, ip), 0);
				PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
				Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOOOOO)", 1, Sf_Item(&f, h.attack, i), x, y, z, Sf_Global(&f, MR,
					"WaypRadius"));
				Sf_Release(&f, mark);
			}
		}
		n = Sf_Len(&f, h.freeze);
		for (int i = 0; i < n && !f.failed; i++) {
			int mark = f.n;
			PyObject *e = Sf_Entity(&f, Sf_Item(&f, h.freeze, i));
			if (Sf_True(&f, e) > 0) {
				Sf_SetAttr(&f, Sf_Global(&f, MR, "prof"), "CadenceShoot", Sf_Global(&f, MR, "prevCadenceShoot"));
				Sf_SetAttr(&f, Sf_Global(&f, MR, "prof"), "ControlAim", Sf_Global(&f, MR, "prevControlAim"));
				Sf_Ask(&f, "VehiclesNPC", "ModAIProff", "(OO)", e, Sf_Global(&f, MR, "prof"));
				Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Item(&f, h.freeze, i), Sf_Attr(&f, h.ship, "Name"));
			}
			Sf_Release(&f, mark);
		}
	}
	Sdf_Later(&f, Sf_Int(&f, 2), MR, "CheckShipsState", Sf_Build(&f, "()"));
	return Sf_None(&f);
}

/* OnPositionReached(VehicleName): a racer reached the buoy where it waits: it fights there, shooting often and badly */
static PyObject *MortalRace_OnPositionReached(PyObject *self, PyObject *args) {
	PyObject *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &who)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *ship = Sf_Entity(&f, Sf_Global(&f, MR, "shipWithBaton"));
	if (Sf_True(&f, ship) > 0 && Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", who), 1) > 0) {
		Sf_Import(&f, "VehiclesNPC");
		Sf_SetAttr(&f, Sf_Global(&f, MR, "prof"), "CadenceShoot", Sf_Global(&f, MR, "FightingFreezeCadenceShoot"));
		Sf_SetAttr(&f, Sf_Global(&f, MR, "prof"), "ControlAim", Sf_Global(&f, MR, "FightingFreezeControlAim"));
		PyObject *e = Sf_Entity(&f, who);
		Sf_Ask(&f, "VehiclesNPC", "ModAIProff", "(OO)", e, Sf_Global(&f, MR, "prof"));
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOO)", 8, who, Sf_Attr(&f, ship, "Name"));
	}
	return Sf_None(&f);
}

/* OnCheckPoint(ItemName, VehicleName): a buoy taken. The player's first one (YourScore -1 to 0) puts the baton at the
 * next start position; with the baton the player or a racer counts it, numWaypoints win (or lose) the race */
static PyObject *MortalRace_OnCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *usrName = Sf_Str(&f, "");
	PyObject *usr = Sdf_Usr(&f);
	if (Sf_True(&f, usr) > 0) {
		usrName = Sf_Attr(&f, usr, "Name");
	}
	int before = Sf_EqI(&f, Sf_Global(&f, MR, "YourScore"), -1);
	if (before > 0) {
		before = Sf_Compare(&f, who, "!=", usrName);
	}
	if (before != 0) {
		if (before > 0 && Sf_EqI(&f, Sf_Global(&f, MR, "YourScore"), -1) > 0) {
			Sf_SetGlobal(&f, MR, "YourScore", Sf_Int(&f, 0));
		}
		return Sf_None(&f);
	}
	PyObject *l = Sf_Entity(&f, item);
	int mine = Sf_True(&f, usr);
	if (mine > 0) {
		mine = Sf_Compare(&f, usrName, "==", who);
	}
	if (mine > 0) {
		int locked = Sf_Compare(&f, Sf_Global(&f, MR, "YourScore"), ">", Sf_Int(&f, -1));
		if (locked > 0) {
			locked = Sf_Compare(&f, Sf_Global(&f, MR, "shipWithBaton"), "!=", usrName);
		}
		if (locked != 0) {
			return Sf_None(&f);
		}
		Sdf_Sound(&f, "WaySound", "WayVolume");
		PyObject *score = Sdf_Add(&f, MR, "YourScore", 1);
		if (Sf_EqI(&f, score, 0) > 0) {
			Mr_NextPos(&f);
			PyObject *pos = Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 0);
			Sdf_Call(&f, MR, "CreateBaton", "(O)", pos);
			Sdf_Call(&f, MR, "MoveRacersToPos", "(OO)", pos, Sf_Global(&f, MR, "BatonRadius"));
			Mr_NextPos(&f);
			if (Sf_True(&f, l) == 0) {
				l = Sdf_Call(&f, MR, "CreateFirstWaypoint", "()");
			}
			Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 0));
			Sdf_Ang(&f, l, Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 1));
			Sf_SetAttrI(&f, l, "Active", 0);
			return Sf_None(&f);
		}
		if (Sf_Compare(&f, Sf_Global(&f, MR, "YourScore"), ">=", Sf_Global(&f, MR, "numWaypoints")) > 0) {
			Sf_Method(&f, l, "Del", "()");
			Sdf_ScorerOff(&f);
			Sdf_Call(&f, MR, "PreOnVictory", "()");
			return Sf_None(&f);
		}
		Sdf_Update(&f, Sf_Global(&f, MR, "YourScore"), Sf_Global(&f, MR, "BestScorer"));
		Sdf_Draw(&f);
		Sdf_Call(&f, MR, "MoveWaypoint", "()");
	} else if (mine == 0 && Sf_True(&f, Sf_Method(&f, Sf_Global(&f, MR, "BayScorer"), "has_key", "(O)", who)) > 0) {
		if (Sf_Compare(&f, Sf_Global(&f, MR, "shipWithBaton"), "!=", who) != 0) {
			return Sf_None(&f);
		}
		Sf_SetAttrI(&f, l, "Active", 0);
		PyObject *n = Sf_OpI(&f, Sf_GetItem(&f, Sf_Global(&f, MR, "BayScorer"), who), '+', 1);
		Sf_SetItem(&f, Sf_Global(&f, MR, "BayScorer"), who, n);
		Sdf_Sound(&f, "EneWaySound", "EneWayVolume");
		PyObject *count = Sf_GetItem(&f, Sf_Global(&f, MR, "BayScorer"), who);
		int lost = Sf_Compare(&f, count, ">=", Sf_Global(&f, MR, "numWaypoints"));
		if (lost > 0) {
			Sf_Method(&f, l, "Del", "()");
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Entity(&f, who), 0);
			Sdf_ScorerOff(&f);
			PyObject *func = Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish");
			PyObject *params = Sf_Build(&f, "(iss)", 0, "MortalRace", "MortalRace.RestartRace");
			PyObject *cad = Sdf_Save(&f, "MortalRaceFinishPlayerLoosesTextCad");
			Sdf_Remote(&f, "ForceRemoteCom", cad, Sdf_Save(&f, "MortalRaceFinishPlayerLoosesTextFoe"), func, params);
			Sdf_Call(&f, MR, "EndOfMortalRace", "()");
			return Sf_None(&f);
		} else if (lost == 0) {
			Sf_SetGlobal(&f, MR, "BestScorer", Sf_Int(&f, 0));
			PyObject *keys = Sf_Method(&f, Sf_Global(&f, MR, "BayScorer"), "keys", "()");
			for (int i = 0;; i++) {
				int mark = f.n;
				PyObject *k = Sf_Next(&f, keys, i);
				if (!k) {
					break;
				}
				PyObject *c = Sf_GetItem(&f, Sf_Global(&f, MR, "BayScorer"), k);
				if (Sf_Compare(&f, c, ">", Sf_Global(&f, MR, "BestScorer")) > 0) {
					Sf_SetGlobal(&f, MR, "BestScorer", Sf_GetItem(&f, Sf_Global(&f, MR, "BayScorer"), k));
				}
				Sf_Release(&f, mark);
			}
			PyObject *pr[2];
			Sf_Unpack(&f, Sdf_Call(&f, MR, "MoveWaypoint", "()"), 2, pr);
			Mr_GoTo(&f, who, pr[0], Sf_Global(&f, MR, "BatonRadius"));
			Sdf_Update(&f, Sf_Global(&f, MR, "YourScore"), Sf_Global(&f, MR, "BestScorer"));
			Sdf_Draw(&f);
		}
	}
	return Sf_None(&f);
}

/* Restart(): the score table of a lost race */
static PyObject *MortalRace_Restart(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "MissionsFuncs", "OnSuperDealFinish", "(iss)", 2, "MortalRace", "MortalRace.RestartRace");
	return Sf_None(&f);
}

/* MoveWaypoint(): the buoy at the next start position, active; (its Pos, its Radius) */
static PyObject *MortalRace_MoveWaypoint(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Mr_NextPos(&f);
	PyObject *l = Sf_Entity(&f, Sf_Global(&f, MR, "waypointName"));
	Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 0));
	Sdf_Ang(&f, l, Sf_Item(&f, Mr_StartPos(&f, Sf_Global(&f, MR, "iStartPos")), 1));
	Sf_SetAttrS(&f, l, "OnTake", "MortalRace.OnCheckPoint");
	Sf_SetAttrI(&f, l, "Active", 1);
	PyObject *pos = Sf_Attr(&f, l, "Pos");
	return Sf_Return(&f, Sf_Build(&f, "(OO)", pos, Sf_Attr(&f, l, "Radius")));
}

/* EndOfMortalRace(): the buoy gone, the racers back to the traffic */
static PyObject *MortalRace_EndOfMortalRace(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
	Sdf_ScorerOff(&f);
	PyObject *e = Sf_Entity(&f, Sf_Global(&f, MR, "waypointName"));
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	Sdf_SetS(&f, "OnItemTakenAI", "");
	Sdf_Call(&f, MR, "MoveRacersToTraffic", "()");
	return Sf_None(&f);
}

static int Mr_Collect(scriptFrame_t *f, PyObject *e, PyObject *name, void *players) {
	(void)name;
	Sf_Method(f, players, "append", "(O)", Sf_Attr(f, e, "Name"));
	return !f->failed;
}

/* MoveRacersToTraffic(): the racers become safe traffic; the race state at its start values */
static PyObject *MortalRace_MoveRacersToTraffic(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *players = Sf_Keep(&f, PyList_New(0));
	Sdf_EachInList(&f, "Race", Mr_Collect, players);
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *n = Sf_Next(&f, players, i);
		if (!n) {
			break;
		}
		PyObject *e = Sf_Entity(&f, n);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "TgType", "none");
			Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.0));
			Sf_SetAttrS(&f, e, "EntityList", "Traffic");
			Sf_SetAttrI(&f, e, "Invulnerable", 1);
			Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, e, "Name"), 1);
		}
		Sf_Release(&f, mark);
	}
	Sf_SetGlobal(&f, MR, "YourScore", Sf_Int(&f, -1));
	Sf_SetGlobal(&f, MR, "waypointName", Sf_Str(&f, ""));
	Sf_SetGlobal(&f, MR, "iStartPos", Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* MoveRacersToPos(pos, radius): every active racer races to pos */
typedef struct {
	PyObject *pos, *radius;
} mrPlace_t;

static int Mr_Race(scriptFrame_t *f, PyObject *e, PyObject *name, void *arg) {
	mrPlace_t *p = arg;
	if (Sf_True(f, Sf_Attr(f, e, "Active")) > 0) {
		Sf_SetAttrS(f, e, "TgType", "green");
		Sf_SetAttr(f, e, "MoveTime", Sf_Float(f, 0.0));
		Mr_GoTo(f, name, p->pos, p->radius);
	}
	return !f->failed;
}

static PyObject *MortalRace_MoveRacersToPos(PyObject *self, PyObject *args) {
	mrPlace_t p;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &p.pos, &p.radius)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "VehiclesNPC");
	Sdf_EachInList(&f, "Race", Mr_Race, &p);
	Sdf_Update(&f, Sf_Global(&f, MR, "YourScore"), Sf_Global(&f, MR, "BestScorer"));
	Sdf_Draw(&f);
	return Sf_None(&f);
}

/* RacersAttackShip(VehicleName): every racer attacks the ship with the baton, with the profile's aim against the
 * player and a bad one against a racer */
typedef struct {
	PyObject *ship;
	byte usr;
} mrAttack_t;

static int Mr_Attack(scriptFrame_t *f, PyObject *e, PyObject *name, void *arg) {
	mrAttack_t *a = arg;
	if (a->usr || Sf_Compare(f, name, "!=", a->ship) > 0) {
		Sf_Ask(f, "VehiclesNPC", "ModAIProff", "(OO)", e, Sf_Global(f, MR, "prof"));
		Sf_Ask(f, "SAI", "SetStateVehicle", "(iOO)", 2, name, a->ship);
	}
	return !f->failed;
}

static PyObject *MortalRace_RacersAttackShip(PyObject *self, PyObject *args) {
	mrAttack_t a;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &a.ship)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *usr = Sdf_Usr(&f);
	PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Race", "First");
	Sf_Entity(&f, name);
	int u = Sdf_UsrIs(&f, usr, a.ship);
	if (u >= 0) {
		a.usr = u > 0;
		Sf_SetAttr(&f, Sf_Global(&f, MR, "prof"), "ControlAim", u > 0 ? Sf_Global(&f, MR, "prevControlAim") : Sf_Int(&f,
			2));
		Sdf_EachInList(&f, "Race", Mr_Attack, &a);
	}
	return Sf_None(&f);
}

/* OnPlayerDeath(LooserName, WinnerName): the player drops the baton where he died; he comes back after respawnTime */
static PyObject *MortalRace_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, loser);
	if (Sf_Compare(&f, loser, "==", Sf_Global(&f, MR, "shipWithBaton")) > 0) {
		Sf_SetGlobal(&f, MR, "userWithBaton", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, MR, "shipWithBaton", Sf_Str(&f, ""));
		Sdf_Call(&f, MR, "CreateBaton", "(O)", Sf_Attr(&f, e, "Pos"));
		Sdf_Later(&f, Sf_Float(&f, 0.2), MR, "MoveRacersToPos", Sf_Build(&f, "(OO)", Sf_Attr(&f, e, "Pos"), Sf_Global(&f,
			MR, "BatonRadius")));
	}
	PyObject *pos = Sf_Attr(&f, e, "Pos");
	Sdf_Later(&f, Sf_Global(&f, MR, "respawnTime"), MR, "RespawnPlayer", Sf_Build(&f, "(OOO)", loser, pos, Sf_Attr(&f, e,
		"Ang")));
	return Sf_None(&f);
}

/* RacerKilled(LooserName, WinnerName): a racer died (it drops the baton when it had it); it comes back after
 * respawnTime */
static PyObject *MortalRace_RacerKilled(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Ask(&f, "Vehicles", "OnDeath", "(OO)", loser, winner);
	Sdf_RemoveIn(&f, MR, "racers", loser);
	PyObject *e = Sf_Entity(&f, loser);
	int drop = Sf_True(&f, e);
	if (drop > 0) {
		drop = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
	}
	if (drop > 0) {
		Sf_SetGlobal(&f, MR, "shipWithBaton", Sf_Str(&f, ""));
		Sdf_Call(&f, MR, "CreateBaton", "(O)", Sf_Attr(&f, e, "Pos"));
		Sdf_Later(&f, Sf_Float(&f, 0.2), MR, "MoveRacersToPos", Sf_Build(&f, "(OO)", Sf_Attr(&f, e, "Pos"), Sf_Global(&f,
			MR, "BatonRadius")));
	}
	PyObject *foe = Sf_GetItem(&f, Sf_Global(&f, MR, "nameProfileDict"), loser);
	Sdf_Later(&f, Sf_Global(&f, MR, "respawnTime"), MR, "RespawnRacer", Sf_Build(&f, "(OO)", loser, foe));
	Sdf_Del(&f, loser);
	if (Sf_True(&f, Sf_Method(&f, Sf_Global(&f, MR, "nameProfileDict"), "has_key", "(O)", loser)) > 0) {
		Sf_DelItem(&f, Sf_Global(&f, MR, "nameProfileDict"), loser);
	}
	return Sf_None(&f);
}

/* RacerDamaged(LooserName, WinnerName, Damage): the player hit a racer before the start: the race starts from the
 * first buoy */
static PyObject *MortalRace_RacerDamaged(PyObject *self, PyObject *args) {
	PyObject *loser, *winner, *damage;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &loser, &winner, &damage)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *usr = Sdf_Usr(&f);
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", winner) > 0 && Sf_EqI(&f, Sf_Global(&f, MR, "YourScore"),
		-1) > 0) {
		Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "");
		Sf_Ask(&f, "RacerScorer", "InitMissionScorer", "(iiO)", 0, 0, Sf_Repr(&f, Sf_Global(&f, MR, "numWaypoints")));
		Sf_SetMod(&f, "Menu", "MissionScorerCall", Sf_Mod(&f, "RacerScorer", "MissionScorer"));
		Sdf_Draw(&f);
		Sdf_Call(&f, MR, "OnCheckPoint", "(OO)", Sf_Global(&f, MR, "waypointName"), winner);
	}
	return Sf_None(&f);
}

/* RespawnRacer(racerName, foeType): the racer again at the last start position while the race runs; it attacks the
 * ship with the baton, else goes for the baton or the player */
static int Mr_RedFirst(scriptFrame_t *f, PyObject *e, PyObject *name, void *arg) {
	PyObject **v = arg;
	int red = Sf_EqS(f, Sf_Attr(f, e, "TgType"), "red");
	if (red > 0) {
		Sf_Ask(f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(f, v[0], "Name"), name);
		v[1] = Py_True;
	}
	return red == 0;
}

static PyObject *MortalRace_RespawnRacer(PyObject *self, PyObject *args) {
	PyObject *name, *foe;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &foe)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	if (Sf_True(&f, Sf_Entity(&f, Sf_Global(&f, MR, "waypointName"))) == 0) {
		return Sf_None(&f);
	}
	PyObject *pos = Sf_Item(&f, Mr_StartPos(&f, Mr_PrevPos(&f)), 0);
	PyObject *v = Sdf_Create(&f, name, pos, "Car");
	Sf_SetItem(&f, Sf_Global(&f, MR, "nameProfileDict"), Sf_Attr(&f, v, "Name"), foe);
	Sf_Ask(&f, "RaceMaker", "InitShipIAProfile", "(OO)", foe, Sf_Global(&f, MR, "profileType"));
	PyObject *ship = Sf_Ask(&f, "RaceMaker", "GetRandomShipIAProfile", "(OO)", foe, Sf_Global(&f, MR, "profileType"));
	Sf_Ask(&f, "VehiclesNPC", "SetIAProf", "(OO)", v, ship);
	Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, v, "Name"));
	Sf_SetAttrS(&f, v, "TgType", "green");
	Sf_SetAttrS(&f, v, "EntityList", "Race");
	Sf_SetAttrI(&f, v, "Money", -1);
	Sf_SetAttrI(&f, v, "LaunchTaunts", 0);
	static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
	for (int a = 0; a < 3; a++) {
		Sf_SetAttr(&f, v, ammo[a], Sf_Ask(&f, "SWeap", "GetFAmmo", "(is)", a, "Max"));
	}
	Sdf_AppendName(&f, MR, "racers", v);
	PyObject *found[2] = { v, NULL };
	Sdf_EachInList(&f, "Race", Mr_RedFirst, found);
	if (found[1] || f.failed) {
		return Sf_None(&f);
	}
	PyObject *baton = Sf_Global(&f, MR, "baton");
	if (Sf_True(&f, baton) > 0) {
		PyObject *bp = Sf_Attr(&f, Sf_Global(&f, MR, "baton"), "Pos");
		Mr_GoTo(&f, Sf_Attr(&f, v, "Name"), bp, Sf_Global(&f, MR, "BatonRadius"));
	} else if (!f.failed && Sf_True(&f, Sdf_Usr(&f)) > 0) {
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, v, "Name"), Sf_Attr(&f, Sdf_Usr(&f), "Name"));
	} else if (!f.failed) {
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, v, "Name"), 0);
	}
	return Sf_None(&f);
}

/* the racer with the waypoint marker gets the target "" (racerWithWaypoint of the script is never set) */
static int Mr_Marker(scriptFrame_t *f, PyObject *e, PyObject *name, void *arg) {
	(void)name;
	(void)arg;
	int m = Sf_EqI(f, Sf_Attr(f, e, "RadarShow"), 1);
	if (m > 0) {
		Sf_SetAttrS(f, e, "Target", "");
	}
	return m == 0;
}

/* RespawnPlayer(Usrname, pos, ang): the player's ship where it died, at the last start position when the baton lies
 * there */
static PyObject *MortalRace_RespawnPlayer(PyObject *self, PyObject *args) {
	PyObject *name, *pos, *ang;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &ang)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "RaceMaker");
	PyObject *usr = Sf_Entity(&f, name);
	Sf_Ask(&f, "Vehicles", "ReSpawnShip", "(O)", Sf_Attr(&f, usr, "Name"));
	PyObject *spawnPos = pos, *spawnAng = ang;
	if (Sf_True(&f, Sf_Global(&f, MR, "baton")) > 0) {
		PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, Sf_Global(&f,
			MR, "baton"), "Pos"), pos));
		PyObject *r = Sf_Op(&f, Sf_Global(&f, MR, "BatonRadius"), '*', Sf_Global(&f, MR, "BatonRadius"));
		if (Sf_Compare(&f, d, "<", Sf_ToFloat(&f, r)) > 0) {
			PyObject *i = Mr_PrevPos(&f);
			spawnPos = Sf_Item(&f, Mr_StartPos(&f, i), 0);
			spawnAng = Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Mr_StartPos(&f, i), 1), 0);
		}
	}
	Sf_SetAttr(&f, usr, "Pos", spawnPos);
	Sf_SetAttr(&f, usr, "Ang", spawnAng);
	Sdf_EachInList(&f, "Race", Mr_Marker, NULL);
	return Sf_None(&f);
}

/* PreOnVictory(): the race over, the player safe, the last message, then the score table */
static PyObject *MortalRace_PreOnVictory(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sdf_Call(&f, MR, "EndOfMortalRace", "()");
	Sdf_UsrSafe(&f);
	Sdf_SetS(&f, "OnItemTakenAI", "");
	PyObject *cad = Sdf_Save(&f, "MortalRaceFinishPlayerWinsTextCad");
	Sdf_Remote(&f, "ForceRemoteCom", cad, Sdf_Save(&f, "MortalRaceFinishPlayerWinsTextFoe"), Sf_Mod(&f, "MissionsFuncs",
		"OnSuperDealFinish"),
		Sf_Build(&f, "(iss)", 1, "MortalRace", ""));
	return Sf_None(&f);
}

PyMethodDef g_MortalRaceMethods[] = {
	{ "GetMenuDescription", MortalRace_GetMenuDescription, 1, NULL },
	{ "InitStatus", MortalRace_InitStatus, 1, NULL },
	{ "Init", MortalRace_Init, 1, NULL },
	{ "ReleaseBaton", MortalRace_ReleaseBaton, 1, NULL },
	{ "LostBatonMessage", MortalRace_LostBatonMessage, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartMortalRace", MortalRace_CrazyGamblerStartMortalRace, 1, NULL },
	{ "SetVulnerable", Sd_SetVulnerable, 1, NULL },
	{ "CreateBaton", MortalRace_CreateBaton, 1, NULL },
	{ "TakeBaton", MortalRace_TakeBaton, 1, NULL },
	{ "InitRace", (PyCFunction)(void (*)(void))MortalRace_InitRace, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateFirstWaypoint", MortalRace_CreateFirstWaypoint, 1, NULL },
	{ "OnRaceStart", MortalRace_OnRaceStart, 1, NULL },
	{ "RaceGo", MortalRace_RaceGo, 1, NULL },
	{ "CheckShipsState", MortalRace_CheckShipsState, 1, NULL },
	{ "OnPositionReached", MortalRace_OnPositionReached, 1, NULL },
	{ "OnCheckPoint", MortalRace_OnCheckPoint, 1, NULL },
	{ "Restart", MortalRace_Restart, 1, NULL },
	{ "RestartRace", Sd_Restart, 1, NULL },
	{ "MoveWaypoint", MortalRace_MoveWaypoint, 1, NULL },
	{ "EndOfMortalRace", MortalRace_EndOfMortalRace, 1, NULL },
	{ "MoveRacersToTraffic", MortalRace_MoveRacersToTraffic, 1, NULL },
	{ "MoveRacersToPos", MortalRace_MoveRacersToPos, 1, NULL },
	{ "RacersAttackShip", MortalRace_RacersAttackShip, 1, NULL },
	{ "OnPlayerDeath", MortalRace_OnPlayerDeath, 1, NULL },
	{ "RacerKilled", MortalRace_RacerKilled, 1, NULL },
	{ "RacerDamaged", MortalRace_RacerDamaged, 1, NULL },
	{ "RespawnRacer", MortalRace_RespawnRacer, 1, NULL },
	{ "RespawnPlayer", MortalRace_RespawnPlayer, 1, NULL },
	{ "PreOnVictory", MortalRace_PreOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL }
};
