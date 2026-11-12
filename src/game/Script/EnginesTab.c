/*
 * The EnginesTab module in C (scripts/shipedit/enginestab.py, issue #2): the engines tab of the ship editor. The
 * player picks an engine bay of the ship (a group of one or two slots), sees the engines of the list (four at a time,
 * red when the ship would get too heavy) and buys one for every slot of the bay or recycles the mounted ones. The
 * layout is data/menus/enginestab.def. enginesList (mass and name, by mass), canBeMounted (2 can be mounted, 0 not
 * now, 1 never; SetEnginesPanel appends to it without clearing it, like the script), engineIndex (the first engine
 * shown), currentEngineGroup (the bay), nEngSlots, the colours and UseTxt stay variables of the module. ShipEdit,
 * HangarTab, ShipMaker and Sound are used through their modules.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values and set the scorer items in C; the
 * function table rows name the entries. The module's variables are read where the script reads them, canBeMounted,
 * nEngSlots and enginesList changed in place on the module's objects; the ship being edited is
 * HangarTab.prevShip as a handle value (OBJLIB 7), the tables of ShipMaker and ShipEdit are read as their
 * attributes. HangarTab, ShipEdit and ShipMaker are called through their refs (PORT_CALL: directly while the attribute
 * holds the port's function, else by name), string.atoi and string.find through their native twins, and the module's
 * own functions through its globals as the script calls them (PORT_OWN, the same seam on the global). The link
 * functions of Menu come from a from import (OBJLIB 6) and are called through their entries while they are Menu's own
 * (held as values, OBJLIB 7: Mode_LinkCallV).
 */
#include <limits.h>
#include <math.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static const char enginestab_def[] = {
#embed "../../../data/menus/enginestab.def"
	, 0 };

#define ENGINESTAB_FILE "menus/enginestab.def"
#define MOD "EnginesTab"
#define ENGINES_Y 125			/* EnginesTab.EnginesYOffset */
#define ET_NAME 0x40
#define ET_ITEM 0x200			/* an item name made of values of the script */

/* EnginesTab.NormalColor*, SelectedColor*, ForbiddenColor* (red, green, blue) */
static const int et_normal[3] = { 115, 150, 255 };
static const int et_selected[3] = { 250, 230, 90 };
static const int et_forbidden[3] = { 200, 30, 30 };


/* the step usehint <key>: the Hint of the item, Scrap.GetLangStr(key) % UseTxt (the error left set stops the file) */
static void ET_StepHint(int id, const nativeVal_t *args, int n) {
	nativeVal_t hint = { 0 };
	const char *k;
	/* the first value of the line (args[0] of the script) */
	if ((n > 0 || Native_Raise(NE_INDEX, "tuple index out of range")) && Mode_ItemText(&args[0], &k) &&
		Mode_LangUse(MOD, k, &hint)) {
		Mode_ScorerPut(id, Script_CurrentItem(), "Hint", &hint);
	}
	Native_Free(&hint);
}

static const scriptStep_t enginestab_steps[] = {
	{ "usehint", ET_StepHint },
	{ NULL, NULL },
};

/* a section of enginestab.def with its $names, n pairs of a name and a value (Mode_DefRun): 0 with the error of the file */
#define ET_Run(id, section, n, ...) \
	Mode_DefRun(id, ENGINESTAB_FILE, section, enginestab_def, enginestab_steps, 1, n __VA_OPT__(, ) __VA_ARGS__)

/* the section with the value v as $v */
static byte ET_RunV(int id, const char *section, const nativeVal_t *v) {
	return ET_Run(id, section, 1, "v", v);
}

/* a global of the module as its functions read it (out None before) */
static byte ET_G(const char *name, nativeVal_t *out) {
	return Mode_G(MOD, name, out);
}

/* the C text prefix + `n` + suffix in a buffer of size bytes (a longer one is the engine's fatal check, as in the
 * script's port) */
static void ET_NameIn(char *out, int size, const char *prefix, const nativeVal_t *n, const char *suffix) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, n, NR_REPR);
	Str_Printf(out, size, "%s%s%s", prefix, b.p, suffix);
	Native_BufFree(&b);
}

static void ET_Name(char *out, const char *prefix, const nativeVal_t *n, const char *suffix) {
	ET_NameIn(out, ET_NAME, prefix, n, suffix);
}

/* HangarTab.prevShip.<prefix + `n`> */
static byte ET_ShipN(const char *prefix, const nativeVal_t *n, nativeVal_t *out) {
	char var[ET_NAME];
	ET_Name(var, prefix, n, "");
	return Mode_ShipGet(var, out);
}

/* the hint of a key (with UseTxt when use) into the item */
static byte ET_SetHint(int id, const char *item, const char *key, byte use) {
	nativeVal_t hint;
	if (use) {
		if (!Mode_LangUse(MOD, key, &hint)) {
			return 0;
		}
	} else {
		Mode_Lang(key, &hint);
	}
	byte ok = Mode_ScorerPut(id, item, "Hint", &hint);
	Native_Free(&hint);
	return ok;
}

/* seq[i] of the script (an int subscript) as a copy */
static byte ET_Item(const nativeVal_t *seq, long i, nativeVal_t *out) {
	nativeVal_t tmp;
	const nativeVal_t *v = Native_GetItem(seq, i, 1, &tmp);
	if (!v) {
		return 0;
	}
	Native_Copy(out, v);
	if (v == &tmp) {
		Native_Free(&tmp);
	}
	return 1;
}

/* enginesList[i][k] */
static byte ET_Engine(const nativeVal_t *i, long k, nativeVal_t *out) {
	nativeVal_t list = { 0 }, row = { 0 };
	byte ok = ET_G("enginesList", &list) && Native_Subscript(&list, i, &row) && ET_Item(&row, k, out);
	Native_Free(&list);
	Native_Free(&row);
	return ok;
}

static byte ET_EngineI(long i, long k, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetInt(&n, i);
	return ET_Engine(&n, k, out);
}

/* len(global) of the module: -1 with the error */
static int ET_Len(const char *name) {
	nativeVal_t v = { 0 };
	int n = ET_G(name, &v) ? Native_Size(&v) : -1;
	Native_Free(&v);
	return n;
}

/* a op b of the script into out (Native_Free out first when it holds a value) */
static byte ET_Op(const nativeVal_t *a, char op, const nativeVal_t *b, nativeVal_t *out) {
	return Native_Arith(op, a, b, out);
}

static byte ET_OpI(const nativeVal_t *a, char op, long b, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetInt(&n, b);
	return Native_Arith(op, a, &n, out);
}

static int ET_CmpS(const nativeVal_t *a, const char *s, char op) {
	nativeVal_t b;
	Native_SetStr(&b, s);
	return Mode_CmpOp(a, &b, op);
}

/* int(seq[len(seq) - 1]) - 1 of a value (control or currentEngineGroup) */
static byte ET_LastNum(const nativeVal_t *seq, long minus, nativeVal_t *out) {
	nativeVal_t c = { 0 }, n = { 0 };
	int len = Native_Size(seq);
	byte ok = len >= 0 && ET_Item(seq, len - 1, &c) && Native_ToInt(&c, &n) && (minus ? ET_OpI(&n, '-', minus, out) :
		(Native_Copy(out, &n), 1));
	Native_Free(&c);
	Native_Free(&n);
	return ok;
}

/* int(currentEngineGroup[len(currentEngineGroup) - 1]) - 1: the bay */
static byte ET_Bay(nativeVal_t *out) {
	nativeVal_t g = { 0 };
	byte ok = ET_G("currentEngineGroup", &g) && ET_LastNum(&g, 1, out);
	Native_Free(&g);
	return ok;
}

/* currentEngineGroup, an item name: its C text in buf (ET_ITEM) */
static byte ET_Group(char *buf) {
	nativeVal_t g = { 0 };
	const char *s;
	byte ok = ET_G("currentEngineGroup", &g) && Mode_ItemText(&g, &s);
	if (ok) {
		Str_Copy(buf, s, ET_ITEM);
	}
	Native_Free(&g);
	return ok;
}

/* string.upper(HangarTab.prevShip.<prefix + `n`>) */
static byte ET_ShipUpper(const char *prefix, const nativeVal_t *n, nativeVal_t *out) {
	nativeVal_t v = { 0 };
	byte ok = ET_ShipN(prefix, n, &v) && Native_Upper(&v, out);
	Native_Free(&v);
	return ok;
}

/* ShipMaker.MotorValues[key][i] (key upper first when up) */
static byte ET_Motor(const nativeVal_t *key, byte up, long i, nativeVal_t *out) {
	nativeVal_t motors = { 0 }, k = { 0 }, row = { 0 };
	byte ok = Mode_ModVar("ShipMaker", "MotorValues", &motors) && (up ? Native_Upper(key, &k) : (Native_Copy(&k, key), 1)) &&
		Native_Subscript(&motors, &k, &row) && ET_Item(&row, i, out);
	Native_Free(&motors);
	Native_Free(&k);
	Native_Free(&row);
	return ok;
}

/* ShipMaker.GetEnginePrice(name) */
static byte ET_Price(const nativeVal_t *name, nativeVal_t *out) {
	return PORT_CALL(ShipMaker, GetEnginePrice, out, "v", name);
}

/* slots * ShipMaker.GetEnginePrice(name) */
static byte ET_SlotsPrice(const nativeVal_t *slots, const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t p = { 0 };
	byte ok = ET_Price(name, &p) && ET_Op(slots, '*', &p, out);
	Native_Free(&p);
	return ok;
}

/* ShipEdit.shipsEnginesLayouts[HangarTab.prevShip.Model] */
static byte ET_Layout(nativeVal_t *out) {
	nativeVal_t layouts = { 0 }, model = { 0 };
	byte ok = Mode_ModVar("ShipEdit", "shipsEnginesLayouts", &layouts) && Mode_ShipGet("Model", &model) &&
		Native_Subscript(&layouts, &model, out);
	Native_Free(&layouts);
	Native_Free(&model);
	return ok;
}

/* the C name HangarTab.prevShip.Model + "EngineGroup" + `n` of the script (ET_ITEM) */
static byte ET_GroupName(long n, char *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = Mode_V(&t), *name = Mode_V(&t), *rep = Mode_V(&t), *num = Mode_V(&t);
	const char *s;
	Native_SetInt(num, n);
	Mode_Repr(num, rep);
	byte ok = Mode_ShipGet("Model", model) && Mode_Cat(&t, name, 3, model, Mode_S(&t, "EngineGroup"), rep) &&
		Mode_ItemText(name, &s);
	if (ok) {
		Str_Copy(out, s, ET_ITEM);
	}
	return Mode_Done(&t, ok);
}

/* the bay loads of the current bay, as the script reads them: mass and number of the engines mounted (0 for none),
 * the slots and the bay number */
typedef struct {
	nativeVal_t weight, maxWeight, mass, num, slots, minMass;
} etLoad_t;

