/*
 * The building blocks of all menus, from the Menu module of the game in C (issue #2, step 4a): links, logos,
 * circuit, title bar, a new menu, tabs, the vertical menu, the question, the text box, slider and arrows. The fixed
 * layouts are data/menus/base.def; the functions here compute the places (with the integer and float arithmetic of
 * the script) and hand them to it.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static const char base_def[] = {
#embed "../../../data/menus/base.def"
	, 0 };

#define BASE_FILE "menus/base.def"
#define TAB_QUAD 14			/* Menu.TabQuadSize */

static void Base_TitleBar(int id, PyObject *y, const char *text, const char *font);
static void Base_Circuit(int id, byte drawBack);

static void Base_Link(int id, const char *a, const char *va, const char *b, const char *vb) {
	Script_SetStr(id, a, va, b);
	Script_SetStr(id, b, vb, a);
}

/* ---- the steps of base.def ---- */

/* titlebar y text font */
static void Step_TitleBar(int id, PyObject *args) {
	PyObject *y;
	char *text, *font;
	if (!PyArg_ParseTuple(args, "Oss", &y, &text, &font)) {
		PyErr_Print();
		return;
	}
	Base_TitleBar(id, y, text, font);
}

static void Step_Circuit(int id, PyObject *args) {
	(void)args;
	Base_Circuit(id, 1);
}

static const scriptStep_t base_steps[] = {
	{ "titlebar", Step_TitleBar },
	{ "circuit", Step_Circuit },
	{ NULL, NULL },
};

static void Base_Run(int id, const char *section, PyObject *vars) {
	Script_RunFile(id, BASE_FILE, section, base_def, base_steps, vars);
}

/* ---- the pieces ---- */

/* the bar behind a title, as wide as the text in whole quads of 18 */
static void Base_TitleBar(int id, PyObject *y, const char *text, const char *font) {
	float pos[2], size[2] = { 0.0f, 0.0f };
	CFont *f = Font_Get(font);
	if (f) {
		Font_TextSize(f, pos, size, 0.0f, 0.0f, "%s", text);
	}
	int width = (int)(size[0] / 18) * 18 + 36;
	PyObject *vars = Py_BuildValue("{s:O,s:i,s:i,s:i,s:i}", "y", y, "lx", 320 - width / 2 - 9, "mx", 320 - width / 2,
		"rx", 320 + width / 2, "scale", width / 18 * 2);
	Base_Run(id, "titlebar", vars);
	Py_XDECREF(vars);
}

/* the circuit; in the main menu the version, in a game the dark background */
static void Base_Circuit(int id, byte drawBack) {
	char version[0x40], *sp;
	byte inMain = Script_GetInt("Init", "inMainMenu", 0) != 0;
	byte mainMenu = (inMain || Script_GetInt("Init", "isShipEdit", 0)) && !Script_GetInt("Menu", "isXbox", 0);
	byte back = !inMain && drawBack;
	int marginH = Script_GetInt("Scorer", "MarginH", 0), marginV = Script_GetInt("Scorer", "MarginV", 0);

	Str_Printf(version, sizeof(version), "Version %s", Version_Get());
	if ((sp = strchr(version + 8, ' '))) {
		*sp = 0;
	}
	if (back) {
		GVar_SetNum("R_SceneBlurValue", 1);
	}
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:s,s:s,s:i,s:i,s:i,s:i,s:i}", "mainmenu", mainMenu, "demo",
		Script_GetInt("Init", "isDemo", 0) != 0, "version", version, "copyright",
		"2004 \xa9 Mercury Steam Entertainment SL.", "back", back, "left", marginH + 5, "right", 640 - marginH - 5,
		"bottom", 480 - marginV - 20, "bottom2", 480 - marginV - 40);
	Base_Run(id, "circuit", vars);
	Py_XDECREF(vars);
}

