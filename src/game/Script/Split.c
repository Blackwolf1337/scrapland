/*
 * The split screen in C (the scripts split/split.py and the four modes sdeathmatch.py, sflaghunt.py, spostman.py and
 * srace.py, issue #2): two players on one screen, the ship choice before a round, the scores, the pause and end menus
 * (data/menus/split.def: the scorer items and the keys), and the modes (the flag to hunt or carry, the race routes of
 * data/split/srace.def). The scripts start a split game by setting SplitScreen and loading a level (init.py preloads
 * Split and the mode of ServerType).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values and call the engine in C. The state
 * stays in the module dicts and is read where the script reads the global (OBJLIB 5), because the tests, the modes and
 * the .def keys read and set it: the map tables, MortalProfiles, PlayersReady, Scorer, FirstTime, RespawnTime,
 * ModLib, OnDeathFunc and the spawn functions FuncGetSpawnPos/FuncGetReSpawnPos (the race replaces them) of Split;
 * ZoneList, AcFlag, BattonOwner, FlagInitPos, race and the model and sound names of the modes. The C works on them in
 * the struct split; the lists of Split are changed in place, as the script does. Flag, pl0 and pl1 are entity handles
 * and stay Python objects until entities are native values (OBJLIB 7). The functions call each other and the other
 * ports by name (OBJLIB 3); the .def sections take their $names as a dict (OBJLIB 6).
 */
#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static const char split_def[] = {
#embed "../../../data/menus/split.def"
	, 0 };

static const char srace_def[] = {
#embed "../../../data/split/srace.def"
	, 0 };

static const scriptStep_t split_steps[] = {
	{ NULL, NULL },
};

static struct {
	nativeVal_t scorer;		/* Split.Scorer: the points of both players */
	nativeVal_t mortalProfiles;	/* Split.MortalProfiles: the hangar profile each player looks at */
	nativeVal_t playersReady;	/* Split.PlayersReady */
	nativeVal_t zoneList;		/* ZoneList of the mode that runs: the flag positions */
	nativeVal_t acFlag;		/* AcFlag of the mode: the zone of the flag, or the checkpoint of the race */
	nativeVal_t race;		/* SRace.race: the checkpoints, (pos, angle) each */
	nativeVal_t raceBuild;		/* the checkpoints srace.def gives while it runs */
} split;

/* the functions of a module through its global or attribute, other ports by name (OBJLIB 3) */
#define OWN(name, out, ...) Native_CallGlobal("Split", name, out, __VA_ARGS__)
#define CALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* a section of split.def for player id with the $names of the n pairs (OBJLIB 6: a dict until the .def runner takes
 * C values): 0 with the error set when a line raised */
static byte Split_Run(int id, const char *section, int n, ...) {
	PyObject *vars = NULL;
	if (n) {
		va_list va;
		va_start(va, n);
		vars = PyDict_New();
		for (int i = 0; vars && i < n; i++) {
			const char *name = va_arg(va, const char *);
			PyObject *v = Native_ToPy(va_arg(va, const nativeVal_t *));
			if (!v || PyDict_SetItemString(vars, (char *)name, v) < 0) {
				Py_XDECREF(v);
				Py_DECREF(vars);
				vars = NULL;
				break;
			}
			Py_DECREF(v);
		}
		va_end(va);
		if (!vars) {
			return 0;
		}
	}
	byte ok = Script_RunFileTry(id, "menus/split.def", section, split_def, split_steps, vars);
	Py_XDECREF(vars);
	return ok;
}

/* "<text>" + `v` */
static byte Split_Name(modeVals_t *t, const char *text, const nativeVal_t *v, nativeVal_t *out) {
	nativeBuf_t b;
	nativeVal_t *rep = Mode_V(t);
	Native_BufInit(&b);
	Native_Repr(&b, v, NR_REPR);
	Native_SetText(rep, b.p, b.len);
	Native_BufFree(&b);
	return Mode_Cat(t, out, 2, Mode_S(t, text), rep);
}

/* Scrap.GetLangStr(key) % (button,), the text alone when button is NULL */
static byte Split_Lang(const char *key, const char *button, nativeVal_t *out) {
	nativeVal_t a;
	if (!button) {
		Mode_Lang(key, out);
		return 1;
	}
	Native_SetEmpty(&a, NV_TUPLE);
	Native_SetStr(Native_Append(&a), button);
	byte ok = Mode_LangFmt(key, &a, out);
	Native_Free(&a);
	return ok;
}

/* (0, sp[1], 0) */
static byte Split_Ang(modeVals_t *t, const nativeVal_t *sp, nativeVal_t *out) {
	nativeVal_t *a = Mode_V(t);
	if (!Native_Subscript(sp, Mode_I(t, 1), a)) {
		return 0;
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), 0);
	Native_Copy(Native_Append(out), a);
	Native_SetInt(Native_Append(out), 0);
	return 1;
}

/* list[idx] = list[idx] + d of a list global of Split (Split.Scorer, Split.MortalProfiles), in place */
static byte Split_Add(const char *name, nativeVal_t *slot, const nativeVal_t *idx, long d) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *n = Mode_V(&t);
	return Mode_Done(&t, Mode_Read("Split", name, slot) && Native_Subscript(slot, idx, v) && Native_Arith('+', v, Mode_I(&t, d), n) &&
		Mode_DictPut("Split", name, slot, idx, n));
}

/* the int of a player id as PyInt_AsLong reads it */
static byte Split_Id(const nativeVal_t *id, int *out) {
	return Native_ArgInt(id, out);
}

/* the keys of a player off (StartGame, EndOfGame bind them per player) */
static byte Split_KeysOff(int id) {
	return Split_Run(id, "keys_off", 0);
}

/* the menu texts of player id from row 70, 22 apart (textorl0..2), then the black band */
static byte Split_Texts(const nativeVal_t *id, nativeVal_t *const *texts, int n) {
	modeVals_t t = { .n = 0 };
	for (int i = 0; i < n; i++) {
		nativeVal_t *obj = Mode_V(&t);
		if (!Split_Name(&t, "textorl", Mode_I(&t, i), obj) || !OWN("LittleText", NULL, "vvvi", id, obj, texts[i], (long)(70 + i * 22))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, OWN("BlackBack", NULL, "v", id));
}

/* ---- Split ---- */

/* CreateUserShip(id): Player<id> at a spawn point with its ship, and the camera Cam<id> following it */
static byte Split_CreateUserShipC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *at = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *ang = Mode_V(&t), *cname = Mode_V(&t), *zero = Mode_V(&t), *pos = Mode_V(&t), *vn = Mode_V(&t), *cn = Mode_V(&t);
	PyObject *fn;
	cEntity *v, *cam;
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !(fn = Mode_GlobalPy("Split", "FuncGetSpawnPos"))) {
		return Mode_Done(&t, 0);
	}
	byte ok = Native_Call(fn, sp, "v", id);
	Py_DECREF(fn);
	if (!ok || !Native_Subscript(sp, Mode_I(&t, 0), at) || !Native_Subscript(at, Mode_I(&t, 0), x) ||
		!Native_Subscript(at, Mode_I(&t, 1), y) || !Native_Subscript(at, Mode_I(&t, 2), z) || !Split_Name(&t, "Player", id, name) ||
		!Mode_Create(name, x, y, z, "Car", &v)) {
		return Mode_Done(&t, 0);
	}
	/* the name the handle of the ship keeps */
	char n[0x80];
	Str_Copy(n, v ? v->name : Native_Text(name), sizeof(n));
	if (!Split_Ang(&t, sp, ang) || !Mode_Set(v, n, "Ang", ang) || !OWN("NextShip", NULL, "v", id) || !Split_Name(&t, "Cam", id, cname)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(zero, 0.0);
	if (!Mode_Create(cname, zero, zero, zero, "FCam", &cam) || !Native_Subscript(sp, Mode_I(&t, 0), pos) ||
		!Mode_Set(cam, Native_Text(cname), "camPos", pos) || !Split_Ang(&t, sp, ang) || !Mode_Set(cam, Native_Text(cname), "camAng", ang)) {
		return Mode_Done(&t, 0);
	}
	/* v.Name after NextShip: the handle finds its entity again */
	return Mode_Done(&t, Mode_NameGet(n, "Name", vn) && Native_EntVar(cam, Native_Text(cname), "Name", cn) &&
		CALL("Vehicles", "SetActiveCam", NULL, "vvv", vn, cn, id));
}

