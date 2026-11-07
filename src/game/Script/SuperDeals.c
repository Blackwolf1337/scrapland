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

#include "scrap/input.h"
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

/* h.var = i, h.var = "text" of a handle value */
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

/* ---- the lines of the modes still on Sf_ frames (until each mode is native) ---- */

/* Scrap.GetSaveVar(name), int(Scrap.GetSaveVar(name)), Scrap.SetSaveVar(name, "text") */
static PyObject *Sdf_Save(scriptFrame_t *f, const char *name) {
	return Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", name);
}

static PyObject *Sdf_SaveInt(scriptFrame_t *f, const char *name) {
	return Sf_ToInt(f, Sdf_Save(f, name));
}

static void Sdf_SetSave(scriptFrame_t *f, const char *name, const char *text) {
	Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", name, text);
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

static int Sdf_Text(scriptFrame_t *f, PyObject *cad) {
	int t = Sf_True(f, cad);
	return t > 0 ? Sf_Compare(f, cad, "!=", Sf_Str(f, "")) : t;
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

/* spawnParams = OutMap.GetSpawnPos(); e = Scrap.CreateEntity(name, spawnParams[0][0], ..., "Car") (spawnParams in
 * *sp for the angle) */
static PyObject *Sdf_SpawnCar(scriptFrame_t *f, PyObject *name, PyObject **sp) {
	*sp = Sf_Ask(f, "OutMap", "GetSpawnPos", "()");
	return Sdf_Create(f, name, Sf_Item(f, *sp, 0), "Car");
}

/* the same with e.Ang = (0, spawnParams[1], 0) right after */
static PyObject *Sdf_SpawnCarAng(scriptFrame_t *f, PyObject *name) {
	PyObject *sp;
	PyObject *e = Sdf_SpawnCar(f, name, &sp);
	Sdf_Ang(f, e, Sf_Item(f, sp, 1));
	return e;
}

/* prefix + `i` */
static PyObject *Sdf_Numbered(scriptFrame_t *f, const char *prefix, long i) {
	return Sf_Cat(f, "sO", prefix, Sf_ReprI(f, i));
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

/* RacerScorer.InitMissionScorer(a, b, c); Menu.MissionScorerCall = RacerScorer.MissionScorer;
 * RacerScorer.MissionScorer(0) */
static void Sdf_ScorerOn(scriptFrame_t *f, PyObject *a, PyObject *b, PyObject *c) {
	Sf_Ask(f, "RacerScorer", "InitMissionScorer", "(OOO)", a, b, c);
	Sf_SetMod(f, "Menu", "MissionScorerCall", Sf_Mod(f, "RacerScorer", "MissionScorer"));
	Sf_Ask(f, "RacerScorer", "MissionScorer", "(i)", 0);
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

/* if (cad and cad != ""): Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params) else:
 * apply(func, params) */
static void Sdf_End(scriptFrame_t *f, const char *how, PyObject *cad, PyObject *who, PyObject *func, PyObject *params) {
	int t = Sdf_Text(f, cad);
	if (t > 0) {
		Sdf_Remote(f, how, cad, who, func, params);
	} else if (t == 0) {
		Sf_CallArgs(f, func, Script_Ref(params));
	}
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

/* if (Parking.PlayerShip): Parking.PlayerShip.Invulnerable = 1 */
static void Sdf_ShipSafe(scriptFrame_t *f) {
	if (Sf_True(f, Sf_Mod(f, "Parking", "PlayerShip")) > 0) {
		Sf_SetAttrI(f, Sf_Mod(f, "Parking", "PlayerShip"), "Invulnerable", 1);
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

/* cam = Scrap.GetEntity(Scrap.GetCam(0)); if (cam): cam.Target = winner; MissionsFuncs.ActivateTarget(
 * Scrap.GetEntity(winner), 0) */
static void Sdf_CamOnWinner(scriptFrame_t *f, PyObject *winner) {
	PyObject *cam = Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
	if (Sf_True(f, cam) > 0) {
		Sf_SetAttr(f, cam, "Target", winner);
		Sf_Ask(f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Entity(f, winner), 0);
	}
}

/* import RaceMaker, VehiclesNPC; the AI profile of the ship from the type and profile save variables */
static void Sdf_Profile(scriptFrame_t *f, PyObject *ship, const char *typeVar, const char *profVar) {
	Sf_Import(f, "RaceMaker");
	Sf_Import(f, "VehiclesNPC");
	PyObject *type = Sdf_Save(f, typeVar);
	PyObject *prof = Sdf_Save(f, profVar);
	Sf_Ask(f, "RaceMaker", "InitShipIAProfile", "(OO)", type, prof);
	PyObject *s = Sf_Ask(f, "RaceMaker", "GetRandomShipIAProfile", "(OO)", type, prof);
	Sf_Ask(f, "VehiclesNPC", "SetIAProf", "(OO)", ship, s);
}

/* `for name in list: e = Scrap.GetEntity(name); if (e): e.TgType = tg; e.EntityList = list` (tg NULL leaves it) */
static void Sdf_MarkShips(scriptFrame_t *f, PyObject *names, const char *tg, const char *list) {
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *n = Sf_Next(f, names, i);
		if (!n) {
			break;
		}
		PyObject *e = Sf_Entity(f, n);
		if (Sf_True(f, e) > 0) {
			if (tg) {
				Sf_SetAttrS(f, e, "TgType", tg);
			}
			Sf_SetAttrS(f, e, "EntityList", list);
		}
		Sf_Release(f, mark);
	}
}

/* `for zoneName in zonesStates.keys(): e = Scrap.GetEntity(zoneName); if (e): e.RadarShow = 0` */
static void Sdf_HideZones(scriptFrame_t *f) {
	PyObject *keys = Sf_Method(f, Sf_Global(f, DOM, "zonesStates"), "keys", "()");
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *z = Sf_Next(f, keys, i);
		if (!z) {
			break;
		}
		PyObject *e = Sf_Entity(f, z);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrI(f, e, "RadarShow", 0);
		}
		Sf_Release(f, mark);
	}
}

/* usr = Scrap.GetEntity(name); Vehicles.ReSpawnShip(name); usr.Pos = OutMap.GetSpawnPos()[0];
 * usr.Ang = 0, OutMap.GetSpawnPos()[1], 0 (CTF and Domination) */
static PyObject *Sdf_RespawnUsr(scriptFrame_t *f, PyObject *name) {
	Sf_Import(f, "Parking");
	Sf_Import(f, "Vehicles");
	Sf_Import(f, "OutMap");
	PyObject *usr = Sf_Entity(f, name);
	Sf_Ask(f, "Vehicles", "ReSpawnShip", "(O)", name);
	Sf_SetAttr(f, usr, "Pos", Sf_Item(f, Sf_Ask(f, "OutMap", "GetSpawnPos", "()"), 0));
	Sdf_Ang(f, usr, Sf_Item(f, Sf_Ask(f, "OutMap", "GetSpawnPos", "()"), 1));
	return usr;
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

static PyObject *Sdf_InitStatus(PyObject *args, const char *mod) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sdf_Status(&f, mod, NULL);
	return Sf_None(&f);
}

/* SetInvulnerableStatus(entity): the entity and the player's ship (TeamDeathMatch tests the entity first) */
static PyObject *Sdf_SetInvulnerableStatus(PyObject *args, byte test) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	if (!test || Sf_True(&f, e) > 0) {
		Sf_SetAttrI(&f, e, "Invulnerable", 1);
	}
	Sdf_ShipSafe(&f);
	return Sf_None(&f);
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

/* PreRestartMenu() and PreOnVictory() of CTF and Domination: the player safe and the last message of the deal, then
 * the score table (who NULL: the crazy gambler speaks) */
static PyObject *Sdf_PreEnd(PyObject *args, byte win, const char *cadVar, const char *whoVar, const char *mode,
	const char *retry) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sdf_ShipSafe(&f);
	if (win) {
		Sdf_UsrSafe(&f);
	} else {
		PyObject *usr = Sdf_Usr(&f);
		if (Sf_True(&f, usr) > 0) {
			Sf_SetAttrI(&f, usr, "Invulnerable", 1);
		}
	}
	PyObject *cad = Sdf_Save(&f, cadVar);
	PyObject *who = whoVar ? Sdf_Save(&f, whoVar) : Sf_Str(&f, CG);
	Sdf_Remote(&f, "ForceRemoteCom", cad, who, Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish"),
		Sf_Build(&f, "(iss)", win, mode, retry));
	return Sf_None(&f);
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

/* the end of a deathmatch (and of a team deathmatch): the lines both share after the frag that ends it */
static void Dmf_Finish(scriptFrame_t *f) {
	Sf_SetMod(f, "Chars", "OnMainCharDeath", Py_None);
	Sf_SetMod(f, "Vehicles", "OnDeathCallback", Py_None);
	Sf_SetMod(f, "Parking", "SwitchCharMissionCallback", Py_None);
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

static PyObject *Versus_GetMenuDescription(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_MenuDescription(args, "SuperDeal_Versus_Description", "CombatDeaths");
}

static PyObject *Versus_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* Init(LevelPath): the camera of the end, the deaths to reach, the enemy, the start when the player boards */
static PyObject *Versus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Vehicles");
	Sdf_Camera(&f, "CA");
	Sf_SetMod(&f, "Vehicles", "DelPlayerShipOnDeath", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, VS, "combatDeaths", Sdf_SaveInt(&f, "CombatDeaths"));
	Sf_Import(&f, "Chars");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, VS, "OnPlayerDeath"));
	Sdf_Call(&f, VS, "CreateEnemy", "()");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, VS, "CrazyGamblerStartCombat"));
	return Sf_None(&f);
}

/* CreateEnemy(markAsEnemy = 0): the enemy at a spawn point with the ship of its type and profile; marked as the
 * target, else invulnerable until the start */
static PyObject *Versus_CreateEnemy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "markAsEnemy", NULL };
	PyObject *mark = NULL, *pos[2];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &mark)) {
		return NULL;
	}
	if (!mark) {
		mark = Sf_Int(&f, 0);
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "OutMap");
	Sf_Unpack(&f, Sf_Ask(&f, "OutMap", "GetSpawnPos", "()"), 2, pos);
	PyObject *e = Sdf_Create(&f, Sf_Str(&f, "Enemy"), pos[0], "Car");
	Sdf_Ang(&f, e, pos[1]);
	Sf_SetGlobal(&f, VS, "enemyName", Sf_Attr(&f, e, "Name"));
	Sf_SetGlobal(&f, VS, "combatEnemyType", Sdf_Save(&f, "CombatEnemyType"));
	Sf_SetGlobal(&f, VS, "combatEnemyProfile", Sdf_Save(&f, "CombatEnemyProfile"));
	PyObject *type = Sf_Global(&f, VS, "combatEnemyType");
	Sf_Ask(&f, "RaceMaker", "InitShipIAProfile", "(OO)", type, Sf_Global(&f, VS, "combatEnemyProfile"));
	type = Sf_Global(&f, VS, "combatEnemyType");
	PyObject *ship = Sf_Cat(&f, "sOsO", "S", type, "1_", Sf_Global(&f, VS, "combatEnemyProfile"));
	Sf_Ask(&f, "VehiclesNPC", "SetIAProf", "(OO)", e, ship);
	int m = Sf_EqI(&f, mark, 1);
	if (m > 0) {
		Sf_SetAttrS(&f, e, "TgType", "red");
		Sf_SetAttrS(&f, e, "EntityList", "Enemies");
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	} else if (m == 0) {
		Sf_SetAttrI(&f, e, "Invulnerable", 1);
	}
	return Sf_None(&f);
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first round, a later one
 * starts after 3 s */
