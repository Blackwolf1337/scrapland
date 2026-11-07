/*
 * The helpers of scripts/scorer/pcmenu.py that work on the module variables of the video menu of the script
 * (YesValue, NoValue, TextureFilterList, TextureDetailList, ShadowsDetailList, VideoModes, SelectedVideoMode, the
 * Last_ values), the two slider formulas and DrawLine. The video menu of ScrapC (MenuVideo.c) keeps its own state and
 * calls none of them. They are here so that PCMenu, made in C from data/modules/pcmenu.mod, has every function of
 * the script; each does what the script does, line by line on the module variables (a missing one is a NameError).
 */
#include "scrap/script.h"

#define PM "PCMenu"

/* SScorer.Get(id, item, var) == YesValue (SScorer.Get first, as the line of the script) */
static int Vv_IsYes(scriptFrame_t *f, int id, const char *item) {
	PyObject *t = Sf_Ask(f, "SScorer", "Get", "(iss)", id, item, "Text");
	return Sf_Compare(f, t, "==", Sf_Global(f, PM, "YesValue"));
}

/* Scrap.Get(var) == i */
static int Vv_GetIs(scriptFrame_t *f, const char *var, long i) {
	return Sf_EqI(f, Sf_Ask(f, "Scrap", "Get", "(s)", var), i);
}

/* SScorer.Set(id, item, "Text", list[i]) with list a module variable */
static byte Vv_SetFrom(scriptFrame_t *f, int id, const char *item, const char *list, long i) {
	PyObject *l = Sf_Global(f, PM, list);
	return Sf_ScorerSet(f, id, item, "Text", Sf_Item(f, l, i));
}

/* SScorer.Set(id, item, "Text", YesValue if yes else NoValue) */
static byte Vv_SetYes(scriptFrame_t *f, int id, const char *item, int yes) {
	return yes >= 0 && Sf_ScorerSet(f, id, item, "Text", Sf_Global(f, PM, yes ? "YesValue" : "NoValue"));
}

/* Apply_SetActive(id, control): the Apply button flashes */
static PyObject *PCMenu_Apply_SetActive(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	SF_ARGS("iO", &id, &control);
	Sf_ScorerStr(&f, id, "Apply", "Effect", "Outline");
	Sf_ScorerNum(&f, id, "Apply", "Red", 255);
	Sf_ScorerNum(&f, id, "Apply", "Green", 255);
	Sf_ScorerNum(&f, id, "Apply", "Blue", 180);
	Sf_ScorerStr(&f, id, "Apply", "OnRender", "!ScorerSpriteFlash");
	return Sf_None(&f);
}

/* Apply_SetFog(id, control): R_FogEnable from the Fog item */
static PyObject *PCMenu_Apply_SetFog(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	SF_ARGS("iO", &id, &control);
	int yes = Vv_IsYes(&f, id, "Fog");
	if (yes >= 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "R_FogEnable", yes);
	}
	return Sf_None(&f);
}

/* Apply_SetTextureFilter(id, control): the filter variables of the TextureFilter item (TextureFilterList) */
static PyObject *PCMenu_Apply_SetTextureFilter(PyObject *self, PyObject *args) {
	static const long values[6][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 2, 1, 2 }, { 2, 1, 4 }, { 2, 1, 8 } };
	int id;
	PyObject *control;
	SF_ARGS("iO", &id, &control);
	PyObject *tex = Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "TextureFilter", "Text");
	for (int i = 0; i < 6; i++) {
		PyObject *list = Sf_Global(&f, PM, "TextureFilterList");
		int eq = Sf_Compare(&f, tex, "==", Sf_Item(&f, list, i));
		if (eq) {
			if (eq > 0) {
				Sf_Run(&f, "Scrap", "Set", "(sl)", "R_TexFilter", values[i][0]);
				Sf_Run(&f, "Scrap", "Set", "(sl)", "R_MipFilter", values[i][1]);
				Sf_Run(&f, "Scrap", "Set", "(sl)", "R_Anisotropy", values[i][2]);
			}
			break;
		}
	}
	return Sf_None(&f);
}

/* Apply_SetVideoLock(id, control): the lock mode of the Lock item */
static PyObject *PCMenu_Apply_SetVideoLock(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	SF_ARGS("iO", &id, &control);
	int yes = Vv_IsYes(&f, id, "Lock");
	if (yes >= 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "R_VideoLockMode", yes ? 1 : 2);
		Sf_Run(&f, "Scrap", "Set", "(si)", "R_VideoLockBuf", 1);
	}
	return Sf_None(&f);
}

