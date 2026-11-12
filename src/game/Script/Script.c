/*
 * The script loader of ScrapC (include/scrap/script.h): the C functions of pack modules, the helpers they share and
 * the .def files (the data of the ported scripts: menus, the HUD, the characters).
 *
 * A .def file is a list of lines, # starts a comment:
 *	style <name>			the attribute lines that follow belong to the style, until the next command
 *	item <name> <type> [style ...]	SScorer.Add (unless the item is there), then the attributes of the styles
 *	<Attr> <value>			SScorer.Set of the last item (or of the style being defined)
 *	use <style>			the attributes of a style onto the last item, at this point
 *	set <item> <Attr> <value>	SScorer.Set of another item
 *	lr <a> <b>, ud <a> <b>		Menu.LinkLR, Menu.LinkUD (through their refs)
 *	default <item>			SScorer.SetDefault
 *	list <name>			the lines that follow are the entries of a list, until the next command
 *	call <Module.Func> [arg ...]	a function of a script, Name=value for a keyword argument
 *	run <step> [arg ...]		a step the C code of the file does
 *	section <name>			a section: the C function that runs the file picks one, the lines before
 *					the first section always run
 *	do <name>			the lines of another section, here
 *	if <cond>, elif <cond>, else, end	the lines between only when cond holds: a value (true when not 0 or
 *					empty), <value> == <value>, <value> != <value>, or the numbers compared with
 *					<, >, <= or >=
 *	action <name>, getaction <name>	SAct.CreateAction, SAct.GetAction: the attribute lines that follow set
 *					the action (SAct.SetAct); action alone: the current action
 *	class <name>			SAct.GetClass: the attribute lines that follow set the class (SetCls); class
 *					alone: the current class
 *	event <time> <name> <func> [discardable]	an event of the current action
 *	setvar <Module.var> <value>	a variable of a script module
 *	entity <name>			the attribute lines that follow set script variables of the entity
 *	let <name> <value> [value ...]	$name: the sum of the values (or the one value that is no number)
 *	measure <name> <font> <text>	$name: the width of the text in the font
 * A value is a number, a word or a "quoted text"; {Key} in a text is the language text Key. Unquoted: @Item.Var the
 * number of a variable of another item, %Module.var a variable of a script module, $id the player, $section the
 * section, $name a value the C function gives or let and measure made,
 * [name] a list (a tuple of lists, one per entry line; a line with only a value that is a list or tuple is that
 * entry), (a,b,c) a tuple of values, None, &Var a variable of SAct's current action (&TimeOfFrame43,
 * &AnmTotalTime). @, %, $ and & values may end in operations with numbers, done left to right with Python's numbers:
 * $x+10, &AnmTotalTime/4*2.
 * $(name) inside a word or text is the value name as text.
 * The values are native values (nativeVal_t, see the comment above Def_Lookup). A call line calls the entry of a port or
 * engine function through the registry of stage 4 (Port_Invoke) while the module attribute is that function itself,
 * else by name through Python as a script did (Def_Call).
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#if defined(__GLIBC__)
#include <dlfcn.h>
#include <execinfo.h>
#endif

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/scriptmodes.h"
#include "scrap/pyentity.h"
#include "scrap/pyfunction.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"
#include "scrap/timer.h"
#include "compile.h"

#define SCRIPT_MAX_FILE 0x10000
#define SCRIPT_MAX_TOKENS 16
#define SCRIPT_MAX_TEXT 0x200
#define SCRIPT_MAX_BLOCKS 48
#define SCRIPT_MAX_DEPTH 8
#define SCRIPT_MAX_NESTED 4

static const scriptModule_t script_modules[] = {
	{ "PCMenu", g_MenuVideoMethods },
	{ "PCMenu", g_MenuControlsMethods },
	{ "PCMenu", g_MenuNetMethods },
	{ "PCMenu", g_MenuVideoVarsMethods },
	{ "Menu", g_MenuBaseMethods },
	{ "Menu", g_MenuMainMethods },
	{ "Menu", g_MenuMissionMethods },
	{ "Scorer", g_ScorerMethods },
	{ "CharScorer", g_CharScorerMethods },
	{ "RacerScorer", g_RacerScorerMethods },
	{ "Chars", g_CharsMethods },
	{ "MakeChar", g_MakeCharMethods },
	{ "CharAct", g_CharActMethods },
	{ "Speech", g_SpeechMethods },
	{ "SaveGame", g_SaveGameMethods },
	{ "Fx", g_FxMethods },
	{ "Items", g_ItemsMethods },
	{ "Net", g_NetMethods },
	{ "NDeathMatch", g_NDeathMatchMethods },
	{ "NDeathMatchBots", g_NDeathMatchBotsMethods },
	{ "NTeamDM", g_NTeamDMMethods },
	{ "NTeamFlag", g_NTeamFlagMethods },
	{ "NFlagHunt", g_NFlagHuntMethods },
	{ "NPostman", g_NPostmanMethods },
	{ "NTeamPost", g_NTeamPostMethods },
	{ "Weapons", g_WeaponsMethods },
	{ "Vulcan", g_VulcanMethods },
	{ "Devastator", g_DevastatorMethods },
	{ "Tesla", g_TeslaMethods },
	{ "ATPC", g_ATPCMethods },
	{ "Laser", g_LaserMethods },
	{ "Swarm", g_SwarmMethods },
	{ "Inferno", g_InfernoMethods },
	{ "Cloud", g_CloudMethods },
	{ "EMI", g_EMIMethods },
	{ "Sonic", g_SonicMethods },
	{ "SputnikInterface", g_SputnikInterfaceMethods },
	{ "ShipEdit", g_ShipEditMethods },
	{ "HangarTab", g_HangarTabMethods },
	{ "Vehicles", g_VehiclesMethods },
	{ "VehiclesNPC", g_VehiclesNPCMethods },
	{ "Split", g_SplitMethods },
	{ "SDeathMatch", g_SDeathMatchMethods },
	{ "SFlagHunt", g_SFlagHuntMethods },
	{ "SPostman", g_SPostmanMethods },
	{ "SRace", g_SRaceMethods },
	{ "Init", g_InitMethods },
	{ "string", g_StringMethods },
	{ "BankDirectorSound", g_BankDirectorSoundMethods },
	{ "BankMasterSound", g_BankMasterSoundMethods },
	{ "BertoSound", g_BertoSoundMethods },
	{ "BettySound", g_BettySoundMethods },
	{ "BishopSound", g_BishopSoundMethods },
	{ "BossSound", g_BossSoundMethods },
	{ "CrazyGamblerSound", g_CrazyGamblerSoundMethods },
	{ "DesktopSound", g_DesktopSoundMethods },
	{ "DtritusSound", g_DtritusSoundMethods },
	{ "FunctionarySound", g_FunctionarySoundMethods },
	{ "GateKeeperSound", g_GateKeeperSoundMethods },
	{ "GearSound", g_GearSoundMethods },
	{ "HumanSound", g_HumanSoundMethods },
	{ "HumphreySound", g_HumphreySoundMethods },
	{ "MaintenanceSound", g_MaintenanceSoundMethods },
	{ "MayorSound", g_MayorSoundMethods },
	{ "MercenarySound", g_MercenarySoundMethods },
	{ "MessengerSound", g_MessengerSoundMethods },
	{ "NurseSound", g_NurseSoundMethods },
	{ "PoliceBossSound", g_PoliceBossSoundMethods },
	{ "PoliceSound", g_PoliceSoundMethods },
	{ "RustySound", g_RustySoundMethods },
	{ "SebastianSound", g_SebastianSoundMethods },
	{ "SentinelSound", g_SentinelSoundMethods },
	{ "SputnikJunkyardSound", g_SputnikJunkyardSoundMethods },
	{ "SputnikSound", g_SputnikSoundMethods },
	{ "TestCSound", g_TestCSoundMethods },
	{ "CharsNPC", g_CharsNPCMethods },
	{ "Betty", g_BettyMethods },
	{ "BankDirector", g_BankDirectorMethods },
	{ "Boss", g_BossMethods },
	{ "Boss", g_BossFuncs },
	{ "CrazyGambler", g_CrazyGamblerMethods },
	{ "CrazyGambler", g_CrazyGamblerFuncs },
	{ "Humphrey", g_HumphreyMethods },
	{ "Maintenance", g_MaintenanceMethods },
	{ "Maintenance", g_MaintenanceFuncs },
	{ "Mayor", g_MayorMethods },
	{ "Mayor", g_MayorFuncs },
	{ "Rusty", g_RustyMethods },
	{ "Rusty", g_RustyFuncs },
	{ "Sebastian", g_SebastianMethods },
	{ "Sentinel", g_SentinelMethods },
	{ "SputnikJunkyard", g_SputnikJunkyardMethods },
	{ "SputnikJunkyard", g_SputnikJunkyardFuncs },
	{ "Bishop", g_BishopMethods },
	{ "Bishop", g_BishopFuncs },
	{ "Functionary", g_FunctionaryMethods },
	{ "Functionary", g_FunctionaryFuncs },
	{ "Gear", g_GearMethods },
	{ "Gear", g_GearFuncs },
	{ "Gear", g_GearDefFuncs },
	{ "BankMaster", g_BankMasterMethods },
	{ "Berto", g_BertoMethods },
	{ "Nurse", g_NurseMethods },
	{ "Desktop", g_DesktopMethods },
	{ "Police", g_PoliceMethods },
	{ "Police", g_PoliceFuncs },
	{ "Police", g_PoliceDefFuncs },
	{ "GateKeeper", g_GateKeeperMethods },
	{ "GateKeeper", g_GateKeeperFuncs },
	{ "Dtritus", g_DtritusMethods },
	{ "Sputnik", g_SputnikMethods },
	{ "Sputnik", g_SputnikFuncs },
	{ "PoliceBoss", g_PoliceBossMethods },
	{ "Messenger", g_MessengerMethods },
	{ "Messenger", g_MessengerFuncs },
	{ "Human", g_HumanMethods },
	{ "MercenaryA", g_MercenaryAMethods },
	{ "MercenaryB", g_MercenaryBMethods },
	{ "MercenaryC", g_MercenaryCMethods },
	{ "MercenaryL", g_MercenaryLMethods },
	{ "InMap", g_InMapMethods },
	{ "Bureaucracy", g_BureaucracyMethods },
	{ "Chat", g_ChatMethods },
	{ "Ffa", g_FfaMethods },
	{ "CreditsText", g_CreditsTextMethods },
	{ "Traffic", g_TrafficMethods },
	{ "OutPolice", g_OutPoliceMethods },
	{ "InPolice", g_InPoliceMethods },
	{ "PhoneCab", g_PhoneCabMethods },
	{ "DTritusDesktop", g_DTritusDesktopMethods },
	{ "Metro", g_MetroMethods },
	{ "Challenge", g_ChallengeMethods },
	{ "Alarm", g_AlarmMethods },
	{ "NewsPanel", g_NewsPanelMethods },
	{ "Doors", g_DoorsMethods },
	{ "Elevator", g_ElevatorMethods },
	{ "Teleport", g_TeleportMethods },
	{ "MisItems", g_MisItemsMethods },
	{ "CrazyWing", g_CrazyWingMethods },
	{ "MissionsFuncs", g_MissionsFuncsMethods },
	{ "Scene", g_SceneMethods },
	{ "CharConversor", g_CharConversorMethods },
	{ "RaceMaker", g_RaceMakerMethods },
	{ "Sound", g_SoundMethods },
	{ "OutSound", g_OutSoundMethods },
	{ "DoorSound", g_DoorSoundMethods },
	{ "OutMap", g_OutMapMethods },
	{ "InTraffic", g_InTrafficMethods },
	{ "ShipMaker", g_ShipMakerMethods },
	{ "Parking", g_ParkingMethods },
	{ "Accelerator", g_AcceleratorMethods },
	{ "DeathMatch", g_DeathMatchMethods },
	{ "TeamDeathMatch", g_TeamDeathMatchMethods },
	{ "Versus", g_VersusMethods },
	{ "CTF", g_CTFMethods },
	{ "Domination", g_DominationMethods },
	{ "MortalRace", g_MortalRaceMethods },
	{ "EnginesTab", g_EnginesTabMethods },
	{ "WeaponsTab", g_WeaponsTabMethods },
	{ "HullTab", g_HullTabMethods },
};

/* whether an item of the tuple t is the object cur */
static byte Script_TupleHas(const nativeVal_t *t, const nativeVal_t *cur) {
	for (int i = 0; i < t->n; i++) {
		if (NObj_Is(&t->items[i], cur)) {
			return 1;
		}
	}
	return 0;
}

