/*
 * The WeaponsTab module in C (scripts/shipedit/weaponstab.py, issue #2): the weapons tab of the ship editor. The
 * player picks one of the six weapons, then loads or unloads it in the slots the ship has for its kind (bullets
 * nSlot00, energy nSlot02, missiles nSlot01), paid for with WeaponCosts, as long as the mass stays under the limit;
 * in single player one of its two upgrades. The layout is data/menus/weaponstab.def. controlsToLinkList, prevWeapon
 * (the weapon picked), the tables slotId, weapId, upgradeId, auxUpgradeId and auxUpgradeId2, the colours and UseTxt
 * stay variables of the module; the script builds the ship variables from the tables (Weap<slot><weapon>,
 * Upgrd<weapon>), so the C reads the tables too. ShipEdit, HangarTab, ShipMaker and Sound are used through their
 * modules.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"

static const char weaponstab_def[] = {
#embed "../../../data/menus/weaponstab.def"
	, 0 };

#define WEAPONSTAB_FILE "menus/weaponstab.def"
#define MOD "WeaponsTab"

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

static PyObject *WT_WeaponHint(scriptFrame_t *f, const char *key, byte use, PyObject *weapon);

/* the step weaponhint <key>: the Hint of a weapon button, string.replace(Scrap.GetLangStr(key) % UseTxt, "<n>",
 * Scrap.GetLangStr("ShipEdit_" + name + "Name")) (the error left set stops the file) */
static void WT_StepHint(int id, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	const char *item = Script_CurrentItem();
	PyObject *hint = WT_WeaponHint(&f, Sf_Text(&f, Sf_Item(&f, args, 0)), 1, Sf_Str(&f, item));
	Sf_ScorerSet(&f, id, item, "Hint", hint);
	Sf_Done(&f, NULL);
}

static const scriptStep_t weaponstab_steps[] = {
	{ "weaponhint", WT_StepHint },
	{ NULL, NULL },
};

static void WT_Run(scriptFrame_t *f, int id, const char *section, PyObject *vars) {
	if (!f->failed) {
		Sf_Check(f, Script_RunFileTry(id, WEAPONSTAB_FILE, section, weaponstab_def, weaponstab_steps, vars));
	}
}

static PyObject *WT_Global(scriptFrame_t *f, const char *name) {
	return Sf_Global(f, MOD, name);
}

static PyObject *WT_Ship(scriptFrame_t *f, const char *var) {
	return Sf_Attr(f, Sf_Mod(f, "HangarTab", "prevShip"), var);
}

/* HangarTab.prevShip.<prefix + text> */
static PyObject *WT_ShipS(scriptFrame_t *f, const char *prefix, PyObject *text) {
	char var[0x40];
	const char *s = Sf_Text(f, text);
	if (!s) {
		return NULL;
	}
	Str_Printf(var, sizeof(var), "%s%s", prefix, s);
	return WT_Ship(f, var);
}

static PyObject *WT_Upper(scriptFrame_t *f, PyObject *o) {
	return Sf_Ask(f, "string", "upper", "(O)", o);
}

static PyObject *WT_Lang(scriptFrame_t *f, const char *key) {
	return key ? Sf_Lang(f, Sf_Str(f, key)) : NULL;
}

/* Scrap.GetLangStr(key) % UseTxt */
static PyObject *WT_Use(scriptFrame_t *f, const char *key) {
	return Sf_Op(f, WT_Lang(f, key), '%', WT_Global(f, "UseTxt"));
}

/* string.replace(text, "<n>", Scrap.GetLangStr(prefix + name + suffix)): a hint with a name in it */
static PyObject *WT_Named(scriptFrame_t *f, PyObject *text, const char *prefix, PyObject *name, const char *suffix) {
	PyObject *n = Sf_Lang(f, Sf_Cat(f, "sOs", prefix, name, suffix));
	return Sf_Ask(f, "string", "replace", "(OsO)", text, "<n>", n);
}

/* the hint of a weapon (ShipEdit_<weapon>Name in it) from key, with UseTxt when use */
static PyObject *WT_WeaponHint(scriptFrame_t *f, const char *key, byte use, PyObject *weapon) {
	return WT_Named(f, use ? WT_Use(f, key) : WT_Lang(f, key), "ShipEdit_", weapon, "Name");
}

/* the hint of an upgrade: HINT_UpgradeName_<upgradeId[upgrade]> into key % UseTxt */
static PyObject *WT_UpgradeHint(scriptFrame_t *f, const char *key, PyObject *upgrade) {
	PyObject *text = WT_Use(f, key);
	return WT_Named(f, text, "HINT_UpgradeName_", Sf_GetItem(f, WT_Global(f, "upgradeId"), upgrade), "");
}

static void WT_Color(scriptFrame_t *f, int id, const char *item, const int *c) {
	Sf_ScorerNum(f, id, item, "Red", c[0]);
	Sf_ScorerNum(f, id, item, "Green", c[1]);
	Sf_ScorerNum(f, id, item, "Blue", c[2]);
}

static void WT_Visible(scriptFrame_t *f, int id, const char *item, int v) {
	Sf_ScorerNum(f, id, item, "Visible", v);
}

/* the C text of a + b (texts of the script) into out */
static const char *WT_Join(scriptFrame_t *f, char *out, int size, PyObject *a, const char *b) {
	const char *s = Sf_Text(f, a);
	if (!s) {
		return NULL;
	}
	Str_Printf(out, size, "%s%s", s, b);
	return out;
}

/* control + "Slot" + `i` */
static const char *WT_Slot(scriptFrame_t *f, char *out, int size, PyObject *control, long i) {
	char suffix[0x20];
	Str_Printf(suffix, sizeof(suffix), "Slot%ld", i);
	return WT_Join(f, out, size, control, suffix);
}

/* slotId[num] + weapId[weapon]: the name of a slot in the ship variables */
static PyObject *WT_NameId(scriptFrame_t *f, PyObject *num, PyObject *weapon) {
	PyObject *slot = Sf_GetItem(f, WT_Global(f, "slotId"), num);
	return Sf_Op(f, slot, '+', Sf_GetItem(f, WT_Global(f, "weapId"), weapon));
}

/* the slots of the kind of weapon (0 for another name) */
static PyObject *WT_NumSlots(scriptFrame_t *f, PyObject *weapon) {
	static const char *const kinds[3][3] = { { "Vulcan", "Devastator", "nSlot00" }, { "Tesla", "ATPC", "nSlot02" },
		{ "Inferno", "Swarm", "nSlot01" } };
	for (int k = 0; k < 3 && !f->failed; k++) {
		int is = Sf_EqS(f, weapon, kinds[k][0]);
		if (is == 0) {
			is = Sf_EqS(f, weapon, kinds[k][1]);
		}
		if (is > 0) {
			return WT_Ship(f, kinds[k][2]);
		}
	}
	return Sf_Int(f, 0);
}

