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
 *                            and last the native objects (scrap/nativeobj.h, batch 9a, NObj_Counts): "objects made
 *                            <n> freed <n> live <n> views <n> crossed <n>" (made and freed since the start, never
 *                            reset; views: the entries of the identity table now)
 *   native ports reset       the counts back to 0
 *   native objtest           the self-test of the native objects: the checks of ot_script on objects made natively,
 *                            one line "<name>: <repr of the tuple of results>" per check (an error as (type, value),
 *                            as tc_catch gives it), then the objects it made and freed. The same checks run as a
 *                            channel case on Python objects (tools/test/objcase.py writes the case from ot_script);
 *                            the two outputs are equal line by line apart from addresses
 *   native objtest cross     the same with every object sent to Python once when it is stored (the crossing of
 *                            scrap/nativeobj.h: from then on the native object is the view of its Python object)
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/nativeobj.h"
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
		unsigned long made, freed, views, crossed;
		NObj_Counts(&made, &freed, &views, &crossed);
		snprintf(line, sizeof(line), "objects made %lu freed %lu live %lu views %lu crossed %lu\n", made, freed,
			made - freed, views, crossed);
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

/* ---- native objtest: the self-test of the native objects (scrap/nativeobj.h, batch 9a) ---- */

/*
 * The checks, one text per step. "-- name" starts a check: its steps give the items of one result tuple, printed as
 * "name: <repr>" (a name with '!' is a check without a Python twin: the functions of Python code the game no longer
 * has; it is left out of the channel case and of the crossing run). A step is "<op> <arguments>" with the arguments
 * separated by one blank: $x is the variable x of the test, - an omitted bound, anything else a literal of the native
 * reader (written without blanks). "x= <step>" stores the result in x in place of adding an item (at the start of a
 * check only: the channel case makes it a statement before the tuple), as setslice and delslice.
 *   lit v              v                                   tuple a b ...      (a, b, ...)
 *   class M.N attrs    tc_class('M.N', attrs)              twin N             the class N of the game data
 *   port M.F           the port function M.F               cb name            the test function name (ot_cbs)
 *   fn name            the same as a function of Python code                  mod M              the module M
 *   method f self C    the method object of f for C (self None: unbound)
 *   val x              x                                   rt x               x after a trip to Python and back
 *   repr x, str x, type x (type(x).__name__), len x, not x, hash x (hash(x) == hash(x)), hasheq a b
 *   get x k (x[k]), slice x lo hi, put x k v, del x k, setslice x lo hi v, delslice x lo hi, clear x (del x[:])
 *   attr x name, setattr x name v, delattr x name, call f args..., meth x name args...
 *   in v x, cmp a b (its sign), eq a b, is a b
 * tools/test/objcase.py reads this table and writes the same checks as lines of a channel case.
 */
