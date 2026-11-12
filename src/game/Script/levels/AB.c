/*
 * The scripts of the level AB in C (levels/ab/scripts of data03.packed, issue #2): MapFinalFight, the fight against
 * the boss in three phases (the shield, the regeneration bar, the taunts, the hints after deaths), then the end of
 * the game in ABIndoor; and Map, the asteroid field (asteroids that drift a moment, split in two on every second death).
 * LoadFinal and MapSnd are data (the files in data/levels/ab). The module variables (the BOSS_ numbers, the hint
 * times, respawnTime, bossName, numTaunt, pos, time, FASE, HINT_COUNTER, PrevCreateStdShips; Map's SpawnPositions,
 * asttype, SinglePlayer, idctr, LastTime, ctrTimer, Duplicate) stay in the scripts.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of both
 * modules are read where the script reads them and written through at once; ShipMaker.StdShips is changed in place,
 * the function values put into ShipMaker, Chars, Parking, Traffic and Menu are the objects of the module (OBJLIB 5
 * (9c)). The boss, its shield and the asteroids are handle values (OBJLIB 7). The alarm, the save variables, the
 * regeneration, the ammo of the weapons, the cinema bars and the action set are C; Scrap.CreateEntity, Scrap.Set,
 * Scrap.SetAlarm, Scrap.SetLockAlarm, Scrap.Rand, Scrap.Preload3DObject, Scrap.DeleteScheduledFuncs,
 * Scrap.AddScheduledFunc, Scrap.UsrEntity, SSound.SetMusic, SScorer.Set and SScorer.Fade go through their seams
 * (cases record them). The engine call of the shield (Scrap.SetCallFunc, AddParams, AddParami, Execute) and
 * SAI.SetStateVehicle go through their refs (stage 7: the native entries, a function a case put there by name).
 *
 * Direct calls (stage 3): the functions of the other ports (Speech, Vehicles, VehiclesNPC, ShipMaker, Traffic,
 * Elevator, Scorer, Menu, SaveGame) and the own functions of both modules (their globals) go through their refs
 * (PORT_CALL, PORT_LOWN, LV_SCORERG; the seam takes the call by name when a mission or a case has put something else
 * there, OBJLIB 5 (9c)). By name: PrevCreateStdShips (a variable holding ShipMaker's own function; OBJLIB 5 (9c)). The
 * functions given to the scheduler and to the entities (Mode_Later, Mode_Schedule, the OnDeath texts) take the entry of
 * a port's own function (stage 4: Port_Scheduled, PYF_ENTRY), the ones given to ShipMaker, Chars, Parking and Traffic are
 * values of their variables (OBJLIB 5 (9c)).
 */
#include <string.h>

#include "scrap/alarm.h"
#include "scrap/game.h"
#include "scrap/logic.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL ab

#define FF "MapFinalFight"

/* ---- shared lines ---- */

/* Scrap.SetAlarm(0) through its seam */
static byte Ab_AlarmOff(void) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Mode_SetAlarm(&zero);
}

/* SLogic.SetShipToRegenerate(name, speed, endSpeed) ("sff": a ship that is no Car regenerates none) */
static byte Ab_Regen(const nativeVal_t *name, const nativeVal_t *speed, const nativeVal_t *endSpeed) {
	const char *n;
	double a, b;
	if (!Native_ArgStr(name, 1, &n) || !Native_ArgDouble(speed, &a) || !Native_ArgDouble(endSpeed, &b)) {
		return 0;
	}
	cEntity *e = World_FindEntity(n);
	if (e) {
		ShipRegen_Set(&g_pWorld->logic.shipRegen, Entity_IsType(e, TYEN_CAR) ? e : NULL, (float)a, (float)b);
	}
	return 1;
}

/* the same with the ship and the two speeds globals of the module (read in this order) */
static byte Ab_RegenG(const char *name, const char *speed, const char *endSpeed) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t);
	byte ok = Mode_G(FF, name, n) && Mode_G(FF, speed, a) && Mode_G(FF, endSpeed, b) && Ab_Regen(n, a, b);
	return Mode_Done(&t, ok);
}

/* Scrap.Set(name, v) with v a global of the module */
static byte Ab_SetG(const char *name, const char *global) {
	nativeVal_t v;
	byte ok = Mode_G(FF, global, &v) && Mode_GVarSet(name, &v);
	Native_Free(&v);
	return ok;
}

/* module.var = d */
static byte Ab_ModF(const char *module, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Mode_SetModVar(module, var, &v);
}

/* ---- MapFinalFight ---- */

/* GetBossProfile(): the AI profile of the boss ship */
static byte Ff_GetBossProfile(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const profile[] = { "Ship=SBOSS1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
		"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=3", "UsePercSonic=70", "UsePercCloud=15",
		"UsePercElectro=15", "UsePercHook=0", "FactorHookMap=8", "CadenceShoot=13", "ControlAim=8", "AngAttack=0.7",
		"CounterAttack=0.9", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
		"FactorDamageGetAway=0.40", "FactorDamageItemLife=0.3", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1",
		"UsePercHookGetAway=10", "TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL };
	(void)a;
	return LvN_Profile(profile, out);
}

/* the ship of a standard ship table: (model, motor 1, motor 2, ..., weapons, mass) */
static void Ff_StdShip(nativeVal_t *ship, const char *const *texts, long n) {
	Native_SetEmpty(ship, NV_TUPLE);
	for (int i = 0; i < 6; i++) {
		LvN_Str(Native_Append(ship), texts[i]);
	}
	Native_SetInt(Native_Append(ship), n);
}

/* FakeShips(): ShipMaker.CreateStdShips while the level runs: the standard ships and the boss ship */
static byte Ff_FakeShips(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const boss[] = { "SBoss1", "MBoss1", "MBoss1", "", "", "15,15,15,15,15,15" };
	nativeVal_t ship, key;
	(void)a;
	(void)out;
	/* OBJLIB 5 (9c): PrevCreateStdShips is a variable that holds ShipMaker's function (InitStatus), called by name */
	if (!Mode_Import("ShipMaker") || !LV_OWN(FF, "PrevCreateStdShips", NULL, "")) {
		return 0;
	}
	Ff_StdShip(&ship, boss, 150);
	LvN_Str(&key, "SBOSS1");
	byte ok = Mode_ModDictPut("ShipMaker", "StdShips", &key, &ship);
	Native_Free(&ship);
	Native_Free(&key);
	return ok;
}

/* InitStatus(LevelPath): FakeShips replaces ShipMaker.CreateStdShips (the old one kept in PrevCreateStdShips) */
static byte Ff_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("ShipMaker")) {
		return 0;
	}
	/* OBJLIB 5 (9c): the function object of ShipMaker, kept as the object it is */
	return Mode_PutFromMod(FF, "PrevCreateStdShips", "ShipMaker", "CreateStdShips") &&
		LvN_ModSetGlobal("ShipMaker", "CreateStdShips", FF, "FakeShips");
}

