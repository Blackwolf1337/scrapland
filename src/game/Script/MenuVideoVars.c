/*
 * The helpers of scripts/scorer/pcmenu.py that work on the module variables of the video menu of the script
 * (YesValue, NoValue, TextureFilterList, TextureDetailList, ShadowsDetailList, VideoModes, SelectedVideoMode, the
 * Last_ values), the two slider formulas and DrawLine. The video menu of ScrapC (MenuVideo.c) keeps its own state and
 * calls none of them. They are here so that PCMenu, made in C from data/modules/pcmenu.mod, has every function of
 * the script; each does what the script does, line by line on the module variables (a missing one is a NameError).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters. The
 * module variables are read as globals where the script reads them and written at once (OBJLIB 5). The scorer and
 * engine calls the script made by name go through their seams (SScorer.Get, SScorer.Set, SScorer.Add, Scrap.Get,
 * Scrap.Set; OBJLIB 7); the video modes, the language texts and string.find are C. GammaFormula is called with its
 * keyword as a Python function (OBJLIB 3).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define PM "PCMenu"

/* SScorer.Get(id, item, var) == YesValue (SScorer.Get first, as the line of the script): 1, 0, -1 with the error */
static int Vv_IsYes(int id, const char *item) {
	nativeVal_t t = { 0 }, yes = { 0 }, idv, it;
	Native_SetInt(&idv, id);
	Native_SetStr(&it, item);
	int r = Mode_ScorerGetVal(&idv, &it, "Text", &t) && Mode_G(PM, "YesValue", &yes) ? Mode_Cmp(&t, "==", &yes) : -1;
	Native_Free(&t);
	Native_Free(&yes);
	return r;
}

/* Scrap.Get(var) == i: 1, 0, -1 with the error */
static int Vv_GetIs(const char *var, long i) {
	nativeVal_t v = { 0 }, name;
	Native_SetStr(&name, var);
	int r = Mode_GVarGetV(&name, &v) ? Mode_CmpI(&v, "==", i) : -1;
	Native_Free(&v);
	return r;
}

/* SScorer.Set(id, item, "Text", list[i]) with list a module variable */
static byte Vv_SetFrom(int id, const char *item, const char *list, long i) {
	nativeVal_t l = { 0 }, v = { 0 }, k;
	Native_SetInt(&k, i);
	byte ok = Mode_G(PM, list, &l) && Mode_Sub(&l, &k, &v) && Mode_ScorerPut(id, item, "Text", &v);
	Native_Free(&l);
	Native_Free(&v);
	return ok;
}

/* SScorer.Set(id, item, "Text", YesValue if yes else NoValue); yes -1 is a failed condition (0) */
static byte Vv_SetYes(int id, const char *item, int yes) {
	nativeVal_t v = { 0 };
	byte ok = yes >= 0 && Mode_G(PM, yes ? "YesValue" : "NoValue", &v) && Mode_ScorerPut(id, item, "Text", &v);
	Native_Free(&v);
	return ok;
}

/* the adapters of the functions (id, control) with control any value */
#define VV_FN(func, call) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		int id; \
		PyObject *control; \
		nativeVal_t c; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "iO", &id, &control)) { \
			return NULL; \
		} \
		Native_Arg(control, &c); \
		byte ok = call; \
		Native_Free(&c); \
		return Mode_Result(ok); \
	}

/* Apply_SetFog(id, control): R_FogEnable from the Fog item */
static byte Vv_ApplyFog(int id) {
	int yes = Vv_IsYes(id, "Fog");
	return yes >= 0 && Mode_GVarSetI("R_FogEnable", yes);
}

/* Apply_SetTextureFilter(id, control): the filter variables of the TextureFilter item (TextureFilterList) */
static byte Vv_ApplyFilter(int id) {
	static const long values[6][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 2, 1, 2 }, { 2, 1, 4 }, { 2, 1, 8 } };
	nativeVal_t tex = { 0 }, idv, item;
	Native_SetInt(&idv, id);
	Native_SetStr(&item, "TextureFilter");
	byte ok = Mode_ScorerGetVal(&idv, &item, "Text", &tex);
	for (int i = 0; ok && i < 6; i++) {
		nativeVal_t list = { 0 }, v = { 0 }, k;
		Native_SetInt(&k, i);
		int eq = Mode_G(PM, "TextureFilterList", &list) && Mode_Sub(&list, &k, &v) ? Mode_Cmp(&tex, "==", &v) : -1;
		Native_Free(&list);
		Native_Free(&v);
		if (eq) {
			ok = eq > 0 && Mode_GVarSetI("R_TexFilter", values[i][0]) && Mode_GVarSetI("R_MipFilter", values[i][1]) &&
				Mode_GVarSetI("R_Anisotropy", values[i][2]);
			break;
		}
	}
	Native_Free(&tex);
	return ok;
}