static const char *const ot_script[] = {
	"-- list-read",
	"L= lit [3,1,2]",
	"repr $L", "str $L", "type $L", "len $L", "not $L", "get $L 0", "get $L -1", "get $L 3", "get $L -4", "get $L 'a'",
	"get $L 1.0", "slice $L 1 -", "slice $L - 2", "slice $L -2 -", "slice $L 5 9", "slice $L 2 1", "in 1 $L", "in 9 $L",
	"in 'a' $L", "hash $L", "call $L", "call $L 1", "not []", "val $L",
	"-- list-append",
	"meth $L append 5", "meth $L append", "meth $L append 1 2", "meth $L append None", "meth $L append [7]", "val $L",
	"len $L",
	"-- list-insert",
	"meth $L insert 0 'a'", "meth $L insert 100 'z'", "meth $L insert -5 'b'", "meth $L insert -100 'c'",
	"meth $L insert 1", "meth $L insert", "meth $L insert 'x' 1", "meth $L insert 1 2 3", "meth $L insert (2,'t')",
	"meth $L insert 1.9 'f'", "meth $L insert None 1", "meth $L insert [1]", "val $L",
	"-- list-extend",
	"E= lit [1,2]",
	"meth $E extend [7,8]", "meth $E extend $E", "meth $E extend (1,2)", "meth $E extend", "meth $E extend [1] [2]",
	"meth $E extend []", "meth $E extend 'ab'", "meth $E extend {}", "val $E",
	"-- list-pop",
	"meth $E pop", "meth $E pop 0", "meth $E pop -1", "meth $E pop 100", "meth $E pop -100", "meth $E pop 'a'",
	"meth $E pop 1 2", "meth $E pop 1.5", "val $E", "meth [] pop", "meth [] pop 0",
	"-- list-find",
	"F= lit [1,'a',2.0,(1,2),None,1,[5],'a']",
	"meth $F index 1", "meth $F index 'a'", "meth $F index 2", "meth $F index (1,2)", "meth $F index 1 2",
	"meth $F index None", "meth $F index [5]", "meth $F index 99", "meth $F index", "meth $F count 1",
	"meth $F count 'a'", "meth $F count 1.0", "meth $F count 7", "meth $F count", "meth $F count 1 2",
	"meth $F remove 'a'", "meth $F remove 99", "meth $F remove", "meth $F remove 1 2", "meth $F remove [5]", "val $F",
	"-- list-order",
	"meth $F reverse", "val $F", "meth $F reverse 1", "meth $F sort", "val $F",
	"-- list-sort",
	"S= lit [3,'b',1.5,None,(1,2),[0],'a',2,{},(1,),[],{'k':1},-1,2L,'']",
	"rev= cb cmprev",
	"bad= cb bad",
	"meth $S sort", "val $S", "meth $S sort $rev", "val $S", "meth $S sort $bad", "meth $S sort 1 2", "meth $S sort 5",
	"meth $S sort None", "val $S",
	"-- list-sort-equal",
	"Q= lit [1.0,1,2,1L,0,2.0,1]",
	"meth $Q sort", "val $Q", "meth $Q reverse", "meth $Q sort", "val $Q", "meth [] sort", "meth [5] sort",
	"-- list-store",
	"P= lit [0,1,2,3,4]",
	"put $P 0 'x'", "put $P -1 'y'", "put $P 5 1", "put $P -6 1", "put $P 'a' 1", "put $P 1.0 1", "val $P", "del $P 1",
	"del $P -1", "del $P 9", "del $P 'a'", "val $P",
	"-- list-slices",
	"setslice $P 1 2 [7,8,9]",
	"R= lit [0,1,2,3,4,5]",
	"setslice $R 1 3 []",
	"setslice $R -1 - ['e']",
	"setslice $R 0 0 $R",
	"delslice $R 0 1",
	"delslice $R 5 2",
	"val $P", "val $R", "slice $R -100 100", "slice $R 3 -1",
	"-- list-attr",
	"m= attr $L append",
	"m2= attr $L append",
	"k= attr $L sort",
	"attr $L __methods__", "attr $L nope", "attr $L __doc__", "attr $L __dict__", "setattr $L x 1", "delattr $L x",
	"delattr $L append", "repr $m", "str $m", "type $m", "attr $m __name__", "attr $m __doc__", "attr $m __members__",
	"attr $m nope", "setattr $m x 1", "delattr $m x", "eq $m $m2", "is $m $m2", "eq $m $k", "hash $m", "not $m",
	"len $m", "call $m 'via'", "get $L -1", "call $k 1 2", "get $m 0",
	"-- list-self",
	"ms= attr $m __self__",
	"is $ms $L", "eq $ms $L",
	"-- list-share",
	"M= lit [1]",
	"N= lit [[1],{'a':[2]},([3],4)]",
	"meth $L append $M", "meth $M append 2", "get $L -1", "is $M $M", "eq $M [1,2]", "eq [1,2] $M", "eq $M [1,3]",
	"eq $M (1,2)", "cmp $M [1,2]", "cmp $M [1,3]", "cmp $M [1]", "cmp [1,2,3] $M", "cmp $M (1,2)", "cmp $M 5",
	"cmp $M 'a'", "cmp $M None", "cmp $M {}", "cmp 5 $M", "eq $M $L", "in $M $L", "in [1,2] $L", "meth $L index $M",
	"meth $L count [1,2]",
	"-- list-nested",
	"x= get $N 0",
	"y= get $N 1",
	"z= get $y 'a'",
	"w= get $N 2",
	"v= get $w 0",
	"meth $x append 9", "meth $z append 8", "meth $v append 7", "val $N", "is $x $x", "eq $x [1,9]",
	"-- list-trip",
	"T= rt $L",
	"is $T $L", "meth $T append 'trip'", "get $L -1", "len $L", "len $T", "repr $L", "meth $L pop", "val $T",
	"type $L", "hash $L", "in 'a' $T", "slice $T 0 2", "meth $L insert 0 'first'", "get $T 0", "meth $T sort $rev",
	"val $L", "eq $L $T", "cmp $L []", "eq $L [1]", "attr $L __methods__", "meth $L extend [1]", "put $L 0 0",
	"del $L 1", "val $T", "meth $L append",
	"-- dict-read",
	"D= lit {'a':1,'b':2,3:'c',(1,2):[4],None:0,1.5:'f'}",
	"repr $D", "str $D", "type $D", "len $D", "not $D", "not {}", "get $D 'a'", "get $D 3", "get $D (1,2)",
	"get $D None", "get $D 1.5", "get $D 'zz'", "get $D 99", "get $D [1]", "get $D {}", "get $D (1,[2])",
	"in 'a' $D", "slice $D 0 1", "hash $D", "call $D", "val $D",
	"-- dict-methods",
	"meth $D keys", "meth $D values", "meth $D items", "meth $D keys 1", "meth $D values 1", "meth $D items 1 2",
	"meth $D has_key 'a'", "meth $D has_key 'q'", "meth $D has_key", "meth $D has_key [1]", "meth $D has_key 1 2",
	"meth $D get 'a'", "meth $D get 'q'", "meth $D get 'q' 5", "meth $D get", "meth $D get 1 2 3", "meth $D get [1]",
	"meth $D get [1] 2",
	"-- dict-update",
	"U= lit {'a':1}",
	"meth $U update {'z':9,'a':7}", "meth $U update $U", "meth $U update [1]", "meth $U update", "meth $U update {} {}",
	"meth $U update 5", "val $U", "meth $U update $D", "val $U", "len $U",
	"-- dict-copy",
	"C= meth $U copy",
	"is $C $U", "eq $C $U", "type $C", "meth $U copy 1", "meth $C clear", "val $C", "len $U", "meth $U clear 1",
	"eq $C {}", "eq $C $U", "cmp $C $U", "cmp $U $C", "cmp $C {}", "cmp $U $U",
	"-- dict-store",
	"put $C 'k' 1", "put $C (1,2) 'tuple'", "put $C [1] 2", "put $C {} 2", "put $C 'k' [1]", "val $C", "del $C 'k'",
	"del $C 'k'", "del $C [1]", "del $C 99", "val $C", "clear $C", "clear 5",
	"-- dict-attr",
	"dm= attr $D keys",
	"dm2= attr $D keys",
	"attr $D __methods__", "attr $D nope", "setattr $D x 1", "delattr $D x", "repr $dm", "type $dm", "attr $dm __name__",
	"eq $dm $dm2", "is $dm $dm2", "hash $dm", "call $dm", "call $dm 1",
	"-- dict-order",
	"O= lit {0:0,1:1,2:2,3:3,4:4,5:5,6:6,7:7,8:8,9:9,10:10,'a':1,'b':2,'c':3,'d':4,'e':5,'f':6,'g':7,'Init':8,'x':9}",
	"val $O", "meth $O keys", "del $O 3", "del $O 'c'", "put $O 'new' 1", "put $O 3 'back'", "meth $O keys",
	"meth $O values", "len $O",
	"-- dict-grow",
	"G= lit {}",
	"put $G 'a' 1", "put $G 'b' 2", "put $G 'c' 3", "put $G 'd' 4", "put $G 'e' 5", "put $G 'f' 6", "meth $G keys",
	"del $G 'a'", "del $G 'b'", "put $G 'g' 7", "put $G 'h' 8", "put $G 'i' 9", "put $G 'a' 10", "meth $G items",
	"-- dict-share",
	"V= lit {'l':[1],'d':{'x':[2]}}",
	"a= get $V 'l'",
	"b= get $V 'd'",
	"c= get $b 'x'",
	"meth $a append 5", "meth $c append 6", "val $V", "meth $V values", "meth $V items",
	"-- dict-trip",
	"W= rt $O",
	"is $W $O", "put $W 'trip' 1", "get $O 'trip'", "meth $O keys", "len $W", "del $O 'trip'", "meth $W has_key 'trip'",
	"meth $W get 'x'", "repr $O", "eq $O $W", "type $O", "get $O 'zz'", "get $O [1]", "meth $O update {'u':1}",
	"meth $W keys", "meth $O copy", "meth $O clear", "val $W", "meth $O keys 1", "hash $O", "in 1 $O",
	"-- class-read",
	"K= class OL9a.Thing {'x':1,'y':[1,2],'n':None}",
	"B= class OL9a.Base {'bx':7,'x':'base'}",
	"type $K", "attr $K __name__", "attr $K __bases__", "attr $K x", "attr $K y", "attr $K n", "attr $K nope",
	"attr $K __module__", "attr $K __doc__", "len $K", "get $K 0", "put $K 0 1", "del $K 0", "slice $K 0 1", "in 1 $K",
	"not $K", "hash $K", "eq $K $K", "eq $K $B", "is $K $K", "cmp $K $K", "eq $K 5", "eq $K None",
	"-- class-repr",
	"repr $K", "str $K", "repr $B",
	"-- class-dict",
	"kd= attr $K __dict__",
	"type $kd", "meth $kd keys", "len $kd", "put $kd 'viadict' 3", "attr $K viadict", "delattr $K viadict",
	"meth $kd has_key 'viadict'",
	"-- class-store",
	"T1= tuple $B",
	"T2= tuple $K",
	"T3= tuple 1",
	"setattr $K z 5", "attr $K z", "delattr $K z", "delattr $K z", "attr $K z", "setattr $K __name__ 5",
	"setattr $K __name__ 'Renamed'", "attr $K __name__", "setattr $K __bases__ [1]", "setattr $K __bases__ $T3",
	"setattr $K __bases__ $T1", "attr $K bx", "attr $K x", "setattr $B __bases__ $T2", "setattr $K __bases__ $T2",
	"setattr $K __dict__ 5", "delattr $K __name__", "delattr $K __bases__", "delattr $K __dict__",
	"setattr $K __name__ 'a\\000b'",
	"-- class-bases",
	"bs= attr $K __bases__",
	"b0= get $bs 0",
	"is $b0 $B", "len $bs", "type $bs",
	"-- inst-read",
	"i= call $K",
	"j= call $K",
	"type $i", "not $i", "len $i", "get $i 0", "put $i 0 1", "del $i 0", "slice $i 0 1", "in 1 $i", "call $i",
	"call $i 1", "hash $i", "eq $i $i", "eq $i $j", "is $i $i", "is $i $j", "cmp $i $i", "eq $i 5", "eq $i None",
	"call $K 1", "call $K 1 2", "attr $i x", "attr $i bx", "attr $i n", "attr $i nope", "attr $i __doc__",
	"hasheq $i $i", "hasheq $i $j",
	"-- inst-repr",
	"repr $i", "str $i",
	"-- inst-store",
	"iy= attr $i y",
	"ky= attr $K y",
	"ic= attr $i __class__",
	"id= attr $i __dict__",
	"is $iy $ky", "is $ic $K", "type $id", "val $id", "setattr $i x 5", "attr $i x", "attr $j x", "attr $K x",
	"val $id", "delattr $i x", "attr $i x", "delattr $i x", "delattr $i nope", "put $id 'viadict' [1]",
	"attr $i viadict", "setattr $i lst [1]", "meth $id keys",
	"-- inst-special",
	"pair= cb pair",
	"sfn= cb str",
	"nfn= cb none",
	"ifn= cb one",
	"tfn= cb strt",
	"setattr $i __call__ $pair", "call $i", "call $i 1 'a'", "setattr $i __repr__ $sfn", "repr $i", "str $i",
	"setattr $i __str__ $tfn", "str $i", "repr $i", "delattr $i __repr__", "delattr $i __str__",
	"delattr $i __call__", "call $i", "setattr $i __call__ 5", "call $i", "delattr $i __call__",
	"-- inst-class",
	"setattr $i __class__ 5", "setattr $i __class__ $i", "setattr $i __class__ $B", "attr $i x", "attr $i bx",
	"setattr $i __dict__ 5", "setattr $i __dict__ [1]", "setattr $i __dict__ {'q':1}", "attr $i q", "attr $i lst",
	"delattr $i __class__", "delattr $i __dict__",
	"-- inst-class-repr",
	"repr $i",
	"-- inst-trip",
	"tj= rt $j",
	"tk= rt $K",
	"is $tj $j", "is $tk $K", "setattr $tj viatrip 1", "attr $j viatrip", "attr $j x", "setattr $K afterTrip 2",
	"attr $tj afterTrip", "attr $j nope", "delattr $j nope", "type $j", "type $K", "len $j", "call $j", "not $j",
	"hash $j", "eq $j $tj", "attr $K __name__", "attr $K nope", "delattr $K nope", "call $K 1",
	"-- inst-trip-repr",
	"n2= call $K",
	"repr $j", "repr $K", "repr $n2", "type $n2", "attr $n2 afterTrip",
	"-- twin-spawn",
	"SP= twin CharSpawnPos",
	"sd= attr $SP __dict__",
	"s= call $SP",
	"s2= call $SP",
	"sm= attr $s __init__",
	"um= attr $SP __init__",
	"sc= attr $s CallbackFunc",
	"sc2= attr $s2 CallbackFunc",
	"attr $SP __name__", "attr $SP __bases__", "meth $sd keys", "attr $SP Pos", "attr $SP Name", "attr $SP __module__",
	"attr $s CallbackFunc", "is $sc $sc2", "attr $s Name", "attr $s Pos", "attr $s Entity", "attr $s repos",
	"attr $s __dict__", "type $sm", "attr $sm __name__", "type $um", "attr $um __name__", "call $um", "call $um 5",
	"call $um $s", "call $sm", "call $sm 1", "call $SP 1", "call $SP 1 2", "eq $sm $sm", "not $sm", "len $sm",
	"attr $sm nope", "setattr $sm x 1", "attr $um im_self", "attr $um nope",
	"-- twin-spawn-repr",
	"smc= attr $sm im_class",
	"sms= attr $sm im_self",
	"smf= attr $sm im_func",
	"repr $SP", "repr $s", "repr $sm", "repr $um", "str $sm", "is $smc $SP", "is $sms $s", "type $smf",
	"attr $smf __name__", "hash $sm", "hash $um",
	"-- twin-ship",
	"SA= twin cShipAI",
	"ad= attr $SA __dict__",
	"sa= call $SA",
	"meth $ad keys", "meth $ad values", "attr $sa Ship", "setattr $sa Ship 'X'", "attr $sa Ship", "attr $SA Ship",
	"attr $sa __dict__", "call $SA 1", "attr $SA __name__", "attr $SA __module__", "attr $sa AngAttack",
	"-- twin-ship-repr",
	"repr $SA", "repr $sa",
	"-- twin-gate",
	"GK= twin GateKeeperType",
	"gd= attr $GK __dict__",
	"gk= call $GK",
	"meth $gd keys", "attr $gk FoeType", "attr $gk Door", "attr $gk CanPassFunc", "setattr $gk MPass 1",
	"attr $gk __dict__", "attr $GK MPass",
	"-- twin-gate-repr",
	"repr $GK", "repr $gk",
	"-- port-func",
	"f= port Fx.GetMotorClass",
	"f2= port Fx.GetMotorClass",
	"h= port Fx.MoneyTransfer",
	"type $f", "repr $f", "str $f", "attr $f __name__", "attr $f __doc__", "attr $f __self__", "attr $f __members__",
	"attr $f nope", "setattr $f x 1", "delattr $f x", "not $f", "len $f", "get $f 0", "hash $f", "hasheq $f $f2",
	"eq $f $f2", "eq $f $h", "eq $f 5", "call $f 'ol9a_none'", "call $f", "call $f 1 2", "call $f 5", "call $f [1]",
	"call $f 1 2 3 4 5 6 7 8 9 10 11 12", "in 1 $f",
	"-- port-in-class",
	"FK= class OL9a.Holder {}",
	"setattr $FK fn $f",
	"-- port-in-instance",
	"fi= call $FK",
	"ff= attr $fi fn",
	"type $ff", "eq $ff $f", "call $ff 'ol9a_none'", "call $ff",
	"-- callbacks",
	"one= cb one",
	"sec= cb second",
	"call $pair", "call $pair 1", "call $pair 1 'a' [1] {'k':2} None (1,2)", "call $nfn", "call $nfn 1 2", "call $one",
	"call $sec 1 2", "call $sec 1", "not $pair", "len $pair", "eq $pair $pair", "eq $pair $one", "call $pair $L",
	"call $pair $i",
	"-- callback-identity",
	"r= call $pair $M $D",
	"r1= get $r 1",
	"r10= get $r1 0",
	"r11= get $r1 1",
	"is $r10 $M", "is $r11 $D",
	"-- module",
	"I= mod Init",
	"I2= mod Init",
	"type $I", "is $I $I2", "eq $I $I2", "attr $I __name__", "attr $I ol9a_nope", "setattr $I ol9a_x [1]",
	"attr $I ol9a_x", "delattr $I ol9a_x", "delattr $I ol9a_x", "setattr $I __dict__ 5", "not $I", "len $I",
	"call $I", "get $I 0", "hash $I", "in 1 $I", "mod OL9aNoModule",
	"-- module-share",
	"setattr $I ol9a_l $M",
	"-- module-shared",
	"il= attr $I ol9a_l",
	"is $il $M", "meth $il append 'mod'", "get $M -1", "delattr $I ol9a_l",
	"-- values",
	"len (1,2)", "len 'abc'", "len 5", "len None", "get 'abc' 1", "get (1,2) 5", "get 5 0", "get {'a':1} 'a'",
	"attr 5 x", "attr 'abc' x", "attr None x", "attr (1,) x", "attr 1.5 x", "attr 2L x", "setattr 5 x 1",
	"setattr 'a' x 1", "delattr None x", "setattr [1] x 1", "setattr {} x 1", "call 5", "call 'a' 1", "call None",
	"call (1,)", "call [1]", "call {}", "meth [1,2] index 2", "meth {'a':1} keys", "meth [1] nope", "meth 5 nope",
	"put (1,2) 0 5", "put 'abc' 0 'x'", "put 5 0 1", "del 'abc' 0", "del (1,2) 0", "del None 0", "slice 'abcdef' 1 3",
	"slice (1,2,3) 1 -", "slice 5 0 1", "slice {} 0 1", "in 'a' 'abc'", "in 1 (1,2)", "in 1 5", "in 1 {}", "hash [1]",
	"hash {}", "hash (1,[2])", "hash (1,2)", "hash 'a'", "cmp [1,2] [1,3]", "cmp (1,) [1]", "not 0", "not 'a'",
	"-- !python-function",
	"pf= fn pair",
	"PK= class OL9a.WithMethod {}",
	"setattr $PK meth $pf",
	"pi= call $PK",
	"bm= attr $pi meth",
	"ub= attr $PK meth",
	"repr $pf", "type $pf", "attr $pf __name__", "attr $pf func_name", "attr $pf nope", "hash $pf", "type $bm",
	"repr $bm", "repr $ub", "call $bm 1 2", "call $ub", "call $ub 1", "call $ub $pi 3", "attr $bm im_self",
	"attr $bm im_func", "attr $bm im_class", "attr $bm __name__", "eq $bm $bm", "hash $bm", "call $pf 1",
	"-- !method-object",
	"mo= method $pair $s $SP",
	"mu= method $pair None $SP",
	"repr $mo", "repr $mu", "call $mo 1", "call $mu 1", "call $mu $s 1", "call $mu $i", "method $L $s $SP",
	"-- !special-result",
	"setattr $pi __repr__ $nfn", "repr $pi", "setattr $pi __str__ $one", "str $pi",
	"-- !callback-repr",
	"df= datafn Fx GetMotorClass",
	"repr $pair", "type $pair", "attr $pair __name__", "attr $pair __self__", "hash $pair", "repr $df",
	"call $df 'ol9a_none'",
	"call $df", "repr $I",
	NULL,
};

