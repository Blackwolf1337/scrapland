/*
 * The HullTab module in C (scripts/shipedit/hulltab.py, issue #2): the hull tab of the ship editor, where the player
 * puts hull plates on the ship or takes them off for money (ShipMaker.armorPieceCost each, ShipEdit.armorPieceWeight
 * of mass each, up to the hull and mass limits of ShipMaker.ShipTon). The layout of the tab is data/menus/hulltab.def.
 * ShipEdit, HangarTab and ShipMaker are called through their modules. The colours and UseTxt stay variables of the
 * module.
 */
#include "scrap/script.h"

static const char hulltab_def[] = {
#embed "../../../data/menus/hulltab.def"
	, 0 };

#define HULLTAB_FILE "menus/hulltab.def"

/* HullTab.HullColor*, the hull text above the minimum, and HullMinColor* at the minimum */
#define HULL_RED 254
#define HULL_GREEN 133
#define HULL_BLUE 230
#define HULL_MIN_RED 122
#define HULL_MIN_GREEN 161
#define HULL_MIN_BLUE 176

/* the step usehint <key>: the Hint of the item, Scrap.GetLangStr(key) % UseTxt (the error left set stops the file) */
static void HT_StepHint(int id, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	PyObject *key = Sf_Item(&f, args, 0);
	PyObject *hint = Sf_Op(&f, Sf_Lang(&f, key), '%', Sf_Global(&f, "HullTab", "UseTxt"));
	Sf_ScorerSet(&f, id, Script_CurrentItem(), "Hint", hint);
	Sf_Done(&f, NULL);
}

static const scriptStep_t hulltab_steps[] = {
	{ "usehint", HT_StepHint },
	{ NULL, NULL },
};

static void HT_Run(scriptFrame_t *f, int id, const char *section, PyObject *vars) {
	if (!f->failed) {
		Sf_Check(f, Script_RunFileTry(id, HULLTAB_FILE, section, hulltab_def, hulltab_steps, vars));
	}
}

/* HangarTab.prevShip.<var> */
static PyObject *HT_Ship(scriptFrame_t *f, const char *var) {
	return Sf_Attr(f, Sf_Mod(f, "HangarTab", "prevShip"), var);
}

/* ShipMaker.ShipTon[string.upper(HangarTab.prevShip.Model)][i] */
static PyObject *HT_Ton(scriptFrame_t *f, int i) {
	PyObject *model = Sf_Ask(f, "string", "upper", "(O)", HT_Ship(f, "Model"));
	return Sf_Item(f, Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "ShipTon"), model), i);
}

static void HT_Visible(int id, const char *const *items, int n, int v) {
	for (int i = 0; i < n; i++) {
		Script_SetNum(id, items[i], "Visible", v);
	}
}

/* the prediction arrows and the money the hull arrows show, hidden again */
static void HT_HidePrediction(int id, const char *weight) {
	const char *items[] = { weight, "WeightBack", "BoostUp", "BoostDown", "BoostEqual", "BoostBack", "MoneyPredict" };
	HT_Visible(id, items, 7, 0);
}

/* ShowHullPredictionUp(id, control, lostfocus): the mass and boost after one more plate, and its cost */
static PyObject *HT_ShowHullPredictionUp(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_ScorerNum(&f, id, "WeightUp", "Visible", 1);
	Sf_ScorerNum(&f, id, "WeightBack", "Visible", 1);
	PyObject *mass = Sf_ToInt(&f, HT_Ship(&f, "AcMass"));
	PyObject *weight = Sf_Op(&f, mass, '+', Sf_Mod(&f, "ShipEdit", "armorPieceWeight"));
	Sf_Ask(&f, "HangarTab", "ShowBoostPrediction", "(iO)", id, weight);
	PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
	int poor = Sf_Compare(&f, money, "<", Sf_Mod(&f, "ShipMaker", "armorPieceCost"));
	Sf_ScorerNum(&f, id, "MoneyPredict", "Green", poor ? 0 : 255);
	PyObject *text = Sf_Cat(&f, "sO", "-", Sf_Repr(&f, Sf_Mod(&f, "ShipMaker", "armorPieceCost")));
	Sf_ScorerSet(&f, id, "MoneyPredict", "Text", text);
	Sf_ScorerNum(&f, id, "MoneyPredict", "Visible", 1);
	return Sf_None(&f);
}

