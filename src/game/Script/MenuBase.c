/*
 * The building blocks of all menus, from the Menu module of the game in C (issue #2, step 4a): links, logos,
 * circuit, title bar, a new menu, tabs, the vertical menu, the question, the text box, slider and arrows. The fixed
 * layouts are data/menus/base.def; the functions here compute the places (with the integer and float arithmetic of
 * the script) and hand them to it.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters that
 * parse the arguments as before (the same TypeErrors) and take the objects (titles, menus, places) as native values.
 * A menu that holds handles or other objects is made native item by item (Mode_ElemsIn), the objects stay NV_OBJ. The
 * variables of other modules are read where the C read them (Init.inMainMenu, isShipEdit, isDemo, Menu.isXbox,
 * TitleBarYOff, BackButtonYStart, QuestionMenuYStart, backFunc, Scorer.MarginH and MarginV), Menu.currentSoundPitch
 * and VerticalMenuLastControl are written at once (OBJLIB 5). Menu.InitMenuSys is called through its ref (PORT_CALL:
 * directly while the attribute holds the port's function, else by name); ShowWarning schedules itself as a Python
 * function (OBJLIB 4); the .def runner takes Python values (OBJLIB 6).
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
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"

#include "game/Script/levels/Levels.h"

static const char base_def[] = {
#embed "../../../data/menus/base.def"
	, 0 };

#define BASE_FILE "menus/base.def"
#define TAB_QUAD 14			/* Menu.TabQuadSize */

static void Base_TitleBar(int id, const nativeVal_t *y, const char *text, const char *font);
static void Base_Circuit(int id, byte drawBack);

static void Base_Link(int id, const char *a, const char *va, const char *b, const char *vb) {
	Script_SetStr(id, a, va, b);
	Script_SetStr(id, b, vb, a);
}

/* ---- values ---- */

/* element i of a sequence as a string into out ("" when it is not a string; seq NULL gives "" too) */
static const char *Base_Str(const nativeVal_t *seq, int i, char *out, int size) {
	nativeVal_t tmp = { 0 };
	const nativeVal_t *e = seq ? Native_GetItem(seq, i, 0, &tmp) : NULL;
	out[0] = 0;
	if (e && e->type == NV_STR) {
		Str_Copy(out, Native_Text(e), size);
	}
	Native_Free(&tmp);
	/* OBJLIB 7: the IndexError or TypeError of an odd entry dropped */
	PyErr_Clear();
	return out;
}

/* element k of element i of a sequence as a string (Base_Str) */
static const char *Base_Str2(const nativeVal_t *seq, int i, int k, char *out, int size) {
	nativeVal_t tmp = { 0 };
	const nativeVal_t *e = Native_GetItem(seq, i, 0, &tmp);
	Base_Str(e, k, out, size);
	Native_Free(&tmp);
	return out;
}

/* the name of entry i of a vertical menu or tabs: element k when the entry has more than two, else prefix + i+1 */
static const char *Base_EntryName(const nativeVal_t *menu, int i, int k, const char *prefix, char *out, int size) {
	nativeVal_t tmp = { 0 };
	const nativeVal_t *e = Native_GetItem(menu, i, 0, &tmp);
	int n;
	if (e && Native_Len(e, &n) && n > 2) {
		Base_Str(e, k, out, size);
	} else {
		Str_Printf(out, size, "%s%d", prefix, i + 1);
	}
	Native_Free(&tmp);
	PyErr_Clear();
	return out;
}

static double Base_Num(const nativeVal_t *v, byte *isInt) {
	*isInt = v->type == NV_INT;
	return v->type == NV_INT ? (double)v->i : v->type == NV_FLOAT ? v->f : 0.0;
}

/* a value of an entry as the scorer variable: a text or a number (v NULL: nothing) */
static void Base_SetObj(int id, const char *item, const char *var, const nativeVal_t *v) {
	if (v && v->type == NV_STR) {
		Script_SetStr(id, item, var, Native_Text(v));
	} else if (v && v->type == NV_INT) {
		Script_SetNum(id, item, var, (double)v->i);
	} else if (v && v->type == NV_FLOAT) {
		Script_SetNum(id, item, var, v->f);
	}
}

