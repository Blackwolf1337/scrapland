/*
 * The split screen in C (the scripts split/split.py and the four modes sdeathmatch.py, sflaghunt.py, spostman.py and
 * srace.py, issue #2): two players on one screen, the ship choice before a round, the scores, the pause and end menus
 * (data/menus/split.def: the scorer items and the keys), and the modes (the flag to hunt or carry, the race routes of
 * data/split/srace.def). The map tables, MortalProfiles, PlayersReady, Scorer, FirstTime, RespawnTime, ModLib,
 * OnDeathFunc and the spawn functions FuncGetSpawnPos/FuncGetReSpawnPos (the race replaces them) stay variables of
 * Split; ZoneList, Flag, AcFlag, BattonOwner, FlagInitPos, race, pl0, pl1 and the model and sound names variables of
 * the modes. The scripts start a split game by setting SplitScreen and loading a level (init.py preloads Split and
 * the mode of ServerType).
 */
#include "scrap/script.h"
#include "scrap/scrap.h"

static const char split_def[] = {
#embed "../../../data/menus/split.def"
	, 0 };

static const char srace_def[] = {
#embed "../../../data/split/srace.def"
	, 0 };

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

static const scriptStep_t split_steps[] = {
	{ NULL, NULL },
};

/* a section of split.def for player id with the values of vars (a dict given away, NULL for none): 0 with the
 * error set when a line raised */
static byte Split_Run(int id, const char *section, PyObject *vars) {
	byte ok = Script_RunFileTry(id, "menus/split.def", section, split_def, split_steps, vars);
	Py_XDECREF(vars);
	return ok;
}

/* fn(args) through the module global fn (args given away): the result, a new reference */
static PyObject *Split_CallG(const char *module, const char *fn, PyObject *args) {
	PyObject *f = Script_Global(module, fn);
	if (!f) {
		Py_XDECREF(args);
		return NULL;
	}
	PyObject *v = Script_Invoke(f, args);
	Py_DECREF(f);
	return v;
}

/* the same, the result dropped */
static byte Split_Do(const char *module, const char *fn, PyObject *args) {
	PyObject *v = Split_CallG(module, fn, args);
	Py_XDECREF(v);
	return v != NULL;
}

/* name[idx] = name[idx] + d of a module global list (Split.Scorer, Split.MortalProfiles) */
static byte Split_Add(const char *module, const char *name, PyObject *idx, long d) {
	PyObject *list = Script_Global(module, name), *v = list ? PyObject_GetItem(list, idx) : NULL;
	PyObject *dv = v ? PyInt_FromLong(d) : NULL, *n = dv ? PyNumber_Add(v, dv) : NULL;
	int bad = !n || PyObject_SetItem(list, idx, n) < 0;
	Py_XDECREF(n);
	Py_XDECREF(dv);
	Py_XDECREF(v);
	Py_XDECREF(list);
	return !bad;
}

/* name[idx] = v of a module global list (v given away) */
static byte Split_SetAt(const char *module, const char *name, PyObject *idx, PyObject *v) {
	PyObject *list = v ? Script_Global(module, name) : NULL;
	int bad = !list || PyObject_SetItem(list, idx, v) < 0;
	Py_XDECREF(list);
	Py_XDECREF(v);
	return !bad;
}

/* name[idx] of a module global list (a new reference) */
static PyObject *Split_At(const char *module, const char *name, PyObject *idx) {
	PyObject *list = Script_Global(module, name), *v = list ? PyObject_GetItem(list, idx) : NULL;
	Py_XDECREF(list);
	return v;
}

/* "<text>" + `n` (a new reference) */
static PyObject *Split_Name(const char *text, long n) {
	PyObject *i = PyInt_FromLong(n), *rep = i ? Script_Repr(i) : NULL, *v = rep ? Script_SPlus(text, rep) : NULL;
	Py_XDECREF(rep);
	Py_XDECREF(i);
	return v;
}

/* Scrap.GetLangStr(key) % (button,), the text alone when button is NULL (a new reference) */
static PyObject *Split_Lang(const char *key, const char *button) {
	PyObject *t = Script_LangOf(PyString_FromString((char *)key));
	if (!t || !button) {
		return t;
	}
	PyObject *a = Py_BuildValue("(s)", button), *v = a ? Script_Mod(t, a) : NULL;
	Py_XDECREF(a);
	Py_DECREF(t);
	return v;
}

/* (0, sp[1], 0) (a new reference) */
static PyObject *Split_Ang(PyObject *sp) {
	PyObject *a = Script_Index(sp, 1), *v = a ? Py_BuildValue("(iOi)", 0, a, 0) : NULL;
	Py_XDECREF(a);
	return v;
}

/* ---- Split ---- */

static PyObject *Split_PurgeServerMapList(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *keep = K(PyList_New(0)), *text = keep ? K(Script_Ask("Scrap", "Get", "(s)", "ServerMapList")) : NULL;
	PyObject *prev = text ? K(Script_EvalIn("Split", text)) : NULL, *modes = prev ? K(Script_Global("Split", "ModesMap")) : NULL;
	PyObject *type = modes ? K(Script_Ask("Scrap", "Get", "(s)", "ServerType")) : NULL;
	PyObject *valid = type ? K(PyObject_GetItem(modes, type)) : NULL, *a;
	if (!valid) {
		FAIL();
	}
	int more;
	for (int i = 0; (more = Script_Next(prev, i, &a)) > 0; i++) {
		int in = PySequence_Contains(valid, a);
		int bad = in < 0 || (in && PyList_Append(keep, a) < 0);
		Py_DECREF(a);
		if (bad) {
			FAIL();
		}
	}
	PyObject *rep = more == 0 ? K(Script_Repr(keep)) : NULL;
	return Script_Done(&r, rep && Script_Try("Scrap", "Set", "(sO)", "ServerMapList", rep) ? Py_ReturnNone() : NULL);
}

/* GetSpawnPos(id) and GetReSpawnPos(v): a spawn point of OutMap */
static PyObject *Split_GetSpawnPos(PyObject *self, PyObject *args) {
	PyObject *a;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &a) || !Script_Import((const char *const[]){ "OutMap", NULL })) {
		return NULL;
	}
	return Script_Ask("OutMap", "GetSpawnPos", "()");
}

/* CreateUserShip(id): Player<id> at a spawn point with its ship, and the camera Cam<id> following it */
static PyObject *Split_CreateUserShip(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Script_Import((const char *const[]){ "Vehicles", "OutMap", NULL })) {
		return NULL;
	}
	PyObject *sp = K(Split_CallG("Split", "FuncGetSpawnPos", Py_BuildValue("(O)", id)));
	PyObject *at = sp ? K(Script_Index(sp, 0)) : NULL, *x = at ? K(Script_Index(at, 0)) : NULL;
	PyObject *y = x ? K(Script_Index(at, 1)) : NULL, *z = y ? K(Script_Index(at, 2)) : NULL;
	PyObject *name = z ? K(Script_SPlus("Player", K(Script_Repr(id)))) : NULL;
	PyObject *v = name ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Car")) : NULL;
	if (!v || !Script_HSet(v, "Ang", Split_Ang(sp)) || !Split_Do("Split", "NextShip", Py_BuildValue("(O)", id))) {
		FAIL();
	}
	PyObject *cname = K(Script_SPlus("Cam", K(Script_Repr(id))));
	PyObject *cam = cname ? K(Script_Ask("Scrap", "CreateEntity", "(Oddds)", cname, 0.0, 0.0, 0.0, "FCam")) : NULL;
	PyObject *vn = cam && Script_HSet(cam, "camPos", Script_Index(sp, 0)) && Script_HSet(cam, "camAng", Split_Ang(sp)) ?
		K(Script_GetAttr(v, "Name")) : NULL;
	PyObject *cn = vn ? K(Script_GetAttr(cam, "Name")) : NULL;
	return Script_Done(&r, cn && Script_Try("Vehicles", "SetActiveCam", "(OOO)", vn, cn, id) ? Py_ReturnNone() : NULL);
}

