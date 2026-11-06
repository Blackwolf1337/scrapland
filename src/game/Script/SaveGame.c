/*
 * The SaveGame module in C (scripts/savegame.py, issue #2): saving and loading the single player game, the way points
 * (map, spawn point, character, batteries, alarm, money), the start of a new game with its save variables, the level
 * change (QLoad) and the save registers of the missions (SaveReg, GetReg). The module code (the save variables it
 * creates, the photo message) stays in the script. Values keep their Python types and every failing line stops the
 * function like the script.
 */
#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* Scrap.SetSaveVar(name, v) (v a Python value, not given away) */
static byte SG_SetSave(const char *name, PyObject *v) {
	return v && Script_Try("Scrap", "SetSaveVar", "(sO)", name, v);
}

static byte SG_SetSaveS(const char *name, const char *v) {
	return Script_Try("Scrap", "SetSaveVar", "(ss)", name, v);
}

/* Scrap.GetTime() as a Python float (a new reference) */
static PyObject *SG_Time(void) {
	return PyFloat_FromDouble((double)Timer_GetTime(&g_pWorld->timer));
}

/* a function of this module as other code may have replaced it (the script calls its own globals) */
static byte SG_Call(const char *func, const char *fmt, ...);

/* ---- the mission descriptions ---- */

/* MMDesc(Desc, Persist): the primary mission text, its scorer on or off, kept in the save game when Persist */
static PyObject *SaveGame_MMDesc(PyObject *self, PyObject *args) {
	PyObject *desc, *persist;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &desc, &persist) || !Script_Import((const char *const[]){ "Scorer", NULL }) ||
		!Script_Try("Scrap", "Set", "(sO)", "PrimaryMissionDesc", desc)) {
		return NULL;
	}
	int on = desc != Py_None ? Script_True(desc) : 0;
	if (on < 0 || !Script_Try("Scorer", "ShowMissionScorer", "(iii)", 0, 0, on)) {
		return NULL;
	}
	int keep = Script_True(persist);
	if (keep < 0 || (keep && !SG_SetSave("PrimaryMissionDesc", desc))) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* SMDesc(Desc, Persist): the secondary mission text */
static PyObject *SaveGame_SMDesc(PyObject *self, PyObject *args) {
	PyObject *desc, *persist;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &desc, &persist) || !Script_Import((const char *const[]){ "Scorer", NULL }) ||
		!Script_Try("Scrap", "Set", "(sO)", "SecondaryMissionDesc", desc)) {
		return NULL;
	}
	int keep = Script_True(persist);
	if (keep < 0 || (keep && !SG_SetSave("SecondaryMissionDesc", desc))) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* ---- the alarm and the money of a way point ---- */

/* SaveAlarm(): the state of the alarm into the save variables (once the level runs) */
static byte SG_SaveAlarm(void) {
	if (Timer_GetTime(&g_pWorld->timer) <= 0.0f) {
		return 1;
	}
	scriptRefs_t r = { .n = 0 };
	PyObject *a = K(Script_Ask("Scrap", "GetAlarm", "()")), *on = a ? K(Script_Index(a, 0)) : NULL;
	int active = on ? Script_True(on) : -1;
	PyObject *b = active >= 0 && SG_SetSaveS("AlarmActive", active ? "1" : "0") ? K(Script_Ask("Scrap", "GetAlarm", "()")) : NULL;
	PyObject *level = b ? K(Script_Index(b, 1)) : NULL, *text = level ? K(Script_Repr(level)) : NULL;
	byte ok = text && SG_SetSave("AlarmStatus", text);
	Script_Done(&r, NULL);
	return ok;
}

/* SaveMoney() */
static byte SG_SaveMoney(void) {
	if (Timer_GetTime(&g_pWorld->timer) <= 0.0f) {
		return 1;
	}
	scriptRefs_t r = { .n = 0 };
	PyObject *m = K(Script_Ask("Scrap", "GetMoney", "()")), *text = m ? K(Script_Repr(m)) : NULL;
	byte ok = text && SG_SetSave("Money", text);
	Script_Done(&r, NULL);
	return ok;
}

static PyObject *SaveGame_SaveAlarm(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return SG_SaveAlarm() ? Py_ReturnNone() : NULL;
}

static PyObject *SaveGame_SaveMoney(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return SG_SaveMoney() ? Py_ReturnNone() : NULL;
}

/* print Scrap.GetLockAlarm(), Scrap.GetSaveVar("Char"), Scrap.GetAlarmChars() (the script's trace of LoadAlarm) */
static byte SG_PrintAlarm(void) {
	scriptRefs_t r = { .n = 0 };
	PyObject *items[3];
	items[0] = K(Script_Ask("Scrap", "GetLockAlarm", "()"));
	items[1] = items[0] ? K(Script_SaveVar("Char")) : NULL;
	items[2] = items[1] ? K(Script_Ask("Scrap", "GetAlarmChars", "()")) : NULL;
	byte ok = items[2] && Script_Print(items, 3);
	Script_Done(&r, NULL);
	return ok;
}

