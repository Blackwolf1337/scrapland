/*
 * The general sound scripts in C (scripts/sound/sound.py, outsound.py and doorsound.py, issue #2). What they set is
 * data: the sound files and engine variables a level loads, the sound variables of a ship and the sound events of
 * the door classes are sections of data/sound/sound.def, outsound.def and doorsound.def. Here the tests of the
 * scripts (given to the files as values), the order of the loads, the point sounds of the map scripts (SetPSLoop,
 * SetPSInterval) and the door sound lists.
 *
 * The module variables stay in the scripts, other scripts read them: Sound.ItemTake, WaySound, WayVolume,
 * EneWaySound, EneWayVolume, PhotoSound, PhotoVolume, DeathSound, TVOffSound and the ship editor sounds, OutSound.isPC,
 * OnLandSound and OnTakeOffSound, DoorSound.SoundList and AllSounds (LoadSndList sorts the lists in place like the
 * script). The functions call the others of their module through the module's globals.
 *
 * The modules are native C (docs/objlib-plan.md, stage 2): the functions work on C values and read the variables of
 * Init and of their module where the scripts read them (OBJLIB 5: ItemTake changed in place, the door lists sorted as
 * the objects they are). Scrap.GetEntity, Set, OpenPack, ClosePack, ConsoleError, SSound.LoadSound and the SAct
 * functions go through their seams (OBJLIB 7); string.upper is C. SSound.CreatePS and SetPS, SAct.GetAction of a
 * value that is no text, DoorSound.LoadSounds from Sound and Sound.SetStdDamageSounds from OutSound stay by name
 * (OBJLIB 3, 7). The .def runner takes its $names as a dict and gives its steps Python arguments (OBJLIB 6).
 */
#include <string.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static const char sound_def[] = {
#embed "../../../data/sound/sound.def"
	, 0 };

static const char outsound_def[] = {
#embed "../../../data/sound/outsound.def"
	, 0 };

static const char doorsound_def[] = {
#embed "../../../data/sound/doorsound.def"
	, 0 };

static void Sound_StepTake(int id, PyObject *args);
static void DoorSound_StepInitSound(int id, PyObject *args);

static const scriptStep_t gsound_steps[] = {
	{ "take", Sound_StepTake },
	{ "initsound", DoorSound_StepInitSound },
	{ NULL, NULL },
};

/* a section of one of the files with the $names of vars (Mode_DefVars, given away; NULL: none): an error in the file
 * (a call that raises, an entity that refuses a variable) stops it like the script */
static byte Gs_Run(const char *file, const char *builtin, const char *section, PyObject *vars) {
	byte ok = Script_RunFileTry(0, file, section, builtin, gsound_steps, vars);
	Py_XDECREF(vars);
	return ok;
}

#define Gs_RunSound(section, vars) Gs_Run("sound/sound.def", sound_def, section, vars)
#define Gs_RunOut(section, vars) Gs_Run("sound/outsound.def", outsound_def, section, vars)
#define Gs_RunDoor(section, vars) Gs_Run("sound/doorsound.def", doorsound_def, section, vars)

/* `if Init.<var>:`: 1, 0, -1 with the error */
static int Gs_Init(const char *var) {
	return Mode_ModTrue("Init", var);
}

/* `if not Init.Outdoor or Init.isJunkYard or Init.isShipEdit:` of the vehicle sounds: 1, 0, -1 after an error */
static int Gs_NotOutdoor(void) {
	int t = Gs_Init("Outdoor");
	if (t == 0) {
		return 1;
	}
	if (t > 0) {
		t = Gs_Init("isJunkYard");
	}
	if (t == 0) {
		t = Gs_Init("isShipEdit");
	}
	return t;
}

/* print text */
static byte Gs_Print(const char *text) {
	nativeVal_t v;
	Native_SetStr(&v, text);
	const nativeVal_t *items[1] = { &v };
	return Mode_Print(items, 1);
}

