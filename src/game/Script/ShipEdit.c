/*
 * The ShipEdit module in C (scripts/shipedit/shipedit.py, issue #2): the ship editor of the junkyard, of the ShipEdit
 * level (the garage of the network games) and of Sputnik's hangar screen. The platform that brings up the ship of the
 * selected hangar (the door class EditShip, its Change animation and Rusty on it), the camera, the ships of the nine
 * hangars and the emergency ship made from the save variables Hangar.HangarShip1-9 and Hangar.HangarShipAux (or the
 * engine variables HangarShip1-9 in a network game), the tabs (Hangar, Engines, Weapons, Hull) and the turning ship,
 * entering and leaving the editor and saving the ships. The tabs and the background are data/menus/shipedit.def.
 *
 * The module variables stay in the script, the panels and the levels read and set them: editedShips, destroyedShips,
 * fromSputnik, prevTab, tabs, camera, lastCam, editShipPosDummy, editPlatform, EditCameraPitch, CameraDist,
 * shipsToEditList, auxShipsToEditList, availableWeaponsList, availableUpgradesList, shipsEnginesLayouts, tabPixelX,
 * armorPieceWeight, ctnRotating, isRotating, EndShipEditCallBack, isXbox, UseTxt and the constants. The module code
 * (the save variables of the hangars) stays in the script. EnginesTab, WeaponsTab and HullTab stay Python and are
 * called through their modules.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/scrap.h"
#include "game/Script/levels/Levels.h"
#include "ShipEdit.h"

#define G(name) Sf_Global(&f, SE, name)
#define GF(name) Sf_Global(f, SE, name)
#define HTA(name) Se_Mod(&f, HT, name)

static const char shipedit_def[] = {
#embed "../../../data/menus/shipedit.def"
	, 0 };

/* ---- shared with HangarTab.c ---- */

void Se_Join(scriptFrame_t *f, scriptFrame_t *g) {
	Sf_Release(g, 0);
	if (g->failed) {
		f->failed = 1;
	}
}

PyObject *Se_JoinValue(scriptFrame_t *f, scriptFrame_t *g, PyObject *v) {
	Py_XINCREF(v);
	Se_Join(f, g);
	if (f->failed) {
		Py_XDECREF(v);
		return NULL;
	}
	return Sf_Keep(f, v);
}

void Se_SetI(scriptFrame_t *f, int id, const char *item, const char *var, double v) {
	if (!f->failed) {
		Script_SetNum(id, item, var, v);
	}
}

void Se_SetS(scriptFrame_t *f, int id, const char *item, const char *var, const char *s) {
	if (!f->failed) {
		Script_SetStr(id, item, var, s);
	}
}

void Se_SetOO(scriptFrame_t *f, int id, PyObject *item, const char *var, PyObject *v) {
	if (f->failed || !item || !v) {
		Sf_Check(f, 0);
		return;
	}
	PyObject *r = Script_Ask("SScorer", "Set", "(iOsO)", id, item, var, v);
	Sf_Check(f, r != NULL);
	Py_XDECREF(r);
}

void Se_SetO(scriptFrame_t *f, int id, const char *item, const char *var, PyObject *v) {
	if (f->failed || !v) {
		Sf_Check(f, 0);
		return;
	}
	PyObject *r = Script_Ask("SScorer", "Set", "(issO)", id, item, var, v);
	Sf_Check(f, r != NULL);
	Py_XDECREF(r);
}

void Se_SetLang(scriptFrame_t *f, int id, const char *item, const char *var, const char *key) {
	Se_SetS(f, id, item, var, f->failed ? "" : Script_Lang(key));
}

void Se_SetUse(scriptFrame_t *f, int id, PyObject *item, const char *var, const char *key, const char *module) {
	if (f->failed) {
		return;
	}
	PyObject *text = PyString_FromString((char *)Script_Lang(key)), *use = Script_Global(module, "UseTxt");
	PyObject *v = use ? Script_Mod(text, use) : NULL;
	Py_XDECREF(text);
	Py_XDECREF(use);
	if (!Sf_Check(f, v != NULL)) {
		return;
	}
	Se_SetOO(f, id, item, var, v);
	Py_DECREF(v);
}

void Se_SetAll(scriptFrame_t *f, int id, const char *const *items, const char *var, PyObject *v) {
	for (; *items && !f->failed; items++) {
		Se_SetO(f, id, *items, var, v);
	}
}

void Se_Link(scriptFrame_t *f, const char *fn, int id, const char *a, const char *b) {
	if (!f->failed) {
		PyObject *r = Script_Ask("Menu", fn, "(iss)", id, a, b);
		Sf_Check(f, r != NULL);
		Py_XDECREF(r);
	}
}

void Se_LinkO(scriptFrame_t *f, const char *fn, int id, PyObject *a, PyObject *b) {
	if (f->failed || !a || !b) {
		Sf_Check(f, 0);
		return;
	}
	PyObject *r = Script_Ask("Menu", fn, "(iOO)", id, a, b);
	Sf_Check(f, r != NULL);
	Py_XDECREF(r);
}

void Se_TabRing(scriptFrame_t *f, int id, PyObject *tabs, const char *fn) {
	/* for i in range(len(tabs)): fn(id, "ShipEditTab" + tabs[i], "ShipEditTab" + tabs[(i+1)%len(tabs)]) */
	int n = Sf_Len(f, tabs);
	for (int i = 0; i < n && !f->failed; i++) {
		int mark = f->n;
		PyObject *a = Sf_Cat(f, "sO", "ShipEditTab", Sf_Item(f, tabs, i));
		int len = Sf_Len(f, tabs);
		PyObject *b = len > 0 ? Sf_Cat(f, "sO", "ShipEditTab", Sf_Item(f, tabs, (i + 1) % len)) : NULL;
		Se_LinkO(f, fn, id, a, b);
		Sf_Release(f, f->failed ? f->n : mark);
	}
}

void Se_TabsOn(scriptFrame_t *f, int id, PyObject *tabs, byte on) {
	/* for t in tabs[1:]: tabName = "ShipEditTab" + t ... */
	PyObject *rest = Sf_Slice(f, tabs, 1, 0x7fffffff);
	for (int i = 0; !f->failed; i++) {
		int mark = f->n;
		PyObject *t = Sf_Next(f, rest, i);
		if (!t) {
			break;
		}
		PyObject *name = Sf_Cat(f, "sO", "ShipEditTab", t);
		static const char *const vars[] = { "Highlight", "Alpha", "TextColorAlpha", "TextColorRed", "TextColorGreen",
			"TextColorBlue" };
		const double vals[] = { on, 255, 255, on ? 255 : 161, on ? 255 : 161, on ? 255 : 161 };
		for (int k = 0; k < 6 && !f->failed; k++) {
			PyObject *r = name ? Script_Ask("SScorer", "Set", "(iOsd)", id, name, vars[k], vals[k]) : NULL;
			Sf_Check(f, r != NULL);
			Py_XDECREF(r);
		}
		Se_SetOO(f, id, name, "OnAccept", Sf_Str(f, on ? "ShipEdit.ChangeTab" : ""));
		Sf_Release(f, f->failed ? f->n : mark);
	}
}

int Se_Multi(scriptFrame_t *f, byte own) {
	return Sf_True(f, own ? Sf_Own(f, SE, "IsEditingMultiplayer", "()") : Sf_Ask(f, SE, "IsEditingMultiplayer", "()"));
}

PyObject *Se_Mod(scriptFrame_t *f, const char *module, const char *name) {
	return Sf_Mod(f, module, name);
}

/* the module whose UseTxt the step use formats with */
static const char *se_useModule = SE;

/* run use <Attr> <key> [item]: Attr = GetLangStr(key) % UseTxt (of the current item, or of item) */
static void Step_Use(int id, PyObject *args) {
	scriptFrame_t f = { 0 };
	char *var, *key, *item = NULL;
	if (!PyArg_ParseTuple(args, "ss|s", &var, &key, &item)) {
		return;
	}
	Se_SetUse(&f, id, Sf_Str(&f, item ? item : Script_CurrentItem()), var, key, se_useModule);
	Sf_Done(&f, NULL);
}

/* run sputnikhint <n>: the hint of hangar n on Sputnik's screen (no ship, a destroyed one with the rebuild cost, an
 * incomplete one, a valid one) */
static void Step_SputnikHint(int id, PyObject *args) {
	scriptFrame_t f = { 0 };
	long n;
	if (!PyArg_ParseTuple(args, "l", &n)) {
		return;
	}
	char name[0x20];
	Str_Printf(name, sizeof(name), "Hangar%ld", n);
	PyObject *i = Sf_Int(&f, n - 1);
	int none = Sf_Compare(&f, Sf_GetItem(&f, Sf_Mod(&f, SE, "editedShips"), i), "==", Py_None);
	int lost = 0;
	if (none == 0) {
		lost = Sf_EqI(&f, Sf_GetItem(&f, Sf_Mod(&f, SE, "destroyedShips"), i), 1);
		if (lost > 0) {
			lost = Se_Multi(&f, 0);
			lost = lost < 0 ? -1 : !lost;
		}
	}
	if (none > 0) {
		Se_SetLang(&f, id, name, "Hint", "HINT_Sputnik_NoShip");
	} else if (lost > 0) {
		Sf_Import(&f, "string");
		PyObject *cost = Sf_Own(&f, HT, "ComputeShipTotalCost", "(O)",
			Sf_Cat(&f, "sOs", "ShipEdit.editedShips[", Sf_Repr(&f, i), "]"));
		PyObject *text;
		if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), ">=", cost) > 0) {
			text = Sf_Op(&f, Sf_Str(&f, Script_Lang("HINT_Sputnik_DestroyedShip")), '%', Sf_Global(&f, HT, "UseTxt"));
			text = Sf_Ask(&f, "string", "replace", "(OsO)", text, "<n>", Sf_Repr(&f, cost));
		} else {
			text = Sf_Str(&f, Script_Lang("HINT_Sputnik_CantRebuildShip"));
		}
		Se_SetO(&f, id, name, "Hint", text);
		Se_SetS(&f, id, name, "OnGainFocus", "HangarTab.ShowRebuildShipMoney");
		Se_SetS(&f, id, name, "OnLooseFocus", "HangarTab.HideRebuildShipMoney");
	} else if (lost == 0) {
		int valid = Sf_True(&f, Sf_Own(&f, HT, "IsShipValid", "(O)", Sf_GetItem(&f, Sf_Mod(&f, SE, "editedShips"), i)));
		if (valid == 0) {
			Se_SetLang(&f, id, name, "Hint", "HINT_Sputnik_IncompleteShip");
		} else if (valid > 0) {
			Se_SetUse(&f, id, Sf_Str(&f, name), "Hint", "HINT_Sputnik_ValidShip", HT);
		}
	}
	Sf_Done(&f, NULL);
}

/* run ramp: SpeedRamp2.X = baseShipInfoX + speedRampX + SpeedRamp1.ScaleX * SpeedRamp1.W */
static void Step_Ramp(int id, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)args;
	PyObject *x = Sf_Op(&f, Sf_Global(&f, HT, "baseShipInfoX"), '+', Sf_Global(&f, HT, "speedRampX"));
	PyObject *sx = Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "ScaleX");
	x = Sf_Op(&f, x, '+', Sf_Op(&f, sx, '*', Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "W")));
	Se_SetO(&f, id, "SpeedRamp2", "X", x);
	Sf_Done(&f, NULL);
}

static const scriptStep_t shipedit_steps[] = {
	{ "use", Step_Use },
	{ "sputnikhint", Step_SputnikHint },
	{ "ramp", Step_Ramp },
	{ NULL, NULL },
};

byte Se_Def(scriptFrame_t *f, int id, const char *section, PyObject *vars, const char *module) {
	if (f->failed) {
		return 0;
	}
	const char *outer = se_useModule;
	se_useModule = module;
	byte ok = Script_RunFileTry(id, "menus/shipedit.def", section, shipedit_def, shipedit_steps, vars);
	se_useModule = outer;
	return Sf_Check(f, ok);
}

