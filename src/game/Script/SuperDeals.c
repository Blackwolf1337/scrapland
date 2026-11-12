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
 * stays in the variables of the modules, because the missions, MissionsFuncs, the level files and the score tables
 * read and set it (DeathMatch.enemiesFrags, MortalRace.BayScorer, CTF.flags, Domination.zones,
 * TeamDeathMatch.KeepPlaying, ...): the registry (scrap/modvars.h) is the one place of each value; a function reads a
 * variable where the script reads the global and writes it where the script writes it, lists and dicts are changed in
 * place on the object the variable holds. Entities the script holds are handle values (NV_OBJ, OBJLIB 7): a handle
 * whose entity is gone gives the errors of the script. The engine
 * functions the cases record or swap go through their seams (OBJLIB 7, scrap/scriptmodes.h: Scrap.AddScheduledFunc,
 * Scrap.UsrEntity, Scrap.Set, SSound.Play, SScorer.SetMsgText, SScorer.Set, Scrap.Preload3DObject,
 * SAI.GetStateVehicle); SAI.SetStateVehicle, SLogic.ChangeBatonState and SetDominationZones go through their refs
 * (stage 7).
 *
 * Direct calls (stage 3): the functions call the other functions of their module through PORT_OWN (its global, a
 * mission may replace one) and the other ports through PORT_CALL (scrap/scriptnative.h: the port seam). The
 * functions given to the scheduler take their entries (stage 4: Port_Scheduled); the ones handed to Speech as
 * AfterRemoteComFunc are values Speech keeps (OBJLIB 5 (9c: they go to Speech as their Python function objects until
 * functions are native values)) and calls through their entries, apply() on MissionsFuncs.OnSuperDealFinish as the
 * script read it takes its entry (MODE_CBAPPLY); the class VehiclesNPC.cShipAI is a variable the .mod defines
 * (OBJLIB 5 (9c: a Python class until the .mod classes are native)); the hooks of the missions (KeepPlaying) and the
 * callbacks set in other modules are variables read from the registry where the script reads them.
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

/* the class VehiclesNPC.cShipAI by name (OBJLIB 5 (9c: a class the .mod defines, no port function, a Python class until
 * the .mod classes are native); its instances are Python objects, OBJLIB 7) */
#define CALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* ---- shared lines ---- */

/* module.var.attr = i (Parking.PlayerShip.Invulnerable = 0) */
static byte Sd_ModSetI(const char *module, const char *var, const char *attr, long i) {
	nativeVal_t h;
	if (!Mode_ModGet(module, var, &h)) {
		return 0;
	}
	byte ok = Mode_VSetI(&h, attr, i);
	Native_Free(&h);
	return ok;
}

/* `cad and cad != ""` as a condition: 1, 0, -1 with the error */
static int Sd_Text(const nativeVal_t *cad) {
	return Native_True(cad) && !Native_IsStr(cad, "");
}

/* a == b and a == i of the script (Native_Compare can not fail on native values) */
static byte Sd_Eq(const nativeVal_t *a, const nativeVal_t *b) {
	return !Native_Compare(a, b);
}

/* name = name + add of a global of the module, the new value in *slot (Native_Free it) */
static byte Sd_Add(const char *mod, const char *name, nativeVal_t *slot, long add) {
	nativeVal_t n, r;
	Native_SetInt(&n, add);
	Native_SetNone(&r);
	byte ok = Mode_Read(mod, name, slot) && Native_Arith('+', slot, &n, &r) && Mode_Put(mod, name, slot, &r);
	Native_Free(&r);
	return ok;
}

/* a global of the module = an int */
static byte Sd_PutI(const char *mod, const char *name, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_Put(mod, name, NULL, &v);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func of the module, args) (Mode_Later for a number) with delay a
 * value of the script (a global of the module, the result of a function); func as a value (its entry) */
static byte Sd_LaterV(const nativeVal_t *delay, const char *mod, const char *func, const nativeVal_t *args, int n) {
	nativeVal_t now, t, fn;
	if (!Mode_RefG(mod, func, &fn)) {
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&t);
	byte ok = Native_Arith('+', &now, delay, &t) && Mode_ScheduleV(&t, &fn, args, n, NULL);
	Native_Free(&t);
	Native_Free(&fn);
	return ok;
}

/* the same with func a value held by reference (Speech.ForceRemoteCom, MissionsFuncs.OnSuperDealFinish; OBJLIB 7) at
 * Scrap.GetTime() + delay (the entry of a port's own function) */
static byte Sd_LaterObj(double delay, const nativeVal_t *fn, const nativeVal_t *args, int n) {
	nativeVal_t t;
	Native_SetFloat(&t, Mode_Time() + delay);
	return Mode_ScheduleV(&t, fn, args, n, NULL);
}

/* MissionsFuncs.OnSuperDealFinish as the module holds it, by reference (OBJLIB 7: the function object Speech keeps
 * and the scheduler gets) */
static byte Sd_FinishRef(nativeVal_t *out) {
	return Mode_RefMod("MissionsFuncs", "OnSuperDealFinish", out);
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
	byte ok = PORT_CALL(OutMap, GetSpawnPos, sp, "") && Mode_Item(sp, 0, &pos) && Sd_Create(name, &pos, "Car", out);
	Native_Free(&pos);
	return ok;
}

/* the same with e.Ang = (0, spawnParams[1], 0) right after */
static byte Sd_SpawnCarAng(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t sp, ang;
	Native_SetNone(&sp);
	Native_SetNone(&ang);
	byte ok = Sd_SpawnCar(name, &sp, out) && Mode_Item(&sp, 1, &ang) && Mode_SetAng(out, &ang);
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

/* if x in list: list.remove(x), of a global list of the module, in place */
static byte Sd_RemoveIn(const char *mod, const char *list, const nativeVal_t *x) {
	nativeVal_t l;
	Native_SetNone(&l);
	byte ok = Mode_Read(mod, list, &l);
	int in = ok ? Native_Contains(&l, x) : -1;
	ok = in > 0 ? Mode_Remove(mod, list, x) && Mode_Read(mod, list, &l) : in == 0;
	Native_Free(&l);
	return ok;
}

/* Scrap.GetEntity(name).Del() */
static byte Sd_Del(const nativeVal_t *name) {
	cEntity *e;
	return Mode_Ent(name, &e) && Mode_Del(e) >= 0;
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

/* SInput.GetActionSet() */
static void Sd_ActionSet(nativeVal_t *out) {
	Native_SetStr(out, TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
}

/* Menu.DrawScorer(0, SInput.GetActionSet()), with Menu.MissionScorerCall = None before it */
static byte Sd_Draw(void) {
	nativeVal_t set;
	Sd_ActionSet(&set);
	byte ok = PORT_CALL(Menu, DrawScorer, NULL, "iv", 0L, &set);
	Native_Free(&set);
	return ok;
}

static byte Sd_ScorerOff(void) {
	return Mode_ModNone("Menu", "MissionScorerCall") && Sd_Draw();
}

/* RacerScorer.InitMissionScorer(a, b, c); Menu.MissionScorerCall = RacerScorer.MissionScorer;
 * RacerScorer.MissionScorer(0) */
static byte Sd_ScorerOn(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c) {
	return PORT_CALL(RacerScorer, InitMissionScorer, NULL, "vvv", a, b, c) &&
		Mode_ModTo("Menu", "MissionScorerCall", "RacerScorer", "MissionScorer") &&
		PORT_CALL(RacerScorer, MissionScorer, NULL, "i", 0L);
}

/* RacerScorer.UpdateMissionScorer(0, a, b) */
static byte Sd_Update(const nativeVal_t *a, const nativeVal_t *b) {
	return PORT_CALL(RacerScorer, UpdateMissionScorer, NULL, "ivv", 0L, a, b);
}

/* the parameters (win, mode, retry) of MissionsFuncs.OnSuperDealFinish */
static void Sd_Params(long win, const char *mode, const char *retry, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), win);
	Native_SetStr(Native_Append(out), mode);
	Native_SetStr(Native_Append(out), retry);
}

/* Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params), how ForceRemoteCom or AddRemoteCom; func
 * a reference, given to Speech as the object it is */
static byte Sd_Remote(const char *how, const nativeVal_t *cad, const nativeVal_t *who, const nativeVal_t *func,
	const nativeVal_t *params) {
	if (!strcmp(how, "ForceRemoteCom")) {
		return PORT_CALL(Speech, ForceRemoteCom, NULL, "vv;AfterRemoteComFunc=O,Parameters=v", cad, who, Native_PyOf(func), params);
	}
	return PORT_CALL(Speech, AddRemoteCom, NULL, "vv;AfterRemoteComFunc=O,Parameters=v", cad, who, Native_PyOf(func), params);
}

/* if (cad and cad != ""): Speech.<how>(cad, who, AfterRemoteComFunc = func, Parameters = params) else:
 * apply(func, params) (func a value: its entry while it is a port's own function, MODE_CBAPPLY) */
static byte Sd_End(const char *how, const nativeVal_t *cad, const nativeVal_t *who, const nativeVal_t *func,
	const nativeVal_t *params) {
	if (Sd_Text(cad)) {
		return Sd_Remote(how, cad, who, func, params);
	}
	return Mode_Apply(func, params, NULL);
}

/* the same with the crazy gambler speaking and func (MissionsFuncs.OnSuperDealFinish as the script read it) called
 * with (win, mode, retry) */
static byte Sd_FinishFn(const char *how, const nativeVal_t *cad, const nativeVal_t *func, long win, const char *mode,
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
	nativeVal_t func;
	byte ok = Sd_FinishRef(&func) && Sd_FinishFn(how, cad, &func, win, mode, retry);
	Native_Free(&func);
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
			Mode_VSetI(&usr, "Invulnerable", 1);
	}
	Native_Free(&life);
	Native_Free(&usr);
	return ok;
}

/* usr = Scrap.UsrEntity(0); if usr: usr.Invulnerable = 1 */
static byte Sd_UsrInvulnerable(void) {
	nativeVal_t usr;
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	byte ok = !Native_True(&usr) || Mode_VSetI(&usr, "Invulnerable", 1);
	Native_Free(&usr);
	return ok;
}

/* if (Parking.PlayerShip): Parking.PlayerShip.Invulnerable = 1 */
static byte Sd_ShipSafe(void) {
	nativeVal_t ship;
	if (!Mode_ModGet("Parking", "PlayerShip", &ship)) {
		return 0;
	}
	byte ok = !Native_True(&ship) || Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 1);
	Native_Free(&ship);
	return ok;
}

/* MissionsFuncs.cameraData = MissionsFuncs.mapsCameraData[map] (the object of the table, as the script shares it) */
static byte Sd_Camera(const char *map) {
	nativeVal_t cams, key, c = { 0 };
	Native_SetText(&key, map, (int)strlen(map));
	byte ok = Mode_RefMod("MissionsFuncs", "mapsCameraData", &cams) && Mode_RefSub(&cams, &key, &c) &&
		Mode_RefToMod("MissionsFuncs", "cameraData", &c);
	Native_Free(&c);
	Native_Free(&key);
	Native_Free(&cams);
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
			PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", &w, 0L);
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
	byte ok = PORT_CALL(RaceMaker, InitShipIAProfile, NULL, "vv", &type, &prof) &&
		PORT_CALL(RaceMaker, GetRandomShipIAProfile, &s, "vv", &type, &prof) &&
		PORT_CALL(VehiclesNPC, SetIAProf, NULL, "vv", ship, &s);
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

/* GetMenuDescription(): string.replace(the text of the deal, "<n1>", Scrap.GetSaveVar(var)) */
static byte Sd_MenuDescriptionC(const char *key, const char *var, nativeVal_t *out) {
	nativeVal_t text, n;
	if (!Mode_Import("string")) {
		return 0;
	}
	Mode_Lang(key, &text);
	Mode_SaveVar(var, &n);
	byte ok = PORT_CALL(string, replace, out, "vsv", &text, "<n1>", &n);
	Native_Free(&text);
	Native_Free(&n);
	return ok;
}

/* import Chars, Parking, Vehicles[, more]; the player and his ship invulnerable when created, no ship lost on death */
static byte Sd_StatusC(const char *mod, const char *more) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Mode_Import("Chars") && Mode_Import("Parking") && Mode_Import("Vehicles") && (!more || Mode_Import(more)) &&
		Mode_ModSetGlobal("Chars", "OnCreateMainChar", mod, "SetInvulnerableStatus") &&
		Mode_ModSetGlobal("Parking", "OnCreatePlayerShip", mod, "SetInvulnerableStatus") &&
		Mode_SetModVar("Vehicles", "DelPlayerShipOnDeath", &zero);
}

/* SetInvulnerableStatus(entity): the entity and the player's ship (TeamDeathMatch tests the entity first) */
static byte Sd_SetInvulnerableStatusC(const nativeVal_t *e, byte test) {
	return Mode_Import("Parking") && (test && !Native_True(e) ? 1 : Mode_VSetI(e, "Invulnerable", 1)) && Sd_ShipSafe();
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
	int car = Mode_UsrInCar(&usr);
	byte ok = car >= 0 && (!car || Mode_VSetI(&usr, "Invulnerable", 0)) && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0);
	Native_Free(&usr);
	return ok;
}

static byte Sd_SpawnAt(const nativeVal_t *usr) {
	nativeVal_t sp, v;
	Native_SetNone(&sp);
	Native_SetNone(&v);
	byte ok = PORT_CALL(OutMap, GetSpawnPos, &sp, "") && Mode_Item(&sp, 0, &v) && Mode_VSet(usr, "Pos", &v);
	Native_Free(&sp);
	Native_Free(&v);
	ok = ok && PORT_CALL(OutMap, GetSpawnPos, &sp, "") && Mode_Item(&sp, 1, &v) && Mode_SetAng(usr, &v);
	Native_Free(&sp);
	Native_Free(&v);
	return ok;
}

/* RespawnPlayer(LooserName) of DeathMatch and TeamDeathMatch: the player's ship again at a spawn point (one
 * GetSpawnPos for both) */
static byte Sd_RespawnPlayerC(const nativeVal_t *name) {
	nativeVal_t me, sp, v;
	if (!Mode_Import("Parking") || !Mode_Import("Vehicles") || !Mode_Import("OutMap") ||
		!PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", name)) {
		return 0;
	}
	Native_SetNone(&me);
	Native_SetNone(&sp);
	Native_SetNone(&v);
	byte ok = Mode_EntityH(name, &me) && PORT_CALL(OutMap, GetSpawnPos, &sp, "") && Mode_Item(&sp, 0, &v) &&
		Mode_VSet(&me, "Pos", &v);
	Native_Free(&v);
	ok = ok && Mode_Item(&sp, 1, &v) && Mode_SetAng(&me, &v);
	Native_Free(&me);
	Native_Free(&sp);
	Native_Free(&v);
	return ok;
}

/* RestartCTF(id, control) and RestartRace(id, control): the level again */
static byte Sd_RestartC(void) {
	return PORT_CALL(SaveGame, Restart, NULL, "");
}

MODE_FN(Sd, SetInvulnerable, 1, Sd_SetInvulnerableStatusC(&a[0], 0))
MODE_FN(Sd, AbortRemoteMessages, 0, Sd_AbortRemoteMessagesC())
MODE_FN(Sd, SetVulnerable, 0, Sd_SetVulnerableC())
MODE_FN(Sd, RespawnPlayer, 1, Sd_RespawnPlayerC(&a[0]))
MODE_FN(Sd, Restart, 2, Sd_RestartC())

/* ---- DeathMatch: all against all in the CA, the first with CombatDeaths frags wins ---- */

