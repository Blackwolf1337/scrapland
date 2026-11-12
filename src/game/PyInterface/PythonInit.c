/*
 * Start and end of the interpreter, and modules from the script pack. Original source: PyInterface/PythonInit.cpp.
 */
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/pyfunction.h"
#include "scrap/mempool.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/staticinit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cGVar GVar_Viewer;

#define g_IsViewer (GVar_Viewer.value.i)		/* model viewer build: Viewer.py instead of Init.py */
static cGVar GVar_PythonExecute;
#define g_PythonExecute (GVar_PythonExecute.value.s)	/* engine variable PythonExecute */
cStaticBuffer cStaticBuffer_PyStaticBuffer[2];

#define g_PyStaticBuffer (&cStaticBuffer_PyStaticBuffer[0])

/* OBJLIB 7e4b: frees the free lists of the object types of the library at the end */
void Py_FiniObjects(void);

byte g_PythonReady;

/*
 * The start and end lines of the exe, each a PyRun_SimpleString of the engine in __main__ (single_input; "Scrap
 * input", CR dropped), made in C on the native modules (src/game/Script/ScriptImport.c): a line that fails prints its
 * error and the next line runs; a value an expression statement gives is printed when it is not None (PRINT_EXPR of
 * the interactive mode); Py_FlushLine ends every line. Names are looked up in __main__ as LOAD_NAME does, imports
 * stored there.
 */

/* the end of a start line: its error printed, else the line flushed */
static void Py_LineEnd(byte ok) {
	if (!ok) {
		Native_ErrPrint();
	} else if (!Script_FlushLine()) {
		Native_ErrClear();
	}
}

/* an expression statement of single_input (PRINT_EXPR): a value that is not None printed with its repr and kept in
 * __builtin__._; 0 with the error set */
static byte Py_LineExpr(const nativeVal_t *v) {
	nativeVal_t key, none, out;
	if (v->type == NV_NONE) {
		return 1;
	}
	Native_SetStr(&key, "_");
	Native_SetNone(&none);
	byte ok = NObj_SetItem(Script_Builtins(), &key, &none) && Script_FlushLine();
	if (ok && !Script_SysVar("stdout", &out)) {
		ok = Native_Raise(NE_RUNTIME, "lost sys.stdout");
	} else if (ok) {
		ok = Script_FileWrite(&out, v, 0);
		if (ok) {
			Script_SoftSpace(&out, 1);
			ok = Script_FlushLine();
		}
		Native_Free(&out);
	}
	ok = ok && NObj_SetItem(Script_Builtins(), &key, v);
	Native_Free(&key);
	return ok;
}

/* name = v in __main__ (STORE_NAME into the registry's variables of __main__) */
static byte Py_LineStore(const char *name, const nativeVal_t *v) {
	modVarRef_t ref;
	return ModVar_OfVal(Script_Main(), &ref) && ModVar_SetVal(&ref, name, v) == 0;
}

