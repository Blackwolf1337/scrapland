/*
 * The MisItems and CrazyWing modules in C (scripts/missions/misitems.py and crazywing.py, issue #2 step 8a). The
 * items of the missions: the coins a dead NPC or ship drops (their model by the amount), the boxes of a ship, the
 * batteries of the Gears and the coins a dazed character loses. The crazy wind of the indoor levels: now and then a
 * character near the player goes crazy for a while, and a photo of it pays a prize (all of them the big prize).
 *
 * The constants and lists stay in the modules (MONEY_TIME, MoneyQuantos, the coin radii, DazedReward, CrazyChars, the
 * times and prizes, CrazyWingList) with the module code (the save variables of the crazy wind); the functions read
 * them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. The variables are read where the script reads them (its globals, Init.Outdoor and
 * Init.SuperDealModule as attributes, OBJLIB 5); CrazyWingList is appended to in place. The module's own functions
 * the script calls as its globals (GetMoneyModel, GetObjModel, CreateItem, CreateCoin, GetTargetReward, the Activate
 * functions, InitCrazyWing, SetCrazy, DeCrazy, AllInList), Items, SaveGame.SaveMoney and the functions the scheduler
 * keeps stay by name (OBJLIB 3, 4). Entities are handle values (OBJLIB 7): the engine gives them to the callbacks and
 * the scripts pass them on.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* Scrap.Rand(a, b) as the port calls it (the C directly) */
static void Mi_Rand(double a, double b, nativeVal_t *out) {
	Native_SetFloat(out, Scrap_RandRange(a, b));
}

/* Scrap.GetTime() + v of the script */
static byte Mi_TimePlus(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t now;
	Native_SetFloat(&now, Mode_Time());
	return Native_Arith('+', &now, v, out);
}

/* Init.Outdoor (and the other variables of Init the functions read as its attributes) */
static byte Mi_Init(const char *var, nativeVal_t *out) {
	return Mode_ModVar("Init", var, out);
}

/* SSound.PlaySound(itm.Pos, a[0], a[1]) with a = Sound.ItemTake[key] */
static byte Mi_TakeSound(const nativeVal_t *itm, const char *key) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *table = Mode_V(&t), *a = Mode_V(&t), *pos = Mode_V(&t), *snd = Mode_V(&t), *vol = Mode_V(&t);
	byte ok = Mode_ModVar("Sound", "ItemTake", table) && Native_Subscript(table, Mode_S(&t, key), a) &&
		Mode_VGet(itm, "Pos", pos) && Native_Subscript(a, Mode_I(&t, 0), snd) && Native_Subscript(a, Mode_I(&t, 1), vol) &&
		Mode_PlaySound(pos, snd, vol);
	return Mode_Done(&t, ok);
}

/* Scrap.AddScheduledFunc(when, h.Del, (), name): the item goes at when */
static byte Mi_DelLater(const nativeVal_t *h, const nativeVal_t *when, const char *name) {
	nativeVal_t nm;
	PyObject *del = Mode_VBound(h, "Del");
	if (!del) {
		return 0;
	}
	Native_SetStr(&nm, name);
	byte ok = Mode_Schedule(when, del, NULL, 0, &nm);
	Py_DECREF(del);
	return ok;
}

/* Init(): the models of the coins, indoors the extra life and the battery too */
static byte Mi_InitModels(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *q = Mode_V(&t), *out = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_G("MisItems", "MoneyQuantos", q)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(q);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n + 1; i++) {
		int mark = t.n;
		nativeVal_t *money = Mode_V(&t), *model = Mode_V(&t);
		if (i == 0) {
			Native_SetInt(money, 1);
		} else {
			ok = Mode_Read("MisItems", "MoneyQuantos", q) && Native_Subscript(q, Mode_I(&t, i - 1), money);
		}
		ok = ok && Native_CallGlobal("MisItems", "GetMoneyModel", model, "v", money) && Mode_PreloadOnes(model);
		Mode_Release(&t, mark);
	}
	if (ok && Mi_Init("Outdoor", out) && !Native_True(out)) {
		nativeVal_t *model = Mode_V(&t), *model2 = Mode_V(&t);
		ok = Native_CallGlobal("MisItems", "GetObjModel", model, "s", "ExtraLife") && Mode_PreloadOnes(model) &&
			Native_CallGlobal("MisItems", "GetObjModel", model2, "s", "Battery") && Mode_PreloadOnes(model2);
	}
	return Mode_Done(&t, ok && !PyErr_Occurred());
}

/* the kind of box a dead ship leaves for the player (what the ship needs most); 0 with the error */
static byte Mi_BoxKind(modeVals_t *t, nativeVal_t *typo) {
	nativeVal_t *usr = Mode_V(t), *rnd = Mode_V(t), *life = Mode_V(t), *min = Mode_V(t), *ratio = Mode_V(t), *need = Mode_V(t);
	nativeVal_t *kinds = Mode_V(t);
	if (!Mode_Import("Items") || !Mode_UsrHandle(usr)) {
		return 0;
	}
	int car = Native_True(usr) ? Mode_VIsType(usr, "Car") : 0;
	if (car < 0) {
		return 0;
	}
	if (!car) {
		Native_SetStr(typo, "Life");
		return 1;
	}
	Mi_Rand(0.25, 0.5, rnd);
	if (!Mode_VGet(usr, "Life", life) || !Mode_VGet(usr, "MinLife", min) || !Native_Arith('/', life, min, ratio)) {
		return 0;
	}
	int low = Native_Compare(rnd, ratio) > 0;
	if (PyErr_Occurred()) {
		return 0;
	}
	if (low) {
		Native_SetStr(typo, "Life");
		return 1;
	}
	Native_SetEmpty(kinds, NV_TUPLE);
	Native_SetStr(Native_Append(kinds), "Part");
	Native_SetStr(Native_Append(kinds), "Miss");
	Native_SetStr(Native_Append(kinds), "Ener");
	return Mode_VGet(usr, "iWeaponNeed", need) && Native_Subscript(kinds, need, typo);
}

