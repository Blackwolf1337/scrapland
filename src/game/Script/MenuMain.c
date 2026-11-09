/*
 * The main menus of the Menu module in C (issue #2, step 4b): the main menu, the single player menu, the audio
 * options, the skill, new, saved and loaded games, the death screen, entering and leaving the menu during a game
 * and the scorer of a movie. The layout is data/menus/main.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters that
 * parse the arguments as before (the same TypeErrors). The variables of other modules are read where the C read them
 * (Init.inMainMenu, isShipEdit, isDemo, Outdoor, isJunkYard, MultiPlayer, Menu.LastMode, LastTimeSpeed,
 * currentSoundPitch, saveGamesList, saveGamesSlots, slotToLoad, slotToOverwrite, fromSinglePlayer, isXbox, HeadBoss,
 * OutHeadMessenger, MovieScorerCall, Scorer.MarginH and MarginV, the sounds of Sound), the variables of Menu are
 * written at once (OBJLIB 5). The menus, SaveGame, Speech and MissionsFuncs are called through their refs (PORT_CALL:
 * directly while the attribute holds the port's function, else by name, as a mod, a mission or a case may replace
 * them); the aliases and callbacks of Menu and the scorers (Menu.onCreateMainMenu, MovieScorerCall,
 * RacerScorer.BackMenuFunc, CharScorer.BackMenuFunc) are variables called by name (OBJLIB 5; BackMenuFunc through the
 * entry of the function it holds, MODE_PRINTEDCALL), the Init of the scorer of the mode by its module chosen at run
 * time (MODE_CBATTR; OBJLIB 7: both are Mode_PyNone, with no ref); SScorer.SetCinema, Fade, SSound.Play and Scrap.Exit,
 * GetCam, GetEntity, UsrEntity, CreateEntity and TestLine go through their seams (OBJLIB 7); the death camera
 * schedules functions of Menu and Scrap.SetCam (the scheduler takes their entries, Port_Scheduled); the list of saved
 * games comes from the engine as a Python list and the .def runner takes Python values (OBJLIB 6).
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"

static const char main_def[] = {
#embed "../../../data/menus/main.def"
	, 0 };

#define MAIN_FILE "menus/main.def"

static const struct {
	const char *slider, *var;
} main_volumes[] = { { "SoundSlider", "SoundMainVolume" }, { "VoiceSlider", "VoiceMainVolume" },
	{ "MusicSlider", "MusicMainVolume" } };

/* volume slider var: the slider at the value of the engine variable */
static void Step_Volume(int id, PyObject *args) {
	const char *slider, *var;
	/* OBJLIB 6: the steps take the arguments of the line as a tuple */
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

/* OBJLIB 6: the $names of the section as a dict (given away, or NULL) */
static void Main_Run(int id, const char *section, PyObject *vars) {
	Script_RunFile(id, MAIN_FILE, section, main_def, main_steps, vars);
	Py_XDECREF(vars);
}

/* ---- values and calls ---- */

/* Menu.YesNoMenu(id, the language text of key, yes, no) */
static void Main_YesNo(int id, const char *key, const char *yes, const char *no) {
	MODE_PRINTED(Menu, YesNoMenu, "isss", (long)id, Script_Lang(key), yes, no);
}

/* SaveGame.QLoad(level) */
static void Main_QLoad(const char *level) {
	MODE_PRINTED(SaveGame, QLoad, "s", level);
}

/* module.var = i of Menu, a failure dropped */
static void Main_PutInt(const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	Mode_ModPut("Menu", var, &v);
}

/* the list of saved games of the engine into *out: [(desc, slot), ...] as Scrap.GetSaveGamesList gives it */
static void Main_SaveList(nativeVal_t *out) {
	cSaveGameInfo saves[SAVE_SLOTS];
	int n = SaveGames_GetList("Save/", saves);
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0; i < n; i++) {
		nativeVal_t *e = Native_Append(out);
		Native_SetEmpty(e, NV_TUPLE);
		Native_SetText(Native_Append(e), saves[i].desc, (int)strlen(saves[i].desc));
		Native_SetInt(Native_Append(e), saves[i].slot);
	}
}

/* len(seq) as PySequence_Length takes it: -1 for a value without length (the error dropped, the -1 kept as the C kept
 * it) */
static int Main_Len(const nativeVal_t *seq) {
	int n;
	if (!Native_Len(seq, &n)) {
		PyErr_Clear();
		return -1;
	}
	return n;
}

/* Menu.saveGamesList into *out (lists held as one NV_OBJ made native): 0 when it is not there (the error dropped) */
static byte Main_SavesVar(nativeVal_t *out) {
	if (!Mode_ModGet("Menu", "saveGamesList", out)) {
		PyErr_Clear();
		return 0;
	}
	Mode_ElemsIn(out);
	return 1;
}

/* the number of saved games; the list goes to Menu.saveGamesList when keep */
static int Main_SaveGames(byte keep) {
	nativeVal_t list;
	Main_SaveList(&list);
	int n = Main_Len(&list);
	if (keep) {
		Mode_ModPut("Menu", "saveGamesList", &list);
	}
	Native_Free(&list);
	return n;
}

static byte Main_InMainMenu(void) {
	return Mode_ModGetI("Init", "inMainMenu", 0) || Mode_ModGetI("Init", "isShipEdit", 0);
}

static byte Main_NetGame(void) {
	return (g_pApp->flags & 7) != 0;
}

/* the mode the player was in before the menu */
static void Main_LastMode(char *out, int size) {
	Mode_ModGetS("Menu", "LastMode", out, size);
}

/* ---- the menus ---- */

static void Main_Single(int id) {
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	int saves = Main_SaveGames(0);
	byte main = Main_InMainMenu();
	byte xbox = GVar_GetNum("IsXbox", 0) == 1;
	byte canSave = (GVar_GetNum("IsXbox", 0) == 0 || GVar_GetNum("XboxCanSave", 0) == 1) &&
		atoi(SaveVar_Get("Player.NumLives")) > -1;
	Main_Run(id, "Single", Py_BuildValue("{s:s,s:i,s:i,s:i}", "menu", main ? "main" : "game", "saves", saves > 0, "xbox",
		xbox, "cansave", canSave));
	if (!main) {
		Main_PutInt("fromSinglePlayer", 1);
	}
}

