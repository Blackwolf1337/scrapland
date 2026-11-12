/*
 * The MissionsFuncs module in C (scripts/missions/missionsfuncs.py, issue #2 step 8a): the mission flow of the single
 * player game. The start of a level (the arrow to the next mission, the auto save, the initial message), the next
 * mission and its map, the mission targets on the radar, the dialog of the mission characters, the death of the
 * player (lives, the jail, the revenge of a challenge), the photo objects, the waves of Bertos, the crazy deals and
 * the super deals with their score tables.
 *
 * The tables and the state stay in the module (WorldMap, currentTarget, targetsList, the Berto waves with their hooks
 * OnNewWaveCallback and OnBertoDeathCallback, cameraData, mapsCameraData, the names and colours of the score tables)
 * with the module code (the save variables it creates), because the missions read and change them; the functions read
 * them as their globals.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the globals where the
 * script reads them and write them back at once (OBJLIB 5: the missions and the cases read and set them); the lists
 * targetsList and currentBertos change in place, and the list SetMultipleMissionTargets gets becomes targetsList itself.
 * The save variables, the engine variables, the time, the language texts, the entity lists and string.upper are C;
 * the save texts are made with the 1.5.2 repr and read with the literal reader of the native layer. Scrap.UsrEntity,
 * GetEntity, CreateEntity, Set, SetAlarm, DelSaveVars, ConsoleError, AddScheduledFunc, SInput.GetActionSet,
 * SetActionSet and SScorer.SetMsgText go through their seams, and so does the Scrap.SetSaveVar of SuperDealOnVictory
 * (OBJLIB 7: the cases record or swap them); Scrap.SetCam and SAI.SetStateVehicle go through their refs (stage 7).
 * Entities are handle values and the characters of CharsNPC.CharRes and their option lists the objects they are
 * (OBJLIB 7, 5). The other ports (SaveGame, Speech, Menu, Challenge, PhoneCab, Items, VehiclesNPC, Vehicles,
 * RacerScorer, Scorer) and the functions of the module the script calls through its globals are called through their
 * refs (PORT_CALL, PORT_OWN: the port seam, OBJLIB 5). The hooks OnNewWaveCallback and OnBertoDeathCallback (values
 * a mission puts into the module, OBJLIB 5) and the builders of the super deal types (a table of names) are called
 * through the entries of the functions they hold (MODE_CBGLOBAL). The functions given to the scheduler by name or
 * value, the revenge and the auto save among them, take their entries (stage 4: Port_Scheduled). The code strings of
 * OnAbort and OnVictory run through the reader of the lines the game execs (ScriptExec.c), on native values with the
 * calls through the entries.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define MF "MissionsFuncs"

/* ---- helpers of the module ---- */

/* v == "text" of the script (a text with the same bytes) */
static byte Mf_Is(const nativeVal_t *v, const char *text) {
	return v->type == NV_STR && v->n == (int)strlen(text) && !memcmp(Native_Text(v), text, (size_t)v->n);
}

/* Scrap.GetSaveVar(name) == text: a save variable is a C text or None */
static byte Mf_SaveIs(const char *name, const char *text) {
	return LvN_SaveIs(name, text);
}

/* "CrazyDeal." + `number` + suffix */
static byte Mf_DealVar(modeVals_t *t, const nativeVal_t *number, const char *suffix, nativeVal_t *out) {
	nativeVal_t *rep = Mode_V(t);
	Mode_Repr(number, rep);
	return Mode_Cat(t, out, 3, Mode_S(t, "CrazyDeal."), rep, Mode_S(t, suffix));
}

/* `x in` the global list of the module (a list that holds handles the Python way): 1, 0, -1 */
static int Mf_InG(const nativeVal_t *x, const char *name) {
	nativeVal_t l = { 0 };
	int r = Mode_G(MF, name, &l) ? Mode_In(&l, x) : -1;
	Native_Free(&l);
	return r;
}

/* d.has_key(key) of a value: a native dict in C, any other value through its method (the errors of the script) */
static int Mf_HasKey(const nativeVal_t *d, const nativeVal_t *key) {
	long h;
	if (d->type == NV_DICT && Native_Hash(key, &h)) {
		return Native_DictGet(d, key) != NULL;
	}
	return Mode_RefHasKey(d, key);
}

/* e = Scrap.GetEntity(Scrap.EntityListGet(list, "First")) (the list a value of the script; GetEntity through its seam) */
static byte Mf_First(const nativeVal_t *list, nativeVal_t *e) {
	nativeVal_t name = { 0 };
	const char *s;
	Native_SetNone(e);
	byte ok = Native_ArgStr(list, 1, &s) && Mode_ListGet(s, "First", &name) && Mode_EntitySeam(&name, e);
	Native_Free(&name);
	return ok;
}

/* the Scrap.SetSaveVar(name, text) of SuperDealOnVictory through its ref (the seam: the stage 4 cases
 * record it) */
static byte Mf_SetSaveSeam(const char *name, const char *text) {
	return PORT_CALL(Scrap, SetSaveVar, NULL, "ss", name, text);
}

/* (a, b) != (Scrap.GetSaveVar("Mission.Map"), Scrap.GetSaveVar("Mission.Library")): the level is reloaded. The
 * script compares the mission with the map and the map with the mission, so this holds unless both are equal. */
static byte Mf_ForceReload(const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t pair, saved;
	Native_SetEmpty(&pair, NV_TUPLE);
	Native_Copy(Native_Append(&pair), a);
	Native_Copy(Native_Append(&pair), b);
	Native_SetEmpty(&saved, NV_TUPLE);
	Mode_SaveVar("Mission.Map", Native_Append(&saved));
	Mode_SaveVar("Mission.Library", Native_Append(&saved));
	int c = Mode_Cmp(&pair, "!=", &saved);
	Native_Free(&pair);
	Native_Free(&saved);
	return c >= 0 && (!c || Mode_GVarSetI("ForceReload", 1));
}

/* ActivateTarget(GetEntity(name), status) when the entity is there */
static byte Mf_Target(const nativeVal_t *name, long status) {
	nativeVal_t e = { 0 };
	byte ok = Mode_EntitySeam(name, &e) && (!Native_True(&e) || PORT_OWN(MissionsFuncs, ActivateTarget, NULL, "vi", &e, status));
	Native_Free(&e);
	return ok;
}

/* Mf_Target(x, status) for every x of the global list name (read once, as the for loop of the script) */
static byte Mf_TargetAll(const char *name, long status) {
	nativeVal_t l = { 0 }, x = { 0 };
	byte ok = Mode_G(MF, name, &l);
	int n = ok ? Mode_Loop(&l) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&l, i, &x) && Mf_Target(&x, status);
		Native_Free(&x);
	}
	Native_Free(&l);
	return ok;
}

/* ---- the mission flow ---- */

/* Init(): the start of a level: the challenges outdoors, the arrow to the mission, the auto save and the initial
 * message */