/* str(v) of the script into b (Native_BufFree it); an object's str the Python way (OBJLIB 7), "" when it fails */
static const char *Base_Text(const nativeVal_t *v, nativeBuf_t *b) {
	Native_BufInit(b);
	if (v->type != NV_OBJ) {
		Native_Repr(b, v, NR_STR);
		return b->p;
	}
	PyObject *t = PyObject_Str(v->obj);
	if (t) {
		Native_BufAdd(b, PyString_AsString(t), PyString_Size(t));
		Py_DECREF(t);
	}
	return b->p;
}

/* ---- the steps of base.def ---- */

/* titlebar y text font */
static void Step_TitleBar(int id, PyObject *args) {
	PyObject *y;
	char *text, *font;
	/* OBJLIB 6: the steps take the arguments of the line as a tuple */
	if (!PyArg_ParseTuple(args, "Oss", &y, &text, &font)) {
		PyErr_Print();
		return;
	}
	nativeVal_t v;
	Native_Arg(y, &v);
	Base_TitleBar(id, &v, text, font);
	Native_Free(&v);
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

/* a section that stops where the script stops: 0 with the error set */
static byte Base_RunTry(int id, const char *section, PyObject *vars) {
	return Script_RunFileTry(id, BASE_FILE, section, base_def, base_steps, vars);
}

/* ---- the pieces ---- */

/* the bar behind a title, as wide as the text in whole quads of 18 */
static void Base_TitleBar(int id, const nativeVal_t *y, const char *text, const char *font) {
	float pos[2], size[2] = { 0.0f, 0.0f };
	CFont *f = Font_Get(font);
	if (f) {
		Font_TextSize(f, pos, size, 0.0f, 0.0f, "%s", text);
	}
	int width = (int)(size[0] / 18) * 18 + 36;
	/* OBJLIB 6: the $names of the section as a dict */
	PyObject *py = Native_ToPy(y);
	PyObject *vars = py ? Py_BuildValue("{s:O,s:i,s:i,s:i,s:i}", "y", py, "lx", 320 - width / 2 - 9, "mx",
		320 - width / 2, "rx", 320 + width / 2, "scale", width / 18 * 2) : NULL;
	Py_XDECREF(py);
	Base_Run(id, "titlebar", vars);
	Py_XDECREF(vars);
}

/* the circuit; in the main menu the version, in a game the dark background */
static void Base_Circuit(int id, byte drawBack) {
	char version[0x40], *sp;
	byte inMain = Mode_ModGetI("Init", "inMainMenu", 0) != 0;
	byte mainMenu = (inMain || Mode_ModGetI("Init", "isShipEdit", 0)) && !Mode_ModGetI("Menu", "isXbox", 0);
	byte back = !inMain && drawBack;
	int marginH = Mode_ModGetI("Scorer", "MarginH", 0), marginV = Mode_ModGetI("Scorer", "MarginV", 0);

	Str_Printf(version, sizeof(version), "Version %s", Version_Get());
	if ((sp = strchr(version + 8, ' '))) {
		*sp = 0;
	}
	if (back) {
		GVar_SetNum("R_SceneBlurValue", 1);
	}
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:s,s:s,s:i,s:i,s:i,s:i,s:i}", "mainmenu", mainMenu, "demo",
		Mode_ModGetI("Init", "isDemo", 0) != 0, "version", version, "copyright",
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
	nativeVal_t pitch;

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
	Native_SetFloat(&pitch, GVar_GetNum("S_PitchScale", 0));
	Mode_ModPut("Menu", "currentSoundPitch", &pitch);
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

/*
 * AddTabs(id, menu, actual[, ParamTabSize[, Hint[, CenterTextInTabs]]]): tabs above the menu, one per entry
 * [name, function, text]; actual is lit. The places use the integer division of the script. 0 with the error set
 */
static byte Base_AddTabs(int id, const nativeVal_t *menu, const char *actual, int tabSize, int hint, int center) {
	const int sizeX = 37 * TAB_QUAD, sizeY = 26 * TAB_QUAD, tabX = 320 - sizeX / 2, tabY = 45;
	int n = Mode_SeqLen(menu);
	int current = 0;
	tabSize *= TAB_QUAD;
	if (n <= 0) {
		return Native_Raise(NE_INDEX, "AddTabs: no tabs");
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
		nativeVal_t tmp = { 0 };
		const nativeVal_t *e = Native_GetItem(menu, i, 0, &tmp);
		if (!e) {
			return 0;
		}
		int len;
		Base_EntryName(menu, i, 0, "Tab", name, sizeof(name));
		Base_Str(e, 1, func, sizeof(func));
		Base_Str(e, 2, text, sizeof(text));
		byte lit = Native_Len(e, &len) && len > 0 && !strcmp(Base_Str(e, 0, def, sizeof(def)), actual);
		Native_Free(&tmp);
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
	Base_Str2(menu, (current + 1) % n, 1, next, sizeof(next));
	Base_Str2(menu, current > 0 ? current - 1 : n - 1, 1, prev, sizeof(prev));
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, next);
		cPy_Function_SetFunc(&sc->onPrev, prev);
	}
	return 1;
}

