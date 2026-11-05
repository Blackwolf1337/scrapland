/*
 * The CreditsText module in C (scripts/scorer/creditstext.py, issue #2): the pages of the credits. The menu level
 * preloads it when it starts with ShowCredits set (Menu.CreditsMenu), the end movie (level Fake) always. Map.py
 * calls Init, sets EndOfCredits (its next page) and calls ShowCredit(n) per page; a page fades in (ShowUp), stays
 * for a time that grows with its lines and goes (ShowDown), then EndOfCredits runs. Leaving the movie (another
 * action set than Inactive) empties the page. The item of the page is data/menus/credits.def.
 *
 * MAX_TEXT_LINES, TextInfo (the lines of the page), TextPos, TextFont, TextLines, EndOfCredits and ctr (the corner of
 * the next page) stay variables of the module.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

static const char credits_def[] = {
#embed "../../../data/menus/credits.def"
	, 0 };

#define CREDITS_ITEM "Credits"

/* range(MAX_TEXT_LINES) */
static int Credits_Max(void) {
	return Script_GetInt("CreditsText", "MAX_TEXT_LINES", 20);
}

/* the line i of the page: SScorer.Set(id, "Credits", "Text<i>", text) */
static byte Credits_SetLine(int id, int i, PyObject *text) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "Text%d", i);
	return Script_Try("SScorer", "Set", "(issO)", id, CREDITS_ITEM, var, text);
}

/* CreditsScorer(id): the item of the page, its lines empty */
static byte Credits_Scorer(int id) {
	PyObject *scorer = PyImport_ImportModule("Scorer");
	if (!scorer) {
		return 0;
	}
	Py_DECREF(scorer);
	Script_RunFile(id, "menus/credits.def", "CreditsScorer", credits_def, NULL, NULL);
	PyObject *empty = PyString_FromString("");
	byte ok = empty != NULL;
	for (int i = 0, n = Credits_Max(); ok && i < n; i++) {
		ok = Credits_SetLine(id, i, empty);
	}
	Py_XDECREF(empty);
	return ok;
}

static PyObject *CreditsText_CreditsScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	return Credits_Scorer(id) ? Py_ReturnNone() : NULL;
}

/* Init(): the scorer of the movie gets the page item too (Menu.MovieScorerCall), and player 0 gets it now */
static PyObject *CreditsText_Init(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *menu = PyImport_ImportModule("Menu");
	PyObject *f = menu ? Script_Attr("CreditsText", "CreditsScorer") : NULL;
	Py_XDECREF(menu);
	if (!f) {
		return NULL;
	}
	Script_SetVar("Menu", "MovieScorerCall", f);
	return Credits_Scorer(0) ? Py_ReturnNone() : NULL;
}

/* PrintData(Vis): the lines of TextInfo on the page of player 0 */
static byte Credits_Print(void) {
	PyObject *info = Script_Attr("CreditsText", "TextInfo");
	int n = info ? PySequence_Length(info) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		PyObject *line = PySequence_GetItem(info, i);
		ok = line && Credits_SetLine(0, i, line);
		Py_XDECREF(line);
	}
	Py_XDECREF(info);
	return ok;
}

static PyObject *CreditsText_PrintData(PyObject *self, PyObject *args) {
	PyObject *vis;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &vis)) {
		return NULL;
	}
	return Credits_Print() ? Py_ReturnNone() : NULL;
}

/* PrintDataGuay(Vis): the lines with random characters (Vis 1 all random, 0 the text, between a mix; nothing calls
 * it). Each character draws Scrap.Rand(Vis - 1, Vis): below 0 the character, below 1 a random one, else a space;
 * spaces stay */
static PyObject *CreditsText_PrintDataGuay(PyObject *self, PyObject *args) {
	static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789!$%&/=?|@#\\/*+-.";
	double vis;
	(void)self;
	if (!PyArg_ParseTuple(args, "d", &vis)) {
		return NULL;
	}
	PyObject *info = Script_Attr("CreditsText", "TextInfo");
	int n = info ? PySequence_Length(info) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		PyObject *line = PySequence_GetItem(info, i);
		if (line && !PyString_Check(line)) {
			/* len() of the line fails first, a sequence of other things fails at txt + tinfo */
			int len = PyObject_Length(line);
			if (len > 0) {
				Scrap_RandRange(vis - 1, vis);
				PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
			}
			Py_DECREF(line);
			line = len ? NULL : PyString_FromString("");
		}
		int len = line ? PyString_Size(line) : 0;
		PyObject *txt = line ? PyString_FromStringAndSize(NULL, len) : NULL;
		if (txt) {
			const char *s = PyString_AsString(line);
			char *o = PyString_AsString(txt);
			for (int j = 0; j < len; j++) {
				double rnd = Scrap_RandRange(vis - 1, vis);
				o[j] = s[j] == ' ' || rnd < 0 ? s[j] : rnd < 1 ? chars[(int)Scrap_RandRange(0, sizeof(chars) - 1)] : ' ';
			}
		}
		ok = txt && Credits_SetLine(0, i, txt);
		Py_XDECREF(txt);
		Py_XDECREF(line);
	}
	Py_XDECREF(info);
	return ok ? Py_ReturnNone() : NULL;
}

