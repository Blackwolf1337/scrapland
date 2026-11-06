/*
 * The SputnikInterface module in C (scripts/shipedit/sputnikinterface.py, issue #2): the hangar screen Sputnik opens,
 * where the player picks the ship to fly (a destroyed one is rebuilt for rebuildCost). The ship editor itself
 * (ShipEdit, HangarTab) stays Python and is called through its modules. offsetX, offsetY, rebuildCost and
 * EndSputnikCallBack stay variables of the module.
 */
#include "scrap/script.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* DrawBackground(id, elem, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32) */
static PyObject *SI_DrawBackground(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "elem", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static const char *const vars[] = { "X", "Y", "ScaleX", "ScaleY", "Unit", "Alpha", "Red", "Green", "Blue" };
	static const long defs[] = { 64, 64, 512, 352, 1, 92, 0, 0, 32 };
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *elem, *v[9] = { NULL };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", names, &id, &elem, &v[0], &v[1], &v[2], &v[3], &v[5], &v[6],
		&v[7], &v[8])) {
		return NULL;
	}
	PyObject *name = K(Script_SPlus("BackgroundElement", elem));
	if (!name || !Script_Try("SScorer", "Add", "(OOs)", id, name, "Sprite")) {
		FAIL();
	}
	for (int i = 0; i < 9; i++) {
		PyObject *val = v[i] ? v[i] : K(PyInt_FromLong(defs[i]));
		if (!val || !Script_Try("SScorer", "Set", "(OOsO)", id, name, vars[i], val)) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* AddDefaultPanelContent(id): the editor background with the Hangar tab only */
static PyObject *SI_AddDefaultPanelContent(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Script_Import((const char *const[]){ "ShipEdit", NULL })) {
		return NULL;
	}
	PyObject *tabs = Py_BuildValue("[s]", "Hangar");
	byte ok = tabs && Script_Try("ShipEdit", "AddShipEditTabs", "(OO)", id, tabs) &&
		Script_Try("ShipEdit", "AddDefaultPanelContent", "(Oi)", id, 1);
	Py_XDECREF(tabs);
	return ok ? Py_ReturnNone() : NULL;
}

/* ShipEdit.editedShips[i] (a new reference) */
static PyObject *SI_Edited(PyObject *i) {
	PyObject *ships = Script_Attr("ShipEdit", "editedShips");
	PyObject *e = ships && i ? PyObject_GetItem(ships, i) : NULL;
	Py_XDECREF(ships);
	return e;
}

/* ShipEdit.editedShips[i].Active = 0 */
static byte SI_Stop(PyObject *i) {
	PyObject *e = SI_Edited(i);
	byte ok = e && Script_HSet(e, "Active", PyInt_FromLong(0));
	Py_XDECREF(e);
	return ok;
}

/* int(Scrap.GetSaveVar("Hangar.iHangarShip")) (a new reference) */
static PyObject *SI_SavedHangar(void) {
	PyObject *s = Script_SaveVar("Hangar.iHangarShip");
	PyObject *i = Script_Int(s);
	Py_XDECREF(s);
	return i;
}

/* HangarTab.currentHangar (a new reference) */
static PyObject *SI_Current(void) {
	return Script_Attr("HangarTab", "currentHangar");
}