/*
 * VerticalMenu(id, title, menu, backfunc[, XStart, VerticalStep, Font, MinYStart, YStart, FontTitle]): a column of
 * entries [text, function[, name[, OnDelete[, OnGainFocus]]]] under an optional title, linked round. The optional
 * values are NULL when not given. 0 with the error set
 */
static byte Base_VerticalMenu(int id, const nativeVal_t *title, const nativeVal_t *menu, const nativeVal_t *back,
	const nativeVal_t *xStartV, const nativeVal_t *stepV, const char *font, const nativeVal_t *minV,
	const nativeVal_t *yStart, const char *fontTitle) {
	byte stepInt = 1, dummy;
	double xStart = xStartV ? Base_Num(xStartV, &dummy) : -1;
	double step = stepV ? Base_Num(stepV, &stepInt) : 45;
	double minY = minV ? Base_Num(minV, &dummy) : 100;
	int n = Mode_SeqLen(menu);
	double start;
	char backText[0x200];

	if (n < 0) {
		return 0;
	}
	if (!yStart || !Native_True(yStart)) {
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
	byte hasTitle = Native_True(title);
	if (hasTitle) {
		nativeBuf_t tb;
		nativeVal_t y;
		const char *tt = Base_Text(title, &tb);
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
		Native_SetFloat(&y, start + Mode_ModGetI("Menu", "TitleBarYOff", -2));
		Base_TitleBar(id, &y, tt, fontTitle);
		Native_BufFree(&tb);
		start += step + 16;
	}
	/* the script reads the last entry after the title */
	if (n <= 0) {
		return Native_Raise(NE_INDEX, menu->type == NV_LIST ? "list index out of range" : "tuple index out of range");
	}
	Str_Copy(backText, Script_Lang("Menu_Back"), sizeof(backText));
	byte xbox = GVar_GetNum("IsXbox", 0) != 0;
	char last0[0x200];
	Base_Str2(menu, n - 1, 0, last0, sizeof(last0));
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
		Script_SetNum(id, "BackB", "Y", Mode_ModGetI("Menu", "BackButtonYStart", 430));
		Script_SetNum(id, "BackB", "CentralText", 1);
		Script_SetStr(id, "BackB", "Effect", "Shadow");
	}
	char iname[0x80] = "";
	char lastText[0x200] = "";
	for (int i = 0; i < count; i++) {
		nativeVal_t etmp = { 0 }, ttmp = { 0 };
		const nativeVal_t *e = Native_GetItem(menu, i, 0, &etmp);
		if (!e) {
			return 0;
		}
		int len = Mode_SeqLen(e);
		char func[0x80], s[0x80];
		Base_EntryName(menu, i, 2, "Item", iname, sizeof(iname));
		const nativeVal_t *text = Native_GetItem(e, 0, 0, &ttmp);
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
		if (text && text->type == NV_STR) {
			Str_Copy(lastText, Native_Text(text), sizeof(lastText));
		}
		PyErr_Clear();
		Native_Free(&ttmp);
		Native_Free(&etmp);
	}
	nativeVal_t last;
	Native_SetStr(&last, iname);
	Mode_ModPut("Menu", "VerticalMenuLastControl", &last);
	/* Back always at the same place */
	if (!strcmp(lastText, backText) && !xbox) {
		Script_SetStr(id, iname, "Font", "ScrapBig");
		Script_SetNum(id, iname, "X", 320);
		Script_SetNum(id, iname, "Y", Mode_ModGetI("Menu", "BackButtonYStart", 430));
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
	if (sc && back && back->type == NV_STR) {
		cPy_Function_SetFunc(&sc->onCancel, Native_Text(back));
	}
	return 1;
}

/* ShowTextBox(id, desc, accept, back[, button]): a text in a box with one button under it (button NULL or None: Back,
 * or the A button of the Xbox); 0 with the error of the section */
static byte Base_TextBox(int id, const nativeVal_t *desc, const char *accept, const char *back, const nativeVal_t *button) {
	char text[0x200];
	byte xbox = GVar_GetNum("IsXbox", 0) != 0;
	Base_NewMenu(id);
	if (!button || button->type == NV_NONE) {
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
	PyObject *d = Native_ToPy(desc);
	PyObject *b = button ? Native_ToPy(button) : PyString_FromString(text);
	PyObject *vars = d && b ? Py_BuildValue("{s:O,s:s,s:s,s:O,s:i}", "desc", d, "accept", accept, "back", back,
		"button", b, "xbox", xbox) : NULL;
	byte ok = vars && Base_RunTry(id, "textbox", vars);
	Py_XDECREF(vars);
	Py_XDECREF(b);
	Py_XDECREF(d);
	return ok;
}

/*
 * ShowWarning(id, desc): a text box once the player plays (on foot or in a ship, time running): 1, 0 with the error;
 * 2 when the player does not play yet (the adapter schedules the call again)
 */
static int Base_ShowWarning(int id, const nativeVal_t *desc) {
	cActionSet *set = TABLE_AT(g_ActionSets, Input_GetActionSet());
	if (!(ActionSet_IsNamed(set, "Walk") || ActionSet_IsNamed(set, "Racer")) || g_pWorld->timer.speed <= 0.0f) {
		return 2;
	}
	char back[0x80];
	nativeVal_t bf;
	if (!PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) || !Mode_G("Menu", "backFunc", &bf)) {
		return 0;
	}
	back[0] = 0;
	if (bf.type == NV_STR) {
		Str_Copy(back, Native_Text(&bf), sizeof(back));
	}
	Native_Free(&bf);
	const char *func = back[0] ? back : "Menu.ExitMissionInfo";
	return Base_TextBox(id, desc, func, func, NULL);
}

/* the section of the arrows or the slider with the $names name, x, y (and size: NULL for none) */
static void Base_Place(int id, const char *section, const char *name, const nativeVal_t *x, const nativeVal_t *y,
	const nativeVal_t *size) {
	PyObject *px = Native_ToPy(x), *py = px ? Native_ToPy(y) : NULL, *ps = py && size ? Native_ToPy(size) : NULL;
	PyObject *vars = !py || (size && !ps) ? NULL : size ? Py_BuildValue("{s:s,s:O,s:O,s:O}", "name", name, "x", px,
		"y", py, "size", ps) : Py_BuildValue("{s:s,s:O,s:O}", "name", name, "x", px, "y", py);
	Base_Run(id, section, vars);
	Py_XDECREF(vars);
	Py_XDECREF(ps);
	Py_XDECREF(py);
	Py_XDECREF(px);
}

/* ---- the functions of Menu (adapters) ---- */

#define BASE_LINK(name, va, vb, clear) \
	static byte Menu_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		int id; \
		const char *x, *y; \
		(void)out; \
		if (!Mode_Parse(a, n, "iss", &id, &x, &y)) { \
			return 0; \
		} \
		if (clear) { \
			Script_SetStr(id, x, va, ""); \
			Script_SetStr(id, y, vb, ""); \
		} else { \
			Base_Link(id, x, va, y, vb); \
		} \
		return 1; \
	} \
	MODE_ADAPTER(Menu, name)

