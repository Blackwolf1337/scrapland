/*
 * The Speech module in C (scripts/chars/speech.py, issue #2 steps 7b and 7c): the talks between the player and the
 * characters. Part 1: the voices, a speech and its answer (OnSpch, OnSpeech, AutoAnswer, PerformAnswer), the end and
 * the abort of a speech, the answer list of a character, the player's own answers, InspectChar and AutoSpeech.
 * Part 2: the non-interactive talks (NISpeech, AddNI, RunNI), the remote communications and taunts over the radio
 * (with their queue RemoteCom), and the speeches with objects that carry the head of a character.
 *
 * The state of a speech stays in the variables of the module (PlayerName, NPCName, OnEndOfSpeech, the colours,
 * AnsAnima, TalkNI and so on), because the character scripts and the missions read and set them. The code works on
 * the entity handles like the script, so the values it passes on keep their types, and it stops at the first error
 * like the script.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the variables of the
 * module where the script reads them and write them back at once, the lists TalkNI and RemoteCom changed in place
 * (the missions, the levels and the cases read and set them). The functions call each other in C as the
 * port did; the scheduled ones, the hooks (OnEndOfSpeech, PlayerAnsCallback, OnOutOfRange, the SpeechRoutines) and
 * the functions RunNI calls (OnSpch, OnObjSpch) are the values of the module. The entities are handle values; the
 * voices, SScorer.SetSpeechText, SetSpeechCallback, SetHeadMonitor, CancelSpeech, SetMsgText and Set, Scrap.Set,
 * GetEntity, UsrEntity, SetLockAlarm, GetAlarm, GetLangEntries, DeleteScheduledFuncs and the scheduler go through
 * their seams (the cases record most of them); SVec.GetRotAng and NormAng go through their refs (stage 7),
 * string.split and find are the native twins (PORT_CALL; by name for an argument that holds an object, OBJLIB 9).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define SP "Speech"

/* ---- the variables of the module ---- */

/* a variable of the module as a function of the script reads it (a NameError with the name when it is missing) */
static byte Sp_G(const char *var, nativeVal_t *out) {
	return Mode_G(SP, var, out);
}

/* a global of the module when it is there (globals().get of the script): 1 with *out, else 0 (no error) */
static byte Sp_Has(const char *var, nativeVal_t *out) {
	if (Sp_G(var, out)) {
		return 1;
	}
	Native_ErrClear();
	return 0;
}

/* ---- values ---- */

/* `v` == n of the script (object.c Object_Compare): -1, 0, 1 */
static int Sp_NumCmp(const nativeVal_t *v, long n) {
	nativeVal_t b;
	Native_SetInt(&b, n);
	return Native_Compare(v, &b);
}

/* a + text, text + a of the script */
static byte Sp_Cat(const nativeVal_t *a, const char *text, nativeVal_t *out) {
	nativeVal_t t;
	Native_SetText(&t, text, (int)strlen(text));
	byte ok = Native_Arith('+', a, &t, out);
	Native_Free(&t);
	return ok;
}

static byte Sp_CatS(const char *text, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t t;
	Native_SetText(&t, text, (int)strlen(text));
	byte ok = Native_Arith('+', &t, b, out);
	Native_Free(&t);
	return ok;
}

/* v[i] as abstract.c Sequence_GetItem takes it (a dict is no sequence; an NV_OBJ the Python way) */
static byte Sp_Item(const nativeVal_t *v, long i, nativeVal_t *out) {
	nativeVal_t tmp;
	Native_SetNone(out);
	if (Native_IsObj(v)) {
		nativeVal_t r;
		if (!Mode_RefItem(v, i, &r)) {
			return 0;
		}
		Mode_RefPlain(&r, out);
		Native_Free(&r);
		return 1;
	}
	Native_SetNone(&tmp);
	const nativeVal_t *x = Native_GetItem(v, i, 0, &tmp);
	if (x) {
		Native_Copy(out, x);
	}
	Native_Free(&tmp);
	return x != NULL;
}

/* e.var == text (a text variable): 1, 0, -1 with the error (as the handle reads it) */
static int Sp_HIs(const nativeVal_t *e, const char *var, const char *text) {
	nativeVal_t v = { 0 };
	int r = Mode_VObj(e, var, &v) ? Native_IsStr(&v, text) : -1;
	Native_Free(&v);
	return r;
}

/* e.var[0:len(prefix)] == prefix (as the handle reads it) */
static int Sp_HPrefix(const nativeVal_t *e, const char *var, const char *prefix) {
	nativeVal_t v = { 0 }, head = { 0 };
	int r = Mode_VObj(e, var, &v) && Native_Slice(&v, 0, (long)strlen(prefix), &head) ? Native_IsStr(&head, prefix) : -1;
	Native_Free(&v);
	Native_Free(&head);
	return r;
}

/* ---- the engine ---- */

/* Scrap.GetEntity(<module variable var>): a handle or None */
static byte Sp_Entity(const char *var, nativeVal_t *out) {
	nativeVal_t name = { 0 };
	Native_SetNone(out);
	byte ok = Sp_G(var, &name) && Mode_EntitySeam(&name, out);
	Native_Free(&name);
	return ok;
}

/* Scrap.UsrEntity(0) */
static byte Sp_Usr(nativeVal_t *out) {
	Native_SetNone(out);
	return Mode_UsrHandle(out);
}

/* SScorer.SetMsgText and the speeches: Scrap.GetLangStr(key) of a C text */
static void Sp_Lang(const char *key, nativeVal_t *out) {
	Mode_Lang(key, out);
}

/* Scrap.Get("VoicePath") + text, opened on a voice channel */
static byte Sp_OpenVoice(const nativeVal_t *text, int channel) {
	nativeVal_t path = { 0 }, file = { 0 }, ch;
	Native_SetInt(&ch, channel);
	byte ok = Native_GVarGet("VoicePath", &path) && Native_Arith('+', &path, text, &file) && Mode_OpenVoice(&file, &ch, NULL);
	Native_Free(&path);
	Native_Free(&file);
	return ok;
}

static byte Sp_OpenVoiceS(const char *text, int channel) {
	nativeVal_t t;
	Native_SetText(&t, text, (int)strlen(text));
	byte ok = Sp_OpenVoice(&t, channel);
	Native_Free(&t);
	return ok;
}

/* SScorer.SetSpeechText(text, time, colour[0], colour[1], colour[2]) with a colour variable of the module */
static byte Sp_SpeechText(const nativeVal_t *text, const nativeVal_t *time, const char *colour) {
	nativeVal_t c = { 0 }, r = { 0 }, g = { 0 }, b = { 0 };
	byte ok = Sp_G(colour, &c) && Sp_Item(&c, 0, &r) && Sp_Item(&c, 1, &g) && Sp_Item(&c, 2, &b) &&
		Mode_SpeechText(text, time, &r, &g, &b);
	Native_Free(&c);
	Native_Free(&r);
	Native_Free(&g);
	Native_Free(&b);
	return ok;
}

/* SVec.GetRotAng(SVec.Sub(me.Pos, ch.Pos)): the angles from ch to me */
static byte Sp_RotAng(const nativeVal_t *me, const nativeVal_t *ch, nativeVal_t *out) {
	nativeVal_t a = { 0 }, b = { 0 }, d = { 0 };
	Native_SetNone(out);
	byte ok = Mode_VObj(me, "Pos", &a) && Mode_VObj(ch, "Pos", &b) && Mode_VecSub(&a, &b, &d) &&
		PORT_CALL(SVec, GetRotAng, out, "v", &d);
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&d);
	return ok;
}

/* (vang[0], vang[1] - turn, 0): the view of the player turned from the character */
static byte Sp_Turned(const nativeVal_t *vang, long turn, nativeVal_t *out) {
	nativeVal_t x = { 0 }, y = { 0 }, yt = { 0 }, t;
	Native_SetInt(&t, turn);
	Native_SetNone(out);
	byte ok = Sp_Item(vang, 0, &x) && Sp_Item(vang, 1, &y) && Native_Arith('-', &y, &t, &yt);
	if (ok) {
		Native_SetEmpty(out, NV_TUPLE);
		Native_Copy(Native_Append(out), &x);
		Native_Copy(Native_Append(out), &yt);
		Native_SetInt(Native_Append(out), 0);
	}
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&yt);
	return ok;
}

/* e.var = "text", = i, = d (the handle values of the script) */
#define Sp_HSetS LvN_SetS
#define Sp_HSetI LvN_SetI
#define Sp_HSetF LvN_SetF

/* ---- the voices ---- */

/* ScheduledVoiceCallback(channel, rtime): the voice starts; the talk time is its rest plus rtime (0.2 at least) */
static byte Sp_ScheduledVoiceCallback(const nativeVal_t *channel, const nativeVal_t *rtime) {
	nativeVal_t full = { 0 }, rest = { 0 }, t = { 0 }, least;
	Native_SetFloat(&least, 0.2);
	byte ok = Mode_PlayVoice(channel, NULL) && Native_GVarGet("FullTalkTime", &full) && Mode_VoiceRemain(channel, &full, &rest) &&
		Mode_Arith('+', &rest, rtime, &t);
	if (ok && Native_Compare(&least, &t) > 0) {
		Native_Free(&t);
		t = least;
	}
	ok = ok && Mode_GVarSet("FullTalkTime", &t) && Mode_GVarSet("RemainTalkTime", &t);
	Native_Free(&full);
	Native_Free(&rest);
	Native_Free(&t);
	return ok;
}

/* PlayVoiceScheduled(channel, stime, rtime) */
static byte Sp_PlayVoiceScheduled(const nativeVal_t *channel, double stime, const nativeVal_t *rtime) {
	nativeVal_t a[2];
	Native_Copy(&a[0], channel);
	Native_Copy(&a[1], rtime);
	byte ok = Mode_LaterIn(stime, SP, "ScheduledVoiceCallback", a, 2, NULL);
	Native_Free(&a[0]);
	Native_Free(&a[1]);
	return ok;
}

/* the same with the voice delay of the module (Script_GetFloat of VoiceShTime, 0.25) and a channel number */
static byte Sp_PlayVoiceAfter(int channel, const nativeVal_t *rtime) {
	nativeVal_t c;
	Native_SetInt(&c, channel);
	return Sp_PlayVoiceScheduled(&c, Mode_ModGetN(SP, "VoiceShTime", 0.25), rtime);
}

static byte Sp_PlayVoiceAfterI(int channel, long rtime) {
	nativeVal_t r;
	Native_SetInt(&r, rtime);
	return Sp_PlayVoiceAfter(channel, &r);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime()+stime, ScheduledVoiceCallback, (channel, rtime)) with any stime: the
 * TypeError of the addition for one that is no number */