/* the variables of the test */
#define OT_VARS 128
#define OT_TOKENS 16

typedef struct {
	char name[24];
	nativeVal_t v;
} otVar_t;

static otVar_t ot_vars[OT_VARS];
static int ot_nvars;
static byte ot_cross;

static nativeVal_t *OT_Var(const char *name, byte make) {
	for (int i = 0; i < ot_nvars; i++) {
		if (!strcmp(ot_vars[i].name, name)) {
			return &ot_vars[i].v;
		}
	}
	if (!make || ot_nvars == OT_VARS) {
		return NULL;
	}
	otVar_t *v = &ot_vars[ot_nvars++];
	Str_Copy(v->name, name, sizeof(v->name));
	Native_SetNone(&v->v);
	return &v->v;
}

/* an argument of a step: $x the variable, else a literal; 0 with a SystemError for a text that is neither */
static byte OT_Atom(const char *tok, nativeVal_t *out) {
	Native_SetNone(out);
	if (*tok == '$') {
		const nativeVal_t *v = OT_Var(tok + 1, 0);
		if (!v) {
			return Native_Raise(NE_SYSTEM, "objtest: no variable %s", tok);
		}
		Native_Copy(out, v);
		return 1;
	}
	/* "a\000b": the escape of the table text is read by the literal reader */
	return Native_Literal(tok, out) || Native_Raise(NE_SYSTEM, "objtest: not a literal: %s", tok);
}

