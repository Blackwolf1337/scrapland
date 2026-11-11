/*
 * The options and controls menus of PCMenu in C (issue #2, step 2): OptionsMenu, ControlsMenu, the mouse, the keys of
 * the actions and the pad. The layout is data/menus/options.def, one section per menu.
 *
 * Native C (docs/objlib-plan.md, stage 2): the state of the keys menu is the struct controls, the functions call the
 * engine in C and work on C values; the PyMethodDef entries are adapters. The pad menu keeps its state in the module
 * variables options and currentActionSet as the script (OBJLIB 5: read where the C read them, written at once, the
 * option rows of optionsChar and optionsShip put into options as the objects they are). The helpers of the pad menu
 * the script calls through its globals are called so through their refs (PORT_OWN: directly while the global holds
 * the port's function, else by name; PlaceMenuOption, SetOptionValue, ShowPadConfig, FillControllerMenu), and
 * Menu.YesNoMenu through the module (PORT_CALL, a failure printed); the scorer and engine calls the C made by name go
 * through their seams (SScorer.Get, Set and Add, Scrap.Get and Set, SInput.ResetToDefault; OBJLIB 7); the .def runner
 * takes Python values (OBJLIB 6).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#include "game/Script/levels/Levels.h"

static const char options_def[] = {
#embed "../../../data/menus/options.def"
	, 0 };

#define OPTIONS_FILE "menus/options.def"
#define KEYS_MAX 24
#define PM "PCMenu"

static struct {
	int id;				/* the player of the keys menu */
	char actions[KEYS_MAX][0x40];	/* its actions, "Racer.Forward" */
	int numActions;
	char keys[0x800];		/* the keys of the last action read, kept when a read fails (as the script) */
} controls;


/* "Set.Action" into set and the action; 0 without a dot */
static byte Controls_Split(const char *control, char *set, int size, const char **action) {
	const char *dot = strchr(control, '.');
	if (!dot || dot - control >= size) {
		return 0;
	}
	Str_Copy(set, control, (int)(dot - control) + 1);
	*action = dot + 1;
	return 1;
}

static void Controls_SetColor(int id, const char *item, int r, int g, int b) {
	Script_SetNum(id, item, "Red", r);
	Script_SetNum(id, item, "Green", g);
	Script_SetNum(id, item, "Blue", b);
}

/* ---- the keys of the actions ---- */

/* the keys of each action right of it; four keys are the most an action takes */
static void Controls_Refresh(void) {
	static const struct {
		const char *device;
		char symbol;
	} devices[] = { { "Mouse", '\2' }, { "Kb", '\3' }, { "Joy1", '\4' }, { "Joy2", '\5' }, { "Joy3", '\6' }, { "Joy4", '\7' } };
	int id = controls.id;

	for (int i = 0; i < controls.numActions; i++) {
		const char *control = controls.actions[i];
		char set[0x40], item[0x48], text[0x800];
		const char *action;
		if (!Controls_Split(control, set, sizeof(set), &action)) {
			continue;
		}
		char buf[sizeof(controls.keys)];
		if (!Input_GetDefinedList(id, set, action, buf, sizeof(buf), 1)) {
			Str_Copy(controls.keys, buf, sizeof(controls.keys));
		}
		/* "Kb:A, Mouse:Button 1": each key as a device symbol and its name */
		int count = 1;
		text[0] = 0;
		for (const char *p = controls.keys; *p;) {
			const char *end = strstr(p, ", ");
			int len = end ? (int)(end - p) : (int)strlen(p);
			const char *colon = memchr(p, ':', len);
			for (size_t d = 0; colon && d < sizeof(devices) / sizeof(devices[0]); d++) {
				if ((int)strlen(devices[d].device) == colon - p && !strncmp(p, devices[d].device, colon - p)) {
					char key[0x100];
					Str_Copy(key, colon + 1, (int)(p + len - colon));
					Str_Printf(text + strlen(text), sizeof(text) - (int)strlen(text), " %c%s", devices[d].symbol, key);
					break;
				}
			}
			if (!end) {
				break;
			}
			count++;
			p = end + 2;
		}
		Str_Printf(item, sizeof(item), "1%s", control);
		Script_SetStr(id, item, "Text", text);
		if (count >= 4) {
			Script_SetStr(id, control, "OnAccept", "Menu.DummyFunc");
			Controls_SetColor(id, item, 128, 128, 128);
			Controls_SetColor(id, control, 128, 128, 128);
		} else {
			Script_SetStr(id, control, "OnAccept", "PCMenu.WaitForKeyPress");
			Controls_SetColor(id, control, 183, 220, 255);
			Controls_SetColor(id, item, 183, 220, 255);
		}
	}
}

static void Step_KeyItems(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	for (int i = 0; i < controls.numActions; i++) {
		const char *control = controls.actions[i];
		char item[0x48];
		Str_Printf(item, sizeof(item), "1%s", control);
		Script_Add(id, item, "Text");
		Script_SetNum(id, item, "X", 300);
		Script_SetNum(id, item, "Y", Script_GetNum(id, control, "Y") + 10);
		Script_SetStr(id, item, "Font", "Terminal8");
		Script_SetStr(id, item, "Align", "Left");
		Script_SetStr(id, control, "OnDelete", "PCMenu.ClearKeyEntry");
	}
	Controls_Refresh();
}

