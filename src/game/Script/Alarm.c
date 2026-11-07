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
 */
#include <string.h>

#include "scrap/script.h"

/* ---- Alarm ---- */

/* hook (a global of Alarm) when it is set, else once the variable once (set to None before the call) */
static void Alarm_Hook(scriptFrame_t *f, const char *temp, const char *once) {
	PyObject *t = Sf_Global(f, "Alarm", temp);
	int set = Sf_True(f, t);
	if (set > 0) {
		Sf_Call(f, t, "()");
	} else if (set == 0) {
		PyObject *cb = Sf_Global(f, "Alarm", once);
		if (Sf_True(f, cb) > 0) {
			Sf_SetGlobal(f, "Alarm", once, Py_None);
			Sf_Call(f, cb, "()");
		}
	}
}

/* SetAlarm(status): the alarm goes on or off; the hooks of the mission, then the callbacks waiting in
 * SetAlarmCallback (the list is emptied first, a callback that wants more puts itself back) */
static PyObject *Alarm_SetAlarm(PyObject *self, PyObject *args) {
	PyObject *status;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &status)) {
		return NULL;
	}
	int on = Sf_True(&f, status);
	if (on > 0) {
		Sf_Print(&f, "s", "Sound the alarm !");
		Sf_Call(&f, Sf_Global(&f, "Alarm", "ActivateAlarm"), "()");
		Alarm_Hook(&f, "TempAbortMissionCallback", "AbortMissionCallback");
	} else if (on == 0) {
		Sf_Print(&f, "s", "Free way!");
		Sf_Call(&f, Sf_Global(&f, "Alarm", "DeactivateAlarm"), "()");
		Alarm_Hook(&f, "TempRestoreMissionCallback", "RestoreMissionCallback");
	}
	PyObject *prev = Sf_Global(&f, "Alarm", "SetAlarmCallback");
	Sf_SetGlobal(&f, "Alarm", "SetAlarmCallback", Sf_Keep(&f, PyList_New(0)));
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *func = Sf_Next(&f, prev, i);
		if (!func) {
			break;
		}
		Sf_Call(&f, func, "(O)", status);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ActivateAlarm(), Init(): nothing */
static PyObject *Alarm_Nothing(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* DeactivateAlarm(): outdoors the police waves start over */
static PyObject *Alarm_DeactivateAlarm(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "InMapSkip")) == 0) {
		Sf_Import(&f, "OutPolice");
		Sf_SetMod(&f, "OutPolice", "iPoliceWave", Sf_Int(&f, 0));
	}
	return Sf_None(&f);
}

