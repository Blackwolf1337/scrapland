/*
 * The EnginesTab module in C (scripts/shipedit/enginestab.py, issue #2): the engines tab of the ship editor. The
 * player picks an engine bay of the ship (a group of one or two slots), sees the engines of the list (four at a time,
 * red when the ship would get too heavy) and buys one for every slot of the bay or recycles the mounted ones. The
 * layout is data/menus/enginestab.def. enginesList (mass and name, by mass), canBeMounted (2 can be mounted, 0 not
 * now, 1 never; SetEnginesPanel appends to it without clearing it, like the script), engineIndex (the first engine
 * shown), currentEngineGroup (the bay), nEngSlots, the colours and UseTxt stay variables of the module. ShipEdit,
 * HangarTab, ShipMaker and Sound are used through their modules.
 */
#include "scrap/global.h"
#include "scrap/script.h"

static const char enginestab_def[] = {
#embed "../../../data/menus/enginestab.def"
	, 0 };

#define ENGINESTAB_FILE "menus/enginestab.def"
#define MOD "EnginesTab"
#define ENGINES_Y 125			/* EnginesTab.EnginesYOffset */

/* EnginesTab.NormalColor*, SelectedColor*, ForbiddenColor* (red, green, blue) */
static const int et_normal[3] = { 115, 150, 255 };
static const int et_selected[3] = { 250, 230, 90 };
static const int et_forbidden[3] = { 200, 30, 30 };

static PyObject *ET_Hint(scriptFrame_t *f, const char *key);

/* the step usehint <key>: the Hint of the item, Scrap.GetLangStr(key) % UseTxt (the error left set stops the file) */
static void ET_StepHint(int id, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	Sf_ScorerSet(&f, id, Script_CurrentItem(), "Hint", ET_Hint(&f, Sf_Text(&f, Sf_Item(&f, args, 0))));
	Sf_Done(&f, NULL);
}

static const scriptStep_t enginestab_steps[] = {
	{ "usehint", ET_StepHint },
	{ NULL, NULL },
};

static void ET_Run(scriptFrame_t *f, int id, const char *section, PyObject *vars) {
	if (!f->failed) {
		Sf_Check(f, Script_RunFileTry(id, ENGINESTAB_FILE, section, enginestab_def, enginestab_steps, vars));
	}
}

/* a value given to the file and the section run */
static void ET_RunV(scriptFrame_t *f, int id, const char *section, PyObject *v) {
	ET_Run(f, id, section, Sf_Build(f, "{s:O}", "v", v));
}

static PyObject *ET_Global(scriptFrame_t *f, const char *name) {
	return Sf_Global(f, MOD, name);
}

/* HangarTab.prevShip.<var>, and the variable named prefix + `n` */
static PyObject *ET_Ship(scriptFrame_t *f, const char *var) {
	return Sf_Attr(f, Sf_Mod(f, "HangarTab", "prevShip"), var);
}

static PyObject *ET_ShipN(scriptFrame_t *f, const char *prefix, PyObject *n) {
	char var[0x40];
	const char *s = Sf_Text(f, Sf_Repr(f, n));
	if (!s) {
		return NULL;
	}
	Str_Printf(var, sizeof(var), "%s%s", prefix, s);
	return ET_Ship(f, var);
}

static PyObject *ET_Upper(scriptFrame_t *f, PyObject *o) {
	return Sf_Ask(f, "string", "upper", "(O)", o);
}

static PyObject *ET_Lang(scriptFrame_t *f, const char *key) {
	return Sf_Lang(f, Sf_Str(f, key));
}

/* Scrap.GetLangStr(key) % UseTxt */
static PyObject *ET_Hint(scriptFrame_t *f, const char *key) {
	return key ? Sf_Op(f, ET_Lang(f, key), '%', ET_Global(f, "UseTxt")) : NULL;
}

/* ShipMaker.<table>[string.upper(HangarTab.prevShip.Model)][i] */
static PyObject *ET_ShipTable(scriptFrame_t *f, const char *table, int i) {
	PyObject *model = ET_Upper(f, ET_Ship(f, "Model"));
	return Sf_Item(f, Sf_GetItem(f, Sf_Mod(f, "ShipMaker", table), model), i);
}

/* enginesList[i][k] */
static PyObject *ET_Engine(scriptFrame_t *f, PyObject *i, int k) {
	return Sf_Item(f, Sf_GetItem(f, ET_Global(f, "enginesList"), i), k);
}

/* int(currentEngineGroup[len(currentEngineGroup) - 1]) - 1: the bay */
static PyObject *ET_Bay(scriptFrame_t *f) {
	int len = Sf_Len(f, ET_Global(f, "currentEngineGroup"));
	PyObject *last = len < 0 ? NULL : Sf_Item(f, ET_Global(f, "currentEngineGroup"), len - 1);
	return Sf_OpI(f, Sf_ToInt(f, last), '-', 1);
}

/* the C text of currentEngineGroup, an item name */
static const char *ET_Group(scriptFrame_t *f) {
	return Sf_Text(f, ET_Global(f, "currentEngineGroup"));
}

static void ET_Color(scriptFrame_t *f, int id, const char *item, const int *c) {
	Sf_ScorerNum(f, id, item, "Red", c[0]);
	Sf_ScorerNum(f, id, item, "Green", c[1]);
	Sf_ScorerNum(f, id, item, "Blue", c[2]);
}

static void ET_Visible(scriptFrame_t *f, int id, const char *item, int v) {
	Sf_ScorerNum(f, id, item, "Visible", v);
}

/* the C text of `n` + suffix into out */
static const char *ET_Name(scriptFrame_t *f, char *out, int size, const char *prefix, PyObject *n, const char *suffix) {
	const char *s = Sf_Text(f, Sf_Repr(f, n));
	if (!s) {
		return NULL;
	}
	Str_Printf(out, size, "%s%s%s", prefix, s, suffix);
	return out;
}

/* the bay loads of the current bay, as the script reads them: mass and number of the engines mounted (0 for none),
 * the slots and the bay number */
typedef struct {
	PyObject *weight, *maxWeight, *mass, *num, *slots, *minMass;
} etLoad_t;

/* the unused layout = ShipEdit.shipsEnginesLayouts[...] of the scripts is read before the bay (layoutFirst, the
 * scroll functions) or after it (UpdateEnginesList) */
static void ET_Load(scriptFrame_t *f, etLoad_t *l, byte layoutFirst) {
	l->weight = Sf_ToInt(f, ET_Ship(f, "AcMass"));
	l->maxWeight = Sf_ToInt(f, ET_ShipTable(f, "ShipTon", 2));
	if (layoutFirst) {
		Sf_GetItem(f, Sf_Mod(f, "ShipEdit", "shipsEnginesLayouts"), ET_Ship(f, "Model"));
	}
	PyObject *bay = ET_Bay(f);
	if (!layoutFirst) {
		Sf_GetItem(f, Sf_Mod(f, "ShipEdit", "shipsEnginesLayouts"), ET_Ship(f, "Model"));
	}
	l->mass = l->num = Sf_Int(f, 0);
	l->slots = ET_ShipN(f, "nEng", bay);
	PyObject *key = ET_ShipN(f, "MEng", bay);
	if (Sf_Compare(f, key, "!=", Sf_Str(f, "")) > 0) {
		l->mass = Sf_Item(f, Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "MotorValues"), ET_Upper(f, key)), 0);
		l->num = ET_ShipN(f, "NEng", bay);
	}
	l->minMass = ET_ShipTable(f, "ShipValues", 0);
}

/*
 * engine i of the list against the load: red when weight - num * mass + slots * enginesList[i][0] > maxWeight (with
 * canBeMounted[i] 0 when an empty ship could take it in the bay, minMass + slots * mass <= maxWeight, else 1), else
 * the normal colour and 2; the hint of the tile Engine<i+1>. layout: ShowEnginesPanel's form, num and slots are
 * layout[0][4] and the second test reads HangarTab.prevShip.nEng0
 */
