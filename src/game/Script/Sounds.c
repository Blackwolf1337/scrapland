/*
 * The sound scripts of the characters (scripts/sound/<name>sound.py, issue #2 step 7d) as data: data/sound/<name>.def
 * holds the sound files of a class, the sound events of its actions and the loops of a new entity (see the header of
 * a file). The C here runs the sections, one C function for each function of a script: Init(CharName) (MercenarySound:
 * Init(CharClass, CharName)), InitLoops(EntityName), LoadSounds(CharName), SetSounds(CharName) and the Talk sections
 * (CharName, ActName). The module code (isPC) stays in the scripts.
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

/*
 * LoadSounds(CharName), SetSounds(CharName) and the Talk sections (CharName, ActName): $char, and $talk for a Talk
 * section; they stop where the script stops (Scene.SetDefSounds execs <Type>Sound.SetSounds('<scene><name>'))
 */
static PyObject *Sound_Section(const char *file, const char *builtin, const char *section, PyObject *args, int talk) {
	PyObject *c, *a = NULL;
	if (!(talk ? PyArg_ParseTuple(args, "OO", &c, &a) : PyArg_ParseTuple(args, "O", &c))) {
		return NULL;
	}
	PyObject *vars = talk ? Py_BuildValue("{s:O,s:O}", "char", c, "talk", a) : Py_BuildValue("{s:O}", "char", c);
	byte ok = vars && Script_RunFileTry(0, file, section, builtin, sound_steps, vars);
	Py_XDECREF(vars);
	return ok ? Py_ReturnNone() : NULL;
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

/* a section with CharName (talk 0) or CharName, ActName (talk 1) */
#define SOUND_SECTION(mod, file, section, talk) \
	static PyObject *mod##_##section(PyObject *self, PyObject *args) { \
		(void)self; \
		return Sound_Section("sound/" #file ".def", file##_def, #section, args, talk); \
	}

#define SOUND_FN(mod, name) { #name, mod##_##name, 1, NULL }

/* every function of a script: its section */

SOUND_SECTION(BankDirectorSound, bankdirectorsound, LoadSounds, 0)
SOUND_SECTION(BankDirectorSound, bankdirectorsound, SetSounds, 0)
SOUND_SECTION(BankDirectorSound, bankdirectorsound, TalkSnd, 1)
SOUND_SECTION(BankDirectorSound, bankdirectorsound, Talk0Snd, 1)
SOUND_SECTION(BankDirectorSound, bankdirectorsound, Talk1Snd, 1)
SOUND_SECTION(BankDirectorSound, bankdirectorsound, Talk2Snd, 1)
SOUND_LOOPS(BankDirectorSound, bankdirectorsound)
SOUND_INIT(BankDirectorSound, bankdirectorsound)
PyMethodDef g_BankDirectorSoundMethods[] = {
	SOUND_FN(BankDirectorSound, LoadSounds), SOUND_FN(BankDirectorSound, SetSounds),
	SOUND_FN(BankDirectorSound, TalkSnd), SOUND_FN(BankDirectorSound, Talk0Snd),
	SOUND_FN(BankDirectorSound, Talk1Snd), SOUND_FN(BankDirectorSound, Talk2Snd),
	SOUND_FN(BankDirectorSound, InitLoops), SOUND_FN(BankDirectorSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(BankMasterSound, bankmastersound, LoadSounds, 0)
SOUND_SECTION(BankMasterSound, bankmastersound, SetSounds, 0)
SOUND_SECTION(BankMasterSound, bankmastersound, TalkSnd, 1)
SOUND_SECTION(BankMasterSound, bankmastersound, Talk0Snd, 1)
SOUND_SECTION(BankMasterSound, bankmastersound, Talk1Snd, 1)
SOUND_LOOPS(BankMasterSound, bankmastersound)
SOUND_INIT(BankMasterSound, bankmastersound)
PyMethodDef g_BankMasterSoundMethods[] = {
	SOUND_FN(BankMasterSound, LoadSounds), SOUND_FN(BankMasterSound, SetSounds),
	SOUND_FN(BankMasterSound, TalkSnd), SOUND_FN(BankMasterSound, Talk0Snd), SOUND_FN(BankMasterSound, Talk1Snd),
	SOUND_FN(BankMasterSound, InitLoops), SOUND_FN(BankMasterSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(BertoSound, bertosound, LoadSounds, 0)
SOUND_SECTION(BertoSound, bertosound, SetSounds, 0)
SOUND_SECTION(BertoSound, bertosound, TalkSnd, 1)
SOUND_SECTION(BertoSound, bertosound, Talk0Snd, 1)
SOUND_SECTION(BertoSound, bertosound, Talk1Snd, 1)
SOUND_SECTION(BertoSound, bertosound, Talk2Snd, 1)
SOUND_LOOPS(BertoSound, bertosound)
SOUND_INIT(BertoSound, bertosound)
PyMethodDef g_BertoSoundMethods[] = {
	SOUND_FN(BertoSound, LoadSounds), SOUND_FN(BertoSound, SetSounds), SOUND_FN(BertoSound, TalkSnd),
	SOUND_FN(BertoSound, Talk0Snd), SOUND_FN(BertoSound, Talk1Snd), SOUND_FN(BertoSound, Talk2Snd),
	SOUND_FN(BertoSound, InitLoops), SOUND_FN(BertoSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(BettySound, bettysound, LoadSounds, 0)
SOUND_SECTION(BettySound, bettysound, SetSounds, 0)
SOUND_SECTION(BettySound, bettysound, TalkSnd, 1)
SOUND_SECTION(BettySound, bettysound, Talk0Snd, 1)
SOUND_SECTION(BettySound, bettysound, Talk1Snd, 1)
SOUND_SECTION(BettySound, bettysound, Talk2Snd, 1)
SOUND_LOOPS(BettySound, bettysound)
SOUND_INIT(BettySound, bettysound)
PyMethodDef g_BettySoundMethods[] = {
	SOUND_FN(BettySound, LoadSounds), SOUND_FN(BettySound, SetSounds), SOUND_FN(BettySound, TalkSnd),
	SOUND_FN(BettySound, Talk0Snd), SOUND_FN(BettySound, Talk1Snd), SOUND_FN(BettySound, Talk2Snd),
	SOUND_FN(BettySound, InitLoops), SOUND_FN(BettySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(BishopSound, bishopsound, LoadSounds, 0)
SOUND_SECTION(BishopSound, bishopsound, SetSounds, 0)
SOUND_SECTION(BishopSound, bishopsound, TalkSnd, 1)
SOUND_SECTION(BishopSound, bishopsound, Talk0Snd, 1)
SOUND_SECTION(BishopSound, bishopsound, Talk1Snd, 1)
SOUND_SECTION(BishopSound, bishopsound, Talk2Snd, 1)
SOUND_LOOPS(BishopSound, bishopsound)
SOUND_INIT(BishopSound, bishopsound)
PyMethodDef g_BishopSoundMethods[] = {
	SOUND_FN(BishopSound, LoadSounds), SOUND_FN(BishopSound, SetSounds), SOUND_FN(BishopSound, TalkSnd),
	SOUND_FN(BishopSound, Talk0Snd), SOUND_FN(BishopSound, Talk1Snd), SOUND_FN(BishopSound, Talk2Snd),
	SOUND_FN(BishopSound, InitLoops), SOUND_FN(BishopSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(BossSound, bosssound, LoadSounds, 0)
SOUND_SECTION(BossSound, bosssound, SetSounds, 0)
SOUND_SECTION(BossSound, bosssound, TalkSnd, 1)
SOUND_SECTION(BossSound, bosssound, Talk0Snd, 1)
SOUND_SECTION(BossSound, bosssound, Talk1Snd, 1)
SOUND_SECTION(BossSound, bosssound, Talk2Snd, 1)
SOUND_LOOPS(BossSound, bosssound)
SOUND_INIT(BossSound, bosssound)
PyMethodDef g_BossSoundMethods[] = {
	SOUND_FN(BossSound, LoadSounds), SOUND_FN(BossSound, SetSounds), SOUND_FN(BossSound, TalkSnd),
	SOUND_FN(BossSound, Talk0Snd), SOUND_FN(BossSound, Talk1Snd), SOUND_FN(BossSound, Talk2Snd),
	SOUND_FN(BossSound, InitLoops), SOUND_FN(BossSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(CrazyGamblerSound, crazygamblersound, LoadSounds, 0)
SOUND_SECTION(CrazyGamblerSound, crazygamblersound, SetSounds, 0)
SOUND_SECTION(CrazyGamblerSound, crazygamblersound, TalkSnd, 1)
SOUND_SECTION(CrazyGamblerSound, crazygamblersound, Talk0Snd, 1)
SOUND_SECTION(CrazyGamblerSound, crazygamblersound, Talk1Snd, 1)
SOUND_SECTION(CrazyGamblerSound, crazygamblersound, Talk2Snd, 1)
SOUND_LOOPS(CrazyGamblerSound, crazygamblersound)
SOUND_INIT(CrazyGamblerSound, crazygamblersound)
PyMethodDef g_CrazyGamblerSoundMethods[] = {
	SOUND_FN(CrazyGamblerSound, LoadSounds), SOUND_FN(CrazyGamblerSound, SetSounds),
	SOUND_FN(CrazyGamblerSound, TalkSnd), SOUND_FN(CrazyGamblerSound, Talk0Snd),
	SOUND_FN(CrazyGamblerSound, Talk1Snd), SOUND_FN(CrazyGamblerSound, Talk2Snd),
	SOUND_FN(CrazyGamblerSound, InitLoops), SOUND_FN(CrazyGamblerSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(DesktopSound, desktopsound, LoadSounds, 0)
SOUND_SECTION(DesktopSound, desktopsound, SetSounds, 0)
SOUND_SECTION(DesktopSound, desktopsound, TalkSnd, 1)
SOUND_SECTION(DesktopSound, desktopsound, Talk0Snd, 1)
SOUND_LOOPS(DesktopSound, desktopsound)
SOUND_INIT(DesktopSound, desktopsound)
PyMethodDef g_DesktopSoundMethods[] = {
	SOUND_FN(DesktopSound, LoadSounds), SOUND_FN(DesktopSound, SetSounds), SOUND_FN(DesktopSound, TalkSnd),
	SOUND_FN(DesktopSound, Talk0Snd), SOUND_FN(DesktopSound, InitLoops), SOUND_FN(DesktopSound, Init),
	{ NULL, NULL, 0, NULL }
};

SOUND_SECTION(DtritusSound, dtritussound, LoadSounds, 0)
SOUND_SECTION(DtritusSound, dtritussound, SetSounds, 0)
SOUND_SECTION(DtritusSound, dtritussound, TalkSnd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk0Snd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk1Snd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk2Snd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk3Snd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk4Snd, 1)
SOUND_SECTION(DtritusSound, dtritussound, Talk5Snd, 1)
SOUND_LOOPS(DtritusSound, dtritussound)
SOUND_INIT(DtritusSound, dtritussound)
PyMethodDef g_DtritusSoundMethods[] = {
	SOUND_FN(DtritusSound, LoadSounds), SOUND_FN(DtritusSound, SetSounds), SOUND_FN(DtritusSound, TalkSnd),
	SOUND_FN(DtritusSound, Talk0Snd), SOUND_FN(DtritusSound, Talk1Snd), SOUND_FN(DtritusSound, Talk2Snd),
	SOUND_FN(DtritusSound, Talk3Snd), SOUND_FN(DtritusSound, Talk4Snd), SOUND_FN(DtritusSound, Talk5Snd),
	SOUND_FN(DtritusSound, InitLoops), SOUND_FN(DtritusSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(FunctionarySound, functionarysound, LoadSounds, 0)
SOUND_SECTION(FunctionarySound, functionarysound, SetSounds, 0)
SOUND_SECTION(FunctionarySound, functionarysound, TalkSnd, 1)
SOUND_SECTION(FunctionarySound, functionarysound, Talk0Snd, 1)
SOUND_SECTION(FunctionarySound, functionarysound, Talk2Snd, 1)
SOUND_SECTION(FunctionarySound, functionarysound, Talk1Snd, 1)
SOUND_LOOPS(FunctionarySound, functionarysound)
SOUND_INIT(FunctionarySound, functionarysound)
PyMethodDef g_FunctionarySoundMethods[] = {
	SOUND_FN(FunctionarySound, LoadSounds), SOUND_FN(FunctionarySound, SetSounds),
	SOUND_FN(FunctionarySound, TalkSnd), SOUND_FN(FunctionarySound, Talk0Snd),
	SOUND_FN(FunctionarySound, Talk2Snd), SOUND_FN(FunctionarySound, Talk1Snd),
	SOUND_FN(FunctionarySound, InitLoops), SOUND_FN(FunctionarySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(GateKeeperSound, gatekeepersound, LoadSounds, 0)
SOUND_SECTION(GateKeeperSound, gatekeepersound, SetSounds, 0)
SOUND_SECTION(GateKeeperSound, gatekeepersound, TalkSnd, 1)
SOUND_SECTION(GateKeeperSound, gatekeepersound, TalkNoSnd, 1)
SOUND_LOOPS(GateKeeperSound, gatekeepersound)
SOUND_INIT(GateKeeperSound, gatekeepersound)
PyMethodDef g_GateKeeperSoundMethods[] = {
	SOUND_FN(GateKeeperSound, LoadSounds), SOUND_FN(GateKeeperSound, SetSounds),
	SOUND_FN(GateKeeperSound, TalkSnd), SOUND_FN(GateKeeperSound, TalkNoSnd),
	SOUND_FN(GateKeeperSound, InitLoops), SOUND_FN(GateKeeperSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(GearSound, gearsound, LoadSounds, 0)
SOUND_SECTION(GearSound, gearsound, SetSounds, 0)
SOUND_SECTION(GearSound, gearsound, TalkSnd, 1)
SOUND_INIT(GearSound, gearsound)
SOUND_LOOPS(GearSound, gearsound)
PyMethodDef g_GearSoundMethods[] = {
	SOUND_FN(GearSound, LoadSounds), SOUND_FN(GearSound, SetSounds), SOUND_FN(GearSound, TalkSnd),
	SOUND_FN(GearSound, Init), SOUND_FN(GearSound, InitLoops), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(HumanSound, humansound, LoadSounds, 0)
SOUND_SECTION(HumanSound, humansound, SetSounds, 0)
SOUND_SECTION(HumanSound, humansound, TalkSnd, 1)
SOUND_SECTION(HumanSound, humansound, Talk0Snd, 1)
SOUND_INIT(HumanSound, humansound)
PyMethodDef g_HumanSoundMethods[] = {
	SOUND_FN(HumanSound, LoadSounds), SOUND_FN(HumanSound, SetSounds), SOUND_FN(HumanSound, TalkSnd),
	SOUND_FN(HumanSound, Talk0Snd), SOUND_FN(HumanSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(HumphreySound, humphreysound, LoadSounds, 0)
SOUND_SECTION(HumphreySound, humphreysound, SetSounds, 0)
SOUND_SECTION(HumphreySound, humphreysound, TalkSnd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk0Snd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk1Snd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk2Snd, 1)
SOUND_INIT(HumphreySound, humphreysound)
PyMethodDef g_HumphreySoundMethods[] = {
	SOUND_FN(HumphreySound, LoadSounds), SOUND_FN(HumphreySound, SetSounds), SOUND_FN(HumphreySound, TalkSnd),
	SOUND_FN(HumphreySound, Talk0Snd), SOUND_FN(HumphreySound, Talk1Snd), SOUND_FN(HumphreySound, Talk2Snd),
	SOUND_FN(HumphreySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(MaintenanceSound, maintenancesound, LoadSounds, 0)
SOUND_SECTION(MaintenanceSound, maintenancesound, SetSounds, 0)
SOUND_SECTION(MaintenanceSound, maintenancesound, TalkSnd, 1)
SOUND_LOOPS(MaintenanceSound, maintenancesound)
SOUND_INIT(MaintenanceSound, maintenancesound)
PyMethodDef g_MaintenanceSoundMethods[] = {
	SOUND_FN(MaintenanceSound, LoadSounds), SOUND_FN(MaintenanceSound, SetSounds),
	SOUND_FN(MaintenanceSound, TalkSnd), SOUND_FN(MaintenanceSound, InitLoops), SOUND_FN(MaintenanceSound, Init),
	{ NULL, NULL, 0, NULL }
};

SOUND_SECTION(MayorSound, mayorsound, LoadSounds, 0)
SOUND_SECTION(MayorSound, mayorsound, SetSounds, 0)
SOUND_SECTION(MayorSound, mayorsound, TalkSnd, 1)
SOUND_SECTION(MayorSound, mayorsound, Talk0Snd, 1)
SOUND_SECTION(MayorSound, mayorsound, Talk1Snd, 1)
SOUND_SECTION(MayorSound, mayorsound, Talk2Snd, 1)
SOUND_LOOPS(MayorSound, mayorsound)
SOUND_INIT(MayorSound, mayorsound)
PyMethodDef g_MayorSoundMethods[] = {
	SOUND_FN(MayorSound, LoadSounds), SOUND_FN(MayorSound, SetSounds), SOUND_FN(MayorSound, TalkSnd),
	SOUND_FN(MayorSound, Talk0Snd), SOUND_FN(MayorSound, Talk1Snd), SOUND_FN(MayorSound, Talk2Snd),
	SOUND_FN(MayorSound, InitLoops), SOUND_FN(MayorSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(MessengerSound, messengersound, LoadSounds, 0)
SOUND_SECTION(MessengerSound, messengersound, SetSounds, 0)
SOUND_SECTION(MessengerSound, messengersound, TalkSnd, 1)
SOUND_SECTION(MessengerSound, messengersound, Talk0Snd, 1)
SOUND_SECTION(MessengerSound, messengersound, Talk1Snd, 1)
SOUND_LOOPS(MessengerSound, messengersound)
SOUND_INIT(MessengerSound, messengersound)
PyMethodDef g_MessengerSoundMethods[] = {
	SOUND_FN(MessengerSound, LoadSounds), SOUND_FN(MessengerSound, SetSounds), SOUND_FN(MessengerSound, TalkSnd),
	SOUND_FN(MessengerSound, Talk0Snd), SOUND_FN(MessengerSound, Talk1Snd), SOUND_FN(MessengerSound, InitLoops),
	SOUND_FN(MessengerSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(NurseSound, nursesound, LoadSounds, 0)
SOUND_SECTION(NurseSound, nursesound, SetSounds, 0)
SOUND_SECTION(NurseSound, nursesound, TalkSnd, 1)
SOUND_SECTION(NurseSound, nursesound, Talk0Snd, 1)
SOUND_SECTION(NurseSound, nursesound, Talk1Snd, 1)
SOUND_LOOPS(NurseSound, nursesound)
SOUND_INIT(NurseSound, nursesound)
PyMethodDef g_NurseSoundMethods[] = {
	SOUND_FN(NurseSound, LoadSounds), SOUND_FN(NurseSound, SetSounds), SOUND_FN(NurseSound, TalkSnd),
	SOUND_FN(NurseSound, Talk0Snd), SOUND_FN(NurseSound, Talk1Snd), SOUND_FN(NurseSound, InitLoops),
	SOUND_FN(NurseSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(PoliceBossSound, policebosssound, LoadSounds, 0)
SOUND_SECTION(PoliceBossSound, policebosssound, SetSounds, 0)
SOUND_SECTION(PoliceBossSound, policebosssound, TalkSnd, 1)
SOUND_SECTION(PoliceBossSound, policebosssound, Talk0Snd, 1)
SOUND_SECTION(PoliceBossSound, policebosssound, Talk1Snd, 1)
SOUND_SECTION(PoliceBossSound, policebosssound, Talk2Snd, 1)
SOUND_INIT(PoliceBossSound, policebosssound)
PyMethodDef g_PoliceBossSoundMethods[] = {
	SOUND_FN(PoliceBossSound, LoadSounds), SOUND_FN(PoliceBossSound, SetSounds),
	SOUND_FN(PoliceBossSound, TalkSnd), SOUND_FN(PoliceBossSound, Talk0Snd), SOUND_FN(PoliceBossSound, Talk1Snd),
	SOUND_FN(PoliceBossSound, Talk2Snd), SOUND_FN(PoliceBossSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(PoliceSound, policesound, LoadSounds, 0)
SOUND_SECTION(PoliceSound, policesound, SetSounds, 0)
SOUND_SECTION(PoliceSound, policesound, TalkSnd, 1)
SOUND_SECTION(PoliceSound, policesound, Talk0Snd, 1)
SOUND_SECTION(PoliceSound, policesound, Talk1Snd, 1)
SOUND_SECTION(PoliceSound, policesound, Talk2Snd, 1)
SOUND_LOOPS(PoliceSound, policesound)
SOUND_INIT(PoliceSound, policesound)
PyMethodDef g_PoliceSoundMethods[] = {
	SOUND_FN(PoliceSound, LoadSounds), SOUND_FN(PoliceSound, SetSounds), SOUND_FN(PoliceSound, TalkSnd),
	SOUND_FN(PoliceSound, Talk0Snd), SOUND_FN(PoliceSound, Talk1Snd), SOUND_FN(PoliceSound, Talk2Snd),
	SOUND_FN(PoliceSound, InitLoops), SOUND_FN(PoliceSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(RustySound, rustysound, LoadSounds, 0)
SOUND_SECTION(RustySound, rustysound, SetSounds, 0)
SOUND_SECTION(RustySound, rustysound, TalkSnd, 1)
SOUND_SECTION(RustySound, rustysound, Talk0Snd, 1)
SOUND_SECTION(RustySound, rustysound, Talk1Snd, 1)
SOUND_SECTION(RustySound, rustysound, Talk2Snd, 1)
SOUND_LOOPS(RustySound, rustysound)
SOUND_INIT(RustySound, rustysound)
PyMethodDef g_RustySoundMethods[] = {
	SOUND_FN(RustySound, LoadSounds), SOUND_FN(RustySound, SetSounds), SOUND_FN(RustySound, TalkSnd),
	SOUND_FN(RustySound, Talk0Snd), SOUND_FN(RustySound, Talk1Snd), SOUND_FN(RustySound, Talk2Snd),
	SOUND_FN(RustySound, InitLoops), SOUND_FN(RustySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(SebastianSound, sebastiansound, LoadSounds, 0)
SOUND_SECTION(SebastianSound, sebastiansound, SetSounds, 0)
SOUND_SECTION(SebastianSound, sebastiansound, TalkSnd, 1)
SOUND_SECTION(SebastianSound, sebastiansound, Talk1Snd, 1)
SOUND_SECTION(SebastianSound, sebastiansound, Talk2Snd, 1)
SOUND_SECTION(SebastianSound, sebastiansound, Talk3Snd, 1)
SOUND_SECTION(SebastianSound, sebastiansound, Talk4Snd, 1)
SOUND_LOOPS(SebastianSound, sebastiansound)
SOUND_INIT(SebastianSound, sebastiansound)
PyMethodDef g_SebastianSoundMethods[] = {
	SOUND_FN(SebastianSound, LoadSounds), SOUND_FN(SebastianSound, SetSounds), SOUND_FN(SebastianSound, TalkSnd),
	SOUND_FN(SebastianSound, Talk1Snd), SOUND_FN(SebastianSound, Talk2Snd), SOUND_FN(SebastianSound, Talk3Snd),
	SOUND_FN(SebastianSound, Talk4Snd), SOUND_FN(SebastianSound, InitLoops), SOUND_FN(SebastianSound, Init),
	{ NULL, NULL, 0, NULL }
};

SOUND_SECTION(SentinelSound, sentinelsound, LoadSounds, 0)
SOUND_SECTION(SentinelSound, sentinelsound, SetSounds, 0)
SOUND_SECTION(SentinelSound, sentinelsound, TalkSnd, 1)
SOUND_SECTION(SentinelSound, sentinelsound, Talk0Snd, 1)
SOUND_SECTION(SentinelSound, sentinelsound, Talk1Snd, 1)
SOUND_LOOPS(SentinelSound, sentinelsound)
SOUND_INIT(SentinelSound, sentinelsound)
PyMethodDef g_SentinelSoundMethods[] = {
	SOUND_FN(SentinelSound, LoadSounds), SOUND_FN(SentinelSound, SetSounds), SOUND_FN(SentinelSound, TalkSnd),
	SOUND_FN(SentinelSound, Talk0Snd), SOUND_FN(SentinelSound, Talk1Snd), SOUND_FN(SentinelSound, InitLoops),
	SOUND_FN(SentinelSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, LoadSounds, 0)
SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, SetSounds, 0)
SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, TalkSnd, 1)
SOUND_LOOPS(SputnikJunkyardSound, sputnikjunkyardsound)
SOUND_INIT(SputnikJunkyardSound, sputnikjunkyardsound)
PyMethodDef g_SputnikJunkyardSoundMethods[] = {
	SOUND_FN(SputnikJunkyardSound, LoadSounds), SOUND_FN(SputnikJunkyardSound, SetSounds),
	SOUND_FN(SputnikJunkyardSound, TalkSnd), SOUND_FN(SputnikJunkyardSound, InitLoops),
	SOUND_FN(SputnikJunkyardSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(SputnikSound, sputniksound, LoadSounds, 0)
SOUND_SECTION(SputnikSound, sputniksound, SetSounds, 0)
SOUND_SECTION(SputnikSound, sputniksound, TalkSnd, 1)
SOUND_LOOPS(SputnikSound, sputniksound)
SOUND_INIT(SputnikSound, sputniksound)
PyMethodDef g_SputnikSoundMethods[] = {
	SOUND_FN(SputnikSound, LoadSounds), SOUND_FN(SputnikSound, SetSounds), SOUND_FN(SputnikSound, TalkSnd),
	SOUND_FN(SputnikSound, InitLoops), SOUND_FN(SputnikSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(TestCSound, testcsound, LoadSounds, 0)
SOUND_SECTION(TestCSound, testcsound, SetSounds, 0)
SOUND_SECTION(TestCSound, testcsound, TalkSnd, 1)
SOUND_LOOPS(TestCSound, testcsound)
SOUND_INIT(TestCSound, testcsound)
PyMethodDef g_TestCSoundMethods[] = {
	SOUND_FN(TestCSound, LoadSounds), SOUND_FN(TestCSound, SetSounds), SOUND_FN(TestCSound, TalkSnd),
	SOUND_FN(TestCSound, InitLoops), SOUND_FN(TestCSound, Init), { NULL, NULL, 0, NULL }
};

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

SOUND_SECTION(MercenarySound, mercenarysound, LoadSounds, 0)
SOUND_SECTION(MercenarySound, mercenarysound, SetSounds, 0)
PyMethodDef g_MercenarySoundMethods[] = {
	SOUND_FN(MercenarySound, LoadSounds), SOUND_FN(MercenarySound, SetSounds), SOUND_FN(MercenarySound, Init),
	{ NULL, NULL, 0, NULL }
};