/* the box a dead ship v leaves (OBJLIB 3: Items by name) */
static byte Mi_DropBox(modeVals_t *t, const nativeVal_t *v) {
	nativeVal_t *typo = Mode_V(t), *pos = Mode_V(t), *name = Mode_V(t), *vn = Mode_V(t), *l = Mode_V(t);
	nativeVal_t *take = Mode_V(t), *rt = Mode_V(t), *when = Mode_V(t), *tag = Mode_V(t);
	/* x, y, z of the unpack: three values one after the other */
	nativeVal_t *xyz = Mode_V(t);
	Mode_V(t);
	Mode_V(t);
	if (!Mi_BoxKind(t, typo) || !Mode_VGet(v, "Pos", pos) || !Mode_Unpack(pos, 3, xyz) || !Mode_VGet(v, "Name", vn) ||
		!Native_Arith('+', Mode_S(t, "Item"), vn, name) ||
		!Native_CallMod("Items", "CreateBox", l, "vvvvv", name, &xyz[0], &xyz[1], &xyz[2], typo) ||
		!Native_CallMod("Items", "SetSound", NULL, "vv", l, typo) || !Native_Arith('+', Mode_S(t, "!Take"), typo, take) ||
		!Mode_VSet(l, "OnTake", take)) {
		return 0;
	}
	Native_SetFloat(rt, 0.25);
	if (!Mode_VSet(l, "rTime", rt)) {
		return 0;
	}
	PyObject *del = Mode_VBound(l, "Del");
	byte ok = del && Mi_TimePlus(Mode_I(t, 20), when) && Native_Arith('+', Mode_S(t, "Delete"), typo, tag) &&
		Mode_Schedule(when, del, NULL, 0, tag);
	Py_XDECREF(del);
	return ok;
}

/* OnVehicleDeat(v): a ship that dies below the atmosphere drops coins (traffic, or at random without a super deal)
 * or a box with what the player's ship needs most; a ship of nobody goes */
