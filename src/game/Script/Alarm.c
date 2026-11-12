/*
 * The Alarm and NewsPanel modules in C (scripts/missions/alarm.py and newspanel.py, issue #2 step 8a). The alarm:
 * SetAlarm is the engine's AlarmCallback, it calls the abort and restore hooks of the mission and the one-shot
 * callbacks other scripts put into SetAlarmCallback. The news panels of the indoor levels: the panels at the
 * DM_NewsPanel_ dummies, their options from the language texts of the act and the news on the monitor.
 *
 * The hooks (SetAlarmCallback, AbortMissionCallback, RestoreMissionCallback and the Temp ones), the state of the
 * panels (Panels, ShowingNews, MissionUse, NewsOnMem, act, NewsList) stay in the modules; the functions read them as
 * their globals. The module code is data/modules/alarm.mod and newspanel.mod; the loop of newspanel.py over the
 * engine variables of the panels is the hook NewsPanel_ModSettings.
 *
 * Both modules are native C (docs/objlib-plan.md, stage 2): the functions work on C values and read the globals where
 * the script reads them and write them back at once (the missions, the levels and the cases read and set
 * them). The hooks and callbacks are called as the values they hold, the panels are handle values and Scrap.Set,
 * CreateEntity and the dummy search go through their seams. Speech.RemoteComunication and the functions
 * of the modules through their globals are called through their refs (PORT_CALL, PORT_OWN: the port seam, OBJLIB 5
 * (9c)); the imports stay (OBJLIB 6), and a news line that is not the form GetNews makes runs through the exec reader
 * (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

/* ---- Alarm ---- */

/* the hook temp (a global of Alarm) when it is set, else once the variable once (set to None before the call) */
static byte Al_Hook(const char *temp, const char *once) {
	nativeVal_t t = { 0 }, cb = { 0 }, none;
	byte ok = Mode_G("Alarm", temp, &t);
	if (ok && Native_True(&t)) {
		ok = Mode_CallArgs(&t, NULL, NULL, 0);
	} else if (ok) {
		Native_SetNone(&none);
		ok = Mode_G("Alarm", once, &cb) && (!Native_True(&cb) || (Mode_Put("Alarm", once, NULL, &none) &&
			Mode_CallArgs(&cb, NULL, NULL, 0)));
	}
	Native_Free(&t);
	Native_Free(&cb);
	return ok;
}

/* SetAlarm(status): the alarm goes on or off; the hooks of the mission, then the callbacks waiting in
 * SetAlarmCallback (the list is emptied first, a callback that wants more puts itself back) */
static byte Al_SetAlarm(const nativeVal_t *status) {
	nativeVal_t prev = { 0 }, empty, func = { 0 };
	byte ok;
	if (Native_True(status)) {
		ok = Mode_PrintS("Sound the alarm !") && PORT_OWN(Alarm, ActivateAlarm, NULL, "") &&
			Al_Hook("TempAbortMissionCallback", "AbortMissionCallback");
	} else {
		ok = Mode_PrintS("Free way!") && PORT_OWN(Alarm, DeactivateAlarm, NULL, "") &&
			Al_Hook("TempRestoreMissionCallback", "RestoreMissionCallback");
	}
	Native_SetEmpty(&empty, NV_LIST);
	ok = ok && Mode_G("Alarm", "SetAlarmCallback", &prev) && Mode_Put("Alarm", "SetAlarmCallback", NULL, &empty);
	int n = ok ? Mode_Loop(&prev) : -1;
	for (int i = 0; i < n && ok; i++) {
		ok = Native_SeqItem(&prev, i, &func) && Mode_CallArgs(&func, NULL, status, 1);
		Native_Free(&func);
	}
	Native_Free(&prev);
	Native_Free(&empty);
	return ok && n >= 0;
}

/* DeactivateAlarm(): outdoors the police waves start over */
static byte Al_DeactivateAlarm(void) {
	nativeVal_t skip = { 0 };
	byte ok = Mode_Import("Init") && LvN_ModGet("Init", "InMapSkip", &skip) &&
		(Native_True(&skip) || (Mode_Import("OutPolice") && LvN_ModSetI("OutPolice", "iPoliceWave", 0)));
	Native_Free(&skip);
	return ok;
}