static void Main_Create(int id, const char *back) {
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	int saves = Main_SaveGames(1);
	Main_Run(id, "Main", Py_BuildValue("{s:i,s:s,s:i}", "saves", saves > 0, "back", back, "steam", 0));
}

/* the main menu through Menu.onCreateMainMenu, which a mod may replace; Escape returns to the game */
static void Main_MainMenu(int id, const char *control) {
	char mode[0x20];
	Main_LastMode(mode, sizeof(mode));
	/* OBJLIB 5: the alias Menu.onCreateMainMenu (= CreateMainMenu of menu.mod) by name */
	Mode_Printed(Native_CallMod("Menu", "onCreateMainMenu", NULL, "iss", (long)id, control,
		strcmp(mode, "Menu") ? "Menu.BackToGame" : ""), "Menu", "onCreateMainMenu");
}

/* the menu during a game: the mode and the time speed are kept, the menu set active, the time stopped offline */
static void Main_InitMenuSys(double timeSpeed) {
	cActionSet *set = TABLE_AT(g_ActionSets, Input_GetActionSet());
	nativeVal_t v;
	Native_SetText(&v, set->name, (int)strlen(set->name));
	Mode_ModPut("Menu", "LastMode", &v);
	Native_Free(&v);
	Input_SelectActionSetNamed("Menu");
	Native_SetFloat(&v, g_pWorld->timer.speed);
	Mode_ModPut("Menu", "LastTimeSpeed", &v);
	if (!Main_NetGame()) {
		Timer_SetSpeed(&g_pWorld->timer, (float)timeSpeed);
	}
}

/* SScorer.SetCinema(id, on, 0.25) through its seam, the failure printed */
static void Main_Cinema(int id, int on) {
	Mode_Printed(Mode_SetCinema(id, on, 0.25), "SScorer", "SetCinema");
}

static void Main_MovieScorer(int id) {
	cScorer *sc = Scorer_Get(id);
	int marginH = Mode_ModGetI("Scorer", "MarginH", 0), marginV = Mode_ModGetI("Scorer", "MarginV", 0);
	if (sc) {
		cScorer_Clear(sc);
	}
	Main_Cinema(id, 1);
	GVar_SetNum("MaxTalkLineSizePix", 620 - marginH * 2);
	Main_Run(id, "Movie", Py_BuildValue("{s:i}", "talky", 475 - marginV));
	/* OBJLIB 5: the callback variable of Menu */
	Mode_CallVarIf("Menu", "MovieScorerCall", id);
}

static void Main_ApplyVolumes(int id) {
	for (int i = 0; i < 3; i++) {
		GVar_SetNum(main_volumes[i].var, Script_GetNum(id, main_volumes[i].slider, "Value"));
	}
}

/* the dummies left and right of a volume title step its slider and give the focus back: 0 with the error set */
static byte Main_VolumeStep(int id, const char *prev, double sign) {
	int n = !strcmp(prev, "Item1") ? 0 : !strcmp(prev, "Item2") ? 1 : !strcmp(prev, "Item3") ? 2 : -1;
	/* without the slider the script failed on None + None, before it moved the focus */
	if (n < 0 || !Script_HasItem(id, main_volumes[n].slider)) {
		return Native_Raise(NE_TYPE, sign > 0 ? "bad operand type(s) for +" : "bad operand type(s) for -");
	}
	const char *s = main_volumes[n].slider;
	Script_SetNum(id, s, "Value", Script_GetNum(id, s, "Value") + sign * Script_GetNum(id, s, "ValueStep"));
	Script_SetDefault(id, prev);
	Main_ApplyVolumes(id);
	return 1;
}

/* BackToGame: the scorer of the mode the player was in, the time and the sound as before the menu */
static void Main_BackToGame(int id) {
	char mode[0x20];
	nativeVal_t speed, pitch;
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	Main_LastMode(mode, sizeof(mode));
	Input_SelectActionSetNamed(mode);
	byte haveSpeed = Mode_ModGet("Menu", "LastTimeSpeed", &speed);
	byte havePitch = Mode_ModGet("Menu", "currentSoundPitch", &pitch);
	PyErr_Clear();
	if (haveSpeed && (speed.type == NV_FLOAT || speed.type == NV_INT)) {
		Timer_SetSpeed(&g_pWorld->timer, (float)(speed.type == NV_FLOAT ? speed.f : (double)speed.i));
	}
	MODE_PRINTED(Menu, DrawScorer, "is", (long)id, mode);
	double p = havePitch && pitch.type == NV_FLOAT ? pitch.f : havePitch && pitch.type == NV_INT ? (double)pitch.i : 0;
	GVar_SetNum("S_PitchScale", p ? p : 1.0);
	GVar_SetNum("R_SceneBlurValue", 0);
	Native_Free(&pitch);
	Native_Free(&speed);
}

/* DrawScorer(id, mode): the scorer of a ship, of the character, or of a movie */
static void Main_DrawScorer(int id, const char *mode) {
	/* OBJLIB 5: the aliases BackMenuFunc (= Create of racerscorer.mod and charscorer.mod) by name, called through the
	 * entry of the function they hold */
	if (!strcmp(mode, "Racer")) {
		MODE_PRINTEDCALL("RacerScorer", "BackMenuFunc", id);
	} else if (!strcmp(mode, "Walk")) {
		MODE_PRINTEDCALL("CharScorer", "BackMenuFunc", id);
	} else if (!strcmp(mode, "Inactive")) {
		Main_MovieScorer(id);
	}
}

/* Init(id): the menu opens: in a game the single or multiplayer menu, else the main menu */
static void Main_Init(int id) {
	Main_InitMenuSys(0);
	if (!Main_InMainMenu()) {
		if (!Main_NetGame()) {
			Main_Single(id);
		} else {
			MODE_PRINTED(PCMenu, MultiPlayerMenu, "is", (long)id, "");
		}
	} else {
		Main_MainMenu(id, "");
	}
	Script_SetNum(id, "Cursor", "Visible", 0);
}