/* a new, empty menu with the cursor where it was; the effects of a game are off */
static void Base_NewMenu(int id) {
	byte had = Script_HasItem(id, "Cursor");
	double x = Script_GetNum(id, "Cursor", "X"), y = Script_GetNum(id, "Cursor", "Y");
	double visible = Script_GetNum(id, "Cursor", "Visible");
	cScorer *sc = Scorer_Get(id);

	if (sc) {
		cScorer_Clear(sc);
	}
	Base_Run(id, "cursor", NULL);
	Script_SetCursor(id, "Cursor");
	if (had) {
		Script_SetNum(id, "Cursor", "X", x);
		Script_SetNum(id, "Cursor", "Y", y);
		Script_SetNum(id, "Cursor", "Visible", visible);
	}
	PyObject *pitch = PyFloat_FromDouble(GVar_GetNum("S_PitchScale", 0));
	Script_SetVar("Menu", "currentSoundPitch", pitch);
	GVar_SetNum("S_PitchScale", 1.0);
	GVar_SetNum("NoiseFactor", 0);
	GVar_SetNum("MotionBlurFactorBegin", 0);
	GVar_SetNum("MotionBlurFactorEnd", 0.0);
	GVar_SetNum("MotionBlurTime", 0.0);
	GVar_SetNum("MotionBlurEndTime", 0.0);
	GVar_SetNum("MotionBlurOffVar", 0.0);
}

static void Base_TabDefaults(int id, const char *control) {
	PyObject *vars = Py_BuildValue("{s:s}", "control", control);
	Base_Run(id, "tabdefaults", vars);
	Py_XDECREF(vars);
}

/* a value of an entry as the scorer variable: a text or a number */
static void Base_SetObj(int id, const char *item, const char *var, PyObject *v) {
	if (v && PyString_Check(v)) {
		Script_SetStr(id, item, var, PyString_AsString(v));
	} else if (v && PyInt_Check(v)) {
		Script_SetNum(id, item, var, (double)PyInt_AsLong(v));
	} else if (v && PyFloat_Check(v)) {
		Script_SetNum(id, item, var, PyFloat_AsDouble(v));
	}
}

/* element i of a sequence as a string into out ("" when it is not a string) */
static const char *Base_Str(PyObject *seq, int i, char *out, int size) {
	PyObject *e = PySequence_GetItem(seq, i);
	out[0] = 0;
	if (e && PyString_Check(e)) {
		Str_Copy(out, PyString_AsString(e), size);
	}
	PyErr_Clear();
	Py_XDECREF(e);
	return out;
}

/* the name of entry i of a vertical menu or tabs: element k when the entry has more than two, else prefix + i+1 */
static const char *Base_EntryName(PyObject *menu, int i, int k, const char *prefix, char *out, int size) {
	PyObject *e = PySequence_GetItem(menu, i);
	if (e && PySequence_Length(e) > 2) {
		Base_Str(e, k, out, size);
	} else {
		Str_Printf(out, size, "%s%d", prefix, i + 1);
	}
	PyErr_Clear();
	Py_XDECREF(e);
	return out;
}

static double Base_Num(PyObject *v, byte *isInt) {
	*isInt = PyInt_Check(v);
	return PyInt_Check(v) ? (double)PyInt_AsLong(v) : PyFloat_Check(v) ? PyFloat_AsDouble(v) : 0.0;
}

/* ---- the functions of Menu ---- */

#define BASE_LINK(func, va, vb, clear) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		int id; \
		char *a, *b; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "iss", &id, &a, &b)) { \
			return NULL; \
		} \
		if (clear) { \
			Script_SetStr(id, a, va, ""); \
			Script_SetStr(id, b, vb, ""); \
		} else { \
			Base_Link(id, a, va, b, vb); \
		} \
		return Py_ReturnNone(); \
	}

BASE_LINK(Menu_LinkLR, "Right", "Left", 0)
BASE_LINK(Menu_LinkUD, "Down", "Up", 0)
BASE_LINK(Menu_UnLinkLR, "Right", "Left", 1)
BASE_LINK(Menu_UnLinkUD, "Down", "Up", 1)

