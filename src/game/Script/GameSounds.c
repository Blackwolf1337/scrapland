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
 */
#include <stdarg.h>

#include "scrap/script.h"

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

/* a function of the module as its script calls it (a global of the module) */
static PyObject *Snd_Own(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...) {
	PyObject *fn = Sf_Global(f, module, func);
	if (!fn) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_CallArgs(f, fn, Sf_Check(f, args != NULL) ? args : NULL);
}

/* a section of one of the files with the values of vars (a dict, or NULL), unless the frame failed; an error in the
 * file (a call that raises, an entity that refuses a variable) stops it and the frame like the script */
static void Snd_Run(scriptFrame_t *f, const char *file, const char *builtin, const char *section, PyObject *vars) {
	if (!f->failed) {
		Sf_Check(f, Script_RunFileTry(0, file, section, builtin, gsound_steps, vars));
	}
}

static void Snd_RunSound(scriptFrame_t *f, const char *section, PyObject *vars) {
	Snd_Run(f, "sound/sound.def", sound_def, section, vars);
}

static void Snd_RunOut(scriptFrame_t *f, const char *section, PyObject *vars) {
	Snd_Run(f, "sound/outsound.def", outsound_def, section, vars);
}

/* `if not Init.Outdoor or Init.isJunkYard or Init.isShipEdit:` of the vehicle sounds: 1, 0, -1 after an error */
static int Snd_NotOutdoor(scriptFrame_t *f) {
	int t = Sf_True(f, Sf_Mod(f, "Init", "Outdoor"));
	if (t == 0) {
		return 1;
	}
	if (t > 0) {
		t = Sf_True(f, Sf_Mod(f, "Init", "isJunkYard"));
	}
	if (t == 0) {
		t = Sf_True(f, Sf_Mod(f, "Init", "isShipEdit"));
	}
	return t;
}

/* ---- Sound ---- */

/* run take <key> <sound> <volume>: ItemTake[key] = (sound, volume) and the sound file Missions/<sound>.wav (an error
 * is left set, it stops the file) */
static void Sound_StepTake(int id, PyObject *args) {
	PyObject *key, *snd, *vol;
	scriptFrame_t f = { 0 };
	(void)id;
	if (!PyArg_ParseTuple(args, "OOO", &key, &snd, &vol)) {
		return;
	}
	PyObject *take = Sf_Global(&f, "Sound", "ItemTake");
	Sf_SetItem(&f, take, key, Sf_Build(&f, "(OO)", snd, vol));
	Sf_Ask(&f, "SSound", "LoadSound", "(O)", Sf_Cat(&f, "sOs", "Missions/", snd, ".wav"));
	Py_XDECREF(Sf_None(&f));
}

/* SetStdDamageSounds(EntityName): the damage sounds of a ship in a level with ships */
static PyObject *Sound_SetStdDamageSounds(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	if (Snd_NotOutdoor(&f) != 0) {
		return Sf_None(&f);
	}
	PyObject *v = Sf_Entity(&f, name);
	Sf_Method(&f, v, "IsType", "(s)", "WithLife");
	/* the first set through the handle: the error of the script for a handle of no entity (None) */
	Sf_SetAttrI(&f, v, "sdd_n", 9);
	Snd_RunSound(&f, "SetStdDamageSounds", Sf_Build(&f, "{s:O}", "name", name));
	Snd_Own(&f, "Sound", "SetStdDamageSoundLoops", "(O)", v);
	return Sf_None(&f);
}

/* SetStdDamageSoundLoops(v): the damage loops of the entity of the handle v */
static PyObject *Sound_SetStdDamageSoundLoops(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	Sf_SetAttrI(&f, v, "sdl_n", 5);
	Snd_RunSound(&f, "SetStdDamageSoundLoops", Sf_Build(&f, "{s:O}", "name", Sf_Attr(&f, v, "Name")));
	return Sf_None(&f);
}

/* LoadDamageSounds(): the hits, loops and fire of the ships ($out), and the death of the characters */
static PyObject *Sound_LoadDamageSounds(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int out = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	if (out > 0) {
		out = Sf_True(&f, Sf_Mod(&f, "Init", "isJunkYard"));
		out = out < 0 ? -1 : !out;
	}
	if (out > 0) {
		out = Sf_True(&f, Sf_Mod(&f, "Init", "isShipEdit"));
		out = out < 0 ? -1 : !out;
	}
	Snd_RunSound(&f, "LoadDamageSounds", Sf_Build(&f, "{s:i}", "out", out));
	return Sf_None(&f);
}

/* the sections without tests: LoadMenuSounds, ScorerSounds, PhotoSounds, CharConversionSounds,
 * LoadShipEditInterfaceSounds */