/* Init(): the mode's state, both players with ships, the deaths through OnDeathCallback, the ship choice */
static byte Split_InitC(void) {
	PyObject *mod = Mode_GlobalPy("Split", "ModLib");
	PyObject *v = mod ? Script_Method(mod, "InitStatus", "()") : NULL;
	Py_XDECREF(mod);
	Py_XDECREF(v);
	if (!v || !Mode_GVarSetS("MainMissionEvent", "") || !Mode_Import("Menu") || !Mode_Import("Vehicles") ||
		!OWN("CreateUserShip", NULL, "i", 0L) || !OWN("CreateUserShip", NULL, "i", 1L)) {
		return 0;
	}
	/* Vehicles.OnDeathCallback = OnDeathCallback (OBJLIB 3: the function as a value; a failed set is no error) */
	PyObject *cb = Mode_GlobalPy("Split", "OnDeathCallback");
	if (!cb) {
		return 0;
	}
	Script_SetVar("Vehicles", "OnDeathCallback", cb);
	return OWN("ChooseShips", NULL, "");
}

/* ReSpawnShip(LooserName): back at a spawn point (the race: where it died) without upgrades */
static byte Split_ReSpawnShipC(const nativeVal_t *name) {
	static const char *const upgrades[] = { "Upgrd00", "Upgrd01", "Upgrd02", "Upgrd03", "Upgrd04", "Upgrd05", "Upgrd06" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *ang = Mode_V(&t), *pos = Mode_V(&t);
	cEntity *e;
	PyObject *fn;
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !Mode_Ent(name, &e) || !(fn = Mode_GlobalPy("Split", "FuncGetReSpawnPos"))) {
		return Mode_Done(&t, 0);
	}
	/* the handle of the ship goes to the spawn function (OBJLIB 7) */
	const char *n = Native_Text(name);
	PyObject *h = Mode_Handle(e);
	byte ok = Native_Call(fn, sp, "O", h);
	Py_DECREF(h);
	Py_DECREF(fn);
	cEntity *at = e ? World_FindEntity(n) : NULL;
	if (!ok || !Split_Ang(&t, sp, ang) || !(e ? Native_EntSet(at, n, "Ang", ang) : Mode_Set(NULL, n, "Ang", ang)) ||
		!Native_Subscript(sp, Mode_I(&t, 0), pos) || !Native_EntSet(World_FindEntity(n), n, "Pos", pos)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 7; i++) {
		if (!Native_EntSet(World_FindEntity(n), n, upgrades[i], Mode_I(&t, 0))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, CALL("Vehicles", "ReSpawnShip", NULL, "v", name));
}

/* OnDeathCallback(LooserName, WinnerName): the respawn RespawnTime later, then the mode's OnDeathFunc */
static byte Split_OnDeathCallbackC(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rt = Mode_V(&t), *f = Mode_V(&t), *cb = Mode_V(&t);
	if (!Mode_G("Split", "RespawnTime", rt) || !Native_ToFloat(rt, f) || !Mode_LaterIn(f->f, "Split", "ReSpawnShip", looser, 1, NULL)) {
		return Mode_Done(&t, 0);
	}
	PyObject *fn = Mode_GlobalPy("Split", "OnDeathFunc");
	if (!fn) {
		return Mode_Done(&t, 0);
	}
	int set = PyObject_IsTrue(fn);
	byte ok = set >= 0 && (!set || Native_Call(fn, cb, "vv", looser, winner));
	Py_DECREF(fn);
	return Mode_Done(&t, ok);
}

static byte Split_ChooseShipsC(void) {
	Mode_SetActionSet("Inactive");
	return Mode_GVarSetS("EscapeEvent", "") && OWN("InitChooseScorer", NULL, "i", 0L) && OWN("InitChooseScorer", NULL, "i", 1L);
}

/* ShipMaker.PilotsList[int(Scrap.Rand(0, len(ShipMaker.PilotsList)))] (the engine's Rand in C, as before) */
static byte Split_RandomPilot(modeVals_t *t, nativeVal_t *out) {
	nativeVal_t *pilots = Mode_V(t);
	if (!Mode_ModVar("ShipMaker", "PilotsList", pilots)) {
		return 0;
	}
	int n = Native_Size(pilots);
	return n >= 0 && Native_Subscript(pilots, Mode_I(t, (long)Scrap_RandRange(0, n)), out);
}

/* the ship of hangar profile p for player name when it has an engine: 1 done, 0 no such ship, -1 error */
static int Split_TryProfile(const nativeVal_t *name, const nativeVal_t *profile) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *var = Mode_V(&t), *attribs = Mode_V(&t), *a = Mode_V(&t), *it[7], *life = Mode_V(&t), *pilot = Mode_V(&t);
	cEntity *v;
	if (!Split_Name(&t, "HangarShip", profile, var)) {
		return Mode_Done(&t, 0), -1;
	}
	const char *vn;
	if (!Native_ArgStr(var, 1, &vn) || !Native_GVarGet(vn, attribs)) {
		return Mode_Done(&t, 0), -1;
	}
	if (Native_IsStr(attribs, "")) {
		return Mode_Done(&t, 0), 0;
	}
	if (!Mode_Import("string") || !Native_Split(attribs, "<-", a)) {
		return Mode_Done(&t, 0), -1;
	}
	int any = 0;
	for (int i = 1; i <= 4 && !any; i++) {
		nativeVal_t *x = Mode_V(&t);
		if (!Native_Subscript(a, Mode_I(&t, i), x)) {
			return Mode_Done(&t, 0), -1;
		}
		any = !Native_IsStr(x, "");
	}
	if (!any) {
		return Mode_Done(&t, 0), 0;
	}
	if (!Mode_Ent(name, &v)) {
		return Mode_Done(&t, 0), -1;
	}
	const char *n = Native_Text(name);
	if (v && !Native_EntSet(v, n, "Active", Mode_I(&t, 0))) {
		return Mode_Done(&t, 0), -1;
	}
	for (int i = 0; i < 7; i++) {
		it[i] = Mode_V(&t);
		if (!Native_Subscript(a, Mode_I(&t, i), it[i])) {
			return Mode_Done(&t, 0), -1;
		}
	}
	if (!Native_ToInt(it[6], life) || !Split_RandomPilot(&t, pilot) ||
		!CALL("Vehicles", "SetProperties", NULL, "vvvvvvvvvvi", name, it[0], life, pilot, it[1], it[2], it[3], it[4], it[5], name, 1L) ||
		(v && !Native_EntSet(World_FindEntity(n), n, "Active", Mode_I(&t, 1)))) {
		return Mode_Done(&t, 0), -1;
	}
	return Mode_Done(&t, 1);
}

/* ChangeShip(id, Up=1): the next (or previous) hangar ship of the profiles 1 to 9 with an engine, else a SMerc1 */
static byte Split_ChangeShipC(const nativeVal_t *id, const nativeVal_t *up) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *prof = Mode_V(&t), *pilot = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("ShipMaker") || !Mode_Import("Vehicles")) {
		return Mode_Done(&t, 0);
	}
	int u = up ? Native_True(up) : 1;
	if (!Split_Name(&t, "Player", id, name)) {
		return Mode_Done(&t, 0);
	}
	for (int n = 0; n < 9; n++) {
		if (!Split_Add("MortalProfiles", &split.mortalProfiles, id, u ? 1 : -1) || !Native_Subscript(&split.mortalProfiles, id, prof)) {
			return Mode_Done(&t, 0);
		}
		int wrap = Native_Compare(prof, Mode_I(&t, u ? 9 : 1));
		Native_Free(prof);
		if ((u ? wrap > 0 : wrap < 0) && !Mode_DictPut("Split", "MortalProfiles", &split.mortalProfiles, id, Mode_I(&t, u ? 1 : 9))) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_Read("Split", "MortalProfiles", &split.mortalProfiles) || !Native_Subscript(&split.mortalProfiles, id, prof)) {
			return Mode_Done(&t, 0);
		}
		int got = Split_TryProfile(name, prof);
		Native_Free(prof);
		if (got) {
			return Mode_Done(&t, got > 0);
		}
	}
	return Mode_Done(&t, Split_RandomPilot(&t, pilot) && CALL("Vehicles", "SetStdProperties", NULL, "vsv", name, "SMerc1", pilot));
}