/* (k, v) for a set after the walk of a dict: a set may resize the dict under the walk (later made at the first) */
static void Script_Defer(nativeVal_t *later, const nativeVal_t *k, const nativeVal_t *v) {
	nativeVal_t *kv;
	if (later->type != NV_LIST) {
		Native_SetEmpty(later, NV_LIST);
	}
	kv = Native_Append(later);
	Native_SetEmpty(kv, NV_TUPLE);
	Native_Copy(Native_Append(kv), k);
	Native_Copy(Native_Append(kv), v);
}

static void Script_SetDeferred(const nativeVal_t *d, nativeVal_t *later) {
	for (int i = 0; later->type == NV_LIST && i < later->n; i++) {
		if (!NObj_SetItem(d, &later->items[i].items[0], &later->items[i].items[1])) {
			Native_ErrClear();
		}
	}
	Native_Free(later);
}

/*
 * The writes the loader made after the .pyc for the function cur under key, with the values in place: the other
 * names of the module that hold cur, the tuples that hold it (set again under their names: Doors.DefDoorInfo = 800,
 * StdInitializator, StdObjInitializator, 0, 0) and the dict values that hold it, one level deep (not __builtins__). The
 * sets come after the walk of the variables. When the .pyc ran, the loader put the C function in the place of the
 * Python one there (and rebound the engine callbacks to it); no Python code loads now, so nothing is replaced and only
 * the writes stay. They decide the key order of the module's variables (PyDict_SetItem doubles a dict that is 2/3 full
 * before a write, also for a key that is in it, and the registry's table does the same).
 */
static void Script_ReplayRefs(const modVarRef_t *ref, const nativeVal_t *space, const nativeVal_t *key,
	const nativeVal_t *cur) {
	nativeVal_t k, v, later;
	int pos = 0;
	Native_SetNone(&later);
	while (ModVar_Next(ref, &pos, &k, &v)) {
		if (NObj_Is(&v, cur)) {
			if (!Native_Equal(&k, key)) {
				Script_Defer(&later, &k, cur);
			}
		} else if (v.type == NV_TUPLE) {
			if (Script_TupleHas(&v, cur)) {
				Script_Defer(&later, &k, &v);
			}
		} else if (NObj_Kind(&v) == NO_DICT && !NObj_Is(&v, space) && !Native_IsStr(&k, "__builtins__")) {
			nativeVal_t items, inner;
			Native_SetNone(&inner);
			if (NObj_CallMethod(&v, "items", NULL, 0, &items)) {
				int n = NObj_Len(&items);
				for (int i = 0; i < n; i++) {
					nativeVal_t ik, kv;
					Native_SetInt(&ik, i);
					if (NObj_GetItem(&items, &ik, &kv) && kv.type == NV_TUPLE && kv.n == 2 &&
						NObj_Is(&kv.items[1], cur)) {
						Script_Defer(&inner, &kv.items[0], cur);
					}
					Native_Free(&kv);
				}
			}
			Native_Free(&items);
			Script_SetDeferred(&v, &inner);
		}
		Native_Free(&k);
		Native_Free(&v);
	}
	Script_SetDeferred(space, &later);
	Native_ErrClear();
}


/*
 * ---- the check of NULL returns (SCRAP_NULL_CHECK) ----
 *
 * A C function must return NULL only with a Python error set, else ceval raises "SystemError: error return without
 * exception set" where the script raised something else or nothing. SCRAP_NULL_CHECK=<file> (or 1 for the console)
 * puts every C function of the script tables into a wrapper that reports a NULL without an error and a result with an
 * error left pending. A report names the kind, the
 * innermost C function that runs, the line of the Python code that called it and (glibc) the return addresses as
 * ScrapC+offset for addr2line -f -i -e ScrapC. Off, nothing changes: the functions are the plain PyCFunctions.
 * SCRAP_NULL_CHECK_ENGINE=1 as well wraps the functions of the engine modules (Scrap, SAI, SAct, ...) when Python
 * starts, also with SCRAP_PACK_SCRIPTS=1, so both versions report; then the Python line is part of what a report is
 * deduplicated by, and a call made while an error is pending reports "call with error pending" (where a stale error
 * is carried to).
 */
typedef struct {
	PyMethodDef def;		/* the wrapper: the name of the function, Script_CheckCall */
	PyMethodDef orig;		/* the C function */
	PyObject *self;			/* its self */
	char name[64];
	char label[96];			/* Module.Func */
} scriptChecked_t;

#define SCRIPT_CHECK_DEPTH 64
#define SCRIPT_CHECK_SEEN 1024
static int script_check = -1;
static byte script_checkEngine;
static FILE *script_checkFile;
static const char *script_checkStack[SCRIPT_CHECK_DEPTH];
static int script_checkDepth;
static unsigned script_checkSeen[SCRIPT_CHECK_SEEN];

static byte Script_CheckOn(void) {
	if (script_check < 0) {
		const char *e = getenv("SCRAP_NULL_CHECK");
		script_check = e && *e && strcmp(e, "0");
		if (script_check && strcmp(e, "1")) {
			script_checkFile = fopen(e, "a");
		}
		e = getenv("SCRAP_NULL_CHECK_ENGINE");
		script_checkEngine = script_check && e && *e == '1';
	}
	return script_check > 0;
}

static unsigned Script_CheckHash(unsigned h, const char *s) {
	while (s && *s) {
		h = (h ^ (byte)*s++) * 16777619u;
	}
	return h;
}

void Script_CheckSilent(const char *kind) {
	if (!Script_CheckOn()) {
		return;
	}
	const char *label = script_checkDepth > 0 ? script_checkStack[script_checkDepth - 1] : "?";
	char where[160] = "-", trace[400] = "";
	/* there is no Python frame to name */
	unsigned h = Script_CheckHash(Script_CheckHash(2166136261u, kind), label);
	if (script_checkEngine) {
		h = Script_CheckHash(h, where);
	}
#if defined(__GLIBC__)
	void *addr[8];
	int n = backtrace(addr, 8);
	for (int i = 1; i < n; i++) {
		Dl_info info;
		size_t len = strlen(trace);
		if (dladdr(addr[i], &info) && info.dli_fname) {
			const char *base = strrchr(info.dli_fname, '/');
			Str_Printf(trace + len, sizeof(trace) - len, " %s+0x%lx", base ? base + 1 : info.dli_fname,
				(unsigned long)((char *)addr[i] - (char *)info.dli_fbase));
		} else {
			Str_Printf(trace + len, sizeof(trace) - len, " %p", addr[i]);
		}
		if (i <= 3) {
			h = (h ^ (unsigned)(size_t)addr[i]) * 16777619u;
		}
	}
#endif
	for (int i = 0; i < SCRIPT_CHECK_SEEN; i++) {
		if (script_checkSeen[i] == h) {
			return;
		}
		if (!script_checkSeen[i]) {
			script_checkSeen[i] = h;
			break;
		}
	}
	if (script_checkFile) {
		fprintf(script_checkFile, "NULLCHECK %s: %s (python %s)%s\n", kind, label, where, trace);
		fflush(script_checkFile);
	} else {
		Con_Warnf("NULLCHECK %s: %s (python %s)%s\n", kind, label, where, trace);
	}
}