/* Init(): the mode's state, both players with ships, the deaths through OnDeathCallback, the ship choice */
static PyObject *Split_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *mod = K(Script_Global("Split", "ModLib"));
	if (!mod || !K(Script_Method(mod, "InitStatus", "()")) || !Script_Try("Scrap", "Set", "(ss)", "MainMissionEvent", "") ||
		!Script_Import((const char *const[]){ "Menu", "Vehicles", NULL }) || !Split_Do("Split", "CreateUserShip", Py_BuildValue("(i)", 0)) ||
		!Split_Do("Split", "CreateUserShip", Py_BuildValue("(i)", 1))) {
		FAIL();
	}
	PyObject *cb = K(Script_Global("Split", "OnDeathCallback"));
	if (!cb) {
		FAIL();
	}
	Script_SetVar("Vehicles", "OnDeathCallback", Script_Ref(cb));
	return Script_Done(&r, Split_Do("Split", "ChooseShips", PyTuple_New(0)) ? Py_ReturnNone() : NULL);
}

/* ReSpawnShip(LooserName): back at a spawn point (the race: where it died) without upgrades */
static PyObject *Split_ReSpawnShip(PyObject *self, PyObject *args) {
	static const char *const upgrades[] = { "Upgrd00", "Upgrd01", "Upgrd02", "Upgrd03", "Upgrd04", "Upgrd05", "Upgrd06" };
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import((const char *const[]){ "Vehicles", "OutMap", NULL })) {
		return NULL;
	}
	PyObject *e = K(Script_Entity(name)), *sp = e ? K(Split_CallG("Split", "FuncGetReSpawnPos", Py_BuildValue("(O)", e))) : NULL;
	if (!sp || !Script_HSet(e, "Ang", Split_Ang(sp)) || !Script_HSet(e, "Pos", Script_Index(sp, 0))) {
		FAIL();
	}
	PyObject *zero = K(PyInt_FromLong(0));
	for (int i = 0; i < 7; i++) {
		if (!zero || !Script_HSet(e, upgrades[i], Script_Ref(zero))) {
			FAIL();
		}
	}
	return Script_Done(&r, Script_Try("Vehicles", "ReSpawnShip", "(O)", name) ? Py_ReturnNone() : NULL);
}

/* OnDeathCallback(LooserName, WinnerName): the respawn RespawnTime later, then the mode's OnDeathFunc */
static PyObject *Split_OnDeathCallback(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	PyObject *t = K(Script_Global("Split", "RespawnTime")), *f = t ? K(PyNumber_Float(t)) : NULL;
	if (!f || !Script_LaterIn(PyFloat_AsDouble(f), "Split", "ReSpawnShip", Py_BuildValue("(O)", looser), NULL)) {
		FAIL();
	}
	PyObject *cb = K(Script_Global("Split", "OnDeathFunc"));
	int set = cb ? Script_True(cb) : -1;
	if (set < 0 || (set && !K(Script_Invoke(cb, Py_BuildValue("(OO)", looser, winner))))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

static PyObject *Split_ChooseShips(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Script_Try("SInput", "SetActionSet", "(s)", "Inactive") && Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "") &&
		Split_Do("Split", "InitChooseScorer", Py_BuildValue("(i)", 0)) &&
		Split_Do("Split", "InitChooseScorer", Py_BuildValue("(i)", 1)) ? Py_ReturnNone() : NULL;
}

/* ShipMaker.PilotsList[int(Scrap.Rand(0, len(ShipMaker.PilotsList)))] (a new reference) */
static PyObject *Split_RandomPilot(void) {
	PyObject *pilots = Script_Attr("ShipMaker", "PilotsList");
	int n = pilots ? PyObject_Length(pilots) : -1;
	PyObject *p = n >= 0 ? Script_Index(pilots, (int)Scrap_RandRange(0, n)) : NULL;
	Py_XDECREF(pilots);
	return p;
}

/* the ship of hangar profile p for player name when it has an engine: 1 done, 0 no such ship, -1 error */
static int Split_TryProfile(PyObject *name, PyObject *profile) {
	scriptRefs_t r = { .n = 0 };
	PyObject *var = K(Script_SPlus("HangarShip", K(Script_Repr(profile))));
	PyObject *attribs = var ? K(Script_Ask("Scrap", "Get", "(O)", var)) : NULL;
	if (!attribs) {
		return Script_Done(&r, NULL), -1;
	}
	if (Script_Is(attribs, "")) {
		return Script_Done(&r, NULL), 0;
	}
	PyObject *a = K(Script_Ask("string", "split", "(Os)", attribs, "<-")), *it[7] = { NULL };
	int any = 0;
	for (int i = 1; a && i <= 4 && !any; i++) {
		PyObject *v = K(Script_Index(a, i));
		if (!v) {
			return Script_Done(&r, NULL), -1;
		}
		any = !Script_Is(v, "");
	}
	if (!a) {
		return Script_Done(&r, NULL), -1;
	}
	if (!any) {
		return Script_Done(&r, NULL), 0;
	}
	PyObject *v = K(Script_Entity(name));
	int none = v ? Script_Eq(v, Py_None) : -1;
	if (none < 0 || (!none && !Script_HSet(v, "Active", PyInt_FromLong(0)))) {
		return Script_Done(&r, NULL), -1;
	}
	for (int i = 0; i < 7; i++) {
		if (!(it[i] = K(Script_Index(a, i)))) {
			return Script_Done(&r, NULL), -1;
		}
	}
	PyObject *life = K(Script_Int(it[6])), *pilot = life ? K(Split_RandomPilot()) : NULL;
	PyObject *done = pilot ? K(Script_Ask("Vehicles", "SetProperties", "(OOOOOOOOOOi)", name, it[0], life, pilot, it[1], it[2], it[3],
		it[4], it[5], name, 1)) : NULL;
	if (!done || (!none && !Script_HSet(v, "Active", PyInt_FromLong(1)))) {
		return Script_Done(&r, NULL), -1;
	}
	Script_Done(&r, NULL);
	return 1;
}

/* ChangeShip(id, Up=1): the next (or previous) hangar ship of the profiles 1 to 9 with an engine, else a SMerc1 */
static PyObject *Split_ChangeShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "Up", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *up = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &id, &up) ||
		!Script_Import((const char *const[]){ "string", "ShipMaker", "Vehicles", NULL })) {
		return NULL;
	}
	int u = up ? Script_True(up) : 1;
	PyObject *name = K(Script_SPlus("Player", K(Script_Repr(id))));
	if (u < 0 || !name) {
		FAIL();
	}
	for (int n = 0; n < 9; n++) {
		if (!Split_Add("Split", "MortalProfiles", id, u ? 1 : -1)) {
			FAIL();
		}
		PyObject *prof = K(Split_At("Split", "MortalProfiles", id));
		int wrap = prof ? Script_NumCmp(prof, u ? 9 : 1) : -2;
		if (wrap == -2 || ((u ? wrap > 0 : wrap < 0) && !Split_SetAt("Split", "MortalProfiles", id, PyInt_FromLong(u ? 1 : 9)))) {
			FAIL();
		}
		prof = K(Split_At("Split", "MortalProfiles", id));
		int got = prof ? Split_TryProfile(name, prof) : -1;
		if (got < 0) {
			FAIL();
		}
		if (got) {
			return Script_Done(&r, Py_ReturnNone());
		}
	}
	PyObject *pilot = K(Split_RandomPilot());
	return Script_Done(&r, pilot && Script_Try("Vehicles", "SetStdProperties", "(OsO)", name, "SMerc1", pilot) ? Py_ReturnNone() : NULL);
}

static PyObject *Split_NextShip(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) && Split_Do("Split", "ChangeShip", Py_BuildValue("(Oi)", id, 1)) ? Py_ReturnNone() : NULL;
}

