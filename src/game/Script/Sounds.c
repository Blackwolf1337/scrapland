/*
 * The sound scripts of the characters (scripts/sound/<name>sound.py, issue #2 step 7d) as data: data/sound/<name>.def
 * holds the sound files of a class, the sound events of its actions and the loops of a new entity (see the header of
 * a file). The C here runs the sections: Init(CharName) of each module (MercenarySound: Init(CharClass, CharName)) and
 * InitLoops(EntityName) where the script has it. The module code (isPC) stays in the scripts.
 */
#include <string.h>

#include "scrap/script.h"

static const char bankdirectorsound_def[] = {
#embed "../../../data/sound/bankdirectorsound.def"
	, 0 };

static const char bankmastersound_def[] = {
#embed "../../../data/sound/bankmastersound.def"
	, 0 };

static const char bertosound_def[] = {
#embed "../../../data/sound/bertosound.def"
	, 0 };

static const char bettysound_def[] = {
#embed "../../../data/sound/bettysound.def"
	, 0 };

static const char bishopsound_def[] = {
#embed "../../../data/sound/bishopsound.def"
	, 0 };

static const char bosssound_def[] = {
#embed "../../../data/sound/bosssound.def"
	, 0 };

static const char crazygamblersound_def[] = {
#embed "../../../data/sound/crazygamblersound.def"
	, 0 };

static const char desktopsound_def[] = {
#embed "../../../data/sound/desktopsound.def"
	, 0 };

static const char dtritussound_def[] = {
#embed "../../../data/sound/dtritussound.def"
	, 0 };

static const char functionarysound_def[] = {
#embed "../../../data/sound/functionarysound.def"
	, 0 };

static const char gatekeepersound_def[] = {
#embed "../../../data/sound/gatekeepersound.def"
	, 0 };

static const char gearsound_def[] = {
#embed "../../../data/sound/gearsound.def"
	, 0 };

static const char humansound_def[] = {
#embed "../../../data/sound/humansound.def"
	, 0 };

static const char humphreysound_def[] = {
#embed "../../../data/sound/humphreysound.def"
	, 0 };

static const char maintenancesound_def[] = {
#embed "../../../data/sound/maintenancesound.def"
	, 0 };

static const char mayorsound_def[] = {
#embed "../../../data/sound/mayorsound.def"
	, 0 };

static const char mercenarysound_def[] = {
#embed "../../../data/sound/mercenarysound.def"
	, 0 };

static const char messengersound_def[] = {
#embed "../../../data/sound/messengersound.def"
	, 0 };

static const char nursesound_def[] = {
#embed "../../../data/sound/nursesound.def"
	, 0 };

static const char policebosssound_def[] = {
#embed "../../../data/sound/policebosssound.def"
	, 0 };

static const char policesound_def[] = {
#embed "../../../data/sound/policesound.def"
	, 0 };

static const char rustysound_def[] = {
#embed "../../../data/sound/rustysound.def"
	, 0 };

static const char sebastiansound_def[] = {
#embed "../../../data/sound/sebastiansound.def"
	, 0 };

static const char sentinelsound_def[] = {
#embed "../../../data/sound/sentinelsound.def"
	, 0 };

static const char sputnikjunkyardsound_def[] = {
#embed "../../../data/sound/sputnikjunkyardsound.def"
	, 0 };

static const char sputniksound_def[] = {
#embed "../../../data/sound/sputniksound.def"
	, 0 };

static const char testcsound_def[] = {
#embed "../../../data/sound/testcsound.def"
	, 0 };

static const scriptStep_t sound_steps[] = {
	{ "print", Script_StepPrint },
	{ NULL, NULL },
};

/* a section of a sound file with one value (char for Init, name for InitLoops) */
static PyObject *Sound_Run(const char *file, const char *builtin, const char *section, const char *var, PyObject *args) {
	PyObject *v;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:O}", var, v);
	Script_RunFile(0, file, section, builtin, sound_steps, vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

#define SOUND_INIT(mod, file) \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		return Sound_Run("sound/" #file ".def", file##_def, "Init", "char", args); \
	}

#define SOUND_LOOPS(mod, file) \
	static PyObject *mod##_InitLoops(PyObject *self, PyObject *args) { \
		(void)self; \
		return Sound_Run("sound/" #file ".def", file##_def, "InitLoops", "name", args); \
	}

/* Init only */
#define SOUND_MODULE0(mod, file) \
	SOUND_INIT(mod, file) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

/* Init and InitLoops */
#define SOUND_MODULE1(mod, file) \
	SOUND_INIT(mod, file) \
	SOUND_LOOPS(mod, file) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { "InitLoops", mod##_InitLoops, 1, NULL }, \
		{ NULL, NULL, 0, NULL } };

SOUND_MODULE1(BankDirectorSound, bankdirectorsound)
SOUND_MODULE1(BankMasterSound, bankmastersound)
SOUND_MODULE1(BertoSound, bertosound)
SOUND_MODULE1(BettySound, bettysound)
SOUND_MODULE1(BishopSound, bishopsound)
SOUND_MODULE1(BossSound, bosssound)
SOUND_MODULE1(CrazyGamblerSound, crazygamblersound)
SOUND_MODULE1(DesktopSound, desktopsound)
SOUND_MODULE1(DtritusSound, dtritussound)
SOUND_MODULE1(FunctionarySound, functionarysound)
SOUND_MODULE1(GateKeeperSound, gatekeepersound)
SOUND_MODULE1(GearSound, gearsound)
SOUND_MODULE0(HumanSound, humansound)
SOUND_MODULE0(HumphreySound, humphreysound)
SOUND_MODULE1(MaintenanceSound, maintenancesound)
SOUND_MODULE1(MayorSound, mayorsound)
SOUND_MODULE1(MessengerSound, messengersound)
SOUND_MODULE1(NurseSound, nursesound)
SOUND_MODULE0(PoliceBossSound, policebosssound)
SOUND_MODULE1(PoliceSound, policesound)
SOUND_MODULE1(RustySound, rustysound)
SOUND_MODULE1(SebastianSound, sebastiansound)
SOUND_MODULE1(SentinelSound, sentinelsound)
SOUND_MODULE1(SputnikJunkyardSound, sputnikjunkyardsound)
SOUND_MODULE1(SputnikSound, sputniksound)
SOUND_MODULE1(TestCSound, testcsound)

/* MercenarySound.Init(CharClass, CharName): the sounds of one mercenary for its class */
static PyObject *MercenarySound_Init(PyObject *self, PyObject *args) {
	PyObject *cls, *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &cls, &name)) {
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:O,s:O}", "class", cls, "char", name);
	Script_RunFile(0, "sound/mercenarysound.def", "Init", mercenarysound_def, sound_steps, vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

PyMethodDef g_MercenarySoundMethods[] = { { "Init", MercenarySound_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
