/*
 * The SaveGame module in C (scripts/savegame.py, issue #2): saving and loading the single player game, the way points
 * (map, spawn point, character, batteries, alarm, money), the start of a new game with its save variables, the level
 * change (QLoad) and the save registers of the missions (SaveReg, GetReg). The module code (the save variables it
 * creates, the photo message) stays in the .mod file. Every failing line stops the function like the script.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values. The save variables, the
 * engine variables, the money, the time, the language texts, the files, the alarm and the save game files are C; the
 * save texts are made with the 1.5.2 repr of the native layer and read with its literal reader, so
 * every text the game writes stays byte for byte. Scrap.UsrEntity, GetAlarm, SetAlarm, SetMoney, Set, DelSaveVars,
 * ConsoleError, AddScheduledFunc, SNet.IsServer, IsClient, SInput.GetActionSet, SetActionSet, SSound.StopVoice,
 * StopAllSounds and SScorer.SetMsgText go through their seams (the cases record or swap them);
 * Scrap.MusicPlayer, LoadLevel and Achievement go through their refs (stage 7: the cases record them, the seam calls
 * what they put there), and so do SNet.CloseServer and CloseClient (the online games run through them).
 * The other ports (Scorer, Challenge, Speech, MissionsFuncs) and the functions of the module the script calls through
 * its globals are called through their refs (PORT_CALL, PORT_OWN: the port seam, OBJLIB 5); string.zfill has a twin in
 * the file. The function given to the scheduler by name takes its entry (Port_Scheduled); the Menu object a script may
 * put into the module is a global (OBJLIB 5) whose ShowTextBox is called through its entry (PORT_VALUE). The registers
 * keep their Python objects: SaveReg reads the attributes of any object and GetReg makes a class whose attributes are
 * read with the Python literal reader (OBJLIB 6).
 */
#include <limits.h>
#include <string.h>

#include "scrap/alarm.h"
#include "scrap/filepak.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define SG "SaveGame"

/* ---- engine calls of the module ---- */

/* Scrap.DelSaveVars(prefix) of a value through its ref (the seam: fr7b.py records it) */
static byte SG_DelSaveVarsV(const nativeVal_t *prefix) {
	return PORT_CALL(Scrap, DelSaveVars, NULL, "v", prefix);
}

/* Scrap.GetAlarm() through its ref (fc-pk-dt and the edge files swap it): (on, level, value) */
static byte SG_Alarm(nativeVal_t *out) {
	return PORT_CALL(Scrap, GetAlarm, out, "");
}

/* Scrap.FileExist(name): 1 or 0, -1 with the TypeError of "s" */
static int SG_FileExist(const nativeVal_t *name) {
	const char *s;
	cFilePack pack;
	if (!Native_ArgStr(name, 1, &s)) {
		return -1;
	}
	cFilePack_ctor(&pack);
	int r = cFilePack_Exists(&pack.file.base, s) != 0;
	cFilePack_dtor(&pack);
	return r;
}

/* Scrap.SaveGameVars(name, title): 1 or 0 into *out as the engine gives it (the TypeError of "s|s") */
static byte SG_SaveGameVars(const nativeVal_t *name, const nativeVal_t *title, nativeVal_t *out) {
	const char *n, *t;
	if (!Native_ArgStr(name, 1, &n) || !Native_ArgStr(title, 2, &t)) {
		return 0;
	}
	Native_SetInt(out, SaveGame_Save(n, t) != 0);
	return 1;
}

/* Scrap.GameplayTime + Scrap.GetTime() of the eval'd save variable into *out (the script's eval in the module) */
static byte SG_PlayTime(const nativeVal_t *saved, nativeVal_t *out) {
	nativeVal_t v = { 0 }, now;
	Native_SetFloat(&now, Mode_Time());
	byte ok = Mode_EvalSave(SG, saved, &v) && Mode_Arith('+', &v, &now, out);
	Native_Free(&v);
	return ok;
}

