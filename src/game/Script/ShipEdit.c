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
 * (the save variables of the hangars) stays in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, the PyMethodDef entries are adapters with
 * the argument formats of the C port. The variables of the module are read where the script reads them and written at
 * once (OBJLIB 5); the ships, the camera and the platform are handle values (OBJLIB 7), editedShips and tabPixelX are
 * changed in place on their objects. HangarTab, the tabs, Elevator, Vehicles, Parking, Menu, SaveGame, the menus of
 * the multiplayer game and the module's own functions the script calls as its globals stay by name (OBJLIB 3); the
 * .def runner takes Python values (OBJLIB 6). The texts of the ships in the save and engine variables are made as
 * before.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/scrap.h"
#include "game/Script/levels/Levels.h"
#include "ShipEdit.h"

static const char shipedit_def[] = {
#embed "../../../data/menus/shipedit.def"
	, 0 };

static const nativeVal_t se_none = { 0 };

/* the module whose UseTxt the step use formats with */
static const char *se_useModule = SE;

/* run use <Attr> <key> [item]: Attr = GetLangStr(key) % UseTxt (of the current item, or of item); the error left set
 * stops the file */
static void Step_Use(int id, PyObject *args) {
	char *var, *key, *item = NULL;
	if (PyArg_ParseTuple(args, "ss|s", &var, &key, &item)) {
		Sn_SetUseC(id, item ? item : Script_CurrentItem(), var, key, se_useModule);
	}
}

/* run sputnikhint <n>: the hint of hangar n on Sputnik's screen (no ship, a destroyed one with the rebuild cost, an
 * incomplete one, a valid one) */
static void Step_SputnikHint(int id, PyObject *args) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *i = Mode_V(&t), *e = Mode_V(&t), *x = Mode_V(&t), *r = Mode_V(&t), *cad = Mode_V(&t), *cost = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *lang = Mode_V(&t), *use = Mode_V(&t), *text = Mode_V(&t), *rep = Mode_V(&t);
	long n;
	if (!PyArg_ParseTuple(args, "l", &n)) {
		return;
	}
	char name[0x20];
	Str_Printf(name, sizeof(name), "Hangar%ld", n);
	Native_SetInt(i, n - 1);
	int none = Sn_At(SE, "editedShips", i, e) ? Mode_Cmp(e, "==", &se_none) : -1;
	int lost = 0;
	if (none == 0) {
		lost = Sn_At(SE, "destroyedShips", i, x) ? LvN_EqI(x, 1) : -1;
		if (lost > 0) {
			lost = Sn_Multi(0);
			lost = lost < 0 ? -1 : !lost;
		}
	}
	byte ok = none >= 0 && lost >= 0;
	if (ok && none) {
		Script_SetStr(id, name, "Hint", Script_Lang("HINT_Sputnik_NoShip"));
	} else if (ok && lost) {
		Mode_Repr(i, r);
		ok = Mode_Import("string") && Sn_Cat("ShipEdit.editedShips[", r, "]", cad) &&
			Native_CallGlobal(HT, "ComputeShipTotalCost", cost, "v", cad);
		Native_SetInt(money, Mode_GetMoney());
		int c = ok ? Mode_Cmp(money, ">=", cost) : -1;
		if (c > 0) {
			Mode_Lang("HINT_Sputnik_DestroyedShip", lang);
			Native_Free(r);
			ok = Mode_G(HT, "UseTxt", use) && Native_Arith('%', lang, use, text);
			if (ok) {
				Mode_Repr(cost, rep);
				Mode_Replace(text, "<n>", rep, r);
			}
		} else if (c == 0) {
			Native_Free(r);
			Mode_Lang("HINT_Sputnik_CantRebuildShip", r);
		}
		ok = ok && c >= 0 && Sn_SetC(id, name, "Hint", r);
		if (ok) {
			Script_SetStr(id, name, "OnGainFocus", "HangarTab.ShowRebuildShipMoney");
			Script_SetStr(id, name, "OnLooseFocus", "HangarTab.HideRebuildShipMoney");
		}
	} else if (ok) {
		Native_Free(e);
		ok = Sn_At(SE, "editedShips", i, e) && Native_CallGlobal(HT, "IsShipValid", x, "v", e);
		if (ok && !Native_True(x)) {
			Script_SetStr(id, name, "Hint", Script_Lang("HINT_Sputnik_IncompleteShip"));
		} else if (ok) {
			ok = Sn_SetUseC(id, name, "Hint", "HINT_Sputnik_ValidShip", HT);
		}
	}
	Mode_Done(&t, ok);
}

/* run ramp: SpeedRamp2.X = baseShipInfoX + speedRampX + SpeedRamp1.ScaleX * SpeedRamp1.W */
static void Step_Ramp(int id, PyObject *args) {
	nativeVal_t a = { 0 }, b = { 0 }, x = { 0 }, sx = { 0 }, w = { 0 }, p = { 0 }, s = { 0 };
	(void)args;
	byte ok = Mode_G(HT, "baseShipInfoX", &a) && Mode_G(HT, "speedRampX", &b) && Mode_Arith('+', &a, &b, &x);
	if (ok) {
		Mode_ScorerGet(id, "SpeedRamp1", "ScaleX", &sx);
		Mode_ScorerGet(id, "SpeedRamp1", "W", &w);
		ok = Mode_Arith('*', &sx, &w, &p) && Mode_Arith('+', &x, &p, &s) && Sn_SetC(id, "SpeedRamp2", "X", &s);
	}
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&x);
	Native_Free(&sx);
	Native_Free(&w);
	Native_Free(&p);
	Native_Free(&s);
}

static const scriptStep_t shipedit_steps[] = {
	{ "use", Step_Use },
	{ "sputnikhint", Step_SputnikHint },
	{ "ramp", Step_Ramp },
	{ NULL, NULL },
};

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

/* a global of ShipEdit as its functions read it (the NameError of a missing one) */
static byte Se_G(const char *name, nativeVal_t *out) {
	return Mode_G(SE, name, out);
}

/* Scrap.GetEntity(name) through its seam */
static byte Se_Entity(const char *name, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetStr(&n, name);
	return Mode_EntitySeam(&n, out);
}

/* (v[0], 0, v[2]) of a position */
static byte Se_Flat(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t a = { 0 }, b = { 0 };
	Native_SetNone(out);
	byte ok = LvN_Item(v, 0, &a) && LvN_Item(v, 2, &b);
	if (ok) {
		Native_SetEmpty(out, NV_TUPLE);
		*Native_Append(out) = a;
		Native_SetInt(Native_Append(out), 0);
		*Native_Append(out) = b;
	} else {
		Native_Free(&a);
		Native_Free(&b);
	}
	return ok;
}

/* the ship of HangarTab: HangarTab.prevShip (its attribute) */
static byte Se_Ship(nativeVal_t *out) {
	return LvN_ModGet(HT, "prevShip", out);
}

/* HangarTab.prevShip != None: 1, 0, -1 */
static int Se_HasShip(void) {
	nativeVal_t s = { 0 };
	int r = Se_Ship(&s) ? Mode_Cmp(&s, "!=", &se_none) : -1;
	Native_Free(&s);
	return r;
}

/* HangarTab.prevShip.var = i, = "text", = v */
static byte Se_ShipSetI(const char *var, long i) {
	nativeVal_t s = { 0 };
	byte ok = Se_Ship(&s) && LvN_SetI(&s, var, i);
	Native_Free(&s);
	return ok;
}

static byte Se_ShipSetS(const char *var, const char *text) {
	nativeVal_t s = { 0 };
	byte ok = Se_Ship(&s) && LvN_SetS(&s, var, text);
	Native_Free(&s);
	return ok;
}

static byte Se_ShipSet(const char *var, const nativeVal_t *v) {
	nativeVal_t s = { 0 };
	byte ok = Se_Ship(&s) && Mode_VSet(&s, var, v);
	Native_Free(&s);
	return ok;
}

/* HangarTab.prevShip.var */
static byte Se_ShipGet(const char *var, nativeVal_t *out) {
	nativeVal_t s = { 0 };
	byte ok = Se_Ship(&s) && Mode_VGet(&s, var, out);
	Native_Free(&s);
	return ok;
}

/* editedShips[HangarTab.currentHangar] (the global list first) and destroyedShips[...] */
static byte Se_AtCur(const char *var, nativeVal_t *out) {
	nativeVal_t l = { 0 }, k = { 0 };
	Native_SetNone(out);
	byte ok = Se_G(var, &l) && LvN_ModGet(HT, "currentHangar", &k) && Mode_Sub(&l, &k, out);
	Native_Free(&l);
	Native_Free(&k);
	return ok;
}

/* the global list[key] = v, in place */
static byte Se_SetAt(const char *var, const nativeVal_t *key, const nativeVal_t *v) {
	return Mode_DictPut(SE, var, NULL, key, v);
}

/* the global list[i] (any item) */
static byte Se_AtI(const char *var, long i, nativeVal_t *out) {
	nativeVal_t l = { 0 };
	Native_SetNone(out);
	byte ok = Se_G(var, &l) && LvN_Item(&l, i, out);
	Native_Free(&l);
	return ok;
}

/* IsEditingMultiplayer(): the editor of a network game (or of the ShipEdit level): Init.isShipEdit, else
 * Scrap.GetNetFlags() != (0, 0, 0) */
static byte Se_IsEditingMultiplayer(nativeVal_t *out) {
	if (!Mode_Import("Init") || !LvN_ModGet("Init", "isShipEdit", out)) {
		return 0;
	}
	if (Native_True(out)) {
		return 1;
	}
	int off = Native_NetOff();
	Native_Free(out);
	Native_SetInt(out, !off);
	return off >= 0;
}

/* RustyOnPlatform(rusty): the scrap dealer stands on the platform (0 or 1) */
static byte Se_RustyOnPlatform(const nativeVal_t *rusty, nativeVal_t *out) {
	nativeVal_t plt = { 0 }, pos = { 0 }, pp = { 0 }, rp = { 0 }, d = { 0 }, m = { 0 }, r = { 0 };
	Native_SetNone(out);
	byte ok = Se_G("editPlatform", &plt) && Mode_VGet(&plt, "Pos", &pos) && Se_Flat(&pos, &pp);
	Native_Free(&pos);
	ok = ok && Mode_VGet(rusty, "Pos", &pos) && Se_Flat(&pos, &rp) && Mode_VecSub(&rp, &pp, &d) && Mode_VecModSqr(&d, &m) &&
		Native_GVarGet("PlatformRadiusSqr", &r);
	int c = ok ? Mode_Cmp(&m, "<=", &r) : -1;
	if (c >= 0) {
		Native_SetInt(out, c);
	}
	Native_Free(&plt);
	Native_Free(&pos);
	Native_Free(&pp);
	Native_Free(&rp);
	Native_Free(&d);
	Native_Free(&m);
	Native_Free(&r);
	return c >= 0;
}