/* the call of the C function as call_builtin of ceval makes it */
static PyObject *Script_CheckCall(PyObject *self, PyObject *args, PyObject *kw) {
	scriptChecked_t *c = PyCObject_AsVoidPtr(self);
	int flags = c->orig.ml_flags;
	PyObject *arg = args, *r;
	byte pending = Native_Failed();
	if (!(flags & METH_VARARGS)) {
		int size = PyTuple_Size(args);
		arg = size == 1 ? PyTuple_GET_ITEM(args, 0) : size == 0 ? NULL : args;
	}
	if (script_checkDepth < SCRIPT_CHECK_DEPTH) {
		script_checkStack[script_checkDepth] = c->label;
	}
	script_checkDepth++;
	if (pending && script_checkEngine) {
		Script_CheckSilent("call with error pending");
	}
	if (flags & METH_KEYWORDS) {
		r = ((PyCFunctionWithKeywords)(void (*)(void))c->orig.ml_meth)(c->self, arg, kw);
	} else if (kw && PyDict_Size(kw) != 0) {
		Native_Raise(NE_TYPE, "this function takes no keyword arguments");
		r = NULL;
	} else {
		r = c->orig.ml_meth(c->self, arg);
	}
	if (!r && !Native_Failed()) {
		Script_CheckSilent("return");
	} else if (r && !pending && Native_Failed()) {
		/* the error stays pending and raises at a later line (one pending before the call is an engine
		 * function's that returned a value with an error set, as the original does) */
		Script_CheckSilent("return with error set");
	}
	script_checkDepth--;
	return r;
}

static void Script_CheckFree(void *p) {
	scriptChecked_t *c = p;
	Py_XDECREF(c->self);
	free(c);
}

PyObject *Script_CheckFunction(PyMethodDef *def, PyObject *self, const char *module) {
	if (!Script_CheckOn()) {
		return PyCFunction_New(def, self);
	}
	scriptChecked_t *c = calloc(1, sizeof(*c));
	if (!c) {
		return Native_RaiseVal(NE_MEMORY, NULL), NULL;
	}
	c->orig = *def;
	c->self = self;
	Py_XINCREF(self);
	Str_Copy(c->name, def->ml_name, sizeof(c->name));
	Str_Printf(c->label, sizeof(c->label), "%s.%s", module ? module : "?", def->ml_name);
	c->def.ml_name = c->name;
	c->def.ml_meth = (PyCFunction)(void (*)(void))Script_CheckCall;
	c->def.ml_flags = METH_VARARGS | METH_KEYWORDS;
	c->def.ml_doc = def->ml_doc;
	PyObject *o = PyCObject_FromVoidPtr(c, Script_CheckFree);
	if (!o) {
		Script_CheckFree(c);
		return NULL;
	}
	PyObject *f = PyCFunction_New(&c->def, o);
	Py_DECREF(o);
	return f;
}

const char *Script_Row(const portFn_t *fns, int i, scriptRow_t *row) {
	row->fn = &fns[i];
	return fns[i].name;
}

/* the last row of module's tables that has name (as the loader put them in): 1 with *found, 0 if none */
static byte Script_TableDef(const char *module, const char *name, scriptRow_t *found) {
	byte any = 0;
	for (size_t i = 0; i < sizeof(script_modules) / sizeof(script_modules[0]); i++) {
		const scriptModule_t *sm = &script_modules[i];
		scriptRow_t row;
		const char *rn;
		if (strcmp(sm->name, module)) {
			continue;
		}
		for (int k = 0; (rn = Script_Row(sm->fns, k, &row)) != NULL; k++) {
			if (!strcmp(rn, name)) {
				*found = row;
				any = 1;
			}
		}
	}
	return any;
}

byte Script_Checking(void) {
	return Script_CheckOn();
}

byte Script_FunctionOf(const scriptRow_t *row, const char *module, nativeVal_t *out) {
	/* the native function of its ref (SCRAP_NULL_CHECK has no wrapper for it) */
	const portRef_t *ref = Port_OfFn(module, row->fn);
	Native_SetNone(out);
	if (!ref) {
		return Native_Raise(NE_SYSTEM, "%s.%s has no port ref", module, row->fn->name);
	}
	NObj_NewPortFunc(out, ref);
	return 1;
}

byte Script_TableFunction(const char *module, const char *name, nativeVal_t *out) {
	scriptRow_t found;
	Native_SetNone(out);
	return Script_TableDef(module, name, &found) && Script_FunctionOf(&found, module, out);
}

byte Script_TableHas(const char *module, const char *name) {
	scriptRow_t found;
	return Script_TableDef(module, name, &found);
}

/*
 * The dict writes the loader made after the .pyc for a table, on a module made in C with the values in place: per
 * function of the table the other names, tuple items and dict values that hold it (Script_ReplayRefs) and the name
 * itself, which a function the script lacks gets here as the loader gave it. PyDict_SetItem doubles a dict that is
 * 2/3 full before the write, also for a key that is in it, so these writes decide the key order of the module dict
 * as they did after the .pyc (Sound modules of 21 keys).
 */
void Script_TableReplay(const nativeVal_t *module, const portFn_t *fns) {
	modVarRef_t ref;
	scriptRow_t row;
	const char *name;
	nativeVal_t mname;
	if (!ModVar_OfVal(module, &ref) || !ModVar_GetVal(&ref, "__name__", &mname)) {
		return;
	}
	for (int k = 0; (name = Script_Row(fns, k, &row)) != NULL; k++) {
		nativeVal_t cur;
		if (ModVar_GetVal(&ref, name, &cur)) {
			Script_ReplaySet(&ref, name, &cur);
			Native_Free(&cur);
			continue;
		}
		if (Script_FunctionOf(&row, Native_Text(&mname), &cur)) {
			ModVar_SetVal(&ref, name, &cur);
		}
		Native_Free(&cur);
	}
	Native_Free(&mname);
}

void Script_ReplaySet(const modVarRef_t *ref, const char *name, const nativeVal_t *cur) {
	nativeVal_t key, space, v;
	Native_SetStr(&key, name);
	Native_Copy(&v, cur);
	ModVar_Space(ref, &space);
	Script_ReplayRefs(ref, &space, &key, &v);
	ModVar_SetVal(ref, name, &v);
	Native_Free(&space);
	Native_Free(&key);
	Native_Free(&v);
}

void Script_ModuleExtras(const char *name, const nativeVal_t *module, const nativeVal_t *defs) {
	for (size_t i = 0; i < sizeof(script_modules) / sizeof(script_modules[0]); i++) {
		if (!strcmp(script_modules[i].name, name)) {
			Script_TableReplay(module, script_modules[i].fns);
		}
	}
	Script_LevelModuleExtras(name, module, defs);
}

/* ---- the scorer of a player ---- */

static cScorerItem *Script_Item(int id, const char *item) {
	cScorer *sc = Scorer_Get(id);
	return sc ? cScorer_FindItem(sc, item) : NULL;
}

void Script_Add(int id, const char *item, const char *type) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_AddItem(sc, item, type, 0);
	}
}

void Script_SetStr(int id, const char *item, const char *var, const char *s) {
	cScorerItem *it = Script_Item(id, item);
	char *v = (char *)s;
	if (it) {
		ScorerItem_Var(it, var, 1, &v, NULL, NULL);
	}
}

void Script_SetNum(int id, const char *item, const char *var, double d) {
	cScorerItem *it = Script_Item(id, item);
	int i = (int)d;
	if (it) {
		ScorerItem_Var(it, var, 1, NULL, &d, &i);
	}
}

const char *Script_GetStr(int id, const char *item, const char *var) {
	cScorerItem *it = Script_Item(id, item);
	char *s = NULL;
	double d;
	int i;
	if (it && ScorerItem_Var(it, var, 0, &s, &d, &i) == 1 && s) {
		return s;
	}
	return "";
}

double Script_GetNum(int id, const char *item, const char *var) {
	cScorerItem *it = Script_Item(id, item);
	char *s;
	double d = 0.0;
	int i = 0;
	if (!it) {
		return 0.0;
	}
	switch (ScorerItem_Var(it, var, 0, &s, &d, &i)) {
	case 2:
		return d;
	case 3:
		return i;
	}
	return 0.0;
}

void Script_SetDefault(int id, const char *item) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_SetDefault(sc, item);
	}
}

byte Script_HasItem(int id, const char *item) {
	return Script_Item(id, item) != NULL;
}

const char *Script_GetDefault(int id) {
	cScorer *sc = Scorer_Get(id);
	return sc && sc->defaultItem ? sc->defaultItem : "";
}

void Script_SetCursor(int id, const char *item) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		sc->cursor = cScorer_FindItem(sc, item);
	}
}

const char *Script_Lang(const char *key) {
	const char *s = Lang_GetStr(key);
	if (!s) {
		Con_Warnf("STRING NOT FOUND: '%s'\n", key);
		return "";
	}
	return s;
}

/* ---- the scripts ---- */

PyObject *Script_Attr(const char *module, const char *func) {
	return ModVar_ModAttr(module, func);
}

double Script_GetFloat(const char *module, const char *var, double def) {
	PyObject *v = Script_Attr(module, var);
	double d = v && PyFloat_Check(v) ? PyFloat_AsDouble(v) : v && PyInt_Check(v) ? (double)PyInt_AsLong(v) : def;
	if (!v) {
		Native_ErrClear();
	}
	Py_XDECREF(v);
	return d;
}

/* ---- entity handles and values as a script uses them ---- */