/* DeathInfo(id): the player died; lives left or the jail */
static void Main_DeathInfo(int id) {
	char text[0x200];
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
	Main_Run(id, "Death", Py_BuildValue("{s:s,s:s}", "show", show, "text2", text));
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
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Str_Printf(current, sizeof(current), "Item%d", (int)GVar_GetNum("GameSkill", 0) + 1);
	Main_Run(id, "Skill", Py_BuildValue("{s:s,s:s,s:s,s:i}", "func", func, "back", back, "current", current, "lit", lit));
}

/* the menu entry ItemN of a control: its last digit (the script reads the one character) */
static int Main_Digit(const char *control, int pos) {
	int len = (int)strlen(control);
	if (pos < 0) {
		pos += len;
	}
	return pos >= 0 && pos < len ? control[pos] - '0' : 0;
}

/* element k of entry i of a list of saved games as a copy in *out: 0 when it is not there (the error dropped) */
static byte Main_Entry(const nativeVal_t *list, int i, int k, nativeVal_t *out) {
	nativeVal_t t1 = { 0 }, t2 = { 0 };
	const nativeVal_t *e = Native_GetItem(list, i, 0, &t1);
	const nativeVal_t *v = e ? Native_GetItem(e, k, 0, &t2) : NULL;
	Native_SetNone(out);
	if (v) {
		Native_Copy(out, v);
	} else {
		PyErr_Clear();
	}
	Native_Free(&t2);
	Native_Free(&t1);
	return v != NULL;
}

/* element k of entry i of Menu.saveGamesList */
static byte Main_SaveEntry(int i, int k, nativeVal_t *out) {
	nativeVal_t list;
	byte ok = Main_SavesVar(&list) && Main_Entry(&list, i, k, out);
	if (!ok) {
		Native_SetNone(out);
	}
	Native_Free(&list);
	return ok;
}

/* SaveGame.LoadGame (load) or SaveGame.SaveGame(the file of entry i of the list) */
static void Main_SaveCall(byte load, int i) {
	nativeVal_t file;
	if (Main_SaveEntry(i, 1, &file)) {
		if (load) {
			MODE_PRINTED(SaveGame, LoadGame, "v", &file);
		} else {
			MODE_PRINTED(SaveGame, SaveGame, "v", &file);
		}
	}
	Native_Free(&file);
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
	/* saveGameCad[i + index:]: a slice of the whole name, an autosave name without a dash goes on from character 8 */
	int from = (autoSave ? 9 : 0) + index;
	if (from < 0) {
		from += (int)strlen(save);
	}
	Str_Printf(text, sizeof(text), "%s %s", *lang ? lang : station, save + (from < 0 ? 0 : from));
	if (autoSave) {
		Str_Printf(out, size, "(%s) %s", Script_Lang("Menu_AutoSave"), text);
	} else {
		Str_Copy(out, text, size);
	}
}

/* an entry [text, function] of a menu at the end of list */
static void Main_AddEntry(nativeVal_t *list, const char *text, const char *func) {
	nativeVal_t *e = Native_Append(list);
	Native_SetEmpty(e, NV_LIST);
	Native_SetText(Native_Append(e), text, (int)strlen(text));
	Native_SetText(Native_Append(e), func, (int)strlen(func));
}

/* the menu of the saved games: load (all) or save (without the automatic one, a new slot first while there is room) */
static void Main_GamesMenu(int id, byte save) {
	nativeVal_t list, name0 = { 0 }, entries;
	Main_SaveList(&list);
	int n = Main_Len(&list);
	char first[0x100] = "";
	if (n > 0 && Main_Entry(&list, 0, 0, &name0) && name0.type == NV_STR) {
		Str_Copy(first, Native_Text(&name0), sizeof(first));
	}
	Native_Free(&name0);
	byte autoSave = !save && n > 1 && Main_IsAutoSave(first);
	if (save && n > 0 && Main_IsAutoSave(first)) {
		nativeVal_t rest;
		if (Native_Slice(&list, 1, n, &rest)) {
			Native_Free(&list);
			list = rest;
		} else {
			PyErr_Clear();
		}
		n--;
	}
	Mode_ModPut("Menu", "saveGamesList", &list);
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Native_SetEmpty(&entries, NV_LIST);
	if (save && n < 6) {
		Main_AddEntry(&entries, Script_Lang("Menu_SaveGame_New"), "Menu.SaveGame");
	}
	for (int i = 0; i < n; i++) {
		char text[0x200] = "";
		nativeVal_t nm;
		if (Main_Entry(&list, i, 0, &nm) && nm.type == NV_STR) {
			Main_SaveName(Native_Text(&nm), text, sizeof(text));
		}
		Native_Free(&nm);
		Main_AddEntry(&entries, text, save ? "Menu.SaveGame" : "Menu.LoadGame");
	}
	Main_AddEntry(&entries, Script_Lang("Menu_Back"), "Menu.SinglePlayerMenu");
	char current[0x20] = "";
	if (save) {
		Str_Printf(current, sizeof(current), "Item%d", n < 6 ? n + 1 : 1);
	}
	PyObject *pe = Native_ToPy(&entries);
	Main_Run(id, "Games", pe ? Py_BuildValue("{s:s,s:O,s:i,s:s}", "title", Script_Lang(save ? "Menu_SaveGame" :
		"Menu_LoadGame"), "entries", pe, "autosave", autoSave, "current", current) : NULL);
	Py_XDECREF(pe);
	Native_Free(&entries);
	Native_Free(&list);
}

/* SetSkill(id, "ItemN"): skill N-1, back to the game with a message; like the script a skill out of the three is set
 * and saved before the name lookup raises (a negative one counts from the end). 0 with the error set */