/* LoadAlarm(): the alarm of the save game back (always on while the boss controls the robots) */
static PyObject *SaveGame_LoadAlarm(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!SG_PrintAlarm()) {
		FAIL();
	}
	PyObject *set = K(Script_Ask("SInput", "GetActionSet", "()"));
	PyObject *status = set && Script_Try("SInput", "SetActionSet", "(s)", "Walk") ? K(Script_SaveVar("AlarmStatus")) : NULL;
	PyObject *level = status ? K(Script_EvalIn("SaveGame", status)) : NULL;
	PyObject *boss = level && Script_Try("Scrap", "SetAlarm", "(O)", level) && Script_Try("Scrap", "SetAlarmGrow", "(i)", -1) ?
		K(Script_SaveVar("RobotsControlledByBoss")) : NULL;
	PyObject *active = boss ? K(Script_SaveVar("AlarmActive")) : NULL;
	if (!active) {
		FAIL();
	}
	byte bossRobots = Script_Is(boss, "1");
	if (Script_Is(active, "1") || bossRobots) {
		if (!Script_Try("Scrap", "SetAlarm", "(i)", 1) || (bossRobots && !Script_Try("Scrap", "Set", "(si)", "AlarmFallDelta", 0))) {
			FAIL();
		}
	} else if (!Script_Try("Scrap", "SetAlarm", "(i)", 0)) {
		FAIL();
	}
	PyObject *ch = K(Script_SaveVar("Char"));
	if (!ch || !Script_Try("Scrap", "SetAlarmChar", "(O)", ch) || !Script_Try("SInput", "SetActionSet", "(O)", set) ||
		!SG_PrintAlarm()) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* LoadMoney() */