static PyObject *Versus_CrazyGamblerStartCombat(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	int first = Sf_EqI(&f, Sdf_SaveInt(&f, "NumEnemyDeaths"), 0);
	if (first > 0) {
		first = Sf_EqI(&f, Sdf_SaveInt(&f, "NumPlayerDeaths"), 0);
	}
	if (first > 0) {
		PyObject *cad = Sdf_Save(&f, "CombatStartTextCad");
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(OsO)", cad, CG, Sf_Global(&f, VS, "StartCombat"));
	} else if (first == 0) {
		Sdf_Later(&f, Sf_Float(&f, 3.0), VS, "StartCombat", Sf_Build(&f, "()"));
	}
	return Sf_None(&f);
}

/* the text of the enemy's type (auxText of the script, which is never used): CharLangTrans[type] or the type */
static void Vs_AuxText(scriptFrame_t *f) {
	PyObject *trans = Sf_Global(f, VS, "CharLangTrans");
	PyObject *has = Sf_Attr(f, trans, "has_key");
	if (Sf_True(f, Sf_Call(f, has, "(O)", Sf_Global(f, VS, "combatEnemyType"))) > 0) {
		Sf_GetItem(f, Sf_Global(f, VS, "CharLangTrans"), Sf_Global(f, VS, "combatEnemyType"));
	} else {
		Sf_Global(f, VS, "combatEnemyType");
	}
}

/* StartCombat(): the deaths counted, the scorer, the enemy as the red target */
static PyObject *Versus_StartCombat(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "MissionsFuncs");
	PyObject *e = Sf_Entity(&f, Sf_Global(&f, VS, "enemyName"));
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Sf_Global(&f, VS, "OnEnemyDeath"));
	PyObject *a = Sdf_SaveInt(&f, "NumEnemyDeaths");
	PyObject *b = Sdf_SaveInt(&f, "NumPlayerDeaths");
	Sdf_ScorerOn(&f, a, b, Sf_Repr(&f, Sf_Global(&f, VS, "combatDeaths")));
	Sf_SetAttrS(&f, e, "TgType", "red");
	Sf_SetAttrS(&f, e, "EntityList", "Enemies");
	Sf_SetAttrI(&f, e, "Invulnerable", 0);
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	Sf_SetGlobal(&f, VS, "combatEnemyType", Sdf_Save(&f, "CombatEnemyType"));
	Vs_AuxText(&f);
	return Sf_None(&f);
}

/* save variable name = `int(Scrap.GetSaveVar(name)) + 1` */
static void Vs_Count(scriptFrame_t *f, const char *name) {
	Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", name, Sf_Repr(f, Sf_OpI(f, Sdf_SaveInt(f, name), '+', 1)));
}

/* RacerScorer.UpdateMissionScorer(0, int(NumEnemyDeaths), int(NumPlayerDeaths)) */
static void Vs_Update(scriptFrame_t *f) {
	PyObject *a = Sdf_SaveInt(f, "NumEnemyDeaths");
	Sdf_Update(f, a, Sdf_SaveInt(f, "NumPlayerDeaths"));
}

/* OnEnemyDeath(LooserName, WinnerName): the enemy died; the player wins at combatDeaths, else the enemy comes back
 * after 5 s */
static PyObject *Versus_OnEnemyDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	if (Sf_Compare(&f, loser, "==", Sf_Global(&f, VS, "enemyName")) > 0) {
		Vs_Count(&f, "NumEnemyDeaths");
		int end = Sf_Compare(&f, Sdf_SaveInt(&f, "NumEnemyDeaths"), "==", Sf_Global(&f, VS, "combatDeaths"));
		if (end > 0) {
			Sf_Import(&f, "Menu");
			Sf_Import(&f, "Chars");
			Sf_Import(&f, "Vehicles");
			Sf_Import(&f, "Parking");
			Sdf_ScorerOff(&f);
			Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "NumEnemyDeaths");
			Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "NumPlayerDeaths");
			Sdf_SetSave(&f, "PlayerWinsCombat", "1");
			Dmf_Finish(&f);
			PyObject *cad = Sdf_Save(&f, "CombatFinishPlayerWinsTextCad");
			Sdf_UsrSafe(&f);
			int t = Sdf_Text(&f, cad);
			if (t > 0) {
				PyObject *fn = Sf_Mod(&f, "Speech", "ForceRemoteCom");
				PyObject *a = Sf_Build(&f, "(OsiO(iss))", cad, CG, 1, Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish"),
					1, "Versus", "");
				Sf_Later(&f, Sf_Int(&f, 2), fn, a, NULL);
			} else if (t == 0) {
				PyObject *fn = Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish");
				Sf_Later(&f, Sf_Float(&f, 5.0), fn, Sf_Build(&f, "(iss)", 1, "Versus", ""), NULL);
			}
		} else if (end == 0) {
			Sf_Import(&f, "RacerScorer");
			Vs_Update(&f);
			Sdf_Later(&f, Sf_Float(&f, 5.0), VS, "RespawnEnemy", Sf_Build(&f, "()"));
			Sdf_Del(&f, Sf_Str(&f, "Enemy"));
		}
	}
	return Sf_None(&f);
}

/* RespawnEnemy(): the enemy again, as the target, and the next round */
static PyObject *Versus_RespawnEnemy(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sdf_Call(&f, VS, "CreateEnemy", "(i)", 1);
	Sdf_Call(&f, VS, "StartCombat", "()");
	return Sf_None(&f);
}

/* OnPlayerDeath(LooserName, WinnerName): the enemy relaxes; the enemy wins at combatDeaths, else the player comes
 * back after 5 s where he died */
static PyObject *Versus_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	PyObject *enemy = Sf_Entity(&f, Sf_Global(&f, VS, "enemyName"));
	if (Sf_True(&f, enemy) > 0) {
		Sf_SetAttrS(&f, enemy, "EntityList", "");
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, enemy, "Name"), 0);
	}
	Vs_Count(&f, "NumPlayerDeaths");
	int end = Sf_Compare(&f, Sdf_SaveInt(&f, "NumPlayerDeaths"), "==", Sf_Global(&f, VS, "combatDeaths"));
	if (end > 0) {
		Sf_Import(&f, "Menu");
		Sdf_ScorerOff(&f);
		Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "NumEnemyDeaths");
		Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "NumPlayerDeaths");
		Sdf_CamOnWinner(&f, winner);
		PyObject *cad = Sdf_Save(&f, "CombatFinishPlayerLoosesTextCad");
		int t = Sdf_Text(&f, cad);
		if (t > 0) {
			Sdf_Later(&f, Sf_Int(&f, 2), VS, "EnemyWins", Sf_Build(&f, "(O)", cad));
		} else if (t == 0) {
			PyObject *fn = Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish");
			Sf_Later(&f, Sf_Int(&f, 5), fn, Sf_Build(&f, "(iss)", 0, "Versus", "Versus.RestartCombat"), NULL);
		}
	} else if (end == 0) {
		Sf_Import(&f, "RacerScorer");
		PyObject *e = Sf_Entity(&f, loser);
		PyObject *pos = Sf_Attr(&f, e, "Pos");
		Sdf_Later(&f, Sf_Float(&f, 5.0), VS, "RespawnPlayer", Sf_Build(&f, "(OOO)", loser, pos, Sf_Attr(&f, e, "Ang")));
		Vs_AuxText(&f);
		Vs_Update(&f);
	}
	return Sf_None(&f);
}

/* EnemyWins(cad): the enemy's message, then the score table */
static PyObject *Versus_EnemyWins(PyObject *self, PyObject *args) {
	PyObject *cad;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cad)) {
		return NULL;
	}
	PyObject *head = Sdf_Save(&f, "CombatEnemyTypeHead");
	PyObject *fn = Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish");
	Sf_Ask(&f, "Speech", "AddRemoteCom", "(OOO(iss))", cad, head, fn, 0, "Versus", "Versus.RestartCombat");
	return Sf_None(&f);
}

/* RespawnPlayer(Usrname, pos, ang): the player's ship again where it died; the enemy attacks it */
static PyObject *Versus_RespawnPlayer(PyObject *self, PyObject *args) {
	PyObject *name, *pos, *ang;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &ang)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "OutMap");
	PyObject *usr = Sf_Entity(&f, name);
	Sf_Ask(&f, "Vehicles", "ReSpawnShip", "(O)", Sf_Attr(&f, usr, "Name"));
	Sf_SetAttr(&f, usr, "Pos", pos);
	Sf_SetAttr(&f, usr, "Ang", ang);
	PyObject *enemy = Sf_Entity(&f, Sf_Global(&f, VS, "enemyName"));
	if (Sf_True(&f, enemy) > 0) {
		Sf_SetAttrS(&f, enemy, "TgType", "red");
		Sf_SetAttrS(&f, enemy, "EntityList", "Enemies");
		PyObject *en = Sf_Global(&f, VS, "enemyName");
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOO)", 2, en, Sf_Attr(&f, usr, "Name"));
	}
	return Sf_None(&f);
}

/* RestartCombat(id, control): the deaths at 0 and the level again */
static PyObject *Versus_RestartCombat(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sdf_SetSave(&f, "NumEnemyDeaths", "0");
	Sdf_SetSave(&f, "NumPlayerDeaths", "0");
	Sf_Ask(&f, "SaveGame", "Restart", "()");
	return Sf_None(&f);
}

PyMethodDef g_VersusMethods[] = {
	{ "GetMenuDescription", Versus_GetMenuDescription, 1, NULL },
	{ "InitStatus", Versus_InitStatus, 1, NULL },
	{ "Init", Versus_Init, 1, NULL },
	{ "CreateEnemy", (PyCFunction)(void (*)(void))Versus_CreateEnemy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartCombat", Versus_CrazyGamblerStartCombat, 1, NULL },
	{ "StartCombat", Versus_StartCombat, 1, NULL },
	{ "OnEnemyDeath", Versus_OnEnemyDeath, 1, NULL },
	{ "RespawnEnemy", Versus_RespawnEnemy, 1, NULL },
	{ "OnPlayerDeath", Versus_OnPlayerDeath, 1, NULL },
	{ "EnemyWins", Versus_EnemyWins, 1, NULL },
	{ "RespawnPlayer", Versus_RespawnPlayer, 1, NULL },
	{ "RestartCombat", Versus_RestartCombat, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- TeamDeathMatch: the player's team against the enemies in the QF, CombatDeaths frags of a team win ---- */

static PyObject *TeamDeathMatch_GetMenuDescription(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_MenuDescription(args, "SuperDeal_TeamDeathMatch_Description", "CombatDeaths");
}

static PyObject *TeamDeathMatch_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_InitStatus(args, TDM);
}

/* Init(LevelPath): the camera of the end, the death of the player, both teams, the start when the player boards */
static PyObject *TeamDeathMatch_Init(PyObject *self, PyObject *args) {
	static const char *const libs[] = { "SuperDeal_Fourth", "SuperDeal_Eleventh", "Mission_BankersCombat", NULL };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sdf_CameraFor(&f, libs, "QF");
	Sf_Import(&f, "Chars");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, TDM, "OnPlayerDeath"));
	Sdf_Call(&f, TDM, "CreateFriends", "()");
	Sdf_Call(&f, TDM, "CreateEnemies", "()");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, TDM, "CrazyGamblerStartCombat"));
	return Sf_None(&f);
}

static PyObject *TeamDeathMatch_SetInvulnerableStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_SetInvulnerableStatus(args, 1);
}

/* SetFriendProfile(friend): the AI of a ship of the player's team */
static PyObject *TeamDeathMatch_SetFriendProfile(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sdf_Profile(&f, e, "CombatFriendType", "CombatFriendProfile");
	return Sf_None(&f);
}

/* CreateFriends() and CreateEnemies(): <prefix>1 to <prefix>N of a team at spawn points, invulnerable until the
 * start, their frags at 0 */
