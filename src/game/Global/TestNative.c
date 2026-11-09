/*
 * The command "native" of the test channel (src/game/Global/TestChannel.c): values of the native layer of the script
 * ports (src/game/Script/ScriptNative.c) read and printed without Python objects, for the check of the native repr
 * and literal reader against the stored texts (tools/test/litfuzz.sh). Test only, off with the channel.
 *   native <literal>         the repr of what the native literal reader makes of the text
 *   native str <literal>     its str
 *   native check <literal>   its text as check() of the golden test writes it (floats with 17 digits)
 *   native esc ...           the same three with the literal escaped, \n a line end and \\ a backslash (the channel
 *                            reads lines; litfuzz.sh sends the stored texts that have a line end this way)
 *   native ports             (OBJLIB 7) the counts of the direct calls between ports (PORT_CALL, PORT_OWN) per
 *                            call site that ran: "<file>:<line> <Module>.<Func> attr|own fast <n> slow <n> errors
 *                            <n>", and the calls of values (Port_Value, stage 4) as "<file>:<line> <Module>.<Func>
 *                            value ..." with the port function the site called last through its entry ("value.?"
 *                            when none), sorted by file and line ("no sites" when none ran); then the callbacks by name
 *                            bound to a port's entry (cPy_Function PYF_ENTRY, stage 4) since the start: "callback
 *                            <Module>.<Func> calls <n> errors <n>", sorted by name, and the calls of cPy_Function by
 *                            kind: "callbacks python <n> native <n> entry <n>"; then the functions given to the
 *                            scheduler as a port's entry (cScheduledFunc, stage 4) since the start: "scheduled
 *                            <Module>.<Func> calls <n> errors <n>", sorted by name, and the calls of the scheduler
 *                            by kind: "scheduled python <n> entry <n>"; then the modules by name (batch 6b,
 *                            Script_ModuleCounts): "modules pack <n> none <n> absent <n> other <n> added <n> made <n>
 *                            loaded <n>" (pack modules found in sys.modules, whose entry is None, not loaded; other
 *                            names; pack modules made empty by Script_ModuleAdd, made from their .mod file, found
 *                            loaded by PreloadLibrary); then the call lines of the .def files (Script_DefCounts) by
 *                            path: "def calls registry <n> byname <n> other <n> errors <n>" (the
 *                            registry ones also on the line of their site in Script.c)
 *   native ports reset       the counts back to 0
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/pyfunction.h"
#include "scrap/scheduler.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

static void TN_Line(nativeBuf_t *out, const char *text, nativeRepr_t mode) {
	nativeVal_t v;
	if (!Native_Literal(text, &v)) {
		Native_BufAdd(out, "error: native: not a literal of the native reader\n", -1);
		return;
	}
	Native_Repr(out, &v, mode);
	Native_BufAdd(out, "\n", 1);
	Native_Free(&v);
}

/* the sites by file, then by line number */
static int TN_SiteOrder(const void *x, const void *y) {
	const portSite_t *a = *(const portSite_t *const *)x, *b = *(const portSite_t *const *)y;
	int c = strcmp(a->file, b->file);
	return c ? c : a->line - b->line;
}

/* the callbacks by module, then by name */
static int TN_CallbackOrder(const void *x, const void *y) {
	const portSite_t *a = *(const portSite_t *const *)x, *b = *(const portSite_t *const *)y;
	int c = strcmp(a->ref->module, b->ref->module);
	return c ? c : strcmp(a->ref->name, b->ref->name);
}

/* the counts of the port functions bound for a later call (the sites of list), "<label> <Module>.<Func> calls <n>
 * errors <n>" sorted by name */
static void TN_Bound(nativeBuf_t *out, portSite_t *list, const char *label, byte reset) {
	static const portSite_t *cbs[PORT_COUNT];
	int n = 0;
	for (portSite_t *s = list; s; s = s->next) {
		if (reset) {
			s->fast = s->slow = s->errors = 0;
		} else if (n < PORT_COUNT) {
			cbs[n++] = s;
		}
	}
	if (reset) {
		return;
	}
	qsort(cbs, n, sizeof(cbs[0]), TN_CallbackOrder);
	for (int i = 0; i < n; i++) {
		char line[256];
		snprintf(line, sizeof(line), "%s %s.%s calls %lu errors %lu\n", label, cbs[i]->ref->module, cbs[i]->ref->name,
			cbs[i]->fast, cbs[i]->errors);
		Native_BufAdd(out, line, -1);
	}
}