/* the test functions (cb name, fn name): the lambdas of the channel case */
static byte OT_CbPair(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: (len(a), a) */
	(void)ctx;
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), n);
	nativeVal_t *args = Native_Append(out);
	Native_SetEmpty(args, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(args), &a[i]);
	}
	return 1;
}

static byte OT_CbNone(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: None */
	(void)ctx;
	(void)a;
	(void)n;
	(void)out;
	return 1;
}

static byte OT_CbOne(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: 1 */
	(void)ctx;
	(void)a;
	(void)n;
	Native_SetInt(out, 1);
	return 1;
}

static byte OT_CbStr(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: 'S' */
	(void)ctx;
	(void)a;
	(void)n;
	Native_SetStr(out, "S");
	return 1;
}

static byte OT_CbStrT(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: 'T' */
	(void)ctx;
	(void)a;
	(void)n;
	Native_SetStr(out, "T");
	return 1;
}

static byte OT_CbSecond(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda *a: a[1] */
	(void)ctx;
	if (n < 2) {
		return Native_Raise(NE_INDEX, "tuple index out of range");
	}
	Native_Copy(out, &a[1]);
	return 1;
}

static byte OT_CbCmpRev(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda x, y: cmp(y, x) */
	(void)ctx;
	if (n != 2) {
		return Native_Raise(NE_TYPE, "not enough arguments; expected 2, got %d", n);
	}
	Native_SetInt(out, Native_Compare(&a[1], &a[0]));
	return 1;
}

