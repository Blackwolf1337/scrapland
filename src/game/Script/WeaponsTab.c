/*
 * The WeaponsTab module in C (scripts/shipedit/weaponstab.py, issue #2): the weapons tab of the ship editor. The
 * player picks one of the six weapons, then loads or unloads it in the slots the ship has for its kind (bullets
 * nSlot00, energy nSlot02, missiles nSlot01), paid for with WeaponCosts, as long as the mass stays under the limit;
 * in single player one of its two upgrades. The layout is data/menus/weaponstab.def. controlsToLinkList, prevWeapon
 * (the weapon picked), the tables slotId, weapId, upgradeId, auxUpgradeId and auxUpgradeId2, the colours and UseTxt
 * stay variables of the module; the script builds the ship variables from the tables (Weap<slot><weapon>,
 * Upgrd<weapon>), so the C reads the tables too. ShipEdit, HangarTab, ShipMaker and Sound are used through their
 * modules.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values and set the scorer items in C; the
 * PyMethodDef entries are adapters. The module's variables are read where the script reads them (OBJLIB 5), the ship
 * being edited is HangarTab.prevShip as a handle value (OBJLIB 5 and 7), the tables of ShipMaker and the lists of
 * ShipEdit are read as their attributes. HangarTab, ShipEdit and ShipMaker are called through their refs (PORT_CALL:
 * directly while the attribute holds the port's function, else by name), the module's own functions through its
 * globals as the script calls them (PORT_OWN, the same seam on the global). The link functions of Menu stay by name
 * (OBJLIB 4: a from import, and they have no entry); the .def runner takes Python values (OBJLIB 6).
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static const char weaponstab_def[] = {
#embed "../../../data/menus/weaponstab.def"
	, 0 };

#define WEAPONSTAB_FILE "menus/weaponstab.def"
#define MOD "WeaponsTab"
#define WT_NAME 0x40

/* WeaponsTab.NormalColor*, SelectedColor*, NotAvailableColor* (red, green, blue) */
static const int wt_normal[3] = { 115, 150, 255 };
static const int wt_selected[3] = { 250, 230, 90 };
static const int wt_grey[3] = { 128, 128, 128 };
static const int wt_white[3] = { 255, 255, 255 };
static const int wt_missing[3] = { 8, 8, 8 };

/* the weapons in the order of ShowWeaponsPanel, of SelectWeapon's unlinks and of the list of FillSlots */
static const char *const wt_shown[6] = { "Vulcan", "Devastator", "Tesla", "ATPC", "Swarm", "Inferno" };
static const char *const wt_unlink[6] = { "Devastator", "Tesla", "ATPC", "Inferno", "Swarm", "Vulcan" };
static const char *const wt_linked[6] = { "Vulcan", "Devastator", "Tesla", "ATPC", "Inferno", "Swarm" };

static byte WT_WeaponHint(const char *key, byte use, const nativeVal_t *weapon, nativeVal_t *out);

/* the step weaponhint <key>: the Hint of a weapon button, string.replace(Scrap.GetLangStr(key) % UseTxt, "<n>",
 * Scrap.GetLangStr("ShipEdit_" + name + "Name")) (the error left set stops the file) */
static void WT_StepHint(int id, PyObject *args) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *key = Mode_V(&t), *hint = Mode_V(&t);
	const char *item = Script_CurrentItem(), *k;
	/* OBJLIB 6: the step takes the arguments of the line as a tuple */
	PyObject *o = Script_Index(args, 0);
	byte ok = o && Native_Arg(o, key) && Mode_ItemText(key, &k);
	Py_XDECREF(o);
	ok = ok && WT_WeaponHint(k, 1, Mode_S(&t, item), hint) && Mode_ScorerPut(id, item, "Hint", hint);
	Mode_Done(&t, ok);
}

static const scriptStep_t weaponstab_steps[] = {
	{ "weaponhint", WT_StepHint },
	{ NULL, NULL },
};

/* a section of weaponstab.def (vars: the $names, NULL for none): 0 with the error of the file */
static byte WT_Run(int id, const char *section, PyObject *vars) {
	return Script_RunFileTry(id, WEAPONSTAB_FILE, section, weaponstab_def, weaponstab_steps, vars);
}

/* a global of the module as its functions read it (out None before) */
static byte WT_G(const char *name, nativeVal_t *out) {
	return Mode_G(MOD, name, out);
}

/* the C name prefix + text (a text of the script: the TypeError of another value), cut to WT_NAME */
static byte WT_Cat(char *out, const char *prefix, const nativeVal_t *text, const char *suffix) {
	const char *s;
	if (!Mode_ItemText(text, &s)) {
		return 0;
	}
	Str_Printf(out, WT_NAME, "%s%s%s", prefix, s, suffix);
	return 1;
}

/* HangarTab.prevShip.<prefix + text> */
static byte WT_ShipS(const char *prefix, const nativeVal_t *text, nativeVal_t *out) {
	char var[WT_NAME];
	return WT_Cat(var, prefix, text, "") && Mode_ShipGet(var, out);
}

/* string.replace(text, "<n>", Scrap.GetLangStr(prefix + name + suffix)): a hint with a name in it */
static byte WT_Named(const nativeVal_t *text, const char *prefix, const nativeVal_t *name, const char *suffix,
	nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *key = Mode_V(&t), *n = Mode_V(&t);
	if (!Mode_Cat(&t, key, 3, Mode_S(&t, prefix), name, Mode_S(&t, suffix)) || !Mode_LangV(key, n)) {
		return Mode_Done(&t, 0);
	}
	Mode_Replace(text, "<n>", n, out);
	return Mode_Done(&t, 1);
}

/* the hint of a weapon (ShipEdit_<weapon>Name in it) from key, with UseTxt when use */
static byte WT_WeaponHint(const char *key, byte use, const nativeVal_t *weapon, nativeVal_t *out) {
	nativeVal_t text;
	if (use) {
		if (!Mode_LangUse(MOD, key, &text)) {
			return 0;
		}
	} else {
		Mode_Lang(key, &text);
	}
	byte ok = WT_Named(&text, "ShipEdit_", weapon, "Name", out);
	Native_Free(&text);
	return ok;
}

/* the hint of an upgrade: HINT_UpgradeName_<upgradeId[upgrade]> into key % UseTxt */
static byte WT_UpgradeHint(const char *key, const nativeVal_t *upgrade, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *text = Mode_V(&t), *ids = Mode_V(&t), *name = Mode_V(&t);
	byte ok = Mode_LangUse(MOD, key, text) && WT_G("upgradeId", ids) && Native_Subscript(ids, upgrade, name) &&
		WT_Named(text, "HINT_UpgradeName_", name, "", out);
	return Mode_Done(&t, ok);
}

/* a hint into the item and its MaxHintLineWidth */
static byte WT_SetHint(int id, const char *item, const nativeVal_t *hint) {
	if (!Mode_ScorerPut(id, item, "Hint", hint)) {
		return 0;
	}
	Script_SetNum(id, item, "MaxHintLineWidth", 550);
	return 1;
}

/* the hint of a weapon into the item, with its MaxHintLineWidth */
static byte WT_PutWeaponHint(int id, const char *item, const char *key, byte use, const nativeVal_t *weapon) {
	nativeVal_t hint = { 0 };
	byte ok = WT_WeaponHint(key, use, weapon, &hint) && WT_SetHint(id, item, &hint);
	Native_Free(&hint);
	return ok;
}

/* control + "Slot" + `i` */
static byte WT_Slot(char *out, const nativeVal_t *control, long i) {
	char suffix[0x20];
	Str_Printf(suffix, sizeof(suffix), "Slot%ld", i);
	return WT_Cat(out, "", control, suffix);
}

/* slotId[num] + weapId[weapon]: the name of a slot in the ship variables */
static byte WT_NameId(const nativeVal_t *num, const nativeVal_t *weapon, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ids = Mode_V(&t), *slot = Mode_V(&t), *wids = Mode_V(&t), *wid = Mode_V(&t);
	byte ok = WT_G("slotId", ids) && Native_Subscript(ids, num, slot) && WT_G("weapId", wids) &&
		Native_Subscript(wids, weapon, wid) && Native_Arith('+', slot, wid, out);
	return Mode_Done(&t, ok);
}

/* HangarTab.prevShip.Weap<slotId[num] + weapId[weapon]> */
static byte WT_Loaded(const nativeVal_t *num, const nativeVal_t *weapon, nativeVal_t *out) {
	nativeVal_t id = { 0 };
	byte ok = WT_NameId(num, weapon, &id) && WT_ShipS("Weap", &id, out);
	Native_Free(&id);
	return ok;
}

/* the same for slot i */
static byte WT_LoadedI(long i, const nativeVal_t *weapon, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetInt(&n, i);
	return WT_Loaded(&n, weapon, out);
}