static void ET_LoadFree(etLoad_t *l) {
	Native_Free(&l->weight);
	Native_Free(&l->maxWeight);
	Native_Free(&l->mass);
	Native_Free(&l->num);
	Native_Free(&l->slots);
	Native_Free(&l->minMass);
}

/* int(HangarTab.prevShip.AcMass) and int(ShipMaker.ShipTon[...][2]) into the load */
static byte ET_LoadMasses(etLoad_t *l) {
	nativeVal_t am = { 0 }, top = { 0 };
	byte ok = Mode_ShipGet("AcMass", &am) && Native_ToInt(&am, &l->weight) && Mode_ShipTable("ShipTon", 2, &top) &&
		Native_ToInt(&top, &l->maxWeight);
	Native_Free(&am);
	Native_Free(&top);
	return ok;
}

/* the unused layout = ShipEdit.shipsEnginesLayouts[...] of the scripts is read before the bay (layoutFirst, the
 * scroll functions) or after it (UpdateEnginesList); l zeroed by the caller */
static byte ET_Load(etLoad_t *l, byte layoutFirst) {
	nativeVal_t layout = { 0 }, bay = { 0 }, key = { 0 };
	byte ok = ET_LoadMasses(l) && (!layoutFirst || ET_Layout(&layout)) && ET_Bay(&bay) && (layoutFirst || ET_Layout(&layout));
	if (ok) {
		Native_SetInt(&l->mass, 0);
		Native_SetInt(&l->num, 0);
		ok = ET_ShipN("nEng", &bay, &l->slots) && ET_ShipN("MEng", &bay, &key);
	}
	int some = ok ? ET_CmpS(&key, "", '!') : -1;
	if (some > 0) {
		Native_Free(&l->num);
		some = ET_Motor(&key, 1, 0, &l->mass) && ET_ShipN("NEng", &bay, &l->num) ? 1 : -1;
	}
	ok = some >= 0 && Mode_ShipTable("ShipValues", 0, &l->minMass);
	Native_Free(&layout);
	Native_Free(&bay);
	Native_Free(&key);
	return ok;
}

/* canBeMounted[i] = v, in place on the module's list */
static byte ET_SetCan(const nativeVal_t *i, long v) {
	nativeVal_t x;
	Native_SetInt(&x, v);
	return Mode_DictPut(MOD, "canBeMounted", NULL, i, &x);
}

/*
 * engine i of the list against the load: red when weight - num * mass + slots * enginesList[i][0] > maxWeight (with
 * canBeMounted[i] 0 when an empty ship could take it in the bay, minMass + slots * mass <= maxWeight, else 1), else
 * the normal colour and 2; the hint of the tile Engine<i+1>. layout: ShowEnginesPanel's form, num and slots are
 * layout[0][4] and the second test reads HangarTab.prevShip.nEng0
 */
static byte ET_Mount(int id, long i, const etLoad_t *l, const nativeVal_t *layout) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *idx = Mode_V(&t), *row = Mode_V(&t), *lnum = Mode_V(&t), *nm = Mode_V(&t), *w = Mode_V(&t);
	nativeVal_t *em = Mode_V(&t), *sm = Mode_V(&t), *w2 = Mode_V(&t), *slots2 = Mode_V(&t), *em2 = Mode_V(&t);
	nativeVal_t *sm2 = Mode_V(&t), *empty = Mode_V(&t);
	char name[ET_NAME], tile[ET_NAME];
	Native_SetInt(idx, i);
	Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
	Str_Printf(tile, sizeof(tile), "Engine%ld", i + 1);
	const nativeVal_t *num = l ? &l->num : lnum, *slots = l ? &l->slots : lnum;
	if (layout && (!ET_Item(layout, 0, row) || !ET_Item(row, 4, lnum))) {
		return Mode_Done(&t, 0);
	}
	if (layout) {
		/* the second layout[0][4] of the script: the same value */
		num = slots = lnum;
	}
	if (!ET_Op(num, '*', &l->mass, nm) || !ET_Op(&l->weight, '-', nm, w) || !ET_Engine(idx, 0, em) ||
		!ET_Op(slots, '*', em, sm) || !ET_Op(w, '+', sm, w2)) {
		return Mode_Done(&t, 0);
	}
	int heavy = Mode_CmpOp(w2, &l->maxWeight, '>');
	if (heavy > 0) {
		if (layout ? !Mode_ShipGet("nEng0", slots2) : (Native_Copy(slots2, &l->slots), 0)) {
			return Mode_Done(&t, 0);
		}
		if (!ET_Engine(idx, 0, em2) || !ET_Op(slots2, '*', em2, sm2) || !ET_Op(&l->minMass, '+', sm2, empty)) {
			return Mode_Done(&t, 0);
		}
		int fits = Mode_CmpOp(empty, &l->maxWeight, 'l');
		if (fits < 0 || !ET_SetCan(idx, fits ? 0 : 1) || !ET_SetHint(id, tile, fits ? "HINT_Engines4" : "HINT_Engines5", 0)) {
			return Mode_Done(&t, 0);
		}
		Script_SetNum(id, name, "Alpha", 255);
		Mode_ItemColor(id, name, et_forbidden);
	} else if (heavy == 0) {
		Script_SetNum(id, name, "Alpha", 255);
		Mode_ItemColor(id, name, et_normal);
		if (!ET_SetCan(idx, 2) || !ET_SetHint(id, tile, "HINT_Engines2", 1)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, heavy >= 0);
}

/* the hint of a bay: HINT_Engines6 for a bay without slots, else HINT_Engines<6 + slots> */
static byte ET_BayHint(int id, const char *item, const nativeVal_t *slots) {
	int none = Mode_CmpOpI(slots, 0, '=');
	if (none) {
		return none > 0 && ET_SetHint(id, item, "HINT_Engines6", 1);
	}
	nativeVal_t six, n = { 0 }, text;
	nativeBuf_t key;
	Native_SetInt(&six, 6);
	if (!ET_Op(&six, '+', slots, &n)) {
		return 0;
	}
	Native_BufInit(&key);
	Native_BufAdd(&key, "HINT_Engines", -1);
	Native_Repr(&key, &n, NR_REPR);
	Native_Free(&n);
	Mode_Lang(key.p, &text);
	Native_BufFree(&key);
	Script_SetStr(id, item, "Hint", Native_Text(&text));
	Native_Free(&text);
	return 1;
}