/* StartGame(id=0): the round starts (the mode's Init the first time), the scores and separators */
static byte Split_StartGameC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *first = Mode_V(&t), *msg = Mode_V(&t);
	if (!Mode_Import("Menu") || !Split_KeysOff(0) || !Split_KeysOff(1) || !Mode_ScorerClear(Mode_I(&t, 0)) ||
		!Mode_ScorerClear(Mode_I(&t, 1)) || !Mode_GVarSetS("EscapeEvent", "Split.PauseGame")) {
		return Mode_Done(&t, 0);
	}
	Mode_SetActionSet("Racer");
	if (!CALL("Menu", "DrawScorer", NULL, "is", 0L, "Racer") || !Mode_G("Split", "FirstTime", first)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(first)) {
		PyObject *mod = Mode_GlobalPy("Split", "ModLib");
		PyObject *v = mod ? Script_Method(mod, "Init", "()") : NULL;
		Py_XDECREF(mod);
		Py_XDECREF(v);
		Native_SetStr(msg, "Cu\xf1" "aaaao");
		if (!v || !Mode_Put("Split", "FirstTime", NULL, Mode_I(&t, 0)) || !Mode_PrintV(msg)) {
			return Mode_Done(&t, 0);
		}
	}
	Mode_SetTimeSpeed(1.0);
	for (int i = 0; i < 2; i++) {
		if (!OWN("MissionScorer", NULL, "i", (long)i) || !OWN("DefaultSplitScorer", NULL, "i", (long)i)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* DefaultSplitScorer(id): the line between the halves */
static byte Split_DefaultSplitScorerC(int id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	return Mode_Done(&t, Split_Name(&t, "SplitSeparator", Mode_I(&t, id), name) &&
		Split_Run(id, "separator", 2, "name", name, "y", Mode_I(&t, 239 + id)));
}

/* EndOfGame(id): player id won; the result for both, the next map 5 s later */
static byte Split_EndOfGameC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	Mode_SetActionSet("Inactive");
	if (!Mode_GVarSetS("EscapeEvent", "")) {
		return Mode_Done(&t, 0);
	}
	byte second = !Native_Compare(id, Mode_I(&t, 1));
	const char *results[2] = { second ? "XBox_Split_You_Loose" : "XBox_Split_You_Win", second ? "XBox_Split_You_Win" : "XBox_Split_You_Loose" };
	for (int i = 0; i < 2; i++) {
		nativeVal_t *obj = Mode_V(&t), *text = Mode_V(&t);
		if (!Split_Name(&t, "Pl", Mode_I(&t, i), obj) || !Mode_ScorerClear(Mode_I(&t, i)) || !Split_Lang(results[i], NULL, text) ||
			!OWN("LittleText", NULL, "ivvi", (long)i, obj, text, 100L) || !OWN("BlackBack", NULL, "i", (long)i) || !Split_KeysOff(i)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_LaterIn(5.0, "Split", "ReloadNext", NULL, 0, NULL));
}

/* ReloadNext(): the next map of the server list (or the same) */
static byte Split_ReloadNextC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *text = Mode_V(&t), *maps = Mode_V(&t), *path = Mode_V(&t), *here = Mode_V(&t), *at = Mode_V(&t), *nx = Mode_V(&t);
	nativeVal_t *k = Mode_V(&t), *map = Mode_V(&t), *next = Mode_V(&t), *len = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_Import("SaveGame") || !OWN("PurgeServerMapList", NULL, "") || !Native_GVarGet("ServerMapList", text) ||
		!Native_EvalText("Split", text, maps)) {
		return Mode_Done(&t, 0);
	}
	int any = Native_True(maps), n = any ? Native_Size(maps) : 0;
	if (n < 0 || !Mode_ModVar("Init", "Path", path)) {
		return Mode_Done(&t, 0);
	}
	if (!any || n == 0) {
		Native_Copy(next, path);
	} else {
		if (!Native_Slice(path, 7, INT_MAX, here)) {
			return Mode_Done(&t, 0);
		}
		int in = Native_Contains(maps, here);
		byte ok = in >= 0;
		if (in > 0) {
			Native_SetInt(len, n);
			ok = Mode_Index(maps, here, at) && Native_Arith('+', at, Mode_I(&t, 1), nx) && Native_Arith('%', nx, len, k) &&
				Native_Subscript(maps, k, map);
		} else if (in == 0) {
			ok = Native_Subscript(maps, Mode_I(&t, 0), map);
		}
		if (!ok || !Mode_Cat(&t, next, 2, Mode_S(&t, "Levels/"), map)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, CALL("SaveGame", "QLoad", NULL, "v", next));
}

/* ActualizeScorer(): a player at FragLimit wins, else the scores are shown */
static byte Split_ActualizeScorerC(void) {
	modeVals_t t = { .n = 0 };
	for (int i = 0; i < 2; i++) {
		nativeVal_t *v = Mode_V(&t), *limit = Mode_V(&t);
		if (!Mode_Read("Split", "Scorer", &split.scorer) || !Native_Subscript(&split.scorer, Mode_I(&t, i), v) ||
			!Native_GVarGet("FragLimit", limit)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(v, limit) >= 0) {
			return Mode_Done(&t, OWN("EndOfGame", NULL, "i", (long)i));
		}
	}
	for (int i = 0; i < 2; i++) {
		nativeVal_t *v = Mode_V(&t), *rep = Mode_V(&t);
		nativeBuf_t b;
		if (!Mode_Read("Split", "Scorer", &split.scorer) || !Native_Subscript(&split.scorer, Mode_I(&t, i), v)) {
			return Mode_Done(&t, 0);
		}
		Native_BufInit(&b);
		Native_Repr(&b, v, NR_REPR);
		Native_SetText(rep, b.p, b.len);
		Native_BufFree(&b);
		if (!Mode_ScorerSet(Mode_I(&t, i), Mode_S(&t, "MissionBest"), "Text", rep)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* MissionScorer(id): the score, the limit and the tab of player id, then ActualizeScorer */
static byte Split_MissionScorerC(int id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *limit = Mode_V(&t), *rep = Mode_V(&t);
	nativeBuf_t b;
	if (!Mode_Import("Scorer") || !Mode_Import("Menu") || !Native_GVarGet("FragLimit", limit)) {
		return Mode_Done(&t, 0);
	}
	Native_BufInit(&b);
	Native_Repr(&b, limit, NR_REPR);
	Native_SetText(rep, b.p, b.len);
	Native_BufFree(&b);
	return Mode_Done(&t, Split_Run(id, "mission", 2, "y", Mode_I(&t, 190 + 240 * id), "limit", rep) && OWN("ActualizeScorer", NULL, ""));
}

/* SeleShip(id): player id is ready; the round starts when both are */
static byte Split_SeleShipC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t);
	if (!Mode_DictPut("Split", "PlayersReady", &split.playersReady, id, Mode_I(&t, 1))) {
		return Mode_Done(&t, 0);
	}
	int both = 1;
	for (int i = 0; i < 2 && both; i++) {
		Native_Free(v);
		if (!Native_Subscript(&split.playersReady, Mode_I(&t, i), v)) {
			return Mode_Done(&t, 0);
		}
		both = !Native_Compare(v, Mode_I(&t, 1));
	}
	return Mode_Done(&t, both ? OWN("StartGame", NULL, "") : OWN("ReadyToStartScorer", NULL, "v", id));
}

/* InitChooseScorer(id): the ship choice of player id */
static byte Split_InitChooseScorerC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tx[3] = { Mode_V(&t), Mode_V(&t), Mode_V(&t) };
	int i;
	return Mode_Done(&t, Mode_DictPut("Split", "PlayersReady", &split.playersReady, id, Mode_I(&t, 0)) && Split_Id(id, &i) &&
		Split_Run(i, "keys_choose", 0) && Mode_ScorerClear(id) && Split_Lang("XBox_Split_SelectShip", NULL, tx[0]) &&
		Split_Lang("XBox_Split_Start", "\x14", tx[1]) && Split_Lang("XBox_Split_Abort", "\x17", tx[2]) && Split_Texts(Mode_I(&t, i), tx, 3));
}

/* ReadyToStartScorer(id): player id waits for the other */
static byte Split_ReadyToStartScorerC(int id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *wait = Mode_V(&t), *back = Mode_V(&t);
	return Mode_Done(&t, Mode_ScorerClear(Mode_I(&t, id)) && Split_Lang("XBox_Split_Waiting", NULL, wait) &&
		OWN("LittleText", NULL, "isvi", (long)id, "ReadyControl", wait, 74L) && Split_Lang("XBox_Split_Back", "\x17", back) &&
		OWN("LittleText", NULL, "isvi", (long)id, "PresToAbort", back, 124L) && OWN("BlackBack", NULL, "i", (long)id) &&
		Split_Run(id, "keys_ready", 0));
}

/* ConfirmAbort(id, event, Execute="Split.Abort", TXT=None): yes runs Execute, no runs event (exec, txt NULL: the
 * defaults) */
static byte Split_ConfirmAbortC(const nativeVal_t *id, const nativeVal_t *event, const nativeVal_t *exec, const nativeVal_t *txt) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tx[3] = { Mode_V(&t), Mode_V(&t), Mode_V(&t) };
	int i;
	if (!exec) {
		exec = Mode_S(&t, "Split.Abort");
	}
	if (!txt || txt->type == NV_NONE) {
		Mode_Lang("XBox_Split_Exit", tx[0]);
	} else {
		Native_Copy(tx[0], txt);
	}
	return Mode_Done(&t, Split_Id(id, &i) && Split_Run(i, "keys_confirm", 2, "exec", exec, "event", event) && Mode_ScorerClear(id) &&
		Split_Lang("XBox_Split_Yes", "\x14", tx[1]) && Split_Lang("XBox_Split_No", "\x15", tx[2]) && Split_Texts(Mode_I(&t, i), tx, 3));
}