/* string.zfill(s, width) of a text (strings.py: the zeros after the sign) */
static void SG_Zfill(const nativeVal_t *s, int width, nativeVal_t *out) {
	char buf[NATIVE_TEXT];
	const char *p = Native_Text(s);
	int n = s->n;
	if (n >= width || width >= (int)sizeof(buf)) {
		Native_Copy(out, s);
		return;
	}
	int k = 0;
	if (*p == '-' || *p == '+') {
		buf[k++] = *p++;
		n--;
	}
	while (k + n < width) {
		buf[k++] = '0';
	}
	memcpy(buf + k, p, (size_t)n);
	Native_SetText(out, buf, k + n);
}

/* ---- the mission descriptions ---- */

/* MMDesc(Desc, Persist): the primary mission text, its scorer on or off, kept in the save game when Persist */
static byte SG_MMDesc(const nativeVal_t *desc, const nativeVal_t *persist) {
	if (!Mode_Import("Scorer") || !Native_GVarSet("PrimaryMissionDesc", desc)) {
		return 0;
	}
	long on = desc->type != NV_NONE ? Native_True(desc) : 0;
	if (!PORT_CALL(Scorer, ShowMissionScorer, NULL, "iii", 0L, 0L, on)) {
		return 0;
	}
	return !Native_True(persist) || Mode_SetSaveVar("PrimaryMissionDesc", desc);
}

/* SMDesc(Desc, Persist): the secondary mission text */
static byte SG_SMDesc(const nativeVal_t *desc, const nativeVal_t *persist) {
	if (!Mode_Import("Scorer") || !Native_GVarSet("SecondaryMissionDesc", desc)) {
		return 0;
	}
	return !Native_True(persist) || Mode_SetSaveVar("SecondaryMissionDesc", desc);
}

/* ---- the alarm and the money of a way point ---- */

