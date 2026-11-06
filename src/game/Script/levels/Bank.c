/*
 * The scripts of the level Bank in C (Data.packed levels/bank/scripts, issue #2): the Map module (the NPCs, the
 * elevators, the gatekeeper of the bank master's room), MapTreasure (the treasure coins: a dollar starts a chain of
 * copper coins against the clock, the last one leaves a prize) and the missions Mission_CatchRudolph,
 * Mission_GetDataPart2 (the downloads at the desks of three functionaries) and Mission_InfiltrateBank (the gatekeeper,
 * the possession of the bank master and the scene DirectorDiscovered). The plain functions are data
 * (data/levels/bank/<module>.def): MapTreasure.Init, Mission_CatchRudolph.Init, Mission_GetDataPart2.SetNextMission,
 * Mission_InfiltrateBank.OnPossession and LoadScene.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, the discardable characters
 * _BankDirector, _Bishop, _Functionary, _Messenger), MapTreasure (TimeToSpawn, the models, PrizeModels, PrizeMoney,
 * Circuit, Coins, DealCoins and the state of a running chain shent, counter, idx), Mission_CatchRudolph (rudolph),
 * Mission_GetDataPart2 (funcNames, activatedDesktops, dataDownloaded, maxDataDownloaded, speedFactor, dd, firstTime),
 * Mission_InfiltrateBank (BankMasterName).
 */
#include "Levels.h"

#define MAP "Map"
#define MT "MapTreasure"
#define CR "Mission_CatchRudolph"
#define GD "Mission_GetDataPart2"
#define IB "Mission_InfiltrateBank"

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map next to Outskirts; the NPCs that come back, the bank master */
static PyObject *BankMap_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "CharsNPC");
	Sf_Import(&f, "Elevator");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Init", "OutArea", Sf_Str(&f, "Outskirts"));
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Sentinel", 2);
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Police", 3);
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Nurse", 2);
	if (Lv_PcOr(&f, MAP, "_BankDirector") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "BankDirector", 5);
	}
	if (Lv_PcOr(&f, MAP, "_Bishop") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Bishop", 2);
	}
	int m = Lv_PcOr(&f, MAP, "_Messenger");
	if (m > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Messenger", 2);
	} else if (m == 0) {
		Sf_Import(&f, "Messenger");
		Sf_SetMod(&f, "Messenger", "MakeMDP", Sf_Int(&f, 0));
	}
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Outskirts");
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_Metro_Outskirts");
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_Metro_Outskirts");
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission.Library"), "!=", Sf_Str(&f, "Mission_GetDataPart2")) > 0 &&
		Lv_PcOr(&f, MAP, "_Functionary") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Functionary", 3);
	}
	if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "BankMasterAtBank"), "1") > 0) {
		Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "BankMaster_Default");
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the elevators, the metro and the teleporter the gatekeeper of the bank master's room guards */
static PyObject *BankMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Import(&f, "GateKeeper");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Outskirts", "Outskirts");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([ss][]i)", "DM_Door_Teleporter_01", "DM_Door_Teleporter_02", 0);
	Sf_Ask(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_Metro_Outskirts");
	Sf_Ask(&f, "GateKeeper", "SetDoor", "(ss)", "GateKeeper_BankMasterRoom", "DM_Door_Teleporter_02");
	Sf_Ask(&f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_BankMasterRoom", Sf_Mod(&f, "GateKeeper", "BankMasterGroupGateKeeper"));
	PyObject *elv = Sf_Entity(&f, Sf_Str(&f, "DM_Door_Teleporter_02"));
	Sf_SetAttrS(&f, elv, "UseFunc", "GateKeeper.UseDoor");
	return Sf_None(&f);
}

PyMethodDef g_BankMapMethods[] = {
	{ "InitStatus", BankMap_InitStatus, METH_VARARGS, NULL },
	{ "Init", BankMap_Init, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapTreasure ---- */

#define MTG(name) Sf_Global(&f, MT, name)

/* SSound.Play(a[0], a[1]) with a the sound tuple MapSnd.<name> */
static void Mt_Sound(scriptFrame_t *f, const char *name) {
	PyObject *a = Sf_Mod(f, "MapSnd", name);
	Sf_Ask(f, "SSound", "Play", "(OO)", Sf_Item(f, a, 0), Sf_Item(f, a, 1));
}

/* Scrap.SetMoney(Scrap.GetMoney() + add) */
static void Mt_AddMoney(scriptFrame_t *f, PyObject *add) {
	Sf_Ask(f, "Scrap", "SetMoney", "(O)", Sf_Op(f, Sf_Ask(f, "Scrap", "GetMoney", "()"), '+', add));
}

/* Coins[idx]: the chain that runs */
static PyObject *Mt_Chain(scriptFrame_t *f) {
	return Sf_GetItem(f, Sf_Global(f, MT, "Coins"), Sf_Global(f, MT, "idx"));
}

/* InitDeal(ItemName, UsrName): the dollar of a chain taken: the copper coins appear one after the other, the first
 * blinks; the chain ends after 30 s */
static PyObject *MapTreasure_InitDeal(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "MapSnd");
	Sf_SetGlobal(&f, MT, "shent", Sf_Build(&f, "[]"));
	Mt_AddMoney(&f, Sf_Int(&f, 1));
	Mt_Sound(&f, "InitDeal");
	Sf_SetGlobal(&f, MT, "idx", Sf_ToInt(&f, Sf_Slice(&f, item, 8, 0x7fffffff)));
	PyObject *dl = Sf_GetItem(&f, MTG("Coins"), Sf_ToInt(&f, Sf_Slice(&f, item, 8, 0x7fffffff)));
	Sf_Later(&f, Sf_Int(&f, 30), MTG("DeleteShent"), Sf_Build(&f, "()"), "DeleteShent");
	PyObject *deal = MTG("DealCoins");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *l = Sf_Next(&f, deal, i);
		if (!l) {
			break;
		}
		if (Sf_True(&f, l) > 0) {
			Sf_SetAttrI(&f, l, "rTime", 86400);
		}
		Sf_Release(&f, mark);
	}
	Sf_SetGlobal(&f, MT, "counter", Sf_Int(&f, 0));
	PyObject *chain = Sf_Item(&f, dl, 4);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *pos = Sf_Next(&f, chain, i);
		if (!pos) {
			break;
		}
		PyObject *l = Sf_Own(&f, MT, "MakeCoin", "(OOO)", Sf_Cat(&f, "sO", "DealCoin", Sf_Repr(&f, MTG("counter"))), MTG("CopperModel"), pos);
		Sf_SetAttr(&f, l, "rTime", Sf_Op(&f, Sf_Op(&f, Sf_Float(&f, 0.1), '*', MTG("counter")), '+', Sf_Float(&f, 0.1)));
		Sf_SetAttrS(&f, l, "OnTake", "MapTreasure.TakeMiniDeal");
		Sf_Method(&f, MTG("shent"), "append", "(O)", l);
		Sf_SetGlobal(&f, MT, "counter", Sf_OpI(&f, MTG("counter"), '+', 1));
		Sf_Release(&f, mark);
	}
	Sf_SetAttrI(&f, Sf_Item(&f, MTG("shent"), 0), "Blink", 1);
	return Sf_None(&f);
}

