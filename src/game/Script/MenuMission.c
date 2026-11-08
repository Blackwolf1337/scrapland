/*
 * The mission info pages of the Menu module in C (issue #2, step 4c): the primary mission with the radar, the crazy
 * deal with its objectives and the notes with the blueprints found. The layout is data/menus/mission.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the pages work on C texts and values. The save variables, the engine
 * variables, the scorer and the language texts are C; Init.Outdoor, HasSuperDeal, isDemo and Menu.UseTxt are read as
 * the attributes of those modules (OBJLIB 5); MissionsFuncs.CrazyDealStatus and the Menu functions are called through
 * their refs (PORT_CALL: directly while the attribute holds the port's function, else by name), the GetMenuDescription
 * of the running super deal by name (OBJLIB 4: its module is a variable of Init); their errors are printed as before
 * (OBJLIB 7). The .def runner takes its $names as a dict and the texts of the content step as Python values
 * (OBJLIB 6).
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static const char mission_def[] = {
#embed "../../../data/menus/mission.def"
	, 0 };

#define MISSION_FILE "menus/mission.def"

typedef struct {
	const char *page, *left, *right;	/* the page and the pages its arrows lead to */
	int red, green, blue;			/* the frame */
	int tred, tgreen, tblue;		/* the texts */
	const char *hintLeft, *hintRight;	/* the hints of the arrows */
} missionPage_t;

static const missionPage_t mission_pages[3] = {
	{ "PrimaryMission", "Notes", "CrazyDeal", 255, 255, 64, 251, 241, 174, "Notes_Hint", "CrazyDeal_Hint" },
	{ "CrazyDeal", "PrimaryMission", "Notes", 255, 64, 32, 251, 174, 174, "PrimaryMission_Hint", "Notes_Hint" },
	{ "Notes", "CrazyDeal", "PrimaryMission", 37, 119, 255, 174, 229, 251, "CrazyDeal_Hint", "PrimaryMission_Hint" },
};

static const missionPage_t *Mission_Page(const char *name) {
	for (int i = 0; i < 3; i++) {
		if (!strcmp(mission_pages[i].page, name)) {
			return &mission_pages[i];
		}
	}
	return NULL;
}

/* the page that runs, for its content step */
static const missionPage_t *mission_page;

static void Mission_Run(int id, const char *section, PyObject *vars);

/* module.var as an int: an int as it is, a float cut, anything else (or no such variable) def (Script_GetInt) */
static int Mission_ModInt(const char *module, const char *var, int def) {
	nativeVal_t v = { 0 };
	int r = def;
	if (!Mode_ModGet(module, var, &v)) {
		/* OBJLIB 7 */
		PyErr_Clear();
	} else if (v.type == NV_INT) {
		r = (int)v.i;
	} else if (v.type == NV_FLOAT) {
		r = (int)v.f;
	}
	Native_Free(&v);
	return r;
}

/* a text of any length as a value (Native_Free it) */
static const nativeVal_t *Mission_Text(nativeVal_t *v, const char *s) {
	Native_SetText(v, s, (int)strlen(s));
	return v;
}

/* the text with the first %s (or <tag>) replaced by arg */
static void Mission_Replace(const char *text, const char *tag, const char *arg, char *out, int size) {
	const char *at = strstr(text, tag);
	if (at) {
		Str_Printf(out, size, "%.*s%s%s", (int)(at - text), text, arg, at + strlen(tag));
	} else {
		Str_Copy(out, text, size);
	}
}

/* one line of objectives or notes in the colours of the page (red..blue the frame, t* the texts) */
static void Mission_Objective(int id, int num, const nativeVal_t *text1, const nativeVal_t *text2, const int *frame,
	const int *text) {
	nativeVal_t v[8];
	PyObject *vars = Mode_DefVars(10, "num", Mode_IntVal(&v[0], num), "y", Mode_IntVal(&v[1], 150 + num * 50), "text1", text1,
		"text2", text2, "red", Mode_IntVal(&v[2], frame[0]), "green", Mode_IntVal(&v[3], frame[1]), "blue",
		Mode_IntVal(&v[4], frame[2]), "tred", Mode_IntVal(&v[5], text[0]), "tgreen", Mode_IntVal(&v[6], text[1]), "tblue",
		Mode_IntVal(&v[7], text[2]));
	Mission_Run(id, "objective", vars);
	Py_XDECREF(vars);
}