static void ET_Mount(scriptFrame_t *f, int id, long i, etLoad_t *l, PyObject *layout) {
	char name[0x40], tile[0x40];
	PyObject *idx = Sf_Int(f, i);
	Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
	Str_Printf(tile, sizeof(tile), "Engine%ld", i + 1);
	PyObject *num = layout ? Sf_Item(f, Sf_Item(f, layout, 0), 4) : l->num;
	PyObject *w = Sf_Op(f, l->weight, '-', Sf_Op(f, num, '*', l->mass));
	PyObject *slots = layout ? Sf_Item(f, Sf_Item(f, layout, 0), 4) : l->slots;
	w = Sf_Op(f, w, '+', Sf_Op(f, slots, '*', ET_Engine(f, idx, 0)));
	int heavy = Sf_Compare(f, w, ">", l->maxWeight);
	if (heavy > 0) {
		PyObject *slots2 = layout ? ET_Ship(f, "nEng0") : l->slots;
		PyObject *empty = Sf_Op(f, l->minMass, '+', Sf_Op(f, slots2, '*', ET_Engine(f, idx, 0)));
		int fits = Sf_Compare(f, empty, "<=", l->maxWeight);
		if (fits >= 0) {
			Sf_SetItem(f, ET_Global(f, "canBeMounted"), idx, Sf_Int(f, fits ? 0 : 1));
			Sf_ScorerSet(f, id, tile, "Hint", ET_Lang(f, fits ? "HINT_Engines4" : "HINT_Engines5"));
		}
		Sf_ScorerNum(f, id, name, "Alpha", 255);
		ET_Color(f, id, name, et_forbidden);
	} else if (heavy == 0) {
		Sf_ScorerNum(f, id, name, "Alpha", 255);
		ET_Color(f, id, name, et_normal);
		Sf_SetItem(f, ET_Global(f, "canBeMounted"), idx, Sf_Int(f, 2));
		Sf_ScorerSet(f, id, tile, "Hint", ET_Hint(f, "HINT_Engines2"));
	}
}

/* the hint of a bay: HINT_Engines6 for a bay without slots, else HINT_Engines<6 + slots> */
static void ET_BayHint(scriptFrame_t *f, int id, const char *item, PyObject *slots) {
	if (Sf_EqI(f, slots, 0) > 0) {
		Sf_ScorerSet(f, id, item, "Hint", ET_Hint(f, "HINT_Engines6"));
	} else if (!f->failed) {
		PyObject *key = Sf_Cat(f, "sO", "HINT_Engines", Sf_Repr(f, Sf_Op(f, Sf_Int(f, 6), '+', slots)));
		Sf_ScorerSet(f, id, item, "Hint", Sf_Lang(f, key));
	}
}

/* the range(a, b) limits of a script, with the TypeError of range */
static byte ET_Range(scriptFrame_t *f, PyObject *a, long *out) {
	return Sf_Check(f, !f->failed && Script_Range(a, out));
}

/* HighlightCurrentMountedEngine(id, setDefault=1): the tile of the engine mounted in the bay in the selected colour */
static PyObject *ET_HighlightCurrentMountedEngine(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "setDefault", NULL };
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *setDefault = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "i|O", names, &id, &setDefault)) {
		return NULL;
	}
	if (!setDefault) {
		setDefault = Sf_Int(&f, 1);
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "string");
	int len = Sf_Len(&f, ET_Global(&f, "currentEngineGroup"));
	PyObject *number = len == 0 ? Sf_Str(&f, "1") : Sf_Item(&f, ET_Global(&f, "currentEngineGroup"), len - 1);
	PyObject *engineName = ET_ShipN(&f, "MEng", Sf_OpI(&f, Sf_ToInt(&f, number), '-', 1));
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	PyObject *pos = Sf_Int(&f, 0);
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		pos = Sf_Int(&f, i);
		PyObject *a = ET_Upper(&f, engineName);
		int same = Sf_Compare(&f, a, "==", ET_Upper(&f, ET_Engine(&f, pos, 1)));
		Sf_ReleaseKeep(&f, mark, &pos, 1);
		if (same > 0) {
			break;
		}
	}
	if (count > 4) {
		count = 4;
	}
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		char name[0x20], name2[0x40];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		int sel = Sf_Compare(&f, Sf_Op(&f, Sf_Int(&f, i), '+', ET_Global(&f, "engineIndex")), "==", pos);
		if (sel > 0) {
			sel = Sf_Compare(&f, engineName, "!=", Sf_Str(&f, ""));
		}
		if (sel > 0) {
			Sf_ScorerSet(&f, id, name, "Hint", ET_Lang(&f, "HINT_Engines3"));
			Sf_ScorerNum(&f, id, name, "Alpha", 255);
			ET_Color(&f, id, name, et_selected);
			PyObject *n2 = Sf_Op(&f, Sf_Int(&f, i + 1), '+', ET_Global(&f, "engineIndex"));
			if (ET_Name(&f, name2, sizeof(name2), "Engine", n2, "InList")) {
				Sf_ScorerNum(&f, id, name2, "Alpha", 255);
				ET_Color(&f, id, name2, et_selected);
			}
			if (Sf_EqI(&f, setDefault, 1) > 0) {
				Sf_ScorerDefault(&f, id, name);
			}
		} else if (sel == 0) {
			Sf_ScorerNum(&f, id, name, "Alpha", 255);
			ET_Color(&f, id, name, et_normal);
			if (Sf_EqI(&f, Sf_Item(&f, ET_Global(&f, "canBeMounted"), i), 2) > 0) {
				Sf_ScorerSet(&f, id, name, "Hint", ET_Hint(&f, "HINT_Engines2"));
			}
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ShowEngineDesc(id, control, lostfocus): the description of the engine of the tile, and the money, mass, boost and
 * speed predictions for the bay */
static PyObject *ET_ShowEngineDesc(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	int len = Sf_Len(&f, control);
	PyObject *n = Sf_ToInt(&f, len < 0 ? NULL : Sf_Item(&f, control, len - 1));
	PyObject *eng = Sf_OpI(&f, Sf_Op(&f, n, '+', ET_Global(&f, "engineIndex")), '-', 1);
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	int inList = count < 0 ? -1 : Sf_Compare(&f, eng, "<", Sf_Int(&f, count));
	if (inList > 0) {
		char tile[0x40];
		PyObject *can = Sf_GetItem(&f, ET_Global(&f, "canBeMounted"), eng);
		int c0 = Sf_EqI(&f, can, 0), c1 = c0 == 0 ? Sf_EqI(&f, can, 1) : 0;
		if ((c0 > 0 || c1 > 0) && ET_Name(&f, tile, sizeof(tile), "Engine", n, "")) {
			Sf_ScorerSet(&f, id, tile, "Hint", ET_Lang(&f, c0 > 0 ? "HINT_Engines4" : "HINT_Engines5"));
		}
		PyObject *engineName = ET_Engine(&f, eng, 1);
		PyObject *motors = Sf_Mod(&f, "ShipMaker", "MotorValues");
		PyObject *t1 = ET_Lang(&f, "ShipEdit_Engine");
		PyObject *t2 = Sf_Lang(&f, Sf_Cat(&f, "sOs", "ShipEdit_", engineName, "Name"));
		PyObject *t3 = ET_Lang(&f, "ShipEdit_Power");
		PyObject *power = Sf_Repr(&f, Sf_Item(&f, Sf_GetItem(&f, motors, engineName), 2));
		PyObject *t4 = ET_Lang(&f, "ShipEdit_PowerUnits");
		PyObject *t5 = ET_Lang(&f, "ShipEdit_Weight");
		PyObject *weight = Sf_Repr(&f, Sf_Item(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "MotorValues"), engineName), 0));
		PyObject *t6 = ET_Lang(&f, "ShipEdit_Cost");
		PyObject *price = Sf_Repr(&f, Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", engineName));
		PyObject *desc = Sf_Cat(&f, "OsOsOsOsOsOsOsOsO", t1, ": ", t2, "\n", t3, ": ", power, " ", t4, "\n", t5, ": ", weight,
			"\n", t6, ": ", price);
		Sf_ScorerSet(&f, id, "EngineDesc", "Text", desc);
		Sf_ScorerNum(&f, id, "EngineDesc", "XAlignRight", 580);
		double tab = 40 + Sf_ScorerGet(&f, id, "EngineDesc", "SizeX");
		Sf_ScorerNum(&f, id, "EngineDesc", "MinX", 590 - tab + 10);
		Sf_ScorerNum(&f, id, "EngineDescTab", "SizeX", tab - 2);
		Sf_ScorerNum(&f, id, "EngineDescTab", "X", 590 - tab);
		ET_Visible(&f, id, "EngineDesc", 1);
		ET_Visible(&f, id, "EngineDescTab", 1);
	} else if (inList == 0) {
		ET_Visible(&f, id, "EngineDesc", 0);
		ET_Visible(&f, id, "EngineDescTab", 0);
	}
	int l = Sf_Len(&f, ET_Global(&f, "currentEngineGroup"));
	if (l == 0 || f.failed) {
		return Sf_None(&f);
	}
	PyObject *bay = ET_Bay(&f);
	PyObject *engineName = ET_Upper(&f, ET_ShipN(&f, "MEng", bay));
	if (Sf_Compare(&f, engineName, "!=", ET_Engine(&f, eng, 1)) <= 0) {
		return Sf_None(&f);
	}
	PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
	PyObject *slots = ET_ShipN(&f, "nEng", bay);
	PyObject *cost = Sf_Op(&f, slots, '*', Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", ET_Engine(&f, eng, 1)));
	if (Sf_Compare(&f, engineName, "!=", Sf_Str(&f, "")) > 0) {
		cost = Sf_Op(&f, cost, '-', Sf_Op(&f, slots, '*', Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", engineName)));
	}
	Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 255);
	if (Sf_Compare(&f, Sf_Op(&f, money, '-', cost), "<", Sf_Int(&f, 0)) > 0) {
		Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 0);
	}
	int neg = Sf_Compare(&f, cost, "<", Sf_Int(&f, 0));
	if (neg > 0) {
		PyObject *a = Sf_Keep(&f, f.failed ? NULL : PyNumber_Absolute(cost));
		Sf_ScorerSet(&f, id, "MoneyPredict", "Text", Sf_Cat(&f, "sO", "+", Sf_Repr(&f, a)));
	} else if (neg == 0) {
		Sf_ScorerSet(&f, id, "MoneyPredict", "Text", Sf_Cat(&f, "sO", "-", Sf_Repr(&f, cost)));
	}
	ET_Visible(&f, id, "MoneyPredict", 1);
	PyObject *weight = Sf_ToInt(&f, ET_Ship(&f, "AcMass"));
	PyObject *mass = Sf_Int(&f, 0), *num = mass;
	PyObject *groupSlots = ET_ShipN(&f, "nEng", bay);
	PyObject *key = ET_ShipN(&f, "MEng", bay);
	if (Sf_Compare(&f, key, "!=", Sf_Str(&f, "")) > 0) {
		mass = Sf_Item(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "MotorValues"), ET_Upper(&f, key)), 0);
		num = ET_ShipN(&f, "NEng", bay);
	}
	PyObject *newWeight = Sf_Op(&f, weight, '-', Sf_Op(&f, num, '*', mass));
	newWeight = Sf_Op(&f, newWeight, '+', Sf_Op(&f, groupSlots, '*', ET_Engine(&f, eng, 0)));
	int up = Sf_Compare(&f, newWeight, ">", weight), down = up == 0 ? Sf_Compare(&f, newWeight, "<", weight) : 0;
	ET_Visible(&f, id, up > 0 ? "WeightUp" : down > 0 ? "WeightDown" : "WeightEqual", 1);
	ET_Visible(&f, id, "WeightBack", 1);
	Sf_Ask(&f, "HangarTab", "ShowBoostPrediction", "(iO)", id, newWeight);
	PyObject *cur = Sf_Ask(&f, "ShipMaker", "GetMotor", "(O)", ET_ShipN(&f, "MEng", bay));
	PyObject *next = Sf_Ask(&f, "ShipMaker", "GetMotor", "(O)", ET_Engine(&f, eng, 1));
	PyObject *a = Sf_Op(&f, Sf_Item(&f, cur, 1), '+', Sf_Item(&f, cur, 2));
	PyObject *b = Sf_Op(&f, Sf_Item(&f, next, 1), '+', Sf_Item(&f, next, 2));
	int faster = Sf_Compare(&f, a, "<", b), slower = 0;
	if (faster == 0) {
		a = Sf_Op(&f, Sf_Item(&f, cur, 1), '+', Sf_Item(&f, cur, 2));
		b = Sf_Op(&f, Sf_Item(&f, next, 1), '+', Sf_Item(&f, next, 2));
		slower = Sf_Compare(&f, a, ">", b);
	}
	ET_Visible(&f, id, faster > 0 ? "SpeedUp" : slower > 0 ? "SpeedDown" : "SpeedEqual", 1);
	ET_Visible(&f, id, "SpeedBack", 1);
	return Sf_None(&f);
}

