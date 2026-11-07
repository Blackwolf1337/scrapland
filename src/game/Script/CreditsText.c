/*
 * The CreditsText module in C (scripts/scorer/creditstext.py, issue #2): the pages of the credits. The menu level
 * preloads it when it starts with ShowCredits set (Menu.CreditsMenu), the end movie (level Fake) always. Map.py
 * calls Init, sets EndOfCredits (its next page) and calls ShowCredit(n) per page; a page fades in (ShowUp), stays
 * for a time that grows with its lines and goes (ShowDown), then EndOfCredits runs. Leaving the movie (another
 * action set than Inactive) empties the page. The item of the page is data/menus/credits.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. MAX_TEXT_LINES, TextInfo (the lines of the page), TextPos, TextFont, TextLines and ctr (the
 * corner of the next page) are mirrored from the module dict where the script reads them and written through where
 * it writes them (OBJLIB 5): creditstext.mod sets them, credits.def and the tests read them there. EndOfCredits (a
 * function), the scheduled steps and Menu.MovieScorerCall stay by name.
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

static const char credits_def[] = {
#embed "../../../data/menus/credits.def"
	, 0 };

#define CREDITS_ITEM "Credits"

static struct {
	nativeVal_t maxLines;		/* MAX_TEXT_LINES: an int or a float, else 20 */
	nativeVal_t textInfo;		/* TextInfo: the lines (a list, tuple or text) */
	nativeVal_t textPos;		/* TextPos: (x, y) */
	nativeVal_t textFont;		/* TextFont */
	nativeVal_t textLines;		/* TextLines: the count of lines with a text, a number */
	nativeVal_t ctr;		/* ctr: the corner of the next page, 0 to 3 */
} credits;

/* OBJLIB 5: the mirror in the module dict, read where the script reads each variable */
static const nativeVar_t credits_vars[] = {
	{ "MAX_TEXT_LINES", &credits.maxLines, 0, NV_ANY },
	{ "TextInfo", &credits.textInfo, 0, NV_ANY },
	{ "TextPos", &credits.textPos, 0, NV_ANY },
	{ "TextFont", &credits.textFont, 0, NV_ANY },
	{ "TextLines", &credits.textLines, 0, NV_ANY },
	{ "ctr", &credits.ctr, 0, NV_ANY },
	{ NULL, NULL, 0, 0 },
};
enum { CV_MAX, CV_INFO, CV_POS, CV_FONT, CV_LINES, CV_CTR };

/* the variable k of the table from the module: 0 with the error of the script's read (an AttributeError) */
static byte Credits_Read(int k) {
	const nativeVar_t one[] = { credits_vars[k], { NULL, NULL, 0, 0 } };
	return Native_Import("CreditsText", one);
}

/* the C value of the variable k into the module, as Script_SetVar of the script port (a failed set is no error) */
static void Credits_Put(int k) {
	if (!Native_Put("CreditsText", &credits_vars[k], 0)) {
		PyErr_Clear();
	}
}

/* the number of a variable as Script_GetInt and Script_GetFloat read it (an int or a float, else def) */
static double Credits_Num(int k, double def) {
	if (!Credits_Read(k)) {
		PyErr_Clear();
		return def;
	}
	const nativeVal_t *v = credits_vars[k].values;
	return v->type == NV_INT ? (double)v->i : v->type == NV_FLOAT ? v->f : def;
}

/* range(MAX_TEXT_LINES) (Script_GetInt: an int, a float cut, else 20) */
static int Credits_Max(void) {
	if (!Credits_Read(CV_MAX)) {
		PyErr_Clear();
		return 20;
	}
	const nativeVal_t *v = &credits.maxLines;
	return v->type == NV_INT ? (int)v->i : v->type == NV_FLOAT ? (int)v->f : 20;
}

/* `import name` of a function of the script: 0 with the error set */
static byte Credits_Import(const char *name) {
	/* OBJLIB 6: module loading stays with the module objects until then */
	PyObject *m = PyImport_ImportModule((char *)name);
	Py_XDECREF(m);
	return m != NULL;
}