/* the counts of the callbacks and scheduled functions bound to port entries, of cPy_Function's calls and of the
 * scheduler's calls by kind */
static void TN_Callbacks(nativeBuf_t *out, byte reset) {
	unsigned long python, native, entry;
	char line[128];
	TN_Bound(out, Port_Callbacks(), "callback", reset);
	cPy_Function_Counts(&python, &native, &entry, reset);
	if (!reset) {
		snprintf(line, sizeof(line), "callbacks python %lu native %lu entry %lu\n", python, native, entry);
		Native_BufAdd(out, line, -1);
	}
	TN_Bound(out, Port_ScheduledSites(), "scheduled", reset);
	Scheduler_Counts(&python, &entry, reset);
	if (!reset) {
		snprintf(line, sizeof(line), "scheduled python %lu entry %lu\n", python, entry);
		Native_BufAdd(out, line, -1);
	}
	unsigned long mods[SMC_COUNT];
	Script_ModuleCounts(mods, reset);
	if (!reset) {
		snprintf(line, sizeof(line), "modules pack %lu none %lu absent %lu other %lu added %lu made %lu loaded %lu\n",
			mods[SMC_PACK], mods[SMC_NONE], mods[SMC_ABSENT], mods[SMC_OTHER], mods[SMC_ADDED], mods[SMC_MADE],
			mods[SMC_LOADED]);
		Native_BufAdd(out, line, -1);
	}
	unsigned long other, errors;
	Script_DefCounts(&entry, &python, &other, &errors, reset);
	if (!reset) {
		snprintf(line, sizeof(line), "def calls registry %lu byname %lu other %lu errors %lu\n", entry, python, other,
			errors);
		Native_BufAdd(out, line, -1);
	}
}

/* native ports [reset] */
static void TN_Ports(nativeBuf_t *out, byte reset) {
	static const portSite_t *sites[4096];
	int n = 0;
	for (portSite_t *s = Port_Sites(); s; s = s->next) {
		if (reset) {
			s->fast = s->slow = s->errors = 0;
		} else if (n < (int)(sizeof(sites) / sizeof(sites[0]))) {
			sites[n++] = s;
		}
	}
	if (reset) {
		TN_Callbacks(out, 1);
		Native_BufAdd(out, "ok\n", -1);
		return;
	}
	qsort(sites, n, sizeof(sites[0]), TN_SiteOrder);
	for (int i = 0; i < n; i++) {
		const char *f = strstr(sites[i]->file, "src/game/Script/");
		char line[512];
		snprintf(line, sizeof(line), "%s:%d %s.%s %s fast %lu slow %lu errors %lu\n", f ? f + 16 : sites[i]->file,
			sites[i]->line, sites[i]->ref->module, sites[i]->ref->name, sites[i]->global == 2 ? "value" : sites[i]->global ? "own" : "attr",
			sites[i]->fast, sites[i]->slow, sites[i]->errors);
		Native_BufAdd(out, line, -1);
	}
	if (!n) {
		Native_BufAdd(out, "no sites\n", -1);
	}
	TN_Callbacks(out, 0);
}

void TestNative_Command(nativeBuf_t *out, const char *arg) {
	byte esc = !strncmp(arg, "esc ", 4);
	nativeRepr_t mode = NR_REPR;

	if (esc) {
		arg += 4;
	} else if (!strcmp(arg, "ports") || !strcmp(arg, "ports reset")) {
		TN_Ports(out, arg[5] != 0);
		return;
	}
	if (!strncmp(arg, "str ", 4)) {
		mode = NR_STR;
		arg += 4;
	} else if (!strncmp(arg, "check ", 6)) {
		mode = NR_CHECK;
		arg += 6;
	}
	if (!esc) {
		TN_Line(out, arg, mode);
		return;
	}
	nativeBuf_t text;
	Native_BufInit(&text);
	for (; *arg; arg++) {
		if (*arg == '\\' && (arg[1] == 'n' || arg[1] == '\\')) {
			Native_BufAdd(&text, arg[1] == 'n' ? "\n" : "\\", 1);
			arg++;
		} else {
			Native_BufAdd(&text, arg, 1);
		}
	}
	TN_Line(out, text.p, mode);
	Native_BufFree(&text);
}
