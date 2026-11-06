/*
 * The scripts of the level AB in C (levels/ab/scripts of data03.packed, issue #2): MapFinalFight, the fight against
 * the boss in three phases (the shield, the regeneration bar, the taunts, the hints after deaths), then the end of
 * the game in ABIndoor. LoadFinal, Map and MapSnd are data (the files in data/levels/ab). The module variables (the
 * BOSS_ numbers, the hint times, respawnTime, bossName, numTaunt, pos, time, FASE, HINT_COUNTER,
 * PrevCreateStdShips) stay in the script.
 */
#include "scrap/script.h"
#include "Levels.h"

#define FF "MapFinalFight"

#define GLOBAL(n) Sf_Global(&f, FF, n)

/* GetBossProfile(): the AI profile of the boss ship */
static PyObject *Ff_GetBossProfile(PyObject *self, PyObject *args) {
	static const char *const profile[] = { "Ship=SBOSS1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
		"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=3", "UsePercSonic=70", "UsePercCloud=15",
		"UsePercElectro=15", "UsePercHook=0", "FactorHookMap=8", "CadenceShoot=13", "ControlAim=8", "AngAttack=0.7",
		"CounterAttack=0.9", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
		"FactorDamageGetAway=0.40", "FactorDamageItemLife=0.3", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1",
		"UsePercHookGetAway=10", "TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *sp = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttrs(&f, sp, FF, profile);
	return Sf_Return(&f, sp);
}

/* FakeShips(): ShipMaker.CreateStdShips while the level runs: the standard ships and the boss ship */
static PyObject *Ff_FakeShips(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Sf_Call(&f, GLOBAL("PrevCreateStdShips"), "()");
	PyObject *ship = Sf_Build(&f, "(ssssssi)", "SBoss1", "MBoss1", "MBoss1", "", "", "15,15,15,15,15,15", 150);
	Sf_SetItem(&f, Sf_Mod(&f, "ShipMaker", "StdShips"), Sf_Str(&f, "SBOSS1"), ship);
	return Sf_None(&f);
}

/* InitStatus(LevelPath): FakeShips replaces ShipMaker.CreateStdShips (the old one kept in PrevCreateStdShips) */
static PyObject *Ff_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Sf_SetGlobal(&f, FF, "PrevCreateStdShips", Sf_Mod(&f, "ShipMaker", "CreateStdShips"));
	Sf_SetMod(&f, "ShipMaker", "CreateStdShips", GLOBAL("FakeShips"));
	return Sf_None(&f);
}

/* Init(LevelPath): the start time, no alarm, the callbacks of the player's death and of the ship changes, the music */
static PyObject *Ff_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	static const char *const mods[] = { "Speech", "Chars", "Parking", "Vehicles", "SSound" };
	for (int i = 0; i < 5; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetGlobal(&f, FF, "time", Sf_Time(&f));
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 1);
	Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_FinalFight_RemoteMessage1", "CrazyGambler");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", GLOBAL("OnPlayerDeath"));
	Sf_SetMod(&f, "Vehicles", "DelPlayerShipOnDeath", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", GLOBAL("CreateBoss"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", GLOBAL("BossRelax"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "InfernoTurnSpeed", 60);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(s)", "Models/GFX/BossShield/BossShield.m3d");
	Sf_Run(&f, "Scrap", "Set", "(si)", "useDynamicMusic", 0);
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/FinalFight.ogg", 1.0);
	return Sf_None(&f);
}

/* the boss gets its profile again: pd = GetBossProfile(); VehiclesNPC.SetStdProperties(e.Name, pd.Ship);
 * VehiclesNPC.ModAIProff(e, pd) */
static void Ff_Profile(scriptFrame_t *f, PyObject *e) {
	int mark = f->n;
	PyObject *pd = Sf_Call(f, Sf_Global(f, FF, "GetBossProfile"), "()");
	Sf_Run(f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(f, e, "Name"), Sf_Attr(f, pd, "Ship"));
	Sf_Run(f, "VehiclesNPC", "ModAIProff", "(OO)", e, pd);
	Sf_Release(f, mark);
}

/* CreateBoss(): the switch to the ship: the first time the shield and the boss at its dummy (phase 1, the scorer, the
 * taunts every 30 s); then the boss attacks */