/* EnterSputnikInterface(id): the hangars of the save game, the one being flown selected */
static PyObject *SI_EnterSputnikInterface(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id) || !Script_Try("Scrap", "InitLoading", "()") ||
		!Script_Import((const char *const[]){ "Menu", "ShipEdit", "HangarTab", NULL }) ||
		!Script_Try("Menu", "InitMenuSys", "(i)", 0) || !Script_Try("Menu", "StartNewMenu", "(O)", id) ||
		!Script_Try("SputnikInterface", "AddDefaultPanelContent", "(O)", id)) {
		return NULL;
	}
	PyObject *hint = K(Script_LangOf(PyString_FromString("HINT_NoMoney")));
	if (!hint || !Script_Try("SScorer", "SetOnSpecialHint", "(OsO)", id, "ShipEdit.ShowSpecialHint", hint) ||
		!Script_Try("ShipEdit", "CleanEditedShips", "()")) {
		FAIL();
	}
	for (int i = 0; i < 9; i++) {
		char var[0x20];
		Str_Printf(var, sizeof(var), "Hangar.HangarShip%d", i + 1);
		PyObject *cad = K(Script_SaveVar(var)), *empty = cad ? K(PyString_FromString("")) : NULL;
		int same = empty ? Script_Eq(cad, empty) : -1;
		if (same < 0 || (!same && !Script_Try("ShipEdit", "AssignShip", "(Oi)", cad, i))) {
			FAIL();
		}
	}
	PyObject *aux = K(Script_SaveVar("Hangar.HangarShipAux"));
	if (!aux || !Script_Try("ShipEdit", "AssignShip", "(Oi)", aux, 9)) {
		FAIL();
	}
	Script_SetVar("ShipEdit", "fromSputnik", PyInt_FromLong(1));
	PyObject *text = K(Script_SaveVar("Hangar.DestroyedShips")), *lost = text ? K(Script_EvalIn("SputnikInterface", text)) : NULL;
	if (!lost) {
		FAIL();
	}
	Script_SetVar("ShipEdit", "destroyedShips", Script_Ref(lost));
	PyObject *ships = K(Script_Attr("ShipEdit", "destroyedShips")), *zero = ships ? K(PyInt_FromLong(0)) : NULL;
	PyObject *nine = zero ? K(PyInt_FromLong(9)) : NULL;
	if (!nine || PyObject_SetItem(ships, nine, zero) < 0) {
		FAIL();
	}
	PyObject *cur = K(SI_SavedHangar());
	if (!cur) {
		FAIL();
	}
	Script_SetVar("HangarTab", "currentHangar", Script_Ref(cur));
	if (!Script_Try("HangarTab", "SetHangarPanel", "(O)", id) || !Script_Try("HangarTab", "SetSellVisible", "(Oi)", id, 0)) {
		FAIL();
	}
	cur = K(SI_Current());
	int aux9 = cur ? Script_NumCmp(cur, 9) : -2;
	if (aux9 == -2) {
		FAIL();
	}
	if (aux9 == 0) {
		if (!Script_Try("SScorer", "SetDefault", "(Os)", id, "HangarAux")) {
			FAIL();
		}
	} else {
		cur = K(SI_Current());
		int pos = cur ? Script_NumCmp(cur, 0) : -2;
		if (pos == -2) {
			FAIL();
		}
		PyObject *hangar = NULL;
		if (pos >= 0) {
			cur = K(SI_Current());
			PyObject *one = cur ? K(PyInt_FromLong(1)) : NULL, *n = one ? K(PyNumber_Add(cur, one)) : NULL;
			hangar = n ? K(Script_SPlus("Hangar", K(Script_Repr(n)))) : NULL;
			if (!hangar) {
				FAIL();
			}
		}
		if (!Script_Try("SScorer", "SetDefault", "(Os)", id, hangar ? PyString_AsString(hangar) : "Hangar1")) {
			FAIL();
		}
	}
	cur = K(SI_Current());
	if (!cur || !SI_Stop(cur) || !Script_Try("HangarTab", "ShowHangarPanel", "(O)", id) ||
		!Script_Try("HangarTab", "ShowShipInfo", "(O)", id) ||
		!Script_Try("SScorer", "SetOnCancel", "(Os)", id, "SputnikInterface.ExitSputnikInterface") ||
		!Script_Try("Scrap", "EndLoading", "()")) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ExitSputnikInterface(id, control): the money and the destroyed ships saved, back to the game, the callback once */
