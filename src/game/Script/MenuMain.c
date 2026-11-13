/*
 * The main menus of the Menu module in C (issue #2, step 4b): the main menu, the single player menu, the audio
 * options, the skill, new, saved and loaded games, the death screen, entering and leaving the menu during a game
 * and the scorer of a movie. The layout is data/menus/main.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the function table rows name the entries,
 * which parse the arguments as before (the same TypeErrors). The variables of other modules are read where the C read
 * them (Init.inMainMenu, isShipEdit, isDemo, Outdoor, isJunkYard, MultiPlayer, Menu.LastMode, LastTimeSpeed,
 * currentSoundPitch, saveGamesList, saveGamesSlots, slotToLoad, slotToOverwrite, fromSinglePlayer, isXbox, HeadBoss,
 * OutHeadMessenger, MovieScorerCall, Scorer.MarginH and MarginV, the sounds of Sound), the variables of Menu are
 * written at once. The menus, SaveGame, Speech and MissionsFuncs are called through their refs (PORT_CALL: directly
 * while the attribute holds the port's function, else by name, as a mod, a mission or a case may replace them); the
 * aliases and callbacks of Menu and the scorers (Menu.onCreateMainMenu, MovieScorerCall, RacerScorer.BackMenuFunc,
 * CharScorer.BackMenuFunc) are variables called by name (OBJLIB 5 (9c); BackMenuFunc through the entry of the function
 * it holds, MODE_PRINTEDCALL), the Init of the scorer of the mode by its module chosen at run time (MODE_CBATTR; both
 * are the entry Mode_PyNone_E); SScorer.SetCinema, Fade, SSound.Play and Scrap.Exit, GetCam, GetEntity, UsrEntity,
 * CreateEntity and TestLine go through their seams; the death camera schedules functions of Menu and Scrap.SetCam (the
 * scheduler takes their entries, Port_Scheduled); the list of saved games comes from the engine as a C array and the
 * .def sections get their $names as values (Mode_DefRun). Every function of the table has a native entry (stage 6,
 * batch 6h: Mode_IdControl, Mode_Parse, MODE_FNANY).
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
static void Step_Volume(int id, const nativeVal_t *args, int n) {
	const char *slider, *var;
	if (!Mode_Parse(args, n, "ss", &slider, &var)) {
		Native_ErrPrint();
		return;
	}
	Script_SetNum(id, slider, "Value", GVar_GetNum(var, 0));
}

/* the entries without a left neighbour lead to the Spacelords button */
static void Step_Spacelords(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
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

/* a section of main.def with its $names: n pairs of a name and a value (Mode_DefRun) */
#define Main_Run(id, section, n, ...) \
	Mode_DefRun(id, MAIN_FILE, section, main_def, main_steps, 0, n __VA_OPT__(, ) __VA_ARGS__)

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

/* len(seq) as abstract.c Sequence_Length takes it: -1 for a value without length (the error dropped, the -1 kept as the
 * C kept it) */
static int Main_Len(const nativeVal_t *seq) {
	int n;
	if (!Native_Len(seq, &n)) {
		Native_ErrClear();
		return -1;
	}
	return n;
}

/* Menu.saveGamesList into *out (lists held as one NV_OBJ made native): 0 when it is not there (the error dropped) */
static byte Main_SavesVar(nativeVal_t *out) {
	if (!Mode_ModGet("Menu", "saveGamesList", out)) {
		Native_ErrClear();
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Single", 4, "menu", Mode_S(&t, main ? "main" : "game"), "saves", Mode_I(&t, saves > 0), "xbox",
		Mode_I(&t, xbox), "cansave", Mode_I(&t, canSave));
	Mode_Done(&t, 1);
	if (!main) {
		Main_PutInt("fromSinglePlayer", 1);
	}
}

static void Main_Create(int id, const char *back) {
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	int saves = Main_SaveGames(1);
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Main", 3, "saves", Mode_I(&t, saves > 0), "back", Mode_S(&t, back), "steam", Mode_I(&t, 0));
	Mode_Done(&t, 1);
}

/* the main menu through Menu.onCreateMainMenu, which a mod may replace; Escape returns to the game */
static void Main_MainMenu(int id, const char *control) {
	char mode[0x20];
	Main_LastMode(mode, sizeof(mode));
	/* OBJLIB 5 (9c): the alias Menu.onCreateMainMenu (= CreateMainMenu of menu.mod) by name */
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Movie", 1, "talky", Mode_I(&t, 475 - marginV));
	Mode_Done(&t, 1);
	/* OBJLIB 5 (9c): the callback variable of Menu */
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
	Native_ErrClear();
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
	/* OBJLIB 5 (9c): the aliases BackMenuFunc (= Create of racerscorer.mod and charscorer.mod) by name, called through
	 * the entry of the function they hold */
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Death", 2, "show", Mode_S(&t, show), "text2", Mode_S(&t, text));
	Mode_Done(&t, 1);
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Skill", 4, "func", Mode_S(&t, func), "back", Mode_S(&t, back), "current", Mode_S(&t, current), "lit",
		Mode_I(&t, lit));
	Mode_Done(&t, 1);
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
		Native_ErrClear();
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
			Native_ErrClear();
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Games", 4, "title", Mode_S(&t, Script_Lang(save ? "Menu_SaveGame" : "Menu_LoadGame")), "entries",
		&entries, "autosave", Mode_I(&t, autoSave), "current", Mode_S(&t, current));
	Mode_Done(&t, 1);
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
		Native_ErrPrint();
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
	/* OBJLIB 7: PCMenu.Menu is the module object of Menu (held as NV_OBJ) */
	nativeVal_t menu;
	if (Mode_RefModule("Menu", &menu)) {
		Mode_ModPut("PCMenu", "Menu", &menu);
	}
	Native_Free(&menu);
	Native_ErrClear();
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
	modeVals_t t = { .n = 0 };
	byte heads = !main && !(shipEdit && GVar_GetNum("isXbox", 0));
	byte bosses = edit || strcmp(Main_SaveVarOr("GameAct"), "Epilogue");
	byte boss = Mode_ModGetI("Menu", "HeadBoss", 0) || edit;
	byte lonelyMerc = !strcmp(Main_SaveVarOr("LonelyMercDataActive"), "1");
	byte bertos = !strcmp(Main_SaveVarOr("CreateBertos"), "1");
	byte messenger = Mode_ModGetI("Menu", "OutHeadMessenger", 0) != 0;
	Main_Run(0, "preload", 15, "main", Mode_I(&t, main), "xbox", Mode_I(&t, xbox), "outdoor", Mode_I(&t, outdoor),
		"outdoorpc", Mode_I(&t, outdoor && !xbox), "metro", Mode_I(&t, !shipEdit && !junkyard && !xbox), "multi",
		Mode_I(&t, multiPlayer || net), "editor", Mode_I(&t, junkyard || shipEdit || net), "heads", Mode_I(&t, heads),
		"edit", Mode_I(&t, edit), "bosses", Mode_I(&t, bosses), "boss", Mode_I(&t, boss), "dtritus",
		Mode_I(&t, !outdoor || edit), "lonelymerc", Mode_I(&t, lonelyMerc), "bertos", Mode_I(&t, bertos), "messenger",
		Mode_I(&t, messenger));
	Mode_Done(&t, 1);
	if (!main) {
		/* the module chosen at run time (both Init are the entry Mode_PyNone_E) */
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
	modeVals_t t = { .n = 0 };
	Main_Run(id, "Debug", 3, "back", Mode_S(&t, Mode_ModGetI("Menu", "fromSinglePlayer", 0) ? "Menu.SinglePlayerMenu" :
		"Menu.MainMenu"), "debug", Mode_S(&t, debug), "ai", Mode_S(&t, ai));
	Mode_Done(&t, 1);
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
		Native_ErrPrint();
	}
	return ok;
}