/* an int as a value */
static const nativeVal_t *Gs_Int(nativeVal_t *v, long i) {
	Native_SetInt(v, i);
	return v;
}

/* ---- Sound ---- */

#define SND "Sound"

/* run take <key> <sound> <volume>: ItemTake[key] = (sound, volume) and the sound file Missions/<sound>.wav (an error
 * is left set, it stops the file) */
static void Sound_StepTake(int id, PyObject *args) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *a, *pair, *file;
	(void)id;
	if (!Mode_Args(args, 3, &t, &a)) {
		Mode_Done(&t, 0);
		return;
	}
	pair = Mode_V(&t);
	file = Mode_V(&t);
	Native_SetEmpty(pair, NV_TUPLE);
	Native_Copy(Native_Append(pair), &a[1]);
	Native_Copy(Native_Append(pair), &a[2]);
	Mode_Done(&t, Mode_DictPut(SND, "ItemTake", NULL, &a[0], pair) &&
		Mode_Cat(&t, file, 3, Mode_S(&t, "Missions/"), &a[1], Mode_S(&t, ".wav")) && Mode_LoadSoundSeam(file));
}

/* SetStdDamageSounds(EntityName): the damage sounds of a ship in a level with ships */
static byte Gs_SetStdDamageSounds(const nativeVal_t *name) {
	nativeVal_t v = { 0 };
	int no = Mode_Import("Init") ? Gs_NotOutdoor() : -1;
	if (no) {
		return no > 0;
	}
	/* the first set through the handle: the error of the script for a handle of no entity (None) */
	byte ok = Mode_EntitySeam(name, &v) && Mode_VIsType(&v, "WithLife") >= 0 && LvN_SetI(&v, "sdd_n", 9) &&
		Gs_RunSound("SetStdDamageSounds", Mode_DefVars(1, "name", name)) && Native_CallGlobal(SND, "SetStdDamageSoundLoops", NULL, "v", &v);
	Native_Free(&v);
	return ok;
}

/* SetStdDamageSoundLoops(v): the damage loops of the entity of the handle v */
static byte Gs_SetStdDamageSoundLoops(const nativeVal_t *v) {
	nativeVal_t name = { 0 };
	byte ok = LvN_SetI(v, "sdl_n", 5) && Mode_VObj(v, "Name", &name) && Gs_RunSound("SetStdDamageSoundLoops", Mode_DefVars(1, "name", &name));
	Native_Free(&name);
	return ok;
}

/* LoadDamageSounds(): the hits, loops and fire of the ships ($out), and the death of the characters */
static byte Gs_LoadDamageSounds(void) {
	nativeVal_t v;
	int out = Mode_Import("Init") ? Gs_Init("Outdoor") : -1;
	if (out > 0) {
		out = Gs_Init("isJunkYard");
		out = out < 0 ? -1 : !out;
	}
	if (out > 0) {
		out = Gs_Init("isShipEdit");
		out = out < 0 ? -1 : !out;
	}
	return out >= 0 && Gs_RunSound("LoadDamageSounds", Mode_DefVars(1, "out", Gs_Int(&v, out)));
}

/* WayPoints(): the money, the check points and the items of the outdoor levels ($out), or the battery and the extra
 * life indoors */
static byte Gs_WayPoints(void) {
	nativeVal_t v;
	int out = Mode_Import("Init") ? Gs_Init("Outdoor") : -1;
	return out >= 0 && Gs_RunSound("WayPoints", Mode_DefVars(1, "out", Gs_Int(&v, out)));
}

/* SetOutMusic(): the relax and action music of the level */
static byte Gs_SetOutMusic(void) {
	static const char *const action[] = { "BANK", "TEMPLE", "PRESS", "POLICE", "TOWNHALL", NULL };
	nativeVal_t a, b, c, map = { 0 };
	int out = Mode_Import("Init") ? Gs_Init("Outdoor") : -1;
	int act = 0;
	if (out == 0) {
		act = Mode_ModGet("Init", "UpperMap", &map) ? 0 : -1;
		for (int i = 0; !act && action[i]; i++) {
			act = LvN_EqS(&map, action[i]);
		}
		Native_Free(&map);
	}
	int demo = out >= 0 && act >= 0 ? Gs_Init("isDemo") : -1;
	return demo >= 0 && Gs_RunSound("SetOutMusic", Mode_DefVars(3, "out", Gs_Int(&a, out), "action", Gs_Int(&b, act), "demo",
		Gs_Int(&c, demo)));
}