static byte Mi_OnVehicleDeat(const nativeVal_t *v) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *pos = Mode_V(&t), *y = Mode_V(&t), *atm = Mode_V(&t);
	nativeVal_t *traf = Mode_V(&t), *rnd = Mode_V(&t), *six = Mode_V(&t), *cpos = Mode_V(&t), *reward = Mode_V(&t);
	nativeVal_t *usr = Mode_V(&t);
	if (!Mode_VGet(v, "Money", money)) {
		return Mode_Done(&t, 0);
	}
	int drop = Native_Compare(money, Mode_I(&t, 0)) >= 0;
	if (drop && (!Mode_VGet(v, "Pos", pos) || !Native_Subscript(pos, Mode_I(&t, 1), y) ||
		!Native_GVarGet("AtmosphereLimit", atm))) {
		return Mode_Done(&t, 0);
	}
	drop = drop && Native_Compare(y, atm) < 0;
	if (PyErr_Occurred()) {
		return Mode_Done(&t, 0);
	}
	if (drop) {
		/* Init.SuperDealModule holds the module of the running super deal (None without one), which is no native
		 * value: only its truth is taken, from the Python object */
		PyObject *sd = Mode_Import("Init") ? Script_Attr("Init", "SuperDealModule") : NULL;
		int deal = sd ? PyObject_IsTrue(sd) : -1;
		Py_XDECREF(sd);
		if (deal < 0) {
			return Mode_Done(&t, 0);
		}
		int coin = !deal;
		if (coin) {
			if (!Mode_VGet(v, "Traf_Active", traf)) {
				return Mode_Done(&t, 0);
			}
			coin = Native_True(traf);
			if (!coin) {
				Mi_Rand(0, 1, rnd);
				Native_SetFloat(six, 0.6);
				coin = Native_Compare(rnd, six) > 0;
			}
		}
		byte ok;
		if (coin) {
			/* create = CreateCoin first, as the script reads it before the arguments */
			PyObject *create = Mode_GlobalPy("MisItems", "CreateCoin");
			ok = create && Mode_VGet(v, "Pos", cpos) && Native_CallGlobal("MisItems", "GetTargetReward", reward, "v", v) &&
				Native_Call(create, NULL, "vv", cpos, reward);
			Py_XDECREF(create);
		} else {
			ok = Mi_DropBox(&t, v);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	nativeVal_t *ctl = usr;
	if (!Mode_VGet(v, "UsrControl", ctl)) {
		return Mode_Done(&t, 0);
	}
	int del = !Native_Compare(ctl, Mode_I(&t, -1));
	return Mode_Done(&t, !PyErr_Occurred() && (!del || Mode_VDel(v)));
}

MODE_FN(MisItems, OnVehicleDeat, 1, Mi_OnVehicleDeat(&a[0]))

/* GetTargetReward(v): the money of v, else 400 to 1200 for a ship, else 1 */
static byte Mi_TargetReward(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t money = { 0 }, zero, rnd;
	Native_SetInt(&zero, 0);
	if (!Mode_VGet(v, "Money", &money)) {
		return 0;
	}
	int has = Native_Compare(&money, &zero) > 0;
	Native_Free(&money);
	if (PyErr_Occurred()) {
		return 0;
	}
	if (has) {
		return Mode_VGet(v, "Money", out);
	}
	int car = Mode_VIsType(v, "Car");
	if (car > 0) {
		Mi_Rand(400, 1200, &rnd);
		return Native_ToInt(&rnd, out);
	}
	Native_SetInt(out, 1);
	return car == 0;
}

MODE_FN(MisItems, GetTargetReward, 1, Mi_TargetReward(&a[0], out))

/* int(names[2]) of the coin's name Usr_Coin_<amount>_Euros */
static byte Mi_CoinAmount(const nativeVal_t *names, nativeVal_t *out) {
	nativeVal_t k, s = { 0 };
	Native_SetInt(&k, 2);
	byte ok = Native_Subscript(names, &k, &s) && Native_ToInt(&s, out);
	Native_Free(&s);
	return ok;
}

/* GiveMoney(ItemName, VehicleName): a coin taken (Usr_Coin_<amount>_Euros): the money goes to the player, or to the
 * character or ship that took it; the coin stays for its sound */
static byte Mi_GiveMoney(const nativeVal_t *item, const nativeVal_t *vehicle) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *names = Mode_V(&t), *e = Mode_V(&t), *usr = Mode_V(&t), *un = Mode_V(&t), *amount = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t), *reward = Mode_V(&t), *itm = Mode_V(&t), *mt = Mode_V(&t), *rt = Mode_V(&t);
	nativeVal_t *ctl = Mode_V(&t), *money = Mode_V(&t);
	if (!Mode_Import("string") || !Native_Split(item, "_", names) || !Mode_EntityH(vehicle, e) || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int none = Native_Compare(usr, Mode_V(&t)) == 0;
	if (PyErr_Occurred() || none) {
		return Mode_Done(&t, !PyErr_Occurred());
	}
	Native_Free(usr);
	if (!Mode_UsrHandle(usr) || !Mode_VGet(usr, "Name", un)) {
		return Mode_Done(&t, 0);
	}
	int mine = !Native_Compare(vehicle, un);
	if (PyErr_Occurred()) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (mine) {
		Native_SetInt(money, Mode_GetMoney());
		ok = Mi_CoinAmount(names, amount) && Native_Arith('+', money, amount, sum) && Mode_SetMoney(sum);
	} else {
		ok = Native_CallGlobal("MisItems", "GetTargetReward", reward, "v", e) && Mi_CoinAmount(names, amount) &&
			Native_Arith('+', reward, amount, sum) && Mode_VSet(e, "Money", sum);
	}
	ok = ok && Mode_EntityH(item, itm) && Mode_G("MisItems", "MONEY_TIME", mt) && Native_Arith('*', mt, Mode_I(&t, 2), rt) &&
		Mode_VSet(itm, "rTime", rt) && Mode_Import("Sound") && Mode_VGet(e, "UsrControl", ctl);
	if (ok) {
		int other = Native_Compare(ctl, Mode_I(&t, -1)) != 0;
		ok = !PyErr_Occurred() && Mi_TakeSound(itm, other ? "MoneyItem" : "MoneyItemOther");
	}
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, GiveMoney, 2, Mi_GiveMoney(&a[0], &a[1]))

/* `n of the script as a text value */
static void Mi_ReprI(long n, nativeVal_t *out) {
	char s[0x20];
	Str_Printf(s, sizeof(s), "%ld", n);
	Native_SetStr(out, s);
}

/* GetMoneyModel(quanto): the coin model of the amount (Coin1 below the first of MoneyQuantos and so on) */
static byte Mi_MoneyModel(const nativeVal_t *quanto, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *q = Mode_V(&t), *res = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_G("MisItems", "MoneyQuantos", q)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(q);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Mi_ReprI(n + 1, res);
	if (!Mode_Read("MisItems", "MoneyQuantos", q) || (n = Native_Size(q)) < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t *v = Mode_V(&t);
		if (!Mode_Read("MisItems", "MoneyQuantos", q) || !Native_Subscript(q, Mode_I(&t, i), v)) {
			return Mode_Done(&t, 0);
		}
		int more = Native_Compare(v, quanto) > 0;
		if (PyErr_Occurred()) {
			return Mode_Done(&t, 0);
		}
		if (more) {
			Native_Free(res);
			Mi_ReprI(i + 1, res);
			break;
		}
	}
	byte ok = Native_Arith('+', Mode_S(&t, "Coin"), res, name) && Native_CallGlobal("MisItems", "GetObjModel", out, "v", name);
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, GetMoneyModel, 1, Mi_MoneyModel(&a[0], out))

