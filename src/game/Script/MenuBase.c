/*
 * The building blocks of all menus, from the Menu module of the game in C (issue #2, step 4a): links, logos,
 * circuit, title bar, a new menu, tabs, the vertical menu, the question, the text box, slider and arrows. The fixed
 * layouts are data/menus/base.def; the functions here compute the places (with the integer and float arithmetic of
 * the script) and hand them to it.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the function table rows name the entries,
 * which parse the arguments as before (the same TypeErrors) and take the objects (titles, menus, places) as native
 * values. A menu that holds handles or other objects is made native item by item (Mode_ElemsIn), the objects stay
 * NV_OBJ. The variables of other modules are read where the C read them (Init.inMainMenu, isShipEdit, isDemo,
 * Menu.isXbox, TitleBarYOff, BackButtonYStart, QuestionMenuYStart, backFunc, Scorer.MarginH and MarginV),
 * Menu.currentSoundPitch and VerticalMenuLastControl are written at once. Menu.InitMenuSys is called through its ref
 * (PORT_CALL: directly while the attribute holds the port's function, else by name); ShowWarning schedules itself, the
 * scheduler takes its entry (stage 4: Mode_SchedAt, Port_Scheduled).
 */
#include <string.h>

#include "scrap/coop.h"
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
	Native_ErrClear();
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
	Native_ErrClear();
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

/* str(v) of the script into b (Native_BufFree it; Mode_StrBuf), "" when it fails */
static const char *Base_Text(const nativeVal_t *v, nativeBuf_t *b) {
	Native_BufInit(b);
	Mode_StrBuf(v, b);
	return b->p;
}

/* ---- the steps of base.def ---- */

/* titlebar y text font */
static void Step_TitleBar(int id, const nativeVal_t *args, int n) {
	const nativeVal_t *y;
	const char *text, *font;
	nativeVal_t v;
	if (!Mode_Parse(args, n, "Oss", &y, &text, &font) || !Native_Pass(y, &v)) {
		Native_ErrPrint();
		return;
	}
	Base_TitleBar(id, &v, text, font);
	Native_Free(&v);
}

static void Step_Circuit(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	Base_Circuit(id, 1);
}

static const scriptStep_t base_steps[] = {
	{ "titlebar", Step_TitleBar },
	{ "circuit", Step_Circuit },
	{ NULL, NULL },
};

/* a section of base.def with its $names: n pairs of a name and a value (Mode_DefRun) */
#define Base_Run(id, section, n, ...) \
	Mode_DefRun(id, BASE_FILE, section, base_def, base_steps, 0, n __VA_OPT__(, ) __VA_ARGS__)
/* a section that stops where the script stops: 0 with the error set */
#define Base_RunTry(id, section, n, ...) \
	Mode_DefRun(id, BASE_FILE, section, base_def, base_steps, 1, n __VA_OPT__(, ) __VA_ARGS__)

/* ---- the pieces ---- */

/* the bar behind a title, as wide as the text in whole quads of 18 */
static void Base_TitleBar(int id, const nativeVal_t *y, const char *text, const char *font) {
	float pos[2], size[2] = { 0.0f, 0.0f };
	CFont *f = Font_Get(font);
	if (f) {
		Font_TextSize(f, pos, size, 0.0f, 0.0f, "%s", text);
	}
	int width = (int)(size[0] / 18) * 18 + 36;
	modeVals_t t = { .n = 0 };
	Base_Run(id, "titlebar", 5, "y", y, "lx", Mode_I(&t, 320 - width / 2 - 9), "mx", Mode_I(&t, 320 - width / 2), "rx",
		Mode_I(&t, 320 + width / 2), "scale", Mode_I(&t, width / 18 * 2));
	Mode_Done(&t, 1);
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
	modeVals_t t = { .n = 0 };
	Base_Run(id, "circuit", 9, "mainmenu", Mode_I(&t, mainMenu), "demo",
		Mode_I(&t, Mode_ModGetI("Init", "isDemo", 0) != 0), "version", Mode_S(&t, version), "copyright", Mode_S(&t, "2004 \xa9 Mercury Steam Entertainment SL."), "back",
		Mode_I(&t, back), "left", Mode_I(&t, marginH + 5), "right", Mode_I(&t, 640 - marginH - 5), "bottom",
		Mode_I(&t, 480 - marginV - 20), "bottom2", Mode_I(&t, 480 - marginV - 40));
	Mode_Done(&t, 1);
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
	Base_Run(id, "cursor", 0);
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
	nativeVal_t c;
	Native_SetText(&c, control, (int)strlen(control));
	Base_Run(id, "tabdefaults", 1, "control", &c);
	Native_Free(&c);
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
		Native_ErrClear();
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
	modeVals_t t = { .n = 0 };
	byte ok = Base_RunTry(id, "textbox", 5, "desc", desc, "accept", Mode_S(&t, accept), "back", Mode_S(&t, back),
		"button", button ? button : Mode_S(&t, text), "xbox", Mode_I(&t, xbox));
	return Mode_Done(&t, ok);
}