/* ---- native helpers shared with HangarTab.c ---- */

int Sn_Multi(byte own) {
	nativeVal_t v = { 0 };
	byte ok = own ? Native_CallGlobal(SE, "IsEditingMultiplayer", &v, "") : Native_CallMod(SE, "IsEditingMultiplayer", &v,
		"");
	int r = ok ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

byte Sn_Set(int id, const nativeVal_t *item, const char *var, const nativeVal_t *v) {
	if (item->type == NV_STR && strlen(Native_Text(item)) == (size_t)item->n) {
		return Native_ScorerSet(id, Native_Text(item), var, v);
	}
	return Native_CallMod("SScorer", "Set", NULL, "ivsv", (long)id, item, var, v);
}

byte Sn_SetC(int id, const char *item, const char *var, const nativeVal_t *v) {
	return Native_ScorerSet(id, item, var, v);
}

byte Sn_SetUse(int id, const nativeVal_t *item, const char *var, const char *key, const char *module) {
	nativeVal_t text = { 0 }, use = { 0 }, v = { 0 };
	Mode_Lang(key, &text);
	byte ok = Mode_G(module, "UseTxt", &use) && Native_Arith('%', &text, &use, &v) && Sn_Set(id, item, var, &v);
	Native_Free(&text);
	Native_Free(&use);
	Native_Free(&v);
	return ok;
}

byte Sn_SetUseC(int id, const char *item, const char *var, const char *key, const char *module) {
	nativeVal_t i;
	Native_SetStr(&i, item);
	return Sn_SetUse(id, &i, var, key, module);
}

byte Sn_Link(const char *fn, int id, const char *a, const char *b) {
	return Native_CallMod("Menu", fn, NULL, "iss", (long)id, a, b);
}

byte Sn_LinkV(const char *fn, int id, const nativeVal_t *a, const nativeVal_t *b) {
	return Native_CallMod("Menu", fn, NULL, "ivv", (long)id, a, b);
}

byte Sn_From(const char *name) {
	PyObject *o = Mode_From("Menu", name);
	Py_XDECREF(o);
	return o != NULL;
}

byte Sn_Cat(const char *prefix, const nativeVal_t *v, const char *suffix, nativeVal_t *out) {
	nativeVal_t p, a = { 0 };
	Native_SetStr(&p, prefix);
	Native_SetNone(out);
	/* an empty prefix is no operand: v + suffix */
	byte ok = 1;
	if (*prefix) {
		ok = Native_Arith('+', &p, v, suffix ? &a : out);
	} else {
		Native_Copy(suffix ? &a : out, v);
	}
	if (ok && suffix) {
		Native_SetStr(&p, suffix);
		ok = Native_Arith('+', &a, &p, out);
	}
	Native_Free(&a);
	return ok;
}

byte Sn_Rest(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t c;
	Native_Copy(&c, v);
	Mode_Elems(&c);
	byte ok = Native_Slice(&c, 1, 0x7fffffff, out);
	Native_Free(&c);
	return ok;
}

byte Sn_TabRing(int id, const nativeVal_t *tabs, const char *fn) {
	/* for i in range(len(tabs)): fn(id, "ShipEditTab" + tabs[i], "ShipEditTab" + tabs[(i+1)%len(tabs)]) */
	nativeVal_t l, x = { 0 }, a = { 0 }, b = { 0 };
	Native_Copy(&l, tabs);
	int n = Mode_Len(&l);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = LvN_Item(&l, i, &x) && Sn_Cat("ShipEditTab", &x, NULL, &a);
		Native_Free(&x);
		ok = ok && LvN_Item(&l, (i + 1) % n, &x) && Sn_Cat("ShipEditTab", &x, NULL, &b) && Sn_LinkV(fn, id, &a, &b);
		Native_Free(&x);
		Native_Free(&a);
		Native_Free(&b);
	}
	Native_Free(&l);
	return ok;
}

byte Sn_TabsOn(int id, const nativeVal_t *tabs, byte on) {
	/* for t in tabs[1:]: tabName = "ShipEditTab" + t ... */
	static const char *const vars[] = { "Highlight", "Alpha", "TextColorAlpha", "TextColorRed", "TextColorGreen",
		"TextColorBlue" };
	const double vals[] = { on, 255, 255, on ? 255 : 161, on ? 255 : 161, on ? 255 : 161 };
	nativeVal_t rest = { 0 }, t = { 0 }, name = { 0 }, v = { 0 };
	byte ok = Sn_Rest(tabs, &rest);
	int n = ok ? Native_ForCount(&rest) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&rest, i, &t) && Sn_Cat("ShipEditTab", &t, NULL, &name);
		for (int k = 0; ok && k < 6; k++) {
			Native_SetFloat(&v, vals[k]);
			ok = Sn_Set(id, &name, vars[k], &v);
		}
		Native_SetStr(&v, on ? "ShipEdit.ChangeTab" : "");
		ok = ok && Sn_Set(id, &name, "OnAccept", &v);
		Native_Free(&t);
		Native_Free(&name);
	}
	Native_Free(&rest);
	return ok;
}

byte Sn_At(const char *module, const char *var, const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t l = { 0 };
	Native_SetNone(out);
	byte ok = LvN_ModGet(module, var, &l) && Mode_Sub(&l, key, out);
	Native_Free(&l);
	return ok;
}

byte Sn_FrontAng(const nativeVal_t *dummy, const nativeVal_t *extra, nativeVal_t *out) {
	nativeVal_t ang = { 0 }, a0 = { 0 }, a1 = { 0 }, y = { 0 }, y2 = { 0 }, k;
	Native_SetInt(&k, 180);
	Native_SetNone(out);
	byte ok = LvN_Item(dummy, 2, &ang) && LvN_Item(&ang, 0, &a0) && LvN_Item(&ang, 1, &a1) && Mode_Arith('+', &a1, &k, &y) &&
		(!extra || Mode_Arith('+', &y, extra, &y2));
	if (ok) {
		Native_SetEmpty(out, NV_TUPLE);
		Native_Copy(Native_Append(out), &a0);
		Native_Copy(Native_Append(out), extra ? &y2 : &y);
		Native_SetInt(Native_Append(out), 0);
	}
	Native_Free(&ang);
	Native_Free(&a0);
	Native_Free(&a1);
	Native_Free(&y);
	Native_Free(&y2);
	return ok;
}

PyObject *Sn_Vars(int n, ...) {
	PyObject *d = PyDict_New();
	va_list ap;
	va_start(ap, n);
	for (int i = 0; i < n; i++) {
		const char *name = va_arg(ap, const char *);
		const nativeVal_t *v = va_arg(ap, const nativeVal_t *);
		PyObject *o = d ? Native_ToPy(v) : NULL;
		if (!o || PyDict_SetItemString(d, (char *)name, o) < 0) {
			Py_XDECREF(d);
			d = NULL;
		}
		Py_XDECREF(o);
	}
	va_end(ap);
	return d;
}

byte Sn_Def(int id, const char *section, PyObject *vars, const char *module) {
	const char *outer = se_useModule;
	se_useModule = module;
	byte ok = Script_RunFileTry(id, "menus/shipedit.def", section, shipedit_def, shipedit_steps, vars);
	se_useModule = outer;
	return ok;
}

/* ---- the platform ---- */

PyObject *Se_FrontAng(scriptFrame_t *f, PyObject *dummy, PyObject *extra) {
	PyObject *ang = Sf_Item(f, dummy, 2);
	PyObject *y = Sf_OpI(f, Sf_Item(f, ang, 1), '+', 180);
	if (extra) {
		y = Sf_Op(f, y, '+', extra);
	}
	return Sf_Build(f, "(OOi)", Sf_Item(f, ang, 0), y, 0);
}

/* IsEditingMultiplayer(): the editor of a network game (or of the ShipEdit level): Init.isShipEdit, else
 * Scrap.GetNetFlags() != (0, 0, 0) */
static PyObject *SE_IsEditingMultiplayer(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	PyObject *v = Sf_Mod(&f, "Init", "isShipEdit");
	if (Sf_True(&f, v) != 0) {
		return Sf_Return(&f, v);
	}
	int c = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetNetFlags", "()"), "!=", Sf_Build(&f, "(iii)", 0, 0, 0));
	return Sf_Return(&f, c >= 0 ? Sf_Int(&f, c) : NULL);
}

/* RustyOnPlatform(rusty): the scrap dealer stands on the platform (the comparison, a new kept value) */
static PyObject *Se_RustyOnPlatform_(scriptFrame_t *f, PyObject *rusty) {
	PyObject *pp = Sf_Build(f, "(OiO)", Sf_Item(f, Sf_Attr(f, GF("editPlatform"), "Pos"), 0), 0,
		Sf_Item(f, Sf_Attr(f, GF("editPlatform"), "Pos"), 2));
	PyObject *rp = Sf_Build(f, "(OiO)", Sf_Item(f, Sf_Attr(f, rusty, "Pos"), 0), 0, Sf_Item(f, Sf_Attr(f, rusty, "Pos"), 2));
	PyObject *d = Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", rp, pp));
	int c = Sf_Compare(f, d, "<=", Sf_Ask(f, "Scrap", "Get", "(s)", "PlatformRadiusSqr"));
	return c >= 0 ? Sf_Int(f, c) : NULL;
}

static PyObject *Se_RustyOnPlatform(scriptFrame_t *f, PyObject *rusty) {
	PyObject *r = NULL;
	if (!f->failed) {
		scriptFrame_t g = { 0 };
		r = Se_JoinValue(f, &g, Se_RustyOnPlatform_(&g, rusty));
	}
	return r;
}

/* rusty and RustyOnPlatform(rusty): 1, 0, -1 */
static int Se_RustyOn(scriptFrame_t *f, PyObject *rusty) {
	int t = Sf_True(f, rusty);
	return t > 0 ? Sf_True(f, Se_RustyOnPlatform(f, rusty)) : t;
}

static PyObject *SE_RustyOnPlatform(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *rusty;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &rusty)) {
		return NULL;
	}
	return Sf_Return(&f, Se_RustyOnPlatform(&f, rusty));
}

/* PointObjectTo(object, point): the object turns to the point (around the vertical axis) */
static void Se_PointTo_(scriptFrame_t *f, PyObject *obj, PyObject *point) {
	PyObject *start = Sf_Build(f, "(iii)", 0, 0, -1);
	PyObject *p = Sf_Build(f, "(OiO)", Sf_Item(f, point, 0), 0, Sf_Item(f, point, 2));
	PyObject *q = Sf_Build(f, "(OiO)", Sf_Item(f, Sf_Attr(f, obj, "Pos"), 0), 0, Sf_Item(f, Sf_Attr(f, obj, "Pos"), 2));
	PyObject *pq = Sf_Ask(f, "SVec", "Sub", "(OO)", p, q);
	PyObject *ang = Sf_Ask(f, "SVec", "GetAngle", "(OO)", start, pq);
	PyObject *d = Sf_Ask(f, "SVec", "CProd", "(OO)", start, pq);
	if (Sf_Compare(f, Sf_Item(f, d, 1), "<", Sf_Int(f, 0)) > 0) {
		ang = f->failed ? NULL : Sf_Keep(f, PyNumber_Negative(ang));
	}
	Sf_SetAttr(f, obj, "Ang", Sf_Build(f, "(iOi)", 0, ang, 0));
}

static void Se_PointTo(scriptFrame_t *f, PyObject *obj, PyObject *point) {
	SE_SUB(f, Se_PointTo_(&g, obj, point));
}

static PyObject *SE_PointObjectTo(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *obj, *point;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &obj, &point)) {
		return NULL;
	}
	Se_PointTo(&f, obj, point);
	return Sf_None(&f);
}