/* DrawBackMainMenu(id) and the others: the logo, then the circuit */
#define BASE_BACK(func, section) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		int id; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "i", &id)) { \
			return NULL; \
		} \
		Base_Run(id, section, NULL); \
		Base_Circuit(id, 1); \
		return Py_ReturnNone(); \
	}

BASE_BACK(Menu_DrawBackMainMenu, "backmain")
BASE_BACK(Menu_DrawBackSubMenu, "backsub")
BASE_BACK(Menu_DrawBackOptionMenu, "backoption")

static PyObject *Menu_DrawCircuitMenu(PyObject *self, PyObject *args) {
	int id, drawBack = 1;
	(void)self;
	if (!PyArg_ParseTuple(args, "i|i", &id, &drawBack)) {
		return NULL;
	}
	Base_Circuit(id, drawBack != 0);
	return Py_ReturnNone();
}

/* DrawMenuTitleBar(id[, Y[, text[, font]]]) */
static PyObject *Menu_DrawMenuTitleBar(PyObject *self, PyObject *args) {
	int id;
	PyObject *y = NULL;
	char *text = "", *font = "ScrapBig";
	(void)self;
	if (!PyArg_ParseTuple(args, "i|Oss", &id, &y, &text, &font)) {
		return NULL;
	}
	PyObject *zero = y ? NULL : PyInt_FromLong(0);
	Base_TitleBar(id, y ? y : zero, text, font);
	Py_XDECREF(zero);
	return Py_ReturnNone();
}

static PyObject *Menu_StartNewMenu(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Base_NewMenu(id);
	return Py_ReturnNone();
}

/*
 * AddTabs(id, menu, actual[, ParamTabSize[, Hint[, CenterTextInTabs]]]): tabs above the menu, one per entry
 * [name, function, text]; actual is lit. The places use the integer division of the script.
 */