/* HideEngineDesc(id, control, gainfocus) */
static PyObject *ET_HideEngineDesc(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *gain;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &gain)) {
		return NULL;
	}
	ET_Run(&f, id, "hidedesc", NULL);
	return Sf_None(&f);
}

/* ChangeRecycleButtonState(id): the recycle button works only for a bay with engines; the links follow */
static PyObject *ET_ChangeRecycleButtonState(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *linkLR = Sf_From(&f, "Menu", "LinkLR");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipEdit");
	int l = Sf_Len(&f, ET_Global(&f, "currentEngineGroup"));
	if (l <= 0) {
		return Sf_None(&f);
	}
	PyObject *bay = ET_Bay(&f);
	int has = 0;
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_EqI(&f, bay, i) > 0) {
			char var[0x10];
			Str_Printf(var, sizeof(var), "NEng%d", i);
			has = Sf_Compare(&f, ET_Ship(&f, var), ">", Sf_Int(&f, 0)) > 0;
			break;
		}
	}
	int groups = Sf_Len(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipEdit", "shipsEnginesLayouts"), ET_Ship(&f, "Model")));
	const char *to = has ? "Recycle" : "Exit";
	if (has) {
		Sf_ScorerNum(&f, id, "Recycle", "Highlight", 1);
		Sf_ScorerNum(&f, id, "Recycle", "SpriteIndex", 8);
		Sf_ScorerStr(&f, id, "Recycle", "OnAccept", "EnginesTab.RecycleEngine");
	} else {
		Sf_ScorerNum(&f, id, "Recycle", "Highlight", 0);
		Sf_ScorerNum(&f, id, "Recycle", "SpriteIndex", 10);
		Sf_ScorerStr(&f, id, "Recycle", "OnAccept", "");
	}
	/* the bays 2 .. n first, then bay 1 */
	for (int i = 0; i < groups; i++) {
		int mark = f.n;
		PyObject *name = Sf_Cat(&f, "OsO", ET_Ship(&f, "Model"), "EngineGroup", Sf_ReprI(&f, i < groups - 1 ? i + 2 : 1));
		Sf_Line(&f, linkLR, "(iOs)", id, name, to);
		Sf_Release(&f, mark);
	}
	if (groups == 0) {
		Sf_Line(&f, linkLR, "(iOs)", id, Sf_Cat(&f, "Os", ET_Ship(&f, "Model"), "EngineGroup1"), to);
	}
	if (has) {
		Sf_Line(&f, linkLR, "(iss)", id, "Recycle", "Exit");
	}
	return Sf_None(&f);
}

