/*
 * The main menus of the Menu module in C (issue #2, step 4b): the main menu, the single player menu, the audio
 * options, the skill, new, saved and loaded games, the death screen, entering and leaving the menu during a game
 * and the scorer of a movie. The layout is data/menus/main.menu.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static const char main_menu[] = {
#embed "../../../data/menus/main.menu"
	, 0 };

#define MAIN_FILE "menus/main.menu"

static const struct {
	const char *slider, *var;
} main_volumes[] = { { "SoundSlider", "SoundMainVolume" }, { "VoiceSlider", "VoiceMainVolume" },
	{ "MusicSlider", "MusicMainVolume" } };

/* volume slider var: the slider at the value of the engine variable */
static void Step_Volume(int id, PyObject *args) {
	const char *slider, *var;
	if (!PyArg_ParseTuple(args, "ss", &slider, &var)) {
		PyErr_Print();
		return;
	}
	Script_SetNum(id, slider, "Value", GVar_GetNum(var, 0));
}

/* the entries without a left neighbour lead to the Spacelords button */
static void Step_Spacelords(int id, PyObject *args) {
	(void)args;
	for (int i = 1;; i++) {
		char item[0x20];
		Str_Printf(item, sizeof(item), "Item%d", i);
		if (!Script_HasItem(id, item) || Script_GetStr(id, item, "Left")[0]) {
			break;
		}
		Script_SetStr(id, item, "Left", "Spacelords");
	}
}

static const scriptStep_t main_steps[] = {
	{ "volume", Step_Volume },
	{ "spacelords", Step_Spacelords },
	{ NULL, NULL },
};

static void Main_Run(int id, const char *section, PyObject *vars) {
	Script_RunMenu(id, MAIN_FILE, section, main_menu, main_steps, vars);
}

/* the number of saved games; the list goes to Menu.saveGamesList when keep */
static int Main_SaveGames(byte keep) {
	PyObject *list = SaveGames_GetList("Save/");
	int n = list ? PySequence_Length(list) : 0;
	PyErr_Clear();
	if (keep && list) {
		Py_INCREF(list);
		Script_SetVar("Menu", "saveGamesList", list);
	}
	Py_XDECREF(list);
	return n;
}

static byte Main_InMainMenu(void) {
	return Script_GetInt("Init", "inMainMenu", 0) || Script_GetInt("Init", "isShipEdit", 0);
}

static byte Main_NetGame(void) {
	return (g_pApp->flags & 7) != 0;
}

/* the mode the player was in before the menu */
static void Main_LastMode(char *out, int size) {
	Script_GetString("Menu", "LastMode", out, size);
}

static void Main_Single(int id) {
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	int saves = Main_SaveGames(0);
	byte main = Main_InMainMenu();
	byte xbox = GVar_GetNum("IsXbox", 0) == 1;
	byte canSave = (GVar_GetNum("IsXbox", 0) == 0 || GVar_GetNum("XboxCanSave", 0) == 1) &&
		atoi(SaveVar_Get("Player.NumLives")) > -1;
	PyObject *vars = Py_BuildValue("{s:s,s:i,s:i,s:i}", "menu", main ? "main" : "game", "saves", saves > 0, "xbox", xbox,
		"cansave", canSave);
	Main_Run(id, "Single", vars);
	Py_XDECREF(vars);
	if (!main) {
		Script_SetInt("Menu", "fromSinglePlayer", 1);
	}
}

static void Main_Create(int id, const char *back) {
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	int saves = Main_SaveGames(1);
	PyObject *vars = Py_BuildValue("{s:i,s:s,s:i}", "saves", saves > 0, "back", back, "steam", 0);
	Main_Run(id, "Main", vars);
	Py_XDECREF(vars);
}

/* the main menu through Menu.onCreateMainMenu, which a mod may replace; Escape returns to the game */
static void Main_MainMenu(int id, const char *control) {
	char mode[0x20];
	Main_LastMode(mode, sizeof(mode));
	Script_Call("Menu", "onCreateMainMenu", "(iss)", id, control, strcmp(mode, "Menu") ? "Menu.BackToGame" : "");
}

