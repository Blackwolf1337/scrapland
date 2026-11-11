/*
 * The HullTab module in C (scripts/shipedit/hulltab.py, issue #2): the hull tab of the ship editor, where the player
 * puts hull plates on the ship or takes them off for money (ShipMaker.armorPieceCost each, ShipEdit.armorPieceWeight
 * of mass each, up to the hull and mass limits of ShipMaker.ShipTon). The layout of the tab is data/menus/hulltab.def.
 * ShipEdit, HangarTab and ShipMaker are called through their modules. The colours and UseTxt stay variables of the
 * module.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values and set the scorer items in C; the
 * PyMethodDef entries are adapters. The variables of the other modules are read where the script reads them
 * (HangarTab.prevShip, the ship being edited, as a handle value: OBJLIB 5 and 7; ShipMaker.ShipTon and the costs,
 * ShipEdit.armorPieceWeight). HangarTab, ShipEdit and ShipMaker are called through their refs (PORT_CALL: directly
 * while the attribute holds the port's function, else by name), the module's own functions through its globals as
 * the script calls them (PORT_OWN, the same seam on the global). The link functions of Menu come from a from import
 * (OBJLIB 6) and are called through their entries while they are Menu's own (held as values, OBJLIB 7:
 * Mode_LinkCallV); the .def sections get their $names as values (Mode_DefRun; OBJLIB 6: a dict inside the runner).
 */
#include <limits.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"

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
static void HT_StepHint(int id, const nativeVal_t *args, int n) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lang = Mode_V(&t), *use = Mode_V(&t), *hint = Mode_V(&t);
	const char *s;
	/* the first value of the line (args[0] of the script) */
	byte ok = (n > 0 || Native_Raise(NE_INDEX, "tuple index out of range")) && Native_ArgStr(&args[0], 1, &s);
	if (ok) {
		Mode_Lang(s, lang);
		ok = Mode_G("HullTab", "UseTxt", use) && Native_Arith('%', lang, use, hint) &&
			Mode_ScorerPut(id, Script_CurrentItem(), "Hint", hint);
	}
	Mode_Done(&t, ok);
}

static const scriptStep_t hulltab_steps[] = {
	{ "usehint", HT_StepHint },
	{ NULL, NULL },
};

/* a section of hulltab.def with its $names, n pairs of a name and a value (Mode_DefRun; OBJLIB 6: the runner takes
 * them as a dict): 0 with the error of the file */
#define HT_Run(id, section, n, ...) \
	Mode_DefRun(id, HULLTAB_FILE, section, hulltab_def, hulltab_steps, 1, n __VA_OPT__(, ) __VA_ARGS__)

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

/* ShowHullPrediction(id, control, lostfocus) of the arrow up (up 1) or down: the mass and boost after one plate more
 * or less, and its cost or the money back */
