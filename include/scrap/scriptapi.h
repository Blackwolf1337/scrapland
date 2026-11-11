/*
 * The engine modules on native entries (stage 7 of docs/objlib-plan.md, batch 7a). Each function of an engine table
 * (Scrap, SScorer, SNet, ...: src/game/PyInterface, SD_PY_MODULE) has an entry of the stage 3 form, byte
 * <adapter>_E(const nativeVal_t *a, int n, nativeVal_t *out), and its adapter is Mode_Entry on it: the arguments as
 * native values (Native_Arg), the entry, the result as Python. tools/script/ports.py reads the engine tables as well
 * as the port tables, so every function with an entry has a ref in include/scrap/scriptports.h (port_Scrap_GetTime)
 * and in the registry (Port_Find("Scrap", "GetTime")), and a value or callback that holds it takes the entry.
 *
 * The entry reads its arguments with Mode_Parse in the format of PyArg_ParseTuple the function had (the same letters,
 * arity texts and TypeErrors) and gives its result with Api_Build (the letters of Py_BuildValue). An entity handle is
 * an NV_ENT (Api_Handle: a new handle, the adapter makes its cPyEntity); OBJLIB 7: a function whose arguments or result
 * stay Python objects
 * (a callable or arguments it keeps) has the entry of MODE_PYENTRY, and its ref takes the slow path.
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
 * The table keeps the adapter (Scrap_GetFloor); the line PORTS(PyInterface_ScrapPy) at the end of the file defines
 * the refs, which tools/script/ports.py lists when it runs again (--lines adds a missing PORTS line).
 */
#ifndef SCRAP_SCRIPTAPI_H
#define SCRAP_SCRIPTAPI_H

#include "scrap/scriptmodes.h"

/* the engine function name of module pre: pre_name_E, its entry, whose body follows the macro (a, n and out may stay
 * unused), and pre_name, its adapter for the PyMethodDef table (Mode_Entry) */
#define API_FN(pre, name) \
	static byte pre##_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out); \
	MODE_ADAPTER(pre, name) \
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