/* HideHullPredictionUp(id, control, lostfocus) */
static PyObject *HT_HideHullPredictionUp(PyObject *self, PyObject *args) {
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	HT_HidePrediction(id, "WeightUp");
	return Py_ReturnNone();
}

/* ShowHullPredictionDown(id, control, lostfocus): the mass and boost with one plate less, and the money back */
static PyObject *HT_ShowHullPredictionDown(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_ScorerNum(&f, id, "WeightDown", "Visible", 1);
	Sf_ScorerNum(&f, id, "WeightBack", "Visible", 1);
	PyObject *mass = Sf_ToInt(&f, HT_Ship(&f, "AcMass"));
	PyObject *weight = Sf_Op(&f, mass, '-', Sf_Mod(&f, "ShipEdit", "armorPieceWeight"));
	Sf_Ask(&f, "HangarTab", "ShowBoostPrediction", "(iO)", id, weight);
	Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 255);
	PyObject *text = Sf_Cat(&f, "sO", "+", Sf_Repr(&f, Sf_Mod(&f, "ShipMaker", "armorPieceCost")));
	Sf_ScorerSet(&f, id, "MoneyPredict", "Text", text);
	Sf_ScorerNum(&f, id, "MoneyPredict", "Visible", 1);
	return Sf_None(&f);
}

/* HideHullPredictionDown(id, control, lostfocus) */
static PyObject *HT_HideHullPredictionDown(PyObject *self, PyObject *args) {
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	HT_HidePrediction(id, "WeightDown");
	return Py_ReturnNone();
}

/* TestArrows(id, changeDefault=1): an arrow disappears at the hull limits, the links follow */
static PyObject *HT_TestArrows(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "changeDefault", NULL };
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *change = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "i|O", names, &id, &change)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	PyObject *linkUD = Sf_From(&f, "Menu", "LinkUD");
	PyObject *linkLR = Sf_From(&f, "Menu", "LinkLR");
	PyObject *unlinkLR = Sf_From(&f, "Menu", "UnLinkLR");
	PyObject *def = change ? change : Sf_Int(&f, 1);
	Sf_Line(&f, unlinkLR, "(iss)", id, "HullDown", "HullUp");
	int atMin = Sf_Compare(&f, HT_Ship(&f, "MaxLife"), "==", HT_Ship(&f, "MinLife"));
	int atMax = 0;
	if (atMin == 0) {
		atMax = Sf_Compare(&f, HT_Ship(&f, "MaxLife"), "==", HT_Ton(&f, 1));
	}
	if (atMin > 0) {
		if (Sf_True(&f, def) > 0) {
			Sf_ScorerDefault(&f, id, "HullUp");
		}
		Sf_ScorerNum(&f, id, "HullDown", "W", 0);
		Sf_ScorerNum(&f, id, "HullDown", "H", 0);
		Sf_ScorerNum(&f, id, "HullDown", "SpriteIndex", 2);
		Sf_Line(&f, linkUD, "(iss)", id, "ShipEditTabHull", "HullUp");
		Sf_Line(&f, linkUD, "(iss)", id, "HullUp", "Exit");
		Sf_Line(&f, unlinkLR, "(iss)", id, "HullDown", "HullUp");
		Sf_ScorerStr(&f, id, "Exit", "Up", "HullUp");
	} else if (atMax > 0) {
		if (Sf_True(&f, def) > 0) {
			Sf_ScorerDefault(&f, id, "HullDown");
		}
		Sf_ScorerNum(&f, id, "HullUp", "W", 0);
		Sf_ScorerNum(&f, id, "HullUp", "H", 0);
		Sf_ScorerNum(&f, id, "HullUp", "SpriteIndex", 2);
		Sf_Line(&f, linkUD, "(iss)", id, "ShipEditTabHull", "HullDown");
		Sf_Line(&f, linkUD, "(iss)", id, "HullDown", "Exit");
		Sf_ScorerStr(&f, id, "Exit", "Up", "HullDown");
	} else if (!f.failed) {
		Sf_ScorerNum(&f, id, "HullUp", "W", 25);
		Sf_ScorerNum(&f, id, "HullUp", "H", 25);
		Sf_ScorerNum(&f, id, "HullUp", "SpriteIndex", 3);
		Sf_ScorerNum(&f, id, "HullDown", "W", 25);
		Sf_ScorerNum(&f, id, "HullDown", "H", 25);
		Sf_ScorerNum(&f, id, "HullDown", "SpriteIndex", 3);
		Sf_Line(&f, linkLR, "(iss)", id, "HullDown", "HullUp");
		Sf_Line(&f, linkUD, "(iss)", id, "ShipEditTabHull", "HullUp");
		Sf_Line(&f, linkUD, "(iss)", id, "HullUp", "Exit");
		Sf_ScorerStr(&f, id, "Exit", "Up", "HullUp");
	}
	Sf_ScorerStr(&f, id, "HullDown", "Up", "ShipEditTabHull");
	Sf_ScorerStr(&f, id, "HullDown", "Down", "Exit");
	Sf_ScorerStr(&f, id, "HullUp", "Up", "ShipEditTabHull");
	Sf_ScorerStr(&f, id, "HullUp", "Down", "Exit");
	return Sf_None(&f);
}