/* Apply_SetVideoLock(id, control): the lock mode of the Lock item */
static byte Vv_ApplyLock(int id) {
	int yes = Vv_IsYes(id, "Lock");
	return yes >= 0 && Mode_GVarSetI("R_VideoLockMode", yes ? 1 : 2) && Mode_GVarSetI("R_VideoLockBuf", 1);
}

/* ChangeBoolean(id, control): the item text flips between YesValue and NoValue */
static byte Vv_ChangeBoolean(int id, const nativeVal_t *control) {
	nativeVal_t t = { 0 }, yes = { 0 }, v = { 0 }, idv;
	Native_SetInt(&idv, id);
	int y = Mode_ScorerGetVal(&idv, control, "Text", &t) && Mode_G(PM, "YesValue", &yes) ? Mode_Cmp(&t, "==", &yes) : -1;
	byte ok = y >= 0 && Mode_G(PM, y ? "NoValue" : "YesValue", &v) && Mode_ScorerSet(&idv, control, "Text", &v);
	Native_Free(&t);
	Native_Free(&yes);
	Native_Free(&v);
	return ok;
}

VV_FN(PCMenu_Apply_SetActive, ((void)c, Mode_ApplyFlash(id)))
VV_FN(PCMenu_Apply_SetFog, ((void)c, Vv_ApplyFog(id)))
VV_FN(PCMenu_Apply_SetTextureFilter, ((void)c, Vv_ApplyFilter(id)))
VV_FN(PCMenu_Apply_SetVideoLock, ((void)c, Vv_ApplyLock(id)))
VV_FN(PCMenu_ChangeBoolean, Vv_ChangeBoolean(id, &c))

/* DrawLine(id, name, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32): a sprite as a
 * coloured rectangle, added when the item is not there (v: the eight values) */
static byte Vv_DrawLine(const nativeVal_t *id, const nativeVal_t *name, const nativeVal_t *v) {
	static const char *const vars[8] = { "X", "Y", "ScaleX", "ScaleY", "Alpha", "Red", "Green", "Blue" };
	nativeVal_t vis = { 0 }, none, one;
	Native_SetNone(&none);
	Native_SetInt(&one, 1);
	int add = Mode_ScorerGetVal(id, name, "Visible", &vis) ? Mode_Cmp(&vis, "==", &none) : -1;
	Native_Free(&vis);
	if (add < 0 || (add && !Mode_ScorerAddV(id, name, "Sprite"))) {
		return 0;
	}
	for (int i = 0; i < 8; i++) {
		if (!Mode_ScorerSet(id, name, vars[i], &v[i]) || (i == 3 && !Mode_ScorerSet(id, name, "Unit", &one))) {
			return 0;
		}
	}
	return Mode_ScorerSet(id, name, "Visible", &one);
}

static PyObject *PCMenu_DrawLine(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "id", "name", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static const long defs[8] = { 64, 64, 512, 352, 92, 0, 0, 32 };
	PyObject *o[10] = { NULL };
	nativeVal_t v[10];
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", kwlist, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6],
		&o[7], &o[8], &o[9])) {
		return NULL;
	}
	for (int i = 0; i < 10; i++) {
		if (o[i]) {
			Native_Arg(o[i], &v[i]);
		} else {
			Native_SetInt(&v[i], defs[i - 2]);
		}
	}
	byte ok = Vv_DrawLine(&v[0], &v[1], &v[2]);
	for (int i = 0; i < 10; i++) {
		Native_Free(&v[i]);
	}
	return Mode_Result(ok);
}

/* `global name; name = []` then name.append(prefix + Scrap.GetLangStr(key)) per key (the key itself when lang is 0,
 * no prefix when prefix is NULL) */