/* HighlightCurrentMountedEngine(id, setDefault=1): the tile of the engine mounted in the bay in the selected colour */
static byte ET_Highlight(int id, const nativeVal_t *setDefault) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *g = Mode_V(&t), *number = Mode_V(&t), *bay = Mode_V(&t), *engineName = Mode_V(&t), *pos = Mode_V(&t);
	nativeVal_t *ni = Mode_V(&t), *index = Mode_V(&t), *can = Mode_V(&t);
	if (!Script_Import(mods) || !ET_G("currentEngineGroup", g)) {
		return Mode_Done(&t, 0);
	}
	int len = Native_Size(g);
	if (len < 0 || (len == 0 ? (Native_SetStr(number, "1"), 0) : !ET_Item(g, len - 1, number)) ||
		!Native_ToInt(number, ni) || !ET_OpI(ni, '-', 1, bay) || !ET_ShipN("MEng", bay, engineName)) {
		return Mode_Done(&t, 0);
	}
	int count = ET_Len("enginesList");
	if (count < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(pos, 0);
	for (int i = 0; i < count; i++) {
		nativeVal_t a = { 0 }, e = { 0 }, b = { 0 };
		Native_SetInt(pos, i);
		int same = Native_Upper(engineName, &a) && ET_EngineI(i, 1, &e) && Native_Upper(&e, &b) ? Mode_CmpOp(&a, &b, '=') : -1;
		Native_Free(&a);
		Native_Free(&e);
		Native_Free(&b);
		if (same < 0) {
			return Mode_Done(&t, 0);
		}
		if (same) {
			break;
		}
	}
	if (count > 4) {
		count = 4;
	}
	for (int i = 0; i < count; i++) {
		int mark = t.n;
		nativeVal_t *n = Mode_V(&t), *sum = Mode_V(&t), *n2 = Mode_V(&t), *sum2 = Mode_V(&t), *list = Mode_V(&t);
		char name[0x20], name2[ET_NAME];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		Native_SetInt(n, i);
		int sel = ET_G("engineIndex", index) && ET_Op(n, '+', index, sum) ? Mode_CmpOp(sum, pos, '=') : -1;
		Native_Free(index);
		if (sel > 0) {
			sel = ET_CmpS(engineName, "", '!');
		}
		byte ok = sel >= 0;
		if (sel > 0) {
			ok = ET_SetHint(id, name, "HINT_Engines3", 0);
			if (ok) {
				Script_SetNum(id, name, "Alpha", 255);
				Mode_ItemColor(id, name, et_selected);
				Native_SetInt(n2, i + 1);
				ok = ET_G("engineIndex", index) && ET_Op(n2, '+', index, sum2);
				Native_Free(index);
			}
			if (ok) {
				ET_Name(name2, "Engine", sum2, "InList");
				Script_SetNum(id, name2, "Alpha", 255);
				Mode_ItemColor(id, name2, et_selected);
				int def = Mode_CmpOpI(setDefault, 1, '=');
				ok = def >= 0;
				if (def > 0) {
					Script_SetDefault(id, name);
				}
			}
		} else if (sel == 0) {
			Script_SetNum(id, name, "Alpha", 255);
			Mode_ItemColor(id, name, et_normal);
			int two = ET_G("canBeMounted", list) && ET_Item(list, i, can) ? Mode_CmpOpI(can, 2, '=') : -1;
			Native_Free(can);
			ok = two >= 0 && (!two || ET_SetHint(id, name, "HINT_Engines2", 1));
		}
		Mode_Release(&t, mark);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

static byte ET_HighlightCurrentMountedEngine_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *def = NULL;
	nativeVal_t one;
	int id;
	(void)out;
	Native_SetInt(&one, 1);
	return Mode_KwArity(a, n, "i|O") && Mode_Parse(a, n, "i|O", &id, &def) && ET_Highlight(id, def ? def : &one);
}

static char *ET_HighlightCurrentMountedEngine_N[] = { "id", "setDefault", NULL };
static const char ET_HighlightCurrentMountedEngine_F[] = "i|O";

/* the description of engine eng of the list (the tile Engine<n>) */
static byte ET_Desc(int id, const nativeVal_t *n, const nativeVal_t *eng) {
	static const char *const labels[6] = { "ShipEdit_Engine", "ShipEdit_Power", "ShipEdit_PowerUnits", "ShipEdit_Weight",
		"ShipEdit_Cost", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *list = Mode_V(&t), *can = Mode_V(&t), *engineName = Mode_V(&t), *key = Mode_V(&t), *t2 = Mode_V(&t);
	nativeVal_t *power = Mode_V(&t), *weight = Mode_V(&t), *price = Mode_V(&t), *lab[5];
	char tile[ET_NAME];
	if (!ET_G("canBeMounted", list) || !Native_Subscript(list, eng, can)) {
		return Mode_Done(&t, 0);
	}
	int c0 = Mode_CmpOpI(can, 0, '='), c1 = c0 == 0 ? Mode_CmpOpI(can, 1, '=') : 0;
	if (c0 < 0 || c1 < 0) {
		return Mode_Done(&t, 0);
	}
	if (c0 || c1) {
		ET_Name(tile, "Engine", n, "");
		if (!ET_SetHint(id, tile, c0 ? "HINT_Engines4" : "HINT_Engines5", 0)) {
			return Mode_Done(&t, 0);
		}
	}
	nativeVal_t *motors = Mode_V(&t);
	const char *k;
	for (int i = 0; i < 5; i++) {
		lab[i] = Mode_V(&t);
	}
	if (!ET_Engine(eng, 1, engineName) || !Mode_ModVar("ShipMaker", "MotorValues", motors)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(labels[0], lab[0]);
	if (!Mode_Cat(&t, key, 3, Mode_S(&t, "ShipEdit_"), engineName, Mode_S(&t, "Name")) || !Native_ArgStr(key, 1, &k)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang(k, t2);
	Mode_Lang(labels[1], lab[1]);
	nativeVal_t *row = Mode_V(&t), *x = Mode_V(&t);
	if (!Native_Subscript(motors, engineName, row) || !ET_Item(row, 2, x)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(x, power);
	Mode_Lang(labels[2], lab[2]);
	Mode_Lang(labels[3], lab[3]);
	Native_Free(x);
	if (!ET_Motor(engineName, 0, 0, x)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(x, weight);
	Mode_Lang(labels[4], lab[4]);
	Native_Free(x);
	if (!ET_Price(engineName, x)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(x, price);
	const nativeVal_t *parts[17] = { lab[0], NULL, t2, NULL, lab[1], NULL, power, NULL, lab[2], NULL, lab[3], NULL, weight,
		NULL, lab[4], NULL, price };
	static const char *const seps[17] = { NULL, ": ", NULL, "\n", NULL, ": ", NULL, " ", NULL, "\n", NULL, ": ", NULL, "\n",
		NULL, ": ", NULL };
	nativeBuf_t b;
	Native_BufInit(&b);
	for (int i = 0; i < 17; i++) {
		if (parts[i]) {
			Native_BufAdd(&b, Native_Text(parts[i]), parts[i]->n);
		} else {
			Native_BufAdd(&b, seps[i], -1);
		}
	}
	Script_SetStr(id, "EngineDesc", "Text", b.p);
	Native_BufFree(&b);
	Script_SetNum(id, "EngineDesc", "XAlignRight", 580);
	double tab = 40 + Script_GetNum(id, "EngineDesc", "SizeX");
	Script_SetNum(id, "EngineDesc", "MinX", 590 - tab + 10);
	Script_SetNum(id, "EngineDescTab", "SizeX", tab - 2);
	Script_SetNum(id, "EngineDescTab", "X", 590 - tab);
	Mode_ItemVisible(id, "EngineDesc", 1);
	Mode_ItemVisible(id, "EngineDescTab", 1);
	return Mode_Done(&t, 1);
}

/* abs(v) of a number (PyNumber_Absolute: the OverflowError of the smallest int) */
static byte ET_Abs(const nativeVal_t *v, nativeVal_t *out) {
	switch (v->type) {
	case NV_INT:
		if (v->i == LONG_MIN) {
			return Native_Raise(NE_OVERFLOW, "integer negation");
		}
		Native_SetInt(out, v->i < 0 ? -v->i : v->i);
		return 1;
	case NV_FLOAT:
		Native_SetFloat(out, fabs(v->f));
		return 1;
	case NV_LONG: {
		const char *s = Native_Text(v);
		Native_SetLong(out, s[0] == '-' ? s + 1 : s, s[0] == '-' ? v->n - 1 : v->n);
		return 1;
	}
	}
	return Native_Raise(NE_TYPE, "bad operand type for abs()");
}

/* cur[1] + cur[2] of a motor of ShipMaker.GetMotor */
static byte ET_MotorSpeed(const nativeVal_t *m, nativeVal_t *out) {
	nativeVal_t a = { 0 }, b = { 0 };
	byte ok = ET_Item(m, 1, &a) && ET_Item(m, 2, &b) && ET_Op(&a, '+', &b, out);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* the money, mass, boost and speed predictions of ShowEngineDesc for engine eng in the bay */
static byte ET_Predict(int id, const nativeVal_t *eng) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bay = Mode_V(&t), *engineName = Mode_V(&t), *name = Mode_V(&t), *money = Mode_V(&t), *slots = Mode_V(&t);
	nativeVal_t *cost = Mode_V(&t), *back = Mode_V(&t), *rest = Mode_V(&t), *left = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *rep = Mode_V(&t), *text = Mode_V(&t), *am = Mode_V(&t), *weight = Mode_V(&t), *mass = Mode_V(&t);
	nativeVal_t *num = Mode_V(&t), *groupSlots = Mode_V(&t), *key = Mode_V(&t), *nm = Mode_V(&t), *w1 = Mode_V(&t);
	nativeVal_t *em = Mode_V(&t), *sm = Mode_V(&t), *newWeight = Mode_V(&t), *mk = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t *next = Mode_V(&t), *sa = Mode_V(&t), *sb = Mode_V(&t);
	if (!ET_Bay(bay) || !ET_ShipUpper("MEng", bay, engineName) || !ET_Engine(eng, 1, name)) {
		return Mode_Done(&t, 0);
	}
	int other = Mode_CmpOp(engineName, name, '!');
	if (other <= 0) {
		return Mode_Done(&t, other == 0);
	}
	Native_SetInt(money, Mode_GetMoney());
	Native_Free(name);
	if (!ET_ShipN("nEng", bay, slots) || !ET_Engine(eng, 1, name) || !ET_SlotsPrice(slots, name, cost)) {
		return Mode_Done(&t, 0);
	}
	int some = ET_CmpS(engineName, "", '!');
	if (some > 0) {
		if (!ET_SlotsPrice(slots, engineName, back) || !ET_Op(cost, '-', back, rest)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(cost);
		Native_Copy(cost, rest);
	}
	if (some < 0) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "MoneyPredict", "Green", 255);
	int poor = ET_Op(money, '-', cost, left) ? Mode_CmpOpI(left, 0, '<') : -1;
	if (poor < 0) {
		return Mode_Done(&t, 0);
	}
	if (poor) {
		Script_SetNum(id, "MoneyPredict", "Green", 0);
	}
	int neg = Mode_CmpOpI(cost, 0, '<');
	if (neg < 0 || (neg && !ET_Abs(cost, a))) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(neg ? a : cost, rep);
	if (!ET_Op(Mode_S(&t, neg ? "+" : "-"), '+', rep, text)) {
		return Mode_Done(&t, 0);
	}
	Script_SetStr(id, "MoneyPredict", "Text", Native_Text(text));
	Mode_ItemVisible(id, "MoneyPredict", 1);
	if (!Mode_ShipGet("AcMass", am) || !Native_ToInt(am, weight)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(mass, 0);
	Native_SetInt(num, 0);
	if (!ET_ShipN("nEng", bay, groupSlots) || !ET_ShipN("MEng", bay, key)) {
		return Mode_Done(&t, 0);
	}
	some = ET_CmpS(key, "", '!');
	if (some > 0) {
		some = ET_Motor(key, 1, 0, mass) && ET_ShipN("NEng", bay, num) ? 1 : -1;
	}
	if (some < 0 || !ET_Op(num, '*', mass, nm) || !ET_Op(weight, '-', nm, w1) || !ET_Engine(eng, 0, em) ||
		!ET_Op(groupSlots, '*', em, sm) || !ET_Op(w1, '+', sm, newWeight)) {
		return Mode_Done(&t, 0);
	}
	int up = Mode_CmpOp(newWeight, weight, '>'), down = up == 0 ? Mode_CmpOp(newWeight, weight, '<') : 0;
	if (up < 0 || down < 0) {
		return Mode_Done(&t, 0);
	}
	Mode_ItemVisible(id, up ? "WeightUp" : down ? "WeightDown" : "WeightEqual", 1);
	Mode_ItemVisible(id, "WeightBack", 1);
	Native_Free(name);
	if (!PORT_CALL(HangarTab, ShowBoostPrediction, NULL, "iv", (long)id, newWeight) || !ET_ShipN("MEng", bay, mk) ||
		!PORT_CALL(ShipMaker, GetMotor, cur, "v", mk) || !ET_Engine(eng, 1, name) ||
		!PORT_CALL(ShipMaker, GetMotor, next, "v", name) || !ET_MotorSpeed(cur, sa) || !ET_MotorSpeed(next, sb)) {
		return Mode_Done(&t, 0);
	}
	int faster = Mode_CmpOp(sa, sb, '<'), slower = 0;
	if (faster == 0) {
		Native_Free(sa);
		Native_Free(sb);
		slower = ET_MotorSpeed(cur, sa) && ET_MotorSpeed(next, sb) ? Mode_CmpOp(sa, sb, '>') : -1;
	}
	if (faster < 0 || slower < 0) {
		return Mode_Done(&t, 0);
	}
	Mode_ItemVisible(id, faster ? "SpeedUp" : slower ? "SpeedDown" : "SpeedEqual", 1);
	Mode_ItemVisible(id, "SpeedBack", 1);
	return Mode_Done(&t, 1);
}

/* ShowEngineDesc(id, control, lostfocus): the description of the engine of the tile, and the money, mass, boost and
 * speed predictions for the bay */
static byte ET_ShowEngineDesc(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *index = Mode_V(&t), *sum = Mode_V(&t), *eng = Mode_V(&t);
	if (!Script_Import(mods) || !ET_LastNum(control, 0, n) || !ET_G("engineIndex", index) || !ET_Op(n, '+', index, sum) ||
		!ET_OpI(sum, '-', 1, eng)) {
		return Mode_Done(&t, 0);
	}
	int count = ET_Len("enginesList");
	int inList = count < 0 ? -1 : Mode_CmpOpI(eng, count, '<');
	if (inList < 0 || (inList && !ET_Desc(id, n, eng))) {
		return Mode_Done(&t, 0);
	}
	if (!inList) {
		Mode_ItemVisible(id, "EngineDesc", 0);
		Mode_ItemVisible(id, "EngineDescTab", 0);
	}
	int l = ET_Len("currentEngineGroup");
	if (l <= 0) {
		return Mode_Done(&t, l == 0);
	}
	return Mode_Done(&t, ET_Predict(id, eng));
}

MODE_IFN(ET_ShowEngineDescPy, 3, ET_ShowEngineDesc(id, &a[1]))
MODE_IFN(ET_HideEngineDesc, 3, ET_Run(id, "hidedesc", 0))

/* ChangeRecycleButtonState(id): the recycle button works only for a bay with engines; the links follow (from
 * Menu import LinkLR: OBJLIB 6, the calls through Mode_LinkCallV on the function values) */
static byte ET_RecycleState(int id) {
	static const char *const mods[] = { "HangarTab", "ShipEdit", NULL };
	nativeVal_t bay = { 0 }, ne = { 0 }, layout = { 0 };
	nativeVal_t linkLR;
	byte ok = Mode_FromV("Menu", "LinkLR", &linkLR) && Script_Import(mods);
	int l = ok ? ET_Len("currentEngineGroup") : -1;
	if (l <= 0) {
		Native_Free(&linkLR);
		return l == 0;
	}
	int has = 0;
	ok = ET_Bay(&bay);
	for (int i = 0; ok && i < 4; i++) {
		int is = Mode_CmpOpI(&bay, i, '=');
		if (is) {
			char var[0x10];
			Str_Printf(var, sizeof(var), "NEng%d", i);
			has = is > 0 && Mode_ShipGet(var, &ne) ? Mode_CmpOpI(&ne, 0, '>') : -1;
			ok = has >= 0;
			break;
		}
	}
	int groups = ok && ET_Layout(&layout) ? Native_Size(&layout) : -1;
	ok = groups >= 0;
	const char *to = has ? "Recycle" : "Exit";
	if (ok && has) {
		Script_SetNum(id, "Recycle", "Highlight", 1);
		Script_SetNum(id, "Recycle", "SpriteIndex", 8);
		Script_SetStr(id, "Recycle", "OnAccept", "EnginesTab.RecycleEngine");
	} else if (ok) {
		Script_SetNum(id, "Recycle", "Highlight", 0);
		Script_SetNum(id, "Recycle", "SpriteIndex", 10);
		Script_SetStr(id, "Recycle", "OnAccept", "");
	}
	/* the bays 2 .. n first, then bay 1 */
	for (int i = 0; ok && i < groups; i++) {
		char name[ET_ITEM];
		ok = ET_GroupName(i < groups - 1 ? i + 2 : 1, name) && Mode_LinkCallV(&linkLR, id, name, to);
	}
	if (ok && groups == 0) {
		char name[ET_ITEM];
		ok = ET_GroupName(1, name) && Mode_LinkCallV(&linkLR, id, name, to);
	}
	ok = ok && (!has || Mode_LinkCallV(&linkLR, id, "Recycle", "Exit"));
	Native_Free(&bay);
	Native_Free(&ne);
	Native_Free(&layout);
	Native_Free(&linkLR);
	return ok;
}

MODE_IFN(ET_ChangeRecycleButtonState, 1, ET_RecycleState(id))

/* UpdateEnginesList(id): which engines of the list fit the bay */
static byte ET_UpdateList(int id) {
	static const char *const mods[] = { "ShipEdit", "ShipMaker", "string", "HangarTab", NULL };
	etLoad_t l = { 0 };
	byte ok = Script_Import(mods) && ET_Load(&l, 0);
	int count = ok ? ET_Len("enginesList") : -1;
	ok = count >= 0;
	for (int i = 0; ok && i < count; i++) {
		ok = ET_Mount(id, i, &l, NULL);
	}
	ET_LoadFree(&l);
	return ok;
}

MODE_IFN(ET_UpdateEnginesList, 1, ET_UpdateList(id))

/* the speed texts of the info panel */
static void ET_SpeedText(int id, int v) {
	static const char *const items[] = { "MaxSpeedText", "MaxSpeed", "BoostRechargeTime", "BoostRechargeTimeUnits",
		"BoostRechargeTimeText" };
	for (int i = 0; i < 5; i++) {
		Mode_ItemVisible(id, items[i], v);
	}
}

/* the new state of the ship: SetShipParams, the info panel, the list, the recycle button and the tiles */
static byte ET_Refresh(int id) {
	nativeVal_t name = { 0 };
	byte ok = Mode_ShipGet("Name", &name) && PORT_CALL(ShipMaker, SetShipParams, NULL, "v", &name) &&
		PORT_CALL(HangarTab, ShowShipInfo, NULL, "i", (long)id) &&
		PORT_OWN(EnginesTab, UpdateEnginesList, NULL, "i", (long)id) &&
		PORT_OWN(EnginesTab, ChangeRecycleButtonState, NULL, "i", (long)id);
	Native_Free(&name);
	return ok;
}

/* the price of the engine of the tile for the bay less the mounted ones: slots * price - slots * price of the
 * mounted */
static byte ET_BayCost(const nativeVal_t *control, const nativeVal_t *bay, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *index = Mode_V(&t), *sum = Mode_V(&t), *eng = Mode_V(&t), *slots = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *engineName = Mode_V(&t), *slots2 = Mode_V(&t), *back = Mode_V(&t);
	if (!ET_LastNum(control, 0, n) || !ET_G("engineIndex", index) || !ET_Op(n, '+', index, sum) || !ET_OpI(sum, '-', 1, eng) ||
		!ET_ShipN("nEng", bay, slots) || !ET_Engine(eng, 1, name) || !ET_SlotsPrice(slots, name, out) ||
		!ET_ShipUpper("MEng", bay, engineName)) {
		return Mode_Done(&t, 0);
	}
	int some = ET_CmpS(engineName, "", '!');
	if (some > 0) {
		nativeVal_t *rest = Mode_V(&t);
		if (!ET_ShipN("nEng", bay, slots2) || !ET_SlotsPrice(slots2, engineName, back) || !ET_Op(out, '-', back, rest)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(out);
		Native_Copy(out, rest);
	}
	return Mode_Done(&t, some >= 0);
}

/* canBeMounted[i] != 2: 1 or 0, -1 with the error */
static int ET_NotMountable(const nativeVal_t *i) {
	nativeVal_t list = { 0 }, v = { 0 };
	int r = ET_G("canBeMounted", &list) && Native_Subscript(&list, i, &v) ? Mode_CmpOpI(&v, 2, '!') : -1;
	Native_Free(&list);
	Native_Free(&v);
	return r;
}

/* ShipEdit.IsEditingMultiplayer() != 1: 1 or 0, -1 with the error */
static int ET_Single(void) {
	nativeVal_t v = { 0 };
	int r = PORT_CALL(ShipEdit, IsEditingMultiplayer, &v, "") ? Mode_CmpOpI(&v, 1, '!') : -1;
	Native_Free(&v);
	return r;
}

/* PutEngine(id, control): the engine of the tile into every slot of the bay, paid for (less the mounted ones) */
static byte ET_PutEngine(int id, const nativeVal_t *control) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "ShipEdit", "string", "Sound", NULL };
	static const char *const digits[] = { "1", "2", "3", "4" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *g = Mode_V(&t), *n = Mode_V(&t), *n1 = Mode_V(&t), *index = Mode_V(&t), *inList = Mode_V(&t);
	nativeVal_t *last = Mode_V(&t), *bay = Mode_V(&t), *cost = Mode_V(&t), *money = Mode_V(&t), *left = Mode_V(&t);
	nativeVal_t *rep = Mode_V(&t), *list = Mode_V(&t), *row = Mode_V(&t), *model = Mode_V(&t), *prev = Mode_V(&t);
	nativeVal_t *nCEG = Mode_V(&t), *slots = Mode_V(&t), *one = Mode_V(&t), *li = Mode_V(&t);
	char group[ET_ITEM];
	if (!Script_Import(mods) || !ET_G("currentEngineGroup", g)) {
		return Mode_Done(&t, 0);
	}
	int l = Native_Size(g);
	if (l <= 0) {
		return Mode_Done(&t, l == 0);
	}
	if (!ET_LastNum(control, 0, n) || !ET_OpI(n, '-', 1, n1) || !ET_G("engineIndex", index) || !ET_Op(n1, '+', index, inList)) {
		return Mode_Done(&t, 0);
	}
	int no = ET_NotMountable(inList);
	if (no) {
		return Mode_Done(&t, no > 0);
	}
	Native_Free(g);
	if (!ET_G("currentEngineGroup", g) || !ET_Item(g, l - 1, last) || !Native_ToInt(last, li) || !ET_OpI(li, '-', 1, bay)) {
		return Mode_Done(&t, 0);
	}
	int single = ET_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		if (!ET_BayCost(control, bay, cost)) {
			return Mode_Done(&t, 0);
		}
		Native_SetInt(money, Mode_GetMoney());
		int poor = ET_Op(money, '-', cost, left) ? Mode_CmpOpI(left, 0, '<') : -1;
		if (poor) {
			return Mode_Done(&t, poor > 0);
		}
		Native_Free(left);
		if (!ET_Op(money, '-', cost, left) || !Mode_SetMoney(left)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(left, rep);
		Script_SetStr(id, "Money", "Text", Native_Text(rep));
	}
	Native_SetInt(one, 1);
	if (!ET_G("enginesList", list) || !Native_Subscript(list, inList, row) || !Native_Subscript(row, one, model) ||
		!ET_ShipUpper("MEng", bay, prev)) {
		return Mode_Done(&t, 0);
	}
	int change = Mode_CmpOp(model, prev, '!');
	if (change < 0 || (change && !Mode_SoundOf("ShipEditLoadEngine"))) {
		return Mode_Done(&t, 0);
	}
	int show = 0;
	Native_Free(g);
	if (!ET_G("currentEngineGroup", g) || !ET_Item(g, l - 1, nCEG)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		int is = ET_CmpS(nCEG, digits[i], '=');
		if (is < 0) {
			return Mode_Done(&t, 0);
		}
		if (is) {
			int no2 = ET_NotMountable(inList);
			if (no2 < 0) {
				return Mode_Done(&t, 0);
			}
			if (!no2) {
				char var[0x10];
				Str_Printf(var, sizeof(var), "MEng%d", i);
				if (!Mode_ShipSet(var, model)) {
					return Mode_Done(&t, 0);
				}
				show = 1;
			}
			break;
		}
	}
	if (!ET_ShipN("nEng", bay, slots) || !ET_Group(group) || !ET_BayHint(id, group, slots)) {
		return Mode_Done(&t, 0);
	}
	if (show) {
		ET_SpeedText(id, 1);
	}
	if (!ET_Refresh(id)) {
		return Mode_Done(&t, 0);
	}
	int can = ET_NotMountable(inList);
	byte ok = can > 0 ? PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "ii", (long)id, 0L) :
		can == 0 && PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "i", (long)id);
	return Mode_Done(&t, ok);
}

MODE_IFN(ET_PutEnginePy, 2, ET_PutEngine(id, &a[1]))

/* RecycleEngine(id, control): the engines of the bay sold */
static byte ET_RecycleEngine(int id) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", "ShipEdit", "Sound", NULL };
	static const char *const digits[] = { "1", "2", "3", "4" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *g = Mode_V(&t), *number = Mode_V(&t), *ni = Mode_V(&t), *bay = Mode_V(&t), *engineName = Mode_V(&t);
	nativeVal_t *slots = Mode_V(&t), *cost = Mode_V(&t), *money = Mode_V(&t), *sum = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *m = Mode_V(&t), *empty = Mode_S(&t, "");
	char group[ET_ITEM];
	if (!Script_Import(mods) || !ET_G("currentEngineGroup", g)) {
		return Mode_Done(&t, 0);
	}
	int len = Native_Size(g);
	if (len < 0 || (len == 0 ? (Native_SetStr(number, "1"), 0) : !ET_Item(g, len - 1, number)) ||
		!Native_ToInt(number, ni) || !ET_OpI(ni, '-', 1, bay) || !ET_ShipN("MEng", bay, engineName)) {
		return Mode_Done(&t, 0);
	}
	int single = ET_Single();
	if (single < 0) {
		return Mode_Done(&t, 0);
	}
	if (single) {
		if (!ET_ShipN("nEng", bay, slots) || !ET_SlotsPrice(slots, engineName, cost)) {
			return Mode_Done(&t, 0);
		}
		Native_SetInt(money, Mode_GetMoney());
		if (!ET_Op(money, '+', cost, sum) || !Mode_SetMoney(sum)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(sum, rep);
		Script_SetStr(id, "Money", "Text", Native_Text(rep));
	}
	if (!Mode_SoundOf("ShipEditUnLoad")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		int is = ET_CmpS(number, digits[i], '=');
		if (is < 0) {
			return Mode_Done(&t, 0);
		}
		if (is) {
			char var[0x10];
			Str_Printf(var, sizeof(var), "MEng%d", i);
			int set = Mode_ShipGet(var, m) ? ET_CmpS(m, "", '!') : -1;
			if (set < 0 || (set && !Mode_ShipSet(var, empty))) {
				return Mode_Done(&t, 0);
			}
			break;
		}
	}
	int none = 1;
	for (int i = 0; i < 4 && none > 0; i++) {
		char var[0x10];
		Str_Printf(var, sizeof(var), "MEng%d", i);
		Native_Free(m);
		none = Mode_ShipGet(var, m) ? ET_CmpS(m, "", '=') : -1;
	}
	if (none < 0) {
		return Mode_Done(&t, 0);
	}
	if (none) {
		ET_SpeedText(id, 0);
	}
	byte ok = ET_Refresh(id) && PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "i", (long)id) &&
		ET_GroupName(1, group);
	if (ok) {
		Script_SetDefault(id, group);
	}
	return Mode_Done(&t, ok);
}

MODE_IFN(ET_RecycleEnginePy, 2, ET_RecycleEngine(id))

/* ShowMoneySellingEngine(id, control, lostfocus): the money the recycle button would give */
static byte ET_ShowMoneySellingEngine(int id) {
	static const char *const mods[] = { "HangarTab", "ShipMaker", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *bay = Mode_V(&t), *type = Mode_V(&t), *slots = Mode_V(&t), *cost = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t);
	if (!Script_Import(mods) || !ET_Bay(bay) || !ET_ShipN("MEng", bay, type)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(type)) {
		if (!ET_ShipN("nEng", bay, slots) || !ET_SlotsPrice(slots, type, cost)) {
			return Mode_Done(&t, 0);
		}
		Script_SetNum(id, "MoneyPredict", "Green", 255);
		Mode_Repr(cost, rep);
		if (!ET_Op(Mode_S(&t, "+"), '+', rep, text)) {
			return Mode_Done(&t, 0);
		}
		Script_SetStr(id, "MoneyPredict", "Text", Native_Text(text));
		Mode_ItemVisible(id, "MoneyPredict", 1);
	}
	return Mode_Done(&t, 1);
}

MODE_IFN(ET_ShowMoneySellingEnginePy, 3, ET_ShowMoneySellingEngine(id))

/* UpdateLayoutsState(id, string.atoi(currentEngineGroup[-1]) - 1, state) */
static byte ET_LayoutState(int id, int state) {
	nativeVal_t g = { 0 }, c = { 0 }, n = { 0 }, index = { 0 };
	byte ok = ET_G("currentEngineGroup", &g) && ET_Item(&g, -1, &c) && PORT_CALL(string, atoi, &n, "v", &c) &&
		ET_OpI(&n, '-', 1, &index) && PORT_OWN(EnginesTab, UpdateLayoutsState, NULL, "ivi", (long)id, &index, (long)state);
	Native_Free(&g);
	Native_Free(&c);
	Native_Free(&n);
	Native_Free(&index);
	return ok;
}

/* currentEngineGroup != "": 1 or 0, -1 with the error */
static int ET_HasGroup(void) {
	nativeVal_t g = { 0 };
	int r = ET_G("currentEngineGroup", &g) ? ET_CmpS(&g, "", '!') : -1;
	Native_Free(&g);
	return r;
}

/* SelectEngineGroup(id, control, setDefault=1): the bay picked, in green, and the list checked against it */
static byte ET_SelectEngineGroup(int id, const nativeVal_t *control, const nativeVal_t *setDefault) {
	static const char *const mods[] = { "HangarTab", "string", NULL };
	static const int green[3] = { 0, 255, 0 };
	static const int blue[3] = { 57, 139, 202 };
	nativeVal_t hint = { 0 }, bay = { 0 }, slots = { 0 };
	char group[ET_ITEM];
	if (!Script_Import(mods) || !ET_Group(group)) {
		return 0;
	}
	Mode_ItemColor(id, group, blue);
	byte ok = Mode_LangUse(MOD, "HINT_Engines6", &hint) && ET_Group(group) && Mode_ScorerPut(id, group, "Hint", &hint);
	Native_Free(&hint);
	int has = ok ? ET_HasGroup() : -1;
	ok = has >= 0 && (!has || ET_LayoutState(id, 0)) && Mode_Put(MOD, "currentEngineGroup", NULL, control);
	has = ok ? ET_HasGroup() : -1;
	ok = has >= 0 && (!has || ET_LayoutState(id, 1)) && ET_Group(group);
	if (ok) {
		Mode_ItemColor(id, group, green);
	}
	has = ok ? ET_HasGroup() : -1;
	ok = has >= 0 && (!has || (ET_Group(group) && ET_Bay(&bay) && ET_ShipN("nEng", &bay, &slots) && ET_BayHint(id, group, &slots)));
	Native_Free(&bay);
	Native_Free(&slots);
	ok = ok && PORT_OWN(EnginesTab, UpdateEnginesList, NULL, "i", (long)id) &&
		PORT_OWN(EnginesTab, ChangeRecycleButtonState, NULL, "i", (long)id) &&
		PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "ii", (long)id, 0L);
	if (ok && Native_True(setDefault)) {
		ok = ET_Group(group);
		if (ok) {
			Script_SetDefault(id, group);
		}
	}
	return ok;
}

static byte ET_SelectEngineGroupPy_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *control, *def = NULL;
	nativeVal_t one;
	int id;
	(void)out;
	Native_SetInt(&one, 1);
	return Mode_KwArity(a, n, "iO|O") && Mode_Parse(a, n, "iO|O", &id, &control, &def) &&
		ET_SelectEngineGroup(id, control, def ? def : &one);
}

static char *ET_SelectEngineGroupPy_N[] = { "id", "control", "setDefault", NULL };
static const char ET_SelectEngineGroupPy_F[] = "iO|O";

/* the method of a list variable of the module called in place (sort and append of the script's lists) */
static byte ET_ListMethod(const char *var, const char *method, const nativeVal_t *arg) {
	return arg ? Mode_VarCall(MOD, var, 1, method, arg) : Mode_VarCall0(MOD, var, 1, method);
}

/* the bay boxes of SetEnginesPanel for every ship that can be edited */
static byte ET_GroupBoxes(int id, const nativeVal_t *ratio) {
	nativeVal_t ships = { 0 };
	if (!Mode_ModVar("ShipEdit", "shipsToEditList", &ships)) {
		return 0;
	}
	int count = Native_Size(&ships);
	Native_Free(&ships);
	byte ok = count >= 0;
	for (int k = 0; ok && k < count; k++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *list = Mode_V(&t), *ship = Mode_V(&t), *layouts = Mode_V(&t), *layout = Mode_V(&t);
		ok = Mode_ModVar("ShipEdit", "shipsToEditList", list) && ET_Item(list, k, ship) &&
			Mode_ModVar("ShipEdit", "shipsEnginesLayouts", layouts) && Native_Subscript(layouts, ship, layout);
		int groups = ok ? Native_Size(layout) : -1;
		ok = groups >= 0;
		for (int i = 0; ok && i < groups; i++) {
			int mark = t.n;
			nativeVal_t *list2 = Mode_V(&t), *ship2 = Mode_V(&t), *num = Mode_V(&t), *rep = Mode_V(&t), *name = Mode_V(&t);
			nativeVal_t *bay = Mode_V(&t), *b0 = Mode_V(&t), *b1 = Mode_V(&t), *b2 = Mode_V(&t), *b3 = Mode_V(&t);
			nativeVal_t *d = Mode_V(&t), *dr = Mode_V(&t), *x = Mode_V(&t), *dw = Mode_V(&t), *w = Mode_V(&t);
			nativeVal_t *h = Mode_V(&t);
			const char *s;
			Native_SetInt(num, i + 1);
			Mode_Repr(num, rep);
			ok = Mode_ModVar("ShipEdit", "shipsToEditList", list2) && ET_Item(list2, k, ship2) &&
				Mode_Cat(&t, name, 3, ship2, Mode_S(&t, "EngineGroup"), rep) && ET_Item(layout, i, bay) &&
				ET_Item(bay, 0, b0) && ET_OpI(b0, '-', 320, d) && ET_Op(d, '*', ratio, dr) && ET_OpI(dr, '+', 320, x) &&
				ET_Item(bay, 1, b1) && ET_Item(bay, 2, b2) && (Native_Free(b0), ET_Item(bay, 0, b0)) &&
				ET_Op(b2, '-', b0, dw) && ET_Op(dw, '*', ratio, w) && ET_Item(bay, 3, b3) &&
				(Native_Free(b1), ET_Item(bay, 1, b1)) && ET_Op(b3, '-', b1, h) && Mode_ItemText(name, &s);
			if (ok) {
				ok = ET_Run(id, "group", 5, "name", name, "x", x, "y", b1, "w", w, "h", h);
			}
			Mode_Release(&t, mark);
		}
		Mode_Done(&t, 1);
	}
	return ok;
}

/* SetEnginesPanel(id): the items of the tab, hidden; the engine list from the save game, by mass */
static byte ET_SetEnginesPanel(int id) {
	static const char *const mods[] = { "Menu", "ShipEdit", "HangarTab", "ShipMaker", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *aspect = Mode_V(&t), *num = Mode_V(&t), *ratio = Mode_V(&t), *empty = Mode_V(&t), *save = Mode_V(&t);
	nativeVal_t *keys = Mode_V(&t);
	if (!Script_Import(mods) || !ET_Run(id, "desc", 0) || !Native_GVarGet("PhysicalAspectRatio", aspect)) {
		return Mode_Done(&t, 0);
	}
	/* 1.3333333333333 in the source; the marshal of the shipped .pyc kept 12 digits */
	Native_SetFloat(num, 1.33333333333);
	Native_SetEmpty(empty, NV_LIST);
	if (!ET_Op(num, '/', aspect, ratio) || !ET_GroupBoxes(id, ratio) ||
		!PORT_OWN(EnginesTab, CreateLayouts, NULL, "i", (long)id) || !Mode_Put(MOD, "enginesList", NULL, empty) ||
		(Mode_SaveVar("Hangar.availableEnginesList", save), !Native_EvalText(MOD, save, keys))) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t key = { 0 }, mass = { 0 }, pair, two;
		ok = Native_SeqItem(keys, i, &key) && ET_Motor(&key, 0, 0, &mass);
		if (ok) {
			Native_SetEmpty(&pair, NV_TUPLE);
			Native_Copy(Native_Append(&pair), &mass);
			Native_Copy(Native_Append(&pair), &key);
			Native_SetInt(&two, 2);
			ok = ET_ListMethod("enginesList", "append", &pair) && ET_ListMethod("canBeMounted", "append", &two);
			Native_Free(&pair);
		}
		Native_Free(&key);
		Native_Free(&mass);
	}
	ok = ok && ET_ListMethod("enginesList", "sort", NULL);
	int count = ok ? ET_Len("enginesList") : -1;
	ok = count >= 0;
	for (int i = 0; ok && i < count; i++) {
		nativeVal_t e = { 0 }, file = { 0 }, a, b;
		Native_SetStr(&a, "2D/ShipEditor/");
		Native_SetStr(&b, ".alpha.tga");
		nativeVal_t f1 = { 0 };
		ok = ET_EngineI(i, 1, &e) && ET_Op(&a, '+', &e, &f1) && ET_Op(&f1, '+', &b, &file);
		nativeVal_t num;
		Native_SetInt(&num, i + 1);
		ok = ok && ET_Run(id, "inlist", 2, "n", &num, "file", &file);
		Native_Free(&e);
		Native_Free(&f1);
		Native_Free(&file);
	}
	for (int i = 0; ok && i < 4; i++) {
		nativeVal_t num, y;
		Native_SetInt(&num, i + 1);
		Native_SetInt(&y, ENGINES_Y + 62 * i);
		ok = ET_Run(id, "slot", 2, "n", &num, "y", &y);
	}
	ok = ok && ET_Run(id, "recycle", 0) && PORT_OWN(EnginesTab, SetRecycleVisible, NULL, "ii", (long)id, 0L) &&
		ET_Run(id, "arrows", 0) && PORT_OWN(EnginesTab, SetPrevNextEngineVisible, NULL, "ii", (long)id, 0L);
	return Mode_Done(&t, ok);
}

MODE_IFN(ET_SetEnginesPanelPy, 1, ET_SetEnginesPanel(id))

/* the value of a Visible flag the file cannot set raises the TypeError of SScorer.Set */
static byte ET_SetVisible(const nativeVal_t *a, int n, const char *first, const char *section) {
	const nativeVal_t *v;
	int id;
	return Mode_Parse(a, n, "iO", &id, &v) &&
		(v->type == NV_INT || v->type == NV_FLOAT || v->type == NV_STR || Mode_ScorerPut(id, first, "Visible", v)) &&
		ET_RunV(id, section, v);
}

/* SetPrevNextEngineVisible(id, bVisible) and SetRecycleVisible(id, bVisible) */
static byte ET_SetPrevNextEngineVisible_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return ET_SetVisible(a, n, "PrevEngine", "arrowsvisible");
}

static byte ET_SetRecycleVisible_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return ET_SetVisible(a, n, "Recycle", "recyclevisible");
}

/* the links of ShowEnginesPanel between the list, the bays, the recycle button and the exit (from Menu import
 * LinkLR, LinkUD, UnLinkUD: OBJLIB 6, the calls through Mode_LinkCallV on the function values) */
static byte ET_ShowLinks(int id, int groups) {
	static const char *const left[] = { "NextEngine", "PrevEngine", "Engine2", "Engine3", "Engine4" };
	char name[ET_ITEM], other[ET_ITEM];
	nativeVal_t linkLR = { 0 }, linkUD = { 0 }, unlinkUD = { 0 };
	byte ok = Mode_FromV("Menu", "LinkLR", &linkLR) && Mode_FromV("Menu", "LinkUD", &linkUD) &&
		Mode_FromV("Menu", "UnLinkUD", &unlinkUD) && ET_GroupName(1, name);
	for (int i = 0; ok && i < 5; i++) {
		ok = Mode_LinkCallV(&linkLR, id, left[i], name);
	}
	for (int i = groups; ok && i > 0; i--) {
		ok = ET_GroupName(i, name) && Mode_LinkCallV(&linkLR, id, "Engine1", name) &&
			Mode_LinkCallV(&linkLR, id, name, "Recycle");
	}
	ok = ok && Mode_LinkCallV(&linkLR, id, "Recycle", "Exit");
	for (int i = 0; ok && i < groups - 1; i++) {
		ok = ET_GroupName(i + 1, name) && ET_GroupName(i + 2, other) && Mode_LinkCallV(&linkUD, id, name, other);
	}
	/* currentEngineGroup = ...: a local of the script, the module variable stays */
	ok = ok && ET_GroupName(1, name);
	Native_Free(&linkLR);
	Native_Free(&linkUD);
	Native_Free(&unlinkUD);
	return ok;
}

/* ShowEnginesPanel(id): the tab with the list from its start and the bays of the ship (from Menu import LinkUD:
 * OBJLIB 6, the calls through Mode_LinkCallV on the function values) */
static byte ET_ShowEnginesPanel(int id) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", NULL };
	static const char *const mods2[] = { "ShipMaker", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *layout = Mode_V(&t), *key = Mode_V(&t);
	etLoad_t l = { 0 };
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	nativeVal_t linkUD;
	byte ok = Mode_FromV("Menu", "LinkUD", &linkUD) && Script_Import(mods) &&
		Mode_Put(MOD, "engineIndex", NULL, &zero) && ET_Layout(layout);
	int groups = ok ? Native_Size(layout) : -1;
	ok = groups >= 0 && PORT_OWN(EnginesTab, SetRecycleVisible, NULL, "ii", (long)id, 1L);
	if (ok) {
		Script_SetNum(id, "Recycle", "Highlight", 0);
		Script_SetNum(id, "Recycle", "SpriteIndex", 10);
		Script_SetStr(id, "Recycle", "OnAccept", "");
	}
	int count = ok ? ET_Len("enginesList") : -1;
	ok = count >= 0 && Mode_LinkCallV(&linkUD, id, "PrevEngine", "Engine1");
	for (int i = 0; ok && i < count; i++) {
		char name[0x20], next[0x20];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		Mode_ItemVisible(id, name, 1);
		if (i + 2 <= count) {
			Str_Printf(next, sizeof(next), "Engine%d", i + 2);
			ok = Mode_LinkCallV(&linkUD, id, name, next);
		} else {
			Script_SetStr(id, name, "Down", "");
		}
	}
	ok = ok && (count < 4 || Mode_LinkCallV(&linkUD, id, "Engine4", "NextEngine")) &&
		PORT_OWN(EnginesTab, SetPrevNextEngineVisible, NULL, "ii", (long)id, 1L);
	if (count > 4) {
		count = 4;
	}
	for (int i = 0; ok && i < count; i++) {
		/* the name of the script, with the engine instead of the number: an item that is not there */
		nativeVal_t e = { 0 }, name = { 0 };
		modeVals_t u = { .n = 0 };
		const char *s;
		ok = ET_EngineI(i, 1, &e) && Mode_Cat(&u, &name, 3, Mode_S(&u, "Engine"), &e, Mode_S(&u, "InList")) &&
			Mode_ItemText(&name, &s);
		if (ok) {
			Mode_ItemVisible(id, s, 1);
		}
		Mode_Done(&u, 1);
		Native_Free(&e);
		Native_Free(&name);
	}
	ok = ok && Script_Import(mods2) && ET_LoadMasses(&l);
	if (ok) {
		Native_SetInt(&l.mass, 0);
		ok = Mode_ShipGet("MEng0", key);
	}
	int some = ok ? ET_CmpS(key, "", '!') : -1;
	if (some > 0) {
		Native_Free(&l.mass);
		some = ET_Motor(key, 1, 0, &l.mass) ? 1 : -1;
	}
	ok = some >= 0 && Mode_ShipTable("ShipValues", 0, &l.minMass);
	for (int i = 0; ok && i < count; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%dInList", i + 1);
		Mode_ItemVisible(id, name, 1);
		Script_SetNum(id, name, "X", 36);
		Script_SetNum(id, name, "Y", ENGINES_Y + 64 * i);
		ok = ET_Mount(id, i, &l, layout);
	}
	ok = ok && ET_ShowLinks(id, groups) && PORT_OWN(EnginesTab, ChangeRecycleButtonState, NULL, "i", (long)id);
	if (ok) {
		Script_SetNum(id, "NextEngine", "W", 25);
		Script_SetNum(id, "NextEngine", "H", 25);
		Script_SetNum(id, "NextEngine", "SpriteIndex", 3);
		Script_SetNum(id, "PrevEngine", "W", 0);
		Script_SetNum(id, "PrevEngine", "H", 0);
		Script_SetNum(id, "PrevEngine", "SpriteIndex", 2);
		Script_SetStr(id, "Engine1", "Up", "ShipEditTabEngines");
	}
	int len = ok ? ET_Len("enginesList") : -1;
	ok = len >= 0;
	if (ok && len < 5) {
		Script_SetNum(id, "NextEngine", "W", 0);
		Script_SetNum(id, "NextEngine", "H", 0);
		Script_SetNum(id, "NextEngine", "SpriteIndex", 2);
		Script_SetStr(id, "Engine4", "Down", "");
	}
	ok = ok && PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "ii", (long)id, 0L);
	ET_LoadFree(&l);
	Native_Free(&linkUD);
	return Mode_Done(&t, ok);
}

MODE_IFN(ET_ShowEnginesPanelPy, 1, ET_ShowEnginesPanel(id))

/* the eight bay icons 0A .. 3B */
static const char *ET_IconName(char *out, int size, int i, int j) {
	Str_Printf(out, size, "%d%c", i, "AB"[j]);
	return out;
}

/* CreateLayouts(id): the bay icons with their lines */
static byte ET_CreateLayouts(int id) {
	byte ok = Mode_Import("HangarTab");
	for (int i = 0; ok && i < 4; i++) {
		for (int j = 0; ok && j < 2; j++) {
			char n[8];
			nativeVal_t icon;
			Native_SetStr(&icon, ET_IconName(n, sizeof(n), i, j));
			ok = ET_Run(id, "icons", 1, "n", &icon);
		}
	}
	return ok;
}

MODE_IFN(ET_CreateLayoutsPy, 1, ET_CreateLayouts(id))

/* Rotate of a bay icon: 0 for a bay of one slot, else pointing out from the middle of the screen */
static int ET_Rotate(int j, const nativeVal_t *slots, double u) {
	if (j == 0 && Mode_CmpOpI(slots, j + 1, '=') > 0) {
		return 0;
	}
	return u > 320 ? 1 : 3;
}

/* the number of a place the engine gave */
static double ET_Num(const nativeVal_t *v) {
	return v->type == NV_FLOAT ? v->f : v->type == NV_INT ? (double)v->i : 0.0;
}

/* the screen place of HangarTab.prevShip.PEngSlot<n>: 1 with u, v */
static byte ET_SlotPos(const char *n, double *u, double *v) {
	char var[0x20];
	nativeVal_t pos = { 0 }, uv = { 0 }, pair[2];
	Str_Printf(var, sizeof(var), "PEngSlot%s", n);
	byte ok = Mode_ShipGet(var, &pos) && Mode_Get2DPos(&pos, &uv) && Mode_Unpack(&uv, 2, pair);
	if (ok) {
		*u = ET_Num(&pair[0]);
		*v = ET_Num(&pair[1]);
		Native_Free(&pair[0]);
		Native_Free(&pair[1]);
	}
	Native_Free(&pos);
	Native_Free(&uv);
	return ok;
}

/* nEngSlots[i] of the module */
static byte ET_SlotsOf(long i, nativeVal_t *out) {
	nativeVal_t list = { 0 };
	byte ok = ET_G("nEngSlots", &list) && ET_Item(&list, i, out);
	Native_Free(&list);
	return ok;
}

/* the icons of slot j of bay i (n its name) at the slot of the ship on the screen, or hidden */
static byte ET_Icon(int id, int i, int j, const char *n) {
	nativeVal_t slots = { 0 }, jv;
	char name[0x30];
	Native_SetInt(&jv, j);
	int used = ET_SlotsOf(i, &slots) ? Mode_CmpOp(&jv, &slots, '<') : -1;
	Native_Free(&slots);
	if (used > 0) {
		double u, v;
		if (!ET_SlotPos(n, &u, &v)) {
			return 0;
		}
		static const char *const prefix[2] = { "IconEngine", "IconEngineGlow" };
		for (int k = 0; k < 2; k++) {
			Str_Printf(name, sizeof(name), "%s%s", prefix[k], n);
			Mode_ItemVisible(id, name, k == 0);
			Script_SetNum(id, name, "X", u);
			Script_SetNum(id, name, "Y", v);
			Script_SetNum(id, name, "PivotX", 0.5);
			Script_SetNum(id, name, "PivotY", 0.5);
			if (!ET_SlotsOf(i, &slots)) {
				return 0;
			}
			Script_SetNum(id, name, "Rotate", ET_Rotate(j, &slots, u));
			Native_Free(&slots);
		}
	} else if (used == 0) {
		Str_Printf(name, sizeof(name), "IconEngine%s", n);
		Mode_ItemVisible(id, name, 0);
		Str_Printf(name, sizeof(name), "IconEngineGlow%s", n);
		Mode_ItemVisible(id, name, 0);
	}
	return used >= 0;
}

/* the lines between the two slots of bay i */
static byte ET_Lines(int id, int i) {
	double u1, v1, u2, v2, minx, maxx, miny, maxy;
	char n[8], name[0x30];
	ET_IconName(n, sizeof(n), i, 0);
	if (!ET_SlotPos(n, &u1, &v1)) {
		return 0;
	}
	/* the lines take the name of slot B, the last n of the script */
	ET_IconName(n, sizeof(n), i, 1);
	if (!ET_SlotPos(n, &u2, &v2)) {
		return 0;
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
	Mode_ItemVisible(id, name, 1);
	Script_SetNum(id, name, "X", minx + offx);
	Script_SetNum(id, name, "Y", v1);
	Script_SetNum(id, name, "ScaleX", sizex / 2 - offx);
	Script_SetNum(id, name, "ScaleY", 2);
	Str_Printf(name, sizeof(name), "IconEngineLineC%s", n);
	Mode_ItemVisible(id, name, 1);
	Script_SetNum(id, name, "X", minx + sizex / 2);
	Script_SetNum(id, name, "Y", miny);
	Script_SetNum(id, name, "ScaleX", 2);
	Script_SetNum(id, name, "ScaleY", sizey + 2);
	if (maxy == miny) {
		Mode_ItemVisible(id, name, 0);
	}
	Str_Printf(name, sizeof(name), "IconEngineLineR%s", n);
	Mode_ItemVisible(id, name, 1);
	Script_SetNum(id, name, "X", maxx - sizex / 2);
	Script_SetNum(id, name, "Y", v2);
	Script_SetNum(id, name, "ScaleX", sizex / 2 - offx);
	Script_SetNum(id, name, "ScaleY", 2);
	return 1;
}

/* UpdateLayouts(id): the bay icons on the slots of the ship on the screen, the lines between the two slots of a bay */
static byte ET_UpdateLayouts(int id) {
	static const char *const mods[] = { "HangarTab", "ShipEdit", NULL };
	nativeVal_t tab = { 0 };
	int other = Script_Import(mods) && Mode_ModVar("ShipEdit", "prevTab", &tab) ? ET_CmpS(&tab, "ShipEditTabEngines", '!') : -1;
	Native_Free(&tab);
	if (other) {
		return other > 0;
	}
	for (int i = 0; i < 4; i++) {
		char var[0x10], n[8];
		nativeVal_t v = { 0 }, idx, slots = { 0 };
		Str_Printf(var, sizeof(var), "nEng%d", i);
		Native_SetInt(&idx, i);
		byte ok = Mode_ShipGet(var, &v) && Mode_DictPut(MOD, "nEngSlots", NULL, &idx, &v);
		Native_Free(&v);
		for (int j = 0; ok && j < 2; j++) {
			ok = ET_Icon(id, i, j, ET_IconName(n, sizeof(n), i, j));
		}
		int two = ok && ET_SlotsOf(i, &slots) ? Mode_CmpOpI(&slots, 2, 'g') : -1;
		Native_Free(&slots);
		if (two < 0 || (two && !ET_Lines(id, i))) {
			return 0;
		}
	}
	return 1;
}

MODE_IFN(ET_UpdateLayoutsPy, 1, ET_UpdateLayouts(id))

static const char *const et_iconParts[5] = { "IconEngine", "IconEngineGlow", "IconEngineLineL", "IconEngineLineC",
	"IconEngineLineR" };

/* HideLayouts(id) */
static byte ET_HideLayouts_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
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
	return 1;
}

/* UpdateLayoutsState(id, index, state): the icons of bay index active (orange) or not (blue) */
static byte ET_UpdateLayoutsState(int id, const nativeVal_t *index, const nativeVal_t *state) {
	static const int on[4] = { 255, 200, 45, 255 };
	static const int off[4] = { 75, 102, 167, 255 };
	for (int j = 0; j < 2; j++) {
		char n[0x20], name[0x40];
		ET_NameIn(n, sizeof(n), "", index, j ? "B" : "A");
		int active = Mode_CmpOpI(state, 1, '=');
		if (active < 0) {
			return 0;
		}
		const int *c = active ? on : off;
		Str_Printf(name, sizeof(name), "IconEngine%s", n);
		Script_SetNum(id, name, "SpriteIndex", active ? 36 : 35);
		for (int k = 2; k < 5; k++) {
			Str_Printf(name, sizeof(name), "%s%s", et_iconParts[k], n);
			Mode_ItemColor(id, name, c);
			Script_SetNum(id, name, "Alpha", c[3]);
		}
	}
	return 1;
}

static byte ET_UpdateLayoutsStatePy_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *index, *state;
	int id;
	(void)out;
	return Mode_Parse(a, n, "iOO", &id, &index, &state) && ET_UpdateLayoutsState(id, index, state);
}