static PyObject *Menu_AddTabs(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "menu", "actual", "ParamTabSize", "Hint", "CenterTextInTabs", NULL };
	int id, tabSize = 17, hint = 1, center = 0;
	PyObject *menu;
	char *actual;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iOs|iii", names, &id, &menu, &actual, &tabSize, &hint, &center)) {
		return NULL;
	}
	const int sizeX = 37 * TAB_QUAD, sizeY = 26 * TAB_QUAD, tabX = 320 - sizeX / 2, tabY = 45;
	int n = PySequence_Length(menu);
	int current = 0;
	tabSize *= TAB_QUAD;
	if (n <= 0) {
		PyErr_SetString(PyExc_IndexError, "AddTabs: no tabs");
		return NULL;
	}
	if (hint == 1) {
		Script_Add(id, "TabInfo", "Tab");
		Base_TabDefaults(id, "TabInfo");
		Script_SetNum(id, "TabInfo", "X", tabX + TAB_QUAD / 4);
		Script_SetNum(id, "TabInfo", "Y", 240 + sizeY / 2 - 58);
		Script_SetNum(id, "TabInfo", "SizeX", sizeX - TAB_QUAD / 2);
		Script_SetNum(id, "TabInfo", "SizeY", TAB_QUAD * 3);
		Script_SetNum(id, "TabInfo", "TabInit", 0);
		Script_SetNum(id, "TabInfo", "TabEnd", 0);
		Script_SetNum(id, "TabInfo", "TabMax", 0);
		Script_SetNum(id, "TabInfo", "SizeTabQuad", 0);
		Script_SetNum(id, "TabInfo", "Type", 0);
		Script_SetNum(id, "TabInfo", "Alpha", 32);
		Script_SetNum(id, "TabInfo", "Red", 255);
		Script_SetNum(id, "TabInfo", "Green", 255);
		Script_SetNum(id, "TabInfo", "Blue", 32);
	}
	for (int i = 0; i < n; i++) {
		char name[0x40], tname[0x48], func[0x80], text[0x200], def[0x80];
		PyObject *e = PySequence_GetItem(menu, i);
		if (!e) {
			return NULL;
		}
		Base_EntryName(menu, i, 0, "Tab", name, sizeof(name));
		Base_Str(e, 1, func, sizeof(func));
		Base_Str(e, 2, text, sizeof(text));
		byte lit = PySequence_Length(e) > 0 && !strcmp(Base_Str(e, 0, def, sizeof(def)), actual);
		Py_DECREF(e);
		double left = 0.0;
		if (center) {
			float pos[2], size[2] = { 0.0f, 0.0f };
			Font_TextSize(Font_Get("ScrapMedium"), pos, size, 0.0f, 0.0f, "%s", text);
			left = tabX + i * (tabSize / 2) + (tabSize / 2 - size[0]) / 2;
		}
		Script_Add(id, name, "Text");
		Script_SetStr(id, name, "Text", text);
		Script_SetNum(id, name, "X", center ? left : tabX + i * (tabSize / 2) + 8);
		Script_SetNum(id, name, "Y", tabY - 9);
		Script_SetStr(id, name, "Font", "ScrapMedium");
		Script_SetStr(id, name, "OnAccept", func);
		if (center) {
			Script_SetNum(id, name, "FocusPivotX", -(left - (tabX + i * (tabSize / 2) + 8)));
		}
		Script_SetNum(id, name, "W", tabSize / 2 - 10);
		Script_SetNum(id, name, "H", TAB_QUAD * 3);
		Str_Copy(def, Script_GetDefault(id), sizeof(def));
		Script_SetStr(id, name, "Down", def);
		Script_SetStr(id, name, "Up", Script_GetStr(id, def, "Up"));
		Script_SetStr(id, name, "Effect", "Shadow");
		Script_SetNum(id, name, "Red", lit ? 255 : 255 / 1.5);
		Script_SetNum(id, name, "Green", lit ? 234 : 234 / 1.5);
		Script_SetNum(id, name, "Blue", lit ? 177 : 177 / 1.5);
		Str_Printf(tname, sizeof(tname), "T%s", name);
		Script_Add(id, tname, "Tab");
		Base_TabDefaults(id, tname);
		Script_SetNum(id, tname, "X", tabX);
		Script_SetNum(id, tname, "Y", tabY);
		Script_SetNum(id, tname, "SizeX", sizeX);
		Script_SetNum(id, tname, "SizeY", sizeY);
		Script_SetNum(id, tname, "TabInit", i * tabSize / TAB_QUAD);
		Script_SetNum(id, tname, "TabEnd", (i + 1) * tabSize / TAB_QUAD);
		Script_SetNum(id, tname, "TabMax", 4 * tabSize / TAB_QUAD);
		Script_SetNum(id, tname, "SizeTabQuad", 3);
		Script_SetNum(id, tname, "Type", lit ? 0 : 1);
		Script_SetNum(id, tname, "Alpha", lit ? 150 : 190);
		Script_SetNum(id, tname, "Red", lit ? 60 : 40);
		Script_SetNum(id, tname, "Green", lit ? 150 : 80);
		Script_SetNum(id, tname, "Blue", lit ? 200 : 150);
		if (lit) {
			current = i;
		}
	}
	char a[0x40], b[0x40], def[0x80], up[0x80];
	for (int i = 0; i < n - 1; i++) {
		Base_Link(id, Base_EntryName(menu, i, 0, "Tab", a, sizeof(a)), "Right",
			Base_EntryName(menu, i + 1, 0, "Tab", b, sizeof(b)), "Left");
	}
	Base_EntryName(menu, n - 1, 0, "Tab", a, sizeof(a));
	Base_EntryName(menu, 0, 0, "Tab", b, sizeof(b));
	Base_Link(id, a, "Right", b, "Left");
	Str_Copy(def, Script_GetDefault(id), sizeof(def));
	Str_Copy(up, Script_GetStr(id, def, "Up"), sizeof(up));
	Script_SetStr(id, up, "Down", b);
	Script_SetStr(id, def, "Up", b);
	char next[0x80], prev[0x80];
	PyObject *en = PySequence_GetItem(menu, (current + 1) % n);
	PyObject *ep = PySequence_GetItem(menu, current > 0 ? current - 1 : n - 1);
	Base_Str(en, 1, next, sizeof(next));
	Base_Str(ep, 1, prev, sizeof(prev));
	Py_XDECREF(en);
	Py_XDECREF(ep);
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, next);
		cPy_Function_SetFunc(&sc->onPrev, prev);
	}
	return Py_ReturnNone();
}