static byte Vv_Fill(const char *name, const char *const *keys, const char *const *prefix, int n, byte lang) {
	nativeVal_t empty;
	Native_SetEmpty(&empty, NV_LIST);
	if (!Mode_Put(PM, name, NULL, &empty)) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t v = { 0 }, pre, item = { 0 };
		if (lang) {
			Mode_Lang(keys[i], &v);
		} else {
			Native_SetStr(&v, keys[i]);
		}
		byte ok = prefix ? (Native_SetStr(&pre, prefix[i]), Native_Arith('+', &pre, &v, &item)) : (Native_Copy(&item, &v), 1);
		ok = ok && Mode_Append(PM, name, &item);
		Native_Free(&v);
		Native_Free(&item);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

static const char *const vv_levels[] = { "Menu_Question_High", "Menu_Question_Medium", "Menu_Question_Low",
	"Menu_Question_No" };

/* FillShadowsDetail(): ShadowsDetailList, high, medium, low with the Yes mark, no with the No mark */
static PyObject *PCMenu_FillShadowsDetail(PyObject *self, PyObject *args) {
	static const char *const marks[] = { "\2 ", "\2 ", "\2 ", "\3 " };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Mode_Result(Vv_Fill("ShadowsDetailList", vv_levels, marks, 4, 1));
}

/* FillTextureDetail(): TextureDetailList, high, medium, low */
static PyObject *PCMenu_FillTextureDetail(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Mode_Result(Vv_Fill("TextureDetailList", vv_levels, NULL, 3, 1));
}

/* FillTextureFilter(): TextureFilterList, the names of the filters */
static PyObject *PCMenu_FillTextureFilter(PyObject *self, PyObject *args) {
	static const char *const filters[] = { "Point", "Bilinear", "Trilinear", "Anisotropic x2", "Anisotropic x4",
		"Anisotropic x8" };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Mode_Result(Vv_Fill("TextureFilterList", filters, NULL, 6, 0));
}

/* int(cad[lo:hi]) of the script into out */
static byte Vv_Number(const nativeVal_t *cad, long lo, long hi, nativeVal_t *out) {
	nativeVal_t s = { 0 };
	byte ok = Native_Slice(cad, lo, hi, &s) && Native_ToInt(&s, out);
	Native_Free(&s);
	return ok;
}

/* string.find(cad, sub) of an engine text (no 0 bytes): the first index or -1 */
static long Vv_Find(const nativeVal_t *cad, const char *sub) {
	const char *t = Native_Text(cad), *at = strstr(t, sub);
	return at ? (long)(at - t) : -1;
}

/* Mode[0] == SelectedVideoMode or (bpp == 32 and (width >= 640 and height >= 480) and (width >= height)) for one
 * [number, text] of VideoModes: 1, 0, -1 with the error */
static int Vv_ModeValid(const nativeVal_t *mode) {
	nativeVal_t sel = { 0 }, w = { 0 }, h = { 0 }, bpp = { 0 };
	const nativeVal_t *cad = &mode->items[1];
	long i1 = Vv_Find(cad, "x"), i2 = Vv_Find(cad, " ("), i3 = Vv_Find(cad, " bpp)");
	int ok = -1;
	if (Vv_Number(cad, 0, i1, &w) && Vv_Number(cad, i1 + 1, i2, &h) && Vv_Number(cad, i2 + 2, i3, &bpp) &&
		Mode_G(PM, "SelectedVideoMode", &sel)) {
		ok = Mode_Cmp(&mode->items[0], "==", &sel);
		if (ok == 0) {
			ok = Mode_CmpI(&bpp, "==", 32);
			ok = ok > 0 ? Mode_CmpI(&w, ">=", 640) : ok;
			ok = ok > 0 ? Mode_CmpI(&h, ">=", 480) : ok;
			ok = ok > 0 ? Mode_Cmp(&w, ">=", &h) : ok;
		}
	}
	Native_Free(&sel);
	Native_Free(&w);
	Native_Free(&h);
	Native_Free(&bpp);
	return ok;
}

/*
 * FillVideoModes(): VideoModes ([number, text] of every mode), CurrentVideoMode, SelectedVideoMode and
 * VideoModesValid (the current mode and the 32 bpp modes of 640x480 and more that are not higher than wide; all of
 * them when that leaves one or none)
 */
static byte Vv_FillVideoModes(void) {
	nativeVal_t cur, sel = { 0 }, modes, valid;
	char buf[256];
	/* OBJLIB 6: import string */
	if (!Mode_Import("string")) {
		return 0;
	}
	Native_SetInt(&cur, Video_GetCurrentMode());
	if (!Mode_Put(PM, "CurrentVideoMode", NULL, &cur) || !Mode_G(PM, "CurrentVideoMode", &sel) ||
		!Mode_Put(PM, "SelectedVideoMode", NULL, &sel)) {
		Native_Free(&sel);
		return 0;
	}
	Native_Free(&sel);
	int num = Video_GetModes(-1, buf);
	Native_SetEmpty(&modes, NV_LIST);
	for (int i = 0; i < num; i++) {
		nativeVal_t *m = Native_Append(&modes);
		Video_GetModes(i, buf);
		Native_SetEmpty(m, NV_LIST);
		Native_SetInt(Native_Append(m), i);
		Native_SetStr(Native_Append(m), buf);
	}
	Native_SetEmpty(&valid, NV_LIST);
	byte ok = Mode_Put(PM, "VideoModes", NULL, &modes);
	for (int i = 0; ok && i < modes.n; i++) {
		int v = Vv_ModeValid(&modes.items[i]);
		if (v > 0) {
			Native_Copy(Native_Append(&valid), &modes.items[i]);
		}
		ok = v >= 0;
	}
	ok = ok && Mode_Put(PM, "VideoModesValid", NULL, &valid);
	if (ok && valid.n <= 1) {
		ok = Mode_Put(PM, "VideoModesValid", NULL, &modes);
	}
	Native_Free(&modes);
	Native_Free(&valid);
	return ok;
}

static PyObject *PCMenu_FillVideoModes(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Mode_Result(Vv_FillVideoModes());
}

/*
 * The slider formulas: GammaFormula(value=-1, gamma=-1) gives the gamma of a slider value (value >= 0) or the value
 * of a gamma; MouseSensitivityFormula(value=-1, sens=-1) the same for the mouse; None when both are negative. The
 * arithmetic is that of the script on the given values (an int divides as an int). a: value and the other, out the
 * result; 0 with the error
 */
static byte Vv_Arith(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	return Mode_Arith(op, a, b, out);
}

static byte Vv_Gamma(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t zero, half, one, four, x = { 0 }, y = { 0 };
	Native_SetInt(&zero, 0);
	Native_SetFloat(&half, 0.5);
	Native_SetInt(&one, 1);
	Native_SetInt(&four, 4);
	byte ok = 0;
	int c = Mode_Cmp(&a[0], ">=", &zero);
	if (c > 0) {
		/* gamma = 1.0, then 0.5 + value below 0.5, else 1 + (value - 0.5) * 4 */
		c = Mode_Cmp(&a[0], "<", &half);
		ok = c > 0 ? Vv_Arith('+', &half, &a[0], out) : c == 0 ? Vv_Arith('-', &a[0], &half, &x) &&
			Vv_Arith('*', &x, &four, &y) && Vv_Arith('+', &one, &y, out) : 0;
	} else if (c == 0 && (c = Mode_Cmp(&a[1], ">=", &zero)) > 0) {
		c = Mode_Cmp(&a[1], "<=", &one);
		ok = c > 0 ? Vv_Arith('-', &a[1], &half, out) : c == 0 ? Vv_Arith('-', &a[1], &one, &x) &&
			Vv_Arith('/', &x, &four, &y) && Vv_Arith('+', &half, &y, out) : 0;
	} else {
		ok = c >= 0;
	}
	Native_Free(&x);
	Native_Free(&y);
	return ok;
}

static byte Vv_Sensitivity(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t zero, five, one, p2, f625, f16, x = { 0 }, y = { 0 };
	Native_SetInt(&zero, 0);
	Native_SetInt(&five, 5);
	Native_SetInt(&one, 1);
	Native_SetFloat(&p2, 0.2);
	Native_SetFloat(&f625, 6.25);
	Native_SetFloat(&f16, 1.6);
	byte ok = 0;
	int c = Mode_Cmp(&a[0], ">=", &zero);
	if (c > 0) {
		/* sens = 1.0, then 0.2 + value / 6.25 up to 5, else 1 + (value - 5) * 1.6 */
		c = Mode_Cmp(&a[0], "<=", &five);
		ok = c > 0 ? Vv_Arith('/', &a[0], &f625, &x) && Vv_Arith('+', &p2, &x, out) : c == 0 ?
			Vv_Arith('-', &a[0], &five, &x) && Vv_Arith('*', &x, &f16, &y) && Vv_Arith('+', &one, &y, out) : 0;
	} else if (c == 0 && (c = Mode_Cmp(&a[1], ">=", &zero)) > 0) {
		c = Mode_Cmp(&a[1], "<=", &one);
		ok = c > 0 ? Vv_Arith('-', &a[1], &p2, &x) && Vv_Arith('*', &x, &f625, out) : c == 0 ?
			Vv_Arith('-', &a[1], &one, &x) && Vv_Arith('/', &x, &f16, &y) && Vv_Arith('+', &five, &y, out) : 0;
	} else {
		ok = c >= 0;
	}
	Native_Free(&x);
	Native_Free(&y);
	return ok;
}

/* the adapters of the formulas: two keyword values, -1 when not given */
static PyObject *Vv_Formula(PyObject *args, PyObject *kw, char **names, byte (*fn)(const nativeVal_t *, nativeVal_t *)) {
	nativeVal_t a[2], out = { 0 };
	byte given[2];
	if (!Mode_Kw(args, kw, "|OO", names, 2, a, given)) {
		return NULL;
	}
	for (int i = 0; i < 2; i++) {
		if (!given[i]) {
			Native_SetInt(&a[i], -1);
		}
	}
	return Mode_KwOut(a, 2, fn(a, &out), &out);
}

static PyObject *PCMenu_GammaFormula(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "value", "gamma", NULL };
	(void)self;
	return Vv_Formula(args, kw, kwlist, Vv_Gamma);
}