static void Controls_AbortWait(void) {
	Script_SetCursor(controls.id, "Cursor");
	Input_AbortListenToDefine();
	GVar_SetStr("EscapeEvent", "Menu.Init");
	Input_SelectActionSetNamed("Menu");
	Script_SetCursor(controls.id, "Cursor");
	Controls_Refresh();
}

/* ---- the pad ---- */

typedef struct {
	const char *item;		/* the value right of the option */
	const char *var;		/* the engine variable it switches */
	const char *on, *off;		/* the texts for 1 and for the rest */
} padOption_t;

static const padOption_t pad_char[4] = {
	{ "ConfigurationTypeChar", "XBCameraAutoPad", "XBox_Menu_Controller_CameraType_Option1",
		"XBox_Menu_Controller_CameraType_Option2" },
	{ "InvertYChar", "XBInvertCharYPad", "Menu_Question_Yes", "Menu_Question_No" },
	{ "DigitalCrossChar", "XBCDigitalAsAnalog", "XBox_Menu_Controller_DigitalCross_Char_Option1",
		"XBox_Menu_Controller_DigitalCross_Char_Option2" },
	{ "InvertTriggersChar", "XBInvertLRChar", "Menu_Question_Yes", "Menu_Question_No" },
};

static const padOption_t pad_ship[4] = {
	{ "ConfigurationTypeShip", "XBVehicleAsFPS", "XBox_Menu_Controller_Configuration2",
		"XBox_Menu_Controller_Configuration1" },
	{ "InvertYShip", "XBInvertVehicleYPad", "Menu_Question_Yes", "Menu_Question_No" },
	{ "DigitalCrossShip", "XBVDigitalAsAnalog", "XBox_Menu_Controller_DigitalCross_Ship_Option2",
		"XBox_Menu_Controller_DigitalCross_Ship_Option1" },
	{ "InvertTriggersShip", "XBInvertLRVehicle", "Menu_Question_Yes", "Menu_Question_No" },
};

/* keys.sort() of the script on a native list (cmp of 1.5.2; the keys of a dict are never equal) */
static void Pad_Sort(nativeVal_t *list) {
	for (int i = 1; i < list->n; i++) {
		nativeVal_t x = list->items[i];
		int j = i;
		for (; j > 0 && Native_Compare(&list->items[j - 1], &x) > 0; j--) {
			list->items[j] = list->items[j - 1];
		}
		list->items[j] = x;
	}
}

/* options[key][1] of the module variable options, read as the script reads its global */
static byte Pad_OptionName(const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t opts = { 0 }, row = { 0 }, one;
	Native_SetInt(&one, 1);
	byte ok = Mode_G(PM, "options", &opts) && Mode_Sub(&opts, key, &row) && Mode_Sub(&row, &one, out);
	Native_Free(&opts);
	Native_Free(&row);
	return ok;
}

/*
 * The option rows of the pad menu as FillControllerMenu of the script makes them, from the module variable options
 * (optionsChar or optionsShip, which the menu functions put there): its keys sorted, then for each
 * PlaceMenuOption(id, i, options[key][1]) and SetOptionValue(id, options[key][1]) of the module; an error stops the
 * step (left set for the runner)
 */
static void Step_PadOptions(int id, const nativeVal_t *args, int n) {
	nativeVal_t keys = { 0 };
	(void)args;
	(void)n;
	if (!Mode_Keys(PM, "options", 1, &keys)) {
		return;
	}
	if (keys.type != NV_LIST) {
		Native_Raise(NE_ATTRIBUTE, "sort");
		Native_Free(&keys);
		return;
	}
	Pad_Sort(&keys);
	for (int i = 0; i < keys.n; i++) {
		nativeVal_t name = { 0 };
		byte ok = Pad_OptionName(&keys.items[i], &name) &&
			PORT_OWN(PCMenu, PlaceMenuOption, NULL, "iiv", (long)id, (long)(i + 1), &name);
		Native_Free(&name);
		ok = ok && Pad_OptionName(&keys.items[i], &name) && PORT_OWN(PCMenu, SetOptionValue, NULL, "iv", (long)id, &name);
		Native_Free(&name);
		if (!ok) {
			break;
		}
	}
	Native_Free(&keys);
}

static void Pad_Show(int id, const char *item, const char *key, int x) {
	if (key) {
		Script_SetStr(id, item, "Text", Script_Lang(key));
	}
	if (x) {
		Script_SetNum(id, item, "X", x);
	}
	Script_SetNum(id, item, "Visible", 1);
}

/*
 * The lines to the digital cross as ShowPadConfig of the script hides them after the picture (which shows all three):
 * the straight line when the cross is analog in the action set (Racer or Walk), else the two of the bend
 */
