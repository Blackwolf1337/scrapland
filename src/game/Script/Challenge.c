/*
 * The challenges in C (issue #2, the mission script missions/challenge.py): races and combats against the
 * characters. A character offers them in its talk (AddChallenge, SetChallengeOptions), the player bets money (or
 * lives against the mercenaries) on 1, 3 or 5 foes (OnChallengeOpt), the challenge starts in the next outdoor level
 * (LaunchChallenge, Init). Leaving it lost becomes a revenge: the foe calls the player at the nearest phone cab
 * (SetRevenge, LaunchRevenge, PhoneCab.RevengeSpeech).
 *
 * The module variables stay in the module (numChallengeEnemies, challengeType, foeType, playerWinsRevenge and the
 * tables groups, combatRewards and raceRewards), because the character talks and PhoneCab read and set them. The
 * code reads them as globals like the script (a missing one is a NameError).
 */
#include <limits.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

#define CHAL_ERROR "ERROR: Par\xe1metro incorrecto pasado a 'OnChallenge'"

/* a global of the module (a new reference) */
static PyObject *Chal_Global(const char *name) {
	return Script_Global("Challenge", name);
}

/* groups[foe] (a new reference) */
static PyObject *Chal_Group(PyObject *foe) {
	PyObject *groups = Chal_Global("groups");
	PyObject *g = groups && foe ? PyObject_GetItem(groups, foe) : NULL;
	Py_XDECREF(groups);
	return g;
}

/* Scrap.SetSaveVar(name, v): 0 with the error set */
static byte Chal_SetSave(const char *name, PyObject *v) {
	return v && Script_Try("Scrap", "SetSaveVar", "(sO)", name, v);
}

/* Scrap.SetSaveVar(name, `v`) */
static byte Chal_SetSaveRepr(const char *name, PyObject *v) {
	PyObject *r = Script_Repr(v);
	byte ok = Chal_SetSave(name, r);
	Py_XDECREF(r);
	return ok;
}

/* int(Scrap.GetSaveVar(name)) (a new reference) */
static PyObject *Chal_SaveInt(const char *name) {
	PyObject *sv = Script_SaveVar(name);
	PyObject *i = Script_Int(sv);
	Py_XDECREF(sv);
	return i;
}

/* the number of challenges of a group so far, from the save variable name (0 when it is empty): 0 with the error
 * set */
static byte Chal_Total(PyObject *name, long *total) {
	PyObject *sv = name ? Script_Ask("Scrap", "GetSaveVar", "(O)", name) : NULL;
	int t = Script_True(sv);
	*total = 0;
	if (t > 0) {
		PyObject *i = Script_Int(sv);
		t = i ? 1 : -1;
		*total = i ? PyInt_AsLong(i) : 0;
		Py_XDECREF(i);
	}
	Py_XDECREF(sv);
	return t >= 0;
}

/* ComputeReward(_type, foe, numEnemies): the money a challenge costs (and pays), less after many of the group */
static PyObject *Chal_Reward(PyObject *type, PyObject *foe, PyObject *num) {
	scriptRefs_t r = { 0 };
	PyObject *group = Script_Keep(&r, Chal_Group(foe));
	byte combat = Script_Is(type, "Combat");
	long total;
	PyObject *name = group ? Script_Keep(&r, Script_SPlus(combat ? "Stats.TotalCombats." : "Stats.TotalRaces.", group)) : NULL;
	if (!name || !Chal_Total(name, &total)) {
		return Script_Done(&r, NULL);
	}
	PyObject *table = Script_Keep(&r, Chal_Global(combat ? "combatRewards" : "raceRewards"));
	PyObject *base = table ? Script_Keep(&r, PyObject_GetItem(table, group)) : NULL;
	if (base && total >= 5) {
		PyObject *f = Script_Keep(&r, PyFloat_FromDouble(total < 8 ? 0.8 : total < 10 ? 0.6 : 0.4));
		base = f ? Script_Keep(&r, PyNumber_Multiply(base, f)) : NULL;
	}
	if (!base) {
		return Script_Done(&r, NULL);
	}
	int c = Script_NumCmp(num, 1);
	if (c == -2) {
		return Script_Done(&r, NULL);
	}
	if (c == 0) {
		return Script_Done(&r, Script_Int(base));
	}
	PyObject *k = Script_Keep(&r, PyFloat_FromDouble(1.2));
	PyObject *a = k ? Script_Keep(&r, PyNumber_Multiply(k, base)) : NULL;
	PyObject *b = a ? Script_Keep(&r, PyNumber_Multiply(a, num)) : NULL;
	return Script_Done(&r, Script_Int(b));
}

static PyObject *Challenge_ComputeReward(PyObject *self, PyObject *args) {
	PyObject *type, *foe, *num;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &type, &foe, &num)) {
		return NULL;
	}
	return Chal_Reward(type, foe, num);
}

static PyObject *Challenge_SetRevenge(PyObject *self, PyObject *args);