static PyObject *Tdm_CreateTeam(PyObject *args, const char *numVar, const char *fragsVar, const char *prefix,
	const char *profile, const char *list) {
	long n = 0;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *num = Sdf_SaveInt(&f, numVar);
	Sf_SetGlobal(&f, TDM, fragsVar, Sf_Int(&f, 0));
	Sf_Check(&f, f.failed || Script_Range(num, &n));
	for (long i = 1; i <= n && !f.failed; i++) {
		int mark = f.n;
		PyObject *e = Sdf_SpawnCarAng(&f, Sdf_Numbered(&f, prefix, i));
		Sdf_Call(&f, TDM, profile, "(O)", e);
		Sdf_AppendName(&f, TDM, list, e);
		Sf_SetAttrI(&f, e, "Invulnerable", 1);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *TeamDeathMatch_CreateFriends(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_CreateTeam(args, "CombatNumFriends", "friendsFrags", "Friend", "SetFriendProfile", "friends");
}

static PyObject *TeamDeathMatch_CreateEnemies(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_CreateTeam(args, "CombatNumEnemies", "enemiesFrags", "Enemy", "SetEnemyProfile", "enemies");
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first combat */
static PyObject *TeamDeathMatch_CrazyGamblerStartCombat(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, TDM, "OnVehicle"));
	int first = Sf_EqI(&f, Sf_Global(&f, TDM, "enemiesFrags"), 0);
	if (first > 0) {
		first = Sf_EqI(&f, Sf_Global(&f, TDM, "friendsFrags"), 0);
	}
	if (first > 0) {
		PyObject *cad = Sdf_Save(&f, "CombatStartTextCad");
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(OsO)", cad, CG, Sf_Global(&f, TDM, "StartCombat"));
	} else if (first == 0) {
		Sdf_Call(&f, TDM, "StartCombat", "()");
	}
	return Sf_None(&f);
}

/* `for name in names: e = Scrap.GetEntity(name); e.EntityList = list; e.Invulnerable = 0` */
static void Tdm_Release(scriptFrame_t *f, PyObject *names, const char *list) {
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *n = Sf_Next(f, names, i);
		if (!n) {
			break;
		}
		PyObject *e = Sf_Entity(f, n);
		Sf_SetAttrS(f, e, "EntityList", list);
		Sf_SetAttrI(f, e, "Invulnerable", 0);
		Sf_Release(f, mark);
	}
}

/* StartCombat(): the deaths of both lists counted, the scorer, the teams in their lists */
static PyObject *TeamDeathMatch_StartCombat(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath");
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath");
	Sdf_ScorerOn(&f, Sf_Int(&f, 0), Sf_Int(&f, 0), Sdf_Save(&f, "CombatDeaths"));
	Tdm_Release(&f, Sf_Global(&f, TDM, "enemies"), "Enemies");
	Tdm_Release(&f, Sf_Global(&f, TDM, "friends"), "Friends");
	return Sf_None(&f);
}

/* the end of the team combat after the frag that won it: OnEnemyDeath (the player's team wins) and OnFriendDeath */
static void Tdm_End(scriptFrame_t *f, byte win) {
	Sf_Import(f, "Menu");
	Sf_Import(f, "Chars");
	Sf_Import(f, "Vehicles");
	Sf_Import(f, "Parking");
	Sdf_SetSave(f, "PlayerWinsCombat", win ? "1" : "0");
	PyObject *cad = Sdf_Save(f, win ? "CombatFinishPlayerWinsTextCad" : "CombatFinishPlayerLoosesTextCad");
	PyObject *func = Sf_Mod(f, "MissionsFuncs", "OnSuperDealFinish");
	Sdf_ScorerOff(f);
	Sf_Ask(f, "MissionsFuncs", "SetInvulnerableShipsInList", "(s)", "Enemies");
	Sf_Ask(f, "MissionsFuncs", "SetInvulnerableShipsInList", "(s)", "Friends");
	Sf_Ask(f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath2");
	Sf_Ask(f, "Scrap", "EntityListSet", "(sss)", "Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath2");
	Sf_SetMod(f, "Chars", "OnMainCharDeath", Sf_Global(f, TDM, "OnPlayerDeath2"));
	Sf_SetMod(f, "Parking", "SwitchCharMissionCallback", Py_None);
	int keep = Sf_True(f, Sf_Global(f, TDM, "KeepPlaying"));
	if (keep == 0) {
		Sdf_UsrSafe(f);
		PyObject *params = win ? Sf_Build(f, "(iss)", 1, "TeamDeathMatch", "") :
			Sf_Build(f, "(iss)", 0, "TeamDeathMatch", "TeamDeathMatch.RestartCombat");
		Sdf_End(f, "ForceRemoteCom", cad, Sf_Str(f, CG), func, params);
	} else if (keep > 0) {
		Sdf_End(f, "ForceRemoteCom", cad, Sf_Str(f, CG), Sf_Global(f, TDM, "KeepPlaying"), Sf_Build(f, "()"));
	}
}

/* OnEnemyDeath and OnFriendDeath(LooserName, WinnerName): a ship of a team died; a frag of the other team ends the
 * combat at CombatDeaths, else the score; a new ship comes after 5 s while the team is short */
static PyObject *Tdm_Death(PyObject *args, byte enemy) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	const char *list = enemy ? "enemies" : "friends", *frags = enemy ? "friendsFrags" : "enemiesFrags";
	Sf_Import(&f, "RacerScorer");
	PyObject *usr = Sdf_Usr(&f);
	if (enemy && Sdf_UsrIs(&f, usr, loser) != 0) {
		return Sf_None(&f);
	}
	Sdf_RemoveIn(&f, TDM, list, loser);
	PyObject *deaths = Sdf_SaveInt(&f, "CombatDeaths");
	int end = 0;
	if (Sf_True(&f, winner) > 0) {
		int u = Sdf_UsrIs(&f, usr, winner);
		if (u > 0) {
			end = Sf_Compare(&f, Sdf_Add(&f, TDM, frags, 1), "==", deaths);
		} else if (u == 0) {
			PyObject *e = Sf_Entity(&f, winner);
			if (Sf_EqS(&f, Sf_Attr(&f, e, "EntityList"), enemy ? "Friends" : "Enemies") > 0) {
				end = Sf_Compare(&f, Sdf_Add(&f, TDM, frags, 1), "==", deaths);
			}
		}
	}
	if (end > 0) {
		Tdm_End(&f, enemy);
	} else if (end == 0) {
		PyObject *a = Sf_Global(&f, TDM, "friendsFrags");
		Sdf_Update(&f, a, Sf_Global(&f, TDM, "enemiesFrags"));
	}
	PyObject *len = Sf_Int(&f, Sf_Len(&f, Sf_Global(&f, TDM, list)));
	if (Sf_Compare(&f, len, "<", Sdf_SaveInt(&f, enemy ? "CombatNumEnemies" : "CombatNumFriends")) > 0) {
		Sdf_Later(&f, Sf_Float(&f, 5.0), TDM, enemy ? "RespawnEnemy" : "RespawnFriend", Sf_Build(&f, "(O)", loser));
		if (enemy) {
			Sdf_Del(&f, loser);
		}
	}
	return Sf_None(&f);
}

static PyObject *TeamDeathMatch_OnEnemyDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Death(args, 1);
}

static PyObject *TeamDeathMatch_OnFriendDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Death(args, 0);
}

/* OnEnemyDeath2 and OnFriendDeath2(LooserName, WinnerName): after the end, the dead ship only comes back */
static PyObject *Tdm_Death2(PyObject *args, byte enemy) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	const char *list = enemy ? "enemies" : "friends";
	Sdf_RemoveIn(&f, TDM, list, loser);
	PyObject *len = Sf_Int(&f, Sf_Len(&f, Sf_Global(&f, TDM, list)));
	if (Sf_Compare(&f, len, "<", Sdf_SaveInt(&f, enemy ? "CombatNumEnemies" : "CombatNumFriends")) > 0) {
		Sdf_Later(&f, Sf_Float(&f, 5.0), TDM, enemy ? "RespawnEnemy" : "RespawnFriend", Sf_Build(&f, "(O)", loser));
		if (enemy) {
			Sdf_Del(&f, loser);
		}
	}
	return Sf_None(&f);
}

static PyObject *TeamDeathMatch_OnEnemyDeath2(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Death2(args, 1);
}

static PyObject *TeamDeathMatch_OnFriendDeath2(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Death2(args, 0);
}

/* RespawnEnemy(enemyName) and RespawnFriend(friendName): the ship again at a spawn point in its team, unless the list
 * is full */
static PyObject *Tdm_Respawn(PyObject *args, const char *entityList, const char *numVar, const char *profile,
	const char *list) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *num = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", entityList, "Num");
	if (Sf_Compare(&f, num, "==", Sdf_SaveInt(&f, numVar)) != 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sdf_SpawnCarAng(&f, name);
	Sf_SetAttrS(&f, e, "EntityList", entityList);
	Sdf_Call(&f, TDM, profile, "(O)", e);
	Sdf_AppendName(&f, TDM, list, e);
	return Sf_None(&f);
}

static PyObject *TeamDeathMatch_RespawnEnemy(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Respawn(args, "Enemies", "CombatNumEnemies", "SetEnemyProfile", "enemies");
}

static PyObject *TeamDeathMatch_RespawnFriend(PyObject *self, PyObject *args) {
	(void)self;
	return Tdm_Respawn(args, "Friends", "CombatNumFriends", "SetFriendProfile", "friends");
}

/* OnPlayerDeath(LooserName, WinnerName): a frag of the enemies ends the combat at CombatDeaths; the player comes back
 * after 5 s */
static PyObject *TeamDeathMatch_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	int in = Sf_In(&f, winner, Sf_Global(&f, TDM, "enemies"));
	if (in > 0) {
		Sdf_Add(&f, TDM, "enemiesFrags", 1);
	}
	int end = in;
	if (end > 0) {
		end = Sf_Compare(&f, Sf_Global(&f, TDM, "enemiesFrags"), ">=", Sdf_SaveInt(&f, "CombatDeaths"));
	}
	if (end > 0) {
		Sf_Import(&f, "Menu");
		Sf_Import(&f, "Chars");
		Sf_Import(&f, "Vehicles");
		Sf_Import(&f, "Parking");
		Sdf_SetSave(&f, "PlayerWinsCombat", "0");
		PyObject *cad = Sdf_Save(&f, "CombatFinishPlayerLoosesTextCad");
		Sdf_ScorerOff(&f);
		Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, TDM, "OnPlayerDeath2"));
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
		int keep = Sf_True(&f, Sf_Global(&f, TDM, "KeepPlaying"));
		if (keep == 0) {
			Sdf_CamOnWinner(&f, winner);
			Sdf_End(&f, "AddRemoteCom", cad, Sf_Str(&f, CG), Sf_Mod(&f, "MissionsFuncs", "OnSuperDealFinish"),
				Sf_Build(&f, "(iss)", 0, "TeamDeathMatch", "TeamDeathMatch.RestartCombat"));
		} else if (keep > 0) {
			Sdf_End(&f, "AddRemoteCom", cad, Sf_Str(&f, CG), Sf_Global(&f, TDM, "KeepPlaying"), Sf_Build(&f, "()"));
		}
	} else if (end == 0) {
		PyObject *a = Sf_Global(&f, TDM, "friendsFrags");
		Sdf_Update(&f, a, Sf_Global(&f, TDM, "enemiesFrags"));
	}
	Sdf_Later(&f, Sf_Float(&f, 5.0), TDM, "RespawnPlayer", Sf_Build(&f, "(O)", loser));
	return Sf_None(&f);
}

/* OnPlayerDeath2(LooserName, WinnerName): after the end the player only comes back */
static PyObject *TeamDeathMatch_OnPlayerDeath2(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sdf_Later(&f, Sf_Float(&f, 5.0), TDM, "RespawnPlayer", Sf_Build(&f, "(O)", loser));
	return Sf_None(&f);
}

/* RestartCombat(id, control): the frags at 0 and the level again */
static PyObject *TeamDeathMatch_RestartCombat(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sf_SetGlobal(&f, TDM, "enemiesFrags", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, TDM, "friendsFrags", Sf_Int(&f, 0));
	Sf_Ask(&f, "SaveGame", "Restart", "()");
	return Sf_None(&f);
}