/* o.name = v of a value, the error printed */
static void Main_SetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	if (!Mode_VSet(o, name, v)) {
		Native_ErrPrint();
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
	Native_ErrClear();
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
		Native_ErrClear();
		Native_Free(&act);
	}
	GVar_SetNum("MotionBlurFactorBegin", 0);
	GVar_SetNum("MotionBlurFactorEnd", 0.75);
	GVar_SetNum("MotionBlurTime", 0.0);
	GVar_SetNum("MotionBlurEndTime", 1.75);
	GVar_SetNum("MotionBlurOffVar", 0);
	byte havePos = Mode_VObj(&lc, "camPos", &camPos);
	byte haveAng = Mode_VObj(&lc, "camAng", &camAng);
	Native_ErrClear();
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
		Native_ErrClear();
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
	Native_ErrClear();
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
			/* a missing item is the NULL modsupport.c BuildValue refused: no clamped place, as the C had none */
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
	Native_ErrClear();
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
	Native_ErrClear();
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
	Native_ErrClear();
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

/* ---- the functions of Menu (entries) ---- */

#define MAIN_ARGS(a, n) \
	int id; \
	const char *control; \
	if (!Mode_IdControl(a, n, &id, &control)) { \
		return 0; \
	}

static byte Menu_SinglePlayerMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_Single(id);
	return 1;
}