/* LaunchPlatformAnim(): the platform changes the ship, Rusty rides along when he stands on it */
static PyObject *SE_LaunchPlatformAnim(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *rusty = Sf_Entity(&f, Sf_Str(&f, "Rusty_0"));
	if (Se_RustyOn(&f, rusty) > 0) {
		PyObject *plt = Sf_Entity(&f, Sf_Str(&f, "DM_Door_EditShip_00"));
		PyObject *ypos = Sf_Op(&f, Sf_Op(&f, Sf_Attr(&f, rusty, "FloorDist"), '+', Sf_Item(&f, Sf_Attr(&f, plt,
			"NodePos_DC_Platform"), 1)), '-', Sf_Item(&f, Sf_Attr(&f, plt, "NodePos_DC_ShipLink"), 1));
		PyObject *off = Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, rusty, "Pos"), Sf_Attr(&f, plt, "Pos"));
		Sf_SetAttr(&f, rusty, "LinkModel", Sf_Cat(&f, "Oss", Sf_Attr(&f, G("editPlatform"), "Name"), ",", "DC_ShipLink"));
		Sf_SetAttr(&f, rusty, "OffModPos", Sf_Build(&f, "(OOO)", Sf_Item(&f, off, 0), ypos, Sf_Item(&f, off, 2)));
		Sf_SetAttr(&f, rusty, "DeltaOffMul", Sf_Float(&f, 0.0));
		Sf_SetAttr(&f, rusty, "OffMul", Sf_Float(&f, 1.0));
		Sf_SetAttrS(&f, rusty, "Action", "TalkListenNPC");
		Sf_SetAttrS(&f, rusty, "DefLoop", "TalkListenNPC");
	}
	Sf_SetAttrS(&f, G("editPlatform"), "Action", "Change");
	return Sf_None(&f);
}

/* ChangeShip(EntityName, EventName, Time): the event in the middle of the Change animation: the ship of the selected
 * hangar appears (unless it is destroyed), Rusty on the platform faces the camera */
static PyObject *SE_ChangeShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *entityName, *eventName, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &entityName, &eventName, &time)) {
		return NULL;
	}
	Sf_Import(&f, HT);
	if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
		Sf_SetAttrI(&f, HTA("prevShip"), "Active", 0);
	}
	if (Sf_Compare(&f, entityName, "!=", Py_None) > 0) {
		PyObject *rusty = Sf_Entity(&f, Sf_Str(&f, "Rusty_0"));
		if (Se_RustyOn(&f, rusty) > 0) {
			PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
			int t = Sf_True(&f, cam);
			if (t == 0) {
				Se_PointTo(&f, rusty, Sf_Attr(&f, Sf_Entity(&f, Sf_Str(&f, "EditCamera")), "Pos"));
			} else if (t > 0) {
				Se_PointTo(&f, rusty, Sf_Attr(&f, cam, "camPos"));
			}
		}
	}
	int c = Sf_Compare(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "!=", Py_None);
	if (c > 0) {
		c = Sf_EqI(&f, Sf_GetItem(&f, G("destroyedShips"), HTA("currentHangar")), 0);
		if (c == 0) {
			c = Se_Multi(&f, 1);
		}
	}
	if (c > 0) {
		Sf_SetAttrI(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "Active", 1);
		Sf_SetMod(&f, HT, "prevShip", Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")));
	}
	return Sf_None(&f);
}

/* VerifyChangeShip(EntityName, EventName, Time): the end of the Change animation: Rusty runs off the platform; an
 * empty or destroyed hangar shows the ship plans to buy (with "Rebuild" at the place of the destroyed one), else the
 * ship information; the buttons work again */
static PyObject *SE_VerifyChangeShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *entityName, *eventName, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &entityName, &eventName, &time)) {
		return NULL;
	}
	Sf_Import(&f, SE);
	Sf_Import(&f, HT);
	Sf_Import(&f, "string");
	PyObject *rusty = Sf_Entity(&f, Sf_Str(&f, "Rusty_0"));
	int c = Sf_Compare(&f, entityName, "!=", Py_None);
	if (c > 0) {
		c = Se_RustyOn(&f, rusty);
	}
	if (c > 0) {
		PyObject *plt = Sf_Entity(&f, Sf_Str(&f, "DM_Door_EditShip_00"));
		Sf_SetAttrS(&f, rusty, "LinkModel", ",");
		PyObject *y = Sf_Op(&f, Sf_Item(&f, Sf_Attr(&f, plt, "NodePos_DC_Platform"), 1), '+', Sf_Attr(&f, rusty, "FloorDist"));
		Sf_SetAttr(&f, rusty, "Pos", Sf_Build(&f, "(OOO)", Sf_Item(&f, Sf_Attr(&f, rusty, "OffModPos"), 0), y,
			Sf_Item(&f, Sf_Attr(&f, rusty, "OffModPos"), 2)));
		Sf_SetAttrI(&f, rusty, "LLogicState", 9);
		Sf_SetAttrS(&f, rusty, "LLogicTarget", "");
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	const int id = 0;
	PyObject *ship = Sf_GetItem(&f, Sf_Mod(&f, SE, "editedShips"), HTA("currentHangar"));
	int lost = Sf_EqI(&f, Sf_GetItem(&f, G("destroyedShips"), HTA("currentHangar")), 1);
	PyObject *isDestroyed = lost >= 0 ? Sf_Int(&f, lost) : NULL;
	c = Sf_Compare(&f, ship, "==", Py_None);
	if (c == 0 && lost > 0) {
		int mp = Se_Multi(&f, 1);
		c = mp < 0 ? -1 : !mp;
	}
	if (c > 0) {
		if (!f.failed) {
			Script_SetDefault(id, "ShipType");
		}
		int t = Sf_True(&f, ship);
		if (t > 0) {
			t = Sf_True(&f, isDestroyed);
		}
		if (t > 0) {
			int l = Sf_Len(&f, Sf_Mod(&f, SE, "shipsToEditList"));
			for (int i = 0; i < l && !f.failed; i++) {
				int mark = f.n;
				PyObject *pos = Sf_Ask(&f, "string", "find", "(OO)", Sf_Attr(&f, ship, "Name"),
					Sf_Item(&f, Sf_Mod(&f, SE, "shipsToEditList"), i));
				if (Sf_Compare(&f, pos, "!=", Sf_Int(&f, -1)) > 0) {
					Sf_SetMod(&f, HT, "currentShipToEdit", Sf_Int(&f, i));
					break;
				}
				Sf_Release(&f, mark);
			}
		}
		int bad = Sf_Compare(&f, HTA("currentShipToEdit"), "<", Sf_Int(&f, 0));
		if (bad == 0) {
			int n = Sf_Len(&f, G("shipsToEditList"));
			bad = n < 0 ? -1 : Sf_Compare(&f, HTA("currentShipToEdit"), ">", Sf_Int(&f, n - 1));
		}
		if (bad > 0) {
			Sf_SetMod(&f, HT, "currentShipToEdit", Sf_Int(&f, 0));
		}
		Sf_SetGlobal(&f, SE, "auxShipsToEditList", Sf_Keep(&f, PyList_New(0)));
		int l = Sf_Len(&f, G("shipsToEditList"));
		for (int i = 0; i < l && !f.failed; i++) {
			int mark = f.n;
			Sf_Method(&f, G("auxShipsToEditList"), "append", "(O)", Sf_Item(&f, G("shipsToEditList"), i));
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		int rebuild = Sf_True(&f, isDestroyed);
		if (rebuild > 0) {
			int mp = Se_Multi(&f, 1);
			rebuild = mp < 0 ? -1 : !mp;
		}
		if (rebuild > 0) {
			Sf_SetItem(&f, G("auxShipsToEditList"), HTA("currentShipToEdit"), Sf_Str(&f, "Rebuild"));
			Sf_Method(&f, G("auxShipsToEditList"), "append", "(O)", Sf_GetItem(&f, G("shipsToEditList"),
				HTA("currentShipToEdit")));
		}
		Sf_Ask(&f, HT, "SelectShipToEdit", "(i)", id);
		if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
			Sf_Ask(&f, HT, "ShowShipInfo", "(ii)", id, 0);
			Sf_Own(&f, SE, "StartRotateLoop", "()");
		}
	} else if (c == 0) {
		if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
			Sf_Ask(&f, HT, "ShowShipInfo", "(ii)", id, 1);
			Sf_Own(&f, SE, "StartRotateLoop", "()");
		}
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, HT, "EnableChangePlatformButtons", "(i)", id);
	return Sf_None(&f);
}

/* DoorInitializator(DoorClass): the door class of the platform: an elevator with the Change animation and its two
 * events */
static PyObject *SE_DoorInitializator(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *cls;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "DoorInitializator", "(O)", cls);
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "VRadius", 500);
	Sf_Ask(&f, "SAct", "CreateAction", "(s)", "Change");
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AnmFilePath", Sf_Cat(&f, "sOs", "Models/Doors/", cls, "/Anm/Change.M3D"));
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "InterpTime", 0);
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "isLoop", 0);
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AddEventAtTime", Sf_Op(&f, Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime"),
		'/', Sf_Float(&f, 2.0)));
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventName", "ChangeShip");
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventFunc", "ShipEdit.ChangeShip");
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AddEventAtTime", Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime"));
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventName", "EndChangeShip");
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventFunc", "ShipEdit.VerifyChangeShip");
	return Sf_None(&f);
}

/* ObjInitializator(elv, ExtraInfo): the platform entity: editPlatform, and editShipPosDummy (the place of the ship) */
static PyObject *SE_ObjInitializator(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *elv, *extra;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &elv, &extra)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "ObjInitializator", "(OO)", elv, extra);
	Sf_SetAttrI(&f, elv, "UseAngSel", 0);
	PyObject *ang = Sf_Build(&f, "(OO)", Sf_Item(&f, Sf_Attr(&f, elv, "Ang"), 0), Sf_Item(&f, Sf_Attr(&f, elv, "Ang"), 1));
	Sf_SetGlobal(&f, SE, "editShipPosDummy", Sf_Build(&f, "(OOOi)", Sf_Attr(&f, elv, "Name"),
		Sf_Attr(&f, elv, "NodePos_DC_ShipLink"), ang, 1));
	Sf_SetGlobal(&f, SE, "editPlatform", elv);
	return Sf_None(&f);
}

/* ---- the turning ship ---- */

/* EndMoveEngine(EntityName): the ship turned (RotateShip); on the Engines tab the engine groups of the model show */
static PyObject *SE_EndMoveEngine(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *entityName;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &entityName)) {
		return NULL;
	}
	Sf_Import(&f, "EnginesTab");
	if (Sf_EqI(&f, G("isRotating"), 0) > 0) {
		Sf_SetAttrI(&f, Sf_Entity(&f, entityName), "MoveTime", 0);
		if (Sf_EqS(&f, G("prevTab"), "ShipEditTabEngines") > 0) {
			const int id = 0;
			Sf_Import(&f, HT);
			PyObject *layout = Sf_GetItem(&f, G("shipsEnginesLayouts"), Sf_Attr(&f, HTA("prevShip"), "Model"));
			int n = Sf_Len(&f, layout);
			for (int i = 0; i < n && !f.failed; i++) {
				int mark = f.n;
				PyObject *name = Sf_Cat(&f, "OsO", Sf_Attr(&f, HTA("prevShip"), "Model"), "EngineGroup", Sf_ReprI(&f, i + 1));
				if (i == 0) {
					Sf_Ask(&f, "EnginesTab", "SelectEngineGroup", "(iOi)", id, name, 0);
					char var[0x10];
					Str_Printf(var, sizeof(var), "nEng%d", i);
					PyObject *slots = Sf_Attr(&f, HTA("prevShip"), var);
					int none = Sf_EqI(&f, slots, 0);
					if (none > 0) {
						Se_SetUse(&f, id, name, "Hint", "HINT_Engines6", SE);
					} else if (none == 0) {
						PyObject *key = Sf_Cat(&f, "sO", "HINT_Engines", Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 6), '+', slots)));
						Se_SetOO(&f, id, name, "Hint", Sf_Lang(&f, key));
					}
				} else {
					Se_SetUse(&f, id, name, "Hint", "HINT_Engines6", SE);
				}
				Se_SetOO(&f, id, name, "Visible", Sf_Int(&f, 1));
				Sf_Release(&f, f.failed ? f.n : mark);
			}
			Sf_Later(&f, Sf_Float(&f, 0.15), Sf_Mod(&f, "EnginesTab", "UpdateLayouts"), Sf_Build(&f, "(i)", id), NULL);
		}
	}
	return Sf_None(&f);
}