MODE_FN(Alarm, SetAlarm, 1, Al_SetAlarm(&a[0]))
/* ActivateAlarm(), Init(): nothing */
MODE_FN(Alarm, Nothing, 0, 1)
MODE_FN(Alarm, DeactivateAlarm, 0, Al_DeactivateAlarm())

const portFn_t g_AlarmMethods[] = {
	PORT_FN(SetAlarm, Alarm_SetAlarm),
	PORT_FN(ActivateAlarm, Alarm_Nothing),
	PORT_FN(DeactivateAlarm, Alarm_DeactivateAlarm),
	PORT_FN(Init, Alarm_Nothing),
	PORT_FNEND
};

/* ---- NewsPanel ---- */

/* `n` of the script as a text value of t */
static nativeVal_t *Np_ReprI(modeVals_t *t, long n) {
	nativeVal_t *r = Mode_V(t);
	Mode_Repr(Mode_I(t, n), r);
	return r;
}

/* GetNews(): NewsList, the option lines of the news of the act ("kb.ArrOpt<n> = '<n+1>:'+keydata", the text); after
 * the death of the archbishop only the first */
static byte Np_GetNews(void) {
	static const char *const dead[] = { "Mission_ArchbishopDead", "Mission_HumphreyArchbishopKilled", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *empty = Mode_V(&t);
	Native_SetEmpty(empty, NV_LIST);
	if (!Mode_Put("NewsPanel", "NewsList", NULL, empty)) {
		return Mode_Done(&t, 0);
	}
	for (long n = 0;; n++) {
		int mark = t.n;
		nativeVal_t *act = Mode_V(&t), *key = Mode_V(&t), *keydata = Mode_V(&t), *line = Mode_V(&t), *pair = Mode_V(&t);
		nativeVal_t *lib = Mode_V(&t);
		if (!Mode_G("NewsPanel", "act", act) || !Mode_Cat(&t, key, 4, Mode_S(&t, "News_"), act, Mode_S(&t, "_Title"),
			Np_ReprI(&t, n + 1)) || !Mode_LangV(key, keydata)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(keydata)) {
			break;
		}
		if (!Mode_Cat(&t, line, 5, Mode_S(&t, "kb.ArrOpt"), Np_ReprI(&t, n), Mode_S(&t, " = '"), Np_ReprI(&t, n + 1),
			Mode_S(&t, ":'+keydata"))) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(pair, NV_TUPLE);
		Native_Copy(Native_Append(pair), line);
		Native_Copy(Native_Append(pair), keydata);
		if (!Mode_Append("NewsPanel", "NewsList", pair)) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveVar("Mission.Library", lib);
		byte stop = 0;
		for (int i = 0; dead[i]; i++) {
			stop |= Native_IsStr(lib, dead[i]);
		}
		if (stop) {
			break;
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, 1);
}

/* exec(xqt[0]) of FillNews: a line as GetNews makes it ("kb.<name> = '<text>'+keydata") is kb.<name> = '<text>' +
 * keydata made directly, any other line runs through the exec reader with the variables kb, xqt and keydata */
static byte Np_Exec(const nativeVal_t *code, const nativeVal_t *kb, const nativeVal_t *xqt, const nativeVal_t *keydata) {
	static const char tail[] = "'+keydata";
	const char *s = code->type == NV_STR ? Native_Text(code) : NULL;
	const char *eq = s ? strstr(s, " = '") : NULL;
	int n = s ? code->n : 0, tl = (int)sizeof(tail) - 1;
	if (eq && (int)strlen(s) == n && !strncmp(s, "kb.", 3) && Script_IsName(s + 3, (int)(eq - s) - 3) &&
		eq + 4 <= s + n - tl && !strcmp(s + n - tl, tail)) {
		char name[0x40];
		int len = (int)(eq - s) - 3, tlen = (int)(s + n - tl - (eq + 4));
		byte plain = len < (int)sizeof(name) && Script_IsQuotableText(eq + 4, tlen);
		if (plain) {
			nativeVal_t text, v = { 0 };
			memcpy(name, s + 3, len);
			name[len] = '\0';
			Native_SetText(&text, eq + 4, tlen);
			byte ok = Mode_Arith('+', &text, keydata, &v) && Mode_VSet(kb, name, &v);
			Native_Free(&text);
			Native_Free(&v);
			return ok;
		}
	}
	execScope_t x = { .module = "NewsPanel" };
	Exec_Var(&x, "kb", kb);
	Exec_Var(&x, "xqt", xqt);
	Exec_Var(&x, "keydata", keydata);
	return Exec_Text(&x, code);
}

/* FillNews(kb): the lines of NewsList run on the panel kb */
static byte Np_FillNews(const nativeVal_t *kb) {
	nativeVal_t list = { 0 }, xqt = { 0 }, keydata = { 0 }, code = { 0 };
	int n = Mode_G("NewsPanel", "NewsList", &list) ? Mode_Loop(&list) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&list, i, &xqt) && LvN_Item(&xqt, 1, &keydata) && LvN_Item(&xqt, 0, &code) &&
			Np_Exec(&code, kb, &xqt, &keydata);
		Native_Free(&xqt);
		Native_Free(&keydata);
		Native_Free(&code);
	}
	Native_Free(&list);
	return ok;
}

