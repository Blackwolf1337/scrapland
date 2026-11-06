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
 */
#include <limits.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

#define RM "RaceMaker"

static const char race_def[] = {
#embed "../../../data/menus/race.def"
	, 0 };

/* a function of the module as its functions call it, with Py_BuildValue arguments */
#define Rm_Call(f, func, ...) Sf_Call(f, Sf_Global(f, RM, func), __VA_ARGS__)
/* a global of the module */
#define Rm_G(f, name) Sf_Global(f, RM, name)

static const char *const rm_mercs[] = { "MercenaryA", "MercenaryB", "MercenaryC", NULL };

/* Scrap.GetSaveVar(name) */
static PyObject *Rm_Save(scriptFrame_t *f, const char *name) {
	return Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", name);
}

/* Scrap.GetSaveVar("HumphreyRaceTutorial") != "1" */
static int Rm_NoTutorial(scriptFrame_t *f) {
	return Sf_Compare(f, Rm_Save(f, "HumphreyRaceTutorial"), "!=", Sf_Str(f, "1"));
}

/* ShowSystemMessage(string.replace(Scrap.GetLangStr(key), "<n>", Scrap.GetSaveVar("Challenge.Money"))) */
static void Rm_MoneyMsg(scriptFrame_t *f, const char *key) {
	PyObject *fn = Rm_G(f, "ShowSystemMessage");
	PyObject *text = Sf_Lang(f, Sf_Str(f, key));
	text = Sf_Ask(f, "string", "replace", "(OsO)", text, "<n>", Rm_Save(f, "Challenge.Money"));
	Sf_Call(f, fn, "(O)", text);
}

/* Speech.AddRemoteCom(prefix + "Mercs" or prefix + ChallengeFoeType, ChallengeFoeType) */
static void Rm_FoeCom(scriptFrame_t *f, const char *prefix) {
	int merc = Sf_InS(f, Rm_G(f, "ChallengeFoeType"), rm_mercs);
	if (merc < 0) {
		return;
	}
	PyObject *foe = Rm_G(f, "ChallengeFoeType");
	PyObject *key = merc ? Sf_Cat(f, "ss", prefix, "Mercs") : Sf_Cat(f, "sO", prefix, foe);
	Sf_Ask(f, "Speech", "AddRemoteCom", "(OO)", key, Rm_G(f, "ChallengeFoeType"));
}

/* the revenge of a race: Revenge.Type Race, the foe, the last winner, the number of enemies */
static void Rm_Revenge(scriptFrame_t *f, const char *winner) {
	Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", "Revenge.Type", "Race");
	Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", "Revenge.Foe", Rm_G(f, "ChallengeFoeType"));
	Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", "Revenge.LastWinner", winner);
	Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", "Revenge.NumEnemies", Rm_Save(f, "Challenge.NumEnemies"));
}

/* the hook of the module when it is set (OnPlayerLoosesRace(), OnPlayerWinsRace()) */
static void Rm_Hook(scriptFrame_t *f, const char *name) {
	PyObject *fn = Rm_G(f, name);
	if (Sf_True(f, fn) > 0) {
		Sf_Call(f, fn, "()");
	}
}

/* 2 * int(Scrap.GetSaveVar("Challenge.Money")) */
static PyObject *Rm_Double(scriptFrame_t *f) {
	return Sf_Op(f, Sf_Int(f, 2), '*', Sf_ToInt(f, Rm_Save(f, "Challenge.Money")));
}

/* the player wins lives against the mercenaries: twice the bet, up to MaxPlayerLives */
static void Rm_WinLives(scriptFrame_t *f) {
	PyObject *lives = Rm_Double(f);
	PyObject *nl = Sf_Op(f, Sf_ToInt(f, Rm_Save(f, "Player.NumLives")), '+', lives);
	PyObject *max = Sf_Ask(f, "Scrap", "Get", "(s)", "MaxPlayerLives");
	if (Sf_Compare(f, nl, ">", max) > 0) {
		nl = max;
	}
	Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", "Player.NumLives", Sf_Repr(f, nl));
	Sf_Ask(f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(f, "SInput", "GetActionSet", "()"));
	Rm_MoneyMsg(f, "MercsSystemWin");
}

/* Scrap.SetMoney(Scrap.GetMoney() + money); SaveGame.SaveMoney() */
static void Rm_AddMoney(scriptFrame_t *f, PyObject *money) {
	Sf_Ask(f, "Scrap", "SetMoney", "(O)", Sf_Op(f, Sf_Ask(f, "Scrap", "GetMoney", "()"), '+', money));
	Sf_Ask(f, "SaveGame", "SaveMoney", "()");
}

/* ShowSystemMessage(cad): a system message for the time it takes to read */
static PyObject *RaceMaker_ShowSystemMessage(PyObject *self, PyObject *args) {
	PyObject *cad;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cad)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *t = Sf_Time(&f);
	Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)",
		cad)));
	return Sf_None(&f);
}

/* ShowSystemMessageDelay(cad, delay) */
static PyObject *RaceMaker_ShowSystemMessageDelay(PyObject *self, PyObject *args) {
	PyObject *cad, *delay;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &cad, &delay)) {
		return NULL;
	}
	Sf_Later(&f, delay, Rm_G(&f, "ShowSystemMessage"), Sf_Build(&f, "(O)", cad), NULL);
	return Sf_None(&f);
}

/* NPCCombatKilled(LooserName, WinnerName): the OnDeath of the enemies of a combat; when the last one dies the player
 * wins lives (the mercenaries) or twice the bet, the revenge is set and the challenge ends */
static PyObject *RaceMaker_NPCCombatKilled(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Challenge");
	if (Sf_In(&f, looser, Rm_G(&f, "challenged_NPCs_Names")) > 0) {
		Sf_Method(&f, Rm_G(&f, "challenged_NPCs_Names"), "remove", "(O)", looser);
	}
	if (Sf_Len(&f, Rm_G(&f, "challenged_NPCs_Names")) != 0) {
		return Sf_None(&f);
	}
	PyObject *group = Sf_GetItem(&f, Sf_Mod(&f, "Challenge", "groups"), Rm_G(&f, "ChallengeFoeType"));
	int mercs = Sf_EqS(&f, group, "Mercs");
	if (mercs > 0) {
		Rm_WinLives(&f);
	} else if (mercs == 0) {
		Rm_AddMoney(&f, Rm_Double(&f));
		Rm_MoneyMsg(&f, "RaceSystemWin");
	}
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(O)", Sf_Cat(&f, "sO", "Stats.Combat.", group));
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(O)", Sf_Cat(&f, "sO", "Stats.TotalCombats.", group));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Revenge.Type", "Combat");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Revenge.Foe", Rm_G(&f, "ChallengeFoeType"));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Revenge.NumEnemies", Rm_Save(&f, "Challenge.NumEnemies"));
	Sf_Ask(&f, "Challenge", "CleanChallenge", "()");
	Sf_Ask(&f, "Challenge", "SetRevenge", "(i)", 0);
	return Sf_None(&f);
}

/* the entity named by the engine variable CurrentWaypointName (plus suffix) without its OnTake, deleted */
static void Rm_DelWaypoint(scriptFrame_t *f, const char *suffix) {
	PyObject *name = Sf_Ask(f, "Scrap", "Get", "(s)", "CurrentWaypointName");
	PyObject *cw = Sf_Entity(f, suffix ? Sf_Cat(f, "Os", name, suffix) : name);
	if (Sf_True(f, cw) > 0) {
		Sf_SetAttrS(f, cw, "OnTake", "");
		Sf_Method(f, cw, "Del", "()");
	}
}

/* DeleteCurrentWaypoint(): the checkpoint and its trigger go */
static PyObject *RaceMaker_DeleteCurrentWaypoint(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Rm_DelWaypoint(&f, NULL);
	Rm_DelWaypoint(&f, "_Trigg");
	return Sf_None(&f);
}

/* MovePlayersToTraffic(): the rivals join the traffic (police ships the cops) */
static PyObject *RaceMaker_MovePlayersToTraffic(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *players = Rm_G(&f, "Players");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, players, i);
		if (!name) {
			break;
		}
		PyObject *veh = Sf_Entity(&f, name);
		if (Sf_True(&f, veh) > 0) {
			Sf_SetAttr(&f, veh, "MoveTime", Sf_Float(&f, 0.0));
			Sf_SetAttrS(&f, veh, "TgType", "None");
			PyObject *at = Sf_Ask(&f, "string", "find", "(Os)", Sf_Attr(&f, veh, "Model"), "SPoli");
			int cop = Sf_Compare(&f, at, ">=", Sf_Int(&f, 0));
			if (cop >= 0) {
				Sf_SetAttrS(&f, veh, "EntityList", cop ? "OutCop" : "Traffic");
			}
			Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 3, Sf_Attr(&f, veh, "Name"));
		}
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "DoResetOutPolice", 1);
	return Sf_None(&f);
}

/* EndOfRace(ItemName, VehicleName): the end of a race; VehicleName is the winner, or "Alarm", "Killed",
 * "CounterMeasure", "Left" (or None for no result). EndOfRaceCallback replaces the result */