/* GetObjModel(res): the model of a coin or item, outdoors the big one */
static byte Mi_ObjModel(const nativeVal_t *res, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *o = Mode_V(&t);
	if (!Mode_Import("Init") || !Mi_Init("Outdoor", o)) {
		return Mode_Done(&t, 0);
	}
	byte ok = Mode_Cat(&t, out, 3, Mode_S(&t, Native_True(o) ? "models/Misc/Coins/B" : "models/Misc/Coins/"), res,
		Mode_S(&t, ".m3d"));
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, GetObjModel, 1, Mi_ObjModel(&a[0], out))

/* CreateItem(Pos, Name, Model, Func): an item that goes after MONEY_TIME; Func(l) makes it takeable after
 * MONEY_APPEARS_TIME; its handle in *out */
static byte Mi_CreateItem(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *model, const nativeVal_t *func,
	nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *c[3], *o = Mode_V(&t), *rad = Mode_V(&t), *rt = Mode_V(&t), *mt = Mode_V(&t), *when = Mode_V(&t);
	nativeVal_t *at = Mode_V(&t), *when2 = Mode_V(&t);
	if (!Mode_Import("Init")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		c[i] = Mode_V(&t);
		if (!Native_Subscript(pos, Mode_I(&t, i), c[i])) {
			return Mode_Done(&t, 0);
		}
	}
	cEntity *e;
	if (!Mode_Create(name, c[0], c[1], c[2], "Item", &e) || !Mode_EntHandle(e, out) || !Mode_VModel(out, model) ||
		!Mi_Init("Outdoor", o) || !Mode_G("MisItems", Native_True(o) ? "COIN_RADIUS_OUTDOOR" : "COIN_RADIUS_INDOOR", rad) ||
		!Mode_VSet(out, "Radius", rad)) {
		return Mode_Done(&t, 0);
	}
	Mi_Rand(0.25, 1, rt);
	if (!Mode_VSet(out, "rTime", rt) || !Mode_G("MisItems", "MONEY_TIME", mt) || !Mi_TimePlus(mt, when) ||
		!Mi_DelLater(out, when, "DeleteCoin") || !Mode_G("MisItems", "MONEY_APPEARS_TIME", at) || !Mi_TimePlus(at, when2)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 4: the function the scheduler keeps (any value the Python way) */
	PyObject *f = Native_ToPy(func);
	byte ok = f && Mode_Schedule(when2, f, out, 1, NULL);
	Py_XDECREF(f);
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, CreateItem, 4, Mi_CreateItem(&a[0], &a[1], &a[2], &a[3], out))

/* CreateCoin(Pos, Money): a coin of the amount (Usr_Coin_<amount>_Euros) */
static byte Mi_CreateCoin(const nativeVal_t *pos, const nativeVal_t *money, nativeVal_t *out) {
	nativeBuf_t b;
	nativeVal_t name, model = { 0 };
	/* OBJLIB 3: the functions of the module as its globals, in the order the script reads them */
	PyObject *create = Mode_GlobalPy("MisItems", "CreateItem");
	if (!create) {
		return 0;
	}
	Native_BufInit(&b);
	Native_BufAdd(&b, "Usr_Coin_", -1);
	Native_Repr(&b, money, NR_REPR);
	Native_BufAdd(&b, "_Euros", -1);
	Native_SetText(&name, b.p, b.len);
	Native_BufFree(&b);
	PyObject *act = NULL;
	byte ok = Native_CallGlobal("MisItems", "GetMoneyModel", &model, "v", money) &&
		(act = Mode_GlobalPy("MisItems", "ActivateCoin")) != NULL && Native_Call(create, out, "vvvO", pos, &name, &model, act);
	Py_XDECREF(act);
	Py_DECREF(create);
	Native_Free(&name);
	Native_Free(&model);
	return ok;
}

MODE_FN(MisItems, CreateCoin, 2, Mi_CreateCoin(&a[0], &a[1], out))

/* ActivateCoin(l), ActivateBattery(l): the item can be taken */
static byte Mi_Activate(const nativeVal_t *l, const char *func) {
	nativeVal_t v;
	Native_SetStr(&v, func);
	return Mode_VSet(l, "OnTake", &v);
}

MODE_FN(MisItems, ActivateCoin, 1, Mi_Activate(&a[0], "MisItems.GiveMoney"))
MODE_FN(MisItems, ActivateBattery, 1, Mi_Activate(&a[0], "MisItems.TakeBattery"))

/* CreateBattery(Pos, ActivateFunc=None): a battery only the player takes */
static byte Mi_CreateBattery(const nativeVal_t *pos, const nativeVal_t *act, nativeVal_t *out) {
	nativeVal_t none, model = { 0 };
	PyObject *a = NULL;
	Native_SetNone(&none);
	if (!Native_Compare(act, &none)) {
		if (!(a = Mode_GlobalPy("MisItems", "ActivateBattery"))) {
			return 0;
		}
	} else if (PyErr_Occurred() || !(a = Native_ToPy(act))) {
		return 0;
	}
	PyObject *create = Mode_GlobalPy("MisItems", "CreateItem");
	byte ok = create && Native_CallGlobal("MisItems", "GetObjModel", &model, "s", "Battery") &&
		Native_Call(create, out, "vsvO", pos, "Battery", &model, a) && Mode_VSet(out, "OnlyUser", (Native_SetInt(&none, 1), &none));
	Py_XDECREF(create);
	Py_DECREF(a);
	Native_Free(&model);
	return ok;
}

static PyObject *MisItems_CreateBattery(PyObject *self, PyObject *args) {
	PyObject *pos, *act = NULL;
	nativeVal_t p, a = { 0 }, out = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &pos, &act)) {
		return NULL;
	}
	Native_Arg(pos, &p);
	if (act) {
		Native_Arg(act, &a);
	}
	byte ok = Mi_CreateBattery(&p, &a, &out);
	Native_Free(&p);
	Native_Free(&a);
	return Mode_Return(ok, &out);
}

