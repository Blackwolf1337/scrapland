/*
 * The Speech module in C (scripts/chars/speech.py, issue #2 steps 7b and 7c): the talks between the player and the
 * characters. Part 1: the voices, a speech and its answer (OnSpch, OnSpeech, AutoAnswer, PerformAnswer), the end and
 * the abort of a speech, the answer list of a character, the player's own answers, InspectChar and AutoSpeech.
 * Part 2: the non-interactive talks (NISpeech, AddNI, RunNI), the remote communications and taunts over the radio
 * (with their queue RemoteCom), and the speeches with objects that carry the head of a character.
 *
 * The state of a speech stays in the variables of the module (PlayerName, NPCName, OnEndOfSpeech, the colours,
 * AnsAnima, TalkNI and so on), because the character scripts and the missions read and set them. The code works on
 * the entity handles like the script, so the values it passes on keep their Python types, and it stops at the first
 * error like the script.
 */
#include <string.h>

#include "scrap/pyentity.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

/* a variable of the module (a new reference, NULL with the error set) */
static PyObject *Sp_Get(const char *var) {
	return Script_Attr("Speech", var);
}

/* Scrap.GetEntity(<module variable var>): a handle or None (a new reference, NULL with the error set) */
static PyObject *Sp_Entity(const char *var) {
	PyObject *name = Sp_Get(var);
	PyObject *e = name ? Script_Ask("Scrap", "GetEntity", "(O)", name) : NULL;
	Py_XDECREF(name);
	return e;
}

/* Scrap.GetLangStr(key) (a new reference) */
static PyObject *Sp_Lang(const char *key) {
	return Script_Ask("Scrap", "GetLangStr", "(s)", key);
}

/* the entity of camera 0 (a new reference) */
static PyObject *Sp_Cam(void) {
	PyObject *name = Script_Ask("Scrap", "GetCam", "(i)", 0);
	PyObject *cam = name ? Script_Ask("Scrap", "GetEntity", "(O)", name) : NULL;
	Py_XDECREF(name);
	return cam;
}

/* Scrap.Get("VoicePath") + text, opened on a voice channel */
static byte Sp_OpenVoice(PyObject *text, int channel) {
	PyObject *path = Script_Ask("Scrap", "Get", "(s)", "VoicePath");
	PyObject *file = path ? PySequence_Concat(path, text) : NULL;
	byte ok = file && Script_Try("SSound", "OpenVoice", "(Oi)", file, channel);
	Py_XDECREF(path);
	Py_XDECREF(file);
	return ok;
}

static byte Sp_OpenVoiceStr(const char *text, int channel) {
	PyObject *t = PyString_FromString(text);
	byte ok = t && Sp_OpenVoice(t, channel);
	Py_XDECREF(t);
	return ok;
}

/* SScorer.SetSpeechText(text, time, colour[0], colour[1], colour[2]) with a colour variable of the module */
static byte Sp_SpeechText(PyObject *text, PyObject *time, const char *colour) {
	PyObject *c = Sp_Get(colour);
	PyObject *r = c ? PySequence_GetItem(c, 0) : NULL, *g = r ? PySequence_GetItem(c, 1) : NULL;
	PyObject *b = g ? PySequence_GetItem(c, 2) : NULL;
	byte ok = b && Script_Try("SScorer", "SetSpeechText", "(OOOOO)", text, time, r, g, b);
	Py_XDECREF(c);
	Py_XDECREF(r);
	Py_XDECREF(g);
	Py_XDECREF(b);
	return ok;
}

/* a + text: string concatenation as a script does it (a new reference, NULL with the error set) */
static PyObject *Sp_Cat(PyObject *a, const char *text) {
	PyObject *t = PyString_FromString(text);
	PyObject *r = a && t ? PySequence_Concat(a, t) : NULL;
	Py_XDECREF(t);
	return r;
}

/* `if handle:` of a script */
static byte Sp_True(PyObject *o) {
	return o && PyObject_IsTrue(o) > 0;
}

/* SVec.GetRotAng(SVec.Sub(me.Pos, ch.Pos)): the angles from ch to me (a new reference) */
static PyObject *Sp_RotAng(PyObject *me, PyObject *ch) {
	PyObject *a = PyObject_GetAttrString(me, "Pos"), *b = a ? PyObject_GetAttrString(ch, "Pos") : NULL;
	PyObject *d = b ? Script_Ask("SVec", "Sub", "(OO)", a, b) : NULL;
	PyObject *v = d ? Script_Ask("SVec", "GetRotAng", "(O)", d) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(b);
	Py_XDECREF(d);
	return v;
}

/* (vang[0], vang[1] - turn, 0): the view of the player turned from the character */
static PyObject *Sp_Turned(PyObject *vang, long turn) {
	PyObject *x = PySequence_GetItem(vang, 0), *y = x ? PySequence_GetItem(vang, 1) : NULL;
	PyObject *t = y ? PyInt_FromLong(turn) : NULL;
	PyObject *yt = t ? PyNumber_Subtract(y, t) : NULL;
	PyObject *r = yt ? Py_BuildValue("(OOi)", x, yt, 0) : NULL;
	Py_XDECREF(x);
	Py_XDECREF(y);
	Py_XDECREF(t);
	Py_XDECREF(yt);
	return r;
}

/* ---- the voices ---- */

/* ScheduledVoiceCallback(channel, rtime): the voice starts; the talk time is its rest plus rtime (0.2 at least) */
static PyObject *Speech_ScheduledVoiceCallback(PyObject *self, PyObject *args) {
	PyObject *channel, *rtime;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &channel, &rtime) || !Script_Try("SSound", "PlayVoice", "(O)", channel)) {
		return NULL;
	}
	PyObject *full = Script_Ask("Scrap", "Get", "(s)", "FullTalkTime");
	PyObject *rest = full ? Script_Ask("SSound", "VoiceRemain", "(OO)", channel, full) : NULL;
	PyObject *t = rest ? PyNumber_Add(rest, rtime) : NULL;
	PyObject *least = t ? PyFloat_FromDouble(0.2) : NULL;
	Py_XDECREF(full);
	Py_XDECREF(rest);
	if (!least) {
		Py_XDECREF(t);
		return NULL;
	}
	if (PyObject_Compare(least, t) > 0) {
		Py_DECREF(t);
		Py_INCREF(least);
		t = least;
	}
	Py_DECREF(least);
	byte ok = Script_Try("Scrap", "Set", "(sO)", "FullTalkTime", t) && Script_Try("Scrap", "Set", "(sO)", "RemainTalkTime", t);
	Py_DECREF(t);
	return ok ? Py_ReturnNone() : NULL;
}

/* PlayVoiceScheduled(channel, stime, rtime) */
static byte Sp_PlayVoiceScheduled(PyObject *channel, double stime, PyObject *rtime) {
	return Script_LaterIn(stime, "Speech", "ScheduledVoiceCallback", Py_BuildValue("(OO)", channel, rtime), NULL);
}

/* the same with the voice delay of the module and a channel number */
static byte Sp_PlayVoiceAfter(int channel, PyObject *rtime) {
	PyObject *c = PyInt_FromLong(channel);
	byte ok = c && rtime && Sp_PlayVoiceScheduled(c, Script_GetFloat("Speech", "VoiceShTime", 0.25), rtime);
	Py_XDECREF(c);
	Py_XDECREF(rtime);
	return ok;
}

static PyObject *Speech_PlayVoiceScheduled(PyObject *self, PyObject *args) {
	PyObject *channel, *rtime;
	double stime;
	(void)self;
	if (!PyArg_ParseTuple(args, "OdO", &channel, &stime, &rtime)) {
		return NULL;
	}
	return Sp_PlayVoiceScheduled(channel, stime, rtime) ? Py_ReturnNone() : NULL;
}

/* ---- the end of a speech ---- */

/* CallEndOfSpeech(): the voices stop, the alarm unlocks, OnEndOfSpeech(*OnEndOfSpeechParams) runs once */
static PyObject *Speech_CallEndOfSpeech(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	for (int i = 0; i < 3; i++) {
		if (!Script_Try("SSound", "StopVoice", "(i)", i)) {
			return NULL;
		}
	}
	Script_SetVar("Speech", "TalkListen", PyString_FromString("TalkListen"));
	if (Script_GetInt("Speech", "LockAlarm", 0)) {
		if (!Script_Try("Scrap", "SetLockAlarm", "(i)", 0)) {
			return NULL;
		}
		Script_SetInt("Speech", "LockAlarm", 0);
	}
	PyObject *end = Sp_Get("OnEndOfSpeech");
	if (!end) {
		return NULL;
	}
	if (PyObject_IsTrue(end) > 0) {
		PyObject *params = Sp_Get("OnEndOfSpeechParams");
		Py_INCREF(Py_None);
		Script_SetVar("Speech", "OnEndOfSpeech", Py_None);
		Script_SetVar("Speech", "OnEndOfSpeechParams", PyTuple_New(0));
		PyObject *r = params ? PyEval_CallObject(end, params) : NULL;
		Py_XDECREF(params);
		if (!r) {
			Py_DECREF(end);
			return NULL;
		}
		Py_DECREF(r);
	}
	Py_DECREF(end);
	Script_SetVar("Speech", "CurrentRemoteCom", PyString_FromString(""));
	Script_SetVar("Speech", "LaunchedTauntEntityName", PyString_FromString(""));
	Script_SetInt("Speech", "CanLaunchRemote", 1);
	return Py_ReturnNone();
}

/* RestoreState(UsrName): the character offers to talk again and goes back to its own logic */
static PyObject *Speech_RestoreState(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *c = Script_Ask("Scrap", "GetEntity", "(O)", name);
	if (!c) {
		return NULL;
	}
	byte ok = 1;
	if (PyObject_IsTrue(c) > 0) {
		PyObject *talk = Sp_Lang("TalkOpt");
		PyObject *head = talk ? PyString_FromString("Talk:") : NULL;
		PyObject *opt = head ? PySequence_Concat(head, talk) : NULL;
		Py_XDECREF(talk);
		Py_XDECREF(head);
		ok = Script_HSet(c, "ArrnOpt", PyInt_FromLong(0)) && Script_HSet(c, "ArrOpt0", opt) &&
			Script_HSet(c, "LLogicIdle", PyInt_FromLong(0));
	}
	Py_DECREF(c);
	return ok ? Py_ReturnNone() : NULL;
}

/* the player or the character stops talking: Relax unless it does something else */
static byte Sp_StopTalk(PyObject *e) {
	int talk = Script_HPrefix(e, "Action", "Talk");
	return talk >= 0 && (!talk || (Script_HSet(e, "ForceAction", PyString_FromString("Relax")) &&
		Script_HSet(e, "ForceDefLoop", PyString_FromString("Relax"))));
}