BASE_LINK(LinkLR, "Right", "Left", 0)
BASE_LINK(LinkUD, "Down", "Up", 0)
BASE_LINK(UnLinkLR, "Right", "Left", 1)
BASE_LINK(UnLinkUD, "Down", "Up", 1)

/* DrawBackMainMenu(id) and the others: the logo, then the circuit */
#define BASE_BACK(name, section) \
	static byte Menu_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		int id; \
		(void)out; \
		if (!Mode_Parse(a, n, "i", &id)) { \
			return 0; \
		} \
		Base_Run(id, section, NULL); \
		Base_Circuit(id, 1); \
		return 1; \
	} \
	MODE_ADAPTER(Menu, name)

BASE_BACK(DrawBackMainMenu, "backmain")
BASE_BACK(DrawBackSubMenu, "backsub")
BASE_BACK(DrawBackOptionMenu, "backoption")

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
static byte Menu_DrawMenuTitleBar_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	const nativeVal_t *y = &zero;
	const char *text = "", *font = "ScrapBig";
	(void)out;
	if (!Mode_Parse(a, n, "i|Oss", &id, &y, &text, &font)) {
		return 0;
	}
	Base_TitleBar(id, y, text, font);
	return 1;
}
MODE_ADAPTER(Menu, DrawMenuTitleBar)