static PyObject *Menu_DummyFunc(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

/*
 * VerticalMenu(id, title, menu, backfunc[, XStart, VerticalStep, Font, MinYStart, YStart, FontTitle]): a column of
 * entries [text, function[, name[, OnDelete[, OnGainFocus]]]] under an optional title, linked round.
 */
static PyObject *Menu_VerticalMenu(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "title", "menu", "backfunc", "XStart", "VerticalStep", "Font", "MinYStart", "YStart",
		"FontTitle", NULL };
	int id;
	PyObject *title, *menu, *back, *xStartObj = NULL, *stepObj = NULL, *minObj = NULL, *yStart = NULL;
	char *font = "ScrapBig", *fontTitle = "ScrapBig";
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iOOO|OOsOOs", names, &id, &title, &menu, &back, &xStartObj, &stepObj,
		&font, &minObj, &yStart, &fontTitle)) {
		return NULL;
	}
	byte stepInt = 1, dummy;
	double xStart = xStartObj ? Base_Num(xStartObj, &dummy) : -1;
	double step = stepObj ? Base_Num(stepObj, &stepInt) : 45;
	double minY = minObj ? Base_Num(minObj, &dummy) : 100;
	int n = PySequence_Length(menu);
	double start;
	char backText[0x200];

	if (n <= 0) {
		PyErr_SetString(PyExc_IndexError, "VerticalMenu: no entries");
		return NULL;
	}
	if (!yStart || !PyObject_IsTrue(yStart)) {
		double half = stepInt ? (double)(((long)(n * step) + 20) / 2) : (n * step + 20) / 2;
		start = (double)(long)(240 - half);
	} else {
		start = Base_Num(yStart, &dummy);
	}
	if (start < minY) {
		start = minY;
	}
	if (minY < 0) {
		start = -minY;
	}
	byte hasTitle = PyObject_IsTrue(title) > 0;
	if (hasTitle) {
		PyObject *t = PyObject_Str(title);
		const char *tt = t ? PyString_AsString(t) : "";
		Script_Add(id, "Title", "Text");
		Script_SetStr(id, "Title", "Font", fontTitle);
		Base_SetObj(id, "Title", "Text", title);
		Script_SetNum(id, "Title", "X", 320);
		Script_SetNum(id, "Title", "Y", start);
		Script_SetStr(id, "Title", "Align", "Center");
		Script_SetNum(id, "Title", "W", 0);
		Script_SetNum(id, "Title", "H", 0);
		Script_SetNum(id, "Title", "Red", 255);
		Script_SetNum(id, "Title", "Green", 234);
		Script_SetNum(id, "Title", "Blue", 177);
		Script_SetStr(id, "Title", "Effect", "Shadow");
		PyObject *y = PyFloat_FromDouble(start + Script_GetInt("Menu", "TitleBarYOff", -2));
		Base_TitleBar(id, y, tt, fontTitle);
		Py_XDECREF(y);
		Py_XDECREF(t);
		start += step + 16;
	}
	Str_Copy(backText, Script_Lang("Menu_Back"), sizeof(backText));
	byte xbox = GVar_GetNum("IsXbox", 0) != 0;
	char last0[0x200];
	PyObject *le = PySequence_GetItem(menu, n - 1);
	Base_Str(le, 0, last0, sizeof(last0));
	Py_XDECREF(le);
	int count = n;
	if (xbox && !strcmp(last0, backText)) {
		/* the Xbox has a B button for Back instead of the entry */
		count = n - 1;
		Script_Add(id, "BackB", "Text");
		Script_SetStr(id, "BackB", "Font", "Horatio");
		char b[0x200];
		Str_Printf(b, sizeof(b), "\x15 %s", Script_Lang("XBox_Menu_Back"));
		Script_SetStr(id, "BackB", "Text", b);
		Script_SetNum(id, "BackB", "W", 0);
		Script_SetNum(id, "BackB", "H", 0);
		Script_SetNum(id, "BackB", "X", 320);
		Script_SetNum(id, "BackB", "Y", Script_GetInt("Menu", "BackButtonYStart", 430));
		Script_SetNum(id, "BackB", "CentralText", 1);
		Script_SetStr(id, "BackB", "Effect", "Shadow");
	}
	char iname[0x80] = "";
	char lastText[0x200] = "";
	for (int i = 0; i < count; i++) {
		PyObject *e = PySequence_GetItem(menu, i);
		if (!e) {
			return NULL;
		}
		int len = PySequence_Length(e);
		char func[0x80], s[0x80];
		Base_EntryName(menu, i, 2, "Item", iname, sizeof(iname));
		PyObject *text = PySequence_GetItem(e, 0);
		Base_Str(e, 1, func, sizeof(func));
		Script_Add(id, iname, "Text");
		Script_SetStr(id, iname, "Font", font);
		Base_SetObj(id, iname, "Text", text);
		Script_SetNum(id, iname, "X", 320);
		Script_SetNum(id, iname, "Y", start);
		Script_SetNum(id, iname, "CentralText", 1);
		Script_SetStr(id, iname, "OnAccept", func);
		if (len > 3) {
			Script_SetStr(id, iname, "OnDelete", Base_Str(e, 3, s, sizeof(s)));
			if (len > 4) {
				Script_SetStr(id, iname, "OnGainFocus", Base_Str(e, 4, s, sizeof(s)));
			}
		}
		Script_SetNum(id, iname, "Red", 183);
		Script_SetNum(id, iname, "Green", 220);
		Script_SetNum(id, iname, "Blue", 255);
		Script_SetStr(id, iname, "Effect", "Shadow");
		Script_SetStr(id, iname, "Hint", Script_Lang("HINT_DelKey"));
		Script_SetNum(id, iname, "MaxHintLineWidth", 640);
		if (strcmp(func, "!EditText")) {
			start += step;
		}
		if (xStart != -1) {
			Script_SetNum(id, iname, "X", xStart);
			Script_SetStr(id, iname, "Align", "Right");
			Script_SetNum(id, iname, "GetExtens", 1);
			Script_SetStr(id, iname, "Align", "Left");
		}
		lastText[0] = 0;
		if (text && PyString_Check(text)) {
			Str_Copy(lastText, PyString_AsString(text), sizeof(lastText));
		}
		PyErr_Clear();
		Py_XDECREF(text);
		Py_DECREF(e);
	}
	PyObject *last = PyString_FromString(iname);
	Script_SetVar("Menu", "VerticalMenuLastControl", last);
	/* Back always at the same place */
	if (!strcmp(lastText, backText) && !xbox) {
		Script_SetStr(id, iname, "Font", "ScrapBig");
		Script_SetNum(id, iname, "X", 320);
		Script_SetNum(id, iname, "Y", Script_GetInt("Menu", "BackButtonYStart", 430));
		Script_SetNum(id, iname, "CentralText", 1);
	}
	char a[0x80], b[0x80];
	for (int i = 0; i < count - 1; i++) {
		Base_Link(id, Base_EntryName(menu, i, 2, "Item", a, sizeof(a)), "Down",
			Base_EntryName(menu, i + 1, 2, "Item", b, sizeof(b)), "Up");
	}
	Base_EntryName(menu, count - 1, 2, "Item", a, sizeof(a));
	Base_EntryName(menu, 0, 2, "Item", b, sizeof(b));
	Base_Link(id, hasTitle ? a : iname, "Down", b, "Up");
	Script_SetDefault(id, b);
	cScorer *sc = Scorer_Get(id);
	if (sc && back && PyString_Check(back)) {
		cPy_Function_SetFunc(&sc->onCancel, PyString_AsString(back));
	}
	return Py_ReturnNone();
}