/* Abort(id): split screen off, back to the menu level */
static byte Split_AbortC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *aspect = Mode_V(&t);
	Native_SetFloat(aspect, 1.3333);
	return Mode_Done(&t, Mode_Import("SaveGame") && Mode_GVarSet("r_SplitScreen", Mode_I(&t, 0)) && Mode_GVarSet("SplitScreen", Mode_I(&t, 0)) &&
		Mode_GVarSet("CameraDefViewAspect", aspect) && CALL("SaveGame", "QLoad", NULL, "s", "Levels/Menu"));
}

/* Restart(id): the same map again */
static byte Split_RestartC(void) {
	nativeVal_t path;
	Native_SetNone(&path);
	byte ok = Mode_Import("SaveGame") && Mode_Import("Init") && Mode_ModVar("Init", "Path", &path) && CALL("SaveGame", "QLoad", NULL, "v", &path);
	Native_Free(&path);
	return ok;
}

/* PauseGame(id): the time stops, both get the pause menu */
static byte Split_PauseGameC(void) {
	Mode_SetTimeSpeed(0.0);
	Mode_SetActionSet("Inactive");
	return Mode_GVarSetS("EscapeEvent", "") && OWN("PauseGameMenu", NULL, "i", 0L) && OWN("PauseGameMenu", NULL, "i", 1L);
}

/* PauseGameMenu(id): restart, abort or back to the round */
static byte Split_PauseGameMenuC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tx[3] = { Mode_V(&t), Mode_V(&t), Mode_V(&t) };
	int i;
	return Mode_Done(&t, Mode_DictPut("Split", "PlayersReady", &split.playersReady, id, Mode_I(&t, 0)) && Split_Id(id, &i) &&
		Split_Run(i, "keys_pause", 0) && Mode_ScorerClear(id) && Split_Lang("XBox_Split_Restart", "\x14", tx[0]) &&
		Split_Lang("XBox_Split_Abort", "\x17", tx[1]) && Split_Lang("XBox_Split_Return", NULL, tx[2]) && Split_Texts(Mode_I(&t, i), tx, 3));
}

/* LittleText(id, objName, Text, Pos): a centred line of a menu in the half of player id */
static byte Split_LittleTextC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *color = Mode_V(&t), *c[4], *y = Mode_V(&t);
	int i;
	if (!Mode_Import("Scorer")) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t *delta = Mode_I(&t, Native_Compare(&a[0], Mode_I(&t, 0)) == 0 ? 0 : 240);
	if (!Split_Id(&a[0], &i) || !Mode_ScorerAdd(&a[0], &a[1], "Text") || !Mode_ScorerSet(&a[0], &a[1], "Text", &a[2]) ||
		!Mode_ModVar("Scorer", "MissionScorerText_Color", color)) {
		return Mode_Done(&t, 0);
	}
	for (int k = 0; k < 4; k++) {
		c[k] = Mode_V(&t);
		if (!Native_Subscript(color, Mode_I(&t, k), c[k])) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Native_Arith('+', delta, &a[3], y) &&
		Split_Run(i, "little", 6, "name", &a[1], "r", c[0], "g", c[1], "b", c[2], "a", c[3], "y", y));
}

/* BlackBack(id): the dark band behind the menu lines */
static byte Split_BlackBackC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	int lower = Native_Compare(id, Mode_I(&t, 0)), i;
	return Mode_Done(&t, Split_Id(id, &i) && Split_Run(i, "black", 1, "y", Mode_I(&t, lower == 0 ? 50 : 290)));
}

/* the adapters of the module table (OBJLIB 7: arguments, results and the errors of the Python calls) */
#define SP_FN(name, nargs, call) MODE_FN(Split, name, nargs, call)

static PyObject *Split_PurgeServerMapList(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Mode_PurgeServerMapList("Split"));
}

/* GetSpawnPos(id) and GetReSpawnPos(v): a spawn point of OutMap */
SP_FN(GetSpawnPos, 1, Mode_Import("OutMap") && CALL("OutMap", "GetSpawnPos", out, ""))
SP_FN(CreateUserShip, 1, Split_CreateUserShipC(&a[0]))
SP_FN(ReSpawnShip, 1, Split_ReSpawnShipC(&a[0]))
SP_FN(OnDeathCallback, 2, Split_OnDeathCallbackC(&a[0], &a[1]))
SP_FN(NextShip, 1, OWN("ChangeShip", NULL, "vi", &a[0], 1L))
SP_FN(PrevShip, 1, OWN("ChangeShip", NULL, "vi", &a[0], 0L))
SP_FN(EndOfGame, 1, Split_EndOfGameC(&a[0]))
SP_FN(SeleShip, 1, Split_SeleShipC(&a[0]))
SP_FN(InitChooseScorer, 1, Split_InitChooseScorerC(&a[0]))
SP_FN(ConfirmAbortMain, 1, OWN("ConfirmAbort", NULL, "vs", &a[0], "Split.InitChooseScorer"))
SP_FN(ConfirmAbortPause, 1, OWN("ConfirmAbort", NULL, "vs", &a[0], "Split.PauseGameMenu"))
SP_FN(ConfirmRestartPause, 1, Split_Lang("XBox_Split_RestartMatch", NULL, out) &&
	OWN("ConfirmAbort", NULL, "vssv", &a[0], "Split.PauseGameMenu", "Split.Restart", out) && (Native_Free(out), 1))
SP_FN(Abort, 1, Split_AbortC())
SP_FN(Restart, 1, Split_RestartC())
SP_FN(PauseGame, 1, Split_PauseGameC())
SP_FN(PauseGameMenu, 1, Split_PauseGameMenuC(&a[0]))
SP_FN(LittleText, 4, Split_LittleTextC(a))
SP_FN(BlackBack, 1, Split_BlackBackC(&a[0]))

/* the functions without arguments that ignore whatever they get */
#define SP_NOARGS(name, call) \
	static PyObject *Split_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Mode_Result(call); \
	}

SP_NOARGS(Init, Split_InitC())
SP_NOARGS(ChooseShips, Split_ChooseShipsC())
SP_NOARGS(ReloadNext, Split_ReloadNextC())
SP_NOARGS(ActualizeScorer, Split_ActualizeScorerC())

/* the functions of an "i" argument */
#define SP_INT(name, call) \
	static PyObject *Split_##name(PyObject *self, PyObject *args) { \
		int id; \
		(void)self; \
		return PyArg_ParseTuple(args, "i", &id) ? Mode_Result(call) : NULL; \
	}

SP_INT(DefaultSplitScorer, Split_DefaultSplitScorerC(id))
SP_INT(MissionScorer, Split_MissionScorerC(id))
SP_INT(ReadyToStartScorer, Split_ReadyToStartScorerC(id))

static PyObject *Split_ChangeShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "Up", NULL };
	PyObject *id, *up = NULL;
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *u = Mode_V(&t), *out = Mode_V(&t);
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &id, &up)) {
		return Mode_Done(&t, 0), NULL;
	}
	Native_Arg(id, a);
	if (up) {
		Native_Arg(up, u);
	}
	return Mode_Out(&t, Split_ChangeShipC(a, up ? u : NULL), out);
}

static PyObject *Split_StartGame(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	(void)self;
	return PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id) ? Mode_Result(Split_StartGameC()) : NULL;
}