static byte Sp_PlayVoiceScheduledV(const nativeVal_t *channel, const nativeVal_t *stime, const nativeVal_t *rtime) {
	nativeVal_t now, t = { 0 }, a[2];
	Native_SetFloat(&now, Mode_Time());
	if (!Mode_Arith('+', &now, stime, &t)) {
		return 0;
	}
	nativeVal_t fn;
	byte ok = Mode_RefG(SP, "ScheduledVoiceCallback", &fn);
	Native_Copy(&a[0], channel);
	Native_Copy(&a[1], rtime);
	ok = ok && Mode_ScheduleV(&t, &fn, a, 2, NULL);
	Native_Free(&fn);
	Native_Free(&a[0]);
	Native_Free(&a[1]);
	Native_Free(&t);
	return ok;
}

/* ---- the end of a speech ---- */

/* CallEndOfSpeech(): the voices stop, the alarm unlocks, OnEndOfSpeech(*OnEndOfSpeechParams) runs once */
static byte Sp_CallEndOfSpeech(void) {
	nativeVal_t ch, end = { 0 }, params = { 0 }, none, empty;
	for (int i = 0; i < 3; i++) {
		Native_SetInt(&ch, i);
		if (!Mode_StopVoice(&ch)) {
			return 0;
		}
	}
	Mode_ModPutS(SP, "TalkListen", "TalkListen");
	if (Mode_ModGetI(SP, "LockAlarm", 0)) {
		Native_SetInt(&ch, 0);
		if (!Mode_SetLockAlarm(&ch)) {
			return 0;
		}
		Mode_ModPutI(SP, "LockAlarm", 0);
	}
	if (!Sp_G("OnEndOfSpeech", &end)) {
		return 0;
	}
	byte ok = 1;
	if (Native_True(&end)) {
		ok = Sp_G("OnEndOfSpeechParams", &params);
		Native_SetNone(&none);
		Native_SetEmpty(&empty, NV_TUPLE);
		Mode_ModPut(SP, "OnEndOfSpeech", &none);
		Mode_ModPut(SP, "OnEndOfSpeechParams", &empty);
		Native_Free(&empty);
		ok = ok && Mode_Apply(&end, &params, NULL);
	}
	Native_Free(&end);
	Native_Free(&params);
	if (!ok) {
		return 0;
	}
	Mode_ModPutS(SP, "CurrentRemoteCom", "");
	Mode_ModPutS(SP, "LaunchedTauntEntityName", "");
	Mode_ModPutI(SP, "CanLaunchRemote", 1);
	return 1;
}

/* RestoreState(UsrName): the character offers to talk again and goes back to its own logic */
static byte Sp_RestoreState(const nativeVal_t *name) {
	nativeVal_t c = { 0 }, talk = { 0 }, opt = { 0 };
	byte ok = Mode_EntitySeam(name, &c);
	if (ok && Native_True(&c)) {
		Sp_Lang("TalkOpt", &talk);
		ok = Sp_CatS("Talk:", &talk, &opt) && Sp_HSetI(&c, "ArrnOpt", 0) && Mode_VSet(&c, "ArrOpt0", &opt) &&
			Sp_HSetI(&c, "LLogicIdle", 0);
	}
	Native_Free(&c);
	Native_Free(&talk);
	Native_Free(&opt);
	return ok;
}

/* the player or the character stops talking: Relax unless it does something else */
static byte Sp_StopTalk(const nativeVal_t *e) {
	int talk = Sp_HPrefix(e, "Action", "Talk");
	return talk >= 0 && (!talk || (Sp_HSetS(e, "ForceAction", "Relax") && Sp_HSetS(e, "ForceDefLoop", "Relax")));
}

/* AbortSpeech(UsrName="", CharName="", extrangextra=""): the speech ends, both go back to what they did */
static byte Sp_AbortSpeech(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *me = Mode_V(&t), *ch = Mode_V(&t), *def = Mode_V(&t), *arrow = Mode_V(&t), *n = Mode_V(&t), *name = Mode_V(&t);
	if (!Sp_Entity("PlayerName", me) || !Sp_Entity("NPCName", ch)) {
		return Mode_Done(&t, 0);
	}
	/* TextColor = DefTextColor (Script_SetVar of a failed read drops the error) */
	if (Sp_G("DefTextColor", def)) {
		Mode_ModPut(SP, "TextColor", def);
	} else {
		Native_ErrClear();
	}
	if (!Sp_SpeechText(Mode_S(&t, ""), Mode_I(&t, 0), "TextColor") || !Mode_SpeechCallback("")) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(me) && (!Sp_HSetI(me, "AutoSelect", 1) || !Sp_HSetS(me, "SelectedEntity", "") || !Sp_StopTalk(me) ||
		!Sp_HSetS(me, "OutSelectFunc", ""))) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(ch)) {
		int walk = Sp_HIs(ch, "Type", "WalkChar");
		if (walk < 0) {
			return Mode_Done(&t, 0);
		}
		if (walk) {
			if (!Sp_G("ArrowCanMission", arrow) || !Mode_VSet(ch, "ArrowCanMission", arrow) || !Sp_HSetS(ch, "SelectedEntity", "") ||
				!Sp_StopTalk(ch) || !Mode_VObj(ch, "ArrnOpt", n)) {
				return Mode_Done(&t, 0);
			}
			/* the logic of the character back when talking was its only option */
			if (Sp_NumCmp(n, 1) == 0) {
				int talk = Sp_HPrefix(ch, "ArrOpt0", "Talk:");
				if (talk < 0 || (talk > 0 && (!Mode_VObj(ch, "Name", name) || !Sp_RestoreState(name) ||
					!Sp_HSetS(ch, "AbortSelectFunc", "")))) {
					return Mode_Done(&t, 0);
				}
			}
		}
	}
	Mode_Done(&t, 1);
	return Sp_CallEndOfSpeech();
}

/* NormalAbortSpeech(): the end of the answer; the camera turns from the character once the player listened */
static byte Sp_NormalAbortSpeech(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *me = Mode_V(&t), *ch = Mode_V(&t), *cam = Mode_V(&t), *vang = Mode_V(&t), *turned = Mode_V(&t);
	if (!Sp_Entity("PlayerName", me)) {
		return Mode_Done(&t, 0);
	}
	int listen = Native_True(me) ? Sp_HIs(me, "Action", "TalkListen") : 0;
	if (listen < 0) {
		return Mode_Done(&t, 0);
	}
	if (listen && (!Sp_Entity("NPCName", ch) || !Mode_Cam0(cam) || !Sp_HSetF(cam, "VSpeed", 1.1) || !Sp_HSetF(cam, "HSpeed", 1.1) ||
		(Native_True(ch) && (!Sp_RotAng(me, ch, vang) || !Sp_Turned(vang, 25, turned) || !Mode_VSet(me, "ViewAng", turned) ||
		!Sp_HSetI(me, "DisplAng", 25))))) {
		return Mode_Done(&t, 0);
	}
	Mode_Done(&t, 1);
	return Sp_AbortSpeech();
}

/* AbortNPCSpeech(UsrName="", CharName=""): prints the names (OBJLIB 7: on Python's stdout, Mode_WriteLine) */
MODE_KW(Speech, AbortNPCSpeech, "|OO", "UsrName", "CharName") {
	const nativeVal_t *items[2] = { &v[0], &v[1] };
	Mode_KwDefS(v, given, 0, "");
	Mode_KwDefS(v, given, 1, "");
	return Mode_WriteLine(items, 2);
}

/* ---- the answer ---- */

/* PerformAnswer(SpeechText, Time=6): the character answers with its voice; 1, or 0 when it cannot (out) */
static byte Sp_PerformAnswer(const nativeVal_t *text, const nativeVal_t *time, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *me = Mode_V(&t), *ch = Mode_V(&t), *listen = Mode_V(&t), *anima = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *rest = Mode_V(&t);
	if (!Sp_Entity("PlayerName", me) || !Sp_Entity("NPCName", ch)) {
		return Mode_Done(&t, 0);
	}
	int talk = Sp_HPrefix(me, "Action", "Talk");
	if (talk < 0 || (talk && (!Sp_G("TalkListen", listen) || !Mode_VSet(me, "ForceAction", listen) ||
		!Sp_HSetS(me, "ForceDefLoop", "Relax")))) {
		return Mode_Done(&t, 0);
	}
	if (!Sp_G("AnsAnima", anima) || !Mode_VSet(ch, "Action", anima)) {
		return Mode_Done(&t, 0);
	}
	int answers = Sp_HIs(ch, "Action", anima->type == NV_STR ? Native_Text(anima) : "");
	if (answers > 0) {
		if (!Mode_VObj(ch, "Name", name) || !Mode_VSet(ch, "ForceDefLoop", anima) || !Mode_StopVoice(Mode_I(&t, 0)) ||
			!Mode_PlayVoice(Mode_I(&t, 1), name) || !Mode_VoiceRemain(Mode_I(&t, 1), time, rest) ||
			!Mode_SpeechCallback("Speech.NormalAbortSpeech") || !Sp_SpeechText(text, rest, "TextColor")) {
			return Mode_Done(&t, 0);
		}
		Mode_ModPutS(SP, "AnsAnima", "TalkNPC");
		Native_SetInt(out, 1);
		return Mode_Done(&t, 1);
	}
	if (answers < 0 || !Sp_AbortSpeech()) {
		return Mode_Done(&t, 0);
	}
	Mode_ModPutS(SP, "AnsAnima", "TalkNPC");
	Native_SetInt(out, 0);
	return Mode_Done(&t, 1);
}

/* StopMenuEvents(), RestartMenuEvents(): the menu keys while the answer starts */
static byte Sp_StopMenuEvents(void) {
	GVar_SetStr("EscapeEvent", "");
	GVar_SetStr("MainMissionEvent", "");
	return 1;
}

static byte Sp_RestartMenuEvents(void) {
	GVar_SetStr("EscapeEvent", "Menu.Init");
	GVar_SetStr("MainMissionEvent", "MissionsFuncs.ShowMissionInfo");
	return 1;
}

/* AutoAnswer(): the end of the player's line, the answer follows */
static byte Sp_AutoAnswer(void) {
	nativeVal_t me = { 0 }, a[2] = { { 0 }, { 0 } };
	int talk = Sp_Entity("PlayerName", &me) ? Sp_HPrefix(&me, "Action", "Talk") : -1;
	byte ok = talk >= 0 && (!talk || Sp_HSetS(&me, "ForceAction", "TalkListenNPC"));
	Native_Free(&me);
	if (ok) {
		Sp_StopMenuEvents();
		ok = Sp_G("NPCAnswer", &a[0]) && Sp_G("NPCAnswerTime", &a[1]) && Mode_LaterIn(1.0, SP, "RestartMenuEvents", NULL, 0, NULL) &&
			Mode_LaterIn(0.45, SP, "PerformAnswer", a, 2, NULL);
	}
	Native_Free(&a[0]);
	Native_Free(&a[1]);
	return ok;
}