static void Mission_ObjectiveText(int id, int num, const char *text1, const char *text2, const int *frame,
	const int *text) {
	nativeVal_t a, b;
	Mission_Objective(id, num, Mission_Text(&a, text1), Mission_Text(&b, text2), frame, text);
	Native_Free(&a);
	Native_Free(&b);
}

/* MissionsFuncs.CrazyDealStatus(num): (name, done, needed) into out, 0 after the error was printed */
static byte Mission_DealStatus(int num, nativeVal_t *out) {
	if (PORT_CALL(MissionsFuncs, CrazyDealStatus, out, "i", (long)num)) {
		/* a tuple held as one object (it holds others) as its items */
		Mode_Elems(out);
		return 1;
	}
	/* OBJLIB 7 */
	PyErr_Print();
	return 0;
}

/* item i of a status as PySequence_GetItem takes it, NULL when there is none (tmp: Native_Free it) */
static const nativeVal_t *Mission_Item(const nativeVal_t *res, long i, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	return Native_GetItem(res, i, 0, tmp);
}

/* done >= needed of a status, as the script compared the two */
static byte Mission_Ready(const nativeVal_t *res) {
	nativeVal_t ta, tb;
	const nativeVal_t *a = Mission_Item(res, 1, &ta), *b = Mission_Item(res, 2, &tb);
	byte ready = a && b && Native_Compare(a, b) >= 0;
	/* OBJLIB 7 */
	PyErr_Clear();
	Native_Free(&ta);
	Native_Free(&tb);
	return ready;
}

/* the repr of a value into a C buffer ("" for NULL) */
static void Mission_Repr(const nativeVal_t *v, char *out, int size) {
	nativeBuf_t b;
	out[0] = 0;
	if (v) {
		Native_BufInit(&b);
		Native_Repr(&b, v, NR_REPR);
		Str_Copy(out, b.p, size);
		Native_BufFree(&b);
	}
}

/* the line of objective num of the crazy deal: complete, or done / needed */
static byte Mission_DealObjective(int id, int num) {
	static const int frame[3] = { 255, 64, 32 }, text[3] = { 251, 174, 174 };
	nativeVal_t res = { 0 };
	byte got = Mission_DealStatus(num, &res);
	byte ready = got && Mission_Ready(&res);
	if (ready) {
		Mission_ObjectiveText(id, num, Script_Lang("CrazyDeal_Menu_ObjComplete"), "\x02", frame, text);
	} else if (got) {
		char key[0x80], count[0x80], ra[0x80], rb[0x80];
		nativeVal_t tn, ta, tb;
		const nativeVal_t *name = Mission_Item(&res, 0, &tn), *a = Mission_Item(&res, 1, &ta), *b = Mission_Item(&res, 2, &tb);
		Mission_Repr(a, ra, sizeof(ra));
		Mission_Repr(b, rb, sizeof(rb));
		Str_Printf(key, sizeof(key), "CrazyDeal_Menu_%s", name && name->type == NV_STR ? Native_Text(name) : "");
		Str_Printf(count, sizeof(count), "%s / %s", ra, rb);
		Mission_ObjectiveText(id, num, Script_Lang(key), count, frame, text);
		/* OBJLIB 7 */
		PyErr_Clear();
		Native_Free(&tn);
		Native_Free(&ta);
		Native_Free(&tb);
	}
	Native_Free(&res);
	return ready;
}

/* the number of entries of a save variable that holds a Python list (none when it is not set or no literal, -1 for
 * a value without a sequence length) */
static int Mission_ListCount(const char *var) {
	const char *s = SaveVar_Get(var);
	nativeVal_t v;
	int n = 0;
	if (s && Native_Literal(s, &v)) {
		if (!Native_Len(&v, &n)) {
			n = -1;
			/* OBJLIB 7 */
			PyErr_Clear();
		}
		Native_Free(&v);
	}
	return n;
}