static byte Menu_AudioMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Main_Run(id, "Audio", 0);
	return 1;
}

static byte Main_VolumeStepArgs(const nativeVal_t *a, int n, double sign) {
	int id;
	const char *control, *prev;
	if (!Mode_Parse(a, n, "iss", &id, &control, &prev)) {
		return 0;
	}
	return Main_VolumeStep(id, prev, sign);
}

static byte Menu_AudioVolumeSliderFocus_Sub_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Main_VolumeStepArgs(a, n, -1.0);
}

static byte Menu_AudioVolumeSliderFocus_Add_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Main_VolumeStepArgs(a, n, 1.0);
}

static byte Menu_AudioVolumeChange_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_ApplyVolumes(id);
	return 1;
}

MODE_IDFN(Menu_MainMenu, (Main_MainMenu(id, control), 1))

/* CreateMainMenu(id, control, backfunc) */
static byte Menu_CreateMainMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *control;
	const char *back;
	(void)out;
	if (!Mode_Parse(a, n, "iOs", &id, &control, &back)) {
		return 0;
	}
	Main_Create(id, back);
	return 1;
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

static byte Menu_InitMenuSys_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	double speed;
	(void)out;
	if (!Mode_Parse(a, n, "d", &speed)) {
		return 0;
	}
	Main_InitMenuSys(speed);
	return 1;
}

static byte Menu_MovieScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Main_MovieScorer(id);
	return 1;
}

static byte Menu_Init_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Main_Init(id);
	return 1;
}

MODE_IFN(Menu_DeathInfo, 1, (Main_DeathInfo(id), 1))

static byte Menu_ExitGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Mode_Printed(Mode_Exit(), "Scrap", "Exit");
	return 1;
}

static byte Menu_ByeMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_YesNo(id, "Menu_ExitGame_Question", "Menu.ExitGame", "Menu.MainMenu");
	return 1;
}

static byte Menu_SetSkillMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_SkillMenu(id, "Menu.SetSkill", "Menu.OptionsMenu", 1);
	return 1;
}

static byte Menu_SetSkill_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	return Main_SetSkill(id, control);
}

static byte Menu_StartNewGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_SkillMenu(id, "Menu.NewGame", "Menu.MainMenu", 0);
	return 1;
}

static byte Menu_NewGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char skill[0x10];
	(void)out;
	MAIN_ARGS(a, n);
	(void)id;
	MODE_PRINTED(SaveGame, NewGame, "");
	Str_Printf(skill, sizeof(skill), "%d", Main_Digit(control, 4) - 1);
	SaveVar_Set("GameSkill", skill);
	return 1;
}

static byte Menu_AuxAbortGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	SaveVars_Delete("");
	Main_QLoad("Levels/Menu");
	return 1;
}

static byte Menu_AbortGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_YesNo(id, "Menu_AbortGame_Question", "Menu.AuxAbortGame", "Menu.SinglePlayerMenu");
	return 1;
}

/* AuxLoadGame(id, control): the game of slotToLoad, then back to it */
static void Main_AuxLoadGame(int id, const char *control) {
	Main_SaveCall(1, Mode_ModGetI("Menu", "slotToLoad", 0));
	MODE_PRINTED(Menu, BackToGame, "is", (long)id, control);
}

MODE_IDFN(Menu_AuxLoadGame, (Main_AuxLoadGame(id, control), 1))

static byte Menu_LoadGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	Main_LoadGame(id, control);
	return 1;
}

static byte Menu_LoadLastSavedGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Main_LoadLast();
	return 1;
}