static byte HT_ShowPrediction(int id, byte up) {
	static const char *const mods[] = { "ShipMaker", "ShipEdit", "HangarTab", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *am = Mode_V(&t), *mass = Mode_V(&t), *pw = Mode_V(&t), *weight = Mode_V(&t), *cost = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *rep = Mode_V(&t), *text = Mode_V(&t);
	if (!Script_Import(mods)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, up ? "WeightUp" : "WeightDown", "Visible", 1);
	Script_SetNum(id, "WeightBack", "Visible", 1);
	if (!Mode_ShipGet("AcMass", am) || !Native_ToInt(am, mass) || !Mode_ModVar("ShipEdit", "armorPieceWeight", pw) ||
		!Native_Arith(up ? '+' : '-', mass, pw, weight) ||
		!PORT_CALL(HangarTab, ShowBoostPrediction, NULL, "iv", (long)id, weight)) {
		return Mode_Done(&t, 0);
	}
	int green = 255;
	if (up) {
		Native_SetInt(money, Mode_GetMoney());
		if (!Mode_ModVar("ShipMaker", "armorPieceCost", cost)) {
			return Mode_Done(&t, 0);
		}
		green = Native_Compare(money, cost) < 0 ? 0 : 255;
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
	}
	Script_SetNum(id, "MoneyPredict", "Green", green);
	Native_Free(cost);
	if (!Mode_ModVar("ShipMaker", "armorPieceCost", cost)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(cost, rep);
	if (!Native_Arith('+', Mode_S(&t, up ? "-" : "+"), rep, text) || !Mode_ScorerPut(id, "MoneyPredict", "Text", text)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "MoneyPredict", "Visible", 1);
	return Mode_Done(&t, 1);
}

MODE_IFN(HT_ShowHullPredictionUp, 3, HT_ShowPrediction(id, 1))
MODE_IFN(HT_HideHullPredictionUp, 3, (HT_HidePrediction(id, "WeightUp"), 1))
MODE_IFN(HT_ShowHullPredictionDown, 3, HT_ShowPrediction(id, 0))
MODE_IFN(HT_HideHullPredictionDown, 3, (HT_HidePrediction(id, "WeightDown"), 1))

/* prevShip.MaxLife == prevShip.MinLife (other 1), or prevShip.MaxLife == ShipTon[...][1] (other 0): 1 or 0, -1 with
 * the error */
static int HT_AtLimit(byte min) {
	nativeVal_t a = { 0 }, b = { 0 };
	int r = -1;
	if (Mode_ShipGet("MaxLife", &a) && (min ? Mode_ShipGet("MinLife", &b) : Mode_ShipTable("ShipTon", 1, &b))) {
		r = !Native_Compare(&a, &b);
		if (Native_Failed()) {
			r = -1;
		}
	}
	Native_Free(&a);
	Native_Free(&b);
	return r;
}

/* the arrow sizes and sprite of one hull arrow */
static void HT_Arrow(int id, const char *item, int size, int sprite) {
	Script_SetNum(id, item, "W", size);
	Script_SetNum(id, item, "H", size);
	Script_SetNum(id, item, "SpriteIndex", sprite);
}

/* TestArrows(id, changeDefault=1): an arrow disappears at the hull limits, the links follow */
static byte HT_Arrows(int id, const nativeVal_t *def) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "string", NULL };
	if (!Script_Import(mods)) {
		return 0;
	}
	nativeVal_t linkUD = { 0 }, linkLR = { 0 }, unlinkLR = { 0 };
	byte ok = Mode_FromV("Menu", "LinkUD", &linkUD) && Mode_FromV("Menu", "LinkLR", &linkLR) &&
		Mode_FromV("Menu", "UnLinkLR", &unlinkLR) && Mode_LinkCallV(&unlinkLR, id, "HullDown", "HullUp");
	int atMin = ok ? HT_AtLimit(1) : -1, atMax = atMin == 0 ? HT_AtLimit(0) : 0;
	ok = atMin >= 0 && atMax >= 0;
	if (ok && atMin) {
		if (Native_True(def)) {
			Script_SetDefault(id, "HullUp");
		}
		HT_Arrow(id, "HullDown", 0, 2);
		ok = Mode_LinkCallV(&linkUD, id, "ShipEditTabHull", "HullUp") &&
			Mode_LinkCallV(&linkUD, id, "HullUp", "Exit") && Mode_LinkCallV(&unlinkLR, id, "HullDown", "HullUp");
		if (ok) {
			Script_SetStr(id, "Exit", "Up", "HullUp");
		}
	} else if (ok && atMax) {
		if (Native_True(def)) {
			Script_SetDefault(id, "HullDown");
		}
		HT_Arrow(id, "HullUp", 0, 2);
		ok = Mode_LinkCallV(&linkUD, id, "ShipEditTabHull", "HullDown") &&
			Mode_LinkCallV(&linkUD, id, "HullDown", "Exit");
		if (ok) {
			Script_SetStr(id, "Exit", "Up", "HullDown");
		}
	} else if (ok) {
		HT_Arrow(id, "HullUp", 25, 3);
		HT_Arrow(id, "HullDown", 25, 3);
		ok = Mode_LinkCallV(&linkLR, id, "HullDown", "HullUp") &&
			Mode_LinkCallV(&linkUD, id, "ShipEditTabHull", "HullUp") && Mode_LinkCallV(&linkUD, id, "HullUp", "Exit");
		if (ok) {
			Script_SetStr(id, "Exit", "Up", "HullUp");
		}
	}
	if (ok) {
		Script_SetStr(id, "HullDown", "Up", "ShipEditTabHull");
		Script_SetStr(id, "HullDown", "Down", "Exit");
		Script_SetStr(id, "HullUp", "Up", "ShipEditTabHull");
		Script_SetStr(id, "HullUp", "Down", "Exit");
	}
	Native_Free(&linkUD);
	Native_Free(&linkLR);
	Native_Free(&unlinkLR);
	return ok;
}

