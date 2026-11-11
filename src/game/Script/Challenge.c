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
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the globals where the
 * script reads them and write them back at once (OBJLIB 5). The save variables, the money and the language texts are C;
 * Scrap.SetMoney, UsrEntity, GetEntity, DeleteScheduledFuncs and SInput.SetActionSet go through their seams and the
 * characters and vehicles are handle values (OBJLIB 7). The other ports (RaceMaker, Speech, Menu, SaveGame,
 * MissionsFuncs, PhoneCab, Chars) are called through their refs, the keyword calls too (PORT_CALL, stage 3), and
 * string.replace with its count and string.find through the native twins; SAI.SetStateVehicle and
 * SInput.GetActionSet go through their refs (stage 7); the variables of other modules that a test or the game may set
 * to any object (Init.Outdoor, Speech.ObjEntity, PhoneCab.audioComEntity, RaceMaker.Players) are read as values of any
 * type.
 */
#include <limits.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define CHAL_ERROR "ERROR: Par\xe1metro incorrecto pasado a 'OnChallenge'"

/* a global of the module as its functions read it (out None before) */
static byte Chal_G(const char *name, nativeVal_t *out) {
	return Mode_G("Challenge", name, out);
}

/* groups[foe] */
static byte Chal_Group(const nativeVal_t *foe, nativeVal_t *out) {
	nativeVal_t groups = { 0 };
	byte ok = Chal_G("groups", &groups) && Mode_Sub(&groups, foe, out);
	Native_Free(&groups);
	return ok;
}

/* Scrap.SetSaveVar(name, `v`) */
static byte Chal_SetSaveRepr(const char *name, const nativeVal_t *v) {
	nativeVal_t r;
	Mode_Repr(v, &r);
	byte ok = Mode_SetSaveVar(name, &r);
	Native_Free(&r);
	return ok;
}

/* the number of challenges of a group so far, from the save variable name (0 when it is empty) */
static byte Chal_Total(const nativeVal_t *name, long *total) {
	nativeVal_t sv = { 0 }, i = { 0 };
	const char *s;
	*total = 0;
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_SaveVar(s, &sv);
	byte ok = !Native_True(&sv) || Native_ToInt(&sv, &i);
	if (ok && i.type == NV_INT) {
		*total = i.i;
	}
	Native_Free(&sv);
	Native_Free(&i);
	return ok;
}

/* ComputeReward(_type, foe, numEnemies): the money a challenge costs (and pays), less after many of the group */
static byte Chal_Reward(const nativeVal_t *type, const nativeVal_t *foe, const nativeVal_t *num, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *group = Mode_V(&t), *name = Mode_V(&t), *table = Mode_V(&t), *base = Mode_V(&t), *f = Mode_V(&t);
	nativeVal_t *b2 = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t);
	byte combat = Native_IsStr(type, "Combat");
	long total;
	if (!Chal_Group(foe, group) || !Mode_Cat(&t, name, 2, Mode_S(&t, combat ? "Stats.TotalCombats." : "Stats.TotalRaces."),
		group) || !Chal_Total(name, &total) || !Chal_G(combat ? "combatRewards" : "raceRewards", table) ||
		!Mode_Sub(table, group, base)) {
		return Mode_Done(&t, 0);
	}
	if (total >= 5) {
		Native_SetFloat(f, total < 8 ? 0.8 : total < 10 ? 0.6 : 0.4);
		if (!Mode_Arith('*', base, f, b2)) {
			return Mode_Done(&t, 0);
		}
		base = b2;
	}
	int c = Mode_CmpI(num, "==", 1);
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	if (c) {
		return Mode_Done(&t, Native_ToInt(base, out));
	}
	Native_SetFloat(f, 1.2);
	return Mode_Done(&t, Mode_Arith('*', f, base, a) && Mode_Arith('*', a, num, b) && Native_ToInt(b, out));
}

static byte Chal_SetRevenge(const nativeVal_t *looses);

/* Init(Revenge = 0): the race or the combat of the save variables Challenge.*, else the revenge pending (RaceMaker
 * with its keywords, in the order of the script's keyword dicts) */