static PyObject *RaceMaker_EndOfRace(PyObject *self, PyObject *args) {
	static const char *const cancels[] = { "Alarm", "Killed", "CounterMeasure", NULL };
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	static const char *const mods[] = { "Speech", "Parking", "Alarm", "Menu", "SaveGame", "Challenge", "string" };
	for (int i = 0; i < 7; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Entity(&f, item);
	Sf_Ask(&f, "SInput", "Bind", "(isss)", 0, "Racer", "CM", "");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "isActionMusic", 0);
	PyObject *cb = Rm_G(&f, "EndOfRaceCallback");
	int t = Sf_True(&f, cb);
	if (t > 0) {
		Sf_Call(&f, cb, "(OO)", item, veh);
	} else if (t == 0 && Sf_Compare(&f, veh, "!=", Py_None) > 0) {
		PyObject *group = Sf_GetItem(&f, Sf_Mod(&f, "Challenge", "groups"), Rm_G(&f, "ChallengeFoeType"));
		int cancel = Sf_InS(&f, veh, cancels);
		int left = cancel == 0 ? Sf_EqS(&f, veh, "Left") : 0;
		if (cancel > 0) {
			int mercs = Sf_EqS(&f, group, "Mercs");
			if (mercs > 0) {
				Rm_MoneyMsg(&f, "MercsSystemCancel1");
			} else if (mercs == 0 && Rm_NoTutorial(&f) > 0) {
				Rm_MoneyMsg(&f, "RaceSystemCancel1");
			}
			Rm_FoeCom(&f, "RaceInterrupted");
			Rm_Revenge(&f, "Foe");
			Sf_Ask(&f, "Challenge", "SetRevenge", "()");
			Rm_Hook(&f, "OnPlayerLoosesRace");
		} else if (left > 0) {
			if (Rm_NoTutorial(&f) > 0) {
				Rm_Call(&f, "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "RaceSystemCancel2")));
			}
			Rm_FoeCom(&f, "RaceLeft");
			Rm_Revenge(&f, "Foe");
			Sf_Ask(&f, "Challenge", "SetRevenge", "()");
			Rm_AddMoney(&f, Sf_ToInt(&f, Rm_Save(&f, "Challenge.Money")));
			PyObject *npcs = Rm_G(&f, "challenged_NPCs_Names");
			for (int i = 0;; i++) {
				int mark = f.n;
				PyObject *name = Sf_Next(&f, npcs, i);
				if (!name) {
					break;
				}
				PyObject *v = Sf_Entity(&f, name);
				if (Sf_True(&f, v) > 0) {
					int police = Sf_EqS(&f, Rm_G(&f, "ChallengeFoeType"), "Police");
					if (police >= 0) {
						Sf_SetAttrS(&f, v, "EntityList", police ? "OutCop" : "Traffic");
						Sf_SetAttrS(&f, v, "TgType", police ? "green" : "None");
					}
				}
				Sf_Release(&f, mark);
			}
			Rm_Hook(&f, "OnPlayerLoosesRace");
		} else if (left == 0) {
			int player = Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, veh), "UsrControl"), 0);
			if (player > 0) {
				PyObject *money = Rm_Double(&f);
				PyObject *npcs = Rm_G(&f, "challenged_NPCs_Names");
				for (int i = 0;; i++) {
					int mark = f.n;
					PyObject *name = Sf_Next(&f, npcs, i);
					if (!name) {
						break;
					}
					PyObject *v = Sf_Entity(&f, name);
					if (Sf_True(&f, v) > 0) {
						Sf_SetAttr(&f, v, "Money", Sf_Op(&f, Sf_Attr(&f, v, "Money"), '-', money));
					}
					Sf_Release(&f, mark);
				}
				int mercs = Sf_EqS(&f, group, "Mercs");
				if (mercs > 0) {
					Rm_WinLives(&f);
				} else if (mercs == 0) {
					Rm_AddMoney(&f, money);
					if (Rm_NoTutorial(&f) > 0) {
						Rm_MoneyMsg(&f, "RaceSystemWin");
					}
				}
				if (Rm_NoTutorial(&f) > 0) {
					Rm_Revenge(&f, "Player");
					Sf_Ask(&f, "Challenge", "SetRevenge", "(i)", 0);
				}
				group = Sf_GetItem(&f, Sf_Mod(&f, "Challenge", "groups"), Rm_G(&f, "ChallengeFoeType"));
				Sf_Ask(&f, "Scrap", "IncSaveVar", "(O)", Sf_Cat(&f, "sO", "Stats.Race.", group));
				Sf_Ask(&f, "Scrap", "IncSaveVar", "(O)", Sf_Cat(&f, "sO", "Stats.TotalRaces.", group));
				Rm_Hook(&f, "OnPlayerWinsRace");
			} else if (player == 0) {
				int before = Sf_EqS(&f, Rm_Save(&f, "Revenge.Type"), "");
				if (before > 0) {
					before = Sf_EqS(&f, Rm_Save(&f, "Revenge.LastWinner"), "Player");
				}
				if (before > 0) {
					int mercs = Sf_EqS(&f, group, "Mercs");
					if (mercs > 0) {
						Rm_MoneyMsg(&f, "MercsSystemLoose");
					} else if (mercs == 0 && Rm_NoTutorial(&f) > 0) {
						Rm_MoneyMsg(&f, "RaceSystemLoose");
					}
				}
				PyObject *key = Sf_Cat(&f, "sO", "RaceLoose", Rm_G(&f, "ChallengeFoeType"));
				Sf_Ask(&f, "Speech", "AddRemoteCom", "(OO)", key, Rm_G(&f, "ChallengeFoeType"));
				Rm_Revenge(&f, "Foe");
				Sf_Ask(&f, "Challenge", "SetRevenge", "()");
				Rm_Hook(&f, "OnPlayerLoosesRace");
			}
		}
	}
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Rm_Call(&f, "DeleteCurrentWaypoint", "()");
	Rm_Call(&f, "MovePlayersToTraffic", "()");
	Sf_SetGlobal(&f, RM, "challenged_NPCs_Names", Sf_Keep(&f, PyList_New(0)));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	Sf_SetMod(&f, "Alarm", "AbortMissionCallback", Py_None);
	Sf_Import(&f, "Challenge");
	Sf_Ask(&f, "Challenge", "CleanChallenge", "()");
	return Sf_None(&f);
}

/* RaceStart(ItemName, VehicleName): the race starts at the first checkpoint: the checkpoint counts from now on, the
 * race of the AI, the rivals become targets, the counter measures are watched */
static PyObject *RaceMaker_RaceStart(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_SetAttrS(&f, Sf_Entity(&f, item), "OnTake", "RaceMaker.OnCheckPoint");
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Cat(&f, "Os", item, "_Trigg")), "OnTake", "RaceMaker.OnNearCheckPoint");
	Sf_Ask(&f, "SAI", "InitVehicleRace", "()");
	Sf_Ask(&f, "SAI", "AddVehicleRace", "(O)", Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Name"));
	Sf_SetGlobal(&f, RM, "YourScore", Sf_Int(&f, 0));
	Rm_FoeCom(&f, "RaceStart");
	PyObject *players = Rm_G(&f, "Players");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, players, i);
		if (!name) {
			break;
		}
		Sf_Ask(&f, "SAI", "AddVehicleRace", "(O)", name);
		PyObject *v = Sf_Entity(&f, name);
		Sf_SetAttrS(&f, v, "TgType", "Green");
		Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.0));
		Sf_Release(&f, mark);
	}
	Sf_Import(&f, "Menu");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	Sf_Ask(&f, "SInput", "Bind", "(isss)", 0, "Racer", "CM", "RaceMaker.OnCounterMeasure");
	Sf_SetGlobal(&f, RM, "LastCmTime", Sf_Int(&f, -1));
	return Sf_None(&f);
}

/* OnCounterMeasure(id=0): the counter measure key during a race: the rival protests, the second time after 5
 * seconds ends the race */
static PyObject *RaceMaker_OnCounterMeasure(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int none = Sf_Compare(&f, usr, "==", Py_None);
	if (none == 0) {
		none = Sf_EqI(&f, Sf_Attr(&f, usr, "iWeap"), 6);
	}
	if (none != 0) {
		return Sf_None(&f);
	}
	PyObject *speakers = Rm_G(&f, "RaceSpeachers");
	PyObject *cft;
	if (Sf_True(&f, Sf_Method(&f, speakers, "has_key", "(O)", Rm_G(&f, "ChallengeFoeType"))) > 0) {
		cft = Sf_GetItem(&f, Rm_G(&f, "RaceSpeachers"), Rm_G(&f, "ChallengeFoeType"));
	} else {
		cft = Rm_G(&f, "ChallengeFoeType");
	}
	PyObject *last = Rm_G(&f, "LastCmTime");
	if (Sf_EqI(&f, last, -1) > 0) {
		Sf_SetGlobal(&f, RM, "LastCmTime", Sf_Time(&f));
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(OO)", Sf_Cat(&f, "sOs", "Generics_", cft, "Race_1"), cft);
	} else if (!f.failed) {
		PyObject *t = Sf_Op(&f, Rm_G(&f, "LastCmTime"), '+', Sf_Float(&f, 5.0));
		if (Sf_Compare(&f, t, "<", Sf_Time(&f)) > 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(OO)", Sf_Cat(&f, "sOs", "Generics_", cft, "Race_2"), cft);
			Rm_Call(&f, "AbortCameraTravelling", "()");
			Rm_Call(&f, "EndOfRace", "(ss)", "CheckPoint", "CounterMeasure");
		}
	}
	return Sf_None(&f);
}

/* the travelling ends, then EndOfRace("CheckPoint", why) */
static PyObject *Rm_Stop(PyObject *args, const char *fmt, const char *why, byte alarm) {
	PyObject *a = NULL, *b = NULL, *c = NULL;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, (char *)fmt, &a, &b, &c)) {
		return NULL;
	}
	Rm_Call(&f, "AbortCameraTravelling", "()");
	Rm_Call(&f, "EndOfRace", "(ss)", "CheckPoint", why);
	if (alarm) {
		Sf_Ask(&f, "Scrap", "SetAlarm", "(i)", 1);
	}
	return Sf_None(&f);
}

