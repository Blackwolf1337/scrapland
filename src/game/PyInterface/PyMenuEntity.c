/*
 * The script handle of a scorer item (HUD and menu element) by player and name, the type cPyMenuEntity of the exe (a
 * Python type there; a native C object here, NObj_NewCObj). The item is looked up again on every access. Its
 * attributes are the variables of the item (cScorerItem Var: 1 string, 2 double, 3 int); the type has no methods. The
 * original source file is not known.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/nativeobj.h"
#include "scrap/scorer.h"
#include "scrap/scriptnative.h"

typedef struct {
	int player;
	char *name;			/* Mem_Alloc copy */
} menuItem_t;

static void MenuItem_Drop(void *ctx) {
	menuItem_t *m = ctx;
	Mem_Free(m->name);
	Mem_Free(m);
}

/* tp_print: the text goes to fprintf as the format, like in the original */
static void MenuItem_Print(void *ctx, char *buf, int size) {
	menuItem_t *m = ctx;
	cScorer *scorer = Scorer_Get(m->player);

	if (scorer && cScorer_FindItem(scorer, m->name)) {
		Str_Printf(buf, size, "Menu item: %d %s", m->player, m->name);
	} else {
		Str_Printf(buf, size, " : The menu item called '%s' at scorer %d is invalid.", m->name, m->player);
	}
}

static cScorerItem *MenuItem_Item(menuItem_t *m, const char *attr) {
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

/* tp_getattr; NULL without an error for a missing item or member */
static byte MenuItem_GetAttr(void *ctx, const char *name, nativeVal_t *out) {
	menuItem_t *m = ctx;
	cScorerItem *item = MenuItem_Item(m, name);
	/* differs from the original: 0 as in SScorer.Get */
	char *s = NULL;
	double d = 0.0;
	int i = 0;

	Native_SetNone(out);
	if (!item) {
		return 0;
	}
	switch (ScorerItem_Var(item, name, 0, &s, &d, &i)) {
	case 1:
		/* Py_BuildValue("s"): None for no text */
		if (s) {
			Native_SetText(out, s, (int)strlen(s));
		}
		return 1;
	case 2:
		Native_SetFloat(out, d);
		return 1;
	case 3:
		Native_SetInt(out, i);
		return 1;
	}
	Con_Errorf("(%d,'%s')->%s : Menu invalid member.\n", m->player, m->name, name);
	return 0;
}

/*
 * tp_setattr: the value is tried as int, float and string (PyArg_Parse "i", "d", "s"), each type that parses goes to
 * the item until the variable has that type. A missing scorer or item returns 0 (no error) like in the original. The
 * original passes the other two values uninitialized; they are 0 here. A delete (v NULL) parses as none of them.
 */
static byte MenuItem_SetAttr(void *ctx, const char *name, const nativeVal_t *v) {
	menuItem_t *m = ctx;
	cScorerItem *item = MenuItem_Item(m, name);
	int parsed = 0;
	char *s = NULL;
	double d = 0.0;
	int i = 0;
	const char *text;
	nativeVal_t x;

	if (!item) {
		return 1;
	}
	if (v) {
		Native_Pass(v, &x);
	} else {
		Native_SetMissing(&x);
	}
	if (v && Native_ArgInt(&x, &i)) {
		parsed = 1;
		if (ScorerItem_Var(item, name, 1, &s, &d, &i) == 3) {
			Native_Free(&x);
			return 1;
		}
	} else {
		Native_ErrClear();
	}
	if (v && Native_ArgDouble(&x, &d)) {
		parsed = 1;
		if (ScorerItem_Var(item, name, 1, &s, &d, &i) == 2) {
			Native_Free(&x);
			return 1;
		}
	} else {
		Native_ErrClear();
	}
	if (v && Native_ArgStr(&x, 1, &text)) {
		s = (char *)text;
		if (ScorerItem_Var(item, name, 1, &s, &d, &i) == 1) {
			Native_Free(&x);
			return 1;
		}
	} else {
		Native_ErrClear();
		if (!parsed) {
			Native_Free(&x);
			Con_Errorf("(%d,'%s')->%s : Menu invalid type.\n", m->player, m->name, name);
			return 0;
		}
	}
	Native_Free(&x);
	Con_Errorf("(%d,'%s')->%s : Menu invalid Member.\n", m->player, m->name, name);
	return 0;
}

static menuItem_t *MenuItem_New(int player, const char *name) {
	menuItem_t *m = Mem_Alloc(sizeof(*m));
	size_t size = strlen(name) + 1;
	m->player = player;
	m->name = Mem_Alloc(size);
	memcpy(m->name, name, size);
	return m;
}

/* ---- OBJLIB 7e4b: the Python face of a scorer item (the same type functions on a copy of the handle) ---- */

typedef struct {
	PyObject_HEAD
	menuItem_t *m;
} menuItemFace_t;

static PyTypeObject menu_itemFaceType;

static PyObject *MenuItem_ToPy(void *ctx) {
	menuItem_t *m = ctx;
	menuItemFace_t *f = (menuItemFace_t *)_PyObject_New(&menu_itemFaceType);
	if (f) {
		f->m = MenuItem_New(m->player, m->name);
	}
	return (PyObject *)f;
}

static void MenuItemFace_Dealloc(PyObject *self) {
	MenuItem_Drop(((menuItemFace_t *)self)->m);
	ScriptPool_Free(self);
}

static int MenuItemFace_Print(PyObject *self, FILE *fp, int flags) {
	static char buf[0x100];
	(void)flags;
	MenuItem_Print(((menuItemFace_t *)self)->m, buf, sizeof(buf));
	crt_fprintf(fp, buf);
	return 0;
}

static PyObject *MenuItemFace_GetAttr(PyObject *self, char *name) {
	nativeVal_t v;
	PyObject *r = MenuItem_GetAttr(((menuItemFace_t *)self)->m, name, &v) ? Native_ToPy(&v) : NULL;
	Native_Free(&v);
	return r;
}

static int MenuItemFace_SetAttr(PyObject *self, char *name, PyObject *v) {
	nativeVal_t x;
	if (v) {
		Native_Arg(v, &x);
	}
	byte ok = MenuItem_SetAttr(((menuItemFace_t *)self)->m, name, v ? &x : NULL);
	if (v) {
		Native_Free(&x);
	}
	return ok ? 0 : -1;
}

static PyTypeObject menu_itemFaceType = {
	.ob_refcnt = 1,
	.ob_type = &PyType_Type,
	.tp_name = "cPyMenuEntity",
	.tp_basicsize = sizeof(menuItemFace_t),
	.tp_dealloc = (void *)MenuItemFace_Dealloc,
	.tp_print = (void *)MenuItemFace_Print,
	.tp_getattr = (void *)MenuItemFace_GetAttr,
	.tp_setattr = (void *)MenuItemFace_SetAttr,
};

static const nativeCType_t menu_itemType = {
	.name = "cPyMenuEntity",
	.getattr = MenuItem_GetAttr,
	.setattr = MenuItem_SetAttr,
	.print = MenuItem_Print,
	.drop = MenuItem_Drop,
	.pytype = &menu_itemFaceType,
	.toPy = MenuItem_ToPy,
};

byte ScorerItem_New(int player, const char *name, nativeVal_t *out) {
	cScorer *scorer = Scorer_Get(player);
	if (!scorer || !cScorer_FindItem(scorer, name)) {
		Native_SetNone(out);
		return 0;
	}
	NObj_NewCObj(out, &menu_itemType, MenuItem_New(player, name));
	return 1;
}
