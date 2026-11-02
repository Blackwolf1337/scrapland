/*
 * The Python type cPyMenuEntity (type object rva 0x428f78): a handle of a scorer item (HUD and menu element) by
 * player and name. The item is looked up again on every access. Its attributes are the variables of the item
 * (cScorerItem Var: 1 string, 2 double, 3 int); the type has no methods. The original source file is not known.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/scorer.h"

PyObject *ScorerItem_PyNew(int player, const char *name) {
	cScorer *scorer = Scorer_Get(player);
	if (!scorer || !cScorer_FindItem(scorer, name)) {
		return NULL;
	}
	cPyMenuEntity *self = (cPyMenuEntity *)_PyObject_New(&cPyMenuEntity_Type);
	if (!self) {
		return NULL;
	}
	size_t size = strlen(name) + 1;
	self->player = player;
	self->name = Mem_Alloc(size);
	memcpy(self->name, name, size);
	return (PyObject *)self;
}

/* tp_dealloc */
void PyMenuEntity_Dealloc(PyObject *self) {
	Mem_Free(((cPyMenuEntity *)self)->name);
	PyPool_Free(self);
}

/* tp_print: the text goes to fprintf as the format, like in the original */
int PyMenuEntity_Print(PyObject *self, FILE *fp, int flags) {
	static char buf[0x100];
	cPyMenuEntity *m = (cPyMenuEntity *)self;
	cScorer *scorer = Scorer_Get(m->player);

	(void)flags;
	if (scorer && cScorer_FindItem(scorer, m->name)) {
		Str_Printf(buf, sizeof(buf), "Menu item: %d %s", m->player, m->name);
	} else {
		Str_Printf(buf, sizeof(buf), " : The menu item called '%s' at scorer %d is invalid.", m->name, m->player);
	}
	crt_fprintf(fp, buf);
	return 0;
}

static cScorerItem *PyMenuEntity_Item(cPyMenuEntity *m, const char *attr) {
	cScorer *scorer = Scorer_Get(m->player);
	if (!scorer) {
		Con_Errorf("(%d,'%s')->%s : Menu invalid Scorer.\n", m->player, m->name, attr);
		return NULL;
	}
	cScorerItem *item = cScorer_FindItem(scorer, m->name);
	if (!item) {
		Con_Errorf("(%d,'%s')->%s : Menu invalid Item.\n", m->player, m->name, attr);
	}
	return item;
}

/* tp_getattr */
PyObject *PyMenuEntity_GetAttr(PyObject *self, char *name) {
	cPyMenuEntity *m = (cPyMenuEntity *)self;
	cScorerItem *item = PyMenuEntity_Item(m, name);
	char *s;
	double d;
	int i;

	if (!item) {
		return NULL;
	}
	switch (item->vftable->Var(item, name, 0, &s, &d, &i)) {
	case 1:
		return Py_BuildValue("s", s);
	case 2:
		return Py_BuildValue("d", d);
	case 3:
		return Py_BuildValue("i", i);
	}
	Con_Errorf("(%d,'%s')->%s : Menu invalid member.\n", m->player, m->name, name);
	return NULL;
}

/*
 * tp_setattr: the value is tried as int, float and string, each type that parses goes to the item until the
 * variable has that type. A missing scorer or item returns 0 (no error) like in the original. The original
 * passes the other two values uninitialized; they are 0 here.
 */
int PyMenuEntity_SetAttr(PyObject *self, char *name, PyObject *v) {
	cPyMenuEntity *m = (cPyMenuEntity *)self;
	cScorerItem *item = PyMenuEntity_Item(m, name);
	int parsed = 0;
	char *s = NULL;
	double d = 0.0;
	int i = 0;

	if (!item) {
		return 0;
	}
	if (PyArg_Parse(v, "i", &i)) {
		parsed = 1;
		if (item->vftable->Var(item, name, 1, &s, &d, &i) == 3) {
			return 0;
		}
	} else {
		PyErr_Clear();
	}
	if (PyArg_Parse(v, "d", &d)) {
		parsed = 1;
		if (item->vftable->Var(item, name, 1, &s, &d, &i) == 2) {
			return 0;
		}
	} else {
		PyErr_Clear();
	}
	if (PyArg_Parse(v, "s", &s)) {
		if (item->vftable->Var(item, name, 1, &s, &d, &i) == 1) {
			return 0;
		}
	} else {
		PyErr_Clear();
		if (!parsed) {
			Con_Errorf("(%d,'%s')->%s : Menu invalid type.\n", m->player, m->name, name);
			return -1;
		}
	}
	Con_Errorf("(%d,'%s')->%s : Menu invalid Member.\n", m->player, m->name, name);
	return -1;
}

const char str_3c75a8[] = "cPyMenuEntity";

extern const char str_3c75a8[];
PyTypeObject cPyMenuEntity_Type = {
	.ob_refcnt = 1,
	.ob_type = &PyType_Type,
	.tp_name = (void *)str_3c75a8,
	.tp_basicsize = sizeof(cPyMenuEntity),
	.tp_dealloc = (void *)PyMenuEntity_Dealloc,
	.tp_print = (void *)PyMenuEntity_Print,
	.tp_getattr = (void *)PyMenuEntity_GetAttr,
	.tp_setattr = (void *)PyMenuEntity_SetAttr,
};