/* RacerKilled(racerKilled, attacker): a racer died: the race ends and the alarm starts */
static PyObject *RaceMaker_RacerKilled(PyObject *self, PyObject *args) {
	(void)self;
	return Rm_Stop(args, "OO", "Killed", 1);
}

/* RacerInterruptedByAlarm() */
static PyObject *RaceMaker_RacerInterruptedByAlarm(PyObject *self, PyObject *args) {
	(void)self;
	return Rm_Stop(args, "", "Alarm", 0);
}

/* RacerDamaged(racerDamaged, attacker, damageDone): the first hit on a racer counts as its checkpoint, once */
static PyObject *RaceMaker_RacerDamaged(PyObject *self, PyObject *args) {
	PyObject *a, *b, *c;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &a, &b, &c)) {
		return NULL;
	}
	Rm_Call(&f, "AbortCameraTravelling", "()");
	PyObject *fn = Rm_G(&f, "OnCheckPoint");
	PyObject *wp = Sf_Ask(&f, "Scrap", "Get", "(s)", "CurrentWaypointName");
	Sf_Call(&f, fn, "(OO)", wp, Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Race", "First"));
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "");
	return Sf_None(&f);
}

/* RacerLeft(): the time to get into a ship ran out */
static PyObject *RaceMaker_RacerLeft(PyObject *self, PyObject *args) {
	(void)self;
	return Rm_Stop(args, "", "Left", 0);
}

/* AbortCameraTravelling(): the end of the travelling of StartCameraTravelling: the player's ship, the camera and the
 * HUD back */
static PyObject *RaceMaker_AbortCameraTravelling(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	if (Sf_True(&f, Rm_G(&f, "prevCamera")) <= 0) {
		return Sf_None(&f);
	}
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "MissionsFuncs.ShowMissionInfo");
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
	PyObject *usr = Sf_Entity(&f, Rm_G(&f, "cameraVehicleName"));
	Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 0.0));
	Sf_SetAttrI(&f, usr, "UsrControl", 0);
	PyObject *l = Sf_Entity(&f, Rm_G(&f, "cameraItemName"));
	Sf_SetAttrI(&f, l, "rTime", 0);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	Sf_Method(&f, cam, "Del", "()");
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Rm_G(&f, "prevCamera"));
	if (Sf_True(&f, Rm_G(&f, "prevCamera")) > 0) {
		Sf_Ask(&f, "RacerScorer", "Create", "(i)", 0);
	}
	Sf_SetGlobal(&f, RM, "cameraItemName", Py_None);
	Sf_SetGlobal(&f, RM, "cameraVehicleName", Py_None);
	Sf_SetGlobal(&f, RM, "prevCamera", Py_None);
	return Sf_None(&f);
}

/* the angle a, b, c of a camera point with the pitch a quartered */
static PyObject *Rm_Quarter(scriptFrame_t *f, PyObject *ang) {
	PyObject *a = Sf_Op(f, Sf_Item(f, ang, 0), '/', Sf_Float(f, 4.0));
	return Sf_Build(f, "(OOO)", a, Sf_Item(f, ang, 1), Sf_Item(f, ang, 2));
}

/* StartCameraTravelling(ItemName, VehicleName, isRace=1, speed="Normal"): the player's ship flies to the item and a
 * camera path shows the checkpoint (isRace) or the item; it ends by itself; the time of the travelling (None when
 * the vehicle is not the player's) */
static PyObject *RaceMaker_StartCameraTravelling(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "ItemName", "VehicleName", "isRace", "speed", NULL };
	PyObject *item, *veh, *race = NULL, *speed = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OO", names, &item, &veh, &race, &speed)) {
		return NULL;
	}
	race = race ? race : Sf_Int(&f, 1);
	speed = speed ? speed : Sf_Str(&f, "Normal");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "math");
	Sf_SetGlobal(&f, RM, "cameraItemName", item);
	Sf_SetGlobal(&f, RM, "cameraVehicleName", veh);
	PyObject *usr = Sf_Entity(&f, veh);
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "UsrControl"), "!=", Sf_Int(&f, 0)) != 0) {
		return Sf_None(&f);
	}
	PyObject *l = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, l, "rTime", 3600);
	Sf_SetAttrS(&f, l, "OnTake", "");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_SetAttrI(&f, usr, "UsrControl", -1);
	Sf_SetAttr(&f, usr, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttr(&f, usr, "MovePos", Sf_Attr(&f, l, "Pos"));
	Sf_SetAttr(&f, usr, "MoveAng", Sf_Attr(&f, l, "Ang"));
	Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, usr, "MoveFade", 2);
	Sf_SetAttrI(&f, usr, "AvoidCrash", 1);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	Sf_SetGlobal(&f, RM, "prevCamera", Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	PyObject *wayp;
	if (Sf_EqI(&f, race, 1) > 0) {
		wayp = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "CurrentWaypointName"));
	} else {
		wayp = Sf_Entity(&f, item);
	}
	PyObject *ang = Sf_Ask(&f, "SVec", "Add", "(OO)", Sf_Attr(&f, wayp, "Ang"), Sf_Build(&f, "(iii)", 15, 180, 0));
	PyObject *p[3];
	for (int i = 0; i < 3; i++) {
		PyObject *back = i == 0 ? Sf_Int(&f, -5000) : i == 1 ? Sf_Int(&f, -4000) : f.failed ? NULL :
			Sf_Keep(&f, PyNumber_Negative(Sf_Ask(&f, "Scrap", "Get", "(s)", "VehicleCamDist2")));
		PyObject *off = Sf_Ask(&f, "SVec", "Rotate3D", "(OO)", Sf_Build(&f, "(iiO)", 0, 0, back), ang);
		p[i] = Sf_Ask(&f, "SVec", "Add", "(OO)", off, Sf_Attr(&f, wayp, "Pos"));
	}
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "RaceCamera", Sf_Item(&f, p[0], 0),
		Sf_Item(&f, p[0], 1), Sf_Item(&f, p[0], 2), "CamPath");
	Sf_SetAttrI(&f, cam, "nWayPoint", 3);
	int normal = Sf_EqS(&f, speed, "Normal");
	int slow = normal == 0 ? Sf_EqS(&f, speed, "Slow") : 0;
	for (int i = 0; i < 3; i++) {
		char var[16];
		Str_Printf(var, sizeof(var), "TgtCam0%d", i);
		Sf_SetAttr(&f, cam, var, Rm_G(&f, "prevCamera"));
		Sf_SetAttrS(&f, cam, var, "");
		if (i == 2) {
			break;
		}
		Str_Printf(var, sizeof(var), "Pos0%d", i);
		Sf_SetAttr(&f, cam, var, p[i]);
		Str_Printf(var, sizeof(var), "Ang0%d", i);
		Sf_SetAttr(&f, cam, var, i == 0 ? ang : Rm_Quarter(&f, ang));
		Str_Printf(var, sizeof(var), "FadeIn0%d", i);
		Sf_SetAttrI(&f, cam, var, i == 0);
		Str_Printf(var, sizeof(var), "FadeOut0%d", i);
		Sf_SetAttrI(&f, cam, var, i == 1);
		Str_Printf(var, sizeof(var), "Time0%d", i);
		if (normal > 0) {
			Sf_SetAttr(&f, cam, var, Sf_Float(&f, i == 0 ? 1.0 : 3.0));
		} else if (slow > 0) {
			Sf_SetAttr(&f, cam, var, Sf_Float(&f, i == 0 ? 2.0 : 6.0));
		}
	}
	Sf_SetAttr(&f, cam, "Ang02", Rm_Quarter(&f, ang));
	Sf_SetAttr(&f, cam, "Pos02", p[2]);
	Sf_SetAttrI(&f, cam, "Time", 0);
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cam, "Name"));
	PyObject *time = Sf_Op(&f, Sf_Attr(&f, cam, "Time00"), '+', Sf_Attr(&f, cam, "Time01"));
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "");
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	PyObject *t = Sf_Op(&f, Sf_Op(&f, Sf_Time(&f), '+', time), '+', Sf_Float(&f, 0.2));
	Sf_Schedule(&f, t, Rm_G(&f, "AbortCameraTravelling"), Sf_Build(&f, "()"), NULL);
	return Sf_Return(&f, time);
}

/* OnRaceStart(ItemName, VehicleName): the player took the start checkpoint: the travelling, then RaceGo; the
 * challenger's start item goes, the count down and the action music */
static PyObject *RaceMaker_OnRaceStart(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	PyObject *user = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int go = Sf_True(&f, user);
	if (go > 0) {
		go = Sf_Compare(&f, Sf_Attr(&f, user, "Name"), "==", veh);
	}
	if (go <= 0) {
		return Sf_None(&f);
	}
	PyObject *t = Rm_Call(&f, "StartCameraTravelling", "(OO)", item, veh);
	t = Sf_Op(&f, Sf_Time(&f), '+', t);
	Sf_Schedule(&f, t, Rm_G(&f, "RaceGo"), Sf_Build(&f, "(OO)", item, veh), NULL);
	PyObject *npc = Rm_G(&f, "NPC_START_ITEM");
	if (Sf_Compare(&f, npc, "!=", Py_None) > 0) {
		Sf_Method(&f, Rm_G(&f, "NPC_START_ITEM"), "Del", "()");
		Sf_SetGlobal(&f, RM, "NPC_START_ITEM", Py_None);
	}
	Rm_Call(&f, "CreateRaceCountDown", "()");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "isActionMusic", 1);
	return Sf_None(&f);
}