/* control[0:len(control) - cut] and int(control[len(control) - 1]) */
static PyObject *WT_Head(scriptFrame_t *f, PyObject *control, int cut) {
	int len = Sf_Len(f, control);
	return len < 0 ? NULL : Sf_Slice(f, control, 0, len - cut);
}

static PyObject *WT_Last(scriptFrame_t *f, PyObject *control) {
	int len = Sf_Len(f, control);
	return Sf_ToInt(f, len < 0 ? NULL : Sf_Item(f, control, len - 1));
}

/* exec(code) in a function of the module that imported HangarTab; the lines "HangarTab.prevShip.<name> = <literal>"
 * and "HangarTab.prevShip.<name> = 1 - HangarTab.prevShip.<name>" made directly */
static void WT_Exec(scriptFrame_t *f, PyObject *code) {
	static const char ship[] = "HangarTab.prevShip.";
	const int sl = (int)sizeof(ship) - 1;
	PyObject *hangar = Sf_Keep(f, f->failed ? NULL : PyImport_ImportModule("HangarTab"));
	PyObject *mod = hangar ? Sf_Keep(f, PyImport_ImportModule(MOD)) : NULL;
	const char *s = mod && code && PyString_Check(code) ? PyString_AsString(code) : NULL;
	const char *eq = s && (int)strlen(s) == PyString_Size(code) && !strncmp(s, ship, sl) ? strstr(s, " = ") : NULL;
	char attr[0x40];
	int len = eq ? (int)(eq - s) - sl : 0;
	if (eq && len < (int)sizeof(attr) && Script_IsName(s + sl, len)) {
		const char *rhs = eq + 3;
		PyObject *v;
		memcpy(attr, s + sl, len);
		attr[len] = '\0';
		if (!strncmp(rhs, "1 - ", 4) && !strncmp(rhs + 4, ship, sl) && !strcmp(rhs + 4 + sl, attr)) {
			v = Sf_Op(f, Sf_Int(f, 1), '-', Sf_Attr(f, Sf_Attr(f, hangar, "prevShip"), attr));
		} else {
			v = Script_Literal(PyModule_GetDict(mod), rhs);
			v = v ? Sf_Keep(f, v) : NULL;
		}
		if (v || f->failed) {
			Sf_SetAttr(f, Sf_Attr(f, hangar, "prevShip"), attr, v);
			return;
		}
	}
	Sf_Exec(f, code, MOD, Sf_Build(f, "{s:O}", "HangarTab", hangar));
}

/* a sound of the Sound module played: SSound.Play(Sound.<name>Sound, Sound.<name>Volume) */
static void WT_Sound(scriptFrame_t *f, const char *name) {
	char snd[0x40], vol[0x40];
	Str_Printf(snd, sizeof(snd), "%sSound", name);
	Str_Printf(vol, sizeof(vol), "%sVolume", name);
	PyObject *s = Sf_Mod(f, "Sound", snd);
	Sf_Ask(f, "SSound", "Play", "(OO)", s, Sf_Mod(f, "Sound", vol));
}

/* the range of a count of the script */
static long WT_Range(scriptFrame_t *f, PyObject *n) {
	long count = 0;
	Sf_Check(f, !f->failed && Script_Range(n, &count));
	return f->failed ? 0 : count;
}

/* DoNothing(id, control) */
static PyObject *WT_DoNothing(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* ShowWeightPrediction(id, control, lostfocus): the mass and boost after loading or unloading the slot; whether the
 * slot was loaded */
static PyObject *WT_ShowWeightPrediction(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	PyObject *weapon = WT_Head(&f, control, 5);
	PyObject *num = WT_Last(&f, control);
	PyObject *loaded = WT_ShipS(&f, "Weap", WT_NameId(&f, num, weapon));
	int on = Sf_True(&f, loaded);
	if (on >= 0) {
		WT_Visible(&f, id, on ? "WeightDown" : "WeightUp", 1);
		WT_Visible(&f, id, "WeightBack", 1);
		WT_Visible(&f, id, on ? "BoostDown" : "BoostUp", 1);
		WT_Visible(&f, id, "BoostBack", 1);
	}
	PyObject *w = Sf_Ask(&f, "ShipMaker", "GetWeapon", "(O)", weapon);
	PyObject *weight = Sf_ToInt(&f, WT_Ship(&f, "AcMass"));
	on = Sf_True(&f, loaded);
	weight = Sf_Op(&f, weight, on ? '-' : '+', w);
	Sf_Ask(&f, "HangarTab", "ShowBoostPrediction", "(iO)", id, weight);
	return Sf_Return(&f, loaded);
}

/* HideWeightPrediction(id, control, lostfocus) */
static PyObject *WT_HideWeightPrediction(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	WT_Run(&f, id, "hideweight", NULL);
	return Sf_None(&f);
}

/* the tab behind a description as wide as its text */
static void WT_FitTab(scriptFrame_t *f, int id, const char *text, const char *tab, int margin) {
	double size = margin + Sf_ScorerGet(f, id, text, "SizeX");
	if (margin == 40) {
		Sf_ScorerNum(f, id, text, "MinX", 590 - size + 10);
	}
	Sf_ScorerNum(f, id, tab, "SizeX", size - 2);
	Sf_ScorerNum(f, id, tab, "X", 590 - size);
}

/* ShowWeaponDesc(id, control, lostfocus): name, ammo, countermeasure, mass and cost of the weapon */
static PyObject *WT_ShowWeaponDesc(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	PyObject *base = Sf_Cat(&f, "sO", "ShipEdit_", control);
	PyObject *t[10];
	t[0] = WT_Lang(&f, "ShipEdit_Weapon");
	t[1] = Sf_Lang(&f, Sf_Cat(&f, "Os", base, "Name"));
	t[2] = WT_Lang(&f, "ShipEdit_Ammo");
	t[3] = Sf_Lang(&f, Sf_Cat(&f, "Os", base, "Ammo"));
	t[4] = WT_Lang(&f, "ShipEdit_Countermeasure");
	t[5] = Sf_Lang(&f, Sf_Cat(&f, "Os", base, "Countermeasure"));
	t[6] = WT_Lang(&f, "ShipEdit_Weight");
	t[7] = Sf_Repr(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "WeaponValues"), WT_Upper(&f, control)));
	t[8] = WT_Lang(&f, "ShipEdit_Cost");
	t[9] = Sf_Repr(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "WeaponCosts"), WT_Upper(&f, control)));
	PyObject *desc = Sf_Cat(&f, "OsOsOsOsOsOsOsOsOsO", t[0], ": ", t[1], "\n", t[2], ": ", t[3], "\n", t[4], ": ", t[5],
		"\n", t[6], ": ", t[7], "\n", t[8], ": ", t[9]);
	Sf_ScorerSet(&f, id, "WeaponDesc", "Text", desc);
	Sf_ScorerNum(&f, id, "WeaponDesc", "XAlignRight", 580);
	WT_FitTab(&f, id, "WeaponDesc", "WeaponDescTab", 40);
	WT_Visible(&f, id, "WeaponDesc", 1);
	WT_Visible(&f, id, "WeaponDescTab", 1);
	return Sf_None(&f);
}

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
static PyObject *WT_ShowWeaponUpgradeDesc(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	PyObject *name = Sf_Op(&f, WT_Global(&f, "prevWeapon"), '+', WT_Head(&f, control, 4));
	Sf_ScorerNum(&f, id, "WeaponUpgradeDesc", "IsNameValuePair", 0);
	Sf_ScorerSet(&f, id, "WeaponUpgradeDesc", "Text", Sf_Lang(&f, Sf_Cat(&f, "sO", "ShipEdit_", name)));
	Sf_ScorerNum(&f, id, "WeaponUpgradeDesc", "XAlignRight", 583);
	WT_Visible(&f, id, "WeaponUpgradeDesc", 1);
	double sizeY = Sf_ScorerGet(&f, id, "WeaponUpgradeDesc", "SizeY");
	WT_FitTab(&f, id, "WeaponUpgradeDesc", "WeaponUpgradeDescTab", 30);
	/* TabQuadSize + TabQuadSize / 2 */
	Sf_ScorerNum(&f, id, "WeaponUpgradeDescTab", "SizeY", sizeY + 14 + 7);
	WT_Visible(&f, id, "WeaponUpgradeDescTab", 1);
	return Sf_None(&f);
}