static byte Menu_StartNewMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Base_NewMenu(id);
	return 1;
}
MODE_ADAPTER(Menu, StartNewMenu)

static PyObject *Menu_AddTabs(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "menu", "actual", "ParamTabSize", "Hint", "CenterTextInTabs", NULL };
	int id, tabSize = 17, hint = 1, center = 0;
	PyObject *menu;
	char *actual;
	nativeVal_t m;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iOs|iii", names, &id, &menu, &actual, &tabSize, &hint, &center)) {
		return NULL;
	}
	Native_Arg(menu, &m);
	Mode_ElemsIn(&m);
	byte ok = Base_AddTabs(id, &m, actual, tabSize, hint, center);
	Native_Free(&m);
	return Mode_Result(ok);
}

static PyObject *Menu_VerticalMenu(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "title", "menu", "backfunc", "XStart", "VerticalStep", "Font", "MinYStart", "YStart",
		"FontTitle", NULL };
	int id;
	PyObject *o[7] = { NULL };
	char *font = "ScrapBig", *fontTitle = "ScrapBig";
	nativeVal_t v[7];
	(void)self;
	/* title, menu, backfunc, XStart, VerticalStep, MinYStart, YStart */
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iOOO|OOsOOs", names, &id, &o[0], &o[1], &o[2], &o[3], &o[4], &font,
		&o[5], &o[6], &fontTitle)) {
		return NULL;
	}
	for (int i = 0; i < 7; i++) {
		if (o[i]) {
			Native_Arg(o[i], &v[i]);
		} else {
			Native_SetNone(&v[i]);
		}
	}
	Mode_ElemsIn(&v[1]);
	byte ok = Base_VerticalMenu(id, &v[0], &v[1], &v[2], o[3] ? &v[3] : NULL, o[4] ? &v[4] : NULL, font,
		o[5] ? &v[5] : NULL, o[6] ? &v[6] : NULL, fontTitle);
	for (int i = 0; i < 7; i++) {
		Native_Free(&v[i]);
	}
	return Mode_Result(ok);
}

/* YesNoMenu(id, title, yes, no): a question with Yes and No */
static byte Menu_YesNoMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *title;
	const char *yes, *no;
	(void)out;
	if (!Mode_Parse(a, n, "iOss", &id, &title, &yes, &no)) {
		return 0;
	}
	Base_NewMenu(id);
	int y = Mode_ModGetI("Menu", "QuestionMenuYStart", 230);
	PyObject *pt = Native_ToPy(title);
	PyObject *vars = pt ? Py_BuildValue("{s:O,s:s,s:s,s:i,s:i,s:i}", "title", pt, "yes", yes, "no", no, "y", y,
		"bary", y + (int)Mode_ModGetI("Menu", "TitleBarYOff", -2), "exit", !strcmp(yes, "Menu.ExitGame")) : NULL;
	Py_XDECREF(pt);
	Base_Run(id, "yesno", vars);
	Py_XDECREF(vars);
	return 1;
}
MODE_ADAPTER(Menu, YesNoMenu)