static PyObject *PCMenu_MouseSensitivityFormula(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "value", "sens", NULL };
	(void)self;
	return Vv_Formula(args, kw, kwlist, Vv_Sensitivity);
}

/* the colour var of item times 0.75 */
static byte Vv_Dim(const nativeVal_t *id, const nativeVal_t *item, const char *var) {
	nativeVal_t c = { 0 }, f, v = { 0 };
	Native_SetFloat(&f, 0.75);
	byte ok = Mode_ScorerGetVal(id, item, var, &c) && Native_Arith('*', &c, &f, &v) && Mode_ScorerSet(id, item, var, &v);
	Native_Free(&c);
	Native_Free(&v);
	return ok;
}

/* `if SceneEffects and Scrap.Get(var) == 1` for a Yes/No item */
static byte Vv_SceneYes(int id, const char *item, int scene, const char *var) {
	return Vv_SetYes(id, item, scene ? Vv_GetIs(var, 1) : 0);
}

/* the Value of the Gamma slider: GammaFormula(gamma = Scrap.Get("VideoGammaRGB")), the module's function called with
 * its keyword (OBJLIB 3) */
static byte Vv_GammaValue(int id) {
	nativeVal_t g = { 0 }, name, r = { 0 };
	Native_SetStr(&name, "VideoGammaRGB");
	PyObject *fn = Mode_GlobalPy(PM, "GammaFormula");
	byte ok = fn && Mode_GVarGetV(&name, &g);
	PyObject *pg = ok ? Native_ToPy(&g) : NULL;
	PyObject *kw = pg ? Py_BuildValue("{s:O}", "gamma", pg) : NULL;
	PyObject *a = kw ? PyTuple_New(0) : NULL;
	PyObject *o = a ? Script_CallObject(fn, a, kw) : NULL;
	ok = o && Native_Arg(o, &r) && Mode_ScorerPut(id, "Gamma", "Value", &r);
	Py_XDECREF(o);
	Py_XDECREF(a);
	Py_XDECREF(kw);
	Py_XDECREF(pg);
	Py_XDECREF(fn);
	Native_Free(&g);
	Native_Free(&r);
	return ok;
}