/* the slots of the kind of weapon (0 for another name) */
static byte WT_NumSlots(const nativeVal_t *weapon, nativeVal_t *out) {
	static const char *const kinds[3][3] = { { "Vulcan", "Devastator", "nSlot00" }, { "Tesla", "ATPC", "nSlot02" },
		{ "Inferno", "Swarm", "nSlot01" } };
	for (int k = 0; k < 3; k++) {
		nativeVal_t a, b;
		Native_SetStr(&a, kinds[k][0]);
		Native_SetStr(&b, kinds[k][1]);
		if (!Native_Compare(weapon, &a) || !Native_Compare(weapon, &b)) {
			return Mode_ShipGet(kinds[k][2], out);
		}
		if (PyErr_Occurred()) {
			return 0;
		}
	}
	Native_SetInt(out, 0);
	return 1;
}

/* range(n) of the slots of the kind of weapon */
static byte WT_SlotCount(const nativeVal_t *weapon, long *count) {
	nativeVal_t n = { 0 };
	byte ok = WT_NumSlots(weapon, &n) && Mode_Range(&n, count);
	Native_Free(&n);
	return ok;
}

/* control[0:len(control) - cut] */
static byte WT_Head(const nativeVal_t *control, int cut, nativeVal_t *out) {
	int len = Native_Size(control);
	return len >= 0 && Native_Slice(control, 0, len - cut, out);
}

/* control[len(control) - back] as a copy */
static byte WT_Char(const nativeVal_t *control, int back, nativeVal_t *out) {
	nativeVal_t tmp;
	int len = Native_Size(control);
	const nativeVal_t *c = len >= 0 ? Native_GetItem(control, len - back, 1, &tmp) : NULL;
	if (!c) {
		return 0;
	}
	Native_Copy(out, c);
	if (c == &tmp) {
		Native_Free(&tmp);
	}
	return 1;
}

/* int(control[len(control) - 1]) */
static byte WT_Last(const nativeVal_t *control, nativeVal_t *out) {
	nativeVal_t c = { 0 };
	byte ok = WT_Char(control, 1, &c) && Native_ToInt(&c, out);
	Native_Free(&c);
	return ok;
}

/*
 * exec(code) in a function of the module that imported HangarTab: the lines "HangarTab.prevShip.<name> = <literal>"
 * and "HangarTab.prevShip.<name> = 1 - HangarTab.prevShip.<name>" made directly; a text in another form (made of
 * table values a test changed) goes to the exec line reader with its errors (OBJLIB 6)
 */
static byte WT_Exec(const nativeVal_t *code) {
	static const char ship[] = "HangarTab.prevShip.";
	const int sl = (int)sizeof(ship) - 1;
	if (!Mode_Import("HangarTab") || !Mode_Import(MOD)) {
		return 0;
	}
	const char *s = Native_Text(code);
	const char *eq = (int)strlen(s) == code->n && !strncmp(s, ship, sl) ? strstr(s, " = ") : NULL;
	char attr[WT_NAME];
	int len = eq ? (int)(eq - s) - sl : 0;
	if (eq && len < (int)sizeof(attr) && Script_IsName(s + sl, len)) {
		const char *rhs = eq + 3;
		nativeVal_t v = { 0 }, cur = { 0 }, one;
		byte ok = 1, taken = 1;
		memcpy(attr, s + sl, len);
		attr[len] = '\0';
		if (!strncmp(rhs, "1 - ", 4) && !strncmp(rhs + 4, ship, sl) && !strcmp(rhs + 4 + sl, attr)) {
			Native_SetInt(&one, 1);
			ok = Mode_ShipGet(attr, &cur) && Native_Arith('-', &one, &cur, &v);
		} else {
			taken = Native_Literal(rhs, &v);
		}
		if (taken) {
			ok = ok && Mode_ShipSet(attr, &v);
			Native_Free(&v);
			Native_Free(&cur);
			return ok;
		}
	}
	scriptFrame_t f = { .n = 0 };
	PyObject *hangar = Sf_Keep(&f, PyImport_ImportModule("HangarTab"));
	PyObject *text = Sf_Keep(&f, hangar ? PyString_FromStringAndSize(s, code->n) : NULL);
	Sf_Exec(&f, text, MOD, Sf_Build(&f, "{s:O}", "HangarTab", hangar));
	return Sf_Done(&f, NULL), !f.failed;
}

/* the exec text prefix + a + middle + b of the script (a and b texts or other values, as + takes them) */
static byte WT_ExecCat(const char *prefix, const nativeVal_t *a, const char *middle, const nativeVal_t *b) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *code = Mode_V(&t);
	byte ok = b ? Mode_Cat(&t, code, 4, Mode_S(&t, prefix), a, Mode_S(&t, middle), b) :
		Mode_Cat(&t, code, 3, Mode_S(&t, prefix), a, Mode_S(&t, middle));
	return Mode_Done(&t, ok && WT_Exec(code));
}

/* ShipEdit.IsEditingMultiplayer() into *out */
static byte WT_Multi(nativeVal_t *out) {
	return PORT_CALL(ShipEdit, IsEditingMultiplayer, out, "");
}

/* ShipEdit.IsEditingMultiplayer() != 1 (the single player money): 1 or 0, -1 with the error */
static int WT_Single(void) {
	nativeVal_t v = { 0 };
	int r = WT_Multi(&v) ? Mode_CmpOpI(&v, 1, '!') : -1;
	Native_Free(&v);
	return r;
}

/* the truth of ShipEdit.IsEditingMultiplayer(): 1 or 0, -1 with the error */
static int WT_IsMulti(void) {
	nativeVal_t v = { 0 };
	int r = WT_Multi(&v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* ShowWeightPrediction(id, control, lostfocus): the mass and boost after loading or unloading the slot; whether the
 * slot was loaded */
static byte WT_WeightPrediction(int id, const nativeVal_t *control, nativeVal_t *out) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *weapon = Mode_V(&t), *num = Mode_V(&t), *loaded = Mode_V(&t), *w = Mode_V(&t), *am = Mode_V(&t);
	nativeVal_t *mass = Mode_V(&t), *weight = Mode_V(&t);
	if (!Script_Import(mods) || !WT_Head(control, 5, weapon) || !WT_Last(control, num) || !WT_Loaded(num, weapon, loaded)) {
		return Mode_Done(&t, 0);
	}
	byte on = Native_True(loaded);
	Mode_ItemVisible(id, on ? "WeightDown" : "WeightUp", 1);
	Mode_ItemVisible(id, "WeightBack", 1);
	Mode_ItemVisible(id, on ? "BoostDown" : "BoostUp", 1);
	Mode_ItemVisible(id, "BoostBack", 1);
	if (!PORT_CALL(ShipMaker, GetWeapon, w, "v", weapon) || !Mode_ShipGet("AcMass", am) || !Native_ToInt(am, mass) ||
		!Native_Arith(Native_True(loaded) ? '-' : '+', mass, w, weight) ||
		!PORT_CALL(HangarTab, ShowBoostPrediction, NULL, "iv", (long)id, weight)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, loaded);
	return Mode_Done(&t, 1);
}

MODE_IFN(WT_ShowWeightPrediction, 3, WT_WeightPrediction(id, &a[1], out))
MODE_IFN(WT_HideWeightPrediction, 3, WT_Run(id, "hideweight", NULL))

/* the tab behind a description as wide as its text */
static void WT_FitTab(int id, const char *text, const char *tab, int margin) {
	double size = margin + Script_GetNum(id, text, "SizeX");
	if (margin == 40) {
		Script_SetNum(id, text, "MinX", 590 - size + 10);
	}
	Script_SetNum(id, tab, "SizeX", size - 2);
	Script_SetNum(id, tab, "X", 590 - size);
}

/* the text of the value added to b (a text as it is, another value as its repr) */
static void WT_Add(nativeBuf_t *b, const nativeVal_t *v) {
	Native_Repr(b, v, NR_STR);
}

/* ShowWeaponDesc(id, control, lostfocus): name, ammo, countermeasure, mass and cost of the weapon */
static byte WT_ShowWeaponDesc(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "ShipEdit", "ShipMaker", "string", NULL };
	static const char *const labels[5] = { "ShipEdit_Weapon", "ShipEdit_Ammo", "ShipEdit_Countermeasure", "ShipEdit_Weight",
		"ShipEdit_Cost" };
	static const char *const parts[3] = { "Name", "Ammo", "Countermeasure" };
	static const char *const tables[2] = { "WeaponValues", "WeaponCosts" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *base = Mode_V(&t), *v[10];
	if (!Script_Import(mods) || !Native_Arith('+', Mode_S(&t, "ShipEdit_"), control, base)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 10; i++) {
		v[i] = Mode_V(&t);
		if (i % 2 == 0) {
			Mode_Lang(labels[i / 2], v[i]);
			continue;
		}
		int mark = t.n;
		nativeVal_t *key = Mode_V(&t), *tab = Mode_V(&t), *up = Mode_V(&t), *x = Mode_V(&t);
		byte ok = i < 6 ? Native_Arith('+', base, Mode_S(&t, parts[i / 2]), key) && Mode_LangV(key, v[i]) :
			Mode_ModVar("ShipMaker", tables[i / 2 - 3], tab) && Native_Upper(control, up) && Native_Subscript(tab, up, x);
		if (ok && i >= 6) {
			Mode_Repr(x, v[i]);
		}
		Mode_Release(&t, mark);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	for (int i = 0; i < 10; i += 2) {
		if (i) {
			Native_BufAdd(&b, "\n", 1);
		}
		WT_Add(&b, v[i]);
		Native_BufAdd(&b, ": ", 2);
		WT_Add(&b, v[i + 1]);
	}
	nativeVal_t *desc = Mode_V(&t);
	Native_SetText(desc, b.p, b.len);
	Native_BufFree(&b);
	Script_SetStr(id, "WeaponDesc", "Text", Native_Text(desc));
	Script_SetNum(id, "WeaponDesc", "XAlignRight", 580);
	WT_FitTab(id, "WeaponDesc", "WeaponDescTab", 40);
	Mode_ItemVisible(id, "WeaponDesc", 1);
	Mode_ItemVisible(id, "WeaponDescTab", 1);
	return Mode_Done(&t, 1);
}

MODE_IFN(WT_ShowWeaponDescPy, 3, WT_ShowWeaponDesc(id, &a[1]))

/* HideWeaponDesc(id, control, gainfocus) and HideWeaponUpgradeDesc(id, control, gainfocus) */
static PyObject *WT_HideTwo(PyObject *args, const char *a, const char *b) {
	int id;
	PyObject *control, *gain;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &gain)) {
		return NULL;
	}
	Script_SetNum(id, a, "Visible", 0);
	Script_SetNum(id, b, "Visible", 0);
	return Py_ReturnNone();
}