/*
 * ShowWarning(id, desc): a text box once the player plays (on foot or in a ship, time running): 1, 0 with the error;
 * 2 when the player does not play yet (the entry schedules the call again)
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
	nativeVal_t nm;
	Native_SetText(&nm, name, (int)strlen(name));
	/* without a size the name "size" is not given at all (the last pair is not counted) */
	Base_Run(id, section, size ? 4 : 3, "name", &nm, "x", x, "y", y, "size", size);
	Native_Free(&nm);
}

/* ---- the functions of Menu (entries) ---- */

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
	}

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
		Base_Run(id, section, 0); \
		Base_Circuit(id, 1); \
		return 1; \
	}

BASE_BACK(DrawBackMainMenu, "backmain")
BASE_BACK(DrawBackSubMenu, "backsub")
BASE_BACK(DrawBackOptionMenu, "backoption")

static byte Menu_DrawCircuitMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, drawBack = 1;
	(void)out;
	if (!Mode_Parse(a, n, "i|i", &id, &drawBack)) {
		return 0;
	}
	Base_Circuit(id, drawBack != 0);
	return 1;
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

static byte Menu_StartNewMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Base_NewMenu(id);
	return 1;
}

/* AddTabs(id, menu, actual, ParamTabSize=17, Hint=1, CenterTextInTabs=0) */
MODE_KWT(Menu, AddTabs, "iOs|iii", "id", "menu", "actual", "ParamTabSize", "Hint", "CenterTextInTabs") {
	const nativeVal_t *menu;
	const char *actual;
	int id, tabSize, hint, center;
	nativeVal_t m;
	Mode_KwDefI(v, given, 3, 17);
	Mode_KwDefI(v, given, 4, 1);
	Mode_KwDefI(v, given, 5, 0);
	if (!Mode_Parse(v, 6, "iOsiii", &id, &menu, &actual, &tabSize, &hint, &center)) {
		return 0;
	}
	/* the menu list is read in place (Mode_ElemsIn): a copy */
	Native_Copy(&m, menu);
	Mode_ElemsIn(&m);
	byte ok = Base_AddTabs(id, &m, actual, tabSize, hint, center);
	Native_Free(&m);
	return ok;
}

/* the keyword names of VerticalMenu (its ref takes keyword calls, PORT_DEFK) */
static char *Menu_VerticalMenu_N[] = { "id", "title", "menu", "backfunc", "XStart", "VerticalStep", "Font", "MinYStart",
	"YStart", "FontTitle", NULL };
static const char Menu_VerticalMenu_F[] = "iOOO|OOsOOs";

/* the entry of VerticalMenu: the slots of "iOOO|OOsOOs" (the arity errors of getargs.c Arg_ParseTupleAndKeywords), then
 * the id and the fonts converted in slot order with the errors of "i" and "s" (the fonts "ScrapBig" when not given) */
