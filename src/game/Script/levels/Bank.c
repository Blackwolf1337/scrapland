/*
 * The scripts of the level Bank in C (Data.packed levels/bank/scripts, issue #2): the Map module (the NPCs, the
 * elevators, the gatekeeper of the bank master's room), MapTreasure (the treasure coins: a dollar starts a chain of
 * copper coins against the clock, the last one leaves a prize) and the missions Mission_CatchRudolph,
 * Mission_GetDataPart2 (the downloads at the desks of three functionaries) and Mission_InfiltrateBank (the gatekeeper,
 * the possession of the bank master and the scene DirectorDiscovered). The plain functions are data
 * (data/levels/bank/<module>.def): MapTreasure.Init, Mission_CatchRudolph.Init, Mission_GetDataPart2.SetNextMission,
 * Mission_InfiltrateBank.OnPossession and LoadScene.
 *
 * The module code is data/levels/bank/<module>.mod; the loop of MapTreasure over the circuits is the hook
 * MapTreasure_ModCoins. The module variables stay in the modules: Map (Libraries, the discardable characters
 * _BankDirector, _Bishop, _Functionary, _Messenger), MapTreasure (TimeToSpawn, the models, PrizeModels, PrizeMoney,
 * Circuit, Coins, DealCoins and the state of a running chain shent, counter, idx), Mission_CatchRudolph (rudolph),
 * Mission_GetDataPart2 (funcNames, activatedDesktops, dataDownloaded, maxDataDownloaded, speedFactor, dd, firstTime),
 * Mission_InfiltrateBank (BankMasterName).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of the
 * modules are read where the script reads them and written through at once; the lists shent, DealCoins, Coins and
 * activatedDesktops are changed in place on the objects of the module, the coins and desks of the chains and missions
 * are handle values, a list of handles (shent, DealCoins) is read as a native list of handle values (OBJLIB 5, 7).
 * CharsNPC, Elevator, Metro, GateKeeper, Messenger, Chars, CharConversor, DTritusDesktop, MissionsFuncs, SaveGame,
 * Speech, Scene, Menu, Scorer and the functions of the module go by name (OBJLIB 3); the character objects of
 * CharsNPC.CharRes and their CallbackFunc lists stay Python objects (OBJLIB 5). Through their seams (OBJLIB 7):
 * Scrap.AddScheduledFunc, DeleteScheduledFuncs, CreateEntity, Preload3DObject, Set, ConsoleError, UsrEntity and
 * DelSaveVars, SSound.Play, SScorer.Set and Fade, the SAct functions (fc-r6-ib records SetCls) and SInput.GetActionSet
 * in TestEndOfElevator (fc-r6-ib swaps it). MapTreasure_ModCoins, the module code, stays on the Python objects
 * (OBJLIB 6).
 */
#include <limits.h>
#include <string.h>

#include "scrap/input.h"
#include "scrap/scrap.h"
#include "Levels.h"

#define MAP "Map"
#define MT "MapTreasure"
#define CR "Mission_CatchRudolph"
#define GD "Mission_GetDataPart2"
#define IB "Mission_InfiltrateBank"

/* ---- shared lines ---- */

/* [a] or [a, b] (b NULL: one item), [] for a NULL a */
static void Ba_List(nativeVal_t *out, const char *a, const char *b) {
	Native_SetEmpty(out, NV_LIST);
	if (a) {
		LvN_Str(Native_Append(out), a);
	}
	if (b) {
		LvN_Str(Native_Append(out), b);
	}
}

/* prefix + v of the script */
static byte Ba_Cat(const char *prefix, const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t p = { 0 };
	LvN_Str(&p, prefix);
	Native_SetNone(out);
	byte ok = Mode_Arith('+', &p, v, out);
	Native_Free(&p);
	return ok;
}

/* Scrap.GetEntity(name) of a value as the handle value, then `if e:` (1, 0, -1) */
static int Ba_EntTrue(const nativeVal_t *name, nativeVal_t *e) {
	Native_SetNone(e);
	return Mode_EntityH(name, e) ? Native_True(e) : -1;
}

/* Scrap.DelSaveVars(prefix) through its seam (fr7b.py _SPK records it) */
static byte Ba_DelSaves(const char *prefix) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "DelSaveVars", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "s", prefix);
	}
	SaveVars_Delete(prefix);
	return 1;
}

/* the core of a level function on its first argument */
#define BA_CORE1(fn) static byte fn##_C(const nativeVal_t *a, nativeVal_t *out) { (void)out; return fn(&a[0]); }

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map next to Outskirts; the NPCs that come back, the bank master */
static byte BankMap_InitStatus(void) {
	nativeVal_t v = { 0 };
	if (!Mode_Import("Init") || !Mode_Import("CharsNPC") || !Mode_Import("Elevator")) {
		return 0;
	}
	LvN_Int(&v, 0);
	if (!Mode_SetModVar("Init", "Outdoor", &v)) {
		return 0;
	}
	LvN_Str(&v, "Outskirts");
	byte ok = Mode_SetModVar("Init", "OutArea", &v);
	Native_Free(&v);
	if (!ok || !LvN_Repos("Sentinel", 2) || !LvN_Repos("Police", 3) || !LvN_Repos("Nurse", 2)) {
		return 0;
	}
	int c = LvN_PcOr(MAP, "_BankDirector");
	if (c < 0 || (c > 0 && !LvN_Repos("BankDirector", 5))) {
		return 0;
	}
	c = LvN_PcOr(MAP, "_Bishop");
	if (c < 0 || (c > 0 && !LvN_Repos("Bishop", 2))) {
		return 0;
	}
	c = LvN_PcOr(MAP, "_Messenger");
	if (c > 0) {
		ok = LvN_Repos("Messenger", 2);
	} else if (c == 0) {
		LvN_Int(&v, 0);
		ok = Mode_Import("Messenger") && Mode_SetModVar("Messenger", "MakeMDP", &v);
	} else {
		ok = 0;
	}
	if (!ok || !LV_CALL("Elevator", "SetDef", NULL, "ss", "Elevator", "DM_Door_Elevator_Outskirts") ||
		!LV_CALL("Elevator", "SetDef", NULL, "ss", "Metro", "DM_Door_Metro_Outskirts") ||
		!LV_CALL("Elevator", "SetDef", NULL, "ss", "MetroExt", "DM_Door_Metro_Outskirts")) {
		return 0;
	}
	if (!LvN_SaveIs("Mission.Library", "Mission_GetDataPart2")) {
		c = LvN_PcOr(MAP, "_Functionary");
		if (c < 0 || (c > 0 && !LvN_Repos("Functionary", 3))) {
			return 0;
		}
	}
	return !LvN_SaveIs("BankMasterAtBank", "1") || LV_CALL("CharsNPC", "CreateMissionChar", NULL, "s", "BankMaster_Default");
}

/* Init(LevelPath): the elevators, the metro and the teleporter the gatekeeper of the bank master's room guards */
static byte BankMap_Init(void) {
	nativeVal_t a = { 0 }, b = { 0 }, foe = { 0 }, name = { 0 }, elv = { 0 };
	if (!Mode_Import("Elevator") || !Mode_Import("Metro") || !Mode_Import("GateKeeper")) {
		return 0;
	}
	Ba_List(&a, "DM_Door_Elevator_Outskirts", NULL);
	Ba_List(&b, "Outskirts", NULL);
	byte ok = LV_CALL("Elevator", "ConnectElevators", NULL, "vv", &a, &b);
	Native_Free(&a);
	Native_Free(&b);
	Ba_List(&a, "DM_Door_Teleporter_01", "DM_Door_Teleporter_02");
	Ba_List(&b, NULL, NULL);
	ok = ok && LV_CALL("Elevator", "ConnectElevators", NULL, "vvi", &a, &b, 0L);
	Native_Free(&a);
	Native_Free(&b);
	ok = ok && LV_CALL("Metro", "SetAsMetro", NULL, "s", "DM_Door_Metro_Outskirts") &&
		LV_CALL("GateKeeper", "SetDoor", NULL, "ss", "GateKeeper_BankMasterRoom", "DM_Door_Teleporter_02");
	Native_SetNone(&foe);
	Native_SetNone(&elv);
	ok = ok && LvN_ModGet("GateKeeper", "BankMasterGroupGateKeeper", &foe) &&
		LV_CALL("GateKeeper", "DoorFoe", NULL, "sv", "GateKeeper_BankMasterRoom", &foe);
	LvN_Str(&name, "DM_Door_Teleporter_02");
	ok = ok && Mode_EntityH(&name, &elv) && LvN_SetS(&elv, "UseFunc", "GateKeeper.UseDoor");
	Native_Free(&foe);
	Native_Free(&name);
	Native_Free(&elv);
	return ok;
}

LV_CORE0(BankMap_InitStatus)
LV_CORE0(BankMap_Init)
LV_FN(BankMap, InitStatusPy, 1, BankMap_InitStatus_C)
LV_FN(BankMap, InitPy, 1, BankMap_Init_C)