/* Init(): the generic sounds of every level, then the music */
static byte Gs_SoundInit(void) {
	nativeVal_t pack;
	if (!Gs_Print("Loading default sounds...") || !Mode_Import("DoorSound") || !Mode_Import("Init") ||
		!Mode_GVarSetN("SndLoopCharNPCMul", 2.5)) {
		return 0;
	}
	Native_SetStr(&pack, "Sounds/Generic");
	if (!Mode_Pack(&pack) || !Native_CallGlobal(SND, "LoadMenuSounds", NULL, "")) {
		return 0;
	}
	/* not Init.inMainMenu or Init.isShipEdit */
	int t = Gs_Init("inMainMenu");
	t = t == 0 ? 1 : t > 0 ? Gs_Init("isShipEdit") : -1;
	if (t > 0) {
		int skip = Native_CallGlobal(SND, "LoadDamageSounds", NULL, "") && Native_CallMod("DoorSound", "LoadSounds", NULL, "") &&
			Native_CallGlobal(SND, "WayPoints", NULL, "") && Native_CallGlobal(SND, "PhotoSounds", NULL, "") &&
			Native_CallGlobal(SND, "ScorerSounds", NULL, "") ? Gs_Init("InMapSkip") : -1;
		t = skip > 0 ? (Native_CallGlobal(SND, "CharConversionSounds", NULL, "") ? 0 : -1) : skip;
	}
	if (t < 0) {
		return 0;
	}
	/* not Init.inMainMenu and (Init.isShipEdit or Init.isJunkYard or not Init.OffLineGame) */
	t = Gs_Init("inMainMenu");
	if (t == 0) {
		t = Gs_Init("isShipEdit");
		if (t == 0) {
			t = Gs_Init("isJunkYard");
		}
		if (t == 0) {
			t = Gs_Init("OffLineGame");
			t = t < 0 ? -1 : !t;
		}
		if (t > 0) {
			t = Native_CallGlobal(SND, "LoadShipEditInterfaceSounds", NULL, "") ? 0 : -1;
		}
	}
	return t >= 0 && Mode_Pack(NULL) && Native_CallGlobal(SND, "SetOutMusic", NULL, "");
}

/*
 * SetPSLoop(Name, Pos, Snd, Volume=1, Pitch=0, AttenIni=0, AttenEnd=0, Doppler=1.0) and SetPSInterval(Name, Pos, Snd,
 * MinInterval, MaxInterval, Volume=1, ...): a point sound of the map, looping or played at random intervals (a: the
 * arguments in the order of SetPSInterval, the minimum and maximum None for a loop; the defaults put in)
 */
static byte Gs_SetPS(const nativeVal_t *a, byte interval) {
	static const char *const vars[] = { "Volume", "Pitch", "AttenIni", "AttenEnd", "Doppler" };
	const nativeVal_t *name = &a[0];
	nativeVal_t c = { 0 };
	byte ok = Native_CallMod("SSound", "CreatePS", NULL, "v", name) && Native_CallMod("SSound", "SetPS", NULL, "vsv", name, "Sound",
		&a[2]);
	for (int i = 0; ok && i < 3; i++) {
		static const char *const axes[3] = { "X", "Y", "Z" };
		ok = Mode_Item(&a[1], i, &c) && Native_CallMod("SSound", "SetPS", NULL, "vsv", name, axes[i], &c);
		Native_Free(&c);
	}
	for (int i = 0; ok && i < 5; i++) {
		ok = Native_CallMod("SSound", "SetPS", NULL, "vsv", name, vars[i], &a[5 + i]);
		if (ok && i == 1) {
			ok = Native_CallMod("SSound", "SetPS", NULL, "vsi", name, "Loop", interval ? 0L : 1L);
		}
	}
	return ok && (!interval || (Native_CallMod("SSound", "SetPS", NULL, "vsv", name, "MaxInterval", &a[4]) &&
		Native_CallMod("SSound", "SetPS", NULL, "vsv", name, "MinInterval", &a[3])));
}