PyMethodDef g_TeamDeathMatchMethods[] = {
	{ "GetMenuDescription", TeamDeathMatch_GetMenuDescription, 1, NULL },
	{ "InitStatus", TeamDeathMatch_InitStatus, 1, NULL },
	{ "Init", TeamDeathMatch_Init, 1, NULL },
	{ "SetInvulnerableStatus", TeamDeathMatch_SetInvulnerableStatus, 1, NULL },
	{ "SetEnemyProfile", Dm_SetProfile, 1, NULL },
	{ "SetFriendProfile", TeamDeathMatch_SetFriendProfile, 1, NULL },
	{ "CreateFriends", TeamDeathMatch_CreateFriends, 1, NULL },
	{ "CreateEnemies", TeamDeathMatch_CreateEnemies, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartCombat", TeamDeathMatch_CrazyGamblerStartCombat, 1, NULL },
	{ "OnVehicle", Sd_SetVulnerable, 1, NULL },
	{ "StartCombat", TeamDeathMatch_StartCombat, 1, NULL },
	{ "OnEnemyDeath", TeamDeathMatch_OnEnemyDeath, 1, NULL },
	{ "OnEnemyDeath2", TeamDeathMatch_OnEnemyDeath2, 1, NULL },
	{ "RespawnEnemy", TeamDeathMatch_RespawnEnemy, 1, NULL },
	{ "OnFriendDeath", TeamDeathMatch_OnFriendDeath, 1, NULL },
	{ "OnFriendDeath2", TeamDeathMatch_OnFriendDeath2, 1, NULL },
	{ "RespawnFriend", TeamDeathMatch_RespawnFriend, 1, NULL },
	{ "RespawnPlayer", Sd_RespawnPlayer, 1, NULL },
	{ "OnPlayerDeath", TeamDeathMatch_OnPlayerDeath, 1, NULL },
	{ "OnPlayerDeath2", TeamDeathMatch_OnPlayerDeath2, 1, NULL },
	{ "RestartCombat", TeamDeathMatch_RestartCombat, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- CTF: the flags in the FZ, the team of the player against the enemies' leader, flags flags win ---- */

static PyObject *CTF_GetMenuDescription(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_MenuDescription(args, "SuperDeal_CTF_Description", "CTFFlags");
}

static PyObject *CTF_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_InitStatus(args, CTF);
}

/* Init(LevelPath): the camera of the end, the flags to take, the respawn time, both teams, the start when the player
 * boards */
static PyObject *CTF_Init(PyObject *self, PyObject *args) {
	static const char *const libs[] = { "SuperDeal_Fifth", "SuperDeal_Ninth", NULL };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Chars");
	Sdf_CameraFor(&f, libs, "FZ");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, CTF, "OnPlayerDeath"));
	Sf_SetGlobal(&f, CTF, "flags", Sdf_SaveInt(&f, "CTFFlags"));
	Sf_SetGlobal(&f, CTF, "maxFlags", Sf_OpI(&f, Sf_OpI(&f, Sf_Global(&f, CTF, "flags"), '*', 2), '-', 1));
	Sf_SetGlobal(&f, CTF, "onDeathSpawnTime", Sf_ToFloat(&f, Sdf_Save(&f, "CTFOnDeathSpawnTime")));
	if (Sf_Compare(&f, Sf_Global(&f, CTF, "onDeathSpawnTime"), "<", Sf_Float(&f, 1.0)) > 0) {
		Sf_RaiseText(&f, "Demasiado poco tiempo de respawn");
	}
	Sdf_Call(&f, CTF, "CreateEnemies", "()");
	Sdf_Call(&f, CTF, "CreateFriends", "()");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, CTF, "CrazyGamblerStartCTF"));
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(s)", "models/Misc/Flag/Flag.m3d");
	return Sf_None(&f);
}

/* SetProfile(ship, isEnemy) of CTF and Domination: the AI of a ship of either team from the save variables of the
 * mode */
static PyObject *Sdf_SetProfile(PyObject *args, const char *mode) {
	char type[64], prof[64];
	PyObject *ship, *enemy;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &ship, &enemy)) {
		return NULL;
	}
	int e = Sf_EqI(&f, enemy, 1);
	if (e >= 0) {
		Str_Printf(type, sizeof(type), "%s%sType", mode, e ? "Enemy" : "Friend");
		Str_Printf(prof, sizeof(prof), "%s%sProfile", mode, e ? "Enemy" : "Friend");
		Sdf_Profile(&f, ship, type, prof);
	}
	return Sf_None(&f);
}

static PyObject *CTF_SetProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_SetProfile(args, "CTF");
}

/* CreateEnemies() (Enemy1 to EnemyN) and CreateFriends() (Friend1 to Friend<N-1>, the player is the last): the
 * ships of a team at spawn points (CTF) */
static PyObject *Ctf_CreateTeam(PyObject *args, const char *numVar, long less, const char *prefix, int enemy,
	const char *list) {
	long n = 0;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *num = Sdf_SaveInt(&f, numVar);
	Sf_Check(&f, f.failed || Script_Range(num, &n));
	for (long i = 1; i <= n - less && !f.failed; i++) {
		int mark = f.n;
		PyObject *e = Sdf_SpawnCarAng(&f, Sdf_Numbered(&f, prefix, i));
		Sdf_Call(&f, CTF, "SetProfile", "(Oi)", e, enemy);
		Sdf_AppendName(&f, CTF, list, e);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *CTF_CreateEnemies(PyObject *self, PyObject *args) {
	(void)self;
	return Ctf_CreateTeam(args, "CTFNumEnemies", 0, "Enemy", 1, "enemies");
}

static PyObject *CTF_CreateFriends(PyObject *self, PyObject *args) {
	(void)self;
	return Ctf_CreateTeam(args, "CTFNumFriends", 1, "Friend", 0, "friends");
}

/* CrazyGamblerStartCTF(): the player boarded; the crazy gambler's message starts the CTF */
static PyObject *CTF_CrazyGamblerStartCTF(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Speech");
	Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, CTF, "SetVulnerable"));
	PyObject *cad = Sdf_Save(&f, "CTFStartTextCad");
	Sf_Ask(&f, "Speech", "AddRemoteCom", "(OsO)", cad, CG, Sf_Global(&f, CTF, "StartCTF"));
	return Sf_None(&f);
}

/* StartCTF(): the deaths counted, the scorer, the first flag, the enemies red with the first as the leader, the
 * friends green */
static PyObject *CTF_StartCTF(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "RaceMaker");
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Sf_Global(&f, CTF, "OnVehicleDeathCallback"));
	Sdf_ScorerOn(&f, Sf_Int(&f, 0), Sf_Int(&f, 0), Sf_Repr(&f, Sf_Global(&f, CTF, "flags")));
	Sdf_Call(&f, CTF, "CreateFlag", "()");
	Sdf_MarkShips(&f, Sf_Global(&f, CTF, "enemies"), "red", "CTFEnemies");
	if (Sf_Len(&f, Sf_Global(&f, CTF, "enemies")) > 0) {
		PyObject *e = Sf_Entity(&f, Sf_Item(&f, Sf_Global(&f, CTF, "enemies"), 0));
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrI(&f, e, "LLogicIsLeader", 1);
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
			PyObject *usr = Sdf_Usr(&f);
			if (Sdf_UsrCar(&f, usr) > 0) {
				Sf_SetAttr(&f, usr, "Target", Sf_Attr(&f, e, "Name"));
			}
		}
	}
	Sdf_MarkShips(&f, Sf_Global(&f, CTF, "friends"), "green", "CTFFriends");
	Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sdf_Lang(&f, "SuperDeal_CTF_SystemMessage"));
	return Sf_None(&f);
}

/* OnVehicleDeathCallback(LooserName, WinnerName): the player or the enemies' leader drop the flags they carry; the
 * dead ship of a team comes back after onDeathSpawnTime */
static PyObject *CTF_OnVehicleDeathCallback(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *usr = Sdf_Usr(&f);
	int u = Sdf_UsrIs(&f, usr, loser);
	if (u > 0) {
		Sdf_Call(&f, CTF, "SpawnFlags", "(OO)", Sf_Attr(&f, usr, "Pos"), Sf_Global(&f, CTF, "flagsFriends"));
		Sf_SetGlobal(&f, CTF, "flagsFriends", Sf_Int(&f, 0));
		Sdf_Update(&f, Sf_Global(&f, CTF, "flagsFriends"), Sf_Global(&f, CTF, "flagsEnemies"));
	} else if (u == 0) {
		int en = Sf_In(&f, loser, Sf_Global(&f, CTF, "enemies"));
		if (en > 0) {
			Sf_Method(&f, Sf_Global(&f, CTF, "enemies"), "remove", "(O)", loser);
			PyObject *e = Sf_Entity(&f, loser);
			if (Sf_True(&f, e) > 0) {
				int lead = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicIsLeader"), 1);
				if (lead > 0) {
					Sdf_Call(&f, CTF, "SpawnFlags", "(OO)", Sf_Attr(&f, e, "Pos"), Sf_Global(&f, CTF, "flagsEnemies"));
					Sf_SetGlobal(&f, CTF, "flagsEnemies", Sf_Int(&f, 0));
					Sdf_Update(&f, Sf_Global(&f, CTF, "flagsFriends"), Sf_Global(&f, CTF, "flagsEnemies"));
					Sdf_Later(&f, Sf_Global(&f, CTF, "onDeathSpawnTime"), CTF, "RespawnEnemy", Sf_Build(&f, "(i)", 1));
				} else if (lead == 0) {
					Sdf_Later(&f, Sf_Global(&f, CTF, "onDeathSpawnTime"), CTF, "RespawnEnemy", Sf_Build(&f, "()"));
				}
				Sdf_Del(&f, loser);
			}
		} else if (en == 0 && Sf_In(&f, loser, Sf_Global(&f, CTF, "friends")) > 0) {
			Sf_Method(&f, Sf_Global(&f, CTF, "friends"), "remove", "(O)", loser);
			Sdf_Later(&f, Sf_Global(&f, CTF, "onDeathSpawnTime"), CTF, "RespawnFriend", Sf_Build(&f, "()"));
		}
	}
	return Sf_None(&f);
}

/* OnPlayerDeath(LooserName, WinnerName): the player comes back after onDeathSpawnTime (CTF and Domination's start) */
static PyObject *CTF_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sdf_Later(&f, Sf_Global(&f, CTF, "onDeathSpawnTime"), CTF, "RespawnPlayer", Sf_Build(&f, "(O)", loser));
	return Sf_None(&f);
}

/* RemoveFlag(EntityName): the flag gone from the map and from the lists */
static PyObject *CTF_RemoveFlag(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *flag = Sf_Entity(&f, name);
	if (Sf_True(&f, flag) > 0) {
		Sf_SetAttrS(&f, flag, "OnTake", "");
		Sf_Method(&f, flag, "Del", "()");
		Sdf_RemoveIn(&f, CTF, "flagsActive", name);
		Sdf_RemoveIn(&f, CTF, "flagsSpecial", name);
	}
	return Sf_None(&f);
}

/* EndOfCTF(): the leader not the target any more, the flags gone, the ships relax, then the last message */
static PyObject *CTF_EndOfCTF(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Parking");
	PyObject *enemies = Sf_Global(&f, CTF, "enemies");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *n = Sf_Next(&f, enemies, i);
		if (!n) {
			break;
		}
		PyObject *e = Sf_Entity(&f, n);
		int lead = Sf_True(&f, e);
		if (lead > 0) {
			lead = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicIsLeader"), 1);
		}
		if (lead > 0) {
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
			break;
		}
		Sf_Release(&f, mark);
	}
	PyObject *flags = Sf_Global(&f, CTF, "flagsActive");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *n = Sf_Next(&f, flags, i);
		if (!n) {
			break;
		}
		Sdf_Call(&f, CTF, "RemoveFlag", "(O)", n);
		Sf_Release(&f, mark);
	}
	Sdf_SetS(&f, "LeaderFlag", "");
	Sf_Ask(&f, "MissionsFuncs", "SetInvulnerableShipsInList", "(s)", "CTFEnemies");
	Sf_Ask(&f, "MissionsFuncs", "SetInvulnerableShipsInList", "(s)", "CTFFriends");
	Sf_Ask(&f, "MissionsFuncs", "RelaxShipsInList", "(s)", "CTFEnemies");
	Sf_Ask(&f, "MissionsFuncs", "RelaxShipsInList", "(s)", "CTFFriends");
	Sdf_ScorerOff(&f);
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	int won = Sf_Compare(&f, Sf_Global(&f, CTF, "flagsFriends"), ">=", Sf_Global(&f, CTF, "flags"));
	if (won > 0) {
		Sdf_Call(&f, CTF, "PreOnVictory", "()");
	} else if (won == 0) {
		Sdf_Call(&f, CTF, "PreRestartMenu", "()");
	}
	return Sf_None(&f);
}

