/*
 * The mission info pages of the Menu module in C (issue #2, step 4c): the primary mission with the radar, the crazy
 * deal with its objectives and the notes with the blueprints found. The layout is data/menus/mission.menu.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

static const char mission_menu[] = {
#embed "../../../data/menus/mission.menu"
	, 0 };

#define MISSION_FILE "menus/mission.menu"

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
static void Mission_Objective(int id, int num, PyObject *text1, PyObject *text2, const int *frame, const int *text) {
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:O,s:O,s:i,s:i,s:i,s:i,s:i,s:i}", "num", num, "y", 150 + num * 50,
		"text1", text1, "text2", text2, "red", frame[0], "green", frame[1], "blue", frame[2], "tred", text[0], "tgreen",
		text[1], "tblue", text[2]);
	Mission_Run(id, "objective", vars);
	Py_XDECREF(vars);
}

static void Mission_ObjectiveText(int id, int num, const char *text1, const char *text2, const int *frame,
	const int *text) {
	PyObject *a = PyString_FromString(text1), *b = PyString_FromString(text2);
	Mission_Objective(id, num, a, b, frame, text);
	Py_XDECREF(a);
	Py_XDECREF(b);
}

/* MissionsFuncs.CrazyDealStatus(num): (name, done, needed), a new reference */
static PyObject *Mission_DealStatus(int num) {
	PyObject *m = PyImport_ImportModule("MissionsFuncs");
	PyObject *f = m ? PyObject_GetAttrString(m, "CrazyDealStatus") : NULL;
	PyObject *r = f ? PyEval_CallObject(f, Py_BuildValue("(i)", num)) : NULL;
	if (!r) {
		PyErr_Print();
	}
	Py_XDECREF(f);
	Py_XDECREF(m);
	return r;
}

/* done >= needed of a status, as the script compared the two */
static byte Mission_Ready(PyObject *res) {
	PyObject *a = res ? PySequence_GetItem(res, 1) : NULL, *b = res ? PySequence_GetItem(res, 2) : NULL;
	byte ready = a && b && PyObject_Compare(a, b) >= 0;
	PyErr_Clear();
	Py_XDECREF(a);
	Py_XDECREF(b);
	return ready;
}

/* the line of objective num of the crazy deal: complete, or done / needed */
static byte Mission_DealObjective(int id, int num) {
	static const int frame[3] = { 255, 64, 32 }, text[3] = { 251, 174, 174 };
	PyObject *res = Mission_DealStatus(num);
	byte ready = Mission_Ready(res);
	if (ready) {
		Mission_ObjectiveText(id, num, Script_Lang("CrazyDeal_Menu_ObjComplete"), "\x02", frame, text);
	} else if (res) {
		char key[0x80], count[0x80];
		PyObject *name = PySequence_GetItem(res, 0), *a = PySequence_GetItem(res, 1), *b = PySequence_GetItem(res, 2);
		PyObject *ra = a ? PyObject_Repr(a) : NULL, *rb = b ? PyObject_Repr(b) : NULL;
		Str_Printf(key, sizeof(key), "CrazyDeal_Menu_%s", name && PyString_Check(name) ? PyString_AsString(name) : "");
		Str_Printf(count, sizeof(count), "%s / %s", ra ? PyString_AsString(ra) : "", rb ? PyString_AsString(rb) : "");
		Mission_ObjectiveText(id, num, Script_Lang(key), count, frame, text);
		PyErr_Clear();
		Py_XDECREF(ra);
		Py_XDECREF(rb);
		Py_XDECREF(a);
		Py_XDECREF(b);
		Py_XDECREF(name);
	}
	Py_XDECREF(res);
	return ready;
}