static PyObject *Ff_CreateBoss(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	static const char *const mods[] = { "Speech", "Vehicles", "VehiclesNPC", "OutMap" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	int first = Sf_EqS(&f, GLOBAL("bossName"), "");
	if (first > 0) {
		PyObject *shield = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "BossShield", 0, 0, 0, "Item");
		Sf_Method(&f, shield, "SetModel", "(s)", "Models/GFX/BossShield/BossShield.m3d");
		Sf_SetAttrI(&f, shield, "Active", 0);
		Sf_SetAttr(&f, shield, "OffModPos", Sf_Build(&f, "(iii)", 0, 0, 0));
		Sf_SetAttrI(&f, shield, "DeltaOffMul", 4);
		Sf_SetAttrI(&f, shield, "OffMul", 0);
		Sf_SetAttrI(&f, shield, "UseLinkAng", 1);
		PyObject *dummy = Lv_Dummy(&f, "DM_Boss_Spawn", 1);
		Sf_SetGlobal(&f, FF, "pos", Sf_Item(&f, dummy, 1));
		PyObject *e = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "SBoss1", "Boss", 0, 0);
		Sf_SetAttr(&f, e, "Pos", GLOBAL("pos"));
		Ff_Profile(&f, e);
		Sf_SetAttrS(&f, e, "WeapList", "0,15,0,0,0,0");
		Sf_SetAttrI(&f, e, "Money", -1);
		Sf_SetAttrS(&f, e, "TgType", "red");
		Sf_SetAttrI(&f, e, "LaunchTaunts", 0);
		PyObject *life = GLOBAL("BOSS_LIFE");
		Sf_SetAttr(&f, e, "Life", life);
		Sf_SetAttr(&f, e, "MaxLife", life);
		Sf_SetAttrS(&f, e, "OnDeath", "MapFinalFight.OnBossDestroyed");
		Sf_SetGlobal(&f, FF, "bossName", Sf_Attr(&f, e, "Name"));
		Sf_SetAttrS(&f, e, "OnDamage", "!FXBossOnDamage");
		Sf_Run(&f, "Scrap", "Set", "(sO)", "LifeToLaunchCallback", GLOBAL("BOSS_LIFE_BULLET"));
		Sf_Run(&f, "Scrap", "Set", "(ss)", "LifeReachedCallback", "MapFinalFight.OnBossRecoversZero");
		Sf_Run(&f, "SLogic", "SetShipToRegenerate", "(OOO)", GLOBAL("bossName"), GLOBAL("BOSS_RegenInit"),
			GLOBAL("BOSS_RegenEnd"));
		Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_FinalFight_RemoteMessage2", "Boss");
		Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 30), GLOBAL("LaunchTaunt"), Sf_Build(&f, "()"), "LaunchTaunt");
		Sf_SetMod(&f, "Menu", "MissionScorerCall", GLOBAL("MissionScorer"));
		Sf_Run(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
		if (Sf_True(&f, shield) > 0) {
			Sf_SetAttrI(&f, shield, "Active", 1);
			Sf_SetAttrS(&f, shield, "LinkModel", ",");
			Sf_SetAttr(&f, shield, "LinkModel", Sf_Cat(&f, "Oss", Sf_Attr(&f, e, "Name"), ",", "DC_Player_01"));
			Sf_Call(&f, GLOBAL("SetShieldPhase"), "()");
		}
	}
	Sf_Call(&f, GLOBAL("BossAttacks"), "()");
	return Sf_None(&f);
}

/* SetShieldPhase(): the shield effect of the phase (FASE) */
static PyObject *Ff_SetShieldPhase(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "SetCallFunc", "(s)", "!FXBossShieldSet");
	Sf_Run(&f, "Scrap", "AddParams", "(s)", "BossShield");
	Sf_Run(&f, "Scrap", "AddParami", "(O)", GLOBAL("FASE"));
	Sf_Run(&f, "Scrap", "Execute", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the text of the phase and the regeneration bar of the boss */
static PyObject *Ff_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Scorer");
	Lv_ScorerTextBegin(&f, id, "BossText", 0);
	Lv_ScorerTextEnd(&f, id, "BossText");
	Lv_ScorerSet(&f, id, "BossText", "Visible", Sf_Int(&f, 1));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "RegenerationBar", "Bar");
	Sf_Run(&f, "Scorer", "MissionScorerBar_SetDefaults", "(OsOOO)", id, "RegenerationBar",
		Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBack"), Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorMain"),
		Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBlack"));
	Lv_ScorerSet(&f, id, "RegenerationBar", "MaxValue", Sf_Float(&f, 1.0));
	Lv_ScorerSet(&f, id, "RegenerationBar", "Value", Sf_Float(&f, 1.0));
	Lv_ScorerSet(&f, id, "RegenerationBar", "Visible", Sf_Int(&f, 1));
	Sf_Run(&f, "Scrap", "Set", "(ss)", "RegenerationItem", "RegenerationBar");
	Sf_Call(&f, GLOBAL("SetFase"), "()");
	return Sf_None(&f);
}