/* YesNoMenu(id, title, yes, no): a question with Yes and No */
static PyObject *Menu_YesNoMenu(PyObject *self, PyObject *args) {
	int id;
	PyObject *title;
	char *yes, *no;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOss", &id, &title, &yes, &no)) {
		return NULL;
	}
	Base_NewMenu(id);
	int y = Script_GetInt("Menu", "QuestionMenuYStart", 230);
	PyObject *vars = Py_BuildValue("{s:O,s:s,s:s,s:i,s:i,s:i}", "title", title, "yes", yes, "no", no, "y", y, "bary",
		y + Script_GetInt("Menu", "TitleBarYOff", -2), "exit", !strcmp(yes, "Menu.ExitGame"));
	Base_Run(id, "yesno", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

/* DisableMenuItem(id, n, count): entry n of a vertical menu grey, skipped by the keys (not the first or last) */
static PyObject *Menu_DisableMenuItem(PyObject *self, PyObject *args) {
	int id, n, count;
	char name[0x20], prev[0x20], next[0x20];
	(void)self;
	if (!PyArg_ParseTuple(args, "iii", &id, &n, &count)) {
		return NULL;
	}
	if (n < 2 || n > count - 1) {
		return Py_ReturnNone();
	}
	Str_Printf(name, sizeof(name), "Item%d", n);
	Str_Printf(prev, sizeof(prev), "Item%d", n - 1);
	Str_Printf(next, sizeof(next), "Item%d", n + 1);
	Script_SetNum(id, name, "Red", 128);
	Script_SetNum(id, name, "Green", 128);
	Script_SetNum(id, name, "Blue", 128);
	Script_SetNum(id, name, "W", 0);
	Script_SetNum(id, name, "H", 0);
	Script_SetStr(id, name, "OnAccept", "");
	Script_SetStr(id, prev, "Down", next);
	Script_SetStr(id, next, "Up", prev);
	return Py_ReturnNone();
}

/* ShowTextBox(id, desc, accept, back[, button]): a text in a box with one button under it */
static void Base_TextBox(int id, PyObject *desc, const char *accept, const char *back, PyObject *button) {
	char text[0x200];
	byte xbox = GVar_GetNum("IsXbox", 0) != 0;
	Base_NewMenu(id);
	if (!button || button == Py_None) {
		if (xbox) {
			const char *fmt = Script_Lang("XBox_Menu_A_To_Continue");
			const char *at = strstr(fmt, "%s");
			if (at) {
				Str_Printf(text, sizeof(text), "%.*s\x14%s", (int)(at - fmt), fmt, at + 2);
			} else {
				Str_Copy(text, fmt, sizeof(text));
			}
		} else {
			Str_Copy(text, Script_Lang("Menu_Back"), sizeof(text));
		}
		button = NULL;
	}
	PyObject *b = button ? button : PyString_FromString(text);
	if (button) {
		Py_INCREF(b);
	}
	PyObject *vars = Py_BuildValue("{s:O,s:s,s:s,s:O,s:i}", "desc", desc, "accept", accept, "back", back, "button", b,
		"xbox", xbox);
	Base_Run(id, "textbox", vars);
	Py_XDECREF(vars);
	Py_XDECREF(b);
}

static PyObject *Menu_ShowTextBox(PyObject *self, PyObject *args) {
	int id;
	PyObject *desc, *button = NULL;
	char *accept, *back;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOss|O", &id, &desc, &accept, &back, &button)) {
		return NULL;
	}
	Base_TextBox(id, desc, accept, back, button);
	return Py_ReturnNone();
}