/* TakeMiniDeal(ItemName, UsrName): a copper coin taken: the next blinks and the ticks run against the time of the
 * chain; the last one leaves the prize for 60 s */
static PyObject *MapTreasure_TakeMiniDeal(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "MapSnd");
	Mt_AddMoney(&f, Sf_Int(&f, 2));
	PyObject *l = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, l, "rTime", 86400);
	Sf_SetAttrI(&f, l, "Blink", 0);
	Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DeleteShent");
	Sf_SetGlobal(&f, MT, "counter", Sf_OpI(&f, MTG("counter"), '-', 1));
	PyObject *time = Sf_Time(&f);
	if (Sf_Compare(&f, MTG("counter"), "<=", Sf_Int(&f, 0)) > 0) {
		PyObject *coin = Mt_Chain(&f);
		l = Sf_Own(&f, MT, "MakeCoin", "(sOO)", "BigPrize", Sf_GetItem(&f, MTG("PrizeModels"), Sf_Item(&f, coin, 1)), Sf_Item(&f, Mt_Chain(&f), 3));
		Sf_SetAttrI(&f, l, "rTime", 1);
		Sf_SetAttrS(&f, l, "OnTake", "MapTreasure.BigPrize");
		Sf_Schedule(&f, Sf_OpI(&f, time, '+', 50), MTG("BlinkBigPrize"), Sf_Build(&f, "(O)", l), "DeleteShent");
		Sf_Schedule(&f, Sf_OpI(&f, time, '+', 60), Sf_Attr(&f, l, "Del"), Sf_Build(&f, "()"), "DeleteShent");
		Sf_Schedule(&f, Sf_OpI(&f, time, '+', 61), MTG("DeleteShent"), Sf_Build(&f, "()"), "DeleteShent");
		Mt_Sound(&f, "Price");
		return Sf_None(&f);
	}
	Mt_Sound(&f, "CoinTake");
	Sf_Schedule(&f, Sf_Op(&f, time, '+', Sf_Item(&f, Mt_Chain(&f), 0)), MTG("DeleteShent"), Sf_Build(&f, "()"), "DeleteShent");
	PyObject *deltimy = Sf_Mod(&f, "MapSnd", "TickInit");
	PyObject *timy = Sf_Op(&f, Sf_Item(&f, Mt_Chain(&f), 0), '-', deltimy);
	PyObject *bad = Sf_Mod(&f, "MapSnd", "BadCoin");
	Sf_Schedule(&f, Sf_Op(&f, Sf_Op(&f, time, '+', timy), '+', Sf_Mod(&f, "MapSnd", "TickAcc")), Sf_Mod(&f, "SSound", "Play"), bad, "DeleteShent");
	for (int boly = 0; !f.failed; ) {
		int mark = f.n;
		if (Sf_Compare(&f, Sf_Op(&f, timy, '-', deltimy), ">", Sf_Int(&f, 0)) <= 0) {
			break;
		}
		deltimy = Sf_Op(&f, deltimy, '+', Sf_Float(&f, 0.05));
		timy = Sf_Op(&f, timy, '-', deltimy);
		PyObject *tt = Sf_Mod(&f, "MapSnd", boly ? "TickTime" : "TickTime2");
		boly = !boly;
		Sf_Schedule(&f, Sf_Op(&f, time, '+', timy), Sf_Mod(&f, "SSound", "Play"), tt, "DeleteShent");
		PyObject *keep[2] = { deltimy, timy };
		Sf_ReleaseKeep(&f, mark, keep, 2);
	}
	PyObject *shent = MTG("shent");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *c = Sf_Next(&f, shent, i);
		if (!c) {
			break;
		}
		if (Sf_Compare(&f, Sf_Attr(&f, c, "rTime"), "==", Sf_Float(&f, 0.0)) > 0) {
			Sf_SetAttrI(&f, c, "Blink", 1);
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* BlinkBigPrize(l): the prize blinks before it goes */
static PyObject *MapTreasure_BlinkBigPrize(PyObject *self, PyObject *args) {
	PyObject *l;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &l)) {
		return NULL;
	}
	Sf_SetAttrI(&f, l, "Blink", 1);
	return Sf_None(&f);
}

/* BigPrize(ItemName, UsrName): the prize taken: the money, the next circuit of the chain (after the last one the
 * dollar goes), the chain ends */
static PyObject *MapTreasure_BigPrize(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Sound");
	PyObject *varname = Sf_Cat(&f, "sO", "Bank.Circuit.", Sf_Repr(&f, MTG("idx")));
	PyObject *ac = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", varname));
	ac = Sf_OpI(&f, ac, '+', 1);
	int n = Sf_Len(&f, Sf_GetItem(&f, MTG("Circuit"), MTG("idx")));
	if (n >= 0 && Sf_Compare(&f, ac, ">=", Sf_Int(&f, n)) > 0) {
		ac = Sf_Int(&f, 0);
		Sf_Method(&f, Sf_GetItem(&f, MTG("DealCoins"), MTG("idx")), "Del", "()");
		Sf_SetItem(&f, MTG("DealCoins"), MTG("idx"), Py_None);
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(OO)", varname, Sf_Repr(&f, ac));
	Sf_SetItem(&f, MTG("Coins"), MTG("idx"), Sf_GetItem(&f, Sf_GetItem(&f, MTG("Circuit"), MTG("idx")), ac));
	Mt_AddMoney(&f, Sf_GetItem(&f, MTG("PrizeMoney"), Sf_Item(&f, Mt_Chain(&f), 1)));
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "Stats.Treasure");
	PyObject *l = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, l, "rTime", 86400);
	Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Attr(&f, l, "Del"), Sf_Build(&f, "()"), NULL);
	PyObject *a = Sf_GetItem(&f, Sf_Mod(&f, "Sound", "ItemTake"), Sf_Str(&f, "MoneyItem"));
	Sf_Ask(&f, "SSound", "PlaySound", "(OOO)", Sf_Attr(&f, l, "Pos"), Sf_Item(&f, a, 0), Sf_Item(&f, a, 1));
	Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DeleteShent");
	Sf_Own(&f, MT, "DeleteShent", "()");
	return Sf_None(&f);
}