static PyObject *SI_ExitSputnikInterface(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control) ||
		!Script_Import((const char *const[]){ "HangarTab", "Menu", "SaveGame", "ShipEdit", NULL }) ||
		!Script_Try("SaveGame", "SaveMoney", "()")) {
		return NULL;
	}
	PyObject *lost = K(Script_Attr("ShipEdit", "destroyedShips")), *text = lost ? K(Script_Repr(lost)) : NULL;
	if (!text || !Script_Try("Scrap", "SetSaveVar", "(sO)", "Hangar.DestroyedShips", text) ||
		!Script_Try("Menu", "BackToGame", "(O)", id)) {
		FAIL();
	}
	PyObject *cb = K(Script_Global("SputnikInterface", "EndSputnikCallBack"));
	int set = cb ? Script_True(cb) : -1;
	if (set < 0 || (set && (!K(Script_Invoke(cb, PyTuple_New(0))) ||
		!Script_SetGlobal("SputnikInterface", "EndSputnikCallBack", Script_Ref(Py_None))))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* the ship of the hangar the save game flew stops: if ShipEdit.editedShips[prev]: ....Active = 0 */
static byte SI_StopPrev(void) {
	PyObject *prev = SI_SavedHangar(), *e = SI_Edited(prev);
	int live = e ? Script_True(e) : -1;
	byte ok = live == 0 || (live > 0 && SI_Stop(prev));
	Py_XDECREF(e);
	Py_XDECREF(prev);
	return ok;
}

/* Exit through the module, as the script calls it */
static byte SI_Exit(PyObject *id, PyObject *control) {
	return Script_Try("SputnikInterface", "ExitSputnikInterface", "(OO)", id, control);
}

/* the save game flies hangar (its repr as the save variable) */
static byte SI_Fly(PyObject *hangar) {
	PyObject *rep = Script_Repr(hangar);
	byte ok = rep && Script_Try("Scrap", "SetSaveVar", "(sO)", "Hangar.iHangarShip", rep) && SI_Stop(hangar);
	Py_XDECREF(rep);
	return ok;
}

/* SelectShip(id, control): the hangar picked (HangarAux, Hangar1..9): the same, an intact valid ship, or a rebuild */
static PyObject *SI_SelectShip(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control) || !Script_Import((const char *const[]){ "HangarTab", "ShipEdit", NULL })) {
		return NULL;
	}
	if (Script_Is(control, "HangarAux")) {
		PyObject *nine = K(PyInt_FromLong(9));
		byte ok = nine && SI_StopPrev() && Script_Try("Scrap", "SetSaveVar", "(ss)", "Hangar.iHangarShip", "9") &&
			SI_Stop(nine) && SI_Exit(id, control);
		return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
	}
	int len = PyObject_Length(control);
	PyObject *last = len >= 0 ? K(PySequence_GetItem(control, len - 1)) : NULL, *n = last ? K(Script_Int(last)) : NULL;
	PyObject *one = n ? K(PyInt_FromLong(1)) : NULL, *hangar = one ? K(PyNumber_Subtract(n, one)) : NULL;
	PyObject *cur = hangar ? K(SI_Current()) : NULL;
	int same = cur ? Script_Eq(hangar, cur) : -1;
	if (same < 0) {
		FAIL();
	}
	if (same) {
		return Script_Done(&r, SI_Exit(id, control) ? Py_ReturnNone() : NULL);
	}
	PyObject *lost = K(Script_Attr("ShipEdit", "destroyedShips")), *d = lost ? K(PyObject_GetItem(lost, hangar)) : NULL;
	int dead = d ? Script_NumCmp(d, 0) : -2;
	if (dead == -2) {
		FAIL();
	}
	int valid = 0;
	if (dead == 0) {
		PyObject *e = K(SI_Edited(hangar)), *v = e ? K(Script_Ask("HangarTab", "IsShipValid", "(O)", e)) : NULL;
		valid = v ? Script_True(v) : -1;
		if (valid < 0) {
			FAIL();
		}
	}
	if (valid) {
		return Script_Done(&r, SI_StopPrev() && SI_Fly(hangar) && SI_Exit(id, control) ? Py_ReturnNone() : NULL);
	}
	lost = K(Script_Attr("ShipEdit", "destroyedShips"));
	d = lost ? K(PyObject_GetItem(lost, hangar)) : NULL;
	int rebuild = d ? Script_NumCmp(d, 1) : -2;
	if (rebuild == -2) {
		FAIL();
	}
	if (rebuild != 0) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *money = K(Script_Ask("Scrap", "GetMoney", "()")), *cost = money ? K(Script_Global("SputnikInterface", "rebuildCost")) : NULL;
	int poor = cost ? PyObject_Compare(money, cost) : 0;
	if (!cost || PyErr_Occurred()) {
		FAIL();
	}
	if (poor >= 0) {
		PyObject *left = SI_StopPrev() ? K(PyNumber_Subtract(money, cost)) : NULL;
		PyObject *zero = left && Script_Try("Scrap", "SetMoney", "(O)", left) && SI_Fly(hangar) ? K(PyInt_FromLong(0)) : NULL;
		lost = zero ? K(Script_Attr("ShipEdit", "destroyedShips")) : NULL;
		if (!lost || PyObject_SetItem(lost, hangar, zero) < 0 || !SI_Exit(id, control)) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

PyMethodDef g_SputnikInterfaceMethods[] = {
	{ "DrawBackground", (PyCFunction)(void (*)(void))SI_DrawBackground, 3, NULL },
	{ "AddDefaultPanelContent", SI_AddDefaultPanelContent, 1, NULL },
	{ "EnterSputnikInterface", SI_EnterSputnikInterface, 1, NULL },
	{ "ExitSputnikInterface", SI_ExitSputnikInterface, 1, NULL },
	{ "SelectShip", SI_SelectShip, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