/* StdAnsLst(lst): the answers of the player as the options of the character ("msg:text", the Opt text or the Msg).
 * A lst that is no sequence raises at once (the port went on with the TypeError of its length left set) */
static byte Sp_StdAnsLst(const nativeVal_t *lstArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *lst = Mode_V(&t);
	Native_Copy(lst, lstArg);
	int n = Sp_Entity("NPCName", ch) && Sp_HSetI(ch, "ArrnOpt", 0) ? Mode_Len(lst) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *msg = Mode_V(&t), *key = Mode_V(&t), *opt = Mode_V(&t), *head = Mode_V(&t), *v = Mode_V(&t);
		char var[0x20];
		ok = Sp_Item(lst, i, msg) && Sp_Cat(msg, "Opt", key);
		if (ok) {
			Sp_Lang(Native_Text(key), opt);
			if (Native_IsStr(opt, "")) {
				Native_Free(key);
				Native_Free(opt);
				ok = Sp_Cat(msg, "Msg", key);
				if (ok) {
					Sp_Lang(Native_Text(key), opt);
				}
			}
		}
		Str_Printf(var, sizeof(var), "ArrOpt%d", i);
		ok = ok && Sp_Cat(msg, ":", head) && Native_Arith('+', head, opt, v) && Mode_VSet(ch, var, v);
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

/* ---- a speech ---- */

/* CalculateSpeechTime(cad): len(string.split(cad)) * 0.3 + 2, 0.3 s a word and 2 s. A value that is no string goes
 * to string.split, which raises as in the script (the native twin; an object goes to strop by name, OBJLIB 9) */
static byte Sp_SpeechTime(const nativeVal_t *text, nativeVal_t *out) {
	int words = 0;
	if (text->type != NV_STR) {
		nativeVal_t l = { 0 };
		byte ok = PORT_CALL(string, split, &l, "v", text);
		words = ok ? Mode_Len(&l) : -1;
		Native_Free(&l);
		if (words < 0) {
			return 0;
		}
	} else {
		byte in = 0;
		const char *p = Native_Text(text);
		for (int i = 0; i < text->n; i++) {
			byte space = p[i] == ' ' || p[i] == '\t' || p[i] == '\n' || p[i] == '\r' || p[i] == '\v' || p[i] == '\f';
			words += !space && !in;
			in = !space;
		}
	}
	Native_SetFloat(out, words * 0.3 + 2);
	return 1;
}

/* SetPlayerToTalk(me, ch): the player faces the character to talk (the camera turns unless a non-interactive talk
 * goes on), 1 when the player can talk, -1 with the error */
static int Sp_SetPlayerToTalk(const nativeVal_t *me, const nativeVal_t *ch) {
	if (!Native_True(me)) {
		return 0;
	}
	modeVals_t t = { .n = 0 };
	nativeVal_t *sel = Mode_V(&t), *life = Mode_V(&t), *name = Mode_V(&t), *ni = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t *cam = Mode_V(&t), *v = Mode_V(&t), *h = Mode_V(&t), *vang = Mode_V(&t), *y = Mode_V(&t), *pos = Mode_V(&t);
	nativeVal_t *turned = Mode_V(&t), *ang = Mode_V(&t);
	if (!Mode_VObj(me, "AutoSelect", sel)) {
		return Mode_Done(&t, 0), -1;
	}
	int can = Sp_NumCmp(sel, 1) == 0;
	if (can) {
		if (!Mode_VObj(ch, "Life", life)) {
			return Mode_Done(&t, 0), -1;
		}
		can = Sp_NumCmp(life, 0) > 0;
	}
	if (!can) {
		return Mode_Done(&t, 1), 0;
	}
	if (!Mode_VObj(ch, "Name", name) || !Mode_VSet(me, "SelectedEntity", name) || !Sp_HSetS(me, "Action", "Talk") ||
		!Sp_HSetS(me, "DefLoop", "Talk")) {
		return Mode_Done(&t, 0), -1;
	}
	/* globals().get('TalkNI') == None or globals().get('CurrNI') == 0 */
	byte hasNI = Sp_Has("TalkNI", ni), hasCur = Sp_Has("CurrNI", cur);
	if (!hasNI || ni->type == NV_NONE || (hasCur && Sp_NumCmp(cur, 0) == 0)) {
		int fcam = Mode_Cam0(cam) ? Mode_VIsType(cam, "FCam") : -1;
		/* the script keeps the speeds in a local it never uses */
		if (fcam < 0 || (fcam && (!Mode_VObj(cam, "VSpeed", v) || !Mode_VObj(cam, "HSpeed", h) || !Sp_HSetF(cam, "VSpeed", 1.1) ||
			!Sp_HSetF(cam, "HSpeed", 1.1)))) {
			return Mode_Done(&t, 0), -1;
		}
		if (!Sp_RotAng(me, ch, vang) || !Sp_Item(vang, 1, y) || !Mode_VObj(me, "Pos", pos) || !Sp_HSetI(me, "DisplAng", 45) ||
			!Sp_Turned(vang, 45, turned) || !Mode_VSet(me, "ViewAng", turned)) {
			return Mode_Done(&t, 0), -1;
		}
		Native_SetEmpty(ang, NV_TUPLE);
		Native_SetInt(Native_Append(ang), 0);
		Native_Copy(Native_Append(ang), y);
		Native_SetInt(Native_Append(ang), 0);
		if (!Mode_VSet(me, "MoveAng", ang) || !Mode_VSet(me, "MovePos", pos) || !Sp_HSetF(me, "MoveTime", 0.20) ||
			!Sp_HSetI(me, "MoveViewAng", 0) || !Mode_LaterIn(0.25, SP, "RestoreMoveTime", me, 1, NULL)) {
			return Mode_Done(&t, 0), -1;
		}
	}
	if (!Sp_HSetI(me, "AutoSelect", 0) || !Sp_HSetS(me, "OutSelectFunc", "Speech.AbortSpeech")) {
		return Mode_Done(&t, 0), -1;
	}
	return Mode_Done(&t, 1), 1;
}

/* the int result of a speech function into out: 0 for r < 0 (the error) */
static byte Sp_IntOut(int r, nativeVal_t *out) {
	if (r < 0) {
		return 0;
	}
	Native_SetInt(out, r);
	return 1;
}

/* OnSpeech(SpeechText, SpeechAnswer="", _AnsAnima_="TalkNPC"): the player says the text, the character listens and
 * answers when it ends (AutoAnswer); 1 when the speech starts, -1 with the error */
static int Sp_OnSpeech(const nativeVal_t *text, const nativeVal_t *ans, const nativeVal_t *anima) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *time = Mode_V(&t), *ansTime = Mode_V(&t), *me = Mode_V(&t), *ch = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *ca = Mode_V(&t), *ma = Mode_V(&t), *cy = Mode_V(&t), *my = Mode_V(&t), *norm = Mode_V(&t);
	nativeVal_t *rt = Mode_V(&t), *arrow = Mode_V(&t), *one = Mode_V(&t);
	if (!Sp_SpeechTime(text, time) || !Sp_SpeechTime(ans, ansTime)) {
		return Mode_Done(&t, 0), -1;
	}
	Mode_ModPut(SP, "AnsAnima", anima);
	int r = Sp_Entity("PlayerName", me) && Sp_Entity("NPCName", ch) ? Sp_SetPlayerToTalk(me, ch) : -1;
	if (r <= 0) {
		return Mode_Done(&t, 0), r;
	}
	double dc, dm, d;
	if (!Mode_VObj(me, "Name", name) || !Mode_VObj(ch, "Ang", ca) || !Mode_VObj(me, "Ang", ma) || !Sp_Item(ca, 1, cy) ||
		!Sp_Item(ma, 1, my) || !Native_ArgDouble(cy, &dc) || !Native_ArgDouble(my, &dm) ||
		!PORT_CALL(SVec, NormAng, norm, "d", dc - dm - 180) || !Sp_HSetS(ch, "AbortSelectFunc", "Speech.RestoreState") ||
		!Sp_HSetI(ch, "LLogicIdle", 1) || !Mode_VSet(ch, "SelectedEntity", name) || !Native_ArgDouble(norm, &d)) {
		return Mode_Done(&t, 0), -1;
	}
	Native_SetFloat(rt, -0.15);
	if (!Sp_HSetS(ch, "Action", (d < 0 ? -d : d) > 5 ? "FaceToTalk" : "TalkListenNPC") || !Sp_HSetS(ch, "DefLoop", "TalkListenNPC") ||
		!Sp_HSetI(ch, "DisplAng", 0) || !Sp_HSetI(ch, "TimeToExtraRelax", 240) || !Sp_PlayVoiceAfter(0, rt) ||
		!Sp_SpeechText(text, time, "UsrTextColor") || !Mode_SpeechCallback("Speech.AutoAnswer")) {
		return Mode_Done(&t, 0), -1;
	}
	Mode_ModPut(SP, "NPCAnswer", ans);
	Mode_ModPut(SP, "NPCAnswerTime", ansTime);
	if (!Mode_VObj(ch, "ArrowCanMission", arrow)) {
		return Mode_Done(&t, 0), -1;
	}
	Mode_ModPut(SP, "ArrowCanMission", arrow);
	Native_SetInt(one, 1);
	if (!Sp_HSetI(ch, "ArrowCanMission", 0) || (Mode_ModGetI(SP, "LockAlarm", 0) && !Mode_SetLockAlarm(one))) {
		return Mode_Done(&t, 0), -1;
	}
	return Mode_Done(&t, 1), r;
}

/* OnSpch(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC"): a speech by the keys of the language file, with the
 * voices of both */
static int Sp_OnSpch(const nativeVal_t *text, const nativeVal_t *ans, const nativeVal_t *anima) {
	nativeVal_t me = { 0 }, you = { 0 };
	int r = Mode_LangV(text, &me) && Mode_LangV(ans, &you) && Sp_OpenVoice(text, 0) && Sp_OpenVoice(ans, 1) ?
		Sp_OnSpeech(&me, &you, anima) : -1;
	Native_Free(&me);
	Native_Free(&you);
	return r;
}

/* OnChallengeSpch(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC", isMerc=0): the player's text as it is, the
 * challenge voice of the player */
static int Sp_OnChallengeSpch(const nativeVal_t *text, const nativeVal_t *ans, const nativeVal_t *anima, const nativeVal_t *merc) {
	nativeVal_t you = { 0 };
	byte isMerc = merc && Sp_NumCmp(merc, 1) == 0;
	int r = Mode_LangV(ans, &you) && Sp_OpenVoiceS(isMerc ? "ChallengeRaceStdOpt2Voice" : "ChallengeRaceStdOpt1Voice", 0) &&
		Sp_OpenVoice(ans, 1) ? Sp_OnSpeech(text, &you, anima) : -1;
	Native_Free(&you);
	return r;
}