#define SOUND_SECTION(fn) \
	static PyObject *Sound_##fn(PyObject *self, PyObject *args) { \
		scriptFrame_t f = { 0 }; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "")) { \
			return NULL; \
		} \
		Snd_RunSound(&f, #fn, NULL); \
		return Sf_None(&f); \
	}

SOUND_SECTION(LoadMenuSounds)
SOUND_SECTION(ScorerSounds)
SOUND_SECTION(PhotoSounds)
SOUND_SECTION(CharConversionSounds)
SOUND_SECTION(LoadShipEditInterfaceSounds)

/* WayPoints(): the money, the check points and the items of the outdoor levels ($out), or the battery and the
 * extra life indoors */
static PyObject *Sound_WayPoints(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int out = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	Snd_RunSound(&f, "WayPoints", Sf_Build(&f, "{s:i}", "out", out));
	return Sf_None(&f);
}

/* SetOutMusic(): the relax and action music of the level */
static PyObject *Sound_SetOutMusic(PyObject *self, PyObject *args) {
	static const char *const action[] = { "BANK", "TEMPLE", "PRESS", "POLICE", "TOWNHALL", NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int out = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	int act = out == 0 ? Sf_InS(&f, Sf_Mod(&f, "Init", "UpperMap"), action) : 0;
	int demo = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo"));
	Snd_RunSound(&f, "SetOutMusic", Sf_Build(&f, "{s:i,s:i,s:i}", "out", out, "action", act, "demo", demo));
	return Sf_None(&f);
}

/* Init(): the generic sounds of every level, then the music */
static PyObject *Sound_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Print(&f, "s", "Loading default sounds...");
	Sf_Import(&f, "DoorSound");
	Sf_Import(&f, "Init");
	Sf_Ask(&f, "Scrap", "Set", "(sd)", "SndLoopCharNPCMul", 2.5);
	Sf_Ask(&f, "Scrap", "OpenPack", "(s)", "Sounds/Generic");
	Snd_Own(&f, "Sound", "LoadMenuSounds", "()");
	/* not Init.inMainMenu or Init.isShipEdit */
	int t = Sf_True(&f, Sf_Mod(&f, "Init", "inMainMenu"));
	t = t == 0 ? 1 : t > 0 ? Sf_True(&f, Sf_Mod(&f, "Init", "isShipEdit")) : -1;
	if (t > 0) {
		Snd_Own(&f, "Sound", "LoadDamageSounds", "()");
		Sf_Ask(&f, "DoorSound", "LoadSounds", "()");
		Snd_Own(&f, "Sound", "WayPoints", "()");
		Snd_Own(&f, "Sound", "PhotoSounds", "()");
		Snd_Own(&f, "Sound", "ScorerSounds", "()");
		if (Sf_True(&f, Sf_Mod(&f, "Init", "InMapSkip")) > 0) {
			Snd_Own(&f, "Sound", "CharConversionSounds", "()");
		}
	}
	/* not Init.inMainMenu and (Init.isShipEdit or Init.isJunkYard or not Init.OffLineGame) */
	t = Sf_True(&f, Sf_Mod(&f, "Init", "inMainMenu"));
	if (t == 0) {
		t = Sf_True(&f, Sf_Mod(&f, "Init", "isShipEdit"));
		if (t == 0) {
			t = Sf_True(&f, Sf_Mod(&f, "Init", "isJunkYard"));
		}
		if (t == 0) {
			t = Sf_True(&f, Sf_Mod(&f, "Init", "OffLineGame"));
			t = t < 0 ? -1 : !t;
		}
		if (t > 0) {
			Snd_Own(&f, "Sound", "LoadShipEditInterfaceSounds", "()");
		}
	}
	Sf_Ask(&f, "Scrap", "ClosePack", "()");
	Snd_Own(&f, "Sound", "SetOutMusic", "()");
	return Sf_None(&f);
}

/*
 * SetPSLoop(Name, Pos, Snd, Volume=1, Pitch=0, AttenIni=0, AttenEnd=0, Doppler=1.0) and SetPSInterval(Name, Pos, Snd,
 * MinInterval, MaxInterval, Volume=1, ...): a point sound of the map, looping or played at random intervals
 */