/* the TextureFilter item from R_TexFilter, R_MipFilter and R_Anisotropy */
static byte Vv_FilterValue(int id) {
	int c, k;
	if ((c = Vv_GetIs("R_TexFilter", 0)) != 0) {
		return c > 0 && Vv_SetFrom(id, "TextureFilter", "TextureFilterList", 0);
	}
	if ((c = Vv_GetIs("R_TexFilter", 1)) != 0) {
		int mip = c > 0 ? Vv_GetIs("R_MipFilter", 0) : -1;
		return mip >= 0 && Vv_SetFrom(id, "TextureFilter", "TextureFilterList", mip ? 1 : 2);
	}
	if ((c = Vv_GetIs("R_TexFilter", 2)) != 0) {
		if (c < 0 || (c = Vv_GetIs("R_Anisotropy", 2)) < 0) {
			return 0;
		}
		k = 3;
		if (!c) {
			if ((c = Vv_GetIs("R_Anisotropy", 4)) < 0) {
				return 0;
			}
			k = c ? 4 : (c = Vv_GetIs("R_Anisotropy", 8)) < 0 ? -1 : c ? 5 : 3;
		}
		return k >= 0 && Vv_SetFrom(id, "TextureFilter", "TextureFilterList", k);
	}
	return Vv_SetFrom(id, "TextureFilter", "TextureFilterList", 2);
}