static PyObject *SaveGame_LoadMoney(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *m = K(Script_SaveVar("Money")), *v = m ? K(Script_EvalIn("SaveGame", m)) : NULL;
	if (!v || !Script_Try("Scrap", "SetMoney", "(O)", v)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* InitVars(): the alarm and the money at the start of a single player level */
static PyObject *SaveGame_InitVars(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return SG_Call("LoadAlarm", "()") && SG_Call("LoadMoney", "()") ? Py_ReturnNone() : NULL;
}

/* ---- way points ---- */

/* MarkWayPoint(Map, Spawn, ExtraComeFrom=""): where the game goes on, the character and its batteries, the alarm and
 * the money */
static PyObject *SaveGame_MarkWayPoint(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Map", "Spawn", "ExtraComeFrom", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *map, *spawn, *from = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &map, &spawn, &from)) {
		return NULL;
	}
	if (!from) {
		from = K(PyString_FromString(""));
	}
	if (!SG_SetSave("Map", map) || !SG_SetSave("Spawn", spawn) || !SG_SetSave("ComeFrom", from)) {
		FAIL();
	}
	PyObject *e = K(Script_Usr());
	int is = e ? Script_True(e) : -1;
	if (is < 0) {
		FAIL();
	}
	if (is) {
		PyObject *w = K(Script_Method(e, "IsType", "(s)", "WalkChar"));
		int walk = w ? Script_True(w) : -1;
		if (walk < 0) {
			FAIL();
		}
		if (walk) {
			PyObject *type = K(Script_GetAttr(e, "ActCtrl"));
			PyObject *bar = type && SG_SetSave("Char", type) ? K(Script_Ask("Scrap", "Get", "(s)", "EnergyBarActive")) : NULL;
			int on = bar ? Script_True(bar) : -1;
			if (on < 0) {
				FAIL();
			}
			if (on) {
				PyObject *energy = K(Script_GetAttr(e, "Energy")), *text = energy ? K(Script_Repr(energy)) : NULL;
				if (!text || !SG_SetSave("AcBatteries", text)) {
					FAIL();
				}
			}
		}
	}
	if (!SG_Call("SaveAlarm", "()") || !SG_Call("SaveMoney", "()")) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* QWayPoint(): a way point at the current map, spawn point and arrival */
static PyObject *SaveGame_QWayPoint(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *path = Script_Import((const char *const[]){ "Init", NULL }) ? K(Script_Global("Init", "Path")) : NULL;
	PyObject *spawn = path ? K(Script_SaveVar("Spawn")) : NULL, *from = spawn ? K(Script_SaveVar("ComeFrom")) : NULL;
	if (!from || !SG_Call("MarkWayPoint", "(OOO)", path, spawn, from)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ---- the level change ---- */

/* QLoad(MapName, useGameplayTime=1): the level after this frame, the music and voices off, the gameplay time kept */
static PyObject *SaveGame_QLoad(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "MapName", "useGameplayTime", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *map, *useTime = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &map, &useTime) || !Script_Import((const char *const[]){ "string", NULL })) {
		return NULL;
	}
	PyObject *sm3 = K(Script_PlusS(Script_Ref(map), "/Map/Map3D.SM3"));
	PyObject *has = sm3 ? K(Script_Ask("Scrap", "FileExist", "(O)", sm3)) : NULL;
	int found = has ? Script_True(has) : -1;
	if (found == 0) {
		PyObject *m3d = K(Script_PlusS(Script_Ref(map), "/Map/Map3D.M3D"));
		has = m3d ? K(Script_Ask("Scrap", "FileExist", "(O)", m3d)) : NULL;
		found = has ? Script_True(has) : -1;
	}
	if (found < 0) {
		FAIL();
	}
	if (!found) {
		PyObject *msg = K(Script_Mod(K(PyString_FromString("Map not found: %s\n")), map));
		if (!msg || !Script_Try("Scrap", "ConsoleError", "(O)", msg)) {
			FAIL();
		}
		return Script_Done(&r, Py_ReturnNone());
	}
	if (!Script_Try("Scrap", "MusicPlayer", "(ss)", "", "stop")) {
		FAIL();
	}
	for (int i = 0; i < 4; i++) {
		if (!Script_Try("SSound", "StopVoice", "(i)", i)) {
			FAIL();
		}
	}
	if (!Script_Try("SSound", "StopAllSounds", "()")) {
		FAIL();
	}
	int keepTime = useTime ? Script_True(useTime) : 1;
	if (keepTime < 0) {
		FAIL();
	}
	if (keepTime) {
		PyObject *t = Script_Try("Scrap", "CreateSaveVar", "(ss)", "GameplayTime", "0") ? K(Script_SaveVar("GameplayTime")) : NULL;
		PyObject *v = t ? K(Script_EvalIn("SaveGame", t)) : NULL, *now = v ? K(SG_Time()) : NULL;
		PyObject *sum = now ? K(PyNumber_Add(v, now)) : NULL, *text = sum ? K(Script_Repr(sum)) : NULL;
		if (!text || !SG_SetSave("GameplayTime", text)) {
			FAIL();
		}
	}
	PyObject *upper = SG_SetSave("Map", map) ? K(Script_Ask("string", "upper", "(O)", map)) : NULL;
	if (!upper) {
		FAIL();
	}
	/* the maps with credits */
	int credits = Script_In(upper, (const char *const[]){ "LEVELS/MENU", "LEVELS/ORBIT", "LEVELS/FAKE", "LEVELS/JUNKYARD", NULL });
	if (!Script_Try("Scrap", "Set", "(si)", "R_CreditsRTarget", credits)) {
		FAIL();
	}
	PyObject *server = K(Script_Ask("SNet", "IsServer", "()"));
	int s = server ? Script_True(server) : -1;
	if (s < 0) {
		FAIL();
	}
	if (s) {
		if (!Script_Try("SNet", "CloseServer", "(O)", map)) {
			FAIL();
		}
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *client = K(Script_Ask("SNet", "IsClient", "()"));
	int c = client ? Script_True(client) : -1;
	if (c < 0) {
		FAIL();
	}
	if (c) {
		if (!Script_Try("SNet", "CloseClient", "(O)", map)) {
			FAIL();
		}
	} else if (!Script_Import((const char *const[]){ "Speech", NULL }) || !Script_Try("Speech", "CallEndOfSpeech", "()") ||
		!Script_Try("Scrap", "LoadLevel", "(O)", map)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* StartGame(CleanChallenge=1, CleanRevenge=0): the map of the save game, the current one becomes the previous one,
 * the challenge (and the revenge) cleaned unless kept */
static PyObject *SaveGame_StartGame(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CleanChallenge", "CleanRevenge", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *cleanChallenge = NULL, *cleanRevenge = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", names, &cleanChallenge, &cleanRevenge) ||
		!Script_Import((const char *const[]){ "Init", "Challenge", NULL })) {
		return NULL;
	}
	PyObject *path = K(Script_Global("Init", "Path"));
	int a = path && SG_SetSave("PrevMap", path) ? (cleanChallenge ? Script_True(cleanChallenge) : 1) : -1;
	if (a < 0 || (a && !Script_Try("Challenge", "CleanChallenge", "()"))) {
		FAIL();
	}
	int b = cleanRevenge ? Script_True(cleanRevenge) : 0;
	if (b < 0 || (b && !Script_Try("Challenge", "CleanRevenge", "()"))) {
		FAIL();
	}
	PyObject *map = K(Script_SaveVar("Map"));
	if (!map || !SG_Call("QLoad", "(O)", map)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* Restart(): the current level again */
static PyObject *SaveGame_Restart(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *path = Script_Import((const char *const[]){ "Init", NULL }) ? K(Script_Global("Init", "Path")) : NULL;
	if (!path || !SG_Call("QLoad", "(O)", path)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ---- saving and loading ---- */

/* InstantSaveGame(iSlot, showMsgOnFailure=1): the game into Save/Save<n>.sav with the map and the gameplay time as its
 * name, the message of the result, the autosave slot 0 too */
static PyObject *SaveGame_InstantSaveGame(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "iSlot", "showMsgOnFailure", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *slot, *show = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &slot, &show) ||
		!Script_Import((const char *const[]){ "string", "Speech", "Init", NULL }) ||
		!SG_SetSaveS("AutoSaveGameOnLoad", "0") || !SG_Call("QWayPoint", "()")) {
		return NULL;
	}
	PyObject *t = K(Script_SaveVar("GameplayTime")), *v = t ? K(Script_EvalIn("SaveGame", t)) : NULL, *now = v ? K(SG_Time()) : NULL;
	PyObject *gpt = now ? K(PyNumber_Add(v, now)) : NULL, *text = gpt ? K(Script_Repr(gpt)) : NULL;
	if (!text || !SG_SetSave("GameplayTime", text)) {
		FAIL();
	}
	/* "Save/Save" + `iSlot` + ".sav", the map without "Levels/" + " - " + hh:mm:ss */
	PyObject *file = K(Script_PlusS(Script_SPlus("Save/Save", K(Script_Repr(slot))), ".sav"));
	PyObject *map = file ? K(Script_SaveVar("Map")) : NULL, *name = map ? K(PySequence_GetSlice(map, 7, INT_MAX)) : NULL;
	PyObject *secs = name ? K(Script_Int(gpt)) : NULL;
	static const long div[3] = { 60 * 60, 60, 1 };
	PyObject *desc = secs ? K(Script_PlusS(Script_Ref(name), " - ")) : NULL;
	for (int i = 0; desc && i < 3; i++) {
		scriptRefs_t t = { .n = 0 };
		PyObject *d = Script_Keep(&t, PyInt_FromLong(div[i])), *sixty = Script_Keep(&t, PyInt_FromLong(60));
		PyObject *q = d ? Script_Keep(&t, PyNumber_Divide(secs, d)) : NULL;
		PyObject *m = q && sixty ? Script_Keep(&t, PyNumber_Remainder(q, sixty)) : NULL;
		PyObject *rep = m ? Script_Keep(&t, Script_Repr(m)) : NULL;
		PyObject *z = rep ? Script_Keep(&t, Script_Ask("string", "zfill", "(Oi)", rep, 2)) : NULL;
		desc = z ? K(Script_Plus(i ? Script_PlusS(Script_Ref(desc), ":") : Script_Ref(desc), z)) : NULL;
		Script_Done(&t, NULL);
	}
	PyObject *success = desc ? K(Script_Ask("Scrap", "SaveGameVars", "(OO)", file, desc)) : NULL;
	PyObject *now2 = success ? K(SG_Time()) : NULL, *back = now2 ? K(PyNumber_Subtract(gpt, now2)) : NULL;
	PyObject *backText = back ? K(Script_Repr(back)) : NULL;
	if (!backText || !SG_SetSave("GameplayTime", backText)) {
		FAIL();
	}
	int ok = Script_True(success), msg = ok;
	if (!ok) {
		msg = show ? Script_True(show) : 1;
		if (msg > 0) {
			msg = 2;
		}
	}
	if (ok < 0 || msg < 0) {
		FAIL();
	}
	if (msg) {
		/* the confirmation or the failure of the save */
		PyObject *cad = K(Script_Ask("Scrap", "GetLangStr", "(s)", ok ? "SaveGameConfirmation" : "SaveGameFailure"));
		PyObject *len = cad ? K(Script_Ask("Speech", "CalculateSpeechTime", "(O)", cad)) : NULL;
		PyObject *when = len ? K(PyNumber_Add(K(SG_Time()), len)) : NULL;
		if (!when || !Script_Try("SScorer", "SetMsgText", "(OO)", cad, when)) {
			FAIL();
		}
	}
	int notAuto = Script_NumCmp(slot, 0);
	if (notAuto == -2 || (notAuto != 0 && !SG_Call("InstantSaveGame", "(iO)", 0, success))) {
		FAIL();
	}
	/* the debug save of the developers */
	PyObject *dbg = Script_Try("Scrap", "CreateSaveVar", "(ss)", "DebugSave", "0") ? K(Script_SaveVar("DebugSave")) : NULL;
	if (!dbg) {
		FAIL();
	}
	if (Script_Is(dbg, "1")) {
		scriptRefs_t t = { .n = 0 };
		PyObject *m = Script_Keep(&t, Script_SaveVar("Map"));
		PyObject *mm = m ? Script_Keep(&t, Script_SaveVar("Mission.Map")) : NULL;
		PyObject *lib = mm ? Script_Keep(&t, Script_SaveVar("Mission.Library")) : NULL;
		PyObject *nam = lib ? Script_Keep(&t, PySequence_GetSlice(m, 7, INT_MAX)) : NULL;
		nam = nam ? Script_Keep(&t, Script_Plus(Script_PlusS(Script_Ref(nam), " ("), mm)) : NULL;
		nam = nam ? Script_Keep(&t, Script_Plus(Script_PlusS(Script_Ref(nam), " - "), lib)) : NULL;
		PyObject *f = nam ? Script_Keep(&t, Script_PlusS(Script_PlusS(Script_SPlus("Save/", nam), ")"), ".sav")) : NULL;
		byte ok = f && Script_Try("Scrap", "SaveGameVars", "(OO)", f, nam);
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SaveGame(iSlot): on the PC the save now (the Xbox shows its message and saves in the next frame) */
static PyObject *SaveGame_SaveGame(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *slot;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &slot) || !Script_Import((const char *const[]){ "Init", NULL })) {
		return NULL;
	}
	PyObject *xbox = K(Script_Ask("Scrap", "Get", "(s)", "IsXbox"));
	int x = xbox ? Script_True(xbox) : -1;
	if (x < 0) {
		FAIL();
	}
	if (!x) {
		if (!SG_Call("InstantSaveGame", "(O)", slot)) {
			FAIL();
		}
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *path = K(Script_Ask("Scrap", "Get", "(s)", "XboxSavePath"));
	PyObject *prof = path ? K(Script_Ask("Scrap", "Get", "(s)", "XboxProfileName")) : NULL;
	PyObject *msgTime = prof ? K(Script_Global("Init", "XboxSavingMsgTime")) : NULL;
	if (!msgTime || !Script_Try("Scrap", "Set", "(sO)", "XboxSavingMsg", msgTime)) {
		FAIL();
	}
	if (Script_Is(path, "") || Script_Is(prof, "")) {
		if (!Script_Try("Scrap", "Set", "(ss)", "XboxSavingMsgStr", "XBox_Without_Msg_")) {
			FAIL();
		}
	} else if (!Script_Try("Scrap", "Set", "(ss)", "XboxSavingMsgStr", "XBox_Saving_Msg_") ||
		!Script_LaterIn(0, "SaveGame", "InstantSaveGame", Py_BuildValue("(O)", slot), NULL)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* LoadGame(iSlot): the save game back and its level (reloaded when the mission changed) */
static PyObject *SaveGame_LoadGame(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *slot;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &slot) || !Script_Import((const char *const[]){ "Init", NULL })) {
		return NULL;
	}
	PyObject *map0 = K(Script_SaveVar("Mission.Map")), *lib0 = map0 ? K(Script_SaveVar("Mission.Library")) : NULL;
	PyObject *file = lib0 ? K(Script_PlusS(Script_SPlus("Save/Save", K(Script_Repr(slot))), ".sav")) : NULL;
	PyObject *loaded = file ? K(Script_Ask("Scrap", "LoadGameVars", "(O)", file)) : NULL;
	int ok = loaded ? Script_True(loaded) : -1;
	if (ok < 0) {
		FAIL();
	}
	if (ok) {
		PyObject *map1 = K(Script_SaveVar("Mission.Map")), *lib1 = map1 ? K(Script_SaveVar("Mission.Library")) : NULL;
		int same = lib1 ? Script_Eq(map0, map1) : -1;
		if (same > 0) {
			same = Script_Eq(lib0, lib1);
		}
		if (same < 0 || (!same && !Script_Try("Scrap", "Set", "(si)", "ForceReload", 1))) {
			FAIL();
		}
		PyObject *map = K(Script_SaveVar("Map"));
		if (!map || !SG_Call("QLoad", "(Oi)", map, 0)) {
			FAIL();
		}
	} else {
		/* the script reaches Menu without importing it: a NameError unless something put it into the module */
		PyObject *menu = K(Script_Global("SaveGame", "Menu"));
		PyObject *msg = menu ? K(Script_Ask("Scrap", "GetLangStr", "(s)", "XBox_ErrorLoad_Msg")) : NULL;
		PyObject *box = msg ? K(Script_Method(menu, "ShowTextBox", "(iOss)", 0, msg, "Menu.MainMenu", "Menu.MainMenu")) : NULL;
		if (!box) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* the save variables of a new game: name, value (the demo has its own where both are given) */
typedef struct {
	const char *name, *value, *demo;
} sgVar_t;

static const sgVar_t sg_newVars[] = {
	{ "EnergyBarActive", "0", NULL },
	{ "SpecialActionActive", "0", NULL },
	{ "CrazyWing.Status", "0", NULL },
	{ "Conversor.ActiveConversors", "0", NULL },
	{ "Hangar.iHangarShip", "1", NULL },
	{ "Player.NumLives", "0", NULL },
};

/* the ship plans: the demo's, the full game's */
static const sgVar_t sg_demoPlans[] = {
	{ "Traffic.AcShips", "[\"SMerc2\", \"SMerc1\", \"SBanker1\"]", NULL },
	{ "Hangar.shipsToEditList", "[\"SPoli1\", \"SBetty1\", \"SBanker1\", \"SMerc2\"]", NULL },
	{ "Hangar.availableEnginesList", "[\"MPOLI1\", \"MBETTY1\", \"MBANKER1\", \"MMERC2\"]", NULL },
	{ "Hangar.HangarShip1", "SBanker1<-MBANKER1<-<-<-<-3,0,0,3,0,0,1<-65<-0,0,0,0,0,0", NULL },
	{ "Hangar.HangarShip2", "SMerc2<-MMERC2<-MMERC2<-MMERC2<-<-7,0,0,7,0,0,1<-80<-0,0,0,0,0,0", NULL },
	{ "Hangar.HangarShip3", "SPoli1<-MPOLI1<-<-<-<-1,0,0,1,0,0,1<-55<-0,0,0,0,0,0", NULL },
	{ "Hangar.HangarShip4", "SBetty1<-MBETTY1<-<-<-<-1,0,0,1,0,0,1<-60<-0,0,0,0,0,0", NULL },
	{ "Hangar.availableWeaponsList", "[\"Vulcan\",\"Devastator\"]", NULL },
};

static const sgVar_t sg_gamePlans[] = {
	{ "Hangar.shipsToEditList", "[\"SPoli1\"]", NULL },
	{ "Hangar.availableEnginesList", "[\"MPOLI1\"]", NULL },
	{ "Hangar.availableWeaponsList", "[\"Vulcan\"]", NULL },
};

/* after the plans: the parkings, the blueprints of Police and Press, who is in the Gamblin' Den */
static const sgVar_t sg_newVars2[] = {
	{ "Hangar.availableUpgradesList", "[]", NULL },
	{ "JackInTheBox.Active", "0", NULL },
	{ "Parked.Police", "['SPOLI1', 'SPOLI2']", NULL },
	{ "Parked.Mercs", "['SPOLI1']", NULL },
	{ "Parked.TownHall", "['SPOLI1', 'SPOLI2']", NULL },
	{ "Parked.Bank", "['SPOLI2']", NULL },
	{ "Parked.Press", "['SPOLI1']", NULL },
	{ "Parked.Temple", "['SPOLI2']", NULL },
	{ "PoliceBlueprints.Ships", "[]", NULL },
	{ "PoliceBlueprints.Engines", "[]", NULL },
	{ "PressBlueprints.Ships", "[]", NULL },
	{ "PressBlueprints.Engines", "[]", NULL },
	{ "MayorAtGambinDen", "1", "0" },
	{ "PolicesAtGambinDen", "0", "1" },
	{ "PoliceBossAtPolice", "1", "0" },
	{ "CrazyGamblerAtGambinDen", "1", NULL },
	{ "FunctionaryTwinsAtGambinDen", "0", NULL },
	{ "BankersAtGambinDen", "1", NULL },
	{ "BishopsAtGambinDen", "1", NULL },
};

static byte SG_CreateVars(const sgVar_t *v, int n, int demo) {
	for (int i = 0; i < n; i++) {
		if (!Script_Try("Scrap", "CreateSaveVar", "(ss)", v[i].name, demo && v[i].demo ? v[i].demo : v[i].value)) {
			return 0;
		}
	}
	return 1;
}

#define SG_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* NewGame(): a new single player game (the demo starts in the Gamblin' Den), its level */
static PyObject *SaveGame_NewGame(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Init", NULL }) || !Script_Try("Scrap", "DelSaveVars", "(s)", "") ||
		!SG_SetSaveS("AutoSaveGameOnLoad", "0") || !Script_Try("Scrap", "CreateSaveVar", "(ss)", "GameAct", "Prologue") ||
		!Script_Try("Scrap", "CreateSaveVar", "(ss)", "BankDebt", "0")) {
		return NULL;
	}
	PyObject *isDemo = K(Script_Global("Init", "isDemo"));
	int demo = isDemo ? Script_True(isDemo) : -1;
	if (demo < 0) {
		FAIL();
	}
	if (demo) {
		if (!Script_Import((const char *const[]){ "MissionsFuncs", NULL }) ||
			!Script_Try("MissionsFuncs", "SetFirstDemoMission", "()") ||
			!SG_Call("MMDesc", "(si)", "Mission_FirstMeetCrazyGambler_MissionText1", 1) ||
			!Script_Try("Scrap", "CreateSaveVar", "(ss)", "Map", "Levels/GamblinDen")) {
			FAIL();
		}
	} else if (!Script_Try("Scrap", "CreateSaveVar", "(ss)", "Map", "Levels/Orbit") ||
		!Script_Try("Scrap", "CreateSaveVar", "(ss)", "Mission.Library", "MapDecont")) {
		FAIL();
	}
	if (!SG_CreateVars(sg_newVars, SG_COUNT(sg_newVars), demo) ||
		!(demo ? SG_CreateVars(sg_demoPlans, SG_COUNT(sg_demoPlans), demo) : SG_CreateVars(sg_gamePlans, SG_COUNT(sg_gamePlans), demo)) ||
		!SG_CreateVars(sg_newVars2, SG_COUNT(sg_newVars2), demo)) {
		FAIL();
	}
	if (!demo && (!Script_Try("Scrap", "Set", "(ss)", "PlayVideo", "Video/Intro.avi") ||
			!Script_Try("Scrap", "Achievement", "(i)", 1))) {
		FAIL();
	}
	PyObject *map = K(Script_SaveVar("Map"));
	if (!map || !SG_Call("QLoad", "(Oi)", map, 0)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* CheckAllBlueprints(): the achievement for all 38 plans of ships and engines */
static PyObject *SaveGame_CheckAllBlueprints(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *a = K(Script_SaveVar("Hangar.shipsToEditList")), *la = a ? K(Script_EvalIn("SaveGame", a)) : NULL;
	PyObject *b = la ? K(Script_SaveVar("Hangar.availableEnginesList")) : NULL, *lb = b ? K(Script_EvalIn("SaveGame", b)) : NULL;
	int na = lb ? PyObject_Length(la) : -1, nb = na >= 0 ? PyObject_Length(lb) : -1;
	if (nb < 0) {
		FAIL();
	}
	if (na + nb == 38 && !Script_Try("Scrap", "Achievement", "(i)", 13)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ---- the save registers of the missions ---- */

/* SaveReg(name, mreg): the attributes of a class or object as save variables name.<attr>, their list as name */
static PyObject *SaveGame_SaveReg(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *reg;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &reg)) {
		return NULL;
	}
	if (Script_Is(name, "MissionStatus") && !SG_Call("QWayPoint", "()")) {
		return NULL;
	}
	PyObject *names = Script_Try("Scrap", "DelSaveVars", "(O)", name) ? K(PyList_New(0)) : NULL;
	PyObject *attrs = names ? K(Script_Ask("__builtin__", "dir", "(O)", reg)) : NULL;
	if (!attrs) {
		FAIL();
	}
	PyObject *a;
	int st;
	for (int i = 0; (st = Script_Next(attrs, i, &a)) > 0; i++) {
		/* the references of one turn */
		scriptRefs_t t = { .n = 0 };
		Script_Keep(&t, a);
		if (Script_In(a, (const char *const[]){ "__doc__", "__module__", NULL })) {
			Script_Done(&t, NULL);
			continue;
		}
		PyObject *key = Script_Keep(&t, Script_Plus(Script_PlusS(Script_Ref(name), "."), a));
		PyObject *v = key ? Script_Keep(&t, PyObject_GetAttr(reg, a)) : NULL;
		PyObject *text = v ? Script_Keep(&t, Script_Repr(v)) : NULL;
		byte ok = text && Script_Try("Scrap", "SetSaveVar", "(OO)", key, text) && PyList_Append(names, a) == 0;
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	PyObject *list = st == 0 ? K(Script_Repr(names)) : NULL;
	if (!list || !Script_Try("Scrap", "SetSaveVar", "(OO)", name, list)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* QSaveReg(mreg): the register of the mission status */
static PyObject *SaveGame_QSaveReg(PyObject *self, PyObject *args) {
	PyObject *reg;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &reg)) {
		return NULL;
	}
	return SG_Call("SaveReg", "(sO)", "MissionStatus", reg) ? Py_ReturnNone() : NULL;
}

/* GetReg(name): a new class whose attributes are the values of a register (exec "res.<attr> = <saved repr>") */
static PyObject *SaveGame_GetReg(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *mod = K(PyImport_ImportModule("SaveGame")), *g = mod ? PyModule_GetDict(mod) : NULL;
	PyObject *cdict = g ? K(Py_BuildValue("{s:s,s:O}", "__module__", "SaveGame", "__doc__", Py_None)) : NULL;
	PyObject *bases = cdict ? K(PyTuple_New(0)) : NULL, *cname = bases ? K(PyString_FromString("res")) : NULL;
	PyObject *res = cname ? K(PyClass_New(bases, cdict, cname)) : NULL;
	PyObject *lst = res ? K(Script_Ask("Scrap", "GetSaveVar", "(O)", name)) : NULL;
	if (!lst) {
		FAIL();
	}
	if (lst == Py_None) {
		lst = K(PyString_FromString("[]"));
	}
	PyObject *regs = K(Script_EvalIn("SaveGame", lst)), *locals = regs ? K(Py_BuildValue("{s:O,s:O,s:O}", "name", name, "res", res, "lst", lst)) : NULL;
	if (!locals) {
		FAIL();
	}
	PyObject *a;
	int st;
	for (int i = 0; (st = Script_Next(regs, i, &a)) > 0; i++) {
		/* the references of one turn */
		scriptRefs_t t = { .n = 0 };
		Script_Keep(&t, a);
		PyObject *key = Script_Keep(&t, Script_Plus(Script_PlusS(Script_Ref(name), "."), a));
		PyObject *val = key ? Script_Keep(&t, Script_Ask("Scrap", "GetSaveVar", "(O)", key)) : NULL;
		PyObject *code = val ? Script_Keep(&t, Script_Plus(Script_PlusS(Script_SPlus("res.", a), " = "), val)) : NULL;
		/* res.<reg> = <a literal> made directly */
		byte plain = code && PyString_Check(code) && PyString_Check(a) &&
			Script_IsName(PyString_AsString(a), PyString_Size(a)) && PyString_Check(val) &&
			(int)strlen(PyString_AsString(val)) == PyString_Size(val);
		PyObject *lit = plain ? Script_Keep(&t, Script_Literal(g, PyString_AsString(val))) : NULL;
		PyObject *done = lit ? (PyObject_SetAttrString(res, PyString_AsString(a), lit) == 0 ? Py_None : NULL) :
			code && PyString_Check(code) && PyDict_SetItemString(locals, "reg", a) == 0 ?
			Script_Keep(&t, PyRun_String(PyString_AsString(code), Py_file_input, g, locals)) : NULL;
		Script_Done(&t, NULL);
		if (!done) {
			FAIL();
		}
	}
	if (st < 0) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(res));
}

PyMethodDef g_SaveGameMethods[] = {
	{ "MMDesc", SaveGame_MMDesc, 1, NULL },
	{ "SMDesc", SaveGame_SMDesc, 1, NULL },
	{ "SaveGame", SaveGame_SaveGame, 1, NULL },
	{ "InstantSaveGame", (PyCFunction)(void (*)(void))SaveGame_InstantSaveGame, 3, NULL },
	{ "LoadGame", SaveGame_LoadGame, 1, NULL },
	{ "NewGame", SaveGame_NewGame, 1, NULL },
	{ "CheckAllBlueprints", SaveGame_CheckAllBlueprints, 1, NULL },
	{ "MarkWayPoint", (PyCFunction)(void (*)(void))SaveGame_MarkWayPoint, 3, NULL },
	{ "QWayPoint", SaveGame_QWayPoint, 1, NULL },
	{ "SaveAlarm", SaveGame_SaveAlarm, 1, NULL },
	{ "LoadAlarm", SaveGame_LoadAlarm, 1, NULL },
	{ "SaveMoney", SaveGame_SaveMoney, 1, NULL },
	{ "LoadMoney", SaveGame_LoadMoney, 1, NULL },
	{ "QLoad", (PyCFunction)(void (*)(void))SaveGame_QLoad, 3, NULL },
	{ "StartGame", (PyCFunction)(void (*)(void))SaveGame_StartGame, 3, NULL },
	{ "Restart", SaveGame_Restart, 1, NULL },
	{ "QSaveReg", SaveGame_QSaveReg, 1, NULL },
	{ "SaveReg", SaveGame_SaveReg, 1, NULL },
	{ "GetReg", SaveGame_GetReg, 1, NULL },
	{ "InitVars", SaveGame_InitVars, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* the script calls its own functions through the module globals */
static byte SG_Call(const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *a = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = Script_Global("SaveGame", func);
	PyObject *res = Script_Invoke(f, a);
	Py_XDECREF(f);
	Py_XDECREF(res);
	return res != NULL;
}
