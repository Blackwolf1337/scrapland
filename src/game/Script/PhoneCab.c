/*
 * The phone cabs and Dtritus' desktop in C (issue #2, the mission scripts missions/phonecab.py and
 * dtritusdesktop.py). PhoneCab: the cabs outdoors (DM_Phone_* dummies) with their communicator, using one saves the
 * game, and the revenge talk after a lost or won challenge (the foe calls the player at the marked cab).
 * DTritusDesktop: Dtritus' desktop in Press (the mission sets the talk in DialogController), the desktops of the
 * journalists and the journalists themselves.
 *
 * The module variables stay in the modules: PhoneCab.audioComEntity (the communicator in use, Challenge reads it),
 * DTritusDesktop.DTritusDesktop and DialogController (the missions set the talk there). The action classes are
 * data/missions/phonecab.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the function table
 * rows name the entries. The variables are read and written where the script does it (the registry): the module globals
 * above, Init.Path, the Challenge and Speech variables (set as Script_SetVar does, an error is dropped). The calls of
 * SaveGame, Speech, Challenge and CharsNPC go through their refs (PORT_CALL), Challenge's options through its C
 * function; the mission's DialogController is called as the value of the variable (OBJLIB 5 (9c)), and the module's
 * functions put into Speech.OnOutOfRange are values Speech keeps (OBJLIB 5 (9c)) and calls through their entries. The
 * cabs, communicators and desktops are entity handle values (OBJLIB 7): the module variables keep them and the scripts
 * pass them in.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static const char phonecab_def[] = {
#embed "../../../data/missions/phonecab.def"
	, 0 };

#define PHONECAB_FILE "missions/phonecab.def"

/* ---- shared by both ---- */

/* an attribute an entity gets when it is created: a number, a text, "Talk:" + the language text, or a vector */
typedef struct {
	const char *var;
	char kind;		/* 'i', 's', 't' (talk), 'v' */
	int i;
	const char *s;
	int v[3];
} phoneAttr_t;

/* e.var = value for the attributes up to the one with a NULL var (e an entity handle value): 0 with the error set */
static byte Phone_Set(const nativeVal_t *e, const phoneAttr_t *a) {
	for (; a->var; a++) {
		nativeVal_t v = { 0 }, lang;
		byte ok = 1;
		if (a->kind == 'i') {
			Native_SetInt(&v, a->i);
		} else if (a->kind == 's') {
			Native_SetStr(&v, a->s);
		} else if (a->kind == 't') {
			nativeVal_t talk;
			Mode_Lang(a->s, &lang);
			Native_SetStr(&talk, "Talk:");
			ok = Native_Arith('+', &talk, &lang, &v);
			Native_Free(&lang);
		} else {
			Native_SetEmpty(&v, NV_TUPLE);
			for (int k = 0; k < 3; k++) {
				Native_SetInt(Native_Append(&v), a->v[k]);
			}
		}
		ok = ok && Mode_VSet(e, a->var, &v);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* the reset of the object in the module variable var of module when the player walks away (Speech.OnOutOfRange):
 * no options but the talk (language text talk), the relax loop, no head */
static byte Phone_Reset(const char *module, const char *var, const char *talk) {
	const phoneAttr_t attrs[] = {
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "ArrOpt0", 't', 0, talk, { 0 } },
		{ "ArriOpt", 'i', 0, NULL, { 0 } },
		{ "Action", 's', 0, "Relax", { 0 } },
		{ "DefLoop", 's', 0, "Relax", { 0 } },
		{ "HeadModel", 's', 0, "", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	nativeVal_t e;
	byte ok = Mode_G(module, var, &e) && (!Native_True(&e) || Phone_Set(&e, attrs));
	Native_Free(&e);
	return ok;
}

/* e.RadarShow is 1 or 2 (the mark of the radar, read again for the second compare): 1 or 0, -1 with the error */
static int Phone_Marked(const nativeVal_t *e) {
	nativeVal_t radar;
	for (long k = 1; k <= 2; k++) {
		nativeVal_t n;
		Native_SetInt(&n, k);
		if (!Mode_VGet(e, "RadarShow", &radar)) {
			return -1;
		}
		int c = Native_Compare(&radar, &n);
		Native_Free(&radar);
		if (Native_Failed()) {
			return -1;
		}
		if (!c) {
			return 1;
		}
	}
	return 0;
}

/* the use function of a communicator: without the radar mark (RadarShow 1 or 2) the message that it does not work
 * now, then use(UsrName, CharName, optId, FullOption) */
static byte Phone_DefUse(const nativeVal_t *a, byte (*use)(const nativeVal_t *)) {
	nativeVal_t e = { 0 }, msg;
	int marked = Mode_EntityH(&a[1], &e) ? Phone_Marked(&e) : -1;
	Native_Free(&e);
	if (marked < 0) {
		return 0;
	}
	if (!marked) {
		/* SScorer.SetMsgText(Scrap.GetLangStr("CantUsePhone"), Scrap.GetTime() + 5) */
		Mode_Lang("CantUsePhone", &msg);
		byte ok = Mode_MsgText(&msg, Mode_Time() + 5);
		Native_Free(&msg);
		if (!ok) {
			return 0;
		}
	}
	return use(a);
}

/* SaveGame.MarkWayPoint(Init.Path, CharName, CharName) and the next option of the entity: e.ArriOpt =
 * (e.ArriOpt + 1) % e.ArrnOpt; the entity handle in *e */
static byte Phone_Mark(const nativeVal_t *chname, nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *cur = Mode_V(&t), *next = Mode_V(&t), *n = Mode_V(&t), *v = Mode_V(&t);
	byte ok = Mode_Import("SaveGame") && Mode_Import("Init") && Mode_Import("Speech") && Mode_ModVar("Init", "Path", path) &&
		PORT_CALL(SaveGame, MarkWayPoint, NULL, "vvv", path, chname, chname) && Mode_EntityH(chname, e) &&
		Mode_VGet(e, "ArriOpt", cur) && Native_Arith('+', cur, Mode_I(&t, 1), next) && Mode_VGet(e, "ArrnOpt", n) &&
		Native_Arith('%', next, n, v) && Mode_VSet(e, "ArriOpt", v);
	return Mode_Done(&t, ok);
}

/* c.Pos in front of an object at pos with the y angle ang (180 back), c.Ang = 0, ang + turn, 0, on the floor */
static byte Phone_Spawn(const nativeVal_t *c, const nativeVal_t *pos, const nativeVal_t *ang, int turn) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *r = Mode_V(&t), *rot = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t), *ca = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t);
	/* SVec.Rotate3D((0, 0, -180), (0, ang, 0)) */
	Native_SetEmpty(v, NV_TUPLE);
	Native_SetInt(Native_Append(v), 0);
	Native_SetInt(Native_Append(v), 0);
	Native_SetInt(Native_Append(v), -180);
	Native_SetEmpty(r, NV_TUPLE);
	Native_SetInt(Native_Append(r), 0);
	Native_Copy(Native_Append(r), ang);
	Native_SetInt(Native_Append(r), 0);
	byte ok = Mode_Rotate(v, r, rot) && Mode_VecAdd(pos, rot, p) && Mode_VSet(c, "Pos", p) &&
		Native_Arith(turn < 0 ? '-' : '+', ang, Mode_I(&t, turn < 0 ? -turn : turn), a);
	if (ok) {
		Native_SetEmpty(ca, NV_TUPLE);
		Native_SetInt(Native_Append(ca), 0);
		Native_Copy(Native_Append(ca), a);
		Native_SetInt(Native_Append(ca), 0);
	}
	ok = ok && Mode_VSet(c, "Ang", ca) && Mode_VGet(c, "Name", name) && Mode_SetOnFloor(name);
	return Mode_Done(&t, ok);
}

