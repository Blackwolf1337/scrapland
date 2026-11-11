/*
 * The sound scripts of the characters (scripts/sound/<name>sound.py, issue #2 step 7d) as data: data/sound/<name>.def
 * holds the sound files of a class, the sound events of its actions and the loops of a new entity (see the header of
 * a file). The C here runs the sections, one C function for each function of a script: Init(CharName) (MercenarySound:
 * Init(CharClass, CharName)), InitLoops(EntityName), LoadSounds(CharName), SetSounds(CharName) and the Talk sections
 * (CharName, ActName). The module code (isPC) stays in the scripts.
 *
 * Native C (docs/objlib-plan.md, stage 2): the module has no state and calls no engine function itself; the sections
 * call the engine through the .def runner, which takes its $names (and the entity handle $e of InitLoops) as a dict
 * until it takes C values (OBJLIB 6). InitLoops has a native entry for the direct calls of other ports (stage 3,
 * PORT_CALL); it hands its argument on as the Python value.
 */
#include <string.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

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

/*
 * the section Init with vars (given away; $char CharName): it stops where the script stops. The script's first line,
 * Scrap.OpenPack("Sounds/Chars/"+CharName), raises for a CharName that is no string before anything runs ($(char) of
 * the file would take any value as text)
 */
static byte Sound_RunInit(const char *file, const char *builtin, const nativeVal_t *cls, const nativeVal_t *name) {
	/* "Sounds/Chars/" + CharName: string_concat refuses a value that is no text (a class instance with __radd__ is
	 * refused too) */
	return (name->type == NV_STR || Native_Raise(NE_TYPE, "illegal argument type for built-in operation")) &&
		Mode_DefRun(0, file, "Init", builtin, sound_steps, 1, cls ? 2 : 1, "char", name, "class", cls);
}

/* InitLoops(EntityName): e = Scrap.GetEntity(EntityName) through its ref (a case may swap it), then the attribute
 * sets of the section on $e (the handle value) */
static byte Sound_Loops(const char *file, const char *builtin, const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	byte ok = PORT_CALL(Scrap, GetEntity, &e, "v", name) &&
		Mode_DefRun(0, file, "InitLoops", builtin, sound_steps, 1, 2, "name", name, "e", &e);
	Native_Free(&e);
	return ok;
}

/*
 * LoadSounds(CharName), SetSounds(CharName) and the Talk sections (CharName, ActName): $char, and $talk for a Talk
 * section (talk NULL: none); they stop where the script stops (Scene.SetDefSounds execs
 * <Type>Sound.SetSounds('<scene><name>'))
 */
static byte Sound_Section(const char *file, const char *builtin, const char *section, const nativeVal_t *c,
	const nativeVal_t *talk) {
	return Mode_DefRun(0, file, section, builtin, sound_steps, 1, talk ? 2 : 1, "char", c, "talk", talk);
}