/* ChangeBoolean(id, control): the item text flips between YesValue and NoValue */
static PyObject *PCMenu_ChangeBoolean(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	SF_ARGS("iO", &id, &control);
	PyObject *t = Sf_Ask(&f, "SScorer", "Get", "(iOs)", id, control, "Text");
	int yes = Sf_Compare(&f, t, "==", Sf_Global(&f, PM, "YesValue"));
	if (yes >= 0) {
		Sf_Run(&f, "SScorer", "Set", "(iOsO)", id, control, "Text", Sf_Global(&f, PM, yes ? "NoValue" : "YesValue"));
	}
	return Sf_None(&f);
}

/* DrawLine(id, name, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32): a sprite as a
 * coloured rectangle, added when the item is not there */
static PyObject *PCMenu_DrawLine(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "id", "name", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static const long defs[8] = { 64, 64, 512, 352, 92, 0, 0, 32 };
	static const char *const vars[8] = { "X", "Y", "ScaleX", "ScaleY", "Alpha", "Red", "Green", "Blue" };
	scriptFrame_t f = { 0 };
	PyObject *id, *name, *v[8] = { NULL };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", kwlist, &id, &name, &v[0], &v[1], &v[2], &v[3], &v[4],
		&v[5], &v[6], &v[7])) {
		return NULL;
	}
	for (int i = 0; i < 8; i++) {
		v[i] = v[i] ? v[i] : Sf_Int(&f, defs[i]);
	}
	PyObject *vis = Sf_Ask(&f, "SScorer", "Get", "(OOs)", id, name, "Visible");
	if (Sf_Compare(&f, vis, "==", Py_None) > 0) {
		Sf_Run(&f, "SScorer", "Add", "(OOs)", id, name, "Sprite");
	}
	for (int i = 0; i < 8; i++) {
		Sf_Run(&f, "SScorer", "Set", "(OOsO)", id, name, vars[i], v[i]);
		if (i == 3) {
			Sf_Run(&f, "SScorer", "Set", "(OOsi)", id, name, "Unit", 1);
		}
	}
	Sf_Run(&f, "SScorer", "Set", "(OOsi)", id, name, "Visible", 1);
	return Sf_None(&f);
}

/* `global name; name = []` then name.append(prefix + Scrap.GetLangStr(key)) per key (the key itself when lang is 0,
 * no prefix when prefix is NULL) */
static PyObject *Vv_Fill(const char *name, const char *const *keys, const char *const *prefix, int n, byte lang) {
	scriptFrame_t f = { 0 };
	Sf_SetGlobal(&f, PM, name, Sf_Keep(&f, PyList_New(0)));
	for (int i = 0; i < n; i++) {
		PyObject *list = Sf_Global(&f, PM, name);
		PyObject *v = lang ? Sf_Lang(&f, Sf_Str(&f, keys[i])) : Sf_Str(&f, keys[i]);
		Sf_Method(&f, list, "append", "(O)", prefix ? Sf_Cat(&f, "sO", prefix[i], v) : v);
	}
	return Sf_None(&f);
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
	return Vv_Fill("ShadowsDetailList", vv_levels, marks, 4, 1);
}

/* FillTextureDetail(): TextureDetailList, high, medium, low */
static PyObject *PCMenu_FillTextureDetail(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Vv_Fill("TextureDetailList", vv_levels, NULL, 3, 1);
}

/* FillTextureFilter(): TextureFilterList, the names of the filters */
static PyObject *PCMenu_FillTextureFilter(PyObject *self, PyObject *args) {
	static const char *const filters[] = { "Point", "Bilinear", "Trilinear", "Anisotropic x2", "Anisotropic x4",
		"Anisotropic x8" };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Vv_Fill("TextureFilterList", filters, NULL, 6, 0);
}

/* int(cad[lo:hi]) with lo, hi Python ints */
static PyObject *Vv_Number(scriptFrame_t *f, PyObject *cad, PyObject *lo, PyObject *hi) {
	if (!lo || !hi) {
		return NULL;
	}
	return Sf_ToInt(f, Sf_Slice(f, cad, (int)PyInt_AsLong(lo), (int)PyInt_AsLong(hi)));
}

/*
 * FillVideoModes(): VideoModes ([number, text] of every mode), CurrentVideoMode, SelectedVideoMode and
 * VideoModesValid (the current mode and the 32 bpp modes of 640x480 and more that are not higher than wide; all of
 * them when that leaves one or none)
 */