/* DeleteShent(): the coins of the chain hidden, RestartShents in 2.5 s */
static PyObject *MapTreasure_DeleteShent(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *shent = MTG("shent");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *l = Sf_Next(&f, shent, i);
		if (!l) {
			break;
		}
		Sf_SetAttrI(&f, l, "rTime", 86400);
		Sf_Release(&f, mark);
	}
	Sf_Later(&f, Sf_Float(&f, 2.5), MTG("RestartShents"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

/* RestartShents(): the coins of the chain deleted, its state forgotten, the dollars back one after the other */
static PyObject *MapTreasure_RestartShents(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *shent = MTG("shent");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *l = Sf_Next(&f, shent, i);
		if (!l) {
			break;
		}
		Sf_Method(&f, l, "Del", "()");
		Sf_Release(&f, mark);
	}
	Sf_DelGlobal(&f, MT, "shent");
	Sf_DelGlobal(&f, MT, "counter");
	Sf_DelGlobal(&f, MT, "idx");
	PyObject *ctr = Sf_Float(&f, 0.1), *deal = MTG("DealCoins");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *l = Sf_Next(&f, deal, i);
		if (!l) {
			break;
		}
		if (Sf_True(&f, l) > 0) {
			Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, Sf_Item(&f, MTG("Coins"), i), 2));
			Sf_SetAttr(&f, l, "rTime", ctr);
		}
		ctr = Sf_Op(&f, ctr, '+', Sf_Float(&f, 0.1));
		Sf_ReleaseKeep(&f, mark, &ctr, 1);
	}
	return Sf_None(&f);
}

/* InitStatus(LevelPath): the coin models preloaded */
static PyObject *MapTreasure_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", MTG("InitModel"));
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", MTG("CopperModel"));
	PyObject *models = MTG("PrizeModels");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *m = Sf_Next(&f, models, i);
		if (!m) {
			break;
		}
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(O)", m);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* MakeCoin(name, MoneyModel, pos): an item only the player takes */
static PyObject *MapTreasure_MakeCoin(PyObject *self, PyObject *args) {
	PyObject *name, *model, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &model, &pos)) {
		return NULL;
	}
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
	Sf_Method(&f, l, "SetModel", "(O)", model);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttrI(&f, l, "Radius", 40);
	return Sf_Return(&f, l);
}

/* CreateCoins(): the dollar of each chain */
static PyObject *MapTreasure_CreateCoins(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *coins = MTG("Coins");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *cn = Sf_Next(&f, coins, i);
		if (!cn) {
			break;
		}
		PyObject *l = Sf_Own(&f, MT, "MakeCoin", "(OOO)", Sf_Cat(&f, "sO", "Treasure", Sf_ReprI(&f, i)), MTG("InitModel"), Sf_Item(&f, cn, 2));
		Sf_SetAttrS(&f, l, "OnTake", "MapTreasure.InitDeal");
		Sf_Method(&f, MTG("DealCoins"), "append", "(O)", l);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

PyMethodDef g_BankMapTreasureMethods[] = {
	{ "InitDeal", MapTreasure_InitDeal, METH_VARARGS, NULL },
	{ "TakeMiniDeal", MapTreasure_TakeMiniDeal, METH_VARARGS, NULL },
	{ "BlinkBigPrize", MapTreasure_BlinkBigPrize, METH_VARARGS, NULL },
	{ "BigPrize", MapTreasure_BigPrize, METH_VARARGS, NULL },
	{ "DeleteShent", MapTreasure_DeleteShent, METH_VARARGS, NULL },
	{ "RestartShents", MapTreasure_RestartShents, METH_VARARGS, NULL },
	{ "InitStatus", MapTreasure_InitStatus, METH_VARARGS, NULL },
	{ "MakeCoin", MapTreasure_MakeCoin, METH_VARARGS, NULL },
	{ "CreateCoins", MapTreasure_CreateCoins, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_CatchRudolph ---- */

/* InitStatus(LevelPath): Rudolph, the functionary to catch */
static PyObject *CatchRudolph_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_MissionChar(&f, "Functionary_Rudolph", CR, "CreateRudolph");
	return Sf_None(&f);
}

/* CreateRudolph(r): Rudolph talks while he walks and runs away from the player */
static PyObject *CatchRudolph_CreateRudolph(PyObject *self, PyObject *args) {
	PyObject *r;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &r)) {
		return NULL;
	}
	Sf_SetAttrS(&f, r, "UseFunc", "Mission_CatchRudolph.OnCatchRudolph");
	Sf_SetAttr(&f, r, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, r, "LLogicEscapeWithinRadius", 1);
	Sf_SetAttrI(&f, r, "LLogicEscapeRadiusSqr", 562500);
	Sf_SetAttr(&f, r, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Rudolph")));
	Sf_SetGlobal(&f, CR, "rudolph", r);
	return Sf_None(&f);
}

/* OnCatchRudolph(UsrName, CharName, optId, FullOption): Rudolph caught; his talk ends in SetNextMission */
static PyObject *CatchRudolph_OnCatchRudolph(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_SetAttrI(&f, Sf_Entity(&f, ch), "LLogicEscapeWithinRadius", 0);
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Lv_OnEnd(&f, CR, "SetNextMission", NULL);
		Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Functionary");
		Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_GetRudolph_Rudolph_NI_1Msg");
		Sf_Ask(&f, "Speech", "RunNI", "()");
	}
	return Sf_None(&f);
}

