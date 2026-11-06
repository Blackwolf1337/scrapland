/*
 * The MisItems and CrazyWing modules in C (scripts/missions/misitems.py and crazywing.py, issue #2 step 8a). The
 * items of the missions: the coins a dead NPC or ship drops (their model by the amount), the boxes of a ship, the
 * batteries of the Gears and the coins a dazed character loses. The crazy wind of the indoor levels: now and then a
 * character near the player goes crazy for a while, and a photo of it pays a prize (all of them the big prize).
 *
 * The constants and lists stay in the modules (MONEY_TIME, MoneyQuantos, the coin radii, DazedReward, CrazyChars, the
 * times and prizes, CrazyWingList) with the module code (the save variables of the crazy wind); the functions read
 * them as their globals.
 */
#include "scrap/scrap.h"
#include "scrap/script.h"

/* Scrap.Rand(a, b) */
static PyObject *Mi_Rand(scriptFrame_t *f, double a, double b) {
	return f->failed ? NULL : Sf_Float(f, Scrap_RandRange(a, b));
}

/* SSound.PlaySound(pos, a[0], a[1]) with a = Sound.ItemTake[key] */
static void Mi_TakeSound(scriptFrame_t *f, PyObject *pos, const char *key) {
	PyObject *a = Sf_GetItem(f, Sf_Mod(f, "Sound", "ItemTake"), Sf_Str(f, key));
	pos = Sf_Attr(f, pos, "Pos");
	Sf_Ask(f, "SSound", "PlaySound", "(OOO)", pos, Sf_Item(f, a, 0), Sf_Item(f, a, 1));
}

/* Init(): the models of the coins, indoors the extra life and the battery too */
static PyObject *MisItems_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int n = Sf_Len(&f, Sf_Global(&f, "MisItems", "MoneyQuantos"));
	for (int i = 0; i < n + 1 && !f.failed; i++) {
		int mark = f.n;
		PyObject *money = i == 0 ? Sf_Int(&f, 1) : Sf_Item(&f, Sf_Global(&f, "MisItems", "MoneyQuantos"), i - 1);
		PyObject *model = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetMoneyModel"), "(O)", money);
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", model, 1, 1, 1);
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor")) == 0) {
		PyObject *model = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetObjModel"), "(s)", "ExtraLife");
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", model, 1, 1, 1);
		model = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetObjModel"), "(s)", "Battery");
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", model, 1, 1, 1);
	}
	return Sf_None(&f);
}

/* OnVehicleDeat(v): a ship that dies below the atmosphere drops coins (traffic, or at random without a super deal)
 * or a box with what the player's ship needs most; a ship of nobody goes */