static byte Main_SetSkill(int id, const char *control) {
	char n[0x10], text[0x200];
	if (strlen(control) < 5) {
		return Native_Raise(NE_INDEX, "string index out of range");
	}
	if (control[4] < '0' || control[4] > '9') {
		return Native_Raise(NE_VALUE, "invalid literal for int(): %c", control[4]);
	}
	int i = control[4] - '0' - 1;
	GVar_SetNum("GameSkill", i);
	Str_Printf(n, sizeof(n), "%d", i);
	SaveVar_Set("GameSkill", n);
	MODE_PRINTED(Menu, BackToGame, "is", (long)id, control);
	if (i > 2) {
		return Native_Raise(NE_INDEX, "tuple index out of range");
	}
	if (i < 0) {
		i += 3;
	}
	Main_Format(Script_Lang("Menu_Skill_SystemMessage"), Script_Lang(main_skills[i]), text, sizeof(text));
	Scorer_SetMsgText(text, Timer_GetTime(&g_pWorld->timer) + 5.0f, "Horatio");
	return 1;
}

/* LoadGame(id, "ItemN"): game N, after a question during a single player game */
static void Main_LoadGame(int id, const char *control) {
	Main_PutInt("slotToLoad", Main_Digit(control, -1) - 1);
	if (Mode_ModGetI("Menu", "fromSinglePlayer", 0)) {
		Main_YesNo(id, "Menu_LoadGame_Question", "Menu.AuxLoadGame", "Menu.LoadGameMenu");
	} else {
		MODE_PRINTED(Menu, AuxLoadGame, "is", (long)id, control);
	}
}

/* the time "HH:MM:SS" at the end of the name of saved game i, as seconds */
static int Main_SaveTime(int i) {
	nativeVal_t name;
	Main_SaveEntry(i, 0, &name);
	const char *s = name.type == NV_STR ? Native_Text(&name) : "";
	int len = (int)strlen(s);
	int t = 0;
	if (len >= 8) {
		s += len - 8;
		t = atoi(s) * 3600 + atoi(s + 3) * 60 + atoi(s + 6);
	}
	Native_Free(&name);
	return t;
}

/* the length of Menu.saveGamesList: 0 when it is not there, -1 when it has no length */
static int Main_SavesLen(void) {
	nativeVal_t list;
	int n = Main_SavesVar(&list) ? Main_Len(&list) : 0;
	Native_Free(&list);
	return n;
}

/* LoadLastSavedGame: the later of the first two games of the list (by the time of day in the name, as the script) */
static void Main_LoadLast(void) {
	int n = Main_SavesLen();
	if (n == 1) {
		Main_SaveCall(1, 0);
	} else if (n > 1) {
		Main_SaveCall(1, Main_SaveTime(0) > Main_SaveTime(1) ? 0 : 1);
	}
}

static void Main_AuxSaveGame(int id, const char *control) {
	const char *text = Script_Lang("SaveGameConfirmation");
	nativeVal_t r = { 0 };
	/* through the module; its failure printed without the line of the menus, as the C asked it */
	byte ok = PORT_CALL(Speech, CalculateSpeechTime, &r, "s", text);
	double add = ok && r.type == NV_FLOAT ? r.f : ok && r.type == NV_INT ? (double)r.i : 0.0;
	if (!ok) {
		PyErr_Print();
	}
	Native_Free(&r);
	Scorer_SetMsgText(text, (float)(Timer_GetTime(&g_pWorld->timer) + add), "Horatio");
	Main_SaveCall(0, Mode_ModGetI("Menu", "slotToOverwrite", 0) - 1);
	MODE_PRINTED(Menu, BackToGame, "is", (long)id, control);
}

/* SaveGame(id, "ItemN"): a new game in the first entry while there is room, else after a question over game N */
static void Main_SaveGame(int id, const char *control) {
	int num = Main_Digit(control, -1);
	int n = Main_SavesLen();
	if (n < Mode_ModGetI("Menu", "saveGamesSlots", 6)) {
		num--;
	}
	if (num <= 0) {
		MODE_PRINTED(SaveGame, SaveGame, "i", (long)(n + 1));
		MODE_PRINTED(Menu, BackToGame, "is", (long)id, control);
	} else {
		Main_PutInt("slotToOverwrite", num);
		Main_YesNo(id, "Menu_SaveGame_Overwrite_Question", "Menu.AuxSaveGame", "Menu.SaveGameMenu");
	}
}

/* ---- the start of a level ---- */

static const char *Main_SaveVarOr(const char *var) {
	const char *s = SaveVar_Get(var);
	return s ? s : "";
}

/* Initialize: the textures and talking heads of the level, then the scorer of the mode */
static void Main_Initialize(void) {
	/* OBJLIB 6: PCMenu.Menu is the module object of Menu */
	PyObject *menu = Script_Module("Menu");
	if (menu && !Mode_SetModPy("PCMenu", "Menu", menu)) {
		PyErr_Clear();
	}
	Py_XDECREF(menu);
	PyErr_Clear();
	byte main = Mode_ModGetI("Init", "inMainMenu", 0) != 0;
	byte xbox = Mode_ModGetI("Menu", "isXbox", 0) != 0;
	byte outdoor = Mode_ModGetI("Init", "Outdoor", 0) != 0;
	byte shipEdit = Mode_ModGetI("Init", "isShipEdit", 0) != 0;
	byte junkyard = Mode_ModGetI("Init", "isJunkYard", 0) != 0;
	byte multiPlayer = Mode_ModGetI("Init", "MultiPlayer", 0) != 0;
	byte net = Main_NetGame();
	byte edit = shipEdit || multiPlayer;
	if (Mode_ModGetI("Init", "isDemo", 0) && xbox) {
		GVar_SetStr("CheckInactivityFunc", "!ExecuteDemoLauncher");
	}
	Main_Run(0, "preload", Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}", "main", main,
		"xbox", xbox, "outdoor", outdoor, "outdoorpc", outdoor && !xbox, "metro", !shipEdit && !junkyard && !xbox,
		"multi", multiPlayer || net, "editor", junkyard || shipEdit || net, "heads",
		!main && !(shipEdit && GVar_GetNum("isXbox", 0)), "edit", edit, "bosses",
		edit || strcmp(Main_SaveVarOr("GameAct"), "Epilogue"), "boss", Mode_ModGetI("Menu", "HeadBoss", 0) || edit,
		"dtritus", !outdoor || edit, "lonelymerc", !strcmp(Main_SaveVarOr("LonelyMercDataActive"), "1"), "bertos",
		!strcmp(Main_SaveVarOr("CreateBertos"), "1"), "messenger", Mode_ModGetI("Menu", "OutHeadMessenger", 0) != 0));
	if (!main) {
		/* the module chosen at run time (OBJLIB 7: both Init are Mode_PyNone, no ref: the call through Python) */
		const char *scorer = outdoor && !shipEdit ? "RacerScorer" : "CharScorer";
		Mode_Printed(MODE_CBATTR(scorer, "Init", NULL, ""), scorer, "Init");
	}
}