/* the line i of the page: SScorer.Set(id, "Credits", "Text<i>", text) */
static byte Credits_SetLine(int id, int i, const nativeVal_t *text) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "Text%d", i);
	return Native_ScorerSet(id, CREDITS_ITEM, var, text);
}

/* the lines 0 to MAX_TEXT_LINES - 1 of the page of player id empty */
static byte Credits_Empty(int id) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	byte ok = 1;
	for (int i = 0, n = Credits_Max(); ok && i < n; i++) {
		ok = Credits_SetLine(id, i, &empty);
	}
	return ok;
}

/* CreditsScorer(id): the item of the page, its lines empty */
static byte Credits_Scorer(int id) {
	if (!Credits_Import("Scorer")) {
		return 0;
	}
	Script_RunFile(id, "menus/credits.def", "CreditsScorer", credits_def, NULL, NULL);
	return Credits_Empty(id);
}

/* Init(): the scorer of the movie gets the page item too (Menu.MovieScorerCall), and player 0 gets it now */
static byte Credits_Init(void) {
	if (!Credits_Import("Menu")) {
		return 0;
	}
	/* OBJLIB 3, 5: the module's own function goes into a variable of Menu by name */
	PyObject *f = Script_Attr("CreditsText", "CreditsScorer");
	if (!f) {
		return 0;
	}
	Script_SetVar("Menu", "MovieScorerCall", f);
	return Credits_Scorer(0);
}

/* PrintData(Vis): the lines of TextInfo on the page of player 0 */
static byte Credits_Print(void) {
	int n;
	if (!Credits_Read(CV_INFO) || !Native_Len(&credits.textInfo, &n)) {
		return 0;
	}
	byte ok = 1;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t tmp;
		const nativeVal_t *line = Native_GetItem(&credits.textInfo, i, 0, &tmp);
		ok = line && Credits_SetLine(0, i, line);
		if (line == &tmp) {
			Native_Free(&tmp);
		}
	}
	return ok;
}

/* PrintDataGuay(Vis): the lines with random characters (Vis 1 all random, 0 the text, between a mix; nothing calls
 * it). Each character draws Scrap.Rand(Vis - 1, Vis): below 0 the character, below 1 a random one, else a space;
 * spaces stay */
static byte Credits_PrintGuay(double vis) {
	static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789!$%&/=?|@#\\/*+-.";
	int n;
	if (!Credits_Read(CV_INFO) || !Native_Len(&credits.textInfo, &n)) {
		return 0;
	}
	byte ok = 1;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t tmp, txt;
		const nativeVal_t *line = Native_GetItem(&credits.textInfo, i, 0, &tmp);
		if (!line) {
			return 0;
		}
		const char *s = "";
		int len = 0;
		if (line->type == NV_STR) {
			s = Native_Text(line);
			len = line->n;
		} else {
			/* len() of the line fails first (a dict has a length too), a sequence of other things fails at
			 * txt + tinfo */
			int ln = line->type == NV_DICT ? line->n : 0;
			ok = line->type == NV_DICT || Native_Len(line, &ln);
			if (ok && ln > 0) {
				Scrap_RandRange(vis - 1, vis);
				ok = Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
			}
		}
		if (ok) {
			Native_SetText(&txt, s, len);
			char *o = txt.text ? txt.text : txt.s;
			for (int j = 0; j < len; j++) {
				double rnd = Scrap_RandRange(vis - 1, vis);
				o[j] = s[j] == ' ' || rnd < 0 ? s[j] : rnd < 1 ? chars[(int)Scrap_RandRange(0, sizeof(chars) - 1)] : ' ';
			}
			ok = Credits_SetLine(0, i, &txt);
			Native_Free(&txt);
		}
		if (line == &tmp) {
			Native_Free(&tmp);
		}
	}
	return ok;
}