/* the notes: the blueprints of the junkyard, the police and the press */
static void Mission_Notes(int id) {
	static const int frame[3] = { 32, 64, 255 }, text[3] = { 174, 229, 251 };
	static const struct {
		const char *station, *ships, *engines;
	} found[2] = { { "Station_Police", "PoliceBlueprints.Ships", "PoliceBlueprints.Engines" },
		{ "Station_Press", "PressBlueprints.Ships", "PressBlueprints.Engines" } };
	const char *rusty = SaveVar_Get("NewBluePrintAvaliable");
	int n = rusty ? atoi(rusty) : 0;
	char count[0x20], line[0x200];
	int pos = 1;

	if (n > 0) {
		Str_Printf(count, sizeof(count), "%d", n);
		Mission_ObjectiveText(id, pos, Script_Lang("Notes_OutlinesJunkyard_New"), count, frame, text);
	} else {
		Mission_ObjectiveText(id, pos, Script_Lang("Notes_OutlinesJunkyard_None"), "", frame, text);
	}
	pos++;
	for (int i = 0; i < 2; i++) {
		int prints = Mission_ListCount(found[i].ships) + Mission_ListCount(found[i].engines);
		if (prints > 0) {
			Mission_Replace(Script_Lang("Notes_Outlines_New"), "<n1>", Script_Lang(found[i].station), line, sizeof(line));
			Str_Printf(count, sizeof(count), "%d", prints);
			Mission_ObjectiveText(id, pos, line, count, frame, text);
			pos++;
		}
	}
}

/* the description of the crazy deal page, and its objectives */
static void Mission_DealDesc(int id, char *desc, int size) {
	if (Mission_ModInt("Init", "HasSuperDeal", 0)) {
		/* Init.SuperDealModule.GetMenuDescription() (OBJLIB 4: the module of the running super deal) */
		nativeVal_t mod = { 0 }, f = { 0 }, r = { 0 };
		desc[0] = 0;
		if (Mode_ModGet("Init", "SuperDealModule", &mod) && Mode_VObj(&mod, "GetMenuDescription", &f) &&
			Mode_CallArgs(&f, &r, NULL, 0)) {
			if (r.type == NV_STR) {
				Str_Copy(desc, Native_Text(&r), size);
			}
		} else {
			/* OBJLIB 7 */
			PyErr_Print();
		}
		Native_Free(&r);
		Native_Free(&f);
		Native_Free(&mod);
		return;
	}
	if (LvN_SaveIs("JackInTheBox.Active", "0")) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_None"), size);
		return;
	}
	if (LvN_SaveIs("JackInTheBox.Active", "2")) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_SuperDealsComplete"), size);
		return;
	}
	byte all = 1;
	for (int i = 1; i <= 3; i++) {
		nativeVal_t res = { 0 };
		if (Mission_DealStatus(i, &res)) {
			all &= Mission_Ready(&res);
		} else {
			all = 0;
		}
		Native_Free(&res);
	}
	if (!all) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_Doit"), size);
		for (int i = 1; i <= 3; i++) {
			Mission_DealObjective(id, i);
		}
		return;
	}
	if (Mission_ModInt("Init", "isDemo", 0)) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_None"), size);
		return;
	}
	/* all three done: the prize of the super deal */
	char key[0x100];
	const char *lib = SaveVar_Get("SuperDeal.Library");
	Str_Printf(key, sizeof(key), "SuperDeal_%sPrize_Description", lib && strlen(lib) > 10 ? lib + 10 : "");
	Str_Copy(desc, Script_Lang(key), size);
	GVar_SetNum("SuperDealAvaliable", 1);
	for (int i = 1; i <= 3; i++) {
		char deal[0x10], tab[0x10];
		Str_Printf(deal, sizeof(deal), "Deal%d", i);
		Str_Printf(tab, sizeof(tab), "TabDeal%d", i);
		Script_SetNum(id, deal, "Visible", 0);
		Script_SetNum(id, tab, "Visible", 0);
	}
}

/* a $name of the file that runs as a text (OBJLIB 6: the runner keeps Python values) */
static void Mission_SetValue(const char *name, const char *text) {
	nativeVal_t v;
	PyObject *o = Native_ToPy(Mission_Text(&v, text));
	Native_Free(&v);
	Script_SetValue(name, o);
}

/* Menu.UseTxt as a C text ("" for any other value or none) */
static void Mission_UseTxt(char *out, int size) {
	nativeVal_t v = { 0 };
	out[0] = 0;
	if (!Mode_ModGet("Menu", "UseTxt", &v)) {
		/* OBJLIB 7 */
		PyErr_Clear();
	} else if (v.type == NV_STR) {
		Str_Copy(out, Native_Text(&v), size);
	}
	Native_Free(&v);
}