/* AbortSpeech(UsrName="", CharName="", extrangextra=""): the speech ends, both go back to what they did */
static PyObject *Speech_AbortSpeech(PyObject *self, PyObject *args) {
	PyObject *a = NULL, *b = NULL, *c = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|OOO", &a, &b, &c)) {
		return NULL;
	}
	PyObject *me = Sp_Entity("PlayerName"), *ch = me ? Sp_Entity("NPCName") : NULL;
	PyObject *empty = ch ? PyString_FromString("") : NULL, *zero = empty ? PyInt_FromLong(0) : NULL;
	byte ok = zero != NULL;
	if (ok) {
		Script_SetVar("Speech", "TextColor", Sp_Get("DefTextColor"));
		ok = Sp_SpeechText(empty, zero, "TextColor") && Script_Try("SScorer", "SetSpeechCallback", "(s)", "");
	}
	Py_XDECREF(empty);
	Py_XDECREF(zero);
	if (ok && Sp_True(me)) {
		ok = Script_HSet(me, "AutoSelect", PyInt_FromLong(1)) && Script_HSet(me, "SelectedEntity", PyString_FromString(""))
			&& Sp_StopTalk(me) && Script_HSet(me, "OutSelectFunc", PyString_FromString(""));
	}
	if (ok && Sp_True(ch)) {
		int walk = Script_HIs(ch, "Type", "WalkChar");
		ok = walk >= 0;
		if (walk > 0) {
			ok = Script_HSet(ch, "ArrowCanMission", Sp_Get("ArrowCanMission")) &&
				Script_HSet(ch, "SelectedEntity", PyString_FromString("")) && Sp_StopTalk(ch);
			/* the logic of the character back when talking was its only option */
			PyObject *n = ok ? PyObject_GetAttrString(ch, "ArrnOpt") : NULL;
			int one = n ? Script_NumCmp(n, 1) : -2;
			Py_XDECREF(n);
			ok = one != -2;
			if (ok && one == 0) {
				int talk = Script_HPrefix(ch, "ArrOpt0", "Talk:");
				ok = talk >= 0;
				if (talk > 0) {
					PyObject *name = PyObject_GetAttrString(ch, "Name");
					PyObject *ra = name ? Py_BuildValue("(O)", name) : NULL;
					PyObject *r = ra ? Speech_RestoreState(NULL, ra) : NULL;
					Py_XDECREF(name);
					Py_XDECREF(ra);
					ok = r && Script_HSet(ch, "AbortSelectFunc", PyString_FromString(""));
					Py_XDECREF(r);
				}
			}
		}
	}
	Py_XDECREF(me);
	Py_XDECREF(ch);
	return ok ? Speech_CallEndOfSpeech(NULL, NULL) : NULL;
}

/* NormalAbortSpeech(): the end of the answer; the camera turns from the character once the player listened */
static PyObject *Speech_NormalAbortSpeech(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *me = Sp_Entity("PlayerName");
	if (!me) {
		return NULL;
	}
	int listen = Sp_True(me) ? Script_HIs(me, "Action", "TalkListen") : 0;
	byte ok = listen >= 0;
	if (listen > 0) {
		PyObject *ch = Sp_Entity("NPCName"), *cam = ch ? Sp_Cam() : NULL;
		ok = cam && Script_HSet(cam, "VSpeed", PyFloat_FromDouble(1.1)) && Script_HSet(cam, "HSpeed", PyFloat_FromDouble(1.1));
		if (ok && Sp_True(ch)) {
			PyObject *vang = Sp_RotAng(me, ch);
			ok = vang && Script_HSet(me, "ViewAng", Sp_Turned(vang, 25)) && Script_HSet(me, "DisplAng", PyInt_FromLong(25));
			Py_XDECREF(vang);
		}
		Py_XDECREF(ch);
		Py_XDECREF(cam);
	}
	Py_DECREF(me);
	return ok ? Speech_AbortSpeech(NULL, PyTuple_New(0)) : NULL;
}

/* AbortNPCSpeech(UsrName="", CharName=""): prints the names */
static PyObject *Speech_AbortNPCSpeech(PyObject *self, PyObject *args) {
	PyObject *a = NULL, *b = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|OO", &a, &b)) {
		return NULL;
	}
	PyObject *out = PySys_GetObject("stdout");
	PyObject *sa = a ? PyObject_Str(a) : PyString_FromString(""), *sb = b ? PyObject_Str(b) : PyString_FromString("");
	byte ok = out && sa && sb && PyFile_WriteObject(sa, out, Py_PRINT_RAW) == 0 && PyFile_WriteString(" ", out) == 0 &&
		PyFile_WriteObject(sb, out, Py_PRINT_RAW) == 0 && PyFile_WriteString("\n", out) == 0;
	Py_XDECREF(sa);
	Py_XDECREF(sb);
	return ok ? Py_ReturnNone() : NULL;
}

/* ---- the answer ---- */

/* PerformAnswer(SpeechText, Time=6): the character answers with its voice */
static PyObject *Speech_PerformAnswer(PyObject *self, PyObject *args) {
	PyObject *text, *time = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &text, &time)) {
		return NULL;
	}
	PyObject *six = time ? NULL : PyInt_FromLong(6);
	PyObject *me = Sp_Entity("PlayerName"), *ch = me ? Sp_Entity("NPCName") : NULL;
	int talk = ch ? Script_HPrefix(me, "Action", "Talk") : -1;
	byte ok = talk >= 0 && (!talk || (Script_HSet(me, "ForceAction", Sp_Get("TalkListen")) &&
		Script_HSet(me, "ForceDefLoop", PyString_FromString("Relax"))));
	PyObject *anima = ok ? Sp_Get("AnsAnima") : NULL;
	int answers = anima && Script_HSet(ch, "Action", (Py_INCREF(anima), anima)) ? Script_HIs(ch, "Action",
		PyString_Check(anima) ? PyString_AsString(anima) : "") : -1;
	PyObject *r = NULL;
	if (answers > 0) {
		PyObject *name = PyObject_GetAttrString(ch, "Name");
		PyObject *rest = name && Script_HSet(ch, "ForceDefLoop", (Py_INCREF(anima), anima)) &&
			Script_Try("SSound", "StopVoice", "(i)", 0) && Script_Try("SSound", "PlayVoice", "(iO)", 1, name) ?
			Script_Ask("SSound", "VoiceRemain", "(iO)", 1, time ? time : six) : NULL;
		if (rest && Script_Try("SScorer", "SetSpeechCallback", "(s)", "Speech.NormalAbortSpeech") &&
			Sp_SpeechText(text, rest, "TextColor")) {
			Script_SetVar("Speech", "AnsAnima", PyString_FromString("TalkNPC"));
			r = PyInt_FromLong(1);
		}
		Py_XDECREF(name);
		Py_XDECREF(rest);
	} else if (answers == 0) {
		PyObject *a = Speech_AbortSpeech(NULL, PyTuple_New(0));
		if (a) {
			Py_DECREF(a);
			Script_SetVar("Speech", "AnsAnima", PyString_FromString("TalkNPC"));
			r = PyInt_FromLong(0);
		}
	}
	Py_XDECREF(anima);
	Py_XDECREF(me);
	Py_XDECREF(ch);
	Py_XDECREF(six);
	return r;
}

/* StopMenuEvents(), RestartMenuEvents(): the menu keys while the answer starts */
static PyObject *Speech_StopMenuEvents(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	GVar_SetStr("EscapeEvent", "");
	GVar_SetStr("MainMissionEvent", "");
	return Py_ReturnNone();
}

static PyObject *Speech_RestartMenuEvents(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	GVar_SetStr("EscapeEvent", "Menu.Init");
	GVar_SetStr("MainMissionEvent", "MissionsFuncs.ShowMissionInfo");
	return Py_ReturnNone();
}

/* AutoAnswer(): the end of the player's line, the answer follows */
static PyObject *Speech_AutoAnswer(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *me = Sp_Entity("PlayerName");
	int talk = me ? Script_HPrefix(me, "Action", "Talk") : -1;
	byte ok = talk >= 0 && (!talk || Script_HSet(me, "ForceAction", PyString_FromString("TalkListenNPC")));
	Py_XDECREF(me);
	if (ok) {
		Speech_StopMenuEvents(NULL, NULL);
		PyObject *ans = Sp_Get("NPCAnswer"), *t = ans ? Sp_Get("NPCAnswerTime") : NULL;
		ok = t && Script_LaterIn(1.0, "Speech", "RestartMenuEvents", PyTuple_New(0), NULL) &&
			Script_LaterIn(0.45, "Speech", "PerformAnswer", Py_BuildValue("(OO)", ans, t), NULL);
		Py_XDECREF(ans);
		Py_XDECREF(t);
	}
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *Speech_RestoreMoveTime(PyObject *self, PyObject *args) {
	PyObject *me;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &me)) {
		return NULL;
	}
	return Script_HSet(me, "MoveTime", PyInt_FromLong(0)) ? Py_ReturnNone() : NULL;
}

/* StdAnsLst(lst): the answers of the player as the options of the character ("msg:text", the Opt text or the Msg) */
static PyObject *Speech_StdAnsLst(PyObject *self, PyObject *args) {
	PyObject *lst;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &lst)) {
		return NULL;
	}
	PyObject *ch = Sp_Entity("NPCName");
	byte ok = ch && Script_HSet(ch, "ArrnOpt", PyInt_FromLong(0));
	int n = ok ? PySequence_Length(lst) : 0;
	for (int i = 0; ok && i < n; i++) {
		PyObject *msg = PySequence_GetItem(lst, i);
		PyObject *key = Sp_Cat(msg, "Opt");
		PyObject *opt = key && PyString_Check(key) ? Sp_Lang(PyString_AsString(key)) : NULL;
		Py_XDECREF(key);
		if (opt && PyString_Check(opt) && !*PyString_AsString(opt)) {
			Py_DECREF(opt);
			key = Sp_Cat(msg, "Msg");
			opt = key && PyString_Check(key) ? Sp_Lang(PyString_AsString(key)) : NULL;
			Py_XDECREF(key);
		}
		PyObject *head = opt ? Sp_Cat(msg, ":") : NULL;
		PyObject *v = head ? PySequence_Concat(head, opt) : NULL;
		char var[0x20];
		Str_Printf(var, sizeof(var), "ArrOpt%d", i);
		ok = v && Script_HSet(ch, var, v);
		Py_XDECREF(head);
		Py_XDECREF(opt);
		Py_XDECREF(msg);
	}
	Py_XDECREF(ch);
	return ok ? Py_ReturnNone() : NULL;
}

/* ---- a speech ---- */

/* CalculateSpeechTime(cad): 0.3 s a word and 2 s */
static double Sp_SpeechTime(PyObject *text) {
	int words = 0;
	if (text && PyString_Check(text)) {
		byte in = 0;
		for (const char *p = PyString_AsString(text); *p; p++) {
			byte space = *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f';
			words += !space && !in;
			in = !space;
		}
	}
	return words * 0.3 + 2;
}

static PyObject *Speech_CalculateSpeechTime(PyObject *self, PyObject *args) {
	PyObject *text;
	(void)self;
	if (!PyArg_ParseTuple(args, "S", &text)) {
		return NULL;
	}
	return PyFloat_FromDouble(Sp_SpeechTime(text));
}