static PyObject *Sound_SetPSArgs(PyObject *args, PyObject *kw, byte interval) {
	static char *loopNames[] = { "Name", "Pos", "Snd", "Volume", "Pitch", "AttenIni", "AttenEnd", "Doppler", NULL };
	static char *intNames[] = { "Name", "Pos", "Snd", "MinInterval", "MaxInterval", "Volume", "Pitch", "AttenIni",
		"AttenEnd", "Doppler", NULL };
	PyObject *o[10] = { NULL };
	nativeVal_t a[10];
	if (interval ? !PyArg_ParseTupleAndKeywords(args, kw, "OOOOO|OOOOO", intNames, &o[0], &o[1], &o[2], &o[3], &o[4],
			&o[5], &o[6], &o[7], &o[8], &o[9]) :
		!PyArg_ParseTupleAndKeywords(args, kw, "OOO|OOOOO", loopNames, &o[0], &o[1], &o[2], &o[5], &o[6], &o[7], &o[8],
			&o[9])) {
		return NULL;
	}
	for (int i = 0; i < 10; i++) {
		Native_SetNone(&a[i]);
		if (o[i]) {
			Native_Arg(o[i], &a[i]);
		}
	}
	if (!o[5]) {
		Native_SetInt(&a[5], 1);
	}
	for (int i = 6; i < 9; i++) {
		if (!o[i]) {
			Native_SetInt(&a[i], 0);
		}
	}
	if (!o[9]) {
		Native_SetFloat(&a[9], 1.0);
	}
	byte ok = Gs_SetPS(a, interval);
	for (int i = 0; i < 10; i++) {
		Native_Free(&a[i]);
	}
	return Mode_Result(ok);
}

static PyObject *Sound_SetPSLoop(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Sound_SetPSArgs(args, kw, 0);
}

static PyObject *Sound_SetPSInterval(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Sound_SetPSArgs(args, kw, 1);
}

/* the sections without tests: LoadMenuSounds, ScorerSounds, PhotoSounds, CharConversionSounds,
 * LoadShipEditInterfaceSounds */
#define SOUND_SECTION(fn) MODE_FN(Sound, fn, 0, Gs_RunSound(#fn, NULL))

SOUND_SECTION(LoadMenuSounds)
SOUND_SECTION(ScorerSounds)
SOUND_SECTION(PhotoSounds)
SOUND_SECTION(CharConversionSounds)
SOUND_SECTION(LoadShipEditInterfaceSounds)
MODE_FN(Sound, SetStdDamageSounds, 1, Gs_SetStdDamageSounds(&a[0]))
MODE_FN(Sound, SetStdDamageSoundLoops, 1, Gs_SetStdDamageSoundLoops(&a[0]))
MODE_FN(Sound, LoadDamageSounds, 0, Gs_LoadDamageSounds())
MODE_FN(Sound, WayPoints, 0, Gs_WayPoints())
MODE_FN(Sound, SetOutMusic, 0, Gs_SetOutMusic())
MODE_FN(Sound, Init, 0, Gs_SoundInit())