static byte HT_TestArrows_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *change = NULL;
	nativeVal_t def;
	int id;
	(void)out;
	Native_SetInt(&def, 1);
	return Mode_Parse(a, n, "i|O", &id, &change) && HT_Arrows(id, change ? change : &def);
}

/* OBJLIB 7: a keyword adapter (the parameter names and the errors of PyArg_ParseTupleAndKeywords) */
static PyObject *HT_TestArrows(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "changeDefault", NULL };
	(void)self;
	return Mode_KwIdEntry(args, kw, "i|O", names, HT_TestArrows_E);
}

/* IncrementHull(id, control) and DecrementHull(id, control): one plate on or off, then the boost prediction */
static byte HT_StepHull(int id, int step) {
	static const char *const mods[] = { "HangarTab", "ShipEdit", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *am = Mode_V(&t), *pw = Mode_V(&t), *weight = Mode_V(&t);
	byte ok = Script_Import(mods) && PORT_OWN(HullTab, ChangeHull, NULL, "ii", (long)id, (long)step) &&
		Mode_ShipGet("AcMass", am) && Mode_ModVar("ShipEdit", "armorPieceWeight", pw) &&
		Native_Arith(step > 0 ? '+' : '-', am, pw, weight) &&
		PORT_CALL(HangarTab, ShowBoostPrediction, NULL, "iv", (long)id, weight);
	return Mode_Done(&t, ok);
}

MODE_IFN(HT_IncrementHull, 2, HT_StepHull(id, 1))
MODE_IFN(HT_DecrementHull, 2, HT_StepHull(id, -1))

/* ShipEdit.IsEditingMultiplayer() != 1: 1 or 0, -1 with the error */
static int HT_Single(void) {
	nativeVal_t v = { 0 }, one;
	Native_SetInt(&one, 1);
	if (!PORT_CALL(ShipEdit, IsEditingMultiplayer, &v, "")) {
		return -1;
	}
	int c = Native_Compare(&v, &one) != 0;
	Native_Free(&v);
	return Native_Failed() ? -1 : c;
}

/* the hull after the change: MaxLife op value, Life = MaxLife, the ship parameters, the info and the state */
static byte HT_Apply(int id, char op, const nativeVal_t *value) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ml = Mode_V(&t), *nl = Mode_V(&t), *life = Mode_V(&t), *name = Mode_V(&t);
	byte ok = Mode_ShipGet("MaxLife", ml) && Native_Arith(op, ml, value, nl) && Mode_ShipSet("MaxLife", nl) &&
		Mode_ShipGet("MaxLife", life) && Mode_ShipSet("Life", life) && Mode_ShipGet("Name", name) &&
		PORT_CALL(ShipMaker, SetShipParams, NULL, "v", name) &&
		PORT_CALL(HangarTab, ShowShipInfo, NULL, "i", (long)id) &&
		PORT_OWN(HullTab, SetHullState, NULL, "i", (long)id);
	return Mode_Done(&t, ok);
}

/* a > b of the script: 1 or 0, -1 with the error */
static int HT_Gt(const nativeVal_t *a, const nativeVal_t *b) {
	int c = Native_Compare(a, b) > 0;
	return Native_Failed() ? -1 : c;
}

/* plates taken off (value > 0 of them): as many as there are above the minimum; the money back */
static byte HT_TakeOff(int id, nativeVal_t *value, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ml = Mode_V(&t), *mn = Mode_V(&t), *room = Mode_V(&t), *cost = Mode_V(&t), *back = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *sum = Mode_V(&t), *rep = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_ShipGet("MaxLife", ml) || !Mode_ShipGet("MinLife", mn)) {
		return Mode_Done(&t, 0);
	}
	int none = Native_Compare(ml, mn) <= 0;
	if (Native_Failed() || none) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, !Native_Failed());
	}
	Native_Free(ml);
	Native_Free(mn);
	if (!Mode_ShipGet("MaxLife", ml) || !Mode_ShipGet("MinLife", mn) || !Native_Arith('-', ml, mn, room)) {
		return Mode_Done(&t, 0);
	}
	int more = HT_Gt(value, room);
	if (more < 0) {
		return Mode_Done(&t, 0);
	}
	if (more) {
		Native_Free(ml);
		Native_Free(mn);
		Native_Free(value);
		if (!Mode_ShipGet("MaxLife", ml) || !Mode_ShipGet("MinLife", mn) || !Native_Arith('-', ml, mn, value)) {
			return Mode_Done(&t, 0);
		}
	}
	int any = HT_Gt(value, zero);
	if (any <= 0) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, any == 0);
	}
	int single = HT_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		Native_SetInt(money, Mode_GetMoney());
		if (!Mode_ModVar("ShipMaker", "armorPieceCost", cost) || !Native_Arith('*', cost, value, back) ||
			!Native_Arith('+', money, back, sum)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(sum, rep);
		if (!Mode_ScorerPut(id, "Money", "Text", rep) || !Mode_SetMoney(sum)) {
			return Mode_Done(&t, 0);
		}
	}
	nativeVal_t *neg = Mode_V(&t);
	return Mode_Done(&t, HT_Apply(id, '-', value) && Native_Arith('-', zero, value, neg) && (Native_Copy(out, neg), 1));
}