static PyObject *Sound_SetPS(PyObject *args, PyObject *kw, byte interval) {
	static char *loopNames[] = { "Name", "Pos", "Snd", "Volume", "Pitch", "AttenIni", "AttenEnd", "Doppler", NULL };
	static char *intNames[] = { "Name", "Pos", "Snd", "MinInterval", "MaxInterval", "Volume", "Pitch", "AttenIni",
		"AttenEnd", "Doppler", NULL };
	PyObject *name, *pos, *snd, *minI = NULL, *maxI = NULL, *vol = NULL, *pitch = NULL, *ini = NULL, *end = NULL;
	PyObject *dopp = NULL;
	scriptFrame_t f = { 0 };
	if (interval ? !PyArg_ParseTupleAndKeywords(args, kw, "OOOOO|OOOOO", intNames, &name, &pos, &snd, &minI, &maxI,
			&vol, &pitch, &ini, &end, &dopp) :
		!PyArg_ParseTupleAndKeywords(args, kw, "OOO|OOOOO", loopNames, &name, &pos, &snd, &vol, &pitch, &ini, &end,
			&dopp)) {
		return NULL;
	}
	vol = vol ? vol : Sf_Int(&f, 1);
	pitch = pitch ? pitch : Sf_Int(&f, 0);
	ini = ini ? ini : Sf_Int(&f, 0);
	end = end ? end : Sf_Int(&f, 0);
	dopp = dopp ? dopp : Sf_Float(&f, 1.0);
	Sf_Ask(&f, "SSound", "CreatePS", "(O)", name);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Sound", snd);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "X", Sf_Item(&f, pos, 0));
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Y", Sf_Item(&f, pos, 1));
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Z", Sf_Item(&f, pos, 2));
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Volume", vol);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Pitch", pitch);
	Sf_Ask(&f, "SSound", "SetPS", "(Osi)", name, "Loop", interval ? 0 : 1);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "AttenIni", ini);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "AttenEnd", end);
	Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "Doppler", dopp);
	if (interval) {
		Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "MaxInterval", maxI);
		Sf_Ask(&f, "SSound", "SetPS", "(OsO)", name, "MinInterval", minI);
	}
	return Sf_None(&f);
}

static PyObject *Sound_SetPSLoop(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Sound_SetPS(args, kw, 0);
}

static PyObject *Sound_SetPSInterval(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Sound_SetPS(args, kw, 1);
}

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

/* `if isPC:` of the script (NameError when the module lost it), 1, 0 or -1 */
static int Snd_IsPC(scriptFrame_t *f) {
	return Sf_True(f, Sf_Global(f, "OutSound", "isPC"));
}

/* SetVehicleSounds(EntityName): the sound variables of a ship by its model, then the damage sounds of Sound */
static PyObject *OutSound_SetVehicleSounds(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "string");
	Sf_Import(&f, "Traffic");
	if (Snd_NotOutdoor(&f) != 0) {
		return Sf_None(&f);
	}
	PyObject *v = Sf_Entity(&f, name);
	Sf_SetAttrI(&f, v, "snd_n", 32);
	int pc = Snd_IsPC(&f);
	PyObject *model = Sf_Ask(&f, "string", "upper", "(O)", Sf_Attr(&f, v, "Model"));
	int armed = Sf_Compare(&f, Sf_Mod(&f, "Traffic", "PrevVal"), "==", Py_None);
	Snd_RunOut(&f, "SetVehicleSounds", Sf_Build(&f, "{s:O,s:O,s:i,s:i}", "name", name, "model", model, "pc", pc, "armed",
		armed));
	Sf_Import(&f, "Sound");
	Sf_Ask(&f, "Sound", "SetStdDamageSounds", "(O)", name);
	return Sf_None(&f);
}

/* the load sections, LoadEngines and LoadWeapons with $pc */
#define OUTSOUND_SECTION(fn, pcTest) \
	static PyObject *OutSound_##fn(PyObject *self, PyObject *args) { \
		scriptFrame_t f = { 0 }; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "")) { \
			return NULL; \
		} \
		Snd_RunOut(&f, #fn, pcTest ? Sf_Build(&f, "{s:i}", "pc", Snd_IsPC(&f)) : NULL); \
		return Sf_None(&f); \
	}

OUTSOUND_SECTION(LoadEngines, 1)
OUTSOUND_SECTION(LoadWeapons, 1)
OUTSOUND_SECTION(LoadScorer, 0)
OUTSOUND_SECTION(LoadMisc, 0)
OUTSOUND_SECTION(LoadCrash, 0)

/* LoadVehicles(): the vehicle sounds of an outdoor level, from the pack Sounds/Vehicles */
static PyObject *OutSound_LoadVehicles(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	if (Snd_NotOutdoor(&f) != 0) {
		return Sf_None(&f);
	}
	Sf_Print(&f, "s", "Loading vehicle sounds...");
	Sf_Ask(&f, "Scrap", "OpenPack", "(s)", "Sounds/Vehicles");
	Snd_Own(&f, "OutSound", "LoadEngines", "()");
	Snd_Own(&f, "OutSound", "LoadWeapons", "()");
	Snd_Own(&f, "OutSound", "LoadScorer", "()");
	Snd_Own(&f, "OutSound", "LoadMisc", "()");
	Snd_Own(&f, "OutSound", "LoadCrash", "()");
	Sf_Ask(&f, "Scrap", "ClosePack", "()");
	return Sf_None(&f);
}