static byte Chal_Init(const nativeVal_t *revengeArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *revenge = Mode_V(&t), *type = Mode_V(&t), *foe = Mode_V(&t), *money = Mode_V(&t), *num = Mode_V(&t);
	if (!Mode_Import("RaceMaker")) {
		return Mode_Done(&t, 0);
	}
	if (revengeArg) {
		Native_Copy(revenge, revengeArg);
	} else {
		Native_SetInt(revenge, 0);
	}
	Mode_SaveVar("Challenge.Type", type);
	Mode_SaveVar("Challenge.Foe", foe);
	if (Native_IsStr(type, "Race")) {
		Mode_SaveVar("Challenge.Money", money);
		if (!Mode_SaveInt("Challenge.NumEnemies", num)) {
			return Mode_Done(&t, 0);
		}
		int one = Mode_CmpI(num, "==", 1), three = one == 0 ? Mode_CmpI(num, "==", 3) : 0;
		if (one < 0 || three < 0) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, PORT_CALL(RaceMaker, InitRace, NULL, ";waypointlimit=i,numply=i,foeType=v,isRevenge=v", 15L,
			one ? 2L : three ? 3L : 4L, foe, revenge));
	}
	if (Native_IsStr(type, "Combat")) {
		if (!Mode_SaveInt("Challenge.NumEnemies", num)) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, PORT_CALL(RaceMaker, InitCombat, NULL, "vv;numFoes=v", foe, revenge, num));
	}
	Mode_SaveVar("Revenge.Type", money);
	Mode_SaveVar("Revenge.Foe", num);
	return Mode_Done(&t, Native_IsStr(money, "") || Native_IsStr(num, "") || Chal_SetRevenge(NULL));
}

/* the skill of the challengers by the number of challenges against the group: Race.Profile and Combat.Profile */
static byte Chal_Skills(const nativeVal_t *type, const nativeVal_t *group) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	long total;
	if (!Mode_Cat(&t, name, 4, Mode_S(&t, "Stats.Total"), type, Mode_S(&t, "s."), group) || !Chal_Total(name, &total)) {
		return Mode_Done(&t, 0);
	}
	const char *v = total < 5 ? "Pilot" : total < 8 ? "Veteran" : total < 10 ? "Elite" : "MegaAce";
	char race[32];
	Str_Printf(race, sizeof(race), "Race_%s", v);
	return Mode_Done(&t, Mode_SetSaveVar("Race.Profile", Mode_S(&t, race)) && Mode_SetSaveVar("Combat.Profile", Mode_S(&t, v)));
}

/* the talk of the bet: Speech.OnChallengeSpch(cad, ans) indoors, Speech.OnChallengeObjSpch(Speech.ObjEntity, foe,
 * cad, ans) at a phone cab (obj), isMerc=1 for the mercenaries. ans is prefix + name + suffix (name NULL: prefix
 * only) */
static byte Chal_Spch(byte obj, const nativeVal_t *foe, const nativeVal_t *cad, const char *prefix, const nativeVal_t *name,
	const char *suffix, byte merc) {
	modeVals_t t = { .n = 0 };
	/* the arguments in a row: (ent, foe, cad, ans) at a phone cab, (cad, ans) indoors */
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *c = Mode_V(&t), *d = Mode_V(&t), *ans = obj ? d : b;
	if (obj && !LvN_ModGet("Speech", "ObjEntity", a)) {
		return Mode_Done(&t, 0);
	}
	if (name) {
		if (!Mode_Cat(&t, ans, 3, Mode_S(&t, prefix), name, Mode_S(&t, suffix))) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetStr(ans, prefix);
	}
	if (obj) {
		Native_Copy(b, foe);
		Native_Copy(c, cad);
	} else {
		Native_Copy(a, cad);
	}
	if (obj) {
		return Mode_Done(&t, merc ? PORT_CALL(Speech, OnChallengeObjSpch, NULL, "vvvv;isMerc=i", a, b, c, d, 1L) :
			PORT_CALL(Speech, OnChallengeObjSpch, NULL, "vvvv", a, b, c, d));
	}
	return Mode_Done(&t, merc ? PORT_CALL(Speech, OnChallengeSpch, NULL, "vv;isMerc=i", a, b, 1L) :
		PORT_CALL(Speech, OnChallengeSpch, NULL, "vv", a, b));
}

/* Speech.OnEndOfSpeech = the function name of the module (OBJLIB 5: the function object itself) */
static byte Chal_OnEnd(const char *name) {
	return Mode_ModSetGlobal("Speech", "OnEndOfSpeech", "Challenge", name);
}

/* Init.Outdoor == 1: 1, 0, -1 with the error */
static int Chal_Outdoor(void) {
	nativeVal_t o;
	int c = LvN_ModGet("Init", "Outdoor", &o) ? Mode_CmpI(&o, "==", 1) : -1;
	Native_Free(&o);
	return c;
}

/* string.replace(cad, "<n>", n, 1) (the native twin, with the count of the script) */
static byte Chal_Replace1(const nativeVal_t *cad, const nativeVal_t *n, nativeVal_t *out) {
	return PORT_CALL(string, replace, out, "vsvi", cad, "<n>", n, 1L);
}