static PyObject *PCMenu_FillVideoModes(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "string");
	Sf_SetGlobal(&f, PM, "CurrentVideoMode", Sf_Ask(&f, "Scrap", "GetVideoCurrentMode", "()"));
	Sf_SetGlobal(&f, PM, "SelectedVideoMode", Sf_Global(&f, PM, "CurrentVideoMode"));
	Sf_SetGlobal(&f, PM, "VideoModes", Sf_Keep(&f, PyList_New(0)));
	PyObject *num = Sf_Ask(&f, "Scrap", "GetVideoModes", "(i)", -1);
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", num);
	PyObject *mode;
	int mark = f.n;
	for (int i = 0; (mode = Sf_Next(&f, range, i)) != NULL; i++) {
		PyObject *desc = Sf_Ask(&f, "Scrap", "GetVideoModes", "(O)", mode);
		Sf_Method(&f, Sf_Global(&f, PM, "VideoModes"), "append", "(O)", Sf_Build(&f, "[OO]", mode, desc));
		Sf_Release(&f, mark);
	}
	Sf_SetGlobal(&f, PM, "VideoModesValid", Sf_Keep(&f, PyList_New(0)));
	PyObject *modes = Sf_Global(&f, PM, "VideoModes");
	mark = f.n;
	for (int i = 0; (mode = Sf_Next(&f, modes, i)) != NULL; i++) {
		PyObject *cad = Sf_Item(&f, mode, 1);
		PyObject *i1 = Sf_Ask(&f, "string", "find", "(Os)", cad, "x");
		PyObject *i2 = Sf_Ask(&f, "string", "find", "(Os)", cad, " (");
		PyObject *i3 = Sf_Ask(&f, "string", "find", "(Os)", cad, " bpp)");
		PyObject *width = Vv_Number(&f, cad, Sf_Int(&f, 0), i1);
		PyObject *height = Vv_Number(&f, cad, Sf_OpI(&f, i1, '+', 1), i2);
		PyObject *bpp = Vv_Number(&f, cad, Sf_OpI(&f, i2, '+', 2), i3);
		/* Mode[0] == SelectedVideoMode or (bpp==32 and (width>=640 and height>=480) and (width>=height)) */
		PyObject *number = Sf_Item(&f, mode, 0);
		int ok = Sf_Compare(&f, number, "==", Sf_Global(&f, PM, "SelectedVideoMode"));
		if (ok == 0) {
			ok = Sf_EqI(&f, bpp, 32);
			ok = ok > 0 ? Sf_Compare(&f, width, ">=", Sf_Int(&f, 640)) : ok;
			ok = ok > 0 ? Sf_Compare(&f, height, ">=", Sf_Int(&f, 480)) : ok;
			ok = ok > 0 ? Sf_Compare(&f, width, ">=", height) : ok;
		}
		if (ok > 0) {
			Sf_Method(&f, Sf_Global(&f, PM, "VideoModesValid"), "append", "(O)", mode);
		}
		Sf_Release(&f, mark);
	}
	int n = Sf_Len(&f, Sf_Global(&f, PM, "VideoModesValid"));
	if (n >= 0 && n <= 1) {
		Sf_SetGlobal(&f, PM, "VideoModesValid", Sf_Keep(&f, PyList_New(0)));
		modes = Sf_Global(&f, PM, "VideoModes");
		mark = f.n;
		for (int i = 0; (mode = Sf_Next(&f, modes, i)) != NULL; i++) {
			Sf_Method(&f, Sf_Global(&f, PM, "VideoModesValid"), "append", "(O)", mode);
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/*
 * The slider formulas: GammaFormula(value=-1, gamma=-1) gives the gamma of a slider value (value >= 0) or the value
 * of a gamma; MouseSensitivityFormula(value=-1, sens=-1) the same for the mouse; None when both are negative. The
 * arithmetic is that of the script on the given objects (an int divides as an int).
 */
static PyObject *PCMenu_GammaFormula(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "value", "gamma", NULL };
	scriptFrame_t f = { 0 };
	PyObject *value = NULL, *gamma = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", kwlist, &value, &gamma)) {
		return NULL;
	}
	value = value ? value : Sf_Int(&f, -1);
	gamma = gamma ? gamma : Sf_Int(&f, -1);
	int c = Sf_Compare(&f, value, ">=", Sf_Int(&f, 0));
	if (c > 0) {
		/* gamma = 1.0, then 0.5 + value below 0.5, else 1 + (value - 0.5) * 4 */
		c = Sf_Compare(&f, value, "<", Sf_Float(&f, 0.5));
		if (c > 0) {
			gamma = Sf_Op(&f, Sf_Float(&f, 0.5), '+', value);
		} else if (c == 0) {
			gamma = Sf_Op(&f, Sf_Int(&f, 1), '+', Sf_OpI(&f, Sf_Op(&f, value, '-', Sf_Float(&f, 0.5)), '*', 4));
		}
		return Sf_Return(&f, gamma);
	}
	if (c == 0 && Sf_Compare(&f, gamma, ">=", Sf_Int(&f, 0)) > 0) {
		c = Sf_Compare(&f, gamma, "<=", Sf_Int(&f, 1));
		if (c > 0) {
			value = Sf_Op(&f, gamma, '-', Sf_Float(&f, 0.5));
		} else if (c == 0) {
			value = Sf_Op(&f, Sf_Float(&f, 0.5), '+', Sf_OpI(&f, Sf_OpI(&f, gamma, '-', 1), '/', 4));
		}
		return Sf_Return(&f, value);
	}
	return Sf_None(&f);
}