static PyObject *Split_PrevShip(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) && Split_Do("Split", "ChangeShip", Py_BuildValue("(Oi)", id, 0)) ? Py_ReturnNone() : NULL;
}

/* the keys of both players off (StartGame, EndOfGame bind them per player) */
static byte Split_KeysOff(int id) {
	return Split_Run(id, "keys_off", NULL);
}

/* StartGame(id=0): the round starts (the mode's Init the first time), the scores and separators */
static PyObject *Split_StartGame(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id) || !Script_Import((const char *const[]){ "Menu", NULL }) ||
		!Split_KeysOff(0) || !Split_KeysOff(1) || !Script_Try("SScorer", "Clear", "(i)", 0) || !Script_Try("SScorer", "Clear", "(i)", 1) ||
		!Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "Split.PauseGame") || !Script_Try("SInput", "SetActionSet", "(s)", "Racer") ||
		!Script_Try("Menu", "DrawScorer", "(is)", 0, "Racer")) {
		return NULL;
	}
	PyObject *first = Script_Global("Split", "FirstTime");
	int f = first ? Script_True(first) : -1;
	Py_XDECREF(first);
	if (f < 0) {
		return NULL;
	}
	if (f) {
		PyObject *mod = Script_Global("Split", "ModLib"), *v = mod ? Script_Method(mod, "Init", "()") : NULL;
		Py_XDECREF(mod);
		if (!v) {
			return NULL;
		}
		Py_DECREF(v);
		PyObject *msg = Script_SetGlobal("Split", "FirstTime", PyInt_FromLong(0)) ? PyString_FromString("Cu\xf1" "aaaao") : NULL;
		byte ok = msg && Script_Print(&msg, 1);
		Py_XDECREF(msg);
		if (!ok) {
			return NULL;
		}
	}
	if (!Script_Try("Scrap", "SetTimeSpeed", "(i)", 1)) {
		return NULL;
	}
	for (int i = 0; i < 2; i++) {
		if (!Split_Do("Split", "MissionScorer", Py_BuildValue("(i)", i)) || !Split_Do("Split", "DefaultSplitScorer", Py_BuildValue("(i)", i))) {
			return NULL;
		}
	}
	return Py_ReturnNone();
}

/* DefaultSplitScorer(id): the line between the halves */
static PyObject *Split_DefaultSplitScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *name = Split_Name("SplitSeparator", id);
	byte ok = name && Split_Run(id, "separator", Py_BuildValue("{s:O,s:i}", "name", name, "y", 239 + id));
	Py_XDECREF(name);
	return ok ? Py_ReturnNone() : NULL;
}

/* the menu texts of player id from row 70, 22 apart (textorl0..2), then the black band */
static byte Split_Texts(int id, PyObject *const *texts, int n) {
	for (int i = 0; i < n; i++) {
		PyObject *obj = Split_Name("textorl", i);
		byte ok = obj && Split_Do("Split", "LittleText", Py_BuildValue("(iOOi)", id, obj, texts[i], 70 + i * 22));
		Py_XDECREF(obj);
		if (!ok) {
			return 0;
		}
	}
	return Split_Do("Split", "BlackBack", Py_BuildValue("(i)", id));
}

/* EndOfGame(id): player id won; the result for both, the next map 5 s later */
static PyObject *Split_EndOfGame(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Script_Try("SInput", "SetActionSet", "(s)", "Inactive") ||
		!Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "")) {
		return NULL;
	}
	int second = Script_NumCmp(id, 1);
	if (second == -2) {
		return NULL;
	}
	const char *results[2] = { "XBox_Split_You_Win", "XBox_Split_You_Loose" };
	if (second == 0) {
		results[0] = "XBox_Split_You_Loose";
		results[1] = "XBox_Split_You_Win";
	}
	for (int i = 0; i < 2; i++) {
		PyObject *obj = K(Split_Name("Pl", i)), *text = obj && Script_Try("SScorer", "Clear", "(i)", i) ? K(Split_Lang(results[i], NULL)) : NULL;
		if (!text || !Split_Do("Split", "LittleText", Py_BuildValue("(iOOi)", i, obj, text, 100)) ||
			!Split_Do("Split", "BlackBack", Py_BuildValue("(i)", i)) || !Split_KeysOff(i)) {
			FAIL();
		}
	}
	return Script_Done(&r, Script_LaterIn(5.0, "Split", "ReloadNext", PyTuple_New(0), NULL) ? Py_ReturnNone() : NULL);
}

/* ReloadNext(): the next map of the server list (or the same) */
static PyObject *Split_ReloadNext(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Init", "SaveGame", NULL }) || !Split_Do("Split", "PurgeServerMapList", PyTuple_New(0))) {
		return NULL;
	}
	PyObject *text = K(Script_Ask("Scrap", "Get", "(s)", "ServerMapList")), *maps = text ? K(Script_EvalIn("Split", text)) : NULL;
	int any = maps ? Script_True(maps) : -1;
	int n = any > 0 ? PyObject_Length(maps) : 0;
	PyObject *path = any >= 0 && n >= 0 ? K(Script_Attr("Init", "Path")) : NULL, *next = NULL;
	if (!path) {
		FAIL();
	}
	if (!any || n == 0) {
		next = path;
	} else {
		PyObject *here = K(PySequence_GetSlice(path, 7, 0x7fffffff)), *map = NULL;
		int in = here ? PySequence_Contains(maps, here) : -1;
		if (in > 0) {
			PyObject *at = K(Script_Method(maps, "index", "(O)", here)), *one = at ? K(PyInt_FromLong(1)) : NULL, *nx = one ? K(PyNumber_Add(at, one)) : NULL;
			PyObject *len = nx ? K(PyInt_FromLong(n)) : NULL, *k = len ? K(PyNumber_Remainder(nx, len)) : NULL;
			map = k ? K(PyObject_GetItem(maps, k)) : NULL;
		} else if (in == 0) {
			map = K(Script_Index(maps, 0));
		}
		next = map ? K(Script_SPlus("Levels/", map)) : NULL;
	}
	return Script_Done(&r, next && Script_Try("SaveGame", "QLoad", "(O)", next) ? Py_ReturnNone() : NULL);
}

/* ActualizeScorer(): a player at FragLimit wins, else the scores are shown */
static PyObject *Split_ActualizeScorer(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	for (int i = 0; i < 2; i++) {
		PyObject *sc = K(Script_Global("Split", "Scorer")), *v = sc ? K(Script_Index(sc, i)) : NULL;
		PyObject *limit = v ? K(Script_Ask("Scrap", "Get", "(s)", "FragLimit")) : NULL;
		int c = limit ? PyObject_Compare(v, limit) : 0;
		if (!limit || PyErr_Occurred()) {
			FAIL();
		}
		if (c >= 0) {
			return Script_Done(&r, Split_Do("Split", "EndOfGame", Py_BuildValue("(i)", i)) ? Py_ReturnNone() : NULL);
		}
	}
	for (int i = 0; i < 2; i++) {
		PyObject *sc = K(Script_Global("Split", "Scorer")), *v = sc ? K(Script_Index(sc, i)) : NULL, *rep = v ? K(Script_Repr(v)) : NULL;
		if (!rep || !Script_Try("SScorer", "Set", "(issO)", i, "MissionBest", "Text", rep)) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* MissionScorer(id): the score, the limit and the tab of player id, then ActualizeScorer */
static PyObject *Split_MissionScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id) || !Script_Import((const char *const[]){ "Scorer", "Menu", NULL })) {
		return NULL;
	}
	PyObject *limit = Script_Ask("Scrap", "Get", "(s)", "FragLimit"), *rep = limit ? Script_Repr(limit) : NULL;
	byte ok = rep && Split_Run(id, "mission", Py_BuildValue("{s:i,s:O}", "y", 190 + 240 * id, "limit", rep)) &&
		Split_Do("Split", "ActualizeScorer", PyTuple_New(0));
	Py_XDECREF(rep);
	Py_XDECREF(limit);
	return ok ? Py_ReturnNone() : NULL;
}