/* StopRotateLoop(): the ship stops turning */
static void Se_StopRotate_(scriptFrame_t *f) {
	Sf_SetGlobal(f, SE, "ctnRotating", Sf_OpI(f, GF("ctnRotating"), '+', 1));
	Sf_Import(f, HT);
	Sf_SetAttrI(f, Se_Mod(f, HT, "prevShip"), "MoveTime", 0);
	Sf_SetGlobal(f, SE, "isRotating", Sf_Int(f, 0));
}

static void Se_StopRotate(scriptFrame_t *f) {
	SE_SUB(f, Se_StopRotate_(&g));
}

/* RotateShip(Angle): the ship turns to Angle in half a second (EndMoveEngine at the end) */
static void Se_RotateShip_(scriptFrame_t *f, PyObject *angle) {
	Sf_Import(f, HT);
	Se_StopRotate(f);
	if (Sf_Compare(f, Se_Mod(f, HT, "prevShip"), "!=", Py_None) > 0) {
		PyObject *ship = Se_Mod(f, HT, "prevShip");
		Sf_SetAttr(f, ship, "MovePos", Sf_Attr(f, Se_Mod(f, HT, "prevShip"), "Pos"));
		Sf_SetAttr(f, Se_Mod(f, HT, "prevShip"), "MoveAng", Se_FrontAng(f, GF("editShipPosDummy"), angle));
		Sf_SetAttr(f, Se_Mod(f, HT, "prevShip"), "MoveTime", Sf_Float(f, 0.5));
		Sf_SetAttrI(f, Se_Mod(f, HT, "prevShip"), "MoveFade", 3);
		Sf_SetAttrS(f, Se_Mod(f, HT, "prevShip"), "OnEndMove", "ShipEdit.EndMoveEngine");
	}
}

static void Se_RotateShip(scriptFrame_t *f, PyObject *angle) {
	SE_SUB(f, Se_RotateShip_(&g, angle));
}

static PyObject *SE_RotateShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *angle;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &angle)) {
		return NULL;
	}
	Se_RotateShip(&f, angle);
	return Sf_None(&f);
}

/* LoopRotateShip(Rot): one turn of 170 degrees in 10 s, again 9 s later while the loop Rot runs */
static void Se_LoopRotate_(scriptFrame_t *f, PyObject *rot) {
	Sf_Import(f, HT);
	int c = Sf_Compare(f, GF("ctnRotating"), "==", rot);
	if (c > 0) {
		c = Sf_Compare(f, Se_Mod(f, HT, "prevShip"), "!=", Py_None);
	}
	if (c > 0) {
		Sf_SetAttr(f, Se_Mod(f, HT, "prevShip"), "MovePos", Sf_Attr(f, Se_Mod(f, HT, "prevShip"), "Pos"));
		PyObject *y = Sf_OpI(f, Sf_Item(f, Sf_Attr(f, Se_Mod(f, HT, "prevShip"), "Ang"), 1), '+', 170);
		Sf_SetAttr(f, Se_Mod(f, HT, "prevShip"), "MoveAng", Sf_Build(f, "(OOi)", Sf_Item(f, Sf_Item(f,
			GF("editShipPosDummy"), 2), 0), y, 0));
		Sf_SetAttr(f, Se_Mod(f, HT, "prevShip"), "MoveTime", Sf_Float(f, 10.0));
		Sf_SetAttrI(f, Se_Mod(f, HT, "prevShip"), "MoveFade", 0);
		Sf_SetAttrS(f, Se_Mod(f, HT, "prevShip"), "OnEndMove", "");
		Sf_Later(f, Sf_Float(f, 9.0), GF("LoopRotateShip"), Sf_Build(f, "(O)", GF("ctnRotating")), NULL);
	}
}

static void Se_LoopRotate(scriptFrame_t *f, PyObject *rot) {
	SE_SUB(f, Se_LoopRotate_(&g, rot));
}

static PyObject *SE_LoopRotateShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *rot;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &rot)) {
		return NULL;
	}
	Se_LoopRotate(&f, rot);
	return Sf_None(&f);
}

/* StartRotateLoop(): the ship starts turning (a new loop, the old one ends) */
static void Se_StartRotate_(scriptFrame_t *f) {
	Sf_SetGlobal(f, SE, "isRotating", Sf_Int(f, 1));
	Sf_SetGlobal(f, SE, "ctnRotating", Sf_OpI(f, GF("ctnRotating"), '+', 1));
	Se_LoopRotate(f, GF("ctnRotating"));
}

static void Se_StartRotate(scriptFrame_t *f) {
	SE_SUB(f, Se_StartRotate_(&g));
}

static PyObject *SE_StartRotateLoop(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Se_StartRotate(&f);
	return Sf_None(&f);
}

static PyObject *SE_StopRotateLoop(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Se_StopRotate(&f);
	return Sf_None(&f);
}

/* ---- the panels ---- */

/* the sprite name with the attributes of DrawBackground (posX, posY, sizeX, sizeY, alpha, red, green, blue; NULL
 * takes the default) */
static void Se_Background(scriptFrame_t *f, PyObject *id, PyObject *name, PyObject *const *v) {
	static const char *const vars[] = { "X", "Y", "ScaleX", "ScaleY", "Unit", "Alpha", "Red", "Green", "Blue" };
	static const long defs[] = { 64, 64, 512, 352, 1, 92, 0, 0, 32 };
	Sf_Ask(f, "SScorer", "Add", "(OOs)", id, name, "Sprite");
	for (int i = 0; i < 9 && !f->failed; i++) {
		PyObject *val = i == 4 || !v[i < 4 ? i : i - 1] ? Sf_Int(f, defs[i]) : v[i < 4 ? i : i - 1];
		Sf_Ask(f, "SScorer", "Set", "(OOsO)", id, name, vars[i], val);
	}
}

/* DrawBackground(id, elem, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32) of ShipEdit
 * (the sprite BackgroundElement<elem>) and of HangarTab (the sprite Background, no elem) */
static PyObject *Se_DrawBackground(PyObject *args, PyObject *kw, byte elem) {
	static char *names[] = { "id", "elem", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static char *names2[] = { "id", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	scriptFrame_t f = { 0 };
	PyObject *id, *e = NULL, *v[8] = { NULL };
	byte ok = elem ? PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", names, &id, &e, &v[0], &v[1], &v[2], &v[3],
		&v[4], &v[5], &v[6], &v[7]) : PyArg_ParseTupleAndKeywords(args, kw, "O|OOOOOOOO", names2, &id, &v[0], &v[1], &v[2],
		&v[3], &v[4], &v[5], &v[6], &v[7]);
	if (!ok) {
		return NULL;
	}
	PyObject *name = elem ? Sf_Cat(&f, "sO", "BackgroundElement", e) : Sf_Str(&f, "Background");
	Se_Background(&f, id, name, v);
	return Sf_None(&f);
}

PyObject *ShipEdit_DrawBackground(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Se_DrawBackground(args, kw, 1);
}

PyObject *HangarTab_DrawBackground(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Se_DrawBackground(args, kw, 0);
}

/* AddShipEditTabs(id, tabs): the tabs of the editor (the Hangar tab alone on Sputnik's screen); the first one is
 * prevTab when none was */
static PyObject *SE_AddShipEditTabs(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *tabs;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &tabs)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	long counter = 1;
	PyObject *startX = G("offsetX");
	PyObject *tabSize = Sf_OpI(&f, G("TabQuadSize"), '*', 18);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *t = Sf_Next(&f, tabs, i);
		if (!t) {
			break;
		}
		PyObject *tabName = Sf_Cat(&f, "sO", "ShipEditTab", t);
		counter++;
		PyObject *text = Sf_Lang(&f, Sf_Cat(&f, "sOs", "ShipEdit_", t, "TabName"));
		PyObject *init = Sf_Op(&f, Sf_Op(&f, Sf_Int(&f, counter - 2), '*', tabSize), '/', G("TabQuadSize"));
		PyObject *end = Sf_Op(&f, Sf_Op(&f, Sf_Int(&f, counter - 1), '*', tabSize), '/', G("TabQuadSize"));
		int n = Sf_Len(&f, tabs);
		PyObject *vars = Sf_Build(&f, "{s:O,s:O,s:O,s:O,s:O,s:O,s:i,s:i}", "name", tabName, "tab", t, "text", text, "x",
			startX, "init", init, "end", end, "first", counter == 2, "only", n == 1);
		Se_Def(&f, id, "tab", vars, SE);
		PyObject *width = Sf_OpI(&f, tabSize, '/', 2);
		Sf_SetItem(&f, G("tabPixelX"), tabName, Sf_Build(&f, "(OO)", startX, Sf_Op(&f, startX, '+', width)));
		startX = Sf_OpI(&f, Sf_Op(&f, startX, '+', width), '+', 1);
		Sf_ReleaseKeep(&f, mark, &startX, f.failed ? 0 : 1);
	}
	if (Sf_Compare(&f, G("prevTab"), "==", Py_None) > 0) {
		Sf_SetGlobal(&f, SE, "prevTab", Sf_Cat(&f, "sO", "ShipEditTab", Sf_Item(&f, tabs, 0)));
		Se_SetOO(&f, id, G("prevTab"), "TextColorBlue", Sf_Int(&f, 0));
	}
	return Sf_None(&f);
}

/* AddDefaultPanelContent(id, ntabs): the dark frame around the panels, open where the ntabs tabs are */
static PyObject *SE_AddDefaultPanelContent(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *ntabs;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &ntabs)) {
		return NULL;
	}
	/* offsetX + (TabQuadSize*18*ntabs/2) and (TabQuadSize*18*(4-ntabs)/2 + TabQuadSize*6/2) - MarginSize */
	PyObject *x2 = Sf_Op(&f, Sf_Int(&f, 47), '+', Sf_OpI(&f, Sf_Op(&f, Sf_Int(&f, 252), '*', ntabs), '/', 2));
	PyObject *w2 = Sf_OpI(&f, Sf_OpI(&f, Sf_OpI(&f, Sf_Op(&f, Sf_Int(&f, 252), '*', Sf_Op(&f, Sf_Int(&f, 4), '-', ntabs)),
		'/', 2), '+', 42), '-', 1);
	Se_Def(&f, id, "background", Sf_Build(&f, "{s:O,s:O}", "x2", x2, "w2", w2), SE);
	return Sf_None(&f);
}

/* ChangeTabLeft(id, control), ChangeTabRight(id, control): the tab beside the current one (not while the platform
 * changes the ship) */
static PyObject *Se_ChangeTabSide(PyObject *args, const char *side) {
	scriptFrame_t f = { 0 };
	PyObject *id, *control;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	int c = Sf_Compare(&f, G("editPlatform"), "!=", Py_None);
	if (c > 0) {
		c = Sf_EqS(&f, Sf_Attr(&f, G("editPlatform"), "Action"), "Change");
	}
	if (c == 0) {
		Sf_Own(&f, SE, "ChangeTab", "(OOs)", id, control, side);
	}
	return Sf_None(&f);
}

static PyObject *SE_ChangeTabLeft(PyObject *self, PyObject *args) {
	(void)self;
	return Se_ChangeTabSide(args, "Left");
}

static PyObject *SE_ChangeTabRight(PyObject *self, PyObject *args) {
	(void)self;
	return Se_ChangeTabSide(args, "Right");
}