/* CleanCredits(): the page goes */
static byte Credits_Clean(void) {
	Script_SetNum(0, CREDITS_ITEM, "Visible", 0);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	PyObject *empty = PyString_FromString("");
	byte ok = empty != NULL;
	for (int i = 0, n = Credits_Max(); ok && i < n; i++) {
		ok = Credits_SetLine(0, i, empty);
	}
	Py_XDECREF(empty);
	return ok;
}

static PyObject *CreditsText_CleanCredits(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Credits_Clean() ? Py_ReturnNone() : NULL;
}

/* the movie still runs (SInput.GetActionSet() is "Inactive"): 1, 0, or -1 with the error set */
static int Credits_InMovie(void) {
	PyObject *set = Script_Ask("SInput", "GetActionSet", "()");
	if (!set) {
		return -1;
	}
	int in = PyString_Check(set) && !strcmp(PyString_AsString(set), "Inactive");
	Py_DECREF(set);
	return in;
}

/* the next step of ShowUp or ShowDown in 0.05 s with val + d */
static byte Credits_Step(const char *func, PyObject *val, double d) {
	PyObject *step = PyFloat_FromDouble(d);
	PyObject *next = step ? PyNumber_Add(val, step) : NULL;
	Py_XDECREF(step);
	if (!next) {
		return 0;
	}
	PyObject *a = Py_BuildValue("(O)", next);
	Py_DECREF(next);
	return Script_LaterIn(0.05, "CreditsText", func, a, NULL);
}

/* ShowUp(val=0.5): the lines go on the page; at val 0 it fades in for a time by its lines, then ShowDown */
static PyObject *Credits_ShowUp(PyObject *val) {
	int in = Credits_InMovie();
	if (in <= 0) {
		return in < 0 || !Credits_Clean() ? NULL : Py_ReturnNone();
	}
	if (!Credits_Print()) {
		return NULL;
	}
	int c = Script_NumCmp(val, 0);
	if (c == -2) {
		return NULL;
	}
	if (c > 0) {
		return Credits_Step("ShowUp", val, -0.05) ? Py_ReturnNone() : NULL;
	}
	Script_SetNum(0, CREDITS_ITEM, "Visible", 1);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	double time = 3.8 + Script_GetFloat("CreditsText", "TextLines", 0) * 0.6;
	if (time > 6.5) {
		time = 6.75;
	}
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	Script_SetNum(0, CREDITS_ITEM, "Life", time - 1.2 - 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeIn", 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeOut", 1.2);
	return Script_LaterIn(time, "CreditsText", "ShowDown", PyTuple_New(0), NULL) ? Py_ReturnNone() : NULL;
}

static PyObject *CreditsText_ShowUp(PyObject *self, PyObject *args) {
	PyObject *val = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &val)) {
		return NULL;
	}
	PyObject *half = val ? NULL : PyFloat_FromDouble(0.5);
	PyObject *r = val || half ? Credits_ShowUp(val ? val : half) : NULL;
	Py_XDECREF(half);
	return r;
}

/* ShowDown(val=0.0): ten more steps of 0.05 s while the page fades out, then EndOfCredits() when it is set */
static PyObject *CreditsText_ShowDown(PyObject *self, PyObject *args) {
	PyObject *val = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &val)) {
		return NULL;
	}
	int in = Credits_InMovie();
	if (in <= 0) {
		return in < 0 || !Credits_Clean() ? NULL : Py_ReturnNone();
	}
	PyObject *zero = val ? NULL : PyFloat_FromDouble(0.0), *half = PyFloat_FromDouble(0.5);
	if (!val) {
		val = zero;
	}
	int c = val && half ? PyObject_Compare(val, half) : 0;
	PyObject *r = NULL;
	if (val && half && !PyErr_Occurred()) {
		if (c < 0) {
			r = Credits_Step("ShowDown", val, 0.05) ? Py_ReturnNone() : NULL;
		} else {
			PyObject *end = Script_Attr("CreditsText", "EndOfCredits");
			int t = end ? PyObject_IsTrue(end) : -1;
			r = t > 0 ? Script_Invoke(end, PyTuple_New(0)) : t == 0 ? Py_ReturnNone() : NULL;
			Py_XDECREF(end);
		}
	}
	Py_XDECREF(zero);
	Py_XDECREF(half);
	return r;
}