static PyObject *WT_HideWeaponDesc(PyObject *self, PyObject *args) {
	(void)self;
	return WT_HideTwo(args, "WeaponDesc", "WeaponDescTab");
}

static PyObject *WT_HideWeaponUpgradeDesc(PyObject *self, PyObject *args) {
	(void)self;
	return WT_HideTwo(args, "WeaponUpgradeDesc", "WeaponUpgradeDescTab");
}

/* ShowWeaponUpgradeDesc(id, control, lostfocus): the text of the upgrade of the tile */
static byte WT_ShowWeaponUpgradeDesc(int id, const nativeVal_t *control) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *head = Mode_V(&t), *name = Mode_V(&t), *key = Mode_V(&t), *text = Mode_V(&t);
	if (!Mode_Import("ShipEdit") || !WT_G("prevWeapon", prev) || !WT_Head(control, 4, head) ||
		!Native_Arith('+', prev, head, name)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "WeaponUpgradeDesc", "IsNameValuePair", 0);
	if (!Native_Arith('+', Mode_S(&t, "ShipEdit_"), name, key) || !Mode_LangV(key, text)) {
		return Mode_Done(&t, 0);
	}
	Script_SetStr(id, "WeaponUpgradeDesc", "Text", Native_Text(text));
	Script_SetNum(id, "WeaponUpgradeDesc", "XAlignRight", 583);
	Mode_ItemVisible(id, "WeaponUpgradeDesc", 1);
	double sizeY = Script_GetNum(id, "WeaponUpgradeDesc", "SizeY");
	WT_FitTab(id, "WeaponUpgradeDesc", "WeaponUpgradeDescTab", 30);
	/* TabQuadSize + TabQuadSize / 2 */
	Script_SetNum(id, "WeaponUpgradeDescTab", "SizeY", sizeY + 14 + 7);
	Mode_ItemVisible(id, "WeaponUpgradeDescTab", 1);
	return Mode_Done(&t, 1);
}

MODE_IFN(WT_ShowWeaponUpgradeDescPy, 3, WT_ShowWeaponUpgradeDesc(id, &a[1]))

/* ShowSlotMarker(id, control, gainfocus): the marker on the slot of the ship, the mass and cost predictions */
static byte WT_ShowSlotMarker(int id, const nativeVal_t *control, const nativeVal_t *lost) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *sprite = Mode_V(&t), *focus = Mode_V(&t), *num = Mode_V(&t), *prev = Mode_V(&t), *nameId = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *uv = Mode_V(&t), *loaded = Mode_V(&t), *head = Mode_V(&t), *up = Mode_V(&t);
	nativeVal_t *costs = Mode_V(&t), *c = Mode_V(&t), *cost = Mode_V(&t), *text = Mode_V(&t), *ci = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t);
	nativeVal_t *u = Mode_V(&t), *v = Mode_V(&t);
	const char *item;
	if (!Script_Import(mods) || !Mode_ItemText(control, &item)) {
		return Mode_Done(&t, 0);
	}
	Mode_ItemVisible(id, "SlotMarker", 1);
	Mode_ScorerGet(id, item, "SpriteIndex", sprite);
	int blocked = Mode_CmpOpI(sprite, 16, '=');
	if (blocked < 0) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(id, item, blocked ? "SpriteIndex" : "FocusSpriteIndex", focus);
	if (!Mode_ScorerPut(id, "SlotMarker", "FocusSpriteIndex", focus) || !WT_Last(control, num) || !WT_G("prevWeapon", prev) ||
		!WT_NameId(num, prev, nameId) || !WT_ShipS("WPos", nameId, pos) || !Mode_Get2DPos(pos, uv)) {
		return Mode_Done(&t, 0);
	}
	nativeVal_t pair[2];
	if (!Mode_Unpack(uv, 2, pair)) {
		return Mode_Done(&t, 0);
	}
	*u = pair[0];
	*v = pair[1];
	if (!Mode_ScorerPut(id, "SlotMarker", "X", u) || !Mode_ScorerPut(id, "SlotMarker", "Y", v) ||
		!PORT_OWN(WeaponsTab, ShowWeightPrediction, loaded, "ivv", (long)id, control, lost) ||
		!WT_Head(control, 5, head) || !Native_Upper(head, up) || !Mode_ModVar("ShipMaker", "WeaponCosts", costs) ||
		!Native_Subscript(costs, up, c)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(c, cost);
	if (Native_True(loaded)) {
		Script_SetNum(id, "MoneyPredict", "Green", 255);
		if (!Native_Arith('+', Mode_S(&t, "+"), cost, text)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetInt(money, Mode_GetMoney());
		if (!Native_ToInt(cost, ci)) {
			return Mode_Done(&t, 0);
		}
		int poor = Mode_CmpOp(money, ci, '<');
		if (poor < 0) {
			return Mode_Done(&t, 0);
		}
		Script_SetNum(id, "MoneyPredict", "Green", poor ? 0 : 255);
		if (!Native_Arith('+', Mode_S(&t, "-"), cost, text)) {
			return Mode_Done(&t, 0);
		}
	}
	Script_SetStr(id, "MoneyPredict", "Text", Native_Text(text));
	Mode_ItemVisible(id, "MoneyPredict", 1);
	return Mode_Done(&t, 1);
}

MODE_IFN(WT_ShowSlotMarkerPy, 3, WT_ShowSlotMarker(id, &a[1], &a[2]))

/* HideSlotMarker(id, control, gainfocus) */
static byte WT_HideSlotMarker(int id, const nativeVal_t *control, const nativeVal_t *gain) {
	Mode_ItemVisible(id, "SlotMarker", 0);
	Mode_ItemVisible(id, "MoneyPredict", 0);
	return PORT_OWN(WeaponsTab, HideWeightPrediction, NULL, "ivv", (long)id, control, gain);
}

MODE_IFN(WT_HideSlotMarkerPy, 3, WT_HideSlotMarker(id, &a[1], &a[2]))

/* a free slot of the weapon: blocked (DoNothing) when the weapon would make the ship too heavy, else loadable */
static byte WT_FreeSlot(int id, const char *name, const nativeVal_t *weapon, int heavy, const char *highlight) {
	if (heavy) {
		Script_SetNum(id, name, "SpriteIndex", 16);
		Script_SetNum(id, name, "FocusSpriteIndex", 18);
		Script_SetNum(id, name, "HighLight", 1);
		Script_SetStr(id, name, "OnAccept", "WeaponsTab.DoNothing");
		return WT_PutWeaponHint(id, name, "HINT_Weapons5", 0, weapon);
	}
	Script_SetNum(id, name, "SpriteIndex", 17);
	Script_SetNum(id, name, "FocusSpriteIndex", 18);
	Script_SetNum(id, name, highlight, 1);
	Script_SetStr(id, name, "OnAccept", "WeaponsTab.LoadWeapon");
	return 1;
}

/* int(HangarTab.prevShip.AcMass) and the mass limit int(ShipMaker.ShipTon[string.upper(Model)][2]) */
static byte WT_Masses(nativeVal_t *weight, nativeVal_t *max) {
	nativeVal_t am = { 0 }, top = { 0 };
	byte ok = Mode_ShipGet("AcMass", &am) && Native_ToInt(&am, weight) && Mode_ShipTable("ShipTon", 2, &top) &&
		Native_ToInt(&top, max);
	Native_Free(&am);
	Native_Free(&top);
	return ok;
}

/* weight + ShipMaker.WeaponValues[string.upper(weapon)] > max: 1 or 0, -1 with the error */
static int WT_Over(const nativeVal_t *weight, const nativeVal_t *max, const nativeVal_t *weapon) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *values = Mode_V(&t), *up = Mode_V(&t), *w = Mode_V(&t), *sum = Mode_V(&t);
	int r = Mode_ModVar("ShipMaker", "WeaponValues", values) && Native_Upper(weapon, up) && Native_Subscript(values, up, w) &&
		Native_Arith('+', weight, w, sum) ? Mode_CmpOp(sum, max, '>') : -1;
	Mode_Done(&t, 1);
	return r;
}

