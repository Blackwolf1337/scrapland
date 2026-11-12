/*
 * The mission info pages of the Menu module in C (issue #2, step 4c): the primary mission with the radar, the crazy
 * deal with its objectives and the notes with the blueprints found. The layout is data/menus/mission.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the pages work on C texts and values. The save variables, the engine
 * variables, the scorer and the language texts are C; Init.Outdoor, HasSuperDeal, isDemo and Menu.UseTxt are read as
 * the attributes of those modules; MissionsFuncs.CrazyDealStatus and the Menu functions are called through
 * their refs (PORT_CALL: directly while the attribute holds the port's function, else by name), the GetMenuDescription
 * of the running super deal through its entry (Mode_CallArgs; OBJLIB 5 (9c: its module is a variable of Init)); their
 * errors are printed as before (OBJLIB 7). The .def runner takes its $names as a dict and the texts of the content
 * step as Python values (OBJLIB 6).
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

static void Mission_Run(int id, const char *section, const char *const *names, const nativeVal_t *const *vals, int n);

/* module.var as an int: an int as it is, a float cut, anything else (or no such variable) def (Script_GetInt) */
static int Mission_ModInt(const char *module, const char *var, int def) {
	nativeVal_t v = { 0 };
	int r = def;
	if (!Mode_ModGet(module, var, &v)) {
		/* OBJLIB 7 */
		Mode_DropError();
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
	static const char *const names[10] = { "num", "y", "text1", "text2", "red", "green", "blue", "tred", "tgreen",
		"tblue" };
	nativeVal_t v[8];
	const nativeVal_t *vals[10] = { Mode_IntVal(&v[0], num), Mode_IntVal(&v[1], 150 + num * 50), text1, text2,
		Mode_IntVal(&v[2], frame[0]), Mode_IntVal(&v[3], frame[1]), Mode_IntVal(&v[4], frame[2]),
		Mode_IntVal(&v[5], text[0]), Mode_IntVal(&v[6], text[1]), Mode_IntVal(&v[7], text[2]) };
	Mission_Run(id, "objective", names, vals, 10);
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
	Mode_PrintError();
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
	Mode_DropError();
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
		Mode_DropError();
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
			Mode_DropError();
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
		/* Init.SuperDealModule.GetMenuDescription() (OBJLIB 5 (9c: the module of the running super deal, a variable))
		 * */
		nativeVal_t mod = { 0 }, f = { 0 }, r = { 0 };
		desc[0] = 0;
		if (Mode_ModGet("Init", "SuperDealModule", &mod) && Mode_VObj(&mod, "GetMenuDescription", &f) &&
			Mode_CallArgs(&f, &r, NULL, 0)) {
			if (r.type == NV_STR) {
				Str_Copy(desc, Native_Text(&r), size);
			}
		} else {
			/* OBJLIB 7 */
			Mode_PrintError();
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

/* a $name of the file that runs as a text (Mode_SetValue) */
static void Mission_SetValue(const char *name, const char *text) {
	nativeVal_t v;
	Mode_SetValue(name, Mission_Text(&v, text));
	Native_Free(&v);
}

/* Menu.UseTxt as a C text ("" for any other value or none) */
static void Mission_UseTxt(char *out, int size) {
	nativeVal_t v = { 0 };
	out[0] = 0;
	if (!Mode_ModGet("Menu", "UseTxt", &v)) {
		/* OBJLIB 7 */
		Mode_DropError();
	} else if (v.type == NV_STR) {
		Str_Copy(out, Native_Text(&v), size);
	}
	Native_Free(&v);
}

/* run content: the status of the page read, its description, objectives and hints */
static void Step_Content(int id, const nativeVal_t *args, int n) {
	const missionPage_t *p = mission_page;
	char desc[0x1000] = "", title[0x200], use[0x80], hint[0x200];
	(void)args;
	(void)n;
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

/* a section of mission.def with its $names as n names and values (Mode_DefRunV; OBJLIB 6: the runner takes them as a
 * dict) */
static void Mission_Run(int id, const char *section, const char *const *names, const nativeVal_t *const *vals, int n) {
	Mode_DefRunV(id, MISSION_FILE, section, mission_def, mission_steps, 0, names, vals, n);
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
	static const char *const names[12] = { "page", "ltitle", "rtitle",
		"red", "green", "blue", "tred", "tgreen", "tblue",
		"xbox", "outdoor", "xboxback" };
	nativeVal_t v[12];
	const nativeVal_t *vals[12] = { Mission_Text(&v[0], p->page), Mission_Text(&v[1], p->left),
		Mission_Text(&v[2], p->right), Mode_IntVal(&v[3], p->red), Mode_IntVal(&v[4], p->green),
		Mode_IntVal(&v[5], p->blue), Mode_IntVal(&v[6], p->tred), Mode_IntVal(&v[7], p->tgreen),
		Mode_IntVal(&v[8], p->tblue), Mode_IntVal(&v[9], GVar_GetNum("IsXbox", 0) != 0),
		Mode_IntVal(&v[10], Mission_ModInt("Init", "Outdoor", 0) != 0), Mission_Text(&v[11], back) };
	Mission_Run(id, "page", names, vals, 12);
	for (int i = 0; i < 12; i++) {
		Native_Free(&v[i]);
	}
	mission_page = outer;
}

/* ---- the functions of Menu ---- */

#define MISSION_ARGS(a, n) \
	int id; \
	const char *control; \
	if (!Mode_IdControl(a, n, &id, &control)) { \
		return 0; \
	}

static byte Menu_MenuMissionInfo_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MISSION_ARGS(a, n);
	Mission_Menu(id, control);
	return 1;
}
MODE_ADAPTER(Menu, MenuMissionInfo)

/* the arrows: the page of the arrow (its name), the cursor on the arrow that leads further the same way */
static byte Mission_Arrow(const nativeVal_t *a, int n, byte left) {
	MISSION_ARGS(a, n);
	Mission_Menu(id, control);
	const missionPage_t *p = Mission_Page(control);
	if (p) {
		Script_SetDefault(id, left ? p->left : p->right);
	}
	return 1;
}

static byte Menu_MenuMissionInfo_Left_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Mission_Arrow(a, n, 1);
}
MODE_ADAPTER(Menu, MenuMissionInfo_Left)

static byte Menu_MenuMissionInfo_Right_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Mission_Arrow(a, n, 0);
}
MODE_ADAPTER(Menu, MenuMissionInfo_Right)

#define MISSION_GO(func, page) MODE_IDFN(func, (Mission_Menu(id, page), 1))

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

static byte Menu_ExitMissionInfo_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MISSION_ARGS(a, n);
	(void)control;
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, "");
		cPy_Function_SetFunc(&sc->onPrev, "");
	}
	MODE_PRINTED(Menu, BackToGame, "i", (long)id);
	return 1;
}
MODE_ADAPTER(Menu, ExitMissionInfo)

/* MenuObjetive(id, num, text1, text2, (red, green, blue), (textred, textgreen, textblue)) */
static byte Menu_MenuObjetive_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, num, frame[3], text[3];
	const nativeVal_t *t1, *t2;
	(void)out;
	if (!Mode_Parse(a, n, "iiOO(iii)(iii)", &id, &num, &t1, &t2, &frame[0], &frame[1], &frame[2], &text[0], &text[1],
		&text[2])) {
		return 0;
	}
	Mission_Objective(id, num, t1, t2, frame, text);
	return 1;
}
MODE_ADAPTER(Menu, MenuObjetive)

static byte Menu_CrazyDealObjetive_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, num;
	if (!Mode_Parse(a, n, "ii", &id, &num)) {
		return 0;
	}
	Native_SetInt(out, Mission_DealObjective(id, num));
	return 1;
}
MODE_ADAPTER(Menu, CrazyDealObjetive)

static byte Menu_MissionNotesText_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Mission_Notes(id);
	return 1;
}
MODE_ADAPTER(Menu, MissionNotesText)

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