/* ShowSlotMarker(id, control, gainfocus): the marker on the slot of the ship, the mass and cost predictions */
static PyObject *WT_ShowSlotMarker(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	const char *item = Sf_Text(&f, control);
	WT_Visible(&f, id, "SlotMarker", 1);
	PyObject *sprite = Sf_Ask(&f, "SScorer", "Get", "(iss)", id, item, "SpriteIndex");
	int blocked = Sf_EqI(&f, sprite, 16);
	if (blocked >= 0) {
		PyObject *v = Sf_Ask(&f, "SScorer", "Get", "(iss)", id, item, blocked ? "SpriteIndex" : "FocusSpriteIndex");
		Sf_ScorerSet(&f, id, "SlotMarker", "FocusSpriteIndex", v);
	}
	PyObject *num = WT_Last(&f, control);
	PyObject *nameId = WT_NameId(&f, num, WT_Global(&f, "prevWeapon"));
	PyObject *uv[2];
	if (Sf_Unpack(&f, Sf_Ask(&f, "SScorer", "Get2DPos", "(O)", WT_ShipS(&f, "WPos", nameId)), 2, uv)) {
		Sf_ScorerSet(&f, id, "SlotMarker", "X", uv[0]);
		Sf_ScorerSet(&f, id, "SlotMarker", "Y", uv[1]);
	}
	PyObject *loaded = Sf_Own(&f, MOD, "ShowWeightPrediction", "(iOO)", id, control, lost);
	PyObject *name = WT_Upper(&f, WT_Head(&f, control, 5));
	PyObject *cost = Sf_Repr(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "WeaponCosts"), name));
	int on = Sf_True(&f, loaded);
	if (on > 0) {
		Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 255);
		Sf_ScorerSet(&f, id, "MoneyPredict", "Text", Sf_Cat(&f, "sO", "+", cost));
	} else if (on == 0) {
		PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
		int poor = Sf_Compare(&f, money, "<", Sf_ToInt(&f, cost));
		if (poor >= 0) {
			Sf_ScorerNum(&f, id, "MoneyPredict", "Green", poor ? 0 : 255);
		}
		Sf_ScorerSet(&f, id, "MoneyPredict", "Text", Sf_Cat(&f, "sO", "-", cost));
	}
	WT_Visible(&f, id, "MoneyPredict", 1);
	return Sf_None(&f);
}

/* HideSlotMarker(id, control, gainfocus) */
static PyObject *WT_HideSlotMarker(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *gain;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &gain)) {
		return NULL;
	}
	WT_Visible(&f, id, "SlotMarker", 0);
	WT_Visible(&f, id, "MoneyPredict", 0);
	Sf_Own(&f, MOD, "HideWeightPrediction", "(iOO)", id, control, gain);
	return Sf_None(&f);
}

/* a free slot of the weapon: blocked (DoNothing) when the weapon would make the ship too heavy, else loadable */
static void WT_FreeSlot(scriptFrame_t *f, int id, const char *name, PyObject *weapon, int heavy, const char *highlight) {
	if (heavy > 0) {
		Sf_ScorerNum(f, id, name, "SpriteIndex", 16);
		Sf_ScorerNum(f, id, name, "FocusSpriteIndex", 18);
		Sf_ScorerNum(f, id, name, "HighLight", 1);
		Sf_ScorerStr(f, id, name, "OnAccept", "WeaponsTab.DoNothing");
		Sf_ScorerSet(f, id, name, "Hint", WT_WeaponHint(f, "HINT_Weapons5", 0, weapon));
		Sf_ScorerNum(f, id, name, "MaxHintLineWidth", 550);
	} else if (heavy == 0) {
		Sf_ScorerNum(f, id, name, "SpriteIndex", 17);
		Sf_ScorerNum(f, id, name, "FocusSpriteIndex", 18);
		Sf_ScorerNum(f, id, name, highlight, 1);
		Sf_ScorerStr(f, id, name, "OnAccept", "WeaponsTab.LoadWeapon");
	}
}

/* int(HangarTab.prevShip.AcMass) + ShipMaker.WeaponValues[string.upper(weapon)] > the mass limit of the ship */
static int WT_TooHeavy(scriptFrame_t *f, PyObject *weapon) {
	PyObject *weight = Sf_ToInt(f, WT_Ship(f, "AcMass"));
	PyObject *model = WT_Upper(f, WT_Ship(f, "Model"));
	PyObject *max = Sf_ToInt(f, Sf_Item(f, Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "ShipTon"), model), 2));
	PyObject *w = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "WeaponValues"), WT_Upper(f, weapon));
	return Sf_Compare(f, Sf_Op(f, weight, '+', w), ">", max);
}

/* the int(HangarTab.prevShip.AcMass) and mass limit FillSlots reads before its test, then the test */
static int WT_TooHeavyFill(scriptFrame_t *f, PyObject *loaded, PyObject *weapon) {
	PyObject *weight = Sf_ToInt(f, WT_Ship(f, "AcMass"));
	PyObject *model = WT_Upper(f, WT_Ship(f, "Model"));
	PyObject *max = Sf_ToInt(f, Sf_Item(f, Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "ShipTon"), model), 2));
	int empty = Sf_EqI(f, loaded, 0);
	if (empty <= 0) {
		return empty;
	}
	PyObject *w = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "WeaponValues"), WT_Upper(f, weapon));
	return Sf_Compare(f, Sf_Op(f, weight, '+', w), ">", max);
}