byte Script_HSet(PyObject *e, const char *var, PyObject *v) {
	int ok = v && PyObject_SetAttrString(e, (char *)var, v) == 0;
	if (v && !ok && !Native_Failed()) {
		/* an entity variable the engine refuses fails without an exception; ceval reports it so */
		Script_CheckSilent("Script_HSet (engine)");
		Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	Py_XDECREF(v);
	return ok;
}

int Script_HIs(PyObject *e, const char *var, const char *text) {
	PyObject *v = Script_GetAttr(e, var);
	if (!v) {
		return -1;
	}
	int is = PyString_Check(v) && !strcmp(PyString_AsString(v), text);
	Py_DECREF(v);
	return is;
}

int Script_HPrefix(PyObject *e, const char *var, const char *prefix) {
	PyObject *v = Script_GetAttr(e, var);
	PyObject *head = v ? PySequence_GetSlice(v, 0, (int)strlen(prefix)) : NULL;
	Py_XDECREF(v);
	if (!head) {
		return -1;
	}
	int is = PyString_Check(head) && !strcmp(PyString_AsString(head), prefix);
	Py_DECREF(head);
	return is;
}

int Script_NumCmp(PyObject *a, long n) {
	PyObject *b = PyInt_FromLong(n);
	int c = b ? PyObject_Compare(a, b) : 0;
	Py_XDECREF(b);
	return Native_Failed() ? -2 : c;
}

/* ---- script variables of entities, as the Python entity handle reads and writes them ---- */

double Script_EntNum(const char *ent, const char *var, double def) {
	cEntity *e = World_FindEntity(ent);
	int i;
	double d;
	if (e && Entity_VarI(e, var, &i, 0)) {
		return i;
	}
	if (e && Entity_VarF(e, var, &d, 0)) {
		return d;
	}
	return def;
}

const char *Script_EntStr(const char *ent, const char *var, const char *def) {
	cEntity *e = World_FindEntity(ent);
	char *s = NULL;
	return e && Entity_VarS(e, var, &s, 0) && s ? s : def;
}

byte Script_EntTrue(const char *ent, const char *var) {
	cEntity *e = World_FindEntity(ent);
	int i;
	double d, x, y, z;
	char *s = NULL;
	if (!e) {
		return 0;
	}
	if (Entity_VarI(e, var, &i, 0)) {
		return i != 0;
	}
	if (Entity_VarF(e, var, &d, 0)) {
		return d != 0.0;
	}
	if (Entity_VarV(e, var, &x, &y, &z, 0)) {
		return 1;
	}
	return Entity_VarS(e, var, &s, 0) && s && *s;
}

PyObject *Script_Invoke(PyObject *callable, PyObject *args) {
	PyObject *r = callable && args ? Script_CallObject(callable, args, NULL) : NULL;
	Py_XDECREF(args);
	return r;
}

PyObject *Script_Ask(const char *module, const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = Script_Attr(module, func);
	PyObject *r = Script_Invoke(f, args);
	Py_XDECREF(f);
	return r;
}

void Script_StepPrint(int id, const nativeVal_t *args, int n) {
	const char *text;
	(void)id;
	nativeVal_t out;
	byte has = Script_SysVar("stdout", &out);
	if (!(n == 1 ? Native_ArgStr(&args[0], 1, &text) : Port_Arity(n, 1)) || !has ||
		!Script_FileWriteText(&out, text) || !Script_FileWriteText(&out, "\n")) {
		Native_ErrPrint();
	}
	Native_Free(&out);
}

PyObject *Script_Method(PyObject *obj, const char *name, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = Script_GetAttr(obj, name);
	PyObject *r = Script_Invoke(f, args);
	Py_XDECREF(f);
	return r;
}

byte Script_Try(const char *module, const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = Script_Attr(module, func);
	PyObject *r = Script_Invoke(f, args);
	Py_XDECREF(f);
	Py_XDECREF(r);
	return r != NULL;
}

int Script_GetInt(const char *module, const char *var, int def) {
	PyObject *v = Script_Attr(module, var);
	int r = def;
	if (v && PyInt_Check(v)) {
		r = (int)PyInt_AsLong(v);
	} else if (v && PyFloat_Check(v)) {
		r = (int)PyFloat_AsDouble(v);
	}
	if (!v) {
		Native_ErrClear();
	}
	Py_XDECREF(v);
	return r;
}

const char *Script_GetString(const char *module, const char *var, char *out, int size) {
	PyObject *v = Script_Attr(module, var);
	out[0] = 0;
	if (v && PyString_Check(v)) {
		Str_Copy(out, PyString_AsString(v), size);
	}
	if (!v) {
		Native_ErrClear();
	}
	Py_XDECREF(v);
	return out;
}

void Script_SetVar(const char *module, const char *var, PyObject *value) {
	if (!value || ModVar_SetModAttr(module, var, value) < 0) {
		Native_ErrClear();
	}
	Py_XDECREF(value);
}

void Script_SetInt(const char *module, const char *var, int value) {
	Script_SetVar(module, var, PyInt_FromLong(value));
}

byte Script_Args(PyObject *args, int *id, const char **control) {
	PyObject *c = NULL;
	if (!PyArg_ParseTuple(args, "i|O", id, &c)) {
		return 0;
	}
	*control = c && PyString_Check(c) ? PyString_AsString(c) : "";
	return 1;
}

/* ---- the .def files ---- */

typedef struct {
	char *tok[SCRIPT_MAX_TOKENS];
	byte quoted[SCRIPT_MAX_TOKENS];
	int n;
} scriptLine_t;

/* a style or a list: its lines */
typedef struct {
	char name[0x40];
	const char *start;
	const char *end;
	byte list;
} scriptBlock_t;

enum { TARGET_ITEM, TARGET_ACTION, TARGET_CLASS, TARGET_ENTITY, TARGET_OBJECT };

typedef struct {
	int id;
	const scriptStep_t *steps;
	nativeVal_t vars;		/* the $names: the dict object of the C function, the file's own one let and measure
					 * made when it gave none, or None */
	const char *text;		/* the whole file */
	const char *section;
	char item[0x80];		/* the last item */
	byte target;			/* what attribute lines set: TARGET_ITEM, TARGET_ACTION, TARGET_CLASS or TARGET_ENTITY */
	char entity[0x80];		/* the entity of TARGET_ENTITY */
	nativeVal_t object;		/* the object of TARGET_OBJECT */
	byte hasObject;			/* the object line gave a value */
	scriptBlock_t blocks[SCRIPT_MAX_BLOCKS];
	int numBlocks;
	scriptBlock_t *open;		/* the style or list whose lines follow */
	int depth;			/* if */
	byte active[SCRIPT_MAX_DEPTH];	/* the lines of this level run (and those of the levels around it) */
	byte taken[SCRIPT_MAX_DEPTH];	/* a branch of this level ran or may not run */
	int nested;			/* do */
	const char *name;		/* the file and the line, for the warnings */
	int line;
	byte stop;			/* Script_RunFileTry: an error stops the file as it stops a script */
	byte failed;			/* the file stopped, the error is set */
} scriptDef_t;

/* the tokens of one line into buf (a word, a "quoted text", up to # outside quotes); the next line */
static const char *Def_Tokens(const char *p, char *buf, int size, scriptLine_t *l) {
	char *o = buf;
	char *end = buf + size - 1;

	l->n = 0;
	while (*p && *p != '\n') {
		while (*p == ' ' || *p == '\t' || *p == '\r') {
			p++;
		}
		if (!*p || *p == '\n' || *p == '#') {
			break;
		}
		/* a text in "" or in '' (for texts that hold a ", like the Python lines of OnVictory) */
		char q = *p == '"' || *p == '\'' ? *p : 0;
		if (l->n < SCRIPT_MAX_TOKENS) {
			l->quoted[l->n] = q != 0;
			l->tok[l->n++] = o;
		}
		if (q) {
			for (p++; *p && *p != q && *p != '\n'; p++) {
				if (o < end) {
					*o++ = *p;
				}
			}
			if (*p == q) {
				p++;
			}
		} else {
			for (; *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'; p++) {
				if (o < end) {
					*o++ = *p;
				}
			}
		}
		if (o < end) {
			*o++ = 0;
		}
	}
	*end = 0;
	while (*p && *p != '\n') {
		p++;
	}
	return *p ? p + 1 : p;
}

/*
 * The values of a file are native values (nativeVal_t). The $names are a dict object of native values (the C function
 * gives it, a caller may run several sections on one dict, or the file makes its own at the first let): a value goes
 * in as a call through Python hands it on (NObj_CallArg: a list or dict becomes an object, a vector the tuple of its
 * floats), so a list or dict keeps its identity in the runner (a step appends to the list of a $name in place, a call
 * or a setvar may keep it). A module variable (%Module.var) is the registry's value.
 */

/* the $name of the file into *out (a copy, Native_Free it): 0 and *out None when it has none */
static byte Def_Lookup(scriptDef_t *m, const char *name, nativeVal_t *out) {
	const nativeVal_t *d = m ? NObj_DictData(&m->vars) : NULL, *v = NULL;
	Native_SetNone(out);
	if (d) {
		nativeVal_t key;
		Native_SetStr(&key, name);
		v = Native_DictGet(d, &key);
		Native_Free(&key);
	}
	if (v) {
		Native_Copy(out, v);
	}
	return v != NULL;
}

void Script_DefVar(nativeVal_t *vars, const char *name, const nativeVal_t *v) {
	nativeVal_t key, x;
	if (vars->type != NV_REF) {
		NObj_NewDict(vars);
	}
	Native_SetStr(&key, name);
	NObj_CallArg(v, &x);
	NObj_SetItem(vars, &key, &x);
	Native_Free(&x);
	Native_Free(&key);
}

/* the text s with {Key} replaced by the language texts and $(name) by the value name of the file */
static const char *Def_Text(scriptDef_t *m, const char *s, char *out, int size) {
	int n = 0;
	while (*s && n < size - 1) {
		const char *close = *s == '{' ? strchr(s, '}') : s[0] == '$' && s[1] == '(' ? strchr(s, ')') : NULL;
		if (close && close - s < 0x40) {
			char key[0x40];
			const char *t = "";
			nativeBuf_t str;
			Native_BufInit(&str);
			if (*s == '{') {
				Str_Copy(key, s + 1, (int)(close - s));
				t = Script_Lang(key);
			} else {
				Str_Copy(key, s + 2, (int)(close - s) - 1);
				nativeVal_t v;
				byte has = Def_Lookup(m, key, &v);
				if (has && Mode_StrBuf(&v, &str)) {
					t = str.p;
				} else {
					Con_Warnf("%s:%d: no value %s\n", m ? m->name : "", m ? m->line : 0, key);
				}
				Native_Free(&v);
			}
			for (; *t && n < size - 1; t++) {
				out[n++] = *t;
			}
			Native_BufFree(&str);
			s = close + 1;
		} else {
			out[n++] = *s++;
		}
	}
	out[n] = 0;
	return out;
}

/* a number token: 1 and its value */
static byte Def_Number(const char *s, double *d) {
	char *e;
	if (!*s) {
		return 0;
	}
	*d = strtod(s, &e);
	return *e == 0;
}

/* the text s (up to its 0) as the value */
static void Def_SetText(nativeVal_t *v, const char *s) {
	Native_SetText(v, s, (int)strlen(s));
}

/* d as an int when it is integral, else as a float */
static void Def_SetNumber(nativeVal_t *v, double d) {
	if (d == (long)d) {
		Native_SetInt(v, (long)d);
	} else {
		Native_SetFloat(v, d);
	}
}

static byte Def_List(scriptDef_t *m, const char *name, nativeVal_t *out);

/* "A.B" into a (size bytes) and the part after the dot; NULL without a dot */
static const char *Def_Split(const char *s, char *a, int size) {
	const char *dot = strchr(s, '.');
	if (!dot || dot - s >= size) {
		return NULL;
	}
	Str_Copy(a, s, (int)(dot - s) + 1);
	return dot + 1;
}

/* the start of the arithmetic after a name: the first + - * / (or the end) */
static const char *Def_Ops(const char *s) {
	const char *op = strpbrk(s, "+-*/");
	return op ? op : s + strlen(s);
}

/* *v with the operations of ops applied left to right ("+35", "/4*2"): the numbers of Python, an integer stays one
 * with integer operands and divides down; 0 (and *v None) when it is no number but ops has operations */
static byte Def_Arith(scriptDef_t *m, nativeVal_t *v, const char *ops) {
	if (!*ops) {
		return 1;
	}
	if (v->type != NV_INT && v->type != NV_FLOAT) {
		Con_Warnf("%s:%d: no number for %s\n", m->name, m->line, ops);
		Native_Free(v);
		return 0;
	}
	byte isInt = v->type == NV_INT;
	long iv = isInt ? v->i : 0;
	double dv = isInt ? (double)iv : v->f;
	Native_SetNone(v);
	for (const char *p = ops; *p;) {
		char op = *p++, num[0x40];
		const char *next = strpbrk(p, "+-*/");
		/* a sign right after the operator belongs to the number */
		if (next == p) {
			next = strpbrk(p + 1, "+-*/");
		}
		int len = next ? (int)(next - p) : (int)strlen(p);
		Str_Copy(num, p, len + 1 < (int)sizeof(num) ? len + 1 : (int)sizeof(num));
		double x;
		if (!Def_Number(num, &x)) {
			Con_Warnf("%s:%d: bad number %s in %s\n", m->name, m->line, num, ops);
			return 0;
		}
		byte xInt = !strchr(num, '.') && x == (long)x;
		if (isInt && xInt) {
			long y = (long)x;
			if (op == '/' && y == 0) {
				Con_Warnf("%s:%d: division by zero in %s\n", m->name, m->line, ops);
				return 0;
			}
			/* Python 1.5 divides integers down */
			iv = op == '+' ? iv + y : op == '-' ? iv - y : op == '*' ? iv * y :
				(iv / y - ((iv % y != 0) && ((iv < 0) != (y < 0))));
			dv = (double)iv;
		} else {
			if (isInt) {
				dv = (double)iv;
				isInt = 0;
			}
			dv = op == '+' ? dv + x : op == '-' ? dv - x : op == '*' ? dv * x : dv / x;
		}
		p = next ? next : p + len;
	}
	if (isInt) {
		Native_SetInt(v, iv);
	} else {
		Native_SetFloat(v, dv);
	}
	return 1;
}

/* a value token into *out (Native_Free it): 1, or 0 with the warning printed and *out None */
static byte Def_Value(scriptDef_t *m, const char *t, byte quoted, nativeVal_t *out) {
	char text[SCRIPT_MAX_TEXT], a[0x80];
	const char *b;
	double d;

	Native_SetNone(out);
	if (quoted || strstr(t, "$(")) {
		Def_SetText(out, Def_Text(m, t, text, sizeof(text)));
		return 1;
	}
	if (!strcmp(t, "$id")) {
		Native_SetInt(out, m->id);
		return 1;
	}
	if (!strcmp(t, "$section")) {
		Def_SetText(out, m->section ? m->section : "");
		return 1;
	}
	if (t[0] == '$') {
		char name[0x40];
		const char *ops = Def_Ops(t + 1);
		Str_Copy(name, t + 1, ops - t < (int)sizeof(name) ? (int)(ops - t) : (int)sizeof(name));
		if (!Def_Lookup(m, name, out)) {
			Con_Warnf("%s:%d: no value %s\n", m->name, m->line, t);
			return 0;
		}
		return Def_Arith(m, out, ops);
	}
	if (!strcmp(t, "None")) {
		return 1;
	}
	if (t[0] == '(' && t[strlen(t) - 1] == ')') {
		/* (a,b,c): a tuple of values */
		char inner[SCRIPT_MAX_TEXT];
		Str_Copy(inner, t + 1, sizeof(inner));
		inner[strlen(inner) - 1] = 0;
		int n = 0;
		for (char *p = inner; *p; p++) {
			n += *p == ',';
		}
		n += inner[0] && inner[strlen(inner) - 1] != ',';
		Native_SetEmpty(out, NV_TUPLE);
		int k = 0;
		for (char *p = inner; *p && k < n; k++) {
			char *comma = strchr(p, ',');
			if (comma) {
				*comma = 0;
			}
			/* an item in "" or '' is a text (the converter writes tuples of texts without spaces or commas) */
			size_t len = strlen(p);
			byte q = len >= 2 && (p[0] == '"' || p[0] == '\'') && p[len - 1] == p[0];
			if (q) {
				p[len - 1] = 0;
			}
			/* a value that fails is None */
			Def_Value(m, p + q, q, Native_Append(out));
			p = comma ? comma + 1 : p + strlen(p);
		}
		return 1;
	}
	if (t[0] == '[' && t[strlen(t) - 1] == ']') {
		Str_Copy(a, t + 1, sizeof(a));
		a[strlen(a) - 1] = 0;
		return Def_List(m, a, out);
	}
	if (t[0] == '&') {
		/* a variable of SAct's current action */
		char var[0x80], *s;
		double dv = 0.0;
		int iv = 0;
		const char *ops = Def_Ops(t + 1);
		Str_Copy(var, t + 1, ops - t < (int)sizeof(var) ? (int)(ops - t) : (int)sizeof(var));
		switch (g_pAuxAction ? cAction_Var(g_pAuxAction, var, 0, &s, &dv, &iv) : ACTVAR_NONE) {
		case ACTVAR_STRING:
			Def_SetText(out, s ? s : "");
			return 1;
		case ACTVAR_DOUBLE:
			Native_SetFloat(out, dv);
			return Def_Arith(m, out, ops);
		case ACTVAR_INT:
			Native_SetInt(out, iv);
			return Def_Arith(m, out, ops);
		}
		Con_Warnf("%s:%d: no action variable %s\n", m->name, m->line, var);
		return 0;
	}
	if ((t[0] == '%' || t[0] == '@') && (b = Def_Split(t + 1, a, sizeof(a)))) {
		char var[0x80];
		const char *ops = Def_Ops(b);
		Str_Copy(var, b, ops - b < (int)sizeof(var) ? (int)(ops - b) + 1 : (int)sizeof(var));
		if (t[0] == '@') {
			Def_SetNumber(out, Script_GetNum(m->id, a, var));
			return Def_Arith(m, out, ops);
		}
		/* a global of the level module whose function runs ($__module__) raises NameError like the script's read */
		nativeVal_t own;
		byte global = Def_Lookup(m, "__module__", &own) && own.type == NV_STR && !strcmp(Native_Text(&own), a);
		Native_Free(&own);
		if (!(global ? ModVar_GlobalVal(a, var, out) : ModVar_ModAttrVal(a, var, out))) {
			if (m->stop) {
				m->failed = 1;
				return 0;
			}
			Native_ErrClear();
			Con_Warnf("%s:%d: no %s\n", m->name, m->line, t + 1);
			return 0;
		}
		return Def_Arith(m, out, ops);
	}
	if (Def_Number(t, &d)) {
		if (strchr(t, '.')) {
			Native_SetFloat(out, d);
		} else {
			Native_SetInt(out, (long)d);
		}
		return 1;
	}
	Def_SetText(out, Def_Text(m, t, text, sizeof(text)));
	return 1;
}

/* a value token as a string into out (an item name, a link, a compared value) */
static const char *Def_Word(scriptDef_t *m, const char *t, byte quoted, char *out, int size) {
	out[0] = 0;
	if (!quoted && (t[0] == '$' || t[0] == '%' || t[0] == '@') && !strstr(t, "$(")) {
		nativeVal_t v;
		nativeBuf_t str;
		Native_BufInit(&str);
		if (Def_Value(m, t, 0, &v) && Mode_StrBuf(&v, &str)) {
			Str_Copy(out, str.p, size);
		}
		Native_BufFree(&str);
		Native_Free(&v);
		return out;
	}
	return Def_Text(m, t, out, size);
}

/* the value is a list or a tuple (a $name's list is its object) */
static byte Def_IsSequence(const nativeVal_t *v) {
	if (NObj_Native(v)) {
		return NObj_Kind(v) == NO_LIST;
	}
	return v->type == NV_LIST || v->type == NV_TUPLE || (Native_IsObj(v) && (PyList_Check(Native_PyOf(v)) || PyTuple_Check(Native_PyOf(v))));
}

/* the list name as a tuple of lists, one per entry line, into *out (as Def_Value) */
static byte Def_List(scriptDef_t *m, const char *name, nativeVal_t *out) {
	Native_SetNone(out);
	for (int i = m->numBlocks - 1; i >= 0; i--) {
		scriptBlock_t *bl = &m->blocks[i];
		if (!bl->list || strcmp(bl->name, name)) {
			continue;
		}
		Native_SetEmpty(out, NV_TUPLE);
		for (const char *p = bl->start; p < bl->end;) {
			char buf[SCRIPT_MAX_TEXT];
			scriptLine_t l;
			p = Def_Tokens(p, buf, sizeof(buf), &l);
			if (!l.n) {
				continue;
			}
			if (l.n == 1 && !l.quoted[0]) {
				/* a single value that is a list or tuple is the entry itself */
				nativeVal_t v;
				if (Def_Value(m, l.tok[0], 0, &v) && Def_IsSequence(&v)) {
					*Native_Append(out) = v;
					continue;
				}
				Native_Free(&v);
			}
			nativeVal_t *e = Native_Append(out);
			Native_SetEmpty(e, NV_LIST);
			for (int j = 0; j < l.n; j++) {
				/* a value that fails is None */
				Def_Value(m, l.tok[j], l.quoted[j], Native_Append(e));
			}
		}
		return 1;
	}
	Con_Warnf("%s:%d: no list %s\n", m->name, m->line, name);
	return 0;
}

static void Def_Set(scriptDef_t *m, const char *item, const char *var, const char *value, byte quoted) {
	if (!quoted && (value[0] == '@' || value[0] == '%' || value[0] == '$') && !strstr(value, "$(")) {
		nativeVal_t v;
		byte ok = Def_Value(m, value, 0, &v);
		if (ok && (v.type == NV_INT || v.type == NV_FLOAT)) {
			Script_SetNum(m->id, item, var, v.type == NV_INT ? (double)v.i : v.f);
		} else if (ok && v.type == NV_STR) {
			Script_SetStr(m->id, item, var, Native_Text(&v));
		} else if (ok && m->stop) {
			/* another value goes to SScorer.Set as in the script, with its TypeError (through its ref) */
			PyObject *o = Native_ToPy(&v);
			PyObject *r = o ? PORT_ASK(SScorer, Set, "issO", (long)m->id, item, var, o) : NULL;
			m->failed = r == NULL;
			Py_XDECREF(r);
			Py_XDECREF(o);
		}
		Native_Free(&v);
		return;
	}
	double d;
	if (!quoted && Def_Number(value, &d)) {
		Script_SetNum(m->id, item, var, d);
	} else {
		char text[SCRIPT_MAX_TEXT];
		Script_SetStr(m->id, item, var, Def_Text(m, value, text, sizeof(text)));
	}
}

/* the attribute lines of a style onto the item */
static void Def_ApplyStyle(scriptDef_t *m, const char *name) {
	for (int i = m->numBlocks - 1; i >= 0; i--) {
		scriptBlock_t *st = &m->blocks[i];
		if (st->list || strcmp(st->name, name)) {
			continue;
		}
		for (const char *p = st->start; p < st->end;) {
			char buf[SCRIPT_MAX_TEXT];
			scriptLine_t l;
			p = Def_Tokens(p, buf, sizeof(buf), &l);
			if (l.n >= 2) {
				Def_Set(m, m->item, l.tok[0], l.tok[1], l.quoted[1]);
			}
		}
		return;
	}
	Con_Warnf("%s:%d: no style %s\n", m->name, m->line, name);
}

/* the arguments of a call or run line: the positional values (v[0] to v[npos - 1]), then the Name=value ones (their
 * names[i], lens[i] bytes up to the '=') */
typedef struct {
	nativeVal_t v[SCRIPT_MAX_TOKENS];
	const char *names[SCRIPT_MAX_TOKENS];
	int lens[SCRIPT_MAX_TOKENS];
	int npos, n;
} defArgs_t;

/* the arguments from token first on, in the order of the line (a value that fails is None; Def_ArgsFree them) */
static void Def_Args(scriptDef_t *m, scriptLine_t *l, int first, defArgs_t *a) {
	int pos = 0;
	a->npos = 0;
	for (int i = first; i < l->n; i++) {
		a->npos += l->quoted[i] || !strchr(l->tok[i], '=');
	}
	a->n = a->npos;
	for (int i = first; i < l->n; i++) {
		const char *eq = l->quoted[i] ? NULL : strchr(l->tok[i], '=');
		int k = eq ? a->n++ : pos++;
		if (eq) {
			a->names[k] = l->tok[i];
			a->lens[k] = (int)(eq - l->tok[i]);
		}
		Def_Value(m, eq ? eq + 1 : l->tok[i], eq ? eq[1] == '"' : l->quoted[i], &a->v[k]);
	}
}

static void Def_ArgsFree(defArgs_t *a) {
	for (int i = 0; i < a->n; i++) {
		Native_Free(&a->v[i]);
	}
}

/* OBJLIB 7: the calls of the call lines by path, for the test channel: through the registry (def_site: fast through
 * the entry, slow through Python when the entry cannot take the keywords; the site names the function it called
 * last), by name to a function of the registry that something else replaced, by name to any other function */
static portSite_t def_site = { .ref = &port_value, .file = __FILE__, .line = __LINE__ };
static unsigned long def_byName, def_other, def_errors;

void Script_DefCounts(unsigned long *registry, unsigned long *byName, unsigned long *other, unsigned long *errors,
	byte reset) {
	*registry = def_site.fast + def_site.slow;
	*byName = def_byName;
	*other = def_other;
	*errors = def_errors;
	if (reset) {
		def_byName = def_other = def_errors = 0;
	}
}

/* the longest keyword name of a call through the registry (PORT_NAME of src/game/Script/ScriptPort.c) */
#define DEF_KWNAME 0x40

/* the letters of Port_Invoke for the arguments ("vv;Name=v,Other=v") into fmt: 0 when the registry cannot take them
 * that way (more than PORT_ARGS, a keyword name that is empty, too long, or holds a ',' or ';') */
static byte Def_Letters(const defArgs_t *a, char *fmt, int size) {
	int k = 0;
	if (a->n > PORT_ARGS) {
		return 0;
	}
	for (int i = 0; i < a->n; i++) {
		if (i >= a->npos) {
			const char *s = a->names[i];
			int len = a->lens[i];
			if (!len || len >= DEF_KWNAME || memchr(s, ',', len) || memchr(s, ';', len) || k + len + 3 >= size) {
				return 0;
			}
			fmt[k++] = i == a->npos ? ';' : ',';
			memcpy(fmt + k, s, len);
			k += len;
			fmt[k++] = '=';
		}
		fmt[k++] = 'v';
	}
	fmt[k] = 0;
	return 1;
}

/* module.func(args) of a call line: 1, or 0 with the error set. Through the registry (Port_Invoke: the entry, the
 * arguments packed as a call through Python hands them over) while the module attribute is the port's or the engine's
 * own function of module.func (the seam; SCRAP_PORT_SLOW=1 never), else the call by name of what is there with the
 * arguments as a call through Python hands them over (NObj_CallArg) and the keywords in the order of the line: a
 * recorder, a replaced or deleted function, a function of another name or module there, a function that is no port,
 * the keywords of a call the registry cannot take */
static byte Def_Call(const char *module, const char *func, const defArgs_t *a) {
	static_assert(PORT_ARGS == 11, "one pointer per argument");
	char fmt[PORT_ARGS * (DEF_KWNAME + 3) + 2];
	nativeVal_t f, args[SCRIPT_MAX_TOKENS], kw, r;
	if (!ModVar_ModAttrVal(module, func, &f)) {
		return 0;
	}
	byte known = Port_Find(module, func) != NULL;
	const portRef_t *ref = known && !Port_Slow() ? NObj_PortRef(&f) : NULL;
	if (ref && !strcmp(ref->module, module) && !strcmp(ref->name, func) && Def_Letters(a, fmt, sizeof(fmt))) {
		const nativeVal_t *p[PORT_ARGS] = { NULL };
		for (int i = 0; i < a->n; i++) {
			p[i] = &a->v[i];
		}
		Native_Free(&f);
		def_site.ref = ref;
		return Port_Invoke(&def_site, NULL, fmt, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
	}
	if (known) {
		def_byName++;
	} else {
		def_other++;
	}
	Native_SetEmpty(&kw, NV_LIST);
	for (int i = 0; i < a->n; i++) {
		if (i < a->npos) {
			NObj_CallArg(&a->v[i], &args[i]);
		} else {
			/* a name given twice keeps its first place with the last value, as in the dict of the keywords */
			nativeVal_t *pair = NULL;
			for (int k = 0; k < kw.n && !pair; k++) {
				const nativeVal_t *name = &kw.items[k].items[0];
				pair = name->n == a->lens[i] && !memcmp(Native_Text(name), a->names[i], a->lens[i]) ? &kw.items[k] :
					NULL;
			}
			if (pair) {
				Native_Free(&pair->items[1]);
				NObj_CallArg(&a->v[i], &pair->items[1]);
				continue;
			}
			pair = Native_Append(&kw);
			Native_SetEmpty(pair, NV_TUPLE);
			Native_SetText(Native_Append(pair), a->names[i], a->lens[i]);
			NObj_CallArg(&a->v[i], Native_Append(pair));
		}
	}
	byte ok = NObj_CallKw(&f, args, a->npos, kw.n ? &kw : NULL, &r);
	Native_Free(&r);
	for (int i = 0; i < a->npos; i++) {
		Native_Free(&args[i]);
	}
	Native_Free(&kw);
	Native_Free(&f);
	return ok;
}

static void Def_DoCall(scriptDef_t *m, scriptLine_t *l) {
	char module[0x40];
	const char *func = Def_Split(l->tok[1], module, sizeof(module));
	if (!func) {
		Con_Warnf("%s:%d: call needs Module.Function\n", m->name, m->line);
		return;
	}
	defArgs_t a;
	Def_Args(m, l, 2, &a);
	if (!m->failed) {
		byte ok = Def_Call(module, func, &a);
		def_errors += !ok;
		if (m->stop) {
			m->failed = !ok;
		} else if (!ok) {
			Con_Warnf("Script: %s.%s failed\n", module, func);
			Native_ErrPrint();
		}
	}
	Def_ArgsFree(&a);
}

byte Script_StepValues(const nativeVal_t *args, int n, int type, byte pass, nativeVal_t *out) {
	nativeVal_t seq;
	Native_SetEmpty(&seq, type);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&seq), &args[i]);
	}
	if (!pass) {
		*out = seq;
		return 1;
	}
	byte ok = Native_Pass(&seq, out);
	Native_Free(&seq);
	return ok;
}