static PyObject *MisItems_OnVehicleDeat(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	int drop = Sf_Compare(&f, Sf_Attr(&f, v, "Money"), ">=", Sf_Int(&f, 0));
	if (drop > 0) {
		PyObject *y = Sf_Item(&f, Sf_Attr(&f, v, "Pos"), 1);
		drop = Sf_Compare(&f, y, "<", Sf_Ask(&f, "Scrap", "Get", "(s)", "AtmosphereLimit"));
	}
	if (drop > 0) {
		Sf_Import(&f, "Init");
		int coin = Sf_True(&f, Sf_Mod(&f, "Init", "SuperDealModule")) == 0;
		if (coin > 0) {
			coin = Sf_True(&f, Sf_Attr(&f, v, "Traf_Active"));
			if (coin == 0) {
				coin = Sf_Compare(&f, Mi_Rand(&f, 0, 1), ">", Sf_Float(&f, 0.6));
			}
		}
		if (coin > 0) {
			PyObject *create = Sf_Global(&f, "MisItems", "CreateCoin");
			PyObject *pos = Sf_Attr(&f, v, "Pos");
			PyObject *reward = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetTargetReward"), "(O)", v);
			Sf_Call(&f, create, "(OO)", pos, reward);
		} else if (coin == 0) {
			Sf_Import(&f, "Items");
			PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
			int car = Sf_True(&f, usr);
			if (car > 0) {
				car = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
			}
			PyObject *typo = NULL;
			if (car > 0) {
				PyObject *rnd = Mi_Rand(&f, 0.25, 0.5);
				PyObject *ratio = Sf_Op(&f, Sf_Attr(&f, usr, "Life"), '/', Sf_Attr(&f, usr, "MinLife"));
				int low = Sf_Compare(&f, rnd, ">", ratio);
				if (low > 0) {
					typo = Sf_Str(&f, "Life");
				} else if (low == 0) {
					PyObject *kinds = Sf_Build(&f, "(sss)", "Part", "Miss", "Ener");
					typo = Sf_GetItem(&f, kinds, Sf_Attr(&f, usr, "iWeaponNeed"));
				}
			} else if (car == 0) {
				typo = Sf_Str(&f, "Life");
			}
			PyObject *xyz[3];
			if (Sf_Unpack(&f, Sf_Attr(&f, v, "Pos"), 3, xyz)) {
				PyObject *name = Sf_Cat(&f, "sO", "Item", Sf_Attr(&f, v, "Name"));
				PyObject *l = Sf_Ask(&f, "Items", "CreateBox", "(OOOOO)", name, xyz[0], xyz[1], xyz[2], typo);
				Sf_Ask(&f, "Items", "SetSound", "(OO)", l, typo);
				Sf_SetAttr(&f, l, "OnTake", Sf_Cat(&f, "sO", "!Take", typo));
				Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
				PyObject *del = Sf_Attr(&f, l, "Del");
				PyObject *when = Sf_OpI(&f, Sf_Time(&f), '+', 20);
				PyObject *tag = Sf_Cat(&f, "sO", "Delete", typo);
				if (tag) {
					Sf_Schedule(&f, when, del, Sf_Keep(&f, PyTuple_New(0)), PyString_AsString(tag));
				}
			}
		}
	}
	if (Sf_EqI(&f, Sf_Attr(&f, v, "UsrControl"), -1) > 0) {
		Sf_Method(&f, v, "Del", "()");
	}
	return Sf_None(&f);
}

/* GetTargetReward(v): the money of v, else 400 to 1200 for a ship, else 1 */
static PyObject *MisItems_GetTargetReward(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	if (Sf_Compare(&f, Sf_Attr(&f, v, "Money"), ">", Sf_Int(&f, 0)) > 0) {
		return Sf_Return(&f, Sf_Attr(&f, v, "Money"));
	}
	int car = Sf_True(&f, Sf_Method(&f, v, "IsType", "(s)", "Car"));
	if (car > 0) {
		return Sf_Return(&f, Sf_ToInt(&f, Mi_Rand(&f, 400, 1200)));
	}
	return Sf_Return(&f, car == 0 ? Sf_Int(&f, 1) : NULL);
}

/* GiveMoney(ItemName, VehicleName): a coin taken (Usr_Coin_<amount>_Euros): the money goes to the player, or to the
 * character or ship that took it; the coin stays for its sound */
static PyObject *MisItems_GiveMoney(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &vehicle)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *names = Sf_Ask(&f, "string", "splitfields", "(Os)", item, "_");
	PyObject *e = Sf_Entity(&f, vehicle);
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "==", Py_None) != 0) {
		return Sf_None(&f);
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int mine = Sf_Compare(&f, vehicle, "==", Sf_Attr(&f, usr, "Name"));
	if (mine > 0) {
		PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", Sf_Op(&f, money, '+', Sf_ToInt(&f, Sf_Item(&f, names, 2))));
	} else if (mine == 0) {
		PyObject *reward = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetTargetReward"), "(O)", e);
		Sf_SetAttr(&f, e, "Money", Sf_Op(&f, reward, '+', Sf_ToInt(&f, Sf_Item(&f, names, 2))));
	}
	PyObject *itm = Sf_Entity(&f, item);
	Sf_SetAttr(&f, itm, "rTime", Sf_OpI(&f, Sf_Global(&f, "MisItems", "MONEY_TIME"), '*', 2));
	Sf_Import(&f, "Sound");
	int other = Sf_Compare(&f, Sf_Attr(&f, e, "UsrControl"), "!=", Sf_Int(&f, -1));
	if (other >= 0) {
		Mi_TakeSound(&f, itm, other ? "MoneyItem" : "MoneyItemOther");
	}
	return Sf_None(&f);
}