/* UpdateLayoutsGlow(id, control): the icons of the bay control glow, the others not */
static byte ET_UpdateLayoutsGlow(int id, const nativeVal_t *control) {
	nativeVal_t tv = { 0 }, at = { 0 }, c = { 0 }, n = { 0 };
	if (!Mode_Import("string")) {
		return 0;
	}
	Native_SetStr(&tv, "");
	int some = ET_CmpS(control, "", '!');
	byte ok = some >= 0;
	if (some > 0) {
		int found = PORT_CALL(string, find, &at, "vs", control, "EngineGroup") ? Mode_CmpOpI(&at, -1, '!') : -1;
		if (found > 0) {
			ok = ET_Item(control, -1, &c) && PORT_CALL(string, atoi, &n, "v", &c) && (Native_Free(&tv), 1) &&
				ET_OpI(&n, '-', 1, &tv);
		}
		ok = ok && found >= 0;
	}
	for (int i = 0; ok && i < 4; i++) {
		nativeVal_t slots = { 0 }, iv;
		long count = 0;
		ok = ET_SlotsOf(i, &slots) && Mode_Range(&slots, &count);
		Native_Free(&slots);
		Native_SetInt(&iv, i);
		for (long j = 0; ok && j < count; j++) {
			char name[0x30];
			if (j >= 2) {
				ok = Native_Raise(NE_INDEX, "tuple index out of range");
				break;
			}
			int glow = Mode_CmpOp(&iv, &tv, '=');
			ok = glow >= 0;
			if (ok) {
				const char *render = glow ? "!ScorerSpriteGlow" : "";
				Str_Printf(name, sizeof(name), "IconEngineGlow%d%c", i, "AB"[j]);
				Mode_ItemVisible(id, name, glow);
				Script_SetStr(id, name, "OnRender", render);
				for (int k = 2; k < 5; k++) {
					Str_Printf(name, sizeof(name), "%s%d%c", et_iconParts[k], i, "AB"[j]);
					Script_SetStr(id, name, "OnRender", render);
				}
			}
		}
	}
	Native_Free(&tv);
	Native_Free(&at);
	Native_Free(&c);
	Native_Free(&n);
	return ok;
}

