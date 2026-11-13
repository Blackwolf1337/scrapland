/*
 * The script handle of a scorer item (HUD and menu element) by player and name, the type cPyMenuEntity of the exe (a
 * native C object here, NObj_NewCObj). The item is looked up again on every access. Its
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
		/* modsupport.c BuildValue("s"): None for no text */
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
 * tp_setattr: the value is tried as int, float and string (getargs.c Arg_Parse "i", "d", "s"), each type that parses
 * goes to the item until the variable has that type. A missing scorer or item returns 0 (no error) like in the
 * original. The original passes the other two values uninitialized; they are 0 here. A delete (v NULL) parses as none
 * of them.
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

static const nativeCType_t menu_itemType = {
	.name = "cPyMenuEntity",
	.getattr = MenuItem_GetAttr,
	.setattr = MenuItem_SetAttr,
	.print = MenuItem_Print,
	.drop = MenuItem_Drop,
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