/* int(HangarTab.prevShip.AcMass) + ShipMaker.WeaponValues[string.upper(weapon)] > the mass limit of the ship */
static int WT_TooHeavy(const nativeVal_t *weapon) {
	nativeVal_t weight = { 0 }, max = { 0 };
	int r = WT_Masses(&weight, &max) ? WT_Over(&weight, &max, weapon) : -1;
	Native_Free(&weight);
	Native_Free(&max);
	return r;
}

/* the mass and limit FillSlots reads before its test, then the test of an empty slot (0 for a loaded one) */
static int WT_TooHeavyFill(const nativeVal_t *loaded, const nativeVal_t *weapon) {
	nativeVal_t weight = { 0 }, max = { 0 };
	int r = WT_Masses(&weight, &max) ? Mode_CmpOpI(loaded, 0, '=') : -1;
	if (r > 0) {
		r = WT_Over(&weight, &max, weapon);
	}
	Native_Free(&weight);
	Native_Free(&max);
	return r;
}

/* the hint HINT_Weapons4 (loaded > 0) or HINT_Weapons3 of a slot: 0 with the error */
static byte WT_SlotHint(int id, const char *name, const nativeVal_t *loaded, const nativeVal_t *weapon) {
	int full = Mode_CmpOpI(loaded, 0, '>');
	return full >= 0 && WT_PutWeaponHint(id, name, full ? "HINT_Weapons4" : "HINT_Weapons3", 1, weapon);
}

/* LoadWeapon(id, control): the slot loaded or unloaded, paid for; the other free slots checked against the mass */
static byte WT_LoadWeapon(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "ShipEdit", "string", "Sound", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *weapon = Mode_V(&t), *num = Mode_V(&t), *nameId = Mode_V(&t), *costs = Mode_V(&t), *up = Mode_V(&t);
	nativeVal_t *c = Mode_V(&t), *cost = Mode_V(&t), *had = Mode_V(&t), *money = Mode_V(&t), *ci = Mode_V(&t);
	nativeVal_t *left = Mode_V(&t), *rep = Mode_V(&t), *name = Mode_V(&t), *isLoaded = Mode_V(&t);
	const char *item;
	if (!Script_Import(mods) || !WT_Head(control, 5, weapon) || !WT_Last(control, num) || !WT_NameId(num, weapon, nameId) ||
		!Mode_ModVar("ShipMaker", "WeaponCosts", costs) || !Native_Upper(weapon, up) || !Native_Subscript(costs, up, c)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(c, cost);
	if (!WT_ShipS("Weap", nameId, had)) {
		return Mode_Done(&t, 0);
	}
	byte buy = !Native_True(had);
	Native_SetInt(money, Mode_GetMoney());
	if (!Native_ToInt(cost, ci) || !Native_Arith(buy ? '-' : '+', money, ci, left)) {
		return Mode_Done(&t, 0);
	}
	int single = WT_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		int poor = Mode_CmpOpI(left, 0, '<');
		if (poor) {
			return Mode_Done(&t, poor == 1);
		}
		Mode_Repr(left, rep);
		Script_SetStr(id, "Money", "Text", Native_Text(rep));
		if (!Mode_SetMoney(left)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!WT_ExecCat("HangarTab.prevShip.Weap", nameId, " = 1 - HangarTab.prevShip.Weap", nameId) ||
		!Mode_ShipGet("Name", name) || !PORT_CALL(ShipMaker, SetShipParams, NULL, "v", name) ||
		!PORT_CALL(HangarTab, ShowShipInfo, NULL, "i", (long)id) || !WT_ShipS("Weap", nameId, isLoaded) ||
		!Mode_ItemText(control, &item)) {
		return Mode_Done(&t, 0);
	}
	byte on = Native_True(isLoaded);
	Script_SetNum(id, item, "SpriteIndex", on ? 19 : 17);
	Script_SetNum(id, item, "FocusSpriteIndex", on ? 20 : 18);
	long slots;
	if (!Mode_SoundOf(on ? "ShipEditLoadWeapon" : "ShipEditUnLoad") || !WT_SlotCount(weapon, &slots)) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < slots; i++) {
		int mark = t.n;
		nativeVal_t *idx = Mode_V(&t), *has = Mode_V(&t);
		char slot[WT_NAME];
		Native_SetInt(idx, i);
		int other = WT_Loaded(idx, weapon, has) ? Mode_CmpOp(idx, num, '!') : -1;
		if (other > 0) {
			other = Mode_CmpOpI(has, 0, '=');
		}
		if (other > 0) {
			int heavy = WT_Slot(slot, weapon, i) ? WT_TooHeavy(weapon) : -1;
			other = heavy < 0 ? -1 : (WT_FreeSlot(id, slot, weapon, heavy, "HighLight") &&
				(heavy || WT_PutWeaponHint(id, slot, "HINT_Weapons3", 1, weapon))) ? 1 : -1;
		}
		Mode_Release(&t, mark);
		if (other < 0) {
			return Mode_Done(&t, 0);
		}
	}
	char slot[WT_NAME];
	byte ok = WT_Slot(slot, weapon, num->type == NV_INT ? num->i : 0) && WT_SlotHint(id, slot, isLoaded, weapon);
	if (ok) {
		Script_SetDefault(id, slot);
	}
	return Mode_Done(&t, ok);
}

MODE_IFN(WT_LoadWeaponPy, 2, WT_LoadWeapon(id, &a[1]))

/* HideSlots(id, control): the slots and upgrades of the weapon hidden */
static byte WT_HideSlots(int id, const nativeVal_t *control) {
	static const char *const rest[] = { "WeaponSlotText", "UpgradeText", "Upgrade1Tile", "Upgrade2Tile", "SlotMarker" };
	nativeVal_t ship = { 0 }, none;
	char name[WT_NAME];
	Native_SetNone(&none);
	if (!Mode_Import("HangarTab") || !Mode_PrevShip(&ship)) {
		return 0;
	}
	int some = Mode_CmpOp(&ship, &none, '!');
	Native_Free(&ship);
	if (some > 0) {
		long slots;
		if (!WT_SlotCount(control, &slots)) {
			return 0;
		}
		for (long i = 0; i < slots; i++) {
			if (!WT_Slot(name, control, i)) {
				return 0;
			}
			Mode_ItemVisible(id, name, 0);
		}
	}
	if (some < 0 || !WT_Cat(name, "", control, "Upgrade1")) {
		return 0;
	}
	Mode_ItemVisible(id, name, 0);
	if (!WT_Cat(name, "", control, "Upgrade2")) {
		return 0;
	}
	Mode_ItemVisible(id, name, 0);
	for (int i = 0; i < 5; i++) {
		Mode_ItemVisible(id, rest[i], 0);
	}
	return 1;
}

MODE_IFN(WT_HideSlotsPy, 2, WT_HideSlots(id, &a[1]))

/* SScorer.Get(id, item, "HighLight") == 1: 1 or 0, -1 with the error */
static int WT_Lit(int id, const char *item) {
	nativeVal_t v;
	Mode_ScorerGet(id, item, "HighLight", &v);
	int r = Mode_CmpOpI(&v, 1, '=');
	Native_Free(&v);
	return r;
}

/* the links of the upgrade tiles after FillSlots (single player) */
static byte WT_UpgradeLinks(int id, const nativeVal_t *control, long slots, PyObject *linkLR, PyObject *linkUD,
	PyObject *unlinkLR) {
	char last[WT_NAME], first[WT_NAME];
	const char *c;
	int has1 = WT_Lit(id, "Upgrade1Tile"), has2 = has1 < 0 ? -1 : WT_Lit(id, "Upgrade2Tile");
	if (has2 < 0 || !Mode_ItemText(control, &c)) {
		return 0;
	}
	if (has1 || has2) {
		if (!Mode_LinkCall(linkUD, id, "Upgrade1Tile", "Exit") || !Mode_LinkCall(linkUD, id, "Upgrade2Tile", "Exit")) {
			return 0;
		}
		Script_SetStr(id, "Upgrade1Tile", "Up", c);
		Script_SetStr(id, "Upgrade2Tile", "Up", c);
	}
	if (!WT_Slot(last, control, slots - 1) || !WT_Slot(first, control, 0)) {
		return 0;
	}
	if (has1 && has2) {
		return Mode_LinkCall(linkLR, id, "Upgrade1Tile", "Upgrade2Tile") && Mode_LinkCall(linkLR, id, last, "Upgrade1Tile") &&
			Mode_LinkCall(linkLR, id, "Upgrade2Tile", first);
	}
	if (has1 || has2) {
		const char *tile = has1 ? "Upgrade1Tile" : "Upgrade2Tile";
		return Mode_LinkCall(unlinkLR, id, "Upgrade1Tile", "Upgrade2Tile") && Mode_LinkCall(linkLR, id, last, tile) &&
			Mode_LinkCall(linkLR, id, tile, first);
	}
	return 1;
}

