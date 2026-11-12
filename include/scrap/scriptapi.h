/*
 * The engine modules on native entries (stage 7 of docs/objlib-plan.md, batches 7a and 7e2). Each function of an
 * engine module (Scrap, SScorer, SNet, ...: src/game/PyInterface, SD_PY_MODULE) has an entry of the stage 3 form, byte
 * <fn>_E(const nativeVal_t *a, int n, nativeVal_t *out), and a row in the module's function table (portFn_t).
 * tools/script/ports.py reads the engine tables as well as the port tables, so every function has a ref in
 * include/scrap/scriptports.h (port_Scrap_GetTime) and in the registry (Port_Find("Scrap", "GetTime")); the module
 * holds the native function object of the ref (Api_Module), so a call, a value or a callback takes the entry.
 *
 * The entry reads its arguments with Mode_Parse in the format of PyArg_ParseTuple the function had (the same letters,
 * arity texts and TypeErrors) and gives its result with Api_Build (the letters of Py_BuildValue). An entity handle is
 * an NV_ENT (Api_Handle: a new handle); a function or argument the engine keeps is a
 * native value (Scrap.AddScheduledFunc, SScorer.SetSpeechCallback, Scrap.VideoPlayer).
 *
 * A module of the engine gets its entries this way:
 *
 *   #include "scrap/scriptapi.h"
 *
 *   API_FN(Scrap, GetFloor) {
 *       float y;
 *       return Mode_Parse(a, n, "f", &y) && Api_Build(out, "i", Floor_Get(&g_pWorld->floor, y));
 *   }
 *
 * with the row PORT_FN(GetFloor, Scrap_GetFloor) in Scrap_methods (PORT_FND with the __doc__ text); the line
 * PORTS(PyInterface_ScrapPy) at the end of the file defines the refs, which tools/script/ports.py lists when it runs
 * again (--lines adds a missing PORTS line).
 */
#ifndef SCRAP_SCRIPTAPI_H
#define SCRAP_SCRIPTAPI_H

#include "scrap/scriptmodes.h"

/* init<name> of an engine module (the built-in module table of Python, src/python/config.c; rva the function in
 * Scrap.exe): the module made from its function table <name>_methods (portFn_t, in the file of the module) */
#define SD_PY_MODULE(name, rva) \
	void init##name(void) { \
		Api_Module(#name, name##_methods); \
	}
/* the engine module name with a native function object per row of fns (stage 7e: NObj_NewPortFunc of its ref, put in
 * as Py_InitModule put in the C function objects of a PyMethodDef table: in table order, a name twice the last one;
 * OBJLIB 7e3: the module object is Python's, its dict holds the C function objects of the port functions) */
void Api_Module(const char *name, const portFn_t *fns);

/* the engine function name of module pre: pre_name_E, its entry, whose body follows the macro (a, n and out may stay
 * unused); its row in the function table is PORT_FN(name, pre_name) */
#define API_FN(pre, name) \
	static byte pre##_##name##_E([[maybe_unused]] const nativeVal_t *a, [[maybe_unused]] int n, \
		[[maybe_unused]] nativeVal_t *out)

/*
 * *out the value Py_BuildValue(fmt, ...) makes: none of the letters None, one its value, more a tuple of them;
 * "(...)" a tuple of what it holds. Letters: i an int, l a long, d and f a double (a float argument is promoted), s a
 * text (NULL: None), v a const nativeVal_t * (copied: an entity handle of Api_Handle, a list). Blanks and commas between
 * are skipped. Always 1
 */
byte Api_Build(nativeVal_t *out, const char *fmt, ...);
/* the Python object o (a new reference, released here: a scorer item of SScorer.GetItem) as an NV_OBJ in *out
 * (OBJLIB 7): 1, or 0 for NULL (the error as it is) */
byte Api_Object(PyObject *o, nativeVal_t *out);
/* a new handle of the entity called name as an NV_ENT in *out (Native_SetEnt): 1, or 0 without an error when there is
 * no such entity (the adapter's NULL: the SystemError of the call) */
byte Api_Handle(const char *name, nativeVal_t *out);

#endif