static PyObject *PCMenu_MouseSensitivityFormula(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "value", "sens", NULL };
	scriptFrame_t f = { 0 };
	PyObject *value = NULL, *sens = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", kwlist, &value, &sens)) {
		return NULL;
	}
	value = value ? value : Sf_Int(&f, -1);
	sens = sens ? sens : Sf_Int(&f, -1);
	int c = Sf_Compare(&f, value, ">=", Sf_Int(&f, 0));
	if (c > 0) {
		/* sens = 1.0, then 0.2 + value / 6.25 up to 5, else 1 + (value - 5) * 1.6 */
		c = Sf_Compare(&f, value, "<=", Sf_Int(&f, 5));
		if (c > 0) {
			sens = Sf_Op(&f, Sf_Float(&f, 0.2), '+', Sf_Op(&f, value, '/', Sf_Float(&f, 6.25)));
		} else if (c == 0) {
			sens = Sf_Op(&f, Sf_Int(&f, 1), '+', Sf_Op(&f, Sf_OpI(&f, value, '-', 5), '*', Sf_Float(&f, 1.6)));
		}
		return Sf_Return(&f, sens);
	}
	if (c == 0 && Sf_Compare(&f, sens, ">=", Sf_Int(&f, 0)) > 0) {
		c = Sf_Compare(&f, sens, "<=", Sf_Int(&f, 1));
		if (c > 0) {
			value = Sf_Op(&f, Sf_Op(&f, sens, '-', Sf_Float(&f, 0.2)), '*', Sf_Float(&f, 6.25));
		} else if (c == 0) {
			value = Sf_Op(&f, Sf_Int(&f, 5), '+', Sf_Op(&f, Sf_OpI(&f, sens, '-', 1), '/', Sf_Float(&f, 1.6)));
		}
		return Sf_Return(&f, value);
	}
	return Sf_None(&f);
}

/* the colour var of item times 0.75 */
static void Vv_Dim(scriptFrame_t *f, int id, PyObject *item, const char *var) {
	PyObject *c = Sf_Ask(f, "SScorer", "Get", "(iOs)", id, item, var);
	Sf_Run(f, "SScorer", "Set", "(iOsO)", id, item, var, Sf_Op(f, c, '*', Sf_Float(f, 0.75)));
}

/* `if SceneEffects and Scrap.Get(var) == 1` for a Yes/No item */
static void Vv_SceneYes(scriptFrame_t *f, int id, const char *item, PyObject *scene, const char *var) {
	int yes = Sf_True(f, scene);
	if (yes > 0) {
		yes = Vv_GetIs(f, var, 1);
	}
	Vv_SetYes(f, id, item, yes);
}