/* RespawnEnemy(isLeader = 0): an enemy at a spawn point, the leader with the target */
static PyObject *CTF_RespawnEnemy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "isLeader", NULL };
	PyObject *leader = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &leader)) {
		return NULL;
	}
	if (!leader) {
		leader = Sf_Int(&f, 0);
	}
	Sf_Import(&f, "OutMap");
	PyObject *e = Sdf_SpawnCarAng(&f, Sf_Str(&f, "Enemy"));
	Sf_SetAttrS(&f, e, "TgType", "red");
	Sf_SetAttrS(&f, e, "EntityList", "CTFEnemies");
	Sdf_Call(&f, CTF, "SetProfile", "(Oi)", e, 1);
	if (Sf_True(&f, leader) > 0) {
		Sf_SetAttrI(&f, e, "LLogicIsLeader", 1);
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	}
	Sdf_AppendName(&f, CTF, "enemies", e);
	return Sf_None(&f);
}

/* RespawnFriend(): a ship of the player's team at a spawn point */
static PyObject *CTF_RespawnFriend(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *e = Sdf_SpawnCarAng(&f, Sf_Str(&f, "Friend"));
	Sf_SetAttrS(&f, e, "TgType", "green");
	Sf_SetAttrS(&f, e, "EntityList", "CTFFriends");
	Sdf_Call(&f, CTF, "SetProfile", "(Oi)", e, 0);
	Sdf_AppendName(&f, CTF, "friends", e);
	return Sf_None(&f);
}

/* Scrap.Set("CurrentWaypointName", flagsActive[0]) and Scrap.Set("LeaderFlag", flagsActive[0]) while flags are
 * active, else Scrap.Set("LeaderFlag", "") */
static void Ctf_Waypoint(scriptFrame_t *f) {
	if (Sf_Len(f, Sf_Global(f, CTF, "flagsActive")) > 0) {
		Sdf_Set(f, "CurrentWaypointName", Sf_Item(f, Sf_Global(f, CTF, "flagsActive"), 0));
		Sdf_Set(f, "LeaderFlag", Sf_Item(f, Sf_Global(f, CTF, "flagsActive"), 0));
	} else if (!f->failed) {
		Sdf_SetS(f, "LeaderFlag", "");
	}
}

/* len(flagsActive) == 0 and len(flagsSpecial) == 0 */
static int Ctf_NoFlags(scriptFrame_t *f) {
	int none = Sf_Len(f, Sf_Global(f, CTF, "flagsActive")) == 0;
	if (none && !f->failed) {
		none = Sf_Len(f, Sf_Global(f, CTF, "flagsSpecial")) == 0;
	}
	return f->failed ? -1 : none;
}

/* CreateFlag(pos = None): a flag at a spawn point, or thrown from pos by a dead ship (back after 60 s), while fewer
 * than maxFlags are carried; its name, else "" */
static PyObject *CTF_CreateFlag(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "pos", NULL };
	PyObject *pos = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &pos)) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *carried = Sf_Op(&f, Sf_Global(&f, CTF, "flagsFriends"), '+', Sf_Global(&f, CTF, "flagsEnemies"));
	if (Sf_Compare(&f, carried, "<", Sf_Global(&f, CTF, "maxFlags")) > 0) {
		int given = Sf_True(&f, pos);
		PyObject *sp = given == 0 ? Sf_Item(&f, Sf_Ask(&f, "OutMap", "GetSpawnPos", "()"), 0) : pos;
		PyObject *flag = Sdf_Create(&f, Sf_Str(&f, "Flag"), sp, "Item");
		Sf_Method(&f, flag, "SetModel", "(s)", "models/Misc/Flag/Flag.m3d");
		Sf_SetAttrI(&f, flag, "Radius", 1800);
		Sf_SetAttr(&f, flag, "rTime", Sf_Float(&f, 0.25));
		Sf_SetAttrS(&f, flag, "OnTake", "CTF.TakeFlag");
		Sf_SetAttrI(&f, flag, "RadarShow", 2);
		if (Sf_True(&f, pos) > 0) {
			double d[3];
			for (int i = 0; i < 3; i++) {
				d[i] = Scrap_RandRange(0.0, 2.0) - 1.0;
			}
			PyObject *dir = Sf_Build(&f, "(ddd)", d[0], d[1], d[2]);
			Sf_SetAttr(&f, flag, "Vel", Sf_Ask(&f, "SVec", "Prod", "(Oi)", Sf_Ask(&f, "SVec", "Norm", "(O)", dir),
				300));
			Sf_SetAttrI(&f, flag, "BackTime", 60);
			Sf_SetAttrS(&f, flag, "OnReset", "CTF.ResetFlag");
		}
		Sdf_AppendName(&f, CTF, "flagsActive", flag);
		Sdf_Set(&f, "CurrentWaypointName", Sf_Attr(&f, flag, "Name"));
		Sdf_Set(&f, "LeaderFlag", Sf_Item(&f, Sf_Global(&f, CTF, "flagsActive"), 0));
		return Sf_Return(&f, Sf_Attr(&f, flag, "Name"));
	}
	return Sf_Return(&f, Sf_Str(&f, ""));
}

/* ResetFlag(flagName): a thrown flag nobody took goes; a new one when none is left */
static PyObject *CTF_ResetFlag(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sdf_Call(&f, CTF, "RemoveFlag", "(O)", name);
	Ctf_Waypoint(&f);
	if (Ctf_NoFlags(&f) > 0) {
		Sdf_Call(&f, CTF, "CreateFlag", "()");
	}
	return Sf_None(&f);
}

/* TakeFlag(ItemName, CharName): the player or the enemies' leader took a flag; the CTF ends at flags, else a new flag
 * when none is left */
static PyObject *CTF_TakeFlag(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Sound");
	PyObject *usr = Sdf_Usr(&f);
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	int taken = 0;
	Sf_Entity(&f, item);
	int u = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", who);
	if (u > 0) {
		Sdf_Add(&f, CTF, "flagsFriends", 1);
		taken = 1;
		Sdf_Sound(&f, "WaySound", "WayVolume");
	} else if (u == 0 && Sf_In(&f, who, Sf_Global(&f, CTF, "enemies")) > 0) {
		PyObject *e = Sf_Entity(&f, who);
		int lead = Sf_True(&f, e);
		if (lead > 0) {
			lead = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicIsLeader"), 1);
		}
		if (lead > 0) {
			Sdf_Add(&f, CTF, "flagsEnemies", 1);
			taken = 1;
			Sdf_Sound(&f, "EneWaySound", "EneWayVolume");
		}
	}
	if (taken && !f.failed) {
		Sdf_Call(&f, CTF, "RemoveFlag", "(O)", item);
		int fresh = Ctf_NoFlags(&f);
		int lead = Sf_Compare(&f, Sf_Global(&f, CTF, "flagsFriends"), ">", Sf_Global(&f, CTF, "flagsEnemies"));
		Sdf_Set(&f, "PlayerCTFTeamIsWinning", Sf_Int(&f, lead));
		Ctf_Waypoint(&f);
		Sdf_Update(&f, Sf_Global(&f, CTF, "flagsFriends"), Sf_Global(&f, CTF, "flagsEnemies"));
		int end = Sf_Compare(&f, Sf_Global(&f, CTF, "flagsFriends"), "==", Sf_Global(&f, CTF, "flags"));
		if (end == 0) {
			end = Sf_Compare(&f, Sf_Global(&f, CTF, "flagsEnemies"), "==", Sf_Global(&f, CTF, "flags"));
		}
		if (end > 0) {
			Sdf_Call(&f, CTF, "EndOfCTF", "()");
		} else if (end == 0 && fresh > 0) {
			Sdf_Call(&f, CTF, "CreateFlag", "()");
		}
	}
	return Sf_None(&f);
}

/* SpawnFlags(pos, numFlags): the flags a dead ship carried, thrown from pos; the nearest active flag is the player's
 * way point */
static PyObject *CTF_SpawnFlags(PyObject *self, PyObject *args) {
	PyObject *pos, *num;
	long n = 0;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &pos, &num)) {
		return NULL;
	}
	Sf_Check(&f, Script_Range(num, &n));
	for (long i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *special = Sf_Global(&f, CTF, "flagsSpecial");
		Sf_Method(&f, special, "append", "(O)", Sdf_Call(&f, CTF, "CreateFlag", "(O)", pos));
		Sf_Release(&f, mark);
	}
	PyObject *usr = Sdf_Usr(&f);
	int u = Sf_True(&f, usr);
	if (u == 0) {
		Sdf_SetS(&f, "CurrentWaypointName", "");
	} else if (u > 0 && Sf_Len(&f, Sf_Global(&f, CTF, "flagsActive")) > 0) {
		PyObject *minName = Sf_Str(&f, ""), *minDist = Sf_Int(&f, 2000000);
		PyObject *flags = Sf_Global(&f, CTF, "flagsActive");
		int mark = f.n;
		for (int i = 0;; i++) {
			PyObject *n = Sf_Next(&f, flags, i);
			if (!n) {
				break;
			}
			PyObject *e = Sf_Entity(&f, n);
			if (Sf_True(&f, e) > 0) {
				PyObject *ep = Sf_Attr(&f, e, "Pos");
				PyObject *dist = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", ep, Sf_Attr(&f, usr,
					"Pos")));
				if (Sf_Compare(&f, dist, "<", minDist) > 0) {
					minDist = dist;
					minName = n;
				}
			}
			PyObject *keep[2] = { minName, minDist };
			Sf_ReleaseKeep(&f, mark, keep, 2);
		}
		Sdf_Set(&f, "CurrentWaypointName", minName);
	}
	return Sf_None(&f);
}

/* RespawnPlayer(LooserName): the player's ship at a spawn point, the enemies' leader as its target */
static int Ctf_TargetLeader(scriptFrame_t *f, PyObject *e, PyObject *name, void *usr) {
	(void)name;
	int lead = Sf_EqI(f, Sf_Attr(f, e, "LLogicIsLeader"), 1);
	if (lead > 0) {
		Sf_SetAttr(f, usr, "Target", Sf_Attr(f, e, "Name"));
	}
	return lead == 0;
}

static PyObject *CTF_RespawnPlayer(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *usr = Sdf_RespawnUsr(&f, name);
	Sdf_EachInList(&f, "CTFEnemies", Ctf_TargetLeader, usr);
	return Sf_None(&f);
}

static PyObject *CTF_PreRestartMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_PreEnd(args, 0, "CTFFinishPlayerLoosesTextCad", "CTFFriendHead", "CTF", "CTF.RestartCTF");
}

static PyObject *CTF_PreOnVictory(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_PreEnd(args, 1, "CTFFinishPlayerWinsTextCad", "CTFFriendHead", "CTF", "");
}