PyMethodDef g_BankMapMethods[] = {
	{ "InitStatus", BankMap_InitStatusPy, METH_VARARGS, NULL },
	{ "Init", BankMap_InitPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapTreasure ---- */

/* a = MapSnd.<name>; SSound.Play(a[0], a[1]) */
static byte Mt_Sound(const char *name) {
	nativeVal_t a = { 0 }, s = { 0 }, v = { 0 };
	Native_SetNone(&s);
	Native_SetNone(&v);
	byte ok = LvN_ModGet("MapSnd", name, &a) && LvN_Item(&a, 0, &s) && LvN_Item(&a, 1, &v) && Mode_Play(&s, &v);
	Native_Free(&a);
	Native_Free(&s);
	Native_Free(&v);
	return ok;
}

/* Scrap.SetMoney(Scrap.GetMoney() + add) */
static byte Mt_AddMoney(const nativeVal_t *add) {
	nativeVal_t m = { 0 }, r = { 0 };
	Native_SetInt(&m, Mode_GetMoney());
	Native_SetNone(&r);
	byte ok = Mode_Arith('+', &m, add, &r) && Mode_SetMoney(&r);
	Native_Free(&r);
	return ok;
}

static byte Mt_AddMoneyI(long add) {
	nativeVal_t a = { 0 };
	Native_SetInt(&a, add);
	return Mt_AddMoney(&a);
}

/* Coins[idx][i]: an item of the chain that runs (i < 0: the chain itself) */
static byte Mt_Chain(long i, nativeVal_t *out) {
	nativeVal_t coins = { 0 }, idx = { 0 }, c = { 0 };
	Native_SetNone(&idx);
	Native_SetNone(&c);
	Native_SetNone(out);
	byte ok = Mode_G(MT, "Coins", &coins) && Mode_G(MT, "idx", &idx) && Native_Subscript(&coins, &idx, i < 0 ? out : &c) &&
		(i < 0 || LvN_Item(&c, i, out));
	Native_Free(&coins);
	Native_Free(&idx);
	Native_Free(&c);
	return ok;
}

/* Scrap.AddScheduledFunc(at, fn, args, "DeleteShent") with args a value of the script (the tuples of MapSnd): its
 * items; another value goes to the engine function by name as it is (its error) */
static byte Mt_Sched(const nativeVal_t *at, PyObject *fn, const nativeVal_t *args) {
	nativeVal_t name = { 0 }, items[4] = { 0 };
	LvN_Str(&name, "DeleteShent");
	byte ok;
	int n = args->type == NV_TUPLE || args->type == NV_VEC ? Native_Size(args) : -1;
	if (n >= 0 && n <= 4) {
		int k = 0;
		ok = 1;
		for (; k < n && ok; k++) {
			ok = Native_SeqItem(args, k, &items[k]);
		}
		if (!ok) {
			k--;
		}
		ok = ok && Mode_Schedule(at, fn, items, n, &name);
		while (k-- > 0) {
			Native_Free(&items[k]);
		}
	} else {
		ok = LV_CALL("Scrap", "AddScheduledFunc", NULL, "vOvv", at, fn, args, &name);
	}
	Native_Free(&name);
	return ok;
}

/* the same at time + delay with fn the global func of the module and args () or (arg) */
static byte Mt_SchedG(const nativeVal_t *time, long delay, const char *func, const nativeVal_t *arg) {
	nativeVal_t at = { 0 }, d = { 0 }, name = { 0 };
	PyObject *fn = Mode_GlobalPy(MT, func);
	if (!fn) {
		return 0;
	}
	Native_SetInt(&d, delay);
	Native_SetNone(&at);
	LvN_Str(&name, "DeleteShent");
	byte ok = Mode_Arith('+', time, &d, &at) && Mode_Schedule(&at, fn, arg, arg ? 1 : 0, &name);
	Native_Free(&at);
	Native_Free(&name);
	Py_DECREF(fn);
	return ok;
}

/* InitDeal(ItemName, UsrName): the dollar of a chain taken: the copper coins appear one after the other, the first
 * blinks; the chain ends after 30 s */
static byte MapTreasure_InitDeal(const nativeVal_t *item) {
	nativeVal_t v = { 0 }, s = { 0 }, n = { 0 }, coins = { 0 }, dl = { 0 }, deal = { 0 }, chain = { 0 };
	Native_SetNone(&s);
	Native_SetNone(&n);
	Native_SetNone(&coins);
	Native_SetNone(&dl);
	Native_SetNone(&deal);
	Native_SetNone(&chain);
	if (!Mode_Import("Sound") || !Mode_Import("MapSnd")) {
		return 0;
	}
	Native_SetEmpty(&v, NV_LIST);
	byte ok = Mode_Put(MT, "shent", NULL, &v) && Mt_AddMoneyI(1) && Mt_Sound("InitDeal") &&
		Native_Slice(item, 8, INT_MAX, &s) && Native_ToInt(&s, &n) && Mode_Put(MT, "idx", NULL, &n);
	Native_Free(&s);
	Native_Free(&n);
	ok = ok && Mode_G(MT, "Coins", &coins) && Native_Slice(item, 8, INT_MAX, &s) && Native_ToInt(&s, &n) &&
		Native_Subscript(&coins, &n, &dl) && Mode_Later(30, MT, "DeleteShent", NULL, 0, "DeleteShent") &&
		LvN_GElems(MT, "DealCoins", &deal);
	int count = ok ? Native_ForCount(&deal) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t l = { 0 };
		ok = Native_SeqItem(&deal, i, &l) && (!Native_True(&l) || LvN_SetI(&l, "rTime", 86400));
		Native_Free(&l);
	}
	ok = ok && LvN_PutI(MT, "counter", 0) && LvN_Item(&dl, 4, &chain);
	count = ok ? Native_ForCount(&chain) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t pos = { 0 }, ctr = { 0 }, rep = { 0 }, name = { 0 }, model = { 0 }, l = { 0 }, t = { 0 }, f = { 0 };
		Native_SetNone(&ctr);
		Native_SetNone(&rep);
		Native_SetNone(&name);
		Native_SetNone(&model);
		Native_SetNone(&l);
		Native_SetNone(&t);
		ok = Native_SeqItem(&chain, i, &pos) && Mode_G(MT, "counter", &ctr);
		if (ok) {
			Mode_Repr(&ctr, &rep);
			ok = Ba_Cat("DealCoin", &rep, &name) && Mode_G(MT, "CopperModel", &model) &&
				LV_OWN(MT, "MakeCoin", &l, "vvv", &name, &model, &pos);
		}
		/* l.rTime = 0.1 * counter + 0.1 */
		Native_Free(&ctr);
		Native_SetFloat(&f, 0.1);
		ok = ok && Mode_G(MT, "counter", &ctr) && Mode_Arith('*', &f, &ctr, &t);
		Native_Free(&ctr);
		if (ok) {
			ok = Mode_Arith('+', &t, &f, &ctr) && Mode_VSet(&l, "rTime", &ctr) &&
				LvN_SetS(&l, "OnTake", "MapTreasure.TakeMiniDeal") && Mode_Append(MT, "shent", &l) &&
				LvN_GAdd(MT, "counter", 1, NULL);
		}
		Native_Free(&pos);
		Native_Free(&ctr);
		Native_Free(&rep);
		Native_Free(&name);
		Native_Free(&model);
		Native_Free(&l);
		Native_Free(&t);
	}
	if (ok) {
		nativeVal_t shent = { 0 }, first = { 0 };
		Native_SetNone(&first);
		ok = LvN_GElems(MT, "shent", &shent) && LvN_Item(&shent, 0, &first) && LvN_SetI(&first, "Blink", 1);
		Native_Free(&shent);
		Native_Free(&first);
	}
	Native_Free(&s);
	Native_Free(&n);
	Native_Free(&coins);
	Native_Free(&dl);
	Native_Free(&deal);
	Native_Free(&chain);
	return ok;
}

/* the prize at the end of a chain: BigPrize for 60 s, blinking from 50 s */
static byte Mt_Prize(const nativeVal_t *time) {
	nativeVal_t coin = { 0 }, models = { 0 }, k = { 0 }, model = { 0 }, pos = { 0 }, name = { 0 }, l = { 0 };
	nativeVal_t at = { 0 }, d = { 0 };
	Native_SetNone(&models);
	Native_SetNone(&k);
	Native_SetNone(&model);
	Native_SetNone(&pos);
	Native_SetNone(&l);
	Native_SetNone(&at);
	LvN_Str(&name, "BigPrize");
	byte ok = Mt_Chain(-1, &coin) && Mode_G(MT, "PrizeModels", &models) && LvN_Item(&coin, 1, &k) &&
		Native_Subscript(&models, &k, &model) && Mt_Chain(3, &pos) && LV_OWN(MT, "MakeCoin", &l, "vvv", &name, &model, &pos) &&
		LvN_SetI(&l, "rTime", 1) && LvN_SetS(&l, "OnTake", "MapTreasure.BigPrize") && Mt_SchedG(time, 50, "BlinkBigPrize", &l);
	if (ok) {
		PyObject *del = Mode_VBound(&l, "Del");
		LvN_Int(&d, 60);
		ok = del && Mode_Arith('+', time, &d, &at);
		if (ok) {
			nativeVal_t nm = { 0 };
			LvN_Str(&nm, "DeleteShent");
			ok = Mode_Schedule(&at, del, NULL, 0, &nm);
			Native_Free(&nm);
		}
		Py_XDECREF(del);
	}
	ok = ok && Mt_SchedG(time, 61, "DeleteShent", NULL) && Mt_Sound("Price");
	Native_Free(&coin);
	Native_Free(&models);
	Native_Free(&k);
	Native_Free(&model);
	Native_Free(&pos);
	Native_Free(&name);
	Native_Free(&l);
	Native_Free(&at);
	return ok;
}

/* the ticks against the time of the chain, the next coin blinks */
static byte Mt_Ticks(const nativeVal_t *time) {
	nativeVal_t t0 = { 0 }, at = { 0 }, deltimy = { 0 }, timy = { 0 }, bad = { 0 }, acc = { 0 }, x = { 0 };
	nativeVal_t tt = { 0 }, step = { 0 }, zero = { 0 };
	PyObject *play = NULL;
	Native_SetNone(&t0);
	Native_SetNone(&at);
	Native_SetNone(&deltimy);
	Native_SetNone(&timy);
	Native_SetNone(&bad);
	Native_SetNone(&acc);
	Native_SetNone(&x);
	Native_SetFloat(&step, 0.05);
	Native_SetInt(&zero, 0);
	PyObject *fn = Mode_GlobalPy(MT, "DeleteShent");
	byte ok = fn && Mt_Chain(0, &t0) && Mode_Arith('+', time, &t0, &at) && Mt_Sched(&at, fn, LvN_NoArgs());
	Py_XDECREF(fn);
	Native_Free(&t0);
	Native_Free(&at);
	ok = ok && LvN_ModGet("MapSnd", "TickInit", &deltimy) && Mt_Chain(0, &t0) && Mode_Arith('-', &t0, &deltimy, &timy) &&
		LvN_ModGet("MapSnd", "BadCoin", &bad) && Mode_Arith('+', time, &timy, &x) && LvN_ModGet("MapSnd", "TickAcc", &acc) &&
		Mode_Arith('+', &x, &acc, &at) && (play = Script_Attr("SSound", "Play")) != NULL && Mt_Sched(&at, play, &bad);
	for (int boly = 0; ok;) {
		Native_Free(&x);
		ok = Mode_Arith('-', &timy, &deltimy, &x);
		if (!ok || Native_Compare(&x, &zero) <= 0) {
			break;
		}
		nativeVal_t d2 = { 0 }, t2 = { 0 };
		Native_SetNone(&d2);
		Native_SetNone(&t2);
		Native_SetNone(&tt);
		ok = Mode_Arith('+', &deltimy, &step, &d2);
		if (ok) {
			Native_Free(&deltimy);
			deltimy = d2;
			ok = Mode_Arith('-', &timy, &deltimy, &t2);
		}
		if (ok) {
			Native_Free(&timy);
			timy = t2;
		}
		ok = ok && LvN_ModGet("MapSnd", boly ? "TickTime" : "TickTime2", &tt);
		boly = !boly;
		Native_Free(&at);
		Native_SetNone(&at);
		Py_XDECREF(play);
		play = NULL;
		ok = ok && Mode_Arith('+', time, &timy, &at) && (play = Script_Attr("SSound", "Play")) != NULL && Mt_Sched(&at, play, &tt);
		Native_Free(&tt);
	}
	Py_XDECREF(play);
	if (ok) {
		nativeVal_t shent = { 0 }, zf = { 0 };
		Native_SetFloat(&zf, 0.0);
		ok = LvN_GElems(MT, "shent", &shent);
		int count = ok ? Native_ForCount(&shent) : -1;
		ok = count >= 0;
		for (int i = 0; i < count && ok; i++) {
			nativeVal_t c = { 0 }, r = { 0 };
			Native_SetNone(&r);
			ok = Native_SeqItem(&shent, i, &c) && Mode_VGet(&c, "rTime", &r);
			byte hit = ok && !Native_Compare(&r, &zf);
			ok = ok && (!hit || LvN_SetI(&c, "Blink", 1));
			Native_Free(&c);
			Native_Free(&r);
			if (hit) {
				break;
			}
		}
		Native_Free(&shent);
	}
	Native_Free(&at);
	Native_Free(&deltimy);
	Native_Free(&timy);
	Native_Free(&bad);
	Native_Free(&acc);
	Native_Free(&x);
	return ok;
}

/* TakeMiniDeal(ItemName, UsrName): a copper coin taken: the next blinks and the ticks run against the time of the
 * chain; the last one leaves the prize for 60 s */
static byte MapTreasure_TakeMiniDeal(const nativeVal_t *item) {
	nativeVal_t l = { 0 }, ctr = { 0 }, zero = { 0 }, time = { 0 };
	Native_SetNone(&l);
	Native_SetNone(&ctr);
	if (!Mode_Import("Sound") || !Mode_Import("MapSnd")) {
		return 0;
	}
	byte ok = Mt_AddMoneyI(2) && Mode_EntityH(item, &l) && LvN_SetI(&l, "rTime", 86400) && LvN_SetI(&l, "Blink", 0) &&
		Mode_DelSchedS("DeleteShent") && LvN_GAdd(MT, "counter", -1, NULL);
	Native_Free(&l);
	if (!ok) {
		return 0;
	}
	Native_SetFloat(&time, Mode_Time());
	Native_SetInt(&zero, 0);
	ok = Mode_G(MT, "counter", &ctr);
	int last = ok && Native_Compare(&ctr, &zero) <= 0;
	Native_Free(&ctr);
	if (!ok) {
		return 0;
	}
	if (last) {
		return Mt_Prize(&time);
	}
	return Mt_Sound("CoinTake") && Mt_Ticks(&time);
}