/* LoadWeapon(id, control): the slot loaded or unloaded, paid for; the other free slots checked against the mass */
static PyObject *WT_LoadWeapon(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "string");
	Sf_Import(&f, "Sound");
	PyObject *weapon = WT_Head(&f, control, 5);
	PyObject *num = WT_Last(&f, control);
	PyObject *nameId = WT_NameId(&f, num, weapon);
	PyObject *cost = Sf_Repr(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "WeaponCosts"), WT_Upper(&f, weapon)));
	int buy = Sf_True(&f, WT_ShipS(&f, "Weap", nameId));
	buy = buy < 0 ? -1 : !buy;
	PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
	PyObject *left = Sf_Op(&f, money, buy ? '-' : '+', Sf_ToInt(&f, cost));
	if (Sf_Compare(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()"), "!=", Sf_Int(&f, 1)) > 0) {
		if (Sf_Compare(&f, left, "<", Sf_Int(&f, 0)) != 0) {
			return Sf_None(&f);
		}
		Sf_ScorerSet(&f, id, "Money", "Text", Sf_Repr(&f, left));
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", left);
	}
	WT_Exec(&f, Sf_Cat(&f, "sOsO", "HangarTab.prevShip.Weap", nameId, " = 1 - HangarTab.prevShip.Weap", nameId));
	Sf_Ask(&f, "ShipMaker", "SetShipParams", "(O)", WT_Ship(&f, "Name"));
	Sf_Ask(&f, "HangarTab", "ShowShipInfo", "(i)", id);
	PyObject *isLoaded = WT_ShipS(&f, "Weap", nameId);
	const char *item = Sf_Text(&f, control);
	int on = Sf_True(&f, isLoaded);
	if (on >= 0) {
		Sf_ScorerNum(&f, id, item, "SpriteIndex", on ? 19 : 17);
		Sf_ScorerNum(&f, id, item, "FocusSpriteIndex", on ? 20 : 18);
		WT_Sound(&f, on ? "ShipEditLoadWeapon" : "ShipEditUnLoad");
	}
	long slots = WT_Range(&f, WT_NumSlots(&f, weapon));
	for (long i = 0; i < slots && !f.failed; i++) {
		int mark = f.n;
		PyObject *idx = Sf_Int(&f, i);
		PyObject *has = WT_ShipS(&f, "Weap", WT_NameId(&f, idx, weapon));
		int other = Sf_Compare(&f, idx, "!=", num);
		if (other > 0) {
			other = Sf_EqI(&f, has, 0);
		}
		if (other > 0) {
			char name[0x40];
			WT_Slot(&f, name, sizeof(name), weapon, i);
			int heavy = WT_TooHeavy(&f, weapon);
			WT_FreeSlot(&f, id, name, weapon, heavy, "HighLight");
			if (heavy == 0) {
				Sf_ScorerSet(&f, id, name, "Hint", WT_WeaponHint(&f, "HINT_Weapons3", 1, weapon));
				Sf_ScorerNum(&f, id, name, "MaxHintLineWidth", 550);
			}
		}
		Sf_Release(&f, mark);
	}
	char name[0x40];
	WT_Slot(&f, name, sizeof(name), weapon, PyInt_Check(num) ? PyInt_AsLong(num) : 0);
	int full = Sf_Compare(&f, isLoaded, ">", Sf_Int(&f, 0));
	if (full >= 0) {
		Sf_ScorerSet(&f, id, name, "Hint", WT_WeaponHint(&f, full ? "HINT_Weapons4" : "HINT_Weapons3", 1, weapon));
		Sf_ScorerNum(&f, id, name, "MaxHintLineWidth", 550);
	}
	Sf_ScorerDefault(&f, id, name);
	return Sf_None(&f);
}

/* HideSlots(id, control): the slots and upgrades of the weapon hidden */
static PyObject *WT_HideSlots(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	char name[0x40];
	if (Sf_Compare(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "!=", Py_None) > 0) {
		long slots = WT_Range(&f, WT_NumSlots(&f, control));
		for (long i = 0; i < slots && !f.failed; i++) {
			WT_Visible(&f, id, WT_Slot(&f, name, sizeof(name), control, i), 0);
		}
	}
	WT_Visible(&f, id, WT_Join(&f, name, sizeof(name), control, "Upgrade1"), 0);
	WT_Visible(&f, id, WT_Join(&f, name, sizeof(name), control, "Upgrade2"), 0);
	static const char *const rest[] = { "WeaponSlotText", "UpgradeText", "Upgrade1Tile", "Upgrade2Tile", "SlotMarker" };
	for (int i = 0; i < 5; i++) {
		WT_Visible(&f, id, rest[i], 0);
	}
	return Sf_None(&f);
}

/* the links of the upgrade tiles after FillSlots (single player) */
static void WT_UpgradeLinks(scriptFrame_t *f, int id, PyObject *control, long slots, PyObject *linkLR, PyObject *linkUD,
	PyObject *unlinkLR) {
	char last[0x40], first[0x40];
	int has1 = Sf_EqI(f, Sf_Ask(f, "SScorer", "Get", "(iss)", id, "Upgrade1Tile", "HighLight"), 1);
	int has2 = Sf_EqI(f, Sf_Ask(f, "SScorer", "Get", "(iss)", id, "Upgrade2Tile", "HighLight"), 1);
	if (f->failed) {
		return;
	}
	const char *c = Sf_Text(f, control);
	if (has1 || has2) {
		Sf_Line(f, linkUD, "(iss)", id, "Upgrade1Tile", "Exit");
		Sf_Line(f, linkUD, "(iss)", id, "Upgrade2Tile", "Exit");
		Sf_ScorerStr(f, id, "Upgrade1Tile", "Up", c);
		Sf_ScorerStr(f, id, "Upgrade2Tile", "Up", c);
	}
	WT_Slot(f, last, sizeof(last), control, slots - 1);
	WT_Slot(f, first, sizeof(first), control, 0);
	if (has1 && has2) {
		Sf_Line(f, linkLR, "(iss)", id, "Upgrade1Tile", "Upgrade2Tile");
		Sf_Line(f, linkLR, "(iss)", id, last, "Upgrade1Tile");
		Sf_Line(f, linkLR, "(iss)", id, "Upgrade2Tile", first);
	} else if (has1 || has2) {
		const char *tile = has1 ? "Upgrade1Tile" : "Upgrade2Tile";
		Sf_Line(f, unlinkLR, "(iss)", id, "Upgrade1Tile", "Upgrade2Tile");
		Sf_Line(f, linkLR, "(iss)", id, last, tile);
		Sf_Line(f, linkLR, "(iss)", id, tile, first);
	}
}