PyMethodDef g_CTFMethods[] = {
	{ "GetMenuDescription", CTF_GetMenuDescription, 1, NULL },
	{ "InitStatus", CTF_InitStatus, 1, NULL },
	{ "Init", CTF_Init, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "SetProfile", CTF_SetProfile, 1, NULL },
	{ "CreateEnemies", CTF_CreateEnemies, 1, NULL },
	{ "CreateFriends", CTF_CreateFriends, 1, NULL },
	{ "CrazyGamblerStartCTF", CTF_CrazyGamblerStartCTF, 1, NULL },
	{ "SetVulnerable", Sd_SetVulnerable, 1, NULL },
	{ "StartCTF", CTF_StartCTF, 1, NULL },
	{ "OnVehicleDeathCallback", CTF_OnVehicleDeathCallback, 1, NULL },
	{ "OnPlayerDeath", CTF_OnPlayerDeath, 1, NULL },
	{ "RemoveFlag", CTF_RemoveFlag, 1, NULL },
	{ "EndOfCTF", CTF_EndOfCTF, 1, NULL },
	{ "RespawnEnemy", (PyCFunction)(void (*)(void))CTF_RespawnEnemy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "RespawnFriend", CTF_RespawnFriend, 1, NULL },
	{ "CreateFlag", (PyCFunction)(void (*)(void))CTF_CreateFlag, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "ResetFlag", CTF_ResetFlag, 1, NULL },
	{ "TakeFlag", CTF_TakeFlag, 1, NULL },
	{ "SpawnFlags", CTF_SpawnFlags, 1, NULL },
	{ "RespawnPlayer", CTF_RespawnPlayer, 1, NULL },
	{ "PreRestartMenu", CTF_PreRestartMenu, 1, NULL },
	{ "RestartCTF", Sd_Restart, 1, NULL },
	{ "PreOnVictory", CTF_PreOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* ---- Domination: the zones of the OM, taken with the baton; all zones win a round, DominationRounds rounds the deal
 * ---- */

static PyObject *Domination_GetMenuDescription(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *text = Sdf_Lang(&f, "SuperDeal_Domination_Description");
	PyObject *n = Sf_ReprI(&f, Sf_Len(&f, Sf_Global(&f, DOM, "zones")));
	return Sf_Return(&f, Sf_Ask(&f, "string", "replace", "(OsO)", text, "<n1>", n));
}

static PyObject *Domination_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sdf_Status(&f, DOM, NULL);
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, DOM, "ZoneModelNeutral"));
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, DOM, "ZoneModelFriends"));
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, DOM, "ZoneModelEnemies"));
	return Sf_None(&f);
}

/* Init(LevelPath): the camera of the end, the respawn time, both teams in their lists, the leader of the enemies,
 * the start when the player boards; the baton drops when he leaves the ship. The script's `zonesStates = {}` is a
 * local of the function (no global statement), so the table of the zones stays as it is. */
static PyObject *Domination_Init(PyObject *self, PyObject *args) {
	static const char *const libs[] = { "SuperDeal_Sixth", "SuperDeal_Tenth", NULL };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Chars");
	Sdf_CameraFor(&f, libs, "OM");
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", Sf_Global(&f, DOM, "BatonModel"));
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Sf_Global(&f, DOM, "OnPlayerDeath"));
	Sf_SetGlobal(&f, DOM, "onDeathSpawnTime", Sf_ToFloat(&f, Sdf_Save(&f, "DominationOnDeathSpawnTime")));
	if (Sf_Compare(&f, Sf_Global(&f, DOM, "onDeathSpawnTime"), "<", Sf_Float(&f, 1.0)) > 0) {
		Sf_RaiseText(&f, "Demasiado poco tiempo de respawn");
	}
	Sf_SetGlobal(&f, DOM, "zonesCanBeTaken", Sf_Int(&f, 1));
	Sdf_Call(&f, DOM, "CreateEnemies", "()");
	Sdf_Call(&f, DOM, "CreateFriends", "()");
	Sdf_SetS(&f, "OnDominFriendDeath", "Domination.OnFriendDeath");
	Sdf_SetS(&f, "OnDominEnemyDeath", "Domination.OnEnemyDeath");
	Sdf_MarkShips(&f, Sf_Global(&f, DOM, "enemies"), "red", "DominationEnemies");
	Sdf_Set(&f, "LeaderName", Sf_Item(&f, Sf_Global(&f, DOM, "enemies"), 0));
	Sdf_MarkShips(&f, Sf_Global(&f, DOM, "friends"), "green", "DominationFriends");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, DOM, "CrazyGamblerStartDomination"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Sf_Global(&f, DOM, "ReleaseBaton"));
	return Sf_None(&f);
}

/* ReleaseBaton(): the player left his ship with the baton: it drops at a random place */
static PyObject *Domination_ReleaseBaton(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Sf_True(&f, Sf_Global(&f, DOM, "userWithBaton")) > 0) {
		Sdf_Later(&f, Sf_Float(&f, 0.5), DOM, "LostBatonMessage", Sf_Build(&f, "()"));
		Sf_SetGlobal(&f, DOM, "userWithBaton", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, DOM, "shipWithBaton", Sf_Str(&f, ""));
		Sdf_Call(&f, DOM, "CreateBaton", "()");
	}
	Sdf_HideZones(&f);
	return Sf_None(&f);
}

static PyObject *Domination_LostBatonMessage(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_LostBaton(args, "SuperDeal_DominationSystemMessage1");
}

static PyObject *Domination_SetProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_SetProfile(args, "Domination");
}

/* CreateEnemies() and CreateFriends(): the ships of a team at the spawn dummies of the team; the first enemy (the
 * leader) carries no money */
static PyObject *Dom_CreateTeam(PyObject *args, const char *dummies, const char *numVar, const char *prefix, int enemy,
	const char *list) {
	long n = 0;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *spawns = Sf_Keep(&f, PyList_New(0));
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", dummies, 1);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	int mark = f.n;
	while (Sf_True(&f, dummy) > 0) {
		PyObject *ang = Sf_Item(&f, Sf_Item(&f, dummy, 2), 1);
		Sf_Method(&f, spawns, "append", "(O)", Sf_Build(&f, "(OO)", Sf_Item(&f, dummy, 1), ang));
		dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(&f, mark, &dummy, 1);
	}
	if (!enemy) {
		Sdf_Usr(&f);
	}
	PyObject *num = Sdf_SaveInt(&f, numVar);
	Sf_Check(&f, f.failed || Script_Range(num, &n));
	for (long i = 0; i < n && !f.failed; i++) {
		mark = f.n;
		PyObject *pos = Sf_Item(&f, Sf_Item(&f, spawns, i), 0);
		PyObject *ang = Sf_Item(&f, Sf_Item(&f, spawns, i), 1);
		PyObject *e = Sdf_Create(&f, Sdf_Numbered(&f, prefix, i), pos, "Car");
		Sf_SetAttrS(&f, e, "WeapList", "15,15,15,15,15,0");
		if (enemy) {
			Sf_SetAttrI(&f, e, "LaunchTaunts", 0);
		}
		Sdf_Ang(&f, e, ang);
		if (enemy && i == 0) {
			Sf_SetAttrI(&f, e, "Money", -1);
		}
		Sdf_Call(&f, DOM, "SetProfile", "(Oi)", e, enemy);
		Sdf_AppendName(&f, DOM, list, e);
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, e, "Name"));
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *Domination_CreateEnemies(PyObject *self, PyObject *args) {
	(void)self;
	return Dom_CreateTeam(args, "DM_Domination_Enemy_Spawn_*", "DominationNumEnemies", "Enemy", 1, "enemies");
}

static PyObject *Domination_CreateFriends(PyObject *self, PyObject *args) {
	(void)self;
	return Dom_CreateTeam(args, "DM_Domination_Friend_Spawn_*", "DominationNumFriends", "Friend", 0, "friends");
}

/* CreateBaton(pos = None): the baton at pos or at a random point of the AI graph, the mission target on its arrow;
 * the zones off the radar */
static PyObject *Domination_CreateBaton(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "pos", NULL };
	PyObject *pos = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &pos)) {
		return NULL;
	}
	if (Sf_True(&f, pos) == 0) {
		pos = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
	}
	PyObject *l = Sdf_Baton(&f, DOM, pos, "Domination.TakeBaton");
	Sf_SetAttrI(&f, l, "RadarShow", 1);
	Sf_Import(&f, "Items");
	Sf_Ask(&f, "Items", "SetSound", "(Os)", l, "TakeBDataPack");
	PyObject *l2 = Sdf_BatonArrow(&f, l, pos);
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	Sdf_Set(&f, "CurrentWaypointName", Sf_Attr(&f, l2, "Name"));
	Sf_Ask(&f, "SLogic", "ChangeBatonState", "(Oi)", pos, 0);
	Sdf_HideZones(&f);
	return Sf_None(&f);
}

/* CrazyGamblerStartDomination(): the player boarded and aims at the leader; the crazy gambler's message starts the
 * first round */
static PyObject *Domination_CrazyGamblerStartDomination(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(&f, DOM, "OnVehicle"));
	PyObject *usr = Sdf_Usr(&f);
	if (Sdf_UsrCar(&f, usr) > 0) {
		Sf_SetAttr(&f, usr, "Target", Sf_Ask(&f, "Scrap", "Get", "(s)", "LeaderName"));
	}
	int first = Sf_True(&f, Sf_Global(&f, DOM, "CrazyGamblerStartDominationFirst"));
	if (first > 0) {
		first = Sf_EqS(&f, Sdf_Save(&f, "PlayerRounds"), "0");
	}
	if (first > 0) {
		first = Sf_EqS(&f, Sdf_Save(&f, "EnemyRounds"), "0");
	}
	if (first > 0) {
		Sf_SetGlobal(&f, DOM, "CrazyGamblerStartDominationFirst", Sf_Int(&f, 0));
		PyObject *cad = Sdf_Save(&f, "DominationStartTextCad");
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(OsO())", cad, CG, Sf_Global(&f, DOM, "StartDomination"));
	} else if (first == 0) {
		Sdf_Call(&f, DOM, "StartDomination", "()");
	}
	return Sf_None(&f);
}

/* OnVehicle(): the player's ship can be hit again when he boards it */
static PyObject *Domination_OnVehicle(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
	return Sf_None(&f);
}

/* StartDomination(): a round: no zones taken, the scorer, the zones at their places, the baton */
static PyObject *Domination_StartDomination(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "RaceMaker");
	Sf_SetGlobal(&f, DOM, "playerZones", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, DOM, "enemyZones", Sf_Int(&f, 0));
	PyObject *a = Sdf_SaveInt(&f, "PlayerRounds");
	PyObject *b = Sdf_SaveInt(&f, "EnemyRounds");
	Sf_Ask(&f, "RacerScorer", "InitMissionScorer", "(OOO)", a, b, Sdf_Save(&f, "DominationRounds"));
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Sf_Global(&f, DOM, "MissionScorer"));
	Sdf_Call(&f, DOM, "MissionScorer", "(i)", 0);
	int n = Sf_Len(&f, Sf_Global(&f, DOM, "zones"));
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		Sdf_Call(&f, DOM, "CreateZone", "(iO)", i, Sf_Item(&f, Sf_Global(&f, DOM, "zones"), i));
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "SLogic", "SetDominationZones", "(O)", Sf_Global(&f, DOM, "zones"));
	Sdf_Call(&f, DOM, "CreateBaton", "()");
	return Sf_None(&f);
}

/* TakeBaton(ItemName, VehicleName): the leader or the player took the baton; with the player the zones he can take
 * show on the radar */