/* BlinkBigPrize(l): the prize blinks before it goes */
static byte MapTreasure_BlinkBigPrize(const nativeVal_t *l) {
	return LvN_SetI(l, "Blink", 1);
}

/* BigPrize(ItemName, UsrName): the prize taken: the money, the next circuit of the chain (after the last one the
 * dollar goes), the chain ends */
static byte MapTreasure_BigPrize(const nativeVal_t *item) {
	nativeVal_t idx = { 0 }, rep = { 0 }, varname = { 0 }, sv = { 0 }, ac = { 0 }, one = { 0 }, circuit = { 0 };
	nativeVal_t row = { 0 }, deal = { 0 }, d = { 0 }, none = { 0 }, money = { 0 }, k = { 0 }, pm = { 0 }, l = { 0 };
	nativeVal_t take = { 0 }, a = { 0 }, a0 = { 0 }, a1 = { 0 }, pos = { 0 };
	const nativeVal_t *vals[] = { &idx, &rep, &varname, &sv, &ac, &circuit, &row, &deal, &d, &money, &k, &pm, &l, &take,
		&a, &a0, &a1, &pos };
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_SetNone((nativeVal_t *)vals[i]);
	}
	Native_SetNone(&none);
	Native_SetInt(&one, 1);
	if (!Mode_Import("Sound") || !Mode_G(MT, "idx", &idx)) {
		return 0;
	}
	Mode_Repr(&idx, &rep);
	byte ok = Ba_Cat("Bank.Circuit.", &rep, &varname);
	if (ok) {
		Mode_SaveVar(Native_Text(&varname), &sv);
		Native_Free(&rep);
		ok = Native_ToInt(&sv, &rep) && Mode_Arith('+', &rep, &one, &ac);
	}
	/* n = len(Circuit[idx]) */
	Native_Free(&idx);
	ok = ok && Mode_G(MT, "Circuit", &circuit) && Mode_G(MT, "idx", &idx) && Native_Subscript(&circuit, &idx, &row);
	int n = ok ? Native_Size(&row) : -1;
	ok = n >= 0;
	if (ok) {
		nativeVal_t nn = { 0 };
		Native_SetInt(&nn, n);
		if (Native_Compare(&ac, &nn) >= 0) {
			Native_Free(&ac);
			Native_SetInt(&ac, 0);
			Native_Free(&idx);
			ok = LvN_GElems(MT, "DealCoins", &deal) && Mode_G(MT, "idx", &idx) && Native_Subscript(&deal, &idx, &d) &&
				Mode_VDel(&d);
			Native_Free(&idx);
			ok = ok && Mode_G(MT, "idx", &idx) && Mode_DictPut(MT, "DealCoins", NULL, &idx, &none);
		}
	}
	if (ok) {
		Native_Free(&rep);
		Mode_Repr(&ac, &rep);
		ok = Mode_SetSaveVar(Native_Text(&varname), &rep);
	}
	/* Coins[idx] = Circuit[idx][ac] */
	Native_Free(&circuit);
	Native_Free(&idx);
	Native_Free(&row);
	Native_Free(&d);
	ok = ok && Mode_G(MT, "Circuit", &circuit) && Mode_G(MT, "idx", &idx) && Native_Subscript(&circuit, &idx, &row) &&
		Native_Subscript(&row, &ac, &d);
	Native_Free(&idx);
	ok = ok && Mode_G(MT, "idx", &idx) && Mode_DictPut(MT, "Coins", NULL, &idx, &d);
	ok = ok && Mode_G(MT, "PrizeMoney", &pm) && Mt_Chain(1, &k) && Native_Subscript(&pm, &k, &money) && Mt_AddMoney(&money);
	if (ok) {
		SaveVar_Inc("Stats.Treasure", 1);
		ok = Mode_EntityH(item, &l) && LvN_SetI(&l, "rTime", 86400);
	}
	if (ok) {
		/* Scrap.AddScheduledFunc(Scrap.GetTime() + 1.0, l.Del, ()) */
		nativeVal_t at = { 0 };
		PyObject *del = Mode_VBound(&l, "Del");
		Native_SetFloat(&at, Mode_Time() + 1.0);
		ok = del && Mode_Schedule(&at, del, NULL, 0, NULL);
		Py_XDECREF(del);
	}
	ok = ok && LvN_ModGet("Sound", "ItemTake", &take);
	if (ok) {
		nativeVal_t key = { 0 };
		LvN_Str(&key, "MoneyItem");
		ok = Native_Subscript(&take, &key, &a) && Mode_VGet(&l, "Pos", &pos) && LvN_Item(&a, 0, &a0) && LvN_Item(&a, 1, &a1) &&
			Mode_PlaySound(&pos, &a0, &a1) && Mode_DelSchedS("DeleteShent") && LV_OWN(MT, "DeleteShent", NULL, "");
		Native_Free(&key);
	}
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_Free((nativeVal_t *)vals[i]);
	}
	return ok;
}

/* `for l in <global>: l.rTime = 86400` (del: l.Del()) over the handles of a list of the module */
static byte Mt_EachCoin(const char *list, byte del) {
	nativeVal_t seq = { 0 };
	byte ok = LvN_GElems(MT, list, &seq);
	int count = ok ? Native_ForCount(&seq) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t l = { 0 };
		ok = Native_SeqItem(&seq, i, &l) && (del ? Mode_VDel(&l) : LvN_SetI(&l, "rTime", 86400));
		Native_Free(&l);
	}
	Native_Free(&seq);
	return ok;
}

/* DeleteShent(): the coins of the chain hidden, RestartShents in 2.5 s */
static byte MapTreasure_DeleteShent(void) {
	return Mt_EachCoin("shent", 0) && Mode_Later(2.5, MT, "RestartShents", NULL, 0, NULL);
}

/* RestartShents(): the coins of the chain deleted, its state forgotten, the dollars back one after the other */
static byte MapTreasure_RestartShents(void) {
	nativeVal_t ctr = { 0 }, step = { 0 }, deal = { 0 };
	if (!Mt_EachCoin("shent", 1) || !Mode_DelGlobal(MT, "shent") || !Mode_DelGlobal(MT, "counter") || !Mode_DelGlobal(MT, "idx")) {
		return 0;
	}
	Native_SetFloat(&ctr, 0.1);
	Native_SetFloat(&step, 0.1);
	byte ok = LvN_GElems(MT, "DealCoins", &deal);
	int count = ok ? Native_ForCount(&deal) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t l = { 0 }, coins = { 0 }, c = { 0 }, pos = { 0 }, next = { 0 };
		Native_SetNone(&coins);
		Native_SetNone(&c);
		Native_SetNone(&pos);
		Native_SetNone(&next);
		ok = Native_SeqItem(&deal, i, &l);
		if (ok && Native_True(&l)) {
			ok = Mode_G(MT, "Coins", &coins) && LvN_Item(&coins, i, &c) && LvN_Item(&c, 2, &pos) && Mode_VSet(&l, "Pos", &pos) &&
				Mode_VSet(&l, "rTime", &ctr);
		}
		ok = ok && Mode_Arith('+', &ctr, &step, &next);
		if (ok) {
			Native_Free(&ctr);
			ctr = next;
		}
		Native_Free(&l);
		Native_Free(&coins);
		Native_Free(&c);
		Native_Free(&pos);
	}
	Native_Free(&ctr);
	Native_Free(&deal);
	return ok;
}

/* InitStatus(LevelPath): the coin models preloaded */
static byte MapTreasure_InitStatus(void) {
	nativeVal_t m = { 0 }, models = { 0 };
	Native_SetNone(&models);
	byte ok = Mode_G(MT, "InitModel", &m) && Mode_Preload(&m);
	Native_Free(&m);
	ok = ok && Mode_G(MT, "CopperModel", &m) && Mode_Preload(&m);
	Native_Free(&m);
	ok = ok && Mode_G(MT, "PrizeModels", &models);
	int count = ok ? Native_ForCount(&models) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		ok = Native_SeqItem(&models, i, &m) && Mode_Preload(&m);
		Native_Free(&m);
	}
	Native_Free(&models);
	return ok;
}

/* MakeCoin(name, MoneyModel, pos): an item only the player takes */
static byte MapTreasure_MakeCoin(const nativeVal_t *name, const nativeVal_t *model, const nativeVal_t *pos, nativeVal_t *out) {
	return LvN_CreateAt(name, pos, "Item", out) && Mode_VModel(out, model) && LvN_SetI(out, "OnlyUser", 1) &&
		LvN_SetI(out, "Radius", 40);
}

/* CreateCoins(): the dollar of each chain */
static byte MapTreasure_CreateCoins(void) {
	nativeVal_t coins = { 0 };
	byte ok = Mode_G(MT, "Coins", &coins);
	int count = ok ? Native_ForCount(&coins) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t cn = { 0 }, idx = { 0 }, rep = { 0 }, name = { 0 }, model = { 0 }, pos = { 0 }, l = { 0 };
		Native_SetNone(&name);
		Native_SetNone(&model);
		Native_SetNone(&pos);
		Native_SetNone(&l);
		ok = Native_SeqItem(&coins, i, &cn);
		if (ok) {
			Native_SetInt(&idx, i);
			Mode_Repr(&idx, &rep);
			ok = Ba_Cat("Treasure", &rep, &name) && Mode_G(MT, "InitModel", &model) && LvN_Item(&cn, 2, &pos) &&
				LV_OWN(MT, "MakeCoin", &l, "vvv", &name, &model, &pos) && LvN_SetS(&l, "OnTake", "MapTreasure.InitDeal") &&
				Mode_Append(MT, "DealCoins", &l);
			Native_Free(&rep);
			Native_Free(&cn);
		}
		Native_Free(&name);
		Native_Free(&model);
		Native_Free(&pos);
		Native_Free(&l);
	}
	Native_Free(&coins);
	return ok;
}


/* one round of the loop of the module code (f stored before): varname = "Bank.Circuit."+`f`;
 * Scrap.CreateSaveVar(varname, "0"); Coins.append(Circuit[f][int(Scrap.GetSaveVar(varname))]) */