/* the menu during a game: the mode and the time speed are kept, the menu set active, the time stopped offline */
static void Main_InitMenuSys(double timeSpeed) {
	cActionSet *set = TABLE_AT(g_ActionSets, Input_GetActionSet());
	PyObject *mode = PyString_FromString(set->name);
	Script_SetVar("Menu", "LastMode", mode);
	Input_SelectActionSetNamed("Menu");
	Script_SetVar("Menu", "LastTimeSpeed", PyFloat_FromDouble(g_pWorld->timer.speed));
	if (!Main_NetGame()) {
		Timer_SetSpeed(&g_pWorld->timer, (float)timeSpeed);
	}
}

static void Main_MovieScorer(int id) {
	cScorer *sc = Scorer_Get(id);
	int marginH = Script_GetInt("Scorer", "MarginH", 0), marginV = Script_GetInt("Scorer", "MarginV", 0);
	if (sc) {
		cScorer_Clear(sc);
	}
	Script_Call("SScorer", "SetCinema", "(iid)", id, 1, 0.25);
	GVar_SetNum("MaxTalkLineSizePix", 620 - marginH * 2);
	PyObject *vars = Py_BuildValue("{s:i}", "talky", 475 - marginV);
	Main_Run(id, "Movie", vars);
	Py_XDECREF(vars);
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *call = m ? PyObject_GetAttrString(m, "MovieScorerCall") : NULL;
	if (call && call != Py_None && PyObject_IsTrue(call)) {
		PyObject *r = PyEval_CallObject(call, Py_BuildValue("(i)", id));
		if (!r) {
			PyErr_Print();
		}
		Py_XDECREF(r);
	}
	PyErr_Clear();
	Py_XDECREF(call);
	Py_XDECREF(m);
}

/* ---- the functions of Menu ---- */

#define MAIN_ARGS(args) \
	int id; \
	const char *control; \
	if (!Script_Args(args, &id, &control)) { \
		return NULL; \
	}

static PyObject *Menu_SinglePlayerMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_Single(id);
	return Py_ReturnNone();
}

static PyObject *Menu_AudioMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	Main_Run(id, "Audio", NULL);
	return Py_ReturnNone();
}

static void Main_ApplyVolumes(int id) {
	for (int i = 0; i < 3; i++) {
		GVar_SetNum(main_volumes[i].var, Script_GetNum(id, main_volumes[i].slider, "Value"));
	}
}

/* the dummies left and right of a volume title step its slider and give the focus back */
static PyObject *Main_VolumeStep(PyObject *args, double sign) {
	int id;
	char *control, *prev;
	if (!PyArg_ParseTuple(args, "iss", &id, &control, &prev)) {
		return NULL;
	}
	int n = !strcmp(prev, "Item1") ? 0 : !strcmp(prev, "Item2") ? 1 : !strcmp(prev, "Item3") ? 2 : -1;
	/* without the slider the script failed on None + None, before it moved the focus */
	if (n < 0 || !Script_HasItem(id, main_volumes[n].slider)) {
		PyErr_SetString(PyExc_TypeError, sign > 0 ? "bad operand type(s) for +" : "bad operand type(s) for -");
		return NULL;
	}
	const char *s = main_volumes[n].slider;
	Script_SetNum(id, s, "Value", Script_GetNum(id, s, "Value") + sign * Script_GetNum(id, s, "ValueStep"));
	Script_SetDefault(id, prev);
	Main_ApplyVolumes(id);
	return Py_ReturnNone();
}

static PyObject *Menu_AudioVolumeSliderFocus_Sub(PyObject *self, PyObject *args) {
	(void)self;
	return Main_VolumeStep(args, -1.0);
}

static PyObject *Menu_AudioVolumeSliderFocus_Add(PyObject *self, PyObject *args) {
	(void)self;
	return Main_VolumeStep(args, 1.0);
}

static PyObject *Menu_AudioVolumeChange(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_ApplyVolumes(id);
	return Py_ReturnNone();
}

static PyObject *Menu_MainMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_MainMenu(id, control);
	return Py_ReturnNone();
}