/* Init(): the panels at the DM_NewsPanel_ dummies; with news in this act they show them as options */
static byte Np_Init(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *use = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Import("Alarm") || !Mode_Import("string") || !Mode_G("NewsPanel", "MissionUse", use)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(use)) {
		if (!Mode_AppendG("Alarm", "SetAlarmCallback", "NewsPanel", "SwitchAlarm")) {
			return Mode_Done(&t, 0);
		}
	} else {
		static const char *const bmps[] = { "BackNewsBmp", "NewsLogoBmp" };
		for (int i = 0; i < 2; i++) {
			Native_Free(v);
			if (!Native_GVarGet(bmps[i], v) || !Mode_PreloadTexture(v)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!Mode_Read("NewsPanel", "NewsOnMem", v) || (Native_True(v) && !PORT_OWN(NewsPanel, GetNews, NULL, ""))) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_DummyStartSeam("DM_NewsPanel_*", 1)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		int mark = t.n;
		nativeVal_t *dummy = Mode_V(&t), *d0 = Mode_V(&t), *d1 = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
		nativeVal_t *z = Mode_V(&t), *kb = Mode_V(&t), *d2 = Mode_V(&t), *yaw = Mode_V(&t), *ang = Mode_V(&t);
		nativeVal_t *c = Mode_V(&t), *letter = Mode_V(&t), *model = Mode_V(&t), *file = Mode_V(&t), *onmem = Mode_V(&t);
		nativeVal_t *mu = Mode_V(&t), *a1 = Mode_V(&t);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			break;
		}
		if (!LvN_Item(dummy, 0, d0) || !LvN_Item(dummy, 1, d1) || !LvN_Item(d1, 0, x) || !LvN_Item(d1, 1, y) ||
			!LvN_Item(d1, 2, z) || !LvN_Create(d0, x, y, z, "WithLife", kb) || !LvN_Item(dummy, 2, d2) ||
			!LvN_Item(d2, 1, a1) || !Mode_Arith('+', a1, Mode_I(&t, 180), yaw)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(ang, NV_TUPLE);
		Native_SetInt(Native_Append(ang), 0);
		Native_Copy(Native_Append(ang), yaw);
		Native_SetInt(Native_Append(ang), 0);
		if (!Mode_VSet(kb, "Ang", ang) || !LvN_Item(d0, 13, c) || !Native_Upper(c, letter) ||
			!Mode_G("NewsPanel", "NewsPanelModel", model) || !Mode_Cat(&t, file, 3, model, letter, Mode_S(&t, ".m3d")) ||
			!Mode_VModel(kb, file) || !LvN_SetI(kb, "VRadius", 60) || !LvN_SetI(kb, "Life", 0) ||
			!LvN_SetI(kb, "ActiveCMask", 0) || !LvN_SetI(kb, "MapCollision", 1) || !LvN_SetI(kb, "UseMapCheck", 0) ||
			!LvN_SetI(kb, "AlwaysUse", 0) || !Mode_G("NewsPanel", "NewsOnMem", onmem)) {
			return Mode_Done(&t, 0);
		}
		/* NewsOnMem and not MissionUse: the value of the first operand when it is false */
		if (Native_True(onmem)) {
			if (!Mode_G("NewsPanel", "MissionUse", mu) || !LvN_SetI(kb, "ArrowCanUse", !Native_True(mu))) {
				return Mode_Done(&t, 0);
			}
		} else if (!Mode_VSet(kb, "ArrowCanUse", onmem)) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_Read("NewsPanel", "NewsOnMem", onmem) || (Native_True(onmem) &&
			(!PORT_OWN(NewsPanel, FillNews, NULL, "v", kb) || !LvN_SetS(kb, "UseFunc", "NewsPanel.UseNewsPanel"))) ||
			!Mode_Append("NewsPanel", "Panels", kb) || !Mode_GVarSetI("NewsPanelActive", 1)) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, Mode_Read("NewsPanel", "NewsOnMem", v) && (!Native_True(v) || Mode_DelGlobal("NewsPanel", "NewsList")));
}