/* the slots of FillSlots from x on: placed, shown, free ones checked against the mass, their hints */
static byte WT_PlaceSlots(int id, const nativeVal_t *control, long slots, double x, double y) {
	for (long i = 0; i < slots; i++) {
		char name[WT_NAME];
		nativeVal_t loaded = { 0 };
		if (!WT_Slot(name, control, i)) {
			return 0;
		}
		Script_SetNum(id, name, "X", x + i * 26);
		Script_SetNum(id, name, "Y", y);
		Mode_ItemVisible(id, name, 1);
		int heavy = WT_LoadedI(i, control, &loaded) ? WT_TooHeavyFill(&loaded, control) : -1;
		byte ok = heavy >= 0 && WT_FreeSlot(id, name, control, heavy, "Highlight");
		if (ok && !heavy) {
			int full = Mode_CmpOpI(&loaded, 0, '>');
			nativeVal_t hint = { 0 };
			ok = full >= 0 && WT_WeaponHint(full ? "HINT_Weapons4" : "HINT_Weapons3", 1, control, &hint) &&
				Mode_ScorerPut(id, name, "Hint", &hint);
			Native_Free(&hint);
			if (ok) {
				Script_SetNum(id, name, "MaxHintLineWidth", 550);
			}
		}
		Native_Free(&loaded);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* the sprites of the slots by their load, and the hint of the weapon button when one is loaded */
static byte WT_SlotSprites(int id, const nativeVal_t *control, long slots) {
	for (long i = 0; i < slots; i++) {
		char name[WT_NAME];
		nativeVal_t loaded = { 0 };
		byte ok = WT_Slot(name, control, i) && WT_LoadedI(i, control, &loaded);
		if (ok && Script_GetNum(id, name, "SpriteIndex") != 16) {
			byte on = Native_True(&loaded);
			Script_SetNum(id, name, "SpriteIndex", on ? 19 : 17);
			Script_SetNum(id, name, "FocusSpriteIndex", on ? 20 : 18);
		}
		int full = ok ? Mode_CmpOpI(&loaded, 0, '>') : -1;
		if (full > 0) {
			/* name[0:len(name) - 5]: the weapon button itself gets the hint */
			name[strlen(name) >= 5 ? strlen(name) - 5 : 0] = 0;
			full = WT_PutWeaponHint(id, name, "HINT_Weapons4", 1, control) ? 1 : -1;
		}
		Native_Free(&loaded);
		if (full < 0) {
			return 0;
		}
	}
	return 1;
}

/* FillSlots(id, control): the bar with the slots of the weapon, the loaded ones lit, and its links */
static byte WT_FillSlots(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *numSlots = Mode_V(&t), *label = Mode_V(&t), *area = Mode_V(&t);
	char name[WT_NAME], other[WT_NAME];
	PyObject *linkLR = Mode_From("Menu", "LinkLR"), *linkUD = linkLR ? Mode_From("Menu", "LinkUD") : NULL;
	PyObject *unlinkLR = linkUD ? Mode_From("Menu", "UnLinkLR") : NULL;
	byte ok = unlinkLR && Script_Import(mods) && WT_NumSlots(control, numSlots);
	double textWidth = 0, x = 84, y = 151;
	long slots = 0;
	if (ok) {
		Mode_ItemVisible(id, "WeaponSlotText", 1);
		Mode_Lang("ShipEdit_WeaponsMounted", label);
		nativeVal_t pair[2] = { { 0 }, { 0 } };
		ok = Mode_TextArea("Horatio", label, area) && Mode_Unpack(area, 2, pair);
		textWidth = ok ? pair[0].f : 0;
		Native_Free(&pair[0]);
		Native_Free(&pair[1]);
	}
	int multi = ok ? WT_IsMulti() : -1;
	if (multi > 0) {
		ok = Mode_Range(numSlots, &slots);
		x = (640 - (slots * 26 + (slots - 1) * 4) - textWidth - 4) / 2;
	}
	ok = ok && multi >= 0;
	if (ok) {
		Script_SetNum(id, "WeaponSlotText", "W", 0);
		Script_SetNum(id, "WeaponSlotText", "H", 0);
		Script_SetNum(id, "WeaponSlotText", "X", x);
		Script_SetNum(id, "WeaponSlotText", "Y", y);
		x = x + textWidth + 8;
		Mode_ItemVisible(id, "SlotsBox", 1);
		multi = WT_IsMulti();
		ok = multi >= 0;
	}
	if (ok) {
		if (!multi) {
			Mode_ItemVisible(id, "UpgradesBox", 1);
		}
		Mode_ItemVisible(id, "SlotsAndUpgradesBox", 1);
		ok = Mode_Range(numSlots, &slots) && WT_PlaceSlots(id, control, slots, x, y);
	}
	for (long i = 0; ok && i < slots - 1; i++) {
		ok = WT_Slot(name, control, i) && WT_Slot(other, control, i + 1) && Mode_LinkCall(linkLR, id, name, other);
	}
	for (long i = 0; ok && i < slots; i++) {
		ok = WT_Slot(name, control, i) && Native_Call(linkUD, NULL, "ivs", (long)id, control, name) &&
			Mode_LinkCall(linkUD, id, name, "Exit");
	}
	/* list.remove(control) of the six weapons: the others */
	int skip = -1;
	for (int i = 0; ok && skip < 0 && i < 6; i++) {
		nativeVal_t w;
		Native_SetStr(&w, wt_linked[i]);
		if (!Native_Compare(&w, control)) {
			skip = i;
		}
	}
	ok = ok && (skip >= 0 || Native_Raise(NE_VALUE, "list.remove(x): x not in list")) && WT_Slot(other, control, 0);
	for (int i = 0; ok && i < 6; i++) {
		ok = i == skip || Mode_LinkCall(linkUD, id, wt_linked[i], other);
	}
	ok = ok && Native_Call(linkUD, NULL, "ivs", (long)id, control, other) && Mode_LinkCall(linkUD, id, other, "Exit") &&
		WT_SlotSprites(id, control, slots);
	multi = ok ? WT_IsMulti() : -1;
	if (multi == 0) {
		ok = PORT_OWN(WeaponsTab, UpdateUpgrades, NULL, "iv", (long)id, control) &&
			WT_UpgradeLinks(id, control, slots, linkLR, linkUD, unlinkLR);
	} else if (multi > 0) {
		ok = WT_Slot(name, control, slots - 1) && Mode_LinkCall(linkLR, id, name, other);
	}
	ok = ok && multi >= 0;
	if (ok) {
		Script_SetStr(id, "Exit", "Up", other);
	}
	Py_XDECREF(linkLR);
	Py_XDECREF(linkUD);
	Py_XDECREF(unlinkLR);
	return Mode_Done(&t, ok);
}

MODE_IFN(WT_FillSlotsPy, 2, WT_FillSlots(id, &a[1]))

/* the names of upgradeId whose values are a and b (the first by value, "" when none), in its key order: *na and *nb
 * copies (freed first) */
static byte WT_FindUpgrades(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *na, nativeVal_t *nb) {
	nativeVal_t keys = { 0 };
	if (!Mode_Keys(MOD, "upgradeId", 1, &keys)) {
		return 0;
	}
	int n = Native_ForCount(&keys);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t k = { 0 }, ids = { 0 }, v = { 0 };
		ok = Native_SeqItem(&keys, i, &k) && WT_G("upgradeId", &ids) && Native_Subscript(&ids, &k, &v);
		int is = ok ? Mode_CmpOp(&v, a, '=') : -1;
		if (is > 0) {
			Native_Free(na);
			Native_Copy(na, &k);
		} else if (is == 0) {
			Native_Free(&v);
			is = Native_Subscript(&ids, &k, &v) ? Mode_CmpOp(&v, b, '=') : -1;
			if (is > 0) {
				Native_Free(nb);
				Native_Copy(nb, &k);
			}
		}
		ok = is >= 0;
		Native_Free(&k);
		Native_Free(&ids);
		Native_Free(&v);
	}
	Native_Free(&keys);
	return ok;
}

/* the upgrade tile of a weapon in the selected colours, its icon white */
static byte WT_UpgradeOn(int id, const nativeVal_t *control, int k) {
	char name[WT_NAME], tile[0x20];
	Str_Printf(tile, sizeof(tile), "Upgrade%d", k);
	if (!WT_Cat(name, "", control, tile)) {
		return 0;
	}
	Script_SetNum(id, name, "Alpha", 255);
	Mode_ItemColor(id, name, wt_white);
	Str_Printf(tile, sizeof(tile), "Upgrade%dTile", k);
	Script_SetNum(id, tile, "Alpha", 255);
	Mode_ItemColor(id, tile, wt_selected);
	return 1;
}

/* the hint of an upgrade into the item (HINT_Weapons7 of the one mounted, HINT_Weapons6 of the other) */
static byte WT_PutUpgradeHint(int id, const char *item, const char *key, const nativeVal_t *upgrade) {
	nativeVal_t hint = { 0 };
	byte ok = WT_UpgradeHint(key, upgrade, &hint) && Mode_ScorerPut(id, item, "Hint", &hint);
	Native_Free(&hint);
	return ok;
}

/* the hint of an upgrade tile the weapon does not use: HINT_Weapons6 with the upgrade when the player has it, else
 * HINT_Weapons9 */
static byte WT_UpgradeOff(int id, const char *tile, int has, const nativeVal_t *upgrade) {
	if (has) {
		if (!WT_PutUpgradeHint(id, tile, "HINT_Weapons6", upgrade)) {
			return 0;
		}
	} else {
		nativeVal_t text;
		Mode_Lang("HINT_Weapons9", &text);
		Script_SetStr(id, tile, "Hint", Native_Text(&text));
		Native_Free(&text);
	}
	Script_SetNum(id, tile, "MaxHintLineWidth", 550);
	return 1;
}

/* upgrade > 0 and odd (1), > 0 and even (2), else 0; -1 with the error (the tests of the script in its order) */
static int WT_Parity(const nativeVal_t *upgrade) {
	nativeVal_t r = { 0 }, two;
	Native_SetInt(&two, 2);
	int pos = Mode_CmpOpI(upgrade, 0, '>');
	if (pos <= 0) {
		return pos;
	}
	int odd = Native_Arith('%', upgrade, &two, &r) ? Mode_CmpOpI(&r, 0, '!') : -1;
	Native_Free(&r);
	if (odd) {
		return odd;
	}
	pos = Mode_CmpOpI(upgrade, 0, '>');
	if (pos <= 0) {
		return pos;
	}
	int even = Native_Arith('%', upgrade, &two, &r) ? Mode_CmpOpI(&r, 0, '=') : -1;
	Native_Free(&r);
	return even > 0 ? 2 : even;
}

/* `upgrade + d` of the script into *out (`upgrade` for 0) */
static byte WT_ReprPlus(const nativeVal_t *upgrade, long d, nativeVal_t *out) {
	nativeVal_t n, s = { 0 };
	if (!d) {
		Mode_Repr(upgrade, out);
		return 1;
	}
	Native_SetInt(&n, d < 0 ? -d : d);
	if (!Native_Arith(d < 0 ? '-' : '+', upgrade, &n, &s)) {
		return 0;
	}
	Mode_Repr(&s, out);
	Native_Free(&s);
	return 1;
}

/* HangarTab.prevShip.Upgrd<weapId[weapon]> */
static byte WT_Upgrade(const nativeVal_t *weapon, nativeVal_t *out) {
	nativeVal_t wids = { 0 }, wid = { 0 };
	byte ok = WT_G("weapId", &wids) && Native_Subscript(&wids, weapon, &wid) && WT_ShipS("Upgrd", &wid, out);
	Native_Free(&wids);
	Native_Free(&wid);
	return ok;
}

/* UpdateUpgrades(id, control): the two upgrade tiles of the weapon: available or not, the one mounted lit */
static byte WT_UpdateUpgrades(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *upgrade = Mode_V(&t), *u1 = Mode_S(&t, ""), *u2 = Mode_S(&t, ""), *a = Mode_V(&t), *b = Mode_V(&t);
	nativeVal_t *auxs = Mode_V(&t), *aux = Mode_V(&t), *ai = Mode_V(&t);
	char name1[WT_NAME], name2[WT_NAME];
	PyObject *l1 = Script_Import(mods) ? Mode_From("Menu", "LinkLR") : NULL, *l2 = l1 ? Mode_From("Menu", "LinkUD") : NULL;
	byte ok = l2 && WT_Cat(name1, "", control, "Upgrade1") && WT_Cat(name2, "", control, "Upgrade2");
	Py_XDECREF(l1);
	Py_XDECREF(l2);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Mode_ItemVisible(id, name1, 1);
	Mode_ItemVisible(id, name2, 1);
	Mode_ItemVisible(id, "UpgradeText", 1);
	Mode_ItemVisible(id, "Upgrade1Tile", 1);
	Mode_ItemVisible(id, "Upgrade2Tile", 1);
	int has[2] = { 0, 0 };
	for (int k = 0; k < 2; k++) {
		const char *tile = k ? "Upgrade2Tile" : "Upgrade1Tile", *ct = k ? name2 : name1;
		nativeVal_t name, list = { 0 };
		Native_SetStr(&name, ct);
		int got = Mode_ModVar("ShipEdit", "availableUpgradesList", &list) ? Native_Contains(&list, &name) : -1;
		Native_Free(&list);
		if (got < 0) {
			return Mode_Done(&t, 0);
		}
		if (!got) {
			Script_SetStr(id, ct, "File", "2D/ShipEditor/FullEditor.alpha.tga");
			Script_SetNum(id, ct, "SpriteIndex", 32);
		}
		Script_SetNum(id, ct, "Alpha", 255);
		Mode_ItemColor(id, ct, wt_grey);
		Script_SetNum(id, tile, "Alpha", 255);
		Mode_ItemColor(id, tile, wt_normal);
		Script_SetNum(id, tile, "W", got ? 34 : 0);
		Script_SetNum(id, tile, "H", got ? 34 : 0);
		Script_SetNum(id, tile, "HighLight", got);
		Script_SetStr(id, tile, "OnAccept", got ? "WeaponsTab.SelectUpgrade" : "WeaponsTab.DoNothing");
		has[k] = got;
	}
	int kind = WT_Upgrade(control, upgrade) ? WT_Parity(upgrade) : -1;
	if (kind == 1) {
		ok = WT_ReprPlus(upgrade, 0, a) && WT_ReprPlus(upgrade, 1, b) && WT_FindUpgrades(a, b, u1, u2) &&
			WT_UpgradeOn(id, control, 1) && WT_PutUpgradeHint(id, "Upgrade1Tile", "HINT_Weapons7", u1) &&
			WT_UpgradeOff(id, "Upgrade2Tile", has[1], u2);
	} else if (kind == 2) {
		ok = WT_ReprPlus(upgrade, -1, a) && WT_ReprPlus(upgrade, 0, b) && WT_FindUpgrades(a, b, u1, u2) &&
			WT_UpgradeOn(id, control, 2) && WT_PutUpgradeHint(id, "Upgrade2Tile", "HINT_Weapons7", u2) &&
			WT_UpgradeOff(id, "Upgrade1Tile", has[0], u1);
	} else if (kind == 0) {
		int none = Mode_CmpOpI(upgrade, 0, '=');
		ok = none >= 0;
		if (none > 0) {
			ok = WT_G("auxUpgradeId", auxs) && Native_Subscript(auxs, control, aux) && Native_ToInt(aux, ai) &&
				WT_ReprPlus(ai, 1, b) && WT_FindUpgrades(aux, b, u1, u2) && WT_UpgradeOff(id, "Upgrade1Tile", has[0], u1) &&
				WT_UpgradeOff(id, "Upgrade2Tile", has[1], u2);
		}
	} else {
		ok = 0;
	}
	return Mode_Done(&t, ok);
}

MODE_IFN(WT_UpdateUpgradesPy, 2, WT_UpdateUpgrades(id, &a[1]))

/* the button, tile, icon and frame of a weapon in the colours of the state (selected 1) */
static byte WT_WeaponColors(int id, const nativeVal_t *w, byte selected) {
	char name[WT_NAME];
	const int *c = selected ? wt_selected : wt_normal;
	if (!WT_Cat(name, "", w, "Tile")) {
		return 0;
	}
	Mode_ItemColor(id, name, c);
	if (!WT_Cat(name, "", w, "Icon")) {
		return 0;
	}
	Mode_ItemColor(id, name, selected ? wt_white : wt_grey);
	if (!WT_Cat(name, "", w, "Rect")) {
		return 0;
	}
	Script_SetNum(id, name, "Alpha", 255);
	Mode_ItemColor(id, name, c);
	return 1;
}

/* SelectWeapon(id, control): the weapon picked, the previous one back to normal, its slots shown */
static byte WT_SelectWeapon(int id, const nativeVal_t *control) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t);
	const char *p, *c;
	if (!Mode_Import("string") || !WT_G("prevWeapon", prev)) {
		return Mode_Done(&t, 0);
	}
	int some = Mode_CmpOp(prev, Mode_S(&t, ""), '!');
	if (some < 0) {
		return Mode_Done(&t, 0);
	}
	if (some) {
		if (!Mode_ItemText(prev, &p)) {
			return Mode_Done(&t, 0);
		}
		Script_SetNum(id, p, "Alpha", 255);
		Mode_ItemColor(id, p, wt_normal);
		if (!WT_PutWeaponHint(id, p, "HINT_Weapons1", 1, prev) || !WT_WeaponColors(id, prev, 0) ||
			!PORT_OWN(WeaponsTab, HideSlots, NULL, "iv", (long)id, prev)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_Put(MOD, "prevWeapon", NULL, control) || !Mode_ItemText(control, &c)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, c, "Alpha", 255);
	Mode_ItemColor(id, c, wt_selected);
	if (!WT_PutWeaponHint(id, c, "HINT_Weapons2", 0, control)) {
		return Mode_Done(&t, 0);
	}
	Script_SetDefault(id, c);
	PyObject *unlinkUD = WT_WeaponColors(id, control, 1) ? Mode_From("Menu", "UnLinkUD") : NULL;
	byte ok = unlinkUD != NULL;
	for (int i = 0; ok && i < 6; i++) {
		ok = Mode_LinkCall(unlinkUD, id, wt_unlink[i], "Exit");
	}
	Py_XDECREF(unlinkUD);
	if (ok) {
		Script_SetStr(id, "ShipEditTabWeapons", "Down", c);
		Script_SetStr(id, c, "Up", "ShipEditTabWeapons");
		ok = PORT_OWN(WeaponsTab, FillSlots, NULL, "iv", (long)id, control);
	}
	return Mode_Done(&t, ok);
}