/* Init(LevelPath): the start time, no alarm, the callbacks of the player's death and of the ship changes, the music */
static byte Ff_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Speech", "Chars", "Parking", "Vehicles", "SSound" };
	nativeVal_t now, model;
	(void)a;
	(void)out;
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetFloat(&now, Mode_Time());
	if (!Mode_Put(FF, "time", NULL, &now)) {
		return 0;
	}
	if (!Mode_SetLockAlarmI(0) || !Ab_AlarmOff() || !Mode_SetLockAlarmI(1)) {
		return 0;
	}
	if (!LvN_Com("Mission_FinalFight_RemoteMessage1", "CrazyGambler", NULL, NULL, NULL) ||
		!LvN_ModSetGlobal("Chars", "OnMainCharDeath", FF, "OnPlayerDeath") || !LvN_ModSetI("Vehicles", "DelPlayerShipOnDeath", 0) ||
		!LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", FF, "CreateBoss") ||
		!LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", FF, "BossRelax") || !Mode_GVarSetI("InfernoTurnSpeed", 60)) {
		return 0;
	}
	LvN_Str(&model, "Models/GFX/BossShield/BossShield.m3d");
	byte ok = Mode_Preload(&model) && Mode_GVarSetI("useDynamicMusic", 0) && LvN_SetMusic("Music/FinalFight.ogg", 1.0);
	Native_Free(&model);
	return ok;
}

/* the boss gets its profile again: pd = GetBossProfile(); VehiclesNPC.SetStdProperties(e.Name, pd.Ship);
 * VehiclesNPC.ModAIProff(e, pd) */
static byte Ff_Profile(const nativeVal_t *e) {
	nativeVal_t pd;
	Native_SetNone(&pd);
	byte ok = PORT_LOWN(MapFinalFight, GetBossProfile, &pd, "") && LvN_StdAI(e, &pd);
	Native_Free(&pd);
	return ok;
}

/* the shield of the boss (the first CreateBoss): the shield item without its link */
static byte Ff_Shield(nativeVal_t *shield) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *zero = Mode_V(&t), *off = Mode_V(&t);
	Native_SetInt(zero, 0);
	Native_SetEmpty(off, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(off), 0);
	}
	byte ok = LvN_Create(Mode_S(&t, "BossShield"), zero, zero, zero, "Item", shield) &&
		Mode_VModel(shield, Mode_S(&t, "Models/GFX/BossShield/BossShield.m3d")) && LvN_SetI(shield, "Active", 0) &&
		Mode_VSet(shield, "OffModPos", off) && LvN_SetI(shield, "DeltaOffMul", 4) && LvN_SetI(shield, "OffMul", 0) &&
		LvN_SetI(shield, "UseLinkAng", 1);
	return Mode_Done(&t, ok);
}

/* the boss at its dummy (the first CreateBoss): phase 1, the regeneration, the taunts every 30 s, the scorer */
static byte Ff_Boss(nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *name = Mode_V(&t);
	LvN_Dummy("DM_Boss_Spawn", 1, dummy);
	byte ok = LvN_Item(dummy, 1, pos) && Mode_Put(FF, "pos", NULL, pos) &&
		PORT_CALL(VehiclesNPC, MakeAI, e, "ssii", "SBoss1", "Boss", 0L, 0L) && LvN_SetG(e, "Pos", FF, "pos") &&
		Ff_Profile(e) && LvN_SetS(e, "WeapList", "0,15,0,0,0,0") && LvN_SetI(e, "Money", -1) && LvN_SetS(e, "TgType", "red") &&
		LvN_SetI(e, "LaunchTaunts", 0) && LvN_Life(e, FF, "BOSS_LIFE") &&
		LvN_SetS(e, "OnDeath", "MapFinalFight.OnBossDestroyed") && Mode_VGet(e, "Name", name) &&
		Mode_Put(FF, "bossName", NULL, name) && LvN_SetS(e, "OnDamage", "!FXBossOnDamage") &&
		Ab_SetG("LifeToLaunchCallback", "BOSS_LIFE_BULLET") &&
		Mode_GVarSetS("LifeReachedCallback", "MapFinalFight.OnBossRecoversZero") &&
		Ab_RegenG("bossName", "BOSS_RegenInit", "BOSS_RegenEnd") && LvN_Com("Mission_FinalFight_RemoteMessage2", "Boss", NULL, NULL, NULL) &&
		Mode_Later(30, FF, "LaunchTaunt", NULL, 0, "LaunchTaunt") && LV_SCORERG(MapFinalFight, MissionScorer) && LvN_Draw();
	return Mode_Done(&t, ok);
}

/* CreateBoss(): the switch to the ship: the first time the shield and the boss at its dummy (phase 1, the scorer, the
 * taunts every 30 s); then the boss attacks */