/* TakeBattery(ItemName, CharName): one energy more, up to the batteries of the save game */
static byte Mi_TakeBattery(const nativeVal_t *item, const nativeVal_t *charName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *itm = Mode_V(&t), *mt = Mode_V(&t), *rt = Mode_V(&t), *me = Mode_V(&t), *en = Mode_V(&t), *en1 = Mode_V(&t);
	nativeVal_t *bat = Mode_V(&t), *full = Mode_V(&t), *now = Mode_V(&t);
	if (!Mode_Import("Sound") || !Mode_EntityH(item, itm) || !Mode_G("MisItems", "MONEY_TIME", mt) ||
		!Native_Arith('*', mt, Mode_I(&t, 2), rt) || !Mode_VSet(itm, "rTime", rt) || !Mode_EntityH(charName, me) ||
		!Mode_VGet(me, "Energy", en) || !Native_Arith('+', en, Mode_I(&t, 1), en1) || !Mode_VSet(me, "Energy", en1)) {
		return Mode_Done(&t, 0);
	}
	/* Scrap.IncSaveVar("Stats.Battery"), int(Scrap.GetSaveVar("Batteries")) */
	SaveVar_Inc("Stats.Battery", 1);
	Mode_SaveVar("Batteries", bat);
	if (!Native_ToInt(bat, full) || !Mode_VGet(me, "Energy", now)) {
		return Mode_Done(&t, 0);
	}
	int over = Native_Compare(now, full) > 0;
	byte ok = !PyErr_Occurred() && (!over || Mode_VSet(me, "Energy", full)) && Mi_TakeSound(itm, "TakeBattery");
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, TakeBattery, 2, Mi_TakeBattery(&a[0], &a[1]))

/* DazedCoin(EntityName, EventName, Time): a dazed character with enough money loses coins around it where there is
 * room */