MODE_IFN(WT_SelectWeaponPy, 2, WT_SelectWeapon(id, &a[1]))

/* HangarTab.prevShip.Upgrd<weapId[prevWeapon]> = <upgradeId[auxUpgradeId2[prevWeapon + "Upgrade" + k]]> and its
 * UAmmo = 200, as the exec lines of the script */
static byte WT_PutUpgrade(const nativeVal_t *k) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *key = Mode_V(&t), *aux2 = Mode_V(&t), *upg = Mode_V(&t), *wids = Mode_V(&t);
	nativeVal_t *wid = Mode_V(&t), *ids = Mode_V(&t), *value = Mode_V(&t);
	byte ok = WT_G("prevWeapon", prev) && Mode_Cat(&t, key, 3, prev, Mode_S(&t, "Upgrade"), k) &&
		WT_G("auxUpgradeId2", aux2) && Native_Subscript(aux2, key, upg) && WT_G("weapId", wids) &&
		(Native_Free(prev), WT_G("prevWeapon", prev)) && Native_Subscript(wids, prev, wid) && WT_G("upgradeId", ids) &&
		Native_Subscript(ids, upg, value) && WT_ExecCat("HangarTab.prevShip.Upgrd", wid, " = ", value);
	Mode_Release(&t, 0);
	prev = Mode_V(&t);
	wids = Mode_V(&t);
	wid = Mode_V(&t);
	ok = ok && WT_G("weapId", wids) && WT_G("prevWeapon", prev) && Native_Subscript(wids, prev, wid) &&
		WT_ExecCat("HangarTab.prevShip.UAmmo", wid, " = 200", NULL);
	return Mode_Done(&t, ok);
}