/* ---- the debug menu ---- */

static const char *const main_ai[4] = { "None", "ShowNodesAI", "ShowNodes2DAI", "ShowPathAI" };

static void Main_DebugMenu(int id) {
	char debug[0x20];
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	const char *ai = "";
	for (int i = 1; i < 4; i++) {
		if (GVar_GetNum(main_ai[i], 0) == 1) {
			ai = main_ai[i];
		}
	}
	Str_Printf(debug, sizeof(debug), "%d", (int)GVar_GetNum("Debug", 0));
	Main_Run(id, "Debug", Py_BuildValue("{s:s,s:s,s:s}", "back", Mode_ModGetI("Menu", "fromSinglePlayer", 0) ?
		"Menu.SinglePlayerMenu" : "Menu.MainMenu", "debug", debug, "ai", ai));
}

/* the next value of an engine variable, back to min after max */
static int Main_Cycle(const char *var, int min, int max) {
	int v = (int)GVar_GetNum(var, 0);
	v = v < max ? v + 1 : min;
	GVar_SetNum(var, v);
	return v;
}

/* the AI display after the one the entry shows, round */
static void Main_DebugAI(int id, const char *control) {
	char text[0x40];
	const char *opt = Script_GetStr(id, control, "Text");
	opt = strlen(opt) > 4 ? opt + 4 : "";
	int sel = -1;
	for (int i = 0; i < 4; i++) {
		if (i) {
			GVar_SetNum(main_ai[i], 0);
		}
		if (!strcmp(main_ai[i], opt)) {
			sel = i;
		}
	}
	sel = sel + 1 < 4 ? sel + 1 : 0;
	if (sel) {
		GVar_SetNum(main_ai[sel], 1);
	}
	Str_Printf(text, sizeof(text), "AI: %s", main_ai[sel]);
	Script_SetStr(id, control, "Text", text);
}

/* ---- the death in a single player game: the television switches off ---- */

/* the error of a call the C asked printed (the death camera goes on): ok passed on */
static byte Main_Asked(byte ok) {
	if (!ok) {
		PyErr_Print();
	}
	return ok;
}

/* o.name = v of a value, the error printed */
static void Main_SetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	if (!Mode_VSet(o, name, v)) {
		PyErr_Print();
	}
}

static void Main_SetAttrI(const nativeVal_t *o, const char *name, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	Main_SetAttr(o, name, &v);
}

static void Main_SetAttrS(const nativeVal_t *o, const char *name, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	Main_SetAttr(o, name, &v);
	Native_Free(&v);
}

/* SSound.Play(Sound.<sound>, Sound.<volume>) when both are there, the failure printed */
static void Main_Sound(const char *sound, const char *volume) {
	nativeVal_t snd, vol;
	byte haveSnd = Mode_ModGet("Sound", sound, &snd);
	byte haveVol = Mode_ModGet("Sound", volume, &vol);
	if (haveSnd && haveVol) {
		Mode_Printed(Mode_Play(&snd, &vol), "SSound", "Play");
	}
	PyErr_Clear();
	Native_Free(&vol);
	Native_Free(&snd);
}

/*
 * TVOff: the view leaves the player along a camera path up and back, then the screen goes dark. The engine calls are
 * asked one by one with their errors printed; 0 (no error left set) where the C returned NULL: no camera path or no
 * camera entity
 */