static void Pad_Lines(int id, const char *set) {
	if ((!strcmp(set, "Racer") && GVar_GetNum("XBVDigitalAsAnalog", 0) == 1) ||
		(!strcmp(set, "Walk") && GVar_GetNum("XBCDigitalAsAnalog", 0) == 1)) {
		Script_SetNum(id, "DCrossLine", "Visible", 0);
	} else {
		Script_SetNum(id, "DCrossLine1", "Visible", 0);
		Script_SetNum(id, "DCrossLine2", "Visible", 0);
	}
}

/* UpdatePadText of the script: the actions of the buttons and sticks of the action set */
static void Pad_Text(int id, const char *set) {
	static const struct {
		const char *button;
		char symbol;
		byte right;		/* the text left of the symbol */
	} buttons[] = { { "BACK", 0x1b, 1 }, { "WHITE", 0x1c, 0 }, { "X", 0x16, 0 }, { "Y", 0x17, 0 }, { "L", 0x18, 1 },
		{ "BLACK", 0x1d, 0 }, { "B", 0x15, 0 }, { "R", 0x19, 0 }, { "START", 0x1a, 1 }, { "A", 0x14, 0 } };
	static const char *const sticks[] = { "DCrossLR", "DCrossUD", "LeftThumbstickLR", "LeftThumbstickUD",
		"RightThumbstickLR", "RightThumbstickUD" };
	byte walk = !strcmp(set, "Walk");

	for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
		char key[0x80], text[0x100];
		int index;
		char *name = "";
		Input_GetEntry("Pad1", buttons[i].button, set, &index, &name);
		Str_Printf(key, sizeof(key), "XBox_Menu_Controller_%s", name);
		const char *desc = Script_Lang(key);
		if (buttons[i].right) {
			Str_Printf(text, sizeof(text), "%s %c", desc, buttons[i].symbol);
		} else {
			Str_Printf(text, sizeof(text), "%c %s", buttons[i].symbol,
				*desc ? desc : Script_Lang("XBox_Menu_Controller_NoAction"));
		}
		Script_SetStr(id, buttons[i].button, "Text", text);
	}

	for (size_t i = 0; i < sizeof(sticks) / sizeof(sticks[0]); i++) {
		char item[0x40];
		Str_Printf(item, sizeof(item), "Text%s", sticks[i]);
		Script_SetNum(id, sticks[i], "Visible", 0);
		Script_SetNum(id, item, "Visible", 0);
	}
	if (walk) {
		Pad_Show(id, "TextLeftThumbstickLR", "XBox_Menu_Controller_LeftThumbstick_Char_Option", 250);
		Pad_Show(id, "TextRightThumbstickLR", "XBox_Menu_Controller_RightThumbstick_Char_Option", 345);
		if (GVar_GetNum("XBCDigitalAsAnalog", 0) == 0) {
			Pad_Show(id, "TextDCrossLR", "XBox_Menu_Controller_DigitalCross_Char_Option2", 295);
		}
		return;
	}
	if (GVar_GetNum("XBVDigitalAsAnalog", 0) == 0) {
		Pad_Show(id, "DCrossLR", NULL, 0);
		Pad_Show(id, "TextDCrossLR", "XBox_Menu_Controller_DigitalCross_Ship_LR", 0);
		Pad_Show(id, "DCrossUD", NULL, 0);
		Pad_Show(id, "TextDCrossUD", "XBox_Menu_Controller_DigitalCross_Ship_UD", 0);
	}
	if (GVar_GetNum("XBVehicleAsFPS", 0) == 0) {
		Pad_Show(id, "RightThumbstickLR", NULL, 0);
		Pad_Show(id, "TextRightThumbstickLR", "XBox_Menu_Controller_RightThumbstick_Ship_Option1_LR", 0);
		Pad_Show(id, "RightThumbstickUD", NULL, 0);
		Pad_Show(id, "TextRightThumbstickUD", "XBox_Menu_Controller_RightThumbstick_Ship_Option1_UD", 0);
		Pad_Show(id, "TextLeftThumbstickLR", "XBox_Menu_Controller_LeftThumbstick_Ship_Option1", 250);
	} else {
		Pad_Show(id, "LeftThumbstickLR", NULL, 0);
		Pad_Show(id, "TextLeftThumbstickLR", "XBox_Menu_Controller_LeftThumbstick_Ship_Option2_LR", 0);
		Pad_Show(id, "LeftThumbstickUD", NULL, 0);
		Pad_Show(id, "TextLeftThumbstickUD", "XBox_Menu_Controller_LeftThumbstick_Ship_Option2_UD", 0);
		Pad_Show(id, "TextRightThumbstickLR", "XBox_Menu_Controller_RightThumbstick_Ship_Option2", 345);
	}
}

/* UpdatePadText(id, set) for any set: the texts of a text, else the TypeError where the script stops (the first
 * SInput.GetEntry("Pad1", button, set), through its ref for its error); 0 with the error set */
static byte Pad_TextOf(int id, const nativeVal_t *set) {
	if (set->type == NV_STR) {
		Pad_Text(id, Native_Text(set));
		return 1;
	}
	if (PORT_CALL(SInput, GetEntry, NULL, "ssv", "Pad1", "BACK", set)) {
		return Native_Raise(NE_TYPE, "UpdatePadText: the action set is not a text");
	}
	return 0;
}