/* SelectUpgrade(id, control): the upgrade of the tile put on the weapon, swapped for the other one or taken off */
static byte WT_SelectUpgrade(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "HangarTab", "string", "Sound", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *upgrade = Mode_V(&t), *un = Mode_S(&t, ""), *pn = Mode_S(&t, ""), *a = Mode_V(&t);
	nativeVal_t *b = Mode_V(&t), *k = Mode_V(&t), *wids = Mode_V(&t), *wid = Mode_V(&t), *auxs = Mode_V(&t);
	nativeVal_t *up = Mode_V(&t), *ui = Mode_V(&t), *keys = Mode_V(&t), *hint = Mode_V(&t);
	const char *name = "", *c;
	if (!Script_Import(mods) || !WT_G("prevWeapon", prev)) {
		return Mode_Done(&t, 0);
	}
	int kind = WT_Upgrade(prev, upgrade) ? WT_Parity(upgrade) : -1;
	if (kind < 0) {
		return Mode_Done(&t, 0);
	}
	if (kind) {
		name = kind == 1 ? "Upgrade1Tile" : "Upgrade2Tile";
		if (!WT_ReprPlus(upgrade, 0, a) || !WT_ReprPlus(upgrade, kind == 1 ? 1 : -1, b) || !WT_FindUpgrades(a, b, un, pn)) {
			return Mode_Done(&t, 0);
		}
	}
	int len = Native_Size(control);
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	if (name[0]) {
		if (!Mode_ItemText(control, &c)) {
			return Mode_Done(&t, 0);
		}
		int same = Mode_CmpOp(control, Mode_S(&t, name), '=');
		if (same > 0) {
			Native_Free(prev);
			if (!WT_G("weapId", wids) || !WT_G("prevWeapon", prev) || !Native_Subscript(wids, prev, wid) ||
				!WT_ExecCat("HangarTab.prevShip.Upgrd", wid, " = 0", NULL)) {
				return Mode_Done(&t, 0);
			}
			Script_SetDefault(id, c);
			if (!Mode_SoundOf("ShipEditUnLoad")) {
				return Mode_Done(&t, 0);
			}
		} else if (same == 0) {
			Script_SetDefault(id, c);
			if (!WT_Char(control, 5, k) || !WT_PutUpgrade(k) || !Mode_SoundOf("ShipEditLoadWeapon")) {
				return Mode_Done(&t, 0);
			}
		} else {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_Free(prev);
		if (!WT_Char(control, 5, k) || !WT_PutUpgrade(k) || !WT_G("auxUpgradeId", auxs) || !WT_G("prevWeapon", prev) ||
			!Native_Subscript(auxs, prev, up)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(k);
		int two = WT_Char(control, 5, k) ? Mode_CmpOp(k, Mode_S(&t, "2"), '=') : -1;
		if (two < 0) {
			return Mode_Done(&t, 0);
		}
		if (two) {
			nativeVal_t *r = Mode_V(&t);
			if (!Native_ToInt(up, ui) || !WT_ReprPlus(ui, 1, r)) {
				return Mode_Done(&t, 0);
			}
			up = r;
		}
		if (!Mode_Keys(MOD, "upgradeId", 1, keys)) {
			return Mode_Done(&t, 0);
		}
		int n = Native_ForCount(keys);
		byte ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			nativeVal_t key = { 0 }, ids = { 0 }, v = { 0 };
			ok = Native_SeqItem(keys, i, &key) && WT_G("upgradeId", &ids) && Native_Subscript(&ids, &key, &v);
			int is = ok ? Mode_CmpOp(&v, up, '=') : -1;
			if (is > 0) {
				Native_Free(un);
				Native_Copy(un, &key);
				n = 0;
			}
			ok = is >= 0;
			Native_Free(&key);
			Native_Free(&ids);
			Native_Free(&v);
		}
		if (!ok || !Mode_ItemText(control, &c) || !WT_UpgradeHint("HINT_Weapons7", un, hint) || !WT_SetHint(id, c, hint)) {
			return Mode_Done(&t, 0);
		}
		Script_SetDefault(id, c);
		if (!Mode_SoundOf("ShipEditLoadWeapon")) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(prev);
	byte ok = WT_G("prevWeapon", prev) && PORT_OWN(WeaponsTab, UpdateUpgrades, NULL, "iv", (long)id, prev);
	return Mode_Done(&t, ok);
}

MODE_IFN(WT_SelectUpgradePy, 2, WT_SelectUpgrade(id, &a[1]))

/* SetWeaponsPanel(id): the items of the tab, hidden */
static byte WT_SetWeaponsPanel(int id) {
	static const char *const mods[] = { "Menu", "ShipEdit", "string", NULL };
	if (!Script_Import(mods)) {
		return 0;
	}
	/* OBJLIB 6: the .def runner takes its $names as a dict until it takes C values */
	int single = Native_NetOff();
	PyObject *vars = single >= 0 ? Py_BuildValue("{s:i}", "single", single) : NULL;
	byte ok = vars && WT_Run(id, "panel", vars);
	Py_XDECREF(vars);
	nativeVal_t keys = { 0 };
	ok = ok && Mode_Keys(MOD, "auxUpgradeId", 1, &keys);
	int n = ok ? Native_ForCount(&keys) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t w = { 0 };
		ok = Native_SeqItem(&keys, i, &w);
		for (int j = 0; ok && j < 6; j++) {
			char slot[0x20];
			nativeVal_t name = { 0 }, s;
			const char *text;
			Str_Printf(slot, sizeof(slot), "Slot%d", j);
			Native_SetStr(&s, slot);
			ok = Native_Arith('+', &w, &s, &name) && Mode_ItemText(&name, &text);
			PyObject *o = ok ? Native_ToPy(&name) : NULL;
			vars = o ? Py_BuildValue("{s:O}", "name", o) : NULL;
			ok = vars && WT_Run(id, "slot", vars);
			Py_XDECREF(vars);
			Py_XDECREF(o);
			Native_Free(&name);
		}
		Native_Free(&w);
	}
	Native_Free(&keys);
	int multi = ok ? WT_IsMulti() : -1;
	vars = multi >= 0 ? Py_BuildValue("{s:i}", "multi", multi) : NULL;
	ok = vars && WT_Run(id, "boxes", vars);
	Py_XDECREF(vars);
	return ok;
}

MODE_IFN(WT_SetWeaponsPanelPy, 1, WT_SetWeaponsPanel(id))

/* the six weapon buttons as ShowWeaponsPanel shows them; the ones the player has are appended to controlsToLinkList */
static byte WT_ShowWeapon(int id, const char *w) {
	char name[WT_NAME];
	Mode_ItemVisible(id, w, 1);
	Script_SetNum(id, w, "Alpha", 255);
	Mode_ItemColor(id, w, wt_normal);
	Str_Printf(name, sizeof(name), "%sTile", w);
	Mode_ItemVisible(id, name, 1);
	Mode_ItemColor(id, name, wt_normal);
	Str_Printf(name, sizeof(name), "%sIcon", w);
	Mode_ItemVisible(id, name, 1);
	Mode_ItemColor(id, name, wt_grey);
	Str_Printf(name, sizeof(name), "%sRect", w);
	Mode_ItemVisible(id, name, 1);
	Script_SetNum(id, name, "Alpha", 255);
	Mode_ItemColor(id, name, wt_normal);
	nativeVal_t v, list = { 0 };
	Native_SetStr(&v, w);
	int have = Mode_ModVar("ShipEdit", "availableWeaponsList", &list) ? Native_Contains(&list, &v) : -1;
	Native_Free(&list);
	if (have > 0) {
		return Mode_Append(MOD, "controlsToLinkList", &v);
	}
	if (have < 0) {
		return 0;
	}
	nativeVal_t text;
	Script_SetStr(id, w, "OnAccept", "");
	Script_SetStr(id, w, "OnGainFocus", "");
	Script_SetStr(id, w, "OnLooseFocus", "");
	Script_SetNum(id, w, "W", 0);
	Script_SetNum(id, w, "H", 0);
	Mode_Lang("HINT_Weapons8", &text);
	Script_SetStr(id, w, "Hint", Native_Text(&text));
	Native_Free(&text);
	Script_SetNum(id, w, "MaxHintLineWidth", 550);
	Str_Printf(name, sizeof(name), "%sTile", w);
	Script_SetNum(id, name, "Alpha", 222);
	Mode_ItemColor(id, name, wt_missing);
	Str_Printf(name, sizeof(name), "%sIcon", w);
	Script_SetStr(id, name, "File", "2D/ShipEditor/FullEditor.alpha.tga");
	Script_SetNum(id, name, "SpriteIndex", 32);
	Script_SetNum(id, name, "Alpha", 255);
	Mode_ItemColor(id, name, wt_grey);
	return 1;
}

/* controlsToLinkList[i] as the script reads it (the global each time) */
static byte WT_LinkItem(long i, nativeVal_t *out) {
	nativeVal_t list = { 0 }, tmp;
	const nativeVal_t *v = WT_G("controlsToLinkList", &list) ? Native_GetItem(&list, i, 1, &tmp) : NULL;
	if (v) {
		Native_Copy(out, v);
		if (v == &tmp) {
			Native_Free(&tmp);
		}
	}
	Native_Free(&list);
	return v != NULL;
}

/* fn(id, controlsToLinkList[i], controlsToLinkList[j]) (j -1: "Exit") */
static byte WT_LinkItems(PyObject *fn, int id, long i, long j) {
	nativeVal_t a = { 0 }, b = { 0 };
	byte ok = WT_LinkItem(i, &a) && (j < 0 ? (Native_SetStr(&b, "Exit"), 1) : WT_LinkItem(j, &b)) &&
		Native_Call(fn, NULL, "ivv", (long)id, &a, &b);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* ShowWeaponsPanel(id): the six weapons, those the player does not have dark; the ones he has linked */
static byte WT_ShowWeaponsPanel(int id) {
	static const char *const mods[] = { "ShipEdit", "string", NULL };
	PyObject *linkLR = Mode_From("Menu", "LinkLR"), *linkUD = linkLR ? Mode_From("Menu", "LinkUD") : NULL;
	nativeVal_t empty, list = { 0 };
	Native_SetEmpty(&empty, NV_LIST);
	byte ok = linkUD && Script_Import(mods) && Mode_Put(MOD, "controlsToLinkList", NULL, &empty);
	for (int i = 0; ok && i < 6; i++) {
		ok = WT_ShowWeapon(id, wt_shown[i]);
	}
	int last = ok && WT_G("controlsToLinkList", &list) ? Native_Size(&list) - 1 : -2;
	Native_Free(&list);
	ok = last >= -1;
	for (int i = 0; ok && i < last; i++) {
		ok = WT_LinkItems(linkLR, id, i, i + 1) && WT_LinkItems(linkUD, id, i, -1);
	}
	ok = ok && WT_LinkItems(linkLR, id, last, 0) && WT_LinkItems(linkUD, id, last, -1) && WT_LinkItems(linkUD, id, 0, -1);
	Py_XDECREF(linkLR);
	Py_XDECREF(linkUD);
	return ok;
}

MODE_IFN(WT_ShowWeaponsPanelPy, 1, WT_ShowWeaponsPanel(id))

/* HideWeaponsPanel(id) */
static byte WT_HideWeaponsPanel(int id) {
	static const char *const parts[4] = { "", "Tile", "Icon", "Rect" };
	nativeVal_t empty, keys = { 0 }, list = { 0 };
	Native_SetStr(&empty, "");
	byte ok = Mode_Put(MOD, "prevWeapon", NULL, &empty) && Mode_Keys(MOD, "weapId", 1, &keys);
	int n = ok ? Native_ForCount(&keys) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t w = { 0 };
		ok = Native_SeqItem(&keys, i, &w);
		for (int k = 0; ok && k < 4; k++) {
			char name[WT_NAME];
			ok = WT_Cat(name, "", &w, parts[k]);
			if (ok) {
				Mode_ItemVisible(id, name, 0);
			}
		}
		Native_Free(&w);
	}
	Native_Free(&keys);
	if (!ok) {
		return 0;
	}
	Mode_ItemVisible(id, "SlotsBox", 0);
	Mode_ItemVisible(id, "UpgradesBox", 0);
	Mode_ItemVisible(id, "SlotsAndUpgradesBox", 0);
	for (int i = 0; ok && i < 6; i++) {
		ok = PORT_OWN(WeaponsTab, HideSlots, NULL, "is", (long)id, wt_linked[i]);
	}
	PyObject *unlinkUD = ok ? Mode_From("Menu", "UnLinkUD") : NULL, *unlinkLR = unlinkUD ? Mode_From("Menu", "UnLinkLR") : NULL;
	ok = unlinkLR != NULL;
	for (int i = 0; ok && i < 6; i++) {
		ok = Mode_LinkCall(unlinkUD, id, wt_unlink[i], "Exit");
	}
	int last = ok && WT_G("controlsToLinkList", &list) ? Native_Size(&list) : -1;
	Native_Free(&list);
	ok = last >= 0 && (last == 0 || WT_LinkItems(unlinkLR, id, last - 1, -1));
	Py_XDECREF(unlinkUD);
	Py_XDECREF(unlinkLR);
	return ok;
}

MODE_IFN(WT_HideWeaponsPanelPy, 1, WT_HideWeaponsPanel(id))

PyMethodDef g_WeaponsTabMethods[] = {
	{ "DoNothing", Mode_PyNone2, 1, NULL },
	{ "ShowWeightPrediction", WT_ShowWeightPrediction, 1, NULL },
	{ "HideWeightPrediction", WT_HideWeightPrediction, 1, NULL },
	{ "ShowWeaponDesc", WT_ShowWeaponDescPy, 1, NULL },
	{ "HideWeaponDesc", WT_HideWeaponDesc, 1, NULL },
	{ "ShowWeaponUpgradeDesc", WT_ShowWeaponUpgradeDescPy, 1, NULL },
	{ "HideWeaponUpgradeDesc", WT_HideWeaponUpgradeDesc, 1, NULL },
	{ "ShowSlotMarker", WT_ShowSlotMarkerPy, 1, NULL },
	{ "HideSlotMarker", WT_HideSlotMarkerPy, 1, NULL },
	{ "LoadWeapon", WT_LoadWeaponPy, 1, NULL },
	{ "HideSlots", WT_HideSlotsPy, 1, NULL },
	{ "FillSlots", WT_FillSlotsPy, 1, NULL },
	{ "UpdateUpgrades", WT_UpdateUpgradesPy, 1, NULL },
	{ "SelectWeapon", WT_SelectWeaponPy, 1, NULL },
	{ "SelectUpgrade", WT_SelectUpgradePy, 1, NULL },
	{ "SetWeaponsPanel", WT_SetWeaponsPanelPy, 1, NULL },
	{ "ShowWeaponsPanel", WT_ShowWeaponsPanelPy, 1, NULL },
	{ "HideWeaponsPanel", WT_HideWeaponsPanelPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(WeaponsTab)