/* CleanCredits(): the page goes */
static byte Credits_Clean(void) {
	Script_SetNum(0, CREDITS_ITEM, "Visible", 0);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	return Credits_Empty(0);
}

/* the movie still runs (SInput.GetActionSet() is "Inactive") */
static byte Credits_InMovie(void) {
	return !strcmp(TABLE_AT(g_ActionSets, Input_GetActionSet())->name, "Inactive");
}

/* cmp(v, n) of the script against a number: the sign for a number, every other type compares greater (numbers are
 * smaller than all other types in 1.5.2) */
static int Credits_Cmp(const nativeVal_t *v, double n) {
	double d;
	if (v->type == NV_INT) {
		d = (double)v->i;
	} else if (v->type == NV_FLOAT || v->type == NV_LONG) {
		Native_ArgDouble(v, &d);
	} else {
		return 1;
	}
	return d < n ? -1 : d > n;
}

/* the next step of ShowUp or ShowDown in 0.05 s with val + d */
static byte Credits_Step(const char *func, const nativeVal_t *val, double d) {
	nativeVal_t next;
	switch (val->type) {
	case NV_INT:
	case NV_FLOAT:
	case NV_LONG: {
		double v;
		Native_ArgDouble(val, &v);
		Native_SetFloat(&next, v + d);
		break;
	}
	case NV_NONE:
	case NV_DICT:
		return Native_Raise(NE_TYPE, "bad operand type(s) for +");
	default:
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	/* OBJLIB 4: the scheduler takes the function by name until it takes C entries */
	PyObject *n = Native_ToPy(&next);
	return n && Script_LaterIn(0.05, "CreditsText", func, Py_BuildValue("(N)", n), NULL);
}

/* ShowUp(val=0.5): the lines go on the page; at val 0 it fades in for a time by its lines, then ShowDown */
static byte Credits_ShowUp(const nativeVal_t *val) {
	if (!Credits_InMovie()) {
		return Credits_Clean();
	}
	if (!Credits_Print()) {
		return 0;
	}
	if (Credits_Cmp(val, 0) > 0) {
		return Credits_Step("ShowUp", val, -0.05);
	}
	Script_SetNum(0, CREDITS_ITEM, "Visible", 1);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	double time = 3.8 + Credits_Num(CV_LINES, 0) * 0.6;
	if (time > 6.5) {
		time = 6.75;
	}
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	Script_SetNum(0, CREDITS_ITEM, "Life", time - 1.2 - 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeIn", 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeOut", 1.2);
	return Script_LaterIn(time, "CreditsText", "ShowDown", PyTuple_New(0), NULL);
}

/* ShowDown(val=0.0): ten more steps of 0.05 s while the page fades out, then EndOfCredits() when it is set; *res
 * what EndOfCredits() gave (a new reference, NULL for None) */
static byte Credits_ShowDown(const nativeVal_t *val, PyObject **res) {
	*res = NULL;
	if (!Credits_InMovie()) {
		return Credits_Clean();
	}
	if (Credits_Cmp(val, 0.5) < 0) {
		return Credits_Step("ShowDown", val, 0.05);
	}
	/* OBJLIB 4: EndOfCredits is a function the level puts into the module (ShowDown gives what it gives) */
	PyObject *end = Script_Attr("CreditsText", "EndOfCredits");
	int t = end ? PyObject_IsTrue(end) : -1;
	*res = t > 0 ? Script_Invoke(end, PyTuple_New(0)) : NULL;
	Py_XDECREF(end);
	return t == 0 || *res != NULL;
}

/* 590 - x of the script (the right corner) */
static byte Credits_Right(const nativeVal_t *x, nativeVal_t *out) {
	double d;
	if (x->type == NV_INT) {
		Native_SetInt(out, 590 - x->i);
	} else if (x->type == NV_FLOAT || x->type == NV_LONG) {
		Native_ArgDouble(x, &d);
		Native_SetFloat(out, 590 - d);
	} else {
		return Native_Raise(NE_TYPE, "number coercion failed");
	}
	return 1;
}

/* ShowCredit(num): the page num (rep its repr: the language texts Credits_<num>_1 to _20, "-" or missing for an
 * empty line) in the next corner of the screen (ctr goes round: top left, bottom right, top right, bottom left) */
static byte Credits_Show(const char *rep) {
	char cad[0x80];
	Str_Printf(cad, sizeof(cad), "Credits_%s_", rep);
	Native_Free(&credits.textLines);
	Native_SetInt(&credits.textLines, 0);
	Credits_Put(CV_LINES);
	Native_Free(&credits.textInfo);
	Native_SetEmpty(&credits.textInfo, NV_LIST);
	Credits_Put(CV_INFO);
	int lines = 0;
	byte ok = Credits_Read(CV_FONT);
	for (int i = 0, n = ok ? Credits_Max() : 0; ok && i < n; i++) {
		char key[0xa0];
		Str_Printf(key, sizeof(key), "%s%d", cad, i + 1);
		/* Scrap.GetLangStr(key) */
		const char *txt = Script_Lang(key);
		if (*txt && strcmp(txt, "-")) {
			/* SScorer.GetTextArea(TextFont, txt): the script measures the line for a width it never uses */
			const char *font;
			ok = Native_ArgStr(&credits.textFont, 1, &font);
			if (ok) {
				float rect[2], size[2];
				Font_TextSize(Font_Get(font), rect, size, 0.0f, 0.0f, "%s", txt);
				Native_SetText(Native_Append(&credits.textInfo), txt, (int)strlen(txt));
				Native_SetInt(&credits.textLines, ++lines);
				Credits_Put(CV_LINES);
			}
		} else {
			Native_SetStr(Native_Append(&credits.textInfo), "");
		}
	}
	/* the lines so far, as the script's list has them */
	Credits_Put(CV_INFO);
	nativeVal_t sizex = { 0 };
	if (!ok || !Native_GVarGet("R_CreditsSizeX", &sizex) || !Credits_Read(CV_CTR)) {
		Native_Free(&sizex);
		return 0;
	}
	int corner = -1;
	for (int k = 0; k < 4 && corner < 0; k++) {
		if (Credits_Cmp(&credits.ctr, k) == 0) {
			corner = k;
		}
	}
	nativeVal_t right = { 0 };
	ok = !(corner == 1 || corner == 2) || Credits_Right(&sizex, &right);
	Native_Free(&sizex);
	if (corner >= 0) {
		if (!ok) {
			return 0;
		}
		static const int y0[4] = { 80, 300, 80, 300 }, dy[4] = { 0, 25, 0, 23 };
		Native_Free(&credits.textPos);
		Native_SetEmpty(&credits.textPos, NV_TUPLE);
		nativeVal_t *x = Native_Append(&credits.textPos);
		if (corner == 1 || corner == 2) {
			*x = right;
		} else {
			Native_SetInt(x, 50);
		}
		Native_SetInt(Native_Append(&credits.textPos), y0[corner] - lines * dy[corner]);
		Credits_Put(CV_POS);
		Native_Free(&credits.ctr);
		Native_SetInt(&credits.ctr, (corner + 1) % 4);
		Credits_Put(CV_CTR);
	}
	/* Scrap.Set("R_CreditsX", TextPos[0]), Scrap.Set("R_CreditsY", TextPos[1]) */
	if (!Credits_Read(CV_POS)) {
		return 0;
	}
	static const char *const gvars[2] = { "R_CreditsX", "R_CreditsY" };
	for (int i = 0; i < 2; i++) {
		nativeVal_t tmp;
		const nativeVal_t *v = Native_GetItem(&credits.textPos, i, 0, &tmp);
		ok = v && Native_GVarSet(gvars[i], v);
		if (v == &tmp) {
			Native_Free(&tmp);
		}
		if (!ok) {
			return 0;
		}
	}
	nativeVal_t half;
	Native_SetFloat(&half, 0.5);
	return Credits_ShowUp(&half);
}

/* ---- the adapters of the module table (OBJLIB 7: arguments and the errors of the Python calls) ---- */

static PyObject *Credits_Result(byte ok) {
	return ok ? Py_ReturnNone() : NULL;
}

static PyObject *CreditsText_CreditsScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	return PyArg_ParseTuple(args, "i", &id) ? Credits_Result(Credits_Scorer(id)) : NULL;
}