/* IncrementHull(id, control) and DecrementHull(id, control): one plate on or off, then the boost prediction */
static PyObject *HT_StepHull(PyObject *args, int step) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipEdit");
	Sf_Own(&f, "HullTab", "ChangeHull", "(ii)", id, step);
	PyObject *weight = Sf_Op(&f, HT_Ship(&f, "AcMass"), step > 0 ? '+' : '-', Sf_Mod(&f, "ShipEdit", "armorPieceWeight"));
	Sf_Ask(&f, "HangarTab", "ShowBoostPrediction", "(iO)", id, weight);
	return Sf_None(&f);
}

static PyObject *HT_IncrementHull(PyObject *self, PyObject *args) {
	(void)self;
	return HT_StepHull(args, 1);
}

static PyObject *HT_DecrementHull(PyObject *self, PyObject *args) {
	(void)self;
	return HT_StepHull(args, -1);
}

/* the money text after a change of the hull, and the money of the game */
static void HT_Money(scriptFrame_t *f, int id, PyObject *money) {
	Sf_ScorerSet(f, id, "Money", "Text", Sf_Repr(f, money));
}

/* ChangeHull(id, value): value plates on (or off when negative), as many as the limits and the money allow; the
 * number changed (negative for plates taken off) */
static PyObject *HT_ChangeHull(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *value;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &value)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "string");
	PyObject *zero = Sf_Int(&f, 0);
	int neg = Sf_Compare(&f, value, "<", zero);
	if (neg > 0) {
		value = Sf_Keep(&f, f.failed ? NULL : PyNumber_Negative(value));
		if (Sf_Compare(&f, HT_Ship(&f, "MaxLife"), "<=", HT_Ship(&f, "MinLife")) > 0) {
			return Sf_Return(&f, zero);
		}
		PyObject *room = Sf_Op(&f, HT_Ship(&f, "MaxLife"), '-', HT_Ship(&f, "MinLife"));
		if (Sf_Compare(&f, value, ">", room) > 0) {
			value = Sf_Op(&f, HT_Ship(&f, "MaxLife"), '-', HT_Ship(&f, "MinLife"));
		}
		if (Sf_Compare(&f, value, ">", zero) > 0) {
			PyObject *multi = Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()");
			if (Sf_Compare(&f, multi, "!=", Sf_Int(&f, 1)) > 0) {
				PyObject *back = Sf_Op(&f, Sf_Mod(&f, "ShipMaker", "armorPieceCost"), '*', value);
				PyObject *money = Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', back);
				HT_Money(&f, id, money);
				Sf_Ask(&f, "Scrap", "SetMoney", "(O)", money);
			}
			PyObject *ship = Sf_Mod(&f, "HangarTab", "prevShip");
			Sf_SetAttr(&f, ship, "MaxLife", Sf_Op(&f, HT_Ship(&f, "MaxLife"), '-', value));
			ship = Sf_Mod(&f, "HangarTab", "prevShip");
			Sf_SetAttr(&f, ship, "Life", HT_Ship(&f, "MaxLife"));
			Sf_Ask(&f, "ShipMaker", "SetShipParams", "(O)", HT_Ship(&f, "Name"));
			Sf_Ask(&f, "HangarTab", "ShowShipInfo", "(i)", id);
			Sf_Own(&f, "HullTab", "SetHullState", "(i)", id);
			return Sf_Done(&f, f.failed ? NULL : PyNumber_Negative(value));
		}
	} else if (neg == 0 && Sf_Compare(&f, value, ">", zero) > 0) {
		PyObject *left = Sf_Op(&f, HT_Ton(&f, 2), '-', HT_Ship(&f, "AcMass"));
		PyObject *pieces = Sf_Op(&f, left, '/', Sf_Mod(&f, "ShipEdit", "armorPieceWeight"));
		if (Sf_Compare(&f, value, ">", pieces) > 0) {
			value = Sf_ToInt(&f, pieces);
		}
		PyObject *multi = Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()");
		if (Sf_Compare(&f, multi, "!=", Sf_Int(&f, 1)) > 0) {
			PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
			PyObject *byMoney = Sf_ToInt(&f, Sf_Op(&f, money, '/', Sf_Mod(&f, "ShipMaker", "armorPieceCost")));
			if (Sf_Compare(&f, value, ">", byMoney) > 0) {
				value = byMoney;
			}
		}
		if (Sf_Compare(&f, value, ">", zero) > 0) {
			multi = Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()");
			if (Sf_Compare(&f, multi, "!=", Sf_Int(&f, 1)) > 0) {
				PyObject *cost = Sf_Op(&f, Sf_Mod(&f, "ShipMaker", "armorPieceCost"), '*', value);
				PyObject *money = Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '-', cost);
				HT_Money(&f, id, money);
				if (Sf_Compare(&f, money, "<", Sf_Mod(&f, "ShipMaker", "armorPieceCost")) > 0) {
					Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 0);
				}
				Sf_Ask(&f, "Scrap", "SetMoney", "(O)", money);
			}
			PyObject *ship = Sf_Mod(&f, "HangarTab", "prevShip");
			Sf_SetAttr(&f, ship, "MaxLife", Sf_Op(&f, HT_Ship(&f, "MaxLife"), '+', value));
			ship = Sf_Mod(&f, "HangarTab", "prevShip");
			Sf_SetAttr(&f, ship, "Life", HT_Ship(&f, "MaxLife"));
			Sf_Ask(&f, "ShipMaker", "SetShipParams", "(O)", HT_Ship(&f, "Name"));
			Sf_Ask(&f, "HangarTab", "ShowShipInfo", "(i)", id);
			Sf_Own(&f, "HullTab", "SetHullState", "(i)", id);
			return Sf_Return(&f, value);
		}
	}
	return Sf_Return(&f, zero);
}

