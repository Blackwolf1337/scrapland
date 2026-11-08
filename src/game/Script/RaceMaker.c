/*
 * The RaceMaker module in C (scripts/missions/racemaker.py, issue #2): the races and combats of the challenges on the
 * outdoor maps. InitRace builds a race from a MapRace table of checkpoints (StartPos): the challenged pilot and its
 * partners, the checkpoint and its trigger, the timer to get into a ship. Taking the first checkpoint shows the
 * camera travelling and the count down (OnRaceStart, RaceGo); every checkpoint moves to the next place
 * (OnCheckPoint, OnNearCheckPoint steers the rivals) until one side reaches WayPointlimit, the player leaves, dies,
 * fires a counter measure or starts the alarm (EndOfRace): money or lives, the revenge, the ships back into the
 * traffic. InitCombat sets the challenged ships against the player; NPCCombatKilled counts them down.
 *
 * The state and the tables stay in the module, because the missions and the map race tables read and set them
 * (StartPos, iStartPos, nextStartPos, Players, BayScorer, FoesScorer, YourScore, WayPointlimit, SkipTimer,
 * NumOponNotGoNextItem, ChallengeFoeType, ChallengerName, challenged_NPCs_Names, NPC_START_ITEM, prevCamera,
 * cameraItemName, cameraVehicleName, LastCmTime, levelPathFind, nextIAProfile, challengePartners, RaceSpeachers,
 * AISHIPS, the count down sounds, the hooks EndOfRaceCallback, OnPlayerWinsRace, OnPlayerLoosesRace,
 * OnRaceGoCallback, OnCheckPointCallback, OnInitRace, OnPreInitRace, OnInitialTaunt and GetDealType), with the module
 * code (the save variables and the entity list it creates). The scorer is data/menus/race.def.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the state and the
 * tables where the script reads them and write them back at once, the lists and dicts changed in place (OBJLIB 5).
 * The save and engine variables, the money, the language texts, the camera name and the entity lists are C;
 * Scrap.Set, SetMoney, SetAlarm, IncSaveVar, UsrEntity, GetEntity, CreateEntity, Preload3DObject, the dummy search,
 * the scheduler, SInput.Bind, SScorer.Set and SetMsgText, SSound.Play and LoadSound and SAI.GetStateVehicle go through
 * their seams, and the racers, checkpoints and cameras are handle values (OBJLIB 7). The other ports (Challenge,
 * Speech, Menu, SaveGame, VehiclesNPC, OutMap, RacerScorer, string), SAI.SetStateVehicle, InitVehicleRace and
 * AddVehicleRace, Scrap.SetCam, the call function of Scrap, SVec.GetAngle and CProd and SInput.GetActionSet stay by
 * name (OBJLIB 3, 7); the hooks, the races of MapRace and the module's own functions are called as the values the
 * script calls; the scorer file takes its $names as a dict (OBJLIB 6).
 */
#include <limits.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define RM "RaceMaker"

static const char race_def[] = {
#embed "../../../data/menus/race.def"
	, 0 };

static const char *const rm_mercs[] = { "MercenaryA", "MercenaryB", "MercenaryC", NULL };

/* a global of the module as its functions read it (out None before) */
static byte Rm_G(const char *name, nativeVal_t *out) {
	return Mode_G(RM, name, out);
}

/* a function of the module as its functions call it (OBJLIB 3: through its global) */
#define Rm_Call(func, out, ...) Native_CallGlobal(RM, func, out, __VA_ARGS__)

/* x == one of the texts (the == of the script): 1, 0, -1 */
static int Rm_InS(const nativeVal_t *x, const char *const *texts) {
	for (; *texts; texts++) {
		int r = LvN_EqS(x, *texts);
		if (r) {
			return r;
		}
	}
	return 0;
}

/* Scrap.GetSaveVar("HumphreyRaceTutorial") != "1" */
static byte Rm_NoTutorial(void) {
	nativeVal_t sv;
	Mode_SaveVar("HumphreyRaceTutorial", &sv);
	byte r = !LvN_EqS(&sv, "1");
	Native_Free(&sv);
	return r;
}

/* ShowSystemMessage(string.replace(Scrap.GetLangStr(key), "<n>", Scrap.GetSaveVar("Challenge.Money"))) (OBJLIB 3:
 * string.replace by name, ShowSystemMessage through the global) */
static byte Rm_MoneyMsg(const char *key) {
	nativeVal_t lang, money, text = { 0 };
	PyObject *fn = Mode_GlobalPy(RM, "ShowSystemMessage");
	Mode_Lang(key, &lang);
	Mode_SaveVar("Challenge.Money", &money);
	byte ok = fn && PORT_CALL(string, replace, &text, "vsv", &lang, "<n>", &money) && Native_Call(fn, NULL, "v", &text);
	Py_XDECREF(fn);
	Native_Free(&lang);
	Native_Free(&money);
	Native_Free(&text);
	return ok;
}

/* Speech.AddRemoteCom(prefix + "Mercs" or prefix + ChallengeFoeType, ChallengeFoeType) (OBJLIB 3) */
static byte Rm_FoeCom(const char *prefix) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *foe = Mode_V(&t), *key = Mode_V(&t), *who = Mode_V(&t);
	if (!Rm_G("ChallengeFoeType", foe)) {
		return Mode_Done(&t, 0);
	}
	int merc = Rm_InS(foe, rm_mercs);
	return Mode_Done(&t, merc >= 0 && Mode_Cat(&t, key, 2, Mode_S(&t, prefix), merc ? Mode_S(&t, "Mercs") : foe) &&
		Rm_G("ChallengeFoeType", who) && Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, who));
}

/* the revenge of a race: Revenge.Type Race, the foe, the last winner, the number of enemies */
static byte Rm_Revenge(const char *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *foe = Mode_V(&t), *num = Mode_V(&t);
	if (!Mode_SetSaveVar("Revenge.Type", Mode_S(&t, "Race")) || !Rm_G("ChallengeFoeType", foe) ||
		!Mode_SetSaveVar("Revenge.Foe", foe) || !Mode_SetSaveVar("Revenge.LastWinner", Mode_S(&t, winner))) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.NumEnemies", num);
	return Mode_Done(&t, Mode_SetSaveVar("Revenge.NumEnemies", num));
}

/* the hook of the module when it is set (OnPlayerLoosesRace(), OnPlayerWinsRace()) */
static byte Rm_Hook(const char *name) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_GObj(RM, name, &fn) && (!Native_True(&fn) || Mode_CallArgs(&fn, NULL, NULL, 0));
	Native_Free(&fn);
	return ok;
}

/* 2 * int(Scrap.GetSaveVar("Challenge.Money")) */
static byte Rm_Double(nativeVal_t *out) {
	nativeVal_t two, i = { 0 };
	Native_SetInt(&two, 2);
	byte ok = Mode_SaveInt("Challenge.Money", &i) && Mode_Arith('*', &two, &i, out);
	Native_Free(&i);
	return ok;
}

/* Menu.DrawScorer(0, SInput.GetActionSet()) (OBJLIB 3, 7: by name) */
static byte Rm_Draw(void) {
	nativeVal_t set = { 0 };
	byte ok = Native_CallMod("SInput", "GetActionSet", &set, "") && Native_CallMod("Menu", "DrawScorer", NULL, "iv", 0L, &set);
	Native_Free(&set);
	return ok;
}

/* the player wins lives against the mercenaries: twice the bet, up to MaxPlayerLives */
static byte Rm_WinLives(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lives = Mode_V(&t), *n = Mode_V(&t), *nl = Mode_V(&t), *max = Mode_V(&t), *rep = Mode_V(&t);
	if (!Rm_Double(lives) || !Mode_SaveInt("Player.NumLives", n) || !Mode_Arith('+', n, lives, nl) ||
		!Native_GVarGet("MaxPlayerLives", max)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_Cmp(nl, ">", max);
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(c ? max : nl, rep);
	return Mode_Done(&t, Mode_SetSaveVar("Player.NumLives", rep) && Rm_Draw() && Rm_MoneyMsg("MercsSystemWin"));
}

/* Scrap.SetMoney(Scrap.GetMoney() + money); SaveGame.SaveMoney() */
static byte Rm_AddMoney(const nativeVal_t *money) {
	nativeVal_t m, sum = { 0 };
	Native_SetInt(&m, Mode_GetMoney());
	byte ok = Mode_Arith('+', &m, money, &sum) && Mode_SetMoneySeam(&sum) && Native_CallMod("SaveGame", "SaveMoney", NULL, "");
	Native_Free(&sum);
	return ok;
}

/* Scrap.IncSaveVar(prefix + group) */
static byte Rm_IncGroup(const char *prefix, const nativeVal_t *group) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	const char *s;
	return Mode_Done(&t, Mode_Cat(&t, name, 2, Mode_S(&t, prefix), group) && Native_ArgStr(name, 1, &s) && Mode_IncSaveVar(s));
}

/* Challenge.groups[ChallengeFoeType] */
static byte Rm_Group(nativeVal_t *out) {
	nativeVal_t groups = { 0 }, foe = { 0 };
	byte ok = LvN_ModGet("Challenge", "groups", &groups) && Rm_G("ChallengeFoeType", &foe) && Mode_Sub(&groups, &foe, out);
	Native_Free(&groups);
	Native_Free(&foe);
	return ok;
}

/* ShowSystemMessage(cad): a system message for the time it takes to read */
static byte Rm_ShowSystemMessage(const nativeVal_t *cad) {
	return Mode_Import("Speech") && Mode_SpeechMsg(cad);
}

/* NPCCombatKilled(LooserName, WinnerName): the OnDeath of the enemies of a combat; when the last one dies the player
 * wins lives (the mercenaries) or twice the bet, the revenge is set and the challenge ends (OBJLIB 3: Challenge by
 * name) */
static byte Rm_NPCCombatKilled(const nativeVal_t *looser) {
	static const char *const mods[] = { "string", "Menu", "SaveGame", "Challenge" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *npcs = Mode_V(&t), *group = Mode_V(&t), *money = Mode_V(&t), *foe = Mode_V(&t), *num = Mode_V(&t);
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	int in = Rm_G("challenged_NPCs_Names", npcs) ? Native_Contains(npcs, looser) : -1;
	if (in < 0 || (in && !Mode_Remove(RM, "challenged_NPCs_Names", looser))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(npcs);
	int n = Rm_G("challenged_NPCs_Names", npcs) ? Mode_Len(npcs) : -1;
	if (n) {
		return Mode_Done(&t, n > 0);
	}
	if (!Rm_Group(group)) {
		return Mode_Done(&t, 0);
	}
	byte ok = LvN_EqS(group, "Mercs") ? Rm_WinLives() : Rm_Double(money) && Rm_AddMoney(money) && Rm_MoneyMsg("RaceSystemWin");
	if (!ok || !Rm_IncGroup("Stats.Combat.", group) || !Rm_IncGroup("Stats.TotalCombats.", group) ||
		!Mode_SetSaveVar("Revenge.Type", Mode_S(&t, "Combat")) || !Rm_G("ChallengeFoeType", foe) ||
		!Mode_SetSaveVar("Revenge.Foe", foe)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.NumEnemies", num);
	return Mode_Done(&t, Mode_SetSaveVar("Revenge.NumEnemies", num) && Native_CallMod("Challenge", "CleanChallenge", NULL, "") &&
		Native_CallMod("Challenge", "SetRevenge", NULL, "i", 0L));
}

/* the entity named by the engine variable CurrentWaypointName (plus suffix) without its OnTake, deleted */
static byte Rm_DelWaypoint(const char *suffix) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *full = Mode_V(&t), *cw = Mode_V(&t);
	if (!Native_GVarGet("CurrentWaypointName", name) || (suffix && !Mode_Cat(&t, full, 2, name, Mode_S(&t, suffix))) ||
		!Mode_EntitySeam(suffix ? full : name, cw)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(cw) || (LvN_SetS(cw, "OnTake", "") && Mode_VDel(cw)));
}

/* DeleteCurrentWaypoint(): the checkpoint and its trigger go */
static byte Rm_DeleteCurrentWaypoint(void) {
	return Rm_DelWaypoint(NULL) && Rm_DelWaypoint("_Trigg");
}

/* the vehicle name back into the traffic (police ships the cops); OBJLIB 3, 7: string.find and SAI.SetStateVehicle by
 * name */
static byte Rm_ToTraffic(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *veh = Mode_V(&t), *zero = Mode_V(&t), *model = Mode_V(&t), *at = Mode_V(&t), *vname = Mode_V(&t);
	if (!Mode_EntitySeam(name, veh)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(veh)) {
		return Mode_Done(&t, 1);
	}
	Native_SetFloat(zero, 0.0);
	int cop = Mode_VSet(veh, "MoveTime", zero) && LvN_SetS(veh, "TgType", "None") && Mode_VObj(veh, "Model", model) &&
		PORT_CALL(string, find, at, "vs", model, "SPoli") ? Mode_CmpI(at, ">=", 0) : -1;
	return Mode_Done(&t, cop >= 0 && LvN_SetS(veh, "EntityList", cop ? "OutCop" : "Traffic") && Mode_VObj(veh, "Name", vname) &&
		Native_CallMod("SAI", "SetStateVehicle", NULL, "iv", 3L, vname));
}

/* fn(name) for each name of the list global var of the module */
static byte Rm_Each(const char *var, byte (*fn)(const nativeVal_t *, const void *), const void *arg) {
	nativeVal_t list = { 0 };
	int n = Rm_G(var, &list) ? Mode_Loop(&list) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name = { 0 };
		ok = Native_SeqItem(&list, i, &name) && fn(&name, arg);
		Native_Free(&name);
	}
	Native_Free(&list);
	return ok;
}

static byte Rm_ToTrafficEach(const nativeVal_t *name, const void *arg) {
	(void)arg;
	return Rm_ToTraffic(name);
}

/* MovePlayersToTraffic(): the rivals join the traffic (police ships the cops) */
static byte Rm_MovePlayersToTraffic(void) {
	return Mode_Import("string") && Rm_Each("Players", Rm_ToTrafficEach, NULL) && Mode_GVarSetI("DoResetOutPolice", 1);
}

/* the challenged ship name back to the police or the traffic after the player left (police: the foe is the police) */
static byte Rm_LeftShip(const nativeVal_t *name, const void *arg) {
	nativeVal_t v = { 0 }, foe = { 0 };
	(void)arg;
	byte ok = Mode_EntitySeam(name, &v);
	if (ok && Native_True(&v)) {
		ok = Rm_G("ChallengeFoeType", &foe);
		byte police = ok && LvN_EqS(&foe, "Police");
		ok = ok && LvN_SetS(&v, "EntityList", police ? "OutCop" : "Traffic") && LvN_SetS(&v, "TgType", police ? "green" : "None");
	}
	Native_Free(&v);
	Native_Free(&foe);
	return ok;
}

/* v.Money = v.Money - money of the challenged ship name (arg the money) */
static byte Rm_PayShip(const nativeVal_t *name, const void *arg) {
	nativeVal_t v = { 0 }, m = { 0 }, r = { 0 };
	byte ok = Mode_EntitySeam(name, &v) && (!Native_True(&v) || (Mode_VObj(&v, "Money", &m) &&
		Mode_Arith('-', &m, (const nativeVal_t *)arg, &r) && Mode_VSet(&v, "Money", &r)));
	Native_Free(&v);
	Native_Free(&m);
	Native_Free(&r);
	return ok;
}

/* the money message of a lost or cancelled race (mercs: the key of the mercenaries; else the race key when not in
 * Humphrey's tutorial) */
static byte Rm_GroupMsg(const nativeVal_t *group, const char *mercs, const char *race) {
	return LvN_EqS(group, "Mercs") ? Rm_MoneyMsg(mercs) : !Rm_NoTutorial() || Rm_MoneyMsg(race);
}

/* the race of the player won (OBJLIB 3: Challenge by name) */
static byte Rm_RaceWon(nativeVal_t *group) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t);
	if (!Rm_Double(money) || !Rm_Each("challenged_NPCs_Names", Rm_PayShip, money) ||
		!(LvN_EqS(group, "Mercs") ? Rm_WinLives() : Rm_AddMoney(money) && (!Rm_NoTutorial() || Rm_MoneyMsg("RaceSystemWin"))) ||
		(Rm_NoTutorial() && (!Rm_Revenge("Player") || !Native_CallMod("Challenge", "SetRevenge", NULL, "i", 0L)))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(group);
	return Mode_Done(&t, Rm_Group(group) && Rm_IncGroup("Stats.Race.", group) && Rm_IncGroup("Stats.TotalRaces.", group) &&
		Rm_Hook("OnPlayerWinsRace"));
}