/* the tab control in front (yellow title, the tab drawn as the panel) or behind */
static void Se_TabFront_(scriptFrame_t *f, int id, PyObject *tab, byte front) {
	Se_SetOO(f, id, tab, "Red", front ? Sf_Int(f, 255) : Sf_Float(f, 255 / 1.5));
	Se_SetOO(f, id, tab, "Green", front ? Sf_Int(f, 234) : Sf_Float(f, 234 / 1.5));
	Se_SetOO(f, id, tab, "Blue", front ? Sf_Int(f, 177) : Sf_Float(f, 177 / 1.5));
	PyObject *name = Sf_Cat(f, "sO", "Tab_", tab);
	static const char *const vars[] = { "Type", "Alpha", "Red", "Green", "Blue" };
	static const long on[] = { 0, 1, 57, 139, 202 }, off[] = { 1, 192, 20, 60, 160 };
	for (int i = 0; i < 5; i++) {
		Se_SetOO(f, id, name, vars[i], Sf_Int(f, front ? on[i] : off[i]));
	}
}

static void Se_TabFront(scriptFrame_t *f, int id, PyObject *tab, byte front) {
	SE_SUB(f, Se_TabFront_(&g, id, tab, front));
}

/* math.fabs(a - b) of the angles */
static PyObject *Se_Swept(scriptFrame_t *f, double a, PyObject *shipAng) {
	if (!f->failed && !shipAng) {
		Sf_Raise(f, PyExc_NameError, "shipAng");
		return NULL;
	}
	return Sf_Ask(f, "math", "fabs", "(O)", Sf_Op(f, Sf_Float(f, a), '-', shipAng));
}

/* ChangeTab(id, control, direction=None): the tab control (or the one at direction of the current tab) comes to the
 * front: the panel of the old tab hides, the new one shows, the ship turns to the side the panel edits */
static PyObject *SE_ChangeTab(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "control", "direction", NULL };
	scriptFrame_t f = { 0 };
	PyObject *control, *direction = Py_None;
	int id;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iO|O", names, &id, &control, &direction)) {
		return NULL;
	}
	Sf_From(&f, "Menu", "LinkUD");
	Sf_From(&f, "Menu", "UnLinkUD");
	static const char *const mods[] = { HT, "EnginesTab", "WeaponsTab", "HullTab", "Sound", "math" };
	for (int i = 0; i < 6; i++) {
		Sf_Import(&f, mods[i]);
	}
	if (Sf_True(&f, direction) > 0) {
		control = Sf_Ask(&f, "SScorer", "Get", "(iOO)", id, G("prevTab"), direction);
	}
	if (Sf_Compare(&f, control, "==", G("prevTab")) != 0) {
		return Sf_None(&f);
	}
	PyObject *prev = G("prevTab");
	Se_TabFront(&f, id, prev, 0);
	Se_TabFront(&f, id, control, 1);
	int mark = f.n;
	if (Sf_EqS(&f, prev, "ShipEditTabHangar") > 0) {
		Se_SetUse(&f, id, prev, "Hint", "HINT_Tabs5", SE);
		Sf_Ask(&f, HT, "HideHangarPanel", "(i)", id);
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "");
	} else if (Sf_EqS(&f, prev, "ShipEditTabEngines") > 0) {
		Se_SetUse(&f, id, prev, "Hint", "HINT_Tabs6", SE);
		if (Sf_Compare(&f, control, "!=", prev) > 0) {
			Sf_Ask(&f, "EnginesTab", "HideEnginesPanel", "(i)", id);
			Se_RotateShip(&f, Sf_Int(&f, 0));
		}
		Se_Link(&f, "LinkUD", id, "ShipEditTabEngines", "");
	} else if (Sf_EqS(&f, prev, "ShipEditTabWeapons") > 0) {
		Se_SetUse(&f, id, prev, "Hint", "HINT_Tabs7", SE);
		if (Sf_Compare(&f, control, "!=", prev) > 0) {
			Sf_Ask(&f, "WeaponsTab", "HideWeaponsPanel", "(i)", id);
		}
		Se_Link(&f, "LinkUD", id, "ShipEditTabWeapons", "");
	} else if (Sf_EqS(&f, prev, "ShipEditTabHull") > 0) {
		Se_SetUse(&f, id, prev, "Hint", "HINT_Tabs8", SE);
		Sf_Ask(&f, "HullTab", "HideHullPanel", "(i)", id);
		Se_Link(&f, "LinkUD", id, "ShipEditTabHull", "");
	}
	Sf_Release(&f, f.failed ? f.n : mark);
	Se_SetOO(&f, id, G("prevTab"), "TextColorBlue", Sf_Int(&f, 255));
	Se_SetOO(&f, id, G("prevTab"), "SpriteIndex", Sf_Int(&f, 18));
	Se_SetOO(&f, id, control, "TextColorBlue", Sf_Int(&f, 0));
	Se_SetOO(&f, id, control, "SpriteIndex", Sf_Int(&f, 41));
	PyObject *swept = Sf_Int(&f, -1), *shipAng = NULL;
	if (Sf_True(&f, HTA("prevShip")) > 0) {
		PyObject *a = Sf_Item(&f, Sf_Attr(&f, HTA("prevShip"), "Ang"), 1);
		shipAng = Sf_Ask(&f, "math", "fabs", "(O)", Sf_Ask(&f, "math", "fmod", "(Oi)", a, 360));
	}
	mark = f.n;
	if (Sf_EqS(&f, control, "ShipEditTabHangar") > 0) {
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "Sell");
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "Exit");
		Sf_Ask(&f, HT, "ShowHangarPanel", "(i)", id);
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "Hangar3");
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "Hangar2");
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", "Hangar1");
		Se_SetOO(&f, id, control, "Hint", Sf_Str(&f, Script_Lang("HINT_Tabs1")));
		Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, control);
		Se_StartRotate(&f);
	} else if (Sf_EqS(&f, control, "ShipEditTabEngines") > 0) {
		if (Sf_EqS(&f, G("prevTab"), "ShipEditTabHull") > 0) {
			swept = Sf_Float(&f, 90.0);
		} else {
			swept = Se_Swept(&f, 0.0, shipAng);
		}
		if (Sf_Compare(&f, G("prevTab"), "!=", Sf_Str(&f, "ShipEditTabEngines")) > 0) {
			Sf_Ask(&f, "EnginesTab", "ShowEnginesPanel", "(i)", id);
			Se_RotateShip(&f, Sf_Int(&f, 180));
		}
		Se_SetOO(&f, id, control, "Hint", Sf_Str(&f, Script_Lang("HINT_Tabs2")));
		Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, control);
		Se_Link(&f, "LinkUD", id, "ShipEditTabEngines", "Recycle");
		Se_Link(&f, "LinkUD", id, "ShipEditTabEngines", "Exit");
		Se_Link(&f, "LinkUD", id, "ShipEditTabEngines", "PrevEngine");
		Se_LinkO(&f, "LinkUD", id, Sf_Str(&f, "ShipEditTabEngines"), Sf_Cat(&f, "Os", Sf_Attr(&f, HTA("prevShip"), "Model"),
			"EngineGroup1"));
		Se_Link(&f, "LinkUD", id, "ShipEditTabEngines", "Engine1");
	} else if (Sf_EqS(&f, control, "ShipEditTabWeapons") > 0) {
		swept = Se_Swept(&f, 180.0, shipAng);
		Sf_Ask(&f, "WeaponsTab", "ShowWeaponsPanel", "(i)", id);
		Se_SetOO(&f, id, control, "Hint", Sf_Str(&f, Script_Lang("HINT_Tabs3")));
		Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, control);
		static const char *const weapons[] = { "Devastator", "Tesla", "ATPC", "Inferno", "Swarm", "Vulcan" };
		for (int i = 0; i < 6; i++) {
			Se_Link(&f, "LinkUD", id, "ShipEditTabWeapons", weapons[i]);
		}
		Se_RotateShip(&f, Sf_Int(&f, 0));
	} else if (Sf_EqS(&f, control, "ShipEditTabHull") > 0) {
		swept = Se_Swept(&f, 90.0, shipAng);
		Sf_Ask(&f, "HullTab", "ShowHullPanel", "(i)", id);
		Se_SetOO(&f, id, control, "Hint", Sf_Str(&f, Script_Lang("HINT_Tabs4")));
		Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, control);
		Se_RotateShip(&f, Sf_Int(&f, 270));
	}
	PyObject *keep[1] = { swept };
	Sf_ReleaseKeep(&f, mark, keep, f.failed ? 0 : 1);
	int c = Sf_Compare(&f, swept, "!=", Sf_Int(&f, -1));
	if (c > 0) {
		c = Sf_Compare(&f, G("prevTab"), "!=", control);
	}
	if (c > 0) {
		const char *snd = Sf_Compare(&f, swept, "<=", Sf_Int(&f, 91)) > 0 ? "RotateShipFast" : "RotateShip";
		char s[0x20], v[0x20];
		Str_Printf(s, sizeof(s), "%sSound", snd);
		Str_Printf(v, sizeof(v), "%sVolume", snd);
		Sf_Ask(&f, "SSound", "Play", "(OO)", Sf_Mod(&f, "Sound", s), Sf_Mod(&f, "Sound", v));
	}
	Sf_SetGlobal(&f, SE, "prevTab", control);
	return Sf_None(&f);
}

/* ---- the ships of the hangars ---- */

/* CleanEditedShips(): the ships of the hangars are deleted (all but the one on the platform), editedShips is ten
 * Nones */
static void Se_Clean_(scriptFrame_t *f) {
	Sf_Import(f, HT);
	int n = Sf_Len(f, GF("editedShips"));
	for (int i = 0; i < n && !f->failed; i++) {
		int mark = f->n;
		PyObject *e = Sf_Item(f, GF("editedShips"), i);
		int c = Sf_True(f, e);
		if (c > 0) {
			c = Sf_Compare(f, e, "!=", Se_Mod(f, HT, "prevShip"));
		}
		if (c > 0) {
			Sf_Method(f, Sf_Item(f, GF("editedShips"), i), "Del", "()");
			if (Sf_Except(f)) {
				Sf_Print(f, "s", "Ya borrada");
			}
		}
		Sf_Release(f, f->failed ? f->n : mark);
	}
	Sf_SetGlobal(f, SE, "editedShips", Sf_Keep(f, PyList_New(0)));
	for (int i = 0; i < 10; i++) {
		Sf_Method(f, GF("editedShips"), "append", "(O)", Py_None);
	}
}

static void Se_Clean(scriptFrame_t *f) {
	SE_SUB(f, Se_Clean_(&g));
}

static PyObject *SE_CleanEditedShips(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Se_Clean(&f);
	return Sf_None(&f);
}

/* CreatePlayerShip(): the ship the player comes to the junkyard with (the hangar of Hangar.iHangarShip) */
static PyObject *SE_CreatePlayerShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Se_Clean(&f);
	Sf_Import(&f, HT);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "string");
	Sf_Item(&f, G("editShipPosDummy"), 1);
	Sf_SetMod(&f, HT, "currentHangar", Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.iHangarShip")));
	if (Sf_Compare(&f, HTA("currentHangar"), ">=", Sf_Int(&f, 0)) > 0) {
		PyObject *cad;
		int aux = Sf_EqI(&f, HTA("currentHangar"), 9);
		if (aux > 0) {
			cad = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.HangarShipAux");
		} else {
			cad = Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", Sf_Cat(&f, "sO", "Hangar.HangarShip",
				Sf_Repr(&f, Sf_OpI(&f, HTA("currentHangar"), '+', 1))));
		}
		if (Sf_Compare(&f, cad, "!=", Sf_Str(&f, "")) > 0) {
			Sf_Own(&f, SE, "AssignShip", "(OO)", cad, HTA("currentHangar"));
			PyObject *ang = Se_FrontAng(&f, G("editShipPosDummy"), NULL);
			Sf_SetAttr(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "Ang", ang);
			Sf_SetAttrI(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "Active", 1);
			Sf_SetMod(&f, HT, "prevShip", Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")));
		}
	}
	return Sf_None(&f);
}