/* UpdateEnginesList(id): which engines of the list fit the bay */
static PyObject *ET_UpdateEnginesList(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	etLoad_t l;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	Sf_Import(&f, "HangarTab");
	ET_Load(&f, &l, 0);
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		ET_Mount(&f, id, i, &l, NULL);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* the speed texts of the info panel */
static void ET_SpeedText(scriptFrame_t *f, int id, int v) {
	static const char *const items[] = { "MaxSpeedText", "MaxSpeed", "BoostRechargeTime", "BoostRechargeTimeUnits",
		"BoostRechargeTimeText" };
	for (int i = 0; i < 5; i++) {
		ET_Visible(f, id, items[i], v);
	}
}

/* the new state of the ship: SetShipParams, the info panel, the list, the recycle button and the tiles */
static void ET_Refresh(scriptFrame_t *f, int id) {
	Sf_Ask(f, "ShipMaker", "SetShipParams", "(O)", ET_Ship(f, "Name"));
	Sf_Ask(f, "HangarTab", "ShowShipInfo", "(i)", id);
	Sf_Own(f, MOD, "UpdateEnginesList", "(i)", id);
	Sf_Own(f, MOD, "ChangeRecycleButtonState", "(i)", id);
}

/* PutEngine(id, control): the engine of the tile into every slot of the bay, paid for (less the mounted ones) */
static PyObject *ET_PutEngine(PyObject *self, PyObject *args) {
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
	int l = Sf_Len(&f, ET_Global(&f, "currentEngineGroup"));
	if (l <= 0) {
		return Sf_None(&f);
	}
	int len = Sf_Len(&f, control);
	PyObject *inList = Sf_ToInt(&f, len < 0 ? NULL : Sf_Item(&f, control, len - 1));
	inList = Sf_Op(&f, Sf_OpI(&f, inList, '-', 1), '+', ET_Global(&f, "engineIndex"));
	if (Sf_Compare(&f, Sf_GetItem(&f, ET_Global(&f, "canBeMounted"), inList), "!=", Sf_Int(&f, 2)) != 0) {
		return Sf_None(&f);
	}
	PyObject *last = Sf_Item(&f, ET_Global(&f, "currentEngineGroup"), l - 1);
	PyObject *bay = Sf_OpI(&f, Sf_ToInt(&f, last), '-', 1);
	if (Sf_Compare(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()"), "!=", Sf_Int(&f, 1)) > 0) {
		len = Sf_Len(&f, control);
		PyObject *n = Sf_ToInt(&f, len < 0 ? NULL : Sf_Item(&f, control, len - 1));
		PyObject *eng = Sf_OpI(&f, Sf_Op(&f, n, '+', ET_Global(&f, "engineIndex")), '-', 1);
		PyObject *cost = Sf_Op(&f, ET_ShipN(&f, "nEng", bay), '*',
			Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", ET_Engine(&f, eng, 1)));
		PyObject *engineName = ET_Upper(&f, ET_ShipN(&f, "MEng", bay));
		if (Sf_Compare(&f, engineName, "!=", Sf_Str(&f, "")) > 0) {
			PyObject *back = Sf_Op(&f, ET_ShipN(&f, "nEng", bay), '*',
				Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", engineName));
			cost = Sf_Op(&f, cost, '-', back);
		}
		PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
		if (Sf_Compare(&f, Sf_Op(&f, money, '-', cost), "<", Sf_Int(&f, 0)) != 0) {
			return Sf_None(&f);
		}
		PyObject *left = Sf_Op(&f, money, '-', cost);
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", left);
		Sf_ScorerSet(&f, id, "Money", "Text", Sf_Repr(&f, left));
	}
	PyObject *model = Sf_GetItem(&f, Sf_GetItem(&f, ET_Global(&f, "enginesList"), inList), Sf_Int(&f, 1));
	PyObject *prev = ET_Upper(&f, ET_ShipN(&f, "MEng", bay));
	if (Sf_Compare(&f, model, "!=", prev) > 0) {
		PyObject *snd = Sf_Mod(&f, "Sound", "ShipEditLoadEngineSound");
		Sf_Ask(&f, "SSound", "Play", "(OO)", snd, Sf_Mod(&f, "Sound", "ShipEditLoadEngineVolume"));
	}
	int show = 0;
	PyObject *nCEG = Sf_Item(&f, ET_Global(&f, "currentEngineGroup"), l - 1);
	static const char *const digits[] = { "1", "2", "3", "4" };
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_EqS(&f, nCEG, digits[i]) > 0) {
			if (Sf_EqI(&f, Sf_GetItem(&f, ET_Global(&f, "canBeMounted"), inList), 2) > 0) {
				char var[0x10];
				Str_Printf(var, sizeof(var), "MEng%d", i);
				Sf_SetAttr(&f, Sf_Mod(&f, "HangarTab", "prevShip"), var, model);
				show = 1;
			}
			break;
		}
	}
	PyObject *slots = ET_ShipN(&f, "nEng", bay);
	ET_BayHint(&f, id, ET_Group(&f), slots);
	if (show) {
		ET_SpeedText(&f, id, 1);
	}
	ET_Refresh(&f, id);
	int can = Sf_Compare(&f, Sf_GetItem(&f, ET_Global(&f, "canBeMounted"), inList), "!=", Sf_Int(&f, 2));
	if (can > 0) {
		Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(ii)", id, 0);
	} else if (can == 0) {
		Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(i)", id);
	}
	return Sf_None(&f);
}

/* RecycleEngine(id, control): the engines of the bay sold */
static PyObject *ET_RecycleEngine(PyObject *self, PyObject *args) {
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
	Sf_Import(&f, "Sound");
	int len = Sf_Len(&f, ET_Global(&f, "currentEngineGroup"));
	PyObject *number = len == 0 ? Sf_Str(&f, "1") : Sf_Item(&f, ET_Global(&f, "currentEngineGroup"), len - 1);
	PyObject *bay = Sf_OpI(&f, Sf_ToInt(&f, number), '-', 1);
	PyObject *engineName = ET_ShipN(&f, "MEng", bay);
	if (Sf_Compare(&f, Sf_Ask(&f, "ShipEdit", "IsEditingMultiplayer", "()"), "!=", Sf_Int(&f, 1)) > 0) {
		PyObject *cost = Sf_Op(&f, ET_ShipN(&f, "nEng", bay), '*', Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", engineName));
		PyObject *money = Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', cost);
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", money);
		Sf_ScorerSet(&f, id, "Money", "Text", Sf_Repr(&f, money));
	}
	PyObject *snd = Sf_Mod(&f, "Sound", "ShipEditUnLoadSound");
	Sf_Ask(&f, "SSound", "Play", "(OO)", snd, Sf_Mod(&f, "Sound", "ShipEditUnLoadVolume"));
	static const char *const digits[] = { "1", "2", "3", "4" };
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_EqS(&f, number, digits[i]) > 0) {
			char var[0x10];
			Str_Printf(var, sizeof(var), "MEng%d", i);
			if (Sf_Compare(&f, ET_Ship(&f, var), "!=", Sf_Str(&f, "")) > 0) {
				Sf_SetAttrS(&f, Sf_Mod(&f, "HangarTab", "prevShip"), var, "");
			}
			break;
		}
	}
	int none = 1;
	for (int i = 0; i < 4 && none > 0; i++) {
		char var[0x10];
		Str_Printf(var, sizeof(var), "MEng%d", i);
		none = Sf_EqS(&f, ET_Ship(&f, var), "");
	}
	if (none > 0) {
		ET_SpeedText(&f, id, 0);
	}
	ET_Refresh(&f, id);
	Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(i)", id);
	Sf_ScorerDefault(&f, id, Sf_Text(&f, Sf_Cat(&f, "Os", ET_Ship(&f, "Model"), "EngineGroup1")));
	return Sf_None(&f);
}

/* ShowMoneySellingEngine(id, control, lostfocus): the money the recycle button would give */
static PyObject *ET_ShowMoneySellingEngine(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *lost;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	PyObject *bay = ET_Bay(&f);
	PyObject *type = ET_ShipN(&f, "MEng", bay);
	if (Sf_True(&f, type) > 0) {
		PyObject *cost = Sf_Op(&f, ET_ShipN(&f, "nEng", bay), '*', Sf_Ask(&f, "ShipMaker", "GetEnginePrice", "(O)", type));
		Sf_ScorerNum(&f, id, "MoneyPredict", "Green", 255);
		Sf_ScorerSet(&f, id, "MoneyPredict", "Text", Sf_Cat(&f, "sO", "+", Sf_Repr(&f, cost)));
		ET_Visible(&f, id, "MoneyPredict", 1);
	}
	return Sf_None(&f);
}

/* UpdateLayoutsState(id, string.atoi(currentEngineGroup[-1]) - 1, state) */
static void ET_LayoutState(scriptFrame_t *f, int id, int state) {
	PyObject *index = Sf_OpI(f, Sf_Ask(f, "string", "atoi", "(O)", Sf_Item(f, ET_Global(f, "currentEngineGroup"), -1)), '-', 1);
	Sf_Own(f, MOD, "UpdateLayoutsState", "(iOi)", id, index, state);
}

/* SelectEngineGroup(id, control, setDefault=1): the bay picked, in green, and the list checked against it */
static PyObject *ET_SelectEngineGroup(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "control", "setDefault", NULL };
	static const int green[3] = { 0, 255, 0 };
	static const int blue[3] = { 57, 139, 202 };
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *setDefault = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iO|O", names, &id, &control, &setDefault)) {
		return NULL;
	}
	if (!setDefault) {
		setDefault = Sf_Int(&f, 1);
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "string");
	ET_Color(&f, id, ET_Group(&f), blue);
	Sf_ScorerSet(&f, id, ET_Group(&f), "Hint", ET_Hint(&f, "HINT_Engines6"));
	if (Sf_Compare(&f, ET_Global(&f, "currentEngineGroup"), "!=", Sf_Str(&f, "")) > 0) {
		ET_LayoutState(&f, id, 0);
	}
	Sf_SetGlobal(&f, MOD, "currentEngineGroup", control);
	if (Sf_Compare(&f, ET_Global(&f, "currentEngineGroup"), "!=", Sf_Str(&f, "")) > 0) {
		ET_LayoutState(&f, id, 1);
	}
	ET_Color(&f, id, ET_Group(&f), green);
	if (Sf_Compare(&f, ET_Global(&f, "currentEngineGroup"), "!=", Sf_Str(&f, "")) > 0) {
		ET_BayHint(&f, id, ET_Group(&f), ET_ShipN(&f, "nEng", ET_Bay(&f)));
	}
	Sf_Own(&f, MOD, "UpdateEnginesList", "(i)", id);
	Sf_Own(&f, MOD, "ChangeRecycleButtonState", "(i)", id);
	Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(ii)", id, 0);
	if (Sf_True(&f, setDefault) > 0) {
		Sf_ScorerDefault(&f, id, ET_Group(&f));
	}
	return Sf_None(&f);
}