/* the TextureDetail item from TextureLOD (nothing for another value) */
static byte Vv_DetailValue(int id) {
	int c = Vv_GetIs("TextureLOD", 0);
	if (c == 0) {
		c = Vv_GetIs("TextureLOD", -1);
	}
	if (c) {
		return c > 0 && Vv_SetFrom(id, "TextureDetail", "TextureDetailList", 0);
	}
	for (long i = 1; i <= 2; i++) {
		if ((c = Vv_GetIs("TextureLOD", i)) != 0) {
			return c > 0 && Vv_SetFrom(id, "TextureDetail", "TextureDetailList", i);
		}
	}
	return 1;
}

/* the VideoMode item: VideoModes[SelectedVideoMode][1] */
static byte Vv_ModeValue(int id) {
	nativeVal_t modes = { 0 }, sel = { 0 }, mode = { 0 }, text = { 0 }, one;
	Native_SetInt(&one, 1);
	byte ok = Mode_G(PM, "VideoModes", &modes) && Mode_G(PM, "SelectedVideoMode", &sel) &&
		Mode_Sub(&modes, &sel, &mode) && Mode_Sub(&mode, &one, &text) && Mode_ScorerPut(id, "VideoMode", "Text", &text);
	Native_Free(&modes);
	Native_Free(&sel);
	Native_Free(&mode);
	Native_Free(&text);
	return ok;
}

/* the ShadowsDetail item from R_StencilShadows, R_StencilSelf and R_StencilMult; without stencil shadows (-1) the item
 * and its title cannot be chosen and are dimmed */
static byte Vv_ShadowsValue(int id) {
	int c = Vv_GetIs("R_StencilShadows", -1);
	if (c > 0) {
		nativeVal_t idv, items[2], empty;
		Native_SetInt(&idv, id);
		Native_SetStr(&items[0], "TitleShadowsDetail");
		Native_SetStr(&items[1], "ShadowsDetail");
		Native_SetStr(&empty, "");
		if (!Vv_SetFrom(id, "ShadowsDetail", "ShadowsDetailList", 3)) {
			return 0;
		}
		for (int i = 0; i < 2; i++) {
			if (!Mode_ScorerSet(&idv, &items[i], "OnAccept", &empty) || !Vv_Dim(&idv, &items[i], "Red") ||
				!Vv_Dim(&idv, &items[i], "Green") || !Vv_Dim(&idv, &items[i], "Blue")) {
				return 0;
			}
		}
		return 1;
	}
	if (c < 0) {
		return 0;
	}
	nativeVal_t mult = { 0 }, name, f1;
	Native_SetStr(&name, "R_StencilMult");
	Native_SetFloat(&f1, 1.0);
	int shadows = Vv_GetIs("R_StencilShadows", 1);
	int self = shadows >= 0 ? Vv_GetIs("R_StencilSelf", 1) : -1;
	byte ok = self >= 0 && Mode_GVarGetV(&name, &mult);
	if (ok && shadows > 0 && self > 0) {
		ok = Vv_SetFrom(id, "ShadowsDetail", "ShadowsDetailList", 0);
	} else if (ok && shadows > 0) {
		c = Mode_Cmp(&mult, ">=", &f1);
		ok = c >= 0 && Vv_SetFrom(id, "ShadowsDetail", "ShadowsDetailList", c ? 1 : 2);
	} else if (ok && shadows == 0) {
		ok = Vv_SetFrom(id, "ShadowsDetail", "ShadowsDetailList", 3);
	}
	Native_Free(&mult);
	return ok;
}