/* SetFase(): the text and the bar colours of the phase, the shield */
static PyObject *Ff_SetFase(PyObject *self, PyObject *args) {
	static const char *const vars[] = { "MainRed", "MainGreen", "MainBlue", "BlackRed", "BlackGreen", "BlackBlue" };
	static const int colors[3][6] = { { 255, 255, 255, 0, 255, 0 }, { 0, 255, 0, 255, 0, 0 }, { 255, 0, 0, 0, 0, 0 } };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *id = Sf_Int(&f, 0);
	PyObject *key = Sf_Cat(&f, "sO", "Mission_FinalFight_Scorer_", Sf_Repr(&f, GLOBAL("FASE")));
	Lv_ScorerSet(&f, id, "BossText", "Text", Sf_Op(&f, Sf_Lang(&f, key), '+', Sf_Str(&f, ":")));
	int phase = 2;
	if (Sf_EqI(&f, GLOBAL("FASE"), 1) > 0) {
		phase = 0;
	} else if (!f.failed && Sf_EqI(&f, GLOBAL("FASE"), 2) > 0) {
		phase = 1;
	}
	for (int i = 0; i < 6 && !f.failed; i++) {
		Lv_ScorerSet(&f, id, "RegenerationBar", vars[i], Sf_Int(&f, colors[phase][i]));
	}
	Sf_Call(&f, GLOBAL("SetShieldPhase"), "()");
	return Sf_None(&f);
}

/* BossAttacks(): the boss attacks the player */
static PyObject *Ff_BossAttacks(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, GLOBAL("bossName"), Sf_Attr(&f, usr, "Name"));
	}
	return Sf_None(&f);
}

/* BossRelax(): the boss goes back to its spawn point */
static PyObject *Ff_BossRelax(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, GLOBAL("bossName"));
	if (Sf_True(&f, e) > 0) {
		PyObject *name = Sf_Attr(&f, e, "Name"), *pos = GLOBAL("pos");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOi)", 1, name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), 1000);
	}
	return Sf_None(&f);
}

/* OnPlayerDeath(LooserName, WinnerName): the boss goes back and heals, the time of the fight counts for the hints,
 * the player respawns where he died */
static PyObject *Ff_OnPlayerDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", looser) > 0) {
		Sf_Call(&f, GLOBAL("BossRelax"), "()");
		PyObject *hints = Sf_ToFloat(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "FinalBossHintsTime"));
		PyObject *t = Sf_Op(&f, hints, '+', Sf_Op(&f, Sf_Time(&f), '-', GLOBAL("time")));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "FinalBossHintsTime", Sf_Repr(&f, t));
		PyObject *e = Sf_Entity(&f, looser);
		PyObject *at = Sf_Op(&f, Sf_Time(&f), '+', GLOBAL("respawnTime"));
		Sf_Schedule(&f, at, GLOBAL("RespawnPlayer"), Sf_Build(&f, "(OOO)", looser, Sf_Attr(&f, e, "Pos"),
			Sf_Attr(&f, e, "Ang")), NULL);
		PyObject *boss = Sf_Ask(&f, "Scrap", "GetEntity", "(s)", "Boss");
		Sf_SetAttr(&f, boss, "Life", Sf_Attr(&f, boss, "MaxLife"));
	}
	return Sf_None(&f);
}

/* Speech.CancelRemoteCom(); Speech.AddRemoteCom(Mission_FinalFight_RemoteMessage<n>, "CrazyGambler") */
static void Ff_Hint(scriptFrame_t *f, int n) {
	char key[0x40];
	Str_Printf(key, sizeof(key), "Mission_FinalFight_RemoteMessage%d", n);
	Sf_Run(f, "Speech", "CancelRemoteCom", "()");
	Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", key, "CrazyGambler");
}

/* RespawnPlayer(Usrname, respawnPos, ang): the player's ship again; every fifth time from the fourth a hint of the
 * phase; the boss attacks */