static byte Ff_CreateBoss(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Speech", "Vehicles", "VehiclesNPC", "OutMap" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *boss = Mode_V(&t), *shield = Mode_V(&t), *e = Mode_V(&t), *name = Mode_V(&t), *link = Mode_V(&t);
	(void)a;
	(void)out;
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_G(FF, "bossName", boss)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(boss, "")) {
		if (!Ff_Shield(shield) || !Ff_Boss(e)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(shield)) {
			byte ok = LvN_SetI(shield, "Active", 1) && LvN_SetS(shield, "LinkModel", ",") && Mode_VGet(e, "Name", name) &&
				Mode_Cat(&t, link, 3, name, Mode_S(&t, ","), Mode_S(&t, "DC_Player_01")) &&
				Mode_VSet(shield, "LinkModel", link) && PORT_LOWN(MapFinalFight, SetShieldPhase, NULL, "");
			if (!ok) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, PORT_LOWN(MapFinalFight, BossAttacks, NULL, ""));
}

/* SetShieldPhase(): the shield effect of the phase (FASE) */
static byte Ff_SetShieldPhase(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t phase;
	(void)a;
	(void)out;
	/* the engine call function */
	if (!PORT_CALL(Scrap, SetCallFunc, NULL, "s", "!FXBossShieldSet") || !PORT_CALL(Scrap, AddParams, NULL, "s", "BossShield") ||
		!Mode_G(FF, "FASE", &phase)) {
		return 0;
	}
	byte ok = PORT_CALL(Scrap, AddParami, NULL, "v", &phase) && PORT_CALL(Scrap, Execute, NULL, "");
	Native_Free(&phase);
	return ok;
}

/* MissionScorer(id): the text of the phase and the regeneration bar of the boss */
static byte Ff_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *one = Mode_V(&t), *fone = Mode_V(&t);
	const nativeVal_t *id = &a[0];
	(void)out;
	Native_SetInt(one, 1);
	Native_SetFloat(fone, 1.0);
	if (!Mode_Import("Scorer") || !LvN_ScorerTextBegin(id, "BossText", 0) || !LvN_ScorerTextEnd(id, "BossText") ||
		!LvN_ScorerSet(id, "BossText", "Visible", one) || !Mode_ScorerAdd(id, Mode_S(&t, "RegenerationBar"), "Bar")) {
		return Mode_Done(&t, 0);
	}
	/* Scorer.MissionScorerBar_SetDefaults(id, "RegenerationBar", back, main, black) with the three colours of Scorer:
	 * the function is looked up after the colours (the script before them: the same unless both are gone) */
	byte ok = Mode_BarDefaults(id, "RegenerationBar", NULL, NULL) && LvN_ScorerSet(id, "RegenerationBar", "MaxValue", fone) &&
		LvN_ScorerSet(id, "RegenerationBar", "Value", fone) &&
		LvN_ScorerSet(id, "RegenerationBar", "Visible", one) && Mode_GVarSetS("RegenerationItem", "RegenerationBar") &&
		PORT_LOWN(MapFinalFight, SetFase, NULL, "");
	return Mode_Done(&t, ok);
}