/* SetEnginesPanel(id): the items of the tab, hidden; the engine list from the save game, by mass */
static PyObject *ET_SetEnginesPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipMaker");
	ET_Run(&f, id, "desc", NULL);
	PyObject *aspect = Sf_Ask(&f, "Scrap", "Get", "(s)", "PhysicalAspectRatio");
	/* 1.3333333333333 in the source; the marshal of the shipped .pyc kept 12 digits */
	PyObject *ratio = Sf_Op(&f, Sf_Float(&f, 1.33333333333), '/', aspect);
	int ships = Sf_Len(&f, Sf_Mod(&f, "ShipEdit", "shipsToEditList"));
	for (int k = 0; k < ships && !f.failed; k++) {
		int mark = f.n;
		PyObject *ship = Sf_Item(&f, Sf_Mod(&f, "ShipEdit", "shipsToEditList"), k);
		PyObject *layout = Sf_GetItem(&f, Sf_Mod(&f, "ShipEdit", "shipsEnginesLayouts"), ship);
		int groups = Sf_Len(&f, layout);
		for (int i = 0; i < groups && !f.failed; i++) {
			int mark2 = f.n;
			PyObject *name = Sf_Cat(&f, "OsO", Sf_Item(&f, Sf_Mod(&f, "ShipEdit", "shipsToEditList"), k), "EngineGroup",
				Sf_ReprI(&f, i + 1));
			PyObject *bay = Sf_Item(&f, layout, i);
			PyObject *x = Sf_OpI(&f, Sf_Op(&f, Sf_OpI(&f, Sf_Item(&f, bay, 0), '-', 320), '*', ratio), '+', 320);
			PyObject *y = Sf_Item(&f, bay, 1);
			PyObject *w = Sf_Op(&f, Sf_Op(&f, Sf_Item(&f, bay, 2), '-', Sf_Item(&f, bay, 0)), '*', ratio);
			PyObject *h = Sf_Op(&f, Sf_Item(&f, bay, 3), '-', Sf_Item(&f, bay, 1));
			Sf_Text(&f, name);
			ET_Run(&f, id, "group", Sf_Build(&f, "{s:O,s:O,s:O,s:O,s:O}", "name", name, "x", x, "y", y, "w", w, "h", h));
			Sf_Release(&f, mark2);
		}
		Sf_Release(&f, mark);
	}
	Sf_Own(&f, MOD, "CreateLayouts", "(i)", id);
	PyObject *list = Sf_Keep(&f, f.failed ? NULL : PyList_New(0));
	Sf_SetGlobal(&f, MOD, "enginesList", list);
	PyObject *keys = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.availableEnginesList"), MOD);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *key = Sf_Next(&f, keys, i);
		if (!key) {
			break;
		}
		PyObject *mass = Sf_Item(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "MotorValues"), key), 0);
		Sf_Method(&f, ET_Global(&f, "enginesList"), "append", "((OO))", mass, key);
		Sf_Method(&f, ET_Global(&f, "canBeMounted"), "append", "(i)", 2);
		Sf_Release(&f, mark);
	}
	Sf_Method(&f, ET_Global(&f, "enginesList"), "sort", "()");
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		PyObject *file = Sf_Cat(&f, "sOs", "2D/ShipEditor/", ET_Engine(&f, Sf_Int(&f, i), 1), ".alpha.tga");
		ET_Run(&f, id, "inlist", Sf_Build(&f, "{s:i,s:O}", "n", i + 1, "file", file));
		Sf_Release(&f, mark);
	}
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		ET_Run(&f, id, "slot", Sf_Build(&f, "{s:i,s:i}", "n", i + 1, "y", ENGINES_Y + 62 * i));
		Sf_Release(&f, mark);
	}
	ET_Run(&f, id, "recycle", NULL);
	Sf_Own(&f, MOD, "SetRecycleVisible", "(ii)", id, 0);
	ET_Run(&f, id, "arrows", NULL);
	Sf_Own(&f, MOD, "SetPrevNextEngineVisible", "(ii)", id, 0);
	return Sf_None(&f);
}

/* the value of a Visible flag the file cannot set raises the TypeError of SScorer.Set */
static PyObject *ET_SetVisible(PyObject *args, const char *first, const char *section) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *v;
	if (!PyArg_ParseTuple(args, "iO", &id, &v)) {
		return NULL;
	}
	if (!PyInt_Check(v) && !PyFloat_Check(v) && !PyString_Check(v)) {
		Sf_ScorerSet(&f, id, first, "Visible", v);
	}
	ET_RunV(&f, id, section, v);
	return Sf_None(&f);
}

/* SetPrevNextEngineVisible(id, bVisible) and SetRecycleVisible(id, bVisible) */
static PyObject *ET_SetPrevNextEngineVisible(PyObject *self, PyObject *args) {
	(void)self;
	return ET_SetVisible(args, "PrevEngine", "arrowsvisible");
}

static PyObject *ET_SetRecycleVisible(PyObject *self, PyObject *args) {
	(void)self;
	return ET_SetVisible(args, "Recycle", "recyclevisible");
}

/* the links of ShowEnginesPanel between the list, the bays, the recycle button and the exit */
static void ET_ShowLinks(scriptFrame_t *f, int id, int groups) {
	PyObject *linkLR = Sf_From(f, "Menu", "LinkLR");
	PyObject *linkUD = Sf_From(f, "Menu", "LinkUD");
	Sf_From(f, "Menu", "UnLinkUD");
	PyObject *name = Sf_Cat(f, "Os", ET_Ship(f, "Model"), "EngineGroup1");
	static const char *const left[] = { "NextEngine", "PrevEngine", "Engine2", "Engine3", "Engine4" };
	for (int i = 0; i < 5; i++) {
		Sf_Line(f, linkLR, "(isO)", id, left[i], name);
	}
	for (int i = groups; i > 0 && !f->failed; i--) {
		int mark = f->n;
		name = Sf_Cat(f, "OsO", ET_Ship(f, "Model"), "EngineGroup", Sf_ReprI(f, i));
		Sf_Line(f, linkLR, "(isO)", id, "Engine1", name);
		Sf_Line(f, linkLR, "(iOs)", id, name, "Recycle");
		Sf_Release(f, mark);
	}
	Sf_Line(f, linkLR, "(iss)", id, "Recycle", "Exit");
	for (int i = 0; i < groups - 1 && !f->failed; i++) {
		int mark = f->n;
		PyObject *a = Sf_Cat(f, "OsO", ET_Ship(f, "Model"), "EngineGroup", Sf_ReprI(f, i + 1));
		PyObject *b = Sf_Cat(f, "OsO", ET_Ship(f, "Model"), "EngineGroup", Sf_ReprI(f, i + 2));
		Sf_Line(f, linkUD, "(iOO)", id, a, b);
		Sf_Release(f, mark);
	}
	/* currentEngineGroup = ...: a local of the script, the module variable stays */
	Sf_Cat(f, "Os", ET_Ship(f, "Model"), "EngineGroup1");
}