/* Scrap.CreateEntity(name, pos[0], pos[1], pos[2], "WithLife") with Ang = 0, yAng + turn, 0; its handle in *out */
static byte Phone_Create(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *yang, int turn, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *c[3], *a = Mode_V(&t), *ang = Mode_V(&t);
	for (int k = 0; k < 3; k++) {
		c[k] = Mode_V(&t);
		if (!Native_Subscript(pos, Mode_I(&t, k), c[k])) {
			return Mode_Done(&t, 0);
		}
	}
	cEntity *e;
	if (!Mode_Create(name, c[0], c[1], c[2], "WithLife", &e) || !Mode_EntHandle(e, out)) {
		return Mode_Done(&t, 0);
	}
	if (turn) {
		if (!Native_Arith('+', yang, Mode_I(&t, turn), a)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_Copy(a, yang);
	}
	Native_SetEmpty(ang, NV_TUPLE);
	Native_SetInt(Native_Append(ang), 0);
	Native_Copy(Native_Append(ang), a);
	Native_SetInt(Native_Append(ang), 0);
	return Mode_Done(&t, Mode_VSet(out, "Ang", ang));
}

/* Scrap.StartDummySearch(name, 1) and the first dummy in *out */
static byte Phone_Dummy(const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_DummyStart(s, 1);
	Mode_DummyNext(out);
	return 1;
}

/* dummy[2][1]: the y angle of a dummy */
static byte Phone_DummyAng(const nativeVal_t *dummy, nativeVal_t *out) {
	nativeVal_t k, d2 = { 0 };
	Native_SetInt(&k, 2);
	byte ok = Native_Subscript(dummy, &k, &d2) && Native_Subscript(&d2, (Native_SetInt(&k, 1), &k), out);
	Native_Free(&d2);
	return ok;
}

/* the action classes (phonecab.def), $name for a press desktop */
static byte Phone_Class(const char *section, const nativeVal_t *name) {
	Mode_DefRun(0, PHONECAB_FILE, section, phonecab_def, NULL, 0, name ? 1 : 0, "name", name);
	return 1;
}

/* module.var = value of the script (Script_SetVar: an error is dropped) */
static void Phone_SetVar(const char *module, const char *var, const nativeVal_t *value) {
	Script_SetVar(module, var, Native_ToPy(value));
}

/* ---- PhoneCab ---- */

MODE_FNANY(PhoneCab, ForceResetPhoneCab, Phone_Reset("PhoneCab", "audioComEntity", "PhoneCab"))

/* the options of the revenge on ch (save variables Revenge.Type and Revenge.Foe) */
static byte PhoneCab_RevengeOptions(const nativeVal_t *ch) {
	nativeVal_t type, foe;
	if (!Mode_Import("string") || !Mode_Import("Challenge")) {
		return 0;
	}
	Mode_SaveVar("Revenge.Type", &type);
	Mode_SaveVar("Revenge.Foe", &foe);
	/* the script's own copy of Challenge's options: Challenge's C function */
	byte ok = Challenge_SetOptions(ch, &type, &foe);
	Native_Free(&type);
	Native_Free(&foe);
	return ok;
}

MODE_FN(PhoneCab, SetRevengeOptions, 1, PhoneCab_RevengeOptions(&a[0]))

/* print "Error RevengeSpeech(", UsrName, CharName, optId, FullOption, ")": 0 with the error set */
static byte PhoneCab_PrintError(const nativeVal_t *a) {
	nativeVal_t head, tail;
	Native_SetStr(&head, "Error RevengeSpeech(");
	Native_SetStr(&tail, ")");
	const nativeVal_t *items[] = { &head, &a[0], &a[1], &a[2], &a[3], &tail };
	return Mode_Print(items, 6);
}

/* Challenge.playerWinsRevenge (read again where the script reads it): 1 or 0, -1 with the error */
static int PhoneCab_Wins(void) {
	nativeVal_t w;
	if (!Mode_ModVar("Challenge", "playerWinsRevenge", &w)) {
		return -1;
	}
	int t = Native_True(&w);
	Native_Free(&w);
	return t;
}

/* Speech.OnObjSpch(audioComEntity, Revenge.Foe, msg, ans) (msg NULL: the script's NameError of initMsg); the
 * revenge options again when *again (1, or the truth of the talk's result for again NULL) */
static byte PhoneCab_ObjSpch(const nativeVal_t *ch, const nativeVal_t *msg, const nativeVal_t *ans, int *again) {
	nativeVal_t ac = { 0 }, rfoe, v = { 0 };
	if (!Mode_G("PhoneCab", "audioComEntity", &ac)) {
		return 0;
	}
	Mode_SaveVar("Revenge.Foe", &rfoe);
	byte ok = msg ? PORT_CALL(Speech, OnObjSpch, &v, "vvvv", &ac, &rfoe, msg, ans) :
		Native_Raise(NE_NAME, "initMsg");
	int t = !ok ? -1 : again ? PhoneCab_Wins() : Native_True(&v);
	ok = t >= 0 && (!t || PhoneCab_RevengeOptions(ch));
	Native_Free(&ac);
	Native_Free(&rfoe);
	Native_Free(&v);
	return ok;
}

/* the result talk of the revenge (opt "Talk") */
static byte PhoneCab_ResultTalk(const nativeVal_t *a, const nativeVal_t *ch) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *player = Mode_V(&t), *foe = Mode_V(&t), *head = Mode_V(&t), *type = Mode_V(&t), *msg = Mode_V(&t);
	nativeVal_t *ans = Mode_V(&t), *fr = Mode_V(&t), *num = Mode_V(&t), *n = Mode_V(&t), *empty = Mode_V(&t);
	if (!Mode_UsrHandle(player)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(player)) {
		return Mode_Done(&t, 1);
	}
	int wins = PhoneCab_Wins();
	if (wins < 0) {
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
	const char *prefix = wins ? "Win" : "Loose";
	Mode_SaveVar("Revenge.Type", type);
	byte race = Native_IsStr(type, "Race");
	byte known = race || Native_IsStr(type, "Combat");
	if (known) {
		char key[64];
		Str_Printf(key, sizeof(key), "%sRevenge%sInitMsg", prefix, race ? "Race" : "Combat");
		Native_SetStr(msg, key);
		Str_Printf(key, sizeof(key), "%sRevenge%sInitAns", prefix, race ? "Race" : "Combat");
		if (!Native_Arith('+', Mode_S(&t, key), foe, ans)) {
			return Mode_Done(&t, 0);
		}
	} else if (!PhoneCab_PrintError(a)) {
		return Mode_Done(&t, 0);
	}
	/* Speech.OnOutOfRange = ForceResetPhoneCab (OBJLIB 5 (9c: the function of the module as a value)) */
	Script_SetVar("Speech", "OnOutOfRange", Script_Global("PhoneCab", "ForceResetPhoneCab"));
	wins = PhoneCab_Wins();
	if (wins < 0) {
		return Mode_Done(&t, 0);
	}
	if (!wins) {
		Mode_SaveVar("Revenge.NumEnemies", fr);
		/* OBJLIB 5 (9c): Challenge.LaunchRevenge goes into Speech.OnEndOfSpeech as the object it is (Mode_ModTo; a
		 * failed set is dropped as Script_SetVar dropped it); it is looked up first, where the script evaluates it */
		nativeVal_t *launch = Mode_V(&t);
		if (!Native_ToInt(fr, num) || !Mode_ModGet("Challenge", "LaunchRevenge", launch)) {
			return Mode_Done(&t, 0);
		}
		Phone_SetVar("Challenge", "numChallengeEnemies", num);
		if (!Mode_ModTo("Speech", "OnEndOfSpeech", "Challenge", "LaunchRevenge")) {
			Native_ErrClear();
		}
		Native_SetEmpty(empty, NV_TUPLE);
		Phone_SetVar("Speech", "OnEndOfSpeechParams", empty);
	}
	(void)n;
	int again = 1;
	return Mode_Done(&t, PhoneCab_ObjSpch(ch, known ? msg : NULL, ans, &again));
}

/* RevengeSpeech(UsrName, CharName, optId, FullOption): the revenge talk: the foe tells the player the result (Talk),
 * the bets of a new challenge, or the options again */
static byte PhoneCab_Revenge(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *foe = Mode_V(&t), *groups = Mode_V(&t), *group = Mode_V(&t), *ans = Mode_V(&t);
	const nativeVal_t *opt = &a[2];
	if (!Mode_Import("Speech") || !Mode_Import("Challenge") || !Mode_EntityH(&a[1], ch) ||
		!Mode_Put("PhoneCab", "audioComEntity", NULL, ch)) {
		return Mode_Done(&t, 0);
	}
	Phone_SetVar("Speech", "PlayerName", &a[0]);
	Phone_SetVar("Speech", "NPCName", &a[1]);
	byte ok;
	if (Native_IsStr(opt, "Talk")) {
		ok = PhoneCab_ResultTalk(a, ch);
	} else if (Native_IsStr(opt, "ChallengeRaceStdOpt1") || Native_IsStr(opt, "ChallengeRaceStdOpt2") ||
		Native_IsStr(opt, "ChallengeRaceStdOpt3")) {
		ok = PORT_CALL(Challenge, OnChallengeOpt, NULL, "ivi", (long)(Native_Text(opt)[19] - '0'), ch, 0L);
	} else if (Native_IsStr(opt, "ChallengeRaceStdOpt4")) {
		Mode_SaveVar("Revenge.Foe", foe);
		ok = Mode_ModVar("Challenge", "groups", groups) && Native_Subscript(groups, foe, group);
		if (ok && Native_IsStr(group, "Mercs")) {
			Native_Free(foe);
			Native_SetStr(foe, "Mercs");
		}
		/* audioComEntity and Revenge.Foe are read before the answer is made */
		nativeVal_t *ac = Mode_V(&t), *rfoe = Mode_V(&t), *v = Mode_V(&t);
		ok = ok && Mode_G("PhoneCab", "audioComEntity", ac);
		if (ok) {
			Mode_SaveVar("Revenge.Foe", rfoe);
			ok = Mode_Cat(&t, ans, 3, Mode_S(&t, "ChallengeRace"), foe, Mode_S(&t, "Msg3_Filter")) &&
				PORT_CALL(Speech, OnObjSpch, v, "vvsv", ac, rfoe, "ChallengeRaceStdOpt2", ans);
		}
		ok = ok && (!Native_True(v) || PhoneCab_RevengeOptions(ch));
	} else {
		ok = PhoneCab_PrintError(a);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(PhoneCab, RevengeSpeech, 4, PhoneCab_Revenge(a))

/* GetNearestPhoneCab(usrPos): the communicator of PhoneCabs nearest to the position (None when there is none) */
static byte PhoneCab_Nearest(const nativeVal_t *usrPos, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dist = Mode_V(&t), *name = Mode_V(&t), *pc = Mode_V(&t);
	Native_SetNone(out);
	if (!Mode_ListGet("PhoneCabs", "First", name) || !Mode_EntityH(name, pc)) {
		return Mode_Done(&t, 0);
	}
	while (Native_True(pc)) {
		int mark = t.n;
		nativeVal_t *pos = Mode_V(&t), *d = Mode_V(&t), *nd = Mode_V(&t), *next = Mode_V(&t);
		if (!Mode_VGet(pc, "Pos", pos) || !Mode_VecSub(pos, usrPos, d) || !Mode_VecModSqr(d, nd)) {
			return Mode_Done(&t, 0);
		}
		int nearer = dist->type == NV_NONE || Native_Compare(nd, dist) < 0;
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
		if (nearer) {
			Native_Free(out);
			Native_Copy(out, pc);
			Native_Free(dist);
			Native_Copy(dist, nd);
		}
		if (!Mode_VGet(pc, "NextInList", next)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(pc);
		byte ok = Mode_EntityH(next, pc);
		Mode_Release(&t, mark);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

MODE_FN(PhoneCab, GetNearestPhoneCab, 1, PhoneCab_Nearest(&a[0], out))

/* CreatePhoneCab(name, pos, yAng): the cab */
static byte PhoneCab_Cab(const nativeVal_t *a, nativeVal_t *out) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "PhoneCab", { 0 } },
		{ "ArrowCanUse", 'i', 0, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 0, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	return Phone_Create(&a[0], &a[1], &a[2], 180, out) && Phone_Set(out, attrs);
}

MODE_FN(PhoneCab, CreatePhoneCab, 3, PhoneCab_Cab(a, out))

/* CreateAudioCom(name, pos, yAng): the communicator the player uses */
static byte PhoneCab_AudioCom(const nativeVal_t *a, nativeVal_t *out) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "AudioCom", { 0 } },
		{ "VRadius", 'i', 60, NULL, { 0 } },
		{ "UseAngSel", 'i', 0, NULL, { 0 } },
		{ "UseMapCheck", 'i', 0, NULL, { 0 } },
		{ "ArrowCanUse", 'i', 1, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 0, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "ArrOpt0", 't', 0, "PhoneCab", { 0 } },
		{ "UseFunc", 's', 0, "PhoneCab.DefUseAudioCom", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	return Phone_Create(&a[0], &a[1], &a[2], 180, out) && Phone_Set(out, attrs);
}

MODE_FN(PhoneCab, CreateAudioCom, 3, PhoneCab_AudioCom(a, out))

/* UseAudioCom(UsrName, CharName, optId, FullOption): using a communicator saves the game; with a revenge pending
 * the foe answers at the marked cab, or tells the player the police is still after him */
static byte PhoneCab_Use(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *foe = Mode_V(&t), *type = Mode_V(&t), *text = Mode_V(&t);
	if (!Phone_Mark(&a[1], e) || !Mode_Put("PhoneCab", "audioComEntity", NULL, e)) {
		return Mode_Done(&t, 0);
	}
	Script_SetVar("Speech", "OnOutOfRange", Script_Global("PhoneCab", "ForceResetPhoneCab"));
	Mode_SaveVar("Revenge.Foe", foe);
	Mode_SaveVar("Revenge.Type", type);
	if (Native_IsStr(type, "") || Native_IsStr(foe, "")) {
		return Mode_Done(&t, 1);
	}
	byte ok;
	int alarm = Mode_AlarmOn();
	if (alarm > 0) {
		ok = Native_Arith('+', Mode_S(&t, "Generics_StillPolice1_"), foe, text) &&
			PORT_CALL(Speech, StartObjSpch, NULL, "vvv", e, text, foe);
	} else {
		int marked = alarm < 0 ? -1 : Phone_Marked(e);
		ok = marked >= 0 && (!marked || PhoneCab_Revenge(a));
	}
	return Mode_Done(&t, ok);
}

MODE_FN(PhoneCab, UseAudioCom, 4, PhoneCab_Use(a))
MODE_FN(PhoneCab, DefUseAudioCom, 4, Phone_DefUse(a, PhoneCab_Use))

/* SpawnChar(c, Phone): the character in front of the cab (c.Ang reads Phone.Ang again) */
static byte PhoneCab_Spawn(const nativeVal_t *c, const nativeVal_t *phone) {
	nativeVal_t pos = { 0 }, ang = { 0 }, y = { 0 }, k;
	Native_SetInt(&k, 1);
	byte ok = Mode_Import("Chars") && Mode_VGet(phone, "Pos", &pos) && Mode_VGet(phone, "Ang", &ang) &&
		Native_Subscript(&ang, &k, &y) && Phone_Spawn(c, &pos, &y, 180);
	Native_Free(&pos);
	Native_Free(&ang);
	Native_Free(&y);
	return ok;
}

MODE_FN(PhoneCab, SpawnChar, 2, PhoneCab_Spawn(&a[0], &a[1]))

MODE_FNANY(PhoneCab, CreatePhoneCabClass, Phone_Class("PhoneCab", NULL))
MODE_FNANY(PhoneCab, CreateAudioComClass, Phone_Class("AudioCom", NULL))

/* Init(): a cab and its communicator (in PhoneCabs) at each DM_Phone_* dummy that is on */
static byte PhoneCab_InitCabs(void) {
	static const phoneAttr_t link[] = {
		{ "OffModPos", 'v', 0, NULL, { 0, 0, 0 } },
		{ "DeltaOffMul", 'i', 1, NULL, { 0 } },
		{ "OffMul", 'i', 1, NULL, { 0 } },
		{ "UseLinkAng", 'i', 1, NULL, { 0 } },
		{ "EntityList", 's', 0, "PhoneCabs", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	/* Scrap.CreateEntityList("PhoneCabs") */
	CHECK(cWorld_NewEntityList(g_pWorld, "PhoneCabs"));
	Mode_DummyStart("DM_Phone_*", 1);
	for (;;) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *dummy = Mode_V(&t), *on = Mode_V(&t), *d0 = Mode_V(&t), *name = Mode_V(&t), *d1 = Mode_V(&t);
		nativeVal_t *yang = Mode_V(&t), *a = Mode_V(&t), *pc = Mode_V(&t), *ac = Mode_V(&t), *pcname = Mode_V(&t);
		nativeVal_t *lm = Mode_V(&t);
		Mode_DummyNext(dummy);
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		if (!Native_Subscript(dummy, Mode_I(&t, 3), on)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(on)) {
			Mode_Done(&t, 0);
			continue;
		}
		byte ok = Native_Subscript(dummy, Mode_I(&t, 0), d0) && Native_Arith('+', d0, Mode_S(&t, "_PhoneCab"), name) &&
			Native_Subscript(dummy, Mode_I(&t, 1), d1) && Phone_DummyAng(dummy, yang);
		if (ok) {
			Native_SetEmpty(a, NV_TUPLE);
			Native_Copy(Native_Append(a), name);
			Native_Copy(Native_Append(a), d1);
			Native_Copy(Native_Append(a), yang);
			ok = PhoneCab_Cab(a->items, pc);
		}
		/* the communicator reads the dummy again */
		Native_Free(yang);
		ok = ok && Phone_DummyAng(dummy, yang);
		if (ok) {
			Native_Free(a);
			Native_SetEmpty(a, NV_TUPLE);
			Native_Copy(Native_Append(a), d0);
			Native_Copy(Native_Append(a), d1);
			Native_Copy(Native_Append(a), yang);
			ok = PhoneCab_AudioCom(a->items, ac);
		}
		ok = ok && Mode_VGet(pc, "Name", pcname) && Native_Arith('+', pcname, Mode_S(&t, ",DC_JackCom"), lm) &&
			Mode_VSet(ac, "LinkModel", lm) && Phone_Set(ac, link);
		Mode_Done(&t, 0);
		if (!ok) {
			return 0;
		}
	}
}

MODE_FNANY(PhoneCab, Init, PhoneCab_InitCabs())

/* res.RadarShow == 0 (a free cab): 1 or 0, -1 with the error */
static int PhoneCab_IsFree(const nativeVal_t *res) {
	nativeVal_t radar, zero;
	if (!Mode_VGet(res, "RadarShow", &radar)) {
		return -1;
	}
	Native_SetInt(&zero, 0);
	int c = Native_Compare(&radar, &zero);
	Native_Free(&radar);
	return Native_Failed() ? -1 : !c;
}

/* res = Scrap.GetEntity(res.NextInList) */
static byte PhoneCab_Next(nativeVal_t *res) {
	nativeVal_t next;
	if (!Mode_VGet(res, "NextInList", &next)) {
		return 0;
	}
	Native_Free(res);
	byte ok = Mode_EntityH(&next, res);
	Native_Free(&next);
	return ok;
}

/* GetFree(): the cab nearest to the player; without a player a random free one (no radar mark) */
static byte PhoneCab_Free(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *pos = Mode_V(&t), *len = Mode_V(&t), *top = Mode_V(&t), *topf = Mode_V(&t);
	nativeVal_t *rnd = Mode_V(&t), *index = Mode_V(&t), *name = Mode_V(&t), *tenth = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr)) {
		return Mode_Done(&t, Mode_VGet(usr, "Pos", pos) && PhoneCab_Nearest(pos, out));
	}
	Native_SetFloat(tenth, 0.1);
	if (!Mode_ListGet("PhoneCabs", "Num", len) || !Native_Arith('-', len, tenth, top) || !Native_ToFloat(top, topf)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(rnd, Scrap_RandRange(0.0, topf->f));
	if (!Native_ToInt(rnd, index)) {
		return Mode_Done(&t, 0);
	}
	for (int pass = 0; pass < 2; pass++) {
		if (!Mode_ListGet("PhoneCabs", "First", name) || !Mode_EntityH(name, out)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(name);
		long i = 0;
		while (Native_True(out)) {
			if (!pass) {
				nativeVal_t n;
				Native_SetInt(&n, i);
				int same = !Native_Compare(&n, index);
				if (Native_Failed()) {
					return Mode_Done(&t, 0);
				}
				if (same) {
					break;
				}
				i++;
			}
			int k = pass ? PhoneCab_IsFree(out) : 0;
			if (k) {
				return Mode_Done(&t, k > 0);
			}
			if (!PhoneCab_Next(out)) {
				return Mode_Done(&t, 0);
			}
		}
		if (!pass) {
			if (!Native_True(out)) {
				break;
			}
			int k = PhoneCab_IsFree(out);
			if (k) {
				return Mode_Done(&t, k > 0);
			}
		}
	}
	/* OBJLIB 7: the string exception of the script (raise "Aqui ...") */
	return Mode_Done(&t,
		Mode_RaiseText("Aqui no se debe de llegar nunca, siempre se ha de devolver una cabina (res != None)"));
}

MODE_FNANY(PhoneCab, GetFree, PhoneCab_Free(out))

const portFn_t g_PhoneCabMethods[] = {
	PORT_FN(ForceResetPhoneCab, PhoneCab_ForceResetPhoneCab),
	PORT_FN(RevengeSpeech, PhoneCab_RevengeSpeech),
	PORT_FN(SetRevengeOptions, PhoneCab_SetRevengeOptions),
	PORT_FN(GetNearestPhoneCab, PhoneCab_GetNearestPhoneCab),
	PORT_FN(CreatePhoneCab, PhoneCab_CreatePhoneCab),
	PORT_FN(CreateAudioCom, PhoneCab_CreateAudioCom),
	PORT_FN(DefUseAudioCom, PhoneCab_DefUseAudioCom),
	PORT_FN(UseAudioCom, PhoneCab_UseAudioCom),
	PORT_FN(SpawnChar, PhoneCab_SpawnChar),
	PORT_FN(CreatePhoneCabClass, PhoneCab_CreatePhoneCabClass),
	PORT_FN(CreateAudioComClass, PhoneCab_CreateAudioComClass),
	PORT_FN(Init, PhoneCab_Init),
	PORT_FN(GetFree, PhoneCab_GetFree),
	PORT_FNEND
};

/* ---- DTritusDesktop ---- */

MODE_FNANY(DTritus, ForceResetDTritusDesktop, Phone_Reset("DTritusDesktop", "DTritusDesktop", "DtritusDesktop"))

/* SpawnChar(c): the character in front of the desktop (DM_DTritusDesktop) */
static byte DTritus_Spawn(const nativeVal_t *c) {
	nativeVal_t name, dummy = { 0 }, pos = { 0 }, y = { 0 }, k;
	Native_SetStr(&name, "DM_DTritusDesktop");
	Native_SetInt(&k, 1);
	byte ok = Mode_Import("Chars") && Phone_Dummy(&name, &dummy) && Native_Subscript(&dummy, &k, &pos) &&
		Phone_DummyAng(&dummy, &y) && Phone_Spawn(c, &pos, &y, -180);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&y);
	return ok;
}

MODE_FN(DTritus, SpawnChar, 1, DTritus_Spawn(&a[0]))

MODE_FNANY(DTritus, CreateDTritusDesktopClass, Phone_Class("DTritusDesktop", NULL))

/* CreateDTritusDesktop(name, pos, yAng): the desktop the player talks to */
static byte DTritus_Desktop(const nativeVal_t *a, nativeVal_t *out) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "DTritusDesktop", { 0 } },
		{ "VRadius", 'i', 60, NULL, { 0 } },
		{ "ArrowCanUse", 'i', 1, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 1, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "CamTgt", 'v', 0, NULL, { 0, 225, -50 } },
		{ "ArrOpt0", 't', 0, "DtritusDesktop", { 0 } },
		{ "UseFunc", 's', 0, "DTritusDesktop.DefUseDTritusDesktop", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	return Phone_Create(&a[0], &a[1], &a[2], 0, out) && Phone_Set(out, attrs);
}

MODE_FN(DTritus, CreateDTritusDesktop, 3, DTritus_Desktop(a, out))

/* UseDTritusDesktop(UsrName, CharName, optId, FullOption): using it saves the game; the talk of the mission
 * (DialogController), or Deep Throat's taunt while the alarm is on */
static byte DTritus_Use(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *dc = Mode_V(&t);
	if (!Phone_Mark(&a[1], e) || !Mode_Put("DTritusDesktop", "DTritusDesktop", NULL, e)) {
		return Mode_Done(&t, 0);
	}
	Script_SetVar("Speech", "OnOutOfRange", Script_Global("DTritusDesktop", "ForceResetDTritusDesktop"));
	if (!Mode_G("DTritusDesktop", "DialogController", dc)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(dc)) {
		return Mode_Done(&t, 1);
	}
	int alarm = Mode_AlarmOn();
	if (alarm) {
		return Mode_Done(&t, alarm > 0 &&
			PORT_CALL(Speech, AddRemoteCom, NULL, "ss", "Generics_DeepThroatTaunt_1", "DeepThroat"));
	}
	/* OBJLIB 5 (9c): the mission's talk function,

 the variable read again and called as the value it holds */
	return Mode_Done(&t, Native_CallGlobal("DTritusDesktop", "DialogController", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]));
}

MODE_FN(DTritus, UseDTritusDesktop, 4, DTritus_Use(a))
MODE_FN(DTritus, DefUseDTritusDesktop, 4, Phone_DefUse(a, DTritus_Use))

/* CreatePressDesktopClass(DesktopName): the class of a journalist's desktop */
static byte DTritus_PressClass(const nativeVal_t *name) {
	if (name->type != NV_STR) {
		/* the script's SAct.CreateClass raises its TypeError (and the model name its own after it) */
		nativeVal_t head, v = { 0 };
		Native_SetStr(&head, "Models/Misc/DesktopPress/");
		byte ok = PORT_CALL(SAct, CreateClass, NULL, "v", name) && Native_Arith('+', &head, name, &v);
		Native_Free(&v);
		/* a value both take gives no result and no error, as the script's C did */
		return ok && 0;
	}
	return Phone_Class("PressDesktop", name);
}

MODE_FN(DTritus, CreatePressDesktopClass, 1, DTritus_PressClass(&a[0]))

/* CreatePressDesktop(DesktopName): the desktop at its dummy DM_<DesktopName>, when there is one */
static byte DTritus_Press(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pat = Mode_V(&t), *dummy = Mode_V(&t), *pos = Mode_V(&t), *y = Mode_V(&t), *ac = Mode_V(&t);
	if (!Native_Arith('+', Mode_S(&t, "DM_"), name, pat) || !Phone_Dummy(pat, dummy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(dummy)) {
		return Mode_Done(&t, 1);
	}
	byte ok = DTritus_PressClass(name) && Native_Subscript(dummy, Mode_I(&t, 1), pos) && Phone_DummyAng(dummy, y) &&
		Phone_Create(name, pos, y, 0, ac) && Mode_VSet(ac, "ActCtrl", name) && Mode_VSet(ac, "MapCollision", Mode_I(&t, 1)) &&
		Mode_VSet(ac, "VRadius", Mode_I(&t, 60));
	return Mode_Done(&t, ok);
}

MODE_FN(DTritus, CreatePressDesktop, 1, DTritus_Press(&a[0]))

/* Init(): Dtritus' desktop at DM_DTritusDesktop (when the dummy is on) and the three desktops of the journalists */
static byte DTritus_InitDesks(void) {
	static const char *const desks[] = { "BertoDesktop", "BettyDesktop", "HumphreyDesktop" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *on = Mode_V(&t), *a = Mode_V(&t), *desk = Mode_V(&t), *d[3];
	for (int k = 0; k < 3; k++) {
		d[k] = Mode_V(&t);
	}
	if (!Phone_Dummy(Mode_S(&t, "DM_DTritusDesktop"), dummy)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (Native_True(dummy)) {
		ok = Native_Subscript(dummy, Mode_I(&t, 3), on);
		if (ok && Native_True(on)) {
			ok = Native_Subscript(dummy, Mode_I(&t, 0), d[0]) && Native_Subscript(dummy, Mode_I(&t, 1), d[1]) &&
				Phone_DummyAng(dummy, d[2]) && Phone_Class("DTritusDesktop", NULL);
			if (ok) {
				Native_SetEmpty(a, NV_TUPLE);
				for (int k = 0; k < 3; k++) {
					Native_Copy(Native_Append(a), d[k]);
				}
				ok = DTritus_Desktop(a->items, desk) && Mode_Put("DTritusDesktop", "DTritusDesktop", NULL, desk);
			}
		}
	}
	for (int i = 0; ok && i < 3; i++) {
		ok = DTritus_Press(Mode_S(&t, desks[i]));
	}
	return Mode_Done(&t, ok);
}

MODE_FNANY(DTritus, Init, DTritus_InitDesks())

/* InitNPCS(): the journalists whose default map (save variable Journalist.<name>_Defaut) is this one */
static byte DTritus_NPCs(void) {
	static const char *const names[] = { "Humphrey", "Betty", "Berto" };
	if (!Mode_Import("CharsNPC") || !Mode_Import("string") || !Mode_Import("Init")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *sv = Mode_V(&t), *map = Mode_V(&t), *path = Mode_V(&t), *tail = Mode_V(&t), *here = Mode_V(&t);
		char var[64];
		Str_Printf(var, sizeof(var), "Journalist.%s_Defaut", names[i]);
		Mode_SaveVar(var, sv);
		byte ok = Native_Upper(sv, map) && Mode_ModVar("Init", "Path", path) && Native_Slice(path, 7, 0x7fffffff, tail) &&
			Native_Upper(tail, here);
		if (ok && Native_Equal(map, here)) {
			Str_Printf(var, sizeof(var), "%s_Default", names[i]);
			ok = PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", var);
		}
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return 1;
}

MODE_FNANY(DTritus, InitNPCS, DTritus_NPCs())

const portFn_t g_DTritusDesktopMethods[] = {
	PORT_FN(ForceResetDTritusDesktop, DTritus_ForceResetDTritusDesktop),
	PORT_FN(SpawnChar, DTritus_SpawnChar),
	PORT_FN(CreateDTritusDesktopClass, DTritus_CreateDTritusDesktopClass),
	PORT_FN(CreateDTritusDesktop, DTritus_CreateDTritusDesktop),
	PORT_FN(DefUseDTritusDesktop, DTritus_DefUseDTritusDesktop),
	PORT_FN(UseDTritusDesktop, DTritus_UseDTritusDesktop),
	PORT_FN(CreatePressDesktopClass, DTritus_CreatePressDesktopClass),
	PORT_FN(CreatePressDesktop, DTritus_CreatePressDesktop),
	PORT_FN(Init, DTritus_Init),
	PORT_FN(InitNPCS, DTritus_InitNPCS),
	PORT_FNEND
};

PORTS(PhoneCab)