/* "DM_Player_Spawn_" + Init.Path[7:] */
static byte Chal_Spawn(nativeVal_t *out) {
	nativeVal_t path = { 0 }, tail = { 0 }, pre;
	Native_SetStr(&pre, "DM_Player_Spawn_");
	byte ok = LvN_ModGet("Init", "Path", &path) && Native_Slice(&path, 7, INT_MAX, &tail) && Native_Arith('+', &pre, &tail, out);
	Native_Free(&path);
	Native_Free(&tail);
	return ok;
}

/* Menu.MovieScorer(0); SInput.SetActionSet("Inactive") */
static byte Chal_Movie(void) {
	return PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) && Mode_ActionSet("Inactive");
}

/* the bet taken by a foe that is no mercenary (num 1, 3 or 5 foes) */
static byte Chal_BetMoney(int num, int indoor, const nativeVal_t *ch, const nativeVal_t *type, const nativeVal_t *foe,
	const nativeVal_t *money) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *taken = Mode_V(&t), *g = Mode_V(&t), *lang = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *cad = Mode_V(&t), *newMoney = Mode_V(&t), *spawn = Mode_V(&t), *local = Mode_V(&t), *rfoe = Mode_V(&t);
	nativeVal_t *rfoe2 = Mode_V(&t);
	if (!LvN_PutI("Challenge", "numChallengeEnemies", num) || !Chal_G("numChallengeEnemies", n) ||
		!Chal_Reward(type, foe, n, taken) || !Chal_Group(foe, g) || !Chal_Skills(type, g)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("ChallengeRaceStdOpt1", lang);
	Mode_Repr(taken, rep);
	int c = Chal_Replace1(lang, rep, cad) && Mode_Arith('-', money, taken, newMoney) ? Mode_CmpI(newMoney, ">=", 0) : -1;
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (c) {
		if (!Mode_Import("Init")) {
			return Mode_Done(&t, 0);
		}
		if (indoor) {
			int out = Chal_Spawn(spawn) && Mode_SetSaveVar("Spawn", spawn) && Mode_SetSaveVar("ComeFrom", Mode_S(&t, "")) &&
				Chal_Movie() && Chal_G("foeType", local) ? Chal_Outdoor() : -1;
			ok = out >= 0 && Chal_Spch(0, NULL, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0) &&
				Chal_OnEnd("LaunchChallenge");
		} else {
			Mode_SaveVar("Revenge.Foe", local);
			int out = Chal_Outdoor();
			ok = out >= 0 && Chal_Spch(1, local, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0) &&
				Chal_OnEnd("LaunchRevenge");
		}
		return Mode_Done(&t, ok && Mode_SetMoneySeam(newMoney) && Chal_SetSaveRepr("Challenge.Money", taken));
	}
	if (indoor) {
		ok = Chal_Spch(0, NULL, cad, "ChallengeRaceStdMsg", foe, "", 0);
	} else {
		Mode_SaveVar("Revenge.Foe", rfoe);
		Mode_SaveVar("Revenge.Foe", rfoe2);
		ok = Chal_Spch(1, rfoe, cad, "ChallengeRaceStdMsg", rfoe2, "_Filter", 0);
	}
	return Mode_Done(&t, ok && LvN_SetI(ch, "ArriOpt", 0));
}