/* ShowEnginesPanel(id): the tab with the list from its start and the bays of the ship */
static PyObject *ET_ShowEnginesPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	etLoad_t l;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *linkUD = Sf_From(&f, "Menu", "LinkUD");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	Sf_SetGlobal(&f, MOD, "engineIndex", Sf_Int(&f, 0));
	PyObject *layout = Sf_GetItem(&f, Sf_Mod(&f, "ShipEdit", "shipsEnginesLayouts"), ET_Ship(&f, "Model"));
	int groups = Sf_Len(&f, layout);
	Sf_Own(&f, MOD, "SetRecycleVisible", "(ii)", id, 1);
	Sf_ScorerNum(&f, id, "Recycle", "Highlight", 0);
	Sf_ScorerNum(&f, id, "Recycle", "SpriteIndex", 10);
	Sf_ScorerStr(&f, id, "Recycle", "OnAccept", "");
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	Sf_Line(&f, linkUD, "(iss)", id, "PrevEngine", "Engine1");
	for (int i = 0; i < count && !f.failed; i++) {
		char name[0x20], next[0x20];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		ET_Visible(&f, id, name, 1);
		if (i + 2 <= count) {
			Str_Printf(next, sizeof(next), "Engine%d", i + 2);
			Sf_Line(&f, linkUD, "(iss)", id, name, next);
		} else {
			Sf_ScorerStr(&f, id, name, "Down", "");
		}
	}
	if (count >= 4) {
		Sf_Line(&f, linkUD, "(iss)", id, "Engine4", "NextEngine");
	}
	Sf_Own(&f, MOD, "SetPrevNextEngineVisible", "(ii)", id, 1);
	if (count > 4) {
		count = 4;
	}
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		/* the name of the script, with the engine instead of the number: an item that is not there */
		ET_Visible(&f, id, Sf_Text(&f, Sf_Cat(&f, "sOs", "Engine", ET_Engine(&f, Sf_Int(&f, i), 1), "InList")), 1);
		Sf_Release(&f, mark);
	}
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	l.weight = Sf_ToInt(&f, ET_Ship(&f, "AcMass"));
	l.maxWeight = Sf_ToInt(&f, ET_ShipTable(&f, "ShipTon", 2));
	l.mass = Sf_Int(&f, 0);
	PyObject *key = ET_Ship(&f, "MEng0");
	if (Sf_Compare(&f, key, "!=", Sf_Str(&f, "")) > 0) {
		l.mass = Sf_Item(&f, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "MotorValues"), ET_Upper(&f, key)), 0);
	}
	l.minMass = ET_ShipTable(&f, "ShipValues", 0);
	l.num = l.slots = NULL;
	for (int i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%dInList", i + 1);
		ET_Visible(&f, id, name, 1);
		Sf_ScorerNum(&f, id, name, "X", 36);
		Sf_ScorerNum(&f, id, name, "Y", ENGINES_Y + 64 * i);
		ET_Mount(&f, id, i, &l, layout);
		Sf_Release(&f, mark);
	}
	ET_ShowLinks(&f, id, groups);
	Sf_Own(&f, MOD, "ChangeRecycleButtonState", "(i)", id);
	Sf_ScorerNum(&f, id, "NextEngine", "W", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "H", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "SpriteIndex", 3);
	Sf_ScorerNum(&f, id, "PrevEngine", "W", 0);
	Sf_ScorerNum(&f, id, "PrevEngine", "H", 0);
	Sf_ScorerNum(&f, id, "PrevEngine", "SpriteIndex", 2);
	Sf_ScorerStr(&f, id, "Engine1", "Up", "ShipEditTabEngines");
	if (Sf_Len(&f, ET_Global(&f, "enginesList")) < 5 && !f.failed) {
		Sf_ScorerNum(&f, id, "NextEngine", "W", 0);
		Sf_ScorerNum(&f, id, "NextEngine", "H", 0);
		Sf_ScorerNum(&f, id, "NextEngine", "SpriteIndex", 2);
		Sf_ScorerStr(&f, id, "Engine4", "Down", "");
	}
	Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(ii)", id, 0);
	return Sf_None(&f);
}

/* the eight bay icons 0A .. 3B */
static const char *ET_IconName(char *out, int size, int i, int j) {
	Str_Printf(out, size, "%d%c", i, "AB"[j]);
	return out;
}