/* CreateMainMenu(id, control, backfunc) */
static PyObject *Menu_CreateMainMenu(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	char *back;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOs", &id, &control, &back)) {
		return NULL;
	}
	Main_Create(id, back);
	return Py_ReturnNone();
}

/* PlaySpacelords: the store page of another game of the studio; there is no store here */
static PyObject *Menu_PlaySpacelords(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

/* BackToGame: the scorer of the mode the player was in, the time and the sound as before the menu */
static PyObject *Menu_BackToGame(PyObject *self, PyObject *args) {
	char mode[0x20];
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	Main_LastMode(mode, sizeof(mode));
	Input_SelectActionSetNamed(mode);
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *speed = m ? PyObject_GetAttrString(m, "LastTimeSpeed") : NULL;
	PyObject *pitch = m ? PyObject_GetAttrString(m, "currentSoundPitch") : NULL;
	PyErr_Clear();
	if (speed && (PyFloat_Check(speed) || PyInt_Check(speed))) {
		Timer_SetSpeed(&g_pWorld->timer, (float)(PyFloat_Check(speed) ? PyFloat_AsDouble(speed) : PyInt_AsLong(speed)));
	}
	Script_Call("Menu", "DrawScorer", "(is)", id, mode);
	double p = pitch && PyFloat_Check(pitch) ? PyFloat_AsDouble(pitch) : pitch && PyInt_Check(pitch) ? PyInt_AsLong(pitch) : 0;
	GVar_SetNum("S_PitchScale", p ? p : 1.0);
	GVar_SetNum("R_SceneBlurValue", 0);
	Py_XDECREF(pitch);
	Py_XDECREF(speed);
	Py_XDECREF(m);
	return Py_ReturnNone();
}

/* DrawScorer(id, mode): the scorer of a ship, of the character, or of a movie */
static PyObject *Menu_DrawScorer(PyObject *self, PyObject *args) {
	int id;
	char *mode;
	(void)self;
	if (!PyArg_ParseTuple(args, "is", &id, &mode)) {
		return NULL;
	}
	if (!strcmp(mode, "Racer")) {
		Script_Call("RacerScorer", "BackMenuFunc", "(i)", id);
	} else if (!strcmp(mode, "Walk")) {
		Script_Call("CharScorer", "BackMenuFunc", "(i)", id);
	} else if (!strcmp(mode, "Inactive")) {
		Main_MovieScorer(id);
	}
	return Py_ReturnNone();
}

static PyObject *Menu_InitMenuSys(PyObject *self, PyObject *args) {
	double speed;
	(void)self;
	if (!PyArg_ParseTuple(args, "d", &speed)) {
		return NULL;
	}
	Main_InitMenuSys(speed);
	return Py_ReturnNone();
}

static PyObject *Menu_MovieScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Main_MovieScorer(id);
	return Py_ReturnNone();
}

/* Init(id): the menu opens: in a game the single or multiplayer menu, else the main menu */
static PyObject *Menu_Init(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Main_InitMenuSys(0);
	if (!Main_InMainMenu()) {
		if (!Main_NetGame()) {
			Main_Single(id);
		} else {
			Script_Call("PCMenu", "MultiPlayerMenu", "(is)", id, "");
		}
	} else {
		Main_MainMenu(id, "");
	}
	Script_SetNum(id, "Cursor", "Visible", 0);
	return Py_ReturnNone();
}

/* DeathInfo(id): the player died; lives left or the jail */
static PyObject *Menu_DeathInfo(PyObject *self, PyObject *args) {
	int id;
	char text[0x200];
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	const char *show = "";
	text[0] = 0;
	if (!strcmp(SaveVar_Get("Player.InfiniteLives"), "0")) {
		int lives = atoi(SaveVar_Get("Player.NumLives"));
		if (lives < 0) {
			show = "jail";
		} else {
			/* <n> in the text is the number of lives */
			char n[0x10];
			const char *t = Script_Lang("Menu_GameOver_2");
			const char *at = strstr(t, "<n>");
			show = "lives";
			Str_Printf(n, sizeof(n), "%d", lives);
			if (at) {
				Str_Printf(text, sizeof(text), "%.*s%s%s", (int)(at - t), t, n, at + 3);
			} else {
				Str_Copy(text, t, sizeof(text));
			}
		}
	}
	PyObject *vars = Py_BuildValue("{s:s,s:s}", "show", show, "text2", text);
	Main_Run(id, "Death", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *Menu_ExitGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Script_Call("Scrap", "Exit", "()");
	return Py_ReturnNone();
}

static PyObject *Menu_ByeMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_ExitGame_Question"), "Menu.ExitGame",
		"Menu.MainMenu");
	return Py_ReturnNone();
}