static byte Menu_AuxSaveGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	Main_AuxSaveGame(id, control);
	return 1;
}

static byte Menu_SaveGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	Main_SaveGame(id, control);
	return 1;
}

static byte Menu_LoadGameMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_GamesMenu(id, 0);
	return 1;
}

static byte Menu_SaveGameMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_GamesMenu(id, 1);
	return 1;
}

static byte Menu_isAutoSave_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Native_SetInt(out, Main_IsAutoSave(name));
	return 1;
}

static byte Menu_ComposeSaveGameName_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	char text[0x200];
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Main_SaveName(name, text, sizeof(text));
	Native_SetText(out, text, (int)strlen(text));
	return 1;
}

/* Initialize takes any arguments (stage 3: its entry for the call of InitLevel through its ref) */
MODE_FNANY(Menu, Initialize, (Main_Initialize(), 1))

/* CreditsMenu: the credits run in the menu level */
static byte Menu_CreditsMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	GVar_SetNum("ShowCredits", 1);
	Main_QLoad("levels/menu");
	return 1;
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

static byte Menu_DebugMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Main_DebugMenu(id);
	return 1;
}

/* a debug entry that steps an engine variable (any arguments) */
#define MAIN_CYCLE(name, var, max) MODE_FNANY(Menu, name, (Main_Cycle(var, 0, max), 1))

MAIN_CYCLE(DebugSet_Console, "ShowConsoleLog", 1)
MAIN_CYCLE(DebugSet_RenderInfo, "R_ShowInfo", 5)
MAIN_CYCLE(DebugSet_SoundInfo, "S_Show", 2)
MAIN_CYCLE(DebugSet_WorldInfo, "CShow", 7)
MAIN_CYCLE(DebugSet_ObjectPool, "O_Show", 1)
MAIN_CYCLE(DebugSet_SceneRenderTarget, "R_SceneRTarget", 1)
MAIN_CYCLE(DebugSet_MapCollision, "ShowMapCollision", 7)

static byte Menu_DebugSet_Level_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char text[0x40];
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	Str_Printf(text, sizeof(text), "Debug Level: %d", Main_Cycle("Debug", 0, 3));
	Script_SetStr(id, "Item1", "Text", text);
	return 1;
}

static byte Menu_DebugSet_StencilShadows_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	GVar_SetNum("R_StencilTog", 1);
	return 1;
}

static byte Menu_DebugSet_AI_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	Main_DebugAI(id, control);
	return 1;
}

/* the Python code of XboxPyDebug ran here (a debug line typed by the user): there is no interpreter, an error says
 * so */
static byte Menu_DebugSet_XboxPyDebug_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	const char *code = GVar_GetStr("XboxPyDebug");
	Con_Printf("XboxPyDebug: %s\n", code);
	Con_Errorf("XboxPyDebug: no Python interpreter in this build\n");
	return 1;
}

/* DebugSet_CycleVar(var, min, max) */
static byte Menu_DebugSet_CycleVar_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *var;
	int min, max;
	(void)out;
	if (!Mode_Parse(a, n, "sii", &var, &min, &max)) {
		return 0;
	}
	Main_Cycle(var, min, max);
	return 1;
}

static byte Menu_DebugSet_ChangeMapMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	MAIN_ARGS(a, n);
	(void)control;
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Main_Run(id, "ChangeMap", 0);
	return 1;
}

static byte Menu_DebugSet_ChangeMapExec_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char level[0x100];
	(void)out;
	MAIN_ARGS(a, n);
	Str_Printf(level, sizeof(level), "Levels/%s", Script_GetStr(id, control, "Text"));
	Main_QLoad(level);
	return 1;
}

/* TVOff(): any arguments; 0 without an error where the C returned it (the errors of the calls were printed) */
MODE_FNANY(Menu, TVOff, Main_TVOff())

/* SetCam(id, path): the camera on the path, one frame later */
MODE_FN(Menu, SetCam, 2, (Mode_SchedAt(Mode_Time(), "Scrap", "SetCam", a, 2, NULL), 1))

static byte Menu_TVOffEnd_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Main_TVOffEnd();
	return 1;
}

static byte Menu_BlackTV_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	MODE_PRINTED(Menu, DeathInfo, "i", 0L);
	return 1;
}