/* CreateLayouts(id): the bay icons with their lines */
static PyObject *ET_CreateLayouts(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	for (int i = 0; i < 4 && !f.failed; i++) {
		for (int j = 0; j < 2 && !f.failed; j++) {
			char n[8];
			int mark = f.n;
			ET_Run(&f, id, "icons", Sf_Build(&f, "{s:s}", "n", ET_IconName(n, sizeof(n), i, j)));
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* Rotate of a bay icon: 0 for a bay of one slot, else pointing out from the middle of the screen */
static int ET_Rotate(int j, PyObject *slots, double u, scriptFrame_t *f) {
	if (j == 0 && Sf_EqI(f, slots, j + 1) > 0) {
		return 0;
	}
	return u > 320 ? 1 : 3;
}

/* the screen place of HangarTab.prevShip.PEngSlot<n>: 1 with u, v */
static byte ET_SlotPos(scriptFrame_t *f, const char *n, double *u, double *v) {
	char var[0x20];
	PyObject *uv[2];
	Str_Printf(var, sizeof(var), "PEngSlot%s", n);
	if (!Sf_Unpack(f, Sf_Ask(f, "SScorer", "Get2DPos", "(O)", ET_Ship(f, var)), 2, uv)) {
		return 0;
	}
	*u = PyFloat_AsDouble(uv[0]);
	*v = PyFloat_AsDouble(uv[1]);
	return 1;
}

/* UpdateLayouts(id): the bay icons on the slots of the ship on the screen, the lines between the two slots of a bay */
static PyObject *ET_UpdateLayouts(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "ShipEdit");
	if (Sf_Compare(&f, Sf_Mod(&f, "ShipEdit", "prevTab"), "!=", Sf_Str(&f, "ShipEditTabEngines")) != 0) {
		return Sf_None(&f);
	}
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		char var[0x10], n[8], name[0x30];
		Str_Printf(var, sizeof(var), "nEng%d", i);
		Sf_SetItem(&f, ET_Global(&f, "nEngSlots"), Sf_Int(&f, i), ET_Ship(&f, var));
		for (int j = 0; j < 2 && !f.failed; j++) {
			ET_IconName(n, sizeof(n), i, j);
			PyObject *slots = Sf_Item(&f, ET_Global(&f, "nEngSlots"), i);
			int used = Sf_Compare(&f, Sf_Int(&f, j), "<", slots);
			if (used > 0) {
				double u, v;
				if (!ET_SlotPos(&f, n, &u, &v)) {
					break;
				}
				static const char *const prefix[2] = { "IconEngine", "IconEngineGlow" };
				for (int k = 0; k < 2; k++) {
					Str_Printf(name, sizeof(name), "%s%s", prefix[k], n);
					ET_Visible(&f, id, name, k == 0);
					Sf_ScorerNum(&f, id, name, "X", u);
					Sf_ScorerNum(&f, id, name, "Y", v);
					Sf_ScorerNum(&f, id, name, "PivotX", 0.5);
					Sf_ScorerNum(&f, id, name, "PivotY", 0.5);
					int rot = ET_Rotate(j, Sf_Item(&f, ET_Global(&f, "nEngSlots"), i), u, &f);
					Sf_ScorerNum(&f, id, name, "Rotate", rot);
				}
			} else if (used == 0) {
				Str_Printf(name, sizeof(name), "IconEngine%s", n);
				ET_Visible(&f, id, name, 0);
				Str_Printf(name, sizeof(name), "IconEngineGlow%s", n);
				ET_Visible(&f, id, name, 0);
			}
		}
		if (Sf_Compare(&f, Sf_Item(&f, ET_Global(&f, "nEngSlots"), i), ">=", Sf_Int(&f, 2)) > 0) {
			double u1, v1, u2, v2, minx, maxx, miny, maxy;
			ET_IconName(n, sizeof(n), i, 0);
			if (!ET_SlotPos(&f, n, &u1, &v1)) {
				break;
			}
			/* the lines take the name of slot B, the last n of the script */
			ET_IconName(n, sizeof(n), i, 1);
			if (!ET_SlotPos(&f, n, &u2, &v2)) {
				break;
			}
			v1 = v1 - 1;
			v2 = v2 - 1;
			if (u1 > u2) {
				double a = v1;
				minx = u2;
				maxx = u1;
				v1 = v2;
				v2 = a;
			} else {
				minx = u1;
				maxx = u2;
			}
			miny = v1 > v2 ? v2 : v1;
			maxy = v1 > v2 ? v1 : v2;
			if (maxy - miny <= 2) {
				miny = miny + (maxy - miny) / 2;
				v1 = v2 = maxy = miny;
			}
			double sizex = maxx - minx, sizey = maxy - miny, offx = 10;
			Str_Printf(name, sizeof(name), "IconEngineLineL%s", n);
			ET_Visible(&f, id, name, 1);
			Sf_ScorerNum(&f, id, name, "X", minx + offx);
			Sf_ScorerNum(&f, id, name, "Y", v1);
			Sf_ScorerNum(&f, id, name, "ScaleX", sizex / 2 - offx);
			Sf_ScorerNum(&f, id, name, "ScaleY", 2);
			Str_Printf(name, sizeof(name), "IconEngineLineC%s", n);
			ET_Visible(&f, id, name, 1);
			Sf_ScorerNum(&f, id, name, "X", minx + sizex / 2);
			Sf_ScorerNum(&f, id, name, "Y", miny);
			Sf_ScorerNum(&f, id, name, "ScaleX", 2);
			Sf_ScorerNum(&f, id, name, "ScaleY", sizey + 2);
			if (maxy == miny) {
				ET_Visible(&f, id, name, 0);
			}
			Str_Printf(name, sizeof(name), "IconEngineLineR%s", n);
			ET_Visible(&f, id, name, 1);
			Sf_ScorerNum(&f, id, name, "X", maxx - sizex / 2);
			Sf_ScorerNum(&f, id, name, "Y", v2);
			Sf_ScorerNum(&f, id, name, "ScaleX", sizex / 2 - offx);
			Sf_ScorerNum(&f, id, name, "ScaleY", 2);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static const char *const et_iconParts[5] = { "IconEngine", "IconEngineGlow", "IconEngineLineL", "IconEngineLineC",
	"IconEngineLineR" };

/* HideLayouts(id) */
static PyObject *ET_HideLayouts(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 2; j++) {
			char n[8], name[0x30];
			ET_IconName(n, sizeof(n), i, j);
			for (int k = 0; k < 5; k++) {
				Str_Printf(name, sizeof(name), "%s%s", et_iconParts[k], n);
				Script_SetNum(id, name, "Visible", 0);
			}
		}
	}
	return Py_ReturnNone();
}

/* UpdateLayoutsState(id, index, state): the icons of bay index active (orange) or not (blue) */
static PyObject *ET_UpdateLayoutsState(PyObject *self, PyObject *args) {
	static const int on[4] = { 255, 200, 45, 255 };
	static const int off[4] = { 75, 102, 167, 255 };
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *index, *state;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &index, &state)) {
		return NULL;
	}
	for (int j = 0; j < 2 && !f.failed; j++) {
		int mark = f.n;
		char n[0x20], name[0x40];
		ET_Name(&f, n, sizeof(n), "", index, j ? "B" : "A");
		int active = Sf_EqI(&f, state, 1);
		if (active >= 0) {
			const int *c = active ? on : off;
			Str_Printf(name, sizeof(name), "IconEngine%s", n);
			Sf_ScorerNum(&f, id, name, "SpriteIndex", active ? 36 : 35);
			for (int k = 2; k < 5; k++) {
				Str_Printf(name, sizeof(name), "%s%s", et_iconParts[k], n);
				Sf_ScorerNum(&f, id, name, "Red", c[0]);
				Sf_ScorerNum(&f, id, name, "Green", c[1]);
				Sf_ScorerNum(&f, id, name, "Blue", c[2]);
				Sf_ScorerNum(&f, id, name, "Alpha", c[3]);
			}
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* UpdateLayoutsGlow(id, control): the icons of the bay control glow, the others not */
static PyObject *ET_UpdateLayoutsGlow(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *t = Sf_Str(&f, "");
	if (Sf_Compare(&f, control, "!=", t) > 0) {
		PyObject *at = Sf_Ask(&f, "string", "find", "(Os)", control, "EngineGroup");
		if (Sf_Compare(&f, at, "!=", Sf_Int(&f, -1)) > 0) {
			t = Sf_OpI(&f, Sf_Ask(&f, "string", "atoi", "(O)", Sf_Item(&f, control, -1)), '-', 1);
		}
	}
	for (int i = 0; i < 4 && !f.failed; i++) {
		long slots = 0;
		ET_Range(&f, Sf_Item(&f, ET_Global(&f, "nEngSlots"), i), &slots);
		for (long j = 0; j < slots && !f.failed; j++) {
			int mark = f.n;
			char name[0x30];
			if (j >= 2) {
				Sf_Raise(&f, PyExc_IndexError, "tuple index out of range");
				break;
			}
			int glow = Sf_Compare(&f, Sf_Int(&f, i), "==", t);
			if (glow >= 0) {
				const char *render = glow ? "!ScorerSpriteGlow" : "";
				Str_Printf(name, sizeof(name), "IconEngineGlow%d%c", i, "AB"[j]);
				ET_Visible(&f, id, name, glow);
				Sf_ScorerStr(&f, id, name, "OnRender", render);
				for (int k = 2; k < 5; k++) {
					Str_Printf(name, sizeof(name), "%s%d%c", et_iconParts[k], i, "AB"[j]);
					Sf_ScorerStr(&f, id, name, "OnRender", render);
				}
			}
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* LayoutsGainFocus(id, control, lostfocus) and LayoutsLostFocus(id, control, gainfocus) */
static PyObject *ET_LayoutsGainFocus(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *other;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &other)) {
		return NULL;
	}
	Sf_Own(&f, MOD, "UpdateLayoutsGlow", "(iO)", id, control);
	return Sf_None(&f);
}

static PyObject *ET_LayoutsLostFocus(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control, *other;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &other)) {
		return NULL;
	}
	Sf_Own(&f, MOD, "UpdateLayoutsGlow", "(iO)", id, other);
	return Sf_None(&f);
}

/* HideEnginesPanel(id) */
static PyObject *ET_HideEnginesPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "HangarTab");
	if (Sf_Compare(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "!=", Py_None) > 0) {
		PyObject *layout = Sf_GetItem(&f, Sf_Mod(&f, "ShipEdit", "shipsEnginesLayouts"), ET_Ship(&f, "Model"));
		int groups = Sf_Len(&f, layout);
		for (int i = 0; i < groups && !f.failed; i++) {
			int mark = f.n;
			const char *name = Sf_Text(&f, Sf_Cat(&f, "OsO", ET_Ship(&f, "Model"), "EngineGroup", Sf_ReprI(&f, i + 1)));
			ET_Visible(&f, id, name, 0);
			ET_Color(&f, id, name, et_normal);
			Sf_Release(&f, mark);
		}
	}
	Sf_Own(&f, MOD, "HideLayouts", "(i)", id);
	ET_Run(&f, id, "hidepredict", NULL);
	Sf_Own(&f, MOD, "SetRecycleVisible", "(ii)", id, 0);
	for (int i = 0; i < 4; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		ET_Visible(&f, id, name, 0);
	}
	Sf_Own(&f, MOD, "SetPrevNextEngineVisible", "(ii)", id, 0);
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < count; i++) {
			char name[0x20];
			Str_Printf(name, sizeof(name), "Engine%dInList", i + 1);
			ET_Visible(&f, id, name, 0);
		}
	}
	Sf_Line(&f, Sf_From(&f, "Menu", "UnLinkLR"), "(iss)", id, "Recycle", "Exit");
	return Sf_None(&f);
}