/* SetHullState(id): the colour and number of the hull text, the bar, the slider and the arrows */
static PyObject *HT_SetHullState(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	if (Sf_Compare(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "!=", Py_None) > 0) {
		byte above = Sf_Compare(&f, HT_Ship(&f, "Life"), ">", HT_Ship(&f, "MinLife")) > 0;
		Sf_ScorerNum(&f, id, "HullText", "TextColorRed", above ? HULL_RED : HULL_MIN_RED);
		Sf_ScorerNum(&f, id, "HullText", "TextColorGreen", above ? HULL_GREEN : HULL_MIN_GREEN);
		Sf_ScorerNum(&f, id, "HullText", "TextColorBlue", above ? HULL_BLUE : HULL_MIN_BLUE);
	}
	PyObject *minHull = HT_Ton(&f, 0), *maxHull = HT_Ton(&f, 1);
	PyObject *range = Sf_Op(&f, maxHull, '-', minHull);
	PyObject *phase = Sf_Op(&f, Sf_ToFloat(&f, Sf_Op(&f, HT_Ship(&f, "MaxLife"), '-', minHull)), '/', Sf_ToFloat(&f, range));
	Sf_ScorerSet(&f, id, "HullFill", "ScaleX", Sf_OpI(&f, phase, '*', 530));
	Sf_ScorerSet(&f, id, "HullSlider", "Value", HT_Ship(&f, "Life"));
	Sf_ScorerSet(&f, id, "HullText", "Text", Sf_Repr(&f, HT_Ship(&f, "MaxLife")));
	Sf_Own(&f, "HullTab", "TestArrows", "(i)", id);
	return Sf_None(&f);
}