static void Def_DoRun(scriptDef_t *m, scriptLine_t *l) {
	for (const scriptStep_t *s = m->steps; s && s->name; s++) {
		if (strcmp(s->name, l->tok[1])) {
			continue;
		}
		/* the positional values of the line as they are (the Name=value ones are evaluated and dropped) */
		defArgs_t a;
		Def_Args(m, l, 2, &a);
		s->fn(m->id, a.v, a.npos);
		Def_ArgsFree(&a);
		/* a step that failed leaves its error set: it stops a file of Script_RunFileTry */
		if (Native_Failed()) {
			if (m->stop) {
				m->failed = 1;
			} else {
				Con_Warnf("%s:%d: step %s failed\n", m->name, m->line, l->tok[1]);
				Native_ErrPrint();
			}
		}
		return;
	}
	Con_Warnf("%s:%d: no step %s\n", m->name, m->line, l->tok[1]);
}

/* the condition of an if or elif line */
static byte Def_Cond(scriptDef_t *m, scriptLine_t *l) {
	char a[SCRIPT_MAX_TEXT], b[SCRIPT_MAX_TEXT];
	if (l->n == 4 && (!strcmp(l->tok[2], "==") || !strcmp(l->tok[2], "!="))) {
		Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a));
		Def_Word(m, l->tok[3], l->quoted[3], b, sizeof(b));
		return (strcmp(a, b) == 0) == (l->tok[2][0] == '=');
	}
	if (l->n == 4 && strchr("<>", l->tok[2][0]) && (!l->tok[2][1] || (l->tok[2][1] == '=' && !l->tok[2][2]))) {
		/* numbers */
		double x = strtod(Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a)), NULL);
		double y = strtod(Def_Word(m, l->tok[3], l->quoted[3], b, sizeof(b)), NULL);
		byte eq = l->tok[2][1] == '=';
		return l->tok[2][0] == '<' ? (x < y || (eq && x == y)) : (x > y || (eq && x == y));
	}
	if (l->n != 2) {
		Con_Warnf("%s:%d: if <value> or if <value> <op> <value>\n", m->name, m->line);
		return 0;
	}
	Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a));
	return a[0] && strcmp(a, "0");
}