MODE_FN(Dm, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_DeathMatch_Description", "CombatDeaths", out))
MODE_FN(Dm, InitStatus, 1, Sd_StatusC(DM, NULL))

/* Init(LevelPath): the camera of the end, the death of the player, the enemies, the start when the player boards */
static byte Dm_InitC(void) {
	static const char *const third[] = { "SuperDeal_Third", NULL };
	return Mode_Import("Parking") && Mode_Import("Menu") && Sd_CameraFor(third, "CA") && Mode_Import("Chars") &&
		Mode_ModSetGlobal("Chars", "OnMainCharDeath", DM, "OnPlayerDeath") &&
		PORT_OWN(DeathMatch, CreateEnemies, NULL, "") &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", DM, "CrazyGamblerStartCombat");
}

MODE_FN(Dm, Init, 1, Dm_InitC())
/* SetProfile(enemy) (TeamDeathMatch: SetEnemyProfile): the AI of an enemy */
MODE_FN(Dm, SetProfile, 1, Sd_Profile(&a[0], "CombatEnemyType", "CombatEnemyProfile"))

/* CreateEnemies(): Enemy1 to EnemyN at spawn points, with their frags at 0 when the table is empty */
static byte Dm_CreateEnemiesC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *keys = Mode_V(&t);
	long n = 0;
	if (!Mode_Import("OutMap") || !Mode_SaveInt("CombatNumEnemies", num) || !Mode_Keys(DM, "enemiesFrags", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int size = Native_Size(keys);
	if (size < 0 || !Mode_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *en = Mode_V(&t);
		Sd_Numbered("Enemy", i, name);
		ok = Sd_SpawnCarAng(name, e) && PORT_OWN(DeathMatch, SetProfile, NULL, "v", e) &&
			(size || (Mode_VGet(e, "Name", en) && Mode_DictPut(DM, "enemiesFrags", NULL, en, Mode_I(&t, 0))));
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Dm, CreateEnemies, 0, Dm_CreateEnemiesC())

/* Speech.AddRemoteCom(Scrap.GetSaveVar(cadVar), "CrazyGambler", mod.func) (mod.func a value Speech keeps, OBJLIB 5 (9c:
 * its Python function object until functions are native values); its call there takes the entry, Mode_Apply) */
static byte Sd_GamblerSays(const char *cadVar, const char *mod, const char *func) {
	nativeVal_t cad, fn;
	if (!Mode_RefG(mod, func, &fn)) {
		return 0;
	}
	Mode_SaveVar(cadVar, &cad);
	byte ok = PORT_CALL(Speech, AddRemoteCom, NULL, "vsO", &cad, CG, Native_PyOf(&fn));
	Native_Free(&cad);
	Native_Free(&fn);
	return ok;
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the combat */
static byte Dm_CrazyGamblerStartCombatC(void) {
	return Mode_Import("Parking") && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", DM, "SetVulnerable") &&
		Sd_GamblerSays("CombatStartTextCad", DM, "StartCombat");
}

MODE_FN(Dm, CrazyGamblerStartCombat, 0, Dm_CrazyGamblerStartCombatC())

/* StartCombat(): the deaths counted, the scorer, the enemies red in the list DeathMatch */
static byte Dm_StartCombatC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *deaths = Mode_V(&t), *keys = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") ||
		!Mode_ModSetGlobal("Vehicles", "OnDeathCallback", DM, "OnEnemyDeath")) {
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
	return Mode_ModNone("Chars", "OnMainCharDeath") && Mode_ModNone("Vehicles", "OnDeathCallback") &&
		Mode_ModNone("Parking", "SwitchCharMissionCallback");
}

/* the hook of a mission instead of the score table: KeepPlaying(enemiesFrags.keys()), after the message when there
 * is one (how is the Speech function; the hook a variable) */
static byte Dm_Keep(const char *how, const nativeVal_t *cad) {
	nativeVal_t keys, params, who, hook;
	if (!Mode_RefG(DM, "KeepPlaying", &hook)) {
		return 0;
	}
	Native_SetNone(&keys);
	byte ok = Mode_Keys(DM, "enemiesFrags", 1, &keys);
	if (ok) {
		Native_SetEmpty(&params, NV_TUPLE);
		*Native_Append(&params) = keys;
		Native_SetNone(&keys);
		Native_SetStr(&who, CG);
		ok = Sd_End(how, cad, &who, &hook, &params);
		Native_Free(&params);
	}
	Native_Free(&keys);
	Native_Free(&hook);
	return ok;
}

/* the frags of a deathmatch end it: the player safe (safe), the ships invulnerable and relaxed, the save variable
 * of the result (player: he won), the scorer off; then the last message and the score table, or KeepPlaying of a
 * mission. how is the Speech function; cam the winner the camera looks at (NULL: none) */
static byte Dm_End(byte safe, byte player, const char *how, const nativeVal_t *cam) {
	nativeVal_t cad, func = { 0 }, keep;
	if ((safe && !Sd_UsrSafe()) || !Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") ||
		!Mode_Import("Parking") || !PORT_CALL(MissionsFuncs, SetInvulnerableShipsInList, NULL, "s", "DeathMatch") ||
		!PORT_CALL(MissionsFuncs, RelaxShipsInList, NULL, "s", "DeathMatch")) {
		return 0;
	}
	Mode_SaveS("PlayerWinsCombat", player ? "1" : "0");
	Mode_SaveVar(player ? "CombatFinishPlayerWinsTextCad" : "CombatFinishPlayerLoosesTextCad", &cad);
	Native_SetNone(&keep);
	byte ok = (cam || Sd_FinishRef(&func)) && Sd_ScorerOff() &&
		Dm_Finish() && Mode_Read(DM, "KeepPlaying", &keep);
	if (ok && !Native_True(&keep)) {
		if (cam) {
			ok = Sd_CamOnWinner(cam) && Sd_Finish(how, &cad, 0, "DeathMatch", "DeathMatch.RestartCombat");
		} else {
			ok = Sd_FinishFn(how, &cad, &func, player, "DeathMatch", player ? "" : "DeathMatch.RestartCombat");
		}
	} else if (ok) {
		ok = Dm_Keep(how, &cad);
	}
	Native_Free(&keep);
	Native_Free(&func);
	Native_Free(&cad);
	return ok;
}

/* the frag counted without the end: the scorer, the dead ship back after 5 s (func), inactive until then */
static byte Dm_Next(const nativeVal_t *loser, const char *func) {
	nativeVal_t max, frags;
	cEntity *e;
	Native_SetNone(&max);
	Native_SetNone(&frags);
	byte ok = Mode_Read(DM, "playerFrags", &frags) && PORT_OWN(DeathMatch, GetMaxFragsEnemies, &max, "") &&
		Sd_Update(&frags, &max) && Mode_Later(5.0, DM, func, loser, 1, NULL) && Mode_Ent(loser, &e) &&
		Mode_SetI(e, Native_Text(loser), "Active", 0);
	Native_Free(&max);
	Native_Free(&frags);
	return ok;
}

/* enemiesFrags[winner] = enemiesFrags[winner] + 1 */
static byte Dm_Frag(const nativeVal_t *winner) {
	nativeVal_t frags, n, one, r;
	Native_SetInt(&one, 1);
	Native_SetNone(&frags);
	Native_SetNone(&n);
	Native_SetNone(&r);
	byte ok = Mode_Read(DM, "enemiesFrags", &frags) && Native_Subscript(&frags, winner, &n) &&
		Native_Arith('+', &n, &one, &r) && Mode_DictPut(DM, "enemiesFrags", NULL, winner, &r);
	Native_Free(&frags);
	Native_Free(&n);
	Native_Free(&r);
	return ok;
}

/* enemiesFrags[winner] compared with int(CombatDeaths): -1, 0, 1, or -2 with the error */
static int Dm_FragsCmp(const nativeVal_t *winner) {
	nativeVal_t table, frags, deaths;
	Native_SetNone(&table);
	Native_SetNone(&frags);
	Native_SetNone(&deaths);
	int c = Mode_Read(DM, "enemiesFrags", &table) && Native_Subscript(&table, winner, &frags) &&
		Mode_SaveInt("CombatDeaths", &deaths) ? Native_Compare(&frags, &deaths) : -2;
	Native_Free(&table);
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
	nativeVal_t *usr = Mode_V(&t), *deaths = Mode_V(&t), *frags = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int u = Mode_UsrNameIs(usr, loser);
	if (u) {
		return Mode_Done(&t, u > 0);
	}
	int end = 0, player = 0;
	if (Native_True(winner)) {
		u = Mode_UsrNameIs(usr, winner);
		if (u > 0) {
			player = 1;
			end = Sd_Add(DM, "playerFrags", frags, 1) && Mode_SaveInt("CombatDeaths", deaths) ? Sd_Eq(frags, deaths) : -1;
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
	if (!Mode_Import("OutMap") || !Mode_ListGet("DeathMatch", "Num", num) || !Mode_SaveInt("CombatNumEnemies", max)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(num, max) >= 0) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Sd_SpawnCarAng(name, e) && Mode_VSetS(e, "TgType", "red") && Mode_VSetS(e, "EntityList", "DeathMatch") &&
		PORT_OWN(DeathMatch, SetProfile, NULL, "v", e));
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
	byte ok = Mode_Put(DM, "enemiesFrags", NULL, &d) && Sd_PutI(DM, "playerFrags", 0) &&
		PORT_CALL(SaveGame, Restart, NULL, "");
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

MODE_FN(Vs, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_Versus_Description", "CombatDeaths", out))
MODE_FN(Vs, InitStatus, 1, 1)

/* Init(LevelPath): the camera of the end, the deaths to reach, the enemy, the start when the player boards */
static byte Vs_InitC(void) {
	nativeVal_t zero, deaths;
	Native_SetInt(&zero, 0);
	Native_SetNone(&deaths);
	byte ok = Mode_Import("Parking") && Mode_Import("Menu") && Mode_Import("Vehicles") && Sd_Camera("CA") &&
		Mode_SetModVar("Vehicles", "DelPlayerShipOnDeath", &zero) && Mode_SaveInt("CombatDeaths", &deaths) &&
		Mode_Put(VS, "combatDeaths", NULL, &deaths) && Mode_Import("Chars") &&
		Mode_ModSetGlobal("Chars", "OnMainCharDeath", VS, "OnPlayerDeath") && PORT_OWN(Versus, CreateEnemy, NULL, "") &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", VS, "CrazyGamblerStartCombat");
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
	nativeVal_t *e = Mode_V(&t), *v = Mode_V(&t), *ship = Mode_V(&t), *type = Mode_V(&t), *prof = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("VehiclesNPC") || !Mode_Import("MissionsFuncs") || !Mode_Import("OutMap") ||
		!PORT_CALL(OutMap, GetSpawnPos, sp, "") || !Mode_Unpack(sp, 2, pos) ||
		!Sd_Create(Mode_S(&t, "Enemy"), &pos[0], "Car", e) || !Mode_SetAng(e, &pos[1]) || !Mode_VGet(e, "Name", v) ||
		!Mode_Put(VS, "enemyName", NULL, v)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Mode_SaveVar("CombatEnemyType", v);
	if (!Mode_Put(VS, "combatEnemyType", NULL, v)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Mode_SaveVar("CombatEnemyProfile", v);
	if (!Mode_Put(VS, "combatEnemyProfile", NULL, v) || !Mode_Read(VS, "combatEnemyType", type) ||
		!Mode_Read(VS, "combatEnemyProfile", prof) || !PORT_CALL(RaceMaker, InitShipIAProfile, NULL, "vv", type, prof) ||
		!Mode_Read(VS, "combatEnemyType", type) || !Mode_Read(VS, "combatEnemyProfile", prof) ||
		!Mode_Cat(&t, ship, 4, Mode_S(&t, "S"), type, Mode_S(&t, "1_"), prof) ||
		!PORT_CALL(VehiclesNPC, SetIAProf, NULL, "vv", e, ship)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_EqI(mark, 1)) {
		return Mode_Done(&t, Mode_VSetS(e, "TgType", "red") && Mode_VSetS(e, "EntityList", "Enemies") &&
			PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", e, 1L));
	}
	return Mode_Done(&t, Mode_VSetI(e, "Invulnerable", 1));
}

MODE_KW(Vs, CreateEnemy, "|O", "markAsEnemy") {
	Mode_KwDefI(v, given, 0, 0);
	return Vs_CreateEnemyC(&v[0]);
}

/* int(Scrap.GetSaveVar(a)) == 0 and int(Scrap.GetSaveVar(b)) == 0 (no round played): 1, 0, -1 with the error */
static int Sd_BothZero(const char *a, const char *b) {
	nativeVal_t n;
	Native_SetNone(&n);
	int r = Mode_SaveInt(a, &n) ? Mode_EqI(&n, 0) : -1;
	Native_Free(&n);
	if (r > 0) {
		r = Mode_SaveInt(b, &n) ? Mode_EqI(&n, 0) : -1;
		Native_Free(&n);
	}
	return r;
}

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first round, a later one
 * starts after 3 s */
static byte Vs_CrazyGamblerStartCombatC(void) {
	if (!Mode_Import("Parking") || !Mode_ModNone("Parking", "SwitchVehicleMissionCallback")) {
		return 0;
	}
	int first = Sd_BothZero("NumEnemyDeaths", "NumPlayerDeaths");
	return first > 0 ? Sd_GamblerSays("CombatStartTextCad", VS, "StartCombat") :
		first == 0 && Mode_Later(3.0, VS, "StartCombat", NULL, 0, NULL);
}

MODE_FN(Vs, CrazyGamblerStartCombat, 0, Vs_CrazyGamblerStartCombatC())

/* the text of the enemy's type (auxText of the script, which is never used): CharLangTrans[type] or the type */
static byte Vs_AuxText(void) {
	nativeVal_t x, type, trans;
	Native_SetNone(&x);
	Native_SetNone(&type);
	Native_SetNone(&trans);
	byte ok = Mode_Read(VS, "combatEnemyType", &type);
	int has = ok ? Mode_HasKey(VS, "CharLangTrans", 1, &type) : -1;
	if (has == 0) {
		ok = Mode_Read(VS, "combatEnemyType", &type);
	} else if (has > 0) {
		ok = Mode_Read(VS, "CharLangTrans", &trans) && Mode_Read(VS, "combatEnemyType", &type) &&
			Native_Subscript(&trans, &type, &x);
	} else {
		ok = 0;
	}
	Native_Free(&x);
	Native_Free(&type);
	Native_Free(&trans);
	return ok;
}

/* StartCombat(): the deaths counted, the scorer, the enemy as the red target */
static byte Vs_StartCombatC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *limit = Mode_V(&t), *type = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *deaths = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") || !Mode_Import("MissionsFuncs") ||
		!Mode_Read(VS, "enemyName", name) || !Mode_EntityH(name, e) ||
		!Mode_ModSetGlobal("Vehicles", "OnDeathCallback", VS, "OnEnemyDeath") || !Mode_SaveInt("NumEnemyDeaths", a) ||
		!Mode_SaveInt("NumPlayerDeaths", b) || !Mode_Read(VS, "combatDeaths", deaths)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(deaths, limit);
	if (!Sd_ScorerOn(a, b, limit) || !Mode_VSetS(e, "TgType", "red") || !Mode_VSetS(e, "EntityList", "Enemies") ||
		!Mode_VSetI(e, "Invulnerable", 0) || !PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", e, 1L)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("CombatEnemyType", type);
	return Mode_Done(&t, Mode_Put(VS, "combatEnemyType", NULL, type) && Vs_AuxText());
}

MODE_FN(Vs, StartCombat, 0, Vs_StartCombatC())

/* save variable name = `int(Scrap.GetSaveVar(name)) + 1` */
static byte Vs_Count(const char *name) {
	nativeVal_t n, one, r, s;
	Native_SetNone(&n);
	Native_SetNone(&r);
	Native_SetNone(&s);
	Native_SetInt(&one, 1);
	byte ok = Mode_SaveInt(name, &n) && Native_Arith('+', &n, &one, &r);
	if (ok) {
		Mode_Repr(&r, &s);
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
	byte ok = Mode_SaveInt("NumEnemyDeaths", &a) && Mode_SaveInt("NumPlayerDeaths", &b) && Sd_Update(&a, &b);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* int(Scrap.GetSaveVar(name)) == combatDeaths: 1, 0, -1 with the error */
static int Vs_Reached(const char *name) {
	nativeVal_t n, deaths;
	Native_SetNone(&n);
	Native_SetNone(&deaths);
	int r = Mode_SaveInt(name, &n) && Mode_Read(VS, "combatDeaths", &deaths) ? Sd_Eq(&n, &deaths) : -1;
	Native_Free(&n);
	Native_Free(&deaths);
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
	nativeVal_t *cad = Mode_V(&t), *params = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_Read(VS, "enemyName", name)) {
		return Mode_Done(&t, 0);
	}
	if (!Sd_Eq(loser, name)) {
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
	Mode_SaveS("PlayerWinsCombat", "1");
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
		nativeVal_t fn = { 0 }, func = { 0 };
		ok = Mode_RefMod("Speech", "ForceRemoteCom", &fn) && Sd_FinishRef(&func);
		if (ok) {
			nativeVal_t args[5];
			Native_Copy(&args[0], cad);
			Native_SetStr(&args[1], CG);
			Native_SetInt(&args[2], 1);
			Mode_RefPlain(&func, &args[3]);
			args[4] = *params;
			Native_SetNone(params);
			ok = Sd_LaterObj(2.0, &fn, args, 5);
			for (int i = 0; i < 5; i++) {
				Native_Free(&args[i]);
			}
		}
		Native_Free(&func);
		Native_Free(&fn);
	} else {
		nativeVal_t fn;
		ok = Sd_FinishRef(&fn) && Sd_LaterObj(5.0, &fn, params->items, params->n);
		Native_Free(&fn);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Vs, OnEnemyDeath, 2, Vs_OnEnemyDeathC(&a[0]))

/* RespawnEnemy(): the enemy again, as the target, and the next round */
static byte Vs_RespawnEnemyC(void) {
	return PORT_OWN(Versus, CreateEnemy, NULL, "i", 1L) && PORT_OWN(Versus, StartCombat, NULL, "");
}

MODE_FN(Vs, RespawnEnemy, 0, Vs_RespawnEnemyC())

/* OnPlayerDeath(LooserName, WinnerName): the enemy relaxes; the enemy wins at combatDeaths, else the player comes
 * back after 5 s where he died */
static byte Vs_OnPlayerDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *enemy = Mode_V(&t), *name = Mode_V(&t), *cad = Mode_V(&t), *e = Mode_V(&t), *args = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *en = Mode_V(&t);
	if (!Mode_Read(VS, "enemyName", en) || !Mode_EntityH(en, enemy)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(enemy) && (!Mode_VSetS(enemy, "EntityList", "") || !Mode_VGet(enemy, "Name", name) ||
		!PORT_CALL(SAI, SetStateVehicle, NULL, "ivi", 3L, name, 0L))) {
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
	nativeVal_t *fn = Mode_V(&t);
	byte ok = Sd_FinishRef(fn);
	Sd_Params(0, "Versus", "Versus.RestartCombat", args);
	return Mode_Done(&t, ok && Sd_LaterObj(5.0, fn, args->items, args->n));
}

MODE_FN(Vs, OnPlayerDeath, 2, Vs_OnPlayerDeathC(&a[0], &a[1]))

/* EnemyWins(cad): the enemy's message, then the score table */
static byte Vs_EnemyWinsC(const nativeVal_t *cad) {
	nativeVal_t head, params, fn;
	Mode_SaveVar("CombatEnemyTypeHead", &head);
	if (!Sd_FinishRef(&fn)) {
		Native_Free(&head);
		return 0;
	}
	Sd_Params(0, "Versus", "Versus.RestartCombat", &params);
	byte ok = PORT_CALL(Speech, AddRemoteCom, NULL, "vvOv", cad, &head, Native_PyOf(&fn), &params);
	Native_Free(&params);
	Native_Free(&head);
	Native_Free(&fn);
	return ok;
}

MODE_FN(Vs, EnemyWins, 1, Vs_EnemyWinsC(&a[0]))

/* RespawnPlayer(Usrname, pos, ang): the player's ship again where it died; the enemy attacks it */
static byte Vs_RespawnPlayerC(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *ang) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *enemy = Mode_V(&t), *en = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !Mode_EntityH(name, usr) || !Mode_VGet(usr, "Name", un) ||
		!PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", un) || !Mode_VSet(usr, "Pos", pos) ||
		!Mode_VSet(usr, "Ang", ang) || !Mode_Read(VS, "enemyName", en) || !Mode_EntityH(en, enemy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(enemy)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(un);
	return Mode_Done(&t, Mode_VSetS(enemy, "TgType", "red") && Mode_VSetS(enemy, "EntityList", "Enemies") &&
		Mode_Read(VS, "enemyName", en) && Mode_VGet(usr, "Name", un) &&
		PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, en, un));
}

MODE_FN(Vs, RespawnPlayer, 3, Vs_RespawnPlayerC(&a[0], &a[1], &a[2]))

/* RestartCombat(id, control): the deaths at 0 and the level again */
static byte Vs_RestartCombatC(void) {
	Mode_SaveS("NumEnemyDeaths", "0");
	Mode_SaveS("NumPlayerDeaths", "0");
	return PORT_CALL(SaveGame, Restart, NULL, "");
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

MODE_FN(Tdm, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_TeamDeathMatch_Description", "CombatDeaths", out))
MODE_FN(Tdm, InitStatus, 1, Sd_StatusC(TDM, NULL))

/* Init(LevelPath): the camera of the end, the death of the player, both teams, the start when the player boards */
static byte Tdm_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Fourth", "SuperDeal_Eleventh", "Mission_BankersCombat", NULL };
	return Mode_Import("Parking") && Mode_Import("Menu") && Sd_CameraFor(libs, "QF") && Mode_Import("Chars") &&
		Mode_ModSetGlobal("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath") && PORT_OWN(TeamDeathMatch, CreateFriends, NULL, "") &&
		PORT_OWN(TeamDeathMatch, CreateEnemies, NULL, "") && Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", TDM, "CrazyGamblerStartCombat");
}

MODE_FN(Tdm, Init, 1, Tdm_InitC())
MODE_FN(Tdm, SetInvulnerableStatus, 1, Sd_SetInvulnerableStatusC(&a[0], 1))
/* SetFriendProfile(friend): the AI of a ship of the player's team */
MODE_FN(Tdm, SetFriendProfile, 1, Sd_Profile(&a[0], "CombatFriendType", "CombatFriendProfile"))

/* SetEnemyProfile(e) or SetFriendProfile(e) of a team through its global */
static byte Tdm_SetProfile(byte enemy, const nativeVal_t *e) {
	return enemy ? PORT_OWN(TeamDeathMatch, SetEnemyProfile, NULL, "v", e) :
		PORT_OWN(TeamDeathMatch, SetFriendProfile, NULL, "v", e);
}

/* CreateFriends() and CreateEnemies(): <prefix>1 to <prefix>N of a team at spawn points, invulnerable until the
 * start, their frags at 0 */
static byte Tdm_CreateTeam(byte enemy) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t);
	long n = 0;
	if (!Mode_Import("OutMap") || !Mode_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", num) ||
		!Sd_PutI(TDM, enemy ? "enemiesFrags" : "friendsFrags", 0) ||
		!Mode_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Sd_SpawnCarAng(name, e) && Tdm_SetProfile(enemy, e) &&
			Mode_AppendName(TDM, enemy ? "enemies" : "friends", e) && Mode_VSetI(e, "Invulnerable", 1);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Tdm, CreateFriends, 0, Tdm_CreateTeam(0))
MODE_FN(Tdm, CreateEnemies, 0, Tdm_CreateTeam(1))

/* CrazyGamblerStartCombat(): the player boarded; the crazy gambler's message starts the first combat */
static byte Tdm_CrazyGamblerStartCombatC(void) {
	nativeVal_t frags;
	Native_SetNone(&frags);
	if (!Mode_Import("Parking") || !Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) ||
		!Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", TDM, "OnVehicle") ||
		!Mode_Read(TDM, "enemiesFrags", &frags)) {
		Native_Free(&frags);
		return 0;
	}
	byte first = Mode_EqI(&frags, 0);
	if (first) {
		if (!Mode_Read(TDM, "friendsFrags", &frags)) {
			Native_Free(&frags);
			return 0;
		}
		first = Mode_EqI(&frags, 0);
	}
	Native_Free(&frags);
	return first ? Sd_GamblerSays("CombatStartTextCad", TDM, "StartCombat") :
		PORT_OWN(TeamDeathMatch, StartCombat, NULL, "");
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
	nativeVal_t deaths, zero, team;
	if (!Mode_Import("Vehicles") || !Mode_Import("RacerScorer") || !Mode_Import("Menu") ||
		!Mode_ListSet("Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath") ||
		!Mode_ListSet("Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath")) {
		return 0;
	}
	Native_SetInt(&zero, 0);
	Mode_SaveVar("CombatDeaths", &deaths);
	Native_SetNone(&team);
	byte ok = Sd_ScorerOn(&zero, &zero, &deaths) && Mode_Read(TDM, "enemies", &team) && Tdm_Release(&team, "Enemies") &&
		Mode_Read(TDM, "friends", &team) && Tdm_Release(&team, "Friends");
	Native_Free(&team);
	Native_Free(&deaths);
	return ok;
}

MODE_FN(Tdm, StartCombat, 0, Tdm_StartCombatC())

/* the end with the hook of a mission: Speech.<how>(cad, ...) then mod.KeepPlaying(args), or KeepPlaying(args) at
 * once without a message (the hook a variable) */
static byte Sd_EndHook(const char *mod, const char *how, const nativeVal_t *cad, const nativeVal_t *params) {
	nativeVal_t who, hook;
	if (!Mode_RefG(mod, "KeepPlaying", &hook)) {
		return 0;
	}
	Native_SetStr(&who, CG);
	byte ok = Sd_End(how, cad, &who, &hook, params);
	Native_Free(&hook);
	return ok;
}

/* the end of the team combat after the frag that won it: OnEnemyDeath (the player's team wins) and OnFriendDeath */
static byte Tdm_End(byte win) {
	nativeVal_t cad, none, keep, func = { 0 };
	if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking")) {
		return 0;
	}
	Native_SetNone(&keep);
	Mode_SaveS("PlayerWinsCombat", win ? "1" : "0");
	Mode_SaveVar(win ? "CombatFinishPlayerWinsTextCad" : "CombatFinishPlayerLoosesTextCad", &cad);
	Native_SetEmpty(&none, NV_TUPLE);
	byte ok = Sd_FinishRef(&func) && Sd_ScorerOff() &&
		PORT_CALL(MissionsFuncs, SetInvulnerableShipsInList, NULL, "s", "Enemies") &&
		PORT_CALL(MissionsFuncs, SetInvulnerableShipsInList, NULL, "s", "Friends") &&
		Mode_ListSet("Enemies", "OnDeath", "TeamDeathMatch.OnEnemyDeath2") &&
		Mode_ListSet("Friends", "OnDeath", "TeamDeathMatch.OnFriendDeath2") &&
		Mode_ModSetGlobal("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath2") && Mode_ModNone("Parking", "SwitchCharMissionCallback") &&
		Mode_Read(TDM, "KeepPlaying", &keep);
	if (ok && !Native_True(&keep)) {
		ok = Sd_UsrSafe() && Sd_FinishFn("ForceRemoteCom", &cad, &func, win, "TeamDeathMatch", win ? "" : "TeamDeathMatch.RestartCombat");
	} else if (ok) {
		ok = Sd_EndHook(TDM, "ForceRemoteCom", &cad, &none);
	}
	Native_Free(&keep);
	Native_Free(&func);
	Native_Free(&cad);
	return ok;
}

/* the dead ship of a team comes back after 5 s while the team is short (an enemy is deleted at once) */
static byte Tdm_Refill(byte enemy, const nativeVal_t *loser) {
	nativeVal_t num, len, list;
	Native_SetNone(&num);
	Native_SetNone(&list);
	byte ok = Mode_Read(TDM, enemy ? "enemies" : "friends", &list);
	int n = ok ? Native_Size(&list) : -1;
	Native_Free(&list);
	if (!ok) {
		return 0;
	}
	if (n < 0 || !Mode_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", &num)) {
		Native_Free(&num);
		return 0;
	}
	Native_SetInt(&len, n);
	ok = Native_Compare(&len, &num) >= 0 ||
		(Mode_Later(5.0, TDM, enemy ? "RespawnEnemy" : "RespawnFriend", loser, 1, NULL) && (!enemy || Sd_Del(loser)));
	Native_Free(&num);
	return ok;
}

/* OnEnemyDeath and OnFriendDeath(LooserName, WinnerName): a ship of a team died; a frag of the other team ends the
 * combat at CombatDeaths, else the score; a new ship comes after 5 s while the team is short */
static byte Tdm_Death(byte enemy, const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *deaths = Mode_V(&t), *e = Mode_V(&t), *list = Mode_V(&t), *slot = Mode_V(&t);
	nativeVal_t *other = Mode_V(&t);
	const char *frags = enemy ? "friendsFrags" : "enemiesFrags";
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (enemy) {
		int u = Mode_UsrNameIs(usr, loser);
		if (u) {
			return Mode_Done(&t, u > 0);
		}
	}
	if (!Sd_RemoveIn(TDM, enemy ? "enemies" : "friends", loser) || !Mode_SaveInt("CombatDeaths", deaths)) {
		return Mode_Done(&t, 0);
	}
	int end = 0;
	if (Native_True(winner)) {
		int u = Mode_UsrNameIs(usr, winner);
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
	} else if (!Mode_Read(TDM, "friendsFrags", slot) || !Mode_Read(TDM, "enemiesFrags", other) || !Sd_Update(slot, other)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Tdm_Refill(enemy, loser));
}

MODE_FN(Tdm, OnEnemyDeath, 2, Tdm_Death(1, &a[0], &a[1]))
MODE_FN(Tdm, OnFriendDeath, 2, Tdm_Death(0, &a[0], &a[1]))

/* OnEnemyDeath2 and OnFriendDeath2(LooserName, WinnerName): after the end, the dead ship only comes back */
static byte Tdm_Death2(byte enemy, const nativeVal_t *loser) {
	return Sd_RemoveIn(TDM, enemy ? "enemies" : "friends", loser) && Tdm_Refill(enemy, loser);
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
		!Mode_SaveInt(enemy ? "CombatNumEnemies" : "CombatNumFriends", max)) {
		return Mode_Done(&t, 0);
	}
	if (Sd_Eq(num, max)) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Sd_SpawnCarAng(name, e) && Mode_VSetS(e, "EntityList", entityList) &&
		Tdm_SetProfile(enemy, e) && Mode_AppendName(TDM, enemy ? "enemies" : "friends", e));
}

MODE_FN(Tdm, RespawnEnemy, 1, Tdm_Respawn(1, &a[0]))
MODE_FN(Tdm, RespawnFriend, 1, Tdm_Respawn(0, &a[0]))

/* OnPlayerDeath(LooserName, WinnerName): a frag of the enemies ends the combat at CombatDeaths; the player comes back
 * after 5 s */
static byte Tdm_OnPlayerDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *deaths = Mode_V(&t), *cad = Mode_V(&t), *none = Mode_V(&t), *enemies = Mode_V(&t);
	nativeVal_t *ef = Mode_V(&t), *ff = Mode_V(&t), *keep = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_Read(TDM, "enemies", enemies)) {
		return Mode_Done(&t, 0);
	}
	int in = Native_Contains(enemies, winner);
	if (in < 0 || (in > 0 && !Sd_Add(TDM, "enemiesFrags", ef, 1))) {
		return Mode_Done(&t, 0);
	}
	int end = in;
	if (end > 0) {
		end = Mode_Read(TDM, "enemiesFrags", ef) && Mode_SaveInt("CombatDeaths", deaths) ? Native_Compare(ef, deaths) >= 0 : -1;
	}
	if (end < 0) {
		return Mode_Done(&t, 0);
	}
	if (end) {
		if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking")) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveS("PlayerWinsCombat", "0");
		Mode_SaveVar("CombatFinishPlayerLoosesTextCad", cad);
		if (!Sd_ScorerOff() || !Mode_ModSetGlobal("Chars", "OnMainCharDeath", TDM, "OnPlayerDeath2") ||
			!Mode_ModNone("Parking", "SwitchCharMissionCallback") || !Mode_Read(TDM, "KeepPlaying", keep)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(none, NV_TUPLE);
		if (!(Native_True(keep) ? Sd_EndHook(TDM, "AddRemoteCom", cad, none) : Sd_CamOnWinner(winner) &&
			Sd_Finish("AddRemoteCom", cad, 0, "TeamDeathMatch", "TeamDeathMatch.RestartCombat"))) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_Read(TDM, "friendsFrags", ff) || !Mode_Read(TDM, "enemiesFrags", ef) || !Sd_Update(ff, ef)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_Later(5.0, TDM, "RespawnPlayer", loser, 1, NULL));
}

MODE_FN(Tdm, OnPlayerDeath, 2, Tdm_OnPlayerDeathC(&a[0], &a[1]))
/* OnPlayerDeath2(LooserName, WinnerName): after the end the player only comes back */
MODE_FN(Tdm, OnPlayerDeath2, 2, Mode_Later(5.0, TDM, "RespawnPlayer", &a[0], 1, NULL))

/* RestartCombat(id, control): the frags at 0 and the level again */
static byte Tdm_RestartCombatC(void) {
	return Sd_PutI(TDM, "enemiesFrags", 0) && Sd_PutI(TDM, "friendsFrags", 0) &&
		PORT_CALL(SaveGame, Restart, NULL, "");
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

MODE_FN(Ctf, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_CTF_Description", "CTFFlags", out))
MODE_FN(Ctf, InitStatus, 1, Sd_StatusC(CTF, NULL))

/* onDeathSpawnTime = float(Scrap.GetSaveVar(var)); the error of the script when it is under 1 s */
static byte Sd_SpawnTime(const char *mod, const char *var) {
	nativeVal_t s, d, one, time;
	Mode_SaveVar(var, &s);
	Native_SetNone(&d);
	Native_SetNone(&time);
	byte ok = Native_ToFloat(&s, &d) && Mode_Put(mod, "onDeathSpawnTime", NULL, &d) && Mode_Read(mod, "onDeathSpawnTime", &time);
	Native_Free(&s);
	Native_Free(&d);
	Native_SetFloat(&one, 1.0);
	ok = ok && (Native_Compare(&time, &one) >= 0 || Mode_RaiseText("Demasiado poco tiempo de respawn"));
	Native_Free(&time);
	return ok;
}

/* Init(LevelPath): the camera of the end, the flags to take, the respawn time, both teams, the start when the player
 * boards */
static byte Ctf_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Fifth", "SuperDeal_Ninth", NULL };
	nativeVal_t n, two, one, r, max, flags;
	if (!Mode_Import("Parking") || !Mode_Import("Chars") || !Sd_CameraFor(libs, "FZ") ||
		!Mode_ModSetGlobal("Chars", "OnMainCharDeath", CTF, "OnPlayerDeath")) {
		return 0;
	}
	Native_SetNone(&n);
	Native_SetNone(&r);
	Native_SetNone(&max);
	Native_SetNone(&flags);
	Native_SetInt(&two, 2);
	Native_SetInt(&one, 1);
	byte ok = Mode_SaveInt("CTFFlags", &n) && Mode_Put(CTF, "flags", NULL, &n) && Mode_Read(CTF, "flags", &flags) &&
		Native_Arith('*', &flags, &two, &r) && Native_Arith('-', &r, &one, &max) &&
		Mode_Put(CTF, "maxFlags", NULL, &max) && Sd_SpawnTime(CTF, "CTFOnDeathSpawnTime") &&
		PORT_OWN(CTF, CreateEnemies, NULL, "") && PORT_OWN(CTF, CreateFriends, NULL, "") &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", CTF, "CrazyGamblerStartCTF");
	Native_Free(&n);
	Native_Free(&r);
	Native_Free(&max);
	Native_Free(&flags);
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
	byte e = Mode_EqI(enemy, 1);
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
	if (!Mode_Import("OutMap") || !Mode_SaveInt(enemy ? "CTFNumEnemies" : "CTFNumFriends", num) || !Mode_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (long i = 1; ok && i <= n - (enemy ? 0 : 1); i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Sd_SpawnCarAng(name, e) && PORT_OWN(CTF, SetProfile, NULL, "vi", e, (long)enemy) &&
			Mode_AppendName(CTF, enemy ? "enemies" : "friends", e);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Ctf, CreateEnemies, 0, Ctf_CreateTeam(1))
MODE_FN(Ctf, CreateFriends, 0, Ctf_CreateTeam(0))

/* CrazyGamblerStartCTF(): the player boarded; the crazy gambler's message starts the CTF */
static byte Ctf_CrazyGamblerStartCTFC(void) {
	return Mode_Import("Parking") && Mode_Import("Speech") && Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", CTF, "SetVulnerable") &&
		Sd_GamblerSays("CTFStartTextCad", CTF, "StartCTF");
}

MODE_FN(Ctf, CrazyGamblerStartCTF, 0, Ctf_CrazyGamblerStartCTFC())

/* RaceMaker.ShowSystemMessage(Scrap.GetLangStr(key)) */
static byte Sd_SystemMessage(const char *key) {
	nativeVal_t text;
	Mode_Lang(key, &text);
	byte ok = PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

/* StartCTF(): the deaths counted, the scorer, the first flag, the enemies red with the first as the leader, the
 * friends green */
static byte Ctf_StartCTFC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *limit = Mode_V(&t), *first = Mode_V(&t), *e = Mode_V(&t), *usr = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *list = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("Vehicles") || !Mode_Import("Menu") || !Mode_Import("RacerScorer") || !Mode_Import("RaceMaker") ||
		!Mode_ModSetGlobal("Vehicles", "OnDeathCallback", CTF, "OnVehicleDeathCallback") || !Mode_Read(CTF, "flags", list)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(list, limit);
	if (!Sd_ScorerOn(zero, zero, limit) || !PORT_OWN(CTF, CreateFlag, NULL, "") || !Mode_Read(CTF, "enemies", list) ||
		!Sd_MarkShips(list, "red", "CTFEnemies") || !Mode_Read(CTF, "enemies", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(list);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (n > 0) {
		if (!Mode_Read(CTF, "enemies", list) || !Mode_Item(list, 0, first) || !Mode_EntityH(first, e)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(e)) {
			int car;
			if (!Mode_VSetI(e, "LLogicIsLeader", 1) || !PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", e, 1L) ||
				!Mode_UsrHandle(usr) || (car = Mode_UsrInCar(usr)) < 0 ||
				(car && (!Mode_VGet(e, "Name", name) || !Mode_VSet(usr, "Target", name)))) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, Mode_Read(CTF, "friends", list) && Sd_MarkShips(list, "green", "CTFFriends") &&
		Sd_SystemMessage("SuperDeal_CTF_SystemMessage"));
}

MODE_FN(Ctf, StartCTF, 0, Ctf_StartCTFC())

/* list.remove(x) of a global list of the module, in place, and the global read again (its NameError) */
static byte Sd_Remove(const char *mod, const char *list, const nativeVal_t *x) {
	nativeVal_t l;
	Native_SetNone(&l);
	byte ok = Mode_Remove(mod, list, x) && Mode_Read(mod, list, &l);
	Native_Free(&l);
	return ok;
}

/* RacerScorer.UpdateMissionScorer(0, flagsFriends, flagsEnemies) */
static byte Ctf_Update(void) {
	nativeVal_t f, e;
	Native_SetNone(&f);
	Native_SetNone(&e);
	byte ok = Mode_Read(CTF, "flagsFriends", &f) && Mode_Read(CTF, "flagsEnemies", &e) && Sd_Update(&f, &e);
	Native_Free(&f);
	Native_Free(&e);
	return ok;
}

/* CTF.SpawnFlags(pos of the handle, flags<side>); flags<side> = 0; the scorer */
static byte Ctf_Drop(const nativeVal_t *h, const char *var) {
	nativeVal_t pos, n;
	Native_SetNone(&pos);
	Native_SetNone(&n);
	byte ok = Mode_VGet(h, "Pos", &pos) && Mode_Read(CTF, var, &n) && PORT_OWN(CTF, SpawnFlags, NULL, "vv", &pos, &n) &&
		Sd_PutI(CTF, var, 0) && Ctf_Update();
	Native_Free(&pos);
	Native_Free(&n);
	return ok;
}

/* OnVehicleDeathCallback(LooserName, WinnerName): the player or the enemies' leader drop the flags they carry; the
 * dead ship of a team comes back after onDeathSpawnTime */
static byte Ctf_OnVehicleDeathCallbackC(const nativeVal_t *loser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *e = Mode_V(&t), *lead = Mode_V(&t), *list = Mode_V(&t), *time = Mode_V(&t);
	const nativeVal_t *one = Mode_I(&t, 1);
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int u = Mode_UsrNameIs(usr, loser);
	if (u) {
		return Mode_Done(&t, u > 0 && Ctf_Drop(usr, "flagsFriends"));
	}
	if (!Mode_Read(CTF, "enemies", list)) {
		return Mode_Done(&t, 0);
	}
	int en = Native_Contains(list, loser);
	if (en > 0) {
		if (!Sd_Remove(CTF, "enemies", loser) || !Mode_EntityH(loser, e)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(e)) {
			return Mode_Done(&t, 1);
		}
		if (!Mode_VGet(e, "LLogicIsLeader", lead)) {
			return Mode_Done(&t, 0);
		}
		byte ok = Mode_EqI(lead, 1) ? Ctf_Drop(e, "flagsEnemies") && Mode_Read(CTF, "onDeathSpawnTime", time) &&
			Sd_LaterV(time, CTF, "RespawnEnemy", one, 1) :
			Mode_Read(CTF, "onDeathSpawnTime", time) && Sd_LaterV(time, CTF, "RespawnEnemy", NULL, 0);
		return Mode_Done(&t, ok && Sd_Del(loser));
	}
	if (en < 0 || !Mode_Read(CTF, "friends", list)) {
		return Mode_Done(&t, 0);
	}
	int fr = Native_Contains(list, loser);
	return Mode_Done(&t, fr <= 0 ? fr == 0 : Sd_Remove(CTF, "friends", loser) &&
		Mode_Read(CTF, "onDeathSpawnTime", time) && Sd_LaterV(time, CTF, "RespawnFriend", NULL, 0));
}

MODE_FN(Ctf, OnVehicleDeathCallback, 2, Ctf_OnVehicleDeathCallbackC(&a[0]))

/* OnPlayerDeath(LooserName, WinnerName): the player comes back after onDeathSpawnTime (CTF and Domination's start) */
static byte Sd_RespawnLater(const char *mod, const nativeVal_t *loser) {
	nativeVal_t time;
	Native_SetNone(&time);
	byte ok = Mode_Read(mod, "onDeathSpawnTime", &time) && Sd_LaterV(&time, mod, "RespawnPlayer", loser, 1);
	Native_Free(&time);
	return ok;
}

MODE_FN(Ctf, OnPlayerDeath, 2, Sd_RespawnLater(CTF, &a[0]))

/* RemoveFlag(EntityName): the flag gone from the map and from the lists */
static byte Ctf_RemoveFlagC(const nativeVal_t *name) {
	nativeVal_t flag;
	if (!Mode_EntityH(name, &flag)) {
		return 0;
	}
	byte ok = !Native_True(&flag) || (Mode_VSetS(&flag, "OnTake", "") && Mode_VDel(&flag) &&
		Sd_RemoveIn(CTF, "flagsActive", name) && Sd_RemoveIn(CTF, "flagsSpecial", name));
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
	int r = Mode_VGet(e, "LLogicIsLeader", &v) ? Mode_EqI(&v, 1) : -1;
	Native_Free(&v);
	return r;
}

/* EndOfCTF(): the leader not the target any more, the flags gone, the ships relax, then the last message */
static byte Ctf_EndOfCTFC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *list = Mode_V(&t), *flagsN = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Chars") || !Mode_Import("Vehicles") || !Mode_Import("Parking") ||
		!Mode_Read(CTF, "enemies", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(list);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int lead;
		ok = Native_SeqItem(list, i, name) && Mode_EntityH(name, e) && (lead = Sd_IsLeader(e)) >= 0;
		if (ok && lead) {
			ok = PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", e, 0L);
			break;
		}
		Native_Free(name);
		Native_Free(e);
	}
	/* for name in flagsActive: CTF.RemoveFlag(name), on the list object read before the loop as RemoveFlag changes it
	 * (the loop of the script runs on the object itself) */
	nativeVal_t flags = { 0 };
	ok = ok && Mode_RefG(CTF, "flagsActive", &flags);
	for (int i = 0; ok; i++) {
		nativeVal_t item;
		int r = Mode_RefNext(&flags, i, &item);
		if (r <= 0) {
			ok = r == 0;
			break;
		}
		Native_Free(name);
		Mode_RefPlain(&item, name);
		Native_Free(&item);
		ok = PORT_OWN(CTF, RemoveFlag, NULL, "v", name);
	}
	Native_Free(&flags);
	if (!ok || !Mode_GVarSetS("LeaderFlag", "") ||
		!PORT_CALL(MissionsFuncs, SetInvulnerableShipsInList, NULL, "s", "CTFEnemies") ||
		!PORT_CALL(MissionsFuncs, SetInvulnerableShipsInList, NULL, "s", "CTFFriends") ||
		!PORT_CALL(MissionsFuncs, RelaxShipsInList, NULL, "s", "CTFEnemies") ||
		!PORT_CALL(MissionsFuncs, RelaxShipsInList, NULL, "s", "CTFFriends") || !Sd_ScorerOff() ||
		!Mode_ModNone("Vehicles", "OnDeathCallback") || !Mode_ModNone("Parking", "SwitchCharMissionCallback") ||
		!Mode_ModNone("Parking", "SwitchVehicleMissionCallback") || !Mode_Read(CTF, "flagsFriends", list) ||
		!Mode_Read(CTF, "flags", flagsN)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Native_Compare(list, flagsN) >= 0 ? PORT_OWN(CTF, PreOnVictory, NULL, "") :
		PORT_OWN(CTF, PreRestartMenu, NULL, ""));
}

MODE_FN(Ctf, EndOfCTF, 0, Ctf_EndOfCTFC())

/* RespawnEnemy(isLeader = 0): an enemy at a spawn point, the leader with the target */
static byte Ctf_RespawnEnemyC(const nativeVal_t *leader) {
	nativeVal_t e, name;
	Native_SetNone(&e);
	Native_SetStr(&name, "Enemy");
	byte ok = Mode_Import("OutMap") && Sd_SpawnCarAng(&name, &e) && Mode_VSetS(&e, "TgType", "red") &&
		Mode_VSetS(&e, "EntityList", "CTFEnemies") && PORT_OWN(CTF, SetProfile, NULL, "vi", &e, 1L) &&
		(!Native_True(leader) || (Mode_VSetI(&e, "LLogicIsLeader", 1) && PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", &e, 1L))) &&
		Mode_AppendName(CTF, "enemies", &e);
	Native_Free(&e);
	return ok;
}

MODE_KW(Ctf, RespawnEnemy, "|O", "isLeader") {
	Mode_KwDefI(v, given, 0, 0);
	return Ctf_RespawnEnemyC(&v[0]);
}

/* RespawnFriend(): a ship of the player's team at a spawn point */
static byte Ctf_RespawnFriendC(void) {
	nativeVal_t e, name;
	Native_SetNone(&e);
	Native_SetStr(&name, "Friend");
	byte ok = Mode_Import("OutMap") && Sd_SpawnCarAng(&name, &e) && Mode_VSetS(&e, "TgType", "green") &&
		Mode_VSetS(&e, "EntityList", "CTFFriends") && PORT_OWN(CTF, SetProfile, NULL, "vi", &e, 0L) && Mode_AppendName(CTF, "friends", &e);
	Native_Free(&e);
	return ok;
}

MODE_FN(Ctf, RespawnFriend, 0, Ctf_RespawnFriendC())

/* Scrap.Set(var, flagsActive[0]) (the global read again) */
static byte Ctf_SetFirst(const char *var) {
	nativeVal_t v, active;
	Native_SetNone(&v);
	Native_SetNone(&active);
	byte ok = Mode_Read(CTF, "flagsActive", &active) && Mode_Item(&active, 0, &v) && Mode_GVarSet(var, &v);
	Native_Free(&v);
	Native_Free(&active);
	return ok;
}

/* Scrap.Set("CurrentWaypointName", flagsActive[0]) and Scrap.Set("LeaderFlag", flagsActive[0]) while flags are
 * active, else Scrap.Set("LeaderFlag", "") */
static byte Ctf_Waypoint(void) {
	nativeVal_t active;
	Native_SetNone(&active);
	int n = Mode_Read(CTF, "flagsActive", &active) ? Native_Size(&active) : -1;
	Native_Free(&active);
	if (n < 0) {
		return 0;
	}
	return n > 0 ? Ctf_SetFirst("CurrentWaypointName") && Ctf_SetFirst("LeaderFlag") : Mode_GVarSetS("LeaderFlag", "");
}

/* len(flagsActive) == 0 and len(flagsSpecial) == 0: 1, 0, -1 with the error */
static int Ctf_NoFlags(void) {
	nativeVal_t l;
	Native_SetNone(&l);
	int n = Mode_Read(CTF, "flagsActive", &l) ? Native_Size(&l) : -1;
	if (n == 0) {
		n = Mode_Read(CTF, "flagsSpecial", &l) ? Native_Size(&l) : -1;
		n = n < 0 ? -1 : n == 0;
	} else {
		n = n < 0 ? -1 : 0;
	}
	Native_Free(&l);
	return n;
}

/* CreateFlag(pos = None): a flag at a spawn point, or thrown from pos by a dead ship (back after 60 s), while fewer
 * than maxFlags are carried; its name, else "" */
static byte Ctf_CreateFlagC(const nativeVal_t *pos, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *carried = Mode_V(&t), *sp = Mode_V(&t), *at = Mode_V(&t), *flag = Mode_V(&t), *dir = Mode_V(&t);
	nativeVal_t *norm = Mode_V(&t), *vel = Mode_V(&t), *name = Mode_V(&t), *ff = Mode_V(&t), *fe = Mode_V(&t);
	nativeVal_t *max = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_Read(CTF, "flagsFriends", ff) || !Mode_Read(CTF, "flagsEnemies", fe) ||
		!Native_Arith('+', ff, fe, carried) || !Mode_Read(CTF, "maxFlags", max)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(carried, max) >= 0) {
		Native_SetStr(out, "");
		return Mode_Done(&t, 1);
	}
	if (Native_True(pos)) {
		Native_Copy(at, pos);
	} else if (!PORT_CALL(OutMap, GetSpawnPos, sp, "") || !Mode_Item(sp, 0, at)) {
		return Mode_Done(&t, 0);
	}
	if (!Sd_Create(Mode_S(&t, "Flag"), at, "Item", flag) || !Mode_VModel(flag, Mode_S(&t, "models/Misc/Flag/Flag.m3d")) ||
		!Mode_VSetI(flag, "Radius", 1800) || !Mode_VSetF(flag, "rTime", 0.25) || !Mode_VSetS(flag, "OnTake", "CTF.TakeFlag") ||
		!Mode_VSetI(flag, "RadarShow", 2)) {
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
			!Mode_VSetI(flag, "BackTime", 60) || !Mode_VSetS(flag, "OnReset", "CTF.ResetFlag")) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_AppendName(CTF, "flagsActive", flag) || !Mode_VGet(flag, "Name", name) ||
		!Mode_GVarSet("CurrentWaypointName", name) || !Ctf_SetFirst("LeaderFlag") || !Mode_VGet(flag, "Name", out)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, 1);
}

MODE_KW(Ctf, CreateFlag, "|O", "pos") {
	return Ctf_CreateFlagC(&v[0], out);
}

/* ResetFlag(flagName): a thrown flag nobody took goes; a new one when none is left */
static byte Ctf_ResetFlagC(const nativeVal_t *name) {
	if (!PORT_OWN(CTF, RemoveFlag, NULL, "v", name) || !Ctf_Waypoint()) {
		return 0;
	}
	int none = Ctf_NoFlags();
	return none > 0 ? PORT_OWN(CTF, CreateFlag, NULL, "") : none == 0;
}

MODE_FN(Ctf, ResetFlag, 1, Ctf_ResetFlagC(&a[0]))

/* TakeFlag(ItemName, CharName): the player or the enemies' leader took a flag; the CTF ends at flags, else a new flag
 * when none is left */
static byte Ctf_TakeFlagC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *e = Mode_V(&t), *win = Mode_V(&t), *ff = Mode_V(&t), *fe = Mode_V(&t);
	nativeVal_t *flags = Mode_V(&t);
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
		if (!Sd_Add(CTF, "flagsFriends", ff, 1)) {
			return Mode_Done(&t, 0);
		}
		taken = 1;
		if (!Mode_Sound("WaySound", "WayVolume")) {
			return Mode_Done(&t, 0);
		}
	} else {
		int in = Mode_Read(CTF, "enemies", flags) ? Native_Contains(flags, who) : -1;
		int lead = in > 0 ? (Mode_EntityH(who, e) ? Sd_IsLeader(e) : -1) : in;
		if (lead < 0) {
			return Mode_Done(&t, 0);
		}
		if (lead) {
			if (!Sd_Add(CTF, "flagsEnemies", fe, 1)) {
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
	if (!PORT_OWN(CTF, RemoveFlag, NULL, "v", item)) {
		return Mode_Done(&t, 0);
	}
	int fresh = Ctf_NoFlags();
	if (fresh < 0 || !Mode_Read(CTF, "flagsFriends", ff) || !Mode_Read(CTF, "flagsEnemies", fe)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(win, Native_Compare(ff, fe) > 0);
	if (!Mode_GVarSet("PlayerCTFTeamIsWinning", win) || !Ctf_Waypoint() || !Ctf_Update() ||
		!Mode_Read(CTF, "flagsFriends", ff) || !Mode_Read(CTF, "flags", flags)) {
		return Mode_Done(&t, 0);
	}
	byte end = Sd_Eq(ff, flags);
	if (!end) {
		if (!Mode_Read(CTF, "flagsEnemies", fe) || !Mode_Read(CTF, "flags", flags)) {
			return Mode_Done(&t, 0);
		}
		end = Sd_Eq(fe, flags);
	}
	return Mode_Done(&t, end ? PORT_OWN(CTF, EndOfCTF, NULL, "") : !fresh || PORT_OWN(CTF, CreateFlag, NULL, ""));
}

MODE_FN(Ctf, TakeFlag, 2, Ctf_TakeFlagC(&a[0], &a[1]))

/* SpawnFlags(pos, numFlags): the flags a dead ship carried, thrown from pos; the nearest active flag is the player's
 * way point */
static byte Ctf_SpawnFlagsC(const nativeVal_t *pos, const nativeVal_t *num) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *minName = Mode_V(&t), *minDist = Mode_V(&t), *active = Mode_V(&t);
	long n = 0;
	byte ok = Mode_Range(num, &n);
	for (long i = 0; ok && i < n; i++) {
		/* flagsSpecial.append(CTF.CreateFlag(pos)) on the list read before the call */
		nativeVal_t name, list;
		Native_SetNone(&name);
		ok = Mode_RefG(CTF, "flagsSpecial", &list) && PORT_OWN(CTF, CreateFlag, &name, "v", pos) &&
			Mode_RefCall(&list, "append", &name, 1, NULL);
		Native_Free(&list);
		Native_Free(&name);
	}
	if (!ok || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, Mode_GVarSetS("CurrentWaypointName", ""));
	}
	if (!Mode_Read(CTF, "flagsActive", active)) {
		return Mode_Done(&t, 0);
	}
	int count = Native_Size(active);
	if (count <= 0) {
		return Mode_Done(&t, count == 0);
	}
	Native_SetStr(minName, "");
	Native_SetInt(minDist, 2000000);
	if (!Mode_Read(CTF, "flagsActive", active) || (count = Native_ForCount(active)) < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; ok && i < count; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *ep = Mode_V(&t), *up = Mode_V(&t), *d = Mode_V(&t);
		nativeVal_t *dist = Mode_V(&t);
		ok = Native_SeqItem(active, i, name) && Mode_EntityH(name, e);
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
	int lead = Mode_VGet(e, "LLogicIsLeader", &v) ? Mode_EqI(&v, 1) : -1;
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
		PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", name) && Sd_SpawnAt(usr);
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
	nativeVal_t func;
	byte ok = Sd_FinishRef(&func);
	Sd_Params(win, mode, retry, &params);
	ok = ok && Sd_Remote("ForceRemoteCom", &cad, &who, &func, &params);
	Native_Free(&func);
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

/* zones: the places of the zones, then the names; zonesStates: the state of each zone name (0 neutral, 1 the
 * player's, 2 the enemies') */

/* GetMenuDescription(): the text of the deal with the number of zones */
static byte Dom_GetMenuDescriptionC(nativeVal_t *out) {
	nativeVal_t text, n;
	if (!Mode_Import("string")) {
		return 0;
	}
	Mode_Lang("SuperDeal_Domination_Description", &text);
	nativeVal_t zones;
	Native_SetNone(&n);
	Native_SetNone(&zones);
	int len = Mode_Read(DOM, "zones", &zones) ? Native_Size(&zones) : -1;
	Native_Free(&zones);
	byte ok = len >= 0;
	if (ok) {
		char s[0x20];
		Str_Printf(s, sizeof(s), "%d", len);
		Native_SetStr(&n, s);
		ok = PORT_CALL(string, replace, out, "vsv", &text, "<n1>", &n);
	}
	Native_Free(&text);
	Native_Free(&n);
	return ok;
}

MODE_FN(Dom, GetMenuDescription, 0, Dom_GetMenuDescriptionC(out))

/* Scrap.Preload3DObject(name) of a global of the module */
static byte Sd_PreloadVar(const char *mod, const char *name) {
	nativeVal_t v;
	Native_SetNone(&v);
	byte ok = Mode_Read(mod, name, &v) && Mode_Preload(&v);
	Native_Free(&v);
	return ok;
}

/* InitStatus(LevelPath): the status of the modes and the zone models loaded */
static byte Dom_InitStatusC(void) {
	return Sd_StatusC(DOM, NULL) && Sd_PreloadVar(DOM, "ZoneModelNeutral") && Sd_PreloadVar(DOM, "ZoneModelFriends") &&
		Sd_PreloadVar(DOM, "ZoneModelEnemies");
}

MODE_FN(Dom, InitStatus, 1, Dom_InitStatusC())

/* Init(LevelPath): the camera of the end, the respawn time, both teams in their lists, the leader of the enemies,
 * the start when the player boards; the baton drops when he leaves the ship. The script's `zonesStates = {}` is a
 * local of the function (no global statement), so the table of the zones stays as it is. */
static byte Dom_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Sixth", "SuperDeal_Tenth", NULL };
	nativeVal_t first, team;
	Native_SetNone(&first);
	Native_SetNone(&team);
	byte ok = Mode_Import("Parking") && Mode_Import("Chars") && Sd_CameraFor(libs, "OM") &&
		Sd_PreloadVar(DOM, "BatonModel") && Mode_ModSetGlobal("Chars", "OnMainCharDeath", DOM, "OnPlayerDeath") &&
		Sd_SpawnTime(DOM, "DominationOnDeathSpawnTime") &&
		Sd_PutI(DOM, "zonesCanBeTaken", 1) && PORT_OWN(Domination, CreateEnemies, NULL, "") &&
		PORT_OWN(Domination, CreateFriends, NULL, "") &&
		Mode_GVarSetS("OnDominFriendDeath", "Domination.OnFriendDeath") &&
		Mode_GVarSetS("OnDominEnemyDeath", "Domination.OnEnemyDeath") && Mode_Read(DOM, "enemies", &team) &&
		Sd_MarkShips(&team, "red", "DominationEnemies") && Mode_Read(DOM, "enemies", &team) &&
		Mode_Item(&team, 0, &first) && Mode_GVarSet("LeaderName", &first) && Mode_Read(DOM, "friends", &team) &&
		Sd_MarkShips(&team, "green", "DominationFriends") &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", DOM, "CrazyGamblerStartDomination") &&
		Mode_ModSetGlobal("Parking", "SwitchCharMissionCallback", DOM, "ReleaseBaton");
	Native_Free(&first);
	Native_Free(&team);
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
static byte Sd_BatonFree(const char *mod) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	return Sd_PutI(mod, "userWithBaton", 0) && Mode_Put(mod, "shipWithBaton", NULL, &empty);
}

/* ReleaseBaton(): the player left his ship with the baton: it drops at a random place */
static byte Dom_ReleaseBatonC(void) {
	nativeVal_t user;
	Native_SetNone(&user);
	byte ok = Mode_Read(DOM, "userWithBaton", &user) && (!Native_True(&user) ||
		(Mode_Later(0.5, DOM, "LostBatonMessage", NULL, 0, NULL) && Sd_BatonFree(DOM) &&
		PORT_OWN(Domination, CreateBaton, NULL, "")));
	Native_Free(&user);
	return ok && Dom_HideZones();
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
	byte ok = PORT_CALL(Speech, CalculateSpeechTime, &len, "v", &cad) && Native_Arith('+', &now, &len, &t) &&
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
		ok = Mode_Item(dummy, 2, &a2) && Mode_Item(&a2, 1, &ang) && Mode_Item(dummy, 1, &pos);
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
	if (!ok || !Mode_SaveInt(enemy ? "DominationNumEnemies" : "DominationNumFriends", num) || !Mode_Range(num, &n)) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *sp = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
		nativeVal_t *en = Mode_V(&t);
		Sd_Numbered(enemy ? "Enemy" : "Friend", i, name);
		ok = Mode_Item(spawns, i, sp) && Mode_Item(sp, 0, pos) && Mode_Item(sp, 1, ang) &&
			Sd_Create(name, pos, "Car", e) && Mode_VSetS(e, "WeapList", "15,15,15,15,15,0") &&
			(!enemy || Mode_VSetI(e, "LaunchTaunts", 0)) && Mode_SetAng(e, ang) && (!enemy || i || Mode_VSetI(e, "Money", -1)) &&
			PORT_OWN(Domination, SetProfile, NULL, "vi", e, (long)enemy) && Mode_AppendName(DOM, enemy ? "enemies" : "friends", e) &&
			Mode_VGet(e, "Name", en) && PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 0L, en);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(Dom, CreateEnemies, 0, Dom_CreateTeam(1))
MODE_FN(Dom, CreateFriends, 0, Dom_CreateTeam(0))

/* SLogic.ChangeBatonState(pos, state) (other values through the ref for the errors of "(fff)i") */
static byte Sd_BatonState(const nativeVal_t *pos, long state) {
	double x[3];
	if (!Mode_VecOf(pos, x)) {
		return PORT_CALL(SLogic, ChangeBatonState, NULL, "vi", pos, state);
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
	ok = ok && Mode_Read(mod, "BatonRadius", &v) && Mode_VSet(l, "Radius", &v) && Mode_VSetI(l, "AppearsIfFree", 0) &&
		Mode_VSetI(l, "AutoFace", 0) && Mode_VSetI(l, "OnlyUser", 0) && Mode_VSetF(l, "rTime", 0.25) && Mode_VSetS(l, "OnTake", onTake);
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
	byte ok = Mode_VGet(l2, "Name", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
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
		Mode_RandomVisPoint(&pos);
	}
	byte ok = Sd_Baton(DOM, &pos, "Domination.TakeBaton", &l) && Mode_VSetI(&l, "RadarShow", 1) && Mode_Import("Items") &&
		PORT_CALL(Items, SetSound, NULL, "vs", &l, "TakeBDataPack") && Sd_BatonArrow(&l, &pos, &l2) && Sd_BatonTarget(&l2) &&
		Sd_BatonState(&pos, 0) && Dom_HideZones();
	Native_Free(&pos);
	Native_Free(&l);
	Native_Free(&l2);
	return ok;
}

MODE_KW(Dom, CreateBaton, "|O", "pos") {
	return Dom_CreateBatonC(&v[0]);
}

/* Speech.AddRemoteCom(key, "CrazyGambler", mod.func, ()) (mod.func a value Speech keeps, OBJLIB 5 (9c: its Python
 * function object until functions are native values); its call there takes the entry, Mode_Apply) */
static byte Sd_GamblerKey(const char *key, const char *mod, const char *func) {
	nativeVal_t none, fn;
	if (!Mode_RefG(mod, func, &fn)) {
		return 0;
	}
	Native_SetEmpty(&none, NV_TUPLE);
	byte ok = PORT_CALL(Speech, AddRemoteCom, NULL, "ssOv", key, CG, Native_PyOf(&fn), &none);
	Native_Free(&fn);
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
	nativeVal_t *usr = Mode_V(&t), *v = Mode_V(&t), *fn = Mode_V(&t), *start = Mode_V(&t);
	if (!Mode_Import("Parking") || !Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) ||
		!Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", DOM, "OnVehicle") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int car = Mode_UsrInCar(usr);
	if (car < 0 || (car && !Dom_AimLeader(usr)) || !Mode_Read(DOM, "CrazyGamblerStartDominationFirst", start)) {
		return Mode_Done(&t, 0);
	}
	byte first = Native_True(start);
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
		return Mode_Done(&t, PORT_OWN(Domination, StartDomination, NULL, ""));
	}
	if (!Sd_PutI(DOM, "CrazyGamblerStartDominationFirst", 0)) {
		return Mode_Done(&t, 0);
	}
	nativeVal_t *f = Mode_V(&t);
	byte ok = Mode_RefG(DOM, "StartDomination", f);
	Native_Free(v);
	Mode_SaveVar("DominationStartTextCad", v);
	Native_SetEmpty(fn, NV_TUPLE);
	return Mode_Done(&t, ok && PORT_CALL(Speech, AddRemoteCom, NULL, "vsOv", v, CG, Native_PyOf(f), fn));
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
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *rounds = Mode_V(&t), *zones = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("Menu") || !Mode_Import("RacerScorer") || !Mode_Import("RaceMaker") ||
		!Sd_PutI(DOM, "playerZones", 0) || !Sd_PutI(DOM, "enemyZones", 0) ||
		!Mode_SaveInt("PlayerRounds", a) || !Mode_SaveInt("EnemyRounds", b)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("DominationRounds", rounds);
	if (!PORT_CALL(RacerScorer, InitMissionScorer, NULL, "vvv", a, b, rounds) ||
		!Mode_ModSetGlobal("Menu", "MissionScorerCall", DOM, "MissionScorer") || !PORT_OWN(Domination, MissionScorer, NULL, "i", 0L) ||
		!Mode_Read(DOM, "zones", zones)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(zones);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t pos;
		Native_SetNone(&pos);
		ok = Mode_Read(DOM, "zones", zones) && Mode_Item(zones, i, &pos) && PORT_OWN(Domination, CreateZone, NULL, "iv", (long)i, &pos);
		Native_Free(&pos);
	}
	if (!ok || !Mode_Read(DOM, "zones", zones)) {
		return Mode_Done(&t, 0);
	}
	if (zones->type == NV_LIST) {
		Dom_SetZones(zones);
	} else if (!PORT_CALL(SLogic, SetDominationZones, NULL, "v", zones)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, PORT_OWN(Domination, CreateBaton, NULL, ""));
}

MODE_FN(Dom, StartDomination, 0, Dom_StartDominationC())

/* zonesStates[z] == state (the global read again): 1, 0, -1 with the error */
static int Dom_StateIs(const nativeVal_t *z, long state) {
	nativeVal_t v, states;
	Native_SetNone(&v);
	Native_SetNone(&states);
	int r = Mode_Read(DOM, "zonesStates", &states) && Native_Subscript(&states, z, &v) ? Mode_EqI(&v, state) : -1;
	Native_Free(&v);
	Native_Free(&states);
	return r;
}

/* zonesStates[z] = state in place */
static byte Dom_SetState(const nativeVal_t *z, long state) {
	nativeVal_t v;
	Native_SetInt(&v, state);
	return Mode_DictPut(DOM, "zonesStates", NULL, z, &v);
}

/* the player or the leader took the baton: SLogic.ChangeBatonState((0, 0, 0), state), the holder (ship, or the name of
 * the player's handle usr read then), the crazy gambler's message and the sound */
static byte Dom_Took(long state, long user, const nativeVal_t *ship, const nativeVal_t *usr, const char *key,
	const char *snd, const char *vol) {
	nativeVal_t zero, un;
	Native_SetNone(&un);
	Native_SetEmpty(&zero, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(&zero), 0);
	}
	byte ok = Sd_BatonState(&zero, state) && Sd_PutI(DOM, "userWithBaton", user) &&
		(!usr || Mode_VGet(usr, "Name", &un)) && Mode_Put(DOM, "shipWithBaton", NULL, usr ? &un : ship) && PORT_CALL(Speech, ForceRemoteCom, NULL, "ss", key, CG) &&
		Mode_Sound(snd, vol);
	Native_Free(&zero);
	Native_Free(&un);
	return ok;
}

/* TakeBaton(ItemName, VehicleName): the leader or the player took the baton; with the player the zones he can take
 * show on the radar */
static byte Dom_TakeBatonC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *leader = Mode_V(&t), *usr = Mode_V(&t), *keys = Mode_V(&t), *z = Mode_V(&t);
	cEntity *e;
	if (!Native_GVarGet("LeaderName", leader) || !Mode_Import("Sound") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Sd_Eq(who, leader)) {
		if (!Sd_Del(item) || !Sd_DelArrow(item) || !PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", who) ||
			!Mode_GVarSet("CurrentWaypointName", who) ||
			!Dom_Took(1, 0, who, NULL, "SuperDeal_DominationRemoteMessage6", "EneWaySound", "EneWayVolume")) {
			return Mode_Done(&t, 0);
		}
	} else {
		int u = Mode_UsrNameIs(usr, who);
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
			if (!ok || !Sd_Del(item) || !Sd_DelArrow(item) ||
				!Dom_Took(2, 1, NULL, usr, "SuperDeal_DominationRemoteMessage5", "WaySound", "WayVolume")) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!Mode_Ent(leader, &e)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !e || PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 0L, leader));
}

MODE_FN(Dom, TakeBaton, 2, Dom_TakeBatonC(&a[0], &a[1]))

/* CreateZone(i, pos): the neutral zone i at pos and the WithLife the wingmen guard, whose name replaces the place in
 * zones */
static byte Dom_CreateZoneC(const nativeVal_t *i, const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *name = Mode_V(&t), *l = Mode_V(&t), *ln = Mode_V(&t), *wname = Mode_V(&t);
	nativeVal_t *l2 = Mode_V(&t), *model = Mode_V(&t);
	Mode_Repr(i, rep);
	if (!Mode_Cat(&t, name, 2, Mode_S(&t, "Zone"), rep) || !Sd_Create(name, pos, "Item", l) ||
		!Mode_Read(DOM, "ZoneModelNeutral", model) || !Mode_VModel(l, model) || !Mode_VSetI(l, "Radius", 3000) ||
		!Mode_VSetS(l, "OnTake", "Domination.TakeZone") || !Mode_VSetI(l, "RadarShow", 0) || !Mode_VGet(l, "Name", ln) ||
		!Mode_DictPut(DOM, "zonesStates", NULL, ln, Mode_I(&t, 0))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(ln);
	if (!Mode_VGet(l, "Name", ln) || !Mode_Cat(&t, wname, 2, ln, Mode_S(&t, "_WithLife")) || !Sd_Create(wname, pos, "WithLife", l2)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(ln);
	return Mode_Done(&t, Mode_VGet(l2, "Name", ln) && Mode_DictPut(DOM, "zones", NULL, i, ln));
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
static byte Dom_SetCount(const char *item, const char *key, const char *count, const nativeVal_t *n) {
	nativeVal_t len, text, id, it, c, zones;
	Native_SetNone(&text);
	Native_SetNone(&c);
	Native_SetNone(&zones);
	byte ok = Mode_Read(DOM, count, &c);
	if (ok && !n) {
		int k = Mode_Read(DOM, "zones", &zones) ? Native_Size(&zones) : -1;
		ok = k >= 0;
		Native_SetInt(&len, k);
		n = &len;
	}
	Native_SetInt(&id, 0);
	Native_SetStr(&it, item);
	ok = ok && Dom_Count(key, &c, n, &text) && Mode_ScorerSet(&id, &it, "Text", &text);
	Native_Free(&text);
	Native_Free(&c);
	Native_Free(&zones);
	Native_Free(&it);
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
	nativeVal_t *own = Mode_V(&t), *other = Mode_V(&t), *zones = Mode_V(&t);
	const char *ownVar = enemy ? "enemyZones" : "playerZones", *otherVar = enemy ? "playerZones" : "enemyZones";
	int was = Dom_StateIs(item, enemy ? 2 : 1);
	if (was) {
		Mode_Done(&t, 0);
		return was > 0 ? 0 : -1;
	}
	was = Dom_StateIs(item, enemy ? 1 : 2);
	if (was < 0 || (was && (!Sd_Add(DOM, otherVar, other, -1) ||
		!Dom_SetCount(enemy ? "PlayerZonesText" : "EnemyZonesText", enemy ? "SuperDeal_DominationPlayerZones" :
		"SuperDeal_DominationEnemyZones", otherVar, NULL)))) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (!Mode_EntityH(item, z) || !Mode_Read(DOM, enemy ? "ZoneModelEnemies" : "ZoneModelFriends", model) ||
		!Mode_VModel(z, model) || (!enemy && !Mode_VSetI(z, "RadarShow", 0)) || !Mode_Read(DOM, "zones", zones) ||
		!Mode_Cat(&t, wname, 2, item, Mode_S(&t, "_WithLife")) || !Mode_Index(zones, wname, idx)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	/* SLogic.ChangeZoneState(idx, state) ("ii") */
	Domination_SetZoneState((int)idx->i, enemy ? 2 : 1);
	if (!Mode_Sound(enemy ? "EneWaySound" : "WaySound", enemy ? "EneWayVolume" : "WayVolume") || !Sd_Add(DOM, ownVar, own, 1) ||
		!Dom_SetState(item, enemy ? 2 : 1) || !Dom_SetCount(enemy ? "EnemyZonesText" : "PlayerZonesText", enemy ?
		"SuperDeal_DominationEnemyZones" : "SuperDeal_DominationPlayerZones", ownVar, numZones) || !Sd_Draw() ||
		!Mode_Read(DOM, ownVar, own)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (Native_Compare(own, numZones) < 0) {
		return Mode_Done(&t, PORT_CALL(Speech, ForceRemoteCom, NULL, "ss",
			enemy ? "SuperDeal_DominationRemoteMessage4" : "SuperDeal_DominationRemoteMessage3", CG)) ? 1 : -1;
	}
	/* the round won: the rounds of the side counted in its save variable */
	Native_SetInt(one, 1);
	nativeVal_t *won = enemy ? er : pr;
	if (!Mode_SaveInt("PlayerRounds", enemy ? pr : rep) || !Mode_SaveInt("EnemyRounds", enemy ? rep : er) ||
		!Native_Arith('+', rep, one, won) || !Mode_SaveInt("DominationRounds", dr)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	Native_Free(rep);
	Mode_Repr(won, rep);
	if (!Mode_SetSaveVar(enemy ? "EnemyRounds" : "PlayerRounds", rep) || !Sd_PutI(DOM, "zonesCanBeTaken", 0)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	if (Native_Compare(won, dr) >= 0) {
		return Mode_Done(&t, PORT_OWN(Domination, EndOfDomination, NULL, "i", enemy ? 0L : 1L)) ? 1 : -1;
	}
	if (enemy) {
		return Mode_Done(&t, Sd_Update(pr, er) && Sd_GamblerKey("SuperDeal_DominationRemoteMessage2", DOM, "RestartDomination")) ? 1 : -1;
	}
	/* SScorer.Set(0, "ZonesText", "Text", "%d/%d" % (playerZones, numZones)) */
	Native_SetStr(fmt, "%d/%d");
	Native_SetEmpty(args, NV_TUPLE);
	if (!Mode_Read(DOM, "playerZones", own)) {
		return Mode_Done(&t, 0) ? 1 : -1;
	}
	Native_Copy(Native_Append(args), own);
	Native_Copy(Native_Append(args), numZones);
	Native_SetInt(id, 0);
	Native_SetStr(it, "ZonesText");
	return Mode_Done(&t, Native_Format(fmt, args, text) && Mode_ScorerSet(id, it, "Text", text) && Sd_Update(pr, er) &&
		Sd_Draw() && Sd_GamblerKey("SuperDeal_DominationRemoteMessage1", DOM, "RestartDomination") &&
		Mode_VSetI(usr, "Invulnerable", 1)) ? 1 : -1;
}

/* TakeZone(ItemName, CharName): an enemy with the baton marker or the player with the baton takes a zone; all zones
 * win the round (the deal at DominationRounds), else the next one starts after the message */
static byte Dom_TakeZoneC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *we = Mode_V(&t), *radar = Mode_V(&t), *num = Mode_V(&t);
	nativeVal_t *now = Mode_V(&t), *cad = Mode_V(&t), *len = Mode_V(&t), *mt = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_Import("Menu") || !Mode_Import("Sound") || !Mode_Import("Speech") ||
		!Mode_Read(DOM, "zonesCanBeTaken", v)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(v)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int isUser = Mode_UsrNameIs(usr, who);
	if (isUser > 0) {
		isUser = Mode_Read(DOM, "shipWithBaton", v) && Mode_VGet(usr, "Name", un) ? Sd_Eq(v, un) : -1;
		Native_Free(un);
	}
	if (isUser < 0 || !Mode_EntityH(who, we) || !Mode_VGet(we, "RadarShow", radar)) {
		return Mode_Done(&t, 0);
	}
	byte isEnemy = Mode_EqI(radar, 1);
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
		if (!Mode_Read(DOM, "msgTime", v)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(v, now) >= 0) {
			return Mode_Done(&t, 1);
		}
		double d;
		Mode_Lang("SuperDeal_Domination_SystemMessage", cad);
		return Mode_Done(&t, PORT_CALL(Speech, CalculateSpeechTime, len, "v", cad) && Native_Arith('+', now, len, mt) &&
			Mode_Put(DOM, "msgTime", NULL, mt) && Mode_Read(DOM, "msgTime", v) && Native_ArgDouble(v, &d) &&
			Mode_MsgText(cad, d));
	}
	cEntity *z;
	if (!Mode_Ent(item, &z) || !Mode_Read(DOM, "zones", v)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(v);
	if (n < 0 || !Mode_Read(DOM, "zonesStates", v)) {
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
MODE_FN(Dom, RestartDomination, 0, Mode_Import("SaveGame") && PORT_CALL(SaveGame, Restart, NULL, ""))

/* the baton drops at the dead ship: CreateBaton(dead.Pos) of Domination (race 0) or MortalRace (race 1) */
static byte Sd_BatonAt(byte race, const nativeVal_t *dead) {
	nativeVal_t pos;
	Native_SetNone(&pos);
	byte ok = Mode_VGet(dead, "Pos", &pos) &&
		(race ? PORT_OWN(MortalRace, CreateBaton, NULL, "v", &pos) :
		PORT_OWN(Domination, CreateBaton, NULL, "v", &pos));
	Native_Free(&pos);
	return ok;
}

/* OnPlayerDeath(LooserName, WinnerName): the baton drops where the player died; he comes back after
 * onDeathSpawnTime */
static byte Dom_OnPlayerDeathC(const nativeVal_t *loser) {
	nativeVal_t dead, ship;
	Native_SetNone(&dead);
	Native_SetNone(&ship);
	byte ok = Mode_EntityH(loser, &dead) && Mode_Read(DOM, "shipWithBaton", &ship);
	if (ok && Sd_Eq(&ship, loser)) {
		ok = Sd_BatonFree(DOM) && Sd_BatonAt(0, &dead);
	}
	Native_Free(&dead);
	Native_Free(&ship);
	return ok && Dom_HideZones() && Sd_RespawnLater(DOM, loser);
}

MODE_FN(Dom, OnPlayerDeath, 2, Dom_OnPlayerDeathC(&a[0]))

/* Vehicles.OnDeath(loser, winner); dead = Scrap.GetEntity(loser); dead.EntityList = ""; if loser in list:
 * list.remove(loser) (the start of OnEnemyDeath and OnFriendDeath) */
static byte Dom_Dead(const nativeVal_t *loser, const nativeVal_t *winner, const char *list, nativeVal_t *dead) {
	return Mode_Import("Vehicles") && PORT_CALL(Vehicles, OnDeath, NULL, "vv", loser, winner) && Mode_EntityH(loser, dead) &&
		Mode_VSetS(dead, "EntityList", "") && Sd_RemoveIn(DOM, list, loser);
}

/* OnEnemyDeath(LooserName, WinnerName): an enemy died (the engine's OnDominEnemyDeath); a new one comes after
 * onDeathSpawnTime, the leader drops the baton */
static byte Dom_OnEnemyDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dead = Mode_V(&t), *leader = Mode_V(&t), *is = Mode_V(&t), *empty = Mode_V(&t), *v = Mode_V(&t);
	if (!Dom_Dead(loser, winner, "enemies", dead) || !Native_GVarGet("LeaderName", leader)) {
		return Mode_Done(&t, 0);
	}
	byte isLeader = Sd_Eq(leader, loser);
	Native_SetInt(is, isLeader);
	if (!Mode_Read(DOM, "onDeathSpawnTime", v) || !Sd_LaterV(v, DOM, "RespawnEnemy", is, 1) || !Sd_Del(loser)) {
		return Mode_Done(&t, 0);
	}
	if (!isLeader) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Read(DOM, "shipWithBaton", v)) {
		return Mode_Done(&t, 0);
	}
	Native_SetStr(empty, "");
	if (Sd_Eq(v, leader) && (!Mode_GVarSetS("CurrentWaypointName", "") ||
		!Mode_Put(DOM, "shipWithBaton", NULL, empty) || !Sd_BatonAt(0, dead))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_GVarSetS("LeaderName", ""));
}

MODE_FN(Dom, OnEnemyDeath, 2, Dom_OnEnemyDeathC(&a[0], &a[1]))

/* OnFriendDeath(LooserName, WinnerName): a ship of the player's team died; a new one comes after onDeathSpawnTime */
static byte Dom_OnFriendDeathC(const nativeVal_t *loser, const nativeVal_t *winner) {
	nativeVal_t dead, time;
	Native_SetNone(&dead);
	Native_SetNone(&time);
	byte ok = Dom_Dead(loser, winner, "friends", &dead) && Mode_Read(DOM, "onDeathSpawnTime", &time) &&
		Sd_LaterV(&time, DOM, "RespawnFriend", NULL, 0) && Sd_Del(loser);
	Native_Free(&dead);
	Native_Free(&time);
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
		!Mode_VSetS(e, "WeapList", "15,15,15,15,15,0") || (enemy ? !Mode_VSetI(e, "LaunchTaunts", 0) : !Mode_VSetI(e, "Money", -1)) ||
		!Mode_Item(sp, 1, ang) || !Mode_SetAng(e, ang) || !Mode_VSetS(e, "TgType", enemy ? "red" : "green") ||
		!Mode_VSetS(e, "EntityList", enemy ? "DominationEnemies" : "DominationFriends") || (lead && !Mode_VSetI(e, "Money", -1)) ||
		!PORT_OWN(Domination, SetProfile, NULL, "vi", e, (long)enemy) ||
		(enemy && Native_True(leader) && (!Mode_VGet(e, "Name", name) || !Mode_GVarSet("LeaderName", name)))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_AppendName(DOM, enemy ? "enemies" : "friends", e));
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
	nativeVal_t team;
	Native_SetNone(&team);
	byte ok = Mode_Import("Menu") && Mode_Import("Parking") && Mode_Read(DOM, "enemies", &team) && Dom_Freeze(&team) &&
		Mode_Read(DOM, "friends", &team) && Dom_Freeze(&team);
	Native_Free(&team);
	return ok && Sd_ScorerOff() && Mode_ModNone("Parking", "SwitchCharMissionCallback") &&
		Mode_ModNone("Parking", "SwitchVehicleMissionCallback") &&
		(Native_True(wins) ? PORT_OWN(Domination, PreOnVictory, NULL, "") :
		PORT_OWN(Domination, PreRestartMenu, NULL, ""));
}

MODE_FN(Dom, EndOfDomination, 1, Dom_EndOfDominationC(&a[0]))
MODE_FN(Dom, PreRestartMenu, 0, Sd_PreEnd(0, "DominationFinishPlayerLoosesTextCad", NULL, "Domination", "Domination.RestartDomin"))

/* RestartDomin(id, control): the rounds at 0 and the level again */
static byte Dom_RestartDominC(void) {
	Mode_SaveS("PlayerRounds", "0");
	Mode_SaveS("EnemyRounds", "0");
	return PORT_CALL(SaveGame, Restart, NULL, "");
}

MODE_FN(Dom, RestartDomin, 2, Dom_RestartDominC())
MODE_FN(Dom, PreOnVictory, 0, Sd_PreEnd(1, "DominationFinishPlayerWinsTextCad", NULL, "Domination", ""))

/* a text of the mission scorer: the zones of a side (Y below the scorer's text, on the Xbox lower) */
static byte Dom_ScorerText(const nativeVal_t *id, const char *item, long yXbox, long yPc, const char *key,
	const char *count) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *it = Mode_S(&t, item), *v = Mode_V(&t), *xbox = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *colors = Mode_V(&t), *slot = Mode_V(&t);
	if (!Mode_Read(DOM, "zones", slot)) {
		return Mode_Done(&t, 0);
	}
	int len = Native_Size(slot);
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
		if (!Mode_ModVar("Scorer", "MissionScorerText_Color", colors) || !Mode_Item(colors, i, v) || !Mode_ScorerSet(id, it, rgba[i], v)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* MissionScorer(id): the zones of both sides above the race scorer */
static byte Dom_MissionScorerC(const nativeVal_t *id) {
	return Mode_Import("RacerScorer") &&
		Dom_ScorerText(id, "PlayerZonesText", 80, -5, "SuperDeal_DominationPlayerZones", "playerZones") &&
		Dom_ScorerText(id, "EnemyZonesText", 105, 20, "SuperDeal_DominationEnemyZones", "enemyZones") &&
		PORT_CALL(RacerScorer, MissionScorer, NULL, "v", id);
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

/* BayScorer: the buoys of each racer; nameProfileDict: the profile of each racer name; baton: the baton's handle;
 * prof: the AI profile object of VehiclesNPC, changed in place (OBJLIB 7) */

MODE_FN(Mr, GetMenuDescription, 0, Sd_MenuDescriptionC("SuperDeal_MortalRace_Description", "MortalRaceWaypoints", out))

/* InitStatus(LevelPath): no police and no traffic, the AI reports the places it reached */
static byte Mr_InitStatusC(void) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Sd_StatusC(MR, "OutPolice") && Mode_SetModVar("OutPolice", "PoliceActive", &zero) &&
		Mode_GVarSet("iTrafficNumber", &zero) && Mode_GVarSet("MaxFightingTrafficSize", &zero) &&
		Mode_GVarSetS("OnItemTakenAI", "MortalRace.OnPositionReached");
}

MODE_FN(Mr, InitStatus, 1, Mr_InitStatusC())

/* var of the module = conv(Scrap.GetSaveVar(save)) (conv: int, float or the text) */
static byte Mr_FromSave(const char *var, const char *save, char conv) {
	nativeVal_t s, v;
	Mode_SaveVar(save, &s);
	Native_SetNone(&v);
	byte ok = conv == 'i' ? Native_ToInt(&s, &v) : conv == 'f' ? Native_ToFloat(&s, &v) : (Native_Copy(&v, &s), 1);
	ok = ok && Mode_Put(MR, var, NULL, &v);
	Native_Free(&s);
	Native_Free(&v);
	return ok;
}

/* Init(LevelPath): the race of the save variables (buoys, racers' profile, respawn time), the models, the start when
 * the player boards; the baton drops when he leaves the ship */
static byte Mr_InitC(void) {
	static const char *const libs[] = { "SuperDeal_Second", "SuperDeal_Seventh", "SuperDeal_Twelfth", NULL };
	nativeVal_t none, text, start;
	if (!Mode_Import("Parking") || !Mode_Import("Chars") || !Mode_Import("RaceMaker") || !Sd_CameraFor(libs, "FZ")) {
		return 0;
	}
	Native_SetEmpty(&none, NV_TUPLE);
	Native_SetNone(&start);
	byte ok = Mode_SetModVar("MissionsFuncs", "cameraData", &none) && PORT_CALL(RaceMaker, PreloadCnDnResources, NULL, "") &&
		Sd_PreloadVar(MR, "BatonModel") && Sd_PreloadVar(MR, "WayPointModel") &&
		Mode_ModSetGlobal("Chars", "OnMainCharDeath", MR, "OnPlayerDeath");
	if (ok) {
		Mode_SaveVar("MortalRaceRace", &text);
		ok = Native_EvalText(MR, &text, &start) && Mode_SetModVar("RaceMaker", "StartPos", &start);
		Native_Free(&text);
	}
	ok = ok && Mr_FromSave("numWaypoints", "MortalRaceWaypoints", 'i') && Mr_FromSave("respawnTime", "MortalRaceRespawnTime", 'f') &&
		Mr_FromSave("profileType", "MortalRaceRacersProfile", 's') &&
		Mr_FromSave("FightingFreezeControlAim", "MortalRaceFightingFreezeControlAim", 'i') &&
		Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", MR, "CrazyGamblerStartMortalRace") &&
		Mode_ModSetGlobal("Parking", "SwitchCharMissionCallback", MR, "ReleaseBaton");
	Native_Free(&start);
	return ok;
}

MODE_FN(Mr, Init, 1, Mr_InitC())

/* RaceMaker.StartPos[i][k] (the variable of RaceMaker read again) */
static byte Mr_StartPos(const nativeVal_t *i, long k, nativeVal_t *out) {
	nativeVal_t sp, p;
	Native_SetNone(&sp);
	Native_SetNone(&p);
	byte ok = Mode_ModVar("RaceMaker", "StartPos", &sp) && Native_Subscript(&sp, i, &p) && Mode_Item(&p, k, out);
	Native_Free(&sp);
	Native_Free(&p);
	return ok;
}

/* RaceMaker.StartPos[iStartPos][k] */
static byte Mr_AtStart(long k, nativeVal_t *out) {
	nativeVal_t i;
	Native_SetNone(&i);
	byte ok = Mode_Read(MR, "iStartPos", &i) && Mr_StartPos(&i, k, out);
	Native_Free(&i);
	return ok;
}

/* len(RaceMaker.StartPos): -1 with the error */
static int Mr_Count(void) {
	nativeVal_t sp;
	Native_SetNone(&sp);
	int n = Mode_ModVar("RaceMaker", "StartPos", &sp) ? Native_Size(&sp) : -1;
	Native_Free(&sp);
	return n;
}

/* iStartPos - 1, at least 0 */
static byte Mr_PrevPos(nativeVal_t *out) {
	nativeVal_t one, zero, i;
	Native_SetInt(&one, 1);
	Native_SetInt(&zero, 0);
	Native_SetNone(&i);
	byte ok = Mode_Read(MR, "iStartPos", &i) && Native_Arith('-', &i, &one, out);
	Native_Free(&i);
	if (!ok) {
		return 0;
	}
	if (Native_Compare(out, &zero) < 0) {
		Native_Free(out);
		Native_SetInt(out, 0);
	}
	return 1;
}

/* iStartPos = (iStartPos + 1) % len(RaceMaker.StartPos) */
static byte Mr_NextPos(void) {
	nativeVal_t one, i, n, r, cur;
	Native_SetInt(&one, 1);
	Native_SetNone(&i);
	Native_SetNone(&r);
	Native_SetNone(&cur);
	byte ok = Mode_Read(MR, "iStartPos", &cur) && Native_Arith('+', &cur, &one, &i);
	int count = ok ? Mr_Count() : -1;
	if (count >= 0) {
		Native_SetInt(&n, count);
		ok = Native_Arith('%', &i, &n, &r) && Mode_Put(MR, "iStartPos", NULL, &r);
	} else {
		ok = 0;
	}
	Native_Free(&cur);
	Native_Free(&i);
	Native_Free(&r);
	return ok;
}

/* sp = VehiclesNPC.cShipAI(); VehiclesNPC.SetAIProfile(sp, MortalRaceRacersProfile); the ship races to pos */
static byte Mr_GoTo(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *radius) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *prof = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *boost = Mode_V(&t);
	if (!CALL("VehiclesNPC", "cShipAI", sp, "")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("MortalRaceRacersProfile", prof);
	return Mode_Done(&t, PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "vv", sp, prof) && Mode_Item(pos, 0, x) && Mode_Item(pos, 1, y) &&
		Mode_Item(pos, 2, z) && Mode_VGet(sp, "ControlBoostRace", boost) &&
		PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvvvv", 6L, name, x, y, z, radius, boost));
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, MoveRacersToPos, (pos, BatonRadius)) */
static byte Mr_MoveLater(double delay, const nativeVal_t *pos) {
	nativeVal_t args[2];
	Native_Copy(&args[0], pos);
	Native_SetNone(&args[1]);
	byte ok = Mode_Read(MR, "BatonRadius", &args[1]) && Mode_Later(delay, MR, "MoveRacersToPos", args, 2, NULL);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

/* ReleaseBaton(): the player left his ship with the baton: it drops at the last buoy and every racer goes for it */
static byte Mr_ReleaseBatonC(void) {
	nativeVal_t i, one, zero, pos, v;
	Native_SetNone(&v);
	if (!Mode_Import("RaceMaker") || !Mode_Read(MR, "userWithBaton", &v)) {
		Native_Free(&v);
		return 0;
	}
	if (!Native_True(&v)) {
		Native_Free(&v);
		return 1;
	}
	if (!Mode_Later(0.3, MR, "LostBatonMessage", NULL, 0, NULL) || !Sd_BatonFree(MR) || !Mode_Read(MR, "iStartPos", &v)) {
		Native_Free(&v);
		return 0;
	}
	/* i = iStartPos - 1; if (i < 0): i = len(RaceMaker.StartPos) - 1 */
	Native_SetInt(&one, 1);
	Native_SetInt(&zero, 0);
	Native_SetNone(&i);
	Native_SetNone(&pos);
	byte ok = Native_Arith('-', &v, &one, &i);
	Native_Free(&v);
	if (ok && Native_Compare(&i, &zero) < 0) {
		int n = Mr_Count();
		ok = n >= 0;
		Native_Free(&i);
		Native_SetInt(&i, n - 1);
	}
	ok = ok && Mr_StartPos(&i, 0, &pos) && PORT_OWN(MortalRace, CreateBaton, NULL, "v", &pos) &&
		Mr_MoveLater(0.2, &pos);
	Native_Free(&i);
	Native_Free(&pos);
	return ok;
}

MODE_FN(Mr, ReleaseBaton, 0, Mr_ReleaseBatonC())
MODE_FN(Mr, LostBatonMessage, 0, Sd_LostBaton("Mission_WinMortalRace_SystemMessage5"))

/* CrazyGamblerStartMortalRace(): the player boarded: the racers line up, the crazy gambler's message puts the first
 * buoy */
static byte Mr_CrazyGamblerStartMortalRaceC(void) {
	nativeVal_t cad, none, fn = { 0 };
	if (!Mode_Import("Parking") || !Mode_Import("Speech") || !Sd_ModSetI("Parking", "PlayerShip", "Invulnerable", 0) ||
		!Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", MR, "SetVulnerable")) {
		return 0;
	}
	Mode_SaveVar("MortalRaceStartTextCad", &cad);
	Native_SetEmpty(&none, NV_TUPLE);
	byte ok = Mode_RefG(MR, "CreateFirstWaypoint", &fn) && PORT_CALL(Speech, AddRemoteCom, NULL, "vsOv", &cad, CG, Native_PyOf(&fn), &none);
	Native_Free(&fn);
	Native_Free(&cad);
	return ok && PORT_OWN(MortalRace, InitRace, NULL, "");
}

MODE_FN(Mr, CrazyGamblerStartMortalRace, 0, Mr_CrazyGamblerStartMortalRaceC())

/* CreateBaton(pos): the baton at pos, the mission target on its arrow */
static byte Mr_CreateBatonC(const nativeVal_t *pos) {
	nativeVal_t l, l2;
	Native_SetNone(&l);
	Native_SetNone(&l2);
	byte ok = Sd_Baton(MR, pos, "MortalRace.TakeBaton", &l) && Sd_BatonArrow(&l, pos, &l2) && Mode_Put(MR, "baton", NULL, &l) &&
		Sd_BatonTarget(&l2);
	Native_Free(&l);
	Native_Free(&l2);
	return ok;
}

MODE_FN(Mr, CreateBaton, 1, Mr_CreateBatonC(&a[0]))

/* TakeBaton(ItemName, VehicleName): a racer or the player took the baton: the buoy comes back, the others hunt him */
static byte Mr_TakeBatonC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t), *un = Mode_V(&t), *v = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("Sound") || !Mode_Import("VehiclesNPC") || !Mode_UsrHandle(usr) ||
		!Mode_Read(MR, "baton", g)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(g)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Read(MR, "waypointName", g) || !Mode_EntityH(g, e) || !Mode_VSetI(e, "Active", 1) ||
		!Mode_VSetS(e, "OnTake", "MortalRace.OnCheckPoint") || !Sd_Del(item) || !Sd_DelArrow(item) ||
		!Mode_Put(MR, "baton", NULL, Mode_V(&t)) || !Mode_VGet(e, "Pos", pos)) {
		return Mode_Done(&t, 0);
	}
	int u = Native_True(usr);
	int racer = 1;
	if (u) {
		racer = Mode_VGet(usr, "Name", un) ? !Sd_Eq(who, un) : -1;
	}
	if (racer < 0) {
		return Mode_Done(&t, 0);
	}
	if (racer) {
		if (!Mode_Read(MR, "messageKillShipThrown", g)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(g)) {
			int show = u ? Mode_VIsType(usr, "Car") : 1;
			if (show < 0 || (show && !Sd_SystemMessage("Mission_WinMortalRace_SystemMessage2")) ||
				!Sd_PutI(MR, "messageKillShipThrown", 1)) {
				return Mode_Done(&t, 0);
			}
		}
		int car;
		if (!Mode_Put(MR, "shipWithBaton", NULL, who) || !Mode_EntityH(who, v) ||
			!PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", v, 1L) || !Mode_Read(MR, "BatonRadius", g) ||
			!Mr_GoTo(who, pos, g) || !PORT_OWN(MortalRace, RacersAttackShip, NULL, "v", who) || (car = Mode_UsrInCar(usr)) < 0 ||
			(car && !Mode_VSet(usr, "Target", who)) || !Mode_GVarSet("CurrentWaypointName", who) || !Mode_VSetS(v, "TgType", "red")) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, Mode_Sound("EneWaySound", "EneWayVolume"));
	}
	int me = Mode_UsrNameIs(usr, who);
	if (me <= 0) {
		return Mode_Done(&t, me == 0);
	}
	return Mode_Done(&t, Sd_SystemMessage("Mission_WinMortalRace_SystemMessage1") &&
		Sd_PutI(MR, "messageKillShipThrown", 0) && Sd_PutI(MR, "userWithBaton", 1) && Mode_Put(MR, "shipWithBaton", NULL, who) &&
		PORT_OWN(MortalRace, RacersAttackShip, NULL, "v", who) &&
		Mode_Read(MR, "waypointName", g) && Mode_GVarSet("CurrentWaypointName", g) &&
		Mode_Sound("WaySound", "WayVolume"));
}

MODE_FN(Mr, TakeBaton, 2, Mr_TakeBatonC(&a[0], &a[1]))

/* v.Ammo00 to v.Ammo02 = SWeap.GetFAmmo(a, "Max") (None without the value) */
static byte Mr_FullAmmo(const nativeVal_t *v) {
	static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
	for (int a = 0; a < 3; a++) {
		nativeVal_t x;
		float f;
		if (cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, a), "Max", &f)) {
			Native_SetFloat(&x, (double)f);
		} else {
			Native_SetNone(&x);
		}
		if (!Mode_VSet(v, ammo[a], &x)) {
			return 0;
		}
	}
	return 1;
}

/* RaceMaker.InitShipIAProfile(foe, profileType); ship = RaceMaker.GetRandomShipIAProfile(foe, profileType) (ship the
 * profile) */
static byte Mr_RacerProfile(const nativeVal_t *foe, nativeVal_t *ship) {
	nativeVal_t type;
	Native_SetNone(&type);
	byte ok = Mode_Read(MR, "profileType", &type) && PORT_CALL(RaceMaker, InitShipIAProfile, NULL, "vv", foe, &type) &&
		Mode_Read(MR, "profileType", &type) && PORT_CALL(RaceMaker, GetRandomShipIAProfile, ship, "vv", foe, &type);
	Native_Free(&type);
	return ok;
}

/* h.to = h.from of a handle value */
static byte Mr_Copy(const nativeVal_t *h, const char *from, const char *to) {
	nativeVal_t v;
	Native_SetNone(&v);
	byte ok = Mode_VGet(h, from, &v) && Mode_VSet(h, to, &v);
	Native_Free(&v);
	return ok;
}

/* InitRace(createFirstWaypoint = 0): the racers of MortalRaceRacers in a circle around the first start position,
 * static until the start, with full ammo; the AI profile of the first one is kept for the fights */
static byte Mr_InitRaceC(const nativeVal_t *first) {
	static const char *const mods[] = { "Alarm", "Speech", "Parking", "RaceMaker", "RacerScorer", "VehiclesNPC", "math", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *empty = Mode_V(&t), *text = Mode_V(&t), *racers = Mode_V(&t), *prof = Mode_V(&t);
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetEmpty(empty, NV_DICT);
	if (!Sd_PutI(MR, "iStartPos", 0) || !Mode_Put(MR, "BayScorer", NULL, empty) || !Sd_PutI(MR, "YourScore", -1) ||
		!Mode_ListSet("Race", "OnDeath", "MortalRace.RacerKilled") ||
		!Mode_ListSet("Race", "OnDamage", "MortalRace.RacerDamaged") || !Sd_PutI(MR, "iStartPos", 0)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("MortalRaceRacers", text);
	if (!Native_EvalText(MR, text, racers)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(racers);
	byte ok = n >= 0, profile = 1;
	for (int i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *start = Mode_V(&t), *ang = Mode_V(&t), *rot = Mode_V(&t), *spos = Mode_V(&t), *foe = Mode_V(&t);
		nativeVal_t *name = Mode_V(&t), *v = Mode_V(&t), *vn = Mode_V(&t), *ship = Mode_V(&t), *x = Mode_V(&t);
		nativeVal_t *base = Mode_V(&t), *r = Mode_V(&t);
		/* SVec.Rotate3D((0, 2200, 0), (0, start[1], 90 + 360 * i / n)): the angle in C ints, as the script's ints */
		Native_SetEmpty(base, NV_TUPLE);
		Native_SetInt(Native_Append(base), 0);
		Native_SetInt(Native_Append(base), 2200);
		Native_SetInt(Native_Append(base), 0);
		ok = Mr_AtStart(0, start) && Mr_AtStart(1, ang);
		if (ok) {
			Native_SetEmpty(r, NV_TUPLE);
			Native_SetInt(Native_Append(r), 0);
			Native_Copy(Native_Append(r), ang);
			Native_SetInt(Native_Append(r), 90 + 360 * i / n);
		}
		ok = ok && Mode_Rotate(base, r, rot) && Mode_VecAdd(start, rot, spos) && Mode_Item(racers, i, foe) &&
			Mode_Cat(&t, name, 2, Mode_S(&t, "MortalRace_Racer_"), foe) && Sd_Create(name, spos, "Car", v) &&
			Mode_VGet(v, "Name", vn) && Mode_DictPut(MR, "nameProfileDict", NULL, vn, foe) &&
			Mr_RacerProfile(foe, ship);
		if (ok && profile) {
			profile = 0;
			ok = PORT_CALL(VehiclesNPC, GetIAProf, x, "v", ship) && Mode_Put(MR, "prof", NULL, x) &&
				Mode_Read(MR, "prof", prof) && Mode_VGet(prof, "CadenceShoot", Mode_V(&t)) &&
				Mode_Put(MR, "prevCadenceShoot", NULL, &t.v[t.n - 1]) && Mode_Read(MR, "prof", prof) &&
				Mode_VGet(prof, "ControlAim", Mode_V(&t)) && Mode_Put(MR, "prevControlAim", NULL, &t.v[t.n - 1]);
		}
		Native_Free(vn);
		Native_Free(ang);
		ok = ok && PORT_CALL(VehiclesNPC, SetIAProf, NULL, "vv", v, ship) && Mode_VGet(v, "Name", vn) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 0L, vn) && Mode_VSetS(v, "TgType", "none") && Mode_VSetS(v, "EntityList", "Race") &&
			Mr_AtStart(1, ang) && Mode_SetAng(v, ang) && Mode_VSetI(v, "Money", -1) && Mr_Copy(v, "Pos", "MovePos") &&
			Mr_Copy(v, "Ang", "MoveAng") && Mode_VSetF(v, "MoveTime", 0.001) && Mode_VSetI(v, "LaunchTaunts", 0) && Mr_FullAmmo(v);
		Native_Free(vn);
		ok = ok && Mode_VGet(v, "Name", vn) && Mode_DictPut(MR, "BayScorer", NULL, vn, Mode_I(&t, 0)) &&
			Mode_AppendName(MR, "racers", v);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok && (!Native_True(first) || PORT_OWN(MortalRace, CreateFirstWaypoint, NULL, "")));
}

MODE_KW(Mr, InitRace, "|O", "createFirstWaypoint") {
	Mode_KwDefI(v, given, 0, 0);
	return Mr_InitRaceC(&v[0]);
}

/* CreateFirstWaypoint(): the buoy of the start at the first start position (once), the way point; the buoy */
static byte Mr_CreateFirstWaypointC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("Speech") || !PORT_OWN(MortalRace, AbortRemoteMessages, NULL, "") ||
		!Mode_Read(MR, "waypointName", g) || !Mode_EntityH(g, out)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(out)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(out);
	return Mode_Done(&t, Mr_AtStart(0, pos) && Sd_Create(Mode_S(&t, "CheckPoint"), pos, "Item", out) &&
		Mode_Read(MR, "WayPointModel", g) && Mode_VModel(out, g) && Mode_VSetS(out, "OnTake", "MortalRace.OnRaceStart") &&
		Mode_VSetI(out, "RadarShow", 2) && Mode_VSetI(out, "Radius", 2400) && Mr_AtStart(1, ang) && Mode_SetAng(out, ang) &&
		Mode_VSetI(out, "FadeInEffect", 0) && Mode_VSetI(out, "FadeOutEffect", 0) && Mode_VGet(out, "Name", name) &&
		Mode_Put(MR, "waypointName", NULL, name) && Mode_Read(MR, "waypointName", g) && Mode_GVarSet("CurrentWaypointName", g));
}

MODE_FN(Mr, CreateFirstWaypoint, 0, Mr_CreateFirstWaypointC(out))

/* OnRaceStart(ItemName, VehicleName): the player at the first buoy: the camera travels to him, the count down,
 * RaceGo at its end */
static byte Mr_OnRaceStartC(const nativeVal_t *item, const nativeVal_t *who) {
	nativeVal_t usr, tm, args[2];
	if (!Mode_Import("RaceMaker") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	int u = Mode_UsrNameIs(&usr, who);
	Native_Free(&usr);
	if (u <= 0) {
		return u == 0;
	}
	Native_SetNone(&tm);
	Native_Copy(&args[0], item);
	Native_Copy(&args[1], who);
	byte ok = PORT_CALL(RaceMaker, StartCameraTravelling, &tm, "vv", item, who) && Sd_LaterV(&tm, MR, "RaceGo", args, 2) &&
		PORT_CALL(RaceMaker, CreateRaceCountDown, NULL, "");
	Native_Free(&tm);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

MODE_FN(Mr, OnRaceStart, 2, Mr_OnRaceStartC(&a[0], &a[1]))

/* RacerScorer.InitMissionScorer(0, 0, `numWaypoints`); Menu.MissionScorerCall = RacerScorer.MissionScorer; the scorer
 * drawn */
static byte Mr_Scorer(void) {
	nativeVal_t limit, n;
	Native_SetNone(&n);
	if (!Mode_Read(MR, "numWaypoints", &n)) {
		Native_Free(&n);
		return 0;
	}
	Mode_Repr(&n, &limit);
	Native_Free(&n);
	byte ok = PORT_CALL(RacerScorer, InitMissionScorer, NULL, "iiv", 0L, 0L, &limit) &&
		Mode_ModTo("Menu", "MissionScorerCall", "RacerScorer", "MissionScorer") && Sd_Draw();
	Native_Free(&limit);
	return ok;
}

/* RaceGo(ItemName, VehicleName): the start: the first buoy counts, the scorer, the check of the hunters every 2 s */
static byte Mr_RaceGoC(const nativeVal_t *item, const nativeVal_t *who) {
	return Mode_Import("RaceMaker") && Mode_Import("RacerScorer") && PORT_CALL(RaceMaker, AbortCameraTravelling, NULL, "") &&
		PORT_CALL(RaceMaker, DestroyRaceCountDown, NULL, "") && PORT_OWN(MortalRace, OnCheckPoint, NULL, "vv", item, who) && Mr_Scorer() &&
		Mode_Later(2.0, MR, "CheckShipsState", NULL, 0, NULL);
}

MODE_FN(Mr, RaceGo, 2, Mr_RaceGoC(&a[0], &a[1]))

/* the hunters of CheckShipsState: the attackers far from the ship with the baton and the waiting ones near it */
typedef struct {
	const nativeVal_t *ship;
	nativeVal_t attack, freeze;
} mrHunt_t;

/* g * g of a global of the module */
static byte Mr_Square(const char *name, nativeVal_t *out) {
	nativeVal_t a, b;
	Native_SetNone(&a);
	Native_SetNone(&b);
	byte ok = Mode_Read(MR, name, &a) && Mode_Read(MR, name, &b) && Native_Arith('*', &a, &b, out);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

static int Mr_Hunter(const nativeVal_t *e, const nativeVal_t *name, void *arg) {
	mrHunt_t *h = arg;
	modeVals_t t = { .n = 0 };
	nativeVal_t *en = Mode_V(&t), *sn = Mode_V(&t), *state = Mode_V(&t), *sp = Mode_V(&t), *ep = Mode_V(&t), *d = Mode_V(&t);
	nativeVal_t *dsq = Mode_V(&t), *lim = Mode_V(&t);
	(void)name;
	if (!Mode_VGet(e, "Name", en) || !Mode_VGet(h->ship, "Name", sn)) {
		return Mode_Done(&t, 0) - 1;
	}
	if (Sd_Eq(en, sn)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(en);
	if (!Mode_VGet(e, "Name", en) || !Mode_AIState(en, state)) {
		return Mode_Done(&t, 0) - 1;
	}
	int st = Mode_EqI(state, 2) ? 2 : Mode_EqI(state, 8) ? 8 : 0;
	if (!st) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VGet(h->ship, "Pos", sp) || !Mode_VGet(e, "Pos", ep) || !Mode_VecSub(sp, ep, d) || !Mode_VecModSqr(d, dsq) ||
		!Mr_Square(st == 2 ? "MIN_CHASE_DIST" : "MAX_FIGHTING_FREEZE_DIST", lim)) {
		return Mode_Done(&t, 0) - 1;
	}
	int c = Native_Compare(dsq, lim);
	if (st == 2 ? c > 0 : c < 0) {
		Native_Free(en);
		if (!Mode_VGet(e, "Name", en)) {
			return Mode_Done(&t, 0) - 1;
		}
		Native_Copy(Native_Append(st == 2 ? &h->attack : &h->freeze), en);
	}
	return Mode_Done(&t, 1);
}

/* CheckShipsState(): one racer keeps attacking the ship with the baton, the other attackers wait some buoys ahead;
 * the waiting ones it reached attack again with their profile (every 2 s) */
static byte Mr_CheckShipsStateC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *g = Mode_V(&t);
	mrHunt_t h = { .ship = ship };
	Native_SetEmpty(&h.attack, NV_LIST);
	Native_SetEmpty(&h.freeze, NV_LIST);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Read(MR, "shipWithBaton", g) && Mode_EntityH(g, ship);
	if (ok && Native_True(ship)) {
		ok = Sd_EachInList("Race", Mr_Hunter, &h);
		if (ok && h.attack.n > 1) {
			ok = Mode_Import("RaceMaker");
			for (int i = 1; ok && i < h.attack.n; i++) {
				int mark = t.n;
				nativeVal_t *ahead = Mode_V(&t), *at = Mode_V(&t), *at2 = Mode_V(&t), *cnt = Mode_V(&t), *ip = Mode_V(&t);
				nativeVal_t *pos = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *rad = Mode_V(&t);
				int count;
				ok = Mode_Read(MR, "iStartPos", g) && Mode_Read(MR, "NUM_WAYPOINTS_AHEAD", ahead) && Native_Arith('+', g, ahead, at) && Native_Arith('+', at, Mode_I(&t, i - 1), at2) &&
					(count = Mr_Count()) >= 0;
				if (ok) {
					Native_SetInt(cnt, count);
					ok = Native_Arith('%', at2, cnt, ip) && Mr_StartPos(ip, 0, pos) && Mode_Item(pos, 0, x) && Mode_Item(pos, 1, y) &&
						Mode_Item(pos, 2, z) && Mode_Read(MR, "WaypRadius", rad) &&
						PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvvv", 1L, &h.attack.items[i], x, y, z, rad);
				}
				Mode_Release(&t, mark);
			}
		}
		for (int i = 0; ok && i < h.freeze.n; i++) {
			int mark = t.n;
			nativeVal_t *e = Mode_V(&t), *v = Mode_V(&t), *sn = Mode_V(&t);
			ok = Mode_EntityH(&h.freeze.items[i], e);
			if (ok && Native_True(e)) {
				ok = Mode_Read(MR, "prevCadenceShoot", v) && Mode_Read(MR, "prof", g) && Mode_VSet(g, "CadenceShoot", v) &&
					Mode_Read(MR, "prevControlAim", v) && Mode_Read(MR, "prof", g) && Mode_VSet(g, "ControlAim", v) &&
					Mode_Read(MR, "prof", g) && PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", e, g) && Mode_VGet(ship, "Name", sn) &&
					PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, &h.freeze.items[i], sn);
			}
			Mode_Release(&t, mark);
		}
	}
	Native_Free(&h.attack);
	Native_Free(&h.freeze);
	return Mode_Done(&t, ok && Mode_Later(2.0, MR, "CheckShipsState", NULL, 0, NULL));
}

MODE_FN(Mr, CheckShipsState, 0, Mr_CheckShipsStateC())

/* prof.CadenceShoot = mod.<cadence>; prof.ControlAim = mod.<aim>; VehiclesNPC.ModAIProff(e, prof) */
static byte Mr_Prof(const char *cadence, const char *aim, const nativeVal_t *e) {
	nativeVal_t v, prof;
	Native_SetNone(&v);
	Native_SetNone(&prof);
	byte ok = Mode_Read(MR, cadence, &v) && Mode_Read(MR, "prof", &prof) && Mode_VSet(&prof, "CadenceShoot", &v) &&
		Mode_Read(MR, aim, &v) && Mode_Read(MR, "prof", &prof) && Mode_VSet(&prof, "ControlAim", &v);
	ok = ok && (!e || (Mode_Read(MR, "prof", &prof) && PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", e, &prof)));
	Native_Free(&v);
	Native_Free(&prof);
	return ok;
}

/* OnPositionReached(VehicleName): a racer reached the buoy where it waits: it fights there, shooting often and badly */
static byte Mr_OnPositionReachedC(const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *state = Mode_V(&t), *e = Mode_V(&t), *sn = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Import("VehiclesNPC") || !Mode_Read(MR, "shipWithBaton", g) || !Mode_EntityH(g, ship)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(ship)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_AIState(who, state)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_EqI(state, 1)) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Mode_Import("VehiclesNPC") && Mr_Prof("FightingFreezeCadenceShoot", "FightingFreezeControlAim", NULL) &&
		Mode_EntityH(who, e) && Mode_Read(MR, "prof", g) && PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", e, g) &&
		Mode_VGet(ship, "Name", sn) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 8L, who, sn));
}

MODE_FN(Mr, OnPositionReached, 1, Mr_OnPositionReachedC(&a[0]))

/* RacerScorer.UpdateMissionScorer(0, YourScore, BestScorer); the scorer drawn */
static byte Mr_Update(void) {
	nativeVal_t your, best;
	Native_SetNone(&your);
	Native_SetNone(&best);
	byte ok = Mode_Read(MR, "YourScore", &your) && Mode_Read(MR, "BestScorer", &best) && Sd_Update(&your, &best);
	Native_Free(&your);
	Native_Free(&best);
	return ok && Sd_Draw();
}

/* the player took a buoy (with the baton, or his first at the start) */
static byte Mr_PlayerCheckPoint(nativeVal_t *l, const nativeVal_t *usrName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *ang = Mode_V(&t), *your = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Read(MR, "YourScore", your)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(your, Mode_I(&t, -1)) > 0) {
		/* a buoy counts only with the baton */
		if (!Mode_Read(MR, "shipWithBaton", g)) {
			return Mode_Done(&t, 0);
		}
		if (!Sd_Eq(g, usrName)) {
			return Mode_Done(&t, 1);
		}
	}
	if (!Mode_Sound("WaySound", "WayVolume") || !Sd_Add(MR, "YourScore", your, 1)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_EqI(your, 0)) {
		/* the first buoy: the baton at the next start position, the racers go for it, the buoy one further */
		if (!Mr_NextPos() || !Mr_AtStart(0, pos) || !PORT_OWN(MortalRace, CreateBaton, NULL, "v", pos) ||
			!Mode_Read(MR, "BatonRadius", g) || !PORT_OWN(MortalRace, MoveRacersToPos, NULL, "vv", pos, g) ||
			!Mr_NextPos()) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(l)) {
			Native_Free(l);
			if (!PORT_OWN(MortalRace, CreateFirstWaypoint, l, "")) {
				return Mode_Done(&t, 0);
			}
		}
		Native_Free(pos);
		return Mode_Done(&t, Mr_AtStart(0, pos) && Mode_VSet(l, "Pos", pos) && Mr_AtStart(1, ang) && Mode_SetAng(l, ang) &&
			Mode_VSetI(l, "Active", 0));
	}
	if (!Mode_Read(MR, "YourScore", your) || !Mode_Read(MR, "numWaypoints", g)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(your, g) >= 0) {
		return Mode_Done(&t, Mode_VDel(l) && Sd_ScorerOff() && PORT_OWN(MortalRace, PreOnVictory, NULL, ""));
	}
	return Mode_Done(&t, Mr_Update() && PORT_OWN(MortalRace, MoveWaypoint, NULL, ""));
}

/* a racer with the baton took a buoy: the race lost at numWaypoints, else the best racer's count and the next buoy */
static byte Mr_RacerCheckPoint(const nativeVal_t *l, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *r = Mode_V(&t), *e = Mode_V(&t), *cad = Mode_V(&t), *foe = Mode_V(&t), *params = Mode_V(&t);
	nativeVal_t *keys = Mode_V(&t), *pr = Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *res = Mode_V(&t), *bay = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Read(MR, "shipWithBaton", g)) {
		return Mode_Done(&t, 0);
	}
	if (!Sd_Eq(g, who)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VSetI(l, "Active", 0) || !Mode_Read(MR, "BayScorer", bay) || !Native_Subscript(bay, who, n) ||
		!Native_Arith('+', n, Mode_I(&t, 1), r) || !Mode_DictPut(MR, "BayScorer", NULL, who, r) ||
		!Mode_Sound("EneWaySound", "EneWayVolume") || !Mode_Read(MR, "BayScorer", bay)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(n);
	if (!Native_Subscript(bay, who, n) || !Mode_Read(MR, "numWaypoints", g)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(n, g) >= 0) {
		/* the race lost */
		nativeVal_t *func = Mode_V(&t);
		if (!Mode_VDel(l) || !Mode_EntityH(who, e) || !PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", e, 0L) ||
			!Sd_ScorerOff() || !Sd_FinishRef(func)) {
			return Mode_Done(&t, 0);
		}
		Sd_Params(0, "MortalRace", "MortalRace.RestartRace", params);
		Mode_SaveVar("MortalRaceFinishPlayerLoosesTextCad", cad);
		Mode_SaveVar("MortalRaceFinishPlayerLoosesTextFoe", foe);
		return Mode_Done(&t, Sd_Remote("ForceRemoteCom", cad, foe, func, params) && PORT_OWN(MortalRace, EndOfMortalRace, NULL, ""));
	}
	/* BestScorer = the most buoys of a racer */
	if (!Sd_PutI(MR, "BestScorer", 0) || !Mode_Keys(MR, "BayScorer", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int count = Native_ForCount(keys);
	byte ok = count >= 0;
	for (int i = 0; ok && i < count; i++) {
		nativeVal_t k, c;
		Native_SetNone(&c);
		ok = Native_SeqItem(keys, i, &k) && Mode_Read(MR, "BayScorer", bay) && Native_Subscript(bay, &k, &c) &&
			Mode_Read(MR, "BestScorer", g);
		if (ok && Native_Compare(&c, g) > 0) {
			Native_Free(&c);
			ok = Mode_Read(MR, "BayScorer", bay) && Native_Subscript(bay, &k, &c) && Mode_Put(MR, "BestScorer", NULL, &c);
		}
		Native_Free(&k);
		Native_Free(&c);
	}
	return Mode_Done(&t, ok && PORT_OWN(MortalRace, MoveWaypoint, res, "") && Mode_Unpack(res, 2, pr) &&
		Mode_Read(MR, "BatonRadius", g) && Mr_GoTo(who, &pr[0], g) && Mr_Update());
}

/* OnCheckPoint(ItemName, VehicleName): a buoy taken. The player's first one (YourScore -1 to 0) puts the baton at the
 * next start position; with the baton the player or a racer counts it, numWaypoints win (or lose) the race */
static byte Mr_OnCheckPointC(const nativeVal_t *item, const nativeVal_t *who) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usrName = Mode_S(&t, ""), *usr = Mode_V(&t), *l = Mode_V(&t), *your = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("RacerScorer") || !Mode_Import("Sound") || !Mode_Import("VehiclesNPC") ||
		!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr)) {
		Native_Free(usrName);
		if (!Mode_VGet(usr, "Name", usrName)) {
			return Mode_Done(&t, 0);
		}
	}
	/* before the player's first buoy the racers' buoys only start the count */
	if (!Mode_Read(MR, "YourScore", your)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_EqI(your, -1) && !Sd_Eq(who, usrName)) {
		return Mode_Done(&t, !Mode_Read(MR, "YourScore", your) ? 0 : !Mode_EqI(your, -1) || Sd_PutI(MR, "YourScore", 0));
	}
	if (!Mode_EntityH(item, l)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr) && Sd_Eq(usrName, who)) {
		return Mode_Done(&t, Mr_PlayerCheckPoint(l, usrName));
	}
	int has = Mode_HasKey(MR, "BayScorer", 1, who);
	return Mode_Done(&t, has <= 0 ? has == 0 : Mr_RacerCheckPoint(l, who));
}

MODE_FN(Mr, OnCheckPoint, 2, Mr_OnCheckPointC(&a[0], &a[1]))
/* Restart(): the score table of a lost race */
MODE_FN(Mr, Restart, 0, PORT_CALL(MissionsFuncs, OnSuperDealFinish, NULL, "iss", 2L, "MortalRace", "MortalRace.RestartRace"))

/* MoveWaypoint(): the buoy at the next start position, active; (its Pos, its Radius) */
static byte Mr_MoveWaypointC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mr_NextPos() || !Mode_Read(MR, "waypointName", g) || !Mode_EntityH(g, l) || !Mr_AtStart(0, pos) || !Mode_VSet(l, "Pos", pos) || !Mr_AtStart(1, ang) ||
		!Mode_SetAng(l, ang) || !Mode_VSetS(l, "OnTake", "MortalRace.OnCheckPoint") || !Mode_VSetI(l, "Active", 1)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(out, NV_TUPLE);
	return Mode_Done(&t, Mode_VGet(l, "Pos", Native_Append(out)) && Mode_VGet(l, "Radius", Native_Append(out)));
}

MODE_FN(Mr, MoveWaypoint, 0, Mr_MoveWaypointC(out))

/* EndOfMortalRace(): the buoy gone, the racers back to the traffic */
static byte Mr_EndOfMortalRaceC(void) {
	nativeVal_t e, name;
	Native_SetNone(&name);
	byte ok = Mode_Import("Vehicles") && Mode_ModNone("Vehicles", "OnDeathCallback") && Sd_ScorerOff() &&
		Mode_Read(MR, "waypointName", &name) && Mode_EntityH(&name, &e);
	Native_Free(&name);
	if (!ok) {
		return 0;
	}
	ok = (!Native_True(&e) || Mode_VDel(&e)) && Mode_GVarSetS("OnItemTakenAI", "") &&
		PORT_OWN(MortalRace, MoveRacersToTraffic, NULL, "");
	Native_Free(&e);
	return ok;
}

MODE_FN(Mr, EndOfMortalRace, 0, Mr_EndOfMortalRaceC())

static int Mr_Collect(const nativeVal_t *e, const nativeVal_t *name, void *players) {
	(void)name;
	return Mode_VGet(e, "Name", Native_Append(players)) ? 1 : -1;
}

/* MoveRacersToTraffic(): the racers become safe traffic; the race state at its start values */
static byte Mr_MoveRacersToTrafficC(void) {
	nativeVal_t players, empty;
	Native_SetEmpty(&players, NV_LIST);
	byte ok = Sd_EachInList("Race", Mr_Collect, &players);
	for (int i = 0; ok && i < players.n; i++) {
		nativeVal_t e, n;
		Native_SetNone(&e);
		Native_SetNone(&n);
		ok = Mode_EntityH(&players.items[i], &e);
		if (ok && Native_True(&e)) {
			ok = Mode_VSetS(&e, "TgType", "none") && Mode_VSetF(&e, "MoveTime", 0.0) && Mode_VSetS(&e, "EntityList", "Traffic") &&
				Mode_VSetI(&e, "Invulnerable", 1) && Mode_VGet(&e, "Name", &n) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivi", 3L, &n, 1L);
		}
		Native_Free(&e);
		Native_Free(&n);
	}
	Native_Free(&players);
	Native_SetStr(&empty, "");
	return ok && Sd_PutI(MR, "YourScore", -1) && Mode_Put(MR, "waypointName", NULL, &empty) && Sd_PutI(MR, "iStartPos", 0);
}

MODE_FN(Mr, MoveRacersToTraffic, 0, Mr_MoveRacersToTrafficC())

/* MoveRacersToPos(pos, radius): every active racer races to pos */
typedef struct {
	const nativeVal_t *pos, *radius;
} mrPlace_t;

static int Mr_Race(const nativeVal_t *e, const nativeVal_t *name, void *arg) {
	mrPlace_t *p = arg;
	nativeVal_t act;
	Native_SetNone(&act);
	if (!Mode_VGet(e, "Active", &act)) {
		return -1;
	}
	byte on = Native_True(&act);
	Native_Free(&act);
	return !on || (Mode_VSetS(e, "TgType", "green") && Mode_VSetF(e, "MoveTime", 0.0) && Mr_GoTo(name, p->pos, p->radius)) ? 1 : -1;
}

static byte Mr_MoveRacersToPosC(const nativeVal_t *pos, const nativeVal_t *radius) {
	mrPlace_t p = { pos, radius };
	return Mode_Import("RacerScorer") && Mode_Import("VehiclesNPC") && Sd_EachInList("Race", Mr_Race, &p) && Mr_Update();
}

MODE_FN(Mr, MoveRacersToPos, 2, Mr_MoveRacersToPosC(&a[0], &a[1]))

/* RacersAttackShip(VehicleName): every racer attacks the ship with the baton, with the profile's aim against the
 * player and a bad one against a racer */
typedef struct {
	const nativeVal_t *ship;
	byte usr;
} mrAttack_t;

static int Mr_Attack(const nativeVal_t *e, const nativeVal_t *name, void *arg) {
	mrAttack_t *a = arg;
	if (!a->usr && Sd_Eq(name, a->ship)) {
		return 1;
	}
	nativeVal_t prof;
	Native_SetNone(&prof);
	int r = Mode_Read(MR, "prof", &prof) && PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", e, &prof) &&
		PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, name, a->ship) ? 1 : -1;
	Native_Free(&prof);
	return r;
}

static byte Mr_RacersAttackShipC(const nativeVal_t *ship) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *name = Mode_V(&t), *aim = Mode_V(&t), *prof = Mode_V(&t);
	mrAttack_t a = { .ship = ship };
	cEntity *e;
	if (!Mode_Import("VehiclesNPC") || !Mode_UsrHandle(usr) || !Mode_ListGet("Race", "First", name) || !Mode_Ent(name, &e)) {
		return Mode_Done(&t, 0);
	}
	int u = Mode_UsrNameIs(usr, ship);
	if (u < 0) {
		return Mode_Done(&t, 0);
	}
	a.usr = (byte)u;
	if (u) {
		if (!Mode_Read(MR, "prevControlAim", aim)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetInt(aim, 2);
	}
	return Mode_Done(&t, Mode_Read(MR, "prof", prof) && Mode_VSet(prof, "ControlAim", aim) &&
		Sd_EachInList("Race", Mr_Attack, &a));
}

MODE_FN(Mr, RacersAttackShip, 1, Mr_RacersAttackShipC(&a[0]))

/* the baton drops where the ship of dead (a handle value) is: MR.CreateBaton(dead.Pos), the racers go for it 0.2 s
 * later */
static byte Mr_DropBaton(const nativeVal_t *dead) {
	nativeVal_t pos;
	Native_SetNone(&pos);
	byte ok = Sd_BatonAt(1, dead) && Mode_VGet(dead, "Pos", &pos) && Mr_MoveLater(0.2, &pos);
	Native_Free(&pos);
	return ok;
}

/* OnPlayerDeath(LooserName, WinnerName): the player drops the baton where he died; he comes back after respawnTime */
static byte Mr_OnPlayerDeathC(const nativeVal_t *loser) {
	nativeVal_t e, g, args[3];
	Native_SetNone(&e);
	Native_SetNone(&g);
	for (int i = 0; i < 3; i++) {
		Native_SetNone(&args[i]);
	}
	byte ok = Mode_EntityH(loser, &e) && Mode_Read(MR, "shipWithBaton", &g);
	if (ok && Sd_Eq(loser, &g)) {
		ok = Sd_BatonFree(MR) && Mr_DropBaton(&e);
	}
	Native_Copy(&args[0], loser);
	ok = ok && Mode_VGet(&e, "Pos", &args[1]) && Mode_VGet(&e, "Ang", &args[2]) && Mode_Read(MR, "respawnTime", &g) &&
		Sd_LaterV(&g, MR, "RespawnPlayer", args, 3);
	Native_Free(&e);
	Native_Free(&g);
	for (int i = 0; i < 3; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}

MODE_FN(Mr, OnPlayerDeath, 2, Mr_OnPlayerDeathC(&a[0]))

/* RacerKilled(LooserName, WinnerName): a racer died (it drops the baton when it had it); it comes back after
 * respawnTime */
static byte Mr_RacerKilledC(const nativeVal_t *loser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *radar = Mode_V(&t), *empty = Mode_S(&t, ""), *args = Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *g = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !PORT_CALL(Vehicles, OnDeath, NULL, "vv", loser, winner) ||
		!Sd_RemoveIn(MR, "racers", loser) || !Mode_EntityH(loser, e)) {
		return Mode_Done(&t, 0);
	}
	int drop = Native_True(e);
	if (drop) {
		drop = Mode_VGet(e, "RadarShow", radar) ? Mode_EqI(radar, 1) : -1;
	}
	if (drop < 0 || (drop && (!Mode_Put(MR, "shipWithBaton", NULL, empty) || !Mr_DropBaton(e)))) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(&args[0], loser);
	if (!Mode_Read(MR, "nameProfileDict", g) || !Native_Subscript(g, loser, &args[1]) || !Mode_Read(MR, "respawnTime", g) ||
		!Sd_LaterV(g, MR, "RespawnRacer", args, 2) || !Sd_Del(loser)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(MR, "nameProfileDict", 1, loser);
	return Mode_Done(&t, has <= 0 ? has == 0 : Mode_DictDel(MR, "nameProfileDict", NULL, loser));
}

MODE_FN(Mr, RacerKilled, 2, Mr_RacerKilledC(&a[0], &a[1]))

/* RacerDamaged(LooserName, WinnerName, Damage): the player hit a racer before the start: the race starts from the
 * first buoy */
static byte Mr_RacerDamagedC(const nativeVal_t *winner) {
	nativeVal_t usr, un, g;
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	Native_SetNone(&un);
	Native_SetNone(&g);
	byte ok = !Native_True(&usr) || Mode_VGet(&usr, "Name", &un);
	if (ok && Native_True(&usr) && Sd_Eq(&un, winner) && (ok = Mode_Read(MR, "YourScore", &g)) && Mode_EqI(&g, -1)) {
		ok = Mode_ListSet("Race", "OnDamage", "") && Mr_Scorer() && Mode_Read(MR, "waypointName", &g) &&
			PORT_OWN(MortalRace, OnCheckPoint, NULL, "vv", &g, winner);
	}
	Native_Free(&usr);
	Native_Free(&un);
	Native_Free(&g);
	return ok;
}

MODE_FN(Mr, RacerDamaged, 3, Mr_RacerDamagedC(&a[1]))

/* RespawnRacer(racerName, foeType): the racer again at the last start position while the race runs; it attacks the
 * ship with the baton, else goes for the baton or the player */
typedef struct {
	const nativeVal_t *v;
	byte found;
} mrRed_t;

static int Mr_RedFirst(const nativeVal_t *e, const nativeVal_t *name, void *arg) {
	mrRed_t *r = arg;
	nativeVal_t tg, vn;
	Native_SetNone(&tg);
	Native_SetNone(&vn);
	int red = Mode_VGet(e, "TgType", &tg) ? Native_IsStr(&tg, "red") : -1;
	if (red > 0) {
		red = Mode_VGet(r->v, "Name", &vn) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, &vn, name) ? 1 : -1;
		r->found = 1;
	}
	Native_Free(&tg);
	Native_Free(&vn);
	return red < 0 ? -1 : !red;
}

static byte Mr_RespawnRacerC(const nativeVal_t *name, const nativeVal_t *foe) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *w = Mode_V(&t), *prev = Mode_V(&t), *pos = Mode_V(&t), *v = Mode_V(&t), *vn = Mode_V(&t), *ship = Mode_V(&t);
	nativeVal_t *bp = Mode_V(&t), *usr = Mode_V(&t), *un = Mode_V(&t), *g = Mode_V(&t);
	mrRed_t red = { .v = v };
	if (!Mode_Import("RaceMaker") || !Mode_Import("VehiclesNPC") || !Mode_Import("OutMap") ||
		!Mode_Read(MR, "waypointName", g) || !Mode_EntityH(g, w)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(w)) {
		return Mode_Done(&t, 1);
	}
	if (!Mr_PrevPos(prev) || !Mr_StartPos(prev, 0, pos) || !Sd_Create(name, pos, "Car", v) || !Mode_VGet(v, "Name", vn) ||
		!Mode_DictPut(MR, "nameProfileDict", NULL, vn, foe) || !Mr_RacerProfile(foe, ship) ||
		!PORT_CALL(VehiclesNPC, SetIAProf, NULL, "vv", v, ship) ||
		!PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 0L, vn) ||
		!Mode_VSetS(v, "TgType", "green") || !Mode_VSetS(v, "EntityList", "Race") || !Mode_VSetI(v, "Money", -1) ||
		!Mode_VSetI(v, "LaunchTaunts", 0) || !Mr_FullAmmo(v) || !Mode_AppendName(MR, "racers", v) ||
		!Sd_EachInList("Race", Mr_RedFirst, &red)) {
		return Mode_Done(&t, 0);
	}
	if (red.found) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Read(MR, "baton", g)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(vn);
	if (Native_True(g)) {
		return Mode_Done(&t, Mode_Read(MR, "baton", g) && Mode_VGet(g, "Pos", bp) && Mode_VGet(v, "Name", vn) &&
			Mode_Read(MR, "BatonRadius", g) && Mr_GoTo(vn, bp, g));
	}
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr)) {
		Native_Free(usr);
		return Mode_Done(&t, Mode_VGet(v, "Name", vn) && Mode_UsrHandle(usr) && Mode_VGet(usr, "Name", un) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, vn, un));
	}
	return Mode_Done(&t, Mode_VGet(v, "Name", vn) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivi", 3L, vn, 0L));
}