static byte Mt_CoinsBody(PyObject *locals, PyObject *f, void *ctx) {
	(void)ctx;
	PyObject *key = PyString_InternFromString("varname");
	PyObject *repr = key ? Script_Repr(f) : NULL;
	PyObject *name = repr ? Script_SPlus("Bank.Circuit.", repr) : NULL;
	byte ok = name && PyDict_SetItem(locals, key, name) == 0;
	Py_XDECREF(key);
	Py_XDECREF(repr);
	Py_XDECREF(name);
	PyObject *create = ok ? Script_ModuleName(locals, "Scrap.CreateSaveVar") : NULL;
	PyObject *var = create ? Script_ModuleName(locals, "varname") : NULL;
	PyObject *res = var ? Script_Invoke(create, Py_BuildValue("(ON)", var, Script_StrConst("0", 1))) : NULL;
	Py_XDECREF(create);
	Py_XDECREF(var);
	ok = res != NULL;
	Py_XDECREF(res);
	PyObject *append = ok ? Script_ModuleName(locals, "Coins.append") : NULL;
	PyObject *circuit = append ? Script_ModuleName(locals, "Circuit") : NULL;
	PyObject *row = circuit ? PyObject_GetItem(circuit, f) : NULL;
	PyObject *toInt = row ? Script_ModuleName(locals, "int") : NULL;
	PyObject *get = toInt ? Script_ModuleName(locals, "Scrap.GetSaveVar") : NULL;
	var = get ? Script_ModuleName(locals, "varname") : NULL;
	PyObject *saved = var ? Script_Invoke(get, Py_BuildValue("(O)", var)) : NULL;
	PyObject *index = saved ? Script_Invoke(toInt, Py_BuildValue("(O)", saved)) : NULL;
	PyObject *coin = index ? PyObject_GetItem(row, index) : NULL;
	res = coin ? Script_Invoke(append, Py_BuildValue("(O)", coin)) : NULL;
	Py_XDECREF(append);
	Py_XDECREF(circuit);
	Py_XDECREF(row);
	Py_XDECREF(toInt);
	Py_XDECREF(get);
	Py_XDECREF(var);
	Py_XDECREF(saved);
	Py_XDECREF(index);
	Py_XDECREF(coin);
	Py_XDECREF(res);
	return res != NULL;
}
/* maptreasure.py:2836, `for f in range(len(Circuit)): ...`: the chain of each circuit as the save variable keeps it */
int MapTreasure_ModCoins(PyObject *module, PyObject *locals) {
	(void)module;
	PyObject *range = Script_ModuleName(locals, "range");
	PyObject *len = range ? Script_ModuleName(locals, "len") : NULL;
	PyObject *circuit = len ? Script_ModuleName(locals, "Circuit") : NULL;
	PyObject *n = circuit ? Script_Invoke(len, Py_BuildValue("(O)", circuit)) : NULL;
	PyObject *seq = n ? Script_Invoke(range, Py_BuildValue("(O)", n)) : NULL;
	byte ok = seq && Script_ModuleFor(locals, "f", seq, Mt_CoinsBody, NULL);
	Py_XDECREF(range);
	Py_XDECREF(len);
	Py_XDECREF(circuit);
	Py_XDECREF(n);
	Py_XDECREF(seq);
	return ok;
}

BA_CORE1(MapTreasure_InitDeal)
BA_CORE1(MapTreasure_TakeMiniDeal)
BA_CORE1(MapTreasure_BlinkBigPrize)
BA_CORE1(MapTreasure_BigPrize)
LV_CORE0(MapTreasure_DeleteShent)
LV_CORE0(MapTreasure_RestartShents)
LV_CORE0(MapTreasure_InitStatus)
LV_CORE0(MapTreasure_CreateCoins)

static byte MapTreasure_MakeCoin_C(const nativeVal_t *a, nativeVal_t *out) {
	return MapTreasure_MakeCoin(&a[0], &a[1], &a[2], out);
}

LV_FN(MapTreasure, InitDealPy, 2, MapTreasure_InitDeal_C)
LV_FN(MapTreasure, TakeMiniDealPy, 2, MapTreasure_TakeMiniDeal_C)
LV_FN(MapTreasure, BlinkBigPrizePy, 1, MapTreasure_BlinkBigPrize_C)
LV_FN(MapTreasure, BigPrizePy, 2, MapTreasure_BigPrize_C)
LV_FN(MapTreasure, DeleteShentPy, 0, MapTreasure_DeleteShent_C)
LV_FN(MapTreasure, RestartShentsPy, 0, MapTreasure_RestartShents_C)
LV_FN(MapTreasure, InitStatusPy, 1, MapTreasure_InitStatus_C)
LV_FN(MapTreasure, MakeCoinPy, 3, MapTreasure_MakeCoin_C)
LV_FN(MapTreasure, CreateCoinsPy, 0, MapTreasure_CreateCoins_C)

PyMethodDef g_BankMapTreasureMethods[] = {
	{ "InitDeal", MapTreasure_InitDealPy, METH_VARARGS, NULL },
	{ "TakeMiniDeal", MapTreasure_TakeMiniDealPy, METH_VARARGS, NULL },
	{ "BlinkBigPrize", MapTreasure_BlinkBigPrizePy, METH_VARARGS, NULL },
	{ "BigPrize", MapTreasure_BigPrizePy, METH_VARARGS, NULL },
	{ "DeleteShent", MapTreasure_DeleteShentPy, METH_VARARGS, NULL },
	{ "RestartShents", MapTreasure_RestartShentsPy, METH_VARARGS, NULL },
	{ "InitStatus", MapTreasure_InitStatusPy, METH_VARARGS, NULL },
	{ "MakeCoin", MapTreasure_MakeCoinPy, METH_VARARGS, NULL },
	{ "CreateCoins", MapTreasure_CreateCoinsPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_CatchRudolph ---- */

/* InitStatus(LevelPath): Rudolph, the functionary to catch */
static byte CatchRudolph_InitStatus(void) {
	return LvN_MissionChar("Functionary_Rudolph", CR, "CreateRudolph");
}

/* CreateRudolph(r): Rudolph talks while he walks and runs away from the player */
static byte CatchRudolph_CreateRudolph(const nativeVal_t *r) {
	nativeVal_t d = { 0 };
	if (!LvN_SetS(r, "UseFunc", "Mission_CatchRudolph.OnCatchRudolph") || !LvN_SetF(r, "LLogicProbTalkWhenWalking", 1.0) ||
		!LvN_SetI(r, "LLogicEscapeWithinRadius", 1) || !LvN_SetI(r, "LLogicEscapeRadiusSqr", 562500)) {
		return 0;
	}
	Mode_Lang("CharName_Rudolph", &d);
	byte ok = Mode_VSet(r, "Descriptor", &d) && Mode_Put(CR, "rudolph", NULL, r);
	Native_Free(&d);
	return ok;
}

/* OnCatchRudolph(UsrName, CharName, optId, FullOption): Rudolph caught; his talk ends in SetNextMission */
static byte CatchRudolph_OnCatchRudolph(const nativeVal_t *a) {
	nativeVal_t e = { 0 };
	Native_SetNone(&e);
	byte ok = Mode_EntityH(&a[1], &e) && LvN_SetI(&e, "LLogicEscapeWithinRadius", 0) && LvN_Speakers(&a[0], &a[1]);
	Native_Free(&e);
	if (ok && LvN_EqS(&a[2], "Talk")) {
		ok = LvN_OnEnd(CR, "SetNextMission", NULL) && Mode_NISpeech("Functionary", "Mission_GetRudolph_Rudolph_NI_1Msg");
	}
	return ok;
}

/* SetNextMission(): Mission_KillRudolph in Outskirts, a character that can drive, the level changes */
static byte CatchRudolph_SetNextMission(void) {
	nativeVal_t ch = { 0 }, list = { 0 };
	if (!PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", "") || !LV_CALL("SaveGame", "QWayPoint", NULL, "") ||
		!LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_KillRudolph", "Outskirts") ||
		!LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_KillRudolph_MissionText1", 1L) || !Mode_Import("Chars")) {
		return 0;
	}
	Mode_SaveVar("Char", &ch);
	int in = LvN_ModGet("Chars", "CanDriveList", &list) ? Native_Contains(&list, &ch) : -1;
	Native_Free(&ch);
	Native_Free(&list);
	if (in < 0) {
		return 0;
	}
	if (in == 0) {
		SaveVar_Set("Char", "Dtritus");
	}
	SaveVar_Set("PrevMap", "Levels/Bank");
	SaveVar_Set("ComeFrom", "DoorElevator");
	SaveVar_Set("Spawn", "DM_Player_Spawn_Bank");
	return LV_CALL("SaveGame", "QLoad", NULL, "s", "Levels/Outskirts");
}

LV_CORE0(CatchRudolph_InitStatus)
BA_CORE1(CatchRudolph_CreateRudolph)
LV_CORE0(CatchRudolph_SetNextMission)

static byte CatchRudolph_OnCatchRudolph_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return CatchRudolph_OnCatchRudolph(a);
}

LV_FN(CatchRudolph, InitStatusPy, 1, CatchRudolph_InitStatus_C)
LV_FN(CatchRudolph, CreateRudolphPy, 1, CatchRudolph_CreateRudolph_C)
LV_FN(CatchRudolph, OnCatchRudolphPy, 4, CatchRudolph_OnCatchRudolph_C)
LV_FN(CatchRudolph, SetNextMissionPy, 0, CatchRudolph_SetNextMission_C)

PyMethodDef g_BankCatchRudolphMethods[] = {
	{ "InitStatus", CatchRudolph_InitStatusPy, METH_VARARGS, NULL },
	{ "CreateRudolph", CatchRudolph_CreateRudolphPy, METH_VARARGS, NULL },
	{ "OnCatchRudolph", CatchRudolph_OnCatchRudolphPy, METH_VARARGS, NULL },
	{ "SetNextMission", CatchRudolph_SetNextMissionPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetDataPart2 ---- */

/* speedFactor * (maxDataDownloaded - maxDataDownloaded * dataDownloaded * (1.0 / float(maxDataDownloaded))), the
 * globals read in the order of the script */
static byte Gd_StopValue(nativeVal_t *out) {
	nativeVal_t sf = { 0 }, max = { 0 }, dd = { 0 }, mf = { 0 }, one = { 0 }, inv = { 0 }, p = { 0 }, q = { 0 };
	nativeVal_t r = { 0 };
	const nativeVal_t *vals[] = { &sf, &max, &dd, &mf, &inv, &p, &q, &r };
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_SetNone((nativeVal_t *)vals[i]);
	}
	Native_SetFloat(&one, 1.0);
	byte ok = Mode_G(GD, "speedFactor", &sf) && Mode_G(GD, "maxDataDownloaded", &max) && Mode_G(GD, "dataDownloaded", &dd) &&
		Mode_Arith('*', &max, &dd, &p);
	Native_Free(&max);
	ok = ok && Mode_G(GD, "maxDataDownloaded", &max) && Native_ToFloat(&max, &mf) && Mode_Arith('/', &one, &mf, &inv) &&
		Mode_Arith('*', &p, &inv, &q);
	Native_Free(&max);
	Native_Free(&p);
	ok = ok && Mode_G(GD, "maxDataDownloaded", &max) && Mode_Arith('-', &max, &q, &r) && Mode_Arith('*', &sf, &r, out);
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_Free((nativeVal_t *)vals[i]);
	}
	return ok;
}

/* ComputeAutoUpdateStopValue(): where the data bar stops */
static byte GetDataPart2_ComputeAutoUpdateStopValue_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	return Gd_StopValue(out);
}

/* SScorer.Set(id, item, var, v) with id and item C values */
static byte Gd_Set(long id, const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t i = { 0 };
	Native_SetInt(&i, id);
	return LvN_ScorerSet(&i, item, var, v);
}

static byte Gd_SetI(long id, const char *item, const char *var, long n) {
	nativeVal_t v = { 0 };
	Native_SetInt(&v, n);
	return Gd_Set(id, item, var, &v);
}

/* SScorer.Set(0, "DataStatus", "IsAutoUpdatable", n); SScorer.Set(0, "DataStatus", "AutoUpdateStopValue",
 * ComputeAutoUpdateStopValue()) */
static byte Gd_Bar(long autoUpdate) {
	nativeVal_t v = { 0 };
	Native_SetNone(&v);
	byte ok = Gd_SetI(0, "DataStatus", "IsAutoUpdatable", autoUpdate) && LV_OWN(GD, "ComputeAutoUpdateStopValue", &v, "") &&
		Gd_Set(0, "DataStatus", "AutoUpdateStopValue", &v);
	Native_Free(&v);
	return ok;
}