/* GetMoneyModel(quanto): the coin model of the amount (Coin1 below the first of MoneyQuantos and so on) */
static PyObject *MisItems_GetMoneyModel(PyObject *self, PyObject *args) {
	PyObject *quanto;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &quanto)) {
		return NULL;
	}
	int n = Sf_Len(&f, Sf_Global(&f, "MisItems", "MoneyQuantos"));
	PyObject *res = n >= 0 ? Sf_ReprI(&f, n + 1) : NULL;
	n = Sf_Len(&f, Sf_Global(&f, "MisItems", "MoneyQuantos"));
	for (int i = 0; i < n && !f.failed; i++) {
		PyObject *q = Sf_Item(&f, Sf_Global(&f, "MisItems", "MoneyQuantos"), i);
		if (Sf_Compare(&f, q, ">", quanto) > 0) {
			res = Sf_ReprI(&f, i + 1);
			break;
		}
	}
	PyObject *name = Sf_Cat(&f, "sO", "Coin", res);
	return Sf_Return(&f, Sf_Call(&f, Sf_Global(&f, "MisItems", "GetObjModel"), "(O)", name));
}

/* GetObjModel(res): the model of a coin or item, outdoors the big one */
static PyObject *MisItems_GetObjModel(PyObject *self, PyObject *args) {
	PyObject *res;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &res)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int out = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	if (out < 0) {
		return Sf_Done(&f, NULL);
	}
	return Sf_Return(&f, Sf_Cat(&f, "sOs", out ? "models/Misc/Coins/B" : "models/Misc/Coins/", res, ".m3d"));
}

/* CreateItem(Pos, Name, Model, Func): an item that goes after MONEY_TIME; Func(l) makes it takeable after
 * MONEY_APPEARS_TIME */
static PyObject *MisItems_CreateItem(PyObject *self, PyObject *args) {
	PyObject *pos, *name, *model, *func;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &pos, &name, &model, &func)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", model);
	int out = Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor"));
	if (out >= 0) {
		Sf_SetAttr(&f, l, "Radius", Sf_Global(&f, "MisItems", out ? "COIN_RADIUS_OUTDOOR" : "COIN_RADIUS_INDOOR"));
	}
	Sf_SetAttr(&f, l, "rTime", Mi_Rand(&f, 0.25, 1));
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Global(&f, "MisItems", "MONEY_TIME"));
	Sf_Schedule(&f, t, Sf_Attr(&f, l, "Del"), Sf_Keep(&f, PyTuple_New(0)), "DeleteCoin");
	t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Global(&f, "MisItems", "MONEY_APPEARS_TIME"));
	Sf_Schedule(&f, t, func, Sf_Build(&f, "(O)", l), NULL);
	return Sf_Return(&f, l);
}

/* CreateCoin(Pos, Money): a coin of the amount (Usr_Coin_<amount>_Euros) */
static PyObject *MisItems_CreateCoin(PyObject *self, PyObject *args) {
	PyObject *pos, *money;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &pos, &money)) {
		return NULL;
	}
	PyObject *create = Sf_Global(&f, "MisItems", "CreateItem");
	PyObject *name = Sf_Cat(&f, "sOs", "Usr_Coin_", Sf_Repr(&f, money), "_Euros");
	PyObject *model = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetMoneyModel"), "(O)", money);
	PyObject *act = Sf_Global(&f, "MisItems", "ActivateCoin");
	return Sf_Return(&f, Sf_Call(&f, create, "(OOOO)", pos, name, model, act));
}

/* ActivateCoin(l), ActivateBattery(l): the item can be taken */
static PyObject *MisItems_ActivateCoin(PyObject *self, PyObject *args) {
	PyObject *l;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &l)) {
		return NULL;
	}
	Sf_SetAttrS(&f, l, "OnTake", "MisItems.GiveMoney");
	return Sf_None(&f);
}

static PyObject *MisItems_ActivateBattery(PyObject *self, PyObject *args) {
	PyObject *l;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &l)) {
		return NULL;
	}
	Sf_SetAttrS(&f, l, "OnTake", "MisItems.TakeBattery");
	return Sf_None(&f);
}