static const char *const def_commands[] = { "style", "item", "use", "set", "lr", "ud", "default", "list", "call", "run",
	"section", "do", "if", "elif", "else", "end", "let", "measure", "action", "getaction", "class", "event", "setvar",
	"entity", "object" };

/* an attribute of SAct's current action or class (TARGET_ACTION, TARGET_CLASS), as SAct.SetAct and SetCls set it: a
 * text or a number; or a variable of the entity (TARGET_ENTITY) */
static void Def_SetAct(scriptDef_t *m, byte target, const char *var, const char *value, byte quoted) {
	if (target == TARGET_OBJECT) {
		/* an attribute of an object, as `obj.var = value` of a script (a pack module's variable through the
		 * registry) */
		nativeVal_t nv;
		byte got = Def_Value(m, value, quoted, &nv), bad = 0;
		if (got && m->hasObject) {
			bad = !Mode_VSet(&m->object, var, &nv);
		}
		Native_Free(&nv);
		if (bad) {
			if (m->stop) {
				/* an entity handle the engine refuses without an exception: ceval's SystemError */
				if (!Native_Failed()) {
					Script_CheckSilent("def object set (engine)");
					Script_Fail();
				}
				m->failed = 1;
			} else {
				Con_Warnf("%s:%d: cannot set %s\n", m->name, m->line, var);
				Native_ErrClear();
			}
		}
		return;
	}
	if (target == TARGET_ENTITY) {
		/* a script variable of the entity, as `entity.var = value` of a script (the set of the handle on the native
		 * value) */
		nativeVal_t nv;
		if (Def_Value(m, value, quoted, &nv) && !Native_EntStore(World_FindEntity(m->entity), m->entity, var, &nv) &&
			m->stop) {
			/* the attribute set of the handle fails without an exception: ceval's SystemError */
			if (!Native_Failed()) {
				Script_CheckSilent("def entity set (engine)");
				Native_Raise(NE_SYSTEM, "error return without exception set");
			}
			m->failed = 1;
		}
		Native_Free(&nv);
		return;
	}
	void *obj = target == TARGET_CLASS ? (void *)g_pAuxActClass : (void *)g_pAuxAction;
	if (!obj) {
		Con_Warnf("%s:%d: no current %s for %s\n", m->name, m->line, target == TARGET_CLASS ? "class" : "action", var);
		return;
	}
	nativeVal_t v;
	byte ok = Def_Value(m, value, quoted, &v);
	char *s = NULL;
	double d = 0.0;
	int i = 0;
	if (ok && v.type == NV_STR) {
		s = (char *)Native_Text(&v);
	} else if (ok && (v.type == NV_INT || v.type == NV_FLOAT)) {
		d = v.type == NV_INT ? (double)v.i : v.f;
		i = (int)d;
	} else {
		Con_Warnf("%s:%d: %s needs a text or a number\n", m->name, m->line, var);
		Native_Free(&v);
		return;
	}
	if (target == TARGET_CLASS) {
		cActClass_Var(g_pAuxActClass, var, 1, s ? &s : NULL, s ? NULL : &d, s ? NULL : &i);
	} else {
		cAction_Var(g_pAuxAction, var, 1, s ? &s : NULL, s ? NULL : &d, s ? NULL : &i);
	}
	Native_Free(&v);
}