MODE_IFN(ET_UpdateLayoutsGlowPy, 2, ET_UpdateLayoutsGlow(id, &a[1]))

/* LayoutsGainFocus(id, control, lostfocus) and LayoutsLostFocus(id, control, gainfocus) */
MODE_IFN(ET_LayoutsGainFocus, 3, PORT_OWN(EnginesTab, UpdateLayoutsGlow, NULL, "iv", (long)id, &a[1]))
MODE_IFN(ET_LayoutsLostFocus, 3, PORT_OWN(EnginesTab, UpdateLayoutsGlow, NULL, "iv", (long)id, &a[2]))

/* HideEnginesPanel(id) (from Menu import UnLinkLR: OBJLIB 6, the call through Mode_LinkCallV on the function value) */
static byte ET_HideEnginesPanel(int id) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", NULL };
	nativeVal_t ship = { 0 }, none, layout = { 0 };
	Native_SetNone(&none);
	int some = Script_Import(mods) && Mode_PrevShip(&ship) ? Mode_CmpOp(&ship, &none, '!') : -1;
	Native_Free(&ship);
	if (some > 0) {
		int groups = ET_Layout(&layout) ? Native_Size(&layout) : -1;
		Native_Free(&layout);
		some = groups >= 0 ? 1 : -1;
		for (int i = 0; some > 0 && i < groups; i++) {
			char name[ET_ITEM];
			some = ET_GroupName(i + 1, name) ? 1 : -1;
			if (some > 0) {
				Mode_ItemVisible(id, name, 0);
				Mode_ItemColor(id, name, et_normal);
			}
		}
	}
	byte ok = some >= 0 && PORT_OWN(EnginesTab, HideLayouts, NULL, "i", (long)id) && ET_Run(id, "hidepredict", 0) &&
		PORT_OWN(EnginesTab, SetRecycleVisible, NULL, "ii", (long)id, 0L);
	if (!ok) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%d", i + 1);
		Mode_ItemVisible(id, name, 0);
	}
	int count = PORT_OWN(EnginesTab, SetPrevNextEngineVisible, NULL, "ii", (long)id, 0L) ? ET_Len("enginesList") : -1;
	if (count < 0) {
		return 0;
	}
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < count; i++) {
			char name[0x20];
			Str_Printf(name, sizeof(name), "Engine%dInList", i + 1);
			Mode_ItemVisible(id, name, 0);
		}
	}
	nativeVal_t unlinkLR;
	ok = Mode_FromV("Menu", "UnLinkLR", &unlinkLR) && Mode_LinkCallV(&unlinkLR, id, "Recycle", "Exit");
	Native_Free(&unlinkLR);
	return ok;
}

