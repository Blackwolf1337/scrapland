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
 *	lr <a> <b>, ud <a> <b>		Menu.LinkLR, Menu.LinkUD
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
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/pyentity.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/script.h"
#include "scrap/timer.h"

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
	{ "Init", g_InitMethods },
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
	{ "BankMaster", g_BankMasterMethods },
	{ "Berto", g_BertoMethods },
	{ "Nurse", g_NurseMethods },
	{ "Desktop", g_DesktopMethods },
	{ "Police", g_PoliceMethods },
	{ "Police", g_PoliceFuncs },
	{ "GateKeeper", g_GateKeeperMethods },
	{ "Dtritus", g_DtritusMethods },
	{ "Sputnik", g_SputnikMethods },
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
	{ "CreditsText", g_CreditsTextMethods },
	{ "Traffic", g_TrafficMethods },
	{ "OutPolice", g_OutPoliceMethods },
	{ "InPolice", g_InPoliceMethods },
	{ "PhoneCab", g_PhoneCabMethods },
	{ "DTritusDesktop", g_DTritusDesktopMethods },
	{ "Metro", g_MetroMethods },
	{ "Challenge", g_ChallengeMethods },
};

void Script_ModuleLoaded(const char *name, PyObject *module) {
	/* SCRAP_PACK_SCRIPTS=1: the scripts of the packs only (to compare a port with the original) */
	const char *only = getenv("SCRAP_PACK_SCRIPTS");
	if (only && *only == '1') {
		return;
	}
	for (size_t i = 0; i < sizeof(script_modules) / sizeof(script_modules[0]); i++) {
		const scriptModule_t *sm = &script_modules[i];
		if (strcmp(sm->name, name)) {
			continue;
		}
		PyObject *dict = PyModule_GetDict(module);
		for (PyMethodDef *m = sm->methods; m->ml_name; m++) {
			PyObject *f = PyCFunction_New(m, NULL);
			if (!f) {
				continue;
			}
			/* names the module bound to the Python function when it loaded (onCreateMainMenu = CreateMainMenu) */
			PyObject *key, *value;
			int pos = 0;
			while (PyDict_Next(dict, &pos, &key, &value)) {
				if (PyFunction_Check(value) && PyString_Check(((PyFunctionObject *)value)->func_name) &&
					!strcmp(PyString_AsString(((PyFunctionObject *)value)->func_name), m->ml_name) &&
					strcmp(PyString_AsString(key), m->ml_name)) {
					PyDict_SetItem(dict, key, f);
				}
			}
			PyDict_SetItemString(dict, m->ml_name, f);
			Py_DECREF(f);
		}
		Con_Verbosef("Script: the C functions of %s are in place\n", name);
	}
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
	PyObject *m = PyImport_ImportModule((char *)module);
	if (!m) {
		return NULL;
	}
	PyObject *f = PyObject_GetAttrString(m, (char *)func);
	Py_DECREF(m);
	return f;
}

double Script_GetFloat(const char *module, const char *var, double def) {
	PyObject *v = Script_Attr(module, var);
	double d = v && PyFloat_Check(v) ? PyFloat_AsDouble(v) : v && PyInt_Check(v) ? (double)PyInt_AsLong(v) : def;
	if (!v) {
		PyErr_Clear();
	}
	Py_XDECREF(v);
	return d;
}

/* ---- entity handles and values as a script uses them ---- */

byte Script_HSet(PyObject *e, const char *var, PyObject *v) {
	int ok = v && PyObject_SetAttrString(e, (char *)var, v) == 0;
	if (v && !ok && !PyErr_Occurred()) {
		/* an entity variable the engine refuses fails without an exception; ceval reports it so */
		PyErr_SetString(PyExc_SystemError, "error return without exception set");
	}
	Py_XDECREF(v);
	return ok;
}

int Script_HIs(PyObject *e, const char *var, const char *text) {
	PyObject *v = PyObject_GetAttrString(e, (char *)var);
	if (!v) {
		return -1;
	}
	int is = PyString_Check(v) && !strcmp(PyString_AsString(v), text);
	Py_DECREF(v);
	return is;
}

int Script_HPrefix(PyObject *e, const char *var, const char *prefix) {
	PyObject *v = PyObject_GetAttrString(e, (char *)var);
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
	return PyErr_Occurred() ? -2 : c;
}