/* StdOnSpch(optId, _AnsAnima_="TalkNPC"): the speech of an option, the arrow on the next option */
static byte Sp_StdOnSpch(const nativeVal_t *opt, const nativeVal_t *anima) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *msg = Mode_V(&t), *ans = Mode_V(&t), *ch = Mode_V(&t), *i = Mode_V(&t), *n = Mode_V(&t), *next = Mode_V(&t);
	nativeVal_t *mod = Mode_V(&t);
	if (!Sp_Cat(opt, "Msg", msg) || !Sp_Cat(opt, "Ans", ans) || Sp_OnSpch(msg, ans, anima) < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Sp_Entity("NPCName", ch) && Mode_VObj(ch, "ArriOpt", i) && Mode_VObj(ch, "ArrnOpt", n) &&
		Mode_Arith('+', i, Mode_I(&t, 1), next) && Mode_Arith('%', next, n, mod) && Mode_VSet(ch, "ArriOpt", mod));
}

/* ---- the player answers ---- */

/* PlayerAnsEnd(): the end of the player's answer, its callback once */
static byte Sp_PlayerAnsEnd(void) {
	nativeVal_t zero, cb = { 0 };
	Native_SetInt(&zero, 0);
	byte ok = Mode_StopVoice(&zero) && Sp_G("PlayerAnsCallback", &cb) && Mode_CallArgs(&cb, NULL, NULL, 0) &&
		Mode_DelGlobal(SP, "PlayerAnsCallback");
	Native_Free(&cb);
	return ok;
}

/* PlayerAns(me, ch, LangText, callbakck): the player says a text to the character, then the callback; 1 when the
 * player can talk */
static int Sp_PlayerAns(const nativeVal_t *me, const nativeVal_t *ch, const nativeVal_t *key, const nativeVal_t *cb) {
	int r = Sp_SetPlayerToTalk(me, ch);
	if (r > 0) {
		nativeVal_t text = { 0 }, t = { 0 };
		byte ok = Mode_LangV(key, &text) && Sp_SpeechTime(&text, &t) && Sp_SpeechText(&text, &t, "UsrTextColor");
		if (ok) {
			Mode_ModPut(SP, "PlayerAnsCallback", cb);
			ok = Mode_SpeechCallback("Speech.PlayerAnsEnd") && Sp_OpenVoice(key, 0) && Sp_PlayVoiceAfterI(0, 0);
		}
		Native_Free(&text);
		Native_Free(&t);
		if (!ok) {
			r = -1;
		}
	}
	return r;
}

/* ---- the standard speech routines ---- */

/* SScorer.SetMsgText(Scrap.GetLangStr(key), Scrap.GetTime() + 2) */
static byte Sp_Msg(const char *key) {
	nativeVal_t text;
	Sp_Lang(key, &text);
	byte ok = Mode_MsgText(&text, Mode_Time() + 2);
	Native_Free(&text);
	return ok;
}