static byte OT_CbBad(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* lambda x, y: 'x' */
	(void)ctx;
	(void)a;
	(void)n;
	Native_SetStr(out, "x");
	return 1;
}

static byte OT_CbInit(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* CharSpawnPos.__init__(self): self.CallbackFunc = [] */
	nativeVal_t list;
	(void)ctx;
	(void)out;
	if (n != 1) {
		return Port_Arity(n, 1);
	}
	Native_SetEmpty(&list, NV_LIST);
	byte ok = NObj_SetAttr(&a[0], "CallbackFunc", &list);
	Native_Free(&list);
	return ok;
}

static const struct {
	const char *name;
	nativeCall_t call;
} ot_cbs[] = {
	{ "pair", OT_CbPair }, { "none", OT_CbNone }, { "one", OT_CbOne }, { "str", OT_CbStr }, { "strt", OT_CbStrT },
	{ "second", OT_CbSecond }, { "cmprev", OT_CbCmpRev }, { "bad", OT_CbBad },
};

/* the classes of the game data as native classes: the lets of their class bodies in the order of the .mod files
 * (data/modules/vehiclesnpc.mod, charsnpc.mod, gatekeeper.mod); the class body of CharSpawnPos gives its __init__
 * after the lets (the hook SpawnPosInit) */