/* SetNextMission(): Mission_KillRudolph in Outskirts, a character that can drive, the level changes */
static PyObject *CatchRudolph_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_KillRudolph", "Outskirts");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_KillRudolph_MissionText1", 1);
	Sf_Import(&f, "Chars");
	int in = Sf_In(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char"), Sf_Mod(&f, "Chars", "CanDriveList"));
	if (in == 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "PrevMap", "Levels/Bank");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorElevator");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Player_Spawn_Bank");
	Sf_Ask(&f, "SaveGame", "QLoad", "(s)", "Levels/Outskirts");
	return Sf_None(&f);
}

PyMethodDef g_BankCatchRudolphMethods[] = {
	{ "InitStatus", CatchRudolph_InitStatus, METH_VARARGS, NULL },
	{ "CreateRudolph", CatchRudolph_CreateRudolph, METH_VARARGS, NULL },
	{ "OnCatchRudolph", CatchRudolph_OnCatchRudolph, METH_VARARGS, NULL },
	{ "SetNextMission", CatchRudolph_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetDataPart2 ---- */

#define GDG(name) Sf_Global(&f, GD, name)
#define GDGF(name) Sf_Global(f, GD, name)

/* speedFactor * (maxDataDownloaded - maxDataDownloaded * dataDownloaded * (1.0 / float(maxDataDownloaded))) */
static PyObject *Gd_StopValue(scriptFrame_t *f) {
	PyObject *inv = Sf_Op(f, Sf_Float(f, 1.0), '/', Sf_ToFloat(f, GDGF("maxDataDownloaded")));
	PyObject *part = Sf_Op(f, Sf_Op(f, GDGF("maxDataDownloaded"), '*', GDGF("dataDownloaded")), '*', inv);
	return Sf_Op(f, GDGF("speedFactor"), '*', Sf_Op(f, GDGF("maxDataDownloaded"), '-', part));
}

/* ComputeAutoUpdateStopValue(): where the data bar stops */
static PyObject *GetDataPart2_ComputeAutoUpdateStopValue(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Sf_Return(&f, Gd_StopValue(&f));
}

/* SScorer.Set(0, "DataStatus", "IsAutoUpdatable", n); SScorer.Set(0, "DataStatus", "AutoUpdateStopValue",
 * ComputeAutoUpdateStopValue()) */
static void Gd_Bar(scriptFrame_t *f, int autoUpdate) {
	Sf_Ask(f, "SScorer", "Set", "(issi)", 0, "DataStatus", "IsAutoUpdatable", autoUpdate);
	Sf_Ask(f, "SScorer", "Set", "(issO)", 0, "DataStatus", "AutoUpdateStopValue", Sf_Own(f, GD, "ComputeAutoUpdateStopValue", "()"));
}

/* InitStatus(LevelPath): no data packs of the messengers, the engine callbacks of the functionaries, the three
 * functionaries of the mission */
static PyObject *GetDataPart2_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Messenger");
	Sf_SetMod(&f, "Messenger", "MakeMDP", Sf_Int(&f, 0));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "MessengerDataPackMissionArrow", 0);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "OnFunctionaryStartsWorking", "Mission_GetDataPart2.OnFuncStartsWorking");
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "OnFinishAutoUpdate", "Mission_GetDataPart2.OnDownloadComplete");
	Lv_MissionChar(&f, "Functionary_00", GD, "CreateFuncInit");
	Lv_MissionChar(&f, "Functionary_01", GD, "CreateFuncInit");
	Lv_MissionChar(&f, "Functionary_02", GD, "CreateFuncInit");
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "RespawnChars"), "append", "(s)", "Functionary");
	return Sf_None(&f);
}

/* Init(LevelPath): first the talk with the boss, then the downloads */
static PyObject *GetDataPart2_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	int talk = Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState"), "TalkToBoss");
	if (talk >= 0) {
		Sf_Own(&f, GD, talk ? "TalkToBoss" : "InitMission", "()");
	}
	return Sf_None(&f);
}

/* TalkToBoss(): Dtritus' desktop is the target, the boss talks through it */
static PyObject *GetDataPart2_TalkToBoss(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "DTritusDesktop");
	Sf_SetGlobal(&f, GD, "dd", Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop")));
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GDG("dd"), 1);
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", GDG("BossSpeech"));
	return Sf_None(&f);
}

/* BossSpeech(UsrName, CharName, optId, FullOption): the boss explains the mission, PreInitMission at the end */
static PyObject *GetDataPart2_BossSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GDG("dd"), 0);
	Lv_Speakers(&f, usr, ch);
	Lv_OnEnd(&f, GD, "PreInitMission", NULL);
	Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Boss");
	Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_TalkToBoss2__ComBoss_NI_1Msg");
	Sf_Ask(&f, "Speech", "RunNI", "()");
	return Sf_None(&f);
}

/* PreInitMission(): saved, the mission text, the desktop talks no more, the downloads start */
static PyObject *GetDataPart2_PreInitMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "DTritusDesktop");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetDataPart2_MissionText1", 1);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "Funcs");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Own(&f, GD, "InitMission", "()");
	return Sf_None(&f);
}

/* InitMission(): the data bar, the three desks, the callback of a functionary back at his desk */
static PyObject *GetDataPart2_InitMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "Menu", "MissionScorerCall", GDG("MissionScorer"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_00", 1);
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_01", 1);
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_02", 1);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "OnDefStateCallback", "Mission_GetDataPart2.OnFuncDefState");
	return Sf_None(&f);
}

/* SetDesktopState(desktopName, active): the desk offers the download, or its table goes up */
static PyObject *GetDataPart2_SetDesktopState(PyObject *self, PyObject *args) {
	PyObject *name, *active;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &active)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	int none = Sf_Compare(&f, e, "==", Py_None), on = none == 0 ? Sf_True(&f, active) : -1;
	if (none > 0) {
		Sf_Ask(&f, "Scrap", "ConsoleError", "(O)", Sf_Cat(&f, "Os", name, " Not found!"));
	} else if (on > 0) {
		Sf_SetAttrI(&f, e, "VRadius", 60);
		Sf_SetAttrI(&f, e, "UseMapCheck", 0);
		Sf_SetAttr(&f, e, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowMissionFile"));
		Sf_SetAttrI(&f, e, "ArrowCanMission", 1);
		Sf_SetAttrI(&f, e, "ArrowCanUse", 1);
		Sf_SetAttrI(&f, e, "AlwaysUse", 1);
		Sf_SetAttrI(&f, e, "RadarShow", 1);
		Sf_SetAttrI(&f, e, "Life", 0);
		Sf_SetAttrI(&f, e, "ArrnOpt", 0);
		Sf_SetAttr(&f, e, "ArrOpt0", Sf_Cat(&f, "sO", "DownloadData:", Sf_Lang(&f, Sf_Str(&f, "USE_Functionaries_DownloadData"))));
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_GetDataPart2.OnBeginDownloadData");
	} else if (on == 0) {
		Sf_SetAttrS(&f, e, "Action", "LoopUp");
		Sf_SetAttrS(&f, e, "DefLoop", "LoopUp");
		Sf_SetAttrI(&f, e, "ArrowCanMission", 0);
		Sf_SetAttrI(&f, e, "ArrowCanUse", 0);
		Sf_SetAttrI(&f, e, "AlwaysUse", 0);
		Sf_SetAttrI(&f, e, "RadarShow", 0);
		Sf_SetAttrS(&f, e, "UseFunc", "");
	}
	return Sf_None(&f);
}