/* Init(Revenge = 0): the race or the combat of the save variables Challenge.*, else the revenge pending */
static PyObject *Challenge_Init(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "RaceMaker", NULL };
	PyObject *revenge = NULL;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &revenge) || !Script_Import(mods)) {
		return NULL;
	}
	revenge = Script_Keep(&r, revenge ? Script_Ref(revenge) : PyInt_FromLong(0));
	PyObject *type = Script_Keep(&r, Script_SaveVar("Challenge.Type"));
	PyObject *foe = type ? Script_Keep(&r, Script_SaveVar("Challenge.Foe")) : NULL;
	if (!foe) {
		return Script_Done(&r, NULL);
	}
	PyObject *ok = NULL;
	if (Script_Is(type, "Race")) {
		PyObject *num = Script_Keep(&r, Script_SaveVar("Challenge.Money")) ? Script_Keep(&r, Chal_SaveInt("Challenge.NumEnemies")) : NULL;
		int one = num ? Script_NumCmp(num, 1) : -2;
		int three = one != -2 && one != 0 ? Script_NumCmp(num, 3) : 1;
		if (one == -2 || three == -2) {
			return Script_Done(&r, NULL);
		}
		int nump = one == 0 ? 2 : three == 0 ? 3 : 4;
		PyObject *kw = Py_BuildValue("{s:i,s:i,s:O,s:O}", "waypointlimit", 15, "numply", nump, "foeType", foe, "isRevenge", revenge);
		ok = kw ? Script_Keep(&r, Script_AskKw("RaceMaker", "InitRace", PyTuple_New(0), kw)) : NULL;
	} else if (Script_Is(type, "Combat")) {
		PyObject *num = Script_Keep(&r, Chal_SaveInt("Challenge.NumEnemies"));
		PyObject *kw = num ? Py_BuildValue("{s:O}", "numFoes", num) : NULL;
		ok = kw ? Script_Keep(&r, Script_AskKw("RaceMaker", "InitCombat", Py_BuildValue("(OO)", foe, revenge), kw)) : NULL;
	} else {
		PyObject *rtype = Script_Keep(&r, Script_SaveVar("Revenge.Type"));
		PyObject *rfoe = rtype ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
		ok = rfoe;
		if (rfoe && !Script_Is(rtype, "") && !Script_Is(rfoe, "")) {
			PyObject *a = Script_Keep(&r, PyTuple_New(0));
			ok = a ? Script_Keep(&r, Challenge_SetRevenge(NULL, a)) : NULL;
		}
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* the skill of the challengers by the number of challenges against the group: Race.Profile and Combat.Profile */
static byte Chal_Skills(PyObject *type, PyObject *group) {
	PyObject *name = Script_Plus(Script_PlusS(Script_SPlus("Stats.Total", type), "s."), group);
	long total;
	byte ok = Chal_Total(name, &total);
	Py_XDECREF(name);
	if (!ok) {
		return 0;
	}
	const char *v = total < 5 ? "Pilot" : total < 8 ? "Veteran" : total < 10 ? "Elite" : "MegaAce";
	char race[32];
	Str_Printf(race, sizeof(race), "Race_%s", v);
	return Script_Try("Scrap", "SetSaveVar", "(ss)", "Race.Profile", race) &&
		Script_Try("Scrap", "SetSaveVar", "(ss)", "Combat.Profile", v);
}

static PyObject *Challenge_SetChallengerSkills(PyObject *self, PyObject *args) {
	PyObject *type, *group;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &type, &group)) {
		return NULL;
	}
	return Chal_Skills(type, group) ? Py_ReturnNone() : NULL;
}

/* the talk of the bet: Speech.OnChallengeSpch(cad, ans) indoors, Speech.OnChallengeObjSpch(Speech.ObjEntity, foe,
 * cad, ans) at a phone cab (obj), isMerc=1 for the mercenaries. ans is prefix + name + suffix (name NULL: prefix
 * only). 0 with the error set */
static byte Chal_Spch(byte obj, PyObject *foe, PyObject *cad, const char *prefix, PyObject *name, const char *suffix, byte merc) {
	PyObject *ent = obj ? Script_Attr("Speech", "ObjEntity") : NULL;
	if (obj && !ent) {
		return 0;
	}
	PyObject *ans = name ? Script_PlusS(Script_SPlus(prefix, name), suffix) : PyString_FromString(prefix);
	PyObject *a = !ans ? NULL : obj ? Py_BuildValue("(OOOO)", ent, foe, cad, ans) : Py_BuildValue("(OO)", cad, ans);
	PyObject *kw = a && merc ? Py_BuildValue("{s:i}", "isMerc", 1) : NULL;
	PyObject *v = a ? Script_AskKw("Speech", obj ? "OnChallengeObjSpch" : "OnChallengeSpch", a, kw) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(ans);
	Py_XDECREF(ent);
	return v != NULL;
}

/* Speech.OnEndOfSpeech = the function name of the module: 0 with the error set */
static byte Chal_OnEnd(const char *name) {
	PyObject *f = Chal_Global(name);
	if (f) {
		Script_SetVar("Speech", "OnEndOfSpeech", f);
	}
	return f != NULL;
}