/* SetFase(): the text and the bar colours of the phase, the shield */
static byte Ff_SetFase(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const vars[] = { "MainRed", "MainGreen", "MainBlue", "BlackRed", "BlackGreen", "BlackBlue" };
	static const int colors[3][6] = { { 255, 255, 255, 0, 255, 0 }, { 0, 255, 0, 255, 0, 0 }, { 255, 0, 0, 0, 0, 0 } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *id = Mode_V(&t), *phase = Mode_V(&t), *rep = Mode_V(&t), *key = Mode_V(&t), *lang = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *c = Mode_V(&t);
	(void)a;
	(void)out;
	Native_SetInt(id, 0);
	if (!Mode_G(FF, "FASE", phase)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(phase, rep);
	if (!Mode_Cat(&t, key, 2, Mode_S(&t, "Mission_FinalFight_Scorer_"), rep)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(Native_Text(key), lang);
	if (!Mode_Arith('+', lang, Mode_S(&t, ":"), text) || !LvN_ScorerSet(id, "BossText", "Text", text)) {
		return Mode_Done(&t, 0);
	}
	int is = LvN_GEqI(FF, "FASE", 1), p = 0;
	if (is == 0) {
		is = LvN_GEqI(FF, "FASE", 2);
		p = is > 0 ? 1 : 2;
	}
	byte ok = is >= 0;
	for (int i = 0; i < 6 && ok; i++) {
		Native_SetInt(c, colors[p][i]);
		ok = LvN_ScorerSet(id, "RegenerationBar", vars[i], c);
	}
	return Mode_Done(&t, ok && PORT_LOWN(MapFinalFight, SetShieldPhase, NULL, ""));
}

/* BossAttacks(): the boss attacks the player */
static byte Ff_BossAttacks(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *boss = Mode_V(&t), *name = Mode_V(&t);
	(void)a;
	(void)out;
	byte ok = Mode_UsrHandle(usr);
	if (ok && Native_True(usr)) {
		ok = Mode_G(FF, "bossName", boss) && Mode_VGet(usr, "Name", name) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, boss, name);
	}
	return Mode_Done(&t, ok);
}

/* BossRelax(): the boss goes back to its spawn point */
static byte Ff_BossRelax(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *boss = Mode_V(&t), *e = Mode_V(&t), *name = Mode_V(&t), *pos = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	(void)a;
	(void)out;
	byte ok = Mode_G(FF, "bossName", boss) && Mode_EntityH(boss, e);
	if (ok && Native_True(e)) {
		ok = Mode_VGet(e, "Name", name) && Mode_G(FF, "pos", pos) && LvN_Item(pos, 0, x) && LvN_Item(pos, 1, y) &&
			LvN_Item(pos, 2, z) && PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvvi", 1L, name, x, y, z, 1000L);
	}
	return Mode_Done(&t, ok);
}

/* OnPlayerDeath(LooserName, WinnerName): the boss goes back and heals, the time of the fight counts for the hints,
 * the player respawns where he died */
static byte Ff_OnPlayerDeath(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *name = Mode_V(&t), *sv = Mode_V(&t), *hints = Mode_V(&t), *now = Mode_V(&t);
	nativeVal_t *start = Mode_V(&t), *d = Mode_V(&t), *sum = Mode_V(&t), *e = Mode_V(&t), *rt = Mode_V(&t);
	nativeVal_t *at = Mode_V(&t), *args = Mode_V(&t), *boss = Mode_V(&t), *life = Mode_V(&t);
	const nativeVal_t *looser = &a[0];
	(void)out;
	if (!Mode_UsrHandle(usr) || !Mode_VGet(usr, "Name", name)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(name, looser)) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_LOWN(MapFinalFight, BossRelax, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("FinalBossHintsTime", sv);
	if (!Native_ToFloat(sv, hints)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	if (!Mode_G(FF, "time", start) || !Mode_Arith('-', now, start, d) || !Mode_Arith('+', hints, d, sum)) {
		return Mode_Done(&t, 0);
	}
	LvN_SaveRepr("FinalBossHintsTime", sum);
	Native_SetFloat(now, Mode_Time());
	if (!Mode_EntityH(looser, e) || !Mode_G(FF, "respawnTime", rt) || !Mode_Arith('+', now, rt, at)) {
		return Mode_Done(&t, 0);
	}
	/* the global (a mission may replace it; OBJLIB 7: a function held as NV_OBJ), read before the arguments */
	nativeVal_t *fn = Mode_V(&t);
	byte ok = Mode_G(FF, "RespawnPlayer", fn);
	Native_SetEmpty(args, NV_TUPLE);
	Native_Copy(Native_Append(args), looser);
	ok = ok && Mode_VGet(e, "Pos", Native_Append(args)) && Mode_VGet(e, "Ang", Native_Append(args)) &&
		Mode_ScheduleV(at, fn, args->items, args->n, NULL);
	ok = ok && LvN_Entity("Boss", boss) && Mode_VGet(boss, "MaxLife", life) && Mode_VSet(boss, "Life", life);
	return Mode_Done(&t, ok);
}

/* Speech.CancelRemoteCom(); Speech.AddRemoteCom(Mission_FinalFight_RemoteMessage<n>, "CrazyGambler") */
static byte Ff_Hint(int n) {
	char key[0x40];
	Str_Printf(key, sizeof(key), "Mission_FinalFight_RemoteMessage%d", n);
	return PORT_CALL(Speech, CancelRemoteCom, NULL, "") && LvN_Com(key, "CrazyGambler", NULL, NULL, NULL);
}

/* RespawnPlayer(Usrname, respawnPos, ang): the player's ship again; every fifth time from the fourth a hint of the
 * phase; the boss attacks */
static byte Ff_RespawnPlayer(const nativeVal_t *a, nativeVal_t *out) {
	static const int hints[] = { 5, 13, 21 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *count = Mode_V(&t), *five = Mode_V(&t), *m = Mode_V(&t);
	(void)out;
	if (!Mode_Import("Vehicles") || !Mode_Import("Speech") || !PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", &a[0]) ||
		!Mode_EntityH(&a[0], usr) || !Mode_VSet(usr, "Pos", &a[1]) || !Mode_VSet(usr, "Ang", &a[2]) ||
		!LvN_GAdd(FF, "HINT_COUNTER", 1, NULL) || !Mode_G(FF, "HINT_COUNTER", count)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(five, 5);
	if (!Mode_Arith('%', count, five, m)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(m, 4)) {
		for (int i = 0; i < 3; i++) {
			int is = LvN_GEqI(FF, "FASE", i + 1);
			if (is > 0 && !Ff_Hint(hints[i])) {
				return Mode_Done(&t, 0);
			}
			if (is < 0) {
				return Mode_Done(&t, 0);
			}
			if (is) {
				break;
			}
		}
	}
	return Mode_Done(&t, PORT_LOWN(MapFinalFight, BossAttacks, NULL, ""));
}

/* Speech.AddRemoteCom("Mission_FinalFight_RemoteMessage" + `base + mul*(numTaunt % mod)`, "Boss") */
static byte Ff_Taunt(long base, long mul, long mod) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *m = Mode_V(&t), *p = Mode_V(&t), *k = Mode_V(&t), *rep = Mode_V(&t), *key = Mode_V(&t);
	byte ok = Mode_G(FF, "numTaunt", n) && Mode_Arith('%', n, Mode_I(&t, mod), m) &&
		(mul == 1 || Mode_Arith('*', Mode_I(&t, mul), m, p)) && Mode_Arith('+', Mode_I(&t, base), mul == 1 ? m : p, k);
	if (ok) {
		Mode_Repr(k, rep);
		ok = Mode_Cat(&t, key, 2, Mode_S(&t, "Mission_FinalFight_RemoteMessage"), rep) &&
			PORT_CALL(Speech, AddRemoteCom, NULL, "vs", key, "Boss");
	}
	return Mode_Done(&t, ok);
}

/* LaunchTaunt(): a taunt of the boss every 30 s: by phase, by the hints given and in phase 3 by the boss' life */
static byte Ff_LaunchTaunt(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hc = Mode_V(&t), *boss = Mode_V(&t), *e = Mode_V(&t), *life = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("Speech") || !Mode_Later(30, FF, "LaunchTaunt", NULL, 0, "LaunchTaunt")) {
		return Mode_Done(&t, 0);
	}
	for (int phase = 1; phase <= 3; phase++) {
		int is = LvN_GEqI(FF, "FASE", phase);
		if (is == 0) {
			continue;
		}
		if (is < 0 || !Mode_G(FF, "HINT_COUNTER", hc)) {
			return Mode_Done(&t, 0);
		}
		byte ok;
		if (Native_Compare(hc, Mode_I(&t, 3)) < 0) {
			ok = Ff_Taunt(phase == 1 ? 2 : phase == 2 ? 10 : 18, 1, 3);
		} else if (phase < 3) {
			ok = Ff_Taunt(phase == 1 ? 6 : 14, 1, 3);
		} else {
			ok = Mode_G(FF, "bossName", boss) && Mode_EntityH(boss, e) && Mode_VGet(e, "Life", life) &&
				Ff_Taunt(Native_Compare(life, Mode_I(&t, 400)) < 0 ? 22 : 23, 2, 2);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		break;
	}
	return Mode_Done(&t, LvN_GAdd(FF, "numTaunt", 1, NULL));
}

/* the boss after its life reached the mark of a phase (phase 2 or 3): the profile, the counters, the hint of the
 * phase, the weapons and the life, the next mark, the regeneration, the scorer blinks for 2.5 s */
static byte Ff_NextPhase(const nativeVal_t *a, int phase) {
	static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *am = Mode_V(&t), *blink = Mode_V(&t), *speed = Mode_V(&t);
	Native_SetFloat(speed, 0.05);
	if (!Mode_Import("Speech") || (phase == 3 && !Mode_Import("VehiclesNPC"))) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_EqI(&a[1], -1)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_EntityH(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	byte ok = Ff_Profile(e) && LvN_PutI(FF, "FASE", phase) && LvN_PutI(FF, "numTaunt", 0) && LvN_PutI(FF, "HINT_COUNTER", 0) &&
		Ff_Hint(phase == 2 ? 9 : 17) && LvN_SetS(e, "WeapList", phase == 2 ? "15,0,0,15,0,0" : "6,9,6,9,6,9") &&
		LvN_SetG(e, "MaxLife", FF, phase == 2 ? "BOSS_LIFE_BULLET" : "BOSS_LIFE_FINAL") &&
		LvN_SetG(e, "Life", FF, "BOSS_LIFE_BULLET");
	for (int i = 0; i < 3 && ok && phase == 3; i++) {
		/* SWeap.GetFAmmo(i, "Max") */
		float value;
		if (cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, i), "Max", &value)) {
			Native_SetFloat(am, (double)value);
		} else {
			Native_SetNone(am);
		}
		ok = Mode_VSet(e, ammo[i], am);
	}
	ok = ok && LvN_SetS(e, "OnDeath", "MapFinalFight.OnBossDestroyed") && LvN_SetS(e, "OnDamage", "!FXBossOnDamage");
	if (ok && phase == 2) {
		ok = Ab_SetG("LifeToLaunchCallback", "BOSS_LIFE_FINAL") &&
			Mode_GVarSetS("LifeReachedCallback", "MapFinalFight.OnBossRecoversEnd") &&
			Ab_RegenG("bossName", "BOSS_Last_RegenInit", "BOSS_Last_RegenEnd");
	} else if (ok) {
		ok = Mode_GVarSetI("LifeToLaunchCallback", 0) && Mode_GVarSetS("LifeReachedCallback", "") &&
			Ab_Regen(Mode_S(&t, "Boss"), speed, speed);
	}
	ok = ok && PORT_LOWN(MapFinalFight, SetFase, NULL, "") &&
		Mode_ScorerSet(Mode_I(&t, 0), Mode_S(&t, "RegenerationBar"), "Blink", Mode_I(&t, 1));
	if (ok) {
		Native_SetEmpty(blink, NV_TUPLE);
		Native_SetInt(Native_Append(blink), 0);
		LvN_Str(Native_Append(blink), "RegenerationBar");
		LvN_Str(Native_Append(blink), "Blink");
		Native_SetInt(Native_Append(blink), 0);
		ok = Mode_LaterIn(2.5, "SScorer", "Set", blink->items, blink->n, NULL);
	}
	return Mode_Done(&t, ok);
}

/* OnBossRecoversZero(name, idx), OnBossRecoversEnd(name, idx): the engine callback of the life mark (idx -1) */
static byte Ff_OnBossRecoversZero(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ff_NextPhase(a, 2);
}

static byte Ff_OnBossRecoversEnd(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ff_NextPhase(a, 3);
}

/* OnBossDestroyed(LooserName, WinnerName): the death of the boss: its death taunt, no more taunts, the alarm falls,
 * a way point, PreLoadFinal in 4 s */
static byte Ff_OnBossDestroyed(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!Mode_Import("Vehicles") || !Mode_Import("SaveGame") || !LvN_ModSetI("Vehicles", "ForceDeathTaunt", 1) ||
		!PORT_CALL(Vehicles, OnDeath, NULL, "vv", &a[0], &a[1]) || !Mode_DelSchedS("LaunchTaunt")) {
		return 0;
	}
	SaveVars_Delete("FinalBossHintsTime");
	SaveVar_Set("BossDestroyed", "1");
	if (!Mode_GVarSetN("AlarmFallDelta", -0.05)) {
		return 0;
	}
	return Ab_AlarmOff() && PORT_CALL(SaveGame, QWayPoint, NULL, "") && Mode_Later(4, FF, "PreLoadFinal", NULL, 0, NULL);
}

/* PreLoadFinal(): no input, no scorer, fade to black, LoadFinal in 4 s */
static byte Ff_PreLoadFinal(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("Menu")) {
		return 0;
	}
	Mode_SetActionSet("Inactive");
	if (!PORT_CALL(Menu, DrawScorer, NULL, "is", 0L, "Inactive") || !Mode_GVarSetS("EscapeEvent", "") ||
		!Mode_GVarSetS("MainMissionEvent", "")) {
		return 0;
	}
	/* SScorer.SetCinema(0, 0, 0) */
	cScorer *sc = Scorer_Get(0);
	if (sc) {
		ScorerCinema_Set(&sc->cinema, 0, 0.0f);
	}
	return Mode_Fade(0, lv_clear, lv_black, 4.0) && Mode_Later(4, FF, "LoadFinal", NULL, 0, NULL);
}

LV_FN(Ff, GetBossProfilePy, 0, Ff_GetBossProfile)
LV_FN(Ff, FakeShipsPy, 0, Ff_FakeShips)
LV_FN(Ff, InitStatusPy, 1, Ff_InitStatus)
LV_FN(Ff, InitPy, 1, Ff_Init)
LV_FN(Ff, CreateBossPy, 0, Ff_CreateBoss)
LV_FN(Ff, SetShieldPhasePy, 0, Ff_SetShieldPhase)
LV_FN(Ff, MissionScorerPy, 1, Ff_MissionScorer)
LV_FN(Ff, SetFasePy, 0, Ff_SetFase)
LV_FN(Ff, BossAttacksPy, 0, Ff_BossAttacks)
LV_FN(Ff, BossRelaxPy, 0, Ff_BossRelax)
LV_FN(Ff, OnPlayerDeathPy, 2, Ff_OnPlayerDeath)
LV_FN(Ff, RespawnPlayerPy, 3, Ff_RespawnPlayer)
LV_FN(Ff, LaunchTauntPy, 0, Ff_LaunchTaunt)
LV_FN(Ff, OnBossRecoversZeroPy, 2, Ff_OnBossRecoversZero)
LV_FN(Ff, OnBossRecoversEndPy, 2, Ff_OnBossRecoversEnd)
LV_FN(Ff, OnBossDestroyedPy, 2, Ff_OnBossDestroyed)
LV_FN(Ff, PreLoadFinalPy, 0, Ff_PreLoadFinal)

const portFn_t g_ABFinalFightMethods[] = {
	PORT_FN(GetBossProfile, Ff_GetBossProfilePy),
	PORT_FN(FakeShips, Ff_FakeShipsPy),
	PORT_FN(InitStatus, Ff_InitStatusPy),
	PORT_FN(Init, Ff_InitPy),
	PORT_FN(CreateBoss, Ff_CreateBossPy),
	PORT_FN(SetShieldPhase, Ff_SetShieldPhasePy),
	PORT_FN(MissionScorer, Ff_MissionScorerPy),
	PORT_FN(SetFase, Ff_SetFasePy),
	PORT_FN(BossAttacks, Ff_BossAttacksPy),
	PORT_FN(BossRelax, Ff_BossRelaxPy),
	PORT_FN(OnPlayerDeath, Ff_OnPlayerDeathPy),
	PORT_FN(RespawnPlayer, Ff_RespawnPlayerPy),
	PORT_FN(LaunchTaunt, Ff_LaunchTauntPy),
	PORT_FN(OnBossRecoversZero, Ff_OnBossRecoversZeroPy),
	PORT_FN(OnBossRecoversEnd, Ff_OnBossRecoversEndPy),
	PORT_FN(OnBossDestroyed, Ff_OnBossDestroyedPy),
	PORT_FN(PreLoadFinal, Ff_PreLoadFinalPy),
	PORT_FNEND
};

/* ---- Map ---- */

#define MAP "Map"

/* onTrackCreate(o): a traffic asteroid: fixed model, life 20, no money */
static byte AbMap_onTrackCreate(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t life;
	(void)out;
	Native_SetInt(&life, 20);
	return LvN_SetI(&a[0], "FixedModel", 1) && Mode_VSet(&a[0], "Life", &life) && Mode_VSet(&a[0], "MinLife", &life) &&
		Mode_VSet(&a[0], "MaxLife", &life) && LvN_SetI(&a[0], "Money", -1);
}

/* InitStatus(LevelPath): single player only: an outdoor map without police and traffic, quick hull deaths, the
 * traffic asteroids through onTrackCreate */
static byte AbMap_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "Traffic", "Vehicles" };
	static const char *const lists[] = { "AcNormalShips", "AcMediumShips", "AcHeavyShips" };
	static const char *const nums[] = { "NumNormal", "NumMedium", "NumHeavy" };
	nativeVal_t v;
	(void)a;
	(void)out;
	int single = LvN_GTrue(MAP, "SinglePlayer");
	if (single <= 0) {
		return single == 0;
	}
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetEmpty(&v, NV_TUPLE);
	Native_SetFloat(Native_Append(&v), 0.01);
	Native_SetFloat(Native_Append(&v), 0.02);
	byte ok = Mode_SetModVar("Vehicles", "HullLifeTimeDeath", &v) && LvN_ModSetI("Init", "Outdoor", 1) &&
		LvN_ModSetI("OutPolice", "PoliceActive", 0) && Mode_GVarSetI("iTrafficNumber", 0);
	Native_Free(&v);
	Native_SetEmpty(&v, NV_LIST);
	for (int i = 0; i < 3 && ok; i++) {
		ok = Mode_SetModVar("Traffic", lists[i], &v);
	}
	for (int i = 0; i < 3 && ok; i++) {
		ok = LvN_ModSetI("Traffic", nums[i], 0);
	}
	return ok && LvN_ModSetGlobal("Traffic", "onTrackCreate", MAP, "onTrackCreate");
}