/* SetupForEdit(edShps): the ships are linked to the platform */
static PyObject *SE_SetupForEdit(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *ships;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ships)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *o = Sf_Next(&f, ships, i);
		if (!o) {
			break;
		}
		if (Sf_True(&f, o) > 0) {
			Sf_SetAttrI(&f, o, "MoveTime", 0);
			Sf_SetAttrS(&f, o, "LinkModel", ",");
			Sf_SetAttr(&f, o, "LinkModel", Sf_Cat(&f, "Oss", Sf_Attr(&f, G("editPlatform"), "Name"), ",", "DC_ShipLink"));
			Sf_SetAttr(&f, o, "OffModPos", Sf_Build(&f, "(iii)", 0, 0, 0));
			Sf_SetAttr(&f, o, "DeltaOffMul", Sf_Float(&f, 0.0));
			Sf_SetAttr(&f, o, "OffMul", Sf_Float(&f, 1.0));
			Sf_SetAttrI(&f, o, "ArrowCanUse", 0);
			Sf_Ask(&f, "Parking", "SetShipRadius", "(Oi)", o, 1);
		}
		Sf_Release(&f, f.failed ? f.n : mark);
	}
	return Sf_None(&f);
}

/* the text of a ship in a save variable: model<-engine0<-...<-engine3<-weapons<-hull<-upgrades ("" for None) */
static PyObject *Se_ShipText_(scriptFrame_t *f, PyObject *ship) {
	if (Sf_Compare(f, ship, "!=", Py_None) <= 0) {
		return f->failed ? NULL : Sf_Str(f, "");
	}
	static const char *const up[] = { "Upgrd00", "Upgrd03", "Upgrd02", "Upgrd04", "Upgrd05", "Upgrd01" };
	PyObject *upgrades = NULL;
	for (int i = 0; i < 6; i++) {
		PyObject *r = Sf_Repr(f, Sf_Attr(f, ship, up[i]));
		upgrades = upgrades ? Sf_Cat(f, "OsO", upgrades, ",", r) : r;
	}
	static const char *const parts[] = { "Model", "MEng0", "MEng1", "MEng2", "MEng3", "WeapList" };
	PyObject *cad = NULL;
	for (int i = 0; i < 6; i++) {
		PyObject *v = Sf_Attr(f, ship, parts[i]);
		cad = cad ? Sf_Cat(f, "OsO", cad, "<-", v) : v;
	}
	return Sf_Cat(f, "OsOsO", cad, "<-", Sf_Repr(f, Sf_Attr(f, ship, "MaxLife")), "<-", upgrades);
}

static PyObject *Se_ShipText(scriptFrame_t *f, PyObject *ship) {
	PyObject *r = NULL;
	if (!f->failed) {
		scriptFrame_t g = { 0 };
		r = Se_JoinValue(f, &g, Se_ShipText_(&g, ship));
	}
	return r;
}

/* SaveShipInfoInSaveVar(ship, hangar), SaveShipInfoInGVar(ship, hangar): the ship into Hangar.HangarShip<hangar+1>
 * (a save variable) or the engine variable HangarShip<hangar+1> */
static void Se_SaveShip_(scriptFrame_t *f, PyObject *ship, PyObject *hangar, byte save) {
	PyObject *cad = Se_ShipText(f, ship);
	PyObject *n = Sf_Repr(f, Sf_OpI(f, hangar, '+', 1));
	if (save) {
		Sf_Ask(f, "Scrap", "SetSaveVar", "(OO)", Sf_Cat(f, "sO", "Hangar.HangarShip", n), cad);
	} else {
		Sf_Ask(f, "Scrap", "Set", "(OO)", Sf_Cat(f, "sO", "HangarShip", n), cad);
	}
}

static void Se_SaveShip(scriptFrame_t *f, PyObject *ship, PyObject *hangar, byte save) {
	SE_SUB(f, Se_SaveShip_(&g, ship, hangar, save));
}

static PyObject *Se_SaveShipArgs(PyObject *args, byte save) {
	scriptFrame_t f = { 0 };
	PyObject *ship, *hangar;
	if (!PyArg_ParseTuple(args, "OO", &ship, &hangar)) {
		return NULL;
	}
	Se_SaveShip(&f, ship, hangar, save);
	return Sf_None(&f);
}

static PyObject *SE_SaveShipInfoInSaveVar(PyObject *self, PyObject *args) {
	(void)self;
	return Se_SaveShipArgs(args, 1);
}

static PyObject *SE_SaveShipInfoInGVar(PyObject *self, PyObject *args) {
	(void)self;
	return Se_SaveShipArgs(args, 0);
}

/* CreateShipFromSaveVar(cad, pos, name=""): the ship of a save variable text at pos (None for an empty model), with
 * its weapon upgrades in single player */
static PyObject *SE_CreateShipFromSaveVar(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "cad", "pos", "name", NULL };
	scriptFrame_t f = { 0 };
	PyObject *cad, *pos, *name = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &cad, &pos, &name)) {
		return NULL;
	}
	name = name ? Sf_Ref(&f, name) : Sf_Str(&f, "");
	Sf_Import(&f, "string");
	Sf_Import(&f, "Vehicles");
	PyObject *a = Sf_Ask(&f, "string", "split", "(Os)", cad, "<-");
	if (Sf_Compare(&f, Sf_Item(&f, a, 0), "!=", Sf_Str(&f, "")) <= 0) {
		return f.failed ? Sf_Done(&f, NULL) : Sf_None(&f);
	}
	if (Sf_EqS(&f, name, "") > 0) {
		name = Sf_Item(&f, a, 0);
	}
	PyObject *ship = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "Car");
	Sf_SetAttrI(&f, ship, "ActiveCMask", 0);
	PyObject *v[9];
	v[0] = Sf_Attr(&f, ship, "Name");
	v[1] = Sf_Item(&f, a, 0);
	v[2] = Sf_ToInt(&f, Sf_Item(&f, a, 6));
	for (int i = 1; i <= 5; i++) {
		v[i + 2] = Sf_Item(&f, a, i);
	}
	v[8] = Sf_Lang(&f, Sf_Cat(&f, "sO", "SHIP_", Sf_Ask(&f, "string", "upper", "(O)", Sf_Item(&f, a, 0))));
	Sf_Ask(&f, "Vehicles", "SetProperties", "(OOOsOOOOOOi)", v[0], v[1], v[2], "P_None", v[3], v[4], v[5], v[6], v[7], v[8], 1);
	Sf_SetAttrI(&f, ship, "Active", 0);
	int mp = Se_Multi(&f, 1);
	if (mp == 0) {
		PyObject *up = Sf_Ask(&f, "string", "split", "(Os)", Sf_Item(&f, a, 7), ",");
		static const char *const slot[] = { "00", "03", "02", "04", "05", "01" };
		for (int i = 0; i < 6 && !f.failed; i++) {
			PyObject *j = Sf_ToInt(&f, Sf_Item(&f, up, i));
			if (Sf_Compare(&f, j, "!=", Sf_Int(&f, 0)) > 0) {
				char var[0x10];
				Str_Printf(var, sizeof(var), "UAmmo%s", slot[i]);
				Sf_SetAttrI(&f, ship, var, 200);
				Str_Printf(var, sizeof(var), "Upgrd%s", slot[i]);
				Sf_SetAttr(&f, ship, var, j);
			}
		}
	}
	return Sf_Return(&f, ship);
}

/* AssignShip(cad, i): the ship of hangar i from its text, facing the camera, hidden */
static PyObject *SE_AssignShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *cad, *i;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &cad, &i)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, HT);
	Sf_Import(&f, "string");
	Sf_SetItem(&f, G("editedShips"), i, Sf_Own(&f, SE, "CreateShipFromSaveVar", "(OO)", cad, Sf_Item(&f,
		G("editShipPosDummy"), 1)));
	Sf_SetAttr(&f, Sf_GetItem(&f, G("editedShips"), i), "Ang", Se_FrontAng(&f, G("editShipPosDummy"), NULL));
	Sf_SetAttrI(&f, Sf_GetItem(&f, G("editedShips"), i), "Active", 0);
	return Sf_None(&f);
}

/* Scrap.Set(var, Scrap.Def(var)) for the player's ship variables */
static void Se_DefaultShip_(scriptFrame_t *f, byte name) {
	static const char *const vars[] = { "PlayerName", "PilotModel", "PlayerMaxLife", "PlayerModel", "Motor0Model",
		"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", NULL };
	for (const char *const *v = vars + (name ? 0 : 2); *v && !f->failed; v++) {
		Sf_Ask(f, "Scrap", "Set", "(sO)", *v, Sf_Ask(f, "Scrap", "Def", "(s)", *v));
	}
}

static void Se_DefaultShip(scriptFrame_t *f, byte name) {
	SE_SUB(f, Se_DefaultShip_(&g, name));
}

/* a default ship (the Def values of the engine) in hangar p, facing the camera, hidden; pilot the pilot model of
 * SetProperties */
static void Se_MakeDefault_(scriptFrame_t *f, PyObject *p, PyObject *pilot) {
	PyObject *pos = Sf_Item(f, GF("editShipPosDummy"), 1);
	PyObject *model = Sf_Ask(f, "Scrap", "Def", "(s)", "PlayerModel");
	Sf_SetItem(f, GF("editedShips"), p, Sf_Ask(f, "Scrap", "CreateEntity", "(OOOOs)", model, Sf_Item(f, pos, 0),
		Sf_Item(f, pos, 1), Sf_Item(f, pos, 2), "Car"));
	Sf_SetAttrI(f, Sf_GetItem(f, GF("editedShips"), p), "ActiveCMask", 0);
	PyObject *ename = Sf_Attr(f, Sf_GetItem(f, GF("editedShips"), p), "Name");
	static const char *const vars[] = { "PlayerModel", "PlayerMaxLife", NULL, "Motor0Model", "Motor1Model", "Motor2Model",
		"Motor3Model", "WeaponBayList" };
	PyObject *v[8];
	for (int i = 0; i < 8; i++) {
		v[i] = vars[i] ? Sf_Ask(f, "Scrap", "Def", "(s)", vars[i]) : pilot ? pilot : Sf_Ask(f, "Scrap", "Def", "(s)", "PilotModel");
	}
	Sf_Ask(f, "Vehicles", "SetProperties", "(OOOOOOOOOsi)", ename, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], "", 1);
	Sf_SetAttr(f, Sf_GetItem(f, GF("editedShips"), p), "Ang", Se_FrontAng(f, GF("editShipPosDummy"), NULL));
	Sf_SetAttrI(f, Sf_GetItem(f, GF("editedShips"), p), "Active", 0);
}

static void Se_MakeDefault(scriptFrame_t *f, PyObject *p, PyObject *pilot) {
	SE_SUB(f, Se_MakeDefault_(&g, p, pilot));
}

/* SetupEdit(id): the camera flies to the platform; the ships of the hangars are made (from the engine variables in a
 * network game, with a default ship when there is none, else from the save variables) and linked to the platform */