/* the race of the player lost */
static byte Rm_RaceLost(const nativeVal_t *group) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sv = Mode_V(&t), *foe = Mode_V(&t), *key = Mode_V(&t), *who = Mode_V(&t);
	Mode_SaveVar("Revenge.Type", sv);
	int before = LvN_EqS(sv, "");
	if (before) {
		Native_Free(sv);
		Mode_SaveVar("Revenge.LastWinner", sv);
		before = LvN_EqS(sv, "Player");
	}
	return Mode_Done(&t, (!before || Rm_GroupMsg(group, "MercsSystemLoose", "RaceSystemLoose")) && Rm_G("ChallengeFoeType", foe) &&
		Mode_Cat(&t, key, 2, Mode_S(&t, "RaceLoose"), foe) && Rm_G("ChallengeFoeType", who) &&
		Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, who) && Rm_Revenge("Foe") &&
		Native_CallMod("Challenge", "SetRevenge", NULL, "") && Rm_Hook("OnPlayerLoosesRace"));
}

/* the result of a race (veh the winner or a reason) */
static byte Rm_Result(const nativeVal_t *veh) {
	static const char *const cancels[] = { "Alarm", "Killed", "CounterMeasure", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *group = Mode_V(&t), *lang = Mode_V(&t), *money = Mode_V(&t), *v = Mode_V(&t), *ctrl = Mode_V(&t);
	if (!Rm_Group(group)) {
		return Mode_Done(&t, 0);
	}
	int cancel = Rm_InS(veh, cancels);
	int left = cancel == 0 ? LvN_EqS(veh, "Left") : 0;
	if (cancel) {
		return Mode_Done(&t, Rm_GroupMsg(group, "MercsSystemCancel1", "RaceSystemCancel1") && Rm_FoeCom("RaceInterrupted") &&
			Rm_Revenge("Foe") && Native_CallMod("Challenge", "SetRevenge", NULL, "") && Rm_Hook("OnPlayerLoosesRace"));
	}
	if (left) {
		Mode_Lang("RaceSystemCancel2", lang);
		return Mode_Done(&t, (!Rm_NoTutorial() || Rm_Call("ShowSystemMessage", NULL, "v", lang)) && Rm_FoeCom("RaceLeft") &&
			Rm_Revenge("Foe") && Native_CallMod("Challenge", "SetRevenge", NULL, "") &&
			Mode_SaveInt("Challenge.Money", money) && Rm_AddMoney(money) && Rm_Each("challenged_NPCs_Names", Rm_LeftShip, NULL) &&
			Rm_Hook("OnPlayerLoosesRace"));
	}
	if (!Mode_EntitySeam(veh, v) || !Mode_VObj(v, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, LvN_EqI(ctrl, 0) ? Rm_RaceWon(group) : Rm_RaceLost(group));
}

/* EndOfRace(ItemName, VehicleName): the end of a race; VehicleName is the winner, or "Alarm", "Killed",
 * "CounterMeasure", "Left" (or None for no result). EndOfRaceCallback replaces the result (OBJLIB 3: the other ports
 * by name, OBJLIB 5: the callbacks of Menu, Parking and Alarm set to None in their modules) */
static byte Rm_EndOfRace(const nativeVal_t *item, const nativeVal_t *veh) {
	static const char *const mods[] = { "Speech", "Parking", "Alarm", "Menu", "SaveGame", "Challenge", "string" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *cb = Mode_V(&t), *list = Mode_V(&t), *args = Mode_V(&t);
	Mode_V(&t);
	for (int i = 0; i < 7; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_EntitySeam(item, e) || !Mode_Bind(Mode_I(&t, 0), "Racer", "CM", "") || !Mode_GVarSetI("isActionMusic", 0) ||
		!LvN_GObj(RM, "EndOfRaceCallback", cb)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (Native_True(cb)) {
		/* args and the value after it: (item, veh) */
		Native_Copy(args, item);
		Native_Copy(args + 1, veh);
		ok = Mode_CallArgs(cb, NULL, args, 2);
	} else {
		ok = veh->type == NV_NONE || Rm_Result(veh);
	}
	Native_SetEmpty(list, NV_LIST);
	return Mode_Done(&t, ok && LvN_ModNone("Menu", "MissionScorerCall") && Rm_Draw() &&
		Rm_Call("DeleteCurrentWaypoint", NULL, "") && Rm_Call("MovePlayersToTraffic", NULL, "") &&
		Mode_Put(RM, "challenged_NPCs_Names", NULL, list) && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
		LvN_ModNone("Parking", "SwitchVehicleMissionCallback") && LvN_ModNone("Alarm", "AbortMissionCallback") &&
		Mode_Import("Challenge") && Native_CallMod("Challenge", "CleanChallenge", NULL, ""));
}

/* the rival name joins the race of the AI and becomes a target */
static byte Rm_RaceShip(const nativeVal_t *name, const void *arg) {
	nativeVal_t v = { 0 }, zero;
	(void)arg;
	Native_SetFloat(&zero, 0.0);
	byte ok = Native_CallMod("SAI", "AddVehicleRace", NULL, "v", name) && Mode_EntitySeam(name, &v) &&
		LvN_SetS(&v, "TgType", "Green") && Mode_VSet(&v, "MoveTime", &zero);
	Native_Free(&v);
	return ok;
}

/* RaceStart(ItemName, VehicleName): the race starts at the first checkpoint: the checkpoint counts from now on, the
 * race of the AI, the rivals become targets, the counter measures are watched (OBJLIB 7: SAI by name) */
static byte Rm_RaceStart(const nativeVal_t *item) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *tn = Mode_V(&t), *e2 = Mode_V(&t), *usr = Mode_V(&t), *un = Mode_V(&t);
	return Mode_Done(&t, Mode_Import("Speech") && Mode_EntitySeam(item, e) && LvN_SetS(e, "OnTake", "RaceMaker.OnCheckPoint") &&
		Mode_Cat(&t, tn, 2, item, Mode_S(&t, "_Trigg")) && Mode_EntitySeam(tn, e2) &&
		LvN_SetS(e2, "OnTake", "RaceMaker.OnNearCheckPoint") && Native_CallMod("SAI", "InitVehicleRace", NULL, "") &&
		Mode_UsrHandle(usr) && Mode_VObj(usr, "Name", un) && Native_CallMod("SAI", "AddVehicleRace", NULL, "v", un) &&
		LvN_PutI(RM, "YourScore", 0) && Rm_FoeCom("RaceStart") && Rm_Each("Players", Rm_RaceShip, NULL) &&
		Mode_Import("Menu") && LvN_ModNone("Menu", "MissionScorerCall") &&
		Mode_Bind(Mode_I(&t, 0), "Racer", "CM", "RaceMaker.OnCounterMeasure") && LvN_PutI(RM, "LastCmTime", -1));
}

/* OnCounterMeasure(id=0): the counter measure key during a race: the rival protests, the second time after 5
 * seconds ends the race */
static byte Rm_OnCounterMeasure(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *w = Mode_V(&t), *foe = Mode_V(&t), *sp = Mode_V(&t), *cft = Mode_V(&t);
	nativeVal_t *last = Mode_V(&t), *now = Mode_V(&t), *key = Mode_V(&t), *five = Mode_V(&t), *lt = Mode_V(&t);
	if (!Mode_Import("Speech") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int none = usr->type == NV_NONE;
	if (!none) {
		none = Mode_VObj(usr, "iWeap", w) ? LvN_EqI(w, 6) : -1;
	}
	if (none) {
		return Mode_Done(&t, none > 0);
	}
	int has = Rm_G("ChallengeFoeType", foe) ? Mode_HasKey(RM, "RaceSpeachers", 1, foe) : -1;
	Native_Free(foe);
	if (has < 0 || !Rm_G("ChallengeFoeType", foe) || (has ? !Rm_G("RaceSpeachers", sp) || !Mode_Sub(sp, foe, cft) :
		(Native_Copy(cft, foe), 0)) || !Rm_G("LastCmTime", last)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(last, -1)) {
		Native_SetFloat(now, Mode_Time());
		return Mode_Done(&t, Mode_Put(RM, "LastCmTime", NULL, now) &&
			Mode_Cat(&t, key, 3, Mode_S(&t, "Generics_"), cft, Mode_S(&t, "Race_1")) &&
			Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, cft));
	}
	Native_Free(last);
	Native_SetFloat(five, 5.0);
	int c = Rm_G("LastCmTime", last) && Mode_Arith('+', last, five, lt) ? (Native_SetFloat(now, Mode_Time()), Mode_Cmp(lt, "<", now)) : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	return Mode_Done(&t, Mode_Cat(&t, key, 3, Mode_S(&t, "Generics_"), cft, Mode_S(&t, "Race_2")) &&
		Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, cft) && Rm_Call("AbortCameraTravelling", NULL, "") &&
		Rm_Call("EndOfRace", NULL, "ss", "CheckPoint", "CounterMeasure"));
}

/* the travelling ends, then EndOfRace("CheckPoint", why) (alarm: and Scrap.SetAlarm(1)) */
static byte Rm_Stop(const char *why, byte alarm) {
	nativeVal_t one;
	Native_SetInt(&one, 1);
	return Rm_Call("AbortCameraTravelling", NULL, "") && Rm_Call("EndOfRace", NULL, "ss", "CheckPoint", why) &&
		(!alarm || Mode_SetAlarm(&one));
}

/* RacerDamaged(racerDamaged, attacker, damageDone): the first hit on a racer counts as its checkpoint, once */
static byte Rm_RacerDamaged(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *wp = Mode_V(&t), *first = Mode_V(&t);
	PyObject *fn = Rm_Call("AbortCameraTravelling", NULL, "") ? Mode_GlobalPy(RM, "OnCheckPoint") : NULL;
	byte ok = fn && Native_GVarGet("CurrentWaypointName", wp) && Mode_ListGet("Race", "First", first) &&
		Native_Call(fn, NULL, "vv", wp, first) && Mode_ListSet("Race", "OnDamage", "");
	Py_XDECREF(fn);
	return Mode_Done(&t, ok);
}

/* AbortCameraTravelling(): the end of the travelling of StartCameraTravelling: the player's ship, the camera and the
 * HUD back (OBJLIB 3, 7: RacerScorer and Scrap.SetCam by name) */
static byte Rm_AbortCameraTravelling(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *vn = Mode_V(&t), *usr = Mode_V(&t), *zero = Mode_V(&t), *in = Mode_V(&t);
	nativeVal_t *l = Mode_V(&t), *cam = Mode_V(&t), *ce = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Rm_G("prevCamera", prev)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(prev)) {
		return Mode_Done(&t, 1);
	}
	Native_SetFloat(zero, 0.0);
	Native_Free(prev);
	Mode_CamName(0, cam);
	byte ok = Mode_GVarSetS("MainMissionEvent", "MissionsFuncs.ShowMissionInfo") && Mode_GVarSetS("EscapeEvent", "Menu.Init") &&
		Rm_G("cameraVehicleName", vn) && Mode_EntitySeam(vn, usr) && Mode_VSet(usr, "MoveTime", zero) &&
		LvN_SetI(usr, "UsrControl", 0) && Rm_G("cameraItemName", in) && Mode_EntitySeam(in, l) && LvN_SetI(l, "rTime", 0) &&
		Mode_GVarSetS("EscapeEvent", "Menu.Init") && Mode_EntitySeam(cam, ce) && Mode_VDel(ce) && Rm_G("prevCamera", prev) &&
		Native_CallMod("Scrap", "SetCam", NULL, "iv", 0L, prev) && (Native_Free(prev), Rm_G("prevCamera", prev)) &&
		(!Native_True(prev) || Native_CallMod("RacerScorer", "Create", NULL, "i", 0L)) &&
		Mode_Put(RM, "cameraItemName", NULL, none) && Mode_Put(RM, "cameraVehicleName", NULL, none) &&
		Mode_Put(RM, "prevCamera", NULL, none);
	return Mode_Done(&t, ok);
}

/* (a / 4.0, b, c) of a camera angle */
static byte Rm_Quarter(const nativeVal_t *ang, nativeVal_t *out) {
	nativeVal_t a = { 0 }, four, q = { 0 };
	Native_SetFloat(&four, 4.0);
	Native_SetEmpty(out, NV_TUPLE);
	byte ok = LvN_Item(ang, 0, &a) && Mode_Arith('/', &a, &four, &q) && (*Native_Append(out) = q, Native_SetNone(&q), 1) &&
		LvN_Item(ang, 1, Native_Append(out)) && LvN_Item(ang, 2, Native_Append(out));
	Native_Free(&a);
	Native_Free(&q);
	return ok;
}

/* entity e.var = v */
static byte Rm_Set(const nativeVal_t *e, const char *var, const nativeVal_t *v) {
	return Mode_VSet(e, var, v);
}

/* the camera path points of StartCameraTravelling: wayp.Pos + Rotate3D((0, 0, back), ang) */
static byte Rm_CamPoint(const nativeVal_t *wayp, const nativeVal_t *ang, const nativeVal_t *back, nativeVal_t *out) {
	nativeVal_t off, r = { 0 }, pos = { 0 };
	Mode_Ints3(&off, 0, 0, 0);
	Native_Free(&off.items[2]);
	Native_Copy(&off.items[2], back);
	byte ok = Mode_Rotate(&off, ang, &r) && Mode_VObj(wayp, "Pos", &pos) && Mode_VecAdd(&r, &pos, out);
	Native_Free(&off);
	Native_Free(&r);
	Native_Free(&pos);
	return ok;
}

/* Scrap.AddScheduledFunc(time, func, args) with func the global name of the module (OBJLIB 4) */
static byte Rm_At(const nativeVal_t *time, const char *name, const nativeVal_t *args, int n) {
	PyObject *fn = Mode_GlobalPy(RM, name);
	byte ok = fn && Mode_Schedule(time, fn, args, n, NULL);
	Py_XDECREF(fn);
	return ok;
}

/* StartCameraTravelling(ItemName, VehicleName, isRace=1, speed="Normal"): the player's ship flies to the item and a
 * camera path shows the checkpoint (isRace) or the item; it ends by itself; the time of the travelling (None when
 * the vehicle is not the player's); OBJLIB 3, 7: Menu and Scrap.SetCam by name */
static byte Rm_StartCameraTravelling(const nativeVal_t *item, const nativeVal_t *veh, const nativeVal_t *race,
	const nativeVal_t *speed, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *ctrl = Mode_V(&t), *l = Mode_V(&t), *zero3 = Mode_V(&t), *v = Mode_V(&t);
	nativeVal_t *one = Mode_V(&t), *cam0 = Mode_V(&t), *wn = Mode_V(&t), *wayp = Mode_V(&t), *wa = Mode_V(&t);
	nativeVal_t *add = Mode_V(&t), *ang = Mode_V(&t), *back = Mode_V(&t), *cd = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *prev = Mode_V(&t), *q = Mode_V(&t), *tm = Mode_V(&t);
	nativeVal_t *t0 = Mode_V(&t), *t1 = Mode_V(&t), *time = Mode_V(&t), *now = Mode_V(&t), *s1 = Mode_V(&t);
	nativeVal_t *s2 = Mode_V(&t), *fifth = Mode_V(&t), *cn = Mode_V(&t), *p = Mode_V(&t);
	/* p and the two values after it: the three points of the path */
	Mode_V(&t);
	Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("math") || !Mode_Put(RM, "cameraItemName", NULL, item) ||
		!Mode_Put(RM, "cameraVehicleName", NULL, veh) || !Mode_EntitySeam(veh, usr) || !Mode_VObj(usr, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_CmpI(ctrl, "!=", 0);
	if (c) {
		return Mode_Done(&t, c > 0);
	}
	Mode_Ints3(zero3, 0, 0, 0);
	Native_SetFloat(one, 1.0);
	Mode_CamName(0, cam0);
	if (!Mode_EntitySeam(item, l) || !LvN_SetI(l, "rTime", 3600) || !LvN_SetS(l, "OnTake", "") ||
		!Native_CallMod("Menu", "MovieScorer", NULL, "i", 0L) || !LvN_SetI(usr, "UsrControl", -1) ||
		!Mode_VSet(usr, "Vel", zero3) || !Mode_VObj(l, "Pos", v) || !Mode_VSet(usr, "MovePos", v) ||
		(Native_Free(v), !Mode_VObj(l, "Ang", v)) || !Mode_VSet(usr, "MoveAng", v) || !Mode_VSet(usr, "MoveTime", one) ||
		!LvN_SetI(usr, "MoveFade", 2) || !LvN_SetI(usr, "AvoidCrash", 1) || !Mode_GVarSetS("EscapeEvent", "") ||
		!Mode_Put(RM, "prevCamera", NULL, cam0)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(race, 1) ? !Native_GVarGet("CurrentWaypointName", wn) || !Mode_EntitySeam(wn, wayp) :
		!Mode_EntitySeam(item, wayp)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(add, 15, 180, 0);
	if (!Mode_VObj(wayp, "Ang", wa) || !Mode_VecAdd(wa, add, ang)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		Native_Free(back);
		if (i < 2) {
			Native_SetInt(back, i == 0 ? -5000 : -4000);
		} else if (!Native_GVarGet("VehicleCamDist2", cd) || !Mode_Neg(cd, back)) {
			return Mode_Done(&t, 0);
		}
		if (!Rm_CamPoint(wayp, ang, back, &p[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!LvN_Item(&p[0], 0, x) || !LvN_Item(&p[0], 1, y) || !LvN_Item(&p[0], 2, z) ||
		!LvN_Create(Mode_S(&t, "RaceCamera"), x, y, z, "CamPath", cam) || !LvN_SetI(cam, "nWayPoint", 3)) {
		return Mode_Done(&t, 0);
	}
	int normal = LvN_EqS(speed, "Normal"), slow = normal ? 0 : LvN_EqS(speed, "Slow");
	for (int i = 0; i < 3; i++) {
		char var[16];
		Str_Printf(var, sizeof(var), "TgtCam0%d", i);
		Native_Free(prev);
		if (!Rm_G("prevCamera", prev) || !Rm_Set(cam, var, prev) || !LvN_SetS(cam, var, "")) {
			return Mode_Done(&t, 0);
		}
		if (i == 2) {
			break;
		}
		Native_Free(q);
		Str_Printf(var, sizeof(var), "Pos0%d", i);
		byte ok = Rm_Set(cam, var, &p[i]) && (i == 0 ? (Native_Copy(q, ang), 1) : Rm_Quarter(ang, q));
		Str_Printf(var, sizeof(var), "Ang0%d", i);
		ok = ok && Rm_Set(cam, var, q);
		Str_Printf(var, sizeof(var), "FadeIn0%d", i);
		ok = ok && LvN_SetI(cam, var, i == 0);
		Str_Printf(var, sizeof(var), "FadeOut0%d", i);
		ok = ok && LvN_SetI(cam, var, i == 1);
		Str_Printf(var, sizeof(var), "Time0%d", i);
		if (ok && (normal || slow)) {
			Native_SetFloat(tm, normal ? (i == 0 ? 1.0 : 3.0) : (i == 0 ? 2.0 : 6.0));
			ok = Rm_Set(cam, var, tm);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(q);
	Native_SetFloat(fifth, 0.2);
	if (!Rm_Quarter(ang, q) || !Rm_Set(cam, "Ang02", q) || !Rm_Set(cam, "Pos02", &p[2]) || !LvN_SetI(cam, "Time", 0) ||
		!Mode_VGet(cam, "Name", cn) || !Native_CallMod("Scrap", "SetCam", NULL, "iv", 0L, cn) ||
		!Mode_VObj(cam, "Time00", t0) || !Mode_VObj(cam, "Time01", t1) || !Mode_Arith('+', t0, t1, time) ||
		!Mode_GVarSetS("MainMissionEvent", "") || !Mode_GVarSetS("EscapeEvent", "") ||
		(Native_SetFloat(now, Mode_Time()), !Mode_Arith('+', now, time, s1)) || !Mode_Arith('+', s1, fifth, s2) ||
		!Rm_At(s2, "AbortCameraTravelling", NULL, 0)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, time);
	return Mode_Done(&t, 1);
}

/* OnRaceStart(ItemName, VehicleName): the player took the start checkpoint: the travelling, then RaceGo; the
 * challenger's start item goes, the count down and the action music */
static byte Rm_OnRaceStart(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *user = Mode_V(&t), *un = Mode_V(&t), *tr = Mode_V(&t), *now = Mode_V(&t), *at = Mode_V(&t);
	nativeVal_t *npc = Mode_V(&t), *args = Mode_V(&t);
	Mode_V(&t);
	if (!Mode_UsrHandle(user)) {
		return Mode_Done(&t, 0);
	}
	int go = Native_True(user);
	if (go) {
		go = Mode_VObj(user, "Name", un) ? Mode_Cmp(un, "==", veh) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	Native_Copy(args, item);
	Native_Copy(args + 1, veh);
	if (!Rm_Call("StartCameraTravelling", tr, "vv", item, veh) || (Native_SetFloat(now, Mode_Time()), !Mode_Arith('+', now, tr, at)) ||
		!Rm_At(at, "RaceGo", args, 2) || !Rm_G("NPC_START_ITEM", npc)) {
		return Mode_Done(&t, 0);
	}
	int some = Mode_Cmp(npc, "!=", Mode_V(&t));
	if (some < 0) {
		return Mode_Done(&t, 0);
	}
	if (some) {
		Native_Free(npc);
		if (!Rm_G("NPC_START_ITEM", npc) || !Mode_VDel(npc) || !LvN_ModNone(RM, "NPC_START_ITEM")) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Rm_Call("CreateRaceCountDown", NULL, "") && Mode_GVarSetI("isActionMusic", 1));
}

/* RaceGo(ItemName, VehicleName): the end of the count down: OnRaceGoCallback, the race starts at the checkpoint */
static byte Rm_RaceGo(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *cb = Mode_V(&t);
	if (!Rm_G("prevCamera", prev)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(prev)) {
		return Mode_Done(&t, 1);
	}
	if (!LvN_GObj(RM, "OnRaceGoCallback", cb) ||
		(Native_True(cb) && (!Mode_CallArgs(cb, NULL, NULL, 0) || !LvN_ModNone(RM, "OnRaceGoCallback")))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Rm_Call("AbortCameraTravelling", NULL, "") && Rm_Call("OnCheckPoint", NULL, "vv", item, veh) &&
		Rm_Call("DestroyRaceCountDown", NULL, ""));
}

/* StartPos[0][1] + add (add 0: the value itself) */
static byte Rm_StartAng(long add, nativeVal_t *out) {
	nativeVal_t sp = { 0 }, s0 = { 0 }, a = { 0 }, n;
	Native_SetInt(&n, add);
	byte ok = Rm_G("StartPos", &sp) && LvN_Item(&sp, 0, &s0) && LvN_Item(&s0, 1, &a) &&
		(add ? Mode_Arith('+', &a, &n, out) : (Native_Copy(out, &a), 1));
	Native_Free(&sp);
	Native_Free(&s0);
	Native_Free(&a);
	return ok;
}

/* (0, StartPos[0][1] + add, 0) */
static byte Rm_StartAng3(long add, nativeVal_t *out) {
	nativeVal_t a = { 0 };
	if (!Rm_StartAng(add, &a)) {
		return 0;
	}
	Mode_Ints3(out, 0, 0, 0);
	Native_Free(&out->items[1]);
	out->items[1] = a;
	return 1;
}

/* SVec.Add(StartPos[0][0], SVec.Rotate3D((x, y, z), (0, StartPos[0][1], 0))): a place beside the start checkpoint */
static byte Rm_BesideStart(long x, long y, long z, nativeVal_t *out) {
	nativeVal_t sp = { 0 }, s0 = { 0 }, pos = { 0 }, ang = { 0 }, off, r = { 0 };
	Mode_Ints3(&off, x, y, z);
	byte ok = Rm_G("StartPos", &sp) && LvN_Item(&sp, 0, &s0) && LvN_Item(&s0, 0, &pos) && Rm_StartAng3(0, &ang) &&
		Mode_Rotate(&off, &ang, &r) && Mode_VecAdd(&pos, &r, out);
	Native_Free(&sp);
	Native_Free(&s0);
	Native_Free(&pos);
	Native_Free(&ang);
	Native_Free(&off);
	Native_Free(&r);
	return ok;
}

/* e.method(s) of a handle value (OBJLIB 7: the Python method) */
static byte Rm_Method(const nativeVal_t *e, const char *method, const char *s) {
	PyObject *fn = Mode_VBound(e, method);
	byte ok = fn && Native_Call(fn, NULL, "s", s);
	Py_XDECREF(fn);
	return ok;
}

/* one traffic light of the count down (side 0 right, 1 left) with its light effect (OBJLIB 7: the call function of
 * Scrap by name) */
static byte Rm_CountDownLight(int side) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *vec = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *l = Mode_V(&t), *ang = Mode_V(&t);
	const char *name = side == 0 ? "RaceCountDownR" : "RaceCountDownL";
	return Mode_Done(&t, Rm_BesideStart(side == 0 ? -3300 : 3300, 1100, -4000, vec) && LvN_Item(vec, 0, x) &&
		LvN_Item(vec, 1, y) && LvN_Item(vec, 2, z) && LvN_Create(Mode_S(&t, name), x, y, z, "Obj", l) &&
		Rm_Method(l, "SetModel", "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d") &&
		Rm_Method(l, "SetFX", "MeetingPoint-Race.m3d") && Rm_StartAng3(side == 0 ? 0 : 180, ang) && Mode_VSet(l, "Ang", ang) &&
		Native_CallMod("Scrap", "SetCallFunc", NULL, "s", "!FXMeetingPointCountDown") &&
		Native_CallMod("Scrap", "AddParams", NULL, "s", name) && Native_CallMod("Scrap", "AddParamf", NULL, "d", 1.9) &&
		Native_CallMod("Scrap", "AddParami", NULL, "i", side == 0 ? 1L : -1L) && Native_CallMod("Scrap", "Execute", NULL, ""));
}

/* CreateRaceCountDown(): the two traffic lights beside the start with their light effect, and the red, yellow and
 * green flashes and beeps (OBJLIB 4: SScorer.Fade and SSound.Play scheduled as the functions of the modules) */
static byte Rm_CreateRaceCountDown(void) {
	static const long fades[3][4] = { { 255, 0, 0, 64 }, { 255, 255, 0, 64 }, { 0, 255, 0, 64 } };
	static const double fadetimes[3] = { 1.5, 1.0, 1.5 };
	if (!Rm_CountDownLight(0) || !Rm_CountDownLight(1)) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t when, a[4], b[2];
		Native_SetFloat(&when, Mode_Time() + 1.9 * i);
		Native_SetInt(&a[0], 0);
		Native_SetEmpty(&a[1], NV_TUPLE);
		for (int k = 0; k < 4; k++) {
			Native_SetInt(Native_Append(&a[1]), fades[i][k]);
		}
		Mode_Ints3(&a[2], 0, 0, 0);
		Native_SetInt(Native_Append(&a[2]), 0);
		Native_SetFloat(&a[3], fadetimes[i]);
		PyObject *fade = Script_Attr("SScorer", "Fade");
		byte ok = fade && Mode_Schedule(&when, fade, a, 4, NULL);
		Py_XDECREF(fade);
		for (int k = 0; k < 4; k++) {
			Native_Free(&a[k]);
		}
		Native_SetFloat(&when, Mode_Time() + 1.9 * i);
		PyObject *play = ok ? Script_Attr("SSound", "Play") : NULL;
		Native_SetNone(&b[0]);
		Native_SetNone(&b[1]);
		ok = play && Rm_G(i < 2 ? "RaceCountDownSound1" : "RaceCountDownSound2", &b[0]) &&
			Rm_G(i < 2 ? "RaceCountDownSound1Vol" : "RaceCountDownSound2Vol", &b[1]) && Mode_Schedule(&when, play, b, 2, NULL);
		Py_XDECREF(play);
		Native_Free(&b[0]);
		Native_Free(&b[1]);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* DestroyRaceCountDown(): the traffic lights move away and go */
static byte Rm_DestroyRaceCountDown(void) {
	for (int side = 0; side < 2; side++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *name = Mode_S(&t, side == 0 ? "RaceCountDownR" : "RaceCountDownL"), *e = Mode_V(&t), *mp = Mode_V(&t);
		nativeVal_t *ma = Mode_V(&t), *half = Mode_V(&t), *when = Mode_V(&t);
		Native_SetFloat(half, 0.5);
		byte ok = Mode_EntitySeam(name, e) && (!Native_True(e) || (Rm_BesideStart(side == 0 ? -10000 : 10000, 1000, -4500, mp) &&
			Mode_VSet(e, "MovePos", mp) && Rm_StartAng3(90, ma) && Mode_VSet(e, "MoveAng", ma) && Mode_VSet(e, "MoveTime", half) &&
			(Native_SetFloat(when, Mode_Time() + 0.5), Rm_At(when, "DestroyRaceCountDownEnd", name, 1))));
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return 1;
}

/* DestroyRaceCountDownEnd(EntityName) */
static byte Rm_DestroyRaceCountDownEnd(const nativeVal_t *name) {
	nativeVal_t l = { 0 };
	byte ok = Mode_EntitySeam(name, &l) && (!Native_True(&l) || Mode_VDel(&l));
	Native_Free(&l);
	return ok;
}

/* Counter(num, r=255, g=255, b=255): the text and colour of the scorer item Counter during the travelling (the
 * script no longer creates it) */
static byte Rm_Counter(const nativeVal_t *num, nativeVal_t *const *rgb) {
	static const char *const vars[] = { "Red", "Green", "Blue" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *item = Mode_S(&t, "Counter");
	if (!Rm_G("prevCamera", prev)) {
		return Mode_Done(&t, 0);
	}
	int go = Native_True(prev);
	if (go) {
		go = Mode_Cmp(num, "!=", Mode_S(&t, ""));
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	for (int i = 0; i < 3; i++) {
		if (!Mode_ScorerSet(Mode_I(&t, 0), item, vars[i], rgb[i] ? rgb[i] : Mode_I(&t, 255))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_ScorerSet(Mode_I(&t, 0), item, "Text", num));
}

/* a new AI profile of the save variable Race.Profile (OBJLIB 3: VehiclesNPC.cShipAI, SetAIProfile by name) */
static byte Rm_Profile(nativeVal_t *pd) {
	nativeVal_t prof;
	if (!Native_CallMod("VehiclesNPC", "cShipAI", pd, "")) {
		return 0;
	}
	PyObject *fn = Script_Attr("VehiclesNPC", "SetAIProfile");
	Mode_SaveVar("Race.Profile", &prof);
	byte ok = fn && Native_Call(fn, NULL, "vv", pd, &prof);
	Py_XDECREF(fn);
	Native_Free(&prof);
	return ok;
}

/* the arguments (6, name, pos[0], pos[1], pos[2], l.Radius, pd.ControlBoostRace[, 1]) of SAI.SetStateVehicle for a
 * rival heading for the checkpoint l at pos (next: the last argument 1) into out (a tuple) */
static byte Rm_HeadArgs(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *l, const nativeVal_t *pd,
	byte next, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), 6);
	Native_Copy(Native_Append(out), name);
	byte ok = LvN_Item(pos, 0, Native_Append(out)) && LvN_Item(pos, 1, Native_Append(out)) &&
		LvN_Item(pos, 2, Native_Append(out)) && Mode_VObj(l, "Radius", Native_Append(out)) &&
		Mode_VObj(pd, "ControlBoostRace", Native_Append(out));
	if (ok && next) {
		Native_SetInt(Native_Append(out), 1);
	}
	return ok;
}

/* SAI.SetStateVehicle scheduled at timy with the tuple of arguments (OBJLIB 4: the function of SAI); timy + 0.05 */
static byte Rm_HeadLater(nativeVal_t *timy, const nativeVal_t *a) {
	nativeVal_t step, next = { 0 };
	Native_SetFloat(&step, 0.05);
	PyObject *fn = Script_Attr("SAI", "SetStateVehicle");
	byte ok = fn && Mode_Schedule(timy, fn, a->items, a->n, NULL) && Mode_Arith('+', timy, &step, &next);
	Py_XDECREF(fn);
	if (ok) {
		Native_Free(timy);
		*timy = next;
	}
	return ok;
}

/* the checkpoint l moves on to StartPos[iStartPos + 1] (its trigger with it) */
static byte Rm_NextCheckPoint(const nativeVal_t *item, const nativeVal_t *l) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *i = Mode_V(&t), *next = Mode_V(&t), *sp = Mode_V(&t), *n = Mode_V(&t), *r = Mode_V(&t), *at = Mode_V(&t);
	nativeVal_t *p = Mode_V(&t), *a = Mode_V(&t), *ang = Mode_V(&t), *tn = Mode_V(&t), *l2 = Mode_V(&t), *lp = Mode_V(&t);
	int len = Rm_G("iStartPos", i) && Mode_Arith('+', i, Mode_I(&t, 1), next) && Rm_G("StartPos", sp) ? Mode_Len(sp) : -1;
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(n, len);
	Native_Free(sp);
	Native_Free(i);
	if (!Mode_Arith('%', next, n, r) || !Mode_Put(RM, "iStartPos", NULL, r) || !Rm_G("StartPos", sp) || !Rm_G("iStartPos", i) ||
		!Mode_Sub(sp, i, at) || !LvN_Item(at, 0, p) || !Mode_VSet(l, "Pos", p)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(sp);
	Native_Free(i);
	Native_Free(at);
	if (!Rm_G("StartPos", sp) || !Rm_G("iStartPos", i) || !Mode_Sub(sp, i, at) || !LvN_Item(at, 1, a)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(ang, 0, 0, 0);
	Native_Free(&ang->items[1]);
	Native_Copy(&ang->items[1], a);
	return Mode_Done(&t, Mode_VSet(l, "Ang", ang) && Mode_Cat(&t, tn, 2, item, Mode_S(&t, "_Trigg")) &&
		Mode_EntitySeam(tn, l2) && Mode_VObj(l, "Pos", lp) && Mode_VSet(l2, "Pos", lp));
}

/* the checkpoint point of a racer: 1 when the race goes on (0 for the end: the race ended or the vehicle is no
 * rival), -1 with the error */
static int Rm_Score(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = Mode_V(&t), *ctrl = Mode_V(&t), *snd = Mode_V(&t), *vol = Mode_V(&t), *s = Mode_V(&t);
	nativeVal_t *lim = Mode_V(&t);
	if (!Mode_EntitySeam(veh, ply) || !Mode_VObj(ply, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0) - 1;
	}
	int r;
	if (LvN_EqI(ctrl, 0)) {
		if (!LvN_ModGet("Sound", "WaySound", snd) || !LvN_ModGet("Sound", "WayVolume", vol) || !Mode_Play(snd, vol) ||
			!LvN_GAdd(RM, "YourScore", 1, NULL) || !Rm_G("YourScore", s)) {
			return Mode_Done(&t, 0) - 1;
		}
		if (LvN_EqI(s, 0)) {
			r = Rm_Call("RaceStart", NULL, "vv", item, veh) ? 1 : -1;
		} else {
			Native_Free(s);
			int c = Rm_G("YourScore", s) && Rm_G("WayPointlimit", lim) ? Mode_Cmp(s, ">=", lim) : -1;
			r = c < 0 ? -1 : !c ? 1 : Rm_Call("EndOfRace", NULL, "vv", item, veh) ? 0 : -1;
		}
		return Mode_Done(&t, 1), r;
	}
	if (!Rm_G("YourScore", s)) {
		return Mode_Done(&t, 0) - 1;
	}
	if (LvN_EqI(s, -1)) {
		r = Rm_Call("RaceStart", NULL, "vv", item, veh) ? 1 : -1;
		return Mode_Done(&t, 1), r;
	}
	int rival = Mode_HasKey(RM, "BayScorer", 1, veh);
	if (rival <= 0) {
		return Mode_Done(&t, 1), rival;
	}
	Native_Free(s);
	int c = LvN_GAdd(RM, "FoesScorer", 1, NULL) && LvN_ModGet("Sound", "EneWaySound", snd) &&
		LvN_ModGet("Sound", "EneWayVolume", vol) && Mode_Play(snd, vol) && Rm_G("FoesScorer", s) && Rm_G("WayPointlimit", lim) ?
		Mode_Cmp(s, ">=", lim) : -1;
	r = c < 0 ? -1 : !c ? 1 : Rm_Call("EndOfRace", NULL, "vv", item, veh) ? 0 : -1;
	return Mode_Done(&t, 1), r;
}

/* OnCheckPoint(ItemName, VehicleName): the OnTake of the checkpoint: a point for the player or the rivals (the
 * start, or the end at WayPointlimit); the checkpoint moves on and the rivals head for it (OBJLIB 3, 7: Menu, Sound,
 * VehiclesNPC and SAI.SetStateVehicle by name) */
static byte Rm_OnCheckPoint(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *timy = Mode_V(&t), *players = Mode_V(&t), *i = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Sound") || !Mode_Import("VehiclesNPC")) {
		return Mode_Done(&t, 0);
	}
	int go = Rm_Score(item, veh);
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	if (!Mode_EntitySeam(item, l) || !Rm_NextCheckPoint(item, l)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(timy, Mode_Time());
	int n = Rm_G("Players", players) ? Mode_Loop(players) : -1;
	for (int k = 0; k < n; k++) {
		nativeVal_t name = { 0 }, pd = { 0 }, e = { 0 }, pos = { 0 }, a = { 0 };
		int same = Native_SeqItem(players, k, &name) && Rm_Profile(&pd) ? Mode_Cmp(&name, "==", veh) : -1;
		byte ok = same >= 0;
		if (same > 0) {
			ok = Mode_EntitySeam(&name, &e) && (!Native_True(&e) || (Mode_VObj(l, "Pos", &pos) &&
				Rm_HeadArgs(&name, &pos, l, &pd, 0, &a) && Native_CallMod("SAI", "SetStateVehicle", NULL, "vvvvvvv", &a.items[0],
				&a.items[1], &a.items[2], &a.items[3], &a.items[4], &a.items[5], &a.items[6])));
		} else if (same == 0) {
			ok = Mode_VObj(l, "Pos", &pos) && Rm_HeadArgs(&name, &pos, l, &pd, 0, &a) && Rm_HeadLater(timy, &a);
		}
		Native_Free(&name);
		Native_Free(&pd);
		Native_Free(&e);
		Native_Free(&pos);
		Native_Free(&a);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0 && Rm_G("iStartPos", i) && Mode_Put(RM, "nextStartPos", NULL, i) &&
		Rm_Call("SetScorerValues", NULL, "i", 0L) && Rm_Hook("OnCheckPointCallback"));
}

/* OnNearCheckPoint(ItemName, VehicleName): a racer near the checkpoint (its big trigger): the rivals that may head
 * for the next checkpoint go there, one stays on this one when the player is first */
static byte Rm_OnNearCheckPoint(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = Mode_V(&t), *ctrl = Mode_V(&t), *l = Mode_V(&t), *half = Mode_V(&t), *i = Mode_V(&t);
	nativeVal_t *i1 = Mode_V(&t), *sp = Mode_V(&t), *n = Mode_V(&t), *next2 = Mode_V(&t), *ns = Mode_V(&t);
	nativeVal_t *at = Mode_V(&t), *pos = Mode_V(&t), *opon = Mode_V(&t), *num = Mode_V(&t), *timy = Mode_V(&t);
	nativeVal_t *players = Mode_V(&t);
	if (!Mode_Import("VehiclesNPC") || !Mode_EntitySeam(veh, ply) || !Mode_VObj(ply, "UsrControl", ctrl)) {
		return Mode_Done(&t, 0);
	}
	int out = LvN_EqI(ctrl, -1);
	if (out) {
		int in = Mode_HasKey(RM, "BayScorer", 1, veh);
		out = in < 0 ? -1 : !in;
	}
	if (out) {
		return Mode_Done(&t, out > 0);
	}
	Native_SetFloat(half, 0.5);
	int len = Mode_EntitySeam(item, l) && Mode_VSet(l, "rTime", half) && Rm_G("iStartPos", i) &&
		Mode_Arith('+', i, Mode_I(&t, 1), i1) && Rm_G("StartPos", sp) ? Mode_Len(sp) : -1;
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(n, len);
	int same = Mode_Arith('%', i1, n, next2) && Rm_G("nextStartPos", ns) ? Mode_Cmp(ns, "==", next2) : -1;
	if (same) {
		return Mode_Done(&t, same > 0);
	}
	Native_Free(sp);
	Native_Free(ns);
	if (!Mode_Put(RM, "nextStartPos", NULL, next2) || !Rm_G("StartPos", sp) || !Rm_G("nextStartPos", ns) ||
		!Mode_Sub(sp, ns, at) || !LvN_Item(at, 0, pos)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(ctrl);
	int first = Mode_VObj(ply, "UsrControl", ctrl) ? LvN_EqI(ctrl, 0) : -1;
	if (first > 0) {
		first = Rm_G("NumOponNotGoNextItem", num) ? LvN_EqI(num, 0) : -1;
	}
	if (first < 0 || (first ? !Rm_Call("NameOponGoItem", opon, "v", item) : (Native_SetStr(opon, ""), 0))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(timy, Mode_Time());
	int cnt = Rm_G("Players", players) ? Mode_Loop(players) : -1;
	for (int k = 0; k < cnt; k++) {
		nativeVal_t name = { 0 }, e = { 0 }, pd = { 0 }, gn = { 0 }, a = { 0 };
		int other = Native_SeqItem(players, k, &name) ? Mode_Cmp(&name, "!=", veh) : -1;
		byte ok = other >= 0;
		if (other > 0) {
			int head = Mode_EntitySeam(&name, &e) && Rm_Profile(&pd) && Mode_VObj(&pd, "GoNextItemRace", &gn) ? LvN_EqI(&gn, 1) : -1;
			if (head > 0) {
				head = Mode_Cmp(&name, "!=", opon);
			}
			ok = head >= 0 && (!head || (Rm_HeadArgs(&name, pos, l, &pd, 1, &a) && Rm_HeadLater(timy, &a)));
		}
		Native_Free(&name);
		Native_Free(&e);
		Native_Free(&pd);
		Native_Free(&gn);
		Native_Free(&a);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, cnt >= 0);
}

/* NameOponGoItem(ItemName): the rival farthest from the checkpoint (NameError nameEntSel when none is farther than 0,
 * as the script) */
static byte Rm_NameOponGoItem(const nativeVal_t *item, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *it = Mode_V(&t), *best = Mode_V(&t), *sel = Mode_V(&t), *players = Mode_V(&t);
	byte found = 0;
	Native_SetInt(best, 0);
	int n = Mode_EntitySeam(item, it) && Rm_G("Players", players) ? Mode_Loop(players) : -1;
	for (int k = 0; k < n; k++) {
		nativeVal_t name = { 0 }, ent = { 0 }, ep = { 0 }, ip = { 0 }, sub = { 0 }, d = { 0 };
		int c = Native_SeqItem(players, k, &name) && Mode_EntitySeam(&name, &ent) && Mode_VObj(&ent, "Pos", &ep) &&
			Mode_VObj(it, "Pos", &ip) && Mode_VecSub(&ep, &ip, &sub) && Mode_VecModSqr(&sub, &d) ? Mode_Cmp(&d, ">", best) : -1;
		if (c > 0) {
			Native_Free(best);
			Native_Free(sel);
			Native_Copy(best, &d);
			Native_Copy(sel, &name);
			found = 1;
		}
		Native_Free(&name);
		Native_Free(&ent);
		Native_Free(&ep);
		Native_Free(&ip);
		Native_Free(&sub);
		Native_Free(&d);
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
	}
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (!found) {
		return Mode_Done(&t, Native_Raise(NE_NAME, "nameEntSel"));
	}
	Native_Copy(out, sel);
	return Mode_Done(&t, 1);
}

/* GetDealType() == "Race": 1, 0, -1 */
static int Rm_IsRace(void) {
	nativeVal_t r = { 0 };
	int is = Rm_Call("GetDealType", &r, "") ? LvN_EqS(&r, "Race") : -1;
	Native_Free(&r);
	return is;
}

/* StopTimer(): the player got into a ship (Parking.SwitchVehicleMissionCallback): no timer, the race scores */
static byte Rm_StopTimer(void) {
	int race = Mode_Import("Menu") ? Rm_IsRace() : -1;
	return race >= 0 && (race ? LvN_ModSetGlobal("Menu", "MissionScorerCall", RM, "MissionScorer2") :
		LvN_ModNone("Menu", "MissionScorerCall"));
}

/* StartTimer(): the player left the ship (Parking.SwitchCharMissionCallback): 21 seconds to come back */
static byte Rm_StartTimer(void) {
	return Mode_Import("Menu") && Mode_Import("Speech") && Mode_GVarSetI("TimeToCancelCurrentMission", 21) &&
		LvN_ModSetGlobal("Menu", "MissionScorerCall", RM, "MissionScorer") && Rm_FoeCom("RaceComeBack") &&
		Rm_Call("MissionScorer", NULL, "i", 0L);
}

/* the warning text s the first time (the save variable var "1"); OBJLIB 3: Menu by name */
static byte Rm_FirstTime(const nativeVal_t *s, const char *var) {
	nativeVal_t sv, zero;
	Mode_SaveVar(var, &sv);
	byte first = LvN_EqS(&sv, "1");
	Native_Free(&sv);
	Native_SetStr(&zero, "0");
	return !first || (Mode_Import("Menu") && Native_CallMod("Menu", "ShowWarning", NULL, "iv", 0L, s) && Mode_SetSaveVar(var, &zero));
}

/* PointObjectTo(object, point): the object turns towards the point (around the vertical axis); OBJLIB 7: SVec.GetAngle
 * and CProd by name */
static byte Rm_PointObjectTo(const nativeVal_t *obj, const nativeVal_t *point) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *start = Mode_V(&t), *p = Mode_V(&t), *op = Mode_V(&t), *q = Mode_V(&t), *op2 = Mode_V(&t), *pq = Mode_V(&t);
	nativeVal_t *ang = Mode_V(&t), *d = Mode_V(&t), *d1 = Mode_V(&t), *neg = Mode_V(&t), *a = Mode_V(&t);
	Mode_Ints3(start, 0, 0, -1);
	Native_SetEmpty(p, NV_TUPLE);
	Native_SetEmpty(q, NV_TUPLE);
	if (!LvN_Item(point, 0, Native_Append(p)) || (Native_SetInt(Native_Append(p), 0), !LvN_Item(point, 2, Native_Append(p))) ||
		!Mode_VObj(obj, "Pos", op) || !LvN_Item(op, 0, Native_Append(q)) || (Native_SetInt(Native_Append(q), 0), 0) ||
		!Mode_VObj(obj, "Pos", op2) || !LvN_Item(op2, 2, Native_Append(q)) || !Mode_VecSub(p, q, pq) ||
		!Native_CallMod("SVec", "GetAngle", ang, "vv", start, pq) || !Native_CallMod("SVec", "CProd", d, "vv", start, pq) ||
		!LvN_Item(d, 1, d1)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_CmpI(d1, "<", 0);
	if (c < 0 || (c && !Mode_Neg(ang, neg))) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(a, 0, 0, 0);
	Native_Free(&a->items[1]);
	Native_Copy(&a->items[1], c ? neg : ang);
	return Mode_Done(&t, Mode_VSet(obj, "Ang", a));
}

/* PreloadCnDnResources(): the model and the beeps of the count down */
static byte Rm_PreloadCnDnResources(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s1 = Mode_V(&t), *s2 = Mode_V(&t), *f1 = Mode_V(&t), *f2 = Mode_V(&t);
	return Mode_Done(&t, Mode_Preload(Mode_S(&t, "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d")) &&
		Rm_G("RaceCountDownSound1", s1) && Mode_Cat(&t, f1, 3, Mode_S(&t, "Sounds/Vehicles/Misc/"), s1, Mode_S(&t, ".wav")) &&
		Mode_LoadSoundSeam(f1) && Rm_G("RaceCountDownSound2", s2) &&
		Mode_Cat(&t, f2, 3, Mode_S(&t, "Sounds/Vehicles/Misc/"), s2, Mode_S(&t, ".wav")) && Mode_LoadSoundSeam(f2));
}

/* the DM_Challenger_* dummy nearest to the player (None when there is none) */
static byte Rm_NearestDummy(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *upos = Mode_V(&t), *min = Mode_V(&t);
	Native_SetNone(out);
	Native_SetFloat(min, 10000000.0);
	if (!Mode_DummyStartSeam("DM_Challenger_*", 1) || !Mode_UsrHandle(usr) || !Mode_VObj(usr, "Pos", upos)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		nativeVal_t aux = { 0 }, dp = { 0 }, sub = { 0 }, len = { 0 }, none;
		Native_SetNone(&none);
		int more = Mode_DummyNextSeam(&aux) ? Mode_Cmp(&aux, "==", &none) : -1;
		int c = more == 0 && LvN_Item(&aux, 1, &dp) && Mode_VecSub(upos, &dp, &sub) && Mode_VecMod(&sub, &len) ?
			Mode_Cmp(&len, "<", min) : -1;
		if (c > 0) {
			Native_Free(min);
			Native_Free(out);
			Native_Copy(min, &len);
			Native_Copy(out, &aux);
		}
		Native_Free(&aux);
		Native_Free(&dp);
		Native_Free(&sub);
		Native_Free(&len);
		if (more) {
			return Mode_Done(&t, more > 0);
		}
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
	}
}

/* the AI of a challenger ship v of the kind foe: its profiles, a random one of them, static (OBJLIB 3, 7: VehiclesNPC
 * and SAI.SetStateVehicle by name) */
static byte Rm_ShipAI(const nativeVal_t *v, const nativeVal_t *foe, const char *profile) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p1 = Mode_V(&t), *p2 = Mode_V(&t), *ship = Mode_V(&t), *name = Mode_V(&t);
	Mode_SaveVar(profile, p1);
	if (!Rm_Call("InitShipIAProfile", NULL, "vv", foe, p1)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar(profile, p2);
	return Mode_Done(&t, Rm_Call("GetRandomShipIAProfile", ship, "vv", foe, p2) &&
		Native_CallMod("VehiclesNPC", "SetIAProf", NULL, "vv", v, ship) && Mode_VObj(v, "Name", name) &&
		Native_CallMod("SAI", "SetStateVehicle", NULL, "iv", 0L, name));
}

/* challengePartners[ChallengeFoeType] */
static byte Rm_Partners(nativeVal_t *out) {
	nativeVal_t cp = { 0 }, foe = { 0 };
	byte ok = Rm_G("challengePartners", &cp) && Rm_G("ChallengeFoeType", &foe) && Mode_Sub(&cp, &foe, out);
	Native_Free(&cp);
	Native_Free(&foe);
	return ok;
}

/* "BadBoy" + `i` created at pos[0], pos[1], pos[2] (a Car through the seam of Scrap.CreateEntity) */
static byte Rm_BadBoy(const nativeVal_t *i, const nativeVal_t *pos, nativeVal_t *v) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *name = Mode_V(&t);
	Mode_Repr(i, rep);
	return Mode_Done(&t, Native_Arith('+', Mode_S(&t, "BadBoy"), rep, name) && LvN_CreateAt(name, pos, "Car", v));
}

/* the racer i of InitRace (crazy: a crazy deal; partners the count of the partners) */
static byte Rm_Racer(long i, const nativeVal_t *numply, int iscrazy, int partners) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *io = Mode_V(&t), *sp = Mode_V(&t), *start = Mode_V(&t), *a360 = Mode_V(&t), *m = Mode_V(&t);
	nativeVal_t *dv = Mode_V(&t), *deg = Mode_V(&t), *off = Mode_V(&t), *rang = Mode_V(&t), *rot = Mode_V(&t);
	nativeVal_t *s0 = Mode_V(&t), *spos = Mode_V(&t), *v = Mode_V(&t), *npc = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *dummy = Mode_V(&t), *dpos = Mode_V(&t), *vn = Mode_V(&t), *money = Mode_V(&t), *dbl = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t), *foe = Mode_V(&t), *usr = Mode_V(&t), *up = Mode_V(&t), *list = Mode_V(&t);
	nativeVal_t *partner = Mode_V(&t), *vp = Mode_V(&t), *va = Mode_V(&t), *tm = Mode_V(&t), *pd = Mode_V(&t);
	nativeVal_t *gn = Mode_V(&t), *zero = Mode_V(&t);
	Native_SetInt(io, i);
	Native_SetInt(a360, 360);
	if (!Rm_G("StartPos", sp) || !LvN_Item(sp, 0, start) || !Mode_Arith('*', a360, io, m) || !Mode_Arith('/', m, numply, dv) ||
		!Mode_Arith('+', Mode_I(&t, 90), dv, deg)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(off, 0, 2200, 0);
	Native_Free(sp);
	Native_SetEmpty(rang, NV_TUPLE);
	Native_SetInt(Native_Append(rang), 0);
	if (!Rm_G("StartPos", sp) || !LvN_Item(sp, 0, s0) || !LvN_Item(s0, 1, Native_Append(rang))) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(Native_Append(rang), deg);
	Native_Free(s0);
	if (!Mode_Rotate(off, rang, rot) || !LvN_Item(start, 0, s0) || !Mode_VecAdd(s0, rot, spos)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (i == 0) {
		ok = LvN_CreateAt(Mode_S(&t, "NPC_START_ITEM"), spos, "item", npc) && Mode_Put(RM, "NPC_START_ITEM", NULL, npc) &&
			Rm_StartAng3(0, ang) && (Native_Free(npc), Rm_G("NPC_START_ITEM", npc)) && Mode_VSet(npc, "Ang", ang) &&
			(Native_Free(npc), Rm_G("NPC_START_ITEM", npc)) && LvN_SetS(npc, "OnTake", "RaceMaker.PlaceNPCAtRacePos") &&
			(Native_Free(npc), Rm_G("NPC_START_ITEM", npc)) && LvN_SetI(npc, "Radius", 1800);
		if (ok && !iscrazy) {
			ok = Rm_NearestDummy(dummy) && LvN_Item(dummy, 1, dpos) && Rm_BadBoy(io, dpos, v);
		} else if (ok) {
			ok = Rm_BadBoy(io, spos, v);
		}
		ok = ok && Mode_VGet(v, "Name", vn) && Mode_Append(RM, "challenged_NPCs_Names", vn) && (Native_Free(vn), 1) &&
			Mode_VGet(v, "Name", vn) && Mode_Put(RM, "ChallengerName", NULL, vn);
		if (ok && !iscrazy) {
			ok = Mode_VObj(v, "Money", money) && Rm_Double(dbl) && Mode_Arith('+', money, dbl, sum) && Mode_VSet(v, "Money", sum);
		}
		ok = ok && Rm_G("ChallengeFoeType", foe) && Rm_ShipAI(v, foe, "Race.Profile") && LvN_SetS(v, "TgType", "None") &&
			LvN_SetS(v, "EntityList", "Race");
		if (ok && !iscrazy) {
			ok = Mode_UsrHandle(usr) && Mode_VObj(usr, "Pos", up) && Rm_Call("PointObjectTo", NULL, "vv", v, up);
		}
	} else {
		Native_SetFloat(tm, 0.001);
		ok = Rm_BadBoy(io, spos, v) && Rm_Partners(list) && LvN_Item(list, partners == 1 ? 0 : i - 1, partner) &&
			Rm_ShipAI(v, partner, "Race.Profile") && LvN_SetS(v, "TgType", "None") && LvN_SetS(v, "EntityList", "Race") &&
			Rm_StartAng3(0, ang) && Mode_VSet(v, "Ang", ang) && Mode_VObj(v, "Pos", vp) && Mode_VSet(v, "MovePos", vp) &&
			Mode_VObj(v, "Ang", va) && Mode_VSet(v, "MoveAng", va) && Mode_VSet(v, "MoveTime", tm);
	}
	Native_SetInt(zero, 0);
	Native_Free(vn);
	ok = ok && Mode_VGet(v, "Name", vn) && Mode_DictPut(RM, "BayScorer", NULL, vn, zero) && (Native_Free(vn), 1) &&
		Mode_VGet(v, "Name", vn) && Mode_Append(RM, "Players", vn) && LvN_PutI(RM, "levelPathFind", 0) && Rm_Profile(pd) &&
		Mode_VObj(pd, "GoNextItemRace", gn) && (!LvN_EqI(gn, 0) || LvN_GAdd(RM, "NumOponNotGoNextItem", 1, NULL));
	return Mode_Done(&t, ok);
}

/* the checkpoint of InitRace and its trigger at the start */
static byte Rm_CheckPoints(const nativeVal_t *shower) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *s0 = Mode_V(&t), *spos = Mode_V(&t), *l = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *ln = Mode_V(&t), *tn = Mode_V(&t), *l2 = Mode_V(&t), *lp = Mode_V(&t), *wn = Mode_V(&t);
	if (!Rm_G("StartPos", sp) || !LvN_Item(sp, 0, s0) || !LvN_Item(s0, 0, spos) ||
		!LvN_CreateAt(Mode_S(&t, "CheckPoint"), spos, "Item", l) || !Mode_VModel(l, Mode_S(&t, "models/Misc/Wayp/Wayp.m3d")) ||
		!LvN_SetS(l, "OnTake", "RaceMaker.OnRaceStart") || !Mode_VSet(l, "RadarShow", shower) || !LvN_SetI(l, "Radius", 2400) ||
		!Rm_StartAng3(0, ang) || !Mode_VSet(l, "Ang", ang) || !LvN_SetI(l, "FadeInEffect", 0) ||
		!LvN_SetI(l, "FadeOutEffect", 0) || !Mode_VGet(l, "Name", ln) || !Mode_Cat(&t, tn, 2, ln, Mode_S(&t, "_Trigg"))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(sp);
	Native_Free(s0);
	Native_Free(spos);
	return Mode_Done(&t, Rm_G("StartPos", sp) && LvN_Item(sp, 0, s0) && LvN_Item(s0, 0, spos) && LvN_CreateAt(tn, spos, "Item", l2) &&
		Mode_VObj(l, "Pos", lp) && Mode_VSet(l2, "Pos", lp) && LvN_SetI(l2, "Radius", 20000) &&
		LvN_SetI(l2, "FadeInEffect", 0) && LvN_SetI(l2, "FadeOutEffect", 0) && Mode_VGet(l, "Name", wn) &&
		Mode_GVarSet("CurrentWaypointName", wn));
}

/* the taunt, the challenger to its start item and the message to get a ship at the end of InitRace */
static byte Rm_RaceTaunt(const nativeVal_t *foe, int taunt, int msg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *key = Mode_V(&t), *hook = Mode_V(&t), *who = Mode_V(&t), *now = Mode_V(&t), *lang = Mode_V(&t);
	nativeVal_t *st = Mode_V(&t), *when = Mode_V(&t), *players = Mode_V(&t), *a = Mode_V(&t), *npc = Mode_V(&t);
	nativeVal_t *np = Mode_V(&t), *r = Mode_V(&t), *four = Mode_V(&t), *rad = Mode_V(&t), *cad = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t);
	if (taunt && (!Mode_Cat(&t, key, 2, Mode_S(&t, "RaceHurryUp"), foe) || !LvN_GObj(RM, "OnInitialTaunt", hook) ||
		!Rm_G("ChallengeFoeType", who) || (Native_True(hook) ? !Native_CallMod("Speech", "AddRemoteCom", NULL, "vvv", key, who, hook) ||
		!LvN_ModNone(RM, "OnInitialTaunt") : !Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, who)))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	Native_Free(key);
	if (taunt) {
		if (!Mode_Cat(&t, key, 2, Mode_S(&t, "RaceHurryUp"), foe) || !Mode_LangV(key, lang) ||
			!Native_CallMod("Speech", "CalculateSpeechTime", st, "v", lang) || !Mode_Arith('+', now, st, when)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_Copy(when, now);
	}
	PyObject *fn = Script_Attr("SAI", "SetStateVehicle");
	Native_SetFloat(four, 4.0);
	Native_SetEmpty(a, NV_TUPLE);
	Native_SetInt(Native_Append(a), 1);
	byte ok = fn && Rm_G("Players", players) && LvN_Item(players, 0, Native_Append(a)) && Rm_G("NPC_START_ITEM", npc) &&
		Mode_VObj(npc, "Pos", np) && LvN_Item(np, 0, Native_Append(a)) && LvN_Item(np, 1, Native_Append(a)) &&
		LvN_Item(np, 2, Native_Append(a)) && (Native_Free(npc), Rm_G("NPC_START_ITEM", npc)) && Mode_VObj(npc, "Radius", r) &&
		Mode_Arith('/', r, four, rad);
	if (ok) {
		Native_Copy(Native_Append(a), rad);
		Native_SetInt(Native_Append(a), 1);
		Native_SetFloat(Native_Append(a), 1.0);
		ok = Mode_Schedule(when, fn, a->items, a->n, NULL);
	}
	Py_XDECREF(fn);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	if (msg && Rm_NoTutorial()) {
		Mode_Lang("RaceSystemGetShip", cad);
		Native_SetStr(name, "SF_Secondary");
		fn = Mode_GlobalPy(RM, "ShowSystemMessage");
		ok = fn && Mode_Schedule(when, fn, cad, 1, name);
		Py_XDECREF(fn);
	}
	return Mode_Done(&t, ok);
}

/* InitRace(foeType, numply=2, waypointlimit=20, shower=2, isRevenge=0, isCrazyDeal=0, userOnShip=0): a race against
 * the challenger foeType and its partners (3 racers against the mercenaries and the press) on a race of the map's
 * MapRace.Races (by the save variable Race.Num); OBJLIB 3: the other ports by name, the races of MapRace called as
 * the values they are */
static byte Rm_InitRace(const nativeVal_t *foe, nativeVal_t *const *opt) {
	static const char *const press[] = { "Betty", "Berto", "Humphrey", NULL };
	static const char *const mods[] = { "Alarm", "Menu", "Speech", "Parking", "RacerScorer", "MapRace", "math", "VehiclesNPC" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *numply = Mode_V(&t), *pre = Mode_V(&t), *players = Mode_V(&t), *lang = Mode_V(&t), *list = Mode_V(&t);
	nativeVal_t *empty = Mode_V(&t), *dict = Mode_V(&t), *num = Mode_V(&t), *races = Mode_V(&t), *races2 = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *pick = Mode_V(&t), *race = Mode_V(&t), *next = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *skip = Mode_V(&t), *init = Mode_V(&t), *cn = Mode_V(&t), *now = Mode_V(&t), *i0 = Mode_V(&t);
	const nativeVal_t *limit = opt[1] ? opt[1] : Mode_I(&t, 20), *shower = opt[2] ? opt[2] : Mode_I(&t, 2);
	const nativeVal_t *revenge = opt[3] ? opt[3] : Mode_I(&t, 0), *crazy = opt[4] ? opt[4] : Mode_I(&t, 0);
	const nativeVal_t *onship = opt[5] ? opt[5] : Mode_I(&t, 0);
	for (int i = 0; i < 8; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (opt[0]) {
		Native_Copy(numply, opt[0]);
	} else {
		Native_SetInt(numply, 2);
	}
	if (!LvN_GObj(RM, "OnPreInitRace", pre) ||
		(Native_True(pre) && (!Rm_G("Players", players) || !Mode_CallArgs(pre, NULL, players, 1))) ||
		!Mode_GVarSetN("TimeToCancelCurrentMission", 0.0)) {
		return Mode_Done(&t, 0);
	}
	if (Rm_InS(foe, rm_mercs) || Rm_InS(foe, press)) {
		Native_Free(numply);
		Native_SetInt(numply, 3);
	}
	int iscrazy = Native_True(crazy);
	if (!iscrazy) {
		Mode_Lang("RaceHowTo", lang);
		Native_SetFloat(now, Mode_Time());
		PyObject *fn = Mode_GlobalPy(RM, "FirstTimeRace");
		byte ok = fn && Mode_Schedule(now, fn, lang, 1, NULL);
		Py_XDECREF(fn);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	int partners = Mode_Put(RM, "ChallengeFoeType", NULL, foe) && Rm_Partners(list) ? Mode_Len(list) : -1;
	Native_SetEmpty(empty, NV_LIST);
	Native_SetEmpty(dict, NV_DICT);
	if (partners < 0 || !LvN_PutI(RM, "iStartPos", 0) || !Rm_G("iStartPos", i0) || !Mode_Put(RM, "nextStartPos", NULL, i0) ||
		!Mode_Put(RM, "Players", NULL, empty) || !Mode_Put(RM, "BayScorer", NULL, dict) || !LvN_PutI(RM, "FoesScorer", 0) ||
		!LvN_PutI(RM, "YourScore", -1) || !Mode_Put(RM, "WayPointlimit", NULL, limit) || !LvN_PutI(RM, "levelPathFind", -1) ||
		!LvN_PutI(RM, "NumOponNotGoNextItem", 0) || !Mode_ListSet("Race", "OnDeath", "RaceMaker.RacerKilled") ||
		!Mode_ListSet("Race", "OnDamage", "RaceMaker.RacerDamaged") ||
		!LvN_ModSetGlobal("Alarm", "AbortMissionCallback", RM, "RacerInterruptedByAlarm") ||
		!Mode_SaveInt("Race.Num", num) || !LvN_ModGet("MapRace", "Races", races) || !LvN_ModGet("MapRace", "Races", races2)) {
		return Mode_Done(&t, 0);
	}
	int len = Mode_Len(races2);
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(n, len);
	if (!Mode_Arith('%', num, n, pick) || !Mode_Sub(races, pick, race) || !Mode_CallArgs(race, NULL, NULL, 0) ||
		(!iscrazy && (!Mode_Arith('+', num, Mode_I(&t, 1), next) || (Mode_Repr(next, rep), !Mode_SetSaveVar("Race.Num", rep))))) {
		return Mode_Done(&t, 0);
	}
	long count;
	if (!Mode_Range(numply, &count)) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < count; i++) {
		if (!Rm_Racer(i, numply, iscrazy, partners)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(i0);
	if (!LvN_PutI(RM, "iStartPos", 0) || !Rm_G("iStartPos", i0) || !Mode_Put(RM, "nextStartPos", NULL, i0) ||
		!Rm_CheckPoints(shower) || !Rm_Call("PreloadCnDnResources", NULL, "") ||
		!LvN_ModSetGlobal("Menu", "MissionScorerCall", RM, "MissionScorer") || !Rm_Draw()) {
		return Mode_Done(&t, 0);
	}
	int timer = Native_True(onship) ? 0 : Rm_G("SkipTimer", skip) ? !Native_True(skip) : -1;
	if (timer < 0 || (timer && !Mode_GVarSetI("TimeToCancelCurrentMission", 60)) ||
		!LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", RM, "StopTimer") ||
		!LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", RM, "StartTimer")) {
		return Mode_Done(&t, 0);
	}
	int taunt = !Native_True(revenge) && !iscrazy;
	int msg = !iscrazy && !Native_True(onship);
	if (!Rm_RaceTaunt(foe, taunt, msg) || !LvN_GObj(RM, "OnInitRace", init)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(init) || (Rm_G("ChallengerName", cn) && Mode_CallArgs(init, NULL, cn, 1)));
}

/* InitCombat(foeType, isRevenge=0, isCrazyDeal=0, numFoes=1): Challenge.NumEnemies ships of the challenger (and its
 * partners) at the DM_Challenger_* dummy nearest to the player, as enemies; the timer to get into a ship (OBJLIB 3:
 * the other ports by name) */
static byte Rm_InitCombat(const nativeVal_t *foeArg, const nativeVal_t *revengeArg) {
	static const char *const mods[] = { "Menu", "Speech", "Parking", "OutPolice", "OutMap", "VehiclesNPC" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *now = Mode_V(&t), *lang = Mode_V(&t), *dummy = Mode_V(&t), *num = Mode_V(&t), *list = Mode_V(&t);
	nativeVal_t *dpos = Mode_V(&t), *prev = Mode_V(&t), *foe = Mode_V(&t), *npcs = Mode_V(&t), *n0 = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *usr = Mode_V(&t), *up = Mode_V(&t), *key = Mode_V(&t), *who = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *st = Mode_V(&t), *when = Mode_V(&t), *cad = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *bb = Mode_S(&t, "BadBoy");
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetFloat(now, Mode_Time());
	Mode_Lang("CombatHowTo", lang);
	PyObject *fn = Mode_GlobalPy(RM, "FirstTimeCombat");
	byte ok = fn && Mode_Schedule(now, fn, lang, 1, NULL);
	Py_XDECREF(fn);
	int partners = ok && Mode_Put(RM, "ChallengeFoeType", NULL, foeArg) &&
		Mode_ListSet("Enemies", "OnDeath", "RaceMaker.NPCCombatKilled") && Rm_NearestDummy(dummy) &&
		Mode_SaveInt("Challenge.NumEnemies", num) && Rm_Partners(list) ? Mode_Len(list) : -1;
	Native_SetEmpty(prev, NV_TUPLE);
	long count;
	if (partners < 0 || !LvN_Item(dummy, 1, dpos) || !LvN_Item(dpos, 0, Native_Append(prev)) ||
		!LvN_Item(dpos, 1, Native_Append(prev)) || !LvN_Item(dpos, 2, Native_Append(prev)) || !Mode_Range(num, &count)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(foe, foeArg);
	for (long i = 0; i < count; i++) {
		nativeVal_t e = { 0 }, en = { 0 }, np = { 0 }, l = { 0 }, nf = { 0 };
		ok = LvN_CreateAt(bb, prev, "Car", &e) && Mode_VGet(&e, "Name", &en) && Mode_Append(RM, "challenged_NPCs_Names", &en) &&
			Native_CallMod("OutMap", "GetNearestFreeSpawnPosReal", &np, "v", prev) && Rm_ShipAI(&e, foe, "Combat.Profile") &&
			Rm_Partners(&l) && LvN_Item(&l, partners == 1 ? 0 : i - 1, &nf) && LvN_SetS(&e, "EntityList", "Enemies");
		if (ok) {
			Native_Free(prev);
			*prev = np;
			Native_SetNone(&np);
			Native_Free(foe);
			*foe = nf;
			Native_SetNone(&nf);
		}
		Native_Free(&e);
		Native_Free(&en);
		Native_Free(&np);
		Native_Free(&l);
		Native_Free(&nf);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Rm_G("challenged_NPCs_Names", npcs) || !LvN_Item(npcs, 0, n0) || !Mode_EntitySeam(n0, v) || !Mode_UsrHandle(usr) ||
		!Mode_VObj(usr, "Pos", up) || !Rm_Call("PointObjectTo", NULL, "vv", v, up) ||
		!LvN_ModSetGlobal("Menu", "MissionScorerCall", RM, "MissionScorer") || !Rm_Draw() ||
		!Mode_GVarSetI("TimeToCancelCurrentMission", 60)) {
		return Mode_Done(&t, 0);
	}
	int taunt = !(revengeArg && Native_True(revengeArg));
	if (taunt && (!Rm_G("ChallengeFoeType", who) || !Mode_Cat(&t, key, 2, Mode_S(&t, "CombatHurryUp"), who) ||
		(Native_Free(who), !Rm_G("ChallengeFoeType", who)) || !Native_CallMod("Speech", "AddRemoteCom", NULL, "vv", key, who))) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", RM, "StopTimer") ||
		!Native_CallMod("OutMap", "GetNearestFreeSpawnPos", pos, "")) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	Native_Free(key);
	Native_Free(who);
	if (taunt) {
		Native_Free(lang);
		if (!Rm_G("ChallengeFoeType", who) || !Mode_Cat(&t, key, 2, Mode_S(&t, "CombatHurryUp"), who) || !Mode_LangV(key, lang) ||
			!Native_CallMod("Speech", "CalculateSpeechTime", st, "v", lang) || !Mode_Arith('+', now, st, when)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_Copy(when, now);
	}
	Mode_Lang("CombatSystemGetShip", cad);
	Native_SetStr(name, "SF_Secondary");
	fn = Rm_At(when, "MoveCombatNPCsToPos", pos, 1) ? Mode_GlobalPy(RM, "ShowSystemMessage") : NULL;
	ok = fn && Mode_Schedule(when, fn, cad, 1, name);
	Py_XDECREF(fn);
	return Mode_Done(&t, ok);
}

/* MoveCombatNPCsToPos(pos): the challenged ships that do not fight yet fly to the place (OBJLIB 7: SAI.SetStateVehicle
 * by name) */
static byte Rm_MoveNPC(const nativeVal_t *name, const void *arg) {
	const nativeVal_t *pos = arg;
	nativeVal_t st = { 0 }, p[3] = { { 0 }, { 0 }, { 0 } };
	int go = Mode_AIState(name, &st) ? Mode_CmpI(&st, "!=", 2) : -1;
	byte ok = go >= 0 && (!go || (LvN_Item(pos, 0, &p[0]) && LvN_Item(pos, 1, &p[1]) && LvN_Item(pos, 2, &p[2]) &&
		Native_CallMod("SAI", "SetStateVehicle", NULL, "ivvvviidd", 1L, name, &p[0], &p[1], &p[2], 1800L, 0L, 0.85, 20.0, 20.0)));
	Native_Free(&st);
	for (int i = 0; i < 3; i++) {
		Native_Free(&p[i]);
	}
	return ok;
}

/* PlaceNPCAtRacePos(itemName, vehicleName): the challenger reached its start item: it moves onto it and waits */
static byte Rm_PlaceNPCAtRacePos(const nativeVal_t *item, const nativeVal_t *veh) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cn = Mode_V(&t), *i = Mode_V(&t), *v = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t), *vn = Mode_V(&t);
	int go = Rm_G("ChallengerName", cn) ? Mode_Cmp(veh, "==", cn) : -1;
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	return Mode_Done(&t, Mode_EntitySeam(item, i) && Mode_EntitySeam(veh, v) && Mode_VObj(i, "Pos", p) && Mode_VSet(v, "MovePos", p) &&
		Mode_VObj(i, "Ang", a) && Mode_VSet(v, "MoveAng", a) && LvN_SetI(v, "MoveTime", 1) && LvN_SetI(v, "MoveFade", 2) &&
		Mode_VObj(v, "Name", vn) && Native_CallMod("SAI", "SetStateVehicle", NULL, "iv", 0L, vn) && Mode_VDel(i) &&
		LvN_ModNone(RM, "NPC_START_ITEM"));
}

/* SScorer.Set(id, item, "Text", `global`) */
static byte Rm_ScoreText(const nativeVal_t *id, const char *item, const nativeVal_t *v) {
	nativeVal_t rep, it;
	Mode_Repr(v, &rep);
	Native_SetStr(&it, item);
	byte ok = Mode_ScorerSet(id, &it, "Text", &rep);
	Native_Free(&rep);
	return ok;
}

/* SScorer.Set(id, "MissionYou", var, i) */
static byte Rm_You(const nativeVal_t *id, const char *var, long i) {
	nativeVal_t it, v;
	Native_SetStr(&it, "MissionYou");
	Native_SetInt(&v, i);
	return Mode_ScorerSet(id, &it, var, &v);
}

/* SetScorerValues(id): the scores of the race on the scorer (the leader on top), the race scorer when none is set
 * (OBJLIB 3: Menu by name) */
static byte Rm_SetScorerValues(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *call = Mode_V(&t), *best = Mode_V(&t), *lim = Mode_V(&t), *you = Mode_V(&t), *you2 = Mode_V(&t);
	if (!Mode_Import("Menu") || !LvN_ModGet("Menu", "MissionScorerCall", call)) {
		return Mode_Done(&t, 0);
	}
	int race = Native_True(call) ? 0 : Rm_IsRace();
	if (race < 0 || (race && (!LvN_ModSetGlobal("Menu", "MissionScorerCall", RM, "MissionScorer2") || !Rm_Draw()))) {
		return Mode_Done(&t, 0);
	}
	int ahead = Rm_G("FoesScorer", best) && Rm_G("WayPointlimit", lim) && Rm_ScoreText(id, "MissionLimit", lim) &&
		Rm_G("YourScore", you) ? Mode_Cmp(you, ">=", best) : -1;
	if (ahead < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Rm_G("YourScore", you2) && Rm_ScoreText(id, "MissionBest", ahead ? you2 : best) &&
		Rm_ScoreText(id, "MissionBad", ahead ? best : you2) && Rm_You(id, "Y", ahead ? 180 : 200) &&
		Rm_You(id, "Blue", ahead ? 255 : 0) && Rm_You(id, "Red", ahead ? 0 : 255));
}

/* MissionScorer2(id): the race scores once the race started (data/menus/race.def, section scores; OBJLIB 6: the
 * $names as a dict) */
static byte Rm_MissionScorer2(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *you = Mode_V(&t);
	PyObject *mh = Mode_From("Scorer", "MarginH"), *mv = mh ? Mode_From("Scorer", "MarginV") : NULL;
	int go = mv && Mode_Import("Menu") && Rm_G("YourScore", you) ? Mode_CmpI(you, ">", -1) : -1;
	Py_XDECREF(mv);
	int pid = 0;
	PyObject *vars = go > 0 && Native_ArgInt(id, &pid) ? Py_BuildValue("{s:O}", "mh", mh) : NULL;
	Py_XDECREF(mh);
	if (vars) {
		Script_RunFile(pid, "menus/race.def", "scores", race_def, NULL, vars);
		Py_DECREF(vars);
	}
	return Mode_Done(&t, go == 0 || (vars && Rm_Call("SetScorerValues", NULL, "v", id)));
}

/* MissionScorer(id): the timer to get into a ship (data/menus/race.def, section timer), the scores in a race
 * (OBJLIB 6: the $names as a dict) */
static byte Rm_MissionScorer(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tx = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *col = Mode_V(&t), *c = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	int pid = 0;
	if (!Mode_Import("Scorer") || !LvN_ModGet("Scorer", "MissionScorerText_X", tx) || !Mode_Arith('+', tx, Mode_I(&t, 28), x) ||
		!LvN_ModGet("Scorer", "MissionScorerText_Y", y)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		Native_Free(col);
		if (!LvN_ModGet("Scorer", "MissionScorerText_Color", col) || !LvN_Item(col, i, &c[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Native_ArgInt(id, &pid)) {
		return Mode_Done(&t, 0);
	}
	PyObject *o[6] = { Native_ToPy(x), Native_ToPy(y), Native_ToPy(&c[0]), Native_ToPy(&c[1]), Native_ToPy(&c[2]),
		Native_ToPy(&c[3]) };
	PyObject *vars = o[0] && o[1] && o[2] && o[3] && o[4] && o[5] ? Py_BuildValue("{s:O,s:O,s:O,s:O,s:O,s:O}", "x", o[0], "y",
		o[1], "r", o[2], "g", o[3], "b", o[4], "a", o[5]) : NULL;
	for (int i = 0; i < 6; i++) {
		Py_XDECREF(o[i]);
	}
	if (!vars) {
		return Mode_Done(&t, 0);
	}
	Script_RunFile(pid, "menus/race.def", "timer", race_def, NULL, vars);
	Py_DECREF(vars);
	int race = Rm_IsRace();
	return Mode_Done(&t, race >= 0 && (!race || Rm_Call("MissionScorer2", NULL, "v", id)));
}

/* InitShipIAProfile(ChallengeFoeType, profileType): an AI profile in VehiclesNPC.AIProf for each ship of the kind in
 * AISHIPS ("S<kind><n>_<profile>"); OBJLIB 3: the profile objects of VehiclesNPC by name */
static byte Rm_InitShipIAProfile(const nativeVal_t *foe, const nativeVal_t *profile) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cntr = Mode_V(&t), *ais = Mode_V(&t), *ships = Mode_V(&t);
	Native_SetInt(cntr, 0);
	int n = Mode_Import("VehiclesNPC") && Rm_G("AISHIPS", ais) && Mode_Sub(ais, foe, ships) ? Mode_Loop(ships) : -1;
	for (int i = 0; i < n; i++) {
		nativeVal_t type = { 0 }, c = { 0 }, sp = { 0 }, rep = { 0 }, key = { 0 };
		byte ok = Native_SeqItem(ships, i, &type) && Mode_Arith('+', cntr, Mode_I(&t, 1), &c);
		if (ok) {
			Native_Free(cntr);
			Native_Copy(cntr, &c);
			Mode_Repr(cntr, &rep);
			ok = Native_CallMod("VehiclesNPC", "cShipAI", &sp, "") && Mode_VSet(&sp, "Ship", &type) &&
				Native_CallMod("VehiclesNPC", "SetAIProfile", NULL, "vv", &sp, profile) &&
				Mode_Cat(&t, &key, 5, Mode_S(&t, "S"), foe, &rep, Mode_S(&t, "_"), profile) &&
				Mode_ModDictPut("VehiclesNPC", "AIProf", &key, &sp);
		}
		Native_Free(&type);
		Native_Free(&c);
		Native_Free(&sp);
		Native_Free(&rep);
		Native_Free(&key);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* GetRandomShipIAProfile(ChallengeFoeType, profileType): the name of a profile of InitShipIAProfile: in turn for the
 * mercenaries and the press, else at random */
static byte Rm_GetRandomShipIAProfile(const nativeVal_t *foe, const nativeVal_t *profile, nativeVal_t *out) {
	static const char *const groups[] = { "Mercs", "Press", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ais = Mode_V(&t), *ships = Mode_V(&t), *len = Mode_V(&t), *ni = Mode_V(&t), *n1 = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t), *head = Mode_V(&t), *n = Mode_V(&t), *rep = Mode_V(&t), *f = Mode_V(&t), *nine = Mode_V(&t);
	nativeVal_t *s = Mode_V(&t);
	int turn = Rm_InS(foe, groups), l;
	/* the order of the script: the count of the ships first in turn, the head first at random */
	if (turn) {
		l = Rm_G("AISHIPS", ais) && Mode_Sub(ais, foe, ships) ? Mode_Len(ships) : -1;
		if (l < 0) {
			return Mode_Done(&t, 0);
		}
		Native_SetInt(len, l);
		Native_Free(ni);
		if (!Rm_G("nextIAProfile", ni) || !Mode_Arith('+', ni, Mode_I(&t, 1), n1) || !Mode_Arith('%', n1, len, r) ||
			!Mode_Put(RM, "nextIAProfile", NULL, r) || !Mode_Cat(&t, head, 2, Mode_S(&t, "S"), foe) ||
			(Native_Free(ni), !Rm_G("nextIAProfile", ni)) || !Mode_Arith('+', ni, Mode_I(&t, 1), n)) {
			return Mode_Done(&t, 0);
		}
	} else {
		l = Mode_Cat(&t, head, 2, Mode_S(&t, "S"), foe) && Rm_G("AISHIPS", ais) && Mode_Sub(ais, foe, ships) ? Mode_Len(ships) : -1;
		if (l < 0) {
			return Mode_Done(&t, 0);
		}
		Native_SetFloat(f, Scrap_RandRange(1, l));
		Native_SetFloat(nine, 0.9);
		if (!Mode_Arith('+', f, nine, s) || !Native_ToInt(s, n)) {
			return Mode_Done(&t, 0);
		}
	}
	Mode_Repr(n, rep);
	return Mode_Done(&t, Mode_Cat(&t, out, 4, head, rep, Mode_S(&t, "_"), profile));
}

/* the adapters (the arity errors of the script's functions) */
MODE_FN(RaceMaker, ShowSystemMessage, 1, Rm_ShowSystemMessage(&a[0]))
MODE_FN(RaceMaker, ShowSystemMessageDelay, 2, Mode_LaterV(&a[1], RM, "ShowSystemMessage", &a[0], 1, NULL))
MODE_FN(RaceMaker, NPCCombatKilled, 2, Rm_NPCCombatKilled(&a[0]))
MODE_FN(RaceMaker, DeleteCurrentWaypoint, 0, Rm_DeleteCurrentWaypoint())
MODE_FN(RaceMaker, MovePlayersToTraffic, 0, Rm_MovePlayersToTraffic())
MODE_FN(RaceMaker, EndOfRace, 2, Rm_EndOfRace(&a[0], &a[1]))
MODE_FN(RaceMaker, RaceStart, 2, Rm_RaceStart(&a[0]))
MODE_FN(RaceMaker, RacerKilled, 2, Rm_Stop("Killed", 1))
MODE_FN(RaceMaker, RacerInterruptedByAlarm, 0, Rm_Stop("Alarm", 0))
MODE_FN(RaceMaker, RacerDamaged, 3, Rm_RacerDamaged())
MODE_FN(RaceMaker, RacerLeft, 0, Rm_Stop("Left", 0))
MODE_FN(RaceMaker, AbortCameraTravelling, 0, Rm_AbortCameraTravelling())
MODE_FN(RaceMaker, OnRaceStart, 2, Rm_OnRaceStart(&a[0], &a[1]))
MODE_FN(RaceMaker, RaceGo, 2, Rm_RaceGo(&a[0], &a[1]))
MODE_FN(RaceMaker, CreateRaceCountDown, 0, Rm_CreateRaceCountDown())
MODE_FN(RaceMaker, DestroyRaceCountDown, 0, Rm_DestroyRaceCountDown())
MODE_FN(RaceMaker, DestroyRaceCountDownEnd, 1, Rm_DestroyRaceCountDownEnd(&a[0]))
MODE_FN(RaceMaker, OnCheckPoint, 2, Rm_OnCheckPoint(&a[0], &a[1]))
MODE_FN(RaceMaker, OnNearCheckPoint, 2, Rm_OnNearCheckPoint(&a[0], &a[1]))
MODE_FN(RaceMaker, NameOponGoItem, 1, Rm_NameOponGoItem(&a[0], out))
MODE_FN(RaceMaker, LocalGetDealType, 0, (Mode_SaveVar("Challenge.Type", out), 1))
MODE_FN(RaceMaker, StopTimer, 0, Rm_StopTimer())
MODE_FN(RaceMaker, StartTimer, 0, Rm_StartTimer())
MODE_FN(RaceMaker, FirstTimeRace, 1, Rm_FirstTime(&a[0], "Race.FirstTime"))
MODE_FN(RaceMaker, FirstTimeCombat, 1, Rm_FirstTime(&a[0], "Combat.FirstTime"))
MODE_FN(RaceMaker, PointObjectTo, 2, Rm_PointObjectTo(&a[0], &a[1]))
MODE_FN(RaceMaker, PreloadCnDnResources, 0, Rm_PreloadCnDnResources())
MODE_FN(RaceMaker, MoveCombatNPCsToPos, 1, Rm_Each("challenged_NPCs_Names", Rm_MoveNPC, &a[0]))
MODE_FN(RaceMaker, PlaceNPCAtRacePos, 2, Rm_PlaceNPCAtRacePos(&a[0], &a[1]))
MODE_FN(RaceMaker, SetScorerValues, 1, Rm_SetScorerValues(&a[0]))
MODE_FN(RaceMaker, MissionScorer2, 1, Rm_MissionScorer2(&a[0]))
MODE_FN(RaceMaker, MissionScorer, 1, Rm_MissionScorer(&a[0]))
MODE_FN(RaceMaker, InitShipIAProfile, 2, Rm_InitShipIAProfile(&a[0], &a[1]))
MODE_FN(RaceMaker, GetRandomShipIAProfile, 2, Rm_GetRandomShipIAProfile(&a[0], &a[1], out))

/* the keyword functions (MODE_KW) */
MODE_KW(RaceMaker, OnCounterMeasure, "|O", "id") {
	return Rm_OnCounterMeasure();
}

MODE_KW(RaceMaker, StartCameraTravelling, "OO|OO", "ItemName", "VehicleName", "isRace", "speed") {
	Mode_KwDefI(v, given, 2, 1);
	Mode_KwDefS(v, given, 3, "Normal");
	return Rm_StartCameraTravelling(&v[0], &v[1], &v[2], &v[3], out);
}

MODE_KW(RaceMaker, Counter, "O|OOO", "num", "r", "g", "b") {
	nativeVal_t *rgb[3] = { given[1] ? &v[1] : NULL, given[2] ? &v[2] : NULL, given[3] ? &v[3] : NULL };
	return Rm_Counter(&v[0], rgb);
}

MODE_KW(RaceMaker, InitRace, "O|OOOOOO", "foeType", "numply", "waypointlimit", "shower", "isRevenge", "isCrazyDeal",
	"userOnShip") {
	nativeVal_t *opt[6];
	for (int i = 0; i < 6; i++) {
		opt[i] = given[i + 1] ? &v[i + 1] : NULL;
	}
	return Rm_InitRace(&v[0], opt);
}

MODE_KW(RaceMaker, InitCombat, "O|OOO", "foeType", "isRevenge", "isCrazyDeal", "numFoes") {
	return Rm_InitCombat(&v[0], given[1] ? &v[1] : NULL);
}

#define KW(fn) (PyCFunction)(void (*)(void))fn, 3

PyMethodDef g_RaceMakerMethods[] = {
	{ "ShowSystemMessage", RaceMaker_ShowSystemMessage, 1, NULL },
	{ "ShowSystemMessageDelay", RaceMaker_ShowSystemMessageDelay, 1, NULL },
	{ "NPCCombatKilled", RaceMaker_NPCCombatKilled, 1, NULL },
	{ "DeleteCurrentWaypoint", RaceMaker_DeleteCurrentWaypoint, 1, NULL },
	{ "MovePlayersToTraffic", RaceMaker_MovePlayersToTraffic, 1, NULL },
	{ "EndOfRace", RaceMaker_EndOfRace, 1, NULL },
	{ "RaceStart", RaceMaker_RaceStart, 1, NULL },
	{ "OnCounterMeasure", KW(RaceMaker_OnCounterMeasure), NULL },
	{ "RacerKilled", RaceMaker_RacerKilled, 1, NULL },
	{ "RacerInterruptedByAlarm", RaceMaker_RacerInterruptedByAlarm, 1, NULL },
	{ "RacerDamaged", RaceMaker_RacerDamaged, 1, NULL },
	{ "RacerLeft", RaceMaker_RacerLeft, 1, NULL },
	{ "AbortCameraTravelling", RaceMaker_AbortCameraTravelling, 1, NULL },
	{ "StartCameraTravelling", KW(RaceMaker_StartCameraTravelling), NULL },
	{ "OnRaceStart", RaceMaker_OnRaceStart, 1, NULL },
	{ "RaceGo", RaceMaker_RaceGo, 1, NULL },
	{ "CreateRaceCountDown", RaceMaker_CreateRaceCountDown, 1, NULL },
	{ "DestroyRaceCountDown", RaceMaker_DestroyRaceCountDown, 1, NULL },
	{ "DestroyRaceCountDownEnd", RaceMaker_DestroyRaceCountDownEnd, 1, NULL },
	{ "Counter", KW(RaceMaker_Counter), NULL },
	{ "OnCheckPoint", RaceMaker_OnCheckPoint, 1, NULL },
	{ "OnNearCheckPoint", RaceMaker_OnNearCheckPoint, 1, NULL },
	{ "NameOponGoItem", RaceMaker_NameOponGoItem, 1, NULL },
	{ "LocalGetDealType", RaceMaker_LocalGetDealType, 1, NULL },
	{ "StopTimer", RaceMaker_StopTimer, 1, NULL },
	{ "StartTimer", RaceMaker_StartTimer, 1, NULL },
	{ "FirstTimeRace", RaceMaker_FirstTimeRace, 1, NULL },
	{ "FirstTimeCombat", RaceMaker_FirstTimeCombat, 1, NULL },
	{ "PointObjectTo", RaceMaker_PointObjectTo, 1, NULL },
	{ "PreloadCnDnResources", RaceMaker_PreloadCnDnResources, 1, NULL },
	{ "InitRace", KW(RaceMaker_InitRace), NULL },
	{ "InitCombat", KW(RaceMaker_InitCombat), NULL },
	{ "MoveCombatNPCsToPos", RaceMaker_MoveCombatNPCsToPos, 1, NULL },
	{ "PlaceNPCAtRacePos", RaceMaker_PlaceNPCAtRacePos, 1, NULL },
	{ "SetScorerValues", RaceMaker_SetScorerValues, 1, NULL },
	{ "MissionScorer2", RaceMaker_MissionScorer2, 1, NULL },
	{ "MissionScorer", RaceMaker_MissionScorer, 1, NULL },
	{ "InitShipIAProfile", RaceMaker_InitShipIAProfile, 1, NULL },
	{ "GetRandomShipIAProfile", RaceMaker_GetRandomShipIAProfile, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(RaceMaker)