/* Init(CharName) */
#define SOUND_INIT(mod, file) \
	MODE_FN(mod, Init, 1, Sound_RunInit("sound/" #file ".def", file##_def, NULL, &a[0]))

/* InitLoops(EntityName) */
#define SOUND_LOOPS(mod, file) \
	MODE_FN(mod, InitLoops, 1, Sound_Loops("sound/" #file ".def", file##_def, &a[0]))

/* a section with CharName (talk 0) or CharName, ActName (talk 1): keyword functions (MODE_KW: the parameter names of
 * the script) */
#define SOUND_SECTION0(mod, file, section) \
	MODE_KW(mod, section, "O", "CharName") { \
		return Sound_Section("sound/" #file ".def", file##_def, #section, &v[0], NULL); \
	}
#define SOUND_SECTION1(mod, file, section) \
	MODE_KW(mod, section, "OO", "CharName", "ActName") { \
		return Sound_Section("sound/" #file ".def", file##_def, #section, &v[0], &v[1]); \
	}
#define SOUND_SECTION(mod, file, section, talk) SOUND_SECTION##talk(mod, file, section)

#define SOUND_FN(mod, name) { #name, mod##_##name, 1, NULL }
/* the sections take keywords (the parameter names of the script) */
#define SOUND_KW(mod, name) { #name, (PyCFunction)(void (*)(void))mod##_##name, METH_VARARGS | METH_KEYWORDS, NULL }

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
	SOUND_KW(BankDirectorSound, LoadSounds), SOUND_KW(BankDirectorSound, SetSounds),
	SOUND_KW(BankDirectorSound, TalkSnd), SOUND_KW(BankDirectorSound, Talk0Snd),
	SOUND_KW(BankDirectorSound, Talk1Snd), SOUND_KW(BankDirectorSound, Talk2Snd),
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
	SOUND_KW(BankMasterSound, LoadSounds), SOUND_KW(BankMasterSound, SetSounds),
	SOUND_KW(BankMasterSound, TalkSnd), SOUND_KW(BankMasterSound, Talk0Snd), SOUND_KW(BankMasterSound, Talk1Snd),
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
	SOUND_KW(BertoSound, LoadSounds), SOUND_KW(BertoSound, SetSounds), SOUND_KW(BertoSound, TalkSnd),
	SOUND_KW(BertoSound, Talk0Snd), SOUND_KW(BertoSound, Talk1Snd), SOUND_KW(BertoSound, Talk2Snd),
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
	SOUND_KW(BettySound, LoadSounds), SOUND_KW(BettySound, SetSounds), SOUND_KW(BettySound, TalkSnd),
	SOUND_KW(BettySound, Talk0Snd), SOUND_KW(BettySound, Talk1Snd), SOUND_KW(BettySound, Talk2Snd),
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
	SOUND_KW(BishopSound, LoadSounds), SOUND_KW(BishopSound, SetSounds), SOUND_KW(BishopSound, TalkSnd),
	SOUND_KW(BishopSound, Talk0Snd), SOUND_KW(BishopSound, Talk1Snd), SOUND_KW(BishopSound, Talk2Snd),
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
	SOUND_KW(BossSound, LoadSounds), SOUND_KW(BossSound, SetSounds), SOUND_KW(BossSound, TalkSnd),
	SOUND_KW(BossSound, Talk0Snd), SOUND_KW(BossSound, Talk1Snd), SOUND_KW(BossSound, Talk2Snd),
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
	SOUND_KW(CrazyGamblerSound, LoadSounds), SOUND_KW(CrazyGamblerSound, SetSounds),
	SOUND_KW(CrazyGamblerSound, TalkSnd), SOUND_KW(CrazyGamblerSound, Talk0Snd),
	SOUND_KW(CrazyGamblerSound, Talk1Snd), SOUND_KW(CrazyGamblerSound, Talk2Snd),
	SOUND_FN(CrazyGamblerSound, InitLoops), SOUND_FN(CrazyGamblerSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(DesktopSound, desktopsound, LoadSounds, 0)
SOUND_SECTION(DesktopSound, desktopsound, SetSounds, 0)
SOUND_SECTION(DesktopSound, desktopsound, TalkSnd, 1)
SOUND_SECTION(DesktopSound, desktopsound, Talk0Snd, 1)
SOUND_LOOPS(DesktopSound, desktopsound)
SOUND_INIT(DesktopSound, desktopsound)
PyMethodDef g_DesktopSoundMethods[] = {
	SOUND_KW(DesktopSound, LoadSounds), SOUND_KW(DesktopSound, SetSounds), SOUND_KW(DesktopSound, TalkSnd),
	SOUND_KW(DesktopSound, Talk0Snd), SOUND_FN(DesktopSound, InitLoops), SOUND_FN(DesktopSound, Init),
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
	SOUND_KW(DtritusSound, LoadSounds), SOUND_KW(DtritusSound, SetSounds), SOUND_KW(DtritusSound, TalkSnd),
	SOUND_KW(DtritusSound, Talk0Snd), SOUND_KW(DtritusSound, Talk1Snd), SOUND_KW(DtritusSound, Talk2Snd),
	SOUND_KW(DtritusSound, Talk3Snd), SOUND_KW(DtritusSound, Talk4Snd), SOUND_KW(DtritusSound, Talk5Snd),
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
	SOUND_KW(FunctionarySound, LoadSounds), SOUND_KW(FunctionarySound, SetSounds),
	SOUND_KW(FunctionarySound, TalkSnd), SOUND_KW(FunctionarySound, Talk0Snd),
	SOUND_KW(FunctionarySound, Talk2Snd), SOUND_KW(FunctionarySound, Talk1Snd),
	SOUND_FN(FunctionarySound, InitLoops), SOUND_FN(FunctionarySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(GateKeeperSound, gatekeepersound, LoadSounds, 0)
SOUND_SECTION(GateKeeperSound, gatekeepersound, SetSounds, 0)
SOUND_SECTION(GateKeeperSound, gatekeepersound, TalkSnd, 1)
SOUND_SECTION(GateKeeperSound, gatekeepersound, TalkNoSnd, 1)
SOUND_LOOPS(GateKeeperSound, gatekeepersound)
SOUND_INIT(GateKeeperSound, gatekeepersound)
PyMethodDef g_GateKeeperSoundMethods[] = {
	SOUND_KW(GateKeeperSound, LoadSounds), SOUND_KW(GateKeeperSound, SetSounds),
	SOUND_KW(GateKeeperSound, TalkSnd), SOUND_KW(GateKeeperSound, TalkNoSnd),
	SOUND_FN(GateKeeperSound, InitLoops), SOUND_FN(GateKeeperSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(GearSound, gearsound, LoadSounds, 0)
SOUND_SECTION(GearSound, gearsound, SetSounds, 0)
SOUND_SECTION(GearSound, gearsound, TalkSnd, 1)
SOUND_INIT(GearSound, gearsound)
SOUND_LOOPS(GearSound, gearsound)
PyMethodDef g_GearSoundMethods[] = {
	SOUND_KW(GearSound, LoadSounds), SOUND_KW(GearSound, SetSounds), SOUND_KW(GearSound, TalkSnd),
	SOUND_FN(GearSound, Init), SOUND_FN(GearSound, InitLoops), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(HumanSound, humansound, LoadSounds, 0)
SOUND_SECTION(HumanSound, humansound, SetSounds, 0)
SOUND_SECTION(HumanSound, humansound, TalkSnd, 1)
SOUND_SECTION(HumanSound, humansound, Talk0Snd, 1)
SOUND_INIT(HumanSound, humansound)
PyMethodDef g_HumanSoundMethods[] = {
	SOUND_KW(HumanSound, LoadSounds), SOUND_KW(HumanSound, SetSounds), SOUND_KW(HumanSound, TalkSnd),
	SOUND_KW(HumanSound, Talk0Snd), SOUND_FN(HumanSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(HumphreySound, humphreysound, LoadSounds, 0)
SOUND_SECTION(HumphreySound, humphreysound, SetSounds, 0)
SOUND_SECTION(HumphreySound, humphreysound, TalkSnd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk0Snd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk1Snd, 1)
SOUND_SECTION(HumphreySound, humphreysound, Talk2Snd, 1)
SOUND_INIT(HumphreySound, humphreysound)
PyMethodDef g_HumphreySoundMethods[] = {
	SOUND_KW(HumphreySound, LoadSounds), SOUND_KW(HumphreySound, SetSounds), SOUND_KW(HumphreySound, TalkSnd),
	SOUND_KW(HumphreySound, Talk0Snd), SOUND_KW(HumphreySound, Talk1Snd), SOUND_KW(HumphreySound, Talk2Snd),
	SOUND_FN(HumphreySound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(MaintenanceSound, maintenancesound, LoadSounds, 0)
SOUND_SECTION(MaintenanceSound, maintenancesound, SetSounds, 0)
SOUND_SECTION(MaintenanceSound, maintenancesound, TalkSnd, 1)
SOUND_LOOPS(MaintenanceSound, maintenancesound)
SOUND_INIT(MaintenanceSound, maintenancesound)
PyMethodDef g_MaintenanceSoundMethods[] = {
	SOUND_KW(MaintenanceSound, LoadSounds), SOUND_KW(MaintenanceSound, SetSounds),
	SOUND_KW(MaintenanceSound, TalkSnd), SOUND_FN(MaintenanceSound, InitLoops), SOUND_FN(MaintenanceSound, Init),
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
	SOUND_KW(MayorSound, LoadSounds), SOUND_KW(MayorSound, SetSounds), SOUND_KW(MayorSound, TalkSnd),
	SOUND_KW(MayorSound, Talk0Snd), SOUND_KW(MayorSound, Talk1Snd), SOUND_KW(MayorSound, Talk2Snd),
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
	SOUND_KW(MessengerSound, LoadSounds), SOUND_KW(MessengerSound, SetSounds), SOUND_KW(MessengerSound, TalkSnd),
	SOUND_KW(MessengerSound, Talk0Snd), SOUND_KW(MessengerSound, Talk1Snd), SOUND_FN(MessengerSound, InitLoops),
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
	SOUND_KW(NurseSound, LoadSounds), SOUND_KW(NurseSound, SetSounds), SOUND_KW(NurseSound, TalkSnd),
	SOUND_KW(NurseSound, Talk0Snd), SOUND_KW(NurseSound, Talk1Snd), SOUND_FN(NurseSound, InitLoops),
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
	SOUND_KW(PoliceBossSound, LoadSounds), SOUND_KW(PoliceBossSound, SetSounds),
	SOUND_KW(PoliceBossSound, TalkSnd), SOUND_KW(PoliceBossSound, Talk0Snd), SOUND_KW(PoliceBossSound, Talk1Snd),
	SOUND_KW(PoliceBossSound, Talk2Snd), SOUND_FN(PoliceBossSound, Init), { NULL, NULL, 0, NULL }
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
	SOUND_KW(PoliceSound, LoadSounds), SOUND_KW(PoliceSound, SetSounds), SOUND_KW(PoliceSound, TalkSnd),
	SOUND_KW(PoliceSound, Talk0Snd), SOUND_KW(PoliceSound, Talk1Snd), SOUND_KW(PoliceSound, Talk2Snd),
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
	SOUND_KW(RustySound, LoadSounds), SOUND_KW(RustySound, SetSounds), SOUND_KW(RustySound, TalkSnd),
	SOUND_KW(RustySound, Talk0Snd), SOUND_KW(RustySound, Talk1Snd), SOUND_KW(RustySound, Talk2Snd),
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
	SOUND_KW(SebastianSound, LoadSounds), SOUND_KW(SebastianSound, SetSounds), SOUND_KW(SebastianSound, TalkSnd),
	SOUND_KW(SebastianSound, Talk1Snd), SOUND_KW(SebastianSound, Talk2Snd), SOUND_KW(SebastianSound, Talk3Snd),
	SOUND_KW(SebastianSound, Talk4Snd), SOUND_FN(SebastianSound, InitLoops), SOUND_FN(SebastianSound, Init),
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
	SOUND_KW(SentinelSound, LoadSounds), SOUND_KW(SentinelSound, SetSounds), SOUND_KW(SentinelSound, TalkSnd),
	SOUND_KW(SentinelSound, Talk0Snd), SOUND_KW(SentinelSound, Talk1Snd), SOUND_FN(SentinelSound, InitLoops),
	SOUND_FN(SentinelSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, LoadSounds, 0)
SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, SetSounds, 0)
SOUND_SECTION(SputnikJunkyardSound, sputnikjunkyardsound, TalkSnd, 1)
SOUND_LOOPS(SputnikJunkyardSound, sputnikjunkyardsound)
SOUND_INIT(SputnikJunkyardSound, sputnikjunkyardsound)
PyMethodDef g_SputnikJunkyardSoundMethods[] = {
	SOUND_KW(SputnikJunkyardSound, LoadSounds), SOUND_KW(SputnikJunkyardSound, SetSounds),
	SOUND_KW(SputnikJunkyardSound, TalkSnd), SOUND_FN(SputnikJunkyardSound, InitLoops),
	SOUND_FN(SputnikJunkyardSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(SputnikSound, sputniksound, LoadSounds, 0)
SOUND_SECTION(SputnikSound, sputniksound, SetSounds, 0)
SOUND_SECTION(SputnikSound, sputniksound, TalkSnd, 1)
SOUND_LOOPS(SputnikSound, sputniksound)
SOUND_INIT(SputnikSound, sputniksound)
PyMethodDef g_SputnikSoundMethods[] = {
	SOUND_KW(SputnikSound, LoadSounds), SOUND_KW(SputnikSound, SetSounds), SOUND_KW(SputnikSound, TalkSnd),
	SOUND_FN(SputnikSound, InitLoops), SOUND_FN(SputnikSound, Init), { NULL, NULL, 0, NULL }
};

SOUND_SECTION(TestCSound, testcsound, LoadSounds, 0)
SOUND_SECTION(TestCSound, testcsound, SetSounds, 0)
SOUND_SECTION(TestCSound, testcsound, TalkSnd, 1)
SOUND_LOOPS(TestCSound, testcsound)
SOUND_INIT(TestCSound, testcsound)
PyMethodDef g_TestCSoundMethods[] = {
	SOUND_KW(TestCSound, LoadSounds), SOUND_KW(TestCSound, SetSounds), SOUND_KW(TestCSound, TalkSnd),
	SOUND_FN(TestCSound, InitLoops), SOUND_FN(TestCSound, Init), { NULL, NULL, 0, NULL }
};

/* MercenarySound.Init(CharClass, CharName): the sounds of one mercenary for its class */
MODE_FN(MercenarySound, Init, 2, Sound_RunInit("sound/mercenarysound.def", mercenarysound_def, &a[0], &a[1]))

SOUND_SECTION(MercenarySound, mercenarysound, LoadSounds, 0)
SOUND_SECTION(MercenarySound, mercenarysound, SetSounds, 0)
PyMethodDef g_MercenarySoundMethods[] = {
	SOUND_KW(MercenarySound, LoadSounds), SOUND_KW(MercenarySound, SetSounds), SOUND_FN(MercenarySound, Init),
	{ NULL, NULL, 0, NULL }
};

PORTS(Sounds)