/* RaceGo(ItemName, VehicleName): the end of the count down: OnRaceGoCallback, the race starts at the checkpoint */
static PyObject *RaceMaker_RaceGo(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	if (Sf_True(&f, Rm_G(&f, "prevCamera")) > 0) {
		PyObject *cb = Rm_G(&f, "OnRaceGoCallback");
		if (Sf_True(&f, cb) > 0) {
			Sf_Call(&f, cb, "()");
			Sf_SetGlobal(&f, RM, "OnRaceGoCallback", Py_None);
		}
		Rm_Call(&f, "AbortCameraTravelling", "()");
		Rm_Call(&f, "OnCheckPoint", "(OO)", item, veh);
		Rm_Call(&f, "DestroyRaceCountDown", "()");
	}
	return Sf_None(&f);
}

/* SVec.Add(StartPos[0][0], SVec.Rotate3D(off, (0, StartPos[0][1], 0))): a place beside the start checkpoint */
static PyObject *Rm_BesideStart(scriptFrame_t *f, int x, int y, int z) {
	PyObject *start = Sf_Item(f, Rm_G(f, "StartPos"), 0);
	PyObject *pos = Sf_Item(f, start, 0);
	PyObject *ang = Sf_Build(f, "(iOi)", 0, Sf_Item(f, Sf_Item(f, Rm_G(f, "StartPos"), 0), 1), 0);
	return Sf_Ask(f, "SVec", "Add", "(OO)", pos, Sf_Ask(f, "SVec", "Rotate3D", "(OO)", Sf_Build(f, "(iii)", x, y, z),
		ang));
}

/* StartPos[0][1] + add */
static PyObject *Rm_StartAng(scriptFrame_t *f, int add) {
	PyObject *a = Sf_Item(f, Sf_Item(f, Rm_G(f, "StartPos"), 0), 1);
	return add ? Sf_OpI(f, a, '+', add) : a;
}

/* CreateRaceCountDown(): the two traffic lights beside the start with their light effect, and the red, yellow and
 * green flashes and beeps */
static PyObject *RaceMaker_CreateRaceCountDown(PyObject *self, PyObject *args) {
	static const int fades[3][4] = { { 255, 0, 0, 64 }, { 255, 255, 0, 64 }, { 0, 255, 0, 64 } };
	static const double fadetimes[3] = { 1.5, 1.0, 1.5 };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (int side = 0; side < 2 && !f.failed; side++) {
		const char *name = side == 0 ? "RaceCountDownR" : "RaceCountDownL";
		PyObject *vec = Rm_BesideStart(&f, side == 0 ? -3300 : 3300, 1100, -4000);
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", name, Sf_Item(&f, vec, 0), Sf_Item(&f, vec, 1),
			Sf_Item(&f, vec, 2), "Obj");
		Sf_Method(&f, l, "SetModel", "(s)", "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d");
		Sf_Method(&f, l, "SetFX", "(s)", "MeetingPoint-Race.m3d");
		Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Rm_StartAng(&f, side == 0 ? 0 : 180), 0));
		Sf_Ask(&f, "Scrap", "SetCallFunc", "(s)", "!FXMeetingPointCountDown");
		Sf_Ask(&f, "Scrap", "AddParams", "(s)", name);
		Sf_Ask(&f, "Scrap", "AddParamf", "(d)", 1.9);
		Sf_Ask(&f, "Scrap", "AddParami", "(i)", side == 0 ? 1 : -1);
		Sf_Ask(&f, "Scrap", "Execute", "()");
	}
	for (int i = 0; i < 3; i++) {
		PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 1.9 * i));
		Sf_Schedule(&f, t, Sf_Mod(&f, "SScorer", "Fade"), Sf_Build(&f, "(i(iiii)(iiii)d)", 0, fades[i][0], fades[i][1],
			fades[i][2], fades[i][3], 0, 0, 0, 0, fadetimes[i]), NULL);
		t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 1.9 * i));
		PyObject *fn = Sf_Mod(&f, "SSound", "Play");
		PyObject *snd = Rm_G(&f, i < 2 ? "RaceCountDownSound1" : "RaceCountDownSound2");
		PyObject *vol = Rm_G(&f, i < 2 ? "RaceCountDownSound1Vol" : "RaceCountDownSound2Vol");
		Sf_Schedule(&f, t, fn, Sf_Build(&f, "(OO)", snd, vol), NULL);
	}
	return Sf_None(&f);
}

/* DestroyRaceCountDown(): the traffic lights move away and go */
static PyObject *RaceMaker_DestroyRaceCountDown(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (int side = 0; side < 2; side++) {
		const char *name = side == 0 ? "RaceCountDownR" : "RaceCountDownL";
		PyObject *e = Sf_Entity(&f, Sf_Str(&f, name));
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttr(&f, e, "MovePos", Rm_BesideStart(&f, side == 0 ? -10000 : 10000, 1000, -4500));
			Sf_SetAttr(&f, e, "MoveAng", Sf_Build(&f, "(iOi)", 0, Rm_StartAng(&f, 90), 0));
			Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.5));
			PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 0.5));
			Sf_Schedule(&f, t, Rm_G(&f, "DestroyRaceCountDownEnd"), Sf_Build(&f, "(s)", name), NULL);
		}
	}
	return Sf_None(&f);
}

/* DestroyRaceCountDownEnd(EntityName) */
static PyObject *RaceMaker_DestroyRaceCountDownEnd(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *l = Sf_Entity(&f, name);
	if (Sf_True(&f, l) > 0) {
		Sf_Method(&f, l, "Del", "()");
	}
	return Sf_None(&f);
}

/* Counter(num, r=255, g=255, b=255): the text and colour of the scorer item Counter during the travelling (the
 * script no longer creates it) */
static PyObject *RaceMaker_Counter(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "num", "r", "g", "b", NULL };
	PyObject *num, *rgb[3] = { NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OOO", names, &num, &rgb[0], &rgb[1], &rgb[2])) {
		return NULL;
	}
	static const char *const vars[] = { "Red", "Green", "Blue" };
	int go = Sf_True(&f, Rm_G(&f, "prevCamera"));
	if (go > 0) {
		go = Sf_Compare(&f, num, "!=", Sf_Str(&f, ""));
	}
	if (go > 0) {
		for (int i = 0; i < 3; i++) {
			Sf_Ask(&f, "SScorer", "Set", "(issO)", 0, "Counter", vars[i], rgb[i] ? rgb[i] : Sf_Int(&f, 255));
		}
		Sf_Ask(&f, "SScorer", "Set", "(issO)", 0, "Counter", "Text", num);
	}
	return Sf_None(&f);
}

/* a new AI profile of the save variable Race.Profile (VehiclesNPC.cShipAI, SetAIProfile) */
static PyObject *Rm_Profile(scriptFrame_t *f) {
	PyObject *pd = Sf_Ask(f, "VehiclesNPC", "cShipAI", "()");
	PyObject *fn = Sf_Mod(f, "VehiclesNPC", "SetAIProfile");
	Sf_Call(f, fn, "(OO)", pd, Rm_Save(f, "Race.Profile"));
	return pd;
}

/* OnCheckPoint(ItemName, VehicleName): the OnTake of the checkpoint: a point for the player or the rivals (the
 * start, or the end at WayPointlimit); the checkpoint moves on and the rivals head for it */