static PyObject *Ff_RespawnPlayer(PyObject *self, PyObject *args) {
	static const int hints[] = { 5, 13, 21 };
	PyObject *name, *pos, *ang;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &ang)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Speech");
	Sf_Run(&f, "Vehicles", "ReSpawnShip", "(O)", name);
	PyObject *usr = Sf_Entity(&f, name);
	Sf_SetAttr(&f, usr, "Pos", pos);
	Sf_SetAttr(&f, usr, "Ang", ang);
	Sf_SetGlobal(&f, FF, "HINT_COUNTER", Sf_OpI(&f, GLOBAL("HINT_COUNTER"), '+', 1));
	if (Sf_EqI(&f, Sf_OpI(&f, GLOBAL("HINT_COUNTER"), '%', 5), 4) > 0) {
		for (int i = 0; i < 3; i++) {
			int is = Sf_EqI(&f, GLOBAL("FASE"), i + 1);
			if (is > 0) {
				Ff_Hint(&f, hints[i]);
			}
			if (is != 0) {
				break;
			}
		}
	}
	Sf_Call(&f, GLOBAL("BossAttacks"), "()");
	return Sf_None(&f);
}

/* Speech.AddRemoteCom("Mission_FinalFight_RemoteMessage" + `base + mul*(numTaunt % mod)`, "Boss") */
static void Ff_Taunt(scriptFrame_t *f, long base, long mul, long mod) {
	int mark = f->n;
	PyObject *m = Sf_OpI(f, Sf_Global(f, FF, "numTaunt"), '%', mod);
	PyObject *k = Sf_Op(f, Sf_Int(f, base), '+', mul == 1 ? m : Sf_Op(f, Sf_Int(f, mul), '*', m));
	PyObject *key = Sf_Cat(f, "sO", "Mission_FinalFight_RemoteMessage", Sf_Repr(f, k));
	Sf_Run(f, "Speech", "AddRemoteCom", "(Os)", key, "Boss");
	Sf_Release(f, mark);
}

/* LaunchTaunt(): a taunt of the boss every 30 s: by phase, by the hints given and in phase 3 by the boss' life */
static PyObject *Ff_LaunchTaunt(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 30), GLOBAL("LaunchTaunt"), Sf_Build(&f, "()"), "LaunchTaunt");
	for (int phase = 1; phase <= 3 && !f.failed; phase++) {
		int is = Sf_EqI(&f, GLOBAL("FASE"), phase);
		if (is == 0) {
			continue;
		}
		if (is < 0) {
			break;
		}
		int few = Sf_Compare(&f, GLOBAL("HINT_COUNTER"), "<", Sf_Int(&f, 3));
		if (few > 0) {
			Ff_Taunt(&f, phase == 1 ? 2 : phase == 2 ? 10 : 18, 1, 3);
		} else if (few == 0 && phase < 3) {
			Ff_Taunt(&f, phase == 1 ? 6 : 14, 1, 3);
		} else if (few == 0) {
			PyObject *e = Sf_Entity(&f, GLOBAL("bossName"));
			int low = Sf_Compare(&f, Sf_Attr(&f, e, "Life"), "<", Sf_Int(&f, 400));
			if (low >= 0) {
				Ff_Taunt(&f, low ? 22 : 23, 2, 2);
			}
		}
		break;
	}
	Sf_SetGlobal(&f, FF, "numTaunt", Sf_OpI(&f, GLOBAL("numTaunt"), '+', 1));
	return Sf_None(&f);
}

/* the boss after its life reached the mark of a phase (phase 2 or 3): the profile, the counters, the hint of the
 * phase, the weapons and the life, the next mark, the regeneration, the scorer blinks for 2.5 s */