static PyObject *SE_SetupEdit(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	static const char *const mods[] = { "string", "Vehicles", "ShipMaker", HT };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "VehicleEditingMode", 1);
	Sf_SetGlobal(&f, SE, "lastCam", Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetGlobal(&f, SE, "camera", Sf_Ask(&f, "Scrap", "CreateEntity", "(sddds)", "EditCamera", -2431.14, -1913.90, -1758.43,
		"CamPath"));
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, G("camera"), "Name"));
	Sf_SetAttrI(&f, G("camera"), "nWayPoint", 2);
	PyObject *e = Sf_Entity(&f, G("lastCam"));
	if (Sf_Compare(&f, e, "!=", Py_None) > 0) {
		Sf_SetAttr(&f, G("camera"), "Pos00", Sf_Attr(&f, e, "camPos"));
		Sf_SetAttr(&f, G("camera"), "Ang00", Sf_Attr(&f, e, "camAng"));
	} else {
		Sf_SetAttr(&f, G("camera"), "Pos00", Sf_Build(&f, "(iii)", 0, 0, 0));
		Sf_SetAttr(&f, G("camera"), "Ang00", Sf_Build(&f, "(iii)", 0, 0, 0));
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	PyObject *camAng = Sf_Build(&f, "(OOi)", G("EditCameraPitch"), Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, G("editShipPosDummy"),
		2), 1), '-', 180), 0);
	PyObject *back = Sf_Build(&f, "(iiO)", 0, 0, f.failed ? NULL : Sf_Keep(&f, PyNumber_Negative(G("CameraDist"))));
	Sf_SetAttr(&f, G("camera"), "Pos01", Sf_Ask(&f, "SVec", "Add", "(OO)", Sf_Item(&f, G("editShipPosDummy"), 1),
		Sf_Ask(&f, "SVec", "Rotate3D", "(OO)", back, camAng)));
	Sf_SetAttr(&f, G("camera"), "Ang01", camAng);
	Sf_SetAttrI(&f, G("camera"), "FOV01", 70);
	Sf_SetAttrI(&f, G("camera"), "Near01", 50);
	Sf_SetAttrI(&f, G("camera"), "Far01", 200000);
	Sf_SetAttr(&f, G("camera"), "Quake01", Sf_Float(&f, 0.0));
	if (Se_Multi(&f, 1) > 0) {
		Sf_SetAttr(&f, G("camera"), "Time00", Sf_Float(&f, 0.0));
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Se_Clean(&f);
	int mp = Se_Multi(&f, 1);
	if (mp > 0) {
		if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
			Sf_Method(&f, HTA("prevShip"), "Del", "()");
			Sf_SetMod(&f, HT, "prevShip", Py_None);
		}
		PyObject *pilot = Sf_Ask(&f, "Scrap", "Get", "(s)", "PilotModel");
		Sf_SetMod(&f, HT, "currentPilot", Sf_Method(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), "index", "(O)", pilot));
		Sf_SetMod(&f, HT, "currentHangar", Sf_Ask(&f, "Scrap", "Get", "(s)", "iHangarShip"));
		for (int i = 0; i < 9 && !f.failed; i++) {
			int mark = f.n;
			PyObject *cad = Sf_Ask(&f, "Scrap", "Get", "(O)", Sf_Cat(&f, "sO", "HangarShip", Sf_ReprI(&f, i + 1)));
			if (Sf_Compare(&f, cad, "!=", Sf_Str(&f, "")) > 0) {
				Sf_Own(&f, SE, "AssignShip", "(Oi)", cad, i);
			}
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		if (Sf_EqI(&f, HTA("currentHangar"), -1) > 0) {
			Se_DefaultShip(&f, 1);
			long p = 0;
			for (int i = 0; !f.failed; i++) {
				PyObject *s = Sf_Next(&f, G("editedShips"), i);
				if (!s || Sf_Compare(&f, s, "==", Py_None) != 0) {
					break;
				}
				p++;
			}
			if (p >= 9) {
				p = 0;
			}
			Se_MakeDefault(&f, Sf_Int(&f, p), Sf_Str(&f, "P_None"));
			Sf_SetMod(&f, HT, "currentHangar", Sf_Int(&f, p));
			Sf_SetMod(&f, HT, "prevShip", Sf_GetItem(&f, G("editedShips"), Sf_Int(&f, p)));
		} else {
			Sf_SetMod(&f, HT, "prevShip", Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")));
		}
	} else if (mp == 0) {
		Sf_SetMod(&f, HT, "currentHangar", Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.iHangarShip")));
		Sf_SetItem(&f, G("editedShips"), HTA("currentHangar"), HTA("prevShip"));
		for (int i = 0; i < 9 && !f.failed; i++) {
			int mark = f.n;
			if (Sf_Compare(&f, Sf_Int(&f, i), "!=", HTA("currentHangar")) > 0) {
				PyObject *cad = Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", Sf_Cat(&f, "sO", "Hangar.HangarShip",
					Sf_ReprI(&f, i + 1)));
				if (Sf_Compare(&f, cad, "!=", Sf_Str(&f, "")) > 0) {
					Sf_Own(&f, SE, "AssignShip", "(Oi)", cad, i);
				}
			}
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		PyObject *cad = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.HangarShipAux");
		if (Sf_Compare(&f, cad, "!=", Sf_Str(&f, "")) > 0) {
			Sf_Own(&f, SE, "AssignShip", "(Oi)", cad, 9);
		}
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Own(&f, SE, "SetupForEdit", "(O)", G("editedShips"));
	return Sf_None(&f);
}

/* FreeEdit(): the camera of the editor goes, the old one comes back */
static void Se_FreeEdit_(scriptFrame_t *f) {
	if (Sf_True(f, GF("camera")) > 0) {
		Sf_Method(f, GF("camera"), "Del", "()");
		Sf_SetGlobal(f, SE, "camera", Py_None);
		Sf_Ask(f, "Scrap", "SetCam", "(iO)", 0, GF("lastCam"));
	}
}

static void Se_FreeEdit(scriptFrame_t *f) {
	SE_SUB(f, Se_FreeEdit_(&g));
}

static PyObject *SE_FreeEdit(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Se_FreeEdit(&f);
	return Sf_None(&f);
}

/* EnterEditShipMenu(id, control): the editor opens (from the main menu it loads the ShipEdit level first) */
static PyObject *SE_EnterEditShipMenu(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	static const char *const mods[] = { "Menu", HT, "EnginesTab", "WeaponsTab", "HullTab" };
	for (int i = 0; i < 5; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "R_SceneBlurValue", 0);
	if (Sf_True(&f, G("editPlatform")) > 0) {
		Sf_SetAttrS(&f, G("editPlatform"), "OnObjSlot", "");
	}
	Sf_SetGlobal(&f, SE, "prevTab", Py_None);
	Sf_SetGlobal(&f, SE, "fromSputnik", Sf_Int(&f, 0));
	Sf_Import(&f, "Init");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "inMainMenu")) > 0) {
		Sf_Import(&f, "SaveGame");
		Sf_Ask(&f, "SaveGame", "QLoad", "(s)", "Levels/ShipEdit");
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, G("tabs"), "==", Py_None) > 0) {
		Sf_SetGlobal(&f, SE, "tabs", Sf_Build(&f, "[ssss]", "Hangar", "Engines", "Weapons", "Hull"));
	}
	if (Sf_True(&f, HTA("prevShip")) > 0) {
		Sf_SetAttrI(&f, HTA("prevShip"), "ActiveCMask", 0);
	}
	Sf_Own(&f, SE, "SetupEdit", "(i)", id);
	PyObject *lost = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.DestroyedShips"), SE);
	Sf_SetGlobal(&f, SE, "destroyedShips", lost);
	Sf_SetItem(&f, G("destroyedShips"), Sf_Int(&f, 9), Sf_Int(&f, 0));
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, "Menu", "StartNewMenu", "(i)", id);
	Sf_Own(&f, SE, "AddShipEditTabs", "(iO)", id, G("tabs"));
	Sf_Own(&f, SE, "AddDefaultPanelContent", "(ii)", id, 4);
	Sf_Ask(&f, HT, "SetHangarPanel", "(i)", id);
	Sf_Ask(&f, "EnginesTab", "SetEnginesPanel", "(i)", id);
	Sf_Ask(&f, "WeaponsTab", "SetWeaponsPanel", "(i)", id);
	Sf_Ask(&f, "HullTab", "SetHullPanel", "(i)", id);
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, "SScorer", "SetOnSpecialHint", "(iss)", id, "ShipEdit.ShowSpecialHint", f.failed ? "" : Script_Lang("HINT_NoMoney"));
	static const char *const links[] = { "Sell", "Exit", "Hangar2", "Hangar3", "Hangar1" };
	for (int i = 0; i < 5; i++) {
		Se_Link(&f, "LinkUD", id, "ShipEditTabHangar", links[i]);
	}
	Sf_Ask(&f, HT, "SetSellVisible", "(ii)", id, 1);
	Sf_Ask(&f, HT, "SetSellActive", "(ii)", id, 0);
	Sf_Release(&f, f.failed ? f.n : 0);
	PyObject *hangar = Sf_Str(&f, "");
	if (Sf_Compare(&f, HTA("currentHangar"), ">=", Sf_Int(&f, 0)) > 0) {
		hangar = Sf_Cat(&f, "sO", "Hangar", Sf_Repr(&f, Sf_OpI(&f, HTA("currentHangar"), '+', 1)));
	}
	int aux = Sf_EqI(&f, HTA("currentHangar"), 9);
	if (aux > 0) {
		Sf_Ask(&f, HT, "ShowShipInfo", "(i)", id);
		Sf_Ask(&f, "SScorer", "SetOnNext", "(is)", id, "");
		Sf_Ask(&f, "SScorer", "SetOnPrev", "(is)", id, "");
		Sf_Ask(&f, "SScorer", "SetDefault", "(is)", id, "HangarAux");
		Sf_Import(&f, SE);
		Se_TabsOn(&f, id, Sf_Mod(&f, SE, "tabs"), 0);
	} else if (aux == 0 && Sf_Compare(&f, hangar, "!=", Sf_Str(&f, "")) > 0) {
		Sf_SetMod(&f, HT, "prevItem", hangar);
		static const char *const lr[][2] = { { "HangarAux", "Sell" }, { "Hangar3", "Sell" }, { "Hangar9", "Sell" },
			{ "Hangar6", "Sell" }, { "Sell", "Exit" } };
		for (int i = 0; i < 5; i++) {
			Se_Link(&f, "LinkLR", id, lr[i][0], lr[i][1]);
		}
		Se_TabsOn(&f, id, G("tabs"), 1);
		Sf_Ask(&f, "SScorer", "SetOnNext", "(is)", id, "ShipEdit.ChangeTabRight");
		Sf_Ask(&f, "SScorer", "SetOnPrev", "(is)", id, "ShipEdit.ChangeTabLeft");
		Se_TabRing(&f, id, G("tabs"), "LinkLR");
		Sf_Ask(&f, HT, "ShowShipInfo", "(i)", id);
		Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, hangar);
	} else if (!f.failed) {
		Sf_Ask(&f, "SScorer", "SetDefault", "(is)", id, "Hangar1");
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, HT, "ShowHangarPanel", "(i)", id);
	Sf_Ask(&f, "SScorer", "SetOnCancel", "(is)", id, "ShipEdit.ExitEditShipMenu");
	if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
		Se_StartRotate(&f);
	}
	return Sf_None(&f);
}

/* ExitEditShipMenu(id, control): the editor closes: Rusty and the maintenance robot step back, the ships are saved,
 * the camera comes back, the callback (or the multiplayer menu) follows */