/* CreateBattery(Pos, ActivateFunc=None): a battery only the player takes */
static PyObject *MisItems_CreateBattery(PyObject *self, PyObject *args) {
	PyObject *pos, *act = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &pos, &act)) {
		return NULL;
	}
	if (!act) {
		act = Py_None;
	}
	if (Sf_Compare(&f, act, "==", Py_None) > 0) {
		act = Sf_Global(&f, "MisItems", "ActivateBattery");
	}
	PyObject *create = Sf_Global(&f, "MisItems", "CreateItem");
	PyObject *model = Sf_Call(&f, Sf_Global(&f, "MisItems", "GetObjModel"), "(s)", "Battery");
	PyObject *l = Sf_Call(&f, create, "(OsOO)", pos, "Battery", model, act);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	return Sf_Return(&f, l);
}

/* TakeBattery(ItemName, CharName): one energy more, up to the batteries of the save game */
static PyObject *MisItems_TakeBattery(PyObject *self, PyObject *args) {
	PyObject *item, *charName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &charName)) {
		return NULL;
	}
	Sf_Import(&f, "Sound");
	PyObject *itm = Sf_Entity(&f, item);
	Sf_SetAttr(&f, itm, "rTime", Sf_OpI(&f, Sf_Global(&f, "MisItems", "MONEY_TIME"), '*', 2));
	PyObject *me = Sf_Entity(&f, charName);
	Sf_SetAttr(&f, me, "Energy", Sf_OpI(&f, Sf_Attr(&f, me, "Energy"), '+', 1));
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "Stats.Battery");
	PyObject *full = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Batteries"));
	if (Sf_Compare(&f, Sf_Attr(&f, me, "Energy"), ">", full) > 0) {
		Sf_SetAttr(&f, me, "Energy", full);
	}
	Mi_TakeSound(&f, itm, "TakeBattery");
	return Sf_None(&f);
}

/* DazedCoin(EntityName, EventName, Time): a dazed character with enough money loses coins around it where there is
 * room */
static PyObject *MisItems_DazedCoin(PyObject *self, PyObject *args) {
	PyObject *name, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &event, &time)) {
		return NULL;
	}
	Sf_Print(&f, "s", "DazedCoin");
	PyObject *e = Sf_Entity(&f, name);
	PyObject *limit = Sf_OpI(&f, Sf_Global(&f, "MisItems", "DazedReward"), '*', 8);
	if (Sf_Compare(&f, Sf_Attr(&f, e, "Money"), "<=", limit) != 0) {
		return Sf_None(&f);
	}
	PyObject *reward = Sf_ToInt(&f, Sf_Op(&f, Sf_Attr(&f, e, "Money"), '/', Sf_Global(&f, "MisItems", "DazedReward")));
	PyObject *pos = Sf_Attr(&f, e, "Pos");
	PyObject *radius = Sf_Op(&f, Sf_Int(&f, 120), '+', Sf_Attr(&f, e, "VRadius"));
	for (int i = 0; i < 360 && !f.failed; i += 45) {
		int mark = f.n;
		PyObject *yaw = Sf_Op(&f, Sf_Int(&f, i), '+', Sf_Item(&f, Sf_Attr(&f, e, "Ang"), 1));
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((Oii)(iOi))", radius, 0, 0, 0, yaw, 0);
		PyObject *posix = Sf_Ask(&f, "SVec", "Add", "(OO)", pos, rot);
		PyObject *x = Sf_Item(&f, posix, 0), *y = Sf_Item(&f, posix, 1), *z = Sf_Item(&f, posix, 2);
		PyObject *hit = Sf_Ask(&f, "Scrap", "TestSphere", "(OOOis)", x, y, z, 100, "rv");
		if (Sf_Compare(&f, hit, "==", Py_None) > 0) {
			Sf_Call(&f, Sf_Global(&f, "MisItems", "CreateCoin"), "(OO)", posix, reward);
			Sf_SetAttr(&f, e, "Money", Sf_Op(&f, Sf_Attr(&f, e, "Money"), '-', reward));
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

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
static PyObject *CrazyWing_InitCrazyWing(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *status = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "CrazyWing.Status");
	if (Sf_Compare(&f, status, "!=", Sf_Str(&f, "0")) > 0) {
		PyObject *now = Sf_Time(&f);
		PyObject *lo = Sf_Global(&f, "CrazyWing", "CrazyWingLoTime");
		PyObject *hi = Sf_Global(&f, "CrazyWing", "CrazyWingHiTime");
		PyObject *t = Sf_Op(&f, now, '+', Sf_Ask(&f, "Scrap", "Rand", "(OO)", lo, hi));
		Sf_Schedule(&f, t, Sf_Global(&f, "CrazyWing", "CrazyWing"), Sf_Keep(&f, PyTuple_New(0)), "CrazyWind");
	}
	return Sf_None(&f);
}

/* CrazyWing(): the wind blows: the next one, and a character goes crazy unless the alarm is on */
static PyObject *CrazyWing_CrazyWing(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Call(&f, Sf_Global(&f, "CrazyWing", "InitCrazyWing"), "()");
	if (Sf_EqI(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0), 0) > 0) {
		Sf_Call(&f, Sf_Global(&f, "CrazyWing", "SetCrazy"), "()");
	}
	return Sf_None(&f);
}