PyMethodDef g_SoundMethods[] = {
	{ "SetStdDamageSounds", Sound_SetStdDamageSounds, 1, NULL },
	{ "SetStdDamageSoundLoops", Sound_SetStdDamageSoundLoops, 1, NULL },
	{ "LoadDamageSounds", Sound_LoadDamageSounds, 1, NULL },
	{ "LoadMenuSounds", Sound_LoadMenuSounds, 1, NULL },
	{ "ScorerSounds", Sound_ScorerSounds, 1, NULL },
	{ "WayPoints", Sound_WayPoints, 1, NULL },
	{ "PhotoSounds", Sound_PhotoSounds, 1, NULL },
	{ "CharConversionSounds", Sound_CharConversionSounds, 1, NULL },
	{ "LoadShipEditInterfaceSounds", Sound_LoadShipEditInterfaceSounds, 1, NULL },
	{ "Init", Sound_Init, 1, NULL },
	{ "SetOutMusic", Sound_SetOutMusic, 1, NULL },
	{ "SetPSLoop", (PyCFunction)(void (*)(void))Sound_SetPSLoop, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "SetPSInterval", (PyCFunction)(void (*)(void))Sound_SetPSInterval, METH_VARARGS | METH_KEYWORDS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- OutSound ---- */

#define OUT "OutSound"

/* `if isPC:` of the script (the NameError when the module lost it): 1, 0, -1 */
static int Gs_IsPC(void) {
	return LvN_GTrue(OUT, "isPC");
}

/* SetVehicleSounds(EntityName): the sound variables of a ship by its model, then the damage sounds of Sound */
static byte Gs_SetVehicleSounds(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *m = Mode_V(&t), *model = Mode_V(&t), *prev = Mode_V(&t), *pcv = Mode_V(&t);
	nativeVal_t *armedv = Mode_V(&t), *none = Mode_V(&t);
	int no = Mode_Import("Init") && Mode_Import("string") && Mode_Import("Traffic") ? Gs_NotOutdoor() : -1;
	if (no) {
		return Mode_Done(&t, no > 0);
	}
	int pc = Mode_EntitySeam(name, v) && LvN_SetI(v, "snd_n", 32) ? Gs_IsPC() : -1;
	int armed = pc >= 0 && Mode_VObj(v, "Model", m) && Native_Upper(m, model) && Mode_ModGet("Traffic", "PrevVal", prev) ?
		Mode_Cmp(prev, "==", none) : -1;
	if (armed < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Gs_RunOut("SetVehicleSounds", Mode_DefVars(4, "name", name, "model", model, "pc", Gs_Int(pcv, pc), "armed",
		Gs_Int(armedv, armed))) && Mode_Import(SND) && Native_CallMod(SND, "SetStdDamageSounds", NULL, "v", name));
}

/* the load sections, LoadEngines and LoadWeapons with $pc */
static byte Gs_OutSection(const char *section, byte pcTest) {
	nativeVal_t v;
	if (!pcTest) {
		return Gs_RunOut(section, NULL);
	}
	int pc = Gs_IsPC();
	return pc >= 0 && Gs_RunOut(section, Mode_DefVars(1, "pc", Gs_Int(&v, pc)));
}

/* LoadVehicles(): the vehicle sounds of an outdoor level, from the pack Sounds/Vehicles */
static byte Gs_LoadVehicles(void) {
	static const char *const loads[] = { "LoadEngines", "LoadWeapons", "LoadScorer", "LoadMisc", "LoadCrash", NULL };
	nativeVal_t pack;
	int no = Mode_Import("Init") ? Gs_NotOutdoor() : -1;
	if (no) {
		return no > 0;
	}
	Native_SetStr(&pack, "Sounds/Vehicles");
	if (!Gs_Print("Loading vehicle sounds...") || !Mode_Pack(&pack)) {
		return 0;
	}
	for (int i = 0; loads[i]; i++) {
		if (!Native_CallGlobal(OUT, loads[i], NULL, "")) {
			return 0;
		}
	}
	return Mode_Pack(NULL);
}

/* Init(): the sound engine settings of the outdoor levels, then the vehicle sounds */
static byte Gs_OutInit(void) {
	return Gs_Print("Loading outdor sounds...") && Gs_RunOut("Init", NULL) && Native_CallGlobal(OUT, "LoadVehicles", NULL, "");
}

MODE_FN(OutSound, SetVehicleSounds, 1, Gs_SetVehicleSounds(&a[0]))
MODE_FN(OutSound, LoadEngines, 0, Gs_OutSection("LoadEngines", 1))
MODE_FN(OutSound, LoadWeapons, 0, Gs_OutSection("LoadWeapons", 1))
MODE_FN(OutSound, LoadScorer, 0, Gs_OutSection("LoadScorer", 0))
MODE_FN(OutSound, LoadMisc, 0, Gs_OutSection("LoadMisc", 0))
MODE_FN(OutSound, LoadCrash, 0, Gs_OutSection("LoadCrash", 0))
MODE_FN(OutSound, LoadVehicles, 0, Gs_LoadVehicles())
MODE_FN(OutSound, Init, 0, Gs_OutInit())

PyMethodDef g_OutSoundMethods[] = {
	{ "SetVehicleSounds", OutSound_SetVehicleSounds, 1, NULL },
	{ "LoadEngines", OutSound_LoadEngines, 1, NULL },
	{ "LoadWeapons", OutSound_LoadWeapons, 1, NULL },
	{ "LoadScorer", OutSound_LoadScorer, 1, NULL },
	{ "LoadMisc", OutSound_LoadMisc, 1, NULL },
	{ "LoadCrash", OutSound_LoadCrash, 1, NULL },
	{ "LoadVehicles", OutSound_LoadVehicles, 1, NULL },
	{ "Init", OutSound_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- DoorSound ---- */

#define DOOR "DoorSound"

/* LoadSndList(list) of the module's global with a list as the object it is (OBJLIB 5: the lists of the module are
 * sorted in place) */
static byte Gs_LoadSndListOf(PyObject *list) {
	byte ok = list && Native_CallGlobal(DOOR, "LoadSndList", NULL, "O", list);
	Py_XDECREF(list);
	return ok;
}

/* LoadSounds(): the door sounds of the level from SoundList, all of them (with an error) for a level not listed */
static byte Gs_LoadSounds(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *list = Mode_V(&t), *has = Mode_V(&t), *map = Mode_V(&t), *r = Mode_V(&t), *msg = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_G(DOOR, "SoundList", list) || !Mode_VObj(list, "has_key", has) ||
		!Mode_ModGet("Init", "UpperMap", map) || !Mode_CallArgs(has, r, map, 1)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(r)) {
		Native_Free(map);
		return Mode_Done(&t, Mode_ModGet("Init", "UpperMap", map) && Gs_LoadSndListOf(Mode_AtPy(DOOR, "SoundList", map)));
	}
	Native_Free(map);
	return Mode_Done(&t, Gs_LoadSndListOf(Mode_GlobalPy(DOOR, "AllSounds")) && Mode_ModGet("Init", "UpperMap", map) &&
		Mode_Cat(&t, msg, 3, Mode_S(&t, "S O N O R U S    E R R O R : "), map, Mode_S(&t, " not in DoorSound.py.\n")) &&
		Mode_ConsoleError(msg));
}

/* LoadSndList(SndList) after the sort: loads Doors/<name>.wav of each */
static byte Gs_LoadSndList(nativeVal_t *list) {
	int n = Mode_Loop(list);
	for (int i = 0; i < n; i++) {
		nativeVal_t snd = { 0 }, pre, post, a = { 0 }, file = { 0 };
		Native_SetStr(&pre, "Doors/");
		Native_SetStr(&post, ".wav");
		byte ok = Native_SeqItem(list, i, &snd) && Native_Arith('+', &pre, &snd, &a) && Native_Arith('+', &a, &post, &file) &&
			Mode_LoadSoundSeam(&file);
		Native_Free(&snd);
		Native_Free(&a);
		Native_Free(&file);
		if (!ok) {
			return 0;
		}
	}
	return n >= 0;
}

/* the adapter of LoadSndList: the list sorted in place as the object it is (OBJLIB 5), then the loads */
static PyObject *DoorSound_LoadSndList(PyObject *self, PyObject *args) {
	PyObject *list;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &list)) {
		return NULL;
	}
	PyObject *r = Script_Method(list, "sort", "()");
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	nativeVal_t v;
	Native_Arg(list, &v);
	byte ok = Gs_LoadSndList(&v);
	Native_Free(&v);
	return Mode_Result(ok);
}

/* run initsound <args>: SetInitDoorSound(args) through the module's globals, as SetSlide calls it (an error is left
 * set, it stops the file) */
static void DoorSound_StepInitSound(int id, PyObject *args) {
	nativeVal_t f = { 0 }, a;
	(void)id;
	Native_Arg(args, &a);
	if (LvN_GObj(DOOR, "SetInitDoorSound", &f)) {
		Mode_Apply(&f, &a, NULL);
	}
	Native_Free(&f);
	Native_Free(&a);
}

/* SetSlide(DoorClass): the sound events of a door class (the default open and close sounds for a class not listed) */
static byte Gs_SetSlide(const nativeVal_t *cls) {
	return Mode_Import("Scene") && Mode_Import("Chars") && Gs_RunDoor("SetSlide", Mode_DefVars(1, "class", cls));
}

/* SetInitDoorSound(DoorClass, ActionName, SoundName, Vol=1, Pitch=0): a sound at the start of an action */
static byte Gs_SetInitDoorSound(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *rv = Mode_V(&t), *rp = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_SActV("GetClass", &a[0], r)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(r);
	/* SAct.GetAction through its seam with a text, by name for any other value (its TypeError) */
	byte ok = a[1].type == NV_STR && !memchr(Native_Text(&a[1]), 0, (size_t)a[1].n) ? LvN_GetAction(Native_Text(&a[1]), r) :
		Native_CallMod("SAct", "GetAction", NULL, "v", &a[1]);
	Mode_Repr(&a[3], rv);
	Mode_Repr(&a[4], rp);
	return Mode_Done(&t, ok && LvN_SetAct("AddEventAtTime", Mode_I(&t, 0)) &&
		Mode_Cat(&t, name, 5, &a[2], Mode_S(&t, ":"), rv, Mode_S(&t, ":"), rp) && LvN_SetAct("EventName", name) &&
		LvN_SetAct("EventFunc", Mode_S(&t, "!AnmSoundEvent")));
}

static PyObject *DoorSound_SetInitDoorSound(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "DoorClass", "ActionName", "SoundName", "Vol", "Pitch", NULL };
	nativeVal_t a[5];
	byte given[5];
	(void)self;
	if (!Mode_Kw(args, kw, "OOO|OO", names, 5, a, given)) {
		return NULL;
	}
	if (!given[3]) {
		Native_SetInt(&a[3], 1);
	}
	if (!given[4]) {
		Native_SetInt(&a[4], 0);
	}
	return Mode_KwOut(a, 5, Gs_SetInitDoorSound(a), NULL);
}

/* ConversorSetSounds(CharName): the sounds of the conversion point's action */
static byte Gs_ConversorSetSounds(const nativeVal_t *name) {
	return Mode_Import("Chars") && Gs_RunDoor("ConversorSetSounds", Mode_DefVars(1, "char", name));
}

MODE_FN(DoorSound, LoadSounds, 0, Gs_LoadSounds())
MODE_FN(DoorSound, SetSlide, 1, Gs_SetSlide(&a[0]))
MODE_FN(DoorSound, ConversorSetSounds, 1, Gs_ConversorSetSounds(&a[0]))

PyMethodDef g_DoorSoundMethods[] = {
	{ "LoadSounds", DoorSound_LoadSounds, 1, NULL },
	{ "LoadSndList", DoorSound_LoadSndList, 1, NULL },
	{ "SetSlide", DoorSound_SetSlide, 1, NULL },
	{ "SetInitDoorSound", (PyCFunction)(void (*)(void))DoorSound_SetInitDoorSound, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "ConversorSetSounds", DoorSound_ConversorSetSounds, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