/* plates put on (value > 0 of them): as many as the mass limit and the money allow */
static byte HT_PutOn(int id, nativeVal_t *value, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *top = Mode_V(&t), *am = Mode_V(&t), *left = Mode_V(&t), *pw = Mode_V(&t), *pieces = Mode_V(&t);
	nativeVal_t *cost = Mode_V(&t), *money = Mode_V(&t), *q = Mode_V(&t), *byMoney = Mode_V(&t), *pay = Mode_V(&t);
	nativeVal_t *rest = Mode_V(&t), *rep = Mode_V(&t), *cost2 = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_ShipTable("ShipTon", 2, top) || !Mode_ShipGet("AcMass", am) || !Native_Arith('-', top, am, left) ||
		!Mode_ModVar("ShipEdit", "armorPieceWeight", pw) || !Native_Arith('/', left, pw, pieces)) {
		return Mode_Done(&t, 0);
	}
	int more = HT_Gt(value, pieces);
	if (more < 0 || (more && (Native_Free(value), !Native_ToInt(pieces, value)))) {
		return Mode_Done(&t, 0);
	}
	int single = HT_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		Native_SetInt(money, Mode_GetMoney());
		if (!Mode_ModVar("ShipMaker", "armorPieceCost", cost) || !Native_Arith('/', money, cost, q) || !Native_ToInt(q, byMoney)) {
			return Mode_Done(&t, 0);
		}
		more = HT_Gt(value, byMoney);
		if (more < 0) {
			return Mode_Done(&t, 0);
		}
		if (more) {
			Native_Free(value);
			Native_Copy(value, byMoney);
		}
	}
	int any = HT_Gt(value, zero);
	if (any <= 0) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, any == 0);
	}
	single = HT_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		Native_Free(cost);
		Native_SetInt(money, Mode_GetMoney());
		if (!Mode_ModVar("ShipMaker", "armorPieceCost", cost) || !Native_Arith('*', cost, value, pay) ||
			!Native_Arith('-', money, pay, rest)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(rest, rep);
		if (!Mode_ScorerPut(id, "Money", "Text", rep) || !Mode_ModVar("ShipMaker", "armorPieceCost", cost2)) {
			return Mode_Done(&t, 0);
		}
		int poor = Native_Compare(rest, cost2) < 0;
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
		if (poor) {
			Script_SetNum(id, "MoneyPredict", "Green", 0);
		}
		if (!Mode_SetMoney(rest)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, HT_Apply(id, '+', value) && (Native_Copy(out, value), 1));
}

/* ChangeHull(id, value): value plates on (or off when negative), as many as the limits and the money allow; the
 * number changed (negative for plates taken off) */
static byte HT_ChangeHull(int id, const nativeVal_t *v, nativeVal_t *out) {
	static const char *const mods[] = { "ShipMaker", "ShipEdit", "HangarTab", "string", NULL };
	nativeVal_t zero, value = { 0 };
	if (!Script_Import(mods)) {
		return 0;
	}
	Native_SetInt(&zero, 0);
	int neg = Native_Compare(v, &zero) < 0;
	if (Native_Failed()) {
		return 0;
	}
	byte ok;
	if (neg) {
		/* -value (PyNumber_Negative: the overflow of the smallest int) */
		ok = v->type == NV_INT && v->i == LONG_MIN ? Native_Raise(NE_OVERFLOW, "integer negation") :
			Native_Arith('-', &zero, v, &value) && HT_TakeOff(id, &value, out);
	} else {
		int pos = HT_Gt(v, &zero);
		Native_Copy(&value, v);
		ok = pos > 0 ? HT_PutOn(id, &value, out) : (Native_SetInt(out, 0), pos == 0);
	}
	Native_Free(&value);
	return ok;
}