/* ShowWarning(id, desc): a text box once the player plays (on foot or in a ship, time running), else later */
static PyObject *Menu_ShowWarning(PyObject *self, PyObject *args) {
	int id;
	PyObject *desc;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &desc)) {
		return NULL;
	}
	cActionSet *set = TABLE_AT(g_ActionSets, Input_GetActionSet());
	if ((ActionSet_IsNamed(set, "Walk") || ActionSet_IsNamed(set, "Racer")) && g_pWorld->timer.speed > 0.0f) {
		char back[0x80];
		Script_Call("Menu", "InitMenuSys", "(i)", 0);
		Script_GetString("Menu", "backFunc", back, sizeof(back));
		const char *func = back[0] ? back : "Menu.ExitMissionInfo";
		Base_TextBox(id, desc, func, func, NULL);
		return Py_ReturnNone();
	}
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *f = m ? PyObject_GetAttrString(m, "ShowWarning") : NULL;
	if (f) {
		Scheduler_Add(&g_pWorld->scheduler, Timer_GetTime(&g_pWorld->timer) + 0.25f, f, args, "ShowWarning");
	}
	PyErr_Clear();
	Py_XDECREF(f);
	Py_XDECREF(m);
	return Py_ReturnNone();
}

static PyObject *Menu_SetTabDefaults(PyObject *self, PyObject *args) {
	int id;
	char *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "is", &id, &control)) {
		return NULL;
	}
	Base_TabDefaults(id, control);
	return Py_ReturnNone();
}