static const struct {
	const char *name, *module, *attrs;
} ot_twins[] = {
	{ "cShipAI", "VehiclesNPC", "{'Ship':'Spoli1','ControlStrafe':2,'ControlBrake':0,'ControlBoost':1,"
		"'ControlBoostRace':1,'ControlBoostItemLife':1,'ControlBoostItemAmmo':1,'StupidPathFinding':0,"
		"'CadenceShoot':15,'FactorDamageGetAway':0.1,'FactorDamageItemLife':0.25,'FactorItemAmmo':0.25,"
		"'TimeMaxStrafe':2,'VelMaxStrafe':0.25,'AngAttack':0.785,'ControlAim':2,'CounterAttack':1.0,'LifeMult':1.0,"
		"'GoNextItemRace':1,'ControlHookRace':0,'FactorHookMap':0,'ControlStrafeEvasion':0,'CadenceShootCM':3,"
		"'UsePercSonic':50,'UsePercCloud':35,'UsePercElectro':15,'UsePercHook':0,'UsePercHookGetAway':0}" },
	{ "CharSpawnPos", "CharsNPC", "{'Pos':(0,0,0),'Ang':(0,0,0),'Name':'Character NPC','ActCtrl':'Police',"
		"'Entity':None,'ExtraInfo':None,'afterInitSpeechFunc':None,'repos':0}" },
	{ "GateKeeperType", "GateKeeper", "{'FoeType':(),'MPass':0,'Door':'DM_Door_OrbitDoorA_00','CanPassFunc':None,"
		"'ClosedFunc':None}" },
};

/* a key of the text name set in a dict by value */
static void OT_DictSet(nativeVal_t *dict, const char *name, const nativeVal_t *v) {
	nativeVal_t key, x;
	Native_SetStr(&key, name);
	Native_Copy(&x, v);
	Native_DictSet(dict, &key, &x);
}

/* the native class of a class of the game data: its dict has the keys in the order the class body stores them (the
 * literal reader sets them in the order of the text), then __doc__ and __module__ */
static byte OT_Twin(const char *name, nativeVal_t *out) {
	for (size_t i = 0; i < sizeof(ot_twins) / sizeof(ot_twins[0]); i++) {
		nativeVal_t dict, fn, m, x;
		if (strcmp(ot_twins[i].name, name)) {
			continue;
		}
		if (!Native_Literal(ot_twins[i].attrs, &dict)) {
			return 0;
		}
		if (strcmp(name, "CharSpawnPos")) {
			return NObj_NewClass(out, name, ot_twins[i].module, NULL, &dict);
		}
		/* the unbound method around the C function, as CharsNPC_ModSpawnPosInit and ...Class make it: the class
		 * first, then its dict with the method in the place the class body gives it */
		if (!NObj_NewClass(out, name, ot_twins[i].module, NULL, NULL)) {
			Native_Free(&dict);
			return 0;
		}
		NObj_NewCallback(&fn, "__init__", OT_CbInit, NULL, NULL, 0);
		byte ok = NObj_NewMethod(&m, &fn, NULL, out);
		if (ok) {
			OT_DictSet(&dict, "__init__", &m);
			Native_SetNone(&x);
			OT_DictSet(&dict, "__doc__", &x);
			Native_SetStr(&x, ot_twins[i].module);
			OT_DictSet(&dict, "__module__", &x);
			ok = NObj_SetAttr(out, "__dict__", &dict);
		}
		Native_Free(&dict);
		Native_Free(&fn);
		Native_Free(&m);
		return ok;
	}
	return Native_Raise(NE_SYSTEM, "objtest: no twin %s", name);
}

/* a bound of a slice: - is the omitted one */
static byte OT_Bound(const char *tok, long omitted, long *out) {
	nativeVal_t v;
	if (!strcmp(tok, "-")) {
		*out = omitted;
		return 1;
	}
	if (!OT_Atom(tok, &v)) {
		return 0;
	}
	*out = v.i;
	Native_Free(&v);
	return 1;
}