byte Script_Later(double delay, PyObject *func, PyObject *args, const char *name) {
	double t = (double)Timer_GetTime(&g_pWorld->timer) + delay;
	byte ok = func && args && (name ? Script_Try("Scrap", "AddScheduledFunc", "(dOOs)", t, func, args, name) :
		Script_Try("Scrap", "AddScheduledFunc", "(dOO)", t, func, args));
	Py_XDECREF(args);
	return ok;
}

byte Script_LaterIn(double delay, const char *module, const char *func, PyObject *args, const char *name) {
	PyObject *f = Script_Attr(module, func);
	if (!f) {
		Py_XDECREF(args);
		return 0;
	}
	byte ok = Script_Later(delay, f, args, name);
	Py_DECREF(f);
	return ok;
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

int Script_EntSet(const char *ent, const char *var, PyObject *v) {
	int r = v ? PyEntity_SetVar(World_FindEntity(ent), ent, var, v) : -1;
	Py_XDECREF(v);
	return r;
}

PyObject *Script_Invoke(PyObject *callable, PyObject *args) {
	PyObject *r = callable && args ? PyEval_CallObject(callable, args) : NULL;
	Py_XDECREF(args);
	return r;
}

byte Script_CallKw(const char *module, const char *func, PyObject *args, PyObject *kw) {
	PyObject *f = Script_Attr(module, func);
	PyObject *r = NULL;
	if (f && args) {
		r = PyEval_CallObjectWithKeywords(f, args, kw);
	}
	Py_XDECREF(f);
	Py_XDECREF(args);
	Py_XDECREF(kw);
	if (!r) {
		Con_Warnf("Script: %s.%s failed\n", module, func);
		PyErr_Print();
		return 0;
	}
	Py_DECREF(r);
	return 1;
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

void Script_StepPrint(int id, PyObject *args) {
	char *text;
	(void)id;
	PyObject *out = PySys_GetObject("stdout");
	if (!PyArg_ParseTuple(args, "s", &text) || !out || PyFile_WriteString(text, out) < 0 || PyFile_WriteString("\n", out) < 0) {
		PyErr_Print();
	}
}

PyObject *Script_Method(PyObject *obj, const char *name, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = PyObject_GetAttrString(obj, (char *)name);
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

byte Script_Call(const char *module, const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Script_CallKw(module, func, args, NULL);
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
		PyErr_Clear();
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
		PyErr_Clear();
	}
	Py_XDECREF(v);
	return out;
}

void Script_SetVar(const char *module, const char *var, PyObject *value) {
	PyObject *m = PyImport_ImportModule((char *)module);
	if (!m || !value || PyObject_SetAttrString(m, (char *)var, value) < 0) {
		PyErr_Clear();
	}
	Py_XDECREF(value);
	Py_XDECREF(m);
}

void Script_SetInt(const char *module, const char *var, int value) {
	Script_SetVar(module, var, PyInt_FromLong(value));
}

PyObject *Script_Eval(const char *text) {
	PyObject *main = PyImport_AddModule("__main__");
	PyObject *dict = main ? PyModule_GetDict(main) : NULL;
	PyObject *r = dict ? PyRun_String((char *)text, Py_eval_input, dict, dict) : NULL;
	if (!r) {
		PyErr_Clear();
	}
	return r;
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

enum { TARGET_ITEM, TARGET_ACTION, TARGET_CLASS, TARGET_ENTITY };

typedef struct {
	int id;
	const scriptStep_t *steps;
	PyObject *vars;			/* the $names of the C function, a dict or NULL */
	PyObject *ownVars;		/* the dict of let and measure when the function gave none */
	const char *text;		/* the whole file */
	const char *section;
	char item[0x80];		/* the last item */
	byte target;			/* what attribute lines set: TARGET_ITEM, TARGET_ACTION, TARGET_CLASS or TARGET_ENTITY */
	char entity[0x80];		/* the entity of TARGET_ENTITY */
	scriptBlock_t blocks[SCRIPT_MAX_BLOCKS];
	int numBlocks;
	scriptBlock_t *open;		/* the style or list whose lines follow */
	int depth;			/* if */
	byte active[SCRIPT_MAX_DEPTH];	/* the lines of this level run (and those of the levels around it) */
	byte taken[SCRIPT_MAX_DEPTH];	/* a branch of this level ran or may not run */
	int nested;			/* do */
	const char *name;		/* the file and the line, for the warnings */
	int line;
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
		if (l->n < SCRIPT_MAX_TOKENS) {
			l->quoted[l->n] = *p == '"';
			l->tok[l->n++] = o;
		}
		if (*p == '"') {
			for (p++; *p && *p != '"' && *p != '\n'; p++) {
				if (o < end) {
					*o++ = *p;
				}
			}
			if (*p == '"') {
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

/* the text s with {Key} replaced by the language texts and $(name) by the value name of the file */
static const char *Def_Text(scriptDef_t *m, const char *s, char *out, int size) {
	int n = 0;
	while (*s && n < size - 1) {
		const char *close = *s == '{' ? strchr(s, '}') : s[0] == '$' && s[1] == '(' ? strchr(s, ')') : NULL;
		if (close && close - s < 0x40) {
			char key[0x40];
			const char *t = "";
			PyObject *str = NULL;
			if (*s == '{') {
				Str_Copy(key, s + 1, (int)(close - s));
				t = Script_Lang(key);
			} else {
				Str_Copy(key, s + 2, (int)(close - s) - 1);
				PyObject *v = m && m->vars ? PyDict_GetItemString(m->vars, key) : NULL;
				str = v ? PyObject_Str(v) : NULL;
				if (str) {
					t = PyString_AsString(str);
				} else {
					Con_Warnf("%s:%d: no value %s\n", m ? m->name : "", m ? m->line : 0, key);
				}
			}
			for (; *t && n < size - 1; t++) {
				out[n++] = *t;
			}
			Py_XDECREF(str);
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

static PyObject *Def_Number2Py(double d) {
	return d == (long)d ? PyInt_FromLong((long)d) : PyFloat_FromDouble(d);
}

static PyObject *Def_List(scriptDef_t *m, const char *name);

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

/* v with the operations of ops applied left to right ("+35", "/4*2"): the numbers of Python, an integer stays one
 * with integer operands and divides down; v given away, NULL when it is no number but ops has operations */
static PyObject *Def_Arith(scriptDef_t *m, PyObject *v, const char *ops) {
	if (!v || !*ops) {
		return v;
	}
	if (!PyInt_Check(v) && !PyFloat_Check(v)) {
		Con_Warnf("%s:%d: no number for %s\n", m->name, m->line, ops);
		Py_DECREF(v);
		return NULL;
	}
	byte isInt = PyInt_Check(v);
	long iv = isInt ? PyInt_AsLong(v) : 0;
	double dv = isInt ? (double)iv : PyFloat_AsDouble(v);
	Py_DECREF(v);
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
			return NULL;
		}
		byte xInt = !strchr(num, '.') && x == (long)x;
		if (isInt && xInt) {
			long y = (long)x;
			if (op == '/' && y == 0) {
				Con_Warnf("%s:%d: division by zero in %s\n", m->name, m->line, ops);
				return NULL;
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
	return isInt ? PyInt_FromLong(iv) : PyFloat_FromDouble(dv);
}

/* a value token as a Python object (a new reference, NULL with the warning printed) */
static PyObject *Def_Value(scriptDef_t *m, const char *t, byte quoted) {
	char text[SCRIPT_MAX_TEXT], a[0x80];
	const char *b;
	double d;

	if (quoted || strstr(t, "$(")) {
		return PyString_FromString((char *)Def_Text(m, t, text, sizeof(text)));
	}
	if (!strcmp(t, "$id")) {
		return PyInt_FromLong(m->id);
	}
	if (!strcmp(t, "$section")) {
		return PyString_FromString((char *)(m->section ? m->section : ""));
	}
	if (t[0] == '$') {
		char name[0x40];
		const char *ops = Def_Ops(t + 1);
		Str_Copy(name, t + 1, ops - t < (int)sizeof(name) ? (int)(ops - t) : (int)sizeof(name));
		PyObject *v = m->vars ? PyDict_GetItemString(m->vars, name) : NULL;
		if (!v) {
			Con_Warnf("%s:%d: no value %s\n", m->name, m->line, t);
			return NULL;
		}
		Py_INCREF(v);
		return Def_Arith(m, v, ops);
	}
	if (!strcmp(t, "None")) {
		Py_INCREF(Py_None);
		return Py_None;
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
		PyObject *tuple = PyTuple_New(n);
		int k = 0;
		for (char *p = inner; tuple && *p && k < n;) {
			char *comma = strchr(p, ',');
			if (comma) {
				*comma = 0;
			}
			PyObject *v = Def_Value(m, p, 0);
			if (!v) {
				Py_INCREF(Py_None);
				v = Py_None;
			}
			PyTuple_SetItem(tuple, k++, v);
			p = comma ? comma + 1 : p + strlen(p);
		}
		return tuple;
	}
	if (t[0] == '[' && t[strlen(t) - 1] == ']') {
		Str_Copy(a, t + 1, sizeof(a));
		a[strlen(a) - 1] = 0;
		return Def_List(m, a);
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
			return PyString_FromString(s ? s : "");
		case ACTVAR_DOUBLE:
			return Def_Arith(m, PyFloat_FromDouble(dv), ops);
		case ACTVAR_INT:
			return Def_Arith(m, PyInt_FromLong(iv), ops);
		}
		Con_Warnf("%s:%d: no action variable %s\n", m->name, m->line, var);
		return NULL;
	}
	if ((t[0] == '%' || t[0] == '@') && (b = Def_Split(t + 1, a, sizeof(a)))) {
		char var[0x80];
		const char *ops = Def_Ops(b);
		Str_Copy(var, b, ops - b < (int)sizeof(var) ? (int)(ops - b) + 1 : (int)sizeof(var));
		if (t[0] == '@') {
			return Def_Arith(m, Def_Number2Py(Script_GetNum(m->id, a, var)), ops);
		}
		PyObject *v = Script_Attr(a, var);
		if (!v) {
			PyErr_Clear();
			Con_Warnf("%s:%d: no %s\n", m->name, m->line, t + 1);
			return NULL;
		}
		return Def_Arith(m, v, ops);
	}
	if (Def_Number(t, &d)) {
		return strchr(t, '.') ? PyFloat_FromDouble(d) : PyInt_FromLong((long)d);
	}
	return PyString_FromString((char *)Def_Text(m, t, text, sizeof(text)));
}

/* a value token as a string into out (an item name, a link, a compared value) */
static const char *Def_Word(scriptDef_t *m, const char *t, byte quoted, char *out, int size) {
	out[0] = 0;
	if (!quoted && (t[0] == '$' || t[0] == '%' || t[0] == '@') && !strstr(t, "$(")) {
		PyObject *v = Def_Value(m, t, 0);
		PyObject *str = v ? PyObject_Str(v) : NULL;
		if (str) {
			Str_Copy(out, PyString_AsString(str), size);
		}
		Py_XDECREF(str);
		Py_XDECREF(v);
		return out;
	}
	return Def_Text(m, t, out, size);
}

/* the list name as a tuple of lists, one per entry line */
static PyObject *Def_List(scriptDef_t *m, const char *name) {
	for (int i = m->numBlocks - 1; i >= 0; i--) {
		scriptBlock_t *bl = &m->blocks[i];
		if (!bl->list || strcmp(bl->name, name)) {
			continue;
		}
		int n = 0;
		for (const char *p = bl->start; p < bl->end;) {
			char buf[SCRIPT_MAX_TEXT];
			scriptLine_t l;
			p = Def_Tokens(p, buf, sizeof(buf), &l);
			n += l.n > 0;
		}
		PyObject *r = PyTuple_New(n);
		n = 0;
		for (const char *p = bl->start; r && p < bl->end;) {
			char buf[SCRIPT_MAX_TEXT];
			scriptLine_t l;
			p = Def_Tokens(p, buf, sizeof(buf), &l);
			if (!l.n) {
				continue;
			}
			if (l.n == 1 && !l.quoted[0]) {
				/* a single value that is a list or tuple is the entry itself */
				PyObject *v = Def_Value(m, l.tok[0], 0);
				if (v && (PyList_Check(v) || PyTuple_Check(v))) {
					PyTuple_SetItem(r, n++, v);
					continue;
				}
				Py_XDECREF(v);
			}
			PyObject *e = PyList_New(l.n);
			for (int j = 0; e && j < l.n; j++) {
				PyObject *v = Def_Value(m, l.tok[j], l.quoted[j]);
				if (!v) {
					Py_INCREF(Py_None);
					v = Py_None;
				}
				PyList_SetItem(e, j, v);
			}
			PyTuple_SetItem(r, n++, e);
		}
		return r;
	}
	Con_Warnf("%s:%d: no list %s\n", m->name, m->line, name);
	return NULL;
}

static void Def_Set(scriptDef_t *m, const char *item, const char *var, const char *value, byte quoted) {
	if (!quoted && (value[0] == '@' || value[0] == '%' || value[0] == '$') && !strstr(value, "$(")) {
		PyObject *v = Def_Value(m, value, 0);
		if (v && (PyInt_Check(v) || PyFloat_Check(v))) {
			Script_SetNum(m->id, item, var, PyInt_Check(v) ? (double)PyInt_AsLong(v) : PyFloat_AsDouble(v));
		} else if (v && PyString_Check(v)) {
			Script_SetStr(m->id, item, var, PyString_AsString(v));
		}
		Py_XDECREF(v);
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

/* the arguments from token first on: a tuple, and the Name=value ones in *kw (NULL when there are none) */
static PyObject *Def_Args(scriptDef_t *m, scriptLine_t *l, int first, PyObject **kw) {
	int n = 0;
	*kw = NULL;
	for (int i = first; i < l->n; i++) {
		n += l->quoted[i] || !strchr(l->tok[i], '=');
	}
	PyObject *args = PyTuple_New(n);
	n = 0;
	for (int i = first; args && i < l->n; i++) {
		char *eq = l->quoted[i] ? NULL : strchr(l->tok[i], '=');
		PyObject *v = Def_Value(m, eq ? eq + 1 : l->tok[i], eq ? eq[1] == '"' : l->quoted[i]);
		if (!v) {
			Py_INCREF(Py_None);
			v = Py_None;
		}
		if (!eq) {
			PyTuple_SetItem(args, n++, v);
			continue;
		}
		if (!*kw) {
			*kw = PyDict_New();
		}
		*eq = 0;
		if (*kw) {
			PyDict_SetItemString(*kw, l->tok[i], v);
		}
		*eq = '=';
		Py_DECREF(v);
	}
	return args;
}

static void Def_DoCall(scriptDef_t *m, scriptLine_t *l) {
	char module[0x40];
	const char *func = Def_Split(l->tok[1], module, sizeof(module));
	if (!func) {
		Con_Warnf("%s:%d: call needs Module.Function\n", m->name, m->line);
		return;
	}
	PyObject *kw;
	PyObject *args = Def_Args(m, l, 2, &kw);
	Script_CallKw(module, func, args, kw);
}

static void Def_DoRun(scriptDef_t *m, scriptLine_t *l) {
	for (const scriptStep_t *s = m->steps; s && s->name; s++) {
		if (strcmp(s->name, l->tok[1])) {
			continue;
		}
		PyObject *kw;
		PyObject *args = Def_Args(m, l, 2, &kw);
		Py_XDECREF(kw);
		if (args) {
			s->fn(m->id, args);
			Py_DECREF(args);
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
	"entity" };

/* an attribute of SAct's current action or class (TARGET_ACTION, TARGET_CLASS), as SAct.SetAct and SetCls set it: a
 * text or a number; or a variable of the entity (TARGET_ENTITY) */
static void Def_SetAct(scriptDef_t *m, byte target, const char *var, const char *value, byte quoted) {
	if (target == TARGET_ENTITY) {
		/* a script variable of the entity, as `entity.var = value` of a script */
		PyObject *v = Def_Value(m, value, quoted);
		if (v) {
			PyEntity_SetVar(World_FindEntity(m->entity), m->entity, var, v);
			Py_DECREF(v);
		}
		return;
	}
	void *obj = target == TARGET_CLASS ? (void *)g_pAuxActClass : (void *)g_pAuxAction;
	if (!obj) {
		Con_Warnf("%s:%d: no current %s for %s\n", m->name, m->line, target == TARGET_CLASS ? "class" : "action", var);
		return;
	}
	PyObject *v = Def_Value(m, value, quoted);
	char *s = NULL;
	double d = 0.0;
	int i = 0;
	if (v && PyString_Check(v)) {
		s = PyString_AsString(v);
	} else if (v && (PyInt_Check(v) || PyFloat_Check(v))) {
		d = PyInt_Check(v) ? (double)PyInt_AsLong(v) : PyFloat_AsDouble(v);
		i = (int)d;
	} else {
		Con_Warnf("%s:%d: %s needs a text or a number\n", m->name, m->line, var);
		Py_XDECREF(v);
		return;
	}
	if (target == TARGET_CLASS) {
		cActClass_Var(g_pAuxActClass, var, 1, s ? &s : NULL, s ? NULL : &d, s ? NULL : &i);
	} else {
		cAction_Var(g_pAuxAction, var, 1, s ? &s : NULL, s ? NULL : &d, s ? NULL : &i);
	}
	Py_DECREF(v);
}

/* a $name of the file: into its own dict when the C function gave none */
static void Def_SetVar(scriptDef_t *m, const char *name, PyObject *v) {
	if (!m->vars) {
		m->vars = m->ownVars = PyDict_New();
	}
	if (m->vars && v) {
		PyDict_SetItemString(m->vars, (char *)name, v);
	}
	Py_XDECREF(v);
}

/* let <name> <value> [value ...]: the sum of the values, or the value when there is one that is no number */
static void Def_Let(scriptDef_t *m, scriptLine_t *l) {
	double sum = 0.0;
	byte isInt = 1;
	if (l->n == 3) {
		PyObject *v = Def_Value(m, l->tok[2], l->quoted[2]);
		if (v && !PyInt_Check(v) && !PyFloat_Check(v)) {
			Def_SetVar(m, l->tok[1], v);
			return;
		}
		Py_XDECREF(v);
	}
	for (int i = 2; i < l->n; i++) {
		PyObject *v = Def_Value(m, l->tok[i], l->quoted[i]);
		if (v && PyInt_Check(v)) {
			sum += PyInt_AsLong(v);
		} else if (v && PyFloat_Check(v)) {
			sum += PyFloat_AsDouble(v);
			isInt = 0;
		} else {
			Con_Warnf("%s:%d: let adds numbers\n", m->name, m->line);
		}
		Py_XDECREF(v);
	}
	Def_SetVar(m, l->tok[1], isInt && sum == (long)sum ? PyInt_FromLong((long)sum) : PyFloat_FromDouble(sum));
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
	Def_SetVar(m, l->tok[1], PyFloat_FromDouble(size[0]));
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

void Script_SetValue(const char *name, PyObject *value) {
	if (def_running) {
		Def_SetVar(def_running, name, value);
	} else {
		Py_XDECREF(value);
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
	} else if (!strcmp(cmd, "setvar") && l->n == 3) {
		const char *var = Def_Split(l->tok[1], a, sizeof(a));
		if (!var) {
			Con_Warnf("%s:%d: setvar needs Module.var\n", m->name, m->line);
			return;
		}
		Script_SetVar(a, var, Def_Value(m, l->tok[2], l->quoted[2]));
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
		Script_Call("Menu", cmd[0] == 'l' ? "LinkLR" : "LinkUD", "(iss)", m->id,
			Def_Word(m, l->tok[1], l->quoted[1], a, sizeof(a)), Def_Word(m, l->tok[2], l->quoted[2], b, sizeof(b)));
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
		if (!l.quoted[0] && !strcmp(l.tok[0], "section") && l.n == 2) {
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
		}
	}
	if (section && !found) {
		Con_Warnf("%s: no section %s\n", m->name, section);
	}
}

byte Script_RunFile(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	PyObject *vars) {
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
	scriptDef_t m = { .id = id, .steps = steps, .vars = vars, .text = text, .section = section, .name = name };
	scriptDef_t *outer = def_running;
	def_running = &m;
	level++;
	Def_Run(&m, 1, section);
	level--;
	def_running = outer;
	if (m.depth) {
		Con_Warnf("%s: if without end\n", name);
	}
	Py_XDECREF(m.ownVars);
	return 1;
}