/* SetCrazy(): the character next to the player (relaxing or dancing, one of CrazyChars, not a mission target and not
 * photographable) goes crazy for CrazyWingTime seconds and can be photographed */
static PyObject *CrazyWing_SetCrazy(PyObject *self, PyObject *args) {
	static const char *const idle[] = { "Relax", "Dance", NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *me = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, me) <= 0) {
		return Sf_None(&f);
	}
	PyObject *pos = Sf_Attr(&f, me, "Pos");
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *guy = Sf_Ask(&f, "Scrap", "TestSphere", "(OOOisO)", x, y, z, 1000, "r", Sf_Attr(&f, me, "Name"));
	if (Sf_True(&f, guy) <= 0) {
		return Sf_None(&f);
	}
	PyObject *ch = Sf_Entity(&f, guy);
	Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Attr(&f, ch, "ActCtrl"));
	int go = Sf_True(&f, Sf_Attr(&f, ch, "RadarShow")) == 0;
	if (go > 0) {
		go = Sf_True(&f, Sf_Attr(&f, ch, "CanPhoto")) == 0;
	}
	if (go > 0) {
		go = Sf_InS(&f, Sf_Slice(&f, Sf_Attr(&f, ch, "Action"), 0, 5), idle);
	}
	if (go > 0) {
		go = Sf_In(&f, Sf_Attr(&f, ch, "ActCtrl"), Sf_Global(&f, "CrazyWing", "CrazyChars"));
	}
	if (go > 0) {
		Sf_SetAttrS(&f, ch, "Action", "CrazyInit");
		if (Sf_EqS(&f, Sf_Attr(&f, ch, "Action"), "CrazyInit") > 0) {
			PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Global(&f, "CrazyWing", "CrazyWingTime"));
			PyObject *fn = Sf_Global(&f, "CrazyWing", "DeCrazy");
			Sf_Schedule(&f, t, fn, Sf_Build(&f, "(OO)", guy, Sf_Attr(&f, ch, "CanPhoto")), NULL);
			Sf_SetAttrI(&f, ch, "CanPhoto", 1);
			Sf_SetAttrS(&f, ch, "DefLoop", "Crazy");
		}
	}
	return Sf_None(&f);
}

/* DeCrazy(CharName, CanPhoto): the character calms down */
static PyObject *CrazyWing_DeCrazy(PyObject *self, PyObject *args) {
	PyObject *name, *canPhoto;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &canPhoto)) {
		return NULL;
	}
	PyObject *ch = Sf_Entity(&f, name);
	if (Sf_True(&f, ch) > 0) {
		Sf_SetAttr(&f, ch, "CanPhoto", canPhoto);
		if (Sf_EqS(&f, Sf_Attr(&f, ch, "Action"), "Crazy") > 0) {
			Sf_SetAttrS(&f, ch, "Action", "CrazyEnd");
			Sf_SetAttrS(&f, ch, "ForceDefLoop", "Relax");
		}
	}
	return Sf_None(&f);
}