static byte Main_TVOff(void) {
	static const int black[4] = { 0, 0, 0, 0 };
	nativeVal_t lastCam = { 0 }, cp = { 0 }, lc = { 0 }, usr = { 0 }, camPos = { 0 }, camAng = { 0 }, yaw = { 0 };
	nativeVal_t rot = { 0 }, resv = { 0 }, dest = { 0 }, pos = { 0 }, name = { 0 }, v[3];
	byte ok = 0;
	Mode_Printed(Mode_Fade(0, black, black, 0.5), "SScorer", "Fade");
	Main_Cinema(0, 0);
	GVar_SetNum("CinemaBlackBandHeight", 320);
	byte haveLast = Main_Asked(Mode_CamSeam(0, &lastCam));
	byte haveCp = Main_Asked(Mode_CreateSeam("CamPath", 0, 0, 0, "CamPath", &cp));
	byte haveLc = haveLast && Main_Asked(Mode_EntitySeam(&lastCam, &lc));
	byte haveUsr = Main_Asked(Mode_UsrSeam(0, &usr));
	if (!haveCp || !haveLc) {
		goto done;
	}
	byte hasUsr = haveUsr && Native_True(&usr);
	if (hasUsr) {
		nativeVal_t act = { 0 };
		if (Mode_VObj(&usr, "ActCtrl", &act) && act.type == NV_STR && !strcmp(Native_Text(&act), "Dtritus")) {
			nativeVal_t u = { 0 };
			Main_SetAttrS(&usr, "ForceAction", "Death");
			if (Main_Asked(Mode_UsrSeam(0, &u))) {
				Main_SetAttrI(&u, "TimeSpeed", 1);
			}
			Native_Free(&u);
		}
		PyErr_Clear();
		Native_Free(&act);
	}
	GVar_SetNum("MotionBlurFactorBegin", 0);
	GVar_SetNum("MotionBlurFactorEnd", 0.75);
	GVar_SetNum("MotionBlurTime", 0.0);
	GVar_SetNum("MotionBlurEndTime", 1.75);
	GVar_SetNum("MotionBlurOffVar", 0);
	byte havePos = Mode_VObj(&lc, "camPos", &camPos);
	byte haveAng = Mode_VObj(&lc, "camAng", &camAng);
	PyErr_Clear();
	Main_SetAttrI(&cp, "nWayPoint", 2);
	if (havePos) {
		Main_SetAttr(&cp, "camPos", &camPos);
	}
	if (haveAng) {
		Main_SetAttr(&cp, "camAng", &camAng);
	}
	Main_SetAttr(&cp, "TgtCam00", &lastCam);
	Main_SetAttrS(&cp, "TgtCam00", "");
	Main_SetAttr(&cp, "TgtCam01", &lastCam);
	Main_SetAttrS(&cp, "TgtCam01", "");
	Native_SetFloat(&v[0], 3.5);
	Main_SetAttr(&cp, "Time00", &v[0]);
	Main_SetAttrI(&cp, "FadeIn00", 0);
	Main_SetAttrI(&cp, "FadeOut00", 1);
	Main_SetAttrI(&cp, "FadeIn01", 0);
	Main_SetAttrI(&cp, "FadeOut01", 0);
	byte car = 0;
	if (hasUsr) {
		car = Mode_VIsType(&usr, "Car") > 0;
		PyErr_Clear();
	}
	Main_SetAttrI(&cp, "FOV01", car ? 90 : 100);
	/* the end of the path: above and behind the camera, under the top of the level, before a wall */
	if (haveAng) {
		nativeVal_t tmp = { 0 };
		const nativeVal_t *e = Native_GetItem(&camAng, 1, 0, &tmp);
		if (e) {
			Native_Copy(&yaw, e);
		}
		Native_Free(&tmp);
	}
	PyErr_Clear();
	nativeVal_t off, ang;
	Mode_Ints3(&off, 0, car ? 500 : 200, car ? -1000 : -400);
	Native_SetEmpty(&ang, NV_TUPLE);
	Native_SetInt(Native_Append(&ang), 0);
	Native_Copy(Native_Append(&ang), &yaw);
	Native_SetInt(Native_Append(&ang), 0);
	byte haveRot = Main_Asked(Mode_Rotate(&off, &ang, &rot));
	Native_Free(&ang);
	Native_Free(&off);
	byte haveResv = haveRot && havePos && Main_Asked(Mode_VecAdd(&camPos, &rot, &resv));
	double top = GVar_GetNum("TopLimit", 0);
	if (haveResv) {
		nativeVal_t ty = { 0 };
		const nativeVal_t *y = Native_GetItem(&resv, 1, 0, &ty);
		if (y && (y->type == NV_FLOAT ? y->f > top : y->type == NV_INT && y->i > top)) {
			nativeVal_t tx = { 0 }, tz = { 0 }, clamped;
			const nativeVal_t *x = Native_GetItem(&resv, 0, 0, &tx), *z = Native_GetItem(&resv, 2, 0, &tz);
			nativeVal_t t;
			Native_SetFloat(&t, top);
			/* a missing item is the NULL Py_BuildValue refused: no clamped place, as the C had none */
			if (x && z) {
				Mode_Tuple3(&clamped, x, &t, z);
				Native_Free(&resv);
				resv = clamped;
			} else {
				Native_Free(&resv);
				haveResv = 0;
			}
			Native_Free(&tx);
			Native_Free(&tz);
		}
		Native_Free(&ty);
	}
	PyErr_Clear();
	byte haveDest = 0;
	if (haveResv) {
		nativeVal_t list;
		Native_SetText(&list, "", 0);
		haveDest = Main_Asked(Mode_TestLineSeam(&camPos, &resv, &list, &dest));
	}
	byte havePos01 = 0;
	if (haveDest && dest.type != NV_NONE) {
		nativeVal_t tmp = { 0 };
		const nativeVal_t *e = Native_GetItem(&dest, 0, 0, &tmp);
		if (e) {
			Native_Copy(&pos, e);
			havePos01 = 1;
		}
		Native_Free(&tmp);
	} else if (haveResv) {
		Native_Copy(&pos, &resv);
		havePos01 = 1;
	}
	PyErr_Clear();
	if (havePos01) {
		Main_SetAttr(&cp, "Pos01", &pos);
	}
	Native_SetEmpty(&ang, NV_TUPLE);
	Native_SetInt(Native_Append(&ang), 45);
	Native_Copy(Native_Append(&ang), &yaw);
	Native_SetInt(Native_Append(&ang), 0);
	Main_SetAttr(&cp, "Ang01", &ang);
	Native_Free(&ang);
	GVar_SetNum("CinemaBlackBandHeight", 0);
	byte haveName = Mode_VObj(&cp, "Name", &name);
	PyErr_Clear();
	if (haveName) {
		Native_SetInt(&v[0], 0);
		Native_Copy(&v[1], &name);
		Mode_SchedAt(Mode_Time(), "Menu", "SetCam", v, 2, NULL);
		Native_Free(&v[1]);
	}
	Mode_SchedAt(Mode_Time() + 2.0, "Menu", "TVOffEnd", NULL, 0, NULL);
	Main_Sound("DeathSound", "DeathVolume");
	ok = 1;
done:
	Native_Free(&name);
	Native_Free(&pos);
	Native_Free(&dest);
	Native_Free(&resv);
	Native_Free(&rot);
	Native_Free(&yaw);
	Native_Free(&camAng);
	Native_Free(&camPos);
	Native_Free(&usr);
	Native_Free(&lc);
	Native_Free(&cp);
	Native_Free(&lastCam);
	return ok;
}

/* TVOffEnd: noise and the fade to black, then the death screen */
static void Main_TVOffEnd(void) {
	static const int clear[4] = { 0, 0, 0, 0 }, dark[4] = { 0, 0, 0, 64 };
	GVar_SetNum("NoiseEndTime", 0.6);
	GVar_SetNum("NoiseTime", 0.0);
	GVar_SetNum("NoiseFactorBegin", 0);
	GVar_SetNum("NoiseFactor", 16);
	Mode_Printed(Mode_Fade(0, clear, dark, 0.75), "SScorer", "Fade");
	Mode_SchedAt(Mode_Time() + 0.6, "Menu", "BlackTV", NULL, 0, NULL);
	Main_Sound("TVOffSound", "TVOffVolume");
}