/* ---- the skill and the saved games ---- */

static const char *const main_skills[3] = { "Menu_Skill_Easy", "Menu_Skill_Normal", "Menu_Skill_Hard" };

/* the text with its first %s replaced by arg */
static void Main_Format(const char *fmt, const char *arg, char *out, int size) {
	const char *at = strstr(fmt, "%s");
	if (at) {
		Str_Printf(out, size, "%.*s%s%s", (int)(at - fmt), fmt, arg, at + 2);
	} else {
		Str_Copy(out, fmt, size);
	}
}

static void Main_SkillMenu(int id, const char *func, const char *back, byte lit) {
	char current[0x20];
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	Str_Printf(current, sizeof(current), "Item%d", (int)GVar_GetNum("GameSkill", 0) + 1);
	PyObject *vars = Py_BuildValue("{s:s,s:s,s:s,s:i}", "func", func, "back", back, "current", current, "lit", lit);
	Main_Run(id, "Skill", vars);
	Py_XDECREF(vars);
}

/* the menu entry ItemN of a control: its last digit (the script reads the one character) */
static int Main_Digit(const char *control, int pos) {
	int len = (int)strlen(control);
	if (pos < 0) {
		pos += len;
	}
	return pos >= 0 && pos < len ? control[pos] - '0' : 0;
}

/* entry i of Menu.saveGamesList, element k (a new reference, NULL when it is not there) */
static PyObject *Main_SaveEntry(int i, int k) {
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *list = m ? PyObject_GetAttrString(m, "saveGamesList") : NULL;
	PyObject *e = list ? PySequence_GetItem(list, i) : NULL;
	PyObject *v = e ? PySequence_GetItem(e, k) : NULL;
	if (!v) {
		PyErr_Clear();
	}
	Py_XDECREF(e);
	Py_XDECREF(list);
	Py_XDECREF(m);
	return v;
}

/* SaveGame.function(entry i of the list) */
static void Main_SaveCall(const char *func, int i) {
	PyObject *file = Main_SaveEntry(i, 1);
	if (file) {
		Script_CallKw("SaveGame", func, Py_BuildValue("(O)", file), NULL);
	}
	Py_XDECREF(file);
}

static byte Main_IsAutoSave(const char *name) {
	return !strncmp(name, "AutoSave-", 9);
}

/* "Station-Date": the station by its language name and the date; (Autosave) in front of the automatic one */
static void Main_SaveName(const char *save, char *out, int size) {
	char station[0x80], key[0x90], text[0x200];
	byte autoSave = Main_IsAutoSave(save);
	const char *cad = save + (autoSave ? 9 : 0);
	int len = (int)strlen(cad);
	const char *dash = strchr(cad, '-');
	int index = dash ? (int)(dash - cad) : -1;
	/* as the script: the part up to the character before the dash, or all but the last two without a dash */
	int end = index - 1 < 0 ? len + index - 1 : index - 1;
	if (end < 0) {
		end = 0;
	}
	Str_Copy(station, cad, end + 1 < (int)sizeof(station) ? end + 1 : (int)sizeof(station));
	Str_Printf(key, sizeof(key), "Station_%s", station);
	const char *lang = Script_Lang(key);
	int from = index < 0 ? len + index : index;
	Str_Printf(text, sizeof(text), "%s %s", *lang ? lang : station, cad + (from < 0 ? 0 : from));
	if (autoSave) {
		Str_Printf(out, size, "(%s) %s", Script_Lang("Menu_AutoSave"), text);
	} else {
		Str_Copy(out, text, size);
	}
}