/* SeleShip(id): player id is ready; the round starts when both are */
static PyObject *Split_SeleShip(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *list = Script_Global("Split", "PlayersReady"), *one = list ? PyInt_FromLong(1) : NULL;
	int bad = !one || PyObject_SetItem(list, id, one) < 0;
	int both = 1;
	for (int i = 0; !bad && i < 2 && both; i++) {
		PyObject *v = Script_Index(list, i);
		int c = v ? Script_NumCmp(v, 1) : -2;
		Py_XDECREF(v);
		bad = c == -2;
		both = c == 0;
	}
	Py_XDECREF(one);
	Py_XDECREF(list);
	if (bad) {
		return NULL;
	}
	byte ok = both ? Split_Do("Split", "StartGame", PyTuple_New(0)) : Split_Do("Split", "ReadyToStartScorer", Py_BuildValue("(O)", id));
	return ok ? Py_ReturnNone() : NULL;
}

/* PlayersReady[id] = 0 */
static byte Split_NotReady(PyObject *id) {
	PyObject *list = Script_Global("Split", "PlayersReady"), *zero = list ? PyInt_FromLong(0) : NULL;
	int bad = !zero || PyObject_SetItem(list, id, zero) < 0;
	Py_XDECREF(zero);
	Py_XDECREF(list);
	return !bad;
}

/* InitChooseScorer(id): the ship choice of player id */
static PyObject *Split_InitChooseScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Split_NotReady(id)) {
		return NULL;
	}
	long i = PyInt_AsLong(id);
	if (PyErr_Occurred() || !Split_Run((int)i, "keys_choose", NULL) || !Script_Try("SScorer", "Clear", "(O)", id)) {
		return NULL;
	}
	PyObject *t[3];
	t[0] = Split_Lang("XBox_Split_SelectShip", NULL);
	t[1] = t[0] ? Split_Lang("XBox_Split_Start", "\x14") : NULL;
	t[2] = t[1] ? Split_Lang("XBox_Split_Abort", "\x17") : NULL;
	byte ok = t[2] && Split_Texts((int)i, t, 3);
	Py_XDECREF(t[0]);
	Py_XDECREF(t[1]);
	Py_XDECREF(t[2]);
	return ok ? Py_ReturnNone() : NULL;
}

/* ReadyToStartScorer(id): player id waits for the other */
static PyObject *Split_ReadyToStartScorer(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id) || !Script_Try("SScorer", "Clear", "(i)", id)) {
		return NULL;
	}
	PyObject *wait = K(Split_Lang("XBox_Split_Waiting", NULL));
	if (!wait || !Split_Do("Split", "LittleText", Py_BuildValue("(isOi)", id, "ReadyControl", wait, 74))) {
		FAIL();
	}
	PyObject *back = K(Split_Lang("XBox_Split_Back", "\x17"));
	byte ok = back && Split_Do("Split", "LittleText", Py_BuildValue("(isOi)", id, "PresToAbort", back, 124)) &&
		Split_Do("Split", "BlackBack", Py_BuildValue("(i)", id)) && Split_Run(id, "keys_ready", NULL);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* ConfirmAbortMain(id), ConfirmAbortPause(id), ConfirmRestartPause(id): the questions of the menus */
static PyObject *Split_ConfirmAbortMain(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) &&
		Split_Do("Split", "ConfirmAbort", Py_BuildValue("(Os)", id, "Split.InitChooseScorer")) ? Py_ReturnNone() : NULL;
}

static PyObject *Split_ConfirmAbortPause(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) &&
		Split_Do("Split", "ConfirmAbort", Py_BuildValue("(Os)", id, "Split.PauseGameMenu")) ? Py_ReturnNone() : NULL;
}

static PyObject *Split_ConfirmRestartPause(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *text = Split_Lang("XBox_Split_RestartMatch", NULL);
	byte ok = text && Split_Do("Split", "ConfirmAbort", Py_BuildValue("(OssO)", id, "Split.PauseGameMenu", "Split.Restart", text));
	Py_XDECREF(text);
	return ok ? Py_ReturnNone() : NULL;
}

/* ConfirmAbort(id, event, Execute="Split.Abort", TXT=None): yes runs Execute, no runs event */
static PyObject *Split_ConfirmAbort(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "event", "Execute", "TXT", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *event, *exec = NULL, *txt = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OO", names, &id, &event, &exec, &txt)) {
		return NULL;
	}
	if (!exec) {
		exec = K(PyString_FromString("Split.Abort"));
	}
	int none = txt ? Script_Eq(txt, Py_None) : 1;
	if (none < 0) {
		FAIL();
	}
	if (none) {
		txt = K(Split_Lang("XBox_Split_Exit", NULL));
	}
	long i = txt ? PyInt_AsLong(id) : 0;
	if (!txt || PyErr_Occurred() || !Split_Run((int)i, "keys_confirm", Py_BuildValue("{s:O,s:O}", "exec", exec, "event", event)) ||
		!Script_Try("SScorer", "Clear", "(O)", id)) {
		FAIL();
	}
	PyObject *t[3] = { txt, K(Split_Lang("XBox_Split_Yes", "\x14")), NULL };
	t[2] = t[1] ? K(Split_Lang("XBox_Split_No", "\x15")) : NULL;
	return Script_Done(&r, t[2] && Split_Texts((int)i, t, 3) ? Py_ReturnNone() : NULL);
}

/* Abort(id): split screen off, back to the menu level */
static PyObject *Split_Abort(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) && Script_Import((const char *const[]){ "SaveGame", NULL }) &&
		Script_Try("Scrap", "Set", "(si)", "r_SplitScreen", 0) && Script_Try("Scrap", "Set", "(si)", "SplitScreen", 0) &&
		Script_Try("Scrap", "Set", "(sd)", "CameraDefViewAspect", 1.3333) &&
		Script_Try("SaveGame", "QLoad", "(s)", "Levels/Menu") ? Py_ReturnNone() : NULL;
}

/* Restart(id): the same map again */
static PyObject *Split_Restart(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Script_Import((const char *const[]){ "SaveGame", "Init", NULL })) {
		return NULL;
	}
	PyObject *path = Script_Attr("Init", "Path");
	byte ok = path && Script_Try("SaveGame", "QLoad", "(O)", path);
	Py_XDECREF(path);
	return ok ? Py_ReturnNone() : NULL;
}

/* PauseGame(id): the time stops, both get the pause menu */
static PyObject *Split_PauseGame(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	return PyArg_ParseTuple(args, "O", &id) && Script_Try("Scrap", "SetTimeSpeed", "(i)", 0) &&
		Script_Try("SInput", "SetActionSet", "(s)", "Inactive") && Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "") &&
		Split_Do("Split", "PauseGameMenu", Py_BuildValue("(i)", 0)) &&
		Split_Do("Split", "PauseGameMenu", Py_BuildValue("(i)", 1)) ? Py_ReturnNone() : NULL;
}

/* PauseGameMenu(id): restart, abort or back to the round */
static PyObject *Split_PauseGameMenu(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Split_NotReady(id)) {
		return NULL;
	}
	long i = PyInt_AsLong(id);
	if (PyErr_Occurred() || !Split_Run((int)i, "keys_pause", NULL) || !Script_Try("SScorer", "Clear", "(O)", id)) {
		return NULL;
	}
	PyObject *t[3];
	t[0] = Split_Lang("XBox_Split_Restart", "\x14");
	t[1] = t[0] ? Split_Lang("XBox_Split_Abort", "\x17") : NULL;
	t[2] = t[1] ? Split_Lang("XBox_Split_Return", NULL) : NULL;
	byte ok = t[2] && Split_Texts((int)i, t, 3);
	Py_XDECREF(t[0]);
	Py_XDECREF(t[1]);
	Py_XDECREF(t[2]);
	return ok ? Py_ReturnNone() : NULL;
}

