/*
 * The Speech module in C (scripts/chars/speech.py, issue #2 step 7b): the talks between the player and the
 * characters. Part 1: the voices, a speech and its answer (OnSpch, OnSpeech, AutoAnswer, PerformAnswer), the end and
 * the abort of a speech, the answer list of a character, the player's own answers, InspectChar and AutoSpeech.
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
static PyObject *Speech_OnChallengeSpch(PyObject *self, PyObject *args) {
	PyObject *text, *ans, *anima = NULL, *merc = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OS|OO", &text, &ans, &anima, &merc)) {
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
	{ "OnChallengeSpch", Speech_OnChallengeSpch, 1, NULL },
	{ "OnSpeech", Speech_OnSpeech, 1, NULL },
	{ "PlayerAnsEnd", Speech_PlayerAnsEnd, 1, NULL },
	{ "PlayerAns", Speech_PlayerAns, 1, NULL },
	{ "SetPlayerToTalk", Speech_SetPlayerToTalk, 1, NULL },
	{ "InspectChar", Speech_InspectChar, 1, NULL },
	{ "AutoSpeech", Speech_AutoSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