static PyObject *RaceMaker_OnCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *ply = Sf_Entity(&f, veh);
	int player = Sf_EqI(&f, Sf_Attr(&f, ply, "UsrControl"), 0);
	if (player > 0) {
		PyObject *snd = Sf_Mod(&f, "Sound", "WaySound");
		Sf_Ask(&f, "SSound", "Play", "(OO)", snd, Sf_Mod(&f, "Sound", "WayVolume"));
		Sf_SetGlobal(&f, RM, "YourScore", Sf_OpI(&f, Rm_G(&f, "YourScore"), '+', 1));
		if (Sf_EqI(&f, Rm_G(&f, "YourScore"), 0) > 0) {
			Rm_Call(&f, "RaceStart", "(OO)", item, veh);
		} else if (!f.failed && Sf_Compare(&f, Rm_G(&f, "YourScore"), ">=", Rm_G(&f, "WayPointlimit")) > 0) {
			Rm_Call(&f, "EndOfRace", "(OO)", item, veh);
			return Sf_None(&f);
		}
	} else if (player == 0) {
		if (Sf_EqI(&f, Rm_G(&f, "YourScore"), -1) > 0) {
			Rm_Call(&f, "RaceStart", "(OO)", item, veh);
		} else if (!f.failed) {
			int rival = Sf_True(&f, Sf_Method(&f, Rm_G(&f, "BayScorer"), "has_key", "(O)", veh));
			if (rival > 0) {
				Sf_SetGlobal(&f, RM, "FoesScorer", Sf_OpI(&f, Rm_G(&f, "FoesScorer"), '+', 1));
				PyObject *snd = Sf_Mod(&f, "Sound", "EneWaySound");
				Sf_Ask(&f, "SSound", "Play", "(OO)", snd, Sf_Mod(&f, "Sound", "EneWayVolume"));
				if (Sf_Compare(&f, Rm_G(&f, "FoesScorer"), ">=", Rm_G(&f, "WayPointlimit")) > 0) {
					Rm_Call(&f, "EndOfRace", "(OO)", item, veh);
					return Sf_None(&f);
				}
			} else if (rival == 0) {
				return Sf_None(&f);
			}
		}
	}
	PyObject *l = Sf_Entity(&f, item);
	PyObject *next = Sf_OpI(&f, Rm_G(&f, "iStartPos"), '+', 1);
	Sf_SetGlobal(&f, RM, "iStartPos", Sf_Op(&f, next, '%', Sf_Int(&f, Sf_Len(&f, Rm_G(&f, "StartPos")))));
	PyObject *at = Sf_GetItem(&f, Rm_G(&f, "StartPos"), Rm_G(&f, "iStartPos"));
	Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, at, 0));
	at = Sf_GetItem(&f, Rm_G(&f, "StartPos"), Rm_G(&f, "iStartPos"));
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, at, 1), 0));
	PyObject *l2 = Sf_Entity(&f, Sf_Cat(&f, "Os", item, "_Trigg"));
	Sf_SetAttr(&f, l2, "Pos", Sf_Attr(&f, l, "Pos"));
	PyObject *timy = Sf_Time(&f);
	PyObject *players = Rm_G(&f, "Players");
	int mark = f.n;
	for (int i = 0;; i++) {
		PyObject *name = Sf_Next(&f, players, i);
		if (!name) {
			break;
		}
		PyObject *pd = Rm_Profile(&f);
		int same = Sf_Compare(&f, name, "==", veh);
		if (same > 0) {
			if (Sf_True(&f, Sf_Entity(&f, name)) > 0) {
				PyObject *pos = Sf_Attr(&f, l, "Pos");
				Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOOOOOO)", 6, name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
					Sf_Item(&f, pos, 2), Sf_Attr(&f, l, "Radius"), Sf_Attr(&f, pd, "ControlBoostRace"));
			}
		} else if (same == 0) {
			PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
			PyObject *pos = Sf_Attr(&f, l, "Pos");
			PyObject *a = Sf_Build(&f, "(iOOOOOO)", 6, name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
				Sf_Item(&f, pos, 2), Sf_Attr(&f, l, "Radius"), Sf_Attr(&f, pd, "ControlBoostRace"));
			Sf_Schedule(&f, timy, fn, a, NULL);
			timy = Sf_Op(&f, timy, '+', Sf_Float(&f, 0.05));
		}
		Sf_ReleaseKeep(&f, mark, &timy, 1);
	}
	Sf_SetGlobal(&f, RM, "nextStartPos", Rm_G(&f, "iStartPos"));
	Rm_Call(&f, "SetScorerValues", "(i)", 0);
	Rm_Hook(&f, "OnCheckPointCallback");
	return Sf_None(&f);
}

/* OnNearCheckPoint(ItemName, VehicleName): a racer near the checkpoint (its big trigger): the rivals that may head
 * for the next checkpoint go there, one stays on this one when the player is first */
static PyObject *RaceMaker_OnNearCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *ply = Sf_Entity(&f, veh);
	int out = Sf_EqI(&f, Sf_Attr(&f, ply, "UsrControl"), -1);
	if (out > 0) {
		int in = Sf_True(&f, Sf_Method(&f, Rm_G(&f, "BayScorer"), "has_key", "(O)", veh));
		out = in < 0 ? -1 : !in;
	}
	if (out != 0) {
		return Sf_None(&f);
	}
	PyObject *l = Sf_Entity(&f, item);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.5));
	PyObject *next2 = Sf_Op(&f, Sf_OpI(&f, Rm_G(&f, "iStartPos"), '+', 1), '%', Sf_Int(&f, Sf_Len(&f, Rm_G(&f, "StartPos"))));
	if (Sf_Compare(&f, Rm_G(&f, "nextStartPos"), "==", next2) != 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, RM, "nextStartPos", next2);
	PyObject *pos = Sf_Item(&f, Sf_GetItem(&f, Rm_G(&f, "StartPos"), Rm_G(&f, "nextStartPos")), 0);
	PyObject *opon;
	int first = Sf_EqI(&f, Sf_Attr(&f, ply, "UsrControl"), 0);
	if (first > 0) {
		first = Sf_EqI(&f, Rm_G(&f, "NumOponNotGoNextItem"), 0);
	}
	if (first > 0) {
		opon = Rm_Call(&f, "NameOponGoItem", "(O)", item);
	} else {
		opon = Sf_Str(&f, "");
	}
	PyObject *timy = Sf_Time(&f);
	PyObject *players = Rm_G(&f, "Players");
	int mark = f.n;
	for (int i = 0;; i++) {
		PyObject *name = Sf_Next(&f, players, i);
		if (!name) {
			break;
		}
		if (Sf_Compare(&f, name, "!=", veh) > 0) {
			Sf_Entity(&f, name);
			PyObject *pd = Rm_Profile(&f);
			if (Sf_EqI(&f, Sf_Attr(&f, pd, "GoNextItemRace"), 1) > 0 && Sf_Compare(&f, name, "!=", opon) > 0) {
				PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
				PyObject *a = Sf_Build(&f, "(iOOOOOOi)", 6, name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
					Sf_Item(&f, pos, 2), Sf_Attr(&f, l, "Radius"), Sf_Attr(&f, pd, "ControlBoostRace"), 1);
				Sf_Schedule(&f, timy, fn, a, NULL);
				timy = Sf_Op(&f, timy, '+', Sf_Float(&f, 0.05));
			}
		}
		Sf_ReleaseKeep(&f, mark, &timy, 1);
	}
	return Sf_None(&f);
}

/* NameOponGoItem(ItemName): the rival farthest from the checkpoint (NameError nameEntSel when none is farther than 0,
 * as the script) */
static PyObject *RaceMaker_NameOponGoItem(PyObject *self, PyObject *args) {
	PyObject *item;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &item)) {
		return NULL;
	}
	PyObject *it = Sf_Entity(&f, item);
	PyObject *best = Sf_Int(&f, 0);
	PyObject *sel = NULL;
	PyObject *players = Rm_G(&f, "Players");
	int mark = f.n;
	for (int i = 0;; i++) {
		PyObject *name = Sf_Next(&f, players, i);
		if (!name) {
			break;
		}
		PyObject *ent = Sf_Entity(&f, name);
		PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, ent, "Pos"),
			Sf_Attr(&f, it, "Pos")));
		if (Sf_Compare(&f, d, ">", best) > 0) {
			best = d;
			sel = name;
		}
		PyObject *keep[2] = { best, sel };
		Sf_ReleaseKeep(&f, mark, keep, 2);
	}
	if (!f.failed && !sel) {
		Sf_Raise(&f, PyExc_NameError, "nameEntSel");
	}
	return Sf_Return(&f, sel);
}

/* LocalGetDealType(): the kind of the challenge (the module's GetDealType unless a mission replaced it) */
static PyObject *RaceMaker_LocalGetDealType(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Sf_Return(&f, Rm_Save(&f, "Challenge.Type"));
}

/* GetDealType() == "Race" */
static int Rm_IsRace(scriptFrame_t *f) {
	return Sf_EqS(f, Rm_Call(f, "GetDealType", "()"), "Race");
}

/* StopTimer(): the player got into a ship (Parking.SwitchVehicleMissionCallback): no timer, the race scores */
static PyObject *RaceMaker_StopTimer(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	int race = Rm_IsRace(&f);
	if (race > 0) {
		Sf_SetMod(&f, "Menu", "MissionScorerCall", Rm_G(&f, "MissionScorer2"));
	} else if (race == 0) {
		Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	}
	return Sf_None(&f);
}

/* StartTimer(): the player left the ship (Parking.SwitchCharMissionCallback): 21 seconds to come back */
static PyObject *RaceMaker_StartTimer(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Speech");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "TimeToCancelCurrentMission", 21);
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Rm_G(&f, "MissionScorer"));
	Rm_FoeCom(&f, "RaceComeBack");
	Rm_Call(&f, "MissionScorer", "(i)", 0);
	return Sf_None(&f);
}

/* the warning text s the first time (the save variable var "1") */
static PyObject *Rm_FirstTime(PyObject *args, const char *var) {
	PyObject *s;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &s)) {
		return NULL;
	}
	if (Sf_EqS(&f, Rm_Save(&f, var), "1") > 0) {
		Sf_Import(&f, "Menu");
		Sf_Ask(&f, "Menu", "ShowWarning", "(iO)", 0, s);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", var, "0");
	}
	return Sf_None(&f);
}

/* FirstTimeRace(s) */
static PyObject *RaceMaker_FirstTimeRace(PyObject *self, PyObject *args) {
	(void)self;
	return Rm_FirstTime(args, "Race.FirstTime");
}

/* FirstTimeCombat(s) */
static PyObject *RaceMaker_FirstTimeCombat(PyObject *self, PyObject *args) {
	(void)self;
	return Rm_FirstTime(args, "Combat.FirstTime");
}

/* PointObjectTo(object, point): the object turns towards the point (around the vertical axis) */
static PyObject *RaceMaker_PointObjectTo(PyObject *self, PyObject *args) {
	PyObject *obj, *point;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &obj, &point)) {
		return NULL;
	}
	PyObject *start = Sf_Build(&f, "(iii)", 0, 0, -1);
	PyObject *p = Sf_Build(&f, "(OiO)", Sf_Item(&f, point, 0), 0, Sf_Item(&f, point, 2));
	PyObject *qx = Sf_Item(&f, Sf_Attr(&f, obj, "Pos"), 0);
	PyObject *q = Sf_Build(&f, "(OiO)", qx, 0, Sf_Item(&f, Sf_Attr(&f, obj, "Pos"), 2));
	PyObject *pq = Sf_Ask(&f, "SVec", "Sub", "(OO)", p, q);
	PyObject *ang = Sf_Ask(&f, "SVec", "GetAngle", "(OO)", start, pq);
	PyObject *d = Sf_Ask(&f, "SVec", "CProd", "(OO)", start, pq);
	if (Sf_Compare(&f, Sf_Item(&f, d, 1), "<", Sf_Int(&f, 0)) > 0) {
		ang = Sf_Keep(&f, PyNumber_Negative(ang));
	}
	Sf_SetAttr(&f, obj, "Ang", Sf_Build(&f, "(iOi)", 0, ang, 0));
	return Sf_None(&f);
}