/* run content: the status of the page read, its description, objectives and hints */
static void Step_Content(int id, PyObject *args) {
	const missionPage_t *p = mission_page;
	char desc[0x1000] = "", title[0x200], use[0x80], hint[0x200];
	(void)args;
	if (!p) {
		return;
	}
	if (p == &mission_pages[0]) {
		Script_SetNum(id, "Mission", "Status0", 0);
		const char *d = GVar_GetStr("PrimaryMissionDesc");
		const char *lang = *d ? Script_Lang(d) : "";
		Str_Copy(desc, *lang ? lang : d, sizeof(desc));
		Str_Printf(title, sizeof(title), "%s:", Script_Lang("PrimaryMission_Title"));
	} else if (p == &mission_pages[1]) {
		Script_SetNum(id, "Mission", "Status1", 0);
		Mission_DealDesc(id, desc, sizeof(desc));
		Str_Printf(title, sizeof(title), "%s:", Script_Lang("CrazyDeal_Menu_Status"));
	} else {
		Script_SetNum(id, "Mission", "Status2", 0);
		Str_Printf(title, sizeof(title), "%s:", Script_Lang("Notes_Title"));
		const char *nb = SaveVar_Get("NewBluePrintAvaliable");
		if (LvN_SaveIs("GameAct", "Prologue") && LvN_SaveIs("JackInTheBox.Active", "0") &&
			(!nb || atoi(nb) <= 0)) {
			Str_Copy(desc, Script_Lang("Notes_None"), sizeof(desc));
		} else {
			Str_Copy(desc, Script_Lang("Notes_Outlines_Description"), sizeof(desc));
			Mission_Notes(id);
		}
	}
	Mission_SetValue("title", title);
	Mission_SetValue("desc", desc);
	Mission_UseTxt(use, sizeof(use));
	Mission_Replace(Script_Lang(p->hintLeft), "%s", use, hint, sizeof(hint));
	Mission_SetValue("hintl", hint);
	Mission_Replace(Script_Lang(p->hintRight), "%s", use, hint, sizeof(hint));
	Mission_SetValue("hintr", hint);
}

static const scriptStep_t mission_steps[] = {
	{ "content", Step_Content },
	{ NULL, NULL },
};

static void Mission_Run(int id, const char *section, PyObject *vars) {
	Script_RunFile(id, MISSION_FILE, section, mission_def, mission_steps, vars);
}

static void Mission_Menu(int id, const char *name) {
	const missionPage_t *p = Mission_Page(name);
	char back[0x100];
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	if (!p) {
		return;
	}
	const missionPage_t *outer = mission_page;
	mission_page = p;
	Str_Printf(back, sizeof(back), "\x15 %s", Script_Lang("XBox_Menu_Back"));
	nativeVal_t v[12];
	PyObject *vars = Mode_DefVars(12, "page", Mission_Text(&v[0], p->page), "ltitle", Mission_Text(&v[1], p->left), "rtitle",
		Mission_Text(&v[2], p->right), "red", Mode_IntVal(&v[3], p->red), "green", Mode_IntVal(&v[4], p->green), "blue",
		Mode_IntVal(&v[5], p->blue), "tred", Mode_IntVal(&v[6], p->tred), "tgreen", Mode_IntVal(&v[7], p->tgreen), "tblue",
		Mode_IntVal(&v[8], p->tblue), "xbox", Mode_IntVal(&v[9], GVar_GetNum("IsXbox", 0) != 0), "outdoor",
		Mode_IntVal(&v[10], Mission_ModInt("Init", "Outdoor", 0) != 0), "xboxback", Mission_Text(&v[11], back));
	for (int i = 0; i < 12; i++) {
		Native_Free(&v[i]);
	}
	Mission_Run(id, "page", vars);
	Py_XDECREF(vars);
	mission_page = outer;
}

/* ---- the functions of Menu ---- */

#define MISSION_ARGS(args) \
	int id; \
	const char *control; \
	if (!Script_Args(args, &id, &control)) { \
		return NULL; \
	}

static PyObject *Menu_MenuMissionInfo(PyObject *self, PyObject *args) {
	(void)self;
	MISSION_ARGS(args);
	Mission_Menu(id, control);
	return Py_ReturnNone();
}