/* LittleText(id, objName, Text, Pos): a centred line of a menu in the half of player id */
static PyObject *Split_LittleText(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *name, *text, *pos;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &id, &name, &text, &pos) || !Script_Import((const char *const[]){ "Scorer", NULL })) {
		return NULL;
	}
	int lower = Script_NumCmp(id, 0);
	PyObject *delta = lower == -2 ? NULL : K(PyInt_FromLong(lower == 0 ? 0 : 240));
	long i = delta ? PyInt_AsLong(id) : 0;
	if (!delta || PyErr_Occurred() || !Script_Try("SScorer", "Add", "(OOs)", id, name, "Text") ||
		!Script_Try("SScorer", "Set", "(OOsO)", id, name, "Text", text)) {
		FAIL();
	}
	PyObject *color = K(Script_Attr("Scorer", "MissionScorerText_Color")), *c[4] = { NULL };
	for (int k = 0; color && k < 4; k++) {
		if (!(c[k] = K(Script_Index(color, k)))) {
			FAIL();
		}
	}
	PyObject *y = c[3] ? K(PyNumber_Add(delta, pos)) : NULL;
	byte ok = y && Split_Run((int)i, "little", Py_BuildValue("{s:O,s:O,s:O,s:O,s:O,s:O}", "name", name, "r", c[0], "g", c[1], "b", c[2],
		"a", c[3], "y", y));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* BlackBack(id): the dark band behind the menu lines */
static PyObject *Split_BlackBack(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	int lower = Script_NumCmp(id, 0);
	long i = lower == -2 ? 0 : PyInt_AsLong(id);
	if (lower == -2 || PyErr_Occurred()) {
		return NULL;
	}
	return Split_Run((int)i, "black", Py_BuildValue("{s:i}", "y", lower == 0 ? 50 : 290)) ? Py_ReturnNone() : NULL;
}

PyMethodDef g_SplitMethods[] = {
	{ "PurgeServerMapList", Split_PurgeServerMapList, 1, NULL },
	{ "GetSpawnPos", Split_GetSpawnPos, 1, NULL },
	{ "CreateUserShip", Split_CreateUserShip, 1, NULL },
	{ "Init", Split_Init, 1, NULL },
	{ "GetReSpawnPos", Split_GetSpawnPos, 1, NULL },
	{ "ReSpawnShip", Split_ReSpawnShip, 1, NULL },
	{ "OnDeathCallback", Split_OnDeathCallback, 1, NULL },
	{ "ChooseShips", Split_ChooseShips, 1, NULL },
	{ "ChangeShip", (PyCFunction)(void (*)(void))Split_ChangeShip, 3, NULL },
	{ "NextShip", Split_NextShip, 1, NULL },
	{ "PrevShip", Split_PrevShip, 1, NULL },
	{ "StartGame", (PyCFunction)(void (*)(void))Split_StartGame, 3, NULL },
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
	{ "ConfirmAbort", (PyCFunction)(void (*)(void))Split_ConfirmAbort, 3, NULL },
	{ "Abort", Split_Abort, 1, NULL },
	{ "Restart", Split_Restart, 1, NULL },
	{ "PauseGame", Split_PauseGame, 1, NULL },
	{ "PauseGameMenu", Split_PauseGameMenu, 1, NULL },
	{ "LittleText", Split_LittleText, 1, NULL },
	{ "BlackBack", Split_BlackBack, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the modes: SDeathMatch, SFlagHunt, SPostman, SRace ---- */

/* `global pl0, pl1; pl0 = Scrap.GetEntity("Player0"); pl1 = ...` and the TgType of both */
static byte Mode_Players(const char *mod, const char *tg) {
	PyObject *p0 = Script_Ask("Scrap", "GetEntity", "(s)", "Player0");
	if (!p0 || !Script_SetGlobal(mod, "pl0", p0)) {
		return 0;
	}
	PyObject *p1 = Script_Ask("Scrap", "GetEntity", "(s)", "Player1");
	if (!p1 || !Script_SetGlobal(mod, "pl1", p1)) {
		return 0;
	}
	return Script_HSet(p0, "TgType", PyString_FromString((char *)tg)) && Script_HSet(p1, "TgType", PyString_FromString((char *)tg));
}

/* import Split; Split.OnDeathFunc = OnDeathFunc (the mode's) */
static byte Mode_OnDeath(const char *mod) {
	PyObject *f = Script_Import((const char *const[]){ "Split", NULL }) ? Script_Global(mod, "OnDeathFunc") : NULL;
	if (!f) {
		return 0;
	}
	Script_SetVar("Split", "OnDeathFunc", f);
	return 1;
}

/* pl<i>.var = v of the mode (v given away) */
static byte Mode_SetPl(const char *mod, int i, const char *var, PyObject *v) {
	PyObject *pl = Script_Global(mod, i ? "pl1" : "pl0");
	byte ok = pl && Script_HSet(pl, var, v);
	if (!pl) {
		Py_XDECREF(v);
	}
	Py_XDECREF(pl);
	return ok;
}

/* the score of the player called name (Player0, Player1) changes by d: 1, 0 for another name, -1 error */
static int Mode_Score(PyObject *name, long d) {
	int i = Script_Is(name, "Player1") ? 1 : Script_Is(name, "Player0") ? 0 : -1;
	if (i < 0) {
		return 0;
	}
	PyObject *idx = PyInt_FromLong(i);
	byte ok = idx && Split_Add("Split", "Scorer", idx, d);
	Py_XDECREF(idx);
	return ok ? 1 : -1;
}

/* `global ZoneList; ZoneList = []` and the positions of the dummies of pattern */
static byte Mode_Browse(const char *mod, const char *pattern) {
	PyObject *zones = PyList_New(0);
	if (!zones || !Script_SetGlobal(mod, "ZoneList", zones) || !Script_Try("Scrap", "StartDummySearch", "(si)", pattern, 1)) {
		return 0;
	}
	for (;;) {
		PyObject *d = Script_Ask("Scrap", "NextDummySearch", "()");
		int more = d ? Script_True(d) : -1;
		PyObject *list = more > 0 ? Script_Global(mod, "ZoneList") : NULL, *pos = list ? Script_Index(d, 1) : NULL;
		int bad = more < 0 || (more > 0 && (!pos || PyList_Append(list, pos) < 0));
		Py_XDECREF(pos);
		Py_XDECREF(list);
		Py_XDECREF(d);
		if (bad || !more) {
			return !bad;
		}
	}
}

/* GetRndFlag() of SFlagHunt and SPostman: a random zone, the next one when it is the current */
static PyObject *Mode_RndFlag(const char *mod) {
	scriptRefs_t r = { .n = 0 };
	PyObject *zones = K(Script_Global(mod, "ZoneList"));
	int n = zones ? PyObject_Length(zones) : -1;
	if (n < 0) {
		FAIL();
	}
	PyObject *niu = K(PyInt_FromLong((long)Scrap_RandRange(0, n))), *ac = niu ? K(Script_Global(mod, "AcFlag")) : NULL;
	int same = ac ? Script_Eq(niu, ac) : -1;
	if (same < 0) {
		FAIL();
	}
	PyObject *v = NULL;
	if (!same) {
		v = Script_Ref(niu);
	} else {
		PyObject *one = K(PyInt_FromLong(1)), *s = K(PyNumber_Add(ac, one)), *len = s ? K(PyInt_FromLong(n)) : NULL;
		v = len ? PyNumber_Remainder(s, len) : NULL;
	}
	return Script_Done(&r, v && Script_SetGlobal(mod, "AcFlag", v) ? Py_ReturnNone() : NULL);
}

/* Flag.var = v of the mode (v given away) */
static byte Mode_FlagSet(const char *mod, const char *var, PyObject *v) {
	PyObject *flag = Script_Global(mod, "Flag");
	byte ok = flag && Script_HSet(flag, var, v);
	if (!flag) {
		Py_XDECREF(v);
	}
	Py_XDECREF(flag);
	return ok;
}

/* Flag.method(arg) of the mode */
static byte Mode_FlagCall(const char *mod, const char *method, PyObject *arg) {
	PyObject *flag = Script_Global(mod, "Flag"), *v = flag && arg ? Script_Method(flag, method, "(O)", arg) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(flag);
	return v != NULL;
}

/* ZoneList[AcFlag] of the mode (a new reference) */
static PyObject *Mode_Zone(const char *mod) {
	PyObject *zones = Script_Global(mod, "ZoneList"), *ac = zones ? Script_Global(mod, "AcFlag") : NULL;
	PyObject *z = ac ? PyObject_GetItem(zones, ac) : NULL;
	Py_XDECREF(ac);
	Py_XDECREF(zones);
	return z;
}

/* Scrap.Set(var, Flag.Name) */
static byte Mode_WayToFlag(const char *mod, const char *var) {
	PyObject *flag = Script_Global(mod, "Flag"), *name = flag ? Script_GetAttr(flag, "Name") : NULL;
	byte ok = name && Script_Try("Scrap", "Set", "(sO)", var, name);
	Py_XDECREF(name);
	Py_XDECREF(flag);
	return ok;
}

/* `global Flag; Flag = Scrap.CreateEntity("FlagToBeHunted", 0, 0, 0, "Item")` with the model of the module variable
 * model and the radius */
static byte Mode_NewFlag(const char *mod, const char *model, long radius) {
	PyObject *flag = Script_Ask("Scrap", "CreateEntity", "(siiis)", "FlagToBeHunted", 0, 0, 0, "Item");
	if (!flag || !Script_SetGlobal(mod, "Flag", flag)) {
		return 0;
	}
	PyObject *file = Script_Global(mod, model);
	byte ok = file && Mode_FlagCall(mod, "SetModel", file) && Mode_FlagSet(mod, "Radius", PyInt_FromLong(radius)) &&
		Mode_FlagSet(mod, "RadarShow", PyInt_FromLong(1));
	Py_XDECREF(file);
	return ok;
}

/* SSound.Play(Sound.WaySound, Sound.WayVolume) */
static byte Mode_WaySound(void) {
	PyObject *s = Script_Attr("Sound", "WaySound"), *v = s ? Script_Attr("Sound", "WayVolume") : NULL;
	byte ok = v && Script_Try("SSound", "Play", "(OO)", s, v);
	Py_XDECREF(v);
	Py_XDECREF(s);
	return ok;
}

static PyObject *Mode_Pass(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

static PyObject *Mode_Pass2(PyObject *self, PyObject *args) {
	PyObject *a, *b;
	(void)self;
	return PyArg_ParseTuple(args, "OO", &a, &b) ? Py_ReturnNone() : NULL;
}

/* ---- SDeathMatch ---- */

static PyObject *SDeathMatch_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Players("SDeathMatch", "Killer") && Mode_SetPl("SDeathMatch", 0, "Target", PyString_FromString("Player1")) &&
		Mode_SetPl("SDeathMatch", 1, "Target", PyString_FromString("Player0")) && Mode_OnDeath("SDeathMatch") ? Py_ReturnNone() : NULL;
}

/* OnDeathFunc(LooserName, WinnerName): the killer scores, a suicide costs a point */
static PyObject *SDeathMatch_OnDeathFunc(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner) || !Script_Import((const char *const[]){ "Split", NULL })) {
		return NULL;
	}
	int self_ = Script_Is(winner, "") ? 1 : Script_Eq(winner, looser);
	int s = self_ < 0 ? -1 : self_ ? Mode_Score(looser, -1) : Mode_Score(winner, 1);
	return s >= 0 && Script_Try("Split", "ActualizeScorer", "()") ? Py_ReturnNone() : NULL;
}