/* PreloadCnDnResources(): the model and the beeps of the count down */
static PyObject *RaceMaker_PreloadCnDnResources(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(s)", "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d");
	Sf_Ask(&f, "SSound", "LoadSound", "(O)", Sf_Cat(&f, "sOs", "Sounds/Vehicles/Misc/", Rm_G(&f, "RaceCountDownSound1"),
		".wav"));
	Sf_Ask(&f, "SSound", "LoadSound", "(O)", Sf_Cat(&f, "sOs", "Sounds/Vehicles/Misc/", Rm_G(&f, "RaceCountDownSound2"),
		".wav"));
	return Sf_None(&f);
}

/* the DM_Challenger_* dummy nearest to the player (None when there is none) */
static PyObject *Rm_NearestDummy(scriptFrame_t *f) {
	Sf_Ask(f, "Scrap", "StartDummySearch", "(si)", "DM_Challenger_*", 1);
	PyObject *usr = Sf_Attr(f, Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0), "Pos");
	PyObject *dummy = Sf_Ref(f, Py_None);
	PyObject *min = Sf_Float(f, 10000000.0);
	int mark = f->n;
	while (!f->failed) {
		PyObject *aux = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(f, aux, "==", Py_None) != 0) {
			break;
		}
		PyObject *len = Sf_Ask(f, "SVec", "Mod", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", usr, Sf_Item(f, aux, 1)));
		if (Sf_Compare(f, len, "<", min) > 0) {
			min = len;
			dummy = aux;
		}
		PyObject *keep[2] = { dummy, min };
		Sf_ReleaseKeep(f, mark, keep, 2);
	}
	return f->failed ? NULL : dummy;
}

/* the AI of a challenger ship v of the kind foe: its profiles, a random one of them, static */
static void Rm_ShipAI(scriptFrame_t *f, PyObject *v, PyObject *foe, const char *profile) {
	Rm_Call(f, "InitShipIAProfile", "(OO)", foe, Rm_Save(f, profile));
	PyObject *ship = Rm_Call(f, "GetRandomShipIAProfile", "(OO)", foe, Rm_Save(f, profile));
	Sf_Ask(f, "VehiclesNPC", "SetIAProf", "(OO)", v, ship);
	Sf_Ask(f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(f, v, "Name"));
}

/* InitRace(foeType, numply=2, waypointlimit=20, shower=2, isRevenge=0, isCrazyDeal=0, userOnShip=0): a race against
 * the challenger foeType and its partners (3 racers against the mercenaries and the press) on a race of the map's
 * MapRace.Races (by the save variable Race.Num) */