static PyObject *Split_ConfirmAbort(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "event", "Execute", "TXT", NULL };
	PyObject *id, *event, *exec = NULL, *txt = NULL;
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *e = Mode_V(&t), *x = Mode_V(&t), *tx = Mode_V(&t), *out = Mode_V(&t);
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OO", names, &id, &event, &exec, &txt)) {
		return Mode_Done(&t, 0), NULL;
	}
	Native_Arg(id, a);
	Native_Arg(event, e);
	if (exec) {
		Native_Arg(exec, x);
	}
	if (txt) {
		Native_Arg(txt, tx);
	}
	return Mode_Out(&t, Split_ConfirmAbortC(a, e, exec ? x : NULL, txt ? tx : NULL), out);
}

#define KW(fn) (PyCFunction)(void (*)(void))fn, 3

PyMethodDef g_SplitMethods[] = {
	{ "PurgeServerMapList", Split_PurgeServerMapList, 1, NULL },
	{ "GetSpawnPos", Split_GetSpawnPos, 1, NULL },
	{ "CreateUserShip", Split_CreateUserShip, 1, NULL },
	{ "Init", Split_Init, 1, NULL },
	{ "GetReSpawnPos", Split_GetSpawnPos, 1, NULL },
	{ "ReSpawnShip", Split_ReSpawnShip, 1, NULL },
	{ "OnDeathCallback", Split_OnDeathCallback, 1, NULL },
	{ "ChooseShips", Split_ChooseShips, 1, NULL },
	{ "ChangeShip", KW(Split_ChangeShip), NULL },
	{ "NextShip", Split_NextShip, 1, NULL },
	{ "PrevShip", Split_PrevShip, 1, NULL },
	{ "StartGame", KW(Split_StartGame), NULL },
	{ "DefaultSplitScorer", Split_DefaultSplitScorer, 1, NULL },
	{ "EndOfGame", Split_EndOfGame, 1, NULL },
	{ "ReloadNext", Split_ReloadNext, 1, NULL },
	{ "ActualizeScorer", Split_ActualizeScorer, 1, NULL },
	{ "MissionScorer", Split_MissionScorer, 1, NULL },
	{ "SeleShip", Split_SeleShip, 1, NULL },
	{ "InitChooseScorer", Split_InitChooseScorer, 1, NULL },
	{ "ReadyToStartScorer", Split_ReadyToStartScorer, 1, NULL },
	{ "ConfirmAbortMain", Split_ConfirmAbortMain, 1, NULL },
	{ "ConfirmAbortPause", Split_ConfirmAbortPause, 1, NULL },
	{ "ConfirmRestartPause", Split_ConfirmRestartPause, 1, NULL },
	{ "ConfirmAbort", KW(Split_ConfirmAbort), NULL },
	{ "Abort", Split_Abort, 1, NULL },
	{ "Restart", Split_Restart, 1, NULL },
	{ "PauseGame", Split_PauseGame, 1, NULL },
	{ "PauseGameMenu", Split_PauseGameMenu, 1, NULL },
	{ "LittleText", Split_LittleText, 1, NULL },
	{ "BlackBack", Split_BlackBack, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the modes: SDeathMatch, SFlagHunt, SPostman, SRace ---- */

/* `global pl0, pl1; pl0 = Scrap.GetEntity("Player0"); pl1 = ...` and the TgType of both (OBJLIB 7: the handles stay
 * in the module) */
static byte Sm_Players(const char *mod, const char *tg) {
	cEntity *e0 = World_FindEntity("Player0");
	if (!Script_SetGlobal(mod, "pl0", Mode_Handle(e0))) {
		return 0;
	}
	cEntity *e1 = World_FindEntity("Player1");
	return Script_SetGlobal(mod, "pl1", Mode_Handle(e1)) && Mode_SetS(e0, "Player0", "TgType", tg) && Mode_SetS(e1, "Player1", "TgType", tg);
}

/* import Split; Split.OnDeathFunc = OnDeathFunc (the mode's) */
static byte Sm_OnDeath(const char *mod) {
	PyObject *f = Mode_Import("Split") ? Mode_GlobalPy(mod, "OnDeathFunc") : NULL;
	if (!f) {
		return 0;
	}
	Script_SetVar("Split", "OnDeathFunc", f);
	return 1;
}

/* pl<i>.var = v of the mode */
static byte Sm_SetPl(const char *mod, int i, const char *var, const nativeVal_t *v) {
	PyObject *pl = Mode_GlobalPy(mod, i ? "pl1" : "pl0");
	byte ok = pl && Mode_HSet(pl, var, v);
	Py_XDECREF(pl);
	return ok;
}

/* the score of the player called name (Player0, Player1) changes by d: 1, 0 for another name, -1 error */
static int Sm_Score(const nativeVal_t *name, long d) {
	int i = Native_IsStr(name, "Player1") ? 1 : Native_IsStr(name, "Player0") ? 0 : -1;
	if (i < 0) {
		return 0;
	}
	nativeVal_t idx;
	Native_SetInt(&idx, i);
	return Split_Add("Scorer", &split.scorer, &idx, d) ? 1 : -1;
}

/* `global ZoneList; ZoneList = []` and the positions of the dummies of pattern (appended to the list in place) */
static byte Sm_Browse(const char *mod, const char *pattern) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *empty = Mode_V(&t), *d = Mode_V(&t), *pos = Mode_V(&t);
	Native_SetEmpty(empty, NV_LIST);
	if (!Mode_Put(mod, "ZoneList", &split.zoneList, empty)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyStart(pattern, 1);
	for (;;) {
		Native_Free(d);
		Native_Free(pos);
		Mode_DummyNext(d);
		if (!Native_True(d)) {
			return Mode_Done(&t, 1);
		}
		if (!Native_Subscript(d, Mode_I(&t, 1), pos) || !Mode_Append(mod, "ZoneList", pos)) {
			return Mode_Done(&t, 0);
		}
	}
}

/* GetRndFlag() of SFlagHunt and SPostman: a random zone, the next one when it is the current */
static byte Sm_RndFlag(const char *mod) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *niu = Mode_V(&t), *s = Mode_V(&t), *len = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Read(mod, "ZoneList", &split.zoneList)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&split.zoneList);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	/* the engine's Rand in C, as before */
	Native_SetInt(niu, (long)Scrap_RandRange(0, n));
	if (!Mode_Read(mod, "AcFlag", &split.acFlag)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(niu, &split.acFlag)) {
		return Mode_Done(&t, Mode_Put(mod, "AcFlag", &split.acFlag, niu));
	}
	Native_SetInt(len, n);
	return Mode_Done(&t, Native_Arith('+', &split.acFlag, Mode_I(&t, 1), s) && Native_Arith('%', s, len, v) &&
		Mode_Put(mod, "AcFlag", &split.acFlag, v));
}

/* Flag.var = v of the mode */
static byte Sm_FlagSet(const char *mod, const char *var, const nativeVal_t *v) {
	PyObject *flag = Mode_GlobalPy(mod, "Flag");
	byte ok = flag && Mode_HSet(flag, var, v);
	Py_XDECREF(flag);
	return ok;
}

/* Flag.SetModel(file) of the mode */
static byte Sm_FlagModel(const char *mod, const nativeVal_t *file) {
	PyObject *flag = Mode_GlobalPy(mod, "Flag");
	byte ok = flag && Mode_HModel(flag, file);
	Py_XDECREF(flag);
	return ok;
}

/* Flag.Pos = ZoneList[AcFlag] of the mode */
static byte Sm_FlagToZone(const char *mod) {
	nativeVal_t z;
	Native_SetNone(&z);
	byte ok = Mode_Read(mod, "ZoneList", &split.zoneList) && Mode_Read(mod, "AcFlag", &split.acFlag) &&
		Native_Subscript(&split.zoneList, &split.acFlag, &z) && Sm_FlagSet(mod, "Pos", &z);
	Native_Free(&z);
	return ok;
}

/* Flag.Pos = <global> of the mode */
static byte Sm_FlagToGlobal(const char *mod, const char *name) {
	nativeVal_t v;
	byte ok = Mode_G(mod, name, &v) && Sm_FlagSet(mod, "Pos", &v);
	Native_Free(&v);
	return ok;
}

/* Scrap.Set(var, Flag.Name) */
static byte Sm_WayToFlag(const char *mod, const char *var) {
	nativeVal_t name;
	Native_SetNone(&name);
	PyObject *flag = Mode_GlobalPy(mod, "Flag");
	byte ok = flag && Mode_HGet(flag, "Name", &name) && Mode_GVarSet(var, &name);
	Py_XDECREF(flag);
	Native_Free(&name);
	return ok;
}