const portFn_t g_MenuMainMethods[] = {
	PORT_FN(SinglePlayerMenu, Menu_SinglePlayerMenu),
	PORT_FN(AudioMenu, Menu_AudioMenu),
	PORT_FN(AudioVolumeSliderFocus_Sub, Menu_AudioVolumeSliderFocus_Sub),
	PORT_FN(AudioVolumeSliderFocus_Add, Menu_AudioVolumeSliderFocus_Add),
	PORT_FN(AudioVolumeChange, Menu_AudioVolumeChange),
	PORT_FN(MainMenu, Menu_MainMenu),
	PORT_FN(CreateMainMenu, Menu_CreateMainMenu),
	PORT_FN(PlaySpacelords, Mode_PyNone),
	PORT_FN(BackToGame, Menu_BackToGame),
	PORT_FN(DrawScorer, Menu_DrawScorer),
	PORT_FN(InitMenuSys, Menu_InitMenuSys),
	PORT_FN(MovieScorer, Menu_MovieScorer),
	PORT_FN(Init, Menu_Init),
	PORT_FN(DeathInfo, Menu_DeathInfo),
	PORT_FN(ExitGame, Menu_ExitGame),
	PORT_FN(ByeMenu, Menu_ByeMenu),
	PORT_FN(SetSkillMenu, Menu_SetSkillMenu),
	PORT_FN(SetSkill, Menu_SetSkill),
	PORT_FN(StartNewGame, Menu_StartNewGame),
	PORT_FN(NewGame, Menu_NewGame),
	PORT_FN(AuxAbortGame, Menu_AuxAbortGame),
	PORT_FN(AbortGame, Menu_AbortGame),
	PORT_FN(AuxLoadGame, Menu_AuxLoadGame),
	PORT_FN(LoadGame, Menu_LoadGame),
	PORT_FN(LoadLastSavedGame, Menu_LoadLastSavedGame),
	PORT_FN(AuxSaveGame, Menu_AuxSaveGame),
	PORT_FN(SaveGame, Menu_SaveGame),
	PORT_FN(LoadGameMenu, Menu_LoadGameMenu),
	PORT_FN(SaveGameMenu, Menu_SaveGameMenu),
	PORT_FN(isAutoSave, Menu_isAutoSave),
	PORT_FN(ComposeSaveGameName, Menu_ComposeSaveGameName),
	PORT_FN(Initialize, Menu_Initialize),
	PORT_FN(CreditsMenu, Menu_CreditsMenu),
	PORT_FN(GameOver, Menu_GameOver),
	PORT_FN(DebugMenu, Menu_DebugMenu),
	PORT_FN(DebugSet_Level, Menu_DebugSet_Level),
	PORT_FN(DebugSet_Console, Menu_DebugSet_Console),
	PORT_FN(DebugSet_RenderInfo, Menu_DebugSet_RenderInfo),
	PORT_FN(DebugSet_SoundInfo, Menu_DebugSet_SoundInfo),
	PORT_FN(DebugSet_WorldInfo, Menu_DebugSet_WorldInfo),
	PORT_FN(DebugSet_ObjectPool, Menu_DebugSet_ObjectPool),
	PORT_FN(DebugSet_SceneRenderTarget, Menu_DebugSet_SceneRenderTarget),
	PORT_FN(DebugSet_StencilShadows, Menu_DebugSet_StencilShadows),
	PORT_FN(DebugSet_AI, Menu_DebugSet_AI),
	PORT_FN(DebugSet_MapCollision, Menu_DebugSet_MapCollision),
	PORT_FN(DebugSet_XboxPyDebug, Menu_DebugSet_XboxPyDebug),
	PORT_FN(DebugSet_CycleVar, Menu_DebugSet_CycleVar),
	PORT_FN(DebugSet_ChangeMapMenu, Menu_DebugSet_ChangeMapMenu),
	PORT_FN(DebugSet_ChangeMapExec, Menu_DebugSet_ChangeMapExec),
	PORT_FN(TVOff, Menu_TVOff),
	PORT_FN(SetCam, Menu_SetCam),
	PORT_FN(TVOffEnd, Menu_TVOffEnd),
	PORT_FN(BlackTV, Menu_BlackTV),
	PORT_FNEND
};

PORTS(MenuMain)