static PyObject *Ff_NextPhase(PyObject *args, int phase) {
	PyObject *name, *idx;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &name, &idx)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	if (phase == 3) {
		Sf_Import(&f, "VehiclesNPC");
	}
	if (Sf_EqI(&f, idx, -1) <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) <= 0) {
		return Sf_None(&f);
	}
	Ff_Profile(&f, e);
	Sf_SetGlobal(&f, FF, "FASE", Sf_Int(&f, phase));
	Sf_SetGlobal(&f, FF, "numTaunt", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, FF, "HINT_COUNTER", Sf_Int(&f, 0));
	Ff_Hint(&f, phase == 2 ? 9 : 17);
	Sf_SetAttrS(&f, e, "WeapList", phase == 2 ? "15,0,0,15,0,0" : "6,9,6,9,6,9");
	Sf_SetAttr(&f, e, "MaxLife", GLOBAL(phase == 2 ? "BOSS_LIFE_BULLET" : "BOSS_LIFE_FINAL"));
	Sf_SetAttr(&f, e, "Life", GLOBAL("BOSS_LIFE_BULLET"));
	if (phase == 3) {
		static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
		for (int i = 0; i < 3; i++) {
			Sf_SetAttr(&f, e, ammo[i], Sf_Ask(&f, "SWeap", "GetFAmmo", "(is)", i, "Max"));
		}
	}
	Sf_SetAttrS(&f, e, "OnDeath", "MapFinalFight.OnBossDestroyed");
	Sf_SetAttrS(&f, e, "OnDamage", "!FXBossOnDamage");
	if (phase == 2) {
		Sf_Run(&f, "Scrap", "Set", "(sO)", "LifeToLaunchCallback", GLOBAL("BOSS_LIFE_FINAL"));
		Sf_Run(&f, "Scrap", "Set", "(ss)", "LifeReachedCallback", "MapFinalFight.OnBossRecoversEnd");
		Sf_Run(&f, "SLogic", "SetShipToRegenerate", "(OOO)", GLOBAL("bossName"), GLOBAL("BOSS_Last_RegenInit"),
			GLOBAL("BOSS_Last_RegenEnd"));
	} else {
		Sf_Run(&f, "Scrap", "Set", "(si)", "LifeToLaunchCallback", 0);
		Sf_Run(&f, "Scrap", "Set", "(ss)", "LifeReachedCallback", "");
		Sf_Run(&f, "SLogic", "SetShipToRegenerate", "(sdd)", "Boss", 0.05, 0.05);
	}
	Sf_Call(&f, GLOBAL("SetFase"), "()");
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "RegenerationBar", "Blink", 1);
	Sf_Later(&f, Sf_Float(&f, 2.5), Sf_Mod(&f, "SScorer", "Set"), Sf_Build(&f, "(issi)", 0, "RegenerationBar", "Blink", 0),
		NULL);
	return Sf_None(&f);
}

/* OnBossRecoversZero(name, idx), OnBossRecoversEnd(name, idx): the engine callback of the life mark (idx -1) */
static PyObject *Ff_OnBossRecoversZero(PyObject *self, PyObject *args) {
	(void)self;
	return Ff_NextPhase(args, 2);
}

static PyObject *Ff_OnBossRecoversEnd(PyObject *self, PyObject *args) {
	(void)self;
	return Ff_NextPhase(args, 3);
}

/* OnBossDestroyed(LooserName, WinnerName): the death of the boss: its death taunt, no more taunts, the alarm falls,
 * a way point, PreLoadFinal in 4 s */
static PyObject *Ff_OnBossDestroyed(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "SaveGame");
	Sf_SetMod(&f, "Vehicles", "ForceDeathTaunt", Sf_Int(&f, 1));
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "LaunchTaunt");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "FinalBossHintsTime");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "BossDestroyed", "1");
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 4), GLOBAL("PreLoadFinal"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

/* PreLoadFinal(): no input, no scorer, fade to black, LoadFinal in 4 s */
static PyObject *Ff_PreLoadFinal(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Run(&f, "Menu", "DrawScorer", "(is)", 0, "Inactive");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "");
	Sf_Run(&f, "SScorer", "SetCinema", "(iii)", 0, 0, 0);
	Lv_Fade(&f, lv_clear, lv_black, 4.0);
	Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 4), GLOBAL("LoadFinal"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

PyMethodDef g_ABFinalFightMethods[] = {
	{ "GetBossProfile", Ff_GetBossProfile, 1, NULL },
	{ "FakeShips", Ff_FakeShips, 1, NULL },
	{ "InitStatus", Ff_InitStatus, 1, NULL },
	{ "Init", Ff_Init, 1, NULL },
	{ "CreateBoss", Ff_CreateBoss, 1, NULL },
	{ "SetShieldPhase", Ff_SetShieldPhase, 1, NULL },
	{ "MissionScorer", Ff_MissionScorer, 1, NULL },
	{ "SetFase", Ff_SetFase, 1, NULL },
	{ "BossAttacks", Ff_BossAttacks, 1, NULL },
	{ "BossRelax", Ff_BossRelax, 1, NULL },
	{ "OnPlayerDeath", Ff_OnPlayerDeath, 1, NULL },
	{ "RespawnPlayer", Ff_RespawnPlayer, 1, NULL },
	{ "LaunchTaunt", Ff_LaunchTaunt, 1, NULL },
	{ "OnBossRecoversZero", Ff_OnBossRecoversZero, 1, NULL },
	{ "OnBossRecoversEnd", Ff_OnBossRecoversEnd, 1, NULL },
	{ "OnBossDestroyed", Ff_OnBossDestroyed, 1, NULL },
	{ "PreLoadFinal", Ff_PreLoadFinal, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