MODE_FN(Mr, RespawnRacer, 2, Mr_RespawnRacerC(&a[0], &a[1]))

/* the racer with the waypoint marker gets the target "" (racerWithWaypoint of the script is never set) */
static int Mr_Marker(const nativeVal_t *e, const nativeVal_t *name, void *arg) {
	nativeVal_t v;
	(void)name;
	(void)arg;
	Native_SetNone(&v);
	int m = Mode_VGet(e, "RadarShow", &v) ? Mode_EqI(&v, 1) : -1;
	Native_Free(&v);
	if (m > 0 && !Mode_VSetS(e, "Target", "")) {
		return -1;
	}
	return m < 0 ? -1 : !m;
}

/* RespawnPlayer(Usrname, pos, ang): the player's ship where it died, at the last start position when the baton lies
 * there */
static byte Mr_RespawnPlayerC(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *ang) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *bp = Mode_V(&t), *d = Mode_V(&t), *dsq = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *rf = Mode_V(&t), *i = Mode_V(&t), *sp = Mode_V(&t), *sa = Mode_V(&t), *a2 = Mode_V(&t), *g = Mode_V(&t);
	const nativeVal_t *spawnPos = pos, *spawnAng = ang;
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !Mode_Import("RaceMaker") || !Mode_EntityH(name, usr) ||
		!Mode_VGet(usr, "Name", un) || !PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", un) || !Mode_Read(MR, "baton", g)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(g)) {
		if (!Mode_Read(MR, "baton", g) || !Mode_VGet(g, "Pos", bp) || !Mode_VecSub(bp, pos, d) ||
			!Mode_VecModSqr(d, dsq) || !Mr_Square("BatonRadius", r) || !Native_ToFloat(r, rf)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(dsq, rf) < 0) {
			if (!Mr_PrevPos(i) || !Mr_StartPos(i, 0, sp) || !Mr_StartPos(i, 1, sa)) {
				return Mode_Done(&t, 0);
			}
			Native_SetEmpty(a2, NV_TUPLE);
			Native_SetInt(Native_Append(a2), 0);
			Native_Copy(Native_Append(a2), sa);
			Native_SetInt(Native_Append(a2), 0);
			spawnPos = sp;
			spawnAng = a2;
		}
	}
	return Mode_Done(&t, Mode_VSet(usr, "Pos", spawnPos) && Mode_VSet(usr, "Ang", spawnAng) &&
		Sd_EachInList("Race", Mr_Marker, NULL));
}