/* Init.Outdoor == 1: 1, 0, -1 with the error set */
static int Chal_Outdoor(void) {
	PyObject *o = Script_Attr("Init", "Outdoor");
	int c = o ? Script_NumCmp(o, 1) : -2;
	Py_XDECREF(o);
	return c == -2 ? -1 : c == 0;
}

/* print the error of a wrong option: 0 with the error set */
static byte Chal_PrintError(void) {
	PyObject *s = PyString_FromString(CHAL_ERROR);
	byte ok = s && Script_Print(&s, 1);
	Py_XDECREF(s);
	return ok;
}

/* OnChallengeOpt(opt, ch, challengeLaunchedIndoor = 1): the bet of option 1, 2 or 3 (1, 3 or 5 foes; 1 or 3 for the
 * mercenaries, for 2 or 3 lives). With enough money (or lives) the talk ends in the challenge (indoors) or the
 * revenge (at a phone cab) and the bet is taken; else the foe laughs and the options start again */
static PyObject *Challenge_OnChallengeOpt(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Speech", "Menu", "string", NULL };
	static const char *const initmod[] = { "Init", NULL };
	PyObject *opt, *ch, *indoorArg = NULL;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|O", &opt, &ch, &indoorArg) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *money = Script_Keep(&r, Script_Ask("Scrap", "GetMoney", "()"));
	int indoor = !money ? -1 : indoorArg ? Script_True(indoorArg) : 1;
	PyObject *foe = NULL, *type = NULL;
	if (indoor > 0) {
		foe = Script_Keep(&r, Chal_Global("foeType"));
		type = foe ? Script_Keep(&r, Chal_Global("challengeType")) : NULL;
	} else if (indoor == 0) {
		foe = Script_Keep(&r, Script_SaveVar("Challenge.Foe"));
		if (Script_Is(foe, "")) {
			foe = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
		}
		type = foe ? Script_Keep(&r, Script_SaveVar("Challenge.Type")) : NULL;
		if (Script_Is(type, "")) {
			type = Script_Keep(&r, Script_SaveVar("Revenge.Type"));
		}
	}
	PyObject *group = type ? Script_Keep(&r, Chal_Group(foe)) : NULL;
	if (!group) {
		return Script_Done(&r, NULL);
	}
	byte ok;
	int c1 = Script_NumCmp(opt, 1), c2 = c1 ? Script_NumCmp(opt, 2) : 1;
	int c3 = c1 && c2 && !Script_Is(group, "Mercs") ? Script_NumCmp(opt, 3) : 1;
	if (c1 == -2 || c2 == -2 || c3 == -2) {
		return Script_Done(&r, NULL);
	}
	if (!Script_Is(group, "Mercs")) {
		int num = c1 == 0 ? 1 : c2 == 0 ? 3 : c3 == 0 ? 5 : 0;
		if (!num) {
			return Script_Done(&r, Chal_PrintError() ? Py_ReturnNone() : NULL);
		}
		if (!Script_SetGlobal("Challenge", "numChallengeEnemies", PyInt_FromLong(num))) {
			return Script_Done(&r, NULL);
		}
		PyObject *n = Script_Keep(&r, Chal_Global("numChallengeEnemies"));
		PyObject *taken = n ? Script_Keep(&r, Chal_Reward(type, foe, n)) : NULL;
		PyObject *g = taken ? Script_Keep(&r, Chal_Group(foe)) : NULL;
		PyObject *lang = g && Chal_Skills(type, g) ? Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "ChallengeRaceStdOpt1")) : NULL;
		PyObject *rep = lang ? Script_Keep(&r, Script_Repr(taken)) : NULL;
		PyObject *cad = rep ? Script_Keep(&r, Script_Ask("string", "replace", "(OsOi)", lang, "<n>", rep, 1)) : NULL;
		PyObject *newMoney = cad ? Script_Keep(&r, PyNumber_Subtract(money, taken)) : NULL;
		int c = newMoney ? Script_NumCmp(newMoney, 0) : -2;
		if (c == -2) {
			return Script_Done(&r, NULL);
		}
		if (c >= 0) {
			if (!Script_Import(initmod)) {
				return Script_Done(&r, NULL);
			}
			if (indoor) {
				PyObject *path = Script_Keep(&r, Script_Attr("Init", "Path"));
				PyObject *tail = path ? Script_Keep(&r, PySequence_GetSlice(path, 7, INT_MAX)) : NULL;
				PyObject *spawn = tail ? Script_Keep(&r, Script_SPlus("DM_Player_Spawn_", tail)) : NULL;
				PyObject *local = spawn && Chal_SetSave("Spawn", spawn) &&
					Script_Try("Scrap", "SetSaveVar", "(ss)", "ComeFrom", "") && Script_Try("Menu", "MovieScorer", "(i)", 0) &&
					Script_Try("SInput", "SetActionSet", "(s)", "Inactive") ? Script_Keep(&r, Chal_Global("foeType")) : NULL;
				int out = local ? Chal_Outdoor() : -1;
				ok = out >= 0 && Chal_Spch(0, NULL, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0) &&
					Chal_OnEnd("LaunchChallenge");
			} else {
				PyObject *local = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
				int out = local ? Chal_Outdoor() : -1;
				ok = out >= 0 && Chal_Spch(1, local, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0) &&
					Chal_OnEnd("LaunchRevenge");
			}
			ok = ok && Script_Try("Scrap", "SetMoney", "(O)", newMoney) && Chal_SetSaveRepr("Challenge.Money", taken);
		} else {
			if (indoor) {
				ok = Chal_Spch(0, NULL, cad, "ChallengeRaceStdMsg", foe, "", 0);
			} else {
				PyObject *rfoe = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
				PyObject *rfoe2 = rfoe ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
				ok = rfoe2 && Chal_Spch(1, rfoe, cad, "ChallengeRaceStdMsg", rfoe2, "_Filter", 0);
			}
			ok = ok && Script_HSet(ch, "ArriOpt", PyInt_FromLong(0));
		}
		return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
	}
	/* the mercenaries take lives */
	int num = c1 == 0 ? 1 : c2 == 0 ? 3 : 0;
	if (!num) {
		return Script_Done(&r, Chal_PrintError() ? Py_ReturnNone() : NULL);
	}
	long takenLives = num == 1 ? 2 : 3;
	if (!Script_SetGlobal("Challenge", "numChallengeEnemies", PyInt_FromLong(num))) {
		return Script_Done(&r, NULL);
	}
	PyObject *g = Script_Keep(&r, Chal_Group(foe));
	PyObject *taken = g && Chal_Skills(type, g) ? Script_Keep(&r, PyInt_FromLong(takenLives)) : NULL;
	PyObject *lang = taken ? Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "ChallengeMercs")) : NULL;
	PyObject *rep = lang ? Script_Keep(&r, Script_Repr(taken)) : NULL;
	PyObject *cad = rep ? Script_Keep(&r, Script_Ask("string", "replace", "(OsOi)", lang, "<n>", rep, 1)) : NULL;
	PyObject *lives = cad ? Script_Keep(&r, Chal_SaveInt("Player.NumLives")) : NULL;
	PyObject *newLives = lives ? Script_Keep(&r, PyNumber_Subtract(lives, taken)) : NULL;
	int c = newLives ? Script_NumCmp(newLives, 0) : -2;
	if (c == -2) {
		return Script_Done(&r, NULL);
	}
	if (c >= 0) {
		if (!Script_Import(initmod)) {
			return Script_Done(&r, NULL);
		}
		if (indoor) {
			PyObject *local = Script_Try("Menu", "MovieScorer", "(i)", 0) && Script_Try("SInput", "SetActionSet", "(s)", "Inactive") ?
				Script_Keep(&r, Chal_Global("foeType")) : NULL;
			PyObject *lg = local ? Script_Keep(&r, Chal_Group(local)) : NULL;
			if (lg && Script_Is(lg, "Mercs")) {
				local = Script_Keep(&r, PyString_FromString("Mercs"));
			}
			int out = lg ? Chal_Outdoor() : -1;
			ok = out >= 0 && Chal_Spch(0, NULL, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 1) &&
				Chal_OnEnd("LaunchChallenge");
		} else {
			PyObject *local = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
			PyObject *lg = local ? Script_Keep(&r, Chal_Group(local)) : NULL;
			int out = lg ? Chal_Outdoor() : -1;
			if (out < 0) {
				ok = 0;
			} else if (Script_Is(lg, "Mercs")) {
				ok = Chal_Spch(1, local, cad, out ? "ChallengeRaceMercsMsg2_Filter" : "ChallengeRaceMercsMsg2", NULL, NULL, 1);
			} else {
				ok = Chal_Spch(1, local, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0);
			}
			ok = ok && Chal_OnEnd("LaunchRevenge");
		}
		ok = ok && Chal_SetSaveRepr("Player.NumLives", newLives) && Chal_SetSaveRepr("Challenge.Money", taken);
	} else {
		if (indoor) {
			ok = Chal_Spch(0, NULL, cad, "ChallengeMercsMsg", NULL, NULL, 1);
		} else {
			PyObject *rfoe = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
			ok = rfoe && Chal_Spch(1, rfoe, cad, "ChallengeMercsMsg_Filter", NULL, NULL, 1);
		}
		ok = ok && Script_HSet(ch, "ArriOpt", PyInt_FromLong(0));
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* SetRevengeParams(RevengeType, RevengeFoe, NumEnemies): the revenge into the save variables, the game restarts */
static PyObject *Challenge_SetRevengeParams(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "SaveGame", "string", NULL };
	PyObject *type, *foe, *num;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &type, &foe, &num) || !Script_Import(mods)) {
		return NULL;
	}
	if (!Chal_SetSave("Revenge.Type", type) || !Chal_SetSave("Revenge.Foe", foe) || !Chal_SetSave("Revenge.NumEnemies", num)) {
		return NULL;
	}
	PyObject *v = Script_AskKw("SaveGame", "StartGame", PyTuple_New(0), Py_BuildValue("{s:i}", "CleanChallenge", 1));
	Py_XDECREF(v);
	return v ? Py_ReturnNone() : NULL;
}