/* panel.var = v for every panel of Panels */
static byte Np_Panels(const char *var, const nativeVal_t *v) {
	nativeVal_t panels = { 0 }, p = { 0 };
	int n = Mode_G("NewsPanel", "Panels", &panels) ? Mode_Loop(&panels) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&panels, i, &p) && Mode_VSet(&p, var, v);
		Native_Free(&p);
	}
	Native_Free(&panels);
	return ok;
}

/* UseNewsPanel(UsrName, CharName, optId, FullOption): the news of the option on the monitor; every panel moves on to
 * the next option */
static byte Np_UseNewsPanel(const nativeVal_t *charName, const nativeVal_t *opt) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *act = Mode_V(&t), *msg = Mode_V(&t), *end = Mode_V(&t), *r = Mode_V(&t), *ch = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t *next = Mode_V(&t), *num = Mode_V(&t), *iopt = Mode_V(&t);
	if (!Mode_Import("Speech")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("GameAct", act);
	if (!Mode_Cat(&t, msg, 4, Mode_S(&t, "News_"), act, Mode_S(&t, "_RemoteMessage"), opt) ||
		!Mode_G("NewsPanel", "EndOfNews", end) ||
		!PORT_CALL(Speech, RemoteComunication, r, "vsiv", msg, "Betty", 1L, end)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(r)) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, LvN_PutI("NewsPanel", "ShowingNews", 1) && Mode_GVarSetI("NewsPanelShowHead", 1) &&
		Mode_EntitySeam(charName, ch) && Mode_VObj(ch, "ArriOpt", cur) && Mode_Arith('+', cur, Mode_I(&t, 1), next) &&
		Mode_VObj(ch, "ArrnOpt", num) && Mode_Arith('%', next, num, iopt) && Np_Panels("ArriOpt", iopt));
}

/* EndOfNews(): the end of the news on the monitor */
static byte Np_EndOfNews(void) {
	return LvN_PutI("NewsPanel", "ShowingNews", 0) && Mode_GVarSetI("NewsPanelShowHead", 0);
}

/* SwitchAlarm(status): with the alarm on the panels cannot be used and the news stop; it stays an alarm callback */
static byte Np_SwitchAlarm(const nativeVal_t *status) {
	nativeVal_t off;
	if (!Mode_Import("Alarm") || !Mode_AppendG("Alarm", "SetAlarmCallback", "NewsPanel", "SwitchAlarm")) {
		return 0;
	}
	int showing = Native_True(status) ? LvN_GTrue("NewsPanel", "ShowingNews") : 0;
	if (showing < 0 || (showing && !Mode_GVarSetI("RemainTalkTime", 0))) {
		return 0;
	}
	Native_SetInt(&off, !Native_True(status));
	return Np_Panels("ArrowCanUse", &off);
}