MODE_FN(Mr, RespawnPlayer, 3, Mr_RespawnPlayerC(&a[0], &a[1], &a[2]))

/* PreOnVictory(): the race over, the player safe, the last message, then the score table */
static byte Mr_PreOnVictoryC(void) {
	nativeVal_t cad, foe, params, func = { 0 };
	if (!PORT_OWN(MortalRace, EndOfMortalRace, NULL, "") || !Sd_UsrSafe() || !Mode_GVarSetS("OnItemTakenAI", "")) {
		return 0;
	}
	Mode_SaveVar("MortalRaceFinishPlayerWinsTextCad", &cad);
	Mode_SaveVar("MortalRaceFinishPlayerWinsTextFoe", &foe);
	Sd_Params(1, "MortalRace", "", &params);
	byte ok = Sd_FinishRef(&func) && Sd_Remote("ForceRemoteCom", &cad, &foe, &func, &params);
	Native_Free(&func);
	Native_Free(&cad);
	Native_Free(&foe);
	Native_Free(&params);
	return ok;
}

MODE_FN(Mr, PreOnVictory, 0, Mr_PreOnVictoryC())

PyMethodDef g_MortalRaceMethods[] = {
	{ "GetMenuDescription", Mr_GetMenuDescription, 1, NULL },
	{ "InitStatus", Mr_InitStatus, 1, NULL },
	{ "Init", Mr_Init, 1, NULL },
	{ "ReleaseBaton", Mr_ReleaseBaton, 1, NULL },
	{ "LostBatonMessage", Mr_LostBatonMessage, 1, NULL },
	{ "SetInvulnerableStatus", Sd_SetInvulnerable, 1, NULL },
	{ "AbortRemoteMessages", Sd_AbortRemoteMessages, 1, NULL },
	{ "CrazyGamblerStartMortalRace", Mr_CrazyGamblerStartMortalRace, 1, NULL },
	{ "SetVulnerable", Sd_SetVulnerable, 1, NULL },
	{ "CreateBaton", Mr_CreateBaton, 1, NULL },
	{ "TakeBaton", Mr_TakeBaton, 1, NULL },
	{ "InitRace", (PyCFunction)(void (*)(void))Mr_InitRace, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateFirstWaypoint", Mr_CreateFirstWaypoint, 1, NULL },
	{ "OnRaceStart", Mr_OnRaceStart, 1, NULL },
	{ "RaceGo", Mr_RaceGo, 1, NULL },
	{ "CheckShipsState", Mr_CheckShipsState, 1, NULL },
	{ "OnPositionReached", Mr_OnPositionReached, 1, NULL },
	{ "OnCheckPoint", Mr_OnCheckPoint, 1, NULL },
	{ "Restart", Mr_Restart, 1, NULL },
	{ "RestartRace", Sd_Restart, 1, NULL },
	{ "MoveWaypoint", Mr_MoveWaypoint, 1, NULL },
	{ "EndOfMortalRace", Mr_EndOfMortalRace, 1, NULL },
	{ "MoveRacersToTraffic", Mr_MoveRacersToTraffic, 1, NULL },
	{ "MoveRacersToPos", Mr_MoveRacersToPos, 1, NULL },
	{ "RacersAttackShip", Mr_RacersAttackShip, 1, NULL },
	{ "OnPlayerDeath", Mr_OnPlayerDeath, 1, NULL },
	{ "RacerKilled", Mr_RacerKilled, 1, NULL },
	{ "RacerDamaged", Mr_RacerDamaged, 1, NULL },
	{ "RespawnRacer", Mr_RespawnRacer, 1, NULL },
	{ "RespawnPlayer", Mr_RespawnPlayer, 1, NULL },
	{ "PreOnVictory", Mr_PreOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

PORTS(SuperDeals)