/* SetPlayerToTalk(me, ch): the player faces the character to talk (the camera turns unless a non-interactive talk
 * goes on), 1 when the player can talk */
static int Sp_SetPlayerToTalk(PyObject *me, PyObject *ch) {
	if (!Sp_True(me)) {
		return 0;
	}
	PyObject *sel = PyObject_GetAttrString(me, "AutoSelect");
	int can = sel ? Script_NumCmp(sel, 1) == 0 : -1;
	Py_XDECREF(sel);
	if (can > 0) {
		PyObject *life = PyObject_GetAttrString(ch, "Life");
		int c = life ? Script_NumCmp(life, 0) : -2;
		Py_XDECREF(life);
		can = c == -2 ? -1 : c > 0;
	}
	if (can <= 0) {
		return can;
	}
	PyObject *name = PyObject_GetAttrString(ch, "Name");
	byte ok = name && Script_HSet(me, "SelectedEntity", (Py_INCREF(name), name)) && Script_HSet(me, "Action", PyString_FromString("Talk")) &&
		Script_HSet(me, "DefLoop", PyString_FromString("Talk"));
	/* globals().get('TalkNI') == None or globals().get('CurrNI') == 0 */
	PyObject *mod = ok ? PyImport_ImportModule("Speech") : NULL;
	PyObject *dict = mod ? PyModule_GetDict(mod) : NULL;
	PyObject *ni = dict ? PyDict_GetItemString(dict, "TalkNI") : NULL, *cur = dict ? PyDict_GetItemString(dict, "CurrNI") : NULL;
	byte turn = dict && (!ni || ni == Py_None || (cur && Script_NumCmp(cur, 0) == 0));
	Py_XDECREF(mod);
	ok = ok && dict;
	if (ok && turn) {
		PyObject *cam = Sp_Cam();
		PyObject *fcam = cam ? Script_Method(cam, "IsType", "(s)", "FCam") : NULL;
		ok = fcam != NULL;
		if (ok && PyObject_IsTrue(fcam) > 0) {
			/* the script keeps the speeds in a local it never uses */
			PyObject *v = PyObject_GetAttrString(cam, "VSpeed"), *h = v ? PyObject_GetAttrString(cam, "HSpeed") : NULL;
			ok = h && Script_HSet(cam, "VSpeed", PyFloat_FromDouble(1.1)) && Script_HSet(cam, "HSpeed", PyFloat_FromDouble(1.1));
			Py_XDECREF(v);
			Py_XDECREF(h);
		}
		Py_XDECREF(fcam);
		Py_XDECREF(cam);
		PyObject *vang = ok ? Sp_RotAng(me, ch) : NULL;
		PyObject *y = vang ? PySequence_GetItem(vang, 1) : NULL;
		PyObject *pos = y ? PyObject_GetAttrString(me, "Pos") : NULL;
		ok = pos && Script_HSet(me, "DisplAng", PyInt_FromLong(45)) && Script_HSet(me, "ViewAng", Sp_Turned(vang, 45)) &&
			Script_HSet(me, "MoveAng", Py_BuildValue("(iOi)", 0, y, 0)) && Script_HSet(me, "MovePos", (Py_INCREF(pos), pos)) &&
			Script_HSet(me, "MoveTime", PyFloat_FromDouble(0.20)) && Script_HSet(me, "MoveViewAng", PyInt_FromLong(0)) &&
			Script_LaterIn(0.25, "Speech", "RestoreMoveTime", Py_BuildValue("(O)", me), NULL);
		Py_XDECREF(pos);
		Py_XDECREF(y);
		Py_XDECREF(vang);
	}
	Py_XDECREF(name);
	ok = ok && Script_HSet(me, "AutoSelect", PyInt_FromLong(0)) &&
		Script_HSet(me, "OutSelectFunc", PyString_FromString("Speech.AbortSpeech"));
	return ok ? 1 : -1;
}

static PyObject *Speech_SetPlayerToTalk(PyObject *self, PyObject *args) {
	PyObject *me, *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &me, &ch)) {
		return NULL;
	}
	int r = Sp_SetPlayerToTalk(me, ch);
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* OnSpeech(SpeechText, SpeechAnswer="", _AnsAnima_="TalkNPC"): the player says the text, the character listens and
 * answers when it ends (AutoAnswer); 1 when the speech starts */
static PyObject *Speech_OnSpeech(PyObject *self, PyObject *args) {
	PyObject *text, *ans = NULL, *anima = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|OO", &text, &ans, &anima)) {
		return NULL;
	}
	PyObject *noAns = ans ? NULL : PyString_FromString(""), *talk = anima ? NULL : PyString_FromString("TalkNPC");
	if (!ans) {
		ans = noAns;
	}
	if (!anima) {
		anima = talk;
	}
	double time = Sp_SpeechTime(text), ansTime = Sp_SpeechTime(ans);
	Py_INCREF(anima);
	Script_SetVar("Speech", "AnsAnima", anima);
	PyObject *me = Sp_Entity("PlayerName"), *ch = me ? Sp_Entity("NPCName") : NULL;
	int r = ch ? Sp_SetPlayerToTalk(me, ch) : -1;
	if (r > 0) {
		PyObject *name = PyObject_GetAttrString(me, "Name");
		PyObject *ca = name ? PyObject_GetAttrString(ch, "Ang") : NULL, *ma = ca ? PyObject_GetAttrString(me, "Ang") : NULL;
		PyObject *cy = ma ? PySequence_GetItem(ca, 1) : NULL, *my = cy ? PySequence_GetItem(ma, 1) : NULL;
		PyObject *norm = my ? Script_Ask("SVec", "NormAng", "(d)", PyFloat_AsDouble(cy) - PyFloat_AsDouble(my) - 180) : NULL;
		byte ok = norm && Script_HSet(ch, "AbortSelectFunc", PyString_FromString("Speech.RestoreState")) &&
			Script_HSet(ch, "LLogicIdle", PyInt_FromLong(1)) && Script_HSet(ch, "SelectedEntity", (Py_INCREF(name), name));
		if (ok) {
			double d = PyFloat_AsDouble(norm);
			ok = Script_HSet(ch, "Action", PyString_FromString((d < 0 ? -d : d) > 5 ? "FaceToTalk" : "TalkListenNPC")) &&
				Script_HSet(ch, "DefLoop", PyString_FromString("TalkListenNPC")) && Script_HSet(ch, "DisplAng", PyInt_FromLong(0)) &&
				Script_HSet(ch, "TimeToExtraRelax", PyInt_FromLong(240)) && Sp_PlayVoiceAfter(0, PyFloat_FromDouble(-0.15));
		}
		PyObject *t = ok ? PyFloat_FromDouble(time) : NULL;
		ok = t && Sp_SpeechText(text, t, "UsrTextColor") && Script_Try("SScorer", "SetSpeechCallback", "(s)", "Speech.AutoAnswer");
		Py_XDECREF(t);
		if (ok) {
			Py_INCREF(ans);
			Script_SetVar("Speech", "NPCAnswer", ans);
			Script_SetVar("Speech", "NPCAnswerTime", PyFloat_FromDouble(ansTime));
			PyObject *arrow = PyObject_GetAttrString(ch, "ArrowCanMission");
			ok = arrow != NULL;
			if (ok) {
				Script_SetVar("Speech", "ArrowCanMission", arrow);
				ok = Script_HSet(ch, "ArrowCanMission", PyInt_FromLong(0)) &&
					(!Script_GetInt("Speech", "LockAlarm", 0) || Script_Try("Scrap", "SetLockAlarm", "(i)", 1));
			}
		}
		Py_XDECREF(name);
		Py_XDECREF(ca);
		Py_XDECREF(ma);
		Py_XDECREF(cy);
		Py_XDECREF(my);
		Py_XDECREF(norm);
		if (!ok) {
			r = -1;
		}
	}
	Py_XDECREF(me);
	Py_XDECREF(ch);
	Py_XDECREF(noAns);
	Py_XDECREF(talk);
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* OnSpch(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC"): a speech by the keys of the language file, with the
 * voices of both */
static PyObject *Speech_OnSpch(PyObject *self, PyObject *args) {
	PyObject *text, *ans, *anima = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "SS|O", &text, &ans, &anima)) {
		return NULL;
	}
	PyObject *me = Sp_Lang(PyString_AsString(text)), *you = me ? Sp_Lang(PyString_AsString(ans)) : NULL;
	PyObject *a = you && Sp_OpenVoice(text, 0) && Sp_OpenVoice(ans, 1) ?
		(anima ? Py_BuildValue("(OOO)", me, you, anima) : Py_BuildValue("(OO)", me, you)) : NULL;
	PyObject *r = a ? Speech_OnSpeech(NULL, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(me);
	Py_XDECREF(you);
	return r;
}

/* OnChallengeSpch(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC", isMerc=0): the player's text as it is, the
 * challenge voice of the player */
static PyObject *Speech_OnChallengeSpch(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "LngSpeechText", "LngSpeechAnswer", "_AnsAnima_", "isMerc", NULL };
	PyObject *text, *ans, *anima = NULL, *merc = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OS|OO", names, &text, &ans, &anima, &merc)) {
		return NULL;
	}
	PyObject *you = Sp_Lang(PyString_AsString(ans));
	int isMerc = merc ? Script_NumCmp(merc, 1) == 0 : 0;
	PyObject *a = you && Sp_OpenVoiceStr(isMerc ? "ChallengeRaceStdOpt2Voice" : "ChallengeRaceStdOpt1Voice", 0) &&
		Sp_OpenVoice(ans, 1) ? (anima ? Py_BuildValue("(OOO)", text, you, anima) : Py_BuildValue("(OO)", text, you)) : NULL;
	PyObject *r = a ? Speech_OnSpeech(NULL, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(you);
	return r;
}

/* StdOnSpch(optId, _AnsAnima_="TalkNPC"): the speech of an option, the arrow on the next option */
static PyObject *Speech_StdOnSpch(PyObject *self, PyObject *args) {
	PyObject *opt, *anima = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &opt, &anima)) {
		return NULL;
	}
	PyObject *msg = Sp_Cat(opt, "Msg"), *ans = msg ? Sp_Cat(opt, "Ans") : NULL;
	PyObject *a = ans ? (anima ? Py_BuildValue("(OOO)", msg, ans, anima) : Py_BuildValue("(OO)", msg, ans)) : NULL;
	PyObject *r = a ? Speech_OnSpch(NULL, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(msg);
	Py_XDECREF(ans);
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	PyObject *ch = Sp_Entity("NPCName");
	PyObject *i = ch ? PyObject_GetAttrString(ch, "ArriOpt") : NULL, *n = i ? PyObject_GetAttrString(ch, "ArrnOpt") : NULL;
	PyObject *one = n ? PyInt_FromLong(1) : NULL, *next = one ? PyNumber_Add(i, one) : NULL;
	PyObject *mod = next ? PyNumber_Remainder(next, n) : NULL;
	byte ok = mod && Script_HSet(ch, "ArriOpt", mod);
	Py_XDECREF(ch);
	Py_XDECREF(i);
	Py_XDECREF(n);
	Py_XDECREF(one);
	Py_XDECREF(next);
	return ok ? Py_ReturnNone() : NULL;
}