/* CreateFuncInit(e): a functionary of the mission: his desk, his talk; he stops a download that runs at his desk */
static PyObject *GetDataPart2_CreateFuncInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	PyObject *desk = Sf_Cat(&f, "sO", "DM_Desktop", Sf_Attr(&f, e, "Name"));
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_GetDataPart2.OnFunctionarySpeech");
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenWorking", Sf_Float(&f, 1.0));
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenPlaying", Sf_Float(&f, 1.0));
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
	Sf_SetAttr(&f, e, "LLogicGoToWorkSpeed", Sf_Float(&f, 1.0));
	Sf_SetAttr(&f, e, "LLogicForcedDesktopAssigned", desk);
	int in = Sf_In(&f, desk, GDG("activatedDesktops"));
	if (in >= 0) {
		Sf_Own(&f, GD, in ? "AbortDownload" : "SetNormalState", "(O)", Sf_Attr(&f, e, "Name"));
	}
	return Sf_None(&f);
}

/* OnFunctionarySpeech(UsrName, CharName, optId, FullOption): what a functionary says, working or not */
static PyObject *GetDataPart2_OnFunctionarySpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		PyObject *e = Sf_Entity(&f, ch);
		if (Sf_True(&f, e) > 0) {
			int idle = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicKeepWorking"), 0);
			if (idle > 0) {
				Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_GetDataPart2_Functionary_InitMsg1", "Mission_GetDataPart2_Functionary_InitAns1");
			} else if (idle == 0) {
				Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_GetDataPart2_Functionary_InitMsg2", "Mission_GetDataPart2_Functionary_InitAns2");
			}
		}
	}
	return Sf_None(&f);
}

/* OnFuncDefState(funcName): a functionary back in his default state stops a download, or works normally */
static PyObject *GetDataPart2_OnFuncDefState(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	int c = Sf_True(&f, e);
	if (c > 0) {
		c = Sf_EqS(&f, Sf_Attr(&f, e, "ActCtrl"), "Functionary");
	}
	if (c > 0) {
		int busy = Sf_Compare(&f, GDG("dataDownloaded"), ">", Sf_Int(&f, 0));
		if (busy > 0) {
			Sf_Own(&f, GD, "AbortDownload", "(O)", name);
		} else if (busy == 0) {
			Sf_Own(&f, GD, "SetNormalState", "(O)", Sf_Attr(&f, e, "Name"));
		}
	}
	return Sf_None(&f);
}

/* SetStopDownloadingState(funcName): the functionary goes to sit at his desk, whatever the alarm */
static PyObject *GetDataPart2_SetStopDownloadingState(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttr(&f, e, "LLogicTimeToSitDown", Sf_Float(&f, 0.01));
		Sf_SetAttrI(&f, e, "LLogicKeepWorking", 1);
		Sf_SetAttrI(&f, e, "LLogicForceSpecialBehaviour", 1);
		Sf_SetAttrI(&f, e, "LLogicIgnoreAlarm", 1);
	}
	return Sf_None(&f);
}

/* SetNormalState(funcName): the functionary stops working and behaves normally */
static PyObject *GetDataPart2_SetNormalState(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttr(&f, e, "LLogicTimeToStopWorking", Sf_Float(&f, 0.01));
		Sf_SetAttrI(&f, e, "LLogicKeepWorking", 0);
		Sf_SetAttrI(&f, e, "LLogicForceCancelSpecialBehaviour", 1);
		Sf_SetAttr(&f, e, "LLogicTimeToSitDown", Sf_Float(&f, -0.01));
		Sf_SetAttrI(&f, e, "LLogicIgnoreAlarm", 0);
	}
	return Sf_None(&f);
}

/* AbortDownload(funcName): a functionary with local AI goes to stop the download */
static PyObject *GetDataPart2_AbortDownload(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_Compare(&f, e, "==", Py_None) > 0) {
		Sf_Print(&f, "O", Sf_Cat(&f, "sO", "No entity ", name));
		return Sf_None(&f);
	}
	int c = Sf_True(&f, Sf_Attr(&f, e, "isAI"));
	if (c == 0) {
		Sf_Print(&f, "O", Sf_Cat(&f, "sO", "No AI at ", name));
		return Sf_None(&f);
	}
	c = c > 0 ? Sf_True(&f, Sf_Attr(&f, e, "isLocalLogic")) : -1;
	if (c == 0) {
		Sf_Print(&f, "O", Sf_Cat(&f, "sO", "No Local logic at ", name));
		return Sf_None(&f);
	}
	if (c > 0 && Sf_True(&f, Sf_Attr(&f, e, "LLogicIgnoreAlarm")) == 0) {
		Sf_Own(&f, GD, "SetStopDownloadingState", "(O)", name);
	}
	return Sf_None(&f);
}

/* OnDownloadAborted(funcName): the functionary reached his desk: the desk offers the download again, the bar goes
 * back; without downloads all functionaries work normally */