/* InitStatus(LevelPath): no data packs of the messengers, the engine callbacks of the functionaries, the three
 * functionaries of the mission */
static byte GetDataPart2_InitStatus(void) {
	nativeVal_t v = { 0 };
	LvN_Int(&v, 0);
	if (!Mode_Import("Messenger") || !Mode_SetModVar("Messenger", "MakeMDP", &v) || !Mode_GVarSetI("MessengerDataPackMissionArrow", 0) ||
		!Mode_GVarSetS("OnFunctionaryStartsWorking", "Mission_GetDataPart2.OnFuncStartsWorking") ||
		!Mode_GVarSetS("OnFinishAutoUpdate", "Mission_GetDataPart2.OnDownloadComplete") ||
		!LvN_MissionChar("Functionary_00", GD, "CreateFuncInit") || !LvN_MissionChar("Functionary_01", GD, "CreateFuncInit") ||
		!LvN_MissionChar("Functionary_02", GD, "CreateFuncInit")) {
		return 0;
	}
	LvN_Str(&v, "Functionary");
	byte ok = LvN_ModAppend("CharsNPC", "RespawnChars", &v);
	Native_Free(&v);
	return ok;
}

/* Init(LevelPath): first the talk with the boss, then the downloads */
static byte GetDataPart2_Init(void) {
	return LV_OWN(GD, LvN_SaveIs("MissionState", "TalkToBoss") ? "TalkToBoss" : "InitMission", NULL, "");
}

/* TalkToBoss(): Dtritus' desktop is the target, the boss talks through it */
static byte GetDataPart2_TalkToBoss(void) {
	nativeVal_t dd = { 0 };
	Native_SetNone(&dd);
	byte ok = Mode_Import("DTritusDesktop") && LvN_Entity("DM_DTritusDesktop", &dd) && Mode_Put(GD, "dd", NULL, &dd);
	Native_Free(&dd);
	ok = ok && Mode_G(GD, "dd", &dd) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &dd, 1L) &&
		LvN_ModSetGlobal("DTritusDesktop", "DialogController", GD, "BossSpeech");
	Native_Free(&dd);
	return ok;
}

/* BossSpeech(UsrName, CharName, optId, FullOption): the boss explains the mission, PreInitMission at the end */
static byte GetDataPart2_BossSpeech(const nativeVal_t *a) {
	nativeVal_t dd = { 0 };
	byte ok = Mode_G(GD, "dd", &dd) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &dd, 0L) &&
		LvN_Speakers(&a[0], &a[1]) && LvN_OnEnd(GD, "PreInitMission", NULL) && Mode_NISpeech("Boss", "Mission_TalkToBoss2__ComBoss_NI_1Msg");
	Native_Free(&dd);
	return ok;
}

/* PreInitMission(): saved, the mission text, the desktop talks no more, the downloads start */
static byte GetDataPart2_PreInitMission(void) {
	if (!Mode_Import("DTritusDesktop") || !LV_CALL("SaveGame", "QWayPoint", NULL, "") ||
		!LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_GetDataPart2_MissionText1", 1L)) {
		return 0;
	}
	SaveVar_Set("MissionState", "Funcs");
	return LvN_ModNone("DTritusDesktop", "DialogController") && LV_OWN(GD, "InitMission", NULL, "");
}

/* InitMission(): the data bar, the three desks, the callback of a functionary back at his desk */
static byte GetDataPart2_InitMission(void) {
	return LvN_ScorerCallG(GD, "MissionScorer") && LvN_Draw() &&
		LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_00", 1L) &&
		LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_01", 1L) &&
		LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_02", 1L) &&
		Mode_GVarSetS("OnDefStateCallback", "Mission_GetDataPart2.OnFuncDefState");
}

/* SetDesktopState(desktopName, active): the desk offers the download, or its table goes up */
static byte GetDataPart2_SetDesktopState(const nativeVal_t *name, const nativeVal_t *active) {
	nativeVal_t e = { 0 }, v = { 0 }, t = { 0 };
	Native_SetNone(&v);
	Native_SetNone(&t);
	if (!Mode_EntityH(name, &e)) {
		return 0;
	}
	byte ok;
	if (e.type == NV_NONE) {
		LvN_Str(&v, " Not found!");
		ok = Mode_Arith('+', name, &v, &t) && Mode_ConsoleError(&t);
	} else if (Native_True(active)) {
		ok = LvN_SetI(&e, "VRadius", 60) && LvN_SetI(&e, "UseMapCheck", 0) && Native_GVarGet("ArrowMissionFile", &v) &&
			Mode_VSet(&e, "ArrowModMission", &v) && LvN_SetI(&e, "ArrowCanMission", 1) && LvN_SetI(&e, "ArrowCanUse", 1) &&
			LvN_SetI(&e, "AlwaysUse", 1) && LvN_SetI(&e, "RadarShow", 1) && LvN_SetI(&e, "Life", 0) && LvN_SetI(&e, "ArrnOpt", 0);
		if (ok) {
			Native_Free(&v);
			Mode_Lang("USE_Functionaries_DownloadData", &v);
			ok = Ba_Cat("DownloadData:", &v, &t) && Mode_VSet(&e, "ArrOpt0", &t) &&
				LvN_SetS(&e, "UseFunc", "Mission_GetDataPart2.OnBeginDownloadData");
		}
	} else {
		ok = LvN_SetS(&e, "Action", "LoopUp") && LvN_SetS(&e, "DefLoop", "LoopUp") && LvN_SetI(&e, "ArrowCanMission", 0) &&
			LvN_SetI(&e, "ArrowCanUse", 0) && LvN_SetI(&e, "AlwaysUse", 0) && LvN_SetI(&e, "RadarShow", 0) && LvN_SetS(&e, "UseFunc", "");
	}
	Native_Free(&e);
	Native_Free(&v);
	Native_Free(&t);
	return ok;
}

/* CreateFuncInit(e): a functionary of the mission: his desk, his talk; he stops a download that runs at his desk */
static byte GetDataPart2_CreateFuncInit(const nativeVal_t *e) {
	nativeVal_t name = { 0 }, desk = { 0 }, act = { 0 };
	Native_SetNone(&name);
	Native_SetNone(&desk);
	Native_SetNone(&act);
	byte ok = Mode_VGet(e, "Name", &name) && Ba_Cat("DM_Desktop", &name, &desk) &&
		LvN_SetS(e, "UseFunc", "Mission_GetDataPart2.OnFunctionarySpeech") && LvN_SetF(e, "LLogicProbTalkWhenWorking", 1.0) &&
		LvN_SetF(e, "LLogicProbTalkWhenPlaying", 1.0) && LvN_SetF(e, "LLogicProbTalkWhenWalking", 1.0) &&
		LvN_SetF(e, "LLogicGoToWorkSpeed", 1.0) && Mode_VSet(e, "LLogicForcedDesktopAssigned", &desk) &&
		Mode_G(GD, "activatedDesktops", &act);
	int in = ok ? Native_Contains(&act, &desk) : -1;
	Native_Free(&name);
	ok = in >= 0 && Mode_VGet(e, "Name", &name) && LV_OWN(GD, in ? "AbortDownload" : "SetNormalState", NULL, "v", &name);
	Native_Free(&name);
	Native_Free(&desk);
	Native_Free(&act);
	return ok;
}

/* OnFunctionarySpeech(UsrName, CharName, optId, FullOption): what a functionary says, working or not */
static byte GetDataPart2_OnFunctionarySpeech(const nativeVal_t *a) {
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	nativeVal_t e = { 0 }, w = { 0 };
	Native_SetNone(&w);
	int c = Ba_EntTrue(&a[1], &e);
	byte ok = c >= 0;
	if (c > 0) {
		ok = Mode_VGet(&e, "LLogicKeepWorking", &w);
		if (ok) {
			ok = LvN_EqI(&w, 0) ?
				LV_CALL("Speech", "OnSpch", NULL, "ss", "Mission_GetDataPart2_Functionary_InitMsg1", "Mission_GetDataPart2_Functionary_InitAns1") :
				LV_CALL("Speech", "OnSpch", NULL, "ss", "Mission_GetDataPart2_Functionary_InitMsg2", "Mission_GetDataPart2_Functionary_InitAns2");
		}
	}
	Native_Free(&e);
	Native_Free(&w);
	return ok;
}

/* OnFuncDefState(funcName): a functionary back in his default state stops a download, or works normally */
static byte GetDataPart2_OnFuncDefState(const nativeVal_t *name) {
	nativeVal_t e = { 0 }, v = { 0 };
	Native_SetNone(&v);
	int c = Ba_EntTrue(name, &e);
	if (c > 0) {
		c = Mode_VGet(&e, "ActCtrl", &v) ? LvN_EqS(&v, "Functionary") : -1;
	}
	byte ok = c >= 0;
	if (c > 0) {
		nativeVal_t zero = { 0 };
		Native_Free(&v);
		Native_SetInt(&zero, 0);
		ok = Mode_G(GD, "dataDownloaded", &v);
		if (ok && Native_Compare(&v, &zero) > 0) {
			ok = LV_OWN(GD, "AbortDownload", NULL, "v", name);
		} else if (ok) {
			Native_Free(&v);
			ok = Mode_VGet(&e, "Name", &v) && LV_OWN(GD, "SetNormalState", NULL, "v", &v);
		}
	}
	Native_Free(&e);
	Native_Free(&v);
	return ok;
}

/* SetStopDownloadingState(funcName): the functionary goes to sit at his desk, whatever the alarm */
static byte GetDataPart2_SetStopDownloadingState(const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	int c = Ba_EntTrue(name, &e);
	byte ok = c >= 0;
	if (c > 0) {
		ok = LvN_SetF(&e, "LLogicTimeToSitDown", 0.01) && LvN_SetI(&e, "LLogicKeepWorking", 1) &&
			LvN_SetI(&e, "LLogicForceSpecialBehaviour", 1) && LvN_SetI(&e, "LLogicIgnoreAlarm", 1);
	}
	Native_Free(&e);
	return ok;
}

/* SetNormalState(funcName): the functionary stops working and behaves normally */
static byte GetDataPart2_SetNormalState(const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	int c = Ba_EntTrue(name, &e);
	byte ok = c >= 0;
	if (c > 0) {
		ok = LvN_SetF(&e, "LLogicTimeToStopWorking", 0.01) && LvN_SetI(&e, "LLogicKeepWorking", 0) &&
			LvN_SetI(&e, "LLogicForceCancelSpecialBehaviour", 1) && LvN_SetF(&e, "LLogicTimeToSitDown", -0.01) &&
			LvN_SetI(&e, "LLogicIgnoreAlarm", 0);
	}
	Native_Free(&e);
	return ok;
}

/* print prefix + name */
static byte Gd_Print(const char *prefix, const nativeVal_t *name) {
	nativeVal_t t = { 0 };
	byte ok = Ba_Cat(prefix, name, &t);
	if (ok) {
		ok = Mode_PrintV(&t);
	}
	Native_Free(&t);
	return ok;
}