/* InspectChar(UsrName, CharName, optId, FullOption): why the character does not talk */
static byte Sp_InspectChar(const nativeVal_t *usr, const nativeVal_t *chName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *me = Mode_V(&t), *ch = Mode_V(&t), *type = Mode_V(&t), *state = Mode_V(&t), *mine = Mode_V(&t), *def = Mode_V(&t);
	if (!Mode_EntitySeam(usr, me) || !Mode_EntitySeam(chName, ch) || !Mode_VObj(ch, "ActCtrl", type) ||
		!Mode_VObj(ch, "LLogicState", state)) {
		return Mode_Done(&t, 0);
	}
	const char *ty = type->type == NV_STR ? Native_Text(type) : "";
	const char *key = NULL;
	char buf[0x80];
	int a;
	if ((a = Sp_HPrefix(ch, "Action", "Flashed")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Flashed" : NULL;
	} else if ((a = Sp_HPrefix(ch, "Action", "Dazed")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Dazed" : NULL;
	} else if ((a = Sp_HPrefix(ch, "Action", "Crazy")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Crazy" : NULL;
	} else if ((a = Mode_AlarmOn()) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Alarm" : NULL;
	} else if (!strcmp(ty, "Sentinel")) {
		key = Mode_VObj(me, "ActCtrl", mine) && Native_GVarGet("UsrDefChar", def) ?
			Native_Compare(mine, def) == 0 ? "INSPECT_CHAR_Sentinel_Act" : "INSPECT_CHAR_Sentinel" : NULL;
	} else {
		int same = Sp_HIs(me, "ActCtrl", ty);
		if (same < 0) {
			key = NULL;
		} else if (same && (!strcmp(ty, "Mayor") || !strcmp(ty, "PoliceBoss") || !strcmp(ty, "Betty") || !strcmp(ty, "Berto"))) {
			key = "INSPECT_CHAR_Unique";
		} else if (!strcmp(ty, "Nurse") && Sp_NumCmp(state, 3) == 0) {
			key = "INSPECT_CHAR_Nurse";
		} else if (!strcmp(ty, "Gear") && Sp_NumCmp(state, 2) == 0) {
			key = "INSPECT_CHAR_Gear";
		} else if (Sp_NumCmp(state, 4) == 0 || Sp_NumCmp(state, 5) == 0) {
			Str_Printf(buf, sizeof(buf), "INSPECT_CHAR_%s", ty);
			key = buf;
		} else {
			key = "INSPECT_CHAR_CantTalk";
		}
	}
	return Mode_Done(&t, key && Sp_Msg(key));
}

/* AutoSpeech(UsrName, CharName, optId, FullOption): the speech routine of the character's class (called with the
 * four arguments), else a standard answer */
static byte Sp_AutoSpeech(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *type = Mode_V(&t), *routines = Mode_V(&t), *f = Mode_V(&t), *msg = Mode_V(&t), *ans = Mode_V(&t);
	Mode_ModPut(SP, "PlayerName", &a[0]);
	Mode_ModPut(SP, "NPCName", &a[1]);
	if (!Sp_Entity("NPCName", ch) || !Mode_VObj(ch, "ActCtrl", type) || !Sp_G("SpeechRoutines", routines)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_Sub(routines, type, f)) {
		return Mode_Done(&t, Mode_CallArgs(f, NULL, a, 4));
	}
	/* except KeyError */
	if (!Mode_CatchKeyError()) {
		return Mode_Done(&t, 0);
	}
	char buf[0x20];
	Str_Printf(buf, sizeof(buf), "StdAnsw%d", (int)Scrap_RandRange(0, 2.9));
	Native_SetStr(msg, "TalkMsg");
	Native_SetStr(ans, buf);
	return Mode_Done(&t, Sp_OnSpch(msg, ans, Mode_S(&t, "TalkNPC")) >= 0);
}

/* ---- part 2: the non-interactive talks ---- */

/* NISpeech(headName=""): a non-interactive talk starts, the end of the speech waits until it is over */
static byte Sp_NISpeech(const nativeVal_t *head) {
	nativeVal_t end = { 0 }, params = { 0 }, empty;
	byte ok = Sp_G("OnEndOfSpeech", &end) && Sp_G("OnEndOfSpeechParams", &params);
	if (ok) {
		Native_SetEmpty(&empty, NV_LIST);
		Mode_ModPut(SP, "TalkNI", &empty);
		Native_Free(&empty);
		Mode_ModPut(SP, "OnEndOfSpeechNI", &end);
		Mode_ModPut(SP, "OnEndOfSpeechParamsNI", &params);
		Mode_ModPut(SP, "NISpeechHeadName", head);
	}
	Native_Free(&end);
	Native_Free(&params);
	return ok;
}

/* AddNI(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC", HeadName=None): a line of the talk (the head of the
 * talk unless one is given) */
static byte Sp_AddNI(const nativeVal_t *text, const nativeVal_t *ans, const nativeVal_t *anima, const nativeVal_t *head) {
	nativeVal_t h = { 0 }, line, list = { 0 };
	byte ok = head && Native_True(head) ? (Native_Copy(&h, head), 1) : Sp_G("NISpeechHeadName", &h);
	/* the list read as the port read it: its NameError before the line */
	ok = ok && Sp_G("TalkNI", &list);
	if (ok) {
		Native_SetEmpty(&line, NV_TUPLE);
		Native_Copy(Native_Append(&line), text);
		Native_Copy(Native_Append(&line), ans);
		if (anima) {
			Native_Copy(Native_Append(&line), anima);
		} else {
			Native_SetStr(Native_Append(&line), "TalkNPC");
		}
		Native_Copy(Native_Append(&line), &h);
		ok = Mode_ListAddG(SP, "TalkNI", &line, 0);
		Native_Free(&line);
	}
	Native_Free(&h);
	Native_Free(&list);
	return ok;
}

/* AutoAddNI(TextBase, _AnsAnima_="TalkNPC", HeadName=None): the lines Msg/Ans of the language entries of a base with
 * the head given (HeadName=... alone too, as Mission_AttackTraitorius gives DeepThroat's lines his head) */
static byte Sp_AutoAddNI(const nativeVal_t *base, const nativeVal_t *anima, const nativeVal_t *head) {
	nativeVal_t keys = { 0 }, txt = { 0 }, msg = { 0 }, ans = { 0 };
	int n = Native_LangEntries(base, 2, &keys) ? Mode_Len(&keys) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Sp_Item(&keys, i, &txt) && Sp_Cat(&txt, "Msg", &msg) && Sp_Cat(&txt, "Ans", &ans) &&
			Sp_AddNI(&msg, &ans, anima, head);
		Native_Free(&txt);
		Native_Free(&msg);
		Native_Free(&ans);
	}
	Native_Free(&keys);
	return ok;
}

/* TalkNI[i][k] of the module (each a read of the global as the script reads it) */
static byte Sp_NI(const nativeVal_t *i, long k, nativeVal_t *out) {
	nativeVal_t list = { 0 }, line = { 0 };
	Native_SetNone(out);
	byte ok = Sp_G("TalkNI", &list) && Mode_Sub(&list, i, &line) && LvN_Item(&line, k, out);
	Native_Free(&list);
	Native_Free(&line);
	return ok;
}

/* RunNI(i=0): line i of the talk; the next one when it ends, after the last the end of the talk */
static byte Sp_RunNI(const nativeVal_t *i) {
	static const char *const keepMortal[] = { "Human", "GateKeeper", "Sebastian", "Sputnik", NULL };
	modeVals_t t = { .n = 0 };
	/* a: the four arguments of the calls, one after the other */
	nativeVal_t *a = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *list = Mode_V(&t), *npc = Mode_V(&t), *ch = Mode_V(&t), *fn = Mode_V(&t), *me = Mode_V(&t), *head = Mode_V(&t);
	nativeVal_t *cur = Mode_V(&t), *v = Mode_V(&t), *ctrl = Mode_V(&t), *last = Mode_V(&t);
	int n = Sp_G("TalkNI", list) ? Mode_Len(list) : -1;
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(last, n - 1);
	byte ltkn = Native_Compare(i, last) < 0;
	Mode_ModPut(SP, "CurrNI", i);
	Mode_ModPutI(SP, "LockAlarm", 1);
	if (ltkn) {
		Mode_ModPutS(SP, "TalkListen", "TalkListenNPC");
	}
	int isWch = Sp_G("NPCName", npc) && Mode_EntitySeam(npc, ch) ? Mode_VIsType(ch, "WalkChar") : -1;
	if (isWch > 0) {
		if (!Sp_G("OnSpch", fn) || !Sp_NI(i, 0, &a[0]) || !Sp_NI(i, 1, &a[1]) || !Sp_NI(i, 2, &a[2]) ||
			!Mode_CallArgs(fn, NULL, a, 3) || !Sp_HSetI(ch, "Invulnerable", 1)) {
			return Mode_Done(&t, 0);
		}
	} else if (isWch == 0) {
		Native_Free(npc);
		if (!Sp_G("NPCName", npc) || !Sp_Entity("PlayerName", me) || !Mode_VSet(me, "SelectedEntity", npc) || !Sp_G("OnObjSpch", fn)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(&a[0], ch);
		if (!Sp_NI(i, 3, &a[1]) || !Sp_NI(i, 0, &a[2]) || !Sp_NI(i, 1, &a[3]) || !Mode_CallArgs(fn, NULL, a, 4) ||
			!Sp_NI(i, 3, head) || !Sp_G("NISpeechHeadName", cur)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(head, cur) != 0) {
			Native_Free(head);
			if (!Sp_HSetS(ch, "HeadModel", "") || !Sp_NI(i, 3, head)) {
				return Mode_Done(&t, 0);
			}
			Mode_ModPut(SP, "NISpeechHeadName", head);
		}
	} else {
		return Mode_Done(&t, 0);
	}
	if (ltkn) {
		nativeVal_t *next = &a[0];
		Native_Free(next);
		Native_Free(fn);
		if (!Sp_G("RunNI", fn)) {
			return Mode_Done(&t, 0);
		}
		Mode_ModPut(SP, "OnEndOfSpeech", fn);
		if (!Mode_Arith('+', i, Mode_I(&t, 1), next)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(v);
		Native_SetEmpty(v, NV_TUPLE);
		Native_Copy(Native_Append(v), next);
		Mode_ModPut(SP, "OnEndOfSpeechParams", v);
		return Mode_Done(&t, 1);
	}
	if (!Mode_DelGlobal(SP, "CurrNI") || !Mode_DelGlobal(SP, "TalkNI") || !Mode_DelGlobal(SP, "NISpeechHeadName")) {
		return Mode_Done(&t, 0);
	}
	if (isWch > 0) {
		if (!Sp_G("OnEndOfSpeechNI", v)) {
			return Mode_Done(&t, 0);
		}
		Mode_ModPut(SP, "OnEndOfSpeech", v);
		Native_Free(v);
		if (!Sp_G("OnEndOfSpeechParamsNI", v)) {
			return Mode_Done(&t, 0);
		}
		Mode_ModPut(SP, "OnEndOfSpeechParams", v);
		if (!Mode_DelGlobal(SP, "OnEndOfSpeechNI") || !Mode_DelGlobal(SP, "OnEndOfSpeechParamsNI") || !Mode_VObj(ch, "ActCtrl", ctrl)) {
			return Mode_Done(&t, 0);
		}
		byte keep = 0;
		for (int k = 0; keepMortal[k]; k++) {
			keep |= Native_IsStr(ctrl, keepMortal[k]);
		}
		return Mode_Done(&t, keep || Sp_HSetI(ch, "Invulnerable", 0));
	}
	if (!Sp_G("ObjNIEnd", v)) {
		return Mode_Done(&t, 0);
	}
	Mode_ModPut(SP, "OnEndOfSpeech", v);
	Native_Free(v);
	Native_SetEmpty(v, NV_TUPLE);
	Mode_ModPut(SP, "OnEndOfSpeechParams", v);
	return Mode_Done(&t, 1);
}

/* ObjNIEnd(): the end of a talk with an object: its head goes, then the end of the talk */
static byte Sp_ObjNIEnd(void) {
	nativeVal_t end = { 0 }, params = { 0 }, ch = { 0 }, ch2 = { 0 };
	byte ok = Sp_G("OnEndOfSpeechNI", &end) && Sp_G("OnEndOfSpeechParamsNI", &params) && Mode_DelGlobal(SP, "OnEndOfSpeechNI") &&
		Mode_DelGlobal(SP, "OnEndOfSpeechParamsNI") && Sp_Entity("NPCName", &ch) && Sp_HSetS(&ch, "HeadModel", "") &&
		Sp_Entity("NPCName", &ch2) && Sp_HSetS(&ch2, "Action", "Relax") && Mode_Apply(&end, &params, NULL);
	Native_Free(&end);
	Native_Free(&params);
	Native_Free(&ch);
	Native_Free(&ch2);
	return ok;
}

/* ---- remote communications ---- */

/* RestoreSpeech(): the end of a remote communication */
static byte Sp_RestoreSpeech(void) {
	nativeVal_t e;
	byte ok = Sp_Usr(&e);
	if (ok && Native_True(&e)) {
		int w = Mode_VIsType(&e, "WalkChar");
		ok = w >= 0 && (!w || Sp_HSetI(&e, "AutoSelect", 1));
	}
	Native_Free(&e);
	return ok && Sp_CallEndOfSpeech();
}

/* DoRemote(): 1 when a remote communication may start now (no talk, no system message, a player who can listen) */
static int Sp_DoRemote(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *rest = Mode_V(&t), *msg = Mode_V(&t), *set = Mode_V(&t), *cam = Mode_V(&t), *sel = Mode_V(&t);
	if (!Mode_Import("Init") || !Sp_Usr(e)) {
		return Mode_Done(&t, 0), -1;
	}
	double now = Mode_Time(), m;
	int doit = -1;
	if (now < 0.2) {
		doit = 0;
	} else if (!Native_GVarGet("RemainTalkTime", rest)) {
	} else if (Sp_NumCmp(rest, 0) > 0 || !Mode_ModGetI(SP, "CanLaunchRemote", 0)) {
		doit = 0;
	} else if (!Native_GVarGet("TimeToMsg", msg) || !Native_ArgDouble(msg, &m)) {
	} else if (m > now) {
		doit = 0;
	} else if ((Mode_ActionSetName(set), Native_IsStr(set, "Inactive")) || e->type == NV_NONE) {
		doit = 0;
		if (Mode_ModGetI("Init", "HasSuperDeal", 0) && e->type == NV_NONE) {
			if (!Mode_Cam0(cam)) {
				doit = -1;
			} else if (Native_True(cam)) {
				int fcam = Sp_HIs(cam, "Type", "FCam");
				int target = fcam > 0 ? Sp_HIs(cam, "Target", "") : 0;
				int link = fcam > 0 && target == 0 ? Sp_HIs(cam, "EntityLink", "") : 0;
				doit = fcam < 0 || target < 0 || link < 0 ? -1 : fcam > 0 && target == 0 && link > 0;
			}
		}
	} else {
		int w = Mode_VIsType(e, "WalkChar");
		if (w > 0) {
			doit = Mode_VObj(e, "AutoSelect", sel) ? Sp_NumCmp(sel, 1) == 0 : -1;
		} else if (w == 0) {
			doit = 1;
			Mode_ModPutI(SP, "CanLaunchRemote", 0);
		}
	}
	return Mode_Done(&t, 0), doit;
}

/* ForceRemoteComunication(SpeechText, HeadName, HeadIndex=1, AfterRemoteComFunc=None, Parameters=()): the text with
 * the head of the speaker on the monitor, the function after it */
static byte Sp_ForceRemoteComunication(const nativeVal_t *text, const nativeVal_t *head, const nativeVal_t *index,
	const nativeVal_t *after, const nativeVal_t *params) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *time = Mode_V(&t), *e = Mode_V(&t), *full = Mode_V(&t), *txt = Mode_V(&t), *dict = Mode_V(&t), *model = Mode_V(&t);
	if (!Sp_SpeechTime(text, time) || !Sp_SpeechText(text, time, "DefTextColor") || !Sp_Usr(e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		int w = Mode_VIsType(e, "WalkChar");
		if (w < 0 || (w && (!Sp_HSetI(e, "AutoSelect", 0) || !Sp_HSetS(e, "SelectedEntity", "") || !Sp_HSetS(e, "OutSelectFunc", "")))) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Native_IsStr(head, "")) {
		if (!Sp_CatS("CharName_", head, full)) {
			return Mode_Done(&t, 0);
		}
		Sp_Lang(Native_Text(full), txt);
		if (!Sp_G("HeadDict", dict)) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_Sub(dict, head, model)) {
			/* except KeyError */
			if (!Mode_CatchKeyError()) {
				return Mode_Done(&t, 0);
			}
			Native_Copy(model, head);
		}
		if (!Mode_HeadMonitor(model, txt, index)) {
			return Mode_Done(&t, 0);
		}
	}
	Mode_ModPut(SP, "OnEndOfSpeech", after);
	Mode_ModPut(SP, "OnEndOfSpeechParams", params);
	return Mode_Done(&t, Mode_SpeechCallback("Speech.RestoreSpeech"));
}

/* ForceRemoteComInternal(...): the voice of the text on channel 2 and the communication */
static byte Sp_ForceRemoteComInternal(const nativeVal_t *text, const nativeVal_t *head, const nativeVal_t *index,
	const nativeVal_t *after, const nativeVal_t *params) {
	nativeVal_t lang = { 0 };
	if (text->type != NV_STR) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	Sp_Lang(Native_Text(text), &lang);
	byte ok = Sp_OpenVoice(text, 2) && Sp_PlayVoiceAfterI(2, 0) && Sp_ForceRemoteComunication(&lang, head, index, after, params);
	Native_Free(&lang);
	return ok;
}

/* RemoteComunication(...): the communication when one may start now (DoRemote); its result */
static int Sp_RemoteComunication(const nativeVal_t *text, const nativeVal_t *head, const nativeVal_t *index,
	const nativeVal_t *after, const nativeVal_t *params) {
	int doit = Sp_DoRemote();
	if (doit > 0 && !Sp_ForceRemoteComInternal(text, head, index, after, params)) {
		return -1;
	}
	return doit;
}

/* OffVoice(SpeechText): a voice without a head when nobody talks */
static byte Sp_OffVoice(const nativeVal_t *text) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *rest = Mode_V(&t), *sel = Mode_V(&t), *params = Mode_V(&t);
	if (!Sp_Usr(e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	if (!Native_GVarGet("RemainTalkTime", rest)) {
		return Mode_Done(&t, 0);
	}
	int go = Sp_NumCmp(rest, 0) <= 0;
	if (go) {
		int w = Mode_VIsType(e, "WalkChar");
		go = w < 0 ? -1 : !w;
		if (go == 0) {
			go = Mode_VObj(e, "AutoSelect", sel) ? Sp_NumCmp(sel, 1) == 0 : -1;
			if (go > 0) {
				go = Sp_HIs(e, "SelectedEntity", "");
			}
		}
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	Native_SetEmpty(params, NV_TUPLE);
	return Mode_Done(&t, Sp_ForceRemoteComInternal(text, Mode_S(&t, ""), Mode_I(&t, 0), Mode_V(&t), params));
}

/* LaunchTaunt(entityName, charType, numTaunt): the taunt of a character over the radio, not twice in a row */
static byte Sp_LaunchTaunt(const nativeVal_t *ent, const nativeVal_t *typeV, const nativeVal_t *numArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *r = Mode_V(&t), *less = Mode_V(&t), *prevV = Mode_V(&t), *mapV = Mode_V(&t), *tt = Mode_V(&t);
	nativeVal_t *h = Mode_V(&t), *params = Mode_V(&t);
	if (typeV->type != NV_STR) {
		/* string.find(charType, "Mercenary") raises for a value that is no string (one it takes fails at the
		 * "Generics_" + charType after it); the native twin of string.find (an object goes to strop by name,
		 * OBJLIB 9) */
		byte ok = PORT_CALL(string, find, r, "vs", typeV, "Mercenary") && Sp_CatS("Generics_", typeV, less);
		return Mode_Done(&t, ok ? Native_Raise(NE_SYSTEM, "error return without exception set") : 0);
	}
	const char *type = Native_Text(typeV);
	char taunt[0x80], head[0x40], prev[0x80], map[0x40];
	Native_Copy(num, numArg);
	if (strstr(type, "Mercenary")) {
		Mode_Repr(num, r);
		Str_Printf(taunt, sizeof(taunt), "Generics_MercsTaunt_%s", Native_Text(r));
	} else {
		/* in the CA of the eighth super deal the police taunts 5 and 7 (to the group) are 4 and 6 */
		const char *lib = SaveVar_Get("Mission.Library");
		map[0] = 0;
		if (!LvN_ModGet("Init", "UpperMap", mapV)) {
			Native_ErrClear();
		} else if (mapV->type == NV_STR) {
			Str_Copy(map, Native_Text(mapV), sizeof(map));
		}
		if (!strcmp(map, "CA") && lib && !strcmp(lib, "SuperDeal_Eighth") && (Sp_NumCmp(num, 5) == 0 || Sp_NumCmp(num, 7) == 0)) {
			if (!Mode_Arith('-', num, Mode_I(&t, 1), less)) {
				return Mode_Done(&t, 0);
			}
			num = less;
		}
		Mode_Repr(num, r);
		Str_Printf(taunt, sizeof(taunt), "Generics_%sTaunt_%s", type, Native_Text(r));
	}
	prev[0] = 0;
	if (!LvN_ModGet(SP, "prevTaunt", prevV)) {
		Native_ErrClear();
	} else if (prevV->type == NV_STR) {
		Str_Copy(prev, Native_Text(prevV), sizeof(prev));
	}
	if (!strcmp(taunt, prev)) {
		Mode_ModPutS(SP, "LaunchedTauntEntityName", "");
		return Mode_Done(&t, 1);
	}
	Mode_ModPutS(SP, "prevTaunt", taunt);
	const char *bertos = SaveVar_Get("CreateBertos");
	Str_Copy(head, !strcmp(type, "Berto") && bertos && !strcmp(bertos, "1") ? "EvilBerto" : type, sizeof(head));
	Native_SetStr(tt, taunt);
	Native_SetStr(h, head);
	Native_SetEmpty(params, NV_TUPLE);
	int sent = Sp_RemoteComunication(tt, h, Mode_I(&t, 1), Mode_V(&t), params);
	if (sent > 0) {
		Mode_ModPut(SP, "LaunchedTauntEntityName", ent);
	}
	return Mode_Done(&t, sent >= 0);
}

/* AddRemoteCom(SpeechText, HeadName, AfterRemoteComFunc=None, Parameters=(), HeadIndex=1, InsertFirst=0): a message
 * in the queue (not twice), sent 0.2 s later */
static byte Sp_AddRemoteCom(const nativeVal_t *text, const nativeVal_t *head, const nativeVal_t *after, const nativeVal_t *params,
	const nativeVal_t *index, byte first) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cur = Mode_V(&t), *list = Mode_V(&t), *item = Mode_V(&t), *it = Mode_V(&t), *msg = Mode_V(&t);
	if (!Sp_G("CurrentRemoteCom", cur) || !Sp_G("RemoteCom", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Len(list);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	byte dup = Native_Compare(text, cur) == 0;
	for (int i = 0; !dup && i < n; i++) {
		Native_Free(item);
		Native_Free(it);
		if (!Sp_Item(list, i, item) || !Sp_Item(item, 0, it)) {
			return Mode_Done(&t, 0);
		}
		dup = Native_Compare(it, text) == 0;
	}
	if (dup) {
		return Mode_Done(&t, 1);
	}
	Native_SetEmpty(msg, NV_TUPLE);
	Native_Copy(Native_Append(msg), text);
	Native_Copy(Native_Append(msg), head);
	Native_Copy(Native_Append(msg), after);
	Native_Copy(Native_Append(msg), params);
	Native_Free(list);
	if (!Mode_ListAddG(SP, "RemoteCom", msg, first) || !Sp_G("RemoteCom", list)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_Len(list) < 1) {
		return Mode_Done(&t, 1);
	}
	Native_Free(item);
	Native_Free(it);
	if (!Sp_Item(list, 0, item) || !Sp_Item(item, 0, it)) {
		return Mode_Done(&t, 0);
	}
	byte ok = Mode_DelSchedS("ProcessRemoteComList");
	Mode_ModPut(SP, "CurrentRemoteCom", it);
	return Mode_Done(&t, ok && Mode_LaterIn(0.2, SP, "ProcessRemoteCom", index, 1, "ProcessRemoteComList"));
}

/* ProcessRemoteCom(HeadIndex): the first message of the queue when it can be sent, again 5 s later while some wait */
static byte Sp_ProcessRemoteCom(const nativeVal_t *index) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *list = Mode_V(&t), *msg = Mode_V(&t), *a = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	if (!Sp_G("RemoteCom", list)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(list)) {
		return Mode_Done(&t, 1);
	}
	if (!Sp_Item(list, 0, msg) || !Sp_Item(msg, 0, &a[0]) || !Sp_Item(msg, 1, &a[1]) || !Sp_Item(msg, 2, &a[2]) ||
		!Sp_Item(msg, 3, &a[3])) {
		return Mode_Done(&t, 0);
	}
	int sent = Sp_RemoteComunication(&a[0], &a[1], index, &a[2], &a[3]);
	if (sent < 0 || (sent && !Mode_DelItemG(SP, "RemoteCom", 0))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(list);
	if (!Sp_G("RemoteCom", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Len(list);
	return Mode_Done(&t, n >= 0 && (n == 0 || Mode_LaterIn(5.0, SP, "ProcessRemoteCom", index, 1, "ProcessRemoteComList")));
}

/* ---- speeches with objects ---- */

/* the range test each second while OnOutOfRange is set */
static byte Sp_TestLater(void) {
	nativeVal_t f = { 0 };
	byte ok = Sp_G("OnOutOfRange", &f) && (!Native_True(&f) || Mode_LaterIn(1, SP, "TestInRange", NULL, 0, "TestInRange"));
	Native_Free(&f);
	return ok;
}

/* TestInRange(): OnOutOfRange when the player walks away (5 m) or the object offers to talk again */
static byte Sp_TestInRange(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *player = Mode_V(&t), *obj = Mode_V(&t), *hook = Mode_V(&t), *sel = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t);
	nativeVal_t *d = Mode_V(&t), *m = Mode_V(&t);
	if (!Sp_Usr(player) || !Sp_G("ObjEntity", obj)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(player) || !Native_True(obj)) {
		return Mode_Done(&t, 1);
	}
	int abort = Sp_G("OnOutOfRange", hook) ? Native_True(hook) : -1;
	if (abort > 0) {
		abort = Sp_HPrefix(obj, "ArrOpt0", "Talk:");
	}
	if (abort > 0) {
		abort = Mode_VObj(player, "AutoSelect", sel) ? Native_True(sel) : -1;
	}
	int far = 0;
	if (abort == 0) {
		far = Mode_VObj(obj, "Pos", a) && Mode_VObj(player, "Pos", b) && Mode_VecSub(a, b, d) && Mode_VecModSqr(d, m) ?
			Sp_NumCmp(m, 250000) > 0 : -1;
	}
	if (abort < 0 || far < 0) {
		return Mode_Done(&t, 0);
	}
	if (!abort && !far) {
		return Mode_Done(&t, Mode_LaterIn(1, SP, "TestInRange", NULL, 0, "TestInRange"));
	}
	if (!Native_True(hook)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_CallArgs(hook, NULL, NULL, 0) || !Mode_GVarSetN("RemainTalkTime", 0.0)) {
		return Mode_Done(&t, 0);
	}
	Native_SetNone(a);
	Mode_ModPut(SP, "OnOutOfRange", a);
	return Mode_Done(&t, 1);
}

/* StartObjectSpeech(obj, action, defLoop, text, charType, speechCallback): the object speaks over the radio */
static byte Sp_StartObjectSpeech(const nativeVal_t *obj, const nativeVal_t *action, const nativeVal_t *defLoop,
	const nativeVal_t *text, const nativeVal_t *type, const nativeVal_t *cb) {
	nativeVal_t end = { 0 }, params = { 0 }, one;
	Mode_ModPut(SP, "ObjEntity", obj);
	Mode_ModPut(SP, "ObjCharType", type);
	Native_SetInt(&one, 1);
	byte ok = (Native_IsStr(action, "") || Mode_VSet(obj, "Action", action)) &&
		(Native_IsStr(defLoop, "") || Mode_VSet(obj, "DefLoop", defLoop)) && Sp_G("OnEndOfSpeech", &end) &&
		Sp_G("OnEndOfSpeechParams", &params) && Sp_ForceRemoteComunication(text, type, &one, &end, &params) &&
		Mode_SpeechCallbackV(cb) && Sp_TestLater();
	Native_Free(&end);
	Native_Free(&params);
	return ok;
}

/* StartObjSpch(obj, text, charType, speechCallback="Speech.EndObjectAnswer"): with the voice of the text */
static byte Sp_StartObjSpch(const nativeVal_t *obj, const nativeVal_t *text, const nativeVal_t *type, const nativeVal_t *cb) {
	nativeVal_t empty, lang = { 0 };
	Native_SetText(&empty, "", 0);
	byte ok = Sp_OpenVoice(text, 2) && Sp_PlayVoiceAfterI(2, 0) && Mode_LangV(text, &lang) &&
		Sp_StartObjectSpeech(obj, &empty, &empty, &lang, type, cb);
	Native_Free(&lang);
	return ok;
}

/* OnObjectSpeech(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk"): the player
 * talks to an object with the head of a character; it answers when the text ends (ObjectAutoAnswer). 1 into out, None
 * without a player */
static byte Sp_OnObjectSpeech(const nativeVal_t *obj, const nativeVal_t *type, const nativeVal_t *text, const nativeVal_t *ans,
	const nativeVal_t *action, const nativeVal_t *defLoop, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *player = Mode_V(&t), *name = Mode_V(&t), *me = Mode_V(&t), *time = Mode_V(&t), *dict = Mode_V(&t);
	nativeVal_t *model = Mode_V(&t), *one = Mode_V(&t);
	if (out) {
		Native_SetNone(out);
	}
	if (!Sp_Usr(player)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(player)) {
		return Mode_Done(&t, 1);
	}
	Mode_ModPut(SP, "ObjEntity", obj);
	Mode_ModPut(SP, "ObjAnswer", ans);
	Mode_ModPut(SP, "ObjCharType", type);
	if (!Mode_VObj(player, "Name", name) || !Mode_EntitySeam(name, me)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(me) && (!Sp_HSetS(obj, "Action", "Listen") || !Sp_HSetS(me, "Action", "Talk") || !Sp_HSetS(me, "DefLoop", "Talk") ||
		!Sp_HSetS(me, "OutSelectFunc", "Speech.AbortSpeech") || !Sp_HSetI(me, "AutoSelect", 0))) {
		return Mode_Done(&t, 0);
	}
	if (!Sp_SpeechTime(text, time) || !Sp_SpeechText(text, time, "UsrTextColor") || !Sp_G("HeadDict", dict)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_Sub(dict, type, model)) {
		/* except KeyError */
		if (!Mode_CatchKeyError()) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(model, type);
	}
	if (!Mode_VSet(obj, "HeadModel", model) || !Sp_HSetI(obj, "HeadAnimIndex", 0)) {
		return Mode_Done(&t, 0);
	}
	Mode_ModPut(SP, "ObjAutoAnswerAction", action);
	Mode_ModPut(SP, "ObjAutoAnswerDefLoop", defLoop);
	Native_SetInt(one, 1);
	if (!Mode_SpeechCallback("Speech.ObjectAutoAnswer") || (Mode_ModGetI(SP, "LockAlarm", 0) && !Mode_SetLockAlarm(one)) ||
		!Sp_TestLater()) {
		return Mode_Done(&t, 0);
	}
	if (out) {
		Native_SetInt(out, 1);
	}
	return Mode_Done(&t, 1);
}

/* OnObjSpch(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk"): by the keys of the
 * language file, with the voices */
static byte Sp_OnObjSpch(const nativeVal_t *obj, const nativeVal_t *type, const nativeVal_t *text, const nativeVal_t *ans,
	const nativeVal_t *action, const nativeVal_t *defLoop) {
	nativeVal_t tl = { 0 }, al = { 0 };
	byte ok = Sp_OpenVoice(text, 0) && Sp_OpenVoice(ans, 2) && Sp_PlayVoiceAfterI(0, 0) && Mode_LangV(text, &tl) &&
		Mode_LangV(ans, &al) && Sp_OnObjectSpeech(obj, type, &tl, &al, action, defLoop, NULL);
	Native_Free(&tl);
	Native_Free(&al);
	return ok;
}

/* StdObjSpch(_ObjEntity, _ObjCharType, optId, action="Talk", defLoop="Talk") */
static byte Sp_StdObjSpch(const nativeVal_t *obj, const nativeVal_t *type, const nativeVal_t *opt, const nativeVal_t *action,
	const nativeVal_t *defLoop) {
	nativeVal_t msg = { 0 }, ans = { 0 };
	byte ok = Sp_Cat(opt, "Msg", &msg) && Sp_Cat(opt, "Ans", &ans) && Sp_OnObjSpch(obj, type, &msg, &ans, action, defLoop);
	Native_Free(&msg);
	Native_Free(&ans);
	return ok;
}

/* OnChallengeObjSpch(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk", isMerc=0) */
static byte Sp_OnChallengeObjSpch(const nativeVal_t *obj, const nativeVal_t *type, const nativeVal_t *text, const nativeVal_t *ans,
	const nativeVal_t *action, const nativeVal_t *defLoop, const nativeVal_t *merc) {
	nativeVal_t al = { 0 };
	byte isMerc = merc && Sp_NumCmp(merc, 1) == 0;
	byte ok = Sp_OpenVoiceS(isMerc ? "ChallengeRaceStdOpt2Voice" : "ChallengeRaceStdOpt1Voice", 0) && Sp_OpenVoice(ans, 2) &&
		Sp_PlayVoiceAfterI(0, 0) && Mode_LangV(ans, &al) && Sp_OnObjectSpeech(obj, type, text, &al, action, defLoop, NULL);
	Native_Free(&al);
	return ok;
}

/* ObjectAutoAnswer(): the end of the player's line to the object, its answer follows */
static byte Sp_ObjectAutoAnswer(void) {
	nativeVal_t me = { 0 };
	int talk = Sp_Usr(&me) ? Sp_HPrefix(&me, "Action", "Talk") : -1;
	byte ok = talk >= 0 && (!talk || Sp_HSetS(&me, "ForceAction", "TalkListenNPC"));
	Native_Free(&me);
	if (ok) {
		Sp_StopMenuEvents();
		ok = Mode_LaterIn(1.0, SP, "RestartMenuEvents", NULL, 0, NULL) && Mode_LaterIn(0.45, SP, "ObjectAutoAnswer2", NULL, 0, NULL);
	}
	return ok;
}

/* ObjectAutoAnswer2(): the answer of the object over the radio */
static byte Sp_ObjectAutoAnswer2(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *obj = Mode_V(&t), *action = Mode_V(&t), *loop = Mode_V(&t), *ans = Mode_V(&t), *type = Mode_V(&t);
	nativeVal_t *me = Mode_V(&t), *listen = Mode_V(&t);
	if (!Mode_StopVoice(Mode_I(&t, 0)) || !Sp_PlayVoiceScheduled(Mode_I(&t, 2), 0, Mode_I(&t, 0)) || !Sp_G("ObjEntity", obj) ||
		!Sp_G("ObjAutoAnswerAction", action) || !Sp_G("ObjAutoAnswerDefLoop", loop) || !Sp_G("ObjAnswer", ans) ||
		!Sp_G("ObjCharType", type) || !Sp_StartObjectSpeech(obj, action, loop, ans, type, Mode_S(&t, "Speech.EndObjectAnswer")) ||
		!Sp_Usr(me)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(me) || (Sp_G("TalkListen", listen) && Mode_VSet(me, "ForceAction", listen) &&
		Sp_HSetS(me, "ForceDefLoop", "Relax")));
}

/* EndObjectAnswer(): the object listens again, the head stops talking */
static byte Sp_EndObjectAnswer(void) {
	nativeVal_t obj = { 0 };
	int relax = Sp_G("ObjEntity", &obj) ? Sp_HIs(&obj, "Action", "Relax") : -1;
	byte ok = relax >= 0 && (relax || Sp_HSetS(&obj, "Action", "Listen")) && Mode_Set0I("Monitor", "HeadAnimIndex", 0);
	Native_Free(&obj);
	return ok && Sp_RestoreSpeech();
}

/* ---- entries ---- */

/* the functions the port called without parsing their arguments (any arguments are taken) */
MODE_FNANY(Speech, CallEndOfSpeech, Sp_CallEndOfSpeech())
MODE_FNANY(Speech, NormalAbortSpeech, Sp_NormalAbortSpeech())
MODE_FNANY(Speech, StopMenuEvents, Sp_StopMenuEvents())
MODE_FNANY(Speech, RestartMenuEvents, Sp_RestartMenuEvents())
MODE_FNANY(Speech, AutoAnswer, Sp_AutoAnswer())
MODE_FNANY(Speech, ObjNIEnd, Sp_ObjNIEnd())
MODE_FNANY(Speech, RestoreSpeech, Sp_RestoreSpeech())
MODE_FNANY(Speech, CancelRemoteCom, Mode_CancelSpeech())
MODE_FNANY(Speech, ObjectAutoAnswer, Sp_ObjectAutoAnswer())
MODE_FNANY(Speech, ObjectAutoAnswer2, Sp_ObjectAutoAnswer2())

/* DoRemote(): its answer as an int */
static byte Sp_DoRemoteOut(nativeVal_t *out) {
	int r = Sp_DoRemote();
	if (r >= 0) {
		Native_SetInt(out, r);
	}
	return r >= 0;
}

MODE_FNANY(Speech, DoRemote, Sp_DoRemoteOut(out))

MODE_FN(Speech, ScheduledVoiceCallback, 2, Sp_ScheduledVoiceCallback(&a[0], &a[1]))
MODE_FN(Speech, PlayVoiceScheduled, 3, Sp_PlayVoiceScheduledV(&a[0], &a[1], &a[2]))
MODE_FN(Speech, RestoreState, 1, Sp_RestoreState(&a[0]))
MODE_FN(Speech, RestoreMoveTime, 1, Sp_HSetI(&a[0], "MoveTime", 0))
MODE_FN(Speech, StdAnsLst, 1, Sp_StdAnsLst(&a[0]))
MODE_FN(Speech, CalculateSpeechTime, 1, Sp_SpeechTime(&a[0], out))
MODE_FN(Speech, PlayerAnsEnd, 0, Sp_PlayerAnsEnd())
MODE_FN(Speech, PlayerAns, 4, Sp_IntOut(Sp_PlayerAns(&a[0], &a[1], &a[2], &a[3]), out))
MODE_FN(Speech, SetPlayerToTalk, 2, Sp_IntOut(Sp_SetPlayerToTalk(&a[0], &a[1]), out))
MODE_FN(Speech, InspectChar, 4, Sp_InspectChar(&a[0], &a[1]))
MODE_FN(Speech, AutoSpeech, 4, Sp_AutoSpeech(a))
MODE_FN(Speech, LaunchTaunt, 3, Sp_LaunchTaunt(&a[0], &a[1], &a[2]))
MODE_FN(Speech, OffVoice, 1, Sp_OffVoice(&a[0]))
MODE_FN(Speech, ProcessRemoteCom, 1, Sp_ProcessRemoteCom(&a[0]))
MODE_FN(Speech, TestInRange, 0, Sp_TestInRange())
MODE_FN(Speech, StartObjectSpeech, 6, Sp_StartObjectSpeech(&a[0], &a[1], &a[2], &a[3], &a[4], &a[5]))
MODE_FN(Speech, EndObjectAnswer, 0, Sp_EndObjectAnswer())

/* the keyword functions: the arguments by name with the defaults of the script (MODE_KW) */
MODE_KW(Speech, AbortSpeech, "|OOO", "UsrName", "CharName", "extrangextra") {
	return Sp_AbortSpeech();
}

MODE_KW(Speech, PerformAnswer, "O|O", "SpeechText", "Time") {
	Mode_KwDefI(v, given, 1, 6);
	return Sp_PerformAnswer(&v[0], &v[1], out);
}

MODE_KW(Speech, OnSpeech, "O|OO", "SpeechText", "SpeechAnswer", "_AnsAnima_") {
	Mode_KwDefS(v, given, 1, "");
	Mode_KwDefS(v, given, 2, "TalkNPC");
	return Sp_IntOut(Sp_OnSpeech(&v[0], &v[1], &v[2]), out);
}

MODE_KW(Speech, OnSpch, "OO|O", "LngSpeechText", "LngSpeechAnswer", "_AnsAnima_") {
	Mode_KwDefS(v, given, 2, "TalkNPC");
	return Sp_IntOut(Sp_OnSpch(&v[0], &v[1], &v[2]), out);
}

MODE_KW(Speech, OnChallengeSpch, "OO|OO", "LngSpeechText", "LngSpeechAnswer", "_AnsAnima_", "isMerc") {
	Mode_KwDefS(v, given, 2, "TalkNPC");
	return Sp_IntOut(Sp_OnChallengeSpch(&v[0], &v[1], &v[2], given[3] ? &v[3] : NULL), out);
}

MODE_KW(Speech, StdOnSpch, "O|O", "optId", "_AnsAnima_") {
	Mode_KwDefS(v, given, 1, "TalkNPC");
	return Sp_StdOnSpch(&v[0], &v[1]);
}

MODE_KW(Speech, NISpeech, "|O", "headName") {
	Mode_KwDefS(v, given, 0, "");
	return Sp_NISpeech(&v[0]);
}

MODE_KW(Speech, AddNI, "OO|OO", "LngSpeechText", "LngSpeechAnswer", "_AnsAnima_", "HeadName") {
	return Sp_AddNI(&v[0], &v[1], given[2] ? &v[2] : NULL, given[3] ? &v[3] : NULL);
}

MODE_KW(Speech, AutoAddNI, "O|OO", "TextBase", "_AnsAnima_", "HeadName") {
	return Sp_AutoAddNI(&v[0], given[1] ? &v[1] : NULL, given[2] ? &v[2] : NULL);
}

MODE_KW(Speech, RunNI, "|O", "i") {
	Mode_KwDefI(v, given, 0, 0);
	return Sp_RunNI(&v[0]);
}

/* the defaults of (SpeechText, HeadName, HeadIndex=1, AfterRemoteComFunc=None, Parameters=()) */
static void Sp_RemoteDefs(nativeVal_t *v, const byte *given) {
	Mode_KwDefI(v, given, 2, 1);
	if (!given[4]) {
		Native_SetEmpty(&v[4], NV_TUPLE);
	}
}

#define SP_REMOTE(name) MODE_KW(Speech, name, "OO|OOO", "SpeechText", "HeadName", "HeadIndex", "AfterRemoteComFunc", "Parameters")

SP_REMOTE(ForceRemoteComunication) {
	Sp_RemoteDefs(v, given);
	return Sp_ForceRemoteComunication(&v[0], &v[1], &v[2], &v[3], &v[4]);
}

SP_REMOTE(ForceRemoteComInternal) {
	Sp_RemoteDefs(v, given);
	return Sp_ForceRemoteComInternal(&v[0], &v[1], &v[2], &v[3], &v[4]);
}

SP_REMOTE(RemoteComunication) {
	Sp_RemoteDefs(v, given);
	return Sp_IntOut(Sp_RemoteComunication(&v[0], &v[1], &v[2], &v[3], &v[4]), out);
}

/* ForceRemoteCom(...): the message first in the queue, the current speech ends */
SP_REMOTE(ForceRemoteCom) {
	Sp_RemoteDefs(v, given);
	return Sp_AddRemoteCom(&v[0], &v[1], &v[3], &v[4], &v[2], 1) && Mode_CancelSpeech();
}

MODE_KW(Speech, AddRemoteCom, "OO|OOOO", "SpeechText", "HeadName", "AfterRemoteComFunc", "Parameters", "HeadIndex",
	"InsertFirst") {
	if (!given[3]) {
		Native_SetEmpty(&v[3], NV_TUPLE);
	}
	Mode_KwDefI(v, given, 4, 1);
	return Sp_AddRemoteCom(&v[0], &v[1], &v[2], &v[3], &v[4], given[5] && Native_True(&v[5]));
}

MODE_KW(Speech, StartObjSpch, "OOO|O", "obj", "text", "charType", "speechCallback") {
	Mode_KwDefS(v, given, 3, "Speech.EndObjectAnswer");
	return Sp_StartObjSpch(&v[0], &v[1], &v[2], &v[3]);
}

/* the defaults of action and defLoop (slots i and i + 1) */
static void Sp_TalkDefs(nativeVal_t *v, const byte *given, int i) {
	Mode_KwDefS(v, given, i, "Talk");
	Mode_KwDefS(v, given, i + 1, "Talk");
}

MODE_KW(Speech, OnObjectSpeech, "OOOO|OO", "_ObjEntity", "_ObjCharType", "SpeechText", "SpeechAnswer", "action",
	"defLoop") {
	Sp_TalkDefs(v, given, 4);
	return Sp_OnObjectSpeech(&v[0], &v[1], &v[2], &v[3], &v[4], &v[5], out);
}

MODE_KW(Speech, OnObjSpch, "OOOO|OO", "_ObjEntity", "_ObjCharType", "SpeechText", "SpeechAnswer", "action", "defLoop") {
	Sp_TalkDefs(v, given, 4);
	return Sp_OnObjSpch(&v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
}

MODE_KW(Speech, StdObjSpch, "OOO|OO", "_ObjEntity", "_ObjCharType", "optId", "action", "defLoop") {
	Sp_TalkDefs(v, given, 3);
	return Sp_StdObjSpch(&v[0], &v[1], &v[2], &v[3], &v[4]);
}

MODE_KW(Speech, OnChallengeObjSpch, "OOOO|OOO", "_ObjEntity", "_ObjCharType", "SpeechText", "SpeechAnswer", "action",
	"defLoop", "isMerc") {
	Sp_TalkDefs(v, given, 4);
	return Sp_OnChallengeObjSpch(&v[0], &v[1], &v[2], &v[3], &v[4], &v[5], given[6] ? &v[6] : NULL);
}

const portFn_t g_SpeechMethods[] = {
	PORT_FN(ScheduledVoiceCallback, Speech_ScheduledVoiceCallback),
	PORT_FN(PlayVoiceScheduled, Speech_PlayVoiceScheduled),
	PORT_FNK(PerformAnswer, Speech_PerformAnswer),
	PORT_FN(NormalAbortSpeech, Speech_NormalAbortSpeech),
	PORT_FN(CallEndOfSpeech, Speech_CallEndOfSpeech),
	PORT_FNK(AbortSpeech, Speech_AbortSpeech),
	PORT_FN(RestoreState, Speech_RestoreState),
	PORT_FNK(AbortNPCSpeech, Speech_AbortNPCSpeech),
	PORT_FN(AutoAnswer, Speech_AutoAnswer),
	PORT_FN(StopMenuEvents, Speech_StopMenuEvents),
	PORT_FN(RestartMenuEvents, Speech_RestartMenuEvents),
	PORT_FN(RestoreMoveTime, Speech_RestoreMoveTime),
	PORT_FN(StdAnsLst, Speech_StdAnsLst),
	PORT_FNK(StdOnSpch, Speech_StdOnSpch),
	PORT_FN(CalculateSpeechTime, Speech_CalculateSpeechTime),
	PORT_FNK(OnSpch, Speech_OnSpch),
	PORT_FNK(OnChallengeSpch, Speech_OnChallengeSpch),
	PORT_FNK(OnSpeech, Speech_OnSpeech),
	PORT_FN(PlayerAnsEnd, Speech_PlayerAnsEnd),
	PORT_FN(PlayerAns, Speech_PlayerAns),
	PORT_FN(SetPlayerToTalk, Speech_SetPlayerToTalk),
	PORT_FN(InspectChar, Speech_InspectChar),
	PORT_FN(AutoSpeech, Speech_AutoSpeech),
	PORT_FNK(NISpeech, Speech_NISpeech),
	PORT_FNK(AutoAddNI, Speech_AutoAddNI),
	PORT_FNK(AddNI, Speech_AddNI),
	PORT_FNK(RunNI, Speech_RunNI),
	PORT_FN(ObjNIEnd, Speech_ObjNIEnd),
	PORT_FN(LaunchTaunt, Speech_LaunchTaunt),
	PORT_FN(RestoreSpeech, Speech_RestoreSpeech),
	PORT_FN(DoRemote, Speech_DoRemote),
	PORT_FN(OffVoice, Speech_OffVoice),
	PORT_FNK(RemoteComunication, Speech_RemoteComunication),
	PORT_FNK(ForceRemoteComInternal, Speech_ForceRemoteComInternal),
	PORT_FNK(ForceRemoteCom, Speech_ForceRemoteCom),
	PORT_FNK(ForceRemoteComunication, Speech_ForceRemoteComunication),
	PORT_FNK(AddRemoteCom, Speech_AddRemoteCom),
	PORT_FN(ProcessRemoteCom, Speech_ProcessRemoteCom),
	PORT_FN(CancelRemoteCom, Speech_CancelRemoteCom),
	PORT_FN(TestInRange, Speech_TestInRange),
	PORT_FNK(StartObjSpch, Speech_StartObjSpch),
	PORT_FN(StartObjectSpeech, Speech_StartObjectSpeech),
	PORT_FNK(StdObjSpch, Speech_StdObjSpch),
	PORT_FNK(OnObjSpch, Speech_OnObjSpch),
	PORT_FNK(OnChallengeObjSpch, Speech_OnChallengeObjSpch),
	PORT_FNK(OnObjectSpeech, Speech_OnObjectSpeech),
	PORT_FN(ObjectAutoAnswer, Speech_ObjectAutoAnswer),
	PORT_FN(ObjectAutoAnswer2, Speech_ObjectAutoAnswer2),
	PORT_FN(EndObjectAnswer, Speech_EndObjectAnswer),
	PORT_FNEND
};

PORTS(Speech)