/* FillSlots(id, control): the bar with the slots of the weapon, the loaded ones lit, and its links */
static PyObject *WT_FillSlots(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	char name[0x40], other[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	PyObject *linkLR = Sf_From(&f, "Menu", "LinkLR");
	PyObject *linkUD = Sf_From(&f, "Menu", "LinkUD");
	PyObject *unlinkLR = Sf_From(&f, "Menu", "UnLinkLR");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	PyObject *numSlots = WT_NumSlots(&f, control);
	WT_Visible(&f, id, "WeaponSlotText", 1);
	PyObject *area[2];
	double textWidth = 0, x = 84, y = 151;
	if (Sf_Unpack(&f, Sf_Ask(&f, "SScorer", "GetTextArea", "(sO)", "Horatio", WT_Lang(&f, "ShipEdit_WeaponsMounted")), 2,
		area)) {
		textWidth = PyFloat_AsDouble(area[0]);
	}
	long slots = 0;
	if (Sf_True(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()")) > 0) {
		slots = WT_Range(&f, numSlots);
		x = (640 - (slots * 26 + (slots - 1) * 4) - textWidth - 4) / 2;
	}
	Sf_ScorerNum(&f, id, "WeaponSlotText", "W", 0);
	Sf_ScorerNum(&f, id, "WeaponSlotText", "H", 0);
	Sf_ScorerNum(&f, id, "WeaponSlotText", "X", x);
	Sf_ScorerNum(&f, id, "WeaponSlotText", "Y", y);
	x = x + textWidth + 8;
	WT_Visible(&f, id, "SlotsBox", 1);
	if (Sf_True(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()")) == 0) {
		WT_Visible(&f, id, "UpgradesBox", 1);
	}
	WT_Visible(&f, id, "SlotsAndUpgradesBox", 1);
	slots = WT_Range(&f, numSlots);
	for (long i = 0; i < slots && !f.failed; i++) {
		int mark = f.n;
		WT_Slot(&f, name, sizeof(name), control, i);
		Sf_ScorerNum(&f, id, name, "X", x + i * 26);
		Sf_ScorerNum(&f, id, name, "Y", y);
		WT_Visible(&f, id, name, 1);
		PyObject *loaded = WT_ShipS(&f, "Weap", WT_NameId(&f, Sf_Int(&f, i), control));
		int heavy = WT_TooHeavyFill(&f, loaded, control);
		WT_FreeSlot(&f, id, name, control, heavy, "Highlight");
		if (heavy == 0) {
			int full = Sf_Compare(&f, loaded, ">", Sf_Int(&f, 0));
			if (full >= 0) {
				Sf_ScorerSet(&f, id, name, "Hint", WT_WeaponHint(&f, full ? "HINT_Weapons4" : "HINT_Weapons3", 1, control));
			}
			Sf_ScorerNum(&f, id, name, "MaxHintLineWidth", 550);
		}
		Sf_Release(&f, mark);
	}
	for (long i = 0; i < slots - 1 && !f.failed; i++) {
		WT_Slot(&f, name, sizeof(name), control, i);
		WT_Slot(&f, other, sizeof(other), control, i + 1);
		Sf_Line(&f, linkLR, "(iss)", id, name, other);
	}
	for (long i = 0; i < slots && !f.failed; i++) {
		WT_Slot(&f, name, sizeof(name), control, i);
		Sf_Line(&f, linkUD, "(iOs)", id, control, name);
		Sf_Line(&f, linkUD, "(iss)", id, name, "Exit");
	}
	PyObject *list = Sf_Build(&f, "[ssssss]", wt_linked[0], wt_linked[1], wt_linked[2], wt_linked[3], wt_linked[4],
		wt_linked[5]);
	Sf_Method(&f, list, "remove", "(O)", control);
	WT_Slot(&f, other, sizeof(other), control, 0);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *w = Sf_Next(&f, list, i);
		if (!w) {
			break;
		}
		Sf_Line(&f, linkUD, "(iOs)", id, w, other);
		Sf_Release(&f, mark);
	}
	Sf_Line(&f, linkUD, "(iOs)", id, control, other);
	Sf_Line(&f, linkUD, "(iss)", id, other, "Exit");
	for (long i = 0; i < slots && !f.failed; i++) {
		int mark = f.n;
		WT_Slot(&f, name, sizeof(name), control, i);
		PyObject *loaded = WT_ShipS(&f, "Weap", WT_NameId(&f, Sf_Int(&f, i), control));
		if (Sf_ScorerGet(&f, id, name, "SpriteIndex") != 16) {
			int on = Sf_True(&f, loaded);
			if (on >= 0) {
				Sf_ScorerNum(&f, id, name, "SpriteIndex", on ? 19 : 17);
				Sf_ScorerNum(&f, id, name, "FocusSpriteIndex", on ? 20 : 18);
			}
		}
		if (Sf_Compare(&f, loaded, ">", Sf_Int(&f, 0)) > 0) {
			/* name[0:len(name) - 5]: the weapon button itself gets the hint */
			name[strlen(name) >= 5 ? strlen(name) - 5 : 0] = 0;
			Sf_ScorerSet(&f, id, name, "Hint", WT_WeaponHint(&f, "HINT_Weapons4", 1, control));
			Sf_ScorerNum(&f, id, name, "MaxHintLineWidth", 550);
		}
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()")) == 0) {
		Sf_Own(&f, MOD, "UpdateUpgrades", "(iO)", id, control);
		WT_UpgradeLinks(&f, id, control, slots, linkLR, linkUD, unlinkLR);
	} else if (!f.failed) {
		WT_Slot(&f, name, sizeof(name), control, slots - 1);
		Sf_Line(&f, linkLR, "(iss)", id, name, other);
	}
	Sf_ScorerStr(&f, id, "Exit", "Up", other);
	return Sf_None(&f);
}

/* the names of upgradeId whose values are `a` and `b` (the first by value, "" when none), in its key order */
static void WT_FindUpgrades(scriptFrame_t *f, PyObject *a, PyObject *b, PyObject **na, PyObject **nb) {
	PyObject *keys = Sf_Method(f, WT_Global(f, "upgradeId"), "keys", "()");
	for (int i = 0; !f->failed; i++) {
		PyObject *n = Sf_Next(f, keys, i);
		if (!n) {
			break;
		}
		PyObject *v = Sf_GetItem(f, WT_Global(f, "upgradeId"), n);
		int is = Sf_Compare(f, v, "==", a);
		if (is > 0) {
			*na = n;
		} else if (is == 0 && Sf_Compare(f, Sf_GetItem(f, WT_Global(f, "upgradeId"), n), "==", b) > 0) {
			*nb = n;
		}
	}
}

/* the upgrade tile of a weapon in the selected colours, its icon white */
static void WT_UpgradeOn(scriptFrame_t *f, int id, PyObject *control, int k) {
	char name[0x40], tile[0x20];
	Str_Printf(tile, sizeof(tile), "Upgrade%d", k);
	WT_Join(f, name, sizeof(name), control, tile);
	Sf_ScorerNum(f, id, name, "Alpha", 255);
	WT_Color(f, id, name, wt_white);
	Str_Printf(tile, sizeof(tile), "Upgrade%dTile", k);
	Sf_ScorerNum(f, id, tile, "Alpha", 255);
	WT_Color(f, id, tile, wt_selected);
}

/* the hint of an upgrade tile the weapon does not use: HINT_Weapons6 with the upgrade when the player has it, else
 * HINT_Weapons9 */
static void WT_UpgradeOff(scriptFrame_t *f, int id, const char *tile, int has, PyObject *upgrade) {
	if (has) {
		Sf_ScorerSet(f, id, tile, "Hint", WT_UpgradeHint(f, "HINT_Weapons6", upgrade));
	} else {
		Sf_ScorerSet(f, id, tile, "Hint", WT_Lang(f, "HINT_Weapons9"));
	}
	Sf_ScorerNum(f, id, tile, "MaxHintLineWidth", 550);
}

/* UpdateUpgrades(id, control): the two upgrade tiles of the weapon: available or not, the one mounted lit */
static PyObject *WT_UpdateUpgrades(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	char name1[0x40], name2[0x40], ct[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "string");
	Sf_From(&f, "Menu", "LinkLR");
	Sf_From(&f, "Menu", "LinkUD");
	WT_Join(&f, name1, sizeof(name1), control, "Upgrade1");
	WT_Join(&f, name2, sizeof(name2), control, "Upgrade2");
	WT_Visible(&f, id, name1, 1);
	WT_Visible(&f, id, name2, 1);
	WT_Visible(&f, id, "UpgradeText", 1);
	WT_Visible(&f, id, "Upgrade1Tile", 1);
	WT_Visible(&f, id, "Upgrade2Tile", 1);
	int has[2] = { 0, 0 };
	for (int k = 0; k < 2 && !f.failed; k++) {
		const char *tile = k ? "Upgrade2Tile" : "Upgrade1Tile";
		int got = Sf_In(&f, Sf_Str(&f, k ? name2 : name1), Sf_Mod(&f, "ShipEdit", "availableUpgradesList"));
		if (got < 0) {
			break;
		}
		Str_Printf(ct, sizeof(ct), "%s", k ? name2 : name1);
		if (!got) {
			Sf_ScorerStr(&f, id, ct, "File", "2D/ShipEditor/FullEditor.alpha.tga");
			Sf_ScorerNum(&f, id, ct, "SpriteIndex", 32);
		}
		Sf_ScorerNum(&f, id, ct, "Alpha", 255);
		WT_Color(&f, id, ct, wt_grey);
		Sf_ScorerNum(&f, id, tile, "Alpha", 255);
		WT_Color(&f, id, tile, wt_normal);
		Sf_ScorerNum(&f, id, tile, "W", got ? 34 : 0);
		Sf_ScorerNum(&f, id, tile, "H", got ? 34 : 0);
		Sf_ScorerNum(&f, id, tile, "HighLight", got);
		Sf_ScorerStr(&f, id, tile, "OnAccept", got ? "WeaponsTab.SelectUpgrade" : "WeaponsTab.DoNothing");
		has[k] = got;
	}
	PyObject *upgrade = WT_ShipS(&f, "Upgrd", Sf_GetItem(&f, WT_Global(&f, "weapId"), control));
	PyObject *u1 = Sf_Str(&f, ""), *u2 = u1;
	PyObject *zero = Sf_Int(&f, 0);
	int pos = Sf_Compare(&f, upgrade, ">", zero), odd = 0, even = 0;
	if (pos > 0) {
		odd = Sf_Compare(&f, Sf_OpI(&f, upgrade, '%', 2), "!=", zero);
		if (odd == 0 && Sf_Compare(&f, upgrade, ">", zero) > 0) {
			even = Sf_EqI(&f, Sf_OpI(&f, upgrade, '%', 2), 0);
		}
	}
	if (odd > 0) {
		WT_FindUpgrades(&f, Sf_Repr(&f, upgrade), Sf_Repr(&f, Sf_OpI(&f, upgrade, '+', 1)), &u1, &u2);
		WT_UpgradeOn(&f, id, control, 1);
		Sf_ScorerSet(&f, id, "Upgrade1Tile", "Hint", WT_UpgradeHint(&f, "HINT_Weapons7", u1));
		WT_UpgradeOff(&f, id, "Upgrade2Tile", has[1], u2);
	} else if (even > 0) {
		WT_FindUpgrades(&f, Sf_Repr(&f, Sf_OpI(&f, upgrade, '-', 1)), Sf_Repr(&f, upgrade), &u1, &u2);
		WT_UpgradeOn(&f, id, control, 2);
		Sf_ScorerSet(&f, id, "Upgrade2Tile", "Hint", WT_UpgradeHint(&f, "HINT_Weapons7", u2));
		WT_UpgradeOff(&f, id, "Upgrade1Tile", has[0], u1);
	} else if (!f.failed && Sf_EqI(&f, upgrade, 0) > 0) {
		PyObject *aux = Sf_GetItem(&f, WT_Global(&f, "auxUpgradeId"), control);
		WT_FindUpgrades(&f, aux, Sf_Repr(&f, Sf_OpI(&f, Sf_ToInt(&f, aux), '+', 1)), &u1, &u2);
		WT_UpgradeOff(&f, id, "Upgrade1Tile", has[0], u1);
		WT_UpgradeOff(&f, id, "Upgrade2Tile", has[1], u2);
	}
	return Sf_None(&f);
}

/* SelectWeapon(id, control): the weapon picked, the previous one back to normal, its slots shown */
static PyObject *WT_SelectWeapon(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	char name[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	if (Sf_Compare(&f, WT_Global(&f, "prevWeapon"), "!=", Sf_Str(&f, "")) > 0) {
		PyObject *prev = WT_Global(&f, "prevWeapon");
		const char *p = Sf_Text(&f, prev);
		Sf_ScorerNum(&f, id, p, "Alpha", 255);
		WT_Color(&f, id, p, wt_normal);
		Sf_ScorerSet(&f, id, p, "Hint", WT_WeaponHint(&f, "HINT_Weapons1", 1, prev));
		Sf_ScorerNum(&f, id, p, "MaxHintLineWidth", 550);
		WT_Color(&f, id, WT_Join(&f, name, sizeof(name), WT_Global(&f, "prevWeapon"), "Tile"), wt_normal);
		WT_Color(&f, id, WT_Join(&f, name, sizeof(name), WT_Global(&f, "prevWeapon"), "Icon"), wt_grey);
		WT_Join(&f, name, sizeof(name), WT_Global(&f, "prevWeapon"), "Rect");
		Sf_ScorerNum(&f, id, name, "Alpha", 255);
		WT_Color(&f, id, name, wt_normal);
		Sf_Own(&f, MOD, "HideSlots", "(iO)", id, WT_Global(&f, "prevWeapon"));
	}
	Sf_SetGlobal(&f, MOD, "prevWeapon", control);
	const char *c = Sf_Text(&f, control);
	Sf_ScorerNum(&f, id, c, "Alpha", 255);
	WT_Color(&f, id, c, wt_selected);
	Sf_ScorerSet(&f, id, c, "Hint", WT_WeaponHint(&f, "HINT_Weapons2", 0, control));
	Sf_ScorerNum(&f, id, c, "MaxHintLineWidth", 550);
	Sf_ScorerDefault(&f, id, c);
	WT_Color(&f, id, WT_Join(&f, name, sizeof(name), control, "Tile"), wt_selected);
	WT_Color(&f, id, WT_Join(&f, name, sizeof(name), control, "Icon"), wt_white);
	WT_Join(&f, name, sizeof(name), control, "Rect");
	Sf_ScorerNum(&f, id, name, "Alpha", 255);
	WT_Color(&f, id, name, wt_selected);
	PyObject *unlinkUD = Sf_From(&f, "Menu", "UnLinkUD");
	for (int i = 0; i < 6; i++) {
		Sf_Line(&f, unlinkUD, "(iss)", id, wt_unlink[i], "Exit");
	}
	Sf_ScorerStr(&f, id, "ShipEditTabWeapons", "Down", c);
	Sf_ScorerStr(&f, id, c, "Up", "ShipEditTabWeapons");
	Sf_Own(&f, MOD, "FillSlots", "(iO)", id, control);
	return Sf_None(&f);
}

/* HangarTab.prevShip.Upgrd<weapId[prevWeapon]> = <upgradeId[auxUpgradeId2[prevWeapon + "Upgrade" + k]]> and its
 * UAmmo = 200, as the exec lines of the script */
static void WT_PutUpgrade(scriptFrame_t *f, PyObject *k) {
	PyObject *upg = Sf_Cat(f, "OsO", WT_Global(f, "prevWeapon"), "Upgrade", k);
	upg = Sf_GetItem(f, WT_Global(f, "auxUpgradeId2"), upg);
	PyObject *wid = Sf_GetItem(f, WT_Global(f, "weapId"), WT_Global(f, "prevWeapon"));
	WT_Exec(f, Sf_Cat(f, "sOsO", "HangarTab.prevShip.Upgrd", wid, " = ", Sf_GetItem(f, WT_Global(f, "upgradeId"), upg)));
	wid = Sf_GetItem(f, WT_Global(f, "weapId"), WT_Global(f, "prevWeapon"));
	WT_Exec(f, Sf_Cat(f, "sOs", "HangarTab.prevShip.UAmmo", wid, " = 200"));
}

/* SelectUpgrade(id, control): the upgrade of the tile put on the weapon, swapped for the other one or taken off */
static PyObject *WT_SelectUpgrade(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "string");
	Sf_Import(&f, "Sound");
	PyObject *upgrade = WT_ShipS(&f, "Upgrd", Sf_GetItem(&f, WT_Global(&f, "weapId"), WT_Global(&f, "prevWeapon")));
	PyObject *zero = Sf_Int(&f, 0);
	const char *name = "";
	PyObject *un = Sf_Str(&f, ""), *pn = un;
	int pos = Sf_Compare(&f, upgrade, ">", zero), odd = 0, even = 0;
	if (pos > 0) {
		odd = Sf_Compare(&f, Sf_OpI(&f, upgrade, '%', 2), "!=", zero);
		if (odd == 0 && Sf_Compare(&f, upgrade, ">", zero) > 0) {
			even = Sf_EqI(&f, Sf_OpI(&f, upgrade, '%', 2), 0);
		}
	}
	if (odd > 0) {
		name = "Upgrade1Tile";
		WT_FindUpgrades(&f, Sf_Repr(&f, upgrade), Sf_Repr(&f, Sf_OpI(&f, upgrade, '+', 1)), &un, &pn);
	} else if (even > 0) {
		name = "Upgrade2Tile";
		WT_FindUpgrades(&f, Sf_Repr(&f, upgrade), Sf_Repr(&f, Sf_OpI(&f, upgrade, '-', 1)), &un, &pn);
	}
	int len = Sf_Len(&f, control);
	if (name[0]) {
		const char *c = Sf_Text(&f, control);
		int same = Sf_EqS(&f, control, name);
		if (same > 0) {
			PyObject *wid = Sf_GetItem(&f, WT_Global(&f, "weapId"), WT_Global(&f, "prevWeapon"));
			WT_Exec(&f, Sf_Cat(&f, "sOs", "HangarTab.prevShip.Upgrd", wid, " = 0"));
			Sf_ScorerDefault(&f, id, c);
			WT_Sound(&f, "ShipEditUnLoad");
		} else if (same == 0) {
			Sf_ScorerDefault(&f, id, c);
			WT_PutUpgrade(&f, len < 0 ? NULL : Sf_Item(&f, control, len - 5));
			WT_Sound(&f, "ShipEditLoadWeapon");
		}
	} else if (!f.failed) {
		WT_PutUpgrade(&f, len < 0 ? NULL : Sf_Item(&f, control, len - 5));
		PyObject *up = Sf_GetItem(&f, WT_Global(&f, "auxUpgradeId"), WT_Global(&f, "prevWeapon"));
		if (Sf_EqS(&f, Sf_Item(&f, control, len - 5), "2") > 0) {
			up = Sf_Repr(&f, Sf_OpI(&f, Sf_ToInt(&f, up), '+', 1));
		}
		PyObject *keys = Sf_Method(&f, WT_Global(&f, "upgradeId"), "keys", "()");
		for (int i = 0; !f.failed; i++) {
			PyObject *n = Sf_Next(&f, keys, i);
			if (!n) {
				break;
			}
			if (Sf_Compare(&f, Sf_GetItem(&f, WT_Global(&f, "upgradeId"), n), "==", up) > 0) {
				un = n;
				break;
			}
		}
		const char *c = Sf_Text(&f, control);
		Sf_ScorerSet(&f, id, c, "Hint", WT_UpgradeHint(&f, "HINT_Weapons7", un));
		Sf_ScorerNum(&f, id, c, "MaxHintLineWidth", 550);
		Sf_ScorerDefault(&f, id, c);
		WT_Sound(&f, "ShipEditLoadWeapon");
	}
	Sf_Own(&f, MOD, "UpdateUpgrades", "(iO)", id, WT_Global(&f, "prevWeapon"));
	return Sf_None(&f);
}

/* SetWeaponsPanel(id): the items of the tab, hidden */
static PyObject *WT_SetWeaponsPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "string");
	PyObject *vars = Sf_Keep(&f, f.failed ? NULL : PyDict_New());
	PyObject *flags = Sf_Ask(&f, "Scrap", "GetNetFlags", "()");
	int single = Sf_Compare(&f, flags, "==", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_Check(&f, single < 0 || PyDict_SetItemString(vars, "single", Sf_Int(&f, single)) == 0);
	WT_Run(&f, id, "panel", vars);
	PyObject *keys = Sf_Method(&f, WT_Global(&f, "auxUpgradeId"), "keys", "()");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *w = Sf_Next(&f, keys, i);
		if (!w) {
			break;
		}
		for (int j = 0; j < 6 && !f.failed; j++) {
			PyObject *name = Sf_Cat(&f, "OsO", w, "Slot", Sf_ReprI(&f, j));
			Sf_Text(&f, name);
			WT_Run(&f, id, "slot", Sf_Build(&f, "{s:O}", "name", name));
		}
		Sf_Release(&f, mark);
	}
	int multi = Sf_True(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()"));
	WT_Run(&f, id, "boxes", Sf_Build(&f, "{s:i}", "multi", multi));
	return Sf_None(&f);
}

/* ShowWeaponsPanel(id): the six weapons, those the player does not have dark; the ones he has linked */
static PyObject *WT_ShowWeaponsPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	char name[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *linkLR = Sf_From(&f, "Menu", "LinkLR");
	PyObject *linkUD = Sf_From(&f, "Menu", "LinkUD");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "string");
	Sf_SetGlobal(&f, MOD, "controlsToLinkList", Sf_Keep(&f, f.failed ? NULL : PyList_New(0)));
	for (int i = 0; i < 6 && !f.failed; i++) {
		int mark = f.n;
		const char *w = wt_shown[i];
		WT_Visible(&f, id, w, 1);
		Sf_ScorerNum(&f, id, w, "Alpha", 255);
		WT_Color(&f, id, w, wt_normal);
		Str_Printf(name, sizeof(name), "%sTile", w);
		WT_Visible(&f, id, name, 1);
		WT_Color(&f, id, name, wt_normal);
		Str_Printf(name, sizeof(name), "%sIcon", w);
		WT_Visible(&f, id, name, 1);
		WT_Color(&f, id, name, wt_grey);
		Str_Printf(name, sizeof(name), "%sRect", w);
		WT_Visible(&f, id, name, 1);
		Sf_ScorerNum(&f, id, name, "Alpha", 255);
		WT_Color(&f, id, name, wt_normal);
		int have = Sf_In(&f, Sf_Str(&f, w), Sf_Mod(&f, "ShipEdit", "availableWeaponsList"));
		if (have == 0) {
			Sf_ScorerStr(&f, id, w, "OnAccept", "");
			Sf_ScorerStr(&f, id, w, "OnGainFocus", "");
			Sf_ScorerStr(&f, id, w, "OnLooseFocus", "");
			Sf_ScorerNum(&f, id, w, "W", 0);
			Sf_ScorerNum(&f, id, w, "H", 0);
			Sf_ScorerSet(&f, id, w, "Hint", WT_Lang(&f, "HINT_Weapons8"));
			Sf_ScorerNum(&f, id, w, "MaxHintLineWidth", 550);
			Str_Printf(name, sizeof(name), "%sTile", w);
			Sf_ScorerNum(&f, id, name, "Alpha", 222);
			WT_Color(&f, id, name, wt_missing);
			Str_Printf(name, sizeof(name), "%sIcon", w);
			Sf_ScorerStr(&f, id, name, "File", "2D/ShipEditor/FullEditor.alpha.tga");
			Sf_ScorerNum(&f, id, name, "SpriteIndex", 32);
			Sf_ScorerNum(&f, id, name, "Alpha", 255);
			WT_Color(&f, id, name, wt_grey);
		} else if (have > 0) {
			Sf_Method(&f, WT_Global(&f, "controlsToLinkList"), "append", "(s)", w);
		}
		Sf_Release(&f, mark);
	}
	int last = Sf_Len(&f, WT_Global(&f, "controlsToLinkList")) - 1;
	for (int i = 0; i < last && !f.failed; i++) {
		int mark = f.n;
		PyObject *a = Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), i);
		PyObject *b = Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), i + 1);
		Sf_Line(&f, linkLR, "(iOO)", id, a, b);
		Sf_Line(&f, linkUD, "(iOs)", id, Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), i), "Exit");
		Sf_Release(&f, mark);
	}
	PyObject *a = Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), last);
	Sf_Line(&f, linkLR, "(iOO)", id, a, Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), 0));
	Sf_Line(&f, linkUD, "(iOs)", id, Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), last), "Exit");
	Sf_Line(&f, linkUD, "(iOs)", id, Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), 0), "Exit");
	return Sf_None(&f);
}