/* a $name of the file: into its own dict when the C function gave none */
static void Def_SetVar(scriptDef_t *m, const char *name, const nativeVal_t *v) {
	Script_DefVar(&m->vars, name, v);
}

/* let <name> <value> [value ...]: the sum of the values, or the value when there is one that is no number */
static void Def_Let(scriptDef_t *m, scriptLine_t *l) {
	double sum = 0.0;
	byte isInt = 1;
	nativeVal_t v;
	if (l->n == 3) {
		byte ok = Def_Value(m, l->tok[2], l->quoted[2], &v);
		if (ok && v.type != NV_INT && v.type != NV_FLOAT) {
			Def_SetVar(m, l->tok[1], &v);
			Native_Free(&v);
			return;
		}
		Native_Free(&v);
	}
	for (int i = 2; i < l->n; i++) {
		byte ok = Def_Value(m, l->tok[i], l->quoted[i], &v);
		if (ok && v.type == NV_INT) {
			sum += v.i;
		} else if (ok && v.type == NV_FLOAT) {
			sum += v.f;
			isInt = 0;
		} else {
			Con_Warnf("%s:%d: let adds numbers\n", m->name, m->line);
		}
		Native_Free(&v);
	}
	if (isInt && sum == (long)sum) {
		Native_SetInt(&v, (long)sum);
	} else {
		Native_SetFloat(&v, sum);
	}
	Def_SetVar(m, l->tok[1], &v);
}

/* measure <name> <font> <text>: the width of the text in the font (SScorer.GetTextArea) */
static void Def_Measure(scriptDef_t *m, scriptLine_t *l) {
	char font[0x40], text[SCRIPT_MAX_TEXT];
	float pos[2], size[2] = { 0.0f, 0.0f };
	CFont *f = Font_Get(Def_Word(m, l->tok[2], l->quoted[2], font, sizeof(font)));
	if (!f) {
		Con_Warnf("%s:%d: no font %s\n", m->name, m->line, font);
	} else {
		Font_TextSize(f, pos, size, 0.0f, 0.0f, "%s", Def_Word(m, l->tok[3], l->quoted[3], text, sizeof(text)));
	}
	nativeVal_t v;
	Native_SetFloat(&v, size[0]);
	Def_SetVar(m, l->tok[1], &v);
}

static byte Def_IsCommand(scriptLine_t *l) {
	if (l->quoted[0]) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(def_commands) / sizeof(def_commands[0]); i++) {
		if (!strcmp(l->tok[0], def_commands[i])) {
			return 1;
		}
	}
	return 0;
}

/* if, elif, else, end: 1 when the line was one of them */
static byte Def_Branch(scriptDef_t *m, scriptLine_t *l) {
	const char *cmd = l->tok[0];
	int d = m->depth;
	byte outer = d < 2 || m->active[d - 2];

	if (!strcmp(cmd, "if")) {
		if (d >= SCRIPT_MAX_DEPTH) {
			Con_Warnf("%s:%d: if nested too deep\n", m->name, m->line);
			return 1;
		}
		byte run = (d == 0 || m->active[d - 1]) && Def_Cond(m, l);
		m->active[d] = run;
		m->taken[d] = run || !(d == 0 || m->active[d - 1]);
		m->depth++;
	} else if (!strcmp(cmd, "elif") && d) {
		byte run = outer && !m->taken[d - 1] && Def_Cond(m, l);
		m->active[d - 1] = run;
		m->taken[d - 1] |= run;
	} else if (!strcmp(cmd, "else") && d) {
		m->active[d - 1] = outer && !m->taken[d - 1];
		m->taken[d - 1] = 1;
	} else if (!strcmp(cmd, "end") && d) {
		m->depth--;
	} else if (!strcmp(cmd, "elif") || !strcmp(cmd, "else") || !strcmp(cmd, "end")) {
		Con_Warnf("%s:%d: %s without if\n", m->name, m->line, cmd);
	} else {
		return 0;
	}
	return 1;
}

static void Def_Run(scriptDef_t *m, byte header, const char *section);

/* the file that runs (the innermost), for the steps */
static scriptDef_t *def_running;

const char *Script_CurrentItem(void) {
	return def_running ? def_running->item : "";
}

void Script_SetValue(const char *name, const nativeVal_t *value) {
	if (def_running) {
		Def_SetVar(def_running, name, value);
	}
}