static byte Mi_DazedCoin(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *dr = Mode_V(&t), *limit = Mode_V(&t), *money = Mode_V(&t), *q = Mode_V(&t);
	nativeVal_t *reward = Mode_V(&t), *pos = Mode_V(&t), *vr = Mode_V(&t), *radius = Mode_V(&t);
	const nativeVal_t *line[] = { Mode_S(&t, "DazedCoin") };
	if (!Mode_Print(line, 1) || !Mode_EntityH(name, e) || !Mode_G("MisItems", "DazedReward", dr) ||
		!Native_Arith('*', dr, Mode_I(&t, 8), limit) || !Mode_VGet(e, "Money", money)) {
		return Mode_Done(&t, 0);
	}
	int few = Native_Compare(money, limit) <= 0;
	if (PyErr_Occurred() || few) {
		return Mode_Done(&t, !PyErr_Occurred());
	}
	Native_Free(money);
	Native_Free(dr);
	if (!Mode_VGet(e, "Money", money) || !Mode_G("MisItems", "DazedReward", dr) || !Native_Arith('/', money, dr, q) ||
		!Native_ToInt(q, reward) || !Mode_VGet(e, "Pos", pos) || !Mode_VGet(e, "VRadius", vr) ||
		!Native_Arith('+', Mode_I(&t, 120), vr, radius)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (int i = 0; ok && i < 360; i += 45) {
		int mark = t.n;
		nativeVal_t *ang = Mode_V(&t), *a1 = Mode_V(&t), *yaw = Mode_V(&t), *v = Mode_V(&t), *rot = Mode_V(&t);
		nativeVal_t *r = Mode_V(&t), *posix = Mode_V(&t), *hit = Mode_V(&t), *m = Mode_V(&t), *m2 = Mode_V(&t), *c[3];
		for (int k = 0; k < 3; k++) {
			c[k] = Mode_V(&t);
		}
		ok = Mode_VGet(e, "Ang", ang) && Native_Subscript(ang, Mode_I(&t, 1), a1) && Native_Arith('+', Mode_I(&t, i), a1, yaw);
		if (ok) {
			/* SVec.Rotate3D((radius, 0, 0), (0, yaw, 0)) */
			Native_SetEmpty(v, NV_TUPLE);
			Native_Copy(Native_Append(v), radius);
			Native_SetInt(Native_Append(v), 0);
			Native_SetInt(Native_Append(v), 0);
			Native_SetEmpty(rot, NV_TUPLE);
			Native_SetInt(Native_Append(rot), 0);
			Native_Copy(Native_Append(rot), yaw);
			Native_SetInt(Native_Append(rot), 0);
		}
		ok = ok && Mode_Rotate(v, rot, r) && Mode_VecAdd(pos, r, posix);
		for (int k = 0; ok && k < 3; k++) {
			ok = Native_Subscript(posix, Mode_I(&t, k), c[k]);
		}
		ok = ok && Mode_TestSphere(c[0], c[1], c[2], Mode_I(&t, 100), Mode_S(&t, "rv"), NULL, hit);
		if (ok && hit->type == NV_NONE) {
			ok = Native_CallGlobal("MisItems", "CreateCoin", NULL, "vv", posix, reward) && Mode_VGet(e, "Money", m) &&
				Native_Arith('-', m, reward, m2) && Mode_VSet(e, "Money", m2);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

MODE_FN(MisItems, DazedCoin, 3, Mi_DazedCoin(&a[0]))

/* the adapters of the functions that take no arguments (the arity error of PyArg_ParseTuple "") */
#define MI_NOARGS(fn, call) \
	static PyObject *fn(PyObject *self, PyObject *args) { \
		(void)self; \
		return PyArg_ParseTuple(args, "") ? Mode_Result(call) : NULL; \
	}

MI_NOARGS(MisItems_Init, Mi_InitModels())

PyMethodDef g_MisItemsMethods[] = {
	{ "Init", MisItems_Init, 1, NULL },
	{ "OnVehicleDeat", MisItems_OnVehicleDeat, 1, NULL },
	{ "GetTargetReward", MisItems_GetTargetReward, 1, NULL },
	{ "GiveMoney", MisItems_GiveMoney, 1, NULL },
	{ "GetMoneyModel", MisItems_GetMoneyModel, 1, NULL },
	{ "GetObjModel", MisItems_GetObjModel, 1, NULL },
	{ "CreateItem", MisItems_CreateItem, 1, NULL },
	{ "CreateCoin", MisItems_CreateCoin, 1, NULL },
	{ "ActivateCoin", MisItems_ActivateCoin, 1, NULL },
	{ "CreateBattery", MisItems_CreateBattery, 1, NULL },
	{ "ActivateBattery", MisItems_ActivateBattery, 1, NULL },
	{ "TakeBattery", MisItems_TakeBattery, 1, NULL },
	{ "DazedCoin", MisItems_DazedCoin, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- CrazyWing ---- */

/* InitCrazyWing(): the next crazy wind in CrazyWingLoTime to CrazyWingHiTime seconds, unless it is switched off */
static byte Cw_Init(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *status = Mode_V(&t), *now = Mode_V(&t), *lo = Mode_V(&t), *hi = Mode_V(&t), *r = Mode_V(&t), *when = Mode_V(&t);
	Mode_SaveVar("CrazyWing.Status", status);
	if (!Native_Compare(status, Mode_S(&t, "0"))) {
		return Mode_Done(&t, 1);
	}
	Native_SetFloat(now, Mode_Time());
	if (!Mode_G("CrazyWing", "CrazyWingLoTime", lo) || !Mode_G("CrazyWing", "CrazyWingHiTime", hi) || !Mode_Rand(lo, hi, r) ||
		!Native_Arith('+', now, r, when)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 4: the module's CrazyWing as the scheduler keeps it */
	PyObject *f = Mode_GlobalPy("CrazyWing", "CrazyWing");
	byte ok = f && Mode_Schedule(when, f, NULL, 0, Mode_S(&t, "CrazyWind"));
	Py_XDECREF(f);
	return Mode_Done(&t, ok);
}

/* CrazyWing(): the wind blows: the next one, and a character goes crazy unless the alarm is on */
static byte Cw_Blow(void) {
	if (!Native_CallGlobal("CrazyWing", "InitCrazyWing", NULL, "")) {
		return 0;
	}
	/* Scrap.GetAlarm()[0] == 0 */
	int alarm = Mode_AlarmOn();
	return alarm > 0 || (!alarm && Native_CallGlobal("CrazyWing", "SetCrazy", NULL, ""));
}

/* SetCrazy(): the character next to the player (relaxing or dancing, one of CrazyChars, not a mission target and not
 * photographable) goes crazy for CrazyWingTime seconds and can be photographed */
static byte Cw_SetCrazy(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *me = Mode_V(&t), *pos = Mode_V(&t), *mn = Mode_V(&t), *guy = Mode_V(&t), *ch = Mode_V(&t), *ctl = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *head = Mode_V(&t), *chars = Mode_V(&t), *ct = Mode_V(&t), *when = Mode_V(&t);
	nativeVal_t *args = Mode_V(&t), *c[3];
	for (int k = 0; k < 3; k++) {
		c[k] = Mode_V(&t);
	}
	if (!Mode_UsrHandle(me)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(me)) {
		return Mode_Done(&t, 1);
	}
	byte ok = Mode_VGet(me, "Pos", pos);
	for (int k = 0; ok && k < 3; k++) {
		ok = Native_Subscript(pos, Mode_I(&t, k), c[k]);
	}
	if (!ok || !Mode_VGet(me, "Name", mn) || !Mode_TestSphere(c[0], c[1], c[2], Mode_I(&t, 1000), Mode_S(&t, "r"), mn, guy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(guy)) {
		return Mode_Done(&t, 1);
	}
	const char *cls;
	if (!Mode_EntityH(guy, ch) || !Mode_VGet(ch, "ActCtrl", ctl) || !Native_ArgStr(ctl, 1, &cls)) {
		return Mode_Done(&t, 0);
	}
	Native_ActClass(cls);
	int go = Mode_VGet(ch, "RadarShow", v) ? !Native_True(v) : -1;
	Native_Free(v);
	if (go > 0) {
		go = Mode_VGet(ch, "CanPhoto", v) ? !Native_True(v) : -1;
		Native_Free(v);
	}
	if (go > 0) {
		go = Mode_VGet(ch, "Action", v) && Native_Slice(v, 0, 5, head) ? Native_IsStr(head, "Relax") || Native_IsStr(head, "Dance") : -1;
		Native_Free(v);
	}
	if (go > 0) {
		go = Mode_VGet(ch, "ActCtrl", v) && Mode_G("CrazyWing", "CrazyChars", chars) ? Native_Contains(chars, v) : -1;
		Native_Free(v);
	}
	if (go < 0) {
		return Mode_Done(&t, 0);
	}
	if (!go) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VSet(ch, "Action", Mode_S(&t, "CrazyInit")) || !Mode_VGet(ch, "Action", v)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_IsStr(v, "CrazyInit")) {
		return Mode_Done(&t, 1);
	}
	Native_Free(v);
	/* OBJLIB 4: DeCrazy(guy, ch.CanPhoto) in CrazyWingTime */
	PyObject *f = NULL;
	ok = Mode_G("CrazyWing", "CrazyWingTime", ct) && Mi_TimePlus(ct, when) && (f = Mode_GlobalPy("CrazyWing", "DeCrazy")) &&
		Mode_VGet(ch, "CanPhoto", v);
	if (ok) {
		Native_SetEmpty(args, NV_TUPLE);
		Native_Copy(Native_Append(args), guy);
		Native_Copy(Native_Append(args), v);
		ok = Mode_Schedule(when, f, args->items, 2, NULL);
	}
	Py_XDECREF(f);
	ok = ok && Mode_VSet(ch, "CanPhoto", Mode_I(&t, 1)) && Mode_VSet(ch, "DefLoop", Mode_S(&t, "Crazy"));
	return Mode_Done(&t, ok);
}

/* DeCrazy(CharName, CanPhoto): the character calms down */
static byte Cw_DeCrazy(const nativeVal_t *name, const nativeVal_t *canPhoto) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *act = Mode_V(&t);
	if (!Mode_EntityH(name, ch)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(ch)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VSet(ch, "CanPhoto", canPhoto) || !Mode_VGet(ch, "Action", act)) {
		return Mode_Done(&t, 0);
	}
	byte ok = !Native_IsStr(act, "Crazy") ||
		(Mode_VSet(ch, "Action", Mode_S(&t, "CrazyEnd")) && Mode_VSet(ch, "ForceDefLoop", Mode_S(&t, "Relax")));
	return Mode_Done(&t, ok);
}

MODE_FN(CrazyWing, DeCrazy, 2, Cw_DeCrazy(&a[0], &a[1]))

/* RestoreSpeech(): the player selects again */
static byte Cw_RestoreSpeech(void) {
	nativeVal_t usr = { 0 }, one;
	Native_SetInt(&one, 1);
	byte ok = Mode_UsrHandle(&usr) && Mode_VSet(&usr, "AutoSelect", &one);
	Native_Free(&usr);
	return ok;
}

/* AllInList(): every crazy character has been photographed (1 or 0 in *out) */
static byte Cw_AllInList(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *chars = Mode_V(&t), *list = Mode_V(&t);
	if (!Mode_G("CrazyWing", "CrazyChars", chars)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(chars);
	for (int i = 0; i < n; i++) {
		nativeVal_t *c = Mode_V(&t);
		int in = Native_SeqItem(chars, i, c) && Mode_Read("CrazyWing", "CrazyWingList", list) ? Native_Contains(list, c) : -1;
		if (in <= 0) {
			Native_SetInt(out, 0);
			return Mode_Done(&t, in == 0);
		}
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, n >= 0);
}

/* the prize text key with <n> as the amount, on the message line for 6 s (string.replace of the text) */
static byte Cw_PrizeMsg(const char *key, const nativeVal_t *prize) {
	nativeVal_t text, amount, msg;
	nativeBuf_t b, out;
	Mode_Lang(key, &text);
	Native_BufInit(&b);
	Native_Repr(&b, prize, NR_REPR);
	Native_SetText(&amount, b.p, b.len);
	Native_BufFree(&b);
	/* string.replace(text, "<n>", amount): splitfields and joinfields */
	Native_BufInit(&out);
	const char *s = Native_Text(&text);
	int from = 0;
	for (int i = 0; i + 3 <= text.n; i++) {
		if (!memcmp(s + i, "<n>", 3)) {
			Native_BufAdd(&out, s + from, i - from);
			Native_BufAdd(&out, Native_Text(&amount), amount.n);
			from = i + 3;
			i += 2;
		}
	}
	Native_BufAdd(&out, s + from, text.n - from);
	Native_SetText(&msg, out.p, out.len);
	Native_BufFree(&out);
	byte ok = Mode_MsgText(&msg, Mode_Time() + 6.0);
	Native_Free(&text);
	Native_Free(&amount);
	Native_Free(&msg);
	return ok;
}

/* Scrap.SetMoney(Scrap.GetMoney() + prize) and the message of the prize */
static byte Cw_Prize(const char *var, const char *key, const nativeVal_t *given) {
	nativeVal_t prize = { 0 }, money, sum = { 0 };
	byte ok = given ? (Native_Copy(&prize, given), 1) : Mode_G("CrazyWing", var, &prize);
	Native_SetInt(&money, Mode_GetMoney());
	ok = ok && Native_Arith('+', &money, &prize, &sum) && Mode_SetMoney(&sum);
	if (ok && !given) {
		/* the prize read again for the text, as the script does */
		Native_Free(&prize);
		ok = Mode_G("CrazyWing", var, &prize);
	}
	ok = ok && Cw_PrizeMsg(key, &prize);
	Native_Free(&prize);
	Native_Free(&sum);
	return ok;
}

/* CrazyPhoto(e): a photo of a crazy character: a prize (more for each new one, the big prize for all of them, the
 * small one for a repeated one); 1 when it was one of CrazyChars (in *out) */
static byte Cw_Photo(const nativeVal_t *e, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ctr = Mode_V(&t), *chars = Mode_V(&t), *list = Mode_V(&t), *all = Mode_V(&t), *first = Mode_V(&t);
	nativeVal_t *inc = Mode_V(&t), *ni = Mode_V(&t), *prize = Mode_V(&t), *rep = Mode_V(&t);
	if (!Mode_Import("SaveGame") || !Mode_Import("Speech") || !Mode_Import("string") || !Mode_VGet(e, "ActCtrl", ctr) ||
		!Mode_G("CrazyWing", "CrazyChars", chars)) {
		return Mode_Done(&t, 0);
	}
	int crazy = Native_Contains(chars, ctr);
	if (crazy <= 0) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, crazy == 0);
	}
	int seen = Mode_G("CrazyWing", "CrazyWingList", list) ? Native_Contains(list, ctr) : -1;
	byte ok = seen >= 0;
	if (seen > 0) {
		ok = Cw_Prize("SecondPrize", "CrazyWindRepeat", NULL);
	} else if (seen == 0) {
		/* CrazyWingList.append(ctr), the list of the module in place (OBJLIB 5) */
		PyObject *l = Mode_GlobalPy("CrazyWing", "CrazyWingList"), *c = l ? Native_ToPy(ctr) : NULL;
		PyObject *r = c ? Script_Method(l, "append", "(O)", c) : NULL;
		ok = r != NULL;
		Py_XDECREF(r);
		Py_XDECREF(c);
		Py_XDECREF(l);
		ok = ok && Native_CallGlobal("CrazyWing", "AllInList", all, "");
		if (ok && Native_True(all)) {
			/* then Scrap.Achievement(12), which does nothing on the PC */
			ok = Cw_Prize("BigPrize", "CrazyWindAll", NULL);
		} else if (ok) {
			int n = Mode_G("CrazyWing", "FirstPrize", first) && Mode_Read("CrazyWing", "CrazyWingList", list) ? Native_Size(list) : -1;
			ok = n >= 0;
			if (ok) {
				Native_SetInt(ni, n);
				ok = Mode_G("CrazyWing", "IncPrize", inc) && Native_Arith('*', ni, inc, rep) &&
					Native_Arith('+', first, rep, prize) && Cw_Prize(NULL, "CrazyWindNew", prize);
			}
		}
	}
	if (!ok || !Mode_VSet(e, "CanPhoto", Mode_I(&t, 0)) || !Native_CallMod("SaveGame", "SaveMoney", NULL, "") ||
		!Mode_Read("CrazyWing", "CrazyWingList", list)) {
		return Mode_Done(&t, 0);
	}
	/* Scrap.SetSaveVar("CrazyWing.List", `CrazyWingList`) */
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, list, NR_REPR);
	SaveVar_Set("CrazyWing.List", b.p);
	Native_BufFree(&b);
	Native_SetInt(out, 1);
	return Mode_Done(&t, 1);
}

MODE_FN(CrazyWing, CrazyPhoto, 1, Cw_Photo(&a[0], out))

MI_NOARGS(CrazyWing_InitCrazyWing, Cw_Init())
MI_NOARGS(CrazyWing_CrazyWing, Cw_Blow())
MI_NOARGS(CrazyWing_SetCrazy, Cw_SetCrazy())
MI_NOARGS(CrazyWing_RestoreSpeech, Cw_RestoreSpeech())

static PyObject *CrazyWing_AllInList(PyObject *self, PyObject *args) {
	nativeVal_t out = { 0 };
	(void)self;
	return PyArg_ParseTuple(args, "") ? Mode_Return(Cw_AllInList(&out), &out) : NULL;
}

PyMethodDef g_CrazyWingMethods[] = {
	{ "InitCrazyWing", CrazyWing_InitCrazyWing, 1, NULL },
	{ "CrazyWing", CrazyWing_CrazyWing, 1, NULL },
	{ "SetCrazy", CrazyWing_SetCrazy, 1, NULL },
	{ "DeCrazy", CrazyWing_DeCrazy, 1, NULL },
	{ "RestoreSpeech", CrazyWing_RestoreSpeech, 1, NULL },
	{ "AllInList", CrazyWing_AllInList, 1, NULL },
	{ "CrazyPhoto", CrazyWing_CrazyPhoto, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