static PyObject *RaceMaker_InitRace(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "foeType", "numply", "waypointlimit", "shower", "isRevenge", "isCrazyDeal", "userOnShip",
		NULL };
	static const char *const press[] = { "Betty", "Berto", "Humphrey", NULL };
	PyObject *foe, *numply = NULL, *limit = NULL, *shower = NULL, *revenge = NULL, *crazy = NULL, *onship = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OOOOOO", names, &foe, &numply, &limit, &shower, &revenge, &crazy,
		&onship)) {
		return NULL;
	}
	numply = numply ? numply : Sf_Int(&f, 2);
	limit = limit ? limit : Sf_Int(&f, 20);
	shower = shower ? shower : Sf_Int(&f, 2);
	revenge = revenge ? revenge : Sf_Int(&f, 0);
	crazy = crazy ? crazy : Sf_Int(&f, 0);
	onship = onship ? onship : Sf_Int(&f, 0);
	static const char *const mods[] = { "Alarm", "Menu", "Speech", "Parking", "RacerScorer", "MapRace", "math",
		"VehiclesNPC" };
	for (int i = 0; i < 8; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *pre = Rm_G(&f, "OnPreInitRace");
	if (Sf_True(&f, pre) > 0) {
		Sf_Call(&f, pre, "(O)", Rm_G(&f, "Players"));
	}
	Sf_Ask(&f, "Scrap", "Set", "(sd)", "TimeToCancelCurrentMission", 0.0);
	int merc = Sf_InS(&f, foe, rm_mercs);
	int pressrace = Sf_InS(&f, foe, press);
	if (merc > 0 || pressrace > 0) {
		numply = Sf_Int(&f, 3);
	}
	int iscrazy = Sf_True(&f, crazy);
	if (iscrazy == 0) {
		PyObject *t = Sf_Time(&f);
		PyObject *fn = Rm_G(&f, "FirstTimeRace");
		Sf_Schedule(&f, t, fn, Sf_Build(&f, "(O)", Sf_Lang(&f, Sf_Str(&f, "RaceHowTo"))), NULL);
	}
	Sf_SetGlobal(&f, RM, "ChallengeFoeType", foe);
	int partners = Sf_Len(&f, Sf_GetItem(&f, Rm_G(&f, "challengePartners"), Rm_G(&f, "ChallengeFoeType")));
	Sf_SetGlobal(&f, RM, "iStartPos", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, RM, "nextStartPos", Rm_G(&f, "iStartPos"));
	Sf_SetGlobal(&f, RM, "Players", Sf_Keep(&f, PyList_New(0)));
	Sf_SetGlobal(&f, RM, "BayScorer", Sf_Keep(&f, PyDict_New()));
	Sf_SetGlobal(&f, RM, "FoesScorer", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, RM, "YourScore", Sf_Int(&f, -1));
	Sf_SetGlobal(&f, RM, "WayPointlimit", limit);
	Sf_SetGlobal(&f, RM, "levelPathFind", Sf_Int(&f, -1));
	Sf_SetGlobal(&f, RM, "NumOponNotGoNextItem", Sf_Int(&f, 0));
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDeath", "RaceMaker.RacerKilled");
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "RaceMaker.RacerDamaged");
	Sf_SetMod(&f, "Alarm", "AbortMissionCallback", Rm_G(&f, "RacerInterruptedByAlarm"));
	PyObject *num = Sf_ToInt(&f, Rm_Save(&f, "Race.Num"));
	PyObject *races = Sf_Mod(&f, "MapRace", "Races");
	PyObject *pick = Sf_Op(&f, num, '%', Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "MapRace", "Races"))));
	Sf_Call(&f, Sf_GetItem(&f, races, pick), "()");
	if (iscrazy == 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Race.Num", Sf_Repr(&f, Sf_OpI(&f, num, '+', 1)));
	}
	long count = 0;
	Sf_Check(&f, f.failed || Script_Range(numply, &count));
	for (long i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		PyObject *io = Sf_Int(&f, i);
		PyObject *start = Sf_Item(&f, Rm_G(&f, "StartPos"), 0);
		PyObject *deg = Sf_Op(&f, Sf_Int(&f, 90), '+', Sf_Op(&f, Sf_Op(&f, Sf_Int(&f, 360), '*', io), '/', numply));
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "(OO)", Sf_Build(&f, "(iii)", 0, 2200, 0), Sf_Build(&f, "(iOO)", 0,
			Sf_Item(&f, Sf_Item(&f, Rm_G(&f, "StartPos"), 0), 1), deg));
		PyObject *spos = Sf_Ask(&f, "SVec", "Add", "(OO)", Sf_Item(&f, start, 0), rot);
		PyObject *v;
		if (i == 0) {
			PyObject *npc = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "NPC_START_ITEM", Sf_Item(&f, spos, 0),
				Sf_Item(&f, spos, 1), Sf_Item(&f, spos, 2), "item");
			Sf_SetGlobal(&f, RM, "NPC_START_ITEM", npc);
			Sf_SetAttr(&f, Rm_G(&f, "NPC_START_ITEM"), "Ang", Sf_Build(&f, "(iOi)", 0, Rm_StartAng(&f, 0), 0));
			Sf_SetAttrS(&f, Rm_G(&f, "NPC_START_ITEM"), "OnTake", "RaceMaker.PlaceNPCAtRacePos");
			Sf_SetAttrI(&f, Rm_G(&f, "NPC_START_ITEM"), "Radius", 1800);
			if (iscrazy == 0) {
				PyObject *dummy = Rm_NearestDummy(&f);
				PyObject *name = Sf_Op(&f, Sf_Str(&f, "BadBoy"), '+', Sf_Repr(&f, io));
				PyObject *dpos = Sf_Item(&f, dummy, 1);
				v = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, dpos, 0), Sf_Item(&f, dpos, 1),
					Sf_Item(&f, dpos, 2), "Car");
			} else {
				PyObject *name = Sf_Op(&f, Sf_Str(&f, "BadBoy"), '+', Sf_Repr(&f, io));
				v = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, spos, 0), Sf_Item(&f, spos, 1),
					Sf_Item(&f, spos, 2), "Car");
			}
			Sf_Method(&f, Rm_G(&f, "challenged_NPCs_Names"), "append", "(O)", Sf_Attr(&f, v, "Name"));
			Sf_SetGlobal(&f, RM, "ChallengerName", Sf_Attr(&f, v, "Name"));
			if (iscrazy == 0) {
				Sf_SetAttr(&f, v, "Money", Sf_Op(&f, Sf_Attr(&f, v, "Money"), '+', Rm_Double(&f)));
			}
			Rm_ShipAI(&f, v, Rm_G(&f, "ChallengeFoeType"), "Race.Profile");
			Sf_SetAttrS(&f, v, "TgType", "None");
			Sf_SetAttrS(&f, v, "EntityList", "Race");
			if (iscrazy == 0) {
				Rm_Call(&f, "PointObjectTo", "(OO)", v, Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Pos"));
			}
		} else {
			PyObject *name = Sf_Op(&f, Sf_Str(&f, "BadBoy"), '+', Sf_Repr(&f, io));
			v = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, spos, 0), Sf_Item(&f, spos, 1),
				Sf_Item(&f, spos, 2), "Car");
			PyObject *list = Sf_GetItem(&f, Rm_G(&f, "challengePartners"), Rm_G(&f, "ChallengeFoeType"));
			PyObject *partner = Sf_Item(&f, list, partners == 1 ? 0 : i - 1);
			Rm_ShipAI(&f, v, partner, "Race.Profile");
			Sf_SetAttrS(&f, v, "TgType", "None");
			Sf_SetAttrS(&f, v, "EntityList", "Race");
			Sf_SetAttr(&f, v, "Ang", Sf_Build(&f, "(iOi)", 0, Rm_StartAng(&f, 0), 0));
			Sf_SetAttr(&f, v, "MovePos", Sf_Attr(&f, v, "Pos"));
			Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, v, "Ang"));
			Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.001));
		}
		Sf_SetItem(&f, Rm_G(&f, "BayScorer"), Sf_Attr(&f, v, "Name"), Sf_Int(&f, 0));
		Sf_Method(&f, Rm_G(&f, "Players"), "append", "(O)", Sf_Attr(&f, v, "Name"));
		Sf_SetGlobal(&f, RM, "levelPathFind", Sf_Int(&f, 0));
		PyObject *pd = Rm_Profile(&f);
		if (Sf_EqI(&f, Sf_Attr(&f, pd, "GoNextItemRace"), 0) > 0) {
			Sf_SetGlobal(&f, RM, "NumOponNotGoNextItem", Sf_OpI(&f, Rm_G(&f, "NumOponNotGoNextItem"), '+', 1));
		}
		Sf_Release(&f, mark);
	}
	Sf_SetGlobal(&f, RM, "iStartPos", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, RM, "nextStartPos", Rm_G(&f, "iStartPos"));
	PyObject *spos = Sf_Item(&f, Sf_Item(&f, Rm_G(&f, "StartPos"), 0), 0);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "CheckPoint", Sf_Item(&f, spos, 0), Sf_Item(&f, spos, 1),
		Sf_Item(&f, spos, 2), "Item");
	Sf_Method(&f, l, "SetModel", "(s)", "models/Misc/Wayp/Wayp.m3d");
	Sf_SetAttrS(&f, l, "OnTake", "RaceMaker.OnRaceStart");
	Sf_SetAttr(&f, l, "RadarShow", shower);
	Sf_SetAttrI(&f, l, "Radius", 2400);
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Rm_StartAng(&f, 0), 0));
	Sf_SetAttrI(&f, l, "FadeInEffect", 0);
	Sf_SetAttrI(&f, l, "FadeOutEffect", 0);
	PyObject *tname = Sf_Cat(&f, "Os", Sf_Attr(&f, l, "Name"), "_Trigg");
	spos = Sf_Item(&f, Sf_Item(&f, Rm_G(&f, "StartPos"), 0), 0);
	PyObject *l2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", tname, Sf_Item(&f, spos, 0), Sf_Item(&f, spos, 1),
		Sf_Item(&f, spos, 2), "Item");
	Sf_SetAttr(&f, l2, "Pos", Sf_Attr(&f, l, "Pos"));
	Sf_SetAttrI(&f, l2, "Radius", 20000);
	Sf_SetAttrI(&f, l2, "FadeInEffect", 0);
	Sf_SetAttrI(&f, l2, "FadeOutEffect", 0);
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(&f, l, "Name"));
	Rm_Call(&f, "PreloadCnDnResources", "()");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Rm_G(&f, "MissionScorer"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	int timer = Sf_True(&f, onship);
	if (timer == 0) {
		int skip = Sf_True(&f, Rm_G(&f, "SkipTimer"));
		timer = skip < 0 ? -1 : !skip;
	} else if (timer > 0) {
		timer = 0;
	}
	if (timer > 0) {
		Sf_Ask(&f, "Scrap", "Set", "(si)", "TimeToCancelCurrentMission", 60);
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Rm_G(&f, "StopTimer"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Rm_G(&f, "StartTimer"));
	int rev = Sf_True(&f, revenge);
	int taunt = rev < 0 ? -1 : rev ? 0 : iscrazy < 0 ? -1 : !iscrazy;
	if (taunt > 0) {
		PyObject *key = Sf_Cat(&f, "sO", "RaceHurryUp", foe);
		PyObject *hook = Rm_G(&f, "OnInitialTaunt");
		if (Sf_True(&f, hook) > 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(OOO)", key, Rm_G(&f, "ChallengeFoeType"), hook);
			Sf_SetGlobal(&f, RM, "OnInitialTaunt", Py_None);
		} else {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(OO)", key, Rm_G(&f, "ChallengeFoeType"));
		}
	}
	PyObject *t = Sf_Time(&f);
	if (taunt > 0) {
		PyObject *s = Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", Sf_Lang(&f, Sf_Cat(&f, "sO", "RaceHurryUp", foe)));
		t = Sf_Op(&f, t, '+', s);
	}
	PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
	PyObject *first = Sf_Item(&f, Rm_G(&f, "Players"), 0);
	PyObject *npos = Sf_Attr(&f, Rm_G(&f, "NPC_START_ITEM"), "Pos");
	PyObject *rad = Sf_Op(&f, Sf_Attr(&f, Rm_G(&f, "NPC_START_ITEM"), "Radius"), '/', Sf_Float(&f, 4.0));
	PyObject *a = Sf_Build(&f, "(iOOOOOid)", 1, first, Sf_Item(&f, npos, 0), Sf_Item(&f, npos, 1), Sf_Item(&f, npos, 2),
		rad, 1, 1.0);
	Sf_Schedule(&f, t, fn, a, NULL);
	int msg = iscrazy;
	if (msg == 0) {
		int on = Sf_True(&f, onship);
		msg = on < 0 ? -1 : !on;
	} else if (msg > 0) {
		msg = 0;
	}
	if (msg > 0 && Rm_NoTutorial(&f) > 0) {
		fn = Rm_G(&f, "ShowSystemMessage");
		Sf_Schedule(&f, t, fn, Sf_Build(&f, "(O)", Sf_Lang(&f, Sf_Str(&f, "RaceSystemGetShip"))), "SF_Secondary");
	}
	PyObject *init = Rm_G(&f, "OnInitRace");
	if (Sf_True(&f, init) > 0) {
		Sf_Call(&f, init, "(O)", Rm_G(&f, "ChallengerName"));
	}
	return Sf_None(&f);
}

/* InitCombat(foeType, isRevenge=0, isCrazyDeal=0, numFoes=1): Challenge.NumEnemies ships of the challenger (and its
 * partners) at the DM_Challenger_* dummy nearest to the player, as enemies; the timer to get into a ship */
static PyObject *RaceMaker_InitCombat(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "foeType", "isRevenge", "isCrazyDeal", "numFoes", NULL };
	PyObject *foe, *revenge = NULL, *crazy = NULL, *numfoes = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OOO", names, &foe, &revenge, &crazy, &numfoes)) {
		return NULL;
	}
	revenge = revenge ? revenge : Sf_Int(&f, 0);
	static const char *const mods[] = { "Menu", "Speech", "Parking", "OutPolice", "OutMap", "VehiclesNPC" };
	for (int i = 0; i < 6; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *t = Sf_Time(&f);
	PyObject *fn = Rm_G(&f, "FirstTimeCombat");
	Sf_Schedule(&f, t, fn, Sf_Build(&f, "(O)", Sf_Lang(&f, Sf_Str(&f, "CombatHowTo"))), NULL);
	Sf_SetGlobal(&f, RM, "ChallengeFoeType", foe);
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "RaceMaker.NPCCombatKilled");
	PyObject *dummy = Rm_NearestDummy(&f);
	PyObject *num = Sf_ToInt(&f, Rm_Save(&f, "Challenge.NumEnemies"));
	int partners = Sf_Len(&f, Sf_GetItem(&f, Rm_G(&f, "challengePartners"), Rm_G(&f, "ChallengeFoeType")));
	PyObject *dpos = Sf_Item(&f, dummy, 1);
	PyObject *prev = Sf_Build(&f, "(OOO)", Sf_Item(&f, dpos, 0), Sf_Item(&f, dpos, 1), Sf_Item(&f, dpos, 2));
	long count = 0;
	Sf_Check(&f, f.failed || Script_Range(num, &count));
	int mark = f.n;
	for (long i = 0; i < count && !f.failed; i++) {
		PyObject *v = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "BadBoy", Sf_Item(&f, prev, 0), Sf_Item(&f, prev, 1),
			Sf_Item(&f, prev, 2), "Car");
		Sf_Method(&f, Rm_G(&f, "challenged_NPCs_Names"), "append", "(O)", Sf_Attr(&f, v, "Name"));
		prev = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPosReal", "(O)", prev);
		Rm_ShipAI(&f, v, foe, "Combat.Profile");
		PyObject *list = Sf_GetItem(&f, Rm_G(&f, "challengePartners"), Rm_G(&f, "ChallengeFoeType"));
		foe = Sf_Item(&f, list, partners == 1 ? 0 : i - 1);
		Sf_SetAttrS(&f, v, "EntityList", "Enemies");
		PyObject *keep[2] = { prev, foe };
		Sf_ReleaseKeep(&f, mark, keep, 2);
		prev = keep[0];
		foe = keep[1];
	}
	PyObject *v = Sf_Entity(&f, Sf_Item(&f, Rm_G(&f, "challenged_NPCs_Names"), 0));
	Rm_Call(&f, "PointObjectTo", "(OO)", v, Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Pos"));
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Rm_G(&f, "MissionScorer"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "TimeToCancelCurrentMission", 60);
	int taunt = Sf_True(&f, revenge);
	if (taunt == 0) {
		PyObject *key = Sf_Cat(&f, "sO", "CombatHurryUp", Rm_G(&f, "ChallengeFoeType"));
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(OO)", key, Rm_G(&f, "ChallengeFoeType"));
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Rm_G(&f, "StopTimer"));
	PyObject *pos = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "()");
	t = Sf_Time(&f);
	if (taunt == 0) {
		PyObject *key = Sf_Cat(&f, "sO", "CombatHurryUp", Rm_G(&f, "ChallengeFoeType"));
		t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", Sf_Lang(&f, key)));
	}
	Sf_Schedule(&f, t, Rm_G(&f, "MoveCombatNPCsToPos"), Sf_Build(&f, "(O)", pos), NULL);
	fn = Rm_G(&f, "ShowSystemMessage");
	Sf_Schedule(&f, t, fn, Sf_Build(&f, "(O)", Sf_Lang(&f, Sf_Str(&f, "CombatSystemGetShip"))), "SF_Secondary");
	return Sf_None(&f);
}