/* ---- the player answers ---- */

/* PlayerAnsEnd(): the end of the player's answer, its callback once */
static PyObject *Speech_PlayerAnsEnd(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Script_Try("SSound", "StopVoice", "(i)", 0)) {
		return NULL;
	}
	PyObject *cb = Sp_Get("PlayerAnsCallback");
	if (!cb) {
		PyErr_Clear();
		PyErr_SetString(PyExc_NameError, "PlayerAnsCallback");
		return NULL;
	}
	PyObject *a = PyTuple_New(0);
	PyObject *r = a ? PyEval_CallObject(cb, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(cb);
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	PyObject *mod = PyImport_ImportModule("Speech");
	int del = mod ? PyDict_DelItemString(PyModule_GetDict(mod), "PlayerAnsCallback") : -1;
	Py_XDECREF(mod);
	if (del < 0) {
		PyErr_SetString(PyExc_NameError, "PlayerAnsCallback");
		return NULL;
	}
	return Py_ReturnNone();
}

/* PlayerAns(me, ch, LangText, callbakck): the player says a text to the character, then the callback; 1 when the
 * player can talk */
static PyObject *Speech_PlayerAns(PyObject *self, PyObject *args) {
	PyObject *me, *ch, *key, *cb;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOSO", &me, &ch, &key, &cb)) {
		return NULL;
	}
	int r = Sp_SetPlayerToTalk(me, ch);
	if (r > 0) {
		PyObject *text = Sp_Lang(PyString_AsString(key));
		PyObject *t = text ? PyFloat_FromDouble(Sp_SpeechTime(text)) : NULL;
		byte ok = t && Sp_SpeechText(text, t, "UsrTextColor");
		if (ok) {
			Py_INCREF(cb);
			Script_SetVar("Speech", "PlayerAnsCallback", cb);
			ok = Script_Try("SScorer", "SetSpeechCallback", "(s)", "Speech.PlayerAnsEnd") && Sp_OpenVoice(key, 0) &&
				Sp_PlayVoiceAfter(0, PyInt_FromLong(0));
		}
		Py_XDECREF(text);
		Py_XDECREF(t);
		if (!ok) {
			r = -1;
		}
	}
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* ---- the standard speech routines ---- */

/* SScorer.SetMsgText(Scrap.GetLangStr(key), Scrap.GetTime() + 2) */
static byte Sp_Msg(const char *key) {
	PyObject *text = Sp_Lang(key);
	byte ok = text && Script_Try("SScorer", "SetMsgText", "(Od)", text, (double)Timer_GetTime(&g_pWorld->timer) + 2);
	Py_XDECREF(text);
	return ok;
}

/* InspectChar(UsrName, CharName, optId, FullOption): why the character does not talk */
static PyObject *Speech_InspectChar(PyObject *self, PyObject *args) {
	PyObject *usr, *chName, *opt, *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chName, &opt, &full)) {
		return NULL;
	}
	PyObject *me = Script_Ask("Scrap", "GetEntity", "(O)", usr), *ch = me ? Script_Ask("Scrap", "GetEntity", "(O)", chName) : NULL;
	PyObject *type = ch ? PyObject_GetAttrString(ch, "ActCtrl") : NULL, *state = type ? PyObject_GetAttrString(ch, "LLogicState") : NULL;
	const char *t = type && PyString_Check(type) ? PyString_AsString(type) : "";
	const char *key = NULL;
	char buf[0x80];
	int a;
	if (!state) {
		key = NULL;
	} else if ((a = Script_HPrefix(ch, "Action", "Flashed")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Flashed" : NULL;
	} else if ((a = Script_HPrefix(ch, "Action", "Dazed")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Dazed" : NULL;
	} else if ((a = Script_HPrefix(ch, "Action", "Crazy")) != 0) {
		key = a > 0 ? "INSPECT_CHAR_Crazy" : NULL;
	} else {
		PyObject *alarm = Script_Ask("Scrap", "GetAlarm", "()");
		PyObject *on = alarm ? PySequence_GetItem(alarm, 0) : NULL;
		Py_XDECREF(alarm);
		if (on && PyObject_IsTrue(on) > 0) {
			key = "INSPECT_CHAR_Alarm";
		} else if (on && !strcmp(t, "Sentinel")) {
			PyObject *mine = PyObject_GetAttrString(me, "ActCtrl"), *def = mine ? Script_Ask("Scrap", "Get", "(s)", "UsrDefChar") : NULL;
			key = def ? PyObject_Compare(mine, def) == 0 ? "INSPECT_CHAR_Sentinel_Act" : "INSPECT_CHAR_Sentinel" : NULL;
			Py_XDECREF(mine);
			Py_XDECREF(def);
		} else if (on) {
			int same = Script_HIs(me, "ActCtrl", t);
			if (same < 0) {
				key = NULL;
			} else if (same && (!strcmp(t, "Mayor") || !strcmp(t, "PoliceBoss") || !strcmp(t, "Betty") || !strcmp(t, "Berto"))) {
				key = "INSPECT_CHAR_Unique";
			} else if (!strcmp(t, "Nurse") && Script_NumCmp(state, 3) == 0) {
				key = "INSPECT_CHAR_Nurse";
			} else if (!strcmp(t, "Gear") && Script_NumCmp(state, 2) == 0) {
				key = "INSPECT_CHAR_Gear";
			} else if (Script_NumCmp(state, 4) == 0 || Script_NumCmp(state, 5) == 0) {
				Str_Printf(buf, sizeof(buf), "INSPECT_CHAR_%s", t);
				key = buf;
			} else {
				key = "INSPECT_CHAR_CantTalk";
			}
		}
		Py_XDECREF(on);
	}
	byte ok = key && !PyErr_Occurred() && Sp_Msg(key);
	Py_XDECREF(me);
	Py_XDECREF(ch);
	Py_XDECREF(type);
	Py_XDECREF(state);
	return ok ? Py_ReturnNone() : NULL;
}

/* AutoSpeech(UsrName, CharName, optId, FullOption): the speech routine of the character's class, else a standard
 * answer */
static PyObject *Speech_AutoSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *chName, *opt, *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chName, &opt, &full)) {
		return NULL;
	}
	Py_INCREF(usr);
	Script_SetVar("Speech", "PlayerName", usr);
	Py_INCREF(chName);
	Script_SetVar("Speech", "NPCName", chName);
	PyObject *ch = Sp_Entity("NPCName");
	PyObject *type = ch ? PyObject_GetAttrString(ch, "ActCtrl") : NULL;
	PyObject *routines = type ? Sp_Get("SpeechRoutines") : NULL;
	PyObject *f = routines ? PyObject_GetItem(routines, type) : NULL;
	PyObject *r = NULL;
	if (f) {
		r = PyEval_CallObject(f, args);
	} else if (routines && PyErr_ExceptionMatches(PyExc_KeyError)) {
		PyErr_Clear();
		char ans[0x20];
		Str_Printf(ans, sizeof(ans), "StdAnsw%d", (int)Scrap_RandRange(0, 2.9));
		PyObject *a = Py_BuildValue("(ss)", "TalkMsg", ans);
		r = a ? Speech_OnSpch(NULL, a) : NULL;
		Py_XDECREF(a);
		if (r) {
			Py_DECREF(r);
			r = Py_ReturnNone();
		}
	}
	Py_XDECREF(f);
	Py_XDECREF(routines);
	Py_XDECREF(type);
	Py_XDECREF(ch);
	if (r) {
		Py_DECREF(r);
		r = Py_ReturnNone();
	}
	return r;
}

/* ---- part 2: the non-interactive talks ---- */

/* del <module variable> of a script: 0 with a NameError when it is not there */
static byte Sp_Del(const char *var) {
	PyObject *mod = PyImport_ImportModule("Speech");
	int r = mod ? PyDict_DelItemString(PyModule_GetDict(mod), (char *)var) : -1;
	Py_XDECREF(mod);
	if (r < 0) {
		PyErr_Clear();
		PyErr_SetString(PyExc_NameError, var);
	}
	return r == 0;
}

/* Scrap.UsrEntity(0): the handle or None (a new reference) */
static PyObject *Sp_Usr(void) {
	return Script_Ask("Scrap", "UsrEntity", "(i)", 0);
}

/* NISpeech(headName=""): a non-interactive talk starts, the end of the speech waits until it is over */
static PyObject *Speech_NISpeech(PyObject *self, PyObject *args) {
	PyObject *head = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &head)) {
		return NULL;
	}
	PyObject *end = Sp_Get("OnEndOfSpeech"), *params = end ? Sp_Get("OnEndOfSpeechParams") : NULL;
	if (!params) {
		Py_XDECREF(end);
		return NULL;
	}
	Script_SetVar("Speech", "TalkNI", PyList_New(0));
	Script_SetVar("Speech", "OnEndOfSpeechNI", end);
	Script_SetVar("Speech", "OnEndOfSpeechParamsNI", params);
	Script_SetVar("Speech", "NISpeechHeadName", head ? (Py_INCREF(head), head) : PyString_FromString(""));
	return Py_ReturnNone();
}

/* AddNI(LngSpeechText, LngSpeechAnswer, _AnsAnima_="TalkNPC", HeadName=None): a line of the talk (the head of the
 * talk unless one is given) */
static PyObject *Speech_AddNI(PyObject *self, PyObject *args) {
	PyObject *text, *ans, *anima = NULL, *head = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|OO", &text, &ans, &anima, &head)) {
		return NULL;
	}
	PyObject *h = head && PyObject_IsTrue(head) > 0 ? (Py_INCREF(head), head) : Sp_Get("NISpeechHeadName");
	PyObject *list = h ? Sp_Get("TalkNI") : NULL;
	PyObject *line = list ? Py_BuildValue("(OOOO)", text, ans, anima ? anima : PyString_FromString("TalkNPC"), h) : NULL;
	if (!anima && line) {
		/* the default anima string built above is held by the tuple only */
		PyObject *a = PyTuple_GetItem(line, 2);
		Py_DECREF(a);
	}
	if (!h && !PyErr_Occurred()) {
		PyErr_SetString(PyExc_NameError, "NISpeechHeadName");
	}
	byte ok = line && PyList_Append(list, line) == 0;
	Py_XDECREF(line);
	Py_XDECREF(list);
	Py_XDECREF(h);
	return ok ? Py_ReturnNone() : NULL;
}