MODE_IFN(ET_HideEnginesPanelPy, 1, ET_HideEnginesPanel(id))

/* the four engines shown from first on: their places and colours against the load */
static byte ET_ShowFrom(int id, long first, const etLoad_t *l) {
	for (long i = first, j = 0; i < first + 4; i++, j++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
		Mode_ItemVisible(id, name, 1);
		Script_SetNum(id, name, "X", 36);
		Script_SetNum(id, name, "Y", ENGINES_Y + 64 * j);
		if (!ET_Mount(id, i, l, NULL)) {
			return 0;
		}
	}
	return 1;
}

/* the imports of the scroll functions and the load of the bay */
static byte ET_ScrollLoad(etLoad_t *l) {
	static const char *const mods[] = { "ShipMaker", "string", "ShipEdit", "HangarTab", NULL };
	return Script_Import(mods) && ET_Load(l, 1);
}

/* range(engineIndex + plus) */
static byte ET_IndexRange(long plus, long *out) {
	nativeVal_t index = { 0 }, sum = { 0 };
	byte ok = ET_G("engineIndex", &index) && (plus ? ET_OpI(&index, '+', plus, &sum) : (Native_Copy(&sum, &index), 1)) &&
		Mode_Range(&sum, out);
	Native_Free(&index);
	Native_Free(&sum);
	return ok;
}