/* UpdateVideoOptions(id): the values of the engine into the items of the video menu of the script */
static byte Vv_UpdateOptions(int id) {
	if (!Vv_GammaValue(id) || !Vv_FilterValue(id) || !Vv_DetailValue(id) || !Vv_ModeValue(id) || !Vv_ShadowsValue(id) ||
		!Vv_SetYes(id, "VSync", Vv_GetIs("VSync", 1)) || !Vv_SetYes(id, "Lock", Vv_GetIs("R_VideoLockMode", 1)) ||
		!Vv_SetYes(id, "Fog", Vv_GetIs("R_FogEnable", 1))) {
		return 0;
	}
	int scene = Vv_GetIs("R_SceneRTarget", 1);
	return scene >= 0 && Vv_SceneYes(id, "Bloom", scene, "R_SceneBloomRTarget") &&
		Vv_SceneYes(id, "MotionBlur", scene, "R_SceneMotionBlurRTarget") &&
		Vv_SceneYes(id, "DuDv", scene, "R_SceneDuDvRTarget") && Vv_SetYes(id, "Shadows", 0);
}

static PyObject *PCMenu_UpdateVideoOptions(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	return Mode_Result(Vv_UpdateOptions(id));
}

/* VideoGetLast(id): the values the video menu of the script set back on Back */
static byte Vv_GetLast(void) {
	static const char *const vars[][2] = { { "Last_TexFilter", "R_TexFilter" }, { "Last_MipFilter", "R_MipFilter" },
		{ "Last_VideoGamma", "VideoGammaRGB" }, { "Last_Fog", "R_FogEnable" },
		{ "Last_VideoLockMode", "R_VideoLockMode" }, { "Last_VideoLockBuf", "R_VideoLockBuf" } };
	/* OBJLIB 6: import Menu */
	if (!Mode_Import("Menu")) {
		return 0;
	}
	for (int i = 0; i < 6; i++) {
		nativeVal_t name, v = { 0 };
		Native_SetStr(&name, vars[i][1]);
		byte ok = Mode_GVarGetV(&name, &v) && Mode_Put(PM, vars[i][0], NULL, &v);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

static PyObject *PCMenu_VideoGetLast(PyObject *self, PyObject *args) {
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	return Mode_Result(Vv_GetLast());
}

PyMethodDef g_MenuVideoVarsMethods[] = {
	{ "Apply_SetActive", PCMenu_Apply_SetActive, 1, NULL },
	{ "Apply_SetFog", PCMenu_Apply_SetFog, 1, NULL },
	{ "Apply_SetTextureFilter", PCMenu_Apply_SetTextureFilter, 1, NULL },
	{ "Apply_SetVideoLock", PCMenu_Apply_SetVideoLock, 1, NULL },
	{ "ChangeBoolean", PCMenu_ChangeBoolean, 1, NULL },
	{ "DrawLine", (PyCFunction)(void (*)(void))PCMenu_DrawLine, 3, NULL },
	{ "FillShadowsDetail", PCMenu_FillShadowsDetail, 1, NULL },
	{ "FillTextureDetail", PCMenu_FillTextureDetail, 1, NULL },
	{ "FillTextureFilter", PCMenu_FillTextureFilter, 1, NULL },
	{ "FillVideoModes", PCMenu_FillVideoModes, 1, NULL },
	{ "GammaFormula", (PyCFunction)(void (*)(void))PCMenu_GammaFormula, 3, NULL },
	{ "MouseSensitivityFormula", (PyCFunction)(void (*)(void))PCMenu_MouseSensitivityFormula, 3, NULL },
	{ "UpdateVideoOptions", PCMenu_UpdateVideoOptions, 1, NULL },
	{ "VideoGetLast", PCMenu_VideoGetLast, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