/* one step: 1 with its result in *out, 0 with the error set */
static byte OT_Step(char **tok, int n, nativeVal_t *out) {
	nativeVal_t a[OT_TOKENS], r;
	const char *op = tok[0];
	int na = 0, first = 1;
	long lo, hi, h, h2;
	byte ok = 1;
	Native_SetNone(out);
	/* the names of an operation are texts, not values */
	if (!strcmp(op, "class") || !strcmp(op, "twin") || !strcmp(op, "port") || !strcmp(op, "cb") || !strcmp(op, "fn") ||
		!strcmp(op, "mod") || !strcmp(op, "datafn")) {
		first = n;
	}
	for (int i = first; ok && i < n; i++) {
		byte name = (i == 2 && (!strcmp(op, "attr") || !strcmp(op, "setattr") || !strcmp(op, "delattr") ||
			!strcmp(op, "meth"))) || ((i == 2 || i == 3) && strstr(op, "slice"));
		/* a name or a bound: read where it is used */
		if (name) {
			Native_SetNone(&a[na++]);
		} else {
			ok = OT_Atom(tok[i], &a[na++]);
		}
	}
	if (!ok) {
		/* fall through to the cleanup */
	} else if (!strcmp(op, "lit") || !strcmp(op, "val")) {
		Native_Copy(out, &a[0]);
	} else if (!strcmp(op, "tuple")) {
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < na; i++) {
			Native_Copy(Native_Append(out), &a[i]);
		}
	} else if (!strcmp(op, "class")) {
		const char *dot = strrchr(tok[1], '.');
		char mod[64];
		snprintf(mod, sizeof(mod), "%.*s", dot ? (int)(dot - tok[1]) : 0, tok[1]);
		ok = OT_Atom(tok[2], &r) && NObj_NewClass(out, dot ? dot + 1 : tok[1], dot ? mod : "__main__", NULL, &r);
	} else if (!strcmp(op, "twin")) {
		ok = OT_Twin(tok[1], out);
	} else if (!strcmp(op, "port")) {
		const char *dot = strchr(tok[1], '.');
		char mod[64];
		snprintf(mod, sizeof(mod), "%.*s", (int)(dot - tok[1]), tok[1]);
		const portRef_t *ref = Port_Find(mod, dot + 1);
		ok = ref != NULL || Native_Raise(NE_SYSTEM, "objtest: no port function %s", tok[1]);
		if (ok) {
			NObj_NewPortFunc(out, ref);
		}
	} else if (!strcmp(op, "cb") || !strcmp(op, "fn")) {
		size_t i = 0;
		while (i < sizeof(ot_cbs) / sizeof(ot_cbs[0]) && strcmp(ot_cbs[i].name, tok[1])) {
			i++;
		}
		ok = i < sizeof(ot_cbs) / sizeof(ot_cbs[0]) || Native_Raise(NE_SYSTEM, "objtest: no test function %s", tok[1]);
		if (ok) {
			NObj_NewCallback(out, tok[1], ot_cbs[i].call, NULL, NULL, op[0] == 'f');
		}
	} else if (!strcmp(op, "datafn")) {
		NObj_NewDataFunc(out, tok[1], tok[2]);
	} else if (!strcmp(op, "mod")) {
		ok = NObj_Module(out, tok[1]);
	} else if (!strcmp(op, "method")) {
		ok = NObj_NewMethod(out, &a[0], a[1].type == NV_NONE ? NULL : &a[1], &a[2]);
	} else if (!strcmp(op, "rt")) {
		PyObject *py = Native_ToPy(&a[0]);
		ok = py != NULL;
		if (ok) {
			NObj_FromPy(py, out);
			Py_DECREF(py);
		}
	} else if (!strcmp(op, "repr") || !strcmp(op, "str")) {
		nativeBuf_t b;
		Native_BufInit(&b);
		ok = NObj_Repr(&b, &a[0], op[0] == 's' ? NR_STR : NR_REPR);
		if (ok) {
			Native_SetText(out, b.p, b.len);
		}
		Native_BufFree(&b);
	} else if (!strcmp(op, "type")) {
		Native_SetStr(out, Native_TypeName(&a[0]));
	} else if (!strcmp(op, "len")) {
		int len = NObj_Len(&a[0]);
		ok = len >= 0;
		Native_SetInt(out, len);
	} else if (!strcmp(op, "not")) {
		Native_SetInt(out, !Native_True(&a[0]));
	} else if (!strcmp(op, "hash")) {
		ok = NObj_Hash(&a[0], &h) && NObj_Hash(&a[0], &h2);
		Native_SetInt(out, ok && h == h2);
	} else if (!strcmp(op, "hasheq")) {
		ok = NObj_Hash(&a[0], &h) && NObj_Hash(&a[1], &h2);
		Native_SetInt(out, ok && h == h2);
	} else if (!strcmp(op, "get")) {
		ok = NObj_GetItem(&a[0], &a[1], out);
	} else if (!strcmp(op, "put")) {
		ok = NObj_SetItem(&a[0], &a[1], &a[2]);
	} else if (!strcmp(op, "del")) {
		ok = NObj_DelItem(&a[0], &a[1]);
	} else if (!strcmp(op, "slice")) {
		ok = OT_Bound(tok[2], 0, &lo) && OT_Bound(tok[3], NOBJ_END, &hi) && NObj_GetSlice(&a[0], lo, hi, out);
	} else if (!strcmp(op, "setslice") || !strcmp(op, "delslice")) {
		ok = OT_Bound(tok[2], 0, &lo) && OT_Bound(tok[3], NOBJ_END, &hi) &&
			NObj_SetSlice(&a[0], lo, hi, op[0] == 's' ? &a[3] : NULL);
	} else if (!strcmp(op, "clear")) {
		ok = NObj_SetSlice(&a[0], 0, NOBJ_END, NULL);
	} else if (!strcmp(op, "attr")) {
		ok = NObj_GetAttr(&a[0], tok[2], out);
	} else if (!strcmp(op, "setattr")) {
		ok = NObj_SetAttr(&a[0], tok[2], &a[2]);
	} else if (!strcmp(op, "delattr")) {
		ok = NObj_DelAttr(&a[0], tok[2]);
	} else if (!strcmp(op, "call")) {
		ok = NObj_Call(&a[0], &a[1], na - 1, out);
	} else if (!strcmp(op, "meth")) {
		ok = NObj_CallMethod(&a[0], tok[2], &a[2], na - 2, out);
	} else if (!strcmp(op, "in")) {
		int c = NObj_Contains(&a[1], &a[0]);
		ok = c >= 0;
		Native_SetInt(out, c);
	} else if (!strcmp(op, "cmp")) {
		Native_SetInt(out, Native_Compare(&a[0], &a[1]));
	} else if (!strcmp(op, "eq")) {
		Native_SetInt(out, Native_Equal(&a[0], &a[1]));
	} else if (!strcmp(op, "is")) {
		Native_SetInt(out, NObj_Is(&a[0], &a[1]));
	} else {
		ok = Native_Raise(NE_SYSTEM, "objtest: no step %s", op);
	}
	for (int i = 0; i < na; i++) {
		Native_Free(&a[i]);
	}
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