/* the menu of the saved games: load (all) or save (without the automatic one, a new slot first while there is room) */
static void Main_GamesMenu(int id, byte save) {
	PyObject *list = SaveGames_GetList("Save/");
	if (!list) {
		PyErr_Clear();
		list = PyList_New(0);
	}
	int n = PySequence_Length(list);
	char first[0x100] = "";
	PyObject *e0 = n > 0 ? PySequence_GetItem(list, 0) : NULL;
	PyObject *name0 = e0 ? PySequence_GetItem(e0, 0) : NULL;
	if (name0 && PyString_Check(name0)) {
		Str_Copy(first, PyString_AsString(name0), sizeof(first));
	}
	PyErr_Clear();
	Py_XDECREF(name0);
	Py_XDECREF(e0);
	byte autoSave = !save && n > 1 && Main_IsAutoSave(first);
	if (save && n > 0 && Main_IsAutoSave(first)) {
		PyObject *rest = PySequence_GetSlice(list, 1, n);
		Py_DECREF(list);
		list = rest;
		n--;
	}
	Py_INCREF(list);
	Script_SetVar("Menu", "saveGamesList", list);
	Script_Call("Menu", "StartNewMenu", "(i)", id);
	PyObject *entries = PyList_New(0);
	if (save && n < 6) {
		PyList_Append(entries, Py_BuildValue("[ss]", Script_Lang("Menu_SaveGame_New"), "Menu.SaveGame"));
	}
	for (int i = 0; i < n; i++) {
		char text[0x200] = "";
		PyObject *e = PySequence_GetItem(list, i);
		PyObject *nm = e ? PySequence_GetItem(e, 0) : NULL;
		if (nm && PyString_Check(nm)) {
			Main_SaveName(PyString_AsString(nm), text, sizeof(text));
		}
		PyErr_Clear();
		PyObject *entry = Py_BuildValue("[ss]", text, save ? "Menu.SaveGame" : "Menu.LoadGame");
		PyList_Append(entries, entry);
		Py_XDECREF(entry);
		Py_XDECREF(nm);
		Py_XDECREF(e);
	}
	PyObject *back = Py_BuildValue("[ss]", Script_Lang("Menu_Back"), "Menu.SinglePlayerMenu");
	PyList_Append(entries, back);
	Py_XDECREF(back);
	char current[0x20] = "";
	if (save) {
		Str_Printf(current, sizeof(current), "Item%d", n < 6 ? n + 1 : 1);
	}
	PyObject *vars = Py_BuildValue("{s:s,s:O,s:i,s:s}", "title", Script_Lang(save ? "Menu_SaveGame" : "Menu_LoadGame"),
		"entries", entries, "autosave", autoSave, "current", current);
	Main_Run(id, "Games", vars);
	Py_XDECREF(vars);
	Py_XDECREF(entries);
	Py_XDECREF(list);
}

static PyObject *Menu_SetSkillMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_SkillMenu(id, "Menu.SetSkill", "Menu.OptionsMenu", 1);
	return Py_ReturnNone();
}

/* SetSkill(id, "ItemN"): skill N-1, back to the game with a message */
static PyObject *Menu_SetSkill(PyObject *self, PyObject *args) {
	char n[0x10], text[0x200];
	(void)self;
	MAIN_ARGS(args);
	int i = Main_Digit(control, 4) - 1;
	if (i < 0 || i > 2) {
		PyErr_SetString(PyExc_IndexError, "SetSkill: no such skill");
		return NULL;
	}
	GVar_SetNum("GameSkill", i);
	Str_Printf(n, sizeof(n), "%d", i);
	SaveVar_Set("GameSkill", n);
	Script_Call("Menu", "BackToGame", "(is)", id, control);
	Main_Format(Script_Lang("Menu_Skill_SystemMessage"), Script_Lang(main_skills[i]), text, sizeof(text));
	Scorer_SetMsgText(text, Timer_GetTime(&g_pWorld->timer) + 5.0f, "Horatio");
	return Py_ReturnNone();
}