/* AutoAddNI(TextBase, _AnsAnima_="TalkNPC", HeadName=None): the lines Msg/Ans of the language entries of a base */
static PyObject *Speech_AutoAddNI(PyObject *self, PyObject *args) {
	PyObject *base, *anima = NULL, *head = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|OO", &base, &anima, &head)) {
		return NULL;
	}
	PyObject *keys = Script_Ask("Scrap", "GetLangEntries", "(Oi)", base, 2);
	int n = keys ? PySequence_Length(keys) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		PyObject *txt = PySequence_GetItem(keys, i);
		PyObject *msg = Sp_Cat(txt, "Msg"), *ans = msg ? Sp_Cat(txt, "Ans") : NULL;
		PyObject *a = ans ? Py_BuildValue("(OOOO)", msg, ans, anima ? anima : Py_None, head ? head : Py_None) : NULL;
		if (a && !anima) {
			/* AddNI's default for the anima */
			PyObject *t = PyTuple_GetSlice(a, 0, 2);
			Py_DECREF(a);
			a = t;
		}
		PyObject *r = a ? Speech_AddNI(NULL, a) : NULL;
		ok = r != NULL;
		Py_XDECREF(r);
		Py_XDECREF(a);
		Py_XDECREF(msg);
		Py_XDECREF(ans);
		Py_XDECREF(txt);
	}
	Py_XDECREF(keys);
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *Speech_OnObjSpch(PyObject *self, PyObject *args);

/* RunNI(i=0): line i of the talk; the next one when it ends, after the last the end of the talk */
static PyObject *Speech_RunNI(PyObject *self, PyObject *args) {
	PyObject *iv = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &iv)) {
		return NULL;
	}
	PyObject *zero = iv ? NULL : PyInt_FromLong(0);
	if (!iv) {
		iv = zero;
	}
	PyObject *talk = Sp_Get("TalkNI");
	int n = talk ? PySequence_Length(talk) : -1;
	long i = PyInt_Check(iv) ? PyInt_AsLong(iv) : 0;
	byte ok = n >= 0, isWch = 0;
	int last = ok ? !(i < n - 1) : 0;
	PyObject *line = ok ? PyObject_GetItem(talk, iv) : NULL, *ch = NULL;
	if (line) {
		Py_INCREF(iv);
		Script_SetVar("Speech", "CurrNI", iv);
		Script_SetInt("Speech", "LockAlarm", 1);
		if (!last) {
			Script_SetVar("Speech", "TalkListen", PyString_FromString("TalkListenNPC"));
		}
		ch = Sp_Entity("NPCName");
		PyObject *w = ch ? Script_Method(ch, "IsType", "(s)", "WalkChar") : NULL;
		ok = w != NULL;
		isWch = w && PyObject_IsTrue(w) > 0;
		Py_XDECREF(w);
		if (ok && isWch) {
			PyObject *a = PySequence_GetSlice(line, 0, 3);
			PyObject *r = a ? Speech_OnSpch(NULL, a) : NULL;
			ok = r && Script_HSet(ch, "Invulnerable", PyInt_FromLong(1));
			Py_XDECREF(r);
			Py_XDECREF(a);
		} else if (ok) {
			PyObject *me = Sp_Entity("PlayerName");
			PyObject *head = PySequence_GetItem(line, 3), *t = head ? PySequence_GetItem(line, 0) : NULL;
			PyObject *ans = t ? PySequence_GetItem(line, 1) : NULL;
			PyObject *a = ans && me && Script_HSet(me, "SelectedEntity", Sp_Get("NPCName")) ?
				Py_BuildValue("(OOOO)", ch, head, t, ans) : NULL;
			PyObject *r = a ? Speech_OnObjSpch(NULL, a) : NULL;
			PyObject *cur = r ? Sp_Get("NISpeechHeadName") : NULL;
			ok = cur != NULL;
			if (ok && PyObject_Compare(head, cur) != 0) {
				ok = Script_HSet(ch, "HeadModel", PyString_FromString(""));
				Py_INCREF(head);
				Script_SetVar("Speech", "NISpeechHeadName", head);
			}
			Py_XDECREF(cur);
			Py_XDECREF(r);
			Py_XDECREF(a);
			Py_XDECREF(head);
			Py_XDECREF(t);
			Py_XDECREF(ans);
			Py_XDECREF(me);
		}
	}
	if (ok && line && !last) {
		Script_SetVar("Speech", "OnEndOfSpeech", Sp_Get("RunNI"));
		Script_SetVar("Speech", "OnEndOfSpeechParams", Py_BuildValue("(l)", i + 1));
	} else if (ok && line) {
		ok = Sp_Del("CurrNI") && Sp_Del("TalkNI") && Sp_Del("NISpeechHeadName");
		if (ok && isWch) {
			Script_SetVar("Speech", "OnEndOfSpeech", Sp_Get("OnEndOfSpeechNI"));
			Script_SetVar("Speech", "OnEndOfSpeechParams", Sp_Get("OnEndOfSpeechParamsNI"));
			ok = Sp_Del("OnEndOfSpeechNI") && Sp_Del("OnEndOfSpeechParamsNI");
			PyObject *type = ok ? PyObject_GetAttrString(ch, "ActCtrl") : NULL;
			const char *t = type && PyString_Check(type) ? PyString_AsString(type) : "";
			ok = type && (!strcmp(t, "Human") || !strcmp(t, "GateKeeper") || !strcmp(t, "Sebastian") || !strcmp(t, "Sputnik") ||
				Script_HSet(ch, "Invulnerable", PyInt_FromLong(0)));
			Py_XDECREF(type);
		} else if (ok) {
			Script_SetVar("Speech", "OnEndOfSpeech", Sp_Get("ObjNIEnd"));
			Script_SetVar("Speech", "OnEndOfSpeechParams", PyTuple_New(0));
		}
	}
	Py_XDECREF(ch);
	Py_XDECREF(line);
	Py_XDECREF(talk);
	Py_XDECREF(zero);
	return ok && line ? Py_ReturnNone() : NULL;
}

/* ObjNIEnd(): the end of a talk with an object: its head goes, then the end of the talk */
static PyObject *Speech_ObjNIEnd(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *end = Sp_Get("OnEndOfSpeechNI"), *params = end ? Sp_Get("OnEndOfSpeechParamsNI") : NULL;
	PyObject *ch = params && Sp_Del("OnEndOfSpeechNI") && Sp_Del("OnEndOfSpeechParamsNI") ? Sp_Entity("NPCName") : NULL;
	PyObject *ch2 = ch && Script_HSet(ch, "HeadModel", PyString_FromString("")) ? Sp_Entity("NPCName") : NULL;
	PyObject *r = ch2 && Script_HSet(ch2, "Action", PyString_FromString("Relax")) ? PyEval_CallObject(end, params) : NULL;
	Py_XDECREF(end);
	Py_XDECREF(params);
	Py_XDECREF(ch);
	Py_XDECREF(ch2);
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	return Py_ReturnNone();
}

/* ---- remote communications ---- */

/* RestoreSpeech(): the end of a remote communication */
static PyObject *Speech_RestoreSpeech(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *e = Sp_Usr();
	byte ok = e != NULL;
	if (ok && PyObject_IsTrue(e) > 0) {
		PyObject *w = Script_Method(e, "IsType", "(s)", "WalkChar");
		ok = w && (PyObject_IsTrue(w) <= 0 || Script_HSet(e, "AutoSelect", PyInt_FromLong(1)));
		Py_XDECREF(w);
	}
	Py_XDECREF(e);
	return ok ? Speech_CallEndOfSpeech(NULL, NULL) : NULL;
}

/* DoRemote(): 1 when a remote communication may start now (no talk, no system message, a player who can listen) */
static int Sp_DoRemote(void) {
	PyObject *init = PyImport_ImportModule("Init");
	PyObject *e = init ? Sp_Usr() : NULL;
	Py_XDECREF(init);
	if (!e) {
		return -1;
	}
	double now = (double)Timer_GetTime(&g_pWorld->timer);
	int doit = -1;
	PyObject *rest = NULL, *msg = NULL, *set = NULL;
	if (now < 0.2) {
		doit = 0;
	} else if (!(rest = Script_Ask("Scrap", "Get", "(s)", "RemainTalkTime"))) {
	} else if (Script_NumCmp(rest, 0) > 0 || !Script_GetInt("Speech", "CanLaunchRemote", 0)) {
		doit = 0;
	} else if (!(msg = Script_Ask("Scrap", "Get", "(s)", "TimeToMsg"))) {
	} else if (PyFloat_AsDouble(msg) > now) {
		doit = 0;
	} else if (!(set = Script_Ask("SInput", "GetActionSet", "()"))) {
	} else if ((PyString_Check(set) && !strcmp(PyString_AsString(set), "Inactive")) || e == Py_None) {
		doit = 0;
		if (Script_GetInt("Init", "HasSuperDeal", 0) && e == Py_None) {
			PyObject *cam = Sp_Cam();
			if (cam && PyObject_IsTrue(cam) > 0 && Script_HIs(cam, "Type", "FCam") > 0) {
				int target = Script_HIs(cam, "Target", ""), link = target == 0 ? Script_HIs(cam, "EntityLink", "") : 0;
				doit = target == 0 && link > 0;
			}
			Py_XDECREF(cam);
			if (PyErr_Occurred()) {
				doit = -1;
			}
		}
	} else {
		PyObject *w = Script_Method(e, "IsType", "(s)", "WalkChar");
		if (w && PyObject_IsTrue(w) > 0) {
			PyObject *sel = PyObject_GetAttrString(e, "AutoSelect");
			doit = sel ? Script_NumCmp(sel, 1) == 0 : -1;
			Py_XDECREF(sel);
		} else if (w) {
			doit = 1;
			Script_SetInt("Speech", "CanLaunchRemote", 0);
		}
		Py_XDECREF(w);
	}
	Py_XDECREF(rest);
	Py_XDECREF(msg);
	Py_XDECREF(set);
	Py_DECREF(e);
	return doit;
}