/* the standard ship of an asteroid type at its name: ShipMaker.AddShip, AddShipTon, AddShipBalance, InitAnmShip and
 * ShipMaker.StdShips[string.upper(name)] */
static byte AbMap_Type(const nativeVal_t *name) {
	static const char *const texts[] = { "", "", "", "", "0,0,0,0,0,0" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *bal = Mode_V(&t), *none = Mode_V(&t), *ship = Mode_V(&t), *key = Mode_V(&t);
	Native_SetEmpty(bal, NV_TUPLE);
	for (int i = 0; i < 4; i++) {
		Native_SetInt(Native_Append(bal), 0);
	}
	byte ok = PORT_CALL(ShipMaker, AddShip, NULL, "viii", name, 20000L, 1000L, 10000L) &&
		PORT_CALL(ShipMaker, AddShipTon, NULL, "viii", name, 20L, 20L, 30000L) &&
		PORT_CALL(ShipMaker, AddShipBalance, NULL, "vv", name, bal) &&
		PORT_CALL(ShipMaker, InitAnmShip, NULL, "vvvvv", name, none, none, none, none);
	if (ok) {
		Native_SetEmpty(ship, NV_TUPLE);
		Native_Copy(Native_Append(ship), name);
		for (int i = 0; i < 5; i++) {
			LvN_Str(Native_Append(ship), texts[i]);
		}
		Native_SetInt(Native_Append(ship), 20);
		ok = Native_Upper(name, key) && Mode_ModDictPut("ShipMaker", "StdShips", key, ship);
	}
	return Mode_Done(&t, ok);
}

/* Init(LevelPath): the free space magnet around the mission centre, the teleporter, the asteroid ship types, an
 * asteroid at every spawn position */
static byte AbMap_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const axes[] = { "FreeSpaceMagnetCenterX", "FreeSpaceMagnetCenterY", "FreeSpaceMagnetCenterZ" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *p = Mode_V(&t), *x = Mode_V(&t), *from = Mode_V(&t), *to = Mode_V(&t);
	nativeVal_t *types = Mode_V(&t), *spawns = Mode_V(&t), *item = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("Elevator") || !Mode_GVarSetI("FreeSpaceMagnet", 1)) {
		return Mode_Done(&t, 0);
	}
	LvN_Dummy("DM_Mission_Centre", 1, dummy);
	for (int i = 0; i < 3; i++) {
		Native_Free(p);
		Native_Free(x);
		if (!LvN_Item(dummy, 1, p) || !LvN_Item(p, i, x) || !Mode_GVarSet(axes[i], x)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetEmpty(from, NV_LIST);
	LvN_Str(Native_Append(from), "DM_Door_Teleporter_GamblinDen");
	Native_SetEmpty(to, NV_LIST);
	LvN_Str(Native_Append(to), "GamblinDen");
	if (!Mode_GVarSetI("FreeSpaceMagnetHiRadius", 95000) || !Mode_GVarSetI("FreeSpaceMagnetAccel", 56000) ||
		!PORT_CALL(Elevator, ConnectElevators, NULL, "vv", from, to) || !PORT_CALL(Traffic, SetPath, NULL, "") ||
		!LvN_GElems(MAP, "asttype", types)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(types);
	for (int i = 0; i < n; i++) {
		Native_Free(item);
		if (!Native_SeqItem(types, i, item) || !AbMap_Type(item)) {
			return Mode_Done(&t, 0);
		}
	}
	if (n < 0 || !PORT_CALL(Traffic, ResetPath, NULL, "") || !LvN_GElems(MAP, "SpawnPositions", spawns)) {
		return Mode_Done(&t, 0);
	}
	n = Native_ForCount(spawns);
	for (int i = 0; i < n; i++) {
		Native_Free(item);
		if (!Native_SeqItem(spawns, i, item) || !PORT_LOWN(Map, CreateAsteroid, NULL, "sv", "Asteroid", item)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* CreateAsteroid(name, SpwPos): an asteroid of the next type that drifts with the traffic for 0.5 to 2 s: the
 * asteroid */
static byte AbMap_CreateAsteroid(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *types = Mode_V(&t), *id = Mode_V(&t), *k = Mode_V(&t), *type = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t), *ang = Mode_V(&t), *now = Mode_V(&t), *at = Mode_V(&t);
	int len;
	if (!PORT_CALL(Traffic, SetPath, NULL, "") || !LvN_CreateAt(&a[0], &a[1], "Car", out) || !Mode_G(MAP, "asttype", types) ||
		!Mode_G(MAP, "idctr", id) || (len = Native_Size(types)) < 0 || !Mode_Arith('%', id, Mode_I(&t, len), k) ||
		!Native_Subscript(types, k, type) || !Mode_VGet(out, "Name", name) ||
		!PORT_CALL(Vehicles, SetStdProperties, NULL, "vvs", name, type, "")) {
		return Mode_Done(&t, 0);
	}
	byte ok = LvN_SetI(out, "FixedModel", 1) && LvN_SetI(out, "Money", -1) && LvN_SetS(out, "OnDeath", "Map.OnDeath") &&
		LvN_SetI(out, "Traf_Active", 1) && LvN_SetI(out, "Traf_Speed", 28000) && LvN_SetI(out, "MaxSpeed", 28000) &&
		LvN_SetF(out, "Fric", 1.0) && LvN_SetF(out, "InCtrl_Acc", 0.125) && LvN_SetF(out, "InCtrl_HRot", 0.05) &&
		Mode_Rand(Mode_I(&t, -180), Mode_I(&t, 180), r);
	if (ok) {
		Native_SetEmpty(ang, NV_TUPLE);
		Native_SetInt(Native_Append(ang), 0);
		Native_Copy(Native_Append(ang), r);
		Native_SetInt(Native_Append(ang), 0);
		Native_Free(r);
		ok = Mode_VSet(out, "Ang", ang) && PORT_CALL(Traffic, ResetPath, NULL, "") && LvN_GAdd(MAP, "idctr", 1, NULL);
	}
	if (ok) {
		/* Scrap.AddScheduledFunc(Scrap.GetTime() + Scrap.Rand(0.5, 2), detraffic, (p.Name,)) */
		Native_SetFloat(now, Mode_Time());
		nativeVal_t half;
		Native_SetFloat(&half, 0.5);
		ok = Mode_Rand(&half, Mode_I(&t, 2), r) && Mode_Arith('+', now, r, at);
		nativeVal_t fn = { 0 };
		ok = ok && Mode_G(MAP, "detraffic", &fn);
		/* p.Name read again where the script reads it */
		Native_Free(name);
		ok = ok && Mode_VGet(out, "Name", name);
		ok = ok && Mode_ScheduleV(at, &fn, name, 1, NULL);
		Native_Free(&fn);
	}
	if (!ok) {
		Native_Free(out);
		Native_SetNone(out);
	}
	return Mode_Done(&t, ok);
}

/* detraffic(Name): the asteroid leaves the traffic paths */
static byte AbMap_detraffic(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)out;
	if (!Mode_EntityH(&a[0], &e)) {
		return 0;
	}
	byte ok = !Native_True(&e) || LvN_SetI(&e, "Traf_Active", 0);
	Native_Free(&e);
	return ok;
}

/* OnDeath(LooserName, WinnerName): deaths in the same frame are spread 0.05 s apart */
static byte AbMap_OnDeath(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *now = Mode_V(&t), *last = Mode_V(&t), *timer = Mode_V(&t), *sum = Mode_V(&t);
	(void)out;
	Native_SetFloat(now, Mode_Time());
	if (!Mode_G(MAP, "LastTime", last)) {
		return Mode_Done(&t, 0);
	}
	nativeVal_t step;
	Native_SetFloat(&step, 0.05);
	byte ok;
	if (!Native_Compare(now, last)) {
		ok = LvN_LaterG(MAP, "ctrTimer", "RealOnDeath", a, 2) && Mode_G(MAP, "ctrTimer", timer) &&
			Mode_Arith('+', timer, &step, sum) && Mode_Put(MAP, "ctrTimer", NULL, sum);
	} else {
		ok = Mode_Put(MAP, "LastTime", NULL, now) && PORT_LOWN(Map, RealOnDeath, NULL, "vv", &a[0], &a[1]) &&
			Mode_Put(MAP, "ctrTimer", NULL, &step);
	}
	return Mode_Done(&t, ok);
}

/* a = CreateAsteroid("Asteroids", pos); a.Vel = SVec.Sub((0, 0, 0), vel) */
static byte AbMap_Split(const nativeVal_t *pos, const nativeVal_t *vel) {
	nativeVal_t ast, zero, back;
	Native_SetNone(&ast);
	Native_SetNone(&back);
	Native_SetEmpty(&zero, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(&zero), 0);
	}
	byte ok = PORT_LOWN(Map, CreateAsteroid, &ast, "sv", "Asteroids", pos) && Mode_VecSub(&zero, vel, &back) &&
		Mode_VSet(&ast, "Vel", &back);
	Native_Free(&ast);
	Native_Free(&zero);
	Native_Free(&back);
	return ok;
}

/* RealOnDeath(LooserName, WinnerName): a small explosion in place; every second death two asteroids fly apart from
 * where it died */
static byte AbMap_RealOnDeath(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *scale = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t), *ast = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *vel = Mode_V(&t);
	(void)out;
	if (!LvN_ModSetI("Vehicles", "ShipExplosionForceInplace", 1) || !Mode_RandD(0.55, 0.65, scale) ||
		!Mode_SetModVar("Vehicles", "ShipExplosionScaleFactor", scale) || !PORT_CALL(Vehicles, OnDeath, NULL, "vv", &a[0], &a[1]) ||
		!LvN_ModSetI("Vehicles", "ShipExplosionForceInplace", 0) || !Ab_ModF("Vehicles", "ShipExplosionScaleFactor", 1.0)) {
		return Mode_Done(&t, 0);
	}
	int dup = LvN_GTrue(MAP, "Duplicate");
	if (dup < 0 || !LvN_PutI(MAP, "Duplicate", !dup)) {
		return Mode_Done(&t, 0);
	}
	if (dup) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_EntityH(&a[0], e) || !Mode_VGet(e, "Pos", pos) || !PORT_LOWN(Map, CreateAsteroid, ast, "sv", "Asteroids", pos)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(r, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		if (!Mode_Rand(Mode_I(&t, -180), Mode_I(&t, 180), Native_Append(r))) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetEmpty(v, NV_TUPLE);
	Native_SetInt(Native_Append(v), 0);
	Native_SetInt(Native_Append(v), 28000);
	Native_SetInt(Native_Append(v), 0);
	byte ok = Mode_Rotate(v, r, vel) && Mode_VSet(ast, "Vel", vel) && AbMap_Split(pos, vel);
	return Mode_Done(&t, ok);
}

LV_FN(AbMap, onTrackCreatePy, 1, AbMap_onTrackCreate)
LV_FN(AbMap, InitStatusPy, 1, AbMap_InitStatus)
LV_FN(AbMap, InitPy, 1, AbMap_Init)
LV_FN(AbMap, CreateAsteroidPy, 2, AbMap_CreateAsteroid)
LV_FN(AbMap, detrafficPy, 1, AbMap_detraffic)
LV_FN(AbMap, OnDeathPy, 2, AbMap_OnDeath)
LV_FN(AbMap, RealOnDeathPy, 2, AbMap_RealOnDeath)

const portFn_t g_ABMapMethods[] = {
	PORT_FN(onTrackCreate, AbMap_onTrackCreatePy),
	PORT_FN(InitStatus, AbMap_InitStatusPy),
	PORT_FN(Init, AbMap_InitPy),
	PORT_FN(CreateAsteroid, AbMap_CreateAsteroidPy),
	PORT_FN(detraffic, AbMap_detrafficPy),
	PORT_FN(OnDeath, AbMap_OnDeathPy),
	PORT_FN(RealOnDeath, AbMap_RealOnDeathPy),
	PORT_FNEND
};

PORTS(levels_AB)