/* padtext <set>: the end of ShowPadConfig after the picture, the lines and the texts of the action set */
static void Step_PadText(int id, const nativeVal_t *args, int n) {
	const char *set;
	if (Mode_Parse(args, n, "s", &set)) {
		Pad_Lines(id, set);
		Pad_Text(id, set);
	}
}

/* ---- the mouse ---- */

/* the slider value (0..10) of a sensitivity and back: 0.2..1 on the first half, 1..9 on the second */
static double Mouse_SensToValue(double sens) {
	return sens <= 1 ? (sens - 0.2) * 6.25 : 5 + (sens - 1) / 1.6;
}

static double Mouse_ValueToSens(double value) {
	return value <= 5 ? 0.2 + value / 6.25 : 1 + (value - 5) * 1.6;
}

static void Mouse_Apply(int id) {
	GVar_SetNum("MouseSensitivityH", Mouse_ValueToSens(Script_GetNum(id, "SensitivitySliderX", "Value")));
	GVar_SetNum("MouseSensitivityV", Mouse_ValueToSens(Script_GetNum(id, "SensitivitySliderY", "Value")));
}

/* the script switched the variable twice to show it: 1 shows Yes, anything else becomes 0 and shows No */
static void Step_Invert(int id, const nativeVal_t *args, int n) {
	const char *item;
	if (!Mode_Parse(args, n, "s", &item)) {
		Mode_PrintError();
		return;
	}
	byte yes = GVar_GetNum(item, 0) == 1;
	GVar_SetNum(item, yes);
	Script_SetStr(id, item, "Text", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
}

static void Step_Sensitivity(int id, const nativeVal_t *args, int n) {
	const char *item, *var;
	if (!Mode_Parse(args, n, "ss", &item, &var)) {
		Mode_PrintError();
		return;
	}
	Script_SetNum(id, item, "Value", Mouse_SensToValue(GVar_GetNum(var, 0)));
}

/* ---- the menus ---- */

static const scriptStep_t controls_steps[] = {
	{ "keyitems", Step_KeyItems },
	{ "padoptions", Step_PadOptions },
	{ "padtext", Step_PadText },
	{ "invert", Step_Invert },
	{ "sensitivity", Step_Sensitivity },
	{ NULL, NULL },
};

/* a section of options.def with its $names: n pairs of a name and a value (Mode_DefRun; OBJLIB 6: the runner takes
 * them as a dict) */
#define Controls_RunMenu(id, section, n, ...) \
	Mode_DefRun(id, OPTIONS_FILE, section, options_def, controls_steps, 0, n __VA_OPT__(, ) __VA_ARGS__)

/*
 * FillControllerMenu(id, control, tab) of the script, the section pad: the rows of the module variable options, the
 * pad on foot for the tab Char and in a ship for any other, the tabs with tab chosen and the links to Char or Ship;
 * it stops where the script stops (0 with the error set)
 */
static byte Pad_Fill(int id, const char *tab) {
	byte chr = !strcmp(tab, "Char");
	modeVals_t t = { .n = 0 };
	byte ok = Mode_DefRun(id, OPTIONS_FILE, "pad", options_def, controls_steps, 1, 3, "tab", Mode_S(&t, tab), "link",
		Mode_S(&t, chr ? "Char" : "Ship"), "set", Mode_S(&t, chr ? "Walk" : "Racer"));
	return Mode_Done(&t, ok);
}

#define CONTROLS_ARGS(a, n) \
	int id; \
	const char *control; \
	if (!Mode_IdControl(a, n, &id, &control)) { \
		return 0; \
	}

/* a function (id, control) that runs a section */
#define CONTROLS_MENU(func, section) MODE_IDFN(func, (Controls_RunMenu(id, section, 0), 1))

CONTROLS_MENU(PCMenu_OptionsMenu, "Options")
CONTROLS_MENU(PCMenu_ControlsMenu, "Controls")
CONTROLS_MENU(PCMenu_MouseControlsMenu, "Mouse")
CONTROLS_MENU(PCMenu_RacerMoveControlsMenu, "Ship")
CONTROLS_MENU(PCMenu_RacerAttackControlsMenu, "Attack")
CONTROLS_MENU(PCMenu_WalkCharControlsMenu, "Character")
CONTROLS_MENU(PCMenu_UseCharControlsMenu, "Action")

/* CreateControlMenu(id, title, menu, backfunc, tab): the keys of the actions of menu ([text, func, action] each); 0
 * with the error set */
static byte Controls_CreateMenu(int id, const nativeVal_t *title, const nativeVal_t *menu, const char *back,
	const char *tab) {
	int n;
	if (!Native_Len(menu, &n) || n <= 0) {
		return Native_Raise(NE_VALUE, "CreateControlMenu: no actions");
	}
	controls.id = id;
	controls.numActions = 0;
	controls.keys[0] = 0;
	nativeVal_t first = { 0 };
	byte found = 0;
	for (int i = 0; i < n && controls.numActions < KEYS_MAX; i++) {
		nativeVal_t et = { 0 }, at = { 0 };
		const nativeVal_t *e = Native_GetItem(menu, i, 0, &et);
		const nativeVal_t *a = e ? Native_GetItem(e, 2, 0, &at) : NULL;
		if (a && a->type == NV_STR) {
			Str_Copy(controls.actions[controls.numActions++], Native_Text(a), sizeof(controls.actions[0]));
			if (!found) {
				Native_Copy(&first, a);
				found = 1;
			}
		}
		/* OBJLIB 7: the error of an entry without an action dropped */
		Mode_DropError();
		Native_Free(&at);
		Native_Free(&et);
	}
	if (!found) {
		return Native_Raise(NE_VALUE, "CreateControlMenu: the entries need an action");
	}
	modeVals_t t = { .n = 0 };
	Controls_RunMenu(id, "keys", 5, "title", title, "menu", menu, "back", Mode_S(&t, back), "tab", Mode_S(&t, tab),
		"first", &first);
	Native_Free(&first);
	return Mode_Done(&t, 1);
}

static byte PCMenu_CreateControlMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *title, *menu;
	const char *back, *tab;
	nativeVal_t m;
	(void)out;
	if (!Mode_Parse(a, n, "iOOss", &id, &title, &menu, &back, &tab)) {
		return 0;
	}
	/* the menu list is read item by item (Mode_ElemsIn): a copy */
	Native_Copy(&m, menu);
	Mode_ElemsIn(&m);
	byte ok = Controls_CreateMenu(id, title, &m, back, tab);
	Native_Free(&m);
	return ok;
}
MODE_ADAPTER(PCMenu, CreateControlMenu)