/* Init(): the sound engine settings of the outdoor levels, then the vehicle sounds */
static PyObject *OutSound_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Print(&f, "s", "Loading outdor sounds...");
	Snd_RunOut(&f, "Init", NULL);
	Snd_Own(&f, "OutSound", "LoadVehicles", "()");
	return Sf_None(&f);
}

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

/* LoadSounds(): the door sounds of the level from SoundList, all of them (with an error) for a level not listed */
static PyObject *DoorSound_LoadSounds(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	PyObject *list = Sf_Global(&f, "DoorSound", "SoundList");
	PyObject *has = Sf_Attr(&f, list, "has_key");
	PyObject *map = Sf_Mod(&f, "Init", "UpperMap");
	int known = Sf_True(&f, Sf_Call(&f, has, "(O)", map));
	if (known > 0) {
		Snd_Own(&f, "DoorSound", "LoadSndList", "(O)", Sf_GetItem(&f, Sf_Global(&f, "DoorSound", "SoundList"),
			Sf_Mod(&f, "Init", "UpperMap")));
	} else if (known == 0) {
		Snd_Own(&f, "DoorSound", "LoadSndList", "(O)", Sf_Global(&f, "DoorSound", "AllSounds"));
		Sf_Ask(&f, "Scrap", "ConsoleError", "(O)", Sf_Cat(&f, "sOs", "S O N O R U S    E R R O R : ",
			Sf_Mod(&f, "Init", "UpperMap"), " not in DoorSound.py.\n"));
	}
	return Sf_None(&f);
}

/* LoadSndList(SndList): sorts the list in place and loads Doors/<name>.wav of each */
static PyObject *DoorSound_LoadSndList(PyObject *self, PyObject *args) {
	PyObject *list;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &list)) {
		return NULL;
	}
	Sf_Method(&f, list, "sort", "()");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *snd = Sf_Next(&f, list, i);
		if (!snd) {
			break;
		}
		Sf_Ask(&f, "SSound", "LoadSound", "(O)", Sf_Cat(&f, "sOs", "Doors/", snd, ".wav"));
		if (f.failed) {
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* run initsound <args>: SetInitDoorSound(args) through the module's globals, as SetSlide calls it (an error is left
 * set, it stops the file) */
static void DoorSound_StepInitSound(int id, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)id;
	Py_INCREF(args);
	Sf_CallArgs(&f, Sf_Global(&f, "DoorSound", "SetInitDoorSound"), args);
	Py_XDECREF(Sf_None(&f));
}

/* SetSlide(DoorClass): the sound events of a door class (the default open and close sounds for a class not listed) */
static PyObject *DoorSound_SetSlide(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "Chars");
	Snd_Run(&f, "sound/doorsound.def", doorsound_def, "SetSlide", Sf_Build(&f, "{s:O}", "class", cls));
	return Sf_None(&f);
}

/* SetInitDoorSound(DoorClass, ActionName, SoundName, Vol=1, Pitch=0): a sound at the start of an action */
static PyObject *DoorSound_SetInitDoorSound(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "DoorClass", "ActionName", "SoundName", "Vol", "Pitch", NULL };
	PyObject *cls, *act, *snd, *vol = NULL, *pitch = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|OO", names, &cls, &act, &snd, &vol, &pitch)) {
		return NULL;
	}
	vol = vol ? vol : Sf_Int(&f, 1);
	pitch = pitch ? pitch : Sf_Int(&f, 0);
	Sf_Ask(&f, "SAct", "GetClass", "(O)", cls);
	Sf_Ask(&f, "SAct", "GetAction", "(O)", act);
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "AddEventAtTime", 0);
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "EventName", Sf_Cat(&f, "OsOsO", snd, ":", Sf_Repr(&f, vol), ":",
		Sf_Repr(&f, pitch)));
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventFunc", "!AnmSoundEvent");
	return Sf_None(&f);
}

/* ConversorSetSounds(CharName): the sounds of the conversion point's action */
static PyObject *DoorSound_ConversorSetSounds(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Snd_Run(&f, "sound/doorsound.def", doorsound_def, "ConversorSetSounds", Sf_Build(&f, "{s:O}", "char", name));
	return Sf_None(&f);
}

PyMethodDef g_DoorSoundMethods[] = {
	{ "LoadSounds", DoorSound_LoadSounds, 1, NULL },
	{ "LoadSndList", DoorSound_LoadSndList, 1, NULL },
	{ "SetSlide", DoorSound_SetSlide, 1, NULL },
	{ "SetInitDoorSound", (PyCFunction)(void (*)(void))DoorSound_SetInitDoorSound, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "ConversorSetSounds", DoorSound_ConversorSetSounds, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