static PyObject *GetDataPart2_OnDownloadAborted(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	if (Sf_Compare(&f, Sf_Mod(&f, "Menu", "MissionScorerCall"), "!=", Py_None) <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) <= 0) {
		return Sf_None(&f);
	}
	PyObject *desk = Sf_Cat(&f, "sO", "DM_Desktop", name);
	if (Sf_In(&f, desk, GDG("activatedDesktops")) > 0) {
		Sf_Method(&f, GDG("activatedDesktops"), "remove", "(O)", desk);
	}
	Sf_Own(&f, GD, "SetDesktopState", "(Oi)", desk, 1);
	Sf_Own(&f, GD, "SetNormalState", "(O)", Sf_Attr(&f, e, "Name"));
	int n = Sf_Len(&f, GDG("activatedDesktops"));
	if (n >= 0) {
		Sf_SetGlobal(&f, GD, "dataDownloaded", Sf_Int(&f, n));
	}
	if (Sf_Compare(&f, GDG("dataDownloaded"), ">=", Sf_Int(&f, 0)) > 0) {
		Gd_Bar(&f, 2);
	}
	if (Sf_Compare(&f, GDG("dataDownloaded"), "<=", Sf_Int(&f, 0)) > 0) {
		Sf_SetGlobal(&f, GD, "dataDownloaded", Sf_Int(&f, 0));
		PyObject *names = GDG("funcNames");
		for (int i = 0; !f.failed; i++) {
			int mark = f.n;
			PyObject *fn = Sf_Next(&f, names, i);
			if (!fn) {
				break;
			}
			if (Sf_Compare(&f, fn, "!=", name) > 0) {
				PyObject *other = Sf_Entity(&f, fn);
				if (Sf_True(&f, other) > 0) {
					Sf_Own(&f, GD, "SetNormalState", "(O)", Sf_Attr(&f, other, "Name"));
				}
			}
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* OnBeginDownloadData(UsrName, desktopName, optId, FullOption): a download starts at a desk; its functionary goes to
 * stop it */
static PyObject *GetDataPart2_OnBeginDownloadData(PyObject *self, PyObject *args) {
	PyObject *usr, *desk, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &desk, &opt, &full)) {
		return NULL;
	}
	Sf_Method(&f, GDG("activatedDesktops"), "append", "(O)", desk);
	Sf_Own(&f, GD, "SetDesktopState", "(Oi)", desk, 0);
	int n = Sf_Len(&f, GDG("activatedDesktops"));
	if (n >= 0) {
		Sf_SetGlobal(&f, GD, "dataDownloaded", Sf_Int(&f, n));
	}
	Gd_Bar(&f, 1);
	if (Sf_EqI(&f, GDG("dataDownloaded"), 1) > 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_GetDataPart2_RemoteMessage1", "Functionary");
	}
	PyObject *e = Sf_Entity(&f, Sf_Slice(&f, desk, 10, 0x7fffffff));
	int c = Sf_True(&f, e);
	if (c > 0) {
		c = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicKeepWorking"), 0);
	}
	if (c > 0) {
		Sf_Own(&f, GD, "AbortDownload", "(O)", Sf_Attr(&f, e, "Name"));
	}
	return Sf_None(&f);
}

/* OnFuncStartsWorking(funcName): the download at his desk is aborted when he has sat down */
static PyObject *GetDataPart2_OnFuncStartsWorking(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "Functionary");
	Sf_Ask(&f, "SAct", "GetAction", "(s)", "SitDown");
	PyObject *time = Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime");
	PyObject *at = Sf_Op(&f, Sf_Op(&f, Sf_Time(&f), '+', time), '+', Sf_Float(&f, 1.0));
	Sf_Schedule(&f, at, GDG("OnDownloadAborted"), Sf_Build(&f, "(O)", name), NULL);
	return Sf_None(&f);
}

/* OnDownloadComplete(): the bar is empty: the desks close, the functionaries work normally, the boss calls */
static PyObject *GetDataPart2_OnDownloadComplete(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *v = Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "DataStatus", "Value");
	if (Sf_Compare(&f, v, "==", Sf_Float(&f, 0.0)) <= 0) {
		return Sf_None(&f);
	}
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_00", 0);
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_01", 0);
	Sf_Own(&f, GD, "SetDesktopState", "(si)", "DM_DesktopFunctionary_02", 0);
	Sf_DelGlobal(&f, GD, "activatedDesktops");
	Sf_SetGlobal(&f, GD, "activatedDesktops", Sf_Build(&f, "[]"));
	PyObject *names = GDG("funcNames");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *fn = Sf_Next(&f, names, i);
		if (!fn) {
			break;
		}
		if (Sf_True(&f, Sf_Entity(&f, fn)) > 0) {
			Sf_Own(&f, GD, "SetNormalState", "(O)", fn);
		}
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_GetDataPart2_RemoteMessage2", "Boss", GDG("SetNextMission"), Sf_Build(&f, "()"));
	return Sf_None(&f);
}

/* the marker sprites of the data bar */
static void Gd_Marker(scriptFrame_t *f, PyObject *id, const char *item) {
	Sf_Ask(f, "SScorer", "Add", "(Oss)", id, item, "Sprite");
	Sf_Ask(f, "SScorer", "Set", "(Ossi)", id, item, "Red", 0);
	Sf_Ask(f, "SScorer", "Set", "(Ossi)", id, item, "Green", 255);
	Sf_Ask(f, "SScorer", "Set", "(Ossi)", id, item, "Blue", 0);
}

/* a marker at frac of the bar width */
static void Gd_Place(scriptFrame_t *f, PyObject *id, const char *item, PyObject *x, PyObject *y, PyObject *w, PyObject *h, double frac) {
	int mark = f->n;
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "X", Sf_Op(f, x, '+', Sf_Op(f, w, '*', Sf_Float(f, frac))));
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "Y", y);
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "ScaleX", Sf_Op(f, Sf_Float(f, 1.0), '/', Sf_Int(f, 2)));
	Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, "ScaleY", Sf_Op(f, h, '/', Sf_Int(f, 4)));
	Sf_Ask(f, "SScorer", "Set", "(Ossi)", id, item, "Visible", 1);
	Sf_Release(f, mark);
}