static PyObject *Menu_StartNewGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_SkillMenu(id, "Menu.NewGame", "Menu.MainMenu", 0);
	return Py_ReturnNone();
}

static PyObject *Menu_NewGame(PyObject *self, PyObject *args) {
	char n[0x10];
	(void)self;
	MAIN_ARGS(args);
	(void)id;
	Script_Call("SaveGame", "NewGame", "()");
	Str_Printf(n, sizeof(n), "%d", Main_Digit(control, 4) - 1);
	SaveVar_Set("GameSkill", n);
	return Py_ReturnNone();
}

static PyObject *Menu_AuxAbortGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	SaveVars_Delete("");
	Script_Call("SaveGame", "QLoad", "(s)", "Levels/Menu");
	return Py_ReturnNone();
}

static PyObject *Menu_AbortGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_AbortGame_Question"), "Menu.AuxAbortGame",
		"Menu.SinglePlayerMenu");
	return Py_ReturnNone();
}

static PyObject *Menu_AuxLoadGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_SaveCall("LoadGame", Script_GetInt("Menu", "slotToLoad", 0));
	Script_Call("Menu", "BackToGame", "(is)", id, control);
	return Py_ReturnNone();
}

/* LoadGame(id, "ItemN"): game N, after a question during a single player game */
static PyObject *Menu_LoadGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Script_SetInt("Menu", "slotToLoad", Main_Digit(control, -1) - 1);
	if (Script_GetInt("Menu", "fromSinglePlayer", 0)) {
		Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_LoadGame_Question"), "Menu.AuxLoadGame",
			"Menu.LoadGameMenu");
	} else {
		Script_Call("Menu", "AuxLoadGame", "(is)", id, control);
	}
	return Py_ReturnNone();
}

/* the time "HH:MM:SS" at the end of the name of saved game i, as seconds */
static int Main_SaveTime(int i) {
	PyObject *name = Main_SaveEntry(i, 0);
	const char *s = name && PyString_Check(name) ? PyString_AsString(name) : "";
	int len = (int)strlen(s);
	int t = 0;
	if (len >= 8) {
		s += len - 8;
		t = atoi(s) * 3600 + atoi(s + 3) * 60 + atoi(s + 6);
	}
	Py_XDECREF(name);
	return t;
}

/* LoadLastSavedGame: the later of the first two games of the list (by the time of day in the name, as the script) */
static PyObject *Menu_LoadLastSavedGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *list = m ? PyObject_GetAttrString(m, "saveGamesList") : NULL;
	int n = list ? PySequence_Length(list) : 0;
	PyErr_Clear();
	Py_XDECREF(list);
	Py_XDECREF(m);
	if (n == 1) {
		Main_SaveCall("LoadGame", 0);
	} else if (n > 1) {
		Main_SaveCall("LoadGame", Main_SaveTime(0) > Main_SaveTime(1) ? 0 : 1);
	}
	return Py_ReturnNone();
}

static PyObject *Menu_AuxSaveGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	const char *text = Script_Lang("SaveGameConfirmation");
	PyObject *t = PyString_FromString(text);
	PyObject *speech = PyImport_ImportModule("Speech");
	PyObject *f = speech ? PyObject_GetAttrString(speech, "CalculateSpeechTime") : NULL;
	PyObject *r = f && t ? PyEval_CallObject(f, Py_BuildValue("(O)", t)) : NULL;
	double add = r && PyFloat_Check(r) ? PyFloat_AsDouble(r) : r && PyInt_Check(r) ? PyInt_AsLong(r) : 0.0;
	if (!r) {
		PyErr_Print();
	}
	Scorer_SetMsgText(text, (float)(Timer_GetTime(&g_pWorld->timer) + add), "Horatio");
	Py_XDECREF(r);
	Py_XDECREF(f);
	Py_XDECREF(speech);
	Py_XDECREF(t);
	Main_SaveCall("SaveGame", Script_GetInt("Menu", "slotToOverwrite", 0) - 1);
	Script_Call("Menu", "BackToGame", "(is)", id, control);
	return Py_ReturnNone();
}