/* `if e.<var>:` of a handle value: 1, 0, -1 */
static int Gd_Is(const nativeVal_t *e, const char *var) {
	nativeVal_t v = { 0 };
	Native_SetNone(&v);
	int r = Mode_VGet(e, var, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* AbortDownload(funcName): a functionary with local AI goes to stop the download */
static byte GetDataPart2_AbortDownload(const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	if (!Mode_EntityH(name, &e)) {
		return 0;
	}
	byte ok;
	int c;
	if (e.type == NV_NONE) {
		ok = Gd_Print("No entity ", name);
	} else if ((c = Gd_Is(&e, "isAI")) <= 0) {
		ok = c == 0 && Gd_Print("No AI at ", name);
	} else if ((c = Gd_Is(&e, "isLocalLogic")) <= 0) {
		ok = c == 0 && Gd_Print("No Local logic at ", name);
	} else {
		c = Gd_Is(&e, "LLogicIgnoreAlarm");
		ok = c >= 0 && (c > 0 || LV_OWN(GD, "SetStopDownloadingState", NULL, "v", name));
	}
	Native_Free(&e);
	return ok;
}

/* dataDownloaded = len(activatedDesktops) */
static byte Gd_Count(void) {
	nativeVal_t act = { 0 };
	byte ok = Mode_G(GD, "activatedDesktops", &act);
	int n = ok ? Native_Size(&act) : -1;
	Native_Free(&act);
	return n >= 0 && LvN_PutI(GD, "dataDownloaded", n);
}

/* OnDownloadAborted(funcName): the functionary reached his desk: the desk offers the download again, the bar goes
 * back; without downloads all functionaries work normally */
static byte GetDataPart2_OnDownloadAborted(const nativeVal_t *name) {
	nativeVal_t call = { 0 }, e = { 0 }, desk = { 0 }, act = { 0 }, v = { 0 }, zero = { 0 }, names = { 0 };
	Native_SetNone(&desk);
	Native_SetNone(&act);
	Native_SetNone(&v);
	Native_SetNone(&names);
	Native_SetInt(&zero, 0);
	if (!LvN_ModGet("Menu", "MissionScorerCall", &call)) {
		return 0;
	}
	byte none = call.type == NV_NONE;
	Native_Free(&call);
	if (none) {
		return 1;
	}
	int c = Ba_EntTrue(name, &e);
	if (c <= 0) {
		Native_Free(&e);
		return c == 0;
	}
	byte ok = Ba_Cat("DM_Desktop", name, &desk) && Mode_G(GD, "activatedDesktops", &act);
	int in = ok ? Native_Contains(&act, &desk) : -1;
	ok = in >= 0 && (in == 0 || Mode_Remove(GD, "activatedDesktops", &desk)) &&
		LV_OWN(GD, "SetDesktopState", NULL, "vi", &desk, 1L) && Mode_VGet(&e, "Name", &v) &&
		LV_OWN(GD, "SetNormalState", NULL, "v", &v) && Gd_Count();
	Native_Free(&v);
	ok = ok && Mode_G(GD, "dataDownloaded", &v) && (Native_Compare(&v, &zero) < 0 || Gd_Bar(2));
	Native_Free(&v);
	ok = ok && Mode_G(GD, "dataDownloaded", &v);
	if (ok && Native_Compare(&v, &zero) <= 0) {
		ok = LvN_PutI(GD, "dataDownloaded", 0) && Mode_G(GD, "funcNames", &names);
		int count = ok ? Native_ForCount(&names) : -1;
		ok = count >= 0;
		for (int i = 0; i < count && ok; i++) {
			nativeVal_t fn = { 0 }, other = { 0 }, n = { 0 };
			Native_SetNone(&other);
			Native_SetNone(&n);
			ok = Native_SeqItem(&names, i, &fn);
			if (ok && Native_Compare(&fn, name) != 0) {
				c = Ba_EntTrue(&fn, &other);
				ok = c >= 0 && (c == 0 || (Mode_VGet(&other, "Name", &n) && LV_OWN(GD, "SetNormalState", NULL, "v", &n)));
			}
			Native_Free(&fn);
			Native_Free(&other);
			Native_Free(&n);
		}
	}
	Native_Free(&e);
	Native_Free(&desk);
	Native_Free(&act);
	Native_Free(&v);
	Native_Free(&names);
	return ok;
}

/* OnBeginDownloadData(UsrName, desktopName, optId, FullOption): a download starts at a desk; its functionary goes to
 * stop it */
static byte GetDataPart2_OnBeginDownloadData(const nativeVal_t *a) {
	const nativeVal_t *desk = &a[1];
	nativeVal_t v = { 0 }, s = { 0 }, e = { 0 }, n = { 0 };
	Native_SetNone(&s);
	Native_SetNone(&e);
	Native_SetNone(&n);
	byte ok = Mode_Append(GD, "activatedDesktops", desk) && LV_OWN(GD, "SetDesktopState", NULL, "vi", desk, 0L) && Gd_Count() &&
		Gd_Bar(1) && Mode_G(GD, "dataDownloaded", &v);
	if (ok && LvN_EqI(&v, 1)) {
		ok = LvN_Com("Mission_GetDataPart2_RemoteMessage1", "Functionary", NULL, NULL, NULL);
	}
	Native_Free(&v);
	int c = ok && Native_Slice(desk, 10, INT_MAX, &s) ? Ba_EntTrue(&s, &e) : -1;
	if (c > 0) {
		c = Mode_VGet(&e, "LLogicKeepWorking", &v) ? LvN_EqI(&v, 0) : -1;
		Native_Free(&v);
	}
	ok = c >= 0 && (c == 0 || (Mode_VGet(&e, "Name", &n) && LV_OWN(GD, "AbortDownload", NULL, "v", &n)));
	Native_Free(&s);
	Native_Free(&e);
	Native_Free(&n);
	return ok;
}

/* OnFuncStartsWorking(funcName): the download at his desk is aborted when he has sat down */
static byte GetDataPart2_OnFuncStartsWorking(const nativeVal_t *name) {
	nativeVal_t r = { 0 }, time = { 0 }, now = { 0 }, x = { 0 }, one = { 0 }, at = { 0 };
	Native_SetNone(&r);
	Native_SetNone(&time);
	Native_SetNone(&x);
	Native_SetNone(&at);
	Native_SetFloat(&one, 1.0);
	byte ok = LvN_GetClass("Functionary", &r);
	Native_Free(&r);
	ok = ok && LvN_GetAction("SitDown", &r) && LvN_GetAct("AnmTotalTime", &time);
	if (ok) {
		Native_SetFloat(&now, Mode_Time());
		ok = Mode_Arith('+', &now, &time, &x) && Mode_Arith('+', &x, &one, &at);
	}
	if (ok) {
		PyObject *fn = Mode_GlobalPy(GD, "OnDownloadAborted");
		ok = fn && Mode_Schedule(&at, fn, name, 1, NULL);
		Py_XDECREF(fn);
	}
	Native_Free(&r);
	Native_Free(&time);
	Native_Free(&x);
	Native_Free(&at);
	return ok;
}

/* OnDownloadComplete(): the bar is empty: the desks close, the functionaries work normally, the boss calls */
static byte GetDataPart2_OnDownloadComplete(void) {
	nativeVal_t v = { 0 }, zf = { 0 }, names = { 0 };
	Mode_ScorerGet(0, "DataStatus", "Value", &v);
	Native_SetFloat(&zf, 0.0);
	byte empty = !Native_Compare(&v, &zf);
	Native_Free(&v);
	if (!empty) {
		return 1;
	}
	Native_SetNone(&names);
	Native_SetEmpty(&v, NV_LIST);
	byte ok = LvN_ScorerOff() && LvN_Draw() && LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_00", 0L) &&
		LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_01", 0L) &&
		LV_OWN(GD, "SetDesktopState", NULL, "si", "DM_DesktopFunctionary_02", 0L) && Mode_DelGlobal(GD, "activatedDesktops") &&
		Mode_Put(GD, "activatedDesktops", NULL, &v) && Mode_G(GD, "funcNames", &names);
	int count = ok ? Native_ForCount(&names) : -1;
	ok = count >= 0;
	for (int i = 0; i < count && ok; i++) {
		nativeVal_t fn = { 0 }, e = { 0 };
		Native_SetNone(&e);
		ok = Native_SeqItem(&names, i, &fn);
		int c = ok ? Ba_EntTrue(&fn, &e) : -1;
		ok = c >= 0 && (c == 0 || LV_OWN(GD, "SetNormalState", NULL, "v", &fn));
		Native_Free(&fn);
		Native_Free(&e);
	}
	ok = ok && LvN_Com("Mission_GetDataPart2_RemoteMessage2", "Boss", GD, "SetNextMission", LvN_NoArgs());
	Native_Free(&names);
	return ok;
}

/* the marker sprites of the data bar */
static byte Gd_Marker(const nativeVal_t *id, const char *item) {
	nativeVal_t it = { 0 };
	LvN_Str(&it, item);
	byte ok = Mode_ScorerAdd(id, &it, "Sprite");
	Native_Free(&it);
	nativeVal_t r = { 0 }, g = { 0 };
	Native_SetInt(&r, 0);
	Native_SetInt(&g, 255);
	return ok && LvN_ScorerSet(id, item, "Red", &r) && LvN_ScorerSet(id, item, "Green", &g) && LvN_ScorerSet(id, item, "Blue", &r);
}

/* a marker at frac of the bar width */
static byte Gd_Place(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *w,
	const nativeVal_t *h, double frac) {
	nativeVal_t f = { 0 }, p = { 0 }, q = { 0 }, one = { 0 }, two = { 0 }, four = { 0 }, vis = { 0 };
	Native_SetFloat(&f, frac);
	Native_SetFloat(&one, 1.0);
	Native_SetInt(&two, 2);
	Native_SetInt(&four, 4);
	Native_SetInt(&vis, 1);
	Native_SetNone(&p);
	Native_SetNone(&q);
	byte ok = Mode_Arith('*', w, &f, &p) && Mode_Arith('+', x, &p, &q) && LvN_ScorerSet(id, item, "X", &q) &&
		LvN_ScorerSet(id, item, "Y", y);
	Native_Free(&p);
	Native_Free(&q);
	ok = ok && Mode_Arith('/', &one, &two, &p) && LvN_ScorerSet(id, item, "ScaleX", &p) && Mode_Arith('/', h, &four, &q) &&
		LvN_ScorerSet(id, item, "ScaleY", &q) && LvN_ScorerSet(id, item, "Visible", &vis);
	Native_Free(&p);
	Native_Free(&q);
	return ok;
}

/* MissionScorer(id): the data bar with its two markers; full the first time */
static byte GetDataPart2_MissionScorer(const nativeVal_t *id) {
	nativeVal_t t = { 0 }, max = { 0 }, sf = { 0 }, v = { 0 }, it = { 0 }, back = { 0 }, main = { 0 }, black = { 0 };
	nativeVal_t one = { 0 }, xywh[4] = { 0 };
	static const char *const vars[4] = { "X", "Y", "W", "H" };
	Native_SetNone(&t);
	Native_SetNone(&max);
	Native_SetNone(&sf);
	Native_SetNone(&v);
	Native_SetNone(&back);
	Native_SetNone(&main);
	Native_SetNone(&black);
	for (int i = 0; i < 4; i++) {
		Native_SetNone(&xywh[i]);
	}
	LvN_Str(&it, "DataStatus");
	LvN_Label("Mission_GetDataPart2_Scorer_1", &t);
	byte ok = LvN_ScorerTextBegin(id, "DataStatusText", 0) && LvN_ScorerSet(id, "DataStatusText", "Text", &t) &&
		LvN_ScorerTextEnd(id, "DataStatusText") && Gd_Marker(id, "DataMarker1") && Gd_Marker(id, "DataMarker2") &&
		Mode_G(GD, "maxDataDownloaded", &max) && Mode_G(GD, "speedFactor", &sf) && Mode_Arith('*', &max, &sf, &v) &&
		Mode_ScorerAdd(id, &it, "Bar") && LvN_ModGet("Scorer", "MissionScorerBar_ColorBack", &back) &&
		LvN_ModGet("Scorer", "MissionScorerBar_ColorMain", &main) && LvN_ModGet("Scorer", "MissionScorerBar_ColorBlack", &black) &&
		LV_CALL("Scorer", "MissionScorerBar_SetDefaults", NULL, "vsvvv", id, "DataStatus", &back, &main, &black) &&
		LvN_ScorerSet(id, "DataStatus", "MaxValue", &v);
	int first = ok ? LvN_GEqI(GD, "firstTime", 1) : -1;
	if (first > 0) {
		Native_SetInt(&one, 1);
		ok = LvN_ScorerSet(id, "DataStatus", "IsAutoUpdatable", &one) && LvN_ScorerSet(id, "DataStatus", "Value", &v) &&
			LvN_ScorerSet(id, "DataStatus", "AutoUpdateStopValue", &v) && LvN_PutI(GD, "firstTime", 0);
	} else {
		ok = first == 0;
	}
	int i32 = 0;
	ok = ok && Native_ArgInt(id, &i32);
	for (int i = 0; i < 4 && ok; i++) {
		Mode_ScorerGet(i32, "DataStatus", vars[i], &xywh[i]);
	}
	ok = ok && Gd_Place(id, "DataMarker1", &xywh[0], &xywh[1], &xywh[2], &xywh[3], 0.333) &&
		Gd_Place(id, "DataMarker2", &xywh[0], &xywh[1], &xywh[2], &xywh[3], 0.666);
	Native_Free(&t);
	Native_Free(&max);
	Native_Free(&sf);
	Native_Free(&v);
	Native_Free(&it);
	Native_Free(&back);
	Native_Free(&main);
	Native_Free(&black);
	for (int i = 0; i < 4; i++) {
		Native_Free(&xywh[i]);
	}
	return ok;
}

LV_CORE0(GetDataPart2_InitStatus)
LV_CORE0(GetDataPart2_Init)
LV_CORE0(GetDataPart2_TalkToBoss)
LV_CORE0(GetDataPart2_PreInitMission)
LV_CORE0(GetDataPart2_InitMission)
BA_CORE1(GetDataPart2_CreateFuncInit)
BA_CORE1(GetDataPart2_OnFuncDefState)
BA_CORE1(GetDataPart2_SetStopDownloadingState)
BA_CORE1(GetDataPart2_SetNormalState)
BA_CORE1(GetDataPart2_AbortDownload)
BA_CORE1(GetDataPart2_OnDownloadAborted)
BA_CORE1(GetDataPart2_OnFuncStartsWorking)
LV_CORE0(GetDataPart2_OnDownloadComplete)
BA_CORE1(GetDataPart2_MissionScorer)

LV_COREA(GetDataPart2_BossSpeech)
LV_COREA(GetDataPart2_OnFunctionarySpeech)
LV_COREA(GetDataPart2_OnBeginDownloadData)

static byte GetDataPart2_SetDesktopState_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return GetDataPart2_SetDesktopState(&a[0], &a[1]);
}

LV_FN(GetDataPart2, ComputeAutoUpdateStopValuePy, 0, GetDataPart2_ComputeAutoUpdateStopValue_C)
LV_FN(GetDataPart2, InitStatusPy, 1, GetDataPart2_InitStatus_C)
LV_FN(GetDataPart2, InitPy, 1, GetDataPart2_Init_C)
LV_FN(GetDataPart2, TalkToBossPy, 0, GetDataPart2_TalkToBoss_C)
LV_FN(GetDataPart2, BossSpeechPy, 4, GetDataPart2_BossSpeech_C)
LV_FN(GetDataPart2, PreInitMissionPy, 0, GetDataPart2_PreInitMission_C)
LV_FN(GetDataPart2, InitMissionPy, 0, GetDataPart2_InitMission_C)
LV_FN(GetDataPart2, SetDesktopStatePy, 2, GetDataPart2_SetDesktopState_C)
LV_FN(GetDataPart2, CreateFuncInitPy, 1, GetDataPart2_CreateFuncInit_C)
LV_FN(GetDataPart2, OnFunctionarySpeechPy, 4, GetDataPart2_OnFunctionarySpeech_C)
LV_FN(GetDataPart2, OnFuncDefStatePy, 1, GetDataPart2_OnFuncDefState_C)
LV_FN(GetDataPart2, SetStopDownloadingStatePy, 1, GetDataPart2_SetStopDownloadingState_C)
LV_FN(GetDataPart2, SetNormalStatePy, 1, GetDataPart2_SetNormalState_C)
LV_FN(GetDataPart2, AbortDownloadPy, 1, GetDataPart2_AbortDownload_C)
LV_FN(GetDataPart2, OnDownloadAbortedPy, 1, GetDataPart2_OnDownloadAborted_C)
LV_FN(GetDataPart2, OnBeginDownloadDataPy, 4, GetDataPart2_OnBeginDownloadData_C)
LV_FN(GetDataPart2, OnFuncStartsWorkingPy, 1, GetDataPart2_OnFuncStartsWorking_C)
LV_FN(GetDataPart2, OnDownloadCompletePy, 0, GetDataPart2_OnDownloadComplete_C)
LV_FN(GetDataPart2, MissionScorerPy, 1, GetDataPart2_MissionScorer_C)

PyMethodDef g_BankGetDataPart2Methods[] = {
	{ "ComputeAutoUpdateStopValue", GetDataPart2_ComputeAutoUpdateStopValuePy, METH_VARARGS, NULL },
	{ "InitStatus", GetDataPart2_InitStatusPy, METH_VARARGS, NULL },
	{ "Init", GetDataPart2_InitPy, METH_VARARGS, NULL },
	{ "TalkToBoss", GetDataPart2_TalkToBossPy, METH_VARARGS, NULL },
	{ "BossSpeech", GetDataPart2_BossSpeechPy, METH_VARARGS, NULL },
	{ "PreInitMission", GetDataPart2_PreInitMissionPy, METH_VARARGS, NULL },
	{ "InitMission", GetDataPart2_InitMissionPy, METH_VARARGS, NULL },
	{ "SetDesktopState", GetDataPart2_SetDesktopStatePy, METH_VARARGS, NULL },
	{ "CreateFuncInit", GetDataPart2_CreateFuncInitPy, METH_VARARGS, NULL },
	{ "OnFunctionarySpeech", GetDataPart2_OnFunctionarySpeechPy, METH_VARARGS, NULL },
	{ "OnFuncDefState", GetDataPart2_OnFuncDefStatePy, METH_VARARGS, NULL },
	{ "SetStopDownloadingState", GetDataPart2_SetStopDownloadingStatePy, METH_VARARGS, NULL },
	{ "SetNormalState", GetDataPart2_SetNormalStatePy, METH_VARARGS, NULL },
	{ "AbortDownload", GetDataPart2_AbortDownloadPy, METH_VARARGS, NULL },
	{ "OnDownloadAborted", GetDataPart2_OnDownloadAbortedPy, METH_VARARGS, NULL },
	{ "OnBeginDownloadData", GetDataPart2_OnBeginDownloadDataPy, METH_VARARGS, NULL },
	{ "OnFuncStartsWorking", GetDataPart2_OnFuncStartsWorkingPy, METH_VARARGS, NULL },
	{ "OnDownloadComplete", GetDataPart2_OnDownloadCompletePy, METH_VARARGS, NULL },
	{ "MissionScorer", GetDataPart2_MissionScorerPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_InfiltrateBank ---- */

/* SInput.GetActionSet() through its seam (fc-r6-ib swaps it) */
static byte Ib_ActionSet(nativeVal_t *out) {
	PyObject *hook;
	int seam = Native_Seam("SInput", "GetActionSet", &hook);
	Native_SetNone(out);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, out, "");
	}
	Native_SetStr(out, TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	return 1;
}

/* InitStatus(LevelPath): Dtritus' resources; the player is not the bank master */
static byte InfiltrateBank_InitStatus(void) {
	if (!LV_CALL("Chars", "AddNewCharRes", NULL, "si", "Dtritus", 0L)) {
		return 0;
	}
	if (LvN_SaveIs("Char", "BankMaster")) {
		SaveVar_Set("Char", "Dtritus");
	}
	return 1;
}

/* Init(LevelPath): Deep Throat's message, the teleporter to the bank master's room, the possession hooks, the scene */
static byte InfiltrateBank_Init(void) {
	nativeVal_t elv = { 0 }, list = { 0 }, bm = { 0 }, r = { 0 }, one = { 0 };
	byte ok;
	if (LvN_SaveIs("MissionState", "FirstTime")) {
		ok = LvN_Com("Mission_InfiltrateBank_RemoteMessage1", "DeepThroat", NULL, NULL, NULL);
		if (ok) {
			SaveVar_Set("MissionState", "NotFirstTime");
		}
	} else {
		ok = LvN_Com("Mission_InfiltrateBank_RemoteMessage2", "DeepThroat", NULL, NULL, NULL);
	}
	Native_SetNone(&elv);
	Native_SetNone(&list);
	Native_SetNone(&r);
	LvN_Str(&bm, "BankMaster");
	ok = ok && LvN_ModSetGlobal("GateKeeper", "AfterGateKeeperSpeech", IB, "ShowDeepThroatMessage") &&
		LvN_Entity("DM_Door_Teleporter_02", &elv) && LvN_SetS(&elv, "UseFunc", "Mission_InfiltrateBank.UseDoor") &&
		LvN_ModGet("CharConversor", "AvailableChars", &list);
	int in = ok ? Native_Contains(&list, &bm) : -1;
	Native_SetInt(&one, 1);
	ok = in >= 0 && (in == 0 || LvN_ModMethod("CharConversor", "AvailableChars", "remove", &bm)) &&
		LvN_ModSetGlobal("CharConversor", "OnPossession", IB, "OnPossession") &&
		LvN_ModSetGlobal("CharConversor", "afterExitConversor", IB, "ShowDeepThroatMessage") &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", "GateKeeper_BankMasterRoom") && LvN_GetClass("BankMaster", &r) &&
		LvN_SetCls("CanBePossessed", &one) && LV_OWN(IB, "LoadScene", NULL, "");
	Native_Free(&elv);
	Native_Free(&list);
	Native_Free(&bm);
	Native_Free(&r);
	return ok;
}

/* UseDoor(UsrName, ElevatorName, optId, FullOption): the teleporter of the gatekeeper; the scene when it was used */
static byte InfiltrateBank_UseDoor(const nativeVal_t *a) {
	nativeVal_t r = { 0 };
	Native_SetNone(&r);
	byte ok = LV_CALL("GateKeeper", "UseDoor", &r, "vvvv", &a[0], &a[1], &a[2], &a[3]);
	if (ok && Native_True(&r)) {
		ok = LV_OWN(IB, "TestEndOfElevator", NULL, "") && Mode_Fade(0, lv_clear, lv_black, 2.5);
	}
	Native_Free(&r);
	return ok;
}

/* TestEndOfElevator(): the scene once the player walks again */
static byte InfiltrateBank_TestEndOfElevator(void) {
	nativeVal_t set = { 0 }, usr = { 0 };
	if (!Ib_ActionSet(&set)) {
		return 0;
	}
	byte walk = LvN_EqS(&set, "Walk");
	Native_Free(&set);
	if (!walk) {
		return Mode_Later(0.1, IB, "TestEndOfElevator", NULL, 0, NULL);
	}
	byte ok = Mode_UsrHandle(&usr) && LV_OWN(IB, "LaunchScene", NULL, "v", &usr);
	Native_Free(&usr);
	return ok;
}

/* ReAssignTarget(): the bank master is the target as soon as he is there */
static byte InfiltrateBank_ReAssignTarget(void) {
	nativeVal_t name = { 0 }, e = { 0 };
	Native_SetNone(&e);
	if (!Mode_G(IB, "BankMasterName", &name)) {
		return 0;
	}
	int there = Ba_EntTrue(&name, &e);
	Native_Free(&name);
	Native_Free(&e);
	if (there <= 0) {
		return there == 0 && Mode_Later(0.5, IB, "ReAssignTarget", NULL, 0, NULL);
	}
	byte ok = Mode_G(IB, "BankMasterName", &name) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &name);
	Native_Free(&name);
	return ok;
}

/* AfterDTMessage(): the teleporter is the target */
static byte InfiltrateBank_AfterDTMessage(void) {
	nativeVal_t elv = { 0 }, name = { 0 };
	Native_SetNone(&elv);
	Native_SetNone(&name);
	byte ok = LvN_Entity("DM_Door_Teleporter_02", &elv) && Mode_VGet(&elv, "Name", &name) &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &name);
	Native_Free(&elv);
	Native_Free(&name);
	return ok;
}