/* HideWeaponsPanel(id) */
static PyObject *WT_HideWeaponsPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	char name[0x40];
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_SetGlobal(&f, MOD, "prevWeapon", Sf_Str(&f, ""));
	PyObject *keys = Sf_Method(&f, WT_Global(&f, "weapId"), "keys", "()");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *w = Sf_Next(&f, keys, i);
		if (!w) {
			break;
		}
		static const char *const parts[4] = { "", "Tile", "Icon", "Rect" };
		for (int k = 0; k < 4; k++) {
			WT_Visible(&f, id, WT_Join(&f, name, sizeof(name), w, parts[k]), 0);
		}
		Sf_Release(&f, mark);
	}
	WT_Visible(&f, id, "SlotsBox", 0);
	WT_Visible(&f, id, "UpgradesBox", 0);
	WT_Visible(&f, id, "SlotsAndUpgradesBox", 0);
	for (int i = 0; i < 6; i++) {
		Sf_Own(&f, MOD, "HideSlots", "(is)", id, wt_linked[i]);
	}
	PyObject *unlinkUD = Sf_From(&f, "Menu", "UnLinkUD");
	PyObject *unlinkLR = Sf_From(&f, "Menu", "UnLinkLR");
	for (int i = 0; i < 6; i++) {
		Sf_Line(&f, unlinkUD, "(iss)", id, wt_unlink[i], "Exit");
	}
	int last = Sf_Len(&f, WT_Global(&f, "controlsToLinkList"));
	if (last > 0) {
		Sf_Line(&f, unlinkLR, "(iOs)", id, Sf_Item(&f, WT_Global(&f, "controlsToLinkList"), last - 1), "Exit");
	}
	return Sf_None(&f);
}