/* import a, b, ... in __main__ (NULL ends the names; the line stops at the first error) */
static byte Py_LineImport(const char *const *names) {
	for (; *names; names++) {
		nativeVal_t m;
		byte ok = Script_Module(*names, &m) && Py_LineStore(*names, &m);
		Native_Free(&m);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* name(args) as an expression statement of a line ("Init.Init": LOAD_NAME and LOAD_ATTR in __main__), n arguments */
static byte Py_LineCall(const char *name, const nativeVal_t *a, int n) {
	nativeVal_t f, r;
	byte ok = Script_NameChain(NULL, Script_Main(), name, &f) > 0 && NObj_Call(&f, a, n, &r);
	Native_Free(&f);
	if (ok) {
		ok = Py_LineExpr(&r);
		Native_Free(&r);
	}
	return ok;
}

/* func('text') of a line as the exe builds it (Str_Printf in 0x100 chars): the text between the quotes becomes the
 * string the compiler makes of it (escapes); a quote or a control character in it, or a cut line, is the line's
 * SyntaxError */
static byte Py_LineCallText(const char *func, const char *text) {
	char cmd[0x100];
	int n = Str_Printf(cmd, sizeof(cmd), "%s('%s')", func, text), len = (int)strlen(func);
	nativeVal_t v;
	byte ok = 0;
	Native_SetNone(&v);
	if (n == (int)strlen(cmd) && n > len + 2 && cmd[n - 1] == ')') {
		cmd[n - 1] = '\0';
		ok = Native_Literal(cmd + len + 1, &v) && v.type == NV_STR;
	}
	if (!ok) {
		Native_Free(&v);
		Script_NotRead("start line", cmd);
		return 0;
	}
	ok = Py_LineCall(func, &v, 1);
	Native_Free(&v);
	return ok;
}

/* the call of a start line with one text argument */
static byte Py_LineCallStr(const char *func, const char *a) {
	nativeVal_t v;
	Native_SetStr(&v, a);
	byte ok = Py_LineCall(func, &v, 1);
	Native_Free(&v);
	return ok;
}

/* write(message) of the classes ConsoleOutput (ctx 0) and ConsoleErrorOutput (1): Scrap.ConsoleOut(message) or
 * Scrap.ConsoleError(message), 'None' for None */
static byte Py_ConsoleWrite(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t none;
	(void)out;
	if (!Native_Arity(NULL, n, 1, 1)) {
		return 0;
	}
	Native_SetStr(&none, "None");
	const nativeVal_t *m = a[0].type == NV_NONE ? &none : &a[0];
	return ctx ? PORT_CALL(Scrap, ConsoleError, NULL, "v", m) : PORT_CALL(Scrap, ConsoleOut, NULL, "v", m);
}

static byte Py_ConsoleFlush(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	(void)a;
	(void)out;
	return Native_Arity(NULL, n, 0, 0);
}

/* class name: softspace = 1, write(message), flush() (C functions in the class, not bound: called from the instance
 * with the message alone), in __main__; the dict filled in the order of the class body, then __doc__ and __module__ as
 * PyClass_New adds them */
static byte Py_LineConsoleClass(const char *name, byte error) {
	static const char *const keys[] = { "softspace", "write", "flush", "__doc__", "__module__" };
	nativeVal_t d, v[5], cls, key;
	Native_SetInt(&v[0], 1);
	NObj_NewCallback(&v[1], "write", Py_ConsoleWrite, (void *)(intptr_t)error, NULL, 0);
	NObj_NewCallback(&v[2], "flush", Py_ConsoleFlush, NULL, NULL, 0);
	Native_SetNone(&v[3]);
	Native_SetStr(&v[4], "__main__");
	Native_SetEmpty(&d, NV_DICT);
	for (int i = 0; i < 5; i++) {
		Native_SetStr(&key, keys[i]);
		Native_DictSet(&d, &key, &v[i]);
	}
	byte ok = NObj_NewClass(&cls, name, NULL, NULL, &d) && Py_LineStore(name, &cls);
	Native_Free(&cls);
	Native_Free(&d);
	return ok;
}

/* sys.<attr> = <name of a class>() */
static byte Py_LineSysOutput(const char *attr, const char *cls) {
	nativeVal_t c, o, sys;
	Native_SetNone(&sys);
	byte ok = Script_NameChain(NULL, Script_Main(), cls, &c) > 0 && NObj_Call(&c, NULL, 0, &o);
	Native_Free(&c);
	ok = ok && Script_NameChain(NULL, Script_Main(), "sys", &sys) > 0 && NObj_SetAttr(&sys, attr, &o);
	if (ok || o.type != NV_NONE) {
		Native_Free(&o);
	}
	Native_Free(&sys);
	return ok;
}

/* print 'Python Ver '+sys.version */
static byte Py_LineVersion(void) {
	nativeVal_t v, head, t;
	const nativeVal_t *items[1] = { &t };
	Native_SetStr(&head, "Python Ver ");
	Native_SetNone(&t);
	byte ok = Script_NameChain(NULL, Script_Main(), "sys.version", &v) > 0 && Native_Arith('+', &head, &v, &t) &&
		Script_Print(items, 1);
	Native_Free(&v);
	Native_Free(&t);
	return ok;
}

void Python_Init(void) {
	static const char *const sys[] = { "sys", NULL }, *const engine[] = { "Scrap", "SInput", "SNet", "SWeap", "SScorer",
		"SVec", "SSound", "SAI", "SAct", "SFX", "SLogic", NULL }, *const init[] = { "Init", NULL },
		*const viewer[] = { "Viewer", NULL };
	nativeVal_t a[2];

	/* the modules of Py_Initialize */
	CHECK(Script_ImportStart());
	g_PythonReady = 1;
	Py_LineEnd(Py_LineConsoleClass("ConsoleOutput", 0));
	Py_LineEnd(Py_LineConsoleClass("ConsoleErrorOutput", 1));
	Py_LineEnd(Py_LineImport(sys));
	Py_LineEnd(Py_LineSysOutput("stdout", "ConsoleOutput"));
	Py_LineEnd(Py_LineSysOutput("stderr", "ConsoleErrorOutput"));
	Py_LineEnd(Py_LineVersion());
	Py_LineEnd(Py_LineCallStr("sys.path.append", "Scripts"));
	Py_LineEnd(Py_LineImport(engine));
	if (g_IsViewer) {
		Py_LineEnd(Py_LineImport(viewer));
		Py_LineEnd(Py_LineCallText("Viewer.Init", g_pApp->levelPath));
	} else {
		Py_LineEnd(Py_LineCallStr("Scrap.OpenPack", "Scripts"));
		Native_SetStr(&a[0], "Init");
		Native_SetStr(&a[1], "Init.pyc");
		Py_LineEnd(Py_LineCall("Scrap.PreloadLibrary", a, 2));
		Native_Free(&a[0]);
		Native_Free(&a[1]);
		Py_LineEnd(Py_LineCall("Scrap.ClosePack", NULL, 0));
		Py_LineEnd(Py_LineImport(init));
		Py_LineEnd(Py_LineCallText("Init.Init", g_pApp->levelPath));
	}
	/* the engine variable PythonExecute (the command line) was a line of Python run here: there is no interpreter, an
	 * error says so when it is set */
	if (g_PythonExecute && *g_PythonExecute && (!GVar_PythonExecute.def.s ||
		strcmp(g_PythonExecute, GVar_PythonExecute.def.s))) {
		Con_Errorf("PythonExecute ignored, no Python interpreter in this build: %s\n", g_PythonExecute);
	}
	/* the switches that ran the .pyc of the packs instead of the C modules (Script.c, ScriptModule.c) */
	static byte told;
	const char *pack = getenv("SCRAP_PACK_SCRIPTS"), *cmod = getenv("SCRAP_CMOD");
	if (!told && ((pack && *pack == '1') || (cmod && *cmod == '0'))) {
		told = 1;
		Con_Errorf("%s: no script module loads, the scripts of the packs need the Python interpreter (not in this "
			"build)\n", pack && *pack == '1' ? "SCRAP_PACK_SCRIPTS=1" : "SCRAP_CMOD=0");
	}
	/* the buffer the exe's Pack_PreloadLibrary read the .pyc into is not needed after the start (the net code gets it
	 * again when it needs it) */
	cStaticBuffer *buf = g_PyStaticBuffer;
	CHECK(!buf->lock);
	Mem_Free(buf->data);
	buf->data = NULL;
	buf->size = 0;
}

void Python_Close(void) {
	static const char *const init[] = { "Init", NULL };

	Py_LineEnd(Py_LineImport(init) && Py_LineCall("Init.Close", NULL, 0));
	cPy_Function_ClearAll();
	CharsNPC_Close();
	/* the module variables set to None as the module cleanup of Py_Finalize cleared the module dicts */
	ModVar_Close();
	Native_HookClear();
	Script_ImportEnd();
	g_PythonReady = 0;
}

void Python_Shutdown(void) {
	Py_FiniObjects();
	ScriptPool_Delete();
}

/*
 * The gate of the pack modules. There is no interpreter, so a module without a C definition is not loaded (a .pyc of
 * the packs or of a mod): it stays out of the module store (an import of it raises ImportError) and a console line
 * says so the first time for each name. SCRAP_PY_GATE=<file> appends a line per pack module the engine would load:
 * the level, what happened, the module name and the file. What happened: "cmod" (the module is made from its C
 * definition, Script_ModuleMake), "loaded" (it is loaded already, PreloadLibrary does nothing), "refused" and
 * "notread" (a text Script_NotRead does not run).
 */
static int gate_on = -1;
static FILE *gate_file;

#define GATE_IGNORED_MAX 512

/* the console line of a module that is not loaded, once for each name */
static void Gate_IgnoredOnce(const char *name, const char *file) {
	static char seen[GATE_IGNORED_MAX][0x40];
	static int n;

	for (int i = 0; i < n; i++) {
		if (!strncmp(seen[i], name, sizeof(seen[i]) - 1)) {
			return;
		}
	}
	if (n < GATE_IGNORED_MAX) {
		Str_Copy(seen[n++], name, sizeof(seen[0]));
	}
	Con_Printf("%s (%s) not loaded: it is Python code, this build has no interpreter\n", name, file);
}

static byte Gate_On(void) {
	if (gate_on < 0) {
		const char *e = getenv("SCRAP_PY_GATE");
		gate_on = e && *e && strcmp(e, "0");
		if (gate_on) {
			gate_file = strcmp(e, "1") ? fopen(e, "a") : NULL;
		}
	}
	return gate_on > 0;
}

void Script_GateNote(const char *what, const char *name, const char *file) {
	if (!Gate_On()) {
		return;
	}
	const char *level = g_pApp && g_pApp->levelPath ? g_pApp->levelPath : "-";
	if (gate_file) {
		fprintf(gate_file, "%s\t%s\t%s\t%s\n", level, what, name, file);
		fflush(gate_file);
	} else {
		Con_Printf("PYGATE %s %s %s %s\n", level, what, name, file);
	}
}

/* a module without a C definition: the refusal is logged and ImportError set */
static void Gate_Refuse(const char *name, const char *file) {
	Gate_IgnoredOnce(name, file);
	Script_GateNote("refused", name, file);
	Native_Raise(NE_IMPORT, "pack module %.100s (%.100s) refused: no C definition", name, file);
}

/* a module made from its C definition (1), none (0, it is refused) or a failed one (-1, error set); logged */
static int Gate_ModuleMake(const char *name, const char *file, nativeVal_t *m) {
	int made = Script_ModuleMake(name, file, m);
	if (made) {
		Script_GateNote("cmod", name, file);
	}
	return made;
}

/*
 * PreloadLibrary(name, file): the module name of the compiled module file of the open pack (the exe imported the
 * .pyc; while it recorded the files read from packs, through the normal path). A module that has a C definition
 * (Script_ModuleMake) is made from it, one without is refused; no .pyc is read. While the engine records the files
 * read from packs it logs the file.
 */
void Pack_PreloadLibrary(const char *name, const char *file) {
	nativeVal_t m;
	CHECK(Py_IsReady());
	if (PackLog_Closed()) {
		if (Script_ModuleLoaded(name, &m)) {
			Script_ModuleCount(SMC_LOADED);
			Script_GateNote("loaded", name, file);
		} else {
			int made = Gate_ModuleMake(name, file, &m);
			if (made) {
				if (made < 0) {
					Native_ErrPrint();
				}
			} else {
				Gate_Refuse(name, file);
				Native_ErrClear();
			}
		}
		Native_Free(&m);
	} else {
		Con_Verbosef("Trying to open 'PACK: %s', using import %s\n", file, name);
		PackLog_Add(file);
		byte loaded = Script_ModuleLoaded(name, &m);
		int made = loaded ? 0 : Gate_ModuleMake(name, file, &m);
		if (!made) {
			Gate_Refuse(name, file);
		}
		if (made <= 0) {
			/* the message of the exe for a failed import here (Py_VerboseFlag is never set); PyFile_WriteString writes
			 * nothing while the error of the import is set */
			nativeVal_t err;
			if (Script_SysVar("stderr", &err)) {
				Script_FileWriteText(&err, "'import site' failed; use -v for traceback\n");
			}
			Native_Free(&err);
			Native_ErrClear();
		}
		Native_Free(&m);
	}
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_VECTOR(Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_dtor)
INIT_VECTOR(Init_cStaticBuffer_PyStaticBuffer, Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_ctor, cStaticBuffer_dtor)
FINI_GVAR_STRING(Fini_GVar_PythonExecute, &GVar_PythonExecute)
INIT_GVAR_STRING(Init_GVar_PythonExecute, Fini_GVar_PythonExecute, &GVar_PythonExecute, "PythonExecute", 4, "This will run a pyhon command.", "print 'No PythonExecute'")
INIT_GVAR_INT(Init_GVar_Viewer, &GVar_Viewer, "Viewer", 4, " 0(default) : Normal game,  1 : Init Viewer", 0)