/* MissionScorer(id): the data bar with its two markers; full the first time */
static PyObject *GetDataPart2_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Lv_ScorerTextBegin(&f, id, "DataStatusText", 0);
	Lv_ScorerSet(&f, id, "DataStatusText", "Text", Sf_Cat(&f, "Os", Sf_Lang(&f, Sf_Str(&f, "Mission_GetDataPart2_Scorer_1")), ":"));
	Lv_ScorerTextEnd(&f, id, "DataStatusText");
	Gd_Marker(&f, id, "DataMarker1");
	Gd_Marker(&f, id, "DataMarker2");
	PyObject *v = Sf_Op(&f, GDG("maxDataDownloaded"), '*', GDG("speedFactor"));
	Sf_Ask(&f, "SScorer", "Add", "(Oss)", id, "DataStatus", "Bar");
	Sf_Ask(&f, "Scorer", "MissionScorerBar_SetDefaults", "(OsOOO)", id, "DataStatus", Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBack"),
		Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorMain"), Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBlack"));
	Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "DataStatus", "MaxValue", v);
	if (Sf_EqI(&f, GDG("firstTime"), 1) > 0) {
		Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "DataStatus", "IsAutoUpdatable", 1);
		Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "DataStatus", "Value", v);
		Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "DataStatus", "AutoUpdateStopValue", v);
		Sf_SetGlobal(&f, GD, "firstTime", Sf_Int(&f, 0));
	}
	PyObject *x = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "DataStatus", "X");
	PyObject *y = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "DataStatus", "Y");
	PyObject *w = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "DataStatus", "W");
	PyObject *h = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "DataStatus", "H");
	Gd_Place(&f, id, "DataMarker1", x, y, w, h, 0.333);
	Gd_Place(&f, id, "DataMarker2", x, y, w, h, 0.666);
	return Sf_None(&f);
}

PyMethodDef g_BankGetDataPart2Methods[] = {
	{ "ComputeAutoUpdateStopValue", GetDataPart2_ComputeAutoUpdateStopValue, METH_VARARGS, NULL },
	{ "InitStatus", GetDataPart2_InitStatus, METH_VARARGS, NULL },
	{ "Init", GetDataPart2_Init, METH_VARARGS, NULL },
	{ "TalkToBoss", GetDataPart2_TalkToBoss, METH_VARARGS, NULL },
	{ "BossSpeech", GetDataPart2_BossSpeech, METH_VARARGS, NULL },
	{ "PreInitMission", GetDataPart2_PreInitMission, METH_VARARGS, NULL },
	{ "InitMission", GetDataPart2_InitMission, METH_VARARGS, NULL },
	{ "SetDesktopState", GetDataPart2_SetDesktopState, METH_VARARGS, NULL },
	{ "CreateFuncInit", GetDataPart2_CreateFuncInit, METH_VARARGS, NULL },
	{ "OnFunctionarySpeech", GetDataPart2_OnFunctionarySpeech, METH_VARARGS, NULL },
	{ "OnFuncDefState", GetDataPart2_OnFuncDefState, METH_VARARGS, NULL },
	{ "SetStopDownloadingState", GetDataPart2_SetStopDownloadingState, METH_VARARGS, NULL },
	{ "SetNormalState", GetDataPart2_SetNormalState, METH_VARARGS, NULL },
	{ "AbortDownload", GetDataPart2_AbortDownload, METH_VARARGS, NULL },
	{ "OnDownloadAborted", GetDataPart2_OnDownloadAborted, METH_VARARGS, NULL },
	{ "OnBeginDownloadData", GetDataPart2_OnBeginDownloadData, METH_VARARGS, NULL },
	{ "OnFuncStartsWorking", GetDataPart2_OnFuncStartsWorking, METH_VARARGS, NULL },
	{ "OnDownloadComplete", GetDataPart2_OnDownloadComplete, METH_VARARGS, NULL },
	{ "MissionScorer", GetDataPart2_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_InfiltrateBank ---- */

#define IBG(name) Sf_Global(&f, IB, name)

/* InitStatus(LevelPath): Dtritus' resources; the player is not the bank master */
static PyObject *InfiltrateBank_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Ask(&f, "Chars", "AddNewCharRes", "(si)", "Dtritus", 0);
	if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char"), "BankMaster") > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	}
	return Sf_None(&f);
}

/* Init(LevelPath): Deep Throat's message, the teleporter to the bank master's room, the possession hooks, the scene */
static PyObject *InfiltrateBank_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	int first = Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState"), "FirstTime");
	if (first > 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_InfiltrateBank_RemoteMessage1", "DeepThroat");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "NotFirstTime");
	} else if (first == 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_InfiltrateBank_RemoteMessage2", "DeepThroat");
	}
	Sf_SetMod(&f, "GateKeeper", "AfterGateKeeperSpeech", IBG("ShowDeepThroatMessage"));
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "DM_Door_Teleporter_02")), "UseFunc", "Mission_InfiltrateBank.UseDoor");
	if (Sf_In(&f, Sf_Str(&f, "BankMaster"), Sf_Mod(&f, "CharConversor", "AvailableChars")) > 0) {
		Sf_Method(&f, Sf_Mod(&f, "CharConversor", "AvailableChars"), "remove", "(s)", "BankMaster");
	}
	Sf_SetMod(&f, "CharConversor", "OnPossession", IBG("OnPossession"));
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", IBG("ShowDeepThroatMessage"));
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "GateKeeper_BankMasterRoom");
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "BankMaster");
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "CanBePossessed", 1);
	Sf_Own(&f, IB, "LoadScene", "()");
	return Sf_None(&f);
}

/* UseDoor(UsrName, ElevatorName, optId, FullOption): the teleporter of the gatekeeper; the scene when it was used */
static PyObject *InfiltrateBank_UseDoor(PyObject *self, PyObject *args) {
	PyObject *usr, *elev, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &elev, &opt, &full)) {
		return NULL;
	}
	if (Sf_True(&f, Sf_Ask(&f, "GateKeeper", "UseDoor", "(OOOO)", usr, elev, opt, full)) > 0) {
		Sf_Own(&f, IB, "TestEndOfElevator", "()");
		Lv_Fade(&f, lv_clear, lv_black, 2.5);
	}
	return Sf_None(&f);
}