/* rusty and RustyOnPlatform(rusty): 1, 0, -1 */
static int Se_RustyOn(const nativeVal_t *rusty) {
	if (!Native_True(rusty)) {
		return 0;
	}
	nativeVal_t v = { 0 };
	int r = Se_RustyOnPlatform(rusty, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* PointObjectTo(object, point): the object turns to the point (around the vertical axis) */
static byte Se_PointTo(const nativeVal_t *obj, const nativeVal_t *point) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *start = Mode_V(&t), *p = Mode_V(&t), *pos = Mode_V(&t), *q = Mode_V(&t), *pq = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *d = Mode_V(&t), *y = Mode_V(&t), *neg = Mode_V(&t), *a = Mode_V(&t), *z = Mode_V(&t);
	Mode_Ints3(start, 0, 0, -1);
	byte ok = Se_Flat(point, p) && Mode_VGet(obj, "Pos", pos) && Se_Flat(pos, q) && Mode_VecSub(p, q, pq) &&
		Native_CallMod("SVec", "GetAngle", ang, "vv", start, pq) && Native_CallMod("SVec", "CProd", d, "vv", start, pq) &&
		LvN_Item(d, 1, y);
	int c = ok ? Mode_CmpI(y, "<", 0) : -1;
	if (c > 0) {
		ok = Mode_Neg(ang, neg);
	}
	if (!ok || c < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(z, 0);
	Mode_Tuple3(a, z, c ? neg : ang, z);
	return Mode_Done(&t, Mode_VSet(obj, "Ang", a));
}

/* LaunchPlatformAnim(): the platform changes the ship, Rusty rides along when he stands on it */
static byte Se_LaunchPlatformAnim(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rusty = Mode_V(&t), *plt = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *c = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *ppos = Mode_V(&t), *off = Mode_V(&t), *name = Mode_V(&t), *s = Mode_V(&t);
	nativeVal_t *e = Mode_V(&t), *o0 = Mode_V(&t), *o2 = Mode_V(&t), *x = Mode_V(&t);
	if (!Se_Entity("Rusty_0", rusty)) {
		return Mode_Done(&t, 0);
	}
	int on = Se_RustyOn(rusty);
	if (on < 0) {
		return Mode_Done(&t, 0);
	}
	if (on) {
		byte ok = Se_Entity("DM_Door_EditShip_00", plt) && Mode_VGet(rusty, "FloorDist", a) &&
			Mode_VGet(plt, "NodePos_DC_Platform", x) && LvN_Item(x, 1, b) && Mode_Arith('+', a, b, c);
		Native_Free(x);
		Native_Free(b);
		ok = ok && Mode_VGet(plt, "NodePos_DC_ShipLink", x) && LvN_Item(x, 1, b) && Mode_Arith('-', c, b, y) &&
			Mode_VGet(rusty, "Pos", pos) && Mode_VGet(plt, "Pos", ppos) && Mode_VecSub(pos, ppos, off) &&
			Se_G("editPlatform", e) && Mode_VGet(e, "Name", name) && Sn_Cat("", name, ",", s);
		Native_Free(a);
		Native_SetStr(b, "DC_ShipLink");
		ok = ok && Native_Arith('+', s, b, a) && Mode_VSet(rusty, "LinkModel", a) && LvN_Item(off, 0, o0) &&
			LvN_Item(off, 2, o2);
		if (ok) {
			Native_Free(a);
			Mode_Tuple3(a, o0, y, o2);
		}
		ok = ok && Mode_VSet(rusty, "OffModPos", a) && LvN_SetF(rusty, "DeltaOffMul", 0.0) && LvN_SetF(rusty, "OffMul", 1.0) &&
			LvN_SetS(rusty, "Action", "TalkListenNPC") && LvN_SetS(rusty, "DefLoop", "TalkListenNPC");
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(e);
	return Mode_Done(&t, Se_G("editPlatform", e) && LvN_SetS(e, "Action", "Change"));
}

/* editedShips[HangarTab.currentHangar] != None and (destroyedShips[...] == 0 or IsEditingMultiplayer()): 1, 0, -1 */
static int Se_CurIntact(void) {
	nativeVal_t v = { 0 };
	int c = Se_AtCur("editedShips", &v) ? Mode_Cmp(&v, "!=", &se_none) : -1;
	Native_Free(&v);
	if (c > 0) {
		c = Se_AtCur("destroyedShips", &v) ? LvN_EqI(&v, 0) : -1;
		Native_Free(&v);
		if (c == 0) {
			c = Sn_Multi(1);
		}
	}
	return c;
}

/* ChangeShip(EntityName, EventName, Time): the event in the middle of the Change animation: the ship of the selected
 * hangar appears (unless it is destroyed), Rusty on the platform faces the camera */
static byte Se_ChangeShip(const nativeVal_t *entityName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rusty = Mode_V(&t), *cam = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t), *ship = Mode_V(&t);
	if (!Mode_Import(HT)) {
		return Mode_Done(&t, 0);
	}
	int c = Se_HasShip();
	if (c < 0 || (c > 0 && !Se_ShipSetI("Active", 0))) {
		return Mode_Done(&t, 0);
	}
	c = Mode_Cmp(entityName, "!=", &se_none);
	if (c > 0) {
		int on = Se_Entity("Rusty_0", rusty) ? Se_RustyOn(rusty) : -1;
		if (on > 0) {
			byte ok = Mode_Cam0(cam);
			if (ok && !Native_True(cam)) {
				ok = Se_Entity("EditCamera", e) && Mode_VGet(e, "Pos", pos) && Se_PointTo(rusty, pos);
			} else if (ok) {
				ok = Mode_VGet(cam, "camPos", pos) && Se_PointTo(rusty, pos);
			}
			on = ok ? 1 : -1;
		}
		c = on < 0 ? -1 : 1;
	}
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	c = Se_CurIntact();
	if (c > 0) {
		byte ok = Se_AtCur("editedShips", ship) && LvN_SetI(ship, "Active", 1);
		Native_Free(ship);
		c = ok && Se_AtCur("editedShips", ship) && Mode_SetModVar(HT, "prevShip", ship) ? 1 : -1;
	}
	return Mode_Done(&t, c >= 0);
}

/* the ship type of the destroyed ship and the list of ship types with "Rebuild" in its place, before the plans show */
static byte Se_RebuildList(const nativeVal_t *ship, const nativeVal_t *isDestroyed) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *x = Mode_V(&t), *name = Mode_V(&t), *pos = Mode_V(&t), *cur = Mode_V(&t), *k = Mode_V(&t);
	nativeVal_t *aux = Mode_V(&t);
	int c = Native_True(ship) && Native_True(isDestroyed);
	if (c) {
		int n = LvN_ModGet(SE, "shipsToEditList", l) ? Mode_Len(l) : -1;
		byte ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			Native_Free(l);
			Native_Free(x);
			Native_Free(name);
			Native_Free(pos);
			ok = Mode_VGet(ship, "Name", name) && LvN_ModGet(SE, "shipsToEditList", l) && LvN_Item(l, i, x) &&
				Native_CallMod("string", "find", pos, "vv", name, x);
			int f = ok ? Mode_CmpI(pos, "!=", -1) : -1;
			if (f > 0) {
				ok = Mode_SetModVar(HT, "currentShipToEdit", Mode_IntVal(k, i));
				break;
			}
			ok = ok && f == 0;
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(l);
	int bad = LvN_ModGet(HT, "currentShipToEdit", cur) ? Mode_CmpI(cur, "<", 0) : -1;
	if (bad == 0) {
		int n = Se_G("shipsToEditList", l) ? Mode_Len(l) : -1;
		Native_Free(cur);
		bad = n < 0 || !LvN_ModGet(HT, "currentShipToEdit", cur) ? -1 : Mode_CmpI(cur, ">", n - 1);
	}
	if (bad < 0 || (bad > 0 && !Mode_SetModVar(HT, "currentShipToEdit", Mode_IntVal(k, 0)))) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(aux, NV_LIST);
	Native_Free(l);
	int n = Mode_Put(SE, "auxShipsToEditList", NULL, aux) && Se_G("shipsToEditList", l) ? Mode_Len(l) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_Free(l);
		Native_Free(x);
		ok = Se_G("shipsToEditList", l) && LvN_Item(l, i, x) && Mode_Append(SE, "auxShipsToEditList", x);
	}
	int rebuild = ok ? Native_True(isDestroyed) : -1;
	if (rebuild > 0) {
		int mp = Sn_Multi(1);
		rebuild = mp < 0 ? -1 : !mp;
	}
	if (rebuild > 0) {
		Native_SetStr(x, "Rebuild");
		Native_Free(cur);
		Native_Free(l);
		Native_Free(name);
		ok = LvN_ModGet(HT, "currentShipToEdit", cur) && Se_SetAt("auxShipsToEditList", cur, x) && Se_G("shipsToEditList", l) &&
			LvN_ModGet(HT, "currentShipToEdit", k) && Mode_Sub(l, k, name) && Mode_Append(SE, "auxShipsToEditList", name);
		rebuild = ok ? 1 : -1;
	}
	return Mode_Done(&t, rebuild >= 0);
}

/* VerifyChangeShip(EntityName, EventName, Time): the end of the Change animation: Rusty runs off the platform; an
 * empty or destroyed hangar shows the ship plans to buy (with "Rebuild" at the place of the destroyed one), else the
 * ship information; the buttons work again */
static byte Se_VerifyChangeShip(const nativeVal_t *entityName) {
	const int id = 0;
	modeVals_t t = { .n = 0 };
	nativeVal_t *rusty = Mode_V(&t), *plt = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *y = Mode_V(&t), *off = Mode_V(&t);
	nativeVal_t *o0 = Mode_V(&t), *o2 = Mode_V(&t), *pos = Mode_V(&t), *ship = Mode_V(&t), *lostv = Mode_V(&t);
	nativeVal_t *isDestroyed = Mode_V(&t), *x = Mode_V(&t), *k = Mode_V(&t);
	if (!Mode_Import(SE) || !Mode_Import(HT) || !Mode_Import("string") || !Se_Entity("Rusty_0", rusty)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_Cmp(entityName, "!=", &se_none);
	if (c > 0) {
		c = Se_RustyOn(rusty);
	}
	if (c > 0) {
		byte ok = Se_Entity("DM_Door_EditShip_00", plt) && LvN_SetS(rusty, "LinkModel", ",") &&
			Mode_VGet(plt, "NodePos_DC_Platform", x) && LvN_Item(x, 1, a) && Mode_VGet(rusty, "FloorDist", b) &&
			Mode_Arith('+', a, b, y) && Mode_VGet(rusty, "OffModPos", off) && LvN_Item(off, 0, o0);
		Native_Free(off);
		ok = ok && Mode_VGet(rusty, "OffModPos", off) && LvN_Item(off, 2, o2);
		if (ok) {
			Mode_Tuple3(pos, o0, y, o2);
		}
		c = ok && Mode_VSet(rusty, "Pos", pos) && LvN_SetI(rusty, "LLogicState", 9) && LvN_SetS(rusty, "LLogicTarget", "") ? 1 : -1;
	}
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok = LvN_ModGet(SE, "editedShips", x) && LvN_ModGet(HT, "currentHangar", k) && Mode_Sub(x, k, ship) &&
		Se_AtCur("destroyedShips", lostv);
	int lost = ok ? LvN_EqI(lostv, 1) : -1;
	if (lost < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(isDestroyed, lost);
	c = Mode_Cmp(ship, "==", &se_none);
	if (c == 0 && lost) {
		int mp = Sn_Multi(1);
		c = mp < 0 ? -1 : !mp;
	}
	if (c > 0) {
		Script_SetDefault(id, "ShipType");
		ok = Se_RebuildList(ship, isDestroyed) && Native_CallMod(HT, "SelectShipToEdit", NULL, "i", (long)id);
		int has = ok ? Se_HasShip() : -1;
		ok = has == 0 || (has > 0 && Native_CallMod(HT, "ShowShipInfo", NULL, "ii", (long)id, 0L) &&
			Native_CallGlobal(SE, "StartRotateLoop", NULL, ""));
	} else if (c == 0) {
		int has = Se_HasShip();
		ok = has == 0 || (has > 0 && Native_CallMod(HT, "ShowShipInfo", NULL, "ii", (long)id, 1L) &&
			Native_CallGlobal(SE, "StartRotateLoop", NULL, ""));
	} else {
		ok = 0;
	}
	return Mode_Done(&t, ok && Native_CallMod(HT, "EnableChangePlatformButtons", NULL, "i", (long)id));
}

/* DoorInitializator(DoorClass): the door class of the platform: an elevator with the Change animation and its two
 * events */
static byte Se_DoorInitializator(const nativeVal_t *cls) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *path = Mode_V(&t), *total = Mode_V(&t), *half = Mode_V(&t), *two = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t);
	if (!Mode_Import("Elevator") || !Native_CallMod("Elevator", "DoorInitializator", NULL, "v", cls) ||
		!LvN_SetCls("VRadius", Mode_IntVal(v, 500)) || !LvN_CreateAction("Change", r) ||
		!Sn_Cat("Models/Doors/", cls, "/Anm/Change.M3D", path) || !LvN_SetAct("AnmFilePath", path) ||
		!LvN_SetAct("InterpTime", Mode_IntVal(v, 0)) || !LvN_SetAct("isLoop", Mode_IntVal(v, 0))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(two, 2.0);
	byte ok = LvN_GetAct("AnmTotalTime", total) && Mode_Arith('/', total, two, half) && LvN_SetAct("AddEventAtTime", half);
	Native_SetStr(v, "ChangeShip");
	ok = ok && LvN_SetAct("EventName", v);
	Native_SetStr(v, "ShipEdit.ChangeShip");
	ok = ok && LvN_SetAct("EventFunc", v);
	Native_Free(total);
	ok = ok && LvN_GetAct("AnmTotalTime", total) && LvN_SetAct("AddEventAtTime", total);
	Native_SetStr(v, "EndChangeShip");
	ok = ok && LvN_SetAct("EventName", v);
	Native_SetStr(v, "ShipEdit.VerifyChangeShip");
	return Mode_Done(&t, ok && LvN_SetAct("EventFunc", v));
}

/* ObjInitializator(elv, ExtraInfo): the platform entity: editPlatform, and editShipPosDummy (the place of the ship) */
static byte Se_ObjInitializator(const nativeVal_t *elv, const nativeVal_t *extra) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ang = Mode_V(&t), *a0 = Mode_V(&t), *a1 = Mode_V(&t), *ang2 = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *link = Mode_V(&t), *dummy = Mode_V(&t);
	byte ok = Mode_Import("Elevator") && Native_CallMod("Elevator", "ObjInitializator", NULL, "vv", elv, extra) &&
		LvN_SetI(elv, "UseAngSel", 0) && Mode_VGet(elv, "Ang", ang) && LvN_Item(ang, 0, a0);
	Native_Free(ang);
	ok = ok && Mode_VGet(elv, "Ang", ang) && LvN_Item(ang, 1, a1) && Mode_VGet(elv, "Name", name) &&
		Mode_VGet(elv, "NodePos_DC_ShipLink", link);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(ang2, NV_TUPLE);
	Native_Copy(Native_Append(ang2), a0);
	Native_Copy(Native_Append(ang2), a1);
	Native_SetEmpty(dummy, NV_TUPLE);
	Native_Copy(Native_Append(dummy), name);
	Native_Copy(Native_Append(dummy), link);
	Native_Copy(Native_Append(dummy), ang2);
	Native_SetInt(Native_Append(dummy), 1);
	return Mode_Done(&t, Mode_Put(SE, "editShipPosDummy", NULL, dummy) && Mode_Put(SE, "editPlatform", NULL, elv));
}

/* ---- the turning ship ---- */

/* EndMoveEngine(EntityName): the ship turned (RotateShip); on the Engines tab the engine groups of the model show */
static byte Se_EndMoveEngine(const nativeVal_t *entityName) {
	const int id = 0;
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *layouts = Mode_V(&t), *model = Mode_V(&t), *layout = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *slots = Mode_V(&t), *s = Mode_V(&t), *key = Mode_V(&t), *text = Mode_V(&t);
	nativeVal_t *k = Mode_V(&t);
	if (!Mode_Import("EnginesTab")) {
		return Mode_Done(&t, 0);
	}
	int c = LvN_GEqI(SE, "isRotating", 0);
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	if (!Mode_EntitySeam(entityName, e) || !LvN_SetI(e, "MoveTime", 0)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(e);
	c = Se_G("prevTab", e) ? LvN_EqS(e, "ShipEditTabEngines") : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	byte ok = Mode_Import(HT) && Se_G("shipsEnginesLayouts", layouts) && Se_ShipGet("Model", model) &&
		Mode_Sub(layouts, model, layout);
	int n = ok ? Mode_Len(layout) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_Free(model);
		Mode_Repr(Mode_IntVal(k, i + 1), r);
		ok = Se_ShipGet("Model", model) && Sn_Cat("", model, "EngineGroup", s) && Native_Arith('+', s, r, name);
		if (ok && i == 0) {
			char var[0x10];
			Str_Printf(var, sizeof(var), "nEng%d", i);
			ok = Native_CallMod("EnginesTab", "SelectEngineGroup", NULL, "ivi", (long)id, name, 0L) && Se_ShipGet(var, slots);
			int none = ok ? LvN_EqI(slots, 0) : -1;
			if (none > 0) {
				ok = Sn_SetUse(id, name, "Hint", "HINT_Engines6", SE);
			} else if (none == 0) {
				Native_Free(r);
				Native_Free(s);
				ok = Mode_Arith('+', Mode_IntVal(k, 6), slots, s);
				if (ok) {
					Mode_Repr(s, r);
					ok = Sn_Cat("HINT_Engines", r, NULL, key) && Mode_LangV(key, text) && Sn_Set(id, name, "Hint", text);
				}
			}
		} else if (ok) {
			ok = Sn_SetUse(id, name, "Hint", "HINT_Engines6", SE);
		}
		ok = ok && Sn_Set(id, name, "Visible", Mode_IntVal(k, 1));
		Native_Free(r);
		Native_Free(s);
		Native_Free(name);
		Native_Free(slots);
		Native_Free(key);
		Native_Free(text);
	}
	if (ok) {
		Native_SetInt(k, id);
		ok = Mode_LaterIn(0.15, "EnginesTab", "UpdateLayouts", k, 1, NULL);
	}
	return Mode_Done(&t, ok);
}

/* StopRotateLoop(): the ship stops turning */
static byte Se_StopRotate(void) {
	nativeVal_t z;
	return Mode_GOpI(SE, "ctnRotating", '+', 1) && Mode_Import(HT) && Se_ShipSetI("MoveTime", 0) &&
		Mode_Put(SE, "isRotating", NULL, Mode_IntVal(&z, 0));
}

/* RotateShip(Angle): the ship turns to Angle in half a second (EndMoveEngine at the end) */
static byte Se_RotateShip(const nativeVal_t *angle) {
	nativeVal_t ship = { 0 }, pos = { 0 }, dummy = { 0 }, ang = { 0 }, v;
	if (!Mode_Import(HT) || !Se_StopRotate()) {
		return 0;
	}
	int c = Se_HasShip();
	if (c <= 0) {
		return c == 0;
	}
	byte ok = Se_Ship(&ship) && Se_ShipGet("Pos", &pos) && Mode_VSet(&ship, "MovePos", &pos) && Se_G("editShipPosDummy", &dummy) &&
		Sn_FrontAng(&dummy, angle, &ang) && Se_ShipSet("MoveAng", &ang);
	Native_SetFloat(&v, 0.5);
	ok = ok && Se_ShipSet("MoveTime", &v) && Se_ShipSetI("MoveFade", 3) && Se_ShipSetS("OnEndMove", "ShipEdit.EndMoveEngine");
	Native_Free(&ship);
	Native_Free(&pos);
	Native_Free(&dummy);
	Native_Free(&ang);
	return ok;
}

/* LoopRotateShip(Rot): one turn of 170 degrees in 10 s, again 9 s later while the loop Rot runs */
static byte Se_LoopRotate(const nativeVal_t *rot) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cnt = Mode_V(&t), *pos = Mode_V(&t), *dummy = Mode_V(&t), *d2 = Mode_V(&t), *a0 = Mode_V(&t);
	nativeVal_t *ang = Mode_V(&t), *a1 = Mode_V(&t), *y = Mode_V(&t), *k = Mode_V(&t), *v = Mode_V(&t), *z = Mode_V(&t);
	if (!Mode_Import(HT)) {
		return Mode_Done(&t, 0);
	}
	int c = Se_G("ctnRotating", cnt) ? Mode_Cmp(cnt, "==", rot) : -1;
	if (c > 0) {
		c = Se_HasShip();
	}
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	byte ok = Se_ShipGet("Pos", pos) && Se_ShipSet("MovePos", pos) && Se_G("editShipPosDummy", dummy) && LvN_Item(dummy, 2, d2) &&
		LvN_Item(d2, 0, a0) && Se_ShipGet("Ang", ang) && LvN_Item(ang, 1, a1) && Mode_Arith('+', a1, Mode_IntVal(k, 170), y);
	if (ok) {
		Native_SetInt(z, 0);
		Mode_Tuple3(v, a0, y, z);
		ok = Se_ShipSet("MoveAng", v);
	}
	Native_Free(v);
	Native_SetFloat(v, 10.0);
	ok = ok && Se_ShipSet("MoveTime", v) && Se_ShipSetI("MoveFade", 0) && Se_ShipSetS("OnEndMove", "");
	Native_Free(cnt);
	return Mode_Done(&t, ok && Se_G("ctnRotating", cnt) && Mode_Later(9.0, SE, "LoopRotateShip", cnt, 1, NULL));
}