static PyObject *Speech_DoRemote(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	int r = Sp_DoRemote();
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* ForceRemoteComunication(SpeechText, HeadName, HeadIndex=1, AfterRemoteComFunc=None, Parameters=()): the text with
 * the head of the speaker on the monitor, the function after it */
static byte Sp_ForceRemoteComunication(PyObject *text, PyObject *head, PyObject *index, PyObject *after, PyObject *params) {
	PyObject *t = PyFloat_FromDouble(Sp_SpeechTime(text));
	byte ok = t && Sp_SpeechText(text, t, "DefTextColor");
	Py_XDECREF(t);
	PyObject *e = ok ? Sp_Usr() : NULL;
	PyObject *w = e && PyObject_IsTrue(e) > 0 ? Script_Method(e, "IsType", "(s)", "WalkChar") : NULL;
	ok = e && (PyObject_IsTrue(e) <= 0 || w);
	if (ok && w && PyObject_IsTrue(w) > 0) {
		ok = Script_HSet(e, "AutoSelect", PyInt_FromLong(0)) && Script_HSet(e, "SelectedEntity", PyString_FromString("")) &&
			Script_HSet(e, "OutSelectFunc", PyString_FromString(""));
	}
	Py_XDECREF(w);
	Py_XDECREF(e);
	if (ok && !(PyString_Check(head) && !*PyString_AsString(head))) {
		PyObject *key = PyString_FromString("CharName_");
		PyObject *full = key ? PySequence_Concat(key, head) : NULL;
		PyObject *txt = full && PyString_Check(full) ? Sp_Lang(PyString_AsString(full)) : NULL;
		PyObject *dict = txt ? Sp_Get("HeadDict") : NULL;
		PyObject *model = dict ? PyObject_GetItem(dict, head) : NULL;
		if (!model && dict && PyErr_ExceptionMatches(PyExc_KeyError)) {
			PyErr_Clear();
			Py_INCREF(head);
			model = head;
		}
		ok = model && Script_Try("SScorer", "SetHeadMonitor", "(OOO)", model, txt, index);
		Py_XDECREF(key);
		Py_XDECREF(full);
		Py_XDECREF(txt);
		Py_XDECREF(dict);
		Py_XDECREF(model);
	}
	if (ok) {
		Py_INCREF(after);
		Script_SetVar("Speech", "OnEndOfSpeech", after);
		Py_INCREF(params);
		Script_SetVar("Speech", "OnEndOfSpeechParams", params);
		ok = Script_Try("SScorer", "SetSpeechCallback", "(s)", "Speech.RestoreSpeech");
	}
	return ok;
}

/* the arguments (SpeechText, HeadName, HeadIndex=1, AfterRemoteComFunc=None, Parameters=()) */
static byte Sp_RemoteArgs(PyObject *args, PyObject **text, PyObject **head, PyObject **index, PyObject **after,
	PyObject **params, PyObject **own) {
	*index = *after = *params = NULL;
	if (!PyArg_ParseTuple(args, "OO|OOO", text, head, index, after, params)) {
		return 0;
	}
	own[0] = *index ? NULL : PyInt_FromLong(1);
	own[1] = *params ? NULL : PyTuple_New(0);
	if (!*index) {
		*index = own[0];
	}
	if (!*after) {
		*after = Py_None;
	}
	if (!*params) {
		*params = own[1];
	}
	return *index && *params;
}

static PyObject *Speech_ForceRemoteComunication(PyObject *self, PyObject *args) {
	PyObject *text, *head, *index, *after, *params, *own[2] = { NULL, NULL };
	(void)self;
	byte ok = Sp_RemoteArgs(args, &text, &head, &index, &after, &params, own) &&
		Sp_ForceRemoteComunication(text, head, index, after, params);
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return ok ? Py_ReturnNone() : NULL;
}

/* ForceRemoteComInternal(...): the voice of the text on channel 2 and the communication */
static byte Sp_ForceRemoteComInternal(PyObject *text, PyObject *head, PyObject *index, PyObject *after, PyObject *params) {
	PyObject *lang = PyString_Check(text) ? Sp_Lang(PyString_AsString(text)) : NULL;
	byte ok = lang && Sp_OpenVoice(text, 2) && Sp_PlayVoiceAfter(2, PyInt_FromLong(0)) &&
		Sp_ForceRemoteComunication(lang, head, index, after, params);
	if (!lang && !PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
	}
	Py_XDECREF(lang);
	return ok;
}

static PyObject *Speech_ForceRemoteComInternal(PyObject *self, PyObject *args) {
	PyObject *text, *head, *index, *after, *params, *own[2] = { NULL, NULL };
	(void)self;
	byte ok = Sp_RemoteArgs(args, &text, &head, &index, &after, &params, own) &&
		Sp_ForceRemoteComInternal(text, head, index, after, params);
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return ok ? Py_ReturnNone() : NULL;
}

/* RemoteComunication(...): the communication when one may start now (DoRemote); its result */
static int Sp_RemoteComunication(PyObject *text, PyObject *head, PyObject *index, PyObject *after, PyObject *params) {
	int doit = Sp_DoRemote();
	if (doit > 0 && !Sp_ForceRemoteComInternal(text, head, index, after, params)) {
		return -1;
	}
	return doit;
}

static PyObject *Speech_RemoteComunication(PyObject *self, PyObject *args) {
	PyObject *text, *head, *index, *after, *params, *own[2] = { NULL, NULL };
	(void)self;
	int r = Sp_RemoteArgs(args, &text, &head, &index, &after, &params, own) ?
		Sp_RemoteComunication(text, head, index, after, params) : -1;
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return r < 0 ? NULL : PyInt_FromLong(r);
}

/* OffVoice(SpeechText): a voice without a head when nobody talks */
static PyObject *Speech_OffVoice(PyObject *self, PyObject *args) {
	PyObject *text;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &text)) {
		return NULL;
	}
	PyObject *e = Sp_Usr(), *rest = e && PyObject_IsTrue(e) > 0 ? Script_Ask("Scrap", "Get", "(s)", "RemainTalkTime") : NULL;
	byte ok = e && (PyObject_IsTrue(e) <= 0 || rest);
	int go = ok && rest && Script_NumCmp(rest, 0) <= 0;
	if (go) {
		PyObject *w = Script_Method(e, "IsType", "(s)", "WalkChar");
		go = w ? PyObject_IsTrue(w) <= 0 : -1;
		if (go == 0) {
			PyObject *sel = PyObject_GetAttrString(e, "AutoSelect");
			go = sel ? Script_NumCmp(sel, 1) == 0 : -1;
			if (go > 0) {
				go = Script_HIs(e, "SelectedEntity", "");
			}
			Py_XDECREF(sel);
		}
		Py_XDECREF(w);
		ok = go >= 0;
	}
	if (go > 0) {
		PyObject *none = PyString_FromString(""), *zero = PyInt_FromLong(0), *params = PyTuple_New(0);
		ok = Sp_ForceRemoteComInternal(text, none, zero, Py_None, params);
		Py_XDECREF(none);
		Py_XDECREF(zero);
		Py_XDECREF(params);
	}
	Py_XDECREF(rest);
	Py_XDECREF(e);
	return ok ? Py_ReturnNone() : NULL;
}

/* LaunchTaunt(entityName, charType, numTaunt): the taunt of a character over the radio, not twice in a row */
static PyObject *Speech_LaunchTaunt(PyObject *self, PyObject *args) {
	PyObject *ent, *num;
	char *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "OsO", &ent, &type, &num)) {
		return NULL;
	}
	char taunt[0x80], head[0x40];
	Py_INCREF(num);
	if (strstr(type, "Mercenary")) {
		PyObject *r = PyObject_Repr(num);
		Str_Printf(taunt, sizeof(taunt), "Generics_MercsTaunt_%s", r ? PyString_AsString(r) : "");
		Py_XDECREF(r);
	} else {
		/* in the CA of the eighth super deal the police taunts 5 and 7 (to the group) are 4 and 6 */
		char map[0x40];
		const char *lib = SaveVar_Get("Mission.Library");
		Script_GetString("Init", "UpperMap", map, sizeof(map));
		if (!strcmp(map, "CA") && lib && !strcmp(lib, "SuperDeal_Eighth") &&
			(Script_NumCmp(num, 5) == 0 || Script_NumCmp(num, 7) == 0)) {
			PyObject *one = PyInt_FromLong(1), *less = PyNumber_Subtract(num, one);
			Py_XDECREF(one);
			Py_DECREF(num);
			num = less;
		}
		PyObject *r = num ? PyObject_Repr(num) : NULL;
		Str_Printf(taunt, sizeof(taunt), "Generics_%sTaunt_%s", type, r ? PyString_AsString(r) : "");
		Py_XDECREF(r);
	}
	Py_XDECREF(num);
	if (PyErr_Occurred()) {
		return NULL;
	}
	char prev[0x80];
	Script_GetString("Speech", "prevTaunt", prev, sizeof(prev));
	if (!strcmp(taunt, prev)) {
		Script_SetVar("Speech", "LaunchedTauntEntityName", PyString_FromString(""));
		return Py_ReturnNone();
	}
	Script_SetVar("Speech", "prevTaunt", PyString_FromString(taunt));
	const char *bertos = SaveVar_Get("CreateBertos");
	Str_Copy(head, !strcmp(type, "Berto") && bertos && !strcmp(bertos, "1") ? "EvilBerto" : type, sizeof(head));
	PyObject *t = PyString_FromString(taunt), *h = PyString_FromString(head), *one = PyInt_FromLong(1);
	PyObject *params = PyTuple_New(0);
	int r = params ? Sp_RemoteComunication(t, h, one, Py_None, params) : -1;
	Py_XDECREF(t);
	Py_XDECREF(h);
	Py_XDECREF(one);
	Py_XDECREF(params);
	if (r > 0) {
		Py_INCREF(ent);
		Script_SetVar("Speech", "LaunchedTauntEntityName", ent);
	}
	return r < 0 ? NULL : Py_ReturnNone();
}

/* CancelRemoteCom(): the current speech ends */
static PyObject *Speech_CancelRemoteCom(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Script_Try("SScorer", "CancelSpeech", "()") ? Py_ReturnNone() : NULL;
}

/* AddRemoteCom(SpeechText, HeadName, AfterRemoteComFunc=None, Parameters=(), HeadIndex=1, InsertFirst=0): a message
 * in the queue (not twice), sent 0.2 s later */
static byte Sp_AddRemoteCom(PyObject *text, PyObject *head, PyObject *after, PyObject *params, PyObject *index, int first) {
	PyObject *cur = Sp_Get("CurrentRemoteCom"), *list = cur ? Sp_Get("RemoteCom") : NULL;
	int n = list ? PySequence_Length(list) : -1;
	byte ok = n >= 0, dup = ok && PyObject_Compare(text, cur) == 0;
	for (int i = 0; ok && !dup && i < n; i++) {
		PyObject *item = PySequence_GetItem(list, i), *t = item ? PySequence_GetItem(item, 0) : NULL;
		ok = t != NULL;
		dup = t && PyObject_Compare(t, text) == 0;
		Py_XDECREF(item);
		Py_XDECREF(t);
	}
	if (ok && !dup) {
		PyObject *msg = Py_BuildValue("(OOOO)", text, head, after, params);
		ok = msg && (first ? PyList_Insert(list, 0, msg) : PyList_Append(list, msg)) == 0;
		Py_XDECREF(msg);
		if (ok && PySequence_Length(list) >= 1) {
			PyObject *top = PySequence_GetItem(list, 0), *t = top ? PySequence_GetItem(top, 0) : NULL;
			Py_XDECREF(top);
			ok = t && Script_Try("Scrap", "DeleteScheduledFuncs", "(s)", "ProcessRemoteComList");
			if (t) {
				Script_SetVar("Speech", "CurrentRemoteCom", t);
			}
			ok = ok && Script_LaterIn(0.2, "Speech", "ProcessRemoteCom", Py_BuildValue("(O)", index), "ProcessRemoteComList");
		}
	}
	Py_XDECREF(cur);
	Py_XDECREF(list);
	return ok;
}

static PyObject *Speech_AddRemoteCom(PyObject *self, PyObject *args) {
	PyObject *text, *head, *after = Py_None, *params = NULL, *index = NULL, *first = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|OOOO", &text, &head, &after, &params, &index, &first)) {
		return NULL;
	}
	PyObject *p = params ? (Py_INCREF(params), params) : PyTuple_New(0), *i = index ? (Py_INCREF(index), index) : PyInt_FromLong(1);
	byte ok = p && i && Sp_AddRemoteCom(text, head, after, p, i, first && PyObject_IsTrue(first) > 0);
	Py_XDECREF(p);
	Py_XDECREF(i);
	return ok ? Py_ReturnNone() : NULL;
}