MODE_IFN(HT_ChangeHullPy, 2, HT_ChangeHull(id, &a[1], out))

/* SetHullState(id): the colour and number of the hull text, the bar, the slider and the arrows */
static byte HT_SetHullState(int id) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *life = Mode_V(&t), *minl = Mode_V(&t), *minh = Mode_V(&t), *maxh = Mode_V(&t);
	nativeVal_t *range = Mode_V(&t), *ml = Mode_V(&t), *d = Mode_V(&t), *df = Mode_V(&t), *rf = Mode_V(&t);
	nativeVal_t *phase = Mode_V(&t), *scale = Mode_V(&t), *val = Mode_V(&t), *rep = Mode_V(&t);
	if (!Script_Import(mods)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_PrevShip(ship)) {
		return Mode_Done(&t, 0);
	}
	int some = Native_Compare(ship, Mode_V(&t)) != 0;
	if (Native_Failed()) {
		return Mode_Done(&t, 0);
	}
	if (some) {
		if (!Mode_ShipGet("Life", life) || !Mode_ShipGet("MinLife", minl)) {
			return Mode_Done(&t, 0);
		}
		int above = Native_Compare(life, minl) > 0;
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
		Script_SetNum(id, "HullText", "TextColorRed", above ? HULL_RED : HULL_MIN_RED);
		Script_SetNum(id, "HullText", "TextColorGreen", above ? HULL_GREEN : HULL_MIN_GREEN);
		Script_SetNum(id, "HullText", "TextColorBlue", above ? HULL_BLUE : HULL_MIN_BLUE);
	}
	byte ok = Mode_ShipTable("ShipTon", 0, minh) && Mode_ShipTable("ShipTon", 1, maxh) &&
		Native_Arith('-', maxh, minh, range) && Mode_ShipGet("MaxLife", ml) && Native_Arith('-', ml, minh, d) &&
		Native_ToFloat(d, df) && Native_ToFloat(range, rf) && Native_Arith('/', df, rf, phase) &&
		Native_Arith('*', phase, Mode_I(&t, 530), scale) && Mode_ScorerPut(id, "HullFill", "ScaleX", scale) &&
		Mode_ShipGet("Life", val) && Mode_ScorerPut(id, "HullSlider", "Value", val);
	Native_Free(ml);
	ok = ok && Mode_ShipGet("MaxLife", ml);
	if (ok) {
		Mode_Repr(ml, rep);
	}
	ok = ok && Mode_ScorerPut(id, "HullText", "Text", rep) && PORT_OWN(HullTab, TestArrows, NULL, "i", (long)id);
	return Mode_Done(&t, ok);
}

MODE_IFN(HT_SetHullStatePy, 1, HT_SetHullState(id))

/* HullSliderChange(id, control): the plates the slider asks for; it stops where none more could be put on */
static byte HT_SliderChange(int id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *got = Mode_V(&t), *value = Mode_V(&t), *ml = Mode_V(&t), *diff = Mode_V(&t), *ret = Mode_V(&t);
	nativeVal_t *forced = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("HangarTab")) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(id, "HullSlider", "Value", got);
	if (!Native_ToInt(got, value) || !Mode_ShipGet("MaxLife", ml) || !Native_Arith('-', value, ml, diff)) {
		return Mode_Done(&t, 0);
	}
	int change = Native_Compare(diff, zero) != 0;
	if (Native_Failed()) {
		return Mode_Done(&t, 0);
	}
	if (!change) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_OWN(HullTab, ChangeHull, ret, "iv", (long)id, diff)) {
		return Mode_Done(&t, 0);
	}
	int up = HT_Gt(diff, zero);
	int stuck = up > 0 ? !Native_Compare(ret, zero) : 0;
	if (up < 0 || Native_Failed()) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !stuck ||
		(Mode_ShipGet("MaxLife", forced) && Mode_ScorerPut(id, "HullSlider", "MaxValueForced", forced)));
}

MODE_IFN(HT_HullSliderChange, 2, HT_SliderChange(id))