/* MoveCombatNPCsToPos(pos): the challenged ships that do not fight yet fly to the place */
static PyObject *RaceMaker_MoveCombatNPCsToPos(PyObject *self, PyObject *args) {
	PyObject *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &pos)) {
		return NULL;
	}
	PyObject *npcs = Rm_G(&f, "challenged_NPCs_Names");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, npcs, i);
		if (!name) {
			break;
		}
		if (Sf_Compare(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", name), "!=", Sf_Int(&f, 2)) > 0) {
			Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOOOOiidd)", 1, name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
				Sf_Item(&f, pos, 2), 1800, 0, 0.85, 20.0, 20.0);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* PlaceNPCAtRacePos(itemName, vehicleName): the challenger reached its start item: it moves onto it and waits */
static PyObject *RaceMaker_PlaceNPCAtRacePos(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &veh)) {
		return NULL;
	}
	if (Sf_Compare(&f, veh, "==", Rm_G(&f, "ChallengerName")) > 0) {
		PyObject *i = Sf_Entity(&f, item);
		PyObject *v = Sf_Entity(&f, veh);
		Sf_SetAttr(&f, v, "MovePos", Sf_Attr(&f, i, "Pos"));
		Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, i, "Ang"));
		Sf_SetAttrI(&f, v, "MoveTime", 1);
		Sf_SetAttrI(&f, v, "MoveFade", 2);
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, v, "Name"));
		Sf_Method(&f, i, "Del", "()");
		Sf_SetGlobal(&f, RM, "NPC_START_ITEM", Py_None);
	}
	return Sf_None(&f);
}

/* SetScorerValues(id): the scores of the race on the scorer (the leader on top), the race scorer when none is set */
static PyObject *RaceMaker_SetScorerValues(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	if (Sf_True(&f, Sf_Mod(&f, "Menu", "MissionScorerCall")) == 0 && Rm_IsRace(&f) > 0) {
		Sf_SetMod(&f, "Menu", "MissionScorerCall", Rm_G(&f, "MissionScorer2"));
		Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	}
	PyObject *best = Rm_G(&f, "FoesScorer");
	Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "MissionLimit", "Text", Sf_Repr(&f, Rm_G(&f, "WayPointlimit")));
	int ahead = Sf_Compare(&f, Rm_G(&f, "YourScore"), ">=", best);
	if (ahead >= 0) {
		PyObject *top = ahead ? Sf_Repr(&f, Rm_G(&f, "YourScore")) : Sf_Repr(&f, best);
		Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "MissionBest", "Text", top);
		PyObject *low = ahead ? Sf_Repr(&f, best) : Sf_Repr(&f, Rm_G(&f, "YourScore"));
		Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "MissionBad", "Text", low);
		Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "MissionYou", "Y", ahead ? 180 : 200);
		Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "MissionYou", "Blue", ahead ? 255 : 0);
		Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "MissionYou", "Red", ahead ? 0 : 255);
	}
	return Sf_None(&f);
}

/* the player of a scorer function's id (the C scorer helpers take an int) */
static int Rm_Id(scriptFrame_t *f, PyObject *id) {
	int pid = 0;
	Sf_Check(f, f->failed || PyArg_Parse(id, "i", &pid));
	return pid;
}

/* MissionScorer2(id): the race scores once the race started (data/menus/race.def, section scores) */
static PyObject *RaceMaker_MissionScorer2(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *mh = Sf_From(&f, "Scorer", "MarginH");
	Sf_From(&f, "Scorer", "MarginV");
	Sf_Import(&f, "Menu");
	if (Sf_Compare(&f, Rm_G(&f, "YourScore"), ">", Sf_Int(&f, -1)) > 0) {
		int pid = Rm_Id(&f, id);
		PyObject *vars = Sf_Build(&f, "{s:O}", "mh", mh);
		if (vars) {
			Script_RunFile(pid, "menus/race.def", "scores", race_def, NULL, vars);
		}
		Rm_Call(&f, "SetScorerValues", "(O)", id);
	}
	return Sf_None(&f);
}

/* MissionScorer(id): the timer to get into a ship (data/menus/race.def, section timer), the scores in a race */
static PyObject *RaceMaker_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Scorer");
	PyObject *x = Sf_OpI(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_X"), '+', 28);
	PyObject *y = Sf_Mod(&f, "Scorer", "MissionScorerText_Y");
	PyObject *c[4];
	for (int i = 0; i < 4; i++) {
		c[i] = Sf_Item(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Color"), i);
	}
	int pid = Rm_Id(&f, id);
	PyObject *vars = Sf_Build(&f, "{s:O,s:O,s:O,s:O,s:O,s:O}", "x", x, "y", y, "r", c[0], "g", c[1], "b", c[2], "a", c[3]);
	if (vars) {
		Script_RunFile(pid, "menus/race.def", "timer", race_def, NULL, vars);
	}
	if (Rm_IsRace(&f) > 0) {
		Rm_Call(&f, "MissionScorer2", "(O)", id);
	}
	return Sf_None(&f);
}

/* InitShipIAProfile(ChallengeFoeType, profileType): an AI profile in VehiclesNPC.AIProf for each ship of the kind in
 * AISHIPS ("S<kind><n>_<profile>") */
static PyObject *RaceMaker_InitShipIAProfile(PyObject *self, PyObject *args) {
	PyObject *foe, *profile;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &foe, &profile)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *cntr = Sf_Int(&f, 0);
	PyObject *ships = Sf_GetItem(&f, Rm_G(&f, "AISHIPS"), foe);
	int mark = f.n;
	for (int i = 0;; i++) {
		PyObject *type = Sf_Next(&f, ships, i);
		if (!type) {
			break;
		}
		cntr = Sf_OpI(&f, cntr, '+', 1);
		PyObject *sp = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
		Sf_SetAttr(&f, sp, "Ship", type);
		Sf_Ask(&f, "VehiclesNPC", "SetAIProfile", "(OO)", sp, profile);
		PyObject *key = Sf_Cat(&f, "sOOsO", "S", foe, Sf_Repr(&f, cntr), "_", profile);
		Sf_SetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), key, sp);
		Sf_ReleaseKeep(&f, mark, &cntr, 1);
	}
	return Sf_None(&f);
}

/* GetRandomShipIAProfile(ChallengeFoeType, profileType): the name of a profile of InitShipIAProfile: in turn for the
 * mercenaries and the press, else at random */
static PyObject *RaceMaker_GetRandomShipIAProfile(PyObject *self, PyObject *args) {
	static const char *const groups[] = { "Mercs", "Press", NULL };
	PyObject *foe, *profile;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &foe, &profile)) {
		return NULL;
	}
	int turn = Sf_InS(&f, foe, groups);
	PyObject *n = NULL;
	if (turn > 0) {
		PyObject *len = Sf_Int(&f, Sf_Len(&f, Sf_GetItem(&f, Rm_G(&f, "AISHIPS"), foe)));
		Sf_SetGlobal(&f, RM, "nextIAProfile", Sf_Op(&f, Sf_OpI(&f, Rm_G(&f, "nextIAProfile"), '+', 1), '%', len));
		n = Sf_OpI(&f, Rm_G(&f, "nextIAProfile"), '+', 1);
	} else if (turn == 0) {
		int len = Sf_Len(&f, Sf_GetItem(&f, Rm_G(&f, "AISHIPS"), foe));
		if (!f.failed) {
			n = Sf_ToInt(&f, Sf_Op(&f, Sf_Float(&f, Scrap_RandRange(1, len)), '+', Sf_Float(&f, 0.9)));
		}
	}
	return Sf_Return(&f, Sf_Cat(&f, "sOOsO", "S", foe, Sf_Repr(&f, n), "_", profile));
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