/* ForceRemoteCom(...): the message first in the queue, the current speech ends */
static PyObject *Speech_ForceRemoteCom(PyObject *self, PyObject *args) {
	PyObject *text, *head, *index, *after, *params, *own[2] = { NULL, NULL };
	(void)self;
	byte ok = Sp_RemoteArgs(args, &text, &head, &index, &after, &params, own) &&
		Sp_AddRemoteCom(text, head, after, params, index, 1) && Script_Try("SScorer", "CancelSpeech", "()");
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return ok ? Py_ReturnNone() : NULL;
}

/* ProcessRemoteCom(HeadIndex): the first message of the queue when it can be sent, again 5 s later while some wait */
static PyObject *Speech_ProcessRemoteCom(PyObject *self, PyObject *args) {
	PyObject *index;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &index)) {
		return NULL;
	}
	PyObject *list = Sp_Get("RemoteCom");
	if (!list) {
		return NULL;
	}
	byte ok = 1;
	if (PyObject_IsTrue(list) > 0) {
		PyObject *msg = PySequence_GetItem(list, 0);
		PyObject *t = msg ? PySequence_GetItem(msg, 0) : NULL, *h = t ? PySequence_GetItem(msg, 1) : NULL;
		PyObject *f = h ? PySequence_GetItem(msg, 2) : NULL, *p = f ? PySequence_GetItem(msg, 3) : NULL;
		int sent = p ? Sp_RemoteComunication(t, h, index, f, p) : -1;
		ok = sent >= 0 && (!sent || PySequence_DelItem(list, 0) == 0);
		if (ok && PySequence_Length(list) > 0) {
			ok = Script_LaterIn(5.0, "Speech", "ProcessRemoteCom", Py_BuildValue("(O)", index), "ProcessRemoteComList");
		}
		Py_XDECREF(msg);
		Py_XDECREF(t);
		Py_XDECREF(h);
		Py_XDECREF(f);
		Py_XDECREF(p);
	}
	Py_DECREF(list);
	return ok ? Py_ReturnNone() : NULL;
}

/* ---- speeches with objects ---- */

/* the range test each second while OnOutOfRange is set */
static byte Sp_TestLater(void) {
	PyObject *f = Sp_Get("OnOutOfRange");
	byte ok = f && (PyObject_IsTrue(f) <= 0 || Script_LaterIn(1, "Speech", "TestInRange", PyTuple_New(0), "TestInRange"));
	Py_XDECREF(f);
	return ok;
}

/* TestInRange(): OnOutOfRange when the player walks away (5 m) or the object offers to talk again */
static PyObject *Speech_TestInRange(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *player = Sp_Usr(), *obj = player ? Sp_Get("ObjEntity") : NULL;
	byte ok = obj != NULL;
	if (ok && PyObject_IsTrue(player) > 0 && PyObject_IsTrue(obj) > 0) {
		PyObject *hook = Sp_Get("OnOutOfRange");
		int abort = hook ? PyObject_IsTrue(hook) > 0 : -1;
		if (abort > 0) {
			abort = Script_HPrefix(obj, "ArrOpt0", "Talk:");
		}
		if (abort > 0) {
			PyObject *sel = PyObject_GetAttrString(player, "AutoSelect");
			abort = sel ? PyObject_IsTrue(sel) > 0 : -1;
			Py_XDECREF(sel);
		}
		int far = 0;
		if (abort == 0) {
			PyObject *a = PyObject_GetAttrString(obj, "Pos"), *b = a ? PyObject_GetAttrString(player, "Pos") : NULL;
			PyObject *d = b ? Script_Ask("SVec", "Sub", "(OO)", a, b) : NULL, *m = d ? Script_Ask("SVec", "ModSqr", "(O)", d) : NULL;
			far = m ? Script_NumCmp(m, 250000) > 0 : -1;
			Py_XDECREF(a);
			Py_XDECREF(b);
			Py_XDECREF(d);
			Py_XDECREF(m);
		}
		ok = abort >= 0 && far >= 0;
		if (ok && (abort || far)) {
			if (PyObject_IsTrue(hook) > 0) {
				PyObject *a = PyTuple_New(0), *r = a ? PyEval_CallObject(hook, a) : NULL;
				Py_XDECREF(a);
				ok = r && Script_Try("Scrap", "Set", "(sd)", "RemainTalkTime", 0.0);
				Py_XDECREF(r);
				if (ok) {
					Py_INCREF(Py_None);
					Script_SetVar("Speech", "OnOutOfRange", Py_None);
				}
			}
		} else if (ok) {
			ok = Script_LaterIn(1, "Speech", "TestInRange", PyTuple_New(0), "TestInRange");
		}
		Py_XDECREF(hook);
	}
	Py_XDECREF(player);
	Py_XDECREF(obj);
	return ok ? Py_ReturnNone() : NULL;
}

/* StartObjectSpeech(obj, action, defLoop, text, charType, speechCallback): the object speaks over the radio */
static byte Sp_StartObjectSpeech(PyObject *obj, PyObject *action, PyObject *defLoop, PyObject *text, PyObject *type,
	PyObject *cb) {
	Py_INCREF(obj);
	Script_SetVar("Speech", "ObjEntity", obj);
	Py_INCREF(type);
	Script_SetVar("Speech", "ObjCharType", type);
	byte ok = (PyString_Check(action) && !*PyString_AsString(action)) || Script_HSet(obj, "Action", (Py_INCREF(action), action));
	ok = ok && ((PyString_Check(defLoop) && !*PyString_AsString(defLoop)) || Script_HSet(obj, "DefLoop", (Py_INCREF(defLoop), defLoop)));
	PyObject *end = ok ? Sp_Get("OnEndOfSpeech") : NULL, *params = end ? Sp_Get("OnEndOfSpeechParams") : NULL;
	PyObject *one = params ? PyInt_FromLong(1) : NULL;
	ok = one && Sp_ForceRemoteComunication(text, type, one, end, params) &&
		Script_Try("SScorer", "SetSpeechCallback", "(O)", cb) && Sp_TestLater();
	Py_XDECREF(end);
	Py_XDECREF(params);
	Py_XDECREF(one);
	return ok;
}

static PyObject *Speech_StartObjectSpeech(PyObject *self, PyObject *args) {
	PyObject *obj, *action, *defLoop, *text, *type, *cb;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOOO", &obj, &action, &defLoop, &text, &type, &cb)) {
		return NULL;
	}
	return Sp_StartObjectSpeech(obj, action, defLoop, text, type, cb) ? Py_ReturnNone() : NULL;
}

/* StartObjSpch(obj, text, charType, speechCallback="Speech.EndObjectAnswer"): with the voice of the text */
static PyObject *Speech_StartObjSpch(PyObject *self, PyObject *args) {
	PyObject *obj, *text, *type, *cb = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OSO|O", &obj, &text, &type, &cb)) {
		return NULL;
	}
	PyObject *def = cb ? NULL : PyString_FromString("Speech.EndObjectAnswer"), *empty = PyString_FromString("");
	PyObject *lang = empty && Sp_OpenVoice(text, 2) && Sp_PlayVoiceAfter(2, PyInt_FromLong(0)) ? Sp_Lang(PyString_AsString(text)) : NULL;
	byte ok = lang && Sp_StartObjectSpeech(obj, empty, empty, lang, type, cb ? cb : def);
	Py_XDECREF(def);
	Py_XDECREF(empty);
	Py_XDECREF(lang);
	return ok ? Py_ReturnNone() : NULL;
}

/* OnObjectSpeech(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk"): the player
 * talks to an object with the head of a character; it answers when the text ends (ObjectAutoAnswer) */
static PyObject *Sp_OnObjectSpeech(PyObject *obj, PyObject *type, PyObject *text, PyObject *ans, PyObject *action,
	PyObject *defLoop) {
	PyObject *player = Sp_Usr();
	if (!player) {
		return NULL;
	}
	if (PyObject_IsTrue(player) <= 0) {
		Py_DECREF(player);
		return Py_ReturnNone();
	}
	Py_INCREF(obj);
	Script_SetVar("Speech", "ObjEntity", obj);
	Py_INCREF(ans);
	Script_SetVar("Speech", "ObjAnswer", ans);
	Py_INCREF(type);
	Script_SetVar("Speech", "ObjCharType", type);
	PyObject *name = PyObject_GetAttrString(player, "Name");
	PyObject *me = name ? Script_Ask("Scrap", "GetEntity", "(O)", name) : NULL;
	Py_XDECREF(name);
	Py_DECREF(player);
	byte ok = me != NULL;
	if (ok && PyObject_IsTrue(me) > 0) {
		ok = Script_HSet(obj, "Action", PyString_FromString("Listen")) && Script_HSet(me, "Action", PyString_FromString("Talk")) &&
			Script_HSet(me, "DefLoop", PyString_FromString("Talk")) &&
			Script_HSet(me, "OutSelectFunc", PyString_FromString("Speech.AbortSpeech")) && Script_HSet(me, "AutoSelect", PyInt_FromLong(0));
	}
	Py_XDECREF(me);
	PyObject *t = ok ? PyFloat_FromDouble(Sp_SpeechTime(text)) : NULL;
	ok = t && Sp_SpeechText(text, t, "UsrTextColor");
	Py_XDECREF(t);
	PyObject *dict = ok ? Sp_Get("HeadDict") : NULL, *model = dict ? PyObject_GetItem(dict, type) : NULL;
	if (!model && dict && PyErr_ExceptionMatches(PyExc_KeyError)) {
		PyErr_Clear();
		Py_INCREF(type);
		model = type;
	}
	ok = model && Script_HSet(obj, "HeadModel", model) && Script_HSet(obj, "HeadAnimIndex", PyInt_FromLong(0));
	Py_XDECREF(dict);
	if (ok) {
		Py_INCREF(action);
		Script_SetVar("Speech", "ObjAutoAnswerAction", action);
		Py_INCREF(defLoop);
		Script_SetVar("Speech", "ObjAutoAnswerDefLoop", defLoop);
		ok = Script_Try("SScorer", "SetSpeechCallback", "(s)", "Speech.ObjectAutoAnswer") &&
			(!Script_GetInt("Speech", "LockAlarm", 0) || Script_Try("Scrap", "SetLockAlarm", "(i)", 1)) && Sp_TestLater();
	}
	return ok ? PyInt_FromLong(1) : NULL;
}

/* the optional (action="Talk", defLoop="Talk") of the object speeches */
static void Sp_TalkDefaults(PyObject **action, PyObject **defLoop, PyObject **own) {
	own[0] = *action ? NULL : PyString_FromString("Talk");
	own[1] = *defLoop ? NULL : PyString_FromString("Talk");
	if (!*action) {
		*action = own[0];
	}
	if (!*defLoop) {
		*defLoop = own[1];
	}
}