PyMethodDef g_SDeathMatchMethods[] = {
	{ "InitStatus", Mode_Pass, 1, NULL },
	{ "Init", SDeathMatch_Init, 1, NULL },
	{ "OnDeathFunc", SDeathMatch_OnDeathFunc, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- SFlagHunt ---- */

static PyObject *SFlagHunt_BrowseAreas(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Browse("SFlagHunt", "DM_Multiplayer_Flag_*") ? Py_ReturnNone() : NULL;
}

static PyObject *SFlagHunt_GetRndFlag(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_RndFlag("SFlagHunt");
}

/* TakeFlag(ItemName, VehicleName): a player scores, the flag goes to another zone */
static PyObject *SFlagHunt_TakeFlag(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &vehicle) || !Script_Import((const char *const[]){ "Split", "Sound", NULL })) {
		return NULL;
	}
	int doit = Mode_Score(vehicle, 1);
	byte ok = doit == 0 || (doit > 0 && Mode_WaySound() && Script_Try("SFlagHunt", "GetRndFlag", "()") &&
		Mode_FlagSet("SFlagHunt", "Pos", Mode_Zone("SFlagHunt")) && Script_Try("Split", "ActualizeScorer", "()"));
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *SFlagHunt_CreateFlag(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_NewFlag("SFlagHunt", "FlagModel", 2000) && Script_Try("SFlagHunt", "GetRndFlag", "()") &&
		Mode_FlagSet("SFlagHunt", "OnTake", PyString_FromString("SFlagHunt.TakeFlag")) &&
		Mode_FlagSet("SFlagHunt", "Pos", Mode_Zone("SFlagHunt")) && Mode_WayToFlag("SFlagHunt", "CurrentWaypointName") ?
		Py_ReturnNone() : NULL;
}

static PyObject *SFlagHunt_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Script_Try("SFlagHunt", "BrowseAreas", "()") && Script_Try("SFlagHunt", "CreateFlag", "()") ? Py_ReturnNone() : NULL;
}

static PyObject *SFlagHunt_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Players("SFlagHunt", "Killer") && Mode_OnDeath("SFlagHunt") ? Py_ReturnNone() : NULL;
}

/* OnDeathFunc(LooserName, WinnerName): a death costs a point */
static PyObject *SFlagHunt_OnDeathFunc(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner) || !Script_Import((const char *const[]){ "Split", NULL })) {
		return NULL;
	}
	return Mode_Score(looser, -1) >= 0 && Script_Try("Split", "ActualizeScorer", "()") ? Py_ReturnNone() : NULL;
}

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

static PyObject *SPostman_BrowseAreas(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Mode_Browse("SPostman", "DM_Domination_Zone_*") || !Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Domination_Flag", 1)) {
		return NULL;
	}
	PyObject *d = Script_Ask("Scrap", "NextDummySearch", "()"), *pos = d ? Script_Index(d, 1) : NULL;
	Py_XDECREF(d);
	return pos && Script_SetGlobal("SPostman", "FlagInitPos", pos) ? Py_ReturnNone() : NULL;
}

static PyObject *SPostman_GetRndFlag(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_RndFlag("SPostman");
}

/* TakeArea(ItemName, VehicleName): the carrier reaches the zone and scores, the zone moves */
static PyObject *SPostman_TakeArea(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *item, *vehicle;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &vehicle) || !Script_Import((const char *const[]){ "Split", "Sound", NULL })) {
		return NULL;
	}
	PyObject *owner = K(Script_Global("SPostman", "BattonOwner"));
	int carrier = owner ? Script_Eq(vehicle, owner) : -1;
	if (carrier <= 0) {
		return Script_Done(&r, carrier < 0 ? NULL : Py_ReturnNone());
	}
	if (!Script_SetGlobal("SPostman", "BattonOwner", Script_Ref(vehicle)) || !Mode_WaySound() ||
		!Script_Try("SPostman", "GetRndFlag", "()") || !Mode_FlagSet("SPostman", "Pos", Mode_Zone("SPostman"))) {
		FAIL();
	}
	PyObject *tail = K(PySequence_GetSlice(vehicle, 6, 0x7fffffff)), *idx = tail ? K(Script_Int(tail)) : NULL;
	return Script_Done(&r, idx && Split_Add("Split", "Scorer", idx, 1) && Script_Try("Split", "ActualizeScorer", "()") ? Py_ReturnNone() : NULL);
}