static byte PCMenu_RefreshControlMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Controls_Refresh();
	return 1;
}
MODE_ADAPTER(PCMenu, RefreshControlMenu)

static byte PCMenu_ClearKeyEntry_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	char set[0x40];
	const char *action;
	if (Controls_Split(control, set, sizeof(set), &action)) {
		Input_ClearDefinedList(id, set, action, 1);
	}
	Controls_Refresh();
	return 1;
}
MODE_ADAPTER(PCMenu, ClearKeyEntry)

/* the next key pressed is added to the action; Escape aborts */
static byte PCMenu_WaitForKeyPress_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	char set[0x40], item[0x48];
	const char *action;
	Str_Printf(item, sizeof(item), "1%s", control);
	Controls_SetColor(id, item, 255, 241, 174);
	Controls_SetColor(id, control, 255, 241, 174);
	Script_SetCursor(id, "");
	Script_SetNum(id, "Cursor", "Visible", 0);
	if (Controls_Split(control, set, sizeof(set), &action)) {
		Input_ListenToDefine(id, set, action, "PCMenu.KetRedefineFunc");
	}
	GVar_SetStr("EscapeEvent", "PCMenu.AbortWaitRedefine");
	Input_SelectActionSetNamed("Inactive");
	return 1;
}
MODE_ADAPTER(PCMenu, WaitForKeyPress)

/* AbortWaitRedefine(id) and KetRedefineFunc() (sic): the key is defined or Escape was pressed */
static byte PCMenu_AbortWaitRedefine_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Controls_AbortWait();
	return 1;
}
MODE_ADAPTER(PCMenu, AbortWaitRedefine)

static byte PCMenu_ResetEverything_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const char *const pad_vars[] = { "XBInvertCharYPad", "XBCDigitalAsAnalog", "XBCameraAutoPad",
		"XBInvertLRChar", "XBInvertVehicleYPad", "XBVDigitalAsAnalog", "XBVehicleAsFPS", "XBInvertLRVehicle" };
	(void)out;
	CONTROLS_ARGS(a, n);
	(void)control;
	Input_ResetToDefault();
	GVar_SetNum("InvertMouse", 0);
	GVar_SetNum("MouseSensitivityH", 1);
	GVar_SetNum("MouseSensitivityV", 1);
	for (size_t i = 0; i < sizeof(pad_vars) / sizeof(pad_vars[0]); i++) {
		GVar_SetNum(pad_vars[i], GVar_GetNum(pad_vars[i], 1));
	}
	Controls_RunMenu(id, "Controls", 0);
	return 1;
}
MODE_ADAPTER(PCMenu, ResetEverything)

static byte PCMenu_ResetToDefault_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	(void)control;
	MODE_PRINTED(Menu, YesNoMenu, "isss", (long)id, Script_Lang("Menu_Controls_ResetToDefaultQuestion"),
		"PCMenu.ResetEverything", "PCMenu.ControlsMenu");
	return 1;
}
MODE_ADAPTER(PCMenu, ResetToDefault)

/* OptionYesNo(id, control): switches the engine variable named control between 0 and 1 */
static void Controls_YesNo(int id, const char *control) {
	byte yes = GVar_GetNum(control, 0) != 1;
	GVar_SetNum(control, yes);
	Script_SetStr(id, control, "Text", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
}

static byte PCMenu_OptionYesNo_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	Controls_YesNo(id, control);
	return 1;
}
MODE_ADAPTER(PCMenu, OptionYesNo)

static byte PCMenu_InvertMouse_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	(void)control;
	Controls_YesNo(id, "InvertMouse");
	return 1;
}
MODE_ADAPTER(PCMenu, InvertMouse)