/* the error that is set as tc_catch gives it: (type, value) */
static void OT_Error(nativeVal_t *out) {
	PyObject *t, *v, *tb;
	PyErr_Fetch(&t, &v, &tb);
	PyErr_NormalizeException(&t, &v, &tb);
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < 2; i++) {
		PyObject *x = i ? v : t;
		nativeVal_t *item = Native_Append(out);
		if (x) {
			NObj_FromPy(x, item);
		}
	}
	Py_XDECREF(t);
	Py_XDECREF(v);
	Py_XDECREF(tb);
}

static void OT_Run(nativeBuf_t *out, byte cross) {
	nativeVal_t results, r;
	char name[64] = "", text[512];
	byte skip = 0;
	unsigned long made0, freed0, views0, crossed0, made, freed, views, crossed;
	NObj_Counts(&made0, &freed0, &views0, &crossed0);
	ot_cross = cross;
	Native_SetEmpty(&results, NV_TUPLE);
	for (int line = 0;; line++) {
		const char *s = ot_script[line];
		if (!s || !strncmp(s, "-- ", 3)) {
			if (name[0] && !skip) {
				Native_BufAdd(out, name, -1);
				Native_BufAdd(out, ": ", 2);
				Native_Repr(out, &results, NR_REPR);
				Native_BufAdd(out, "\n", 1);
			}
			Native_Free(&results);
			Native_SetEmpty(&results, NV_TUPLE);
			if (!s) {
				break;
			}
			Str_Copy(name, s + 3, sizeof(name));
			skip = cross && name[0] == '!';
			continue;
		}
		if (skip) {
			continue;
		}
		char *tok[OT_TOKENS] = { NULL }, *target = NULL;
		int n = 0;
		Str_Copy(text, s, sizeof(text));
		for (char *p = text; *p && n < OT_TOKENS;) {
			tok[n++] = p;
			p += strcspn(p, " ");
			if (*p) {
				*p++ = '\0';
			}
		}
		if (n && tok[0][strlen(tok[0]) - 1] == '=') {
			tok[0][strlen(tok[0]) - 1] = '\0';
			target = tok[0];
			n--;
		}
		char **step = target ? tok + 1 : tok;
		byte ok = OT_Step(step, n, &r);
		byte store = !strcmp(step[0], "setslice") || !strcmp(step[0], "delslice");
		if (!ok) {
			OT_Error(Native_Append(&results));
		} else if (target) {
			/* a variable is a second holder: a list or dict by value becomes an object */
			nativeVal_t *v = OT_Var(target, 1);
			if (v) {
				Native_Free(v);
				NObj_Share(&r);
				*v = r;
				if (ot_cross && v->type == NV_REF) {
					PyObject *py = Native_ToPy(v);
					if (!py) {
						OT_Error(Native_Append(&results));
					}
					Py_XDECREF(py);
				}
			} else {
				Native_Free(&r);
			}
		} else if (store) {
			Native_Free(&r);
		} else {
			*Native_Append(&results) = r;
		}
	}
	for (int i = 0; i < ot_nvars; i++) {
		/* a class and the unbound method in its dict hold each other (CharSpawnPos, as CharsNPC_Close knows) */
		if (NObj_Kind(&ot_vars[i].v) == NO_CLASS && !NObj_DelAttr(&ot_vars[i].v, "__init__")) {
			PyErr_Clear();
		}
		Native_Free(&ot_vars[i].v);
	}
	ot_nvars = 0;
	/* what the test made is gone again (apart from an object that holds itself) */
	NObj_Counts(&made, &freed, &views, &crossed);
	snprintf(text, sizeof(text), "objects made %lu freed %lu left %lu views left %ld crossed %lu\n", made - made0,
		freed - freed0, (made - made0) - (freed - freed0), (long)views - (long)views0, crossed - crossed0);
	Native_BufAdd(out, text, -1);
}

void TestNative_Command(nativeBuf_t *out, const char *arg) {
	byte esc = !strncmp(arg, "esc ", 4);
	nativeRepr_t mode = NR_REPR;

	if (esc) {
		arg += 4;
	} else if (!strcmp(arg, "ports") || !strcmp(arg, "ports reset")) {
		TN_Ports(out, arg[5] != 0);
		return;
	} else if (!strcmp(arg, "objtest") || !strcmp(arg, "objtest cross")) {
		OT_Run(out, arg[7] != 0);
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