/* `global Flag; Flag = Scrap.CreateEntity("FlagToBeHunted", 0, 0, 0, "Item")` with the model of the module variable
 * model and the radius */
static byte Sm_NewFlag(const char *mod, const char *model, long radius) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *file = Mode_V(&t);
	cEntity *e;
	if (!Mode_Create(Mode_S(&t, "FlagToBeHunted"), Mode_I(&t, 0), Mode_I(&t, 0), Mode_I(&t, 0), "Item", &e) ||
		!Script_SetGlobal(mod, "Flag", Mode_Handle(e))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_G(mod, model, file) && Sm_FlagModel(mod, file) && Sm_FlagSet(mod, "Radius", Mode_I(&t, radius)) &&
		Sm_FlagSet(mod, "RadarShow", Mode_I(&t, 1)));
}

/* Split.ActualizeScorer() */
static byte Sm_Actualize(void) {
	return CALL("Split", "ActualizeScorer", NULL, "");
}

#define SM_NOARGS(pre, name, call) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Mode_Result(call); \
	}

static PyObject *Sm_Pass(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

static PyObject *Sm_Pass2(PyObject *self, PyObject *args) {
	PyObject *a, *b;
	(void)self;
	return PyArg_ParseTuple(args, "OO", &a, &b) ? Py_ReturnNone() : NULL;
}

/* ---- SDeathMatch ---- */

static byte SDeathMatch_InitC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Sm_Players("SDeathMatch", "Killer") && Sm_SetPl("SDeathMatch", 0, "Target", Mode_S(&t, "Player1")) &&
		Sm_SetPl("SDeathMatch", 1, "Target", Mode_S(&t, "Player0")) && Sm_OnDeath("SDeathMatch"));
}

/* OnDeathFunc(LooserName, WinnerName): the killer scores, a suicide costs a point */
static byte SDeathMatch_OnDeathFuncC(const nativeVal_t *looser, const nativeVal_t *winner) {
	if (!Mode_Import("Split")) {
		return 0;
	}
	int self = Native_IsStr(winner, "") || !Native_Compare(winner, looser);
	return (self ? Sm_Score(looser, -1) : Sm_Score(winner, 1)) >= 0 && Sm_Actualize();
}

SM_NOARGS(SDeathMatch, Init, SDeathMatch_InitC())
MODE_FN(SDeathMatch, OnDeathFunc, 2, SDeathMatch_OnDeathFuncC(&a[0], &a[1]))

PyMethodDef g_SDeathMatchMethods[] = {
	{ "InitStatus", Sm_Pass, 1, NULL },
	{ "Init", SDeathMatch_Init, 1, NULL },
	{ "OnDeathFunc", SDeathMatch_OnDeathFunc, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- SFlagHunt ---- */

/* TakeFlag(ItemName, VehicleName): a player scores, the flag goes to another zone */
static byte SFlagHunt_TakeFlagC(const nativeVal_t *vehicle) {
	if (!Mode_Import("Split") || !Mode_Import("Sound")) {
		return 0;
	}
	int doit = Sm_Score(vehicle, 1);
	return doit == 0 || (doit > 0 && Mode_Sound("WaySound", "WayVolume") && CALL("SFlagHunt", "GetRndFlag", NULL, "") &&
		Sm_FlagToZone("SFlagHunt") && Sm_Actualize());
}

static byte SFlagHunt_CreateFlagC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Sm_NewFlag("SFlagHunt", "FlagModel", 2000) && CALL("SFlagHunt", "GetRndFlag", NULL, "") &&
		Sm_FlagSet("SFlagHunt", "OnTake", Mode_S(&t, "SFlagHunt.TakeFlag")) && Sm_FlagToZone("SFlagHunt") &&
		Sm_WayToFlag("SFlagHunt", "CurrentWaypointName"));
}

SM_NOARGS(SFlagHunt, BrowseAreas, Sm_Browse("SFlagHunt", "DM_Multiplayer_Flag_*"))
SM_NOARGS(SFlagHunt, GetRndFlag, Sm_RndFlag("SFlagHunt"))
MODE_FN(SFlagHunt, TakeFlag, 2, SFlagHunt_TakeFlagC(&a[1]))
SM_NOARGS(SFlagHunt, CreateFlag, SFlagHunt_CreateFlagC())
SM_NOARGS(SFlagHunt, InitStatus, CALL("SFlagHunt", "BrowseAreas", NULL, "") && CALL("SFlagHunt", "CreateFlag", NULL, ""))
SM_NOARGS(SFlagHunt, Init, Sm_Players("SFlagHunt", "Killer") && Sm_OnDeath("SFlagHunt"))
/* OnDeathFunc(LooserName, WinnerName): a death costs a point */
MODE_FN(SFlagHunt, OnDeathFunc, 2, Mode_Import("Split") && Sm_Score(&a[0], -1) >= 0 && Sm_Actualize())

PyMethodDef g_SFlagHuntMethods[] = {
	{ "BrowseAreas", SFlagHunt_BrowseAreas, 1, NULL },
	{ "GetRndFlag", SFlagHunt_GetRndFlag, 1, NULL },
	{ "TakeFlag", SFlagHunt_TakeFlag, 1, NULL },
	{ "CreateFlag", SFlagHunt_CreateFlag, 1, NULL },
	{ "InitStatus", SFlagHunt_InitStatus, 1, NULL },
	{ "Init", SFlagHunt_Init, 1, NULL },
	{ "OnDeathFunc", SFlagHunt_OnDeathFunc, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- SPostman ---- */

static byte SPostman_BrowseAreasC(void) {
	nativeVal_t d, pos;
	if (!Sm_Browse("SPostman", "DM_Domination_Zone_*")) {
		return 0;
	}
	Mode_DummyStart("DM_Domination_Flag", 1);
	Mode_DummyNext(&d);
	Native_SetNone(&pos);
	byte ok = Native_Subscript(&d, &(nativeVal_t){ .type = NV_INT, .i = 1 }, &pos) && Mode_Put("SPostman", "FlagInitPos", NULL, &pos);
	Native_Free(&d);
	Native_Free(&pos);
	return ok;
}

/* the carrier of the data pack is vehicle (BattonOwner): 1, 0, -1 */
static int SPostman_Carrier(const nativeVal_t *vehicle) {
	nativeVal_t owner;
	if (!Mode_G("SPostman", "BattonOwner", &owner)) {
		return -1;
	}
	int c = !Native_Compare(vehicle, &owner);
	Native_Free(&owner);
	return c;
}

/* TakeArea(ItemName, VehicleName): the carrier reaches the zone and scores, the zone moves */
static byte SPostman_TakeAreaC(const nativeVal_t *vehicle) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tail = Mode_V(&t), *idx = Mode_V(&t);
	if (!Mode_Import("Split") || !Mode_Import("Sound")) {
		return Mode_Done(&t, 0);
	}
	int carrier = SPostman_Carrier(vehicle);
	if (carrier <= 0) {
		return Mode_Done(&t, carrier == 0);
	}
	return Mode_Done(&t, Mode_Put("SPostman", "BattonOwner", NULL, vehicle) && Mode_Sound("WaySound", "WayVolume") &&
		CALL("SPostman", "GetRndFlag", NULL, "") && Sm_FlagToZone("SPostman") && Native_Slice(vehicle, 6, INT_MAX, tail) &&
		Native_ToInt(tail, idx) && Split_Add("Scorer", &split.scorer, idx, 1) && Sm_Actualize());
}

/* TakeFlag(ItemName, VehicleName): a player takes the data pack and has to bring it to the zone */
static byte SPostman_TakeFlagC(const nativeVal_t *vehicle) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = Mode_V(&t);
	if (!Mode_Import("Split") || !Mode_Import("Sound")) {
		return Mode_Done(&t, 0);
	}
	if (!Native_IsStr(vehicle, "Player1") && !Native_IsStr(vehicle, "Player0")) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_DelSchedS("BattonRepos") || !Mode_Put("SPostman", "BattonOwner", NULL, vehicle) || !Mode_Sound("WaySound", "WayVolume") ||
		!CALL("SPostman", "GetRndFlag", NULL, "") || !Sm_FlagToZone("SPostman") || !Mode_G("SPostman", "FlagModel", model) ||
		!Sm_FlagModel("SPostman", model) || !Sm_FlagSet("SPostman", "OnTake", Mode_S(&t, "SPostman.TakeArea"))) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(vehicle, "Player0")) {
		return Mode_Done(&t, Sm_WayToFlag("SPostman", "CurrentWaypointName") && Mode_GVarSetS("CurrentWaypointName2", "Player0"));
	}
	return Mode_Done(&t, Mode_GVarSetS("CurrentWaypointName", "Player1") && Sm_WayToFlag("SPostman", "CurrentWaypointName2"));
}