/* SliderMenu(id, name, x, y) */
static PyObject *Menu_SliderMenu(PyObject *self, PyObject *args) {
	int id;
	char *name;
	PyObject *x, *y;
	(void)self;
	if (!PyArg_ParseTuple(args, "isOO", &id, &name, &x, &y)) {
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:s,s:O,s:O}", "name", name, "x", x, "y", y);
	Base_Run(id, "slider", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

/* PreviousNextMenu(id, name, x, y, size) and PreviousNextMenuSimple: arrows left and right of a value */
static PyObject *Base_Arrows(PyObject *args, const char *section) {
	int id;
	char *name;
	PyObject *x, *y, *size;
	if (!PyArg_ParseTuple(args, "isOOO", &id, &name, &x, &y, &size)) {
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:s,s:O,s:O,s:O}", "name", name, "x", x, "y", y, "size", size);
	Base_Run(id, section, vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *Menu_PreviousNextMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Base_Arrows(args, "prevnext");
}

static PyObject *Menu_PreviousNextMenuSimple(PyObject *self, PyObject *args) {
	(void)self;
	return Base_Arrows(args, "prevnextsimple");
}

PyMethodDef g_MenuBaseMethods[] = {
	{ "LinkLR", Menu_LinkLR, 1, NULL },
	{ "LinkUD", Menu_LinkUD, 1, NULL },
	{ "UnLinkLR", Menu_UnLinkLR, 1, NULL },
	{ "UnLinkUD", Menu_UnLinkUD, 1, NULL },
	{ "DrawBackMainMenu", Menu_DrawBackMainMenu, 1, NULL },
	{ "DrawBackSubMenu", Menu_DrawBackSubMenu, 1, NULL },
	{ "DrawBackOptionMenu", Menu_DrawBackOptionMenu, 1, NULL },
	{ "DrawCircuitMenu", Menu_DrawCircuitMenu, 1, NULL },
	{ "DrawMenuTitleBar", Menu_DrawMenuTitleBar, 1, NULL },
	{ "StartNewMenu", Menu_StartNewMenu, 1, NULL },
	{ "AddTabs", (PyCFunction)(void (*)(void))Menu_AddTabs, 3, NULL },
	{ "DummyFunc", Menu_DummyFunc, 1, NULL },
	{ "VerticalMenu", (PyCFunction)(void (*)(void))Menu_VerticalMenu, 3, NULL },
	{ "YesNoMenu", Menu_YesNoMenu, 1, NULL },
	{ "DisableMenuItem", Menu_DisableMenuItem, 1, NULL },
	{ "ShowTextBox", Menu_ShowTextBox, 1, NULL },
	{ "ShowWarning", Menu_ShowWarning, 1, NULL },
	{ "SetTabDefaults", Menu_SetTabDefaults, 1, NULL },
	{ "SliderMenu", Menu_SliderMenu, 1, NULL },
	{ "PreviousNextMenu", Menu_PreviousNextMenu, 1, NULL },
	{ "PreviousNextMenuSimple", Menu_PreviousNextMenuSimple, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