/* the bet of the mercenaries: lives (num 1 or 3 foes for 2 or 3 lives) */
static byte Chal_BetLives(int num, int indoor, const nativeVal_t *ch, const nativeVal_t *type, const nativeVal_t *foe) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *g = Mode_V(&t), *taken = Mode_V(&t), *lang = Mode_V(&t), *rep = Mode_V(&t), *cad = Mode_V(&t);
	nativeVal_t *lives = Mode_V(&t), *newLives = Mode_V(&t), *local = Mode_V(&t), *lg = Mode_V(&t), *rfoe = Mode_V(&t);
	if (!LvN_PutI("Challenge", "numChallengeEnemies", num) || !Chal_Group(foe, g) || !Chal_Skills(type, g)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(taken, num == 1 ? 2 : 3);
	Mode_Lang("ChallengeMercs", lang);
	Mode_Repr(taken, rep);
	int c = Chal_Replace1(lang, rep, cad) && Mode_SaveInt("Player.NumLives", lives) &&
		Mode_Arith('-', lives, taken, newLives) ? Mode_CmpI(newLives, ">=", 0) : -1;
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (c) {
		if (!Mode_Import("Init")) {
			return Mode_Done(&t, 0);
		}
		if (indoor) {
			int out = Chal_Movie() && Chal_G("foeType", local) && Chal_Group(local, lg) ? 1 : -1;
			if (out > 0 && Native_IsStr(lg, "Mercs")) {
				Native_Free(local);
				Native_SetStr(local, "Mercs");
			}
			out = out > 0 ? Chal_Outdoor() : -1;
			ok = out >= 0 && Chal_Spch(0, NULL, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 1) &&
				Chal_OnEnd("LaunchChallenge");
		} else {
			Mode_SaveVar("Revenge.Foe", local);
			int out = Chal_Group(local, lg) ? Chal_Outdoor() : -1;
			if (out < 0) {
				ok = 0;
			} else if (Native_IsStr(lg, "Mercs")) {
				ok = Chal_Spch(1, local, cad, out ? "ChallengeRaceMercsMsg2_Filter" : "ChallengeRaceMercsMsg2", NULL, NULL, 1);
			} else {
				ok = Chal_Spch(1, local, cad, "ChallengeRace", local, out ? "Msg2_Filter" : "Msg2", 0);
			}
			ok = ok && Chal_OnEnd("LaunchRevenge");
		}
		return Mode_Done(&t, ok && Chal_SetSaveRepr("Player.NumLives", newLives) && Chal_SetSaveRepr("Challenge.Money", taken));
	}
	if (indoor) {
		ok = Chal_Spch(0, NULL, cad, "ChallengeMercsMsg", NULL, NULL, 1);
	} else {
		Mode_SaveVar("Revenge.Foe", rfoe);
		ok = Chal_Spch(1, rfoe, cad, "ChallengeMercsMsg_Filter", NULL, NULL, 1);
	}
	return Mode_Done(&t, ok && LvN_SetI(ch, "ArriOpt", 0));
}

/* OnChallengeOpt(opt, ch, challengeLaunchedIndoor = 1): the bet of option 1, 2 or 3 (1, 3 or 5 foes; 1 or 3 for the
 * mercenaries, for 2 or 3 lives). With enough money (or lives) the talk ends in the challenge (indoors) or the
 * revenge (at a phone cab) and the bet is taken; else the foe laughs and the options start again */