static PyObject *Domination_TakeBaton(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	PyObject *leader = Sf_Ask(&f, "Scrap", "Get", "(s)", "LeaderName");
	Sf_Import(&f, "Sound");
	PyObject *usr = Sdf_Usr(&f);
	int l = Sf_Compare(&f, who, "==", leader);
	if (l > 0) {
		Sdf_Del(&f, item);
		Sdf_Del(&f, Sf_Cat(&f, "Os", item, "CheckArrow"));
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", who);
		Sdf_Set(&f, "CurrentWaypointName", who);
		Sf_Ask(&f, "SLogic", "ChangeBatonState", "((iii)i)", 0, 0, 0, 1);
		Sf_SetGlobal(&f, DOM, "userWithBaton", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, DOM, "shipWithBaton", who);
		Sf_Ask(&f, "Speech", "ForceRemoteCom", "(ss)", "SuperDeal_DominationRemoteMessage6", CG);
		Sdf_Sound(&f, "EneWaySound", "EneWayVolume");
	} else if (l == 0 && Sdf_UsrIs(&f, usr, who) > 0) {
		PyObject *keys = Sf_Method(&f, Sf_Global(&f, DOM, "zonesStates"), "keys", "()");
		for (int i = 0;; i++) {
			int mark = f.n;
			PyObject *z = Sf_Next(&f, keys, i);
			if (!z) {
				break;
			}
			int show = Sf_EqI(&f, Sf_GetItem(&f, Sf_Global(&f, DOM, "zonesStates"), z), 0);
			if (show == 0) {
				show = Sf_EqI(&f, Sf_GetItem(&f, Sf_Global(&f, DOM, "zonesStates"), z), 2);
			}
			if (show > 0) {
				PyObject *e = Sf_Entity(&f, z);
				if (Sf_True(&f, e) > 0) {
					Sf_SetAttrI(&f, e, "RadarShow", 1);
				}
			}
			Sf_Release(&f, mark);
		}
		Sdf_Del(&f, item);
		Sdf_Del(&f, Sf_Cat(&f, "Os", item, "CheckArrow"));
		Sf_Ask(&f, "SLogic", "ChangeBatonState", "((iii)i)", 0, 0, 0, 2);
		Sf_SetGlobal(&f, DOM, "userWithBaton", Sf_Int(&f, 1));
		Sf_SetGlobal(&f, DOM, "shipWithBaton", Sf_Attr(&f, usr, "Name"));
		Sf_Ask(&f, "Speech", "ForceRemoteCom", "(ss)", "SuperDeal_DominationRemoteMessage5", CG);
		Sdf_Sound(&f, "WaySound", "WayVolume");
	}
	if (Sf_True(&f, Sf_Entity(&f, leader)) > 0) {
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 0, leader);
	}
	return Sf_None(&f);
}

/* CreateZone(i, pos): the neutral zone i at pos and the WithLife the wingmen guard, whose name replaces the place in
 * zones */
static PyObject *Domination_CreateZone(PyObject *self, PyObject *args) {
	PyObject *i, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &i, &pos)) {
		return NULL;
	}
	PyObject *l = Sdf_Create(&f, Sf_Cat(&f, "sO", "Zone", Sf_Repr(&f, i)), pos, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", Sf_Global(&f, DOM, "ZoneModelNeutral"));
	Sf_SetAttrI(&f, l, "Radius", 3000);
	Sf_SetAttrS(&f, l, "OnTake", "Domination.TakeZone");
	Sf_SetAttrI(&f, l, "RadarShow", 0);
	Sf_SetItem(&f, Sf_Global(&f, DOM, "zonesStates"), Sf_Attr(&f, l, "Name"), Sf_Int(&f, 0));
	l = Sdf_Create(&f, Sf_Cat(&f, "Os", Sf_Attr(&f, l, "Name"), "_WithLife"), pos, "WithLife");
	Sf_SetItem(&f, Sf_Global(&f, DOM, "zones"), i, Sf_Attr(&f, l, "Name"));
	return Sf_None(&f);
}

/* lang text + ": %d/%d" % (a, b) */
static PyObject *Dom_Count(scriptFrame_t *f, const char *key, PyObject *a, PyObject *b) {
	PyObject *text = Sdf_Lang(f, key);
	return Sf_Op(f, text, '+', Sf_Op(f, Sf_Str(f, ": %d/%d"), '%', Sf_Build(f, "(OO)", a, b)));
}

/* SScorer.Set(0, item, "Text", lang text + ": %d/%d" % (count, n)), n NULL for len(zones) */
static void Dom_SetCount(scriptFrame_t *f, const char *item, const char *key, const char *count, PyObject *n) {
	PyObject *c = Sf_Global(f, DOM, count);
	PyObject *text = Dom_Count(f, key, c, n ? n : Sf_Int(f, Sf_Len(f, Sf_Global(f, DOM, "zones"))));
	Sf_Ask(f, "SScorer", "Set", "(issO)", 0, item, "Text", text);
}

/* TakeZone(ItemName, CharName): an enemy with the baton marker or the player with the baton takes a zone; all zones
 * win the round (the deal at DominationRounds), else the next one starts after the message */
static PyObject *Domination_TakeZone(PyObject *self, PyObject *args) {
	PyObject *item, *who;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &who)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "Speech");
	if (Sf_True(&f, Sf_Global(&f, DOM, "zonesCanBeTaken")) == 0) {
		return Sf_None(&f);
	}
	PyObject *usr = Sdf_Usr(&f);
	int isUser = Sdf_UsrIs(&f, usr, who);
	if (isUser > 0) {
		isUser = Sf_Compare(&f, Sf_Global(&f, DOM, "shipWithBaton"), "==", Sf_Attr(&f, usr, "Name"));
	}
	int isEnemy = Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, who), "RadarShow"), 1);
	if (isUser == 0 && isEnemy == 0) {
		int msg = Sf_True(&f, usr);
		if (msg > 0) {
			msg = Sf_Compare(&f, who, "==", Sf_Attr(&f, usr, "Name"));
		}
		if (msg > 0) {
			msg = Sf_Compare(&f, Sf_GetItem(&f, Sf_Global(&f, DOM, "zonesStates"), item), "!=", Sf_Int(&f, 1));
		}
		if (msg > 0) {
			PyObject *now = Sf_Time(&f);
			if (Sf_Compare(&f, Sf_Global(&f, DOM, "msgTime"), "<", now) > 0) {
				PyObject *cad = Sdf_Lang(&f, "SuperDeal_Domination_SystemMessage");
				Sf_SetGlobal(&f, DOM, "msgTime", Sf_Op(&f, now, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)",
					cad)));
				Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, Sf_Global(&f, DOM, "msgTime"));
			}
		}
		return Sf_None(&f);
	}
	if (f.failed) {
		return Sf_None(&f);
	}
	PyObject *z = Sf_Entity(&f, item);
	PyObject *numZones = Sf_Int(&f, Sf_Len(&f, Sf_Global(&f, DOM, "zones")));
	PyObject *states = Sf_Global(&f, DOM, "zonesStates");
	if (isEnemy > 0) {
		if (Sf_EqI(&f, Sf_GetItem(&f, states, item), 2) != 0) {
			return Sf_None(&f);
		}
		if (Sf_EqI(&f, Sf_GetItem(&f, Sf_Global(&f, DOM, "zonesStates"), item), 1) > 0) {
			Sdf_Add(&f, DOM, "playerZones", -1);
			Dom_SetCount(&f, "PlayerZonesText", "SuperDeal_DominationPlayerZones", "playerZones", NULL);
		}
		Sf_Method(&f, z, "SetModel", "(O)", Sf_Global(&f, DOM, "ZoneModelEnemies"));
		PyObject *idx = Sf_Method(&f, Sf_Global(&f, DOM, "zones"), "index", "(O)", Sf_Cat(&f, "Os", item, "_WithLife"));
		Sf_Ask(&f, "SLogic", "ChangeZoneState", "(Oi)", idx, 2);
		Sdf_Sound(&f, "EneWaySound", "EneWayVolume");
		Sdf_Add(&f, DOM, "enemyZones", 1);
		Sf_SetItem(&f, Sf_Global(&f, DOM, "zonesStates"), item, Sf_Int(&f, 2));
		Dom_SetCount(&f, "EnemyZonesText", "SuperDeal_DominationEnemyZones", "enemyZones", numZones);
		Sdf_Draw(&f);
		int all = Sf_Compare(&f, Sf_Global(&f, DOM, "enemyZones"), ">=", numZones);
		if (all > 0) {
			PyObject *pr = Sdf_SaveInt(&f, "PlayerRounds");
			PyObject *er = Sf_OpI(&f, Sdf_SaveInt(&f, "EnemyRounds"), '+', 1);
			PyObject *dr = Sdf_SaveInt(&f, "DominationRounds");
			Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "EnemyRounds", Sf_Repr(&f, er));
			Sf_SetGlobal(&f, DOM, "zonesCanBeTaken", Sf_Int(&f, 0));
			int over = Sf_Compare(&f, er, ">=", dr);
			if (over > 0) {
				Sdf_Call(&f, DOM, "EndOfDomination", "(i)", 0);
			} else if (over == 0) {
				Sdf_Update(&f, pr, er);
				Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssO())", "SuperDeal_DominationRemoteMessage2", CG, Sf_Global(&f,
					DOM, "RestartDomination"));
			}
		} else if (all == 0) {
			Sf_Ask(&f, "Speech", "ForceRemoteCom", "(ss)", "SuperDeal_DominationRemoteMessage4", CG);
		}
	} else {
		if (Sf_EqI(&f, Sf_GetItem(&f, states, item), 1) != 0) {
			return Sf_None(&f);
		}
		if (Sf_EqI(&f, Sf_GetItem(&f, Sf_Global(&f, DOM, "zonesStates"), item), 2) > 0) {
			Sdf_Add(&f, DOM, "enemyZones", -1);
			Dom_SetCount(&f, "EnemyZonesText", "SuperDeal_DominationEnemyZones", "enemyZones", NULL);
		}
		Sf_Method(&f, z, "SetModel", "(O)", Sf_Global(&f, DOM, "ZoneModelFriends"));
		Sf_SetAttrI(&f, z, "RadarShow", 0);
		PyObject *idx = Sf_Method(&f, Sf_Global(&f, DOM, "zones"), "index", "(O)", Sf_Cat(&f, "Os", item, "_WithLife"));
		Sf_Ask(&f, "SLogic", "ChangeZoneState", "(Oi)", idx, 1);
		Sdf_Sound(&f, "WaySound", "WayVolume");
		Sdf_Add(&f, DOM, "playerZones", 1);
		Sf_SetItem(&f, Sf_Global(&f, DOM, "zonesStates"), item, Sf_Int(&f, 1));
		Dom_SetCount(&f, "PlayerZonesText", "SuperDeal_DominationPlayerZones", "playerZones", numZones);
		Sdf_Draw(&f);
		int all = Sf_Compare(&f, Sf_Global(&f, DOM, "playerZones"), ">=", numZones);
		if (all > 0) {
			PyObject *pr = Sf_OpI(&f, Sdf_SaveInt(&f, "PlayerRounds"), '+', 1);
			PyObject *er = Sdf_SaveInt(&f, "EnemyRounds");
			PyObject *dr = Sdf_SaveInt(&f, "DominationRounds");
			Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "PlayerRounds", Sf_Repr(&f, pr));
			Sf_SetGlobal(&f, DOM, "zonesCanBeTaken", Sf_Int(&f, 0));
			int over = Sf_Compare(&f, pr, ">=", dr);
			if (over > 0) {
				Sdf_Call(&f, DOM, "EndOfDomination", "(i)", 1);
			} else if (over == 0) {
				PyObject *text = Sf_Op(&f, Sf_Str(&f, "%d/%d"), '%', Sf_Build(&f, "(OO)", Sf_Global(&f, DOM,
					"playerZones"), numZones));
				Sf_Ask(&f, "SScorer", "Set", "(issO)", 0, "ZonesText", "Text", text);
				Sdf_Update(&f, pr, er);
				Sdf_Draw(&f);
				Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssO())", "SuperDeal_DominationRemoteMessage1", CG, Sf_Global(&f,
					DOM, "RestartDomination"));
				Sf_SetAttrI(&f, usr, "Invulnerable", 1);
			}
		} else if (all == 0) {
			Sf_Ask(&f, "Speech", "ForceRemoteCom", "(ss)", "SuperDeal_DominationRemoteMessage3", CG);
		}
	}
	PyObject *e = Sf_Entity(&f, item);
	Sf_SetAttrS(&f, e, "OnTake", "");
	Sdf_Later(&f, Sf_Float(&f, 1.0), DOM, "ActivateZone", Sf_Build(&f, "(O)", item));
	return Sf_None(&f);
}

/* ActivateZone(ItemName): the zone can be taken again */
static PyObject *Domination_ActivateZone(PyObject *self, PyObject *args) {
	PyObject *item;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &item)) {
		return NULL;
	}
	Sf_SetAttrS(&f, Sf_Entity(&f, item), "OnTake", "Domination.TakeZone");
	return Sf_None(&f);
}