static void Def_Line(scriptDef_t *m, scriptLine_t *l, const char *next) {
	const char *cmd = l->tok[0];
	char a[0x80], b[0x80];

	if (Def_Branch(m, l)) {
		m->open = NULL;
		return;
	}
	if (m->depth && !m->active[m->depth - 1]) {
		return;
	}
	if (m->open && !Def_IsCommand(l)) {
		m->open->end = next;	/* a line of the style or list */
		return;
	}
	m->open = NULL;
	if ((!strcmp(cmd, "style") || !strcmp(cmd, "list")) && l->n == 2) {
		if (m->numBlocks >= SCRIPT_MAX_BLOCKS) {
			Con_Warnf("%s:%d: too many styles and lists\n", m->name, m->line);
			return;
		}
		m->open = &m->blocks[m->numBlocks++];
		Str_Copy(m->open->name, l->tok[1], sizeof(m->open->name));
		m->open->start = m->open->end = next;
		m->open->list = cmd[0] == 'l';
	} else if (!strcmp(cmd, "action") && l->n == 1) {
		/* the current action (the last one a helper made) */
		m->target = TARGET_ACTION;
	} else if ((!strcmp(cmd, "action") || !strcmp(cmd, "getaction")) && l->n == 2) {
		Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a));
		if (!g_pAuxActClass) {
			Con_Warnf("%s:%d: %s without a class\n", m->name, m->line, cmd);
			return;
		}
		g_pAuxAction = cmd[0] == 'a' ? cActClass_CreateAction(g_pAuxActClass, a) : cActClass_GetAction(g_pAuxActClass, a);
		m->target = TARGET_ACTION;
	} else if (!strcmp(cmd, "class") && l->n == 1) {
		/* the current class (a sound script adds to the class its character script builds) */
		m->target = TARGET_CLASS;
	} else if (!strcmp(cmd, "class") && l->n == 2) {
		CHECK(g_pActClassList);
		g_pAuxActClass = cActClassList_Get(g_pActClassList, Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a)));
		m->target = TARGET_CLASS;
	} else if (!strcmp(cmd, "event") && (l->n == 4 || l->n == 5)) {
		/* event <time> <name> <function> [discardable]: the order of the script's SetAct lines */
		Def_SetAct(m, TARGET_ACTION, "AddEventAtTime", l->tok[1], l->quoted[1]);
		Def_Word(m, l->tok[2], l->quoted[2], a, sizeof(a));
		if (g_pAuxAction) {
			char *s = a;
			cAction_Var(g_pAuxAction, "EventName", 1, &s, NULL, NULL);
		}
		if (l->n == 5) {
			Def_SetAct(m, TARGET_ACTION, "EventDiscardable", l->tok[4], l->quoted[4]);
		}
		Def_Word(m, l->tok[3], l->quoted[3], b, sizeof(b));
		if (g_pAuxAction) {
			char *s = b;
			cAction_Var(g_pAuxAction, "EventFunc", 1, &s, NULL, NULL);
		}
	} else if (!strcmp(cmd, "entity") && l->n == 2) {
		Def_Word(m, l->tok[1], l->quoted[1], m->entity, sizeof(m->entity));
		m->target = TARGET_ENTITY;
	} else if (!strcmp(cmd, "object") && l->n == 2) {
		Native_Free(&m->object);
		m->hasObject = Def_Value(m, l->tok[1], l->quoted[1], &m->object);
		m->target = TARGET_OBJECT;
		/* a stopping file stops at an object it cannot get, as the script's obj.var = ... raises (a missing $value
		 * is an unbound name there) */
		if (!m->hasObject && m->stop) {
			if (!Native_Failed()) {
				Native_Raise(NE_NAME, "%s", l->tok[1][0] == '$' ? l->tok[1] + 1 : l->tok[1]);
			}
			m->failed = 1;
		}
	} else if (!strcmp(cmd, "setvar") && l->n == 3) {
		const char *var = Def_Split(l->tok[1], a, sizeof(a));
		if (!var) {
			Con_Warnf("%s:%d: setvar needs Module.var\n", m->name, m->line);
			return;
		}
		nativeVal_t v;
		if (!Def_Value(m, l->tok[2], l->quoted[2], &v) || !ModVar_SetModAttrVal(a, var, &v)) {
			Native_ErrClear();
		}
		Native_Free(&v);
	} else if (!strcmp(cmd, "item") && l->n >= 3) {
		m->target = TARGET_ITEM;
		Def_Word(m, l->tok[1], l->quoted[1], m->item, sizeof(m->item));
		if (!Script_Item(m->id, m->item)) {
			Script_Add(m->id, m->item, l->tok[2]);
		}
		for (int i = 3; i < l->n; i++) {
			Def_ApplyStyle(m, l->tok[i]);
		}
	} else if (!strcmp(cmd, "use") && l->n == 2 && m->item[0]) {
		Def_ApplyStyle(m, l->tok[1]);
	} else if (!strcmp(cmd, "set") && l->n == 4) {
		Def_Set(m, Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a)), l->tok[2], l->tok[3], l->quoted[3]);
	} else if ((!strcmp(cmd, "lr") || !strcmp(cmd, "ud")) && l->n == 3) {
		/* Menu.LinkLR(id, a, b), Menu.LinkUD: the second word first, as the call by name took them (the order of
		 * their warnings) */
		Def_Word(m, l->tok[2], l->quoted[2], b, sizeof(b));
		Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a));
		if (!(cmd[0] == 'l' ? PORT_CALL(Menu, LinkLR, NULL, "iss", (long)m->id, a, b) :
			PORT_CALL(Menu, LinkUD, NULL, "iss", (long)m->id, a, b))) {
			Con_Warnf("Script: Menu.%s failed\n", cmd[0] == 'l' ? "LinkLR" : "LinkUD");
			Native_ErrPrint();
		}
	} else if (!strcmp(cmd, "default") && l->n == 2) {
		Script_SetDefault(m->id, Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a)));
	} else if (!strcmp(cmd, "call") && l->n >= 2) {
		Def_DoCall(m, l);
	} else if (!strcmp(cmd, "run") && l->n >= 2) {
		Def_DoRun(m, l);
	} else if (!strcmp(cmd, "let") && l->n >= 3) {
		Def_Let(m, l);
	} else if (!strcmp(cmd, "measure") && l->n == 4) {
		Def_Measure(m, l);
	} else if (!strcmp(cmd, "do") && l->n == 2) {
		if (m->nested >= SCRIPT_MAX_DEPTH) {
			Con_Warnf("%s:%d: do nested too deep\n", m->name, m->line);
			return;
		}
		int depth = m->depth;
		m->nested++;
		Def_Run(m, 0, l->tok[1]);
		m->nested--;
		m->depth = depth;
		m->open = NULL;
	} else if (l->n == 2 && m->target != TARGET_ITEM && !Def_IsCommand(l)) {
		Def_SetAct(m, m->target, cmd, l->tok[1], l->quoted[1]);
	} else if (l->n == 2 && m->item[0] && !Def_IsCommand(l)) {
		Def_Set(m, m->item, cmd, l->tok[1], l->quoted[1]);
	} else {
		Con_Warnf("%s:%d: what is '%s'?\n", m->name, m->line, cmd);
	}
}

/* the lines before the first section (header) and those of section (NULL: all lines of a file without sections) */
static void Def_Run(scriptDef_t *m, byte header, const char *section) {
	byte in = header;
	byte found = 0;
	int line = 0;
	for (const char *p = m->text; *p;) {
		char buf[SCRIPT_MAX_TEXT];
		scriptLine_t l;
		const char *next = Def_Tokens(p, buf, sizeof(buf), &l);
		line++;
		p = next;
		if (!l.n) {
			continue;
		}
		if (!l.quoted[0] && !strcmp(l.tok[0], "section") && l.n >= 2) {
			if (found) {
				break;
			}
			in = section && !strcmp(l.tok[1], section);
			found = in;
			m->open = NULL;
			continue;
		}
		if (in) {
			m->line = line;
			Def_Line(m, &l, next);
			if (m->failed) {
				return;
			}
		}
	}
	if (section && !found) {
		Con_Warnf("%s: no section %s\n", m->name, section);
	}
}

static byte Def_RunFile(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	const nativeVal_t *vars, byte stop) {
	/* a file may run another from a call (CreateControlMenu): one buffer per level */
	static char files[SCRIPT_MAX_NESTED][SCRIPT_MAX_FILE];
	static int level;
	char path[0x100];
	const char *text = builtin;

	Str_Printf(path, sizeof(path), "scrapc/%s", name);
	if (level < SCRIPT_MAX_NESTED) {
		char *file = files[level];
		int size = Pack_ReadFile(path, file, SCRIPT_MAX_FILE - 1);
		if (size >= 0) {
			file[size] = 0;
			text = file;
			Con_Verbosef("Script: %s from the packs\n", path);
		}
	}
	if (!text) {
		Con_Warnf("Script: no %s\n", name);
		return 0;
	}
	scriptDef_t m = { .id = id, .steps = steps, .text = text, .section = section, .name = name, .stop = stop };
	Native_SetNone(&m.object);
	if (vars) {
		Native_Copy(&m.vars, vars);
	} else {
		Native_SetNone(&m.vars);
	}
	scriptDef_t *outer = def_running;
	def_running = &m;
	level++;
	Def_Run(&m, 1, section);
	level--;
	def_running = outer;
	if (m.depth && !m.failed) {
		Con_Warnf("%s: if without end\n", name);
	}
	Native_Free(&m.vars);
	Native_Free(&m.object);
	return !m.failed;
}

byte Script_RunFile(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	const nativeVal_t *vars) {
	return Def_RunFile(id, name, section, builtin, steps, vars, 0);
}

byte Script_RunFileTry(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	const nativeVal_t *vars) {
	if (!Def_RunFile(id, name, section, builtin, steps, vars, 1)) {
		if (!Native_Failed()) {
			Script_CheckSilent("Script_RunFileTry");
			Native_Raise(NE_SYSTEM, "%s could not be read", name);
		}
		return 0;
	}
	return 1;
}