static byte Menu_VerticalMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t v[10], menu;
	byte given[10];
	int id;
	const nativeVal_t *o[7];
	const char *font, *fontTitle;
	(void)out;
	if (!Mode_KwSlots(a, n, "OOOO|OOOOOO", v, given)) {
		return 0;
	}
	Mode_KwDefS(v, given, 6, "ScrapBig");
	Mode_KwDefS(v, given, 9, "ScrapBig");
	if (!Mode_Parse(v, 10, "iOOOOOsOOs", &id, &o[0], &o[1], &o[2], &o[3], &o[4], &font, &o[5], &o[6], &fontTitle)) {
		return 0;
	}
	/* the menu list is read in place (Mode_ElemsIn): a copy */
	Native_Copy(&menu, o[1]);
	Mode_ElemsIn(&menu);
	byte ok = Base_VerticalMenu(id, o[0], &menu, o[2], given[4] ? o[3] : NULL, given[5] ? o[4] : NULL, font,
		given[7] ? o[5] : NULL, given[8] ? o[6] : NULL, fontTitle);
	Native_Free(&menu);
	return ok;
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
	/* not in the original: a question of the coop story's scripts goes to the member (include/scrap/coop.h) */
	if (Coop_MenuYesNo(title->type == NV_STR ? Native_Text(title) : "", yes, no)) {
		return 1;
	}
	Base_NewMenu(id);
	int y = Mode_ModGetI("Menu", "QuestionMenuYStart", 230);
	modeVals_t t = { .n = 0 };
	Base_Run(id, "yesno", 6, "title", title, "yes", Mode_S(&t, yes), "no", Mode_S(&t, no), "y", Mode_I(&t, y), "bary",
		Mode_I(&t, y + (int)Mode_ModGetI("Menu", "TitleBarYOff", -2)), "exit",
		Mode_I(&t, !strcmp(yes, "Menu.ExitGame")));
	return Mode_Done(&t, 1);
}

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

static byte Menu_ShowTextBox_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *desc, *button = NULL;
	const char *accept, *back;
	(void)out;
	return Mode_Parse(a, n, "iOss|O", &id, &desc, &accept, &back, &button) && Base_TextBox(id, desc, accept, back, button);
}

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
	/* the function of the module with the arguments (id, desc) as given: the scheduler takes the entry while it is
	 * the port's own function (Port_Scheduled) */
	Mode_SchedAt(Timer_GetTime(&g_pWorld->timer) + 0.25f, "Menu", "ShowWarning", a, 2, "ShowWarning");
	return 1;
}

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

/* SliderMenu(id, name, x, y) */
static byte Menu_SliderMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const char *name;
	const nativeVal_t *x, *y;
	(void)out;
	if (!Mode_Parse(a, n, "isOO", &id, &name, &x, &y)) {
		return 0;
	}
	Base_Place(id, "slider", name, x, y, NULL);
	return 1;
}

/* PreviousNextMenu(id, name, x, y, size) and PreviousNextMenuSimple: arrows left and right of a value */
static byte Base_Arrows(const nativeVal_t *a, int n, const char *section) {
	int id;
	const char *name;
	const nativeVal_t *o[3];
	if (!Mode_Parse(a, n, "isOOO", &id, &name, &o[0], &o[1], &o[2])) {
		return 0;
	}
	Base_Place(id, section, name, o[0], o[1], o[2]);
	return 1;
}

static byte Menu_PreviousNextMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Base_Arrows(a, n, "prevnext");
}

static byte Menu_PreviousNextMenuSimple_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Base_Arrows(a, n, "prevnextsimple");
}

const portFn_t g_MenuBaseMethods[] = {
	PORT_FN(LinkLR, Menu_LinkLR),
	PORT_FN(LinkUD, Menu_LinkUD),
	PORT_FN(UnLinkLR, Menu_UnLinkLR),
	PORT_FN(UnLinkUD, Menu_UnLinkUD),
	PORT_FN(DrawBackMainMenu, Menu_DrawBackMainMenu),
	PORT_FN(DrawBackSubMenu, Menu_DrawBackSubMenu),
	PORT_FN(DrawBackOptionMenu, Menu_DrawBackOptionMenu),
	PORT_FN(DrawCircuitMenu, Menu_DrawCircuitMenu),
	PORT_FN(DrawMenuTitleBar, Menu_DrawMenuTitleBar),
	PORT_FN(StartNewMenu, Menu_StartNewMenu),
	PORT_FNK(AddTabs, Menu_AddTabs),
	PORT_FN(DummyFunc, Mode_PyNone),
	PORT_FNK(VerticalMenu, Menu_VerticalMenu),
	PORT_FN(YesNoMenu, Menu_YesNoMenu),
	PORT_FN(DisableMenuItem, Menu_DisableMenuItem),
	PORT_FN(ShowTextBox, Menu_ShowTextBox),
	PORT_FN(ShowWarning, Menu_ShowWarning),
	PORT_FN(SetTabDefaults, Menu_SetTabDefaults),
	PORT_FN(SliderMenu, Menu_SliderMenu),
	PORT_FN(PreviousNextMenu, Menu_PreviousNextMenu),
	PORT_FN(PreviousNextMenuSimple, Menu_PreviousNextMenuSimple),
	PORT_FNEND
};

PORTS(MenuBase)