/* TakeFlag(ItemName, VehicleName): a player takes the data pack and has to bring it to the zone */
static PyObject *SPostman_TakeFlag(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &vehicle) || !Script_Import((const char *const[]){ "Split", "Sound", NULL })) {
		return NULL;
	}
	if (!Script_In(vehicle, (const char *const[]){ "Player1", "Player0", NULL })) {
		return Py_ReturnNone();
	}
	PyObject *model = Script_Try("Scrap", "DeleteScheduledFuncs", "(s)", "BattonRepos") &&
		Script_SetGlobal("SPostman", "BattonOwner", Script_Ref(vehicle)) && Mode_WaySound() && Script_Try("SPostman", "GetRndFlag", "()") &&
		Mode_FlagSet("SPostman", "Pos", Mode_Zone("SPostman")) ? Script_Global("SPostman", "FlagModel") : NULL;
	byte ok = model && Mode_FlagCall("SPostman", "SetModel", model) &&
		Mode_FlagSet("SPostman", "OnTake", PyString_FromString("SPostman.TakeArea"));
	Py_XDECREF(model);
	if (ok && Script_Is(vehicle, "Player0")) {
		ok = Mode_WayToFlag("SPostman", "CurrentWaypointName") && Script_Try("Scrap", "Set", "(ss)", "CurrentWaypointName2", "Player0");
	} else if (ok) {
		ok = Script_Try("Scrap", "Set", "(ss)", "CurrentWaypointName", "Player1") && Mode_WayToFlag("SPostman", "CurrentWaypointName2");
	}
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *SPostman_CreateFlag(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_NewFlag("SPostman", "BattModel", 2000) && Script_Try("SPostman", "GetRndFlag", "()") &&
		Mode_FlagSet("SPostman", "OnTake", PyString_FromString("SPostman.TakeFlag")) &&
		Mode_FlagSet("SPostman", "Pos", Script_Global("SPostman", "FlagInitPos")) &&
		Mode_WayToFlag("SPostman", "CurrentWaypointName") ? Py_ReturnNone() : NULL;
}

static PyObject *SPostman_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Script_Try("SPostman", "BrowseAreas", "()") || !Script_Try("SPostman", "CreateFlag", "()")) {
		return NULL;
	}
	PyObject *f = Script_Global("SPostman", "FlagModel");
	byte ok = f && Script_Try("Scrap", "Preload3DObject", "(O)", f);
	Py_XDECREF(f);
	PyObject *b = ok ? Script_Global("SPostman", "BattModel") : NULL;
	ok = b && Script_Try("Scrap", "Preload3DObject", "(O)", b);
	Py_XDECREF(b);
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *SPostman_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Players("SPostman", "Green") && Mode_SetPl("SPostman", 0, "RadarShow", PyInt_FromLong(3)) &&
		Mode_SetPl("SPostman", 1, "RadarShow", PyInt_FromLong(3)) && Mode_OnDeath("SPostman") ? Py_ReturnNone() : NULL;
}

/* OnDeathFunc(LooserName, WinnerName): the carrier drops the data pack where it died; it returns 30 s later */
static PyObject *SPostman_OnDeathFunc(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner) || !Script_Import((const char *const[]){ "Split", NULL })) {
		return NULL;
	}
	PyObject *owner = K(Script_Global("SPostman", "BattonOwner"));
	int carrier = owner ? Script_Eq(looser, owner) : -1;
	if (carrier <= 0) {
		return Script_Done(&r, carrier < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *model = Script_SetGlobal("SPostman", "BattonOwner", PyString_FromString("")) &&
		Mode_WayToFlag("SPostman", "CurrentWaypointName") && Mode_WayToFlag("SPostman", "CurrentWaypointName2") ?
		K(Script_Global("SPostman", "BattModel")) : NULL;
	PyObject *e = model && Mode_FlagCall("SPostman", "SetModel", model) &&
		Mode_FlagSet("SPostman", "OnTake", PyString_FromString("SPostman.TakeFlag")) ? K(Script_Entity(looser)) : NULL;
	byte ok = e && Mode_FlagSet("SPostman", "Pos", Script_GetAttr(e, "Pos")) &&
		Script_LaterIn(30, "SPostman", "BattonRepos", PyTuple_New(0), "BattonRepos");
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* BattonRepos(): the data pack back at its start (the script sets it twice) */
static PyObject *SPostman_BattonRepos(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_FlagSet("SPostman", "Pos", Script_Global("SPostman", "FlagInitPos")) &&
		Mode_FlagSet("SPostman", "Pos", Script_Global("SPostman", "FlagInitPos")) ? Py_ReturnNone() : NULL;
}

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

/* the checkpoints of the route that srace.def builds ("run point") */
static PyObject *g_RacePoints;

static void SRace_StepPoint(int id, PyObject *args) {
	(void)id;
	PyObject *text = PyTuple_Size(args) == 1 ? PyTuple_GetItem(args, 0) : NULL;
	PyObject *p = text && g_RacePoints ? Script_EvalIn("SRace", text) : NULL;
	if (!p || PyList_Append(g_RacePoints, p) < 0) {
		PyErr_Clear();
	}
	Py_XDECREF(p);
}

static const scriptStep_t srace_steps[] = {
	{ "point", SRace_StepPoint },
	{ NULL, NULL },
};

/* BrowseAreas(): the route of the map (Init.UpperMap), the race of before on a map without one */
static PyObject *SRace_BrowseAreas(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Init", NULL })) {
		return NULL;
	}
	PyObject *map = Script_Attr("Init", "UpperMap");
	if (!map) {
		return NULL;
	}
	byte ok = 1;
	if (PyString_Check(map)) {
		g_RacePoints = PyList_New(0);
		Script_RunFile(0, "split/srace.def", PyString_AsString(map), srace_def, srace_steps, NULL);
		if (g_RacePoints && PyList_Size(g_RacePoints) > 0) {
			ok = Script_SetGlobal("SRace", "race", PyList_AsTuple(g_RacePoints));
		}
		Py_XDECREF(g_RacePoints);
		g_RacePoints = NULL;
	}
	Py_DECREF(map);
	return ok ? Py_ReturnNone() : NULL;
}

/* race[AcFlag] (a new reference) */
static PyObject *SRace_Point(void) {
	PyObject *race = Script_Global("SRace", "race"), *ac = race ? Script_Global("SRace", "AcFlag") : NULL;
	PyObject *p = ac ? PyObject_GetItem(race, ac) : NULL;
	Py_XDECREF(ac);
	Py_XDECREF(race);
	return p;
}

/* Flag.Pos = race[AcFlag][0]; Flag.Ang = 0, race[AcFlag][1], 0 */
static byte SRace_PlaceFlag(void) {
	PyObject *p = SRace_Point();
	byte ok = p && Mode_FlagSet("SRace", "Pos", Script_Index(p, 0));
	Py_XDECREF(p);
	p = ok ? SRace_Point() : NULL;
	ok = p && Mode_FlagSet("SRace", "Ang", Split_Ang(p));
	Py_XDECREF(p);
	return ok;
}

/* GetRndFlag(): the next checkpoint */
static PyObject *SRace_GetRndFlag(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *ac = K(Script_Global("SRace", "AcFlag")), *one = ac ? K(PyInt_FromLong(1)) : NULL;
	PyObject *s = one ? K(PyNumber_Add(ac, one)) : NULL, *race = s ? K(Script_Global("SRace", "race")) : NULL;
	int n = race ? PyObject_Length(race) : -1;
	PyObject *len = n >= 0 ? K(PyInt_FromLong(n)) : NULL, *v = len ? PyNumber_Remainder(s, len) : NULL;
	return Script_Done(&r, v && Script_SetGlobal("SRace", "AcFlag", v) ? Py_ReturnNone() : NULL);
}

/* TakeFlag(ItemName, VehicleName): a player scores, the flag goes to the next checkpoint */
static PyObject *SRace_TakeFlag(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &vehicle) || !Script_Import((const char *const[]){ "Split", "Sound", NULL })) {
		return NULL;
	}
	int doit = Mode_Score(vehicle, 1);
	byte ok = doit == 0 || (doit > 0 && Mode_WaySound() && Script_Try("SRace", "GetRndFlag", "()") && SRace_PlaceFlag() &&
		Script_Try("Split", "ActualizeScorer", "()"));
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *SRace_CreateFlag(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_NewFlag("SRace", "FlagModel", 4000) && Script_Try("SRace", "GetRndFlag", "()") &&
		Mode_FlagSet("SRace", "OnTake", PyString_FromString("SRace.TakeFlag")) && SRace_PlaceFlag() &&
		Mode_WayToFlag("SRace", "CurrentWaypointName") ? Py_ReturnNone() : NULL;
}