/* RestoreSpeech(): the player selects again */
static PyObject *CrazyWing_RestoreSpeech(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetAttrI(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "AutoSelect", 1);
	return Sf_None(&f);
}

/* AllInList(): every crazy character has been photographed */
static PyObject *CrazyWing_AllInList(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *chars = Sf_Global(&f, "CrazyWing", "CrazyChars");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *c = Sf_Next(&f, chars, i);
		if (!c) {
			break;
		}
		if (Sf_In(&f, c, Sf_Global(&f, "CrazyWing", "CrazyWingList")) == 0) {
			return Sf_Done(&f, PyInt_FromLong(0));
		}
		Sf_Release(&f, mark);
	}
	return Sf_Done(&f, PyInt_FromLong(1));
}

/* the prize text key with <n> as the amount, on the message line for 6 s */
static void Cw_PrizeMsg(scriptFrame_t *f, const char *key, PyObject *prize) {
	PyObject *text = Sf_Lang(f, Sf_Str(f, key));
	PyObject *msg = Sf_Ask(f, "string", "replace", "(OsO)", text, "<n>", Sf_Repr(f, prize));
	Sf_Ask(f, "SScorer", "SetMsgText", "(OO)", msg, Sf_OpI(f, Sf_Time(f), '+', 6));
}

/* the money of the player plus prize */
static void Cw_Pay(scriptFrame_t *f, PyObject *prize) {
	PyObject *money = Sf_Ask(f, "Scrap", "GetMoney", "()");
	Sf_Ask(f, "Scrap", "SetMoney", "(O)", Sf_Op(f, money, '+', prize));
}

/* CrazyPhoto(e): a photo of a crazy character: a prize (more for each new one, the big prize for all of them, the
 * small one for a repeated one); 1 when it was one of CrazyChars */
static PyObject *CrazyWing_CrazyPhoto(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Speech");
	Sf_Import(&f, "string");
	PyObject *ctr = Sf_Attr(&f, e, "ActCtrl");
	int crazy = Sf_In(&f, ctr, Sf_Global(&f, "CrazyWing", "CrazyChars"));
	if (crazy <= 0) {
		return Sf_Done(&f, crazy == 0 ? PyInt_FromLong(0) : NULL);
	}
	int seen = Sf_In(&f, ctr, Sf_Global(&f, "CrazyWing", "CrazyWingList"));
	if (seen > 0) {
		Cw_Pay(&f, Sf_Global(&f, "CrazyWing", "SecondPrize"));
		Cw_PrizeMsg(&f, "CrazyWindRepeat", Sf_Global(&f, "CrazyWing", "SecondPrize"));
	} else if (seen == 0) {
		Sf_Method(&f, Sf_Global(&f, "CrazyWing", "CrazyWingList"), "append", "(O)", ctr);
		int all = Sf_True(&f, Sf_Call(&f, Sf_Global(&f, "CrazyWing", "AllInList"), "()"));
		if (all > 0) {
			Cw_Pay(&f, Sf_Global(&f, "CrazyWing", "BigPrize"));
			Cw_PrizeMsg(&f, "CrazyWindAll", Sf_Global(&f, "CrazyWing", "BigPrize"));
			Sf_Ask(&f, "Scrap", "Achievement", "(i)", 12);
		} else if (all == 0) {
			PyObject *first = Sf_Global(&f, "CrazyWing", "FirstPrize");
			int n = Sf_Len(&f, Sf_Global(&f, "CrazyWing", "CrazyWingList"));
			PyObject *inc = n >= 0 ? Sf_Op(&f, Sf_Int(&f, n), '*', Sf_Global(&f, "CrazyWing", "IncPrize")) : NULL;
			PyObject *prize = Sf_Op(&f, first, '+', inc);
			Cw_Pay(&f, prize);
			Cw_PrizeMsg(&f, "CrazyWindNew", prize);
		}
	}
	Sf_SetAttrI(&f, e, "CanPhoto", 0);
	Sf_Ask(&f, "SaveGame", "SaveMoney", "()");
	PyObject *list = Sf_Repr(&f, Sf_Global(&f, "CrazyWing", "CrazyWingList"));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "CrazyWing.List", list);
	return Sf_Done(&f, PyInt_FromLong(1));
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