/* AlwaysRun(id, control): not in a menu of the game */
static byte PCMenu_AlwaysRun_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	char text[0x80];
	byte yes = GVar_GetNum("AlwaysRun", 0) == 0;
	GVar_SetNum("AlwaysRun", yes);
	Str_Printf(text, sizeof(text), "Always Run  %s", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
	Script_SetStr(id, control, "Text", text);
	return 1;
}
MODE_ADAPTER(PCMenu, AlwaysRun)

static byte PCMenu_MouseSensitivityChange_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	CONTROLS_ARGS(a, n);
	(void)control;
	Mouse_Apply(id);
	return 1;
}
MODE_ADAPTER(PCMenu, MouseSensitivityChange)

/* the dummies left and right of a sensitivity title step its slider and give the focus back; 0 with the error */
static byte Mouse_StepSlider(int id, const char *prev, double sign) {
	const char *slider = !strcmp(prev, "Item2") ? "SensitivitySliderX" : !strcmp(prev, "Item3") ? "SensitivitySliderY" : NULL;
	/* without the slider the script failed on None + None, before it moved the focus */
	if (!slider || !Script_HasItem(id, slider)) {
		return Native_Raise(NE_TYPE, sign > 0 ? "bad operand type(s) for +" : "bad operand type(s) for -");
	}
	double v = Script_GetNum(id, slider, "Value");
	Script_SetNum(id, slider, "Value", v + sign * Script_GetNum(id, slider, "ValueStep"));
	Mouse_Apply(id);
	Script_SetDefault(id, prev);
	return 1;
}

static byte Mouse_Step(const nativeVal_t *a, int n, double sign) {
	int id;
	const char *control, *prev;
	if (!Mode_Parse(a, n, "iss", &id, &control, &prev)) {
		return 0;
	}
	return Mouse_StepSlider(id, prev, sign);
}

static byte PCMenu_MouseSensitivitySliderFocus_Sub_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Mouse_Step(a, n, -1.0);
}
MODE_ADAPTER(PCMenu, MouseSensitivitySliderFocus_Sub)

static byte PCMenu_MouseSensitivitySliderFocus_Add_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Mouse_Step(a, n, 1.0);
}
MODE_ADAPTER(PCMenu, MouseSensitivitySliderFocus_Add)

/*
 * The pad menus as the script opens them: `global options, currentActionSet`, the action set (NULL: already set) and
 * the rows of the module (optionsChar or optionsShip, the object itself) into them, then FillControllerMenu(id,
 * control, tab) of the module. Each global is stored as often as the script stores it (a store may resize the dict of
 * the module). 0 with the error set
 */
static byte Pad_Open(int id, const nativeVal_t *control, const char *set, byte ship) {
	nativeVal_t s;
	if (set) {
		Native_SetStr(&s, set);
		if (!Mode_Put(PM, "currentActionSet", NULL, &s)) {
			return 0;
		}
	}
	return LvN_ModSetGlobal(PM, "options", PM, ship ? "optionsShip" : "optionsChar") &&
		PORT_OWN(PCMenu, FillControllerMenu, NULL, "ivs", (long)id, control, ship ? "Ship" : "Char");
}

/* ControllerConfig: the pad menu of the mode the player was in (Menu.LastMode Walk: on foot, else in a ship) */
static byte Pad_Config(int id, const nativeVal_t *control) {
	nativeVal_t mode, walk;
	Native_SetStr(&walk, "Walk");
	/* OBJLIB 6: import Menu */
	if (!Mode_Import("Menu") || !LvN_ModGet("Menu", "LastMode", &mode)) {
		return 0;
	}
	int w = Mode_Put(PM, "currentActionSet", NULL, &mode) ? Mode_Cmp(&mode, "==", &walk) : -1;
	Native_Free(&mode);
	return w < 0 ? 0 : w ? Pad_Open(id, control, NULL, 0) : Pad_Open(id, control, "Racer", 1);
}

/* the adapters of the functions (id, control) with control any value */
#define PAD_FN(name, call) \
	static byte PCMenu_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		int id; \
		const nativeVal_t *c; \
		(void)out; \
		return Mode_Parse(a, n, "iO", &id, &c) && (call); \
	} \
	MODE_ADAPTER(PCMenu, name)

PAD_FN(ControllerConfig, Pad_Config(id, c))
PAD_FN(ControllerConfigCharMenu, Pad_Open(id, c, "Walk", 0))
PAD_FN(ControllerConfigShipMenu, Pad_Open(id, c, "Racer", 1))

/* FillControllerMenu(id, control, actualTab): the pad menu of the tab with the option rows of the module variable
 * options, which the callers set to match the tab */
static byte PCMenu_FillControllerMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *control;
	const char *tab;
	int id;
	(void)out;
	return Mode_Parse(a, n, "iOs", &id, &control, &tab) && Pad_Fill(id, tab);
}
MODE_ADAPTER(PCMenu, FillControllerMenu)