static byte Chal_OnChallengeOpt(const nativeVal_t *opt, const nativeVal_t *ch, const nativeVal_t *indoorArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *foe = Mode_V(&t), *type = Mode_V(&t), *group = Mode_V(&t);
	if (!Mode_Import("Speech") || !Mode_Import("Menu") || !Mode_Import("string")) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(money, Mode_GetMoney());
	int indoor = indoorArg ? Native_True(indoorArg) : 1;
	if (indoor) {
		if (!Chal_G("foeType", foe) || !Chal_G("challengeType", type)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Mode_SaveVar("Challenge.Foe", foe);
		if (Native_IsStr(foe, "")) {
			Native_Free(foe);
			Mode_SaveVar("Revenge.Foe", foe);
		}
		Mode_SaveVar("Challenge.Type", type);
		if (Native_IsStr(type, "")) {
			Native_Free(type);
			Mode_SaveVar("Revenge.Type", type);
		}
	}
	if (!Chal_Group(foe, group)) {
		return Mode_Done(&t, 0);
	}
	byte mercs = Native_IsStr(group, "Mercs");
	int c1 = Mode_CmpI(opt, "==", 1), c2 = c1 ? 0 : Mode_CmpI(opt, "==", 2);
	int c3 = !c1 && !c2 && !mercs ? Mode_CmpI(opt, "==", 3) : 0;
	if (c1 < 0 || c2 < 0 || c3 < 0) {
		return Mode_Done(&t, 0);
	}
	int num = c1 > 0 ? 1 : c2 > 0 ? 3 : c3 > 0 && !mercs ? 5 : 0;
	if (!num) {
		return Mode_Done(&t, Mode_PrintS(CHAL_ERROR));
	}
	return Mode_Done(&t, mercs ? Chal_BetLives(num, indoor, ch, type, foe) : Chal_BetMoney(num, indoor, ch, type, foe, money));
}

/* SetRevengeParams(RevengeType, RevengeFoe, NumEnemies): the revenge into the save variables, the game restarts
 * (SaveGame.StartGame with its keyword) */
static byte Chal_SetRevengeParams(const nativeVal_t *type, const nativeVal_t *foe, const nativeVal_t *num) {
	return Mode_Import("Init") && Mode_Import("SaveGame") && Mode_Import("string") && Mode_SetSaveVar("Revenge.Type", type) &&
		Mode_SetSaveVar("Revenge.Foe", foe) && Mode_SetSaveVar("Revenge.NumEnemies", num) &&
		PORT_CALL(SaveGame, StartGame, NULL, ";CleanChallenge=i", 1L);
}

/* SetRevenge(playerLooses = 1): the nearest phone cab is marked and the foe calls (lost: unless in Humphrey's race
 * tutorial) */
static byte Chal_SetRevenge(const nativeVal_t *looses) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *player = Mode_V(&t), *pos = Mode_V(&t), *cab = Mode_V(&t), *tut = Mode_V(&t), *foe = Mode_V(&t);
	nativeVal_t *head = Mode_V(&t), *text = Mode_V(&t), *who = Mode_V(&t);
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("PhoneCab") || !Mode_Import("Speech")) {
		return Mode_Done(&t, 0);
	}
	int lost = looses ? Native_True(looses) : 1;
	if (!LvN_PutI("Challenge", "playerWinsRevenge", !lost) || !Mode_UsrHandle(player)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(player)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VObj(player, "Pos", pos) || !PORT_CALL(PhoneCab, GetNearestPhoneCab, cab, "v", pos)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("HumphreyRaceTutorial", tut);
	if (!PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", cab, Native_IsStr(tut, "1") ? 1L : 2L) ||
		!PORT_CALL(MissionsFuncs, SetSecondary, NULL, "i", 1L)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Revenge.Foe", foe);
	if (!Native_Slice(foe, 0, 4, head)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(head, "Merc")) {
		Native_Free(foe);
		Native_SetStr(foe, "Mercs");
	}
	if (lost) {
		Native_Free(tut);
		Mode_SaveVar("HumphreyRaceTutorial", tut);
		if (Native_IsStr(tut, "1")) {
			return Mode_Done(&t, 1);
		}
	}
	if (!Mode_Cat(&t, text, 2, Mode_S(&t, lost ? "ChallengeLooseRevenge" : "ChallengeWinRevenge"), foe)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Revenge.Foe", who);
	return Mode_Done(&t, PORT_CALL(Speech, AddRemoteCom, NULL, "vv", text, who));
}

/* LaunchRevenge(): the end of the revenge talk: the revenge becomes the challenge and starts here */
static byte Chal_LaunchRevenge(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *type = Mode_V(&t), *foe = Mode_V(&t), *num = Mode_V(&t), *ac = Mode_V(&t);
	if (!Mode_Import("PhoneCab") || !Mode_Import("MissionsFuncs")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Revenge.Type", type);
	if (!Mode_SetSaveVar("Challenge.Type", type)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Revenge.Foe", foe);
	return Mode_Done(&t, Mode_SetSaveVar("Challenge.Foe", foe) && Chal_G("numChallengeEnemies", num) &&
		Chal_SetSaveRepr("Challenge.NumEnemies", num) && Mode_SetSaveVar("Revenge.Type", Mode_S(&t, "")) &&
		Mode_SetSaveVar("Revenge.Foe", Mode_S(&t, "")) && PORT_CALL(PhoneCab, ForceResetPhoneCab, NULL, "") &&
		LvN_ModGet("PhoneCab", "audioComEntity", ac) && PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", ac, 0L) &&
		Chal_Init(Mode_I(&t, 1)));
}

/* LaunchChallenge(): the end of the challenge talk: the challenge into the save variables and the game goes on in
 * the outdoor level of this area (when it is the mission's, the one next to it) */
static byte Chal_LaunchChallenge(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *type = Mode_V(&t), *foe = Mode_V(&t), *out = Mode_V(&t), *area = Mode_V(&t);
	nativeVal_t *sv = Mode_V(&t), *mm = Mode_V(&t), *oa = Mode_V(&t), *level = Mode_V(&t), *spawn = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_Import("SaveGame") || !Mode_Import("MissionsFuncs") || !Mode_Import("string") ||
		!Chal_G("numChallengeEnemies", num) || !Chal_SetSaveRepr("Challenge.NumEnemies", num) ||
		!Chal_G("challengeType", type) || !Mode_SetSaveVar("Challenge.Type", type) || !Chal_G("foeType", foe) ||
		!Mode_SetSaveVar("Challenge.Foe", foe) || !LvN_ModGet("Init", "Outdoor", out) ||
		(Native_True(out) && (!Mode_Import("PhoneCab") || !PORT_CALL(PhoneCab, ForceResetPhoneCab, NULL, ""))) ||
		!PORT_CALL(MissionsFuncs, SetSecondary, NULL, "i", 1L) || !LvN_ModGet("Init", "OutArea", area)) {
		return Mode_Done(&t, 0);
	}
	int empty = Mode_Cmp(area, "==", Mode_S(&t, ""));
	if (empty) {
		return Mode_Done(&t, empty > 0);
	}
	Mode_SaveVar("Mission.Map", sv);
	Native_Free(area);
	if (!Native_Upper(sv, mm) || !LvN_ModGet("Init", "OutArea", area) || !Native_Upper(area, oa)) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t *items[2] = { mm, oa };
	int same = Mode_Print(items, 2) ? Mode_Cmp(mm, "==", oa) : -1;
	if (same < 0) {
		return Mode_Done(&t, 0);
	}
	if (same) {
		const char *next = Native_IsStr(mm, "DOWNTOWN") ? "Commercial" : "DownTown";
		Native_Free(mm);
		Native_SetStr(mm, next);
	} else {
		Native_Free(mm);
		if (!LvN_ModGet("Init", "OutArea", mm)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_Cat(&t, level, 2, Mode_S(&t, "Levels/"), mm) && Chal_Spawn(spawn) &&
		PORT_CALL(SaveGame, MarkWayPoint, NULL, "vvs", level, spawn, "") &&
		PORT_CALL(SaveGame, StartGame, NULL, ";CleanChallenge=i", 0L));
}

/* the end of a challenge or revenge (prefix "Challenge" or "Revenge"): no secondary mission, and its save variables
 * empty */
static byte Chal_Clean(const char *prefix, byte revenge) {
	static const char *const vars[] = { "Type", "Foe", "NumEnemies", "LastWinner" };
	nativeVal_t type, empty;
	char name[32];
	Str_Printf(name, sizeof(name), "%s.Type", prefix);
	Mode_SaveVar(name, &type);
	byte ok = Native_IsStr(&type, "") || (Mode_Import("MissionsFuncs") &&
		PORT_CALL(MissionsFuncs, SetSecondary, NULL, "i", 0L) && Mode_Import("SaveGame") &&
		PORT_CALL(SaveGame, SMDesc, NULL, "si", "", 1L));
	Native_Free(&type);
	Native_SetStr(&empty, "");
	for (int i = 0; ok && i < (revenge ? 4 : 3); i++) {
		Str_Printf(name, sizeof(name), "%s.%s", prefix, vars[i]);
		ok = Mode_SetSaveVar(name, &empty);
	}
	return ok;
}

/* the vehicles of a cancelled challenge (the names in seq) go back to the police or the traffic */
static byte Chal_Release(nativeVal_t *seq) {
	int n = Mode_Loop(seq);
	for (int i = 0; i < n; i++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *name = Mode_V(&t), *veh = Mode_V(&t), *zero = Mode_V(&t), *model = Mode_V(&t), *at = Mode_V(&t);
		nativeVal_t *vname = Mode_V(&t);
		Native_SetFloat(zero, 0.0);
		int c = Native_SeqItem(seq, i, name) && Mode_EntitySeam(name, veh) && Mode_VSet(veh, "MoveTime", zero) &&
			LvN_SetS(veh, "TgType", "None") && Mode_VObj(veh, "Model", model) &&
			PORT_CALL(string, find, at, "vs", model, "SPoli") ? Mode_CmpI(at, ">=", 0) : -1;
		byte ok = c >= 0 && LvN_SetS(veh, "EntityList", c ? "OutCop" : "Traffic") && Mode_VObj(veh, "Name", vname) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 3L, vname);
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return n >= 0;
}

/* CancelChallenge(challType): the player left the race or the combat: the bet is lost, the foes go back to their
 * lists and a revenge is pending (OBJLIB 5: the callbacks of Parking and Alarm set to None in their modules) */
static byte Chal_CancelChallenge(const nativeVal_t *type) {
	static const char *const mods[] = { "Speech", "Menu", "Parking", "Alarm", "RaceMaker", "string" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *lang = Mode_V(&t), *money = Mode_V(&t), *text = Mode_V(&t), *foe = Mode_V(&t), *left = Mode_V(&t);
	nativeVal_t *who = Mode_V(&t), *set = Mode_V(&t), *cp = Mode_V(&t), *list = Mode_V(&t), *cfoe = Mode_V(&t);
	nativeVal_t *cnum = Mode_V(&t);
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!PORT_CALL(Speech, CancelRemoteCom, NULL, "") || !Mode_DelSchedS("SF_Secondary")) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("RaceSystemCancel1", lang);
	Mode_SaveVar("Challenge.Money", money);
	if (!PORT_CALL(string, replace, text, "vsv", lang, "<n>", money) ||
		!PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", text)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.Foe", foe);
	if (!Mode_Cat(&t, left, 2, Mode_S(&t, "RaceLeft"), foe)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.Foe", who);
	if (!PORT_CALL(Speech, AddRemoteCom, NULL, "vv", left, who) ||
		!PORT_CALL(SInput, GetActionSet, set, "") || !LvN_ModNone("Menu", "MissionScorerCall") ||
		!PORT_CALL(Menu, DrawScorer, NULL, "iv", 0L, set)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (Native_IsStr(type, "Race")) {
		ok = Mode_EntitySeam(Mode_S(&t, "CheckPoint"), cp) &&
			(!Native_True(cp) || (LvN_SetS(cp, "OnTake", "") && Mode_VDel(cp))) &&
			LvN_ModGet("RaceMaker", "Players", list) && Chal_Release(list);
	} else if (Native_IsStr(type, "Combat")) {
		ok = LvN_ModGet("RaceMaker", "challenged_NPCs_Names", list) && Chal_Release(list);
	}
	if (!ok || !Mode_SetSaveVar("Revenge.Type", type)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.Foe", cfoe);
	if (!Mode_SetSaveVar("Revenge.Foe", cfoe)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Challenge.NumEnemies", cnum);
	return Mode_Done(&t, Mode_SetSaveVar("Revenge.NumEnemies", cnum) && Chal_SetRevenge(NULL) &&
		LvN_ModNone("Parking", "SwitchCharMissionCallback") && LvN_ModNone("Parking", "SwitchVehicleMissionCallback") &&
		LvN_ModNone("Alarm", "AbortMissionCallback") && Chal_Clean("Challenge", 0));
}

/* ch.var = prefix + string.replace(cad, "<n>", n, 1) */
static byte Chal_Option(const nativeVal_t *ch, const char *var, const char *prefix, const nativeVal_t *cad,
	const nativeVal_t *n) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t), *v = Mode_V(&t);
	return Mode_Done(&t, Chal_Replace1(cad, n, s) && Mode_Cat(&t, v, 2, Mode_S(&t, prefix), s) && Mode_VSet(ch, var, v));
}

/* ch.var = prefix + `ComputeReward(type, foe, num)` with the text cad */
static byte Chal_RewardOption(const nativeVal_t *ch, const char *var, const char *prefix, const nativeVal_t *cad,
	const nativeVal_t *type, const nativeVal_t *foe, long num) {
	nativeVal_t n, v = { 0 }, rep = { 0 };
	Native_SetInt(&n, num);
	byte ok = Chal_Reward(type, foe, &n, &v);
	if (ok) {
		Mode_Repr(&v, &rep);
		ok = Chal_Option(ch, var, prefix, cad, &rep);
	}
	Native_Free(&v);
	Native_Free(&rep);
	return ok;
}

/* ch.var = prefix + Scrap.GetLangStr(key) */
static byte Chal_LangOption(const nativeVal_t *ch, const char *var, const char *prefix, const char *key) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t), *v = Mode_V(&t);
	Mode_Lang(key, s);
	return Mode_Done(&t, Mode_Cat(&t, v, 2, Mode_S(&t, prefix), s) && Mode_VSet(ch, var, v));
}

/* the options of the challenge of type against foe on ch */
static byte Chal_SetOptions(const nativeVal_t *ch, const nativeVal_t *type, const nativeVal_t *foe) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *group = Mode_V(&t), *cad = Mode_V(&t);
	if (!Chal_Group(foe, group)) {
		return Mode_Done(&t, 0);
	}
	byte press = Native_IsStr(group, "Press"), mercs = 0;
	if (!press) {
		/* the elif looks the group up again */
		Native_Free(group);
		if (!Chal_Group(foe, group)) {
			return Mode_Done(&t, 0);
		}
		mercs = Native_IsStr(group, "Mercs");
	}
	Mode_Lang(mercs ? "ChallengeMercs" : "ChallengeRaceStdOpt1", cad);
	byte ok = LvN_SetI(ch, "ArrnOpt", 0);
	if (ok && mercs) {
		ok = Chal_Option(ch, "ArrOpt0", "ChallengeRaceStdOpt1:", cad, Mode_S(&t, "2")) &&
			Chal_Option(ch, "ArrOpt1", "ChallengeRaceStdOpt2:", cad, Mode_S(&t, "3")) &&
			Chal_LangOption(ch, "ArrOpt2", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2");
	} else if (ok) {
		ok = Chal_RewardOption(ch, "ArrOpt0", "ChallengeRaceStdOpt1:", cad, type, foe, 1) &&
			Chal_RewardOption(ch, "ArrOpt1", "ChallengeRaceStdOpt2:", cad, type, foe, 3) &&
			(press ? Chal_LangOption(ch, "ArrOpt2", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2") :
			Chal_RewardOption(ch, "ArrOpt2", "ChallengeRaceStdOpt3:", cad, type, foe, 5) &&
			Chal_LangOption(ch, "ArrOpt3", "ChallengeRaceStdOpt4:", "ChallengeRaceStdOpt2"));
	}
	return Mode_Done(&t, ok && LvN_SetI(ch, "ArriOpt", 0));
}

/* PhoneCab.SetRevengeOptions shares the options (script.h) */
byte Challenge_SetOptions(const nativeVal_t *ch, const nativeVal_t *type, const nativeVal_t *foe) {
	return Chal_SetOptions(ch, type, foe);
}

/* SetChallengeOptions(ch): the options of the challenge of challengeType against foeType */
static byte Chal_SetChallengeOptions(const nativeVal_t *ch) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lang = Mode_V(&t), *foe = Mode_V(&t), *type = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Challenge")) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("ChallengeRaceStdOpt1", lang);
	return Mode_Done(&t, LvN_ModGet("Challenge", "foeType", foe) && LvN_ModGet("Challenge", "challengeType", type) &&
		Chal_SetOptions(ch, type, foe));
}

/* AddChallenge(ch, UsrName, iopt): the race and combat options at iopt and iopt + 1, when challenges are on and the
 * player and the character can drive (Humphrey always) */
static byte Chal_AddChallenge(const nativeVal_t *ch, const nativeVal_t *usr, const nativeVal_t *iopt) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *on = Mode_V(&t), *u = Mode_V(&t), *can = Mode_V(&t), *act = Mode_V(&t);
	if (!Mode_Import("Chars") || !Mode_SaveInt("Challenge", on)) {
		return Mode_Done(&t, 0);
	}
	int ok = Native_True(on);
	if (ok) {
		ok = Mode_EntitySeam(usr, u) && PORT_CALL(Chars, CanDrive, can, "v", u) ? Native_True(can) : -1;
	}
	if (ok > 0) {
		Native_Free(can);
		ok = PORT_CALL(Chars, CanDrive, can, "v", ch) ? Native_True(can) : -1;
		if (ok == 0) {
			ok = Mode_VObj(ch, "ActCtrl", act) ? Native_IsStr(act, "Humphrey") : -1;
		}
	}
	for (int i = 0; ok > 0 && i < 2; i++) {
		/* exec('ch.ArrOpt' + `iopt + i` + ' = ...') */
		nativeVal_t sum = { 0 }, rep = { 0 };
		if (!Mode_Arith('+', iopt, Mode_I(&t, i), &sum)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(&sum, &rep);
		const char *s = Native_Text(&rep);
		byte digits = rep.n > 0 && (int)strlen(s) == rep.n;
		for (const char *p = s; *p; p++) {
			digits &= *p >= '0' && *p <= '9';
		}
		char var[48];
		if (digits) {
			Str_Printf(var, sizeof(var), "ArrOpt%s", s);
		}
		Native_Free(&sum);
		Native_Free(&rep);
		if (!digits) {
			/* OBJLIB 7: the SyntaxError of the exec line */
			PyErr_SetString(PyExc_SyntaxError, "invalid syntax");
			return Mode_Done(&t, 0);
		}
		ok = Chal_LangOption(ch, var, i ? "ChallengeCombatStdOpt0:" : "ChallengeRaceStdOpt0:",
			i ? "ChallengeCombatStdOpt0" : "ChallengeRaceStdOpt0") ? 1 : -1;
	}
	return Mode_Done(&t, ok >= 0);
}

/* the adapters (the arity errors of the script's functions) */
MODE_FN(Challenge, ComputeReward, 3, Chal_Reward(&a[0], &a[1], &a[2], out))
MODE_FNOPT(Challenge, Init, 0, 1, Chal_Init(MODE_OPT(0)))
MODE_FN(Challenge, SetChallengerSkills, 2, Chal_Skills(&a[0], &a[1]))
MODE_FNOPT(Challenge, OnChallengeOpt, 2, 3, Chal_OnChallengeOpt(&a[0], &a[1], MODE_OPT(2)))
MODE_FN(Challenge, SetRevengeParams, 3, Chal_SetRevengeParams(&a[0], &a[1], &a[2]))
MODE_FNOPT(Challenge, SetRevenge, 0, 1, Chal_SetRevenge(MODE_OPT(0)))
MODE_FNANY(Challenge, LaunchRevenge, Chal_LaunchRevenge())
MODE_FNANY(Challenge, LaunchChallenge, Chal_LaunchChallenge())
MODE_FNANY(Challenge, CleanChallenge, Chal_Clean("Challenge", 0))
MODE_FNANY(Challenge, CleanRevenge, Chal_Clean("Revenge", 1))
MODE_FN(Challenge, CancelChallenge, 1, Chal_CancelChallenge(&a[0]))
MODE_FN(Challenge, SetChallengeOptions, 1, Chal_SetChallengeOptions(&a[0]))
MODE_FN(Challenge, AddChallenge, 3, Chal_AddChallenge(&a[0], &a[1], &a[2]))

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

PORTS(Challenge)
