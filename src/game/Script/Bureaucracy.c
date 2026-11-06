/*
 * The Bureaucracy module in C (scripts/chars/bureaucracy.py, issue #2): the desks of the functionaries in the
 * interiors. Init puts an entity on every dummy DM_DesktopFunctionary_*, CreateBureaucracyClass makes the action
 * class of a kind of desk once; the class itself is data/chars/bureaucracy.def. BureaucracyTableFile (the model
 * folder, the Press map changes it) stays a variable of the module.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/global.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

static const char bureaucracy_def[] = {
#embed "../../../data/chars/bureaucracy.def"
	, 0 };

/* CreateBureaucracyClass(BureaucracyClass, FilePath): the class Bureaucracy<BureaucracyClass> unless it is there
 * (SAct.GetClass, which selects it when it is); it stops where the script stops ("Bureaucracy" + BureaucracyClass
 * and "Models/Misc/" + FilePath raise for values that are no strings, the second after the class is made) */
static PyObject *Bureaucracy_CreateBureaucracyClass(PyObject *self, PyObject *args) {
	PyObject *cls, *file;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &cls, &file)) {
		return NULL;
	}
	PyObject *full = Script_SPlus("Bureaucracy", cls);
	if (!full) {
		return NULL;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, PyString_AsString(full));
	byte ok = 1;
	if (!g_pAuxActClass) {
		g_pAuxActClass = cActClassList_Create(g_pActClassList, PyString_AsString(full));
		PyObject *path = Script_SPlus("Models/Misc/", file);
		PyObject *vars = path ? Py_BuildValue("{s:O,s:O}", "cls", cls, "file", file) : NULL;
		ok = vars && Script_RunFileTry(0, "chars/bureaucracy.def", "Class", bureaucracy_def, NULL, vars);
		Py_XDECREF(path);
		Py_XDECREF(vars);
	}
	Py_DECREF(full);
	return ok ? Py_ReturnNone() : NULL;
}

/* names[1] of string.splitfields(name, "_"): a new reference, NULL with the error of the script set */
static PyObject *Bureaucracy_Kind(PyObject *name) {
	if (!PyString_Check(name)) {
		/* the error of string.splitfields on a non-string */
		char msg[0x80];
		Str_Printf(msg, sizeof(msg), "argument 1: expected read-only character buffer, %.50s found", name->ob_type->tp_name);
		PyErr_SetString(PyExc_TypeError, msg);
		return NULL;
	}
	const char *s = PyString_AsString(name), *a = strchr(s, '_');
	if (!a) {
		PyErr_SetString(PyExc_IndexError, "list index out of range");
		return NULL;
	}
	const char *b = strchr(a + 1, '_');
	return PyString_FromStringAndSize((char *)a + 1, b ? (int)(b - a - 1) : (int)strlen(a + 1));
}

/* item i of item j of seq (seq[j][i]), a new reference */
static PyObject *Bureaucracy_Item(PyObject *seq, int j, int i) {
	PyObject *s = PySequence_GetItem(seq, j);
	PyObject *v = s ? PySequence_GetItem(s, i) : NULL;
	Py_XDECREF(s);
	return v;
}

/* one desk on the dummy (name, pos, ang): its class, then the entity; 0 with the error set */
static byte Bureaucracy_Desk(PyObject *dummy, PyObject *list) {
	PyObject *name = PySequence_GetItem(dummy, 0);
	PyObject *kind = name ? Bureaucracy_Kind(name) : NULL;
	PyObject *file = kind ? Script_Attr("Bureaucracy", "BureaucracyTableFile") : NULL;
	PyObject *r = file ? Script_Ask("Bureaucracy", "CreateBureaucracyClass", "(OO)", kind, file) : NULL;
	Py_XDECREF(file);
	PyObject *x = r ? Bureaucracy_Item(dummy, 1, 0) : NULL;
	PyObject *y = x ? Bureaucracy_Item(dummy, 1, 1) : NULL;
	PyObject *z = y ? Bureaucracy_Item(dummy, 1, 2) : NULL;
	PyObject *l = z ? Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "WithLife") : NULL;
	Py_XDECREF(r);
	Py_XDECREF(x);
	Py_XDECREF(y);
	Py_XDECREF(z);
	Py_XDECREF(name);
	PyObject *a0 = l ? Bureaucracy_Item(dummy, 2, 0) : NULL;
	PyObject *a1 = a0 ? Bureaucracy_Item(dummy, 2, 1) : NULL;
	PyObject *ctrl = a1 ? PyString_FromString("Bureaucracy") : NULL;
	if (ctrl) {
		PyString_Concat(&ctrl, kind);
	}
	byte ok = 0;
	if (ctrl && !Script_HSet(l, "Ang", Py_BuildValue("(OOi)", a0, a1, 0))) {
		Py_DECREF(ctrl);
	} else if (ctrl && Script_HSet(l, "ActCtrl", ctrl) && Script_HSet(l, "ActiveCMask", PyInt_FromLong(0)) &&
		Script_HSet(l, "MapCollision", PyInt_FromLong(1))) {
		Py_INCREF(list);
		ok = Script_HSet(l, "EntityList", list);
	}
	Py_XDECREF(a0);
	Py_XDECREF(a1);
	Py_XDECREF(l);
	Py_XDECREF(kind);
	return ok;
}

/* Init(fromViewer=0): the entity list of the desks and a desk on every dummy DM_DesktopFunctionary_* */
static PyObject *Bureaucracy_Init(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "fromViewer", NULL };
	PyObject *viewer = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &viewer)) {
		return NULL;
	}
	PyObject *string = PyImport_ImportModule("string");
	if (!string) {
		return NULL;
	}
	Py_DECREF(string);
	int fromViewer = viewer ? PyObject_IsTrue(viewer) : 0;
	if (fromViewer < 0) {
		return NULL;
	}
	PyObject *list = fromViewer ? PyString_FromString("DesktopsFunctionary") :
		Script_Ask("Scrap", "Get", "(s)", "DesktopsFunctionaryListName");
	if (!list || !Script_Try("Scrap", "CreateEntityList", "(O)", list) ||
		!Script_Try("Scrap", "StartDummySearch", "(si)", "DM_DesktopFunctionary_*", 1)) {
		Py_XDECREF(list);
		return NULL;
	}
	for (;;) {
		PyObject *dummy = Script_Ask("Scrap", "NextDummySearch", "()");
		if (!dummy || dummy == Py_None) {
			Py_DECREF(list);
			if (!dummy) {
				return NULL;
			}
			Py_DECREF(dummy);
			return Py_ReturnNone();
		}
		byte ok = Bureaucracy_Desk(dummy, list);
		Py_DECREF(dummy);
		if (!ok) {
			Py_DECREF(list);
			return NULL;
		}
	}
}

PyMethodDef g_BureaucracyMethods[] = {
	{ "CreateBureaucracyClass", Bureaucracy_CreateBureaucracyClass, 1, NULL },
	{ "Init", (PyCFunction)(void (*)(void))Bureaucracy_Init, 3, NULL },
	{ NULL, NULL, 0, NULL },
};