static byte Mf_Init(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *outdoor = Mode_V(&t), *msg = Mode_V(&t), *time = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_Import("SaveGame") || !Mode_ModGet("Init", "Outdoor", outdoor)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_CmpI(outdoor, "==", 1);
	if (c < 0 || (c && (!Mode_Import("Challenge") || !PORT_CALL(Challenge, Init, NULL, "")))) {
		return Mode_Done(&t, 0);
	}
	if (!PORT_OWN(MissionsFuncs, SetOutMissionArrow, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	if (Mf_SaveIs("AutoSaveGameOnLoad", "1")) {
		/* Scrap.AddScheduledFunc(0, SaveGame.SaveGame, (0,)): the time the int 0 (the scheduler takes the entry of the
		 * function of the module) */
		nativeVal_t *fn = Mode_V(&t);
		byte ok = Mode_RefMod("SaveGame", "SaveGame", fn);
		Native_SetInt(time, 0);
		if (!ok || !Mode_ScheduleV(time, fn, Mode_I(&t, 0), 1, NULL)) {
			return Mode_Done(&t, 0);
		}
	}
	Mode_SaveVar("InitialMessage", msg);
	if (Native_True(msg) && (!PORT_OWN(MissionsFuncs, TryMsg, NULL, "v", msg) || !Mode_DelSaveVars("InitialMessage"))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, 1);
}

/* TryMsg(InitMsg): the message when the player walks or flies and no other one shows, else a second later */
static byte Mf_TryMsg(const nativeVal_t *msg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *set = Mode_V(&t), *at = Mode_V(&t), *now = Mode_V(&t), *len = Mode_V(&t);
	nativeVal_t *when = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int go = Native_True(usr);
	if (go) {
		if (!Mode_ActionSetGet(set)) {
			return Mode_Done(&t, 0);
		}
		go = Mode_Cmp(set, "==", Mode_S(&t, "Racer"));
		if (!go) {
			go = Mode_Cmp(set, "==", Mode_S(&t, "Walk"));
		}
	}
	if (go > 0) {
		if (!Native_GVarGet("TimeToMsg", at)) {
			return Mode_Done(&t, 0);
		}
		Native_SetFloat(now, Mode_Time());
		go = Mode_Cmp(at, "<", now);
	}
	if (go < 0) {
		return Mode_Done(&t, 0);
	}
	if (!go) {
		/* the global TryMsg by name: the scheduler takes its entry */
		return Mode_Done(&t, Mode_Later(1.0, MF, "TryMsg", msg, 1, NULL));
	}
	Native_SetFloat(now, Mode_Time());
	return Mode_Done(&t, Mode_Import("Speech") && PORT_CALL(Speech, CalculateSpeechTime, len, "v", msg) &&
		Mode_Arith('+', now, len, when) && Mode_MsgTextV(msg, when));
}

/* SetNextMission(MissionName, AtMap): the next mission, its arrow, the way point and an auto save at the load */
static byte Mf_SetNextMission(const nativeVal_t *mission, const nativeVal_t *map) {
	if (!Mode_SetSaveVar("Mission.Map", map) || !Mode_SetSaveVar("Mission.Library", mission) ||
		!PORT_OWN(MissionsFuncs, SetOutMissionArrow, NULL, "") || !Mode_Import("SaveGame") ||
		!PORT_CALL(SaveGame, QWayPoint, NULL, "")) {
		return 0;
	}
	SaveVar_Set("AutoSaveGameOnLoad", "1");
	return Mf_ForceReload(mission, map);
}

/* SetFirstDemoMission(): the first mission of the demo */
static byte Mf_SetFirstDemoMission(void) {
	SaveVar_Set("Mission.Map", "GamblinDen");
	SaveVar_Set("Mission.Library", "Mission_CrazyGamblerFirstContact");
	if (!PORT_OWN(MissionsFuncs, SetOutMissionArrow, NULL, "")) {
		return 0;
	}
	SaveVar_Set("AutoSaveGameOnLoad", "1");
	nativeVal_t a, b;
	Native_SetStr(&a, "Mission_CrazyGamblerFirstContact");
	Native_SetStr(&b, "GamblinDen");
	return Mf_ForceReload(&a, &b);
}

/* SetOutMissionArrow(): the exit of this level towards the map of the mission (WorldMap) shows the mission arrow
 * (on the radar only in the junkyard) */
static byte Mf_SetOutMissionArrow(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *rest = Mode_V(&t), *actual = Mode_V(&t), *mission = Mode_V(&t), *target = Mode_V(&t);
	nativeVal_t *world = Mode_V(&t), *exits = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Init") || !Mode_ModGet("Init", "Path", path) ||
		!Native_Slice(path, 7, INT_MAX, rest) || !Native_Upper(rest, actual)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Mission.Map", mission);
	if (!Native_Upper(mission, target)) {
		return Mode_Done(&t, 0);
	}
	int go = Mode_Cmp(actual, "!=", target);
	if (go > 0) {
		go = Mode_G(MF, "WorldMap", world) ? Mf_HasKey(world, actual) : -1;
	}
	if (go > 0) {
		Native_Free(world);
		go = Mode_G(MF, "WorldMap", world) && Mode_Sub(world, actual, exits) ? Mf_HasKey(exits, target) : -1;
	}
	if (go > 0) {
		Native_Free(world);
		Native_Free(exits);
		if (!Mode_G(MF, "WorldMap", world) || !Mode_Sub(world, actual, exits) || !Mode_Sub(exits, target, name) ||
			!Mode_EntitySeam(name, e)) {
			return Mode_Done(&t, 0);
		}
		long out = Mode_Cmp(actual, "!=", Mode_S(&t, "JUNKYARD"));
		return Mode_Done(&t, PORT_OWN(MissionsFuncs, ActivateTarget, NULL, "vii", e, 1L, out));
	}
	return Mode_Done(&t, go >= 0);
}

/* ActivateTarget(e, status, elcomoe=1): on the radar; with elcomoe a character also gets the mission arrow (1) or
 * the target arrow (2) */
static byte Mf_ActivateTarget(const nativeVal_t *e, const nativeVal_t *status, const nativeVal_t *arrow) {
	nativeVal_t file = { 0 };
	if (!Mode_VSet(e, "RadarShow", status)) {
		return 0;
	}
	int life = Mode_VIsType(e, "WithLife");
	if (life > 0 && arrow) {
		life = Native_True(arrow);
	}
	if (life <= 0) {
		return life == 0;
	}
	if (!Mode_VSet(e, "ArrowCanMission", status)) {
		return 0;
	}
	const char *var = Mode_EqI(status, 1) ? "ArrowMissionFile" : Mode_EqI(status, 2) ? "ArrowTargetFile" : NULL;
	byte ok = !var || (Native_GVarGet(var, &file) && Mode_VSet(e, "ArrowModMission", &file));
	Native_Free(&file);
	return ok;
}

/* GetSecondary(val): the state of the secondary mission */
static byte Mf_GetSecondary(nativeVal_t *out) {
	return Native_GVarGet("IsSecondMission", out);
}

/* SetSecondary(val): the state of the secondary mission, saved */
static byte Mf_SetSecondary(const nativeVal_t *val) {
	LvN_SaveRepr("IsSecondMission", val);
	return Native_GVarSet("IsSecondMission", val);
}

/* FinishGame(id): the end of the game */
static byte Mf_FinishGame(const nativeVal_t *id) {
	return Mode_Import("Menu") && Mode_Import("Init") && Mode_Import("SaveGame") && Mode_Import("Init") &&
		PORT_CALL(Menu, MovieScorer, NULL, "v", id) && PORT_CALL(Menu, GameOver, NULL, "v", id) &&
		Mode_ActionSet("Inactive");
}

/* ShowMissionInfo(id): the mission info pages (the engine's MainMissionEvent) */
static byte Mf_ShowMissionInfo(void) {
	return Mode_Import("Menu") && PORT_CALL(Menu, EnterMissionInfo, NULL, "i", 0L);
}

/* ---- the mission targets ---- */

/* SetMissionTarget(newTarget): the one target of the mission */
static byte Mf_SetMissionTarget(const nativeVal_t *tgt) {
	nativeVal_t e = { 0 };
	if (!PORT_OWN(MissionsFuncs, CleanAllTargets, NULL, "") || !Mode_Put(MF, "currentTarget", NULL, tgt)) {
		return 0;
	}
	byte ok = !Native_True(tgt) || (Mode_EntitySeam(tgt, &e) && PORT_OWN(MissionsFuncs, ActivateTarget, NULL, "vi", &e, 1L));
	Native_Free(&e);
	return ok;
}

/* SetMultipleMissionTargets(tgtList): several targets; the list becomes targetsList itself (OBJLIB 5: the mission
 * keeps it and sees DelMissionTarget take its names out) */
static byte Mf_SetMultipleMissionTargets(const nativeVal_t *list) {
	/* a reference is the object itself, a native list becomes a new list object (as its call through Python makes it) */
	return PORT_OWN(MissionsFuncs, CleanAllTargets, NULL, "") && Mode_Put(MF, "targetsList", NULL, list) &&
		LvN_PutNone(MF, "currentTarget") && Mf_TargetAll("targetsList", 1);
}

/* CleanAllTargets(): no target any more */
static byte Mf_CleanAllTargets(void) {
	nativeVal_t cur = { 0 }, empty;
	if (!Mode_G(MF, "currentTarget", &cur)) {
		return 0;
	}
	byte ok = 1;
	if (Native_True(&cur)) {
		Native_Free(&cur);
		ok = Mode_G(MF, "currentTarget", &cur) && Mf_Target(&cur, 0);
	}
	Native_Free(&cur);
	Native_SetEmpty(&empty, NV_LIST);
	return ok && Mf_TargetAll("targetsList", 0) && Mode_Put(MF, "targetsList", NULL, &empty);
}

/* DelMissionTarget(target): one target less; 1 when none is left */
static byte Mf_DelMissionTarget(const nativeVal_t *tgt, nativeVal_t *out) {
	nativeVal_t l = { 0 };
	int in = Mf_InG(tgt, "targetsList");
	if (in < 0 || (in && (!Mode_Remove(MF, "targetsList", tgt) || !Mf_Target(tgt, 0)))) {
		return 0;
	}
	int n = Mode_G(MF, "targetsList", &l) ? Mode_Len(&l) : -1;
	Native_Free(&l);
	if (n < 0) {
		return 0;
	}
	Native_SetInt(out, n == 0);
	return 1;
}

/* IsCharMissionObjective(charName): 1 when the character is a target */
static byte Mf_IsCharMissionObjective(const nativeVal_t *name, nativeVal_t *out) {
	int is = Mode_GCmp(MF, "currentTarget", "==", name);
	if (is == 0) {
		is = Mf_InG(name, "targetsList");
	}
	if (is < 0) {
		return 0;
	}
	Native_SetInt(out, is);
	return 1;
}

/* `if cls.name:` (1, 0, -1 with the error) */
static int Mf_AttrTrue(const nativeVal_t *cls, const char *name) {
	nativeVal_t v;
	int t = Mode_RefGetAttr(cls, name, &v) ? Mode_Truth(&v) : -1;
	Native_Free(&v);
	return t;
}

/* CharMissionDialog(UsrName, CharName, optId, FullOption): the talk of a mission character from its spawn point in
 * CharsNPC.CharRes (initMsg, initAns, speechOptionsList, afterInitSpeechFunc once); the character and its lists are
 * the objects of CharsNPC, held by reference (OBJLIB 5 and 7: the character is a class instance; its attributes go to
 * Speech as the objects they are) */
static byte Mf_CharMissionDialog(const nativeVal_t *usr, const nativeVal_t *name, const nativeVal_t *opt) {
	if (!Mode_Import("Speech") || !Mode_Import("CharsNPC") || !Mode_SetModVar("Speech", "PlayerName", usr) ||
		!Mode_SetModVar("Speech", "NPCName", name)) {
		return 0;
	}
	nativeVal_t cls, msg = { 0 }, ans = { 0 }, l = { 0 }, fn = { 0 }, none;
	if (!Mode_RefTake(Mode_CharRes(name), &cls)) {
		return 0;
	}
	byte ok = 1;
	Native_SetNone(&none);
	if (Mf_Is(opt, "Talk")) {
		ok = Mode_RefGetAttr(&cls, "initMsg", &msg) && Mode_RefGetAttr(&cls, "initAns", &ans) &&
			PORT_CALL(Speech, OnSpch, NULL, "OOs", Native_PyOf(&msg), Native_PyOf(&ans), "TalkNPC");
		int has = ok ? Mf_AttrTrue(&cls, "speechOptionsList") : -1;
		if (has > 0) {
			ok = Mode_RefGetAttr(&cls, "speechOptionsList", &l) && PORT_CALL(Speech, StdAnsLst, NULL, "O", Native_PyOf(&l));
		}
		has = ok && has >= 0 ? Mf_AttrTrue(&cls, "afterInitSpeechFunc") : -1;
		if (has > 0) {
			ok = Mode_RefGetAttr(&cls, "afterInitSpeechFunc", &fn) && Mode_RefToMod("Speech", "OnEndOfSpeech", &fn) &&
				Mode_RefSetAttr(&cls, "afterInitSpeechFunc", &none);
		}
		ok = ok && has >= 0;
	} else {
		int has = Mf_AttrTrue(&cls, "speechOptionsList");
		ok = has >= 0 && (!has || PORT_CALL(Speech, StdOnSpch, NULL, "v", opt));
	}
	Native_Free(&msg);
	Native_Free(&ans);
	Native_Free(&l);
	Native_Free(&fn);
	Native_Free(&cls);
	return ok;
}

/* CancelSecondaryMission(id, control): back to the game, a race or combat challenge is cancelled */
static byte Mf_CancelSecondaryMission(const nativeVal_t *id) {
	nativeVal_t type = { 0 };
	if (!Mode_Import("Challenge") || !Mode_Import("Menu") || !Mode_Import("SaveGame") ||
		!PORT_CALL(Menu, BackToGame, NULL, "v", id)) {
		return 0;
	}
	Mode_SaveVar("Challenge.Type", &type);
	byte ok = (!Native_IsStr(&type, "Race") && !Native_IsStr(&type, "Combat")) ||
		PORT_CALL(Challenge, CancelChallenge, NULL, "v", &type);
	Native_Free(&type);
	return ok && PORT_CALL(SaveGame, SMDesc, NULL, "si", "", 1L);
}

/* the revenge of a ship challenge after the death: Challenge.SetRevengeParams and its arguments for the restart */
static byte Mf_Revenge(nativeVal_t *restart, nativeVal_t *params) {
	nativeVal_t ctype = { 0 }, foe = { 0 }, empty;
	Mode_SaveVar("Challenge.Type", &ctype);
	Mode_SaveVar("Challenge.Foe", &foe);
	Native_SetStr(&empty, "");
	byte revenge = Mode_Cmp(&ctype, "!=", &empty) && Mode_Cmp(&foe, "!=", &empty);
	byte ok = 1;
	if (revenge) {
		/* the function the scheduler gets is the one of the module (its entry) */
		nativeVal_t fn;
		ok = Mode_RefMod("Challenge", "SetRevengeParams", &fn);
		if (ok) {
			Native_Free(restart);
			*restart = fn;
			Native_Free(params);
			Native_SetEmpty(params, NV_TUPLE);
			*Native_Append(params) = ctype;
			*Native_Append(params) = foe;
			Mode_SaveVar("Challenge.NumEnemies", Native_Append(params));
			return 1;
		}
	}
	Native_Free(&ctype);
	Native_Free(&foe);
	return ok;
}

/* SinglePlayerDeath(): the player died: a life less (unless infinite), the alarm off, the mercenaries no friends, a
 * character the player still has; with lives left the way point (a free phone cabin in the junkyard, the revenge of
 * a ship challenge), else the jail; the restart 5 s later */
static byte Mf_SinglePlayerDeath(void) {
	static const char *const mercs[] = { "MercFriends.MercenaryA_Smartie", "MercFriends.MercenaryC_Brutus",
		"MercFriends.MercenaryB_Dumber" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *params = Mode_V(&t), *lifes = Mode_V(&t), *lifes1 = Mode_V(&t), *type = Mode_V(&t);
	nativeVal_t *avail = Mode_V(&t), *text = Mode_V(&t), *demo = Mode_V(&t), *utype = Mode_V(&t), *pcb = Mode_V(&t);
	nativeVal_t *pname = Mode_V(&t);
	if (!Mode_Import("SaveGame") || !Mode_Import("Challenge") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	/* the function the scheduler gets is the one of the module (its entry) */
	nativeVal_t *restart = Mode_V(&t), *at = Mode_V(&t);
	if (!Mode_RefMod("SaveGame", "StartGame", restart)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(params, NV_TUPLE);
	byte ok = Mode_SaveInt("Player.NumLives", lifes);
	if (ok && Mf_SaveIs("Player.InfiniteLives", "0")) {
		ok = Mode_Arith('-', lifes, Mode_I(&t, 1), lifes1);
		if (ok) {
			Native_Free(lifes);
			*lifes = *lifes1;
			Native_SetNone(lifes1);
			LvN_SaveRepr("Player.NumLives", lifes);
		}
	}
	if (ok) {
		SaveVar_Set("AlarmActive", "0");
		SaveVar_Set("AlarmStatus", "0.0");
		ok = Mode_SetAlarm(Mode_I(&t, 0));
	}
	if (ok) {
		for (int i = 0; i < 3; i++) {
			SaveVar_Set(mercs[i], "0");
		}
		Mode_SaveVar("Char", type);
		if (Native_IsStr(type, "Gear")) {
			Native_Free(type);
			Native_SetStr(type, "Police");
		}
		Mode_SaveVar("Conversor.AvailableChars", text);
		ok = Mode_EvalSave(MF, text, avail);
	}
	int in = ok ? Mode_In(avail, type) : -1;
	if (in == 0) {
		int dtritus = Mode_In(avail, Mode_S(&t, "Dtritus"));
		Native_Free(type);
		if (dtritus > 0) {
			Native_SetStr(type, "Dtritus");
		} else if (dtritus == 0) {
			Native_SetStr(type, "Berto");
			Native_Free(text);
			Mode_Lang("Generics_NoMatrix", text);
			Mode_SetSaveVar("InitialMessage", text);
		} else {
			in = -1;
		}
	}
	ok = in >= 0 && Mode_SetSaveVar("Char", type) && Mode_Import("Init") && Mode_ModGet("Init", "isDemo", demo);
	int alive = ok ? !Native_True(demo) : -1;
	if (alive == 0) {
		alive = Mode_CmpI(lifes, ">=", 0);
	}
	if (alive > 0) {
		int left = Mode_CmpI(lifes, ">=", 0);
		if (left > 0) {
			ok = PORT_CALL(SaveGame, QWayPoint, NULL, "");
			int car = ok ? Native_True(usr) : -1;
			if (car > 0) {
				car = Mode_VObj(usr, "Type", utype) ? Mode_Cmp(utype, "==", Mode_S(&t, "Car")) : -1;
			}
			ok = car >= 0 && (!car || Mf_Revenge(restart, params));
			if (ok && Mf_SaveIs("Spawn", "DM_Player_Spawn_Junkyard")) {
				ok = Mode_Import("PhoneCab") && Mode_Import("Init") && Mode_Import("SaveGame") &&
					PORT_CALL(PhoneCab, GetFree, pcb, "") && Mode_VObj(pcb, "Name", pname) &&
					Mode_SetSaveVar("Spawn", pname) && Mode_VRead(pcb, "Name", pname) && Mode_SetSaveVar("ComeFrom", pname);
			}
		} else if (left == 0) {
			ok = PORT_CALL(SaveGame, MarkWayPoint, NULL, "sss", "Levels/Police", "DM_Player_Spawn_Jail", "");
		} else {
			ok = 0;
		}
	} else if (alive < 0) {
		ok = 0;
	}
	if (ok) {
		Native_SetFloat(at, Mode_Time() + 5.0);
		ok = Mode_ScheduleV(at, restart, params->items, params->n, NULL);
	}
	return Mode_Done(&t, ok);
}

/* ---- the photo objects ---- */

/* CreatePhotoObject(modelName, pos, ang, viewFunc=""): a blueprint on the floor to photograph, a mission target */
static byte Mf_CreatePhotoObject(const nativeVal_t *model, const nativeVal_t *pos, const nativeVal_t *ang, const nativeVal_t *view,
	nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *file = Mode_V(&t), *v = Mode_V(&t);
	if (!LvN_CreateAt(model, pos, "WithLife", e) || !Mode_VSet(e, "Ang", ang) || !Mode_VSetI(e, "ArrowCanView", 1) ||
		!Mode_VSetI(e, "CanPhoto", 1) ||
		!Mode_Cat(&t, file, 3, Mode_S(&t, "Models/Misc/Blueprint/"), model, Mode_S(&t, ".M3D")) || !Mode_VModel(e, file) ||
		!Mode_VSet(e, "ViewFunc", view) || !Mode_VSetI(e, "VRadius", 30) || !Mode_Import("Items") ||
		!PORT_CALL(Items, SetInteractiveObjOnFloor, NULL, "v", e) ||
		!PORT_OWN(MissionsFuncs, ActivateTarget, NULL, "vi", e, 1L)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_VObj(e, "Pos", v) && Mode_VSet(e, "MovePos", v) && Mode_VRead(e, "Ang", v) &&
		Mode_VSet(e, "MoveAng", v) && Mode_VSetF(e, "MoveTime", 0.1));
}

/* UnsetPhotoObject(obj): no target, no arrows */
static byte Mf_UnsetPhotoObject(const nativeVal_t *obj) {
	return Mode_VSetI(obj, "RadarShow", 0) && Mode_VSetI(obj, "ArrowCanView", 0) && Mode_VSetI(obj, "ArrowCanMission", 0);
}

/* ---- the waves of Bertos ---- */

/* ReassignBertosPlayerTarget(): the Bertos chase the player's new ship or character */
static byte Mf_ReassignBertosPlayerTarget(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *bertos = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int n = Mode_G(MF, "currentBertos", bertos) ? Mode_Loop(bertos) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *b = Mode_V(&t), *name = Mode_V(&t);
		/* SAI.SetStateVehicle(2, berto, usr.Name) */
		ok = Native_SeqItem(bertos, i, b) && Mode_VObj(usr, "Name", name) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, b, name);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

/* CreateBerto(usrName): a Berto ship at the nearest free spawn point, with the profile and life of the save game */
static byte Mf_CreateBerto(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p = Mode_V(&t), *pd = Mode_V(&t), *prof = Mode_V(&t), *name = Mode_V(&t), *ship = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *life = Mode_V(&t), *ang = Mode_V(&t);
	if (!Mode_Import("VehiclesNPC") || !Mode_Import("OutMap") || !Mode_SpawnCar("Berto", 1, p, NULL) ||
		!PORT_CALL(VehiclesNPC, GetIAProf, pd, "s", "SBERTO1")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("BertosProfile", prof);
	if (!PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "vv", pd, prof) || !Mode_VObj(p, "Name", name) ||
		!Mode_VObj(pd, "Ship", ship) || !PORT_CALL(VehiclesNPC, SetStdProperties, NULL, "vv", name, ship) ||
		!PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", p, pd)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(ang, 0, 0, 0);
	if (!Mode_VSet(p, "Ang", ang) || !Mode_VSetS(p, "OnDeath", "MissionsFuncs.OnBertoDeath") || !Mode_VSetS(p, "TgType", "red") ||
		!Mode_VSetS(p, "EntityList", "Enemies")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("BertosLife", text);
	return Mode_Done(&t, Mode_EvalSave(MF, text, life) && Mode_VSet(p, "Life", life) && Mode_VSet(p, "MaxLife", life) &&
		Mode_VRead(p, "Name", name) && Mode_Append(MF, "currentBertos", name));
}

/* CreateBertos(): a new wave: the hook OnNewWaveCallback, then a Berto a second up to maxBertos */
static byte Mf_CreateBertos(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *usrName = Mode_V(&t), *max = Mode_V(&t), *bertos = Mode_V(&t), *count = Mode_V(&t);
	nativeVal_t *num = Mode_V(&t);
	if (!Mode_Import("Parking") || !LvN_PutI(MF, "launchNewWave", 1)) {
		return Mode_Done(&t, 0);
	}
	/* the hook is a value a mission puts into the module (OBJLIB 5), called through its entry */
	int hook = LvN_GTrue(MF, "OnNewWaveCallback");
	if (hook < 0 || (hook && !MODE_CBGLOBAL(MF, "OnNewWaveCallback", NULL, "")) || !Mode_ModNone("Parking", "SwitchVehicleMissionCallback") ||
		!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VObj(usr, "Name", usrName)) {
		return Mode_Done(&t, 0);
	}
	double now = Mode_Time();
	long n;
	int len = Mode_G(MF, "maxBertos", max) && Mode_G(MF, "currentBertos", bertos) ? Mode_Len(bertos) : -1;
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(num, len);
	/* range(maxBertos - len(currentBertos)) of the builtin (its argument rules) */
	if (!Mode_Arith('-', max, num, count) || !Mode_Range(count, &n)) {
		return Mode_Done(&t, 0);
	}
	for (long k = 0; k < n; k++) {
		nativeVal_t time;
		Native_SetFloat(&time, now + 1.0 * (double)(k + 1));
		/* the global CreateBerto by name: the scheduler takes its entry */
		if (!Mode_AtG(&time, MF, "CreateBerto", usrName, 1)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* OnBertoDeath(LooserName, WinnerName): the death of a ship; a dead Berto starts the next wave timeBetweenWaves
 * later (once a wave), then the hook OnBertoDeathCallback */
static byte Mf_OnBertoDeath(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *now = Mode_V(&t), *wait = Mode_V(&t), *time = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !PORT_CALL(Vehicles, OnDeath, NULL, "vv", looser, winner)) {
		return Mode_Done(&t, 0);
	}
	int in = Mf_InG(looser, "currentBertos");
	if (in < 0) {
		return Mode_Done(&t, 0);
	}
	if (in) {
		int wave = Mode_Remove(MF, "currentBertos", looser) ? LvN_GTrue(MF, "launchNewWave") : -1;
		if (wave < 0) {
			return Mode_Done(&t, 0);
		}
		if (wave) {
			Native_SetFloat(now, Mode_Time());
			/* the global CreateBertos by name: the scheduler takes its entry */
			if (!LvN_PutI(MF, "launchNewWave", 0) || !Mode_G(MF, "timeBetweenWaves", wait) || !Mode_Arith('+', now, wait, time) ||
				!Mode_AtG(time, MF, "CreateBertos", NULL, 0)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	/* the hook is a value a mission puts into the module (OBJLIB 5), called through its entry */
	int hook = LvN_GTrue(MF, "OnBertoDeathCallback");
	if (hook <= 0) {
		return Mode_Done(&t, hook == 0);
	}
	return Mode_Done(&t, Mode_EntitySeam(looser, e) && Mode_VObj(e, "Pos", pos) &&
		MODE_CBGLOBAL(MF, "OnBertoDeathCallback", NULL, "vvv", looser, winner, pos));
}

/* ---- the crazy deals and the super deals ---- */

/* CrazyDealStatus(number): (variable, value, target) of a crazy deal */
static byte Mf_CrazyDealStatus(const nativeVal_t *number, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *var = Mode_V(&t), *name = Mode_V(&t), *val = Mode_V(&t), *tvar = Mode_V(&t), *ttext = Mode_V(&t);
	nativeVal_t *tgt = Mode_V(&t), *ival = Mode_V(&t);
	if (!Mf_DealVar(&t, number, ".Var", var) || !Mode_SaveVarV(var, name) || !Mode_SaveVarV(name, val) ||
		!Mf_DealVar(&t, number, ".Tgt", tvar) || !Mode_SaveVarV(tvar, ttext) || !Native_ToInt(ttext, tgt)) {
		return Mode_Done(&t, 0);
	}
	if (val->type == NV_NONE) {
		Native_SetInt(ival, 0);
	} else if (!Native_ToInt(val, ival)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), name);
	Native_Copy(Native_Append(out), ival);
	Native_Copy(Native_Append(out), tgt);
	return Mode_Done(&t, 1);
}

/* the number of crazy deals done (CrazyDealStatus through the globals of the module): -1 with the error */
static int Mf_DealsDone(void) {
	int done = 0;
	for (long i = 1; i <= 3; i++) {
		nativeVal_t cd = { 0 }, a = { 0 }, b = { 0 };
		int c = PORT_OWN(MissionsFuncs, CrazyDealStatus, &cd, "i", i) && Mode_Item(&cd, 1, &a) && Mode_Item(&cd, 2, &b) ?
			Mode_Cmp(&a, ">=", &b) : -1;
		Native_Free(&cd);
		Native_Free(&a);
		Native_Free(&b);
		if (c < 0) {
			return -1;
		}
		done += c;
	}
	return done;
}

/* GetNumCrazyDealCompleted(): the number of crazy deals done */
static byte Mf_GetNumCrazyDealCompleted(nativeVal_t *out) {
	int done = Mf_DealsDone();
	if (done < 0) {
		return 0;
	}
	Native_SetInt(out, done);
	return 1;
}

/* OnCrazyDealFinishedMessage(): the crazy gambler calls (the engine's OnCrazyDealFinished), not for the first super
 * deal, which has its own scene */
static byte Mf_OnCrazyDealFinishedMessage(void) {
	if (!Mode_Import("Speech") || !Mode_Import("Scorer")) {
		return 0;
	}
	return Mf_SaveIs("SuperDeal.Library", "SuperDeal_First") ||
		PORT_CALL(Speech, AddRemoteCom, NULL, "ss", "SuperDeal_PayAtenttion_RemoteMessage1", "CrazyGambler");
}

/* OnCrazyDealTarget(): the mission scorer (the engine's OnCrazyDealTarget); in the demo only before all three */
static byte Mf_OnCrazyDealTarget(void) {
	nativeVal_t demo = { 0 }, n = { 0 };
	if (!Mode_Import("Scorer") || !Mode_Import("Init") || !Mode_ModGet("Init", "isDemo", &demo)) {
		Native_Free(&demo);
		return 0;
	}
	int show = !Native_True(&demo);
	Native_Free(&demo);
	if (!show) {
		show = PORT_OWN(MissionsFuncs, GetNumCrazyDealCompleted, &n, "") ? Mode_CmpI(&n, "<", 3) : -1;
		Native_Free(&n);
	}
	return show >= 0 && (!show || PORT_CALL(Scorer, ShowMissionScorer, NULL, "ii", 0L, 1L));
}

/* NewCrazyDeal(number, varname, maxval): a new crazy deal on the save variable varname */
static byte Mf_NewCrazyDeal(const nativeVal_t *number, const nativeVal_t *name, const nativeVal_t *max) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *var = Mode_V(&t), *tvar = Mode_V(&t), *rep = Mode_V(&t);
	if (!Mf_DealVar(&t, number, ".Var", var) || !Mode_SetSaveV(var, name) || !Mode_SetSaveV(name, Mode_S(&t, "0")) ||
		!Mf_DealVar(&t, number, ".Tgt", tvar)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(max, rep);
	return Mode_Done(&t, Mode_SetSaveV(tvar, rep));
}

/* SetNextSuperDeal(MapName, MissionName, superDealType): the next super deal */
static byte Mf_SetNextSuperDeal(const nativeVal_t *map, const nativeVal_t *mission, const nativeVal_t *type) {
	return Mode_SetSaveVar("SuperDeal.Map", map) && Mode_SetSaveVar("SuperDeal.Library", mission) &&
		Mode_SetSaveVar("SuperDeal.Type", type);
}

/* StartActualSuperDeal(): the mission is kept aside and the super deal starts (once) */
static byte Mf_StartActualSuperDeal(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *map = Mode_V(&t), *lib = Mode_V(&t), *type = Mode_V(&t);
	Mode_SaveVar("SuperDeal.Mission.Map", v);
	if (Native_True(v)) {
		return Mode_Done(&t, Mode_ConsoleError(Mode_S(&t, "Se ha llamado dos veces a MissionsFuncs.StartActualSuperDeal\n")));
	}
	if (!Mode_SaveCopy("SuperDeal.Mission.Map", "Mission.Map") || !Mode_SaveCopy("SuperDeal.Mission.Library", "Mission.Library")) {
		return Mode_Done(&t, 0);
	}
	/* the global StartSuperDeal is looked up after its arguments (the script before them: the same, the save variables
	 * are read in C) */
	Mode_SaveVar("SuperDeal.Map", map);
	Mode_SaveVar("SuperDeal.Library", lib);
	Mode_SaveVar("SuperDeal.Type", type);
	return Mode_Done(&t, PORT_OWN(MissionsFuncs, StartSuperDeal, NULL, "vvv", map, lib, type));
}

/* EndOfSuperDeal(DealerMessage): the mission back, to the crazy gambler's meeting room with his message */
static byte Mf_EndOfSuperDeal(const nativeVal_t *msg) {
	nativeVal_t text = { 0 };
	if (!Mode_Import("SaveGame") || !Mode_SaveCopy("Mission.Map", "SuperDeal.Mission.Map") ||
		!Mode_SaveCopy("Mission.Library", "SuperDeal.Mission.Library") || !Mode_DelSaveVars("SuperDeal.Mission.Map") ||
		!Mode_DelSaveVars("SuperDeal.Mission.Library") || !Mode_LangV(msg, &text)) {
		return 0;
	}
	byte ok = Mode_SetSaveVar("InitialMessage", &text);
	Native_Free(&text);
	if (!ok) {
		return 0;
	}
	SaveVar_Set("AutoSaveGameOnLoad", "1");
	SaveVar_Set("Spawn", "DM_Player_Spawn_Meeting_Room");
	SaveVar_Set("ComeFrom", "");
	return PORT_CALL(SaveGame, QWayPoint, NULL, "") && PORT_CALL(SaveGame, QLoad, NULL, "s", "Levels/GamblinDen");
}

/* StartSuperDeal(MapName, MissionName, superDealType): Dtritus at the beginning of the super deal's level */
static byte Mf_StartSuperDeal(const nativeVal_t *map, const nativeVal_t *mission, const nativeVal_t *type) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *level = Mode_V(&t);
	if (!Mode_Import("SaveGame")) {
		return Mode_Done(&t, 0);
	}
	SaveVar_Set("Spawn", "DM_Player_Spawn_Beginning");
	SaveVar_Set("Char", "Dtritus");
	SaveVar_Set("ComeFrom", "");
	return Mode_Done(&t, Mode_SetSaveVar("SuperDealType", type) && PORT_CALL(SaveGame, QWayPoint, NULL, "") &&
		PORT_OWN(MissionsFuncs, SetNextMission, NULL, "vv", mission, map) && Mode_Cat(&t, level, 2, Mode_S(&t, "Levels/"), map) &&
		PORT_CALL(SaveGame, QLoad, NULL, "v", level));
}

/* RelaxShipsInList(listName): the ships of the list leave it and relax */
static byte Mf_RelaxShipsInList(const nativeVal_t *list) {
	nativeVal_t e = { 0 }, active = { 0 }, name = { 0 };
	byte ok = Mf_First(list, &e);
	while (ok && Native_True(&e)) {
		int on = Mode_VSetS(&e, "EntityList", "") && Mode_VRead(&e, "Active", &active) ? Native_True(&active) : -1;
		/* SAI.SetStateVehicle(3, e.Name, 1) */
		ok = on >= 0 && (!on || (Mode_VRead(&e, "Name", &name) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivi", 3L, &name, 1L)));
		Native_Free(&e);
		ok = ok && Mf_First(list, &e);
	}
	Native_Free(&e);
	Native_Free(&active);
	Native_Free(&name);
	return ok;
}

/* SetInvulnerableShipsInList(listName, invulState=1): every ship of the list */
static byte Mf_SetInvulnerableShipsInList(const nativeVal_t *list, const nativeVal_t *state) {
	nativeVal_t e = { 0 }, next = { 0 };
	byte ok = Mf_First(list, &e);
	while (ok && Native_True(&e)) {
		ok = Mode_VSet(&e, "Invulnerable", state) && Mode_VRead(&e, "NextInList", &next);
		Native_Free(&e);
		ok = ok && Mode_EntitySeam(&next, &e);
	}
	Native_Free(&e);
	Native_Free(&next);
	return ok;
}

/* ---- the score tables of the super deals ---- */

/* (n, name, textColor, colour) of a score table, with a colour global of the module, appended to the list rows */
static byte Mf_Row(nativeVal_t *rows, const nativeVal_t *n, const nativeVal_t *name, const char *colour) {
	nativeVal_t row;
	Native_SetEmpty(&row, NV_TUPLE);
	Native_Copy(Native_Append(&row), n);
	Native_Copy(Native_Append(&row), name);
	if (!Mode_G(MF, "textColor", Native_Append(&row)) || !Mode_G(MF, colour, Native_Append(&row))) {
		Native_Free(&row);
		return 0;
	}
	*Native_Append(rows) = row;
	return 1;
}

/* RacerScorer.var + add into out */
static byte Mf_Score(const char *var, long add, nativeVal_t *out) {
	nativeVal_t v = { 0 }, a;
	Native_SetInt(&a, add);
	byte ok = Mode_ModGet("RacerScorer", var, &v) && (add ? Mode_Arith('+', &v, &a, out) : (Native_Copy(out, &v), 1));
	Native_Free(&v);
	return ok;
}

/* the list of the two rows (a, aname, acol) and (b, bname, bcol) of a team table */
static byte Mf_Pair(nativeVal_t *data, const nativeVal_t *a, const nativeVal_t *aname, const char *acol, const nativeVal_t *b,
	const nativeVal_t *bname, const char *bcol) {
	Native_SetEmpty(data, NV_LIST);
	return Mf_Row(data, a, aname, acol) && Mf_Row(data, b, bname, bcol);
}

/* 1.5.2's list.sort of a short list (binarysort of listobject.c: the insertion after equal items), then reverse */
static void Mf_SortDown(nativeVal_t *l) {
	for (int i = 1; i < l->n; i++) {
		nativeVal_t pivot = l->items[i];
		int lo = 0, hi = i;
		while (lo < hi) {
			int p = lo + ((hi - lo) >> 1);
			if (Native_Compare(&pivot, &l->items[p]) < 0) {
				hi = p;
			} else {
				lo = p + 1;
			}
		}
		memmove(&l->items[lo + 1], &l->items[lo], (size_t)(i - lo) * sizeof(nativeVal_t));
		l->items[lo] = pivot;
	}
	for (int i = 0, j = l->n - 1; i < j; i++, j--) {
		nativeVal_t x = l->items[i];
		l->items[i] = l->items[j];
		l->items[j] = x;
	}
}

/* BuildMultipleEnemiesData(playerWins, enemiesDict): the score table of all against all, sorted by the frags
 * (the player's one more when he won), Dtritus and the enemies' names */
static byte Mf_BuildMultipleEnemiesData(const nativeVal_t *wins, const nativeVal_t *enemies, nativeVal_t *data) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *frags = Mode_V(&t), *keys = Mode_V(&t), *player = Mode_V(&t);
	if (!Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(frags, NV_LIST);
	if (enemies->type == NV_DICT) {
		int pos = 0;
		const nativeVal_t *k, *v;
		while (Native_DictNext(enemies, &pos, &k, &v)) {
			Native_Copy(Native_Append(frags), v);
		}
	} else {
		/* enemies.keys() and enemies[k] of any other value (its errors) */
		if (!Mode_RefCall(enemies, "keys", NULL, 0, keys)) {
			return Mode_Done(&t, 0);
		}
		int n = Mode_Loop(keys);
		for (int i = 0; i < n; i++) {
			nativeVal_t k = { 0 };
			byte ok = Native_SeqItem(keys, i, &k) && Mode_Sub(enemies, &k, Native_Append(frags));
			Native_Free(&k);
			if (!ok) {
				return Mode_Done(&t, 0);
			}
		}
		if (n < 0) {
			return Mode_Done(&t, 0);
		}
	}
	byte lost = Mode_EqI(wins, 0) || Mode_EqI(wins, 2);
	if (!Mf_Score("missionYou", lost ? 0 : 1, player)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(Native_Append(frags), player);
	Mf_SortDown(frags);
	int index = 0;
	for (int i = 0; i < frags->n; i++) {
		if (!Native_Compare(&frags->items[i], player)) {
			index = i;
			break;
		}
	}
	Native_SetEmpty(data, NV_LIST);
	long enemy = 0;
	for (int i = 0; i < frags->n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *names = Mode_V(&t);
		byte ok;
		if (i == index) {
			Mode_Lang("CharName_Dtritus", name);
			ok = Mf_Row(data, &frags->items[i], name, "bgColorGreen");
		} else {
			ok = Mode_G(MF, "enemiesNames", names) && Mode_Item(names, enemy, name) &&
				Mf_Row(data, &frags->items[i], name, "bgColorRed");
			enemy++;
		}
		Mode_Release(&t, mark);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* BuildTeamDeathMatchData(playerWins): the two teams */
static byte Mf_BuildTeamDeathMatchData(const nativeVal_t *wins, nativeVal_t *data) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *enemy = Mode_V(&t), *dtritus = Mode_V(&t);
	if (!Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	byte won = Native_True(wins);
	if (!Mf_Score(won ? "missionYou" : "missionBad", 1, a)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_DtritusTeam" : "SuperDeal_EnemyTeam", won ? dtritus : enemy);
	if (!Mf_Score(won ? "missionBad" : "missionYou", 0, b)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_EnemyTeam" : "SuperDeal_DtritusTeam", won ? enemy : dtritus);
	return Mode_Done(&t, won ? Mf_Pair(data, a, dtritus, "bgColorGreen", b, enemy, "bgColorRed") :
		Mf_Pair(data, a, enemy, "bgColorRed", b, dtritus, "bgColorGreen"));
}

/* BuildVersusData(playerWins): Dtritus and the one foe of the mission */
static byte Mf_BuildVersusData(const nativeVal_t *wins, nativeVal_t *data) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *foe = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *dtritus = Mode_V(&t);
	if (!Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	if (Mf_SaveIs("Mission.Library", "SuperDeal_First")) {
		Native_SetStr(foe, "Scrapus");
	} else if (Mf_SaveIs("Mission.Library", "Mission_IgorCombat")) {
		Mode_Lang("CharName_Igor", foe);
	} else {
		Native_SetStr(foe, "Multiplus");
	}
	byte won = Native_True(wins);
	if (!Mf_Score(won ? "missionYou" : "missionBad", 1, a)) {
		return Mode_Done(&t, 0);
	}
	if (won) {
		Mode_Lang("CharName_Dtritus", dtritus);
	}
	if (!Mf_Score(won ? "missionBad" : "missionYou", 0, b)) {
		return Mode_Done(&t, 0);
	}
	if (!won) {
		Mode_Lang("CharName_Dtritus", dtritus);
	}
	return Mode_Done(&t, won ? Mf_Pair(data, a, dtritus, "bgColorGreen", b, foe, "bgColorRed") :
		Mf_Pair(data, a, foe, "bgColorRed", b, dtritus, "bgColorGreen"));
}

/* BuildDominationData(playerWins): the rounds of the two teams */
static byte Mf_BuildDominationData(const nativeVal_t *wins, nativeVal_t *data) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *enemy = Mode_V(&t), *dtritus = Mode_V(&t);
	byte won = Native_True(wins);
	if (!Mode_SaveInt("DominationRounds", a)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_DtritusTeam" : "SuperDeal_EnemyTeam", won ? dtritus : enemy);
	if (!Mode_SaveInt(won ? "EnemyRounds" : "PlayerRounds", b)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_EnemyTeam" : "SuperDeal_DtritusTeam", won ? enemy : dtritus);
	return Mode_Done(&t, won ? Mf_Pair(data, a, dtritus, "bgColorGreen", b, enemy, "bgColorRed") :
		Mf_Pair(data, a, enemy, "bgColorRed", b, dtritus, "bgColorGreen"));
}

/* BuildCTFData(playerWins): the flags of the two teams */
static byte Mf_BuildCTFData(const nativeVal_t *wins, nativeVal_t *data) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *enemy = Mode_V(&t), *dtritus = Mode_V(&t);
	if (!Mode_Import("CTF")) {
		return Mode_Done(&t, 0);
	}
	byte won = Native_True(wins);
	if (!Mode_ModGet("CTF", "flags", a)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_DtritusTeam" : "SuperDeal_EnemyTeam", won ? dtritus : enemy);
	if (!Mode_ModGet("CTF", won ? "flagsEnemies" : "flagsFriends", b)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(won ? "SuperDeal_EnemyTeam" : "SuperDeal_DtritusTeam", won ? enemy : dtritus);
	return Mode_Done(&t, won ? Mf_Pair(data, a, dtritus, "bgColorGreen", b, enemy, "bgColorRed") :
		Mf_Pair(data, a, enemy, "bgColorRed", b, dtritus, "bgColorGreen"));
}

/* OnSuperDealFinish(victory, superDealType="", retryFunc=""): the camera of the level, the messages and the score
 * table of the super deal (0 lost with a retry, 1 won, 2 lost) */
static byte Mf_OnSuperDealFinish(const nativeVal_t *victory, const nativeVal_t *type, const nativeVal_t *retry) {
	static const char *const tops[] = { "SuperDeal_YouLoose", "SuperDeal_YouWin", "SuperDeal_YouLoose" };
	static const char *const builders[][2] = { { "TeamDeathMatch", "BuildTeamDeathMatchData" }, { "Versus", "BuildVersusData" },
		{ "Domination", "BuildDominationData" }, { "CTF", "BuildCTFData" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *c = Mode_V(&t), *v = Mode_V(&t), *name = Mode_V(&t), *msg = Mode_V(&t);
	nativeVal_t *table = Mode_V(&t), *data = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Mode_G(MF, "cameraData", cam)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(cam)) {
		nativeVal_t *zero = (nativeVal_t *)Mode_I(&t, 0);
		if (!LvN_Create(Mode_S(&t, "Camara"), zero, zero, zero, "Cam", c) || !Mode_G(MF, "cameraData", cam) ||
			!Mode_Item(cam, 0, v) || !Mode_VSet(c, "camPos", v) || !Mode_G(MF, "cameraData", cam) || !Mode_Item(cam, 1, v) ||
			!Mode_VSet(c, "camAng", v) || !Mode_VObj(c, "Name", name) ||
			!PORT_CALL(Scrap, SetCam, NULL, "iv", 0L, name) || !LvN_PutNone(MF, "cameraData")) {
			return Mode_Done(&t, 0);
		}
	}
	int k = -1;
	for (int i = 0; i < 3 && k < 0; i++) {
		if (Mode_EqI(victory, i)) {
			k = i;
		}
	}
	if (k >= 0) {
		Mode_Lang(tops[k], msg);
		if (!Mode_SetModVar("RacerScorer", "ScorerTopMessage", msg)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(msg);
		if (k == 0) {
			Mode_Lang("SuperDeal_TryAgain", msg);
		} else {
			Native_SetStr(msg, "");
		}
		if (!Mode_SetModVar("RacerScorer", "ScorerBotMessage", msg)) {
			return Mode_Done(&t, 0);
		}
	}
	byte got = 0, ok = 1;
	if (Mf_Is(type, "DeathMatch") || Mf_Is(type, "MortalRace")) {
		/* the global builder is looked up after the table of the mode (the script before it: the same unless both are
		 * gone) */
		byte dm = Mf_Is(type, "DeathMatch");
		ok = Mode_Import(dm ? "DeathMatch" : "MortalRace") &&
			Mode_ModGet(dm ? "DeathMatch" : "MortalRace", dm ? "enemiesFrags" : "BayScorer", table) &&
			PORT_OWN(MissionsFuncs, BuildMultipleEnemiesData, data, "vv", victory, table);
		got = 1;
	}
	for (int i = 0; !got && i < 4; i++) {
		if (Mf_Is(type, builders[i][0])) {
			/* the builder by the name of the table, through its entry */
			ok = MODE_CBGLOBAL(MF, builders[i][1], data, "v", victory);
			got = 1;
		}
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	if (k >= 0 && !got) {
		/* the unbound local of the script for another type */
		return Mode_Done(&t, Native_Raise(NE_NAME, "data"));
	}
	switch (k) {
	case 0:
		return Mode_Done(&t, PORT_CALL(RacerScorer, ShowScorer, NULL, "ivivs", 0L, data, 1L, retry, "MissionsFuncs.SuperDealOnDefeat"));
	case 1:
		return Mode_Done(&t, PORT_CALL(RacerScorer, ShowScorer, NULL, "ivis", 0L, data, 2L, "MissionsFuncs.SuperDealOnVictory"));
	case 2:
		return Mode_Done(&t, PORT_CALL(RacerScorer, ShowScorer, NULL, "iviv", 0L, data, 2L, retry));
	}
	return Mode_Done(&t, 1);
}

/*
 * The code the super deals and the crazy gambler missions leave in the save variables OnAbort and OnVictory
 * (superdeal_*.py, mission_igorcombat, mission_bankerscombat, mission_bankersctf, mission_mortalrace and OM.c write
 * them; a save of the original game holds the same texts): 22 texts of "import Mod" and "Name.Func(constants)"
 * statements, e.g. "import SuperDeal_First;SuperDeal_First.Victory()" or "import SaveGame; SaveGame.QWayPoint();
 * import MissionsFuncs; MissionsFuncs.SetNextMission("Mission_BackFromMortalRace", "GamblinDen");Scrap.SetSaveVar(
 * "Map", "Levels/GamblinDen");import SaveGame; SaveGame.QLoad("Levels/GamblinDen")". They run as exec runs them in
 * SuperDealOnDefeat and SuperDealOnVictory, through the reader of the lines the game execs (Exec_Text): the imports
 * stored in the variables of the function, a name looked up there, in the globals of MissionsFuncs and the builtins
 * (mission_bankerscombat never imports SaveGame: NameError, as in the original), a port or engine function through
 * its entry while the attribute holds it.
 *
 * exec(Scrap.GetSaveVar(var)) when it holds code; on victory the super deal type is cleared first. A text in another
 * form (a mod, a save edited by hand) gets the SyntaxError of the reader. id and control are the values the scorer
 * passed (variables of the function, with cad).
 */
static byte Mf_SuperDealEnd(const nativeVal_t *id, const nativeVal_t *control, const char *var, byte victory) {
	nativeVal_t cad = { 0 };
	Mode_SaveVar(var, &cad);
	if (!Native_True(&cad) || Mf_Is(&cad, "")) {
		Native_Free(&cad);
		return 1;
	}
	if (victory && !Mf_SetSaveSeam("SuperDealType", "")) {
		Native_Free(&cad);
		return 0;
	}
	execScope_t x = { .module = MF };
	Exec_Var(&x, "id", id);
	Exec_Var(&x, "control", control);
	Exec_Var(&x, "cad", &cad);
	byte ok = Exec_Text(&x, &cad);
	Native_Free(&cad);
	return ok;
}

/* SuperDealOnDefeat(id, control), SuperDealOnVictory(id, control): the code the super deal left in OnAbort or
 * OnVictory */
static byte MissionsFuncs_SuperDealOnDefeat_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return n == 2 ? Mf_SuperDealEnd(&a[0], &a[1], "OnAbort", 0) : Port_Arity(n, 2);
}

static byte MissionsFuncs_SuperDealOnVictory_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return n == 2 ? Mf_SuperDealEnd(&a[0], &a[1], "OnVictory", 1) : Port_Arity(n, 2);
}

/* the adapters hand id and control over by reference (OBJLIB 7: a list the text changes is the caller's) */
MODE_REFADAPTER(MissionsFuncs, SuperDealOnDefeat)
MODE_REFADAPTER(MissionsFuncs, SuperDealOnVictory)

/* ---- adapters ---- */

MODE_FN(MissionsFuncs, Init, 0, Mf_Init())
MODE_FN(MissionsFuncs, TryMsg, 1, Mf_TryMsg(&a[0]))
MODE_FN(MissionsFuncs, SetNextMission, 2, Mf_SetNextMission(&a[0], &a[1]))
MODE_FN(MissionsFuncs, SetFirstDemoMission, 0, Mf_SetFirstDemoMission())
MODE_FN(MissionsFuncs, SetOutMissionArrow, 0, Mf_SetOutMissionArrow())
MODE_FN(MissionsFuncs, GetSecondary, 1, Mf_GetSecondary(out))
MODE_FN(MissionsFuncs, SetSecondary, 1, Mf_SetSecondary(&a[0]))
MODE_FN(MissionsFuncs, FinishGame, 1, Mf_FinishGame(&a[0]))
MODE_FN(MissionsFuncs, ShowMissionInfo, 1, Mf_ShowMissionInfo())
MODE_FN(MissionsFuncs, SetMissionTarget, 1, Mf_SetMissionTarget(&a[0]))
MODE_FN(MissionsFuncs, CleanAllTargets, 0, Mf_CleanAllTargets())
MODE_FN(MissionsFuncs, DelMissionTarget, 1, Mf_DelMissionTarget(&a[0], out))
MODE_FN(MissionsFuncs, IsCharMissionObjective, 1, Mf_IsCharMissionObjective(&a[0], out))
MODE_FN(MissionsFuncs, CharMissionDialog, 4, Mf_CharMissionDialog(&a[0], &a[1], &a[2]))
MODE_FN(MissionsFuncs, CancelSecondaryMission, 2, Mf_CancelSecondaryMission(&a[0]))
MODE_FN(MissionsFuncs, SinglePlayerDeath, 0, Mf_SinglePlayerDeath())
MODE_FN(MissionsFuncs, UnsetPhotoObject, 1, Mf_UnsetPhotoObject(&a[0]))
MODE_FN(MissionsFuncs, ReassignBertosPlayerTarget, 0, Mf_ReassignBertosPlayerTarget())
MODE_FN(MissionsFuncs, CreateBerto, 1, Mf_CreateBerto())
MODE_FN(MissionsFuncs, CreateBertos, 0, Mf_CreateBertos())
MODE_FN(MissionsFuncs, OnBertoDeath, 2, Mf_OnBertoDeath(&a[0], &a[1]))
MODE_FN(MissionsFuncs, CrazyDealStatus, 1, Mf_CrazyDealStatus(&a[0], out))
MODE_FN(MissionsFuncs, GetNumCrazyDealCompleted, 0, Mf_GetNumCrazyDealCompleted(out))
MODE_FN(MissionsFuncs, OnCrazyDealFinishedMessage, 0, Mf_OnCrazyDealFinishedMessage())
MODE_FN(MissionsFuncs, OnCrazyDealTarget, 0, Mf_OnCrazyDealTarget())
MODE_FN(MissionsFuncs, NewCrazyDeal, 3, Mf_NewCrazyDeal(&a[0], &a[1], &a[2]))
MODE_FN(MissionsFuncs, SetNextSuperDeal, 3, Mf_SetNextSuperDeal(&a[0], &a[1], &a[2]))
MODE_FN(MissionsFuncs, StartActualSuperDeal, 0, Mf_StartActualSuperDeal())
MODE_FN(MissionsFuncs, EndOfSuperDeal, 1, Mf_EndOfSuperDeal(&a[0]))
MODE_FN(MissionsFuncs, StartSuperDeal, 3, Mf_StartSuperDeal(&a[0], &a[1], &a[2]))
MODE_FN(MissionsFuncs, RelaxShipsInList, 1, Mf_RelaxShipsInList(&a[0]))
MODE_FN(MissionsFuncs, BuildMultipleEnemiesData, 2, Mf_BuildMultipleEnemiesData(&a[0], &a[1], out))
MODE_FN(MissionsFuncs, BuildTeamDeathMatchData, 1, Mf_BuildTeamDeathMatchData(&a[0], out))
MODE_FN(MissionsFuncs, BuildVersusData, 1, Mf_BuildVersusData(&a[0], out))
MODE_FN(MissionsFuncs, BuildDominationData, 1, Mf_BuildDominationData(&a[0], out))
MODE_FN(MissionsFuncs, BuildCTFData, 1, Mf_BuildCTFData(&a[0], out))

/* the functions with optional arguments: "OO|O" and the like (the arity errors of PyArg_ParseTuple), an argument not
 * given NULL or the default of the script */
static byte MissionsFuncs_ActivateTarget_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *e, *status, *arrow = NULL;
	(void)out;
	return Mode_Parse(a, n, "OO|O", &e, &status, &arrow) && Mf_ActivateTarget(e, status, arrow);
}

static byte MissionsFuncs_CreatePhotoObject_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	const nativeVal_t *model, *pos, *ang, *view = &empty;
	return Mode_Parse(a, n, "OOO|O", &model, &pos, &ang, &view) && Mf_CreatePhotoObject(model, pos, ang, view, out);
}

static byte MissionsFuncs_SetInvulnerableShipsInList_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t one;
	Native_SetInt(&one, 1);
	const nativeVal_t *list, *state = &one;
	(void)out;
	return Mode_Parse(a, n, "O|O", &list, &state) && Mf_SetInvulnerableShipsInList(list, state);
}

static byte MissionsFuncs_OnSuperDealFinish_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	const nativeVal_t *victory, *type = &empty, *retry = &empty;
	(void)out;
	return Mode_Parse(a, n, "O|OO", &victory, &type, &retry) && Mf_OnSuperDealFinish(victory, type, retry);
}

MODE_ADAPTER(MissionsFuncs, ActivateTarget)
MODE_ADAPTER(MissionsFuncs, CreatePhotoObject)
MODE_ADAPTER(MissionsFuncs, SetInvulnerableShipsInList)
MODE_ADAPTER(MissionsFuncs, OnSuperDealFinish)

/* SetMultipleMissionTargets(tgtList): the list object itself into targetsList (OBJLIB 5: the adapter hands it over
 * by reference; a native caller's list becomes a new list object, as its call through Python makes it) */
static byte MissionsFuncs_SetMultipleMissionTargets_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return n == 1 ? Mf_SetMultipleMissionTargets(&a[0]) : Port_Arity(n, 1);
}

MODE_REFADAPTER(MissionsFuncs, SetMultipleMissionTargets)

PyMethodDef g_MissionsFuncsMethods[] = {
	{ "Init", MissionsFuncs_Init, 1, NULL },
	{ "TryMsg", MissionsFuncs_TryMsg, 1, NULL },
	{ "SetNextMission", MissionsFuncs_SetNextMission, 1, NULL },
	{ "SetFirstDemoMission", MissionsFuncs_SetFirstDemoMission, 1, NULL },
	{ "SetOutMissionArrow", MissionsFuncs_SetOutMissionArrow, 1, NULL },
	{ "ActivateTarget", MissionsFuncs_ActivateTarget, 1, NULL },
	{ "GetSecondary", MissionsFuncs_GetSecondary, 1, NULL },
	{ "SetSecondary", MissionsFuncs_SetSecondary, 1, NULL },
	{ "FinishGame", MissionsFuncs_FinishGame, 1, NULL },
	{ "ShowMissionInfo", MissionsFuncs_ShowMissionInfo, 1, NULL },
	{ "SetMissionTarget", MissionsFuncs_SetMissionTarget, 1, NULL },
	{ "SetMultipleMissionTargets", MissionsFuncs_SetMultipleMissionTargets, 1, NULL },
	{ "CleanAllTargets", MissionsFuncs_CleanAllTargets, 1, NULL },
	{ "DelMissionTarget", MissionsFuncs_DelMissionTarget, 1, NULL },
	{ "IsCharMissionObjective", MissionsFuncs_IsCharMissionObjective, 1, NULL },
	{ "CharMissionDialog", MissionsFuncs_CharMissionDialog, 1, NULL },
	{ "CancelSecondaryMission", MissionsFuncs_CancelSecondaryMission, 1, NULL },
	{ "SinglePlayerDeath", MissionsFuncs_SinglePlayerDeath, 1, NULL },
	{ "CreatePhotoObject", MissionsFuncs_CreatePhotoObject, 1, NULL },
	{ "UnsetPhotoObject", MissionsFuncs_UnsetPhotoObject, 1, NULL },
	{ "ReassignBertosPlayerTarget", MissionsFuncs_ReassignBertosPlayerTarget, 1, NULL },
	{ "CreateBerto", MissionsFuncs_CreateBerto, 1, NULL },
	{ "CreateBertos", MissionsFuncs_CreateBertos, 1, NULL },
	{ "OnBertoDeath", MissionsFuncs_OnBertoDeath, 1, NULL },
	{ "CrazyDealStatus", MissionsFuncs_CrazyDealStatus, 1, NULL },
	{ "GetNumCrazyDealCompleted", MissionsFuncs_GetNumCrazyDealCompleted, 1, NULL },
	{ "OnCrazyDealFinishedMessage", MissionsFuncs_OnCrazyDealFinishedMessage, 1, NULL },
	{ "OnCrazyDealTarget", MissionsFuncs_OnCrazyDealTarget, 1, NULL },
	{ "NewCrazyDeal", MissionsFuncs_NewCrazyDeal, 1, NULL },
	{ "SetNextSuperDeal", MissionsFuncs_SetNextSuperDeal, 1, NULL },
	{ "StartActualSuperDeal", MissionsFuncs_StartActualSuperDeal, 1, NULL },
	{ "EndOfSuperDeal", MissionsFuncs_EndOfSuperDeal, 1, NULL },
	{ "StartSuperDeal", MissionsFuncs_StartSuperDeal, 1, NULL },
	{ "RelaxShipsInList", MissionsFuncs_RelaxShipsInList, 1, NULL },
	{ "SetInvulnerableShipsInList", MissionsFuncs_SetInvulnerableShipsInList, 1, NULL },
	{ "OnSuperDealFinish", MissionsFuncs_OnSuperDealFinish, 1, NULL },
	{ "BuildMultipleEnemiesData", MissionsFuncs_BuildMultipleEnemiesData, 1, NULL },
	{ "BuildTeamDeathMatchData", MissionsFuncs_BuildTeamDeathMatchData, 1, NULL },
	{ "BuildVersusData", MissionsFuncs_BuildVersusData, 1, NULL },
	{ "BuildDominationData", MissionsFuncs_BuildDominationData, 1, NULL },
	{ "BuildCTFData", MissionsFuncs_BuildCTFData, 1, NULL },
	{ "SuperDealOnDefeat", MissionsFuncs_SuperDealOnDefeat, 1, NULL },
	{ "SuperDealOnVictory", MissionsFuncs_SuperDealOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MissionsFuncs)
