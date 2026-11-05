/*
 * The options and controls menus of PCMenu in C (issue #2, step 2): OptionsMenu, ControlsMenu, the mouse, the keys of
 * the actions and the pad. The layout is data/menus/options.def, one section per menu.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

static const char options_def[] = {
#embed "../../../data/menus/options.def"
	, 0 };

#define OPTIONS_FILE "menus/options.def"
#define KEYS_MAX 24

static struct {
	int id;				/* the player of the keys menu */
	char actions[KEYS_MAX][0x40];	/* its actions, "Racer.Forward" */
	int numActions;
	char keys[0x800];		/* the keys of the last action read, kept when a read fails (as the script) */
	const char *actionSet;		/* the pad menu: Walk or Racer */
} controls;

static void Controls_RunMenu(int id, const char *section, PyObject *vars);

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

static void Step_KeyItems(int id, PyObject *args) {
	(void)args;
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

static const padOption_t *Pad_Options(void) {
	return strcmp(controls.actionSet, "Walk") ? pad_ship : pad_char;
}

static void Pad_SetValue(int id, const padOption_t *o) {
	Script_SetStr(id, o->item, "Text", Script_Lang(GVar_GetNum(o->var, 0) == 1 ? o->on : o->off));
}

/* the value of option n (1..4) right of the middle, the option left of it */
static void Step_PadOptions(int id, PyObject *args) {
	(void)args;
	const padOption_t *opts = Pad_Options();
	for (int n = 1; n <= 4; n++) {
		const char *name = opts[n - 1].item;
		char item[0x10];
		Str_Printf(item, sizeof(item), "Item%d", n);
		double y = Script_GetNum(id, item, "Y");
		Script_SetNum(id, item, "X", 330 - Script_GetNum(id, item, "W") - 5);
		Script_Add(id, name, "Text");
		Script_SetStr(id, name, "Font", "Horatio");
		Script_SetNum(id, name, "X", 330);
		Script_SetNum(id, name, "Y", y);
		Script_SetNum(id, name, "Visible", 1);
		Controls_SetColor(id, name, 183, 220, 255);
		Pad_SetValue(id, &opts[n - 1]);
	}
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

/* the actions of the buttons and sticks of the action set, the lines to the digital cross */
static void Step_PadText(int id, PyObject *args) {
	static const struct {
		const char *button;
		char symbol;
		byte right;		/* the text left of the symbol */
	} buttons[] = { { "BACK", 0x1b, 1 }, { "WHITE", 0x1c, 0 }, { "X", 0x16, 0 }, { "Y", 0x17, 0 }, { "L", 0x18, 1 },
		{ "BLACK", 0x1d, 0 }, { "B", 0x15, 0 }, { "R", 0x19, 0 }, { "START", 0x1a, 1 }, { "A", 0x14, 0 } };
	static const char *const sticks[] = { "DCrossLR", "DCrossUD", "LeftThumbstickLR", "LeftThumbstickUD",
		"RightThumbstickLR", "RightThumbstickUD" };
	const char *set = controls.actionSet;
	byte walk = !strcmp(set, "Walk");
	byte digitalAsAnalog = GVar_GetNum(walk ? "XBCDigitalAsAnalog" : "XBVDigitalAsAnalog", 0) == 1;

	(void)args;
	Script_SetNum(id, "DCrossLine", "Visible", !digitalAsAnalog);
	Script_SetNum(id, "DCrossLine1", "Visible", digitalAsAnalog);
	Script_SetNum(id, "DCrossLine2", "Visible", digitalAsAnalog);

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

static void Pad_Run(int id, const char *section) {
	PyObject *vars = Py_BuildValue("{s:s}", "tab", strcmp(controls.actionSet, "Walk") ? "Ship" : "Char");
	Controls_RunMenu(id, section, vars);
	Py_XDECREF(vars);
}

static void Pad_Menu(int id, const char *actionSet) {
	controls.actionSet = actionSet;
	Pad_Run(id, "pad");
}

/* the picture again after an option changed (the items are there, they get their places back) and the texts */
static void Pad_Picture(int id) {
	Pad_Run(id, "padpicture");
	Step_PadText(id, NULL);
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
static void Step_Invert(int id, PyObject *args) {
	const char *item;
	if (!PyArg_ParseTuple(args, "s", &item)) {
		PyErr_Print();
		return;
	}
	byte yes = GVar_GetNum(item, 0) == 1;
	GVar_SetNum(item, yes);
	Script_SetStr(id, item, "Text", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
}

static void Step_Sensitivity(int id, PyObject *args) {
	const char *item, *var;
	if (!PyArg_ParseTuple(args, "ss", &item, &var)) {
		PyErr_Print();
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

static void Controls_RunMenu(int id, const char *section, PyObject *vars) {
	Script_RunFile(id, OPTIONS_FILE, section, options_def, controls_steps, vars);
}

#define CONTROLS_ARGS(args) \
	int id; \
	const char *control; \
	if (!Script_Args(args, &id, &control)) { \
		return NULL; \
	}

/* a function (id, control) that runs a section */
#define CONTROLS_MENU(func, section) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		(void)self; \
		CONTROLS_ARGS(args); \
		(void)control; \
		Controls_RunMenu(id, section, NULL); \
		return Py_ReturnNone(); \
	}

CONTROLS_MENU(PCMenu_OptionsMenu, "Options")
CONTROLS_MENU(PCMenu_ControlsMenu, "Controls")
CONTROLS_MENU(PCMenu_MouseControlsMenu, "Mouse")
CONTROLS_MENU(PCMenu_RacerMoveControlsMenu, "Ship")
CONTROLS_MENU(PCMenu_RacerAttackControlsMenu, "Attack")
CONTROLS_MENU(PCMenu_WalkCharControlsMenu, "Character")
CONTROLS_MENU(PCMenu_UseCharControlsMenu, "Action")

/* CreateControlMenu(id, title, menu, backfunc, tab): the keys of the actions of menu ([text, func, action] each) */
static PyObject *PCMenu_CreateControlMenu(PyObject *self, PyObject *args) {
	int id;
	PyObject *title, *menu;
	char *back, *tab;

	(void)self;
	if (!PyArg_ParseTuple(args, "iOOss", &id, &title, &menu, &back, &tab)) {
		return NULL;
	}
	int n = PySequence_Length(menu);
	if (n <= 0) {
		PyErr_SetString(PyExc_ValueError, "CreateControlMenu: no actions");
		return NULL;
	}
	controls.id = id;
	controls.numActions = 0;
	controls.keys[0] = 0;
	PyObject *first = NULL;
	for (int i = 0; i < n && controls.numActions < KEYS_MAX; i++) {
		PyObject *e = PySequence_GetItem(menu, i);
		PyObject *a = e ? PySequence_GetItem(e, 2) : NULL;
		if (a && PyString_Check(a)) {
			Str_Copy(controls.actions[controls.numActions++], PyString_AsString(a), sizeof(controls.actions[0]));
			if (!first) {
				first = a;
				Py_INCREF(first);
			}
		}
		PyErr_Clear();
		Py_XDECREF(a);
		Py_XDECREF(e);
	}
	if (!first) {
		PyErr_SetString(PyExc_ValueError, "CreateControlMenu: the entries need an action");
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:O,s:O,s:s,s:s,s:O}", "title", title, "menu", menu, "back", back, "tab", tab,
		"first", first);
	Py_DECREF(first);
	Controls_RunMenu(id, "keys", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_RefreshControlMenu(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Controls_Refresh();
	return Py_ReturnNone();
}

static PyObject *PCMenu_ClearKeyEntry(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	char set[0x40];
	const char *action;
	if (Controls_Split(control, set, sizeof(set), &action)) {
		Input_ClearDefinedList(id, set, action, 1);
	}
	Controls_Refresh();
	return Py_ReturnNone();
}

/* the next key pressed is added to the action; Escape aborts */
static PyObject *PCMenu_WaitForKeyPress(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
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
	return Py_ReturnNone();
}

/* AbortWaitRedefine(id) and KetRedefineFunc() (sic): the key is defined or Escape was pressed */
static PyObject *PCMenu_AbortWaitRedefine(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Controls_AbortWait();
	return Py_ReturnNone();
}

static PyObject *PCMenu_ResetEverything(PyObject *self, PyObject *args) {
	static const char *const pad_vars[] = { "XBInvertCharYPad", "XBCDigitalAsAnalog", "XBCameraAutoPad",
		"XBInvertLRChar", "XBInvertVehicleYPad", "XBVDigitalAsAnalog", "XBVehicleAsFPS", "XBInvertLRVehicle" };
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Input_ResetToDefault();
	GVar_SetNum("InvertMouse", 0);
	GVar_SetNum("MouseSensitivityH", 1);
	GVar_SetNum("MouseSensitivityV", 1);
	for (size_t i = 0; i < sizeof(pad_vars) / sizeof(pad_vars[0]); i++) {
		GVar_SetNum(pad_vars[i], GVar_GetNum(pad_vars[i], 1));
	}
	Controls_RunMenu(id, "Controls", NULL);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ResetToDefault(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_Controls_ResetToDefaultQuestion"),
		"PCMenu.ResetEverything", "PCMenu.ControlsMenu");
	return Py_ReturnNone();
}

/* OptionYesNo(id, control): switches the engine variable named control between 0 and 1 */
static PyObject *PCMenu_OptionYesNo(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	byte yes = GVar_GetNum(control, 0) != 1;
	GVar_SetNum(control, yes);
	Script_SetStr(id, control, "Text", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
	return Py_ReturnNone();
}

static PyObject *PCMenu_InvertMouse(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	PyObject *a = Py_BuildValue("(is)", id, "InvertMouse");
	PyObject *r = a ? PCMenu_OptionYesNo(NULL, a) : NULL;
	Py_XDECREF(a);
	return r;
}

/* AlwaysRun(id, control): not in a menu of the game */
static PyObject *PCMenu_AlwaysRun(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	char text[0x80];
	byte yes = GVar_GetNum("AlwaysRun", 0) == 0;
	GVar_SetNum("AlwaysRun", yes);
	Str_Printf(text, sizeof(text), "Always Run  %s", Script_Lang(yes ? "Menu_Question_Yes" : "Menu_Question_No"));
	Script_SetStr(id, control, "Text", text);
	return Py_ReturnNone();
}

static PyObject *PCMenu_MouseSensitivityChange(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Mouse_Apply(id);
	return Py_ReturnNone();
}

/* the dummies left and right of a sensitivity title step its slider and give the focus back */
static PyObject *Mouse_Step(PyObject *args, double sign) {
	int id;
	char *control, *prev;
	if (!PyArg_ParseTuple(args, "iss", &id, &control, &prev)) {
		return NULL;
	}
	const char *slider = !strcmp(prev, "Item2") ? "SensitivitySliderX" : !strcmp(prev, "Item3") ? "SensitivitySliderY" : NULL;
	if (slider) {
		double v = Script_GetNum(id, slider, "Value");
		Script_SetNum(id, slider, "Value", v + sign * Script_GetNum(id, slider, "ValueStep"));
		Mouse_Apply(id);
	}
	Script_SetDefault(id, prev);
	return Py_ReturnNone();
}

static PyObject *PCMenu_MouseSensitivitySliderFocus_Sub(PyObject *self, PyObject *args) {
	(void)self;
	return Mouse_Step(args, -1.0);
}

static PyObject *PCMenu_MouseSensitivitySliderFocus_Add(PyObject *self, PyObject *args) {
	(void)self;
	return Mouse_Step(args, 1.0);
}

/* ControllerConfig: the pad menu of the mode the player was in (on foot or in a ship) */
static PyObject *PCMenu_ControllerConfig(PyObject *self, PyObject *args) {
	char mode[0x20];
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Script_GetString("Menu", "LastMode", mode, sizeof(mode));
	Pad_Menu(id, strcmp(mode, "Walk") ? "Racer" : "Walk");
	return Py_ReturnNone();
}

static PyObject *PCMenu_ControllerConfigCharMenu(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Pad_Menu(id, "Walk");
	return Py_ReturnNone();
}

static PyObject *PCMenu_ControllerConfigShipMenu(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	(void)control;
	Pad_Menu(id, "Racer");
	return Py_ReturnNone();
}

/* SwitchOption(id, control): control is Item1..Item4, the option of the pad menu */
static PyObject *PCMenu_SwitchOption(PyObject *self, PyObject *args) {
	(void)self;
	CONTROLS_ARGS(args);
	if (!controls.actionSet || strncmp(control, "Item", 4) || control[4] < '1' || control[4] > '4' || control[5]) {
		PyErr_SetString(PyExc_KeyError, control);
		return NULL;
	}
	const padOption_t *o = &Pad_Options()[control[4] - '1'];
	Script_SetInt("PCMenu", "SaveOptions", 1);
	GVar_SetNum(o->var, GVar_GetNum(o->var, 0) == 0);
	Input_ResetToDefault();
	Pad_SetValue(id, o);
	Pad_Picture(id);
	return Py_ReturnNone();
}

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
	{ "SwitchOption", PCMenu_SwitchOption, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