/* ShowDeepThroatMessage(): SetArrowToBankMaster in 1 s (once) */
static byte InfiltrateBank_ShowDeepThroatMessage(void) {
	return Mode_DelSchedS("SetArrowToBankMaster") && Mode_Later(1.0, IB, "SetArrowToBankMaster", NULL, 0, "SetArrowToBankMaster");
}

/* SetArrowToBankMaster(): not possessing the bank master: Deep Throat says so, then the bank master is the target */
static byte InfiltrateBank_SetArrowToBankMaster(void) {
	nativeVal_t usr = { 0 }, ctrl = { 0 }, said = { 0 };
	Native_SetNone(&ctrl);
	Native_SetNone(&said);
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	int c = Native_True(&usr);
	if (c) {
		c = Mode_VGet(&usr, "ActCtrl", &ctrl) ? !LvN_EqS(&ctrl, "BankMaster") : -1;
	}
	byte ok = c >= 0;
	if (c > 0) {
		ok = LV_CALL("Speech", "RemoteComunication", &said, "ss", "Mission_InfiltrateBank_RemoteMessage2", "DeepThroat");
		if (ok) {
			ok = Native_True(&said) ? LV_OWN(IB, "ReAssignTarget", NULL, "") :
				Mode_Later(1.0, IB, "SetArrowToBankMaster", NULL, 0, "SetArrowToBankMaster");
		}
	}
	Native_Free(&usr);
	Native_Free(&ctrl);
	Native_Free(&said);
	return ok;
}