/* SaveGame(id, "ItemN"): a new game in the first entry while there is room, else after a question over game N */
static PyObject *Menu_SaveGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	int num = Main_Digit(control, -1);
	PyObject *m = PyImport_ImportModule("Menu");
	PyObject *list = m ? PyObject_GetAttrString(m, "saveGamesList") : NULL;
	int n = list ? PySequence_Length(list) : 0;
	PyErr_Clear();
	Py_XDECREF(list);
	Py_XDECREF(m);
	if (n < Script_GetInt("Menu", "saveGamesSlots", 6)) {
		num--;
	}
	if (num <= 0) {
		Script_Call("SaveGame", "SaveGame", "(i)", n + 1);
		Script_Call("Menu", "BackToGame", "(is)", id, control);
	} else {
		Script_SetInt("Menu", "slotToOverwrite", num);
		Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_SaveGame_Overwrite_Question"),
			"Menu.AuxSaveGame", "Menu.SaveGameMenu");
	}
	return Py_ReturnNone();
}

static PyObject *Menu_LoadGameMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_GamesMenu(id, 0);
	return Py_ReturnNone();
}

static PyObject *Menu_SaveGameMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_GamesMenu(id, 1);
	return Py_ReturnNone();
}

static PyObject *Menu_isAutoSave(PyObject *self, PyObject *args) {
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return PyInt_FromLong(Main_IsAutoSave(name));
}

static PyObject *Menu_ComposeSaveGameName(PyObject *self, PyObject *args) {
	char *name, out[0x200];
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Main_SaveName(name, out, sizeof(out));
	return PyString_FromString(out);
}

PyMethodDef g_MenuMainMethods[] = {
	{ "SinglePlayerMenu", Menu_SinglePlayerMenu, 1, NULL },
	{ "AudioMenu", Menu_AudioMenu, 1, NULL },
	{ "AudioVolumeSliderFocus_Sub", Menu_AudioVolumeSliderFocus_Sub, 1, NULL },
	{ "AudioVolumeSliderFocus_Add", Menu_AudioVolumeSliderFocus_Add, 1, NULL },
	{ "AudioVolumeChange", Menu_AudioVolumeChange, 1, NULL },
	{ "MainMenu", Menu_MainMenu, 1, NULL },
	{ "CreateMainMenu", Menu_CreateMainMenu, 1, NULL },
	{ "PlaySpacelords", Menu_PlaySpacelords, 1, NULL },
	{ "BackToGame", Menu_BackToGame, 1, NULL },
	{ "DrawScorer", Menu_DrawScorer, 1, NULL },
	{ "InitMenuSys", Menu_InitMenuSys, 1, NULL },
	{ "MovieScorer", Menu_MovieScorer, 1, NULL },
	{ "Init", Menu_Init, 1, NULL },
	{ "DeathInfo", Menu_DeathInfo, 1, NULL },
	{ "ExitGame", Menu_ExitGame, 1, NULL },
	{ "ByeMenu", Menu_ByeMenu, 1, NULL },
	{ "SetSkillMenu", Menu_SetSkillMenu, 1, NULL },
	{ "SetSkill", Menu_SetSkill, 1, NULL },
	{ "StartNewGame", Menu_StartNewGame, 1, NULL },
	{ "NewGame", Menu_NewGame, 1, NULL },
	{ "AuxAbortGame", Menu_AuxAbortGame, 1, NULL },
	{ "AbortGame", Menu_AbortGame, 1, NULL },
	{ "AuxLoadGame", Menu_AuxLoadGame, 1, NULL },
	{ "LoadGame", Menu_LoadGame, 1, NULL },
	{ "LoadLastSavedGame", Menu_LoadLastSavedGame, 1, NULL },
	{ "AuxSaveGame", Menu_AuxSaveGame, 1, NULL },
	{ "SaveGame", Menu_SaveGame, 1, NULL },
	{ "LoadGameMenu", Menu_LoadGameMenu, 1, NULL },
	{ "SaveGameMenu", Menu_SaveGameMenu, 1, NULL },
	{ "isAutoSave", Menu_isAutoSave, 1, NULL },
	{ "ComposeSaveGameName", Menu_ComposeSaveGameName, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