PyMethodDef g_AlarmMethods[] = {
	{ "SetAlarm", Alarm_SetAlarm, 1, NULL },
	{ "ActivateAlarm", Alarm_Nothing, 1, NULL },
	{ "DeactivateAlarm", Alarm_DeactivateAlarm, 1, NULL },
	{ "Init", Alarm_Nothing, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- NewsPanel ---- */

/* GetNews(): NewsList, the option lines of the news of the act ("kb.ArrOpt<n> = '<n+1>:'+keydata", the text); after
 * the death of the archbishop only the first */
static PyObject *NewsPanel_GetNews(PyObject *self, PyObject *args) {
	static const char *const dead[] = { "Mission_ArchbishopDead", "Mission_HumphreyArchbishopKilled", NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, "NewsPanel", "NewsList", Sf_Keep(&f, PyList_New(0)));
	for (long n = 0; !f.failed; n++) {
		int mark = f.n;
		PyObject *key = Sf_Cat(&f, "sOsO", "News_", Sf_Global(&f, "NewsPanel", "act"), "_Title", Sf_ReprI(&f, n + 1));
		PyObject *keydata = Sf_Lang(&f, key);
		if (Sf_True(&f, keydata) <= 0) {
			break;
		}
		PyObject *line = Sf_Cat(&f, "sOsOs", "kb.ArrOpt", Sf_ReprI(&f, n), " = '", Sf_ReprI(&f, n + 1), ":'+keydata");
		Sf_Method(&f, Sf_Global(&f, "NewsPanel", "NewsList"), "append", "((OO))", line, keydata);
		if (Sf_InS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission.Library"), dead)) {
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* exec(xqt[0]) of FillNews: a line as GetNews makes it ("kb.<name> = '<text>'+keydata") is kb.<name> = '<text>' +
 * keydata made directly, any other line runs through exec */
static void Np_Exec(scriptFrame_t *f, PyObject *code, PyObject *kb, PyObject *keydata, PyObject *locals) {
	static const char tail[] = "'+keydata";
	const char *s = !f->failed && code && PyString_Check(code) ? PyString_AsString(code) : NULL;
	const char *eq = s ? strstr(s, " = '") : NULL;
	int n = s ? PyString_Size(code) : 0, tl = (int)sizeof(tail) - 1;
	if (eq && (int)strlen(s) == n && !strncmp(s, "kb.", 3) && Script_IsName(s + 3, (int)(eq - s) - 3) &&
		eq + 4 <= s + n - tl && !strcmp(s + n - tl, tail)) {
		char name[0x40];
		int len = (int)(eq - s) - 3, tlen = (int)(s + n - tl - (eq + 4));
		PyObject *text = len < (int)sizeof(name) ? Sf_Keep(f, Script_StrConst(eq + 4, tlen)) : NULL;
		if (text && Script_IsQuotable(text)) {
			memcpy(name, s + 3, len);
			name[len] = '\0';
			Sf_SetAttr(f, kb, name, Sf_Op(f, text, '+', keydata));
			return;
		}
	}
	Sf_Exec(f, code, "NewsPanel", locals);
}

/* FillNews(kb): the lines of NewsList run on the panel kb */
static PyObject *NewsPanel_FillNews(PyObject *self, PyObject *args) {
	PyObject *kb;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &kb)) {
		return NULL;
	}
	PyObject *list = Sf_Global(&f, "NewsPanel", "NewsList");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *xqt = Sf_Next(&f, list, i);
		if (!xqt) {
			break;
		}
		PyObject *keydata = Sf_Item(&f, xqt, 1);
		PyObject *code = Sf_Item(&f, xqt, 0);
		PyObject *locals = Sf_Build(&f, "{sOsOsO}", "kb", kb, "xqt", xqt, "keydata", keydata);
		Np_Exec(&f, code, kb, keydata, locals);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* Init(): the panels at the DM_NewsPanel_ dummies; with news in this act they show them as options */
static PyObject *NewsPanel_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Alarm");
	Sf_Import(&f, "string");
	int use = Sf_True(&f, Sf_Global(&f, "NewsPanel", "MissionUse"));
	if (use == 0) {
		PyObject *list = Sf_Mod(&f, "Alarm", "SetAlarmCallback");
		PyObject *append = Sf_Attr(&f, list, "append");
		Sf_Call(&f, append, "(O)", Sf_Global(&f, "NewsPanel", "SwitchAlarm"));
	} else if (use > 0) {
		Sf_Ask(&f, "SScorer", "PreloadTexture", "(O)", Sf_Ask(&f, "Scrap", "Get", "(s)", "BackNewsBmp"));
		Sf_Ask(&f, "SScorer", "PreloadTexture", "(O)", Sf_Ask(&f, "Scrap", "Get", "(s)", "NewsLogoBmp"));
	}
	if (Sf_True(&f, Sf_Global(&f, "NewsPanel", "NewsOnMem")) > 0) {
		Sf_Call(&f, Sf_Global(&f, "NewsPanel", "GetNews"), "()");
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_NewsPanel_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *d0 = Sf_Item(&f, dummy, 0), *d1 = Sf_Item(&f, dummy, 1);
		PyObject *x = Sf_Item(&f, d1, 0), *y = Sf_Item(&f, d1, 1), *z = Sf_Item(&f, d1, 2);
		PyObject *kb = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", d0, x, y, z, "WithLife");
		PyObject *yaw = Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180);
		Sf_SetAttr(&f, kb, "Ang", Sf_Build(&f, "(iOi)", 0, yaw, 0));
		PyObject *letter = Sf_Ask(&f, "string", "upper", "(O)", Sf_Item(&f, d0, 13));
		Sf_Method(&f, kb, "SetModel", "(O)", Sf_Cat(&f, "OOs", Sf_Global(&f, "NewsPanel", "NewsPanelModel"), letter, ".m3d"));
		Sf_SetAttrI(&f, kb, "VRadius", 60);
		Sf_SetAttrI(&f, kb, "Life", 0);
		Sf_SetAttrI(&f, kb, "ActiveCMask", 0);
		Sf_SetAttrI(&f, kb, "MapCollision", 1);
		Sf_SetAttrI(&f, kb, "UseMapCheck", 0);
		Sf_SetAttrI(&f, kb, "AlwaysUse", 0);
		/* NewsOnMem and not MissionUse: the value of the first operand when it is false */
		PyObject *onmem = Sf_Global(&f, "NewsPanel", "NewsOnMem");
		int om = Sf_True(&f, onmem);
		if (om > 0) {
			int mu = Sf_True(&f, Sf_Global(&f, "NewsPanel", "MissionUse"));
			Sf_SetAttr(&f, kb, "ArrowCanUse", mu >= 0 ? Sf_Int(&f, !mu) : NULL);
		} else {
			Sf_SetAttr(&f, kb, "ArrowCanUse", onmem);
		}
		if (Sf_True(&f, Sf_Global(&f, "NewsPanel", "NewsOnMem")) > 0) {
			Sf_Call(&f, Sf_Global(&f, "NewsPanel", "FillNews"), "(O)", kb);
			Sf_SetAttrS(&f, kb, "UseFunc", "NewsPanel.UseNewsPanel");
		}
		Sf_Method(&f, Sf_Global(&f, "NewsPanel", "Panels"), "append", "(O)", kb);
		Sf_Ask(&f, "Scrap", "Set", "(si)", "NewsPanelActive", 1);
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, Sf_Global(&f, "NewsPanel", "NewsOnMem")) > 0) {
		Sf_DelGlobal(&f, "NewsPanel", "NewsList");
	}
	return Sf_None(&f);
}

/* UseNewsPanel(UsrName, CharName, optId, FullOption): the news of the option on the monitor; every panel moves on to
 * the next option */
static PyObject *NewsPanel_UseNewsPanel(PyObject *self, PyObject *args) {
	PyObject *usr, *charName, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &charName, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *act = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "GameAct");
	PyObject *msg = Sf_Cat(&f, "sOsO", "News_", act, "_RemoteMessage", opt);
	PyObject *end = msg ? Sf_Global(&f, "NewsPanel", "EndOfNews") : NULL;
	if (Sf_True(&f, Sf_Ask(&f, "Speech", "RemoteComunication", "(OsiO)", msg, "Betty", 1, end)) > 0) {
		Sf_SetGlobal(&f, "NewsPanel", "ShowingNews", Sf_Int(&f, 1));
		Sf_Ask(&f, "Scrap", "Set", "(si)", "NewsPanelShowHead", 1);
		PyObject *ch = Sf_Entity(&f, charName);
		PyObject *next = Sf_OpI(&f, Sf_Attr(&f, ch, "ArriOpt"), '+', 1);
		PyObject *iopt = Sf_Op(&f, next, '%', Sf_Attr(&f, ch, "ArrnOpt"));
		PyObject *panels = Sf_Global(&f, "NewsPanel", "Panels");
		for (int i = 0;; i++) {
			int mark = f.n;
			PyObject *p = Sf_Next(&f, panels, i);
			if (!p) {
				break;
			}
			Sf_SetAttr(&f, p, "ArriOpt", iopt);
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* EndOfNews(): the end of the news on the monitor */
static PyObject *NewsPanel_EndOfNews(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, "NewsPanel", "ShowingNews", Sf_Int(&f, 0));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "NewsPanelShowHead", 0);
	return Sf_None(&f);
}

/* SwitchAlarm(status): with the alarm on the panels cannot be used and the news stop; it stays an alarm callback */
static PyObject *NewsPanel_SwitchAlarm(PyObject *self, PyObject *args) {
	PyObject *status;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &status)) {
		return NULL;
	}
	Sf_Import(&f, "Alarm");
	PyObject *list = Sf_Mod(&f, "Alarm", "SetAlarmCallback");
	PyObject *append = Sf_Attr(&f, list, "append");
	Sf_Call(&f, append, "(O)", Sf_Global(&f, "NewsPanel", "SwitchAlarm"));
	if (Sf_True(&f, status) > 0 && Sf_True(&f, Sf_Global(&f, "NewsPanel", "ShowingNews")) > 0) {
		Sf_Ask(&f, "Scrap", "Set", "(si)", "RemainTalkTime", 0);
	}
	int on = Sf_True(&f, status);
	PyObject *off = on >= 0 ? Sf_Int(&f, !on) : NULL;
	PyObject *panels = Sf_Global(&f, "NewsPanel", "Panels");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *kb = Sf_Next(&f, panels, i);
		if (!kb) {
			break;
		}
		Sf_SetAttr(&f, kb, "ArrowCanUse", off);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

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

/* newspanel.py:7, `for f in (("NewsLogoBmp", "2D/News/LogoNews.bmp"), ...): apply(Scrap.Set, f)` */
int NewsPanel_ModSettings(PyObject *module, PyObject *locals) {
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

PyMethodDef g_NewsPanelMethods[] = {
	{ "GetNews", NewsPanel_GetNews, 1, NULL },
	{ "FillNews", NewsPanel_FillNews, 1, NULL },
	{ "Init", NewsPanel_Init, 1, NULL },
	{ "UseNewsPanel", NewsPanel_UseNewsPanel, 1, NULL },
	{ "EndOfNews", NewsPanel_EndOfNews, 1, NULL },
	{ "SwitchAlarm", NewsPanel_SwitchAlarm, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