/* CreateBankMaster(e): the bank master stays and talks (BankMasterSpeech) */
static byte InfiltrateBank_CreateBankMaster(const nativeVal_t *e) {
	nativeVal_t empty = { 0 };
	LvN_Str(&empty, "");
	byte ok = LvN_Stay(e, 6, &empty) && LvN_SetS(e, "UseFunc", "Mission_InfiltrateBank.BankMasterSpeech");
	Native_Free(&empty);
	return ok;
}

/* LaunchScene(Usr): the scene DirectorDiscovered, LaunchScene2 at its end; the player is Dtritus again */
static byte InfiltrateBank_LaunchScene(const nativeVal_t *usr) {
	return Mode_Import("Elevator") && Mode_Import("GateKeeper") && LvN_ModNone("GateKeeper", "AfterGateKeeperSpeech") &&
		LvN_ModNone("Elevator", "OnElevatorReachUp") && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", "") &&
		LV_CALL("Scene", "Play", NULL, "s", "DirectorDiscovered") && LvN_ModSetGlobal("Scene", "EndOfSceneCallback", IB, "LaunchScene2") &&
		LvN_SetS(usr, "ActCtrl", "Dtritus");
}

/* bm = CharsNPC.CharRes[BankMasterName]; bm.CallbackFunc.append(CreateBankMaster); bm.Pos = dummy[1] into *pos
 * (OBJLIB 5: the character objects of CharsNPC) */
static byte Ib_CharRes(const nativeVal_t *dummy, nativeVal_t *pos) {
	nativeVal_t name = { 0 };
	PyObject *res = Script_Attr("CharsNPC", "CharRes");
	if (!res || !Mode_G(IB, "BankMasterName", &name)) {
		Py_XDECREF(res);
		return 0;
	}
	PyObject *key = Native_ToPy(&name);
	Native_Free(&name);
	PyObject *bm = key ? PyObject_GetItem(res, key) : NULL;
	PyObject *cb = bm ? PyObject_GetAttrString(bm, "CallbackFunc") : NULL;
	PyObject *append = cb ? PyObject_GetAttrString(cb, "append") : NULL;
	PyObject *fn = append ? Mode_GlobalPy(IB, "CreateBankMaster") : NULL;
	byte ok = fn && Native_Call(append, NULL, "O", fn);
	if (ok) {
		nativeVal_t b = { 0 };
		Native_Arg(bm, &b);
		ok = LvN_Item(dummy, 1, pos) && Mode_VSet(&b, "Pos", pos);
		Native_Free(&b);
	}
	Py_XDECREF(fn);
	Py_XDECREF(append);
	Py_XDECREF(cb);
	Py_XDECREF(bm);
	Py_XDECREF(key);
	Py_DECREF(res);
	return ok;
}

/* LaunchScene2(Usr): after the scene the bank master stands at his place and talks; Berto calls */
static byte InfiltrateBank_LaunchScene2(void) {
	nativeVal_t dummy = { 0 }, pos = { 0 }, name = { 0 }, e = { 0 }, n = { 0 };
	Native_SetNone(&pos);
	Native_SetNone(&name);
	Native_SetNone(&e);
	Native_SetNone(&n);
	if (!Mode_Fade(0, lv_black, lv_clear, 2.0)) {
		return 0;
	}
	LvN_Dummy("DM_BankMasterAfterScenePos", 1, &dummy);
	byte ok = Ib_CharRes(&dummy, &pos) && Mode_G(IB, "BankMasterName", &name);
	int c = ok ? Ba_EntTrue(&name, &e) : -1;
	ok = c >= 0;
	if (c > 0) {
		ok = Mode_VSet(&e, "Pos", &pos) && Mode_VGet(&e, "Name", &n) && Mode_SetOnFloor(&n) && Mode_VSet(&e, "LLogicOrigin", &pos) &&
			LV_OWN(IB, "CreateBankMaster", NULL, "v", &e);
	}
	ok = ok && LvN_Com("Mission_InfiltrateBank_RemoteMessage4", "Berto", IB, "SetNextMission", LvN_NoArgs());
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&name);
	Native_Free(&e);
	Native_Free(&n);
	return ok;
}

/* BankMasterSpeech(UsrName, CharName, optId, FullOption): what the bank master says */
static byte InfiltrateBank_BankMasterSpeech(const nativeVal_t *a) {
	return LvN_Speakers(&a[0], &a[1]) && (!LvN_EqS(&a[2], "Talk") ||
		LV_CALL("Speech", "OnSpch", NULL, "ss", "Mission_MeetBerto_BankBoss_InitMsg1", "Mission_MeetBerto_BankBoss_InitAns1"));
}

/* SetNextMission(): the bank master can be possessed again, the hooks off, Mission_MeetBerto in Scrapyard */
static byte InfiltrateBank_SetNextMission(void) {
	nativeVal_t bm = { 0 };
	if (!Mode_Import("CharConversor") || !Ba_DelSaves("MissionState")) {
		return 0;
	}
	LvN_Str(&bm, "BankMaster");
	byte ok = LvN_ModAppend("CharConversor", "AvailableChars", &bm) && LvN_ModNone("CharConversor", "OnPossession") &&
		LvN_ModNone("CharConversor", "afterExitConversor") &&
		LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_MeetBerto", "Scrapyard") &&
		LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_MeetBerto_MissionText1", 1L) && LV_CALL("SaveGame", "QWayPoint", NULL, "");
	Native_Free(&bm);
	return ok;
}

LV_CORE0(InfiltrateBank_InitStatus)
LV_CORE0(InfiltrateBank_Init)
LV_COREA(InfiltrateBank_UseDoor)
LV_CORE0(InfiltrateBank_TestEndOfElevator)
LV_CORE0(InfiltrateBank_ReAssignTarget)
LV_CORE0(InfiltrateBank_AfterDTMessage)
LV_CORE0(InfiltrateBank_ShowDeepThroatMessage)
LV_CORE0(InfiltrateBank_SetArrowToBankMaster)
BA_CORE1(InfiltrateBank_CreateBankMaster)
BA_CORE1(InfiltrateBank_LaunchScene)
LV_CORE0(InfiltrateBank_LaunchScene2)
LV_COREA(InfiltrateBank_BankMasterSpeech)
LV_CORE0(InfiltrateBank_SetNextMission)

LV_FN(InfiltrateBank, InitStatusPy, 1, InfiltrateBank_InitStatus_C)
LV_FN(InfiltrateBank, InitPy, 1, InfiltrateBank_Init_C)
LV_FN(InfiltrateBank, UseDoorPy, 4, InfiltrateBank_UseDoor_C)
LV_FN(InfiltrateBank, TestEndOfElevatorPy, 0, InfiltrateBank_TestEndOfElevator_C)
LV_FN(InfiltrateBank, ReAssignTargetPy, 0, InfiltrateBank_ReAssignTarget_C)
LV_FN(InfiltrateBank, AfterDTMessagePy, 0, InfiltrateBank_AfterDTMessage_C)
LV_FN(InfiltrateBank, ShowDeepThroatMessagePy, 0, InfiltrateBank_ShowDeepThroatMessage_C)
LV_FN(InfiltrateBank, SetArrowToBankMasterPy, 0, InfiltrateBank_SetArrowToBankMaster_C)
LV_FN(InfiltrateBank, CreateBankMasterPy, 1, InfiltrateBank_CreateBankMaster_C)
LV_FN(InfiltrateBank, LaunchScenePy, 1, InfiltrateBank_LaunchScene_C)
LV_FN(InfiltrateBank, LaunchScene2Py, 1, InfiltrateBank_LaunchScene2_C)
LV_FN(InfiltrateBank, BankMasterSpeechPy, 4, InfiltrateBank_BankMasterSpeech_C)
LV_FN(InfiltrateBank, SetNextMissionPy, 0, InfiltrateBank_SetNextMission_C)

PyMethodDef g_BankInfiltrateBankMethods[] = {
	{ "InitStatus", InfiltrateBank_InitStatusPy, METH_VARARGS, NULL },
	{ "Init", InfiltrateBank_InitPy, METH_VARARGS, NULL },
	{ "UseDoor", InfiltrateBank_UseDoorPy, METH_VARARGS, NULL },
	{ "TestEndOfElevator", InfiltrateBank_TestEndOfElevatorPy, METH_VARARGS, NULL },
	{ "ReAssignTarget", InfiltrateBank_ReAssignTargetPy, METH_VARARGS, NULL },
	{ "AfterDTMessage", InfiltrateBank_AfterDTMessagePy, METH_VARARGS, NULL },
	{ "ShowDeepThroatMessage", InfiltrateBank_ShowDeepThroatMessagePy, METH_VARARGS, NULL },
	{ "SetArrowToBankMaster", InfiltrateBank_SetArrowToBankMasterPy, METH_VARARGS, NULL },
	{ "CreateBankMaster", InfiltrateBank_CreateBankMasterPy, METH_VARARGS, NULL },
	{ "LaunchScene", InfiltrateBank_LaunchScenePy, METH_VARARGS, NULL },
	{ "LaunchScene2", InfiltrateBank_LaunchScene2Py, METH_VARARGS, NULL },
	{ "BankMasterSpeech", InfiltrateBank_BankMasterSpeechPy, METH_VARARGS, NULL },
	{ "SetNextMission", InfiltrateBank_SetNextMissionPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Bank)