/* ---- the functions of Menu (adapters) ---- */

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
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Main_Run(id, "Audio", NULL);
	return Py_ReturnNone();
}

static PyObject *Main_VolumeStepArgs(PyObject *args, double sign) {
	int id;
	char *control, *prev;
	if (!PyArg_ParseTuple(args, "iss", &id, &control, &prev)) {
		return NULL;
	}
	return Mode_Result(Main_VolumeStep(id, prev, sign));
}

static PyObject *Menu_AudioVolumeSliderFocus_Sub(PyObject *self, PyObject *args) {
	(void)self;
	return Main_VolumeStepArgs(args, -1.0);
}

static PyObject *Menu_AudioVolumeSliderFocus_Add(PyObject *self, PyObject *args) {
	(void)self;
	return Main_VolumeStepArgs(args, 1.0);
}

static PyObject *Menu_AudioVolumeChange(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_ApplyVolumes(id);
	return Py_ReturnNone();
}

MODE_IDFN(Menu_MainMenu, (Main_MainMenu(id, control), 1))

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

MODE_IDFN(Menu_BackToGame, (Main_BackToGame(id), 1))

static byte Menu_DrawScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const char *mode;
	(void)out;
	if (!Mode_Parse(a, n, "is", &id, &mode)) {
		return 0;
	}
	Main_DrawScorer(id, mode);
	return 1;
}
MODE_ADAPTER(Menu, DrawScorer)

static byte Menu_InitMenuSys_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	double speed;
	(void)out;
	if (!Mode_Parse(a, n, "d", &speed)) {
		return 0;
	}
	Main_InitMenuSys(speed);
	return 1;
}
MODE_ADAPTER(Menu, InitMenuSys)

static byte Menu_MovieScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Main_MovieScorer(id);
	return 1;
}
MODE_ADAPTER(Menu, MovieScorer)

static PyObject *Menu_Init(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Main_Init(id);
	return Py_ReturnNone();
}

MODE_IFN(Menu_DeathInfo, 1, (Main_DeathInfo(id), 1))

static PyObject *Menu_ExitGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Mode_Printed(Mode_Exit(), "Scrap", "Exit");
	return Py_ReturnNone();
}

static PyObject *Menu_ByeMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_YesNo(id, "Menu_ExitGame_Question", "Menu.ExitGame", "Menu.MainMenu");
	return Py_ReturnNone();
}

static PyObject *Menu_SetSkillMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_SkillMenu(id, "Menu.SetSkill", "Menu.OptionsMenu", 1);
	return Py_ReturnNone();
}

static PyObject *Menu_SetSkill(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	return Mode_Result(Main_SetSkill(id, control));
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
	MODE_PRINTED(SaveGame, NewGame, "");
	Str_Printf(n, sizeof(n), "%d", Main_Digit(control, 4) - 1);
	SaveVar_Set("GameSkill", n);
	return Py_ReturnNone();
}

static PyObject *Menu_AuxAbortGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	SaveVars_Delete("");
	Main_QLoad("Levels/Menu");
	return Py_ReturnNone();
}

static PyObject *Menu_AbortGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_YesNo(id, "Menu_AbortGame_Question", "Menu.AuxAbortGame", "Menu.SinglePlayerMenu");
	return Py_ReturnNone();
}

/* AuxLoadGame(id, control): the game of slotToLoad, then back to it */
static void Main_AuxLoadGame(int id, const char *control) {
	Main_SaveCall(1, Mode_ModGetI("Menu", "slotToLoad", 0));
	MODE_PRINTED(Menu, BackToGame, "is", (long)id, control);
}

MODE_IDFN(Menu_AuxLoadGame, (Main_AuxLoadGame(id, control), 1))

static PyObject *Menu_LoadGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_LoadGame(id, control);
	return Py_ReturnNone();
}

static PyObject *Menu_LoadLastSavedGame(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Main_LoadLast();
	return Py_ReturnNone();
}

static PyObject *Menu_AuxSaveGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_AuxSaveGame(id, control);
	return Py_ReturnNone();
}

static PyObject *Menu_SaveGame(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_SaveGame(id, control);
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

/* Initialize takes any arguments (stage 3: its entry for the call of InitLevel through its ref) */
static byte Menu_Initialize_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Main_Initialize();
	return 1;
}

static PyObject *Menu_Initialize(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Main_Initialize();
	return Py_ReturnNone();
}

/* CreditsMenu: the credits run in the menu level */
static PyObject *Menu_CreditsMenu(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	GVar_SetNum("ShowCredits", 1);
	Main_QLoad("levels/menu");
	return Py_ReturnNone();
}

/* GameOver(id): the death of the player in a single player game, the television switches off */
static byte Menu_GameOver_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	MODE_PRINTED(MissionsFuncs, SinglePlayerDeath, "");
	MODE_PRINTED(Menu, TVOff, "");
	Main_Cinema(0, 0);
	return 1;
}
MODE_ADAPTER(Menu, GameOver)

static PyObject *Menu_DebugMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Main_DebugMenu(id);
	return Py_ReturnNone();
}

#define MAIN_CYCLE(func, var, max) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		Main_Cycle(var, 0, max); \
		return Py_ReturnNone(); \
	}

MAIN_CYCLE(Menu_DebugSet_Console, "ShowConsoleLog", 1)
MAIN_CYCLE(Menu_DebugSet_RenderInfo, "R_ShowInfo", 5)
MAIN_CYCLE(Menu_DebugSet_SoundInfo, "S_Show", 2)
MAIN_CYCLE(Menu_DebugSet_WorldInfo, "CShow", 7)
MAIN_CYCLE(Menu_DebugSet_ObjectPool, "O_Show", 1)
MAIN_CYCLE(Menu_DebugSet_SceneRenderTarget, "R_SceneRTarget", 1)
MAIN_CYCLE(Menu_DebugSet_MapCollision, "ShowMapCollision", 7)