/*
 * The helpers of the pad menu in pcmenu.py, called through the module as the script calls them (the step padoptions:
 * PlaceMenuOption and SetOptionValue; SwitchOption: SetOptionValue and ShowPadConfig).
 */

/* PlaceMenuOption(id, n, controlName): the option Item<n> left of the middle, the value controlName right of it; 0
 * with the error set */
static byte Pad_PlaceOption(int id, const nativeVal_t *n, const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *item = Mode_V(&t), *w = Mode_V(&t), *d = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
	const nativeVal_t *idv = Mode_I(&t, id), *xm = Mode_I(&t, 330);
	Mode_Repr(n, rep);
	byte ok = Mode_Cat(&t, item, 2, Mode_S(&t, "Item"), rep) && Mode_ScorerGetVal(idv, item, "W", w) &&
		Native_Arith('-', xm, w, d) && Native_Arith('-', d, Mode_I(&t, 5), x) && Mode_ScorerGetVal(idv, item, "Y", y) &&
		Mode_ScorerSet(idv, item, "X", x) && Mode_ScorerAddV(idv, name, "Text") &&
		Mode_ScorerSet(idv, name, "Font", Mode_S(&t, "Horatio")) && Mode_ScorerSet(idv, name, "X", xm) &&
		Mode_ScorerSet(idv, name, "Y", y) && Mode_ScorerSet(idv, name, "Visible", Mode_I(&t, 1)) &&
		Mode_ScorerSet(idv, name, "Red", Mode_I(&t, 183)) && Mode_ScorerSet(idv, name, "Green", Mode_I(&t, 220)) &&
		Mode_ScorerSet(idv, name, "Blue", Mode_I(&t, 255));
	return Mode_Done(&t, ok);
}

static byte PCMenu_PlaceMenuOption_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *num, *name;
	int id;
	(void)out;
	return Mode_Parse(a, n, "iOO", &id, &num, &name) && Pad_PlaceOption(id, num, name);
}
MODE_ADAPTER(PCMenu, PlaceMenuOption)

/* SetOptionValue(id, control): the text of the value of a pad option (an unknown control does nothing) */
static void Pad_SetValue(int id, const char *control) {
	for (int i = 0; i < 8; i++) {
		const padOption_t *o = i < 4 ? &pad_char[i] : &pad_ship[i - 4];
		if (!strcmp(o->item, control)) {
			/* the script tests == 1, for ConfigurationTypeShip == 0 with the texts the other way round */
			double v = GVar_GetNum(o->var, 0);
			byte on = o == &pad_ship[0] ? v != 0 : v == 1;
			Script_SetStr(id, control, "Text", Script_Lang(on ? o->on : o->off));
			break;
		}
	}
}

MODE_IDFN(PCMenu_SetOptionValue, (Pad_SetValue(id, control), 1))

/* UpdatePadText(id, ActionSet): the texts of the buttons and sticks of the pad picture */
PAD_FN(UpdatePadText, Pad_TextOf(id, c))

/*
 * ShowPadConfig(id, ActionSet, YOffset=0): the pad picture (the section padpicture, the items moved down by YOffset)
 * with the lines to the digital cross as the script shows them, then UpdatePadText (which raises for a set that is
 * not a text, after the picture as in the script)
 */
static byte Pad_ShowConfig(int id, const nativeVal_t *set, double yoff) {
	/* the items of padpicture, which the script places at their Y + YOffset */
	static const char *const items[] = { "BACK", "WHITE", "X", "Y", "L", "BLACK", "B", "R", "START", "A",
		"TextRightThumbstickLR", "TextRightThumbstickUD", "TextLeftThumbstickLR", "TextLeftThumbstickUD", "TextDCrossLR",
		"TextDCrossUD", "RStickLine", "LStickLine", "LTrigger", "RTrigger", "DCrossLine", "DCrossLine1", "DCrossLine2",
		"Pad", "DCrossLR", "DCrossUD", "LeftThumbstickLR", "LeftThumbstickUD", "RightThumbstickLR",
		"RightThumbstickUD" };
	Controls_RunMenu(id, "padpicture", 0);
	for (size_t i = 0; yoff != 0 && i < sizeof(items) / sizeof(items[0]); i++) {
		Script_SetNum(id, items[i], "Y", Script_GetNum(id, items[i], "Y") + yoff);
	}
	Pad_Lines(id, set->type == NV_STR ? Native_Text(set) : "");
	return Pad_TextOf(id, set);
}

static byte PCMenu_ShowPadConfig_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *set;
	double yoff = 0;
	int id;
	(void)out;
	return Mode_Parse(a, n, "iO|d", &id, &set, &yoff) && Pad_ShowConfig(id, set, yoff);
}

/* OBJLIB 7: the keyword adapter (the keywords and the errors of PyArg_ParseTupleAndKeywords as before), then the
 * entry by position */
static PyObject *PCMenu_ShowPadConfig(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "id", "ActionSet", "YOffset", NULL };
	int id;
	PyObject *set;
	double yoff = 0;
	nativeVal_t a[3], out;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iO|d", kwlist, &id, &set, &yoff)) {
		return NULL;
	}
	Native_SetInt(&a[0], id);
	Native_Arg(set, &a[1]);
	Native_SetFloat(&a[2], yoff);
	Native_SetNone(&out);
	return Mode_KwOut(a, 3, PCMenu_ShowPadConfig_E(a, 3, &out), &out);
}