PyMethodDef g_WeaponsTabMethods[] = {
	{ "DoNothing", WT_DoNothing, 1, NULL },
	{ "ShowWeightPrediction", WT_ShowWeightPrediction, 1, NULL },
	{ "HideWeightPrediction", WT_HideWeightPrediction, 1, NULL },
	{ "ShowWeaponDesc", WT_ShowWeaponDesc, 1, NULL },
	{ "HideWeaponDesc", WT_HideWeaponDesc, 1, NULL },
	{ "ShowWeaponUpgradeDesc", WT_ShowWeaponUpgradeDesc, 1, NULL },
	{ "HideWeaponUpgradeDesc", WT_HideWeaponUpgradeDesc, 1, NULL },
	{ "ShowSlotMarker", WT_ShowSlotMarker, 1, NULL },
	{ "HideSlotMarker", WT_HideSlotMarker, 1, NULL },
	{ "LoadWeapon", WT_LoadWeapon, 1, NULL },
	{ "HideSlots", WT_HideSlots, 1, NULL },
	{ "FillSlots", WT_FillSlots, 1, NULL },
	{ "UpdateUpgrades", WT_UpdateUpgrades, 1, NULL },
	{ "SelectWeapon", WT_SelectWeapon, 1, NULL },
	{ "SelectUpgrade", WT_SelectUpgrade, 1, NULL },
	{ "SetWeaponsPanel", WT_SetWeaponsPanel, 1, NULL },
	{ "ShowWeaponsPanel", WT_ShowWeaponsPanel, 1, NULL },
	{ "HideWeaponsPanel", WT_HideWeaponsPanel, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