/* TestEndOfElevator(): the scene once the player walks again */
static PyObject *InfiltrateBank_TestEndOfElevator(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int walk = Sf_EqS(&f, Sf_Ask(&f, "SInput", "GetActionSet", "()"), "Walk");
	if (walk > 0) {
		Sf_Own(&f, IB, "LaunchScene", "(O)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
	} else if (walk == 0) {
		Sf_Later(&f, Sf_Float(&f, 0.1), IBG("TestEndOfElevator"), Sf_Build(&f, "()"), NULL);
	}
	return Sf_None(&f);
}

/* ReAssignTarget(): the bank master is the target as soon as he is there */
static PyObject *InfiltrateBank_ReAssignTarget(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int there = Sf_True(&f, Sf_Entity(&f, IBG("BankMasterName")));
	if (there == 0) {
		Sf_Later(&f, Sf_Float(&f, 0.5), IBG("ReAssignTarget"), Sf_Build(&f, "()"), NULL);
		return Sf_None(&f);
	}
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", IBG("BankMasterName"));
	return Sf_None(&f);
}

/* AfterDTMessage(): the teleporter is the target */
static PyObject *InfiltrateBank_AfterDTMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *elv = Sf_Entity(&f, Sf_Str(&f, "DM_Door_Teleporter_02"));
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, elv, "Name"));
	return Sf_None(&f);
}

/* ShowDeepThroatMessage(): SetArrowToBankMaster in 1 s (once) */
static PyObject *InfiltrateBank_ShowDeepThroatMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "SetArrowToBankMaster");
	Sf_Later(&f, Sf_Float(&f, 1.0), IBG("SetArrowToBankMaster"), Sf_Build(&f, "()"), "SetArrowToBankMaster");
	return Sf_None(&f);
}

/* SetArrowToBankMaster(): not possessing the bank master: Deep Throat says so, then the bank master is the target */
static PyObject *InfiltrateBank_SetArrowToBankMaster(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int c = Sf_True(&f, usr);
	if (c > 0) {
		c = Sf_Compare(&f, Sf_Attr(&f, usr, "ActCtrl"), "!=", Sf_Str(&f, "BankMaster"));
	}
	if (c > 0) {
		int said = Sf_True(&f, Sf_Ask(&f, "Speech", "RemoteComunication", "(ss)", "Mission_InfiltrateBank_RemoteMessage2", "DeepThroat"));
		if (said > 0) {
			Sf_Own(&f, IB, "ReAssignTarget", "()");
		} else if (said == 0) {
			Sf_Later(&f, Sf_Float(&f, 1.0), IBG("SetArrowToBankMaster"), Sf_Build(&f, "()"), "SetArrowToBankMaster");
		}
	}
	return Sf_None(&f);
}

/* CreateBankMaster(e): the bank master stays and talks (BankMasterSpeech) */
static PyObject *InfiltrateBank_CreateBankMaster(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_InfiltrateBank.BankMasterSpeech");
	return Sf_None(&f);
}

/* LaunchScene(Usr): the scene DirectorDiscovered, LaunchScene2 at its end; the player is Dtritus again */
static PyObject *InfiltrateBank_LaunchScene(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "GateKeeper");
	Sf_SetMod(&f, "GateKeeper", "AfterGateKeeperSpeech", Py_None);
	Sf_SetMod(&f, "Elevator", "OnElevatorReachUp", Py_None);
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Ask(&f, "Scene", "Play", "(s)", "DirectorDiscovered");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", IBG("LaunchScene2"));
	Sf_SetAttrS(&f, usr, "ActCtrl", "Dtritus");
	return Sf_None(&f);
}

/* LaunchScene2(Usr): after the scene the bank master stands at his place and talks; Berto calls */
static PyObject *InfiltrateBank_LaunchScene2(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Lv_Fade(&f, lv_black, lv_clear, 2.0);
	PyObject *dummy = Lv_Dummy(&f, "DM_BankMasterAfterScenePos", 1);
	PyObject *bm = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), IBG("BankMasterName"));
	Sf_Call(&f, Sf_Attr(&f, Sf_Attr(&f, bm, "CallbackFunc"), "append"), "(O)", IBG("CreateBankMaster"));
	Sf_SetAttr(&f, bm, "Pos", Sf_Item(&f, dummy, 1));
	PyObject *e = Sf_Entity(&f, IBG("BankMasterName"));
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttr(&f, e, "Pos", Sf_Item(&f, dummy, 1));
		Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, e, "Name"));
		Sf_SetAttr(&f, e, "LLogicOrigin", Sf_Item(&f, dummy, 1));
		Sf_Own(&f, IB, "CreateBankMaster", "(O)", e);
	}
	Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_InfiltrateBank_RemoteMessage4", "Berto", IBG("SetNextMission"), Sf_Build(&f, "()"));
	return Sf_None(&f);
}

/* BankMasterSpeech(UsrName, CharName, optId, FullOption): what the bank master says */
static PyObject *InfiltrateBank_BankMasterSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_MeetBerto_BankBoss_InitMsg1", "Mission_MeetBerto_BankBoss_InitAns1");
	}
	return Sf_None(&f);
}

/* SetNextMission(): the bank master can be possessed again, the hooks off, Mission_MeetBerto in Scrapyard */
static PyObject *InfiltrateBank_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "CharConversor");
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Method(&f, Sf_Mod(&f, "CharConversor", "AvailableChars"), "append", "(s)", "BankMaster");
	Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", Py_None);
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_MeetBerto", "Scrapyard");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_MeetBerto_MissionText1", 1);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

PyMethodDef g_BankInfiltrateBankMethods[] = {
	{ "InitStatus", InfiltrateBank_InitStatus, METH_VARARGS, NULL },
	{ "Init", InfiltrateBank_Init, METH_VARARGS, NULL },
	{ "UseDoor", InfiltrateBank_UseDoor, METH_VARARGS, NULL },
	{ "TestEndOfElevator", InfiltrateBank_TestEndOfElevator, METH_VARARGS, NULL },
	{ "ReAssignTarget", InfiltrateBank_ReAssignTarget, METH_VARARGS, NULL },
	{ "AfterDTMessage", InfiltrateBank_AfterDTMessage, METH_VARARGS, NULL },
	{ "ShowDeepThroatMessage", InfiltrateBank_ShowDeepThroatMessage, METH_VARARGS, NULL },
	{ "SetArrowToBankMaster", InfiltrateBank_SetArrowToBankMaster, METH_VARARGS, NULL },
	{ "CreateBankMaster", InfiltrateBank_CreateBankMaster, METH_VARARGS, NULL },
	{ "LaunchScene", InfiltrateBank_LaunchScene, METH_VARARGS, NULL },
	{ "LaunchScene2", InfiltrateBank_LaunchScene2, METH_VARARGS, NULL },
	{ "BankMasterSpeech", InfiltrateBank_BankMasterSpeech, METH_VARARGS, NULL },
	{ "SetNextMission", InfiltrateBank_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