/* SetRevenge(playerLooses = 1): the nearest phone cab is marked and the foe calls (lost: unless in Humphrey's race
 * tutorial) */
static PyObject *Challenge_SetRevenge(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "MissionsFuncs", "PhoneCab", "Speech", NULL };
	PyObject *looses = NULL;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &looses) || !Script_Import(mods)) {
		return NULL;
	}
	looses = Script_Keep(&r, looses ? Script_Ref(looses) : PyInt_FromLong(1));
	int lost = Script_True(looses);
	if (lost < 0 || !Script_SetGlobal("Challenge", "playerWinsRevenge", PyInt_FromLong(!lost))) {
		return Script_Done(&r, NULL);
	}
	PyObject *player = Script_Keep(&r, Script_Usr());
	int t = Script_True(player);
	if (t <= 0) {
		return Script_Done(&r, t < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *pos = Script_Keep(&r, Script_GetAttr(player, "Pos"));
	PyObject *cab = pos ? Script_Keep(&r, Script_Ask("PhoneCab", "GetNearestPhoneCab", "(O)", pos)) : NULL;
	PyObject *tut = cab ? Script_Keep(&r, Script_SaveVar("HumphreyRaceTutorial")) : NULL;
	PyObject *foe = tut && Script_Try("MissionsFuncs", "ActivateTarget", "(Oi)", cab, Script_Is(tut, "1") ? 1 : 2) &&
		Script_Try("MissionsFuncs", "SetSecondary", "(i)", 1) ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
	PyObject *head = foe ? Script_Keep(&r, PySequence_GetSlice(foe, 0, 4)) : NULL;
	if (!head) {
		return Script_Done(&r, NULL);
	}
	if (Script_Is(head, "Merc")) {
		foe = Script_Keep(&r, PyString_FromString("Mercs"));
	}
	byte ok = 1;
	if (lost) {
		tut = Script_Keep(&r, Script_SaveVar("HumphreyRaceTutorial"));
		if (!tut) {
			return Script_Done(&r, NULL);
		}
		if (!Script_Is(tut, "1")) {
			PyObject *text = Script_Keep(&r, Script_SPlus("ChallengeLooseRevenge", foe));
			PyObject *who = text ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
			ok = who && Script_Try("Speech", "AddRemoteCom", "(OO)", text, who);
		}
	} else {
		PyObject *text = Script_Keep(&r, Script_SPlus("ChallengeWinRevenge", foe));
		PyObject *who = text ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
		ok = who && Script_Try("Speech", "AddRemoteCom", "(OO)", text, who);
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* LaunchRevenge(): the end of the revenge talk: the revenge becomes the challenge and starts here */
static PyObject *Challenge_LaunchRevenge(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "PhoneCab", "MissionsFuncs", NULL };
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	PyObject *type = Script_Keep(&r, Script_SaveVar("Revenge.Type"));
	PyObject *foe = type && Chal_SetSave("Challenge.Type", type) ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
	PyObject *num = foe && Chal_SetSave("Challenge.Foe", foe) ? Script_Keep(&r, Chal_Global("numChallengeEnemies")) : NULL;
	PyObject *ac = num && Chal_SetSaveRepr("Challenge.NumEnemies", num) &&
		Script_Try("Scrap", "SetSaveVar", "(ss)", "Revenge.Type", "") && Script_Try("Scrap", "SetSaveVar", "(ss)", "Revenge.Foe", "") &&
		Script_Try("PhoneCab", "ForceResetPhoneCab", "()") ? Script_Keep(&r, Script_Attr("PhoneCab", "audioComEntity")) : NULL;
	PyObject *a = ac && Script_Try("MissionsFuncs", "ActivateTarget", "(Oi)", ac, 0) ? Script_Keep(&r, Py_BuildValue("(i)", 1)) : NULL;
	byte ok = a && Script_Keep(&r, Challenge_Init(NULL, a));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* LaunchChallenge(): the end of the challenge talk: the challenge into the save variables and the game goes on in
 * the outdoor level of this area (when it is the mission's, the one next to it) */
static PyObject *Challenge_LaunchChallenge(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "SaveGame", "MissionsFuncs", "string", NULL };
	static const char *const phone[] = { "PhoneCab", NULL };
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	PyObject *num = Script_Keep(&r, Chal_Global("numChallengeEnemies"));
	PyObject *type = num && Chal_SetSaveRepr("Challenge.NumEnemies", num) ? Script_Keep(&r, Chal_Global("challengeType")) : NULL;
	PyObject *foe = type && Chal_SetSave("Challenge.Type", type) ? Script_Keep(&r, Chal_Global("foeType")) : NULL;
	PyObject *out = foe && Chal_SetSave("Challenge.Foe", foe) ? Script_Keep(&r, Script_Attr("Init", "Outdoor")) : NULL;
	int t = Script_True(out);
	if (t > 0 && (!Script_Import(phone) || !Script_Try("PhoneCab", "ForceResetPhoneCab", "()"))) {
		t = -1;
	}
	PyObject *area = t >= 0 && Script_Try("MissionsFuncs", "SetSecondary", "(i)", 1) ? Script_Keep(&r, Script_Attr("Init", "OutArea")) : NULL;
	PyObject *empty = area ? Script_Keep(&r, PyString_FromString("")) : NULL;
	t = empty ? Script_Eq(area, empty) : -1;
	if (t != 0) {
		return Script_Done(&r, t > 0 ? Py_ReturnNone() : NULL);
	}
	PyObject *sv = Script_Keep(&r, Script_SaveVar("Mission.Map"));
	PyObject *mm = sv ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", sv)) : NULL;
	area = mm ? Script_Keep(&r, Script_Attr("Init", "OutArea")) : NULL;
	PyObject *oa = area ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", area)) : NULL;
	PyObject *items[2] = { mm, oa };
	t = oa && Script_Print(items, 2) ? Script_Eq(mm, oa) : -1;
	if (t > 0) {
		mm = Script_Keep(&r, PyString_FromString(Script_Is(mm, "DOWNTOWN") ? "Commercial" : "DownTown"));
	} else if (t == 0) {
		mm = Script_Keep(&r, Script_Attr("Init", "OutArea"));
	}
	PyObject *level = t >= 0 && mm ? Script_Keep(&r, Script_SPlus("Levels/", mm)) : NULL;
	PyObject *path = level ? Script_Keep(&r, Script_Attr("Init", "Path")) : NULL;
	PyObject *tail = path ? Script_Keep(&r, PySequence_GetSlice(path, 7, INT_MAX)) : NULL;
	PyObject *spawn = tail ? Script_Keep(&r, Script_SPlus("DM_Player_Spawn_", tail)) : NULL;
	byte ok = spawn && Script_Try("SaveGame", "MarkWayPoint", "(OOs)", level, spawn, "") &&
		Script_Keep(&r, Script_AskKw("SaveGame", "StartGame", PyTuple_New(0), Py_BuildValue("{s:i}", "CleanChallenge", 0)));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* the end of a challenge or revenge (prefix "Challenge" or "Revenge"): no secondary mission, and its save variables
 * empty */
static PyObject *Chal_Clean(const char *prefix, byte revenge) {
	static const char *const mf[] = { "MissionsFuncs", NULL };
	static const char *const sg[] = { "SaveGame", NULL };
	static const char *const vars[] = { "Type", "Foe", "NumEnemies", "LastWinner" };
	char name[32];
	Str_Printf(name, sizeof(name), "%s.Type", prefix);
	PyObject *type = Script_SaveVar(name);
	byte ok = type != NULL;
	if (ok && !Script_Is(type, "")) {
		ok = Script_Import(mf) && Script_Try("MissionsFuncs", "SetSecondary", "(i)", 0) && Script_Import(sg) &&
			Script_Try("SaveGame", "SMDesc", "(si)", "", 1);
	}
	Py_XDECREF(type);
	for (int i = 0; ok && i < (revenge ? 4 : 3); i++) {
		Str_Printf(name, sizeof(name), "%s.%s", prefix, vars[i]);
		ok = Script_Try("Scrap", "SetSaveVar", "(ss)", name, "");
	}
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *Challenge_CleanChallenge(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Chal_Clean("Challenge", 0);
}

static PyObject *Challenge_CleanRevenge(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Chal_Clean("Revenge", 1);
}

/* the vehicles of a cancelled challenge (the names in seq) go back to the police or the traffic: 0 with the error
 * set */
static byte Chal_Release(PyObject *seq) {
	PyObject *name;
	int more;
	for (int i = 0; (more = Script_Next(seq, i, &name)) > 0; i++) {
		scriptRefs_t r = { 0 };
		Script_Keep(&r, name);
		PyObject *veh = Script_Keep(&r, Script_Entity(name));
		PyObject *model = veh && Script_HSet(veh, "MoveTime", PyFloat_FromDouble(0.0)) &&
			Script_HSet(veh, "TgType", PyString_FromString("None")) ? Script_Keep(&r, Script_GetAttr(veh, "Model")) : NULL;
		PyObject *at = model ? Script_Keep(&r, Script_Ask("string", "find", "(Os)", model, "SPoli")) : NULL;
		int c = at ? Script_NumCmp(at, 0) : -2;
		PyObject *vname = c != -2 && Script_HSet(veh, "EntityList", PyString_FromString(c >= 0 ? "OutCop" : "Traffic")) ?
			Script_Keep(&r, Script_GetAttr(veh, "Name")) : NULL;
		byte ok = vname && Script_Try("SAI", "SetStateVehicle", "(iO)", 3, vname);
		Script_Done(&r, NULL);
		if (!ok) {
			return 0;
		}
	}
	return more == 0;
}

/* CancelChallenge(challType): the player left the race or the combat: the bet is lost, the foes go back to their
 * lists and a revenge is pending */
static PyObject *Challenge_CancelChallenge(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Speech", "Menu", "Parking", "Alarm", "RaceMaker", "string", NULL };
	PyObject *type;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &type) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *lang = Script_Try("Speech", "CancelRemoteCom", "()") && Script_Try("Scrap", "DeleteScheduledFuncs", "(s)", "SF_Secondary") ?
		Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "RaceSystemCancel1")) : NULL;
	PyObject *money = lang ? Script_Keep(&r, Script_SaveVar("Challenge.Money")) : NULL;
	PyObject *text = money ? Script_Keep(&r, Script_Ask("string", "replace", "(OsO)", lang, "<n>", money)) : NULL;
	PyObject *foe = text && Script_Try("RaceMaker", "ShowSystemMessage", "(O)", text) ?
		Script_Keep(&r, Script_SaveVar("Challenge.Foe")) : NULL;
	PyObject *left = foe ? Script_Keep(&r, Script_SPlus("RaceLeft", foe)) : NULL;
	PyObject *who = left ? Script_Keep(&r, Script_SaveVar("Challenge.Foe")) : NULL;
	PyObject *set = who && Script_Try("Speech", "AddRemoteCom", "(OO)", left, who) ? Script_Keep(&r, Script_Ask("SInput", "GetActionSet", "()")) : NULL;
	if (!set) {
		return Script_Done(&r, NULL);
	}
	Script_SetVar("Menu", "MissionScorerCall", Script_Ref(Py_None));
	if (!Script_Try("Menu", "DrawScorer", "(iO)", 0, set)) {
		return Script_Done(&r, NULL);
	}
	byte ok = 1;
	if (Script_Is(type, "Race")) {
		PyObject *cp = Script_Keep(&r, Script_Ask("Scrap", "GetEntity", "(s)", "CheckPoint"));
		int t = Script_True(cp);
		if (t > 0) {
			t = Script_HSet(cp, "OnTake", PyString_FromString("")) && Script_Keep(&r, Script_Method(cp, "Del", "()")) ? 1 : -1;
		}
		PyObject *players = t >= 0 ? Script_Keep(&r, Script_Attr("RaceMaker", "Players")) : NULL;
		ok = players && Chal_Release(players);
	} else if (Script_Is(type, "Combat")) {
		PyObject *npcs = Script_Keep(&r, Script_Attr("RaceMaker", "challenged_NPCs_Names"));
		ok = npcs && Chal_Release(npcs);
	}
	PyObject *cfoe = ok && Chal_SetSave("Revenge.Type", type) ? Script_Keep(&r, Script_SaveVar("Challenge.Foe")) : NULL;
	PyObject *cnum = cfoe && Chal_SetSave("Revenge.Foe", cfoe) ? Script_Keep(&r, Script_SaveVar("Challenge.NumEnemies")) : NULL;
	PyObject *a = cnum && Chal_SetSave("Revenge.NumEnemies", cnum) ? Script_Keep(&r, PyTuple_New(0)) : NULL;
	if (!a || !Script_Keep(&r, Challenge_SetRevenge(NULL, a))) {
		return Script_Done(&r, NULL);
	}
	Script_SetVar("Parking", "SwitchCharMissionCallback", Script_Ref(Py_None));
	Script_SetVar("Parking", "SwitchVehicleMissionCallback", Script_Ref(Py_None));
	Script_SetVar("Alarm", "AbortMissionCallback", Script_Ref(Py_None));
	return Script_Done(&r, Chal_Clean("Challenge", 0));
}

/* ch.var = prefix + string.replace(cad, "<n>", n, 1): 0 with the error set */
static byte Chal_Option(PyObject *ch, const char *var, const char *prefix, PyObject *cad, PyObject *n) {
	PyObject *s = n ? Script_Ask("string", "replace", "(OsOi)", cad, "<n>", n, 1) : NULL;
	byte ok = s && Script_HSet(ch, var, Script_SPlus(prefix, s));
	Py_XDECREF(s);
	return ok;
}

/* ch.var = prefix + `ComputeReward(type, foe, num)` with the text cad: 0 with the error set */
static byte Chal_RewardOption(PyObject *ch, const char *var, const char *prefix, PyObject *cad, PyObject *type, PyObject *foe,
	int num) {
	PyObject *n = PyInt_FromLong(num);
	PyObject *v = n ? Chal_Reward(type, foe, n) : NULL;
	PyObject *rep = Script_Repr(v);
	byte ok = rep && Chal_Option(ch, var, prefix, cad, rep);
	Py_XDECREF(rep);
	Py_XDECREF(v);
	Py_XDECREF(n);
	return ok;
}

/* ch.var = prefix + Scrap.GetLangStr(key): 0 with the error set */
static byte Chal_LangOption(PyObject *ch, const char *var, const char *prefix, const char *key) {
	PyObject *s = Script_Ask("Scrap", "GetLangStr", "(s)", key);
	byte ok = s && Script_HSet(ch, var, Script_SPlus(prefix, s));
	Py_XDECREF(s);
	return ok;
}

byte Challenge_SetOptions(PyObject *ch, PyObject *type, PyObject *foe) {
	scriptRefs_t r = { 0 };
	PyObject *group = Script_Keep(&r, Chal_Group(foe));
	byte press = Script_Is(group, "Press"), mercs = 0;
	if (group && !press) {
		/* the elif looks the group up again */
		group = Script_Keep(&r, Chal_Group(foe));
		mercs = Script_Is(group, "Mercs");
	}
	PyObject *cad = group ? Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", mercs ? "ChallengeMercs" : "ChallengeRaceStdOpt1")) : NULL;
	byte ok = cad && Script_HSet(ch, "ArrnOpt", PyInt_FromLong(0));
	if (ok && mercs) {
		PyObject *two = Script_Keep(&r, PyString_FromString("2")), *three = Script_Keep(&r, PyString_FromString("3"));
		ok = Chal_Option(ch, "ArrOpt0", "ChallengeRaceStdOpt1:", cad, two) && Chal_Option(ch, "ArrOpt1", "ChallengeRaceStdOpt2:", cad, three) &&
			Chal_LangOption(ch, "ArrOpt2", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2");
	} else if (ok) {
		ok = Chal_RewardOption(ch, "ArrOpt0", "ChallengeRaceStdOpt1:", cad, type, foe, 1) &&
			Chal_RewardOption(ch, "ArrOpt1", "ChallengeRaceStdOpt2:", cad, type, foe, 3) &&
			(press ? Chal_LangOption(ch, "ArrOpt2", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2") :
			Chal_RewardOption(ch, "ArrOpt2", "ChallengeRaceStdOpt3:", cad, type, foe, 5) &&
			Chal_LangOption(ch, "ArrOpt3", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2"));
	}
	ok = ok && Script_HSet(ch, "ArriOpt", PyInt_FromLong(0));
	Script_Done(&r, NULL);
	return ok;
}

/* SetChallengeOptions(ch): the options of the challenge of challengeType against foeType */
static PyObject *Challenge_SetChallengeOptions(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "string", "Challenge", NULL };
	PyObject *ch;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *foe = Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "ChallengeRaceStdOpt1")) ?
		Script_Keep(&r, Script_Attr("Challenge", "foeType")) : NULL;
	PyObject *type = foe ? Script_Keep(&r, Script_Attr("Challenge", "challengeType")) : NULL;
	byte ok = type && Challenge_SetOptions(ch, type, foe);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* AddChallenge(ch, UsrName, iopt): the race and combat options at iopt and iopt + 1, when challenges are on and the
 * player and the character can drive (Humphrey always) */
static PyObject *Challenge_AddChallenge(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Chars", NULL };
	PyObject *ch, *usr, *iopt;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ch, &usr, &iopt) || !Script_Import(mods)) {
		return NULL;
	}
	int t = Script_True(Script_Keep(&r, Chal_SaveInt("Challenge")));
	if (t > 0) {
		PyObject *u = Script_Keep(&r, Script_Entity(usr));
		t = Script_True(u ? Script_Keep(&r, Script_Ask("Chars", "CanDrive", "(O)", u)) : NULL);
	}
	if (t > 0) {
		t = Script_True(Script_Keep(&r, Script_Ask("Chars", "CanDrive", "(O)", ch)));
		if (t == 0) {
			t = Script_HIs(ch, "ActCtrl", "Humphrey");
		}
	}
	for (int i = 0; t > 0 && i < 2; i++) {
		/* exec('ch.ArrOpt' + `iopt + i` + ' = ...') */
		PyObject *n = Script_Keep(&r, PyInt_FromLong(i));
		PyObject *sum = n ? Script_Keep(&r, PyNumber_Add(iopt, n)) : NULL;
		PyObject *rep = sum ? Script_Keep(&r, Script_Repr(sum)) : NULL;
		if (!rep) {
			return Script_Done(&r, NULL);
		}
		const char *s = PyString_AsString(rep);
		byte digits = *s != 0;
		for (const char *p = s; *p; p++) {
			digits &= *p >= '0' && *p <= '9';
		}
		if (!digits) {
			PyErr_SetString(PyExc_SyntaxError, "invalid syntax");
			return Script_Done(&r, NULL);
		}
		char var[48];
		Str_Printf(var, sizeof(var), "ArrOpt%s", s);
		t = Chal_LangOption(ch, var, i ? "ChallengeCombatStdOpt0:" : "ChallengeRaceStdOpt0:",
			i ? "ChallengeCombatStdOpt0" : "ChallengeRaceStdOpt0") ? 1 : -1;
	}
	return Script_Done(&r, t >= 0 ? Py_ReturnNone() : NULL);
}

PyMethodDef g_ChallengeMethods[] = {
	{ "ComputeReward", Challenge_ComputeReward, 1, NULL },
	{ "Init", Challenge_Init, 1, NULL },
	{ "SetChallengerSkills", Challenge_SetChallengerSkills, 1, NULL },
	{ "OnChallengeOpt", Challenge_OnChallengeOpt, 1, NULL },
	{ "SetRevengeParams", Challenge_SetRevengeParams, 1, NULL },
	{ "SetRevenge", Challenge_SetRevenge, 1, NULL },
	{ "LaunchRevenge", Challenge_LaunchRevenge, 1, NULL },
	{ "LaunchChallenge", Challenge_LaunchChallenge, 1, NULL },
	{ "CleanChallenge", Challenge_CleanChallenge, 1, NULL },
	{ "CleanRevenge", Challenge_CleanRevenge, 1, NULL },
	{ "CancelChallenge", Challenge_CancelChallenge, 1, NULL },
	{ "SetChallengeOptions", Challenge_SetChallengeOptions, 1, NULL },
	{ "AddChallenge", Challenge_AddChallenge, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