/* the four engines shown from engineIndex on, after a scroll: their places and colours */
static void ET_ShowFour(scriptFrame_t *f, int id) {
	etLoad_t l;
	long first = 0;
	Sf_Import(f, "ShipMaker");
	Sf_Import(f, "string");
	Sf_Import(f, "ShipEdit");
	Sf_Import(f, "HangarTab");
	ET_Load(f, &l, 1);
	ET_Range(f, ET_Global(f, "engineIndex"), &first);
	for (long i = first, j = 0; i < first + 4 && !f->failed; i++, j++) {
		int mark = f->n;
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
		ET_Visible(f, id, name, 1);
		Sf_ScorerNum(f, id, name, "X", 36);
		Sf_ScorerNum(f, id, name, "Y", ENGINES_Y + 64 * j);
		ET_Mount(f, id, i, &l, NULL);
		Sf_Release(f, mark);
	}
}

/* the InList sprites from first to the end of the list hidden */
static void ET_HideFrom(scriptFrame_t *f, int id, long first, long end) {
	for (long i = first; i < end; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
		ET_Visible(f, id, name, 0);
	}
}

/* ScrollEngineListUp(id, control): the list one engine up */
static PyObject *ET_ScrollEngineListUp(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	if (count <= 4) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, ET_Global(&f, "engineIndex"), ">", Sf_Int(&f, 0)) > 0) {
		Sf_SetGlobal(&f, MOD, "engineIndex", Sf_OpI(&f, ET_Global(&f, "engineIndex"), '-', 1));
		ET_ShowFour(&f, id);
		long first = 0;
		ET_Range(&f, Sf_OpI(&f, ET_Global(&f, "engineIndex"), '+', 4), &first);
		count = Sf_Len(&f, ET_Global(&f, "enginesList"));
		ET_HideFrom(&f, id, first, count);
	}
	Sf_ScorerNum(&f, id, "PrevEngine", "W", 25);
	Sf_ScorerNum(&f, id, "PrevEngine", "H", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "W", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "H", 30);
	Sf_ScorerNum(&f, id, "PrevEngine", "SpriteIndex", 3);
	Sf_ScorerNum(&f, id, "NextEngine", "SpriteIndex", 3);
	if (Sf_EqI(&f, ET_Global(&f, "engineIndex"), 0) > 0) {
		const char *item = Sf_Text(&f, control);
		Sf_ScorerNum(&f, id, item, "W", 0);
		Sf_ScorerNum(&f, id, item, "H", 0);
		Sf_ScorerNum(&f, id, item, "SpriteIndex", 2);
		Sf_ScorerDefault(&f, id, "Engine1");
		Sf_ScorerStr(&f, id, "Engine1", "Up", "ShipEditTabEngines");
	}
	Sf_ScorerStr(&f, id, "Engine4", "Down", "NextEngine");
	Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(ii)", id, 0);
	return Sf_None(&f);
}

/* ScrollEngineListDown(id, control): the list one engine down */
static PyObject *ET_ScrollEngineListDown(PyObject *self, PyObject *args) {
	scriptFrame_t f = { .n = 0 };
	int id;
	PyObject *control;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	int count = Sf_Len(&f, ET_Global(&f, "enginesList"));
	if (count <= 4) {
		return Sf_None(&f);
	}
	PyObject *rest = Sf_Op(&f, Sf_Int(&f, count), '-', ET_Global(&f, "engineIndex"));
	if (Sf_Compare(&f, rest, ">", Sf_Int(&f, 4)) > 0) {
		Sf_SetGlobal(&f, MOD, "engineIndex", Sf_OpI(&f, ET_Global(&f, "engineIndex"), '+', 1));
		/* the loads are read before the hiding, as in the script */
		etLoad_t l;
		long first = 0, from4 = 0;
		Sf_Import(&f, "ShipMaker");
		Sf_Import(&f, "string");
		Sf_Import(&f, "ShipEdit");
		Sf_Import(&f, "HangarTab");
		ET_Load(&f, &l, 1);
		ET_Range(&f, ET_Global(&f, "engineIndex"), &first);
		ET_HideFrom(&f, id, 0, first);
		ET_Range(&f, Sf_OpI(&f, ET_Global(&f, "engineIndex"), '+', 4), &from4);
		ET_HideFrom(&f, id, from4, Sf_Len(&f, ET_Global(&f, "enginesList")));
		ET_Range(&f, ET_Global(&f, "engineIndex"), &first);
		for (long i = first, j = 0; i < first + 4 && !f.failed; i++, j++) {
			int mark = f.n;
			char name[0x20];
			Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
			ET_Visible(&f, id, name, 1);
			Sf_ScorerNum(&f, id, name, "X", 36);
			Sf_ScorerNum(&f, id, name, "Y", ENGINES_Y + 64 * j);
			ET_Mount(&f, id, i, &l, NULL);
			Sf_Release(&f, mark);
		}
	}
	Sf_Own(&f, MOD, "HighlightCurrentMountedEngine", "(ii)", id, 0);
	Sf_ScorerNum(&f, id, "PrevEngine", "W", 25);
	Sf_ScorerNum(&f, id, "PrevEngine", "H", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "W", 25);
	Sf_ScorerNum(&f, id, "NextEngine", "H", 25);
	Sf_ScorerNum(&f, id, "PrevEngine", "SpriteIndex", 3);
	Sf_ScorerNum(&f, id, "NextEngine", "SpriteIndex", 3);
	if (Sf_Compare(&f, Sf_OpI(&f, ET_Global(&f, "engineIndex"), '+', 4), ">=", Sf_Int(&f, count)) > 0) {
		const char *item = Sf_Text(&f, control);
		Sf_ScorerNum(&f, id, item, "W", 0);
		Sf_ScorerNum(&f, id, item, "H", 0);
		Sf_ScorerNum(&f, id, item, "SpriteIndex", 2);
		Sf_ScorerDefault(&f, id, "Engine4");
		Sf_ScorerStr(&f, id, "Engine4", "Down", "");
	}
	if (Sf_Compare(&f, ET_Global(&f, "engineIndex"), ">", Sf_Int(&f, 0)) > 0) {
		Sf_ScorerNum(&f, id, "PrevEngine", "W", 25);
		Sf_ScorerNum(&f, id, "PrevEngine", "H", 25);
		Sf_ScorerNum(&f, id, "PrevEngine", "SpriteIndex", 3);
		Sf_ScorerStr(&f, id, "Engine1", "Up", "PrevEngine");
	}
	return Sf_None(&f);
}

PyMethodDef g_EnginesTabMethods[] = {
	{ "HighlightCurrentMountedEngine", (PyCFunction)(void (*)(void))ET_HighlightCurrentMountedEngine, 3, NULL },
	{ "ShowEngineDesc", ET_ShowEngineDesc, 1, NULL },
	{ "HideEngineDesc", ET_HideEngineDesc, 1, NULL },
	{ "ChangeRecycleButtonState", ET_ChangeRecycleButtonState, 1, NULL },
	{ "UpdateEnginesList", ET_UpdateEnginesList, 1, NULL },
	{ "PutEngine", ET_PutEngine, 1, NULL },
	{ "RecycleEngine", ET_RecycleEngine, 1, NULL },
	{ "ShowMoneySellingEngine", ET_ShowMoneySellingEngine, 1, NULL },
	{ "SelectEngineGroup", (PyCFunction)(void (*)(void))ET_SelectEngineGroup, 3, NULL },
	{ "SetEnginesPanel", ET_SetEnginesPanel, 1, NULL },
	{ "SetPrevNextEngineVisible", ET_SetPrevNextEngineVisible, 1, NULL },
	{ "SetRecycleVisible", ET_SetRecycleVisible, 1, NULL },
	{ "ShowEnginesPanel", ET_ShowEnginesPanel, 1, NULL },
	{ "CreateLayouts", ET_CreateLayouts, 1, NULL },
	{ "UpdateLayouts", ET_UpdateLayouts, 1, NULL },
	{ "HideLayouts", ET_HideLayouts, 1, NULL },
	{ "UpdateLayoutsState", ET_UpdateLayoutsState, 1, NULL },
	{ "UpdateLayoutsGlow", ET_UpdateLayoutsGlow, 1, NULL },
	{ "LayoutsGainFocus", ET_LayoutsGainFocus, 1, NULL },
	{ "LayoutsLostFocus", ET_LayoutsLostFocus, 1, NULL },
	{ "HideEnginesPanel", ET_HideEnginesPanel, 1, NULL },
	{ "ScrollEngineListUp", ET_ScrollEngineListUp, 1, NULL },
	{ "ScrollEngineListDown", ET_ScrollEngineListDown, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