/* InitRacePos(id): beside the checkpoint before the flag, 1500 to the left or right; (pos, angle) */
static PyObject *SRace_InitRacePos(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *race = K(Script_Global("SRace", "race")), *ac = race ? K(Script_Global("SRace", "AcFlag")) : NULL;
	int n = ac ? PyObject_Length(race) : -1;
	PyObject *len = n >= 0 ? K(PyInt_FromLong(n)) : NULL, *a = len ? K(PyNumber_Add(ac, len)) : NULL;
	PyObject *one = a ? K(PyInt_FromLong(1)) : NULL, *b = one ? K(PyNumber_Subtract(a, one)) : NULL;
	PyObject *k = b ? K(PyNumber_Remainder(b, len)) : NULL, *res = k ? K(PyObject_GetItem(race, k)) : NULL;
	int first = res ? Script_NumCmp(id, 0) : -2;
	if (first == -2) {
		FAIL();
	}
	PyObject *ang = K(Script_Index(res, 1)), *rot = ang ? K(Script_Ask("SVec", "Rotate3D", "((iii)(iOi))", first == 0 ? 1500 : -1500,
		0, 0, 0, ang, 0)) : NULL;
	PyObject *at = rot ? K(Script_Index(res, 0)) : NULL, *pos = at ? K(Script_Ask("SVec", "Add", "(OO)", rot, at)) : NULL;
	PyObject *a2 = pos ? K(Script_Index(res, 1)) : NULL;
	return Script_Done(&r, a2 ? Py_BuildValue("(OO)", pos, a2) : NULL);
}

/* GetReSpawnPos(v): where the ship is, its heading */
static PyObject *SRace_GetReSpawnPos(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *v;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	PyObject *pos = K(Script_GetAttr(v, "Pos")), *ang = pos ? K(Script_GetAttr(v, "Ang")) : NULL;
	PyObject *a1 = ang ? K(Script_Index(ang, 1)) : NULL;
	return Script_Done(&r, a1 ? Py_BuildValue("(OO)", pos, a1) : NULL);
}

/* SSound.LoadSound("Sounds/Vehicles/Misc/" + <module variable> + ".wav") */
static byte SRace_LoadSound(const char *var) {
	PyObject *name = Script_Global("SRace", var);
	PyObject *file = name ? Script_PlusS(Script_SPlus("Sounds/Vehicles/Misc/", name), ".wav") : NULL;
	byte ok = file && Script_Try("SSound", "LoadSound", "(O)", file);
	Py_XDECREF(file);
	Py_XDECREF(name);
	return ok;
}

/* InitStatus(): the spawn functions of Split, the count down sounds, the route and the flag */
static PyObject *SRace_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *f1 = Script_Import((const char *const[]){ "Split", NULL }) ? Script_Global("SRace", "InitRacePos") : NULL;
	if (!f1) {
		return NULL;
	}
	Script_SetVar("Split", "FuncGetSpawnPos", f1);
	PyObject *f2 = Script_Global("SRace", "GetReSpawnPos");
	if (!f2) {
		return NULL;
	}
	Script_SetVar("Split", "FuncGetReSpawnPos", f2);
	return SRace_LoadSound("RaceCountDownSound1") && SRace_LoadSound("RaceCountDownSound2") &&
		Script_Try("SRace", "BrowseAreas", "()") && Script_Try("SRace", "CreateFlag", "()") ? Py_ReturnNone() : NULL;
}

/* Scrap.AddScheduledFunc(when, fn, args) (args given away) */
static byte SRace_At(PyObject *when, PyObject *fn, PyObject *args) {
	byte ok = when && fn && args && Script_Try("Scrap", "AddScheduledFunc", "(OOO)", when, fn, args);
	Py_XDECREF(args);
	return ok;
}

/* Init(): both players wait for the count down 3, 2, 1, GO! (ControlToRacers then) */
static PyObject *SRace_Init(PyObject *self, PyObject *args) {
	static const char *const counts[] = { "2", "1", "GO!" };
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Mode_Players("SRace", "Green") || !Mode_OnDeath("SRace") || !Mode_SetPl("SRace", 0, "UsrControl", PyInt_FromLong(-1)) ||
		!Mode_SetPl("SRace", 1, "UsrControl", PyInt_FromLong(-1))) {
		return NULL;
	}
	PyObject *time = K(Script_Ask("Scrap", "GetTime", "()")), *one = time ? K(PyInt_FromLong(1)) : NULL;
	PyObject *t1 = one ? K(PyNumber_Add(time, one)) : NULL;
	PyObject *s1 = t1 ? K(Script_Global("SRace", "RaceCountDownSound1")) : NULL;
	PyObject *v1 = s1 ? K(Script_Global("SRace", "RaceCountDownSound1Vol")) : NULL;
	if (!v1 || !Script_Try("SScorer", "SetMsgText", "(sO)", "3", t1) || !Script_Try("SSound", "Play", "(OO)", s1, v1)) {
		FAIL();
	}
	PyObject *msg = K(Script_Attr("SScorer", "SetMsgText")), *play = msg ? K(Script_Attr("SSound", "Play")) : NULL;
	for (int i = 0; play && i < 3; i++) {
		PyObject *d = K(PyInt_FromLong(i + 1)), *timy = d ? K(PyNumber_Add(time, d)) : NULL, *end = timy ? K(PyNumber_Add(timy, one)) : NULL;
		if (!end || !SRace_At(timy, msg, Py_BuildValue("(sO)", counts[i], end))) {
			FAIL();
		}
		if (i == 2 && !SRace_At(timy, K(Script_Global("SRace", "ControlToRacers")), PyTuple_New(0))) {
			FAIL();
		}
		PyObject *s = K(Script_Global("SRace", i == 2 ? "RaceCountDownSound2" : "RaceCountDownSound1"));
		PyObject *v = s ? K(Script_Global("SRace", i == 2 ? "RaceCountDownSound2Vol" : "RaceCountDownSound1Vol")) : NULL;
		if (!v || !SRace_At(timy, play, Py_BuildValue("(OO)", s, v))) {
			FAIL();
		}
	}
	return Script_Done(&r, play ? Py_ReturnNone() : NULL);
}

static PyObject *SRace_ControlToRacers(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_SetPl("SRace", 0, "UsrControl", PyInt_FromLong(0)) && Mode_SetPl("SRace", 1, "UsrControl", PyInt_FromLong(1)) ?
		Py_ReturnNone() : NULL;
}

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
	{ "OnDeathFunc", Mode_Pass2, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