/* SetHullPanel(id): the items of the tab, hidden */
static byte HT_SetPanel(int id) {
	static const char *const mods[] = { "string", "ShipMaker", "HangarTab", NULL };
	/* from Menu import LinkUD, LinkLR (not called here) */
	nativeVal_t a = { 0 }, b = { 0 };
	byte ok = Script_Import(mods) && Mode_FromV("Menu", "LinkUD", &a) && Mode_FromV("Menu", "LinkLR", &b) &&
		HT_Run(id, "panel", 0) && PORT_OWN(HullTab, SetHullVisible, NULL, "ii", (long)id, 0L);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

MODE_IFN(HT_SetHullPanel, 1, HT_SetPanel(id))

/* ShowHullPanel(id): the tab with the hull of the ship being edited */
static byte HT_ShowPanel(int id) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ml = Mode_V(&t), *rep = Mode_V(&t), *lo = Mode_V(&t), *hi = Mode_V(&t);
	/* from Menu import LinkLR, LinkUD (not called here) */
	nativeVal_t a = { 0 }, b = { 0 };
	byte ok = Mode_FromV("Menu", "LinkLR", &a) && Mode_FromV("Menu", "LinkUD", &b) && Script_Import(mods) &&
		PORT_OWN(HullTab, SetHullVisible, NULL, "ii", (long)id, 1L) && Mode_ShipGet("MaxLife", ml);
	if (ok) {
		Mode_Repr(ml, rep);
	}
	ok = ok && Mode_ScorerPut(id, "HullText", "Text", rep) && Mode_ShipTable("ShipTon", 0, lo) &&
		Mode_ScorerPut(id, "HullSlider", "MinValue", lo) && Mode_ShipTable("ShipTon", 1, hi) &&
		Mode_ScorerPut(id, "HullSlider", "MaxValue", hi);
	if (ok) {
		Script_SetNum(id, "HullSlider", "Value", 0);
		Script_SetNum(id, "HullSlider", "MaxValueForced", 0);
	}
	ok = ok && PORT_OWN(HullTab, SetHullState, NULL, "i", (long)id);
	Native_Free(&a);
	Native_Free(&b);
	return Mode_Done(&t, ok);
}

MODE_IFN(HT_ShowHullPanel, 1, HT_ShowPanel(id))

/* SetHullVisible(id, bVisible) */
static byte HT_SetVisible(int id, const nativeVal_t *v) {
	/* the TypeError of the first SScorer.Set for another value (a long int it takes) */
	if (v->type != NV_INT && v->type != NV_FLOAT && v->type != NV_STR && !Mode_ScorerPut(id, "HullSection", "Visible", v)) {
		return 0;
	}
	return HT_Run(id, "visible", 1, "v", v);
}

MODE_IFN(HT_SetHullVisible, 2, HT_SetVisible(id, &a[1]))

/* HideHullPanel(id) */
static byte HT_HidePanel(int id) {
	nativeVal_t unlink = { 0 };
	byte ok = HT_Run(id, "hide", 0) && PORT_OWN(HullTab, SetHullVisible, NULL, "ii", (long)id, 0L) &&
		Mode_FromV("Menu", "UnLinkUD", &unlink) && Mode_LinkCallV(&unlink, id, "HullDown", "Exit");
	Native_Free(&unlink);
	return ok;
}

MODE_IFN(HT_HideHullPanel, 1, HT_HidePanel(id))

PyMethodDef g_HullTabMethods[] = {
	{ "ShowHullPredictionUp", HT_ShowHullPredictionUp, 1, NULL },
	{ "HideHullPredictionUp", HT_HideHullPredictionUp, 1, NULL },
	{ "ShowHullPredictionDown", HT_ShowHullPredictionDown, 1, NULL },
	{ "HideHullPredictionDown", HT_HideHullPredictionDown, 1, NULL },
	{ "TestArrows", (PyCFunction)(void (*)(void))HT_TestArrows, 3, NULL },
	{ "IncrementHull", HT_IncrementHull, 1, NULL },
	{ "DecrementHull", HT_DecrementHull, 1, NULL },
	{ "ChangeHull", HT_ChangeHullPy, 1, NULL },
	{ "SetHullState", HT_SetHullStatePy, 1, NULL },
	{ "HullSliderChange", HT_HullSliderChange, 1, NULL },
	{ "SetHullPanel", HT_SetHullPanel, 1, NULL },
	{ "ShowHullPanel", HT_ShowHullPanel, 1, NULL },
	{ "SetHullVisible", HT_SetHullVisible, 1, NULL },
	{ "HideHullPanel", HT_HideHullPanel, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(HullTab)