/* UpdateVideoOptions(id): the values of the engine into the items of the video menu of the script */
static PyObject *PCMenu_UpdateVideoOptions(PyObject *self, PyObject *args) {
	int id;
	SF_ARGS("i", &id);
	PyObject *fn = Sf_Global(&f, PM, "GammaFormula");
	PyObject *g = Sf_Ask(&f, "Scrap", "Get", "(s)", "VideoGammaRGB");
	PyObject *kw = Sf_Build(&f, "{s:O}", "gamma", g);
	Sf_ScorerSet(&f, id, "Gamma", "Value", kw ? Sf_CallKw(&f, fn, PyTuple_New(0), kw) : NULL);

	if (Vv_GetIs(&f, "R_TexFilter", 0) > 0) {
		Vv_SetFrom(&f, id, "TextureFilter", "TextureFilterList", 0);
	} else if (Vv_GetIs(&f, "R_TexFilter", 1) > 0) {
		int mip = Vv_GetIs(&f, "R_MipFilter", 0);
		if (mip >= 0) {
			Vv_SetFrom(&f, id, "TextureFilter", "TextureFilterList", mip ? 1 : 2);
		}
	} else if (Vv_GetIs(&f, "R_TexFilter", 2) > 0) {
		int k = Vv_GetIs(&f, "R_Anisotropy", 2) ? 3 : Vv_GetIs(&f, "R_Anisotropy", 4) ? 4 :
			Vv_GetIs(&f, "R_Anisotropy", 8) ? 5 : 3;
		Vv_SetFrom(&f, id, "TextureFilter", "TextureFilterList", k);
	} else {
		Vv_SetFrom(&f, id, "TextureFilter", "TextureFilterList", 2);
	}

	if (Vv_GetIs(&f, "TextureLOD", 0) || Vv_GetIs(&f, "TextureLOD", -1)) {
		Vv_SetFrom(&f, id, "TextureDetail", "TextureDetailList", 0);
	} else if (Vv_GetIs(&f, "TextureLOD", 1)) {
		Vv_SetFrom(&f, id, "TextureDetail", "TextureDetailList", 1);
	} else if (Vv_GetIs(&f, "TextureLOD", 2)) {
		Vv_SetFrom(&f, id, "TextureDetail", "TextureDetailList", 2);
	}

	PyObject *modes = Sf_Global(&f, PM, "VideoModes");
	PyObject *mode = Sf_GetItem(&f, modes, Sf_Global(&f, PM, "SelectedVideoMode"));
	Sf_ScorerSet(&f, id, "VideoMode", "Text", Sf_Item(&f, mode, 1));

	if (Vv_GetIs(&f, "R_StencilShadows", -1) > 0) {
		Vv_SetFrom(&f, id, "ShadowsDetail", "ShadowsDetailList", 3);
		PyObject *items[2] = { Sf_Str(&f, "TitleShadowsDetail"), Sf_Str(&f, "ShadowsDetail") };
		for (int i = 0; i < 2; i++) {
			Sf_Run(&f, "SScorer", "Set", "(iOss)", id, items[i], "OnAccept", "");
			Vv_Dim(&f, id, items[i], "Red");
			Vv_Dim(&f, id, items[i], "Green");
			Vv_Dim(&f, id, items[i], "Blue");
		}
	} else {
		int shadows = Vv_GetIs(&f, "R_StencilShadows", 1);
		int selfShadows = Vv_GetIs(&f, "R_StencilSelf", 1);
		PyObject *mult = Sf_Ask(&f, "Scrap", "Get", "(s)", "R_StencilMult");
		if (shadows > 0 && selfShadows > 0) {
			Vv_SetFrom(&f, id, "ShadowsDetail", "ShadowsDetailList", 0);
		} else if (shadows > 0 && selfShadows == 0) {
			int c = Sf_Compare(&f, mult, ">=", Sf_Float(&f, 1.0));
			if (c >= 0) {
				Vv_SetFrom(&f, id, "ShadowsDetail", "ShadowsDetailList", c ? 1 : 2);
			}
		} else if (shadows == 0) {
			Vv_SetFrom(&f, id, "ShadowsDetail", "ShadowsDetailList", 3);
		}
	}

	Vv_SetYes(&f, id, "VSync", Vv_GetIs(&f, "VSync", 1));
	Vv_SetYes(&f, id, "Lock", Vv_GetIs(&f, "R_VideoLockMode", 1));
	Vv_SetYes(&f, id, "Fog", Vv_GetIs(&f, "R_FogEnable", 1));
	PyObject *scene = Sf_Int(&f, Vv_GetIs(&f, "R_SceneRTarget", 1));
	Vv_SceneYes(&f, id, "Bloom", scene, "R_SceneBloomRTarget");
	Vv_SceneYes(&f, id, "MotionBlur", scene, "R_SceneMotionBlurRTarget");
	Vv_SceneYes(&f, id, "DuDv", scene, "R_SceneDuDvRTarget");
	Vv_SetYes(&f, id, "Shadows", 0);
	return Sf_None(&f);
}

/* VideoGetLast(id): the values the video menu of the script set back on Back */
static PyObject *PCMenu_VideoGetLast(PyObject *self, PyObject *args) {
	static const char *const vars[][2] = { { "Last_TexFilter", "R_TexFilter" }, { "Last_MipFilter", "R_MipFilter" },
		{ "Last_VideoGamma", "VideoGammaRGB" }, { "Last_Fog", "R_FogEnable" },
		{ "Last_VideoLockMode", "R_VideoLockMode" }, { "Last_VideoLockBuf", "R_VideoLockBuf" } };
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Menu");
	for (int i = 0; i < 6; i++) {
		Sf_SetGlobal(&f, PM, vars[i][0], Sf_Ask(&f, "Scrap", "Get", "(s)", vars[i][1]));
	}
	return Sf_None(&f);
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