/* the InList sprites from first to the end of the list hidden */
static void ET_HideFrom(int id, long first, long end) {
	for (long i = first; i < end; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Engine%ldInList", i + 1);
		Mode_ItemVisible(id, name, 0);
	}
}

/* engineIndex = engineIndex + d */
static byte ET_Step(long d) {
	nativeVal_t index = { 0 }, n = { 0 };
	byte ok = ET_G("engineIndex", &index) && ET_OpI(&index, d < 0 ? '-' : '+', d < 0 ? -d : d, &n) &&
		Mode_Put(MOD, "engineIndex", NULL, &n);
	Native_Free(&index);
	Native_Free(&n);
	return ok;
}

/* engineIndex op i: 1 or 0, -1 with the error */
static int ET_IndexCmp(long plus, long i, char op) {
	nativeVal_t index = { 0 }, sum = { 0 };
	int r = ET_G("engineIndex", &index) && (plus ? ET_OpI(&index, '+', plus, &sum) : (Native_Copy(&sum, &index), 1)) ?
		Mode_CmpOpI(&sum, i, op) : -1;
	Native_Free(&index);
	Native_Free(&sum);
	return r;
}

/* the scroll arrow control hidden at the end of the list, the default on the engine at that end */
static byte ET_ArrowEnd(int id, const nativeVal_t *control, const char *engine) {
	const char *item;
	if (!Mode_ItemText(control, &item)) {
		return 0;
	}
	Script_SetNum(id, item, "W", 0);
	Script_SetNum(id, item, "H", 0);
	Script_SetNum(id, item, "SpriteIndex", 2);
	Script_SetDefault(id, engine);
	return 1;
}