/* SaveAlarm(): the state of the alarm into the save variables (once the level runs) */
static byte SG_SaveAlarm(void) {
	if (Mode_Time() <= 0.0) {
		return 1;
	}
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *on = Mode_V(&t), *b = Mode_V(&t), *level = Mode_V(&t), *text = Mode_V(&t);
	if (!SG_Alarm(a) || !Mode_Item(a, 0, on)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveS("AlarmActive", Native_True(on) ? "1" : "0");
	if (!SG_Alarm(b) || !Mode_Item(b, 1, level)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(level, text);
	return Mode_Done(&t, Mode_SetSaveVar("AlarmStatus", text));
}

/* SaveMoney() */
static byte SG_SaveMoney(void) {
	if (Mode_Time() > 0.0) {
		nativeVal_t m;
		Native_SetInt(&m, Mode_GetMoney());
		LvN_SaveRepr("Money", &m);
	}
	return 1;
}

/* print Scrap.GetLockAlarm(), Scrap.GetSaveVar("Char"), Scrap.GetAlarmChars() (the script's trace of LoadAlarm) */
static byte SG_PrintAlarm(void) {
	nativeVal_t lock, ch, chars;
	Native_SetInt(&lock, Alarm_IsLocked(&g_pWorld->alarm) != 0);
	Mode_SaveVar("Char", &ch);
	Native_SetEmpty(&chars, NV_TUPLE);
	Native_SetText(Native_Append(&chars), g_pWorld->alarm.chars, (int)strlen(g_pWorld->alarm.chars));
	Native_SetText(Native_Append(&chars), g_pWorld->alarm.lastChars, (int)strlen(g_pWorld->alarm.lastChars));
	const nativeVal_t *items[3] = { &lock, &ch, &chars };
	byte ok = Mode_Print(items, 3);
	Native_Free(&ch);
	Native_Free(&chars);
	return ok;
}

/* LoadAlarm(): the alarm of the save game back (always on while the boss controls the robots) */
static byte SG_LoadAlarm(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *set = Mode_V(&t), *status = Mode_V(&t), *level = Mode_V(&t), *boss = Mode_V(&t), *active = Mode_V(&t);
	nativeVal_t *ch = Mode_V(&t), *one = Mode_V(&t);
	const char *s;
	if (!SG_PrintAlarm() || !Mode_ActionSetGet(set) || !Mode_ActionSet("Walk")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("AlarmStatus", status);
	if (!Mode_EvalSave(SG, status, level) || !Mode_SetAlarm(level)) {
		return Mode_Done(&t, 0);
	}
	Alarm_SetGrow(&g_pWorld->alarm, -1);
	Mode_SaveVar("RobotsControlledByBoss", boss);
	Mode_SaveVar("AlarmActive", active);
	byte bossRobots = Native_IsStr(boss, "1");
	Native_SetInt(one, Native_IsStr(active, "1") || bossRobots);
	if (!Mode_SetAlarm(one) || (one->i && bossRobots && !Mode_GVarSetI("AlarmFallDelta", 0))) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Char", ch);
	if (!Native_ArgStr(ch, 1, &s)) {
		return Mode_Done(&t, 0);
	}
	Alarm_SetChar(&g_pWorld->alarm, s);
	return Mode_Done(&t, Mode_ActionSetV(set) && SG_PrintAlarm());
}

/* LoadMoney() */
static byte SG_LoadMoney(void) {
	nativeVal_t m, v = { 0 };
	Mode_SaveVar("Money", &m);
	byte ok = Mode_EvalSave(SG, &m, &v) && Mode_SetMoneySeam(&v);
	Native_Free(&m);
	Native_Free(&v);
	return ok;
}

/* InitVars(): the alarm and the money at the start of a single player level (through the globals of the module) */
static byte SG_InitVars(void) {
	return PORT_OWN(SaveGame, LoadAlarm, NULL, "") && PORT_OWN(SaveGame, LoadMoney, NULL, "");
}

/* ---- way points ---- */

/* MarkWayPoint(Map, Spawn, ExtraComeFrom=""): where the game goes on, the character and its batteries, the alarm and
 * the money */
static byte SG_MarkWayPoint(const nativeVal_t *map, const nativeVal_t *spawn, const nativeVal_t *from) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *type = Mode_V(&t), *bar = Mode_V(&t), *energy = Mode_V(&t), *text = Mode_V(&t);
	if (!Mode_SetSaveVar("Map", map) || !Mode_SetSaveVar("Spawn", spawn) || !Mode_SetSaveVar("ComeFrom", from) ||
		!Mode_UsrHandle(e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		int walk = Mode_VIsType(e, "WalkChar");
		if (walk < 0) {
			return Mode_Done(&t, 0);
		}
		if (walk) {
			if (!Mode_VObj(e, "ActCtrl", type) || !Mode_SetSaveVar("Char", type) || !Native_GVarGet("EnergyBarActive", bar)) {
				return Mode_Done(&t, 0);
			}
			if (Native_True(bar)) {
				if (!Mode_VObj(e, "Energy", energy)) {
					return Mode_Done(&t, 0);
				}
				Mode_Repr(energy, text);
				if (!Mode_SetSaveVar("AcBatteries", text)) {
					return Mode_Done(&t, 0);
				}
			}
		}
	}
	return Mode_Done(&t, PORT_OWN(SaveGame, SaveAlarm, NULL, "") && PORT_OWN(SaveGame, SaveMoney, NULL, ""));
}

/* QWayPoint(): a way point at the current map, spawn point and arrival */
static byte SG_QWayPoint(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *spawn = Mode_V(&t), *from = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_G("Init", "Path", path)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Spawn", spawn);
	Mode_SaveVar("ComeFrom", from);
	return Mode_Done(&t, PORT_OWN(SaveGame, MarkWayPoint, NULL, "vvv", path, spawn, from));
}

/* ---- the level change ---- */

/* QLoad(MapName, useGameplayTime=1): the level after this frame, the music and voices off, the gameplay time kept */
static byte SG_QLoad(const nativeVal_t *map, const nativeVal_t *useTime) {
	static const char *const credits[] = { "LEVELS/MENU", "LEVELS/ORBIT", "LEVELS/FAKE", "LEVELS/JUNKYARD", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *file = Mode_V(&t), *msg = Mode_V(&t), *saved = Mode_V(&t), *sum = Mode_V(&t), *text = Mode_V(&t);
	nativeVal_t *upper = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Cat(&t, file, 2, map, Mode_S(&t, "/Map/Map3D.SM3"))) {
		return Mode_Done(&t, 0);
	}
	int found = SG_FileExist(file);
	if (found == 0) {
		Native_Free(file);
		found = Mode_Cat(&t, file, 2, map, Mode_S(&t, "/Map/Map3D.M3D")) ? SG_FileExist(file) : -1;
	}
	if (found < 0) {
		return Mode_Done(&t, 0);
	}
	if (!found) {
		return Mode_Done(&t, Native_Arith('%', Mode_S(&t, "Map not found: %s\n"), map, msg) && Mode_ConsoleError(msg));
	}
	if (!PORT_CALL(Scrap, MusicPlayer, NULL, "ss", "", "stop")) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < 4; i++) {
		if (!Mode_StopVoice(Mode_I(&t, i))) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_StopAllSounds()) {
		return Mode_Done(&t, 0);
	}
	if (!useTime || Native_True(useTime)) {
		SaveVar_Create("GameplayTime", "0");
		Mode_SaveVar("GameplayTime", saved);
		if (!SG_PlayTime(saved, sum)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(sum, text);
		Mode_SaveS("GameplayTime", Native_Text(text));
	}
	if (!Mode_SetSaveVar("Map", map) || !Native_Upper(map, upper)) {
		return Mode_Done(&t, 0);
	}
	/* the maps with credits */
	long withCredits = 0;
	for (int i = 0; credits[i] && !withCredits; i++) {
		withCredits = Native_IsStr(upper, credits[i]);
	}
	if (!Mode_GVarSetI("R_CreditsRTarget", withCredits)) {
		return Mode_Done(&t, 0);
	}
	/* SNet.CloseServer and CloseClient through their refs (the online tests run through here) */
	int server = Mode_IsServer();
	if (server) {
		return Mode_Done(&t, server > 0 && PORT_CALL(SNet, CloseServer, NULL, "v", map));
	}
	int client = Mode_IsClient();
	if (client) {
		return Mode_Done(&t, client > 0 && PORT_CALL(SNet, CloseClient, NULL, "v", map));
	}
	return Mode_Done(&t, Mode_Import("Speech") && PORT_CALL(Speech, CallEndOfSpeech, NULL, "") &&
		PORT_CALL(Scrap, LoadLevel, NULL, "v", map));
}

/* StartGame(CleanChallenge=1, CleanRevenge=0): the map of the save game, the current one becomes the previous one,
 * the challenge (and the revenge) cleaned unless kept */
static byte SG_StartGame(const nativeVal_t *cleanChallenge, const nativeVal_t *cleanRevenge) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *map = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_Import("Challenge") || !Mode_G("Init", "Path", path) || !Mode_SetSaveVar("PrevMap", path)) {
		return Mode_Done(&t, 0);
	}
	if ((!cleanChallenge || Native_True(cleanChallenge)) && !PORT_CALL(Challenge, CleanChallenge, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	if (cleanRevenge && Native_True(cleanRevenge) && !PORT_CALL(Challenge, CleanRevenge, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Map", map);
	return Mode_Done(&t, PORT_OWN(SaveGame, QLoad, NULL, "v", map));
}

/* Restart(): the current level again */
static byte SG_Restart(void) {
	nativeVal_t path = { 0 };
	byte ok = Mode_Import("Init") && Mode_G("Init", "Path", &path) && PORT_OWN(SaveGame, QLoad, NULL, "v", &path);
	Native_Free(&path);
	return ok;
}

/* ---- saving and loading ---- */

/* "Save/Save" + `iSlot` + ".sav" into *out */
static byte SG_SlotFile(modeVals_t *t, const nativeVal_t *slot, nativeVal_t *out) {
	nativeVal_t *rep = Mode_V(t);
	Mode_Repr(slot, rep);
	return Mode_Cat(t, out, 3, Mode_S(t, "Save/Save"), rep, Mode_S(t, ".sav"));
}

/* the debug save of the developers: Save/<map> (<mission map> - <mission>).sav */
static byte SG_DebugSave(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = Mode_V(&t), *mm = Mode_V(&t), *lib = Mode_V(&t), *nam = Mode_V(&t), *nam2 = Mode_V(&t), *nam3 = Mode_V(&t);
	nativeVal_t *f = Mode_V(&t), *ok = Mode_V(&t);
	Mode_SaveVar("Map", m);
	Mode_SaveVar("Mission.Map", mm);
	Mode_SaveVar("Mission.Library", lib);
	return Mode_Done(&t, Native_Slice(m, 7, INT_MAX, nam) && Mode_Cat(&t, nam2, 3, nam, Mode_S(&t, " ("), mm) &&
		Mode_Cat(&t, nam3, 3, nam2, Mode_S(&t, " - "), lib) &&
		Mode_Cat(&t, f, 4, Mode_S(&t, "Save/"), nam3, Mode_S(&t, ")"), Mode_S(&t, ".sav")) && SG_SaveGameVars(f, nam3, ok));
}

/* InstantSaveGame(iSlot, showMsgOnFailure=1): the game into Save/Save<n>.sav with the map and the gameplay time as its
 * name, the message of the result, the autosave slot 0 too */
static byte SG_InstantSaveGame(const nativeVal_t *slot, const nativeVal_t *show) {
	static const long div[3] = { 60 * 60, 60, 1 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *saved = Mode_V(&t), *gpt = Mode_V(&t), *text = Mode_V(&t), *file = Mode_V(&t), *map = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *secs = Mode_V(&t), *desc = Mode_V(&t), *success = Mode_V(&t), *now = Mode_V(&t);
	nativeVal_t *back = Mode_V(&t), *cad = Mode_V(&t), *len = Mode_V(&t), *when = Mode_V(&t), *dbg = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Speech") || !Mode_Import("Init")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveS("AutoSaveGameOnLoad", "0");
	if (!PORT_OWN(SaveGame, QWayPoint, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("GameplayTime", saved);
	if (!SG_PlayTime(saved, gpt)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(gpt, text);
	Mode_SaveS("GameplayTime", Native_Text(text));
	if (!SG_SlotFile(&t, slot, file)) {
		return Mode_Done(&t, 0);
	}
	/* the map without "Levels/" + " - " + hh:mm:ss */
	Mode_SaveVar("Map", map);
	if (!Native_Slice(map, 7, INT_MAX, name) || !Native_ToInt(gpt, secs) || !Mode_Cat(&t, desc, 2, name, Mode_S(&t, " - "))) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		int mark = t.n;
		nativeVal_t *q = Mode_V(&t), *m = Mode_V(&t), *rep = Mode_V(&t), *z = Mode_V(&t), *d = Mode_V(&t);
		nativeVal_t *k = Mode_V(&t);
		Native_SetInt(d, div[i]);
		if (!Mode_Arith('/', secs, d, q) || !Mode_Arith('%', q, Mode_I(&t, 60), m)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(m, rep);
		SG_Zfill(rep, 2, z);
		if (!(i ? Mode_Cat(&t, k, 3, desc, Mode_S(&t, ":"), z) : Mode_Cat(&t, k, 2, desc, z))) {
			return Mode_Done(&t, 0);
		}
		Native_Free(desc);
		*desc = *k;
		Native_SetNone(k);
		Mode_Release(&t, mark);
	}
	if (!SG_SaveGameVars(file, desc, success)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	if (!Mode_Arith('-', gpt, now, back)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(text);
	Mode_Repr(back, text);
	Mode_SaveS("GameplayTime", Native_Text(text));
	byte ok = Native_True(success), msg = ok || !show || Native_True(show);
	if (msg) {
		/* the confirmation or the failure of the save */
		Mode_Lang(ok ? "SaveGameConfirmation" : "SaveGameFailure", cad);
		if (!PORT_CALL(Speech, CalculateSpeechTime, len, "v", cad)) {
			return Mode_Done(&t, 0);
		}
		Native_SetFloat(now, Mode_Time());
		if (!Mode_Arith('+', now, len, when) || !Mode_MsgTextV(cad, when)) {
			return Mode_Done(&t, 0);
		}
	}
	int notAuto = Mode_CmpI(slot, "!=", 0);
	if (notAuto < 0 || (notAuto && !PORT_OWN(SaveGame, InstantSaveGame, NULL, "iv", 0L, success))) {
		return Mode_Done(&t, 0);
	}
	SaveVar_Create("DebugSave", "0");
	Mode_SaveVar("DebugSave", dbg);
	return Mode_Done(&t, !Native_IsStr(dbg, "1") || SG_DebugSave());
}

/* SaveGame(iSlot): on the PC the save now (the Xbox shows its message and saves in the next frame) */
static byte SG_SaveGame(const nativeVal_t *slot) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *xbox = Mode_V(&t), *path = Mode_V(&t), *prof = Mode_V(&t), *msgTime = Mode_V(&t);
	if (!Mode_Import("Init") || !Native_GVarGet("IsXbox", xbox)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(xbox)) {
		return Mode_Done(&t, PORT_OWN(SaveGame, InstantSaveGame, NULL, "v", slot));
	}
	if (!Native_GVarGet("XboxSavePath", path) || !Native_GVarGet("XboxProfileName", prof) ||
		!Mode_G("Init", "XboxSavingMsgTime", msgTime) || !Native_GVarSet("XboxSavingMsg", msgTime)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(path, "") || Native_IsStr(prof, "")) {
		return Mode_Done(&t, Mode_GVarSetS("XboxSavingMsgStr", "XBox_Without_Msg_"));
	}
	/* the global InstantSaveGame by name: the scheduler takes its entry */
	return Mode_Done(&t, Mode_GVarSetS("XboxSavingMsgStr", "XBox_Saving_Msg_") &&
		Mode_LaterIn(0.0, SG, "InstantSaveGame", slot, 1, NULL));
}

/* LoadGame(iSlot): the save game back and its level (reloaded when the mission changed) */
static byte SG_LoadGame(const nativeVal_t *slot) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *map0 = Mode_V(&t), *lib0 = Mode_V(&t), *file = Mode_V(&t), *map1 = Mode_V(&t), *lib1 = Mode_V(&t);
	nativeVal_t *map = Mode_V(&t), *msg = Mode_V(&t);
	const char *s;
	if (!Mode_Import("Init")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Mission.Map", map0);
	Mode_SaveVar("Mission.Library", lib0);
	if (!SG_SlotFile(&t, slot, file) || !Native_ArgStr(file, 1, &s)) {
		return Mode_Done(&t, 0);
	}
	if (SaveGame_Load(s)) {
		Mode_SaveVar("Mission.Map", map1);
		Mode_SaveVar("Mission.Library", lib1);
		int same = Mode_Cmp(map0, "==", map1);
		if (same > 0) {
			same = Mode_Cmp(lib0, "==", lib1);
		}
		if (same < 0 || (!same && !Mode_GVarSetI("ForceReload", 1))) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveVar("Map", map);
		return Mode_Done(&t, PORT_OWN(SaveGame, QLoad, NULL, "vi", map, 0L));
	}
	/* the script reaches Menu without importing it: a NameError unless something put it into the module (OBJLIB 5:
	 * the global; the function of the object through its entry) */
	nativeVal_t *menu = Mode_V(&t), *box = Mode_V(&t);
	if (!Mode_RefG(SG, "Menu", menu)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("XBox_ErrorLoad_Msg", msg);
	byte ok = Mode_RefGetAttr(menu, "ShowTextBox", box);
	if (ok) {
		const nativeVal_t args[4] = { *Mode_I(&t, 0), *msg, *Mode_S(&t, "Menu.MainMenu"), *Mode_S(&t, "Menu.MainMenu") };
		ok = Mode_CallArgs(box, NULL, args, 4);
	}
	return Mode_Done(&t, ok);
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

/* Scrap.CreateSaveVar of each variable of the table */
static void SG_CreateVars(const sgVar_t *v, int n, byte demo) {
	for (int i = 0; i < n; i++) {
		SaveVar_Create(v[i].name, demo && v[i].demo ? v[i].demo : v[i].value);
	}
}

#define SG_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* NewGame(): a new single player game (the demo starts in the Gamblin' Den), its level */
static byte SG_NewGame(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *isDemo = Mode_V(&t), *map = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_DelSaveVars("")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveS("AutoSaveGameOnLoad", "0");
	SaveVar_Create("GameAct", "Prologue");
	SaveVar_Create("BankDebt", "0");
	if (!Mode_G("Init", "isDemo", isDemo)) {
		return Mode_Done(&t, 0);
	}
	byte demo = Native_True(isDemo);
	if (demo) {
		if (!Mode_Import("MissionsFuncs") || !PORT_CALL(MissionsFuncs, SetFirstDemoMission, NULL, "") ||
			!PORT_OWN(SaveGame, MMDesc, NULL, "si", "Mission_FirstMeetCrazyGambler_MissionText1", 1L)) {
			return Mode_Done(&t, 0);
		}
		SaveVar_Create("Map", "Levels/GamblinDen");
	} else {
		SaveVar_Create("Map", "Levels/Orbit");
		SaveVar_Create("Mission.Library", "MapDecont");
	}
	SG_CreateVars(sg_newVars, SG_COUNT(sg_newVars), demo);
	if (demo) {
		SG_CreateVars(sg_demoPlans, SG_COUNT(sg_demoPlans), demo);
	} else {
		SG_CreateVars(sg_gamePlans, SG_COUNT(sg_gamePlans), demo);
	}
	SG_CreateVars(sg_newVars2, SG_COUNT(sg_newVars2), demo);
	if (!demo && (!Mode_GVarSetS("PlayVideo", "Video/Intro.avi") || !PORT_CALL(Scrap, Achievement, NULL, "i", 1L))) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Map", map);
	return Mode_Done(&t, PORT_OWN(SaveGame, QLoad, NULL, "vi", map, 0L));
}

/* CheckAllBlueprints(): the achievement for all 38 plans of ships and engines */
static byte SG_CheckAllBlueprints(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *la = Mode_V(&t), *b = Mode_V(&t), *lb = Mode_V(&t);
	Mode_SaveVar("Hangar.shipsToEditList", a);
	if (!Mode_EvalSave(SG, a, la)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Hangar.availableEnginesList", b);
	if (!Mode_EvalSave(SG, b, lb)) {
		return Mode_Done(&t, 0);
	}
	int na = Native_Size(la), nb = na >= 0 ? Native_Size(lb) : -1;
	if (nb < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, na + nb != 38 || PORT_CALL(Scrap, Achievement, NULL, "i", 13L));
}

/* ---- the save registers of the missions ---- */

/* SaveReg(name, mreg): the attributes of a class or object as save variables name.<attr>, their list as name */
static byte SG_SaveReg(const nativeVal_t *name, const nativeVal_t *reg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *names = Mode_V(&t), *attrs = Mode_V(&t), *list = Mode_V(&t);
	if (Native_IsStr(name, "MissionStatus") && !PORT_OWN(SaveGame, QWayPoint, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	if (!SG_DelSaveVarsV(name)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(names, NV_LIST);
	/* dir() of any object (OBJLIB 7: the builtin by name) */
	if (!Native_CallMod("__builtin__", "dir", attrs, "v", reg)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(attrs);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *a = Mode_V(&t), *key = Mode_V(&t), *v = Mode_V(&t), *text = Mode_V(&t);
		if (!Native_SeqItem(attrs, i, a)) {
			return Mode_Done(&t, 0);
		}
		if (Native_IsStr(a, "__doc__") || Native_IsStr(a, "__module__")) {
			Mode_Release(&t, mark);
			continue;
		}
		if (!Mode_Cat(&t, key, 3, name, Mode_S(&t, "."), a)) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_VObj(reg, Native_Text(a), v)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(v, text);
		if (!Mode_SetSaveV(key, text)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(Native_Append(names), a);
		Mode_Release(&t, mark);
	}
	Mode_Repr(names, list);
	return Mode_Done(&t, Mode_SetSaveV(name, list));
}

/* QSaveReg(mreg): the register of the mission status */
static byte SG_QSaveReg(const nativeVal_t *reg) {
	return PORT_OWN(SaveGame, SaveReg, NULL, "sv", "MissionStatus", reg);
}

/* GetReg(name): a new class whose attributes are the values of a register (exec "res.<attr> = <saved repr>"). The
 * names and the saved literals are read with the native reader (Mode_EvalSave, Mode_EvalPy); the class and its
 * attributes stay Python objects until stage 7 (OBJLIB 7: the missions read them as attributes of the class) */
static PyObject *SG_GetReg(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lst = Mode_V(&t), *regs = Mode_V(&t);
	PyObject *cdict = Py_BuildValue("{s:s,s:O}", "__module__", SG, "__doc__", Py_None);
	PyObject *bases = cdict ? PyTuple_New(0) : NULL, *cname = bases ? PyString_FromString("res") : NULL;
	PyObject *res = cname ? PyClass_New(bases, cdict, cname) : NULL;
	Py_XDECREF(cname);
	Py_XDECREF(bases);
	Py_XDECREF(cdict);
	byte ok = res && Mode_SaveVarV(name, lst);
	if (ok && lst->type == NV_NONE) {
		Native_SetStr(lst, "[]");
	}
	int n = ok && Mode_EvalSave(SG, lst, regs) ? Mode_Loop(regs) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *a = Mode_V(&t), *key = Mode_V(&t), *val = Mode_V(&t), *code = Mode_V(&t);
		ok = Native_SeqItem(regs, i, a) && Mode_Cat(&t, key, 3, name, Mode_S(&t, "."), a) && Mode_SaveVarV(key, val) &&
			Mode_Cat(&t, code, 4, Mode_S(&t, "res."), a, Mode_S(&t, " = "), val);
		if (ok) {
			/* the exec'd res.<reg> = <value> made directly: STORE_ATTR of the literal; another register name or value
			 * is exec's SyntaxError, nothing runs */
			PyObject *lit = NULL;
			if (a->type == NV_STR && Script_IsName(Native_Text(a), a->n) && val->type == NV_STR) {
				PyObject *v = Native_ToPy(val);
				lit = v ? Mode_EvalPy(SG, v) : NULL;
				Py_XDECREF(v);
			} else {
				Script_NotRead("SaveGame.GetReg", Native_Text(code));
			}
			ok = lit && PyObject_SetAttrString(res, (char *)Native_Text(a), lit) == 0;
			Py_XDECREF(lit);
		}
		Mode_Release(&t, mark);
	}
	Mode_Done(&t, 0);
	if (!ok) {
		Py_XDECREF(res);
		return NULL;
	}
	return res;
}

/* ---- adapters ---- */

MODE_FN(SaveGame, MMDesc, 2, SG_MMDesc(&a[0], &a[1]))
MODE_FN(SaveGame, SMDesc, 2, SG_SMDesc(&a[0], &a[1]))
/* the functions that take any arguments (the C port ignored them) */
MODE_FNANY(SaveGame, SaveAlarm, SG_SaveAlarm())
MODE_FNANY(SaveGame, SaveMoney, SG_SaveMoney())
MODE_FNANY(SaveGame, LoadAlarm, SG_LoadAlarm())
MODE_FNANY(SaveGame, LoadMoney, SG_LoadMoney())
MODE_FNANY(SaveGame, InitVars, SG_InitVars())
MODE_FNANY(SaveGame, QWayPoint, SG_QWayPoint())
MODE_FNANY(SaveGame, Restart, SG_Restart())
MODE_FNANY(SaveGame, NewGame, SG_NewGame())
MODE_FNANY(SaveGame, CheckAllBlueprints, SG_CheckAllBlueprints())
MODE_FN(SaveGame, SaveGame, 1, SG_SaveGame(&a[0]))
MODE_FN(SaveGame, LoadGame, 1, SG_LoadGame(&a[0]))
MODE_FN(SaveGame, SaveReg, 2, SG_SaveReg(&a[0], &a[1]))
MODE_FN(SaveGame, QSaveReg, 1, SG_QSaveReg(&a[0]))

/* GetReg gives the class by reference (OBJLIB 7) */
MODE_FN(SaveGame, GetReg, 1, Mode_RefTake(SG_GetReg(&a[0]), out))

/* the keyword functions (MODE_KW) */
MODE_KW(SaveGame, MarkWayPoint, "OO|O", "Map", "Spawn", "ExtraComeFrom") {
	Mode_KwDefS(v, given, 2, "");
	return SG_MarkWayPoint(&v[0], &v[1], &v[2]);
}

MODE_KW(SaveGame, QLoad, "O|O", "MapName", "useGameplayTime") {
	return SG_QLoad(&v[0], given[1] ? &v[1] : NULL);
}

MODE_KW(SaveGame, StartGame, "|OO", "CleanChallenge", "CleanRevenge") {
	return SG_StartGame(given[0] ? &v[0] : NULL, given[1] ? &v[1] : NULL);
}

MODE_KW(SaveGame, InstantSaveGame, "O|O", "iSlot", "showMsgOnFailure") {
	return SG_InstantSaveGame(&v[0], given[1] ? &v[1] : NULL);
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

PORTS(SaveGame)