static PyObject *Speech_OnObjectSpeech(PyObject *self, PyObject *args) {
	PyObject *obj, *type, *text, *ans, *action = NULL, *defLoop = NULL, *own[2];
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO|OO", &obj, &type, &text, &ans, &action, &defLoop)) {
		return NULL;
	}
	Sp_TalkDefaults(&action, &defLoop, own);
	PyObject *r = action && defLoop ? Sp_OnObjectSpeech(obj, type, text, ans, action, defLoop) : NULL;
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return r;
}

/* OnObjSpch(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk"): by the keys of the
 * language file, with the voices */
static PyObject *Speech_OnObjSpch(PyObject *self, PyObject *args) {
	PyObject *obj, *type, *text, *ans, *action = NULL, *defLoop = NULL, *own[2];
	(void)self;
	if (!PyArg_ParseTuple(args, "OOSS|OO", &obj, &type, &text, &ans, &action, &defLoop)) {
		return NULL;
	}
	Sp_TalkDefaults(&action, &defLoop, own);
	PyObject *t = Sp_OpenVoice(text, 0) && Sp_OpenVoice(ans, 2) && Sp_PlayVoiceAfter(0, PyInt_FromLong(0)) ?
		Sp_Lang(PyString_AsString(text)) : NULL;
	PyObject *a = t ? Sp_Lang(PyString_AsString(ans)) : NULL;
	PyObject *r = a ? Sp_OnObjectSpeech(obj, type, t, a, action, defLoop) : NULL;
	Py_XDECREF(t);
	Py_XDECREF(a);
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	if (r) {
		Py_DECREF(r);
		r = Py_ReturnNone();
	}
	return r;
}

/* StdObjSpch(_ObjEntity, _ObjCharType, optId, action="Talk", defLoop="Talk") */
static PyObject *Speech_StdObjSpch(PyObject *self, PyObject *args) {
	PyObject *obj, *type, *opt, *action = NULL, *defLoop = NULL, *own[2];
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO|OO", &obj, &type, &opt, &action, &defLoop)) {
		return NULL;
	}
	Sp_TalkDefaults(&action, &defLoop, own);
	PyObject *msg = Sp_Cat(opt, "Msg"), *ans = msg ? Sp_Cat(opt, "Ans") : NULL;
	PyObject *a = ans ? Py_BuildValue("(OOOOOO)", obj, type, msg, ans, action, defLoop) : NULL;
	PyObject *r = a ? Speech_OnObjSpch(NULL, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(msg);
	Py_XDECREF(ans);
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	return r;
}

/* OnChallengeObjSpch(_ObjEntity, _ObjCharType, SpeechText, SpeechAnswer, action="Talk", defLoop="Talk", isMerc=0) */
static PyObject *Speech_OnChallengeObjSpch(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "_ObjEntity", "_ObjCharType", "SpeechText", "SpeechAnswer", "action", "defLoop", "isMerc", NULL };
	PyObject *obj, *type, *text, *ans, *action = NULL, *defLoop = NULL, *merc = NULL, *own[2];
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOOS|OOO", names, &obj, &type, &text, &ans, &action, &defLoop, &merc)) {
		return NULL;
	}
	Sp_TalkDefaults(&action, &defLoop, own);
	int isMerc = merc ? Script_NumCmp(merc, 1) == 0 : 0;
	PyObject *a = Sp_OpenVoiceStr(isMerc ? "ChallengeRaceStdOpt2Voice" : "ChallengeRaceStdOpt1Voice", 0) &&
		Sp_OpenVoice(ans, 2) && Sp_PlayVoiceAfter(0, PyInt_FromLong(0)) ? Sp_Lang(PyString_AsString(ans)) : NULL;
	PyObject *r = a ? Sp_OnObjectSpeech(obj, type, text, a, action, defLoop) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(own[0]);
	Py_XDECREF(own[1]);
	if (r) {
		Py_DECREF(r);
		r = Py_ReturnNone();
	}
	return r;
}

/* ObjectAutoAnswer(): the end of the player's line to the object, its answer follows */
static PyObject *Speech_ObjectAutoAnswer(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *me = Sp_Usr();
	int talk = me ? Script_HPrefix(me, "Action", "Talk") : -1;
	byte ok = talk >= 0 && (!talk || Script_HSet(me, "ForceAction", PyString_FromString("TalkListenNPC")));
	Py_XDECREF(me);
	if (ok) {
		Speech_StopMenuEvents(NULL, NULL);
		ok = Script_LaterIn(1.0, "Speech", "RestartMenuEvents", PyTuple_New(0), NULL) &&
			Script_LaterIn(0.45, "Speech", "ObjectAutoAnswer2", PyTuple_New(0), NULL);
	}
	return ok ? Py_ReturnNone() : NULL;
}

/* ObjectAutoAnswer2(): the answer of the object over the radio */
static PyObject *Speech_ObjectAutoAnswer2(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *two = PyInt_FromLong(2), *zero = PyInt_FromLong(0);
	byte ok = Script_Try("SSound", "StopVoice", "(i)", 0) && Sp_PlayVoiceScheduled(two, 0, zero);
	Py_XDECREF(two);
	Py_XDECREF(zero);
	PyObject *obj = ok ? Sp_Get("ObjEntity") : NULL, *action = obj ? Sp_Get("ObjAutoAnswerAction") : NULL;
	PyObject *loop = action ? Sp_Get("ObjAutoAnswerDefLoop") : NULL, *ans = loop ? Sp_Get("ObjAnswer") : NULL;
	PyObject *type = ans ? Sp_Get("ObjCharType") : NULL, *cb = type ? PyString_FromString("Speech.EndObjectAnswer") : NULL;
	ok = cb && Sp_StartObjectSpeech(obj, action, loop, ans, type, cb);
	Py_XDECREF(obj);
	Py_XDECREF(action);
	Py_XDECREF(loop);
	Py_XDECREF(ans);
	Py_XDECREF(type);
	Py_XDECREF(cb);
	PyObject *me = ok ? Sp_Usr() : NULL;
	ok = me && (PyObject_IsTrue(me) <= 0 || (Script_HSet(me, "ForceAction", Sp_Get("TalkListen")) &&
		Script_HSet(me, "ForceDefLoop", PyString_FromString("Relax"))));
	Py_XDECREF(me);
	return ok ? Py_ReturnNone() : NULL;
}

/* EndObjectAnswer(): the object listens again, the head stops talking */
static PyObject *Speech_EndObjectAnswer(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *obj = Sp_Get("ObjEntity");
	int relax = obj ? Script_HIs(obj, "Action", "Relax") : -1;
	byte ok = relax >= 0 && (relax || Script_HSet(obj, "Action", PyString_FromString("Listen"))) &&
		Script_Try("SScorer", "Set", "(issi)", 0, "Monitor", "HeadAnimIndex", 0);
	Py_XDECREF(obj);
	return ok ? Speech_RestoreSpeech(NULL, NULL) : NULL;
}

PyMethodDef g_SpeechMethods[] = {
	{ "ScheduledVoiceCallback", Speech_ScheduledVoiceCallback, 1, NULL },
	{ "PlayVoiceScheduled", Speech_PlayVoiceScheduled, 1, NULL },
	{ "PerformAnswer", Speech_PerformAnswer, 1, NULL },
	{ "NormalAbortSpeech", Speech_NormalAbortSpeech, 1, NULL },
	{ "CallEndOfSpeech", Speech_CallEndOfSpeech, 1, NULL },
	{ "AbortSpeech", Speech_AbortSpeech, 1, NULL },
	{ "RestoreState", Speech_RestoreState, 1, NULL },
	{ "AbortNPCSpeech", Speech_AbortNPCSpeech, 1, NULL },
	{ "AutoAnswer", Speech_AutoAnswer, 1, NULL },
	{ "StopMenuEvents", Speech_StopMenuEvents, 1, NULL },
	{ "RestartMenuEvents", Speech_RestartMenuEvents, 1, NULL },
	{ "RestoreMoveTime", Speech_RestoreMoveTime, 1, NULL },
	{ "StdAnsLst", Speech_StdAnsLst, 1, NULL },
	{ "StdOnSpch", Speech_StdOnSpch, 1, NULL },
	{ "CalculateSpeechTime", Speech_CalculateSpeechTime, 1, NULL },
	{ "OnSpch", Speech_OnSpch, 1, NULL },
	{ "OnChallengeSpch", (PyCFunction)(void (*)(void))Speech_OnChallengeSpch, 3, NULL },
	{ "OnSpeech", Speech_OnSpeech, 1, NULL },
	{ "PlayerAnsEnd", Speech_PlayerAnsEnd, 1, NULL },
	{ "PlayerAns", Speech_PlayerAns, 1, NULL },
	{ "SetPlayerToTalk", Speech_SetPlayerToTalk, 1, NULL },
	{ "InspectChar", Speech_InspectChar, 1, NULL },
	{ "AutoSpeech", Speech_AutoSpeech, 1, NULL },
	{ "NISpeech", Speech_NISpeech, 1, NULL },
	{ "AutoAddNI", Speech_AutoAddNI, 1, NULL },
	{ "AddNI", Speech_AddNI, 1, NULL },
	{ "RunNI", Speech_RunNI, 1, NULL },
	{ "ObjNIEnd", Speech_ObjNIEnd, 1, NULL },
	{ "LaunchTaunt", Speech_LaunchTaunt, 1, NULL },
	{ "RestoreSpeech", Speech_RestoreSpeech, 1, NULL },
	{ "DoRemote", Speech_DoRemote, 1, NULL },
	{ "OffVoice", Speech_OffVoice, 1, NULL },
	{ "RemoteComunication", Speech_RemoteComunication, 1, NULL },
	{ "ForceRemoteComInternal", Speech_ForceRemoteComInternal, 1, NULL },
	{ "ForceRemoteCom", Speech_ForceRemoteCom, 1, NULL },
	{ "ForceRemoteComunication", Speech_ForceRemoteComunication, 1, NULL },
	{ "AddRemoteCom", Speech_AddRemoteCom, 1, NULL },
	{ "ProcessRemoteCom", Speech_ProcessRemoteCom, 1, NULL },
	{ "CancelRemoteCom", Speech_CancelRemoteCom, 1, NULL },
	{ "TestInRange", Speech_TestInRange, 1, NULL },
	{ "StartObjSpch", Speech_StartObjSpch, 1, NULL },
	{ "StartObjectSpeech", Speech_StartObjectSpeech, 1, NULL },
	{ "StdObjSpch", Speech_StdObjSpch, 1, NULL },
	{ "OnObjSpch", Speech_OnObjSpch, 1, NULL },
	{ "OnChallengeObjSpch", (PyCFunction)(void (*)(void))Speech_OnChallengeObjSpch, 3, NULL },
	{ "OnObjectSpeech", Speech_OnObjectSpeech, 1, NULL },
	{ "ObjectAutoAnswer", Speech_ObjectAutoAnswer, 1, NULL },
	{ "ObjectAutoAnswer2", Speech_ObjectAutoAnswer2, 1, NULL },
	{ "EndObjectAnswer", Speech_EndObjectAnswer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