/*
 * SwitchOption(id, control): the option control (Item1..Item4) of the module variable options, (variable, value
 * item), switched between 0 and 1 (0 becomes 1, anything else 0), the keys of the pad reset, the value and the pad
 * picture of currentActionSet shown again; 0 with the error set
 */
static byte Pad_SwitchOption(int id, const nativeVal_t *control) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *opts = Mode_V(&t), *row = Mode_V(&t), *val = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t opt[2] = { { 0 }, { 0 } };
	const char *name;
	if (!Mode_Put(PM, "SaveOptions", NULL, Mode_I(&t, 1)) || !Mode_G(PM, "options", opts) || !Mode_Sub(opts, control, row)) {
		return Mode_Done(&t, 0);
	}
	Mode_Elems(row);
	int off = -1;
	byte ok = Mode_Unpack(row, 2, opt) && Mode_GVarGetV(&opt[0], val) && (off = Mode_CmpI(val, "==", 0)) >= 0;
	if (ok) {
		/* Scrap.Set(name, off == 1); a name a swapped Scrap.Get let through goes through the ref for its error */
		ok = opt[0].type == NV_STR && strlen(Native_Text(&opt[0])) == (size_t)opt[0].n ?
			(name = Native_Text(&opt[0]), Mode_GVarSetI(name, off == 1)) :
			PORT_CALL(Scrap, Set, NULL, "vi", &opt[0], (long)(off == 1));
	}
	ok = ok && Mode_ResetKeys() && PORT_OWN(PCMenu, SetOptionValue, NULL, "iv", (long)id, &opt[1]) &&
		Mode_G(PM, "currentActionSet", cur) && PORT_OWN(PCMenu, ShowPadConfig, NULL, "iv", (long)id, cur);
	Native_Free(&opt[0]);
	Native_Free(&opt[1]);
	return Mode_Done(&t, ok);
}

PAD_FN(SwitchOption, Pad_SwitchOption(id, c))

PyMethodDef g_MenuControlsMethods[] = {
	{ "OptionsMenu", PCMenu_OptionsMenu, 1, NULL },
	{ "ControlsMenu", PCMenu_ControlsMenu, 1, NULL },
	{ "MouseControlsMenu", PCMenu_MouseControlsMenu, 1, NULL },
	{ "RacerMoveControlsMenu", PCMenu_RacerMoveControlsMenu, 1, NULL },
	{ "RacerAttackControlsMenu", PCMenu_RacerAttackControlsMenu, 1, NULL },
	{ "WalkCharControlsMenu", PCMenu_WalkCharControlsMenu, 1, NULL },
	{ "UseCharControlsMenu", PCMenu_UseCharControlsMenu, 1, NULL },
	{ "CreateControlMenu", PCMenu_CreateControlMenu, 1, NULL },
	{ "RefreshControlMenu", PCMenu_RefreshControlMenu, 1, NULL },
	{ "ClearKeyEntry", PCMenu_ClearKeyEntry, 1, NULL },
	{ "WaitForKeyPress", PCMenu_WaitForKeyPress, 1, NULL },
	{ "AbortWaitRedefine", PCMenu_AbortWaitRedefine, 1, NULL },
	{ "KetRedefineFunc", PCMenu_AbortWaitRedefine, 1, NULL },
	{ "ResetEverything", PCMenu_ResetEverything, 1, NULL },
	{ "ResetToDefault", PCMenu_ResetToDefault, 1, NULL },
	{ "OptionYesNo", PCMenu_OptionYesNo, 1, NULL },
	{ "InvertMouse", PCMenu_InvertMouse, 1, NULL },
	{ "AlwaysRun", PCMenu_AlwaysRun, 1, NULL },
	{ "MouseSensitivityChange", PCMenu_MouseSensitivityChange, 1, NULL },
	{ "MouseSensitivitySliderFocus_Sub", PCMenu_MouseSensitivitySliderFocus_Sub, 1, NULL },
	{ "MouseSensitivitySliderFocus_Add", PCMenu_MouseSensitivitySliderFocus_Add, 1, NULL },
	{ "ControllerConfig", PCMenu_ControllerConfig, 1, NULL },
	{ "ControllerConfigCharMenu", PCMenu_ControllerConfigCharMenu, 1, NULL },
	{ "ControllerConfigShipMenu", PCMenu_ControllerConfigShipMenu, 1, NULL },
	{ "FillControllerMenu", PCMenu_FillControllerMenu, 1, NULL },
	{ "PlaceMenuOption", PCMenu_PlaceMenuOption, 1, NULL },
	{ "SetOptionValue", PCMenu_SetOptionValue, 1, NULL },
	{ "ShowPadConfig", (PyCFunction)(void (*)(void))PCMenu_ShowPadConfig, 3, NULL },
	{ "UpdatePadText", PCMenu_UpdatePadText, 1, NULL },
	{ "SwitchOption", PCMenu_SwitchOption, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MenuControls)