/* StartRotateLoop(): the ship starts turning (a new loop, the old one ends) */
static byte Se_StartRotate(void) {
	nativeVal_t v = { 0 };
	byte ok = Mode_Put(SE, "isRotating", NULL, Mode_IntVal(&v, 1)) && Mode_GOpI(SE, "ctnRotating", '+', 1) && Se_G("ctnRotating", &v) &&
		Se_LoopRotate(&v);
	Native_Free(&v);
	return ok;
}

/* ---- the panels ---- */

/* DrawBackground(id, elem, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32) of ShipEdit
 * (the sprite BackgroundElement<elem>) and of HangarTab (the sprite Background, no elem) */
static PyObject *Se_DrawBackground(PyObject *args, PyObject *kw, byte elem) {
	static char *names[] = { "id", "elem", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static char *names2[] = { "id", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static const char *const vars[] = { "X", "Y", "ScaleX", "ScaleY", "Unit", "Alpha", "Red", "Green", "Blue" };
	static const long defs[] = { 64, 64, 512, 352, 1, 92, 0, 0, 32 };
	PyObject *oid, *e = NULL, *v[8] = { NULL };
	byte ok = elem ? PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", names, &oid, &e, &v[0], &v[1], &v[2], &v[3],
		&v[4], &v[5], &v[6], &v[7]) : PyArg_ParseTupleAndKeywords(args, kw, "O|OOOOOOOO", names2, &oid, &v[0], &v[1], &v[2],
		&v[3], &v[4], &v[5], &v[6], &v[7]);
	if (!ok) {
		return NULL;
	}
	nativeVal_t id = { 0 }, name = { 0 }, x = { 0 }, val = { 0 };
	Native_Arg(oid, &id);
	if (elem) {
		Native_Arg(e, &x);
		ok = Sn_Cat("BackgroundElement", &x, NULL, &name);
	} else {
		Native_SetStr(&name, "Background");
	}
	ok = ok && Mode_ScorerAdd(&id, &name, "Sprite");
	for (int i = 0; ok && i < 9; i++) {
		PyObject *o = i == 4 ? NULL : v[i < 4 ? i : i - 1];
		Native_Free(&val);
		if (o) {
			Native_Arg(o, &val);
		} else {
			Native_SetInt(&val, defs[i]);
		}
		ok = Mode_ScorerSet(&id, &name, vars[i], &val);
	}
	Native_Free(&id);
	Native_Free(&name);
	Native_Free(&x);
	Native_Free(&val);
	return Mode_Result(ok);
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
static byte Se_AddShipEditTabs(int id, const nativeVal_t *tabs) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *startX = Mode_V(&t), *quad = Mode_V(&t), *tabSize = Mode_V(&t), *tab = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *key = Mode_V(&t), *text = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t);
	nativeVal_t *init = Mode_V(&t), *end = Mode_V(&t), *first = Mode_V(&t), *only = Mode_V(&t), *width = Mode_V(&t);
	nativeVal_t *px = Mode_V(&t), *x2 = Mode_V(&t), *k = Mode_V(&t), *prev = Mode_V(&t), *t0 = Mode_V(&t);
	if (!Mode_Import("Menu") || !Se_G("offsetX", startX) || !Se_G("TabQuadSize", quad) ||
		!Mode_Arith('*', quad, Mode_IntVal(k, 18), tabSize)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(l, tabs);
	int count = Mode_Loop(l);
	byte ok = count >= 0;
	long counter = 1;
	for (int i = 0; ok && i < count; i++) {
		ok = Native_SeqItem(l, i, tab) && Sn_Cat("ShipEditTab", tab, NULL, name);
		counter++;
		ok = ok && Sn_Cat("ShipEdit_", tab, "TabName", key) && Mode_LangV(key, text);
		Native_Free(quad);
		ok = ok && Mode_Arith('*', Mode_IntVal(k, counter - 2), tabSize, a) && Se_G("TabQuadSize", quad) && Mode_Arith('/', a, quad, init);
		Native_Free(a);
		Native_Free(quad);
		ok = ok && Mode_Arith('*', Mode_IntVal(k, counter - 1), tabSize, a) && Se_G("TabQuadSize", quad) && Mode_Arith('/', a, quad, end);
		int n = ok ? Native_Size(tabs) : -1;
		Native_SetInt(first, counter == 2);
		Native_SetInt(only, n == 1);
		PyObject *vars = n >= 0 ? Sn_Vars(8, "name", name, "tab", tab, "text", text, "x", startX, "init", init, "end", end,
			"first", first, "only", only) : NULL;
		ok = vars && Sn_Def(id, "tab", vars, SE) && Mode_Arith('/', tabSize, Mode_IntVal(k, 2), width) &&
			Mode_Arith('+', startX, width, x2);
		Py_XDECREF(vars);
		if (ok) {
			Native_SetEmpty(px, NV_TUPLE);
			Native_Copy(Native_Append(px), startX);
			Native_Copy(Native_Append(px), x2);
			ok = Se_SetAt("tabPixelX", name, px) && Mode_Arith('+', x2, Mode_IntVal(k, 1), b);
		}
		if (ok) {
			Native_Free(startX);
			*startX = *b;
			Native_SetNone(b);
		}
		Native_Free(tab);
		Native_Free(name);
		Native_Free(key);
		Native_Free(text);
		Native_Free(a);
		Native_Free(init);
		Native_Free(end);
		Native_Free(width);
		Native_Free(x2);
		Native_Free(px);
	}
	int c = ok && Se_G("prevTab", prev) ? Mode_Cmp(prev, "==", &se_none) : -1;
	if (c > 0) {
		Native_Free(prev);
		ok = LvN_Item(tabs, 0, t0) && Sn_Cat("ShipEditTab", t0, NULL, name) && Mode_Put(SE, "prevTab", NULL, name) &&
			Se_G("prevTab", prev) && Sn_Set(id, prev, "TextColorBlue", Mode_IntVal(k, 0));
	}
	return Mode_Done(&t, c >= 0 && ok);
}

/* AddDefaultPanelContent(id, ntabs): the dark frame around the panels, open where the ntabs tabs are */
static byte Se_AddDefaultPanelContent(int id, const nativeVal_t *ntabs) {
	/* offsetX + (TabQuadSize*18*ntabs/2) and (TabQuadSize*18*(4-ntabs)/2 + TabQuadSize*6/2) - MarginSize */
	modeVals_t t = { .n = 0 };
	nativeVal_t *k = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *x2 = Mode_V(&t), *c = Mode_V(&t), *w2 = Mode_V(&t);
	byte ok = Mode_Arith('*', Mode_IntVal(k, 252), ntabs, a) && Mode_Arith('/', a, Mode_IntVal(c, 2), b) && Mode_Arith('+', Mode_IntVal(k, 47), b, x2);
	Native_Free(a);
	Native_Free(b);
	ok = ok && Mode_Arith('-', Mode_IntVal(k, 4), ntabs, a) && Mode_Arith('*', Mode_IntVal(c, 252), a, b);
	Native_Free(a);
	ok = ok && Mode_Arith('/', b, Mode_IntVal(k, 2), a);
	Native_Free(b);
	ok = ok && Mode_Arith('+', a, Mode_IntVal(k, 42), b) && Mode_Arith('-', b, Mode_IntVal(c, 1), w2);
	PyObject *vars = ok ? Sn_Vars(2, "x2", x2, "w2", w2) : NULL;
	ok = vars && Sn_Def(id, "background", vars, SE);
	Py_XDECREF(vars);
	return Mode_Done(&t, ok);
}

/* ChangeTabLeft(id, control), ChangeTabRight(id, control): the tab beside the current one (not while the platform
 * changes the ship) */
static byte Se_ChangeTabSide(const nativeVal_t *id, const nativeVal_t *control, const char *side) {
	nativeVal_t plt = { 0 }, act = { 0 };
	int c = Se_G("editPlatform", &plt) ? Mode_Cmp(&plt, "!=", &se_none) : -1;
	if (c > 0) {
		c = Mode_VGet(&plt, "Action", &act) ? LvN_EqS(&act, "Change") : -1;
	}
	Native_Free(&plt);
	Native_Free(&act);
	return c > 0 || (c == 0 && Native_CallGlobal(SE, "ChangeTab", NULL, "vvs", id, control, side));
}

/* the tab control in front (yellow title, the tab drawn as the panel) or behind */
static byte Se_TabFront(int id, const nativeVal_t *tab, byte front) {
	static const char *const vars[] = { "Type", "Alpha", "Red", "Green", "Blue" };
	static const long on[] = { 0, 1, 57, 139, 202 }, off[] = { 1, 192, 20, 60, 160 };
	static const long rgb[] = { 255, 234, 177 };
	static const char *const cols[] = { "Red", "Green", "Blue" };
	nativeVal_t v, name = { 0 };
	for (int i = 0; i < 3; i++) {
		if (front) {
			Native_SetInt(&v, rgb[i]);
		} else {
			Native_SetFloat(&v, rgb[i] / 1.5);
		}
		if (!Sn_Set(id, tab, cols[i], &v)) {
			return 0;
		}
	}
	byte ok = Sn_Cat("Tab_", tab, NULL, &name);
	for (int i = 0; ok && i < 5; i++) {
		ok = Sn_Set(id, &name, vars[i], Mode_IntVal(&v, front ? on[i] : off[i]));
	}
	Native_Free(&name);
	return ok;
}

/* math.fabs(a - shipAng) of the angles (shipAng NULL: the NameError of the unset local) */
static byte Se_Swept(double a, const nativeVal_t *shipAng, nativeVal_t *out) {
	if (!shipAng) {
		return Native_Raise(NE_NAME, "shipAng");
	}
	nativeVal_t x, d = { 0 };
	Native_SetFloat(&x, a);
	Native_Free(out);
	byte ok = Mode_Arith('-', &x, shipAng, &d) && Native_CallMod("math", "fabs", out, "v", &d);
	Native_Free(&d);
	return ok;
}

/* the hint of the tab: key % UseTxt, or the language text key */
static byte Se_TabHint(int id, const nativeVal_t *tab, const char *key, byte use) {
	if (use) {
		return Sn_SetUse(id, tab, "Hint", key, SE);
	}
	nativeVal_t text = { 0 };
	Mode_Lang(key, &text);
	byte ok = Sn_Set(id, tab, "Hint", &text);
	Native_Free(&text);
	return ok;
}

/* Menu.LinkUD(id, tab, each item of the list) */
static byte Se_LinksUD(int id, const char *tab, const char *const *items) {
	for (; *items; items++) {
		if (!Sn_Link("LinkUD", id, tab, *items)) {
			return 0;
		}
	}
	return 1;
}

/* ChangeTab(id, control, direction=None): the tab control (or the one at direction of the current tab) comes to the
 * front: the panel of the old tab hides, the new one shows, the ship turns to the side the panel edits */
static byte Se_ChangeTab(int id, const nativeVal_t *control0, const nativeVal_t *direction) {
	static const char *const mods[] = { HT, "EnginesTab", "WeaponsTab", "HullTab", "Sound", "math" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *control = Mode_V(&t), *prev = Mode_V(&t), *p = Mode_V(&t), *ang = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *m = Mode_V(&t), *shipAng = Mode_V(&t), *swept = Mode_V(&t), *k = Mode_V(&t), *model = Mode_V(&t);
	nativeVal_t *grp = Mode_V(&t), *tab = Mode_V(&t);
	byte hasAng = 0;
	if (!Sn_From("LinkUD") || !Sn_From("UnLinkUD")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Copy(control, control0);
	if (Native_True(direction)) {
		/* SScorer.Get(id, prevTab, direction) by name: the variable of the tab is the tab beside it */
		Native_Free(control);
		if (!Se_G("prevTab", p) || !Native_CallMod("SScorer", "Get", control, "ivv", (long)id, p, direction)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(p);
	}
	int c = Se_G("prevTab", p) ? Mode_Cmp(control, "==", p) : -1;
	Native_Free(p);
	if (c != 0) {
		return Mode_Done(&t, c > 0);
	}
	if (!Se_G("prevTab", prev) || !Se_TabFront(id, prev, 0) || !Se_TabFront(id, control, 1)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (LvN_EqS(prev, "ShipEditTabHangar")) {
		ok = Se_TabHint(id, prev, "HINT_Tabs5", 1) && Native_CallMod(HT, "HideHangarPanel", NULL, "i", (long)id) &&
			Sn_Link("LinkUD", id, "ShipEditTabHangar", "");
	} else if (LvN_EqS(prev, "ShipEditTabEngines")) {
		ok = Se_TabHint(id, prev, "HINT_Tabs6", 1);
		c = ok ? Mode_Cmp(control, "!=", prev) : -1;
		ok = c == 0 || (c > 0 && Native_CallMod("EnginesTab", "HideEnginesPanel", NULL, "i", (long)id) && Se_RotateShip(Mode_IntVal(k, 0)));
		ok = ok && Sn_Link("LinkUD", id, "ShipEditTabEngines", "");
	} else if (LvN_EqS(prev, "ShipEditTabWeapons")) {
		ok = Se_TabHint(id, prev, "HINT_Tabs7", 1);
		c = ok ? Mode_Cmp(control, "!=", prev) : -1;
		ok = c == 0 || (c > 0 && Native_CallMod("WeaponsTab", "HideWeaponsPanel", NULL, "i", (long)id));
		ok = ok && Sn_Link("LinkUD", id, "ShipEditTabWeapons", "");
	} else if (LvN_EqS(prev, "ShipEditTabHull")) {
		ok = Se_TabHint(id, prev, "HINT_Tabs8", 1) && Native_CallMod("HullTab", "HideHullPanel", NULL, "i", (long)id) &&
			Sn_Link("LinkUD", id, "ShipEditTabHull", "");
	}
	Native_Free(p);
	ok = ok && Se_G("prevTab", p) && Sn_Set(id, p, "TextColorBlue", Mode_IntVal(k, 255));
	Native_Free(p);
	ok = ok && Se_G("prevTab", p) && Sn_Set(id, p, "SpriteIndex", Mode_IntVal(k, 18)) && Sn_Set(id, control, "TextColorBlue", Mode_IntVal(k, 0)) &&
		Sn_Set(id, control, "SpriteIndex", Mode_IntVal(k, 41));
	Native_SetInt(swept, -1);
	int has = ok ? Mode_ModTrue(HT, "prevShip") : -1;
	if (has > 0) {
		ok = Se_ShipGet("Ang", ang) && LvN_Item(ang, 1, a) && Native_CallMod("math", "fmod", m, "vi", a, 360L) &&
			Native_CallMod("math", "fabs", shipAng, "v", m);
		hasAng = ok;
	}
	if (!ok || has < 0) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(control, "ShipEditTabHangar")) {
		static const char *const first[] = { "Sell", "Exit", NULL };
		static const char *const then[] = { "Hangar3", "Hangar2", "Hangar1", NULL };
		ok = Se_LinksUD(id, "ShipEditTabHangar", first) && Native_CallMod(HT, "ShowHangarPanel", NULL, "i", (long)id) &&
			Se_LinksUD(id, "ShipEditTabHangar", then) && Se_TabHint(id, control, "HINT_Tabs1", 0) &&
			Native_CallMod("SScorer", "SetDefault", NULL, "iv", (long)id, control) && Se_StartRotate();
	} else if (LvN_EqS(control, "ShipEditTabEngines")) {
		static const char *const links[] = { "Recycle", "Exit", "PrevEngine", NULL };
		c = Se_G("prevTab", p) ? LvN_EqS(p, "ShipEditTabHull") : -1;
		Native_Free(p);
		if (c > 0) {
			Native_SetFloat(swept, 90.0);
		} else {
			ok = c == 0 && Se_Swept(0.0, hasAng ? shipAng : NULL, swept);
		}
		Native_SetStr(tab, "ShipEditTabEngines");
		c = ok && Se_G("prevTab", p) ? Mode_Cmp(p, "!=", tab) : -1;
		ok = c == 0 || (c > 0 && Native_CallMod("EnginesTab", "ShowEnginesPanel", NULL, "i", (long)id) &&
			Se_RotateShip(Mode_IntVal(k, 180)));
		ok = ok && Se_TabHint(id, control, "HINT_Tabs2", 0) && Native_CallMod("SScorer", "SetDefault", NULL, "iv", (long)id, control) &&
			Se_LinksUD(id, "ShipEditTabEngines", links) && Se_ShipGet("Model", model) && Sn_Cat("", model, "EngineGroup1", grp) &&
			Sn_LinkV("LinkUD", id, tab, grp) && Sn_Link("LinkUD", id, "ShipEditTabEngines", "Engine1");
	} else if (LvN_EqS(control, "ShipEditTabWeapons")) {
		static const char *const weapons[] = { "Devastator", "Tesla", "ATPC", "Inferno", "Swarm", "Vulcan", NULL };
		ok = Se_Swept(180.0, hasAng ? shipAng : NULL, swept) && Native_CallMod("WeaponsTab", "ShowWeaponsPanel", NULL, "i", (long)id) &&
			Se_TabHint(id, control, "HINT_Tabs3", 0) && Native_CallMod("SScorer", "SetDefault", NULL, "iv", (long)id, control) &&
			Se_LinksUD(id, "ShipEditTabWeapons", weapons) && Se_RotateShip(Mode_IntVal(k, 0));
	} else if (LvN_EqS(control, "ShipEditTabHull")) {
		ok = Se_Swept(90.0, hasAng ? shipAng : NULL, swept) && Native_CallMod("HullTab", "ShowHullPanel", NULL, "i", (long)id) &&
			Se_TabHint(id, control, "HINT_Tabs4", 0) && Native_CallMod("SScorer", "SetDefault", NULL, "iv", (long)id, control) &&
			Se_RotateShip(Mode_IntVal(k, 270));
	}
	c = ok ? Mode_CmpI(swept, "!=", -1) : -1;
	if (c > 0) {
		c = Se_G("prevTab", p) ? Mode_Cmp(p, "!=", control) : -1;
	}
	if (c > 0) {
		int fast = Mode_CmpI(swept, "<=", 91);
		c = fast >= 0 && Mode_SoundOf(fast > 0 ? "RotateShipFast" : "RotateShip") ? 1 : -1;
	}
	return Mode_Done(&t, c >= 0 && Mode_Put(SE, "prevTab", NULL, control));
}

/* ---- the ships of the hangars ---- */

/* CleanEditedShips(): the ships of the hangars are deleted (all but the one on the platform), editedShips is ten
 * Nones */
static byte Se_Clean(void) {
	nativeVal_t l = { 0 }, e = { 0 }, s = { 0 }, msg;
	if (!Mode_Import(HT)) {
		return 0;
	}
	int n = Se_G("editedShips", &l) ? Mode_Len(&l) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_Free(&e);
		int c = Se_AtI("editedShips", i, &e) ? Native_True(&e) : -1;
		if (c > 0) {
			c = Se_Ship(&s) ? Mode_Cmp(&e, "!=", &s) : -1;
			Native_Free(&s);
		}
		if (c > 0) {
			Native_Free(&e);
			/* try: editedShips[i].Del() except: print "Ya borrada" (the error of the lookup too) */
			if (!Se_AtI("editedShips", i, &e) || !Mode_VDel(&e)) {
				PyErr_Clear();
				Native_SetStr(&msg, "Ya borrada");
				c = Mode_PrintV(&msg) ? 1 : -1;
			}
		}
		ok = c >= 0;
	}
	Native_Free(&l);
	Native_Free(&e);
	Native_SetEmpty(&l, NV_LIST);
	ok = ok && Mode_Put(SE, "editedShips", NULL, &l);
	for (int i = 0; ok && i < 10; i++) {
		ok = Mode_Append(SE, "editedShips", &se_none);
	}
	Native_Free(&l);
	return ok;
}

/* CreatePlayerShip(): the ship the player comes to the junkyard with (the hangar of Hangar.iHangarShip) */
static byte Se_CreatePlayerShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *d = Mode_V(&t), *x = Mode_V(&t), *sv = Mode_V(&t), *n = Mode_V(&t), *cur = Mode_V(&t), *cad = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t), *name = Mode_V(&t), *k = Mode_V(&t), *ang = Mode_V(&t), *e = Mode_V(&t);
	static const char *const mods[] = { HT, "Vehicles", "string" };
	if (!Se_Clean()) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Mode_SaveVar("Hangar.iHangarShip", sv);
	if (!Se_G("editShipPosDummy", d) || !LvN_Item(d, 1, x) || !Native_ToInt(sv, n) || !Mode_SetModVar(HT, "currentHangar", n) ||
		!LvN_ModGet(HT, "currentHangar", cur)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_CmpI(cur, ">=", 0);
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	if (LvN_EqI(cur, 9)) {
		Mode_SaveVar("Hangar.HangarShipAux", cad);
	} else {
		byte ok = Mode_Arith('+', cur, Mode_IntVal(k, 1), x);
		if (ok) {
			Mode_Repr(x, r);
			ok = Sn_Cat("Hangar.HangarShip", r, NULL, name);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveVar(Native_Text(name), cad);
	}
	Native_SetStr(name, "");
	c = Mode_Cmp(cad, "!=", name);
	if (c > 0) {
		Native_Free(d);
		Native_Free(cur);
		byte ok = LvN_ModGet(HT, "currentHangar", cur) && Native_CallGlobal(SE, "AssignShip", NULL, "vv", cad, cur) &&
			Se_G("editShipPosDummy", d) && Sn_FrontAng(d, NULL, ang) && Se_AtCur("editedShips", e) && Mode_VSet(e, "Ang", ang);
		Native_Free(e);
		ok = ok && Se_AtCur("editedShips", e) && LvN_SetI(e, "Active", 1);
		Native_Free(e);
		c = ok && Se_AtCur("editedShips", e) && Mode_SetModVar(HT, "prevShip", e) ? 1 : -1;
	}
	return Mode_Done(&t, c >= 0);
}

/* SetupForEdit(edShps): the ships are linked to the platform */
static byte Se_SetupForEdit(const nativeVal_t *ships) {
	nativeVal_t l, o = { 0 }, plt = { 0 }, name = { 0 }, s = { 0 }, link = { 0 }, v = { 0 }, z;
	if (!Mode_Import("Parking")) {
		return 0;
	}
	Native_Copy(&l, ships);
	int n = Mode_Loop(&l);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&l, i, &o);
		if (ok && Native_True(&o)) {
			ok = LvN_SetI(&o, "MoveTime", 0) && LvN_SetS(&o, "LinkModel", ",") && Se_G("editPlatform", &plt) &&
				Mode_VGet(&plt, "Name", &name) && Sn_Cat("", &name, ",", &s);
			Native_SetStr(&z, "DC_ShipLink");
			ok = ok && Native_Arith('+', &s, &z, &link) && Mode_VSet(&o, "LinkModel", &link);
			if (ok) {
				Mode_Ints3(&v, 0, 0, 0);
				ok = Mode_VSet(&o, "OffModPos", &v) && LvN_SetF(&o, "DeltaOffMul", 0.0) && LvN_SetF(&o, "OffMul", 1.0) &&
					LvN_SetI(&o, "ArrowCanUse", 0) && Native_CallMod("Parking", "SetShipRadius", NULL, "vi", &o, 1L);
			}
		}
		Native_Free(&o);
		Native_Free(&plt);
		Native_Free(&name);
		Native_Free(&s);
		Native_Free(&link);
		Native_Free(&v);
	}
	Native_Free(&l);
	return ok;
}

/* the text of a ship in a save variable: model<-engine0<-...<-engine3<-weapons<-hull<-upgrades ("" for None) */
static byte Se_ShipText(const nativeVal_t *ship, nativeVal_t *out) {
	static const char *const up[] = { "Upgrd00", "Upgrd03", "Upgrd02", "Upgrd04", "Upgrd05", "Upgrd01" };
	static const char *const parts[] = { "Model", "MEng0", "MEng1", "MEng2", "MEng3", "WeapList" };
	nativeVal_t upgrades = { 0 }, cad = { 0 }, v = { 0 }, r = { 0 }, a = { 0 };
	Native_SetNone(out);
	int c = Mode_Cmp(ship, "!=", &se_none);
	if (c <= 0) {
		Native_SetStr(out, "");
		return c == 0;
	}
	byte ok = 1;
	for (int i = 0; ok && i < 6; i++) {
		ok = Mode_VGet(ship, up[i], &v);
		if (ok) {
			Mode_Repr(&v, &r);
			if (i == 0) {
				upgrades = r;
				Native_SetNone(&r);
			} else {
				ok = Sn_Cat("", &upgrades, ",", &a) && Native_Arith('+', &a, &r, &cad);
				if (ok) {
					Native_Free(&upgrades);
					upgrades = cad;
					Native_SetNone(&cad);
				}
			}
		}
		Native_Free(&v);
		Native_Free(&r);
		Native_Free(&a);
	}
	for (int i = 0; ok && i < 6; i++) {
		ok = Mode_VGet(ship, parts[i], &v);
		if (ok && i == 0) {
			cad = v;
			Native_SetNone(&v);
		} else if (ok) {
			ok = Sn_Cat("", &cad, "<-", &a) && Native_Arith('+', &a, &v, &r);
			if (ok) {
				Native_Free(&cad);
				cad = r;
				Native_SetNone(&r);
			}
		}
		Native_Free(&v);
		Native_Free(&a);
	}
	ok = ok && Mode_VGet(ship, "MaxLife", &v);
	if (ok) {
		Mode_Repr(&v, &r);
		ok = Sn_Cat("", &cad, "<-", &a) && Native_Arith('+', &a, &r, out);
		Native_Free(&a);
		ok = ok && Sn_Cat("", out, "<-", &a);
		Native_Free(out);
		ok = ok && Native_Arith('+', &a, &upgrades, out);
	}
	Native_Free(&upgrades);
	Native_Free(&cad);
	Native_Free(&v);
	Native_Free(&r);
	Native_Free(&a);
	return ok;
}

/* SaveShipInfoInSaveVar(ship, hangar), SaveShipInfoInGVar(ship, hangar): the ship into Hangar.HangarShip<hangar+1>
 * (a save variable) or the engine variable HangarShip<hangar+1> */
static byte Se_SaveShip(const nativeVal_t *ship, const nativeVal_t *hangar, byte save) {
	nativeVal_t cad = { 0 }, n = { 0 }, r = { 0 }, name = { 0 }, k;
	byte ok = Se_ShipText(ship, &cad) && Mode_Arith('+', hangar, Mode_IntVal(&k, 1), &n);
	if (ok) {
		Mode_Repr(&n, &r);
		ok = Sn_Cat(save ? "Hangar.HangarShip" : "HangarShip", &r, NULL, &name) &&
			(save ? Mode_SetSaveVar(Native_Text(&name), &cad) : Native_GVarSet(Native_Text(&name), &cad));
	}
	Native_Free(&cad);
	Native_Free(&n);
	Native_Free(&r);
	Native_Free(&name);
	return ok;
}

/* CreateShipFromSaveVar(cad, pos, name=""): the ship of a save variable text at pos (None for an empty model), with
 * its weapon upgrades in single player */
static byte Se_CreateShipFromSaveVar(const nativeVal_t *cad, const nativeVal_t *pos, const nativeVal_t *name0,
	nativeVal_t *out) {
	static const char *const slot[] = { "00", "03", "02", "04", "05", "01" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *a = Mode_V(&t), *a0 = Mode_V(&t), *name = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *up = Mode_V(&t), *key = Mode_V(&t), *s = Mode_V(&t), *u = Mode_V(&t), *j = Mode_V(&t);
	nativeVal_t *e = Mode_V(&t);
	Native_SetNone(out);
	if (!Mode_Import("string") || !Mode_Import("Vehicles") || !Native_Split(cad, "<-", a) || !LvN_Item(a, 0, a0)) {
		return Mode_Done(&t, 0);
	}
	Native_SetStr(e, "");
	int c = Mode_Cmp(a0, "!=", e);
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	Native_Copy(name, LvN_EqS(name0, "") ? a0 : name0);
	byte ok = LvN_Item(pos, 0, x) && LvN_Item(pos, 1, y) && LvN_Item(pos, 2, z) && LvN_Create(name, x, y, z, "Car", out) &&
		LvN_SetI(out, "ActiveCMask", 0);
	nativeVal_t *vs = Mode_V(&t);
	nativeVal_t *p[9];
	for (int i = 0; i < 9; i++) {
		p[i] = Mode_V(&t);
	}
	ok = ok && Mode_VGet(out, "Name", p[0]) && LvN_Item(a, 0, p[1]) && LvN_Item(a, 6, v) && Native_ToInt(v, p[2]);
	for (int i = 1; ok && i <= 5; i++) {
		ok = LvN_Item(a, i, p[i + 2]);
	}
	ok = ok && LvN_Item(a, 0, s) && Native_Upper(s, u) && Sn_Cat("SHIP_", u, NULL, key) && Mode_LangV(key, p[8]) &&
		Native_CallMod("Vehicles", "SetProperties", NULL, "vvvsvvvvvvi", p[0], p[1], p[2], "P_None", p[3], p[4], p[5], p[6], p[7],
			p[8], 1L) && LvN_SetI(out, "Active", 0);
	int mp = ok ? Sn_Multi(1) : -1;
	if (mp == 0) {
		Native_Free(s);
		ok = LvN_Item(a, 7, s) && Native_Split(s, ",", up);
		for (int i = 0; ok && i < 6; i++) {
			Native_Free(vs);
			Native_Free(j);
			ok = LvN_Item(up, i, vs) && Native_ToInt(vs, j);
			int nz = ok ? Mode_CmpI(j, "!=", 0) : -1;
			if (nz > 0) {
				char var[0x10];
				Str_Printf(var, sizeof(var), "UAmmo%s", slot[i]);
				ok = LvN_SetI(out, var, 200);
				Str_Printf(var, sizeof(var), "Upgrd%s", slot[i]);
				ok = ok && Mode_VSet(out, var, j);
			}
		}
	}
	if (!ok || mp < 0) {
		Native_Free(out);
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, 1);
}

/* AssignShip(cad, i): the ship of hangar i from its text, facing the camera, hidden */
static byte Se_AssignShip(const nativeVal_t *cad, const nativeVal_t *i) {
	nativeVal_t d = { 0 }, pos = { 0 }, ship = { 0 }, l = { 0 }, e = { 0 }, ang = { 0 };
	byte ok = Mode_Import("Vehicles") && Mode_Import(HT) && Mode_Import("string") && Se_G("editShipPosDummy", &d) &&
		LvN_Item(&d, 1, &pos) && Native_CallGlobal(SE, "CreateShipFromSaveVar", &ship, "vv", cad, &pos) &&
		Se_SetAt("editedShips", i, &ship);
	Native_Free(&d);
	ok = ok && Se_G("editShipPosDummy", &d) && Sn_FrontAng(&d, NULL, &ang) && Se_G("editedShips", &l) && Mode_Sub(&l, i, &e) &&
		Mode_VSet(&e, "Ang", &ang);
	Native_Free(&l);
	Native_Free(&e);
	ok = ok && Se_G("editedShips", &l) && Mode_Sub(&l, i, &e) && LvN_SetI(&e, "Active", 0);
	Native_Free(&d);
	Native_Free(&pos);
	Native_Free(&ship);
	Native_Free(&l);
	Native_Free(&e);
	Native_Free(&ang);
	return ok;
}

/* Scrap.Set(var, Scrap.Def(var)) for the player's ship variables */
static byte Se_DefaultShip(byte name) {
	static const char *const vars[] = { "PlayerName", "PilotModel", "PlayerMaxLife", "PlayerModel", "Motor0Model",
		"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", NULL };
	for (const char *const *v = vars + (name ? 0 : 2); *v; v++) {
		if (!Mode_GVarSetDef(*v)) {
			return 0;
		}
	}
	return 1;
}

/* a default ship (the Def values of the engine) in hangar p, facing the camera, hidden; pilot the pilot model of
 * SetProperties (NULL: Scrap.Def("PilotModel")) */
static byte Se_MakeDefault(const nativeVal_t *p, const nativeVal_t *pilot) {
	static const char *const vars[] = { "PlayerModel", "PlayerMaxLife", NULL, "Motor0Model", "Motor1Model", "Motor2Model",
		"Motor3Model", "WeaponBayList" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *d = Mode_V(&t), *pos = Mode_V(&t), *model = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *ship = Mode_V(&t), *e = Mode_V(&t), *ename = Mode_V(&t), *ang = Mode_V(&t), *l = Mode_V(&t);
	nativeVal_t *v[8];
	for (int i = 0; i < 8; i++) {
		v[i] = Mode_V(&t);
	}
	byte ok = Se_G("editShipPosDummy", d) && LvN_Item(d, 1, pos) && Native_GVarDef("PlayerModel", model) && LvN_Item(pos, 0, x) &&
		LvN_Item(pos, 1, y) && LvN_Item(pos, 2, z) && LvN_Create(model, x, y, z, "Car", ship) && Se_SetAt("editedShips", p, ship) &&
		Se_G("editedShips", l) && Mode_Sub(l, p, e) && LvN_SetI(e, "ActiveCMask", 0);
	Native_Free(l);
	Native_Free(e);
	ok = ok && Se_G("editedShips", l) && Mode_Sub(l, p, e) && Mode_VGet(e, "Name", ename);
	for (int i = 0; ok && i < 8; i++) {
		if (vars[i]) {
			ok = Native_GVarDef(vars[i], v[i]);
		} else if (pilot) {
			Native_Copy(v[i], pilot);
		} else {
			ok = Native_GVarDef("PilotModel", v[i]);
		}
	}
	ok = ok && Native_CallMod("Vehicles", "SetProperties", NULL, "vvvvvvvvvsi", ename, v[0], v[1], v[2], v[3], v[4], v[5], v[6],
		v[7], "", 1L);
	Native_Free(d);
	Native_Free(l);
	Native_Free(e);
	ok = ok && Se_G("editShipPosDummy", d) && Sn_FrontAng(d, NULL, ang) && Se_G("editedShips", l) && Mode_Sub(l, p, e) &&
		Mode_VSet(e, "Ang", ang);
	Native_Free(l);
	Native_Free(e);
	return Mode_Done(&t, ok && Se_G("editedShips", l) && Mode_Sub(l, p, e) && LvN_SetI(e, "Active", 0));
}

/* "HangarShip" (prefix) + `i + 1` */
static byte Se_HangarName(const char *prefix, long i, nativeVal_t *out) {
	nativeVal_t r = { 0 }, k;
	Mode_Repr(Mode_IntVal(&k, i + 1), &r);
	byte ok = Sn_Cat(prefix, &r, NULL, out);
	Native_Free(&r);
	return ok;
}

/* the camera of the editor: it flies from the old camera to the platform */
static byte Se_Camera(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *name = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *c = Mode_V(&t);
	nativeVal_t *last = Mode_V(&t), *e = Mode_V(&t), *v = Mode_V(&t), *pitch = Mode_V(&t), *d = Mode_V(&t), *d2 = Mode_V(&t);
	nativeVal_t *a1 = Mode_V(&t), *ay = Mode_V(&t), *camAng = Mode_V(&t), *dist = Mode_V(&t), *neg = Mode_V(&t);
	nativeVal_t *back = Mode_V(&t), *rot = Mode_V(&t), *pos = Mode_V(&t), *p1 = Mode_V(&t), *k = Mode_V(&t);
	Native_SetStr(name, "EditCamera");
	Native_SetFloat(x, -2431.14);
	Native_SetFloat(y, -1913.90);
	Native_SetFloat(z, -1758.43);
	Mode_CamName(0, last);
	byte ok = Mode_Put(SE, "lastCam", NULL, last) && LvN_Create(name, x, y, z, "CamPath", cam) && Mode_Put(SE, "camera", NULL, cam);
	Native_Free(cam);
	ok = ok && Se_G("camera", cam) && Mode_VGet(cam, "Name", c) && Native_CallMod("Scrap", "SetCam", NULL, "iv", 0L, c);
	Native_Free(cam);
	ok = ok && Se_G("camera", cam) && LvN_SetI(cam, "nWayPoint", 2);
	Native_Free(last);
	int has = ok && Se_G("lastCam", last) && Mode_EntitySeam(last, e) ? Mode_Cmp(e, "!=", &se_none) : -1;
	if (has > 0) {
		ok = Mode_VGet(e, "camPos", v) && Mode_VSet(cam, "Pos00", v);
		Native_Free(v);
		ok = ok && Mode_VGet(e, "camAng", v) && Mode_VSet(cam, "Ang00", v);
	} else if (has == 0) {
		Mode_Ints3(v, 0, 0, 0);
		ok = Mode_VSet(cam, "Pos00", v) && Mode_VSet(cam, "Ang00", v);
	} else {
		ok = 0;
	}
	ok = ok && Se_G("EditCameraPitch", pitch) && Se_G("editShipPosDummy", d) && LvN_Item(d, 2, d2) && LvN_Item(d2, 1, a1) &&
		Mode_Arith('-', a1, Mode_IntVal(k, 180), ay);
	if (ok) {
		Native_SetInt(k, 0);
		Mode_Tuple3(camAng, pitch, ay, k);
	}
	ok = ok && Se_G("CameraDist", dist) && Mode_Neg(dist, neg);
	if (ok) {
		Native_SetInt(k, 0);
		Mode_Tuple3(back, k, k, neg);
	}
	Native_Free(d);
	ok = ok && Se_G("editShipPosDummy", d) && LvN_Item(d, 1, p1) && Mode_Rotate(back, camAng, rot) && Mode_VecAdd(p1, rot, pos) &&
		Mode_VSet(cam, "Pos01", pos) && Mode_VSet(cam, "Ang01", camAng) && LvN_SetI(cam, "FOV01", 70) &&
		LvN_SetI(cam, "Near01", 50) && LvN_SetI(cam, "Far01", 200000) && LvN_SetF(cam, "Quake01", 0.0);
	int mp = ok ? Sn_Multi(1) : -1;
	return Mode_Done(&t, mp == 0 || (mp > 0 && LvN_SetF(cam, "Time00", 0.0)));
}

/* SetupEdit(id): the camera flies to the platform; the ships of the hangars are made (from the engine variables in a
 * network game, with a default ship when there is none, else from the save variables) and linked to the platform */
static byte Se_SetupEdit(void) {
	static const char *const mods[] = { "string", "Vehicles", "ShipMaker", HT };
	modeVals_t t = { .n = 0 };
	nativeVal_t *k = Mode_V(&t), *ship = Mode_V(&t), *pilot = Mode_V(&t), *list = Mode_V(&t), *idx = Mode_V(&t);
	nativeVal_t *hangar = Mode_V(&t), *name = Mode_V(&t), *cad = Mode_V(&t), *empty = Mode_V(&t), *l = Mode_V(&t);
	nativeVal_t *s = Mode_V(&t), *cur = Mode_V(&t), *sv = Mode_V(&t), *n = Mode_V(&t), *pn = Mode_V(&t);
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_GVarSetI("VehicleEditingMode", 1) || !Se_Camera() || !Se_Clean()) {
		return Mode_Done(&t, 0);
	}
	Native_SetStr(empty, "");
	int mp = Sn_Multi(1);
	byte ok = mp >= 0;
	if (mp > 0) {
		int c = Se_HasShip();
		ok = c == 0 || (c > 0 && Se_Ship(ship) && Mode_VDel(ship) && Mode_SetModVar(HT, "prevShip", &se_none));
		ok = ok && Native_GVarGet("PilotModel", pilot) && LvN_ModGet("ShipMaker", "PilotsList", list) && Mode_Index(list, pilot, idx) &&
			Mode_SetModVar(HT, "currentPilot", idx) && Native_GVarGet("iHangarShip", hangar) &&
			Mode_SetModVar(HT, "currentHangar", hangar);
		for (int i = 0; ok && i < 9; i++) {
			Native_Free(name);
			Native_Free(cad);
			ok = Se_HangarName("HangarShip", i, name) && Native_GVarGet(Native_Text(name), cad);
			c = ok ? Mode_Cmp(cad, "!=", empty) : -1;
			ok = c == 0 || (c > 0 && Native_CallGlobal(SE, "AssignShip", NULL, "vi", cad, (long)i));
		}
		int def = ok && LvN_ModGet(HT, "currentHangar", cur) ? LvN_EqI(cur, -1) : -1;
		if (def > 0) {
			long p = 0;
			ok = Se_DefaultShip(1) && Se_G("editedShips", l);
			int cnt = ok ? Mode_Loop(l) : -1;
			ok = cnt >= 0;
			for (int i = 0; ok && i < cnt; i++) {
				Native_Free(s);
				ok = Native_SeqItem(l, i, s);
				if (ok && Mode_Cmp(s, "==", &se_none) != 0) {
					break;
				}
				p++;
			}
			if (p >= 9) {
				p = 0;
			}
			Native_SetStr(pn, "P_None");
			Native_Free(ship);
			ok = ok && Se_MakeDefault(Mode_IntVal(n, p), pn) && Mode_SetModVar(HT, "currentHangar", n) && Se_G("editedShips", list) &&
				Mode_Sub(list, n, ship) && Mode_SetModVar(HT, "prevShip", ship);
		} else if (def == 0) {
			Native_Free(ship);
			ok = Se_AtCur("editedShips", ship) && Mode_SetModVar(HT, "prevShip", ship);
		} else {
			ok = 0;
		}
	} else if (mp == 0) {
		Mode_SaveVar("Hangar.iHangarShip", sv);
		ok = Native_ToInt(sv, n) && Mode_SetModVar(HT, "currentHangar", n) && Se_Ship(ship) && LvN_ModGet(HT, "currentHangar", cur) &&
			Se_SetAt("editedShips", cur, ship);
		for (int i = 0; ok && i < 9; i++) {
			Native_Free(cur);
			int c = LvN_ModGet(HT, "currentHangar", cur) ? Mode_Cmp(Mode_IntVal(k, i), "!=", cur) : -1;
			if (c > 0) {
				Native_Free(name);
				Native_Free(cad);
				ok = Se_HangarName("Hangar.HangarShip", i, name);
				if (ok) {
					Mode_SaveVar(Native_Text(name), cad);
					c = Mode_Cmp(cad, "!=", empty);
					ok = c == 0 || (c > 0 && Native_CallGlobal(SE, "AssignShip", NULL, "vi", cad, (long)i));
				}
			}
			ok = ok && c >= 0;
		}
		if (ok) {
			Native_Free(cad);
			Mode_SaveVar("Hangar.HangarShipAux", cad);
			int c = Mode_Cmp(cad, "!=", empty);
			ok = c == 0 || (c > 0 && Native_CallGlobal(SE, "AssignShip", NULL, "vi", cad, 9L));
		}
	}
	Native_Free(l);
	return Mode_Done(&t, ok && Se_G("editedShips", l) && Native_CallGlobal(SE, "SetupForEdit", NULL, "v", l));
}

/* FreeEdit(): the camera of the editor goes, the old one comes back */
static byte Se_FreeEdit(void) {
	nativeVal_t cam = { 0 }, last = { 0 };
	int c = LvN_GTrue(SE, "camera");
	byte ok = c == 0 || (c > 0 && Se_G("camera", &cam) && Mode_VDel(&cam) && Mode_Put(SE, "camera", NULL, &se_none) &&
		Se_G("lastCam", &last) && Native_CallMod("Scrap", "SetCam", NULL, "iv", 0L, &last));
	Native_Free(&cam);
	Native_Free(&last);
	return ok;
}

/* the tab keys of the scorer: ShipEdit.ChangeTabRight and Left, or none */
static byte Se_NextPrev(int id, byte on) {
	return Native_CallMod("SScorer", "SetOnNext", NULL, "is", (long)id, on ? "ShipEdit.ChangeTabRight" : "") &&
		Native_CallMod("SScorer", "SetOnPrev", NULL, "is", (long)id, on ? "ShipEdit.ChangeTabLeft" : "");
}

/* EnterEditShipMenu(id, control): the editor opens (from the main menu it loads the ShipEdit level first) */
static byte Se_EnterEditShipMenu(int id) {
	static const char *const mods[] = { "Menu", HT, "EnginesTab", "WeaponsTab", "HullTab" };
	static const char *const tabNames[] = { "Hangar", "Engines", "Weapons", "Hull" };
	static const char *const links[] = { "Sell", "Exit", "Hangar2", "Hangar3", "Hangar1", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *plt = Mode_V(&t), *tabs = Mode_V(&t), *text = Mode_V(&t), *lost = Mode_V(&t);
	nativeVal_t *k = Mode_V(&t), *z = Mode_V(&t), *cur = Mode_V(&t), *hangar = Mode_V(&t), *x = Mode_V(&t), *r = Mode_V(&t);
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_GVarSetI("R_SceneBlurValue", 0)) {
		return Mode_Done(&t, 0);
	}
	int c = LvN_GTrue(SE, "editPlatform");
	if (c < 0 || (c > 0 && (!Se_G("editPlatform", plt) || !LvN_SetS(plt, "OnObjSlot", "")))) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_Put(SE, "prevTab", NULL, &se_none) || !Mode_Put(SE, "fromSputnik", NULL, Mode_IntVal(k, 0)) || !Mode_Import("Init")) {
		return Mode_Done(&t, 0);
	}
	c = Mode_ModTrue("Init", "inMainMenu");
	if (c != 0) {
		return Mode_Done(&t, c > 0 && Mode_Import("SaveGame") &&
			Native_CallMod("SaveGame", "QLoad", NULL, "s", "Levels/ShipEdit"));
	}
	c = Se_G("tabs", tabs) ? Mode_Cmp(tabs, "==", &se_none) : -1;
	if (c > 0) {
		Native_Free(tabs);
		Mode_Texts(tabs, tabNames, 4);
		c = Mode_Put(SE, "tabs", NULL, tabs) ? 1 : -1;
	}
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	c = Mode_ModTrue(HT, "prevShip");
	if (c < 0 || (c > 0 && !Se_ShipSetI("ActiveCMask", 0))) {
		return Mode_Done(&t, 0);
	}
	if (!Native_CallGlobal(SE, "SetupEdit", NULL, "i", (long)id)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Hangar.DestroyedShips", text);
	Native_Free(tabs);
	byte ok = Native_EvalText(SE, text, lost) && Mode_Put(SE, "destroyedShips", NULL, lost) &&
		Se_SetAt("destroyedShips", Mode_IntVal(k, 9), Mode_IntVal(z, 0)) && Native_CallMod("Menu", "StartNewMenu", NULL, "i", (long)id) &&
		Se_G("tabs", tabs) && Native_CallGlobal(SE, "AddShipEditTabs", NULL, "iv", (long)id, tabs) &&
		Native_CallGlobal(SE, "AddDefaultPanelContent", NULL, "ii", (long)id, 4L) &&
		Native_CallMod(HT, "SetHangarPanel", NULL, "i", (long)id) && Native_CallMod("EnginesTab", "SetEnginesPanel", NULL, "i", (long)id) &&
		Native_CallMod("WeaponsTab", "SetWeaponsPanel", NULL, "i", (long)id) && Native_CallMod("HullTab", "SetHullPanel", NULL, "i", (long)id) &&
		Native_CallMod("SScorer", "SetOnSpecialHint", NULL, "iss", (long)id, "ShipEdit.ShowSpecialHint", Script_Lang("HINT_NoMoney")) &&
		Se_LinksUD(id, "ShipEditTabHangar", links) && Native_CallMod(HT, "SetSellVisible", NULL, "ii", (long)id, 1L) &&
		Native_CallMod(HT, "SetSellActive", NULL, "ii", (long)id, 0L);
	Native_SetStr(hangar, "");
	c = ok && LvN_ModGet(HT, "currentHangar", cur) ? Mode_CmpI(cur, ">=", 0) : -1;
	if (c > 0) {
		Native_Free(cur);
		ok = LvN_ModGet(HT, "currentHangar", cur) && Mode_Arith('+', cur, Mode_IntVal(k, 1), x);
		if (ok) {
			Mode_Repr(x, r);
			ok = Sn_Cat("Hangar", r, NULL, hangar);
		}
	}
	Native_Free(cur);
	int aux = ok && c >= 0 && LvN_ModGet(HT, "currentHangar", cur) ? LvN_EqI(cur, 9) : -1;
	if (aux > 0) {
		Native_Free(tabs);
		ok = Native_CallMod(HT, "ShowShipInfo", NULL, "i", (long)id) && Se_NextPrev(id, 0) &&
			Native_CallMod("SScorer", "SetDefault", NULL, "is", (long)id, "HangarAux") && Mode_Import(SE) &&
			LvN_ModGet(SE, "tabs", tabs) && Sn_TabsOn(id, tabs, 0);
	} else if (aux == 0) {
		Native_SetStr(x, "");
		c = Mode_Cmp(hangar, "!=", x);
		if (c > 0) {
			static const char *const lr[][2] = { { "HangarAux", "Sell" }, { "Hangar3", "Sell" }, { "Hangar9", "Sell" },
				{ "Hangar6", "Sell" }, { "Sell", "Exit" } };
			ok = Mode_SetModVar(HT, "prevItem", hangar);
			for (int i = 0; ok && i < 5; i++) {
				ok = Sn_Link("LinkLR", id, lr[i][0], lr[i][1]);
			}
			Native_Free(tabs);
			ok = ok && Se_G("tabs", tabs) && Sn_TabsOn(id, tabs, 1) && Se_NextPrev(id, 1);
			Native_Free(tabs);
			ok = ok && Se_G("tabs", tabs) && Sn_TabRing(id, tabs, "LinkLR") && Native_CallMod(HT, "ShowShipInfo", NULL, "i", (long)id) &&
				Native_CallMod("SScorer", "SetDefault", NULL, "iv", (long)id, hangar);
		} else {
			ok = c == 0 && Native_CallMod("SScorer", "SetDefault", NULL, "is", (long)id, "Hangar1");
		}
	} else {
		ok = 0;
	}
	ok = ok && Native_CallMod(HT, "ShowHangarPanel", NULL, "i", (long)id) &&
		Native_CallMod("SScorer", "SetOnCancel", NULL, "is", (long)id, "ShipEdit.ExitEditShipMenu");
	c = ok ? Se_HasShip() : -1;
	return Mode_Done(&t, c == 0 || (c > 0 && Se_StartRotate()));
}

/* ExitEditShipMenu(id, control): the editor closes: Rusty and the maintenance robot step back, the ships are saved,
 * the camera comes back, the callback (or the multiplayer menu) follows */
static byte Se_ExitEditShipMenu(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "Menu", HT, "EnginesTab", "WeaponsTab", "HullTab", "SaveGame" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *rusty = Mode_V(&t), *plt = Mode_V(&t), *pp = Mode_V(&t), *rp = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t);
	nativeVal_t *c2 = Mode_V(&t), *at = Mode_V(&t), *vec = Mode_V(&t), *nv = Mode_V(&t), *x = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *k = Mode_V(&t), *robot = Mode_V(&t), *lost = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *d = Mode_V(&t), *ang = Mode_V(&t), *e = Mode_V(&t), *cb = Mode_V(&t);
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	int c = Se_Entity("Rusty_0", rusty) ? Se_RustyOn(rusty) : -1;
	if (c > 0) {
		byte ok = Se_Entity("DM_Door_EditShip_00", plt) && Mode_VGet(plt, "Pos", pp) && LvN_Item(pp, 0, a) &&
			Mode_VGet(rusty, "Pos", rp) && LvN_Item(rp, 1, b) && LvN_Item(pp, 2, c2);
		if (ok) {
			Mode_Tuple3(at, a, b, c2);
		}
		Native_Free(rp);
		ok = ok && Mode_VGet(rusty, "Pos", rp) && Mode_VecSub(rp, at, vec) && Mode_VecNorm(vec, nv);
		Native_Free(a);
		Native_Free(b);
		Native_Free(c2);
		Native_Free(rp);
		ok = ok && LvN_Item(nv, 0, a) && Mode_Arith('*', a, Mode_IntVal(k, 1000), x) && Mode_VGet(rusty, "Pos", rp) && LvN_Item(rp, 1, b) &&
			LvN_Item(nv, 2, c2) && Mode_Arith('*', c2, Mode_IntVal(k, 1000), z);
		if (ok) {
			Mode_Tuple3(pos, x, b, z);
		}
		c = ok && Mode_VSet(rusty, "Pos", pos) ? 1 : -1;
	}
	if (c < 0 || !Se_Entity("Maintenance_00", robot)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(robot)) {
		Mode_Ints3(pos, -908, -1940, 458);
		if (!LvN_SetI(robot, "Active", 0) || !Mode_VSet(robot, "Pos", pos) || !LvN_SetI(robot, "Active", 1)) {
			return Mode_Done(&t, 0);
		}
	}
	byte ok = Native_CallMod("SaveGame", "MarkWayPoint", NULL, "ss", "Levels/Junkyard", "DM_Player_Spawn_") &&
		Se_G("destroyedShips", lost);
	if (ok) {
		Mode_Repr(lost, rep);
		ok = Mode_SetSaveVar("Hangar.DestroyedShips", rep) && Mode_SetModVar(HT, "holograms", Mode_IntVal(k, 0)) &&
			Native_CallMod("SSound", "SetPS", NULL, "ssd", "EditHologramLoopSound", "Volume", 0.0) && Mode_StopAllSounds();
	}
	c = ok ? Se_HasShip() : -1;
	if (c > 0) {
		c = Se_StopRotate() && Se_G("editShipPosDummy", d) && Sn_FrontAng(d, NULL, ang) && Se_ShipSet("Ang", ang) &&
			Se_ShipSetI("ActiveCMask", 1) ? 1 : -1;
	}
	ok = c >= 0 && Native_CallMod(HT, "HideHangarPanel", NULL, "i", (long)id) &&
		Native_CallMod("EnginesTab", "HideEnginesPanel", NULL, "i", (long)id) &&
		Native_CallMod("WeaponsTab", "HideWeaponsPanel", NULL, "i", (long)id) &&
		Native_CallMod("HullTab", "HideHullPanel", NULL, "i", (long)id) && Native_CallGlobal(SE, "SaveShip", NULL, "i", (long)id) &&
		Native_CallGlobal(SE, "FreeEdit", NULL, "") && Mode_GVarSetI("VehicleEditingMode", 0) && Se_G("editPlatform", e) &&
		LvN_SetS(e, "OnObjSlot", "!Usr0AutoDoorSlot");
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	c = LvN_GTrue(SE, "EndShipEditCallBack");
	if (c > 0) {
		nativeVal_t ca[2];
		Native_SetInt(&ca[0], id);
		Native_Copy(&ca[1], control);
		ok = Se_G("EndShipEditCallBack", cb) && Mode_CallArgs(cb, NULL, ca, 2);
		Native_Free(&ca[1]);
	} else if (c == 0) {
		c = LvN_GTrue(SE, "isXbox");
		const char *menu = c > 0 ? "XboxMenu" : "PCMenu";
		ok = c >= 0 && Mode_Import(menu) && Native_CallMod(menu, "MultiPlayerMenu", NULL, "iv", (long)id, control);
	} else {
		ok = 0;
	}
	int off = ok ? Native_NetOff() : -1;
	return Mode_Done(&t, off > 0 || (off == 0 && Mode_Import("Ffa") && Native_CallMod("Ffa", "ModiTry", NULL, "")));
}

/* (s.MEng0 != "") or ... or (s.MEng3 != ""): the ship has an engine (1, 0, -1) */
static int Se_HasEngine(const nativeVal_t *s) {
	nativeVal_t v = { 0 }, e;
	Native_SetStr(&e, "");
	for (int i = 0; i < 4; i++) {
		char var[8];
		Str_Printf(var, sizeof(var), "MEng%d", i);
		Native_Free(&v);
		int c = Mode_VGet(s, var, &v) ? Mode_Cmp(&v, "!=", &e) : -1;
		if (c != 0) {
			Native_Free(&v);
			return c;
		}
	}
	Native_Free(&v);
	return 0;
}

/* SaveShip(id): in a network game the selected (or a valid, or a default) ship becomes the player's ship and the
 * hangars go to the engine variables; in single player to the save variables, an unpaid or destroyed ship goes */
static byte Se_SaveShipAll(int id) {
	static const char *const vars[][2] = { { "PlayerMaxLife", "MaxLife" }, { "PlayerModel", "Model" }, { "Motor0Model", "MEng0" },
		{ "Motor1Model", "MEng1" }, { "Motor2Model", "MEng2" }, { "Motor3Model", "MEng3" }, { "WeaponBayList", "WeapList" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *d = Mode_V(&t), *x = Mode_V(&t), *ship = Mode_V(&t), *s = Mode_V(&t), *l = Mode_V(&t), *k = Mode_V(&t);
	nativeVal_t *firstFree = Mode_V(&t), *pname = Mode_V(&t), *list = Mode_V(&t), *cur = Mode_V(&t), *pilot = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *kind = Mode_V(&t), *rep = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import(HT) || !Mode_Import("Vehicles") || !Mode_Import("ShipMaker") || !Se_G("editShipPosDummy", d) ||
		!LvN_Item(d, 1, x) || !Se_Ship(ship)) {
		return Mode_Done(&t, 0);
	}
	int setDefault = 1, notValid = 0;
	byte hasFree = 0;
	int mp = Sn_Multi(1);
	byte ok = mp >= 0;
	if (mp > 0) {
		int c = Se_HasShip();
		if (c > 0) {
			Native_Free(s);
			int has = Se_Ship(s) ? Se_HasEngine(s) : -1;
			if (has == 0) {
				notValid = 1;
			} else if (has > 0) {
				setDefault = 0;
			}
			ok = has >= 0;
		} else if (c == 0) {
			notValid = 1;
		} else {
			ok = 0;
		}
		if (ok && notValid) {
			int n = Se_G("editedShips", l) ? Mode_Len(l) : -1;
			ok = n >= 0;
			for (int i = 0; ok && i < n; i++) {
				Native_Free(s);
				c = Se_AtI("editedShips", i, s) ? Mode_Cmp(s, "!=", &se_none) : -1;
				if (c > 0) {
					Native_Free(s);
					int has = Se_AtI("editedShips", i, s) ? Se_HasEngine(s) : -1;
					if (has > 0) {
						setDefault = 0;
						Native_Free(ship);
						ok = Se_AtI("editedShips", i, ship) && Mode_SetModVar(HT, "currentHangar", Mode_IntVal(k, i));
						break;
					}
					ok = has == 0;
				} else if (c == 0 && !hasFree) {
					Native_SetInt(firstFree, i);
					hasFree = 1;
				} else {
					ok = c >= 0;
				}
			}
		}
		int xb = ok ? LvN_GTrue(SE, "isXbox") : -1;
		if (xb == 0) {
			Mode_ScorerGet(id, "PilotName", "Text", pname);
			ok = Mode_GVarSet("PlayerName", pname);
		}
		ok = ok && xb >= 0 && LvN_ModGet("ShipMaker", "PilotsList", list) && LvN_ModGet(HT, "currentPilot", cur) &&
			Mode_Sub(list, cur, pilot) && Mode_GVarSet("PilotModel", pilot);
		if (ok && setDefault) {
			ok = Se_DefaultShip(0) && (!hasFree || (Se_MakeDefault(firstFree, NULL) && Mode_SetModVar(HT, "currentHangar", firstFree)));
		}
		if (ok && !setDefault) {
			c = Mode_Cmp(ship, "!=", &se_none);
			for (int i = 0; ok && c > 0 && i < 7; i++) {
				Native_Free(v);
				ok = Mode_VGet(ship, vars[i][1], v) && Mode_GVarSet(vars[i][0], v);
			}
		}
		Native_Free(l);
		int n = ok && Se_G("editedShips", l) ? Mode_Len(l) : -1;
		ok = n >= 0;
		for (int i = 0; ok && i < n - 1; i++) {
			Native_Free(s);
			ok = Se_AtI("editedShips", i, s) && Se_SaveShip(s, Mode_IntVal(k, i), 0);
		}
		Native_Free(cur);
		ok = ok && LvN_ModGet(HT, "currentHangar", cur) && Mode_GVarSet("iHangarShip", cur) &&
			Native_CallMod("Scrap", "SaveConfig", NULL, "");
	} else if (mp == 0) {
		int n = Se_G("editedShips", l) ? Mode_Len(l) : -1;
		ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			Native_Free(s);
			ok = Se_AtI("editedShips", i, s) && Se_SaveShip(s, Mode_IntVal(k, i), 1);
		}
		int c = ok && Mode_Import(SE) && Se_AtCur("editedShips", e) ? Mode_Cmp(e, "==", &se_none) : -1;
		if (c == 0) {
			Native_Free(list);
			Native_Free(cur);
			c = LvN_ModGet(SE, "destroyedShips", list) && LvN_ModGet(HT, "currentHangar", cur) && Mode_Sub(list, cur, v) ?
				LvN_EqI(v, 1) : -1;
			if (c > 0) {
				int m = Sn_Multi(0);
				c = m < 0 ? -1 : !m;
			}
		}
		if (c > 0) {
			c = Se_HasShip();
		}
		if (c > 0) {
			Native_Free(list);
			Native_Free(cur);
			Native_SetStr(rep, "Rebuild");
			int built = LvN_ModGet(SE, "auxShipsToEditList", list) && LvN_ModGet(HT, "currentShipToEdit", cur) &&
				Mode_Sub(list, cur, kind) ? Mode_Cmp(kind, "!=", rep) : -1;
			Native_Free(s);
			Native_Free(cur);
			if (built > 0) {
				ok = Se_Ship(s) && Mode_VDel(s);
			} else if (built == 0) {
				ok = Se_AtCur("editedShips", s) && Mode_VDel(s) && LvN_ModGet(HT, "currentHangar", cur) &&
					Se_SetAt("editedShips", cur, &se_none);
			} else {
				ok = 0;
			}
			ok = ok && Mode_SetModVar(HT, "prevShip", &se_none) && Mode_SetModVar(HT, "currentHangar", Mode_IntVal(k, -1));
		} else {
			ok = c == 0;
		}
		Native_Free(cur);
		if (ok && LvN_ModGet(HT, "currentHangar", cur)) {
			Mode_Repr(cur, rep);
			ok = Mode_SetSaveVar("Hangar.iHangarShip", rep);
		} else {
			ok = 0;
		}
	}
	return Mode_Done(&t, ok);
}

/* ShowSpecialHint(): the special hint (no money) shows while the money prediction is red */
static byte Se_ShowSpecialHint(nativeVal_t *out) {
	nativeVal_t v = { 0 };
	Mode_ScorerGet(0, "MoneyPredict", "Green", &v);
	int c = LvN_EqI(&v, 0);
	Native_Free(&v);
	if (c) {
		Mode_ScorerGet(0, "MoneyPredict", "Visible", &v);
		c = LvN_EqI(&v, 1);
		Native_Free(&v);
	}
	Native_SetInt(out, c);
	return 1;
}

/* ---- adapters: the argument formats of the C port (the errors of PyArg), the objects as native values ---- */

MODE_FN(SE, IsEditingMultiplayer, 0, Se_IsEditingMultiplayer(out))
MODE_FN(SE, RustyOnPlatform, 1, Se_RustyOnPlatform(&a[0], out))
MODE_FN(SE, PointObjectTo, 2, Se_PointTo(&a[0], &a[1]))
MODE_FN(SE, LaunchPlatformAnim, 0, Se_LaunchPlatformAnim())
MODE_FN(SE, ChangeShip, 3, Se_ChangeShip(&a[0]))
MODE_FN(SE, VerifyChangeShip, 3, Se_VerifyChangeShip(&a[0]))
MODE_FN(SE, DoorInitializator, 1, Se_DoorInitializator(&a[0]))
MODE_FN(SE, ObjInitializator, 2, Se_ObjInitializator(&a[0], &a[1]))
MODE_FN(SE, EndMoveEngine, 1, Se_EndMoveEngine(&a[0]))
MODE_FN(SE, RotateShip, 1, Se_RotateShip(&a[0]))
MODE_FN(SE, LoopRotateShip, 1, Se_LoopRotate(&a[0]))
MODE_FN(SE, StartRotateLoop, 0, Se_StartRotate())
MODE_FN(SE, StopRotateLoop, 0, Se_StopRotate())
MODE_IFN(SE_AddShipEditTabs, 2, Se_AddShipEditTabs(id, &a[1]))
MODE_IFN(SE_AddDefaultPanelContent, 2, Se_AddDefaultPanelContent(id, &a[1]))
MODE_FN(SE, ChangeTabLeft, 2, Se_ChangeTabSide(&a[0], &a[1], "Left"))
MODE_FN(SE, ChangeTabRight, 2, Se_ChangeTabSide(&a[0], &a[1], "Right"))
MODE_FN(SE, CleanEditedShips, 0, Se_Clean())
MODE_FN(SE, CreatePlayerShip, 0, Se_CreatePlayerShip())
MODE_FN(SE, SetupForEdit, 1, Se_SetupForEdit(&a[0]))
MODE_FN(SE, SaveShipInfoInSaveVar, 2, Se_SaveShip(&a[0], &a[1], 1))
MODE_FN(SE, SaveShipInfoInGVar, 2, Se_SaveShip(&a[0], &a[1], 0))
MODE_FN(SE, AssignShip, 2, Se_AssignShip(&a[0], &a[1]))
MODE_FN(SE, SetupEdit, 1, Se_SetupEdit())
MODE_FN(SE, FreeEdit, 0, Se_FreeEdit())
MODE_IFN(SE_EnterEditShipMenu, 2, Se_EnterEditShipMenu(id))
MODE_IFN(SE_ExitEditShipMenu, 2, Se_ExitEditShipMenu(id, &a[1]))
MODE_IFN(SE_SaveShip, 1, Se_SaveShipAll(id))
MODE_FN(SE, ShowSpecialHint, 0, Se_ShowSpecialHint(out))

/* ChangeTab(id, control, direction=None) */
static byte SE_ChangeTab_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *control, *direction = NULL;
	nativeVal_t none = { 0 };
	int id;
	(void)out;
	return Mode_Parse(a, n, "iO|O", &id, &control, &direction) && Se_ChangeTab(id, control, direction ? direction : &none);
}

/* the adapter: the keywords and the errors of "iO|O", then the entry */
static PyObject *SE_ChangeTab(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "control", "direction", NULL };
	PyObject *control, *direction = NULL;
	nativeVal_t a[3], out;
	int id;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iO|O", names, &id, &control, &direction)) {
		return NULL;
	}
	Native_SetInt(&a[0], id);
	Native_Arg(control, &a[1]);
	if (direction) {
		Native_Arg(direction, &a[2]);
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, direction ? 3 : 2, SE_ChangeTab_E(a, direction ? 3 : 2, &out), &out);
}

/* CreateShipFromSaveVar(cad, pos, name="") */
static byte SE_CreateShipFromSaveVarK(const nativeVal_t *const *a, nativeVal_t *out) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	return Se_CreateShipFromSaveVar(a[0], a[1], a[2] ? a[2] : &empty, out);
}

static byte SE_CreateShipFromSaveVar_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "OO|O", SE_CreateShipFromSaveVarK, out);
}

static PyObject *SE_CreateShipFromSaveVar(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "cad", "pos", "name", NULL };
	(void)self;
	return Mode_KwCall(args, kw, "OO|O", names, SE_CreateShipFromSaveVarK);
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

PORTS(ShipEdit)