/* ScrollEngineListUp(id, control): the list one engine up */
static byte ET_ScrollUp(int id, const nativeVal_t *control) {
	int count = ET_Len("enginesList");
	if (count <= 4) {
		return count >= 0;
	}
	int more = ET_IndexCmp(0, 0, '>');
	if (more > 0) {
		etLoad_t l = { 0 };
		long first = 0;
		byte ok = ET_Step(-1) && ET_ScrollLoad(&l) && ET_IndexRange(0, &first) && ET_ShowFrom(id, first, &l) &&
			ET_IndexRange(4, &first);
		ET_LoadFree(&l);
		count = ok ? ET_Len("enginesList") : -1;
		if (count < 0) {
			return 0;
		}
		ET_HideFrom(id, first, count);
	} else if (more < 0) {
		return 0;
	}
	Script_SetNum(id, "PrevEngine", "W", 25);
	Script_SetNum(id, "PrevEngine", "H", 25);
	Script_SetNum(id, "NextEngine", "W", 25);
	Script_SetNum(id, "NextEngine", "H", 30);
	Script_SetNum(id, "PrevEngine", "SpriteIndex", 3);
	Script_SetNum(id, "NextEngine", "SpriteIndex", 3);
	int top = ET_IndexCmp(0, 0, '=');
	if (top < 0 || (top && !ET_ArrowEnd(id, control, "Engine1"))) {
		return 0;
	}
	if (top) {
		Script_SetStr(id, "Engine1", "Up", "ShipEditTabEngines");
	}
	Script_SetStr(id, "Engine4", "Down", "NextEngine");
	return PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "ii", (long)id, 0L);
}

MODE_IFN(ET_ScrollEngineListUp, 2, ET_ScrollUp(id, &a[1]))

/* ScrollEngineListDown(id, control): the list one engine down */
static byte ET_ScrollDown(int id, const nativeVal_t *control) {
	int count = ET_Len("enginesList");
	if (count <= 4) {
		return count >= 0;
	}
	nativeVal_t cv, index = { 0 }, rest = { 0 };
	Native_SetInt(&cv, count);
	int more = ET_G("engineIndex", &index) && ET_Op(&cv, '-', &index, &rest) ? Mode_CmpOpI(&rest, 4, '>') : -1;
	Native_Free(&index);
	Native_Free(&rest);
	if (more > 0) {
		/* the loads are read before the hiding, as in the script */
		etLoad_t l = { 0 };
		long first = 0, from4 = 0;
		byte ok = ET_Step(1) && ET_ScrollLoad(&l) && ET_IndexRange(0, &first);
		if (ok) {
			ET_HideFrom(id, 0, first);
		}
		int len = ok && ET_IndexRange(4, &from4) ? ET_Len("enginesList") : -1;
		if (len >= 0) {
			ET_HideFrom(id, from4, len);
		}
		ok = len >= 0 && ET_IndexRange(0, &first) && ET_ShowFrom(id, first, &l);
		ET_LoadFree(&l);
		if (!ok) {
			return 0;
		}
	} else if (more < 0) {
		return 0;
	}
	if (!PORT_OWN(EnginesTab, HighlightCurrentMountedEngine, NULL, "ii", (long)id, 0L)) {
		return 0;
	}
	Script_SetNum(id, "PrevEngine", "W", 25);
	Script_SetNum(id, "PrevEngine", "H", 25);
	Script_SetNum(id, "NextEngine", "W", 25);
	Script_SetNum(id, "NextEngine", "H", 25);
	Script_SetNum(id, "PrevEngine", "SpriteIndex", 3);
	Script_SetNum(id, "NextEngine", "SpriteIndex", 3);
	int end = ET_IndexCmp(4, count, 'g');
	if (end < 0 || (end && !ET_ArrowEnd(id, control, "Engine4"))) {
		return 0;
	}
	if (end) {
		Script_SetStr(id, "Engine4", "Down", "");
	}
	int down = ET_IndexCmp(0, 0, '>');
	if (down > 0) {
		Script_SetNum(id, "PrevEngine", "W", 25);
		Script_SetNum(id, "PrevEngine", "H", 25);
		Script_SetNum(id, "PrevEngine", "SpriteIndex", 3);
		Script_SetStr(id, "Engine1", "Up", "PrevEngine");
	}
	return down >= 0;
}

MODE_IFN(ET_ScrollEngineListDown, 2, ET_ScrollDown(id, &a[1]))

const portFn_t g_EnginesTabMethods[] = {
	PORT_FNK(HighlightCurrentMountedEngine, ET_HighlightCurrentMountedEngine),
	PORT_FN(ShowEngineDesc, ET_ShowEngineDescPy),
	PORT_FN(HideEngineDesc, ET_HideEngineDesc),
	PORT_FN(ChangeRecycleButtonState, ET_ChangeRecycleButtonState),
	PORT_FN(UpdateEnginesList, ET_UpdateEnginesList),
	PORT_FN(PutEngine, ET_PutEnginePy),
	PORT_FN(RecycleEngine, ET_RecycleEnginePy),
	PORT_FN(ShowMoneySellingEngine, ET_ShowMoneySellingEnginePy),
	PORT_FNK(SelectEngineGroup, ET_SelectEngineGroupPy),
	PORT_FN(SetEnginesPanel, ET_SetEnginesPanelPy),
	PORT_FN(SetPrevNextEngineVisible, ET_SetPrevNextEngineVisible),
	PORT_FN(SetRecycleVisible, ET_SetRecycleVisible),
	PORT_FN(ShowEnginesPanel, ET_ShowEnginesPanelPy),
	PORT_FN(CreateLayouts, ET_CreateLayoutsPy),
	PORT_FN(UpdateLayouts, ET_UpdateLayoutsPy),
	PORT_FN(HideLayouts, ET_HideLayouts),
	PORT_FN(UpdateLayoutsState, ET_UpdateLayoutsStatePy),
	PORT_FN(UpdateLayoutsGlow, ET_UpdateLayoutsGlowPy),
	PORT_FN(LayoutsGainFocus, ET_LayoutsGainFocus),
	PORT_FN(LayoutsLostFocus, ET_LayoutsLostFocus),
	PORT_FN(HideEnginesPanel, ET_HideEnginesPanelPy),
	PORT_FN(ScrollEngineListUp, ET_ScrollEngineListUp),
	PORT_FN(ScrollEngineListDown, ET_ScrollEngineListDown),
	PORT_FNEND
};

PORTS(EnginesTab)