static byte SPostman_CreateFlagC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Sm_NewFlag("SPostman", "BattModel", 2000) && CALL("SPostman", "GetRndFlag", NULL, "") &&
		Sm_FlagSet("SPostman", "OnTake", Mode_S(&t, "SPostman.TakeFlag")) && Sm_FlagToGlobal("SPostman", "FlagInitPos") &&
		Sm_WayToFlag("SPostman", "CurrentWaypointName"));
}

static byte SPostman_InitStatusC(void) {
	nativeVal_t f;
	if (!CALL("SPostman", "BrowseAreas", NULL, "") || !CALL("SPostman", "CreateFlag", NULL, "") || !Mode_G("SPostman", "FlagModel", &f)) {
		return 0;
	}
	byte ok = Mode_Preload(&f);
	Native_Free(&f);
	ok = ok && Mode_G("SPostman", "BattModel", &f) && Mode_Preload(&f);
	Native_Free(&f);
	return ok;
}

/* OnDeathFunc(LooserName, WinnerName): the carrier drops the data pack where it died; it returns 30 s later */
static byte SPostman_OnDeathFuncC(const nativeVal_t *looser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = Mode_V(&t), *pos = Mode_V(&t);
	cEntity *e;
	if (!Mode_Import("Split")) {
		return Mode_Done(&t, 0);
	}
	int carrier = SPostman_Carrier(looser);
	if (carrier <= 0) {
		return Mode_Done(&t, carrier == 0);
	}
	return Mode_Done(&t, Mode_Put("SPostman", "BattonOwner", NULL, Mode_S(&t, "")) && Sm_WayToFlag("SPostman", "CurrentWaypointName") &&
		Sm_WayToFlag("SPostman", "CurrentWaypointName2") && Mode_G("SPostman", "BattModel", model) && Sm_FlagModel("SPostman", model) &&
		Sm_FlagSet("SPostman", "OnTake", Mode_S(&t, "SPostman.TakeFlag")) && Mode_Ent(looser, &e) &&
		Mode_Get(e, Native_Text(looser), "Pos", pos) && Sm_FlagSet("SPostman", "Pos", pos) &&
		Mode_LaterIn(30.0, "SPostman", "BattonRepos", NULL, 0, "BattonRepos"));
}

static byte SPostman_InitC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Sm_Players("SPostman", "Green") && Sm_SetPl("SPostman", 0, "RadarShow", Mode_I(&t, 3)) &&
		Sm_SetPl("SPostman", 1, "RadarShow", Mode_I(&t, 3)) && Sm_OnDeath("SPostman"));
}

SM_NOARGS(SPostman, BrowseAreas, SPostman_BrowseAreasC())
SM_NOARGS(SPostman, GetRndFlag, Sm_RndFlag("SPostman"))
MODE_FN(SPostman, TakeArea, 2, SPostman_TakeAreaC(&a[1]))
MODE_FN(SPostman, TakeFlag, 2, SPostman_TakeFlagC(&a[1]))
SM_NOARGS(SPostman, CreateFlag, SPostman_CreateFlagC())
SM_NOARGS(SPostman, InitStatus, SPostman_InitStatusC())
SM_NOARGS(SPostman, Init, SPostman_InitC())
MODE_FN(SPostman, OnDeathFunc, 2, SPostman_OnDeathFuncC(&a[0]))
/* BattonRepos(): the data pack back at its start (the script sets it twice) */
SM_NOARGS(SPostman, BattonRepos, Sm_FlagToGlobal("SPostman", "FlagInitPos") && Sm_FlagToGlobal("SPostman", "FlagInitPos"))

PyMethodDef g_SPostmanMethods[] = {
	{ "BrowseAreas", SPostman_BrowseAreas, 1, NULL },
	{ "GetRndFlag", SPostman_GetRndFlag, 1, NULL },
	{ "TakeArea", SPostman_TakeArea, 1, NULL },
	{ "TakeFlag", SPostman_TakeFlag, 1, NULL },
	{ "CreateFlag", SPostman_CreateFlag, 1, NULL },
	{ "InitStatus", SPostman_InitStatus, 1, NULL },
	{ "Init", SPostman_Init, 1, NULL },
	{ "OnDeathFunc", SPostman_OnDeathFunc, 1, NULL },
	{ "BattonRepos", SPostman_BattonRepos, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- SRace ---- */

/* the step "point <text>" of srace.def: a checkpoint of the route (OBJLIB 6: the steps take Python arguments) */
static void SRace_StepPoint(int id, PyObject *args) {
	nativeVal_t text, p;
	(void)id;
	if (PyTuple_Size(args) != 1 || split.raceBuild.type != NV_LIST) {
		return;
	}
	Native_Arg(PyTuple_GetItem(args, 0), &text);
	if (Native_EvalText("SRace", &text, &p)) {
		*Native_Append(&split.raceBuild) = p;
	} else {
		PyErr_Clear();
	}
	Native_Free(&text);
}

static const scriptStep_t srace_steps[] = {
	{ "point", SRace_StepPoint },
	{ NULL, NULL },
};

/* BrowseAreas(): the route of the map (Init.UpperMap), the race of before on a map without one */
static byte SRace_BrowseAreasC(void) {
	nativeVal_t map;
	if (!Mode_Import("Init") || !Mode_ModVar("Init", "UpperMap", &map)) {
		return 0;
	}
	byte ok = 1;
	if (map.type == NV_STR) {
		Native_Free(&split.raceBuild);
		Native_SetEmpty(&split.raceBuild, NV_LIST);
		Script_RunFile(0, "split/srace.def", Native_Text(&map), srace_def, srace_steps, NULL);
		if (split.raceBuild.n > 0) {
			/* race = tuple of the points */
			split.raceBuild.type = NV_TUPLE;
			ok = Mode_Put("SRace", "race", &split.race, &split.raceBuild);
		}
		Native_Free(&split.raceBuild);
	}
	Native_Free(&map);
	return ok;
}

/* race[AcFlag] into *out */
static byte SRace_Point(nativeVal_t *out) {
	return Mode_Read("SRace", "race", &split.race) && Mode_Read("SRace", "AcFlag", &split.acFlag) &&
		Native_Subscript(&split.race, &split.acFlag, out);
}

/* Flag.Pos = race[AcFlag][0]; Flag.Ang = 0, race[AcFlag][1], 0 */
static byte SRace_PlaceFlag(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t);
	if (!SRace_Point(p) || !Native_Subscript(p, Mode_I(&t, 0), pos) || !Sm_FlagSet("SRace", "Pos", pos)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(p);
	return Mode_Done(&t, SRace_Point(p) && Split_Ang(&t, p, ang) && Sm_FlagSet("SRace", "Ang", ang));
}

/* GetRndFlag(): the next checkpoint */
static byte SRace_GetRndFlagC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t), *len = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Read("SRace", "AcFlag", &split.acFlag) || !Native_Arith('+', &split.acFlag, Mode_I(&t, 1), s) ||
		!Mode_Read("SRace", "race", &split.race)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&split.race);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(len, n);
	return Mode_Done(&t, Native_Arith('%', s, len, v) && Mode_Put("SRace", "AcFlag", &split.acFlag, v));
}

/* TakeFlag(ItemName, VehicleName): a player scores, the flag goes to the next checkpoint */
static byte SRace_TakeFlagC(const nativeVal_t *vehicle) {
	if (!Mode_Import("Split") || !Mode_Import("Sound")) {
		return 0;
	}
	int doit = Sm_Score(vehicle, 1);
	return doit == 0 || (doit > 0 && Mode_Sound("WaySound", "WayVolume") && CALL("SRace", "GetRndFlag", NULL, "") && SRace_PlaceFlag() &&
		Sm_Actualize());
}

static byte SRace_CreateFlagC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Sm_NewFlag("SRace", "FlagModel", 4000) && CALL("SRace", "GetRndFlag", NULL, "") &&
		Sm_FlagSet("SRace", "OnTake", Mode_S(&t, "SRace.TakeFlag")) && SRace_PlaceFlag() && Sm_WayToFlag("SRace", "CurrentWaypointName"));
}