static PyObject *Menu_DebugSet_Level(PyObject *self, PyObject *args) {
	char text[0x40];
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	Str_Printf(text, sizeof(text), "Debug Level: %d", Main_Cycle("Debug", 0, 3));
	Script_SetStr(id, "Item1", "Text", text);
	return Py_ReturnNone();
}

static PyObject *Menu_DebugSet_StencilShadows(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	GVar_SetNum("R_StencilTog", 1);
	return Py_ReturnNone();
}

static PyObject *Menu_DebugSet_AI(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	Main_DebugAI(id, control);
	return Py_ReturnNone();
}

/* the Python code of XboxPyDebug ran here (a debug line typed by the user): there is no interpreter, an error says so */
static PyObject *Menu_DebugSet_XboxPyDebug(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	const char *code = GVar_GetStr("XboxPyDebug");
	Con_Printf("XboxPyDebug: %s\n", code);
	Con_Errorf("XboxPyDebug: no Python interpreter in this build\n");
	return Py_ReturnNone();
}

/* DebugSet_CycleVar(var, min, max) */
static PyObject *Menu_DebugSet_CycleVar(PyObject *self, PyObject *args) {
	char *var;
	int min, max;
	(void)self;
	if (!PyArg_ParseTuple(args, "sii", &var, &min, &max)) {
		return NULL;
	}
	Main_Cycle(var, min, max);
	return Py_ReturnNone();
}

static PyObject *Menu_DebugSet_ChangeMapMenu(PyObject *self, PyObject *args) {
	(void)self;
	MAIN_ARGS(args);
	(void)control;
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Main_Run(id, "ChangeMap", NULL);
	return Py_ReturnNone();
}

static PyObject *Menu_DebugSet_ChangeMapExec(PyObject *self, PyObject *args) {
	char level[0x100];
	(void)self;
	MAIN_ARGS(args);
	Str_Printf(level, sizeof(level), "Levels/%s", Script_GetStr(id, control, "Text"));
	Main_QLoad(level);
	return Py_ReturnNone();
}

/* TVOff(): any arguments; 0 without an error where the C returned it (the errors of the calls were printed) */
MODE_FNANY(Menu, TVOff, Main_TVOff())

/* SetCam(id, path): the camera on the path, one frame later */
static PyObject *Menu_SetCam(PyObject *self, PyObject *args) {
	PyObject *o[2];
	nativeVal_t v[2];
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &o[0], &o[1])) {
		return NULL;
	}
	Native_Arg(o[0], &v[0]);
	Native_Arg(o[1], &v[1]);
	Mode_SchedAt(Mode_Time(), "Scrap", "SetCam", v, 2, NULL);
	Native_Free(&v[0]);
	Native_Free(&v[1]);
	return Py_ReturnNone();
}

static PyObject *Menu_TVOffEnd(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Main_TVOffEnd();
	return Py_ReturnNone();
}

static PyObject *Menu_BlackTV(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	MODE_PRINTED(Menu, DeathInfo, "i", 0L);
	return Py_ReturnNone();
}

PyMethodDef g_MenuMainMethods[] = {
	{ "SinglePlayerMenu", Menu_SinglePlayerMenu, 1, NULL },
	{ "AudioMenu", Menu_AudioMenu, 1, NULL },
	{ "AudioVolumeSliderFocus_Sub", Menu_AudioVolumeSliderFocus_Sub, 1, NULL },
	{ "AudioVolumeSliderFocus_Add", Menu_AudioVolumeSliderFocus_Add, 1, NULL },
	{ "AudioVolumeChange", Menu_AudioVolumeChange, 1, NULL },
	{ "MainMenu", Menu_MainMenu, 1, NULL },
	{ "CreateMainMenu", Menu_CreateMainMenu, 1, NULL },
	/* the store page of another game of the studio; there is no store here */
	{ "PlaySpacelords", Mode_PyNone, 1, NULL },
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
	{ "Initialize", Menu_Initialize, 1, NULL },
	{ "CreditsMenu", Menu_CreditsMenu, 1, NULL },
	{ "GameOver", Menu_GameOver, 1, NULL },
	{ "DebugMenu", Menu_DebugMenu, 1, NULL },
	{ "DebugSet_Level", Menu_DebugSet_Level, 1, NULL },
	{ "DebugSet_Console", Menu_DebugSet_Console, 1, NULL },
	{ "DebugSet_RenderInfo", Menu_DebugSet_RenderInfo, 1, NULL },
	{ "DebugSet_SoundInfo", Menu_DebugSet_SoundInfo, 1, NULL },
	{ "DebugSet_WorldInfo", Menu_DebugSet_WorldInfo, 1, NULL },
	{ "DebugSet_ObjectPool", Menu_DebugSet_ObjectPool, 1, NULL },
	{ "DebugSet_SceneRenderTarget", Menu_DebugSet_SceneRenderTarget, 1, NULL },
	{ "DebugSet_StencilShadows", Menu_DebugSet_StencilShadows, 1, NULL },
	{ "DebugSet_AI", Menu_DebugSet_AI, 1, NULL },
	{ "DebugSet_MapCollision", Menu_DebugSet_MapCollision, 1, NULL },
	{ "DebugSet_XboxPyDebug", Menu_DebugSet_XboxPyDebug, 1, NULL },
	{ "DebugSet_CycleVar", Menu_DebugSet_CycleVar, 1, NULL },
	{ "DebugSet_ChangeMapMenu", Menu_DebugSet_ChangeMapMenu, 1, NULL },
	{ "DebugSet_ChangeMapExec", Menu_DebugSet_ChangeMapExec, 1, NULL },
	{ "TVOff", Menu_TVOff, 1, NULL },
	{ "SetCam", Menu_SetCam, 1, NULL },
	{ "TVOffEnd", Menu_TVOffEnd, 1, NULL },
	{ "BlackTV", Menu_BlackTV, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MenuMain)