static PyObject *CreditsText_Init(PyObject *self, PyObject *args) {
	(void)self;
	return PyArg_ParseTuple(args, "") ? Credits_Result(Credits_Init()) : NULL;
}

static PyObject *CreditsText_PrintData(PyObject *self, PyObject *args) {
	PyObject *vis;
	(void)self;
	return PyArg_ParseTuple(args, "O", &vis) ? Credits_Result(Credits_Print()) : NULL;
}

static PyObject *CreditsText_PrintDataGuay(PyObject *self, PyObject *args) {
	double vis;
	(void)self;
	return PyArg_ParseTuple(args, "d", &vis) ? Credits_Result(Credits_PrintGuay(vis)) : NULL;
}

static PyObject *CreditsText_CleanCredits(PyObject *self, PyObject *args) {
	(void)self;
	return PyArg_ParseTuple(args, "") ? Credits_Result(Credits_Clean()) : NULL;
}

/* the optional argument val of ShowUp and ShowDown (def when it is not given): 0 with the error set */
static byte Credits_Val(PyObject *args, double def, nativeVal_t *val) {
	PyObject *o = NULL;
	if (!PyArg_ParseTuple(args, "|O", &o)) {
		return 0;
	}
	if (!o) {
		Native_SetFloat(val, def);
		return 1;
	}
	return Native_Arg(o, val);
}