/* RestartDomination(): the next round in the level again */
static PyObject *Domination_RestartDomination(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Ask(&f, "SaveGame", "Restart", "()");
	return Sf_None(&f);
}

/* OnPlayerDeath(LooserName, WinnerName): the baton drops where the player died; he comes back after
 * onDeathSpawnTime */
static PyObject *Domination_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	PyObject *dead = Sf_Entity(&f, loser);
	if (Sf_Compare(&f, Sf_Global(&f, DOM, "shipWithBaton"), "==", loser) > 0) {
		Sf_SetGlobal(&f, DOM, "userWithBaton", Sf_Int(&f, 0));
		Sf_SetGlobal(&f, DOM, "shipWithBaton", Sf_Str(&f, ""));
		Sdf_Call(&f, DOM, "CreateBaton", "(O)", Sf_Attr(&f, dead, "Pos"));
	}
	Sdf_HideZones(&f);
	Sdf_Later(&f, Sf_Global(&f, DOM, "onDeathSpawnTime"), DOM, "RespawnPlayer", Sf_Build(&f, "(O)", loser));
	return Sf_None(&f);
}

/* OnEnemyDeath(LooserName, WinnerName): an enemy died (the engine's OnDominEnemyDeath); a new one comes after
 * onDeathSpawnTime, the leader drops the baton */
static PyObject *Domination_OnEnemyDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Ask(&f, "Vehicles", "OnDeath", "(OO)", loser, winner);
	PyObject *dead = Sf_Entity(&f, loser);
	Sf_SetAttrS(&f, dead, "EntityList", "");
	Sdf_RemoveIn(&f, DOM, "enemies", loser);
	PyObject *leader = Sf_Ask(&f, "Scrap", "Get", "(s)", "LeaderName");
	int isLeader = Sf_Compare(&f, leader, "==", loser);
	Sdf_Later(&f, Sf_Global(&f, DOM, "onDeathSpawnTime"), DOM, "RespawnEnemy", Sf_Build(&f, "(i)", isLeader));
	Sdf_Del(&f, loser);
	if (isLeader > 0 && !f.failed) {
		if (Sf_Compare(&f, Sf_Global(&f, DOM, "shipWithBaton"), "==", leader) > 0) {
			Sdf_SetS(&f, "CurrentWaypointName", "");
			Sf_SetGlobal(&f, DOM, "shipWithBaton", Sf_Str(&f, ""));
			Sdf_Call(&f, DOM, "CreateBaton", "(O)", Sf_Attr(&f, dead, "Pos"));
		}
		Sdf_SetS(&f, "LeaderName", "");
	}
	return Sf_None(&f);
}

/* OnFriendDeath(LooserName, WinnerName): a ship of the player's team died; a new one comes after onDeathSpawnTime */
static PyObject *Domination_OnFriendDeath(PyObject *self, PyObject *args) {
	PyObject *loser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &loser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Ask(&f, "Vehicles", "OnDeath", "(OO)", loser, winner);
	Sf_SetAttrS(&f, Sf_Entity(&f, loser), "EntityList", "");
	Sdf_RemoveIn(&f, DOM, "friends", loser);
	Sdf_Later(&f, Sf_Global(&f, DOM, "onDeathSpawnTime"), DOM, "RespawnFriend", Sf_Build(&f, "()"));
	Sdf_Del(&f, loser);
	return Sf_None(&f);
}

/* RespawnPlayer(LooserName): the player's ship at a spawn point, aiming at the leader */
static PyObject *Domination_RespawnPlayer(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *usr = Sdf_RespawnUsr(&f, name);
	Sf_SetAttr(&f, usr, "Target", Sf_Ask(&f, "Scrap", "Get", "(s)", "LeaderName"));
	return Sf_None(&f);
}

/* RespawnEnemy(isLeader): an enemy at a spawn point, the new leader without money */
static PyObject *Domination_RespawnEnemy(PyObject *self, PyObject *args) {
	PyObject *leader, *sp;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &leader)) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *e = Sdf_SpawnCar(&f, Sf_Str(&f, "Enemy"), &sp);
	Sf_SetAttrS(&f, e, "WeapList", "15,15,15,15,15,0");
	Sf_SetAttrI(&f, e, "LaunchTaunts", 0);
	Sdf_Ang(&f, e, Sf_Item(&f, sp, 1));
	Sf_SetAttrS(&f, e, "TgType", "red");
	Sf_SetAttrS(&f, e, "EntityList", "DominationEnemies");
	if (Sf_True(&f, leader) > 0) {
		Sf_SetAttrI(&f, e, "Money", -1);
	}
	Sdf_Call(&f, DOM, "SetProfile", "(Oi)", e, 1);
	if (Sf_True(&f, leader) > 0) {
		Sdf_Set(&f, "LeaderName", Sf_Attr(&f, e, "Name"));
	}
	Sdf_AppendName(&f, DOM, "enemies", e);
	return Sf_None(&f);
}

/* RespawnFriend(): a ship of the player's team at a spawn point */
static PyObject *Domination_RespawnFriend(PyObject *self, PyObject *args) {
	PyObject *sp;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	PyObject *e = Sdf_SpawnCar(&f, Sf_Str(&f, "Friend"), &sp);
	Sf_SetAttrS(&f, e, "WeapList", "15,15,15,15,15,0");
	Sf_SetAttrI(&f, e, "Money", -1);
	Sdf_Ang(&f, e, Sf_Item(&f, sp, 1));
	Sf_SetAttrS(&f, e, "TgType", "green");
	Sf_SetAttrS(&f, e, "EntityList", "DominationFriends");
	Sdf_Call(&f, DOM, "SetProfile", "(Oi)", e, 0);
	Sdf_AppendName(&f, DOM, "friends", e);
	return Sf_None(&f);
}

/* `for name in names: e = Scrap.GetEntity(name); if (e): e.EntityList = ""; e.Invulnerable = 1` */
static void Dom_Freeze(scriptFrame_t *f, PyObject *names) {
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *n = Sf_Next(f, names, i);
		if (!n) {
			break;
		}
		PyObject *e = Sf_Entity(f, n);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrS(f, e, "EntityList", "");
			Sf_SetAttrI(f, e, "Invulnerable", 1);
		}
		Sf_Release(f, mark);
	}
}

/* EndOfDomination(playerWins): the ships out of their lists and safe, then the last message */
static PyObject *Domination_EndOfDomination(PyObject *self, PyObject *args) {
	PyObject *wins;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &wins)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Dom_Freeze(&f, Sf_Global(&f, DOM, "enemies"));
	Dom_Freeze(&f, Sf_Global(&f, DOM, "friends"));
	Sdf_ScorerOff(&f);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	int w = Sf_True(&f, wins);
	if (w > 0) {
		Sdf_Call(&f, DOM, "PreOnVictory", "()");
	} else if (w == 0) {
		Sdf_Call(&f, DOM, "PreRestartMenu", "()");
	}
	return Sf_None(&f);
}

static PyObject *Domination_PreRestartMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_PreEnd(args, 0, "DominationFinishPlayerLoosesTextCad", NULL, "Domination", "Domination.RestartDomin");
}

/* RestartDomin(id, control): the rounds at 0 and the level again */
static PyObject *Domination_RestartDomin(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sdf_SetSave(&f, "PlayerRounds", "0");
	Sdf_SetSave(&f, "EnemyRounds", "0");
	Sf_Ask(&f, "SaveGame", "Restart", "()");
	return Sf_None(&f);
}

static PyObject *Domination_PreOnVictory(PyObject *self, PyObject *args) {
	(void)self;
	return Sdf_PreEnd(args, 1, "DominationFinishPlayerWinsTextCad", NULL, "Domination", "");
}

/* a text of the mission scorer: the zones of a side (Y below the scorer's text, on the Xbox lower) */
static void Dom_ScorerText(scriptFrame_t *f, PyObject *id, const char *item, long yXbox, long yPc, const char *key,
	const char *count) {
	PyObject *n = Sf_Int(f, Sf_Len(f, Sf_Global(f, DOM, "zones")));
	Sf_Ask(f, "SScorer", "Add", "(Oss)", id, item, "Text");
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "X", Sf_Mod(f, "Scorer", "MissionScorerText_X"));
	int xbox = Sf_True(f, Sf_Ask(f, "Scrap", "Get", "(s)", "isXbox"));
	if (xbox >= 0) {
		PyObject *y = Sf_OpI(f, Sf_Mod(f, "Scorer", "MissionScorerText_Y"), '+', xbox ? yXbox : yPc);
		Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "Y", y);
	}
	Sf_Ask(f, "SScorer", "Set", "(Osss)", id, item, "Font", "Horatio");
	Sf_Ask(f, "SScorer", "Set", "(Osss)", id, item, "Effect", "Shadow");
	PyObject *text = Dom_Count(f, key, Sf_Global(f, DOM, count), n);
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "Text", text);
	Sf_Ask(f, "SScorer", "Set", "(Osss)", id, item, "Align", "Left");
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	for (int i = 0; i < 4; i++) {
		PyObject *c = Sf_Item(f, Sf_Mod(f, "Scorer", "MissionScorerText_Color"), i);
		Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, rgba[i], c);
	}
}

/* MissionScorer(id): the zones of both sides above the race scorer */
static PyObject *Domination_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	Dom_ScorerText(&f, id, "PlayerZonesText", 80, -5, "SuperDeal_DominationPlayerZones", "playerZones");
	Dom_ScorerText(&f, id, "EnemyZonesText", 105, 20, "SuperDeal_DominationEnemyZones", "enemyZones");
	Sf_Ask(&f, "RacerScorer", "MissionScorer", "(O)", id);
	return Sf_None(&f);
}

PyMethodDef g_DominationMethods[] = {
	{ "GetMenuDescription", Domination_GetMenuDescription, 1, NULL },
	{ "InitStatus", Domination_InitStatus, 1, NULL },
	{ "Init", Domination_Init, 1, NULL },
	{ "ReleaseBaton", Domination_ReleaseBaton, 1, NULL },
	{ "LostBatonMessage", Domination_LostBatonMessage, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "SetProfile", Domination_SetProfile, 1, NULL },
	{ "CreateEnemies", Domination_CreateEnemies, 1, NULL },
	{ "CreateFriends", Domination_CreateFriends, 1, NULL },
	{ "CreateBaton", (PyCFunction)(void (*)(void))Domination_CreateBaton, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CrazyGamblerStartDomination", Domination_CrazyGamblerStartDomination, 1, NULL },
	{ "OnVehicle", Domination_OnVehicle, 1, NULL },
	{ "StartDomination", Domination_StartDomination, 1, NULL },
	{ "TakeBaton", Domination_TakeBaton, 1, NULL },
	{ "CreateZone", Domination_CreateZone, 1, NULL },
	{ "TakeZone", Domination_TakeZone, 1, NULL },
	{ "ActivateZone", Domination_ActivateZone, 1, NULL },
	{ "RestartDomination", Domination_RestartDomination, 1, NULL },
	{ "OnPlayerDeath", Domination_OnPlayerDeath, 1, NULL },
	{ "OnEnemyDeath", Domination_OnEnemyDeath, 1, NULL },
	{ "OnFriendDeath", Domination_OnFriendDeath, 1, NULL },
	{ "RespawnPlayer", Domination_RespawnPlayer, 1, NULL },
	{ "RespawnEnemy", Domination_RespawnEnemy, 1, NULL },
	{ "RespawnFriend", Domination_RespawnFriend, 1, NULL },
	{ "EndOfDomination", Domination_EndOfDomination, 1, NULL },
	{ "PreRestartMenu", Domination_PreRestartMenu, 1, NULL },
	{ "RestartDomin", Domination_RestartDomin, 1, NULL },
	{ "PreOnVictory", Domination_PreOnVictory, 1, NULL },
	{ "MissionScorer", Domination_MissionScorer, 1, NULL },
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