/* InitRacePos(id): beside the checkpoint before the flag, 1500 to the left or right; (pos, angle) */
static byte SRace_InitRacePosC(const nativeVal_t *id, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *len = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *k = Mode_V(&t), *res = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *src = Mode_V(&t), *rot = Mode_V(&t), *r = Mode_V(&t), *at = Mode_V(&t), *pos = Mode_V(&t), *a2 = Mode_V(&t);
	if (!Mode_Read("SRace", "race", &split.race) || !Mode_Read("SRace", "AcFlag", &split.acFlag)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&split.race);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(len, n);
	if (!Native_Arith('+', &split.acFlag, len, a) || !Native_Arith('-', a, Mode_I(&t, 1), b) || !Native_Arith('%', b, len, k) ||
		!Native_Subscript(&split.race, k, res)) {
		return Mode_Done(&t, 0);
	}
	int first = Native_Compare(id, Mode_I(&t, 0)) == 0;
	if (!Native_Subscript(res, Mode_I(&t, 1), ang)) {
		return Mode_Done(&t, 0);
	}
	/* SVec.Rotate3D((±1500, 0, 0), (0, ang, 0)); SVec.Add(that, res[0]) */
	Native_SetEmpty(src, NV_TUPLE);
	Native_SetInt(Native_Append(src), first ? 1500 : -1500);
	Native_SetInt(Native_Append(src), 0);
	Native_SetInt(Native_Append(src), 0);
	Native_SetEmpty(rot, NV_TUPLE);
	Native_SetInt(Native_Append(rot), 0);
	Native_Copy(Native_Append(rot), ang);
	Native_SetInt(Native_Append(rot), 0);
	if (!Mode_Rotate(src, rot, r) || !Native_Subscript(res, Mode_I(&t, 0), at) || !Mode_VecAdd(r, at, pos) ||
		!Native_Subscript(res, Mode_I(&t, 1), a2)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), pos);
	Native_Copy(Native_Append(out), a2);
	return Mode_Done(&t, 1);
}

/* GetReSpawnPos(v): where the ship is, its heading (v the handle of the ship) */
static byte SRace_GetReSpawnPosC(const nativeVal_t *v, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *ang = Mode_V(&t), *a1 = Mode_V(&t);
	if (!Mode_VGet(v, "Pos", pos) || !Mode_VGet(v, "Ang", ang) || !Native_Subscript(ang, Mode_I(&t, 1), a1)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), pos);
	Native_Copy(Native_Append(out), a1);
	return Mode_Done(&t, 1);
}

/* SSound.LoadSound("Sounds/Vehicles/Misc/" + <module variable> + ".wav") */
static byte SRace_LoadSound(const char *var) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *file = Mode_V(&t);
	return Mode_Done(&t, Mode_G("SRace", var, name) && Mode_Cat(&t, file, 3, Mode_S(&t, "Sounds/Vehicles/Misc/"), name, Mode_S(&t, ".wav")) &&
		Mode_LoadSound(file));
}

/* InitStatus(): the spawn functions of Split, the count down sounds, the route and the flag */
static byte SRace_InitStatusC(void) {
	PyObject *f1 = Mode_Import("Split") ? Mode_GlobalPy("SRace", "InitRacePos") : NULL;
	if (!f1) {
		return 0;
	}
	/* OBJLIB 3: the functions as values of Split */
	Script_SetVar("Split", "FuncGetSpawnPos", f1);
	PyObject *f2 = Mode_GlobalPy("SRace", "GetReSpawnPos");
	if (!f2) {
		return 0;
	}
	Script_SetVar("Split", "FuncGetReSpawnPos", f2);
	return SRace_LoadSound("RaceCountDownSound1") && SRace_LoadSound("RaceCountDownSound2") && CALL("SRace", "BrowseAreas", NULL, "") &&
		CALL("SRace", "CreateFlag", NULL, "");
}

/* Init(): both players wait for the count down 3, 2, 1, GO! (ControlToRacers then) */
static byte SRace_InitC(void) {
	static const char *const counts[] = { "2", "1", "GO!" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *s1 = Mode_V(&t), *v1 = Mode_V(&t), *t1 = Mode_V(&t);
	if (!Sm_Players("SRace", "Green") || !Sm_OnDeath("SRace") || !Sm_SetPl("SRace", 0, "UsrControl", Mode_I(&t, -1)) ||
		!Sm_SetPl("SRace", 1, "UsrControl", Mode_I(&t, -1))) {
		return Mode_Done(&t, 0);
	}
	double time = Mode_Time();
	Native_SetFloat(t1, time + 1.0);
	if (!Mode_G("SRace", "RaceCountDownSound1", s1) || !Mode_G("SRace", "RaceCountDownSound1Vol", v1) || !Mode_MsgText(Mode_S(&t, "3"), t1->f) ||
		!Mode_Play(s1, v1)) {
		return Mode_Done(&t, 0);
	}
	/* the functions SScorer.SetMsgText and SSound.Play as the modules hold them (OBJLIB 4) */
	PyObject *msg = Script_Attr("SScorer", "SetMsgText"), *play = msg ? Script_Attr("SSound", "Play") : NULL;
	byte ok = play != NULL;
	for (int i = 0; ok && i < 3; i++) {
		int mark = t.n;
		nativeVal_t *timy = Mode_V(&t), *args = Mode_V(&t), *s = Mode_V(&t), *v = Mode_V(&t);
		Native_SetFloat(timy, time + (double)(i + 1));
		Native_SetEmpty(args, NV_LIST);
		Native_SetStr(Native_Append(args), counts[i]);
		Native_SetFloat(Native_Append(args), timy->f + 1.0);
		ok = Mode_Schedule(timy, msg, args->items, 2, NULL);
		if (ok && i == 2) {
			PyObject *ctr = Mode_GlobalPy("SRace", "ControlToRacers");
			ok = ctr && Mode_Schedule(timy, ctr, NULL, 0, NULL);
			Py_XDECREF(ctr);
		}
		ok = ok && Mode_G("SRace", i == 2 ? "RaceCountDownSound2" : "RaceCountDownSound1", s) &&
			Mode_G("SRace", i == 2 ? "RaceCountDownSound2Vol" : "RaceCountDownSound1Vol", v);
		if (ok) {
			Native_Free(args);
			Native_SetEmpty(args, NV_LIST);
			Native_Copy(Native_Append(args), s);
			Native_Copy(Native_Append(args), v);
			ok = Mode_Schedule(timy, play, args->items, 2, NULL);
		}
		Mode_Release(&t, mark);
	}
	Py_XDECREF(play);
	Py_XDECREF(msg);
	return Mode_Done(&t, ok);
}

SM_NOARGS(SRace, BrowseAreas, SRace_BrowseAreasC())
SM_NOARGS(SRace, GetRndFlag, SRace_GetRndFlagC())
MODE_FN(SRace, TakeFlag, 2, SRace_TakeFlagC(&a[1]))
SM_NOARGS(SRace, CreateFlag, SRace_CreateFlagC())
MODE_FN(SRace, InitRacePos, 1, SRace_InitRacePosC(&a[0], out))
MODE_FN(SRace, GetReSpawnPos, 1, SRace_GetReSpawnPosC(&a[0], out))
SM_NOARGS(SRace, InitStatus, SRace_InitStatusC())
SM_NOARGS(SRace, Init, SRace_InitC())
SM_NOARGS(SRace, ControlToRacers, Sm_SetPl("SRace", 0, "UsrControl", &(nativeVal_t){ .type = NV_INT, .i = 0 }) &&
	Sm_SetPl("SRace", 1, "UsrControl", &(nativeVal_t){ .type = NV_INT, .i = 1 }))

PyMethodDef g_SRaceMethods[] = {
	{ "BrowseAreas", SRace_BrowseAreas, 1, NULL },
	{ "GetRndFlag", SRace_GetRndFlag, 1, NULL },
	{ "TakeFlag", SRace_TakeFlag, 1, NULL },
	{ "CreateFlag", SRace_CreateFlag, 1, NULL },
	{ "InitRacePos", SRace_InitRacePos, 1, NULL },
	{ "GetReSpawnPos", SRace_GetReSpawnPos, 1, NULL },
	{ "InitStatus", SRace_InitStatus, 1, NULL },
	{ "Init", SRace_Init, 1, NULL },
	{ "ControlToRacers", SRace_ControlToRacers, 1, NULL },
	{ "OnDeathFunc", Sm_Pass2, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