/* the number of entries of a save variable that holds a Python list (none when it is not set) */
static int Mission_ListCount(const char *var) {
	const char *s = SaveVar_Get(var);
	if (!s) {
		return 0;
	}
	PyObject *v = Script_Eval(s);
	int n = v ? PySequence_Length(v) : 0;
	PyErr_Clear();
	Py_XDECREF(v);
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

static byte Mission_SaveVarIs(const char *var, const char *value) {
	const char *s = SaveVar_Get(var);
	return s && !strcmp(s, value);
}

/* the description of the crazy deal page, and its objectives */
static void Mission_DealDesc(int id, char *desc, int size) {
	if (Script_GetInt("Init", "HasSuperDeal", 0)) {
		PyObject *init = PyImport_ImportModule("Init");
		PyObject *mod = init ? PyObject_GetAttrString(init, "SuperDealModule") : NULL;
		PyObject *f = mod ? PyObject_GetAttrString(mod, "GetMenuDescription") : NULL;
		PyObject *r = f ? PyEval_CallObject(f, PyTuple_New(0)) : NULL;
		desc[0] = 0;
		if (r && PyString_Check(r)) {
			Str_Copy(desc, PyString_AsString(r), size);
		} else if (!r) {
			PyErr_Print();
		}
		Py_XDECREF(r);
		Py_XDECREF(f);
		Py_XDECREF(mod);
		Py_XDECREF(init);
		return;
	}
	if (Mission_SaveVarIs("JackInTheBox.Active", "0")) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_None"), size);
		return;
	}
	if (Mission_SaveVarIs("JackInTheBox.Active", "2")) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_SuperDealsComplete"), size);
		return;
	}
	byte all = 1;
	for (int i = 1; i <= 3; i++) {
		PyObject *res = Mission_DealStatus(i);
		all &= Mission_Ready(res);
		Py_XDECREF(res);
	}
	if (!all) {
		Str_Copy(desc, Script_Lang("CrazyDeal_Menu_Doit"), size);
		for (int i = 1; i <= 3; i++) {
			Mission_DealObjective(id, i);
		}
		return;
	}
	if (Script_GetInt("Init", "isDemo", 0)) {
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
		if (Mission_SaveVarIs("GameAct", "Prologue") && Mission_SaveVarIs("JackInTheBox.Active", "0") &&
			(!nb || atoi(nb) <= 0)) {
			Str_Copy(desc, Script_Lang("Notes_None"), sizeof(desc));
		} else {
			Str_Copy(desc, Script_Lang("Notes_Outlines_Description"), sizeof(desc));
			Mission_Notes(id);
		}
	}
	Script_SetValue("title", PyString_FromString(title));
	Script_SetValue("desc", PyString_FromString(desc));
	Script_GetString("Menu", "UseTxt", use, sizeof(use));
	Mission_Replace(Script_Lang(p->hintLeft), "%s", use, hint, sizeof(hint));
	Script_SetValue("hintl", PyString_FromString(hint));
	Mission_Replace(Script_Lang(p->hintRight), "%s", use, hint, sizeof(hint));
	Script_SetValue("hintr", PyString_FromString(hint));
}

static const scriptStep_t mission_steps[] = {
	{ "content", Step_Content },
	{ NULL, NULL },
};

static void Mission_Run(int id, const char *section, PyObject *vars) {
	Script_RunMenu(id, MISSION_FILE, section, mission_menu, mission_steps, vars);
}

static void Mission_Menu(int id, const char *name) {
	const missionPage_t *p = Mission_Page(name);
	char back[0x100];
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	if (!p) {
		return;
	}
	const missionPage_t *outer = mission_page;
	mission_page = p;
	Str_Printf(back, sizeof(back), "\x15 %s", Script_Lang("XBox_Menu_Back"));
	PyObject *vars = Py_BuildValue("{s:s,s:s,s:s,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:s}", "page", p->page, "ltitle",
		p->left, "rtitle", p->right, "red", p->red, "green", p->green, "blue", p->blue, "tred", p->tred, "tgreen",
		p->tgreen, "tblue", p->tblue, "xbox", GVar_GetNum("IsXbox", 0) != 0, "outdoor",
		Script_GetInt("Init", "Outdoor", 0) != 0, "xboxback", back);
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
static PyObject *Menu_EnterMissionInfo(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Script_Call("Menu", "InitMenuSys", "(i)", 0);
	const char *map = SaveVar_Get("SuperDeal.Mission.Map");
	if (map && *map) {
		Mission_Menu(id, "CrazyDeal");
		return Py_ReturnNone();
	}
	if (Script_HasItem(id, "Mission")) {
		int last = (int)Script_GetNum(id, "Mission", "LastStatus");
		if (last >= 0 && last <= 2) {
			Mission_Menu(id, mission_pages[last].page);
		}
	}
	return Py_ReturnNone();
}

static PyObject *Menu_ExitMissionInfo(PyObject *self, PyObject *args) {
	(void)self;
	MISSION_ARGS(args);
	(void)control;
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, "");
		cPy_Function_SetFunc(&sc->onPrev, "");
	}
	Script_Call("Menu", "BackToGame", "(i)", id);
	return Py_ReturnNone();
}

/* MenuObjetive(id, num, text1, text2, (red, green, blue), (textred, textgreen, textblue)) */
static PyObject *Menu_MenuObjetive(PyObject *self, PyObject *args) {
	int id, num, frame[3], text[3];
	PyObject *t1, *t2;
	(void)self;
	if (!PyArg_ParseTuple(args, "iiOO(iii)(iii)", &id, &num, &t1, &t2, &frame[0], &frame[1], &frame[2], &text[0],
		&text[1], &text[2])) {
		return NULL;
	}
	Mission_Objective(id, num, t1, t2, frame, text);
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