/* the arrows: the page of the arrow (its name), the cursor on the arrow that leads further the same way */
static PyObject *Mission_Arrow(PyObject *args, byte left) {
	MISSION_ARGS(args);
	Mission_Menu(id, control);
	const missionPage_t *p = Mission_Page(control);
	if (p) {
		Script_SetDefault(id, left ? p->left : p->right);
	}
	return Py_ReturnNone();
}

static PyObject *Menu_MenuMissionInfo_Left(PyObject *self, PyObject *args) {
	(void)self;
	return Mission_Arrow(args, 1);
}

static PyObject *Menu_MenuMissionInfo_Right(PyObject *self, PyObject *args) {
	(void)self;
	return Mission_Arrow(args, 0);
}

#define MISSION_GO(func, page) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		(void)self; \
		MISSION_ARGS(args); \
		(void)control; \
		Mission_Menu(id, page); \
		return Py_ReturnNone(); \
	}

MISSION_GO(Menu_GoPrimaryMission, "PrimaryMission")
MISSION_GO(Menu_GoCrazyDeal, "CrazyDeal")
MISSION_GO(Menu_GoNotes, "Notes")

/* EnterMissionInfo(id): the menu opens on the page the mission scorer shows (the deal while a super deal runs) */
static byte Menu_EnterMissionInfo_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	MODE_PRINTED(Menu, InitMenuSys, "i", 0L);
	const char *map = SaveVar_Get("SuperDeal.Mission.Map");
	if (map && *map) {
		Mission_Menu(id, "CrazyDeal");
		return 1;
	}
	if (Script_HasItem(id, "Mission")) {
		int last = (int)Script_GetNum(id, "Mission", "LastStatus");
		if (last >= 0 && last <= 2) {
			Mission_Menu(id, mission_pages[last].page);
		}
	}
	return 1;
}
MODE_ADAPTER(Menu, EnterMissionInfo)

static PyObject *Menu_ExitMissionInfo(PyObject *self, PyObject *args) {
	(void)self;
	MISSION_ARGS(args);
	(void)control;
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, "");
		cPy_Function_SetFunc(&sc->onPrev, "");
	}
	MODE_PRINTED(Menu, BackToGame, "i", (long)id);
	return Py_ReturnNone();
}

/* MenuObjetive(id, num, text1, text2, (red, green, blue), (textred, textgreen, textblue)) */
static PyObject *Menu_MenuObjetive(PyObject *self, PyObject *args) {
	int id, num, frame[3], text[3];
	PyObject *t1, *t2;
	nativeVal_t a, b;
	(void)self;
	if (!PyArg_ParseTuple(args, "iiOO(iii)(iii)", &id, &num, &t1, &t2, &frame[0], &frame[1], &frame[2], &text[0],
		&text[1], &text[2])) {
		return NULL;
	}
	Native_Arg(t1, &a);
	Native_Arg(t2, &b);
	Mission_Objective(id, num, &a, &b, frame, text);
	Native_Free(&a);
	Native_Free(&b);
	return Py_ReturnNone();
}

static PyObject *Menu_CrazyDealObjetive(PyObject *self, PyObject *args) {
	int id, num;
	(void)self;
	if (!PyArg_ParseTuple(args, "ii", &id, &num)) {
		return NULL;
	}
	return PyInt_FromLong(Mission_DealObjective(id, num));
}

static PyObject *Menu_MissionNotesText(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Mission_Notes(id);
	return Py_ReturnNone();
}

PyMethodDef g_MenuMissionMethods[] = {
	{ "MenuMissionInfo", Menu_MenuMissionInfo, 1, NULL },
	{ "MenuMissionInfo_Left", Menu_MenuMissionInfo_Left, 1, NULL },
	{ "MenuMissionInfo_Right", Menu_MenuMissionInfo_Right, 1, NULL },
	{ "GoPrimaryMission", Menu_GoPrimaryMission, 1, NULL },
	{ "GoCrazyDeal", Menu_GoCrazyDeal, 1, NULL },
	{ "GoNotes", Menu_GoNotes, 1, NULL },
	{ "EnterMissionInfo", Menu_EnterMissionInfo, 1, NULL },
	{ "ExitMissionInfo", Menu_ExitMissionInfo, 1, NULL },
	{ "MenuObjetive", Menu_MenuObjetive, 1, NULL },
	{ "CrazyDealObjetive", Menu_CrazyDealObjetive, 1, NULL },
	{ "MissionNotesText", Menu_MissionNotesText, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MenuMission)