static PyObject *CreditsText_ShowUp(PyObject *self, PyObject *args) {
	nativeVal_t val;
	(void)self;
	if (!Credits_Val(args, 0.5, &val)) {
		return NULL;
	}
	byte ok = Credits_ShowUp(&val);
	Native_Free(&val);
	return Credits_Result(ok);
}

static PyObject *CreditsText_ShowDown(PyObject *self, PyObject *args) {
	nativeVal_t val;
	PyObject *res;
	(void)self;
	if (!Credits_Val(args, 0.0, &val)) {
		return NULL;
	}
	byte ok = Credits_ShowDown(&val, &res);
	Native_Free(&val);
	return !ok ? NULL : res ? res : Py_ReturnNone();
}

static PyObject *CreditsText_ShowCredit(PyObject *self, PyObject *args) {
	PyObject *num;
	nativeVal_t n;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &num) || !Credits_Import("Scorer")) {
		return NULL;
	}
	/* `num` */
	nativeBuf_t rep;
	Native_BufInit(&rep);
	if (Native_FromPyAny(num, &n)) {
		Native_Repr(&rep, &n, NR_REPR);
		Native_Free(&n);
	} else {
		/* OBJLIB 7: the repr of a value without a native form */
		PyObject *r = PyObject_Repr(num);
		if (!r) {
			return NULL;
		}
		Native_BufAdd(&rep, PyString_AsString(r), PyString_Size(r));
		Py_DECREF(r);
	}
	byte ok = Credits_Show(rep.p);
	Native_BufFree(&rep);
	return Credits_Result(ok);
}

PyMethodDef g_CreditsTextMethods[] = {
	{ "Init", CreditsText_Init, 1, NULL },
	{ "CreditsScorer", CreditsText_CreditsScorer, 1, NULL },
	{ "PrintDataGuay", CreditsText_PrintDataGuay, 1, NULL },
	{ "PrintData", CreditsText_PrintData, 1, NULL },
	{ "ShowUp", CreditsText_ShowUp, 1, NULL },
	{ "ShowDown", CreditsText_ShowDown, 1, NULL },
	{ "CleanCredits", CreditsText_CleanCredits, 1, NULL },
	{ "ShowCredit", CreditsText_ShowCredit, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