/* ShowCredit(num): the page num (the language texts Credits_<num>_1 to _20, "-" or missing for an empty line) in the
 * next corner of the screen (ctr goes round: top left, bottom right, top right, bottom left) */
static PyObject *CreditsText_ShowCredit(PyObject *self, PyObject *args) {
	PyObject *num;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &num)) {
		return NULL;
	}
	PyObject *scorer = PyImport_ImportModule("Scorer");
	PyObject *rep = scorer ? PyObject_Repr(num) : NULL;
	Py_XDECREF(scorer);
	if (!rep) {
		return NULL;
	}
	char cad[0x80];
	Str_Printf(cad, sizeof(cad), "Credits_%s_", PyString_AsString(rep));
	Py_DECREF(rep);
	Script_SetInt("CreditsText", "TextLines", 0);
	PyObject *info = PyList_New(0);
	if (!info) {
		return NULL;
	}
	Py_INCREF(info);
	Script_SetVar("CreditsText", "TextInfo", info);
	PyObject *font = Script_Attr("CreditsText", "TextFont");
	int lines = 0;
	byte ok = font != NULL;
	for (int i = 0, n = Credits_Max(); ok && i < n; i++) {
		char key[0xa0];
		Str_Printf(key, sizeof(key), "%s%d", cad, i + 1);
		PyObject *txt = Script_Ask("Scrap", "GetLangStr", "(s)", key);
		int t = txt ? PyObject_IsTrue(txt) : -1;
		ok = t >= 0;
		if (t > 0 && !(PyString_Check(txt) && !strcmp(PyString_AsString(txt), "-"))) {
			/* the script measures the line for a width it never uses */
			PyObject *area = Script_Ask("SScorer", "GetTextArea", "(OO)", font, txt);
			PyObject *siz = area ? PySequence_GetItem(area, 0) : NULL;
			ok = siz && PyList_Append(info, txt) == 0;
			if (ok) {
				Script_SetInt("CreditsText", "TextLines", ++lines);
			}
			Py_XDECREF(siz);
			Py_XDECREF(area);
		} else if (ok) {
			PyObject *empty = PyString_FromString("");
			ok = empty && PyList_Append(info, empty) == 0;
			Py_XDECREF(empty);
		}
		Py_XDECREF(txt);
	}
	Py_XDECREF(font);
	Py_DECREF(info);
	PyObject *sizex = ok ? Script_Ask("Scrap", "Get", "(s)", "R_CreditsSizeX") : NULL;
	PyObject *ctr = sizex ? Script_Attr("CreditsText", "ctr") : NULL;
	if (!ctr) {
		Py_XDECREF(sizex);
		return NULL;
	}
	PyObject *pos = NULL, *right = NULL, *c590 = PyInt_FromLong(590);
	int corner = -1;
	for (int k = 0; k < 4 && corner < 0; k++) {
		if (Script_NumCmp(ctr, k) == 0) {
			corner = k;
		}
	}
	Py_DECREF(ctr);
	if ((corner == 1 || corner == 2) && c590) {
		right = PyNumber_Subtract(c590, sizex);
	}
	switch (corner) {
	case 0:
		pos = Py_BuildValue("(ii)", 50, 80);
		break;
	case 1:
		pos = right ? Py_BuildValue("(Oi)", right, 300 - lines * 25) : NULL;
		break;
	case 2:
		pos = right ? Py_BuildValue("(Oi)", right, 80) : NULL;
		break;
	case 3:
		pos = Py_BuildValue("(ii)", 50, 300 - lines * 23);
		break;
	}
	Py_XDECREF(right);
	Py_XDECREF(c590);
	Py_DECREF(sizex);
	if (corner >= 0) {
		if (!pos) {
			return NULL;
		}
		Script_SetVar("CreditsText", "TextPos", pos);
		Script_SetInt("CreditsText", "ctr", (corner + 1) % 4);
	}
	pos = Script_Attr("CreditsText", "TextPos");
	PyObject *x = pos ? PySequence_GetItem(pos, 0) : NULL;
	ok = x && Script_Try("Scrap", "Set", "(sO)", "R_CreditsX", x);
	Py_XDECREF(x);
	PyObject *y = ok ? PySequence_GetItem(pos, 1) : NULL;
	ok = y && Script_Try("Scrap", "Set", "(sO)", "R_CreditsY", y);
	Py_XDECREF(y);
	Py_XDECREF(pos);
	if (!ok) {
		return NULL;
	}
	PyObject *half = PyFloat_FromDouble(0.5);
	PyObject *r = half ? Credits_ShowUp(half) : NULL;
	Py_XDECREF(half);
	return r;
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