static PyObject *SE_ExitEditShipMenu(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	static const char *const mods[] = { "Menu", HT, "EnginesTab", "WeaponsTab", "HullTab", "SaveGame" };
	for (int i = 0; i < 6; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *rusty = Sf_Entity(&f, Sf_Str(&f, "Rusty_0"));
	if (Se_RustyOn(&f, rusty) > 0) {
		PyObject *plt = Sf_Entity(&f, Sf_Str(&f, "DM_Door_EditShip_00"));
		PyObject *at = Sf_Build(&f, "(OOO)", Sf_Item(&f, Sf_Attr(&f, plt, "Pos"), 0), Sf_Item(&f, Sf_Attr(&f, rusty, "Pos"), 1),
			Sf_Item(&f, Sf_Attr(&f, plt, "Pos"), 2));
		PyObject *vec = Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, rusty, "Pos"), at);
		vec = Sf_Ask(&f, "SVec", "Norm", "(O)", vec);
		Sf_SetAttr(&f, rusty, "Pos", Sf_Build(&f, "(OOO)", Sf_OpI(&f, Sf_Item(&f, vec, 0), '*', 1000), Sf_Item(&f,
			Sf_Attr(&f, rusty, "Pos"), 1), Sf_OpI(&f, Sf_Item(&f, vec, 2), '*', 1000)));
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	PyObject *robot = Sf_Entity(&f, Sf_Str(&f, "Maintenance_00"));
	if (Sf_True(&f, robot) > 0) {
		Sf_SetAttrI(&f, robot, "Active", 0);
		Sf_SetAttr(&f, robot, "Pos", Sf_Build(&f, "(iii)", -908, -1940, 458));
		Sf_SetAttrI(&f, robot, "Active", 1);
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(ss)", "Levels/Junkyard", "DM_Player_Spawn_");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Hangar.DestroyedShips", Sf_Repr(&f, G("destroyedShips")));
	Sf_SetMod(&f, HT, "holograms", Sf_Int(&f, 0));
	Sf_Ask(&f, "SSound", "SetPS", "(ssd)", "EditHologramLoopSound", "Volume", 0.0);
	Sf_Ask(&f, "SSound", "StopAllSounds", "()");
	if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
		Se_StopRotate(&f);
		Sf_SetAttr(&f, HTA("prevShip"), "Ang", Se_FrontAng(&f, G("editShipPosDummy"), NULL));
		Sf_SetAttrI(&f, HTA("prevShip"), "ActiveCMask", 1);
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, HT, "HideHangarPanel", "(i)", id);
	Sf_Ask(&f, "EnginesTab", "HideEnginesPanel", "(i)", id);
	Sf_Ask(&f, "WeaponsTab", "HideWeaponsPanel", "(i)", id);
	Sf_Ask(&f, "HullTab", "HideHullPanel", "(i)", id);
	Sf_Own(&f, SE, "SaveShip", "(i)", id);
	Sf_Own(&f, SE, "FreeEdit", "()");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "VehicleEditingMode", 0);
	Sf_SetAttrS(&f, G("editPlatform"), "OnObjSlot", "!Usr0AutoDoorSlot");
	Sf_Release(&f, f.failed ? f.n : 0);
	if (Sf_True(&f, G("EndShipEditCallBack")) > 0) {
		Sf_Call(&f, G("EndShipEditCallBack"), "(iO)", id, control);
	} else if (Sf_True(&f, G("isXbox")) > 0) {
		Sf_Import(&f, "XboxMenu");
		Sf_Ask(&f, "XboxMenu", "MultiPlayerMenu", "(iO)", id, control);
	} else {
		Sf_Import(&f, "PCMenu");
		Sf_Ask(&f, "PCMenu", "MultiPlayerMenu", "(iO)", id, control);
	}
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetNetFlags", "()"), "!=", Sf_Build(&f, "(iii)", 0, 0, 0)) > 0) {
		Sf_Import(&f, "Ffa");
		Sf_Ask(&f, "Ffa", "ModiTry", "()");
	}
	return Sf_None(&f);
}

/* (s.MEng0 != "") or ... or (s.MEng3 != ""): the ship has an engine (1, 0, -1) */
static int Se_HasEngine_(scriptFrame_t *f, PyObject *s) {
	for (int i = 0; i < 4; i++) {
		char var[8];
		Str_Printf(var, sizeof(var), "MEng%d", i);
		int c = Sf_Compare(f, Sf_Attr(f, s, var), "!=", Sf_Str(f, ""));
		if (c != 0) {
			return c;
		}
	}
	return 0;
}

static int Se_HasEngine(scriptFrame_t *f, PyObject *s) {
	int r = -1;
	SE_SUB(f, r = Se_HasEngine_(&g, s));
	return f->failed ? -1 : r;
}

/* SaveShip(id): in a network game the selected (or a valid, or a default) ship becomes the player's ship and the
 * hangars go to the engine variables; in single player to the save variables, an unpaid or destroyed ship goes */
static PyObject *SE_SaveShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, HT);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "ShipMaker");
	Sf_Item(&f, G("editShipPosDummy"), 1);
	PyObject *ship = HTA("prevShip");
	int setDefault = 1, notValid = 0;
	int mp = Se_Multi(&f, 1);
	if (mp > 0) {
		if (Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
			int has = Se_HasEngine(&f, HTA("prevShip"));
			if (has == 0) {
				notValid = 1;
			} else if (has > 0) {
				setDefault = 0;
			}
		} else {
			notValid = 1;
		}
		PyObject *firstFree = Py_None;
		if (notValid) {
			int n = Sf_Len(&f, G("editedShips"));
			for (int i = 0; i < n && !f.failed; i++) {
				PyObject *keep[2] = { ship, NULL };
				int mark = f.n;
				PyObject *s = Sf_Item(&f, G("editedShips"), i);
				int c = Sf_Compare(&f, s, "!=", Py_None);
				if (c > 0) {
					if (Se_HasEngine(&f, Sf_Item(&f, G("editedShips"), i)) > 0) {
						setDefault = 0;
						ship = Sf_Item(&f, G("editedShips"), i);
						Sf_SetMod(&f, HT, "currentHangar", Sf_Int(&f, i));
						break;
					}
				} else if (c == 0 && firstFree == Py_None) {
					firstFree = Sf_Int(&f, i);
				}
				keep[1] = firstFree == Py_None ? NULL : firstFree;
				Sf_ReleaseKeep(&f, mark, keep, f.failed ? 0 : 2);
			}
		}
		if (Sf_True(&f, G("isXbox")) == 0) {
			Sf_Ask(&f, "Scrap", "Set", "(sO)", "PlayerName", Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "PilotName", "Text"));
		}
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "PilotModel", Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"),
			HTA("currentPilot")));
		if (setDefault) {
			Se_DefaultShip(&f, 0);
			if (firstFree != Py_None) {
				Se_MakeDefault(&f, firstFree, NULL);
				Sf_SetMod(&f, HT, "currentHangar", firstFree);
			}
		}
		if (!setDefault && Sf_Compare(&f, ship, "!=", Py_None) > 0) {
			static const char *const vars[][2] = { { "PlayerMaxLife", "MaxLife" }, { "PlayerModel", "Model" },
				{ "Motor0Model", "MEng0" }, { "Motor1Model", "MEng1" }, { "Motor2Model", "MEng2" },
				{ "Motor3Model", "MEng3" }, { "WeaponBayList", "WeapList" } };
			for (int i = 0; i < 7; i++) {
				Sf_Ask(&f, "Scrap", "Set", "(sO)", vars[i][0], Sf_Attr(&f, ship, vars[i][1]));
			}
		}
		int n = Sf_Len(&f, G("editedShips"));
		for (int i = 0; i < n - 1 && !f.failed; i++) {
			int mark = f.n;
			Se_SaveShip(&f, Sf_Item(&f, G("editedShips"), i), Sf_Int(&f, i), 0);
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "iHangarShip", HTA("currentHangar"));
		Sf_Ask(&f, "Scrap", "SaveConfig", "()");
	} else if (mp == 0) {
		int n = Sf_Len(&f, G("editedShips"));
		for (int i = 0; i < n && !f.failed; i++) {
			int mark = f.n;
			Se_SaveShip(&f, Sf_Item(&f, G("editedShips"), i), Sf_Int(&f, i), 1);
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		Sf_Import(&f, SE);
		int c = Sf_Compare(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "==", Py_None);
		if (c == 0) {
			c = Sf_EqI(&f, Sf_GetItem(&f, Sf_Mod(&f, SE, "destroyedShips"), HTA("currentHangar")), 1);
			if (c > 0) {
				int m = Se_Multi(&f, 0);
				c = m < 0 ? -1 : !m;
			}
		}
		if (c > 0 && Sf_Compare(&f, HTA("prevShip"), "!=", Py_None) > 0) {
			PyObject *kind = Sf_GetItem(&f, Sf_Mod(&f, SE, "auxShipsToEditList"), HTA("currentShipToEdit"));
			int built = Sf_Compare(&f, kind, "!=", Sf_Str(&f, "Rebuild"));
			if (built > 0) {
				Sf_Method(&f, HTA("prevShip"), "Del", "()");
			} else if (built == 0) {
				Sf_Method(&f, Sf_GetItem(&f, G("editedShips"), HTA("currentHangar")), "Del", "()");
				Sf_SetItem(&f, G("editedShips"), HTA("currentHangar"), Py_None);
			}
			Sf_SetMod(&f, HT, "prevShip", Py_None);
			Sf_SetMod(&f, HT, "currentHangar", Sf_Int(&f, -1));
		}
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Hangar.iHangarShip", Sf_Repr(&f, HTA("currentHangar")));
	}
	return Sf_None(&f);
}

/* ShowSpecialHint(): the special hint (no money) shows while the money prediction is red */
static PyObject *SE_ShowSpecialHint(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int c = Sf_EqI(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "MoneyPredict", "Green"), 0);
	if (c > 0) {
		c = Sf_EqI(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "MoneyPredict", "Visible"), 1);
	}
	return Sf_Return(&f, c >= 0 ? Sf_Int(&f, c) : NULL);
}

PyMethodDef g_ShipEditMethods[] = {
	{ "IsEditingMultiplayer", SE_IsEditingMultiplayer, 1, NULL },
	{ "RustyOnPlatform", SE_RustyOnPlatform, 1, NULL },
	{ "PointObjectTo", SE_PointObjectTo, 1, NULL },
	{ "LaunchPlatformAnim", SE_LaunchPlatformAnim, 1, NULL },
	{ "ChangeShip", SE_ChangeShip, 1, NULL },
	{ "VerifyChangeShip", SE_VerifyChangeShip, 1, NULL },
	{ "DoorInitializator", SE_DoorInitializator, 1, NULL },
	{ "ObjInitializator", SE_ObjInitializator, 1, NULL },
	{ "EndMoveEngine", SE_EndMoveEngine, 1, NULL },
	{ "RotateShip", SE_RotateShip, 1, NULL },
	{ "LoopRotateShip", SE_LoopRotateShip, 1, NULL },
	{ "StartRotateLoop", SE_StartRotateLoop, 1, NULL },
	{ "StopRotateLoop", SE_StopRotateLoop, 1, NULL },
	{ "DrawBackground", (PyCFunction)(void (*)(void))ShipEdit_DrawBackground, 3, NULL },
	{ "AddShipEditTabs", SE_AddShipEditTabs, 1, NULL },
	{ "AddDefaultPanelContent", SE_AddDefaultPanelContent, 1, NULL },
	{ "ChangeTabLeft", SE_ChangeTabLeft, 1, NULL },
	{ "ChangeTabRight", SE_ChangeTabRight, 1, NULL },
	{ "ChangeTab", (PyCFunction)(void (*)(void))SE_ChangeTab, 3, NULL },
	{ "CleanEditedShips", SE_CleanEditedShips, 1, NULL },
	{ "CreatePlayerShip", SE_CreatePlayerShip, 1, NULL },
	{ "SetupForEdit", SE_SetupForEdit, 1, NULL },
	{ "SaveShipInfoInSaveVar", SE_SaveShipInfoInSaveVar, 1, NULL },
	{ "SaveShipInfoInGVar", SE_SaveShipInfoInGVar, 1, NULL },
	{ "CreateShipFromSaveVar", (PyCFunction)(void (*)(void))SE_CreateShipFromSaveVar, 3, NULL },
	{ "AssignShip", SE_AssignShip, 1, NULL },
	{ "SetupEdit", SE_SetupEdit, 1, NULL },
	{ "FreeEdit", SE_FreeEdit, 1, NULL },
	{ "EnterEditShipMenu", SE_EnterEditShipMenu, 1, NULL },
	{ "ExitEditShipMenu", SE_ExitEditShipMenu, 1, NULL },
	{ "SaveShip", SE_SaveShip, 1, NULL },
	{ "ShowSpecialHint", SE_ShowSpecialHint, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