MODE_FN(NewsPanel, GetNews, 0, Np_GetNews())
MODE_FN(NewsPanel, FillNews, 1, Np_FillNews(&a[0]))
MODE_FN(NewsPanel, Init, 0, Np_Init())
MODE_FN(NewsPanel, UseNewsPanel, 4, Np_UseNewsPanel(&a[1], &a[2]))
MODE_FN(NewsPanel, EndOfNews, 0, Np_EndOfNews())
MODE_FN(NewsPanel, SwitchAlarm, 1, Np_SwitchAlarm(&a[0]))

static byte News_SettingsBody(PyObject *locals, PyObject *f, void *ctx) {
	(void)ctx;
	PyObject *apply = Script_ModuleName(locals, "apply");
	PyObject *set = apply ? Script_ModuleName(locals, "Scrap.Set") : NULL;
	PyObject *res = set ? Script_CallObject(apply, Py_BuildValue("(OO)", set, f), NULL) : NULL;
	Py_XDECREF(apply);
	Py_XDECREF(set);
	Py_XDECREF(res);
	return res != NULL;
}

/* newspanel.py:7, `for f in (("NewsLogoBmp", "2D/News/LogoNews.bmp"), ...): apply(Scrap.Set, f)` (OBJLIB 7: a hook of
 * the module code, ScriptModule.c's table: it runs on the dict of the module's names, with the constants of the code
 * object as the compiler made them, `f` left in the module, apply and Scrap.Set looked up there) */
int NewsPanel_ModSettings(PyObject *module, PyObject *locals, nativeVal_t *body) {
	(void)body;
	static const struct {
		const char *var;
		const char *text;	/* a string value, else num */
		double num;
		byte isFloat;
	} vars[] = {
		{ "NewsLogoBmp", "2D/News/LogoNews.bmp", 0, 0 }, { "BackNewsBmp", "2D/News/BackNews.bmp", 0, 0 },
		{ "NewsHead", "Betty", 0, 0 }, { "NewsPanelDistance", NULL, 70, 0 }, { "NewsPanelDisplacement", NULL, 5, 0 },
		{ "NewsPanelMove", NULL, 18, 0 }, { "NewsPanelX", NULL, 350, 0 }, { "NewsPanelY", NULL, 20, 0 },
		{ "NewsPanelW", NULL, 270, 0 }, { "NewsPanelH", NULL, 180, 0 }, { "NewsPanelOpenSpeed", NULL, 3.0, 1 },
		{ "NewsPanelCloseSpeed", NULL, 4.0, 1 },
	};
	enum { NUM_VARS = sizeof(vars) / sizeof(vars[0]) };
	(void)module;
	PyObject *seq = PyTuple_New(NUM_VARS);
	for (int i = 0; seq && i < NUM_VARS; i++) {
		PyObject *v = vars[i].text ? Script_StrConst(vars[i].text, (int)strlen(vars[i].text)) :
			vars[i].isFloat ? PyFloat_FromDouble(vars[i].num) : PyInt_FromLong((long)vars[i].num);
		PyObject *pair = v ? Py_BuildValue("(NN)", Script_StrConst(vars[i].var, (int)strlen(vars[i].var)), v) : NULL;
		if (!pair) {
			Py_DECREF(seq);
			seq = NULL;
			break;
		}
		PyTuple_SET_ITEM(seq, i, pair);
	}
	byte ok = seq && Script_ModuleFor(locals, "f", seq, News_SettingsBody, NULL);
	Py_XDECREF(seq);
	return ok;
}

const portFn_t g_NewsPanelMethods[] = {
	PORT_FN(GetNews, NewsPanel_GetNews),
	PORT_FN(FillNews, NewsPanel_FillNews),
	PORT_FN(Init, NewsPanel_Init),
	PORT_FN(UseNewsPanel, NewsPanel_UseNewsPanel),
	PORT_FN(EndOfNews, NewsPanel_EndOfNews),
	PORT_FN(SwitchAlarm, NewsPanel_SwitchAlarm),
	PORT_FNEND
};

PORTS(Alarm)