/* HullSliderChange(id, control): the plates the slider asks for; it stops where none more could be put on */
static PyObject *HT_HullSliderChange(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	PyObject *value = Sf_ToInt(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "HullSlider", "Value"));
	PyObject *diff = Sf_Op(&f, value, '-', HT_Ship(&f, "MaxLife"));
	PyObject *zero = Sf_Int(&f, 0);
	if (Sf_Compare(&f, diff, "!=", zero) > 0) {
		PyObject *ret = Sf_Own(&f, "HullTab", "ChangeHull", "(iO)", id, diff);
		if (Sf_Compare(&f, diff, ">", zero) > 0 && Sf_Compare(&f, ret, "==", zero) > 0) {
			Sf_ScorerSet(&f, id, "HullSlider", "MaxValueForced", HT_Ship(&f, "MaxLife"));
		}
	}
	return Sf_None(&f);
}

/* SetHullPanel(id): the items of the tab, hidden */
static PyObject *HT_SetHullPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "HangarTab");
	Sf_From(&f, "Menu", "LinkUD");
	Sf_From(&f, "Menu", "LinkLR");
	HT_Run(&f, id, "panel", NULL);
	Sf_Own(&f, "HullTab", "SetHullVisible", "(ii)", id, 0);
	return Sf_None(&f);
}

/* ShowHullPanel(id): the tab with the hull of the ship being edited */
static PyObject *HT_ShowHullPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_From(&f, "Menu", "LinkLR");
	Sf_From(&f, "Menu", "LinkUD");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	Sf_Own(&f, "HullTab", "SetHullVisible", "(ii)", id, 1);
	Sf_ScorerSet(&f, id, "HullText", "Text", Sf_Repr(&f, HT_Ship(&f, "MaxLife")));
	Sf_ScorerSet(&f, id, "HullSlider", "MinValue", HT_Ton(&f, 0));
	Sf_ScorerSet(&f, id, "HullSlider", "MaxValue", HT_Ton(&f, 1));
	Sf_ScorerNum(&f, id, "HullSlider", "Value", 0);
	Sf_ScorerNum(&f, id, "HullSlider", "MaxValueForced", 0);
	Sf_Own(&f, "HullTab", "SetHullState", "(i)", id);
	return Sf_None(&f);
}

/* SetHullVisible(id, bVisible) */
static PyObject *HT_SetHullVisible(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *v;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &v)) {
		return NULL;
	}
	if (!PyInt_Check(v) && !PyFloat_Check(v) && !PyString_Check(v)) {
		/* the TypeError of the first SScorer.Set */
		Sf_ScorerSet(&f, id, "HullSection", "Visible", v);
	}
	HT_Run(&f, id, "visible", Sf_Build(&f, "{s:O}", "v", v));
	return Sf_None(&f);
}

/* HideHullPanel(id) */
static PyObject *HT_HideHullPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	HT_Run(&f, id, "hide", NULL);
	Sf_Own(&f, "HullTab", "SetHullVisible", "(ii)", id, 0);
	Sf_Line(&f, Sf_From(&f, "Menu", "UnLinkUD"), "(iss)", id, "HullDown", "Exit");
	return Sf_None(&f);
}

PyMethodDef g_HullTabMethods[] = {
	{ "ShowHullPredictionUp", HT_ShowHullPredictionUp, 1, NULL },
	{ "HideHullPredictionUp", HT_HideHullPredictionUp, 1, NULL },
	{ "ShowHullPredictionDown", HT_ShowHullPredictionDown, 1, NULL },
	{ "HideHullPredictionDown", HT_HideHullPredictionDown, 1, NULL },
	{ "TestArrows", (PyCFunction)(void (*)(void))HT_TestArrows, 3, NULL },
	{ "IncrementHull", HT_IncrementHull, 1, NULL },
	{ "DecrementHull", HT_DecrementHull, 1, NULL },
	{ "ChangeHull", HT_ChangeHull, 1, NULL },
	{ "SetHullState", HT_SetHullState, 1, NULL },
	{ "HullSliderChange", HT_HullSliderChange, 1, NULL },
	{ "SetHullPanel", HT_SetHullPanel, 1, NULL },
	{ "ShowHullPanel", HT_ShowHullPanel, 1, NULL },
	{ "SetHullVisible", HT_SetHullVisible, 1, NULL },
	{ "HideHullPanel", HT_HideHullPanel, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