/* DisableMenuItem(id, n, count): entry n of a vertical menu grey, skipped by the keys (not the first or last) */
static byte Menu_DisableMenuItem_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, item, count;
	char name[0x20], prev[0x20], next[0x20];
	(void)out;
	if (!Mode_Parse(a, n, "iii", &id, &item, &count)) {
		return 0;
	}
	if (item < 2 || item > count - 1) {
		return 1;
	}
	Str_Printf(name, sizeof(name), "Item%d", item);
	Str_Printf(prev, sizeof(prev), "Item%d", item - 1);
	Str_Printf(next, sizeof(next), "Item%d", item + 1);
	Script_SetNum(id, name, "Red", 128);
	Script_SetNum(id, name, "Green", 128);
	Script_SetNum(id, name, "Blue", 128);
	Script_SetNum(id, name, "W", 0);
	Script_SetNum(id, name, "H", 0);
	Script_SetStr(id, name, "OnAccept", "");
	Script_SetStr(id, prev, "Down", next);
	Script_SetStr(id, next, "Up", prev);
	return 1;
}
MODE_ADAPTER(Menu, DisableMenuItem)

static byte Menu_ShowTextBox_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *desc, *button = NULL;
	const char *accept, *back;
	(void)out;
	return Mode_Parse(a, n, "iOss|O", &id, &desc, &accept, &back, &button) && Base_TextBox(id, desc, accept, back, button);
}
MODE_ADAPTER(Menu, ShowTextBox)

static byte Menu_ShowWarning_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *desc;
	(void)out;
	if (!Mode_Parse(a, n, "iO", &id, &desc)) {
		return 0;
	}
	int r = Base_ShowWarning(id, desc);
	if (r != 2) {
		return (byte)r;
	}
	/* OBJLIB 4: the scheduler keeps the function of the module and the arguments (id, desc) as Python objects */
	nativeVal_t t;
	Native_SetEmpty(&t, NV_TUPLE);
	Native_Copy(Native_Append(&t), &a[0]);
	Native_Copy(Native_Append(&t), &a[1]);
	PyObject *args = Native_ToPy(&t);
	Native_Free(&t);
	if (args) {
		Mode_SchedPy(Timer_GetTime(&g_pWorld->timer) + 0.25f, "Menu", "ShowWarning", args, "ShowWarning");
		Py_DECREF(args);
	}
	return 1;
}
MODE_ADAPTER(Menu, ShowWarning)

static byte Menu_SetTabDefaults_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const char *control;
	(void)out;
	if (!Mode_Parse(a, n, "is", &id, &control)) {
		return 0;
	}
	Base_TabDefaults(id, control);
	return 1;
}
MODE_ADAPTER(Menu, SetTabDefaults)

/* SliderMenu(id, name, x, y) */
static PyObject *Menu_SliderMenu(PyObject *self, PyObject *args) {
	int id;
	char *name;
	PyObject *x, *y;
	nativeVal_t vx, vy;
	(void)self;
	if (!PyArg_ParseTuple(args, "isOO", &id, &name, &x, &y)) {
		return NULL;
	}
	Native_Arg(x, &vx);
	Native_Arg(y, &vy);
	Base_Place(id, "slider", name, &vx, &vy, NULL);
	Native_Free(&vx);
	Native_Free(&vy);
	return Py_ReturnNone();
}

/* PreviousNextMenu(id, name, x, y, size) and PreviousNextMenuSimple: arrows left and right of a value */
static PyObject *Base_Arrows(PyObject *args, const char *section) {
	int id;
	char *name;
	PyObject *o[3];
	nativeVal_t v[3];
	if (!PyArg_ParseTuple(args, "isOOO", &id, &name, &o[0], &o[1], &o[2])) {
		return NULL;
	}
	for (int i = 0; i < 3; i++) {
		Native_Arg(o[i], &v[i]);
	}
	Base_Place(id, section, name, &v[0], &v[1], &v[2]);
	for (int i = 0; i < 3; i++) {
		Native_Free(&v[i]);
	}
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
	{ "DummyFunc", Mode_PyNone, 1, NULL },
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

PORTS(MenuBase)
