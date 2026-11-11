/*
 * The HangarTab module in C (scripts/shipedit/hangartab.py, issue #2): the Hangar panel of the ship editor. The nine
 * hangars and the emergency ship (selecting one sends its ship up on the platform), buying, rebuilding and selling a
 * ship with the money prediction, the ship information (speed ramp, boost, weight, weapons) and the mini information
 * of the hangar under the cursor, the pilots of a network game. The layout is data/menus/shipedit.def (sections
 * word, reference, panel, hangar, info, pilot, pilots and links).
 *
 * The module variables stay in the script, the other panels and the levels read and set them: prevItem, prevShip,
 * miniShip, prevHangar, currentHangar, currentShipToEdit, currentPilot, holograms, shipTotalCost, isXbox, UseTxt and
 * the layout constants (hangarBaseX, baseShipInfoX, speedRampX and the others). The module code (the save variable
 * Hangar.iHangarShip) stays in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, the PyMethodDef entries are adapters with
 * the argument formats of the C port. The variables of the module are read where the script reads them and written at
 * once (OBJLIB 5); prevShip, miniShip and the ships of ShipEdit.editedShips are handle values (OBJLIB 7), the lists of
 * ShipEdit are changed in place on their objects. ShipEdit, ShipMaker and Vehicles are called through their refs
 * (PORT_CALL: directly while the attribute holds the port's function, else by name), the module's own functions through
 * its globals as the script calls them (PORT_OWN, the same seam on the global). Menu's link functions are named at run
 * time and called through their entries while they are Menu's own (Sn_Link), the SScorer and SSound calls go through
 * their refs (stage 7); the helpers of ShipEdit.h (Sn_Multi, Sn_Link) are those of ShipEdit.c. ComputeShipTotalCost
 * evaluates its text through the name chain of the module (OBJLIB 6), the .def runner takes Python values (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "game/Script/levels/Levels.h"
#include "ShipEdit.h"

static const nativeVal_t ht_none = { 0 };

/* a global of HangarTab as its functions read it (the NameError of a missing one) */
static byte Ht_G(const char *name, nativeVal_t *out) {
	return Mode_G(HT, name, out);
}

/* prevShip.<var> with prevShip the global */
static byte Ht_ShipGet(const char *var, nativeVal_t *out) {
	nativeVal_t s = { 0 };
	byte ok = Ht_G("prevShip", &s) && Mode_VGet(&s, var, out);
	Native_Free(&s);
	return ok;
}

/* ShipEdit.<var>[global] (the list first, as Python evaluates it) */
static byte Ht_AtG(const char *var, const char *global, nativeVal_t *out) {
	nativeVal_t l = { 0 }, k = { 0 };
	Native_SetNone(out);
	byte ok = LvN_ModGet(SE, var, &l) && Ht_G(global, &k) && Mode_Sub(&l, &k, out);
	Native_Free(&l);
	Native_Free(&k);
	return ok;
}

/* ShipEdit.<var>[currentHangar] */
static byte Ht_AtCur(const char *var, nativeVal_t *out) {
	return Ht_AtG(var, "currentHangar", out);
}

/* ShipEdit.<var>[i] */
static byte Ht_AtI(const char *var, long i, nativeVal_t *out) {
	nativeVal_t l = { 0 };
	Native_SetNone(out);
	byte ok = LvN_ModGet(SE, var, &l) && LvN_Item(&l, i, out);
	Native_Free(&l);
	return ok;
}

/* ShipEdit.<var>[currentHangar] = v, in place */
static byte Ht_SetAtCur(const char *var, const nativeVal_t *v) {
	nativeVal_t k = { 0 };
	byte ok = Ht_G("currentHangar", &k) && Mode_ModDictPut(SE, var, &k, v);
	Native_Free(&k);
	return ok;
}

/* ShipEdit.<var>[global] op x: 1, 0, -1 */
static int Ht_AtCmp(const char *var, const char *global, const char *op, const nativeVal_t *x) {
	nativeVal_t v = { 0 };
	int r = Ht_AtG(var, global, &v) ? Mode_Cmp(&v, op, x) : -1;
	Native_Free(&v);
	return r;
}

/* the global == i: 1, 0, -1 */
static int Ht_GEq(const char *name, long i) {
	return LvN_GEqI(HT, name, i);
}

/* ShipEdit.IsEditingMultiplayer() and not isXbox: the pilots of a network game (1, 0, -1) */
static int Ht_Pilots(void) {
	int mp = Sn_Multi(0);
	if (mp > 0) {
		int x = LvN_GTrue(HT, "isXbox");
		return x < 0 ? -1 : !x;
	}
	return mp;
}

/* not ShipEdit.IsEditingMultiplayer(): 1, 0, -1 */
static int Ht_Single(void) {
	int mp = Sn_Multi(0);
	return mp < 0 ? -1 : !mp;
}

/* int(v) into whole and the first decimal of v as the script reads it, int((`v - float(int(v))`)[2]) (asInt), or the
 * character */
static byte Ht_Decimal(const nativeVal_t *v, nativeVal_t *whole, nativeVal_t *dec, byte asInt) {
	nativeVal_t fl = { 0 }, d = { 0 }, r = { 0 }, ch = { 0 };
	byte ok = Native_ToInt(v, whole) && Native_ToFloat(whole, &fl) && Mode_Arith('-', v, &fl, &d);
	if (ok) {
		Mode_Repr(&d, &r);
		ok = Native_SeqItem(&r, 2, asInt ? &ch : dec) && (!asInt || Native_ToInt(&ch, dec));
	}
	Native_Free(&fl);
	Native_Free(&d);
	Native_Free(&r);
	Native_Free(&ch);
	return ok;
}

/* the text `whole` "." dec of a boost time */
static byte Ht_BoostText(const nativeVal_t *t, nativeVal_t *out) {
	nativeVal_t whole = { 0 }, dec = { 0 }, r = { 0 }, a = { 0 };
	byte ok = Ht_Decimal(t, &whole, &dec, 0);
	if (ok) {
		Mode_Repr(&whole, &r);
		ok = Sn_Cat("", &r, ".", &a) && Native_Arith('+', &a, &dec, out);
	}
	Native_Free(&whole);
	Native_Free(&dec);
	Native_Free(&r);
	Native_Free(&a);
	return ok;
}

/* SScorer.Set(id, item, var, `v`) */
static byte Ht_SetRepr(int id, const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t r = { 0 };
	Mode_Repr(v, &r);
	byte ok = Sn_SetC(id, item, var, &r);
	Native_Free(&r);
	return ok;
}

/* SScorer.Set(id, item, var, `int(v)`) */
static byte Ht_SetReprInt(int id, const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t i = { 0 };
	byte ok = Native_ToInt(v, &i) && Ht_SetRepr(id, item, var, &i);
	Native_Free(&i);
	return ok;
}

/* the global of HangarTab into the variable of the item */
static byte Ht_SetG(int id, const char *item, const char *var, const char *global) {
	nativeVal_t v = { 0 };
	byte ok = Ht_G(global, &v) && Sn_SetC(id, item, var, &v);
	Native_Free(&v);
	return ok;
}

/* ShowBoostPrediction(id, newWeight): the arrow of the boost time a new weight gives (up, down or equal) */
static byte Ht_BoostPrediction(int id, const nativeVal_t *weight) {
	static const char *const arrows[] = { "BoostUp", "BoostDown", "BoostEqual", "BoostBack" };
	nativeVal_t cur = { 0 }, model = { 0 }, nb = { 0 }, ci = { 0 }, cd = { 0 }, ni = { 0 }, nd = { 0 };
	if (!Mode_Import("ShipMaker")) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		Script_SetNum(id, arrows[i], "Visible", 0);
	}
	byte ok = Ht_ShipGet("BoostTime", &cur) && Ht_ShipGet("Model", &model) &&
		PORT_CALL(ShipMaker, GetBoostFactor, &nb, "vv", &model, weight) && Ht_Decimal(&cur, &ci, &cd, 1) &&
		Ht_Decimal(&nb, &ni, &nd, 1);
	const char *arrow = "BoostEqual";
	int c = ok ? Mode_Cmp(&ni, "<", &ci) : -1;
	if (c > 0) {
		arrow = "BoostDown";
	} else if (c == 0 && (c = Mode_Cmp(&ni, ">", &ci)) > 0) {
		arrow = "BoostUp";
	} else if (c == 0 && (c = Mode_Cmp(&nd, "<", &cd)) > 0) {
		arrow = "BoostDown";
	} else if (c == 0 && (c = Mode_Cmp(&nd, ">", &cd)) > 0) {
		arrow = "BoostUp";
	}
	ok = c >= 0;
	if (ok) {
		Script_SetNum(id, arrow, "Visible", 1);
		Script_SetNum(id, "BoostBack", "Visible", 1);
	}
	Native_Free(&cur);
	Native_Free(&model);
	Native_Free(&nb);
	Native_Free(&ci);
	Native_Free(&cd);
	Native_Free(&ni);
	Native_Free(&nd);
	return ok;
}

/* for t in ShipEdit.tabs[1:]: SScorer.Set(id, "ShipEditTab" + t, "OnAccept", on and "ShipEdit.ChangeTab" or "") */
static byte Ht_TabsAccept(int id, byte on) {
	nativeVal_t tabs = { 0 }, rest = { 0 }, t = { 0 }, name = { 0 }, v;
	Native_SetStr(&v, on ? "ShipEdit.ChangeTab" : "");
	byte ok = LvN_ModGet(SE, "tabs", &tabs) && Sn_Rest(&tabs, &rest);
	int n = ok ? Native_ForCount(&rest) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&rest, i, &t) && Sn_Cat("ShipEditTab", &t, NULL, &name) && Sn_Set(id, &name, "OnAccept", &v);
		Native_Free(&t);
		Native_Free(&name);
	}
	Native_Free(&tabs);
	Native_Free(&rest);
	return ok;
}

/* the tabs of ShipEdit.tabs: the ring of links, on or off */
static byte Ht_TabRing(int id, const char *fn) {
	nativeVal_t tabs = { 0 };
	byte ok = LvN_ModGet(SE, "tabs", &tabs) && Sn_TabRing(id, &tabs, fn);
	Native_Free(&tabs);
	return ok;
}

static byte Ht_TabsOn(int id, byte on) {
	nativeVal_t tabs = { 0 };
	byte ok = LvN_ModGet(SE, "tabs", &tabs) && Sn_TabsOn(id, &tabs, on);
	Native_Free(&tabs);
	return ok;
}

/* SScorer.<fn>(id, text) through the ref of fn (SetOnNext, SetOnPrev, SetDefault, SetOnCancel) */
#define HT_SCORER(fn, id, s) PORT_CALL(SScorer, fn, NULL, "is", (long)(id), s)

/* the tab keys of the scorer: ShipEdit.ChangeTabRight and Left, or none */
static byte Ht_NextPrev(int id, byte on) {
	return HT_SCORER(SetOnNext, id, on ? "ShipEdit.ChangeTabRight" : "") &&
		HT_SCORER(SetOnPrev, id, on ? "ShipEdit.ChangeTabLeft" : "");
}

/* the buttons that change the ship on the platform: OnAccept of the hangars, the tabs, Sell, Exit, the ship type
 * buttons and the pilot buttons; off while the platform moves */
static byte Ht_PlatformButtons(int id, byte on) {
	for (int i = 0; i < 9; i++) {
		char name[0x10];
		Str_Printf(name, sizeof(name), "Hangar%d", i + 1);
		Script_SetStr(id, name, "OnAccept", on ? "HangarTab.SelectItem" : "");
	}
	Script_SetStr(id, "HangarAux", "OnAccept", on ? "HangarTab.SelectItem" : "");
	if (!on) {
		if (!Ht_TabsAccept(id, 0)) {
			return 0;
		}
	} else {
		nativeVal_t e = { 0 };
		int c = Ht_AtCur("editedShips", &e) ? Native_True(&e) : -1;
		Native_Free(&e);
		if (c > 0) {
			c = Ht_GEq("currentHangar", 9);
			c = c < 0 ? -1 : !c;
		}
		if (c > 0) {
			Native_SetInt(&e, 1);
			c = Ht_AtCmp("destroyedShips", "currentHangar", "!=", &e);
		}
		if (c < 0 || (c > 0 && (!Ht_TabRing(id, "LinkLR") || !Ht_TabsAccept(id, 1)))) {
			return 0;
		}
	}
	Script_SetStr(id, "Sell", "OnAccept", on ? "HangarTab.SellShip" : "");
	Script_SetStr(id, "Exit", "OnAccept", on ? "ShipEdit.ExitEditShipMenu" : "");
	Script_SetStr(id, "ShipType", "OnAccept", on ? "HangarTab.BuyShip" : "");
	Script_SetStr(id, "PrevShip", "OnAccept", on ? "HangarTab.PrevShipToEdit" : "");
	Script_SetStr(id, "NextShip", "OnAccept", on ? "HangarTab.NextShipToEdit" : "");
	int p = Ht_Pilots();
	if (p < 0) {
		return 0;
	}
	if (p) {
		Script_SetStr(id, "PrevPilot", "OnAccept", on ? "HangarTab.PrevPilot" : "");
		Script_SetStr(id, "NextPilot", "OnAccept", on ? "HangarTab.NextPilot" : "");
	}
	return (on || Ht_TabRing(id, "UnLinkLR")) && HT_SCORER(SetOnCancel, id, on ? "ShipEdit.ExitEditShipMenu" : "");
}

/* DisableChangePlatformButtons(id), EnableChangePlatformButtons(id) */
static byte Ht_Platform(int id, byte on) {
	return Sn_From(on ? "LinkLR" : "UnLinkLR") && Mode_Import(SE) && Ht_PlatformButtons(id, on);
}

/* SScorer.Set(id, ShipMaker.PilotsList[currentPilot], "Visible", v) */
static byte Ht_PilotVisible(int id, long v) {
	nativeVal_t k = { 0 }, item = { 0 }, x;
	Native_SetInt(&x, v);
	nativeVal_t l = { 0 };
	byte ok = LvN_ModGet("ShipMaker", "PilotsList", &l) && Ht_G("currentPilot", &k) && Mode_Sub(&l, &k, &item) &&
		Sn_Set(id, &item, "Visible", &x);
	Native_Free(&l);
	Native_Free(&k);
	Native_Free(&item);
	return ok;
}

/* len(ShipMaker.PilotsList) */
static int Ht_Pilots_(void) {
	nativeVal_t l = { 0 };
	int n = LvN_ModGet("ShipMaker", "PilotsList", &l) ? Mode_Len(&l) : -1;
	Native_Free(&l);
	return n;
}

/* PrevPilot(id, control), NextPilot(id, control): the head of the pilot before or after the current one */
static byte Ht_Pilot(int id, int step) {
	nativeVal_t v = { 0 }, x;
	if (!Mode_Import("ShipMaker") || !Ht_PilotVisible(id, 0) || !Mode_GOpI(HT, "currentPilot", step < 0 ? '-' : '+', 1) ||
		!Ht_G("currentPilot", &v)) {
		Native_Free(&v);
		return 0;
	}
	int c, n = 0;
	if (step < 0) {
		c = Mode_CmpI(&v, "<", 0);
		if (c > 0) {
			n = Ht_Pilots_();
			c = n < 0 ? -1 : 1;
		}
	} else {
		n = Ht_Pilots_();
		c = n < 0 ? -1 : Mode_CmpI(&v, ">", n - 1);
	}
	Native_Free(&v);
	if (c < 0 || (c > 0 && !Mode_Put(HT, "currentPilot", NULL, Mode_IntVal(&x, step < 0 ? n - 1 : 0)))) {
		return 0;
	}
	return Ht_PilotVisible(id, 1);
}

/* the items of the buttons that show and hide together */
static const char *const ht_sell[] = { "Sell", "SellBack", "SellCircuit1", "SellCircuit2", "SellCircuit3", "SellCircuit4",
	NULL };
static const char *const ht_exit[] = { "Exit", "ExitBack", "ExitCircuit1", "ExitCircuit2", "ExitCircuit3", "ExitCircuit4",
	NULL };
static const char *const ht_shipType[] = { "ShipType", "ShipTypeName", "ShipTypeBack", "ShipTypeCircuit1",
	"ShipTypeCircuit2", "PrevShip", "PrevShipBack", "PrevShipCircuit1", "PrevShipCircuit2", "PrevShipCircuit3", "NextShip",
	"NextShipBack", "NextShipCircuit1", "NextShipCircuit2", "NextShipCircuit3", NULL };
static const char *const ht_pilot[] = { "PrevPilot", "PrevPilotBack", "PrevPilotCircuit1", "PrevPilotCircuit2",
	"PrevPilotCircuit3", "NextPilot", "NextPilotBack", "NextPilotCircuit1", "NextPilotCircuit2", "NextPilotCircuit3",
	"PilotRect", "PilotText", "PilotTextRect", "PilotName", "PilotNameTab", NULL };

/* SetSellVisible, SetExitVisible, SetShipTypeVisible, SetPilotVisible (id, bVisible): the value as it is */
static byte Ht_Visible(int id, const nativeVal_t *v, const char *const *items, const char *import) {
	if (import && !Mode_Import(import)) {
		return 0;
	}
	for (; *items; items++) {
		if (!Sn_SetC(id, *items, "Visible", v)) {
			return 0;
		}
	}
	return 1;
}

/* the same lists from C, with a number */
static void Ht_Show(int id, const char *const *items, long v) {
	for (; *items; items++) {
		Script_SetNum(id, *items, "Visible", v);
	}
}

/* SetSellActive(id, bActive): the Sell button works or is greyed */
static void Ht_SellActive(int id, int on) {
	Script_SetNum(id, "Sell", "HighLight", on);
	Script_SetNum(id, "Sell", "W", on ? 35 : 0);
	Script_SetNum(id, "Sell", "H", on ? 35 : 0);
	Script_SetNum(id, "Sell", "SpriteIndex", on ? 8 : 10);
	Script_SetStr(id, "Sell", "OnAccept", on ? "HangarTab.SellShip" : "");
}

/* the words of the language text key (string.split), each the item prefix<word>: Visible v */
static byte Ht_Words(int id, const char *key, const char *prefix, long v) {
	nativeVal_t text = { 0 }, words = { 0 }, w = { 0 }, name = { 0 }, x;
	Native_SetInt(&x, v);
	Mode_Lang(key, &text);
	byte ok = Native_Split(&text, NULL, &words);
	for (int i = 0; ok && i < words.n; i++) {
		ok = Native_SeqItem(&words, i, &w) && Sn_Cat(prefix, &w, NULL, &name) && Sn_Set(id, &name, "Visible", &x);
		Native_Free(&w);
		Native_Free(&name);
	}
	Native_Free(&text);
	Native_Free(&words);
	return ok;
}

/* HideShipMiniInfo(id, control, lostFocus): the mini information of a hangar hides */
static byte Ht_HideMini(int id) {
	static const char *const items[] = { "HangarShip", "MiniMaxSpeedText", "MiniMaxSpeed", "MiniSpeedRamp1",
		"MiniSpeedRamp2", "MiniBoostRechargeTime", "MiniBoostRechargeTimeText", "MiniHullText", "MiniHullValue",
		"MiniWeight", "MiniCurrentWeight", "MiniMaxWeight", "MiniSlashWeight", "MiniShipInfo", "HLine", "VLine", "MLine",
		"AuxShipText", NULL };
	if (!Mode_Import("string")) {
		return 0;
	}
	int t = LvN_GTrue(HT, "miniShip");
	if (t < 0 || (t > 0 && !Mode_Put(HT, "miniShip", NULL, &ht_none))) {
		return 0;
	}
	if (!Ht_Words(id, "ShipEdit_AvailableHangar", "AvailableHangarText_", 0) ||
		!Ht_Words(id, "ShipEdit_UncompletedShip", "UncompletedShipText_", 0) ||
		!Ht_Words(id, "ShipEdit_DestroyedShip", "DestroyedShipText_", 0)) {
		return 0;
	}
	Ht_Show(id, items, 0);
	return 1;
}

/* HideHangarPanel(id): the items of the Hangar panel hide */
static byte Ht_HideHangarPanel(int id) {
	static const char *const aux[] = { "HangarAux", "AuxShipText", "LittleShipAux", "nAux", NULL };
	static const char *const parts[] = { "LittleShip", "Hangar", "n" };
	if (!Mode_Import(SE) || !Mode_Import("ShipMaker")) {
		return 0;
	}
	Ht_Show(id, aux, 0);
	for (int n = 1; n <= 9; n++) {
		for (int k = 0; k < 3; k++) {
			char name[0x20];
			Str_Printf(name, sizeof(name), "%s%d", parts[k], n);
			Script_SetNum(id, name, "Visible", 0);
		}
	}
	if (!Ht_PilotVisible(id, 0)) {
		return 0;
	}
	Ht_Show(id, ht_pilot, 0);
	Ht_Show(id, ht_sell, 0);
	return Ht_HideMini(id) && Sn_From("UnLinkLR") && Sn_Link("UnLinkLR", id, "Sell", "Exit");
}

/* the colour of an item: normal (83, 114, 193), selected (212, 200, 80) or destroyed (255, 32, 32) */
static void Ht_Color(int id, const char *name, int kind) {
	static const long rgb[3][3] = { { 83, 114, 193 }, { 212, 200, 80 }, { 255, 32, 32 } };
	Script_SetNum(id, name, "Red", rgb[kind][0]);
	Script_SetNum(id, name, "Green", rgb[kind][1]);
	Script_SetNum(id, name, "Blue", rgb[kind][2]);
}

/* IsShipValid(ship): the ship has engines (the lifeboat always counts): the NEng value that decided, 0 or 1 */
static byte Ht_IsShipValid(const nativeVal_t *ship, nativeVal_t *out) {
	nativeVal_t model = { 0 };
	Native_SetNone(out);
	int c = Mode_Cmp(ship, "==", &ht_none);
	if (c != 0) {
		Native_SetInt(out, 0);
		return c > 0;
	}
	if (!Mode_VGet(ship, "Model", &model)) {
		return 0;
	}
	c = LvN_EqS(&model, "SLifeBoat");
	Native_Free(&model);
	if (c) {
		Native_SetInt(out, 1);
		return 1;
	}
	Native_SetInt(out, 0);
	for (int i = 0; i < 4; i++) {
		char var[8];
		Str_Printf(var, sizeof(var), "NEng%d", i);
		Native_Free(out);
		if (!Mode_VGet(ship, var, out)) {
			return 0;
		}
		if (Native_True(out)) {
			break;
		}
	}
	return 1;
}

/* `if IsShipValid(ship):` (the C function): 1, 0, -1 */
static int Ht_Valid(const nativeVal_t *ship) {
	nativeVal_t v = { 0 };
	int r = Ht_IsShipValid(ship, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* the hint of a hangar: key % UseTxt, or the language text key */
static byte Ht_Hint(int id, const char *hangar, const char *key, byte use) {
	if (use) {
		return Sn_SetUseC(id, hangar, "Hint", key, HT);
	}
	Script_SetStr(id, hangar, "Hint", Script_Lang(key));
	return 1;
}

/* UpdateHangars(id): the hangars in their state: selected, empty, with a valid, incomplete or destroyed ship, and
 * their hints */
static byte Ht_UpdateHangars(int id) {
	if (!Mode_Import(SE)) {
		return 0;
	}
	int mp = Sn_Multi(0);
	if (mp < 0) {
		return 0;
	}
	if (!mp) {
		Script_SetNum(id, "HangarAux", "Visible", 1);
		Script_SetNum(id, "AuxShipText", "Visible", 0);
		Script_SetNum(id, "LittleShipAux", "Visible", 1);
		Script_SetNum(id, "nAux", "Visible", 1);
	}
	Script_SetNum(id, "LittleShipAux", "SpriteIndex", 4);
	Ht_Color(id, "LittleShipAux", 0);
	Script_SetNum(id, "nAux", "SpriteIndex", 4);
	Ht_Color(id, "nAux", 0);
	int aux = Ht_GEq("currentHangar", 9);
	if (aux < 0) {
		return 0;
	}
	if (aux) {
		Script_SetStr(id, "HangarAux", "Hint", Script_Lang("HINT_Hangar18"));
		Script_SetNum(id, "HangarAux", "SpriteIndex", 2);
		if (!Ht_SetG(id, "HangarAux", "Alpha", "SelectedColorAlpha")) {
			return 0;
		}
	} else {
		if (!Sn_SetUseC(id, "HangarAux", "Hint", "HINT_Hangar17", HT)) {
			return 0;
		}
		Script_SetNum(id, "HangarAux", "SpriteIndex", 1);
		if (!Ht_SetG(id, "HangarAux", "Alpha", "NormalColorAlpha")) {
			return 0;
		}
	}
	Ht_Color(id, "LittleShipAux", aux);
	Ht_Color(id, "nAux", aux);
	nativeVal_t e = { 0 }, k = { 0 }, x = { 0 }, cur = { 0 };
	byte ok = 1;
	for (int n = 1; ok && n <= 9; n++) {
		char hangar[0x10], little[0x20], num[0x10];
		Str_Printf(hangar, sizeof(hangar), "Hangar%d", n);
		Str_Printf(little, sizeof(little), "LittleShip%d", n);
		Str_Printf(num, sizeof(num), "n%d", n);
		Native_Free(&e);
		int none = Ht_AtI("editedShips", n - 1, &e) ? Mode_Cmp(&e, "==", &ht_none) : -1;
		int exists = none < 0 ? -1 : !none;
		if (exists < 0) {
			ok = 0;
			break;
		}
		Script_SetNum(id, hangar, "Visible", 1);
		Script_SetNum(id, hangar, "Red", 255);
		Script_SetNum(id, hangar, "Green", 255);
		Script_SetNum(id, hangar, "Blue", 255);
		if (exists) {
			ok = Ht_Hint(id, hangar, "HINT_Hangar10", 1);
		} else {
			int sput = Mode_ModTrue(SE, "fromSputnik");
			int c = sput == 0 ? Ht_GEq("currentHangar", n - 1) : 0;
			ok = sput >= 0 && c >= 0 && Ht_Hint(id, hangar, sput ? "HINT_Hangar19" : c ? "HINT_Hangar11" : "HINT_Hangar10",
				!sput && !c);
		}
		Native_Free(&cur);
		int c = ok && Ht_G("currentHangar", &cur) ? Mode_Cmp(Mode_IntVal(&k, n - 1), "==", &cur) : -1;
		if (c < 0) {
			ok = 0;
			break;
		}
		Script_SetNum(id, hangar, "Visible", 1);
		Script_SetNum(id, hangar, "SpriteIndex", c ? 2 : 1);
		if (!Ht_SetG(id, hangar, "Alpha", c ? "SelectedColorAlpha" : "NormalColorAlpha")) {
			ok = 0;
			break;
		}
		Script_SetNum(id, little, "Visible", 1);
		Script_SetNum(id, little, "SpriteIndex", 4);
		Ht_Color(id, little, c);
		Script_SetNum(id, num, "Visible", 1);
		Ht_Color(id, num, c);
		Script_SetNum(id, little, "Visible", exists);
		if (!exists) {
			continue;
		}
		Native_Free(&x);
		int lost = Ht_AtI("destroyedShips", n - 1, &x) ? LvN_EqI(&x, 1) : -1;
		if (lost > 0) {
			lost = Ht_Single();
		}
		if (lost < 0) {
			ok = 0;
		} else if (lost) {
			Script_SetNum(id, little, "Visible", 1);
			Ht_Color(id, little, 2);
			Ht_Color(id, num, 2);
			int sput = Mode_ModTrue(SE, "fromSputnik");
			int here = sput == 0 ? Ht_GEq("currentHangar", n - 1) : 0;
			ok = sput >= 0 && here >= 0 && Ht_Hint(id, hangar, sput || here ? "HINT_Hangar13" : "HINT_Hangar10", !sput && !here);
		} else {
			Native_Free(&e);
			int valid = Ht_AtI("editedShips", n - 1, &e) ? Ht_Valid(&e) : -1;
			int here = valid >= 0 ? Ht_GEq("currentHangar", n - 1) : -1;
			int sput = here >= 0 ? Mode_ModTrue(SE, "fromSputnik") : -1;
			if (sput < 0) {
				ok = 0;
			} else if (sput) {
				ok = Ht_Hint(id, hangar, valid ? "HINT_Hangar20" : "HINT_Hangar21", 0);
			} else {
				ok = Ht_Hint(id, hangar, here ? (valid ? "HINT_Hangar15" : "HINT_Hangar14") : "HINT_Hangar10", !here);
			}
		}
	}
	Native_Free(&e);
	Native_Free(&x);
	Native_Free(&cur);
	return ok;
}

/* ShowHangarPanel(id): the items of the Hangar panel show; Sell works for an intact ship of a hangar */
static byte Ht_ShowHangarPanel(int id) {
	if (!Mode_Import(SE) || !Mode_Import("ShipMaker") || !Sn_From("LinkUD") || !Sn_From("LinkLR") || !Ht_PilotVisible(id, 1)) {
		return 0;
	}
	Ht_Show(id, ht_pilot, 1);
	int p = Ht_Pilots();
	if (p < 0 || (p && (!Sn_Link("LinkLR", id, "PrevPilot", "NextPilot") || !Sn_Link("LinkUD", id, "NextPilot", "PilotName") ||
		!Sn_Link("LinkUD", id, "PrevPilot", "PilotName") || !Sn_Link("LinkUD", id, "PilotName", "Sell") ||
		!Sn_Link("LinkUD", id, "PilotName", "Exit")))) {
		return 0;
	}
	int sput = Mode_ModTrue(SE, "fromSputnik");
	if (sput < 0) {
		return 0;
	}
	if (!sput) {
		Ht_Show(id, ht_sell, 1);
	}
	Ht_SellActive(id, 0);
	nativeVal_t x;
	int c = Mode_ModTrue(SE, "fromSputnik");
	c = c < 0 ? -1 : !c;
	if (c > 0) {
		nativeVal_t cur = { 0 };
		c = Ht_G("currentHangar", &cur) ? Mode_CmpI(&cur, ">", -1) : -1;
		Native_Free(&cur);
	}
	if (c > 0) {
		nativeVal_t cur = { 0 };
		c = Ht_G("currentHangar", &cur) ? Mode_CmpI(&cur, "<", 9) : -1;
		Native_Free(&cur);
	}
	if (c > 0) {
		c = Ht_AtCmp("editedShips", "currentHangar", "!=", &ht_none);
	}
	if (c > 0) {
		c = Ht_AtCmp("destroyedShips", "currentHangar", "!=", Mode_IntVal(&x, 1));
	}
	if (c < 0) {
		return 0;
	}
	if (c) {
		Ht_SellActive(id, 1);
		if (!Sn_Link("LinkLR", id, "Sell", "Exit")) {
			return 0;
		}
	}
	return Ht_UpdateHangars(id);
}

/* eval(text) in a function of HangarTab (its globals, locals the variables of the function in x); a name with
 * attributes and subscripts ("prevShip.Model", "ShipEdit.editedShips[3].NEng0", all the game passes) is looked up
 * directly (Mode_EvalChain) */
static byte Ht_Eval(const nativeVal_t *text, const execScope_t *x, nativeVal_t *out) {
	Native_SetNone(out);
	if (text->type == NV_STR && strlen(Native_Text(text)) == (size_t)text->n) {
		const char *s = Native_Text(text);
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		int found = Mode_EvalChain(x, s, out);
		if (found >= 0) {
			return found;
		}
	}
	/* another expression (only a caller of ComputeShipTotalCost from the console) is Script_NotRead's error */
	Script_NotRead("ComputeShipTotalCost", Native_Text(text));
	return 0;
}

/* eval(cad + attr) */
static byte Ht_EvalAttr(const nativeVal_t *cad, const char *attr, const execScope_t *locals, nativeVal_t *out) {
	nativeVal_t s = { 0 };
	byte ok = Sn_Cat("", cad, attr, &s) && Ht_Eval(&s, locals, out);
	Native_Free(&s);
	return ok;
}

/* eval(cad + attr + v) */
static byte Ht_EvalAttrV(const nativeVal_t *cad, const char *attr, const nativeVal_t *v, const execScope_t *locals,
	nativeVal_t *out) {
	nativeVal_t s = { 0 }, s2 = { 0 };
	byte ok = Sn_Cat("", cad, attr, &s) && Native_Arith('+', &s, v, &s2) && Ht_Eval(&s2, locals, out);
	Native_Free(&s);
	Native_Free(&s2);
	return ok;
}

/* ComputeShipTotalCost(cad="prevShip"): the price of the ship cad names (an expression the function evaluates): the
 * model, its engines, its weapons and the armour above the minimum */
static byte Ht_TotalCost(const nativeVal_t *cad, nativeVal_t *out) {
	static const char *const mods[] = { SE, "ShipMaker", "string" };
	static const char *const slots[] = { "A", "B", "C", "D" };
	static const char *const weap[][2] = { { "Vulcan", "00" }, { "Devastator", "03" }, { "Tesla", "02" }, { "ATPC", "04" },
		{ "Inferno", "05" }, { "Swarm", "01" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *price = Mode_V(&t), *cost = Mode_V(&t), *layouts = Mode_V(&t), *layout = Mode_V(&t);
	nativeVal_t *num = Mode_V(&t), *eng = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *wid = Mode_V(&t);
	nativeVal_t *ids = Mode_V(&t), *key = Mode_V(&t), *val = Mode_V(&t), *up = Mode_V(&t), *costs = Mode_V(&t);
	Native_SetNone(out);
	for (int i = 0; i < 3; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	/* the locals of the function as the name chain reads them: cad and the modules it imported */
	execScope_t scope = { .module = HT };
	const execScope_t *loc = &scope;
	Exec_Var(&scope, "cad", cad);
	for (int i = 0; i < 3; i++) {
		Exec_Var(&scope, mods[i], NULL);
	}
	byte ok = Ht_EvalAttr(cad, ".Model", loc, name) && PORT_CALL(ShipMaker, GetShipPrice, price, "v", name) &&
		Native_ToInt(price, cost) && LvN_ModGet(SE, "shipsEnginesLayouts", layouts) && Mode_Sub(layouts, name, layout);
	int n = ok ? Mode_Len(layout) : -1;
	ok = n >= 0;
	Native_Free(price);
	for (int i = 0; ok && i < n; i++) {
		Mode_Repr(Mode_IntVal(key, i), a);
		ok = Ht_EvalAttrV(cad, ".NEng", a, loc, num);
		int more = ok ? Mode_CmpI(num, ">", 0) : -1;
		if (more > 0) {
			ok = Ht_EvalAttrV(cad, ".MEng", a, loc, eng) && PORT_CALL(ShipMaker, GetEnginePrice, price, "v", eng) &&
				Mode_Arith('*', num, price, p) && Mode_Arith('+', cost, p, b);
			if (ok) {
				Native_Free(cost);
				*cost = *b;
				Native_SetNone(b);
			}
		}
		ok = ok && more >= 0;
		Native_Free(a);
		Native_Free(num);
		Native_Free(eng);
		Native_Free(price);
		Native_Free(p);
	}
	/* slotId = ("A", "B", "C", "D"); weapId = {...}: the keys in the order of the 1.5.2 dict */
	Native_SetEmpty(ids, NV_DICT);
	for (int i = 0; ok && i < 6; i++) {
		Native_SetStr(key, weap[i][0]);
		Native_SetStr(val, weap[i][1]);
		ok = Native_DictSet(ids, key, val);
	}
	const nativeVal_t *k, *v;
	for (int pos = 0; ok && Native_DictNext(ids, &pos, &k, &v);) {
		for (int i = 0; ok && i < 2; i++) {
			Native_SetStr(a, slots[i]);
			ok = Native_Arith('+', a, v, wid) && Ht_EvalAttrV(cad, ".Weap", wid, loc, num);
			if (ok && Native_True(num)) {
				ok = LvN_ModGet("ShipMaker", "WeaponCosts", costs) && Native_Upper(k, up) && Mode_Sub(costs, up, p) &&
					Mode_Arith('+', cost, p, b);
				if (ok) {
					Native_Free(cost);
					*cost = *b;
					Native_SetNone(b);
				}
			}
			Native_Free(wid);
			Native_Free(num);
			Native_Free(costs);
			Native_Free(up);
			Native_Free(p);
		}
	}
	ok = ok && Ht_EvalAttr(cad, ".MaxLife", loc, a) && Ht_EvalAttr(cad, ".MinLife", loc, b) && Mode_Arith('-', a, b, num) &&
		LvN_ModGet("ShipMaker", "armorPieceCost", costs) && Mode_Arith('*', num, costs, p) && Mode_Arith('+', cost, p, out);
	return Mode_Done(&t, ok);
}

/* ComputeShipTotalCost(cad) as the script calls its global (a mission may replace it) */
static byte Ht_OwnTotalCost(const nativeVal_t *cad, nativeVal_t *out) {
	return PORT_OWN(HangarTab, ComputeShipTotalCost, out, "v", cad);
}

/* "ShipEdit.editedShips[" + `n` + "]" */
static byte Ht_ShipText(const nativeVal_t *n, nativeVal_t *out) {
	nativeVal_t r = { 0 };
	Mode_Repr(n, &r);
	byte ok = Sn_Cat("ShipEdit.editedShips[", &r, "]", out);
	Native_Free(&r);
	return ok;
}

/* the money prediction: the text sign + `cost`, red (Green 0) when money - cost < 0 (money NULL: never red) */
static byte Ht_Predict(int id, const char *sign, const nativeVal_t *cost, const nativeVal_t *money) {
	nativeVal_t d = { 0 }, r = { 0 }, s = { 0 };
	Script_SetNum(id, "MoneyPredict", "Green", 255);
	int red = money ? (Mode_Arith('-', money, cost, &d) ? Mode_CmpI(&d, "<", 0) : -1) : 0;
	if (red > 0) {
		Script_SetNum(id, "MoneyPredict", "Green", 0);
	}
	byte ok = red >= 0;
	if (ok) {
		Mode_Repr(cost, &r);
		ok = Sn_Cat(sign, &r, NULL, &s) && Sn_SetC(id, "MoneyPredict", "Text", &s);
	}
	if (ok) {
		Script_SetNum(id, "MoneyPredict", "Visible", 1);
	}
	Native_Free(&d);
	Native_Free(&r);
	Native_Free(&s);
	return ok;
}

/* int(control[-1]) - 1 as the script takes the hangar from the control name, len(control) - 1 first */
static byte Ht_ControlHangar(const nativeVal_t *control, nativeVal_t *out) {
	nativeVal_t c, ch = { 0 }, i = { 0 }, one;
	Native_Copy(&c, control);
	int l = Mode_Len(&c);
	Native_SetInt(&one, 1);
	byte ok = l >= 0 && LvN_Item(&c, l - 1, &ch) && Native_ToInt(&ch, &i) && Mode_Arith('-', &i, &one, out);
	Native_Free(&c);
	Native_Free(&ch);
	Native_Free(&i);
	return ok;
}

/* SScorer.Get(id, item, a) + SScorer.Get(id, item, b) / 2 (half 0: + SScorer.Get(id, item, b)) */
static byte Ht_Mid(int id, const char *item, const char *a, const char *b, byte half, nativeVal_t *out) {
	nativeVal_t x = { 0 }, w = { 0 }, h = { 0 }, two;
	Native_SetInt(&two, 2);
	Mode_ScorerGet(id, item, a, &x);
	Mode_ScorerGet(id, item, b, &w);
	byte ok = half ? Mode_Arith('/', &w, &two, &h) && Mode_Arith('+', &x, &h, out) : Mode_Arith('+', &x, &w, out);
	Native_Free(&x);
	Native_Free(&w);
	Native_Free(&h);
	return ok;
}

/* ShowShipMiniInfo(id, control, lostFocus): the mini information of the hangar under the cursor, linked to it by the
 * lines; the texts of an available hangar, a destroyed or an incomplete ship */
static byte Ht_ShowMini(int id, const nativeVal_t *control) {
	static const char *const show[] = { "MiniMaxSpeedText", "MiniMaxSpeed", "MiniSpeedRamp1", "MiniSpeedRamp2",
		"MiniBoostRechargeTime", "MiniBoostRechargeTimeText", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *i = Mode_V(&t), *mini = Mode_V(&t), *v = Mode_V(&t), *w = Mode_V(&t), *x = Mode_V(&t), *off = Mode_V(&t);
	nativeVal_t *o0 = Mode_V(&t), *o1 = Mode_V(&t), *hname = Mode_V(&t), *sx = Mode_V(&t), *sy = Mode_V(&t);
	nativeVal_t *ex = Mode_V(&t), *ey = Mode_V(&t), *k = Mode_V(&t), *cur = Mode_V(&t);
	if (!Mode_Import(SE) || !Mode_Import("ShipMaker") || !Mode_Import("string")) {
		return Mode_Done(&t, 0);
	}
	int isAux = LvN_EqS(control, "HangarAux");
	if (isAux) {
		int mp = Sn_Multi(0);
		if (mp < 0) {
			return Mode_Done(&t, 0);
		}
		if (!mp) {
			Script_SetNum(id, "AuxShipText", "Visible", 1);
		}
		if (!Ht_AtI("editedShips", 9, v) || !Mode_Put(HT, "miniShip", NULL, v)) {
			return Mode_Done(&t, 0);
		}
		int tr = LvN_GTrue(HT, "miniShip");
		int stop = tr < 0 ? -1 : !tr;
		if (stop == 0) {
			stop = Ht_GEq("currentHangar", 9);
		}
		if (stop != 0) {
			return Mode_Done(&t, stop > 0);
		}
	} else {
		Script_SetNum(id, "AuxShipText", "Visible", 0);
		if (!Ht_ControlHangar(control, i) || !LvN_ModGet(SE, "editedShips", w) || !Mode_Sub(w, i, v) ||
			!Mode_Put(HT, "miniShip", NULL, v)) {
			return Mode_Done(&t, 0);
		}
		int c = Ht_G("currentHangar", cur) ? Mode_Cmp(i, "==", cur) : -1;
		if (c != 0) {
			return Mode_Done(&t, c > 0);
		}
	}
	Script_SetNum(id, "MiniShipInfo", "Visible", 1);
	int has = LvN_GTrue(HT, "miniShip");
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	if (has) {
		Script_SetNum(id, "MiniMaxSpeedText", "Visible", 0);
		Script_SetNum(id, "MiniMaxSpeed", "Visible", 0);
		Native_Free(v);
		Native_Free(w);
		byte ok = Ht_G("miniShip", mini) && Mode_VGet(mini, "Name", v) && Sn_SetC(id, "HangarShip", "TargetName", v);
		if (ok) {
			Script_SetNum(id, "HangarShip", "Visible", 1);
		}
		Native_Free(v);
		Native_Free(mini);
		ok = ok && Ht_G("miniShip", mini) && Mode_VGet(mini, "MaxSpeed", v) && Mode_Arith('/', v, Mode_IntVal(k, 28), w) &&
			Ht_SetReprInt(id, "MiniMaxSpeed", "Text", w);
		if (ok) {
			Script_SetNum(id, "MiniHullText", "Visible", 1);
		}
		Native_Free(v);
		Native_Free(mini);
		ok = ok && Ht_G("miniShip", mini) && Mode_VGet(mini, "MaxLife", v) && Ht_SetRepr(id, "MiniHullValue", "Text", v);
		if (ok) {
			Script_SetNum(id, "MiniHullValue", "Visible", 1);
			Script_SetNum(id, "MiniWeight", "Visible", 1);
		}
		Native_Free(v);
		Native_Free(mini);
		ok = ok && Ht_G("miniShip", mini) && Mode_VGet(mini, "AcMass", v) && Ht_SetReprInt(id, "MiniCurrentWeight", "Text", v);
		if (ok) {
			Script_SetNum(id, "MiniCurrentWeight", "Visible", 1);
		}
		Native_Free(v);
		Native_Free(w);
		Native_Free(mini);
		ok = ok && LvN_ModGet("ShipMaker", "ShipTon", x) && Ht_G("miniShip", mini) && Mode_VGet(mini, "Model", v) &&
			Native_Upper(v, w) && Mode_Sub(x, w, o0) && LvN_Item(o0, 2, o1) && Ht_SetReprInt(id, "MiniMaxWeight", "Text", o1);
		if (ok) {
			Script_SetNum(id, "MiniMaxWeight", "Visible", 1);
			Script_SetNum(id, "MiniSlashWeight", "Visible", 1);
		}
		Native_Free(v);
		Native_Free(w);
		Native_Free(mini);
		Native_Free(o0);
		Native_Free(o1);
		ok = ok && Ht_G("miniShip", mini) && Mode_VGet(mini, "BoostTime", v) && Ht_BoostText(v, w) &&
			Sn_SetC(id, "MiniBoostRechargeTime", "Text", w);
		Native_Free(mini);
		int valid = ok && Ht_G("miniShip", mini) ? Ht_Valid(mini) : -1;
		if (valid < 0) {
			return Mode_Done(&t, 0);
		}
		if (valid) {
			Ht_Show(id, show, 1);
			Native_Free(mini);
			ok = Ht_G("miniShip", mini) && PORT_OWN(HangarTab, GetSpeedRampOffsets, off, "v", mini) && LvN_Item(off, 0, o0) &&
				LvN_Item(off, 1, o1) && PORT_OWN(HangarTab, ScaleSpeedRamp, NULL, "ivvi", (long)id, o0, o1, 1L);
			if (!ok) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (isAux) {
		Native_SetStr(hname, "HangarAux");
	} else {
		Native_Free(v);
		Native_Free(w);
		if (!Mode_Arith('+', i, Mode_IntVal(k, 1), w)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(w, v);
		if (!Sn_Cat("Hangar", v, NULL, hname)) {
			return Mode_Done(&t, 0);
		}
	}
	const char *hn = Native_Text(hname);
	if (!Ht_Mid(id, hn, "X", "W", 1, sx) || !Ht_Mid(id, hn, "Y", "H", 1, sy) || !Ht_Mid(id, "MiniShipInfo", "X", "W", 1, ex) ||
		!Ht_Mid(id, "MiniShipInfo", "Y", "H", 0, ey)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Native_Free(w);
	if (!Sn_SetC(id, "VLine", "X", ex) || !Sn_SetC(id, "VLine", "Y", ey)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "VLine", "ScaleX", 2);
	if (!Mode_Arith('-', sy, ey, v) || !Mode_Arith('+', v, Mode_IntVal(k, 2), w) || !Sn_SetC(id, "VLine", "ScaleY", w)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "VLine", "Visible", 1);
	int invert = Mode_Cmp(sx, ">", ex);
	if (invert < 0) {
		return Mode_Done(&t, 0);
	}
	if (invert) {
		nativeVal_t tmp = *ex;
		*ex = *sx;
		*sx = tmp;
	}
	Native_Free(v);
	if (!Sn_SetC(id, "HLine", "X", sx) || !Sn_SetC(id, "HLine", "Y", sy) || !Mode_Arith('-', ex, sx, v) ||
		!Sn_SetC(id, "HLine", "ScaleX", v)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "HLine", "ScaleY", 2);
	Script_SetNum(id, "HLine", "Visible", 1);
	Native_Free(w);
	if (!Sn_SetC(id, "MLine", "X", invert ? ex : sx) || !Mode_Arith('+', sy, Mode_IntVal(k, 1), w) || !Sn_SetC(id, "MLine", "Y", w)) {
		return Mode_Done(&t, 0);
	}
	Script_SetNum(id, "MLine", "Visible", 1);
	if (isAux) {
		return Mode_Done(&t, 1);
	}
	Native_Free(mini);
	int valid = Ht_G("miniShip", mini) ? Ht_Valid(mini) : -1;
	int ship = valid >= 0 ? LvN_GTrue(HT, "miniShip") : -1;
	int lost = 0;
	if (ship > 0) {
		int mp = Sn_Multi(0);
		Native_Free(v);
		lost = mp == 0 ? (Sn_At(SE, "destroyedShips", i, v) ? LvN_EqI(v, 1) : -1) : mp < 0 ? -1 : 0;
	}
	if (ship < 0 || lost < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (!ship) {
		ok = Ht_Words(id, "ShipEdit_AvailableHangar", "AvailableHangarText_", 1);
	} else if (lost) {
		ok = Ht_Words(id, "ShipEdit_DestroyedShip", "DestroyedShipText_", 1);
	} else if (!valid) {
		ok = Ht_Words(id, "ShipEdit_UncompletedShip", "UncompletedShipText_", 1);
	}
	int mp = ok ? Sn_Multi(0) : -1;
	if (mp == 0) {
		/* the script compares destroyedShips[i] with 0 and does nothing with it */
		Native_Free(v);
		ok = Sn_At(SE, "destroyedShips", i, v);
	}
	return Mode_Done(&t, ok && mp >= 0);
}

/* ShowRebuildShipMoney(id, control, lostFocus): the cost of rebuilding the destroyed ship of the hangar (Sputnik's
 * screen), then its mini information */
static byte Ht_ShowRebuildShipMoney(int id, const nativeVal_t *control) {
	nativeVal_t money = { 0 }, n = { 0 }, text = { 0 }, cost = { 0 };
	if (!Mode_Import("SputnikInterface")) {
		return 0;
	}
	Script_SetNum(id, "MoneyPredict", "Green", 255);
	Native_SetInt(&money, Mode_GetMoney());
	byte ok = Ht_ControlHangar(control, &n) && Ht_ShipText(&n, &text) && Ht_OwnTotalCost(&text, &cost) &&
		Ht_Predict(id, "-", &cost, &money) && Mode_SetModVar("SputnikInterface", "rebuildCost", &cost) && Ht_ShowMini(id, control);
	Native_Free(&n);
	Native_Free(&text);
	Native_Free(&cost);
	return ok;
}

/* HideRebuildShipMoney(id, control, lostFocus) */
static byte Ht_HideRebuildShipMoney(int id) {
	Script_SetNum(id, "MoneyPredict", "Visible", 0);
	return Ht_HideMini(id);
}

/* the ship list of the type buttons: auxShipsToEditList, or shipsToEditList while it is empty; *rebuild: the current
 * entry is the destroyed ship */
static byte Ht_ShipsList(nativeVal_t *list, int *rebuild) {
	nativeVal_t aux = { 0 }, empty, x = { 0 }, cur = { 0 };
	*rebuild = 0;
	Native_SetEmpty(&empty, NV_LIST);
	int e = LvN_ModGet(SE, "auxShipsToEditList", &aux) ? Mode_Cmp(&aux, "==", &empty) : -1;
	Native_Free(&aux);
	if (e != 0) {
		return e > 0 && LvN_ModGet(SE, "shipsToEditList", list);
	}
	byte ok = LvN_ModGet(SE, "auxShipsToEditList", list) && Ht_G("currentShipToEdit", &cur) && Mode_Sub(list, &cur, &x);
	if (ok && LvN_EqS(&x, "Rebuild")) {
		*rebuild = 1;
	}
	Native_Free(&x);
	Native_Free(&cur);
	return ok;
}

/* list[currentShipToEdit] */
static byte Ht_AtShip(const nativeVal_t *list, nativeVal_t *out) {
	nativeVal_t cur = { 0 };
	Native_SetNone(out);
	byte ok = Ht_G("currentShipToEdit", &cur) && Mode_Sub(list, &cur, out);
	Native_Free(&cur);
	return ok;
}

/* int(ShipMaker.GetShipPrice(list[currentShipToEdit])) */
static byte Ht_ShipPrice(const nativeVal_t *list, nativeVal_t *out) {
	nativeVal_t kind = { 0 }, p = { 0 };
	byte ok = Ht_AtShip(list, &kind) && PORT_CALL(ShipMaker, GetShipPrice, &p, "v", &kind) && Native_ToInt(&p, out);
	Native_Free(&kind);
	Native_Free(&p);
	return ok;
}

/* AuxShowMoney(id, symbol): the money a new ship costs ("-") or a sold or rebuilt one gives or costs (its whole
 * price, shipTotalCost) */
static byte Ht_AuxShowMoney(int id, const char *symbol) {
	nativeVal_t money, list = { 0 }, cost = { 0 }, cad;
	if (!Mode_Import(SE) || !Mode_Import("ShipMaker") || !Mode_Import("string")) {
		return 0;
	}
	byte minus = !strcmp(symbol, "-");
	Native_SetInt(&money, Mode_GetMoney());
	int rebuild;
	byte ok = Ht_ShipsList(&list, &rebuild);
	if (ok && minus && !rebuild) {
		ok = Ht_ShipPrice(&list, &cost);
	} else if (ok) {
		int has = LvN_GTrue(HT, "prevShip");
		if (has == 0) {
			Native_Free(&list);
			return 1;
		}
		Native_SetStr(&cad, "prevShip");
		ok = has > 0 && Ht_TotalCost(&cad, &cost) && Mode_Put(HT, "shipTotalCost", NULL, &cost);
	}
	ok = ok && Ht_Predict(id, symbol, &cost, minus ? &money : NULL);
	Native_Free(&list);
	Native_Free(&cost);
	return ok;
}

/* ScaleSpeedRamp(id, offsetX, offsetY, isMiniShip=0): the speed ramp of the ship information (or of the mini
 * information): the time to the top speed (offsetX, at most 1) and the top speed (offsetY), both 0 to 1 */
static byte Ht_ScaleSpeedRamp(int id, const nativeVal_t *offX, const nativeVal_t *oy, const nativeVal_t *mini) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ox = Mode_V(&t), *one = Mode_V(&t), *two = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *c = Mode_V(&t);
	nativeVal_t *d = Mode_V(&t), *e = Mode_V(&t), *sx = Mode_V(&t), *sy = Mode_V(&t), *k = Mode_V(&t);
	Native_SetFloat(one, 1.0);
	int big = Mode_Cmp(offX, ">", one);
	if (big < 0) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(ox, big ? one : offX);
	int isMini = mini ? Native_True(mini) : 0;
	Native_SetInt(one, 1);
	Native_SetInt(two, 2);
	byte ok;
	/* the operands in the order Python evaluates them */
#define HT_OP(op, x, y, out) (Native_Free(out), Mode_Arith(op, x, y, out))
	if (isMini) {
		Native_SetFloat(sx, 20.0 / 32.0);
		Native_SetFloat(sy, 53.0 / 64.0);
		ok = Ht_G("baseMiniShipInfoX", a) && HT_OP('+', a, Mode_IntVal(k, 93), b) && Sn_SetC(id, "MiniSpeedRamp1", "X", b);
		Native_Free(a);
		ok = ok && Ht_G("baseMiniShipInfoY", a) && HT_OP('-', a, Mode_IntVal(k, 51), b) && HT_OP('-', one, oy, c);
		if (ok) {
			Native_Free(d);
			Mode_ScorerGet(id, "MiniSpeedRamp1", "H", d);
		}
		ok = ok && HT_OP('*', c, d, e) && HT_OP('+', b, e, a) && Sn_SetC(id, "MiniSpeedRamp1", "Y", a) &&
			HT_OP('*', two, ox, b) && HT_OP('*', b, sx, c) && Sn_SetC(id, "MiniSpeedRamp1", "ScaleX", c) &&
			HT_OP('*', oy, sy, c) && Sn_SetC(id, "MiniSpeedRamp1", "ScaleY", c);
		if (ok) {
			Native_Free(d);
			Mode_ScorerGet(id, "MiniSpeedRamp1", "W", d);
		}
		ok = ok && HT_OP('*', d, two, b) && HT_OP('*', b, ox, e);
		if (ok) {
			Native_Free(d);
			Mode_ScorerGet(id, "MiniSpeedRamp1", "X", d);
		}
		ok = ok && HT_OP('+', d, e, a) && Sn_SetC(id, "MiniSpeedRamp2", "X", a);
		if (ok) {
			Native_Free(d);
			Mode_ScorerGet(id, "MiniSpeedRamp1", "Y", d);
		}
		ok = ok && Sn_SetC(id, "MiniSpeedRamp2", "Y", d) && HT_OP('-', one, ox, a) && HT_OP('*', a, two, b) &&
			HT_OP('*', b, sx, c) && Sn_SetC(id, "MiniSpeedRamp2", "ScaleX", c) && HT_OP('*', oy, sy, c) &&
			Sn_SetC(id, "MiniSpeedRamp2", "ScaleY", c);
	} else {
		ok = Ht_G("baseShipInfoX", a) && Ht_G("speedRampX", b) && HT_OP('+', a, b, c) && Sn_SetC(id, "SpeedRamp1", "X", c);
		Native_Free(a);
		Native_Free(b);
		ok = ok && Ht_G("baseShipInfoY", a) && Ht_G("speedRampY", b) && HT_OP('+', a, b, c) && HT_OP('-', one, oy, d);
		if (ok) {
			Native_Free(e);
			Mode_ScorerGet(id, "SpeedRamp1", "H", e);
		}
		ok = ok && HT_OP('*', d, e, a) && HT_OP('+', c, a, b) && Sn_SetC(id, "SpeedRamp1", "Y", b) && HT_OP('*', two, ox, a) &&
			Sn_SetC(id, "SpeedRamp1", "ScaleX", a) && Sn_SetC(id, "SpeedRamp1", "ScaleY", oy) && HT_OP('-', one, ox, a) &&
			HT_OP('*', a, two, b) && Sn_SetC(id, "SpeedRamp2", "ScaleX", b) && Sn_SetC(id, "SpeedRamp2", "ScaleY", oy);
		if (ok) {
			Native_Free(c);
			Native_Free(d);
			Mode_ScorerGet(id, "SpeedRamp1", "W", c);
			Mode_ScorerGet(id, "SpeedRamp1", "ScaleX", d);
		}
		ok = ok && HT_OP('*', c, d, e);
		if (ok) {
			Native_Free(c);
			Mode_ScorerGet(id, "SpeedRamp1", "X", c);
		}
		ok = ok && HT_OP('+', c, e, a) && Sn_SetC(id, "SpeedRamp2", "X", a);
		if (ok) {
			Native_Free(c);
			Mode_ScorerGet(id, "SpeedRamp1", "Y", c);
		}
		ok = ok && Sn_SetC(id, "SpeedRamp2", "Y", c);
	}
#undef HT_OP
	return Mode_Done(&t, ok);
}

/* GetMaxSpeedTime(v): the time the ship v needs to reach its top speed */
static byte Ht_MaxSpeedTime(const nativeVal_t *v, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ft = Mode_V(&t), *neg = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *sq = Mode_V(&t), *k = Mode_V(&t);
	nativeVal_t *fric = Mode_V(&t), *max = Mode_V(&t), *in = Mode_V(&t), *root = Mode_V(&t), *num = Mode_V(&t);
	nativeVal_t *den = Mode_V(&t), *c = Mode_V(&t);
	Native_SetNone(out);
	if (!Mode_Import("math")) {
		return Mode_Done(&t, 0);
	}
	byte ok = Mode_VGet(v, "FrwTrust", ft) && Mode_Neg(ft, neg) && Mode_VGet(v, "FrwTrust", a) && Mode_VGet(v, "FrwTrust", b) &&
		Mode_Arith('*', a, b, sq);
	Native_Free(a);
	Native_Free(b);
	ok = ok && Mode_VGet(v, "FrwTrust", a) && Mode_Arith('*', Mode_IntVal(c, 4), a, b) && Mode_VGet(v, "Fric", fric) &&
		Mode_Arith('*', b, fric, k);
	Native_Free(b);
	Native_Free(fric);
	ok = ok && Mode_VGet(v, "MaxSpeed", max) && Mode_Arith('*', k, max, b) && Mode_Arith('+', sq, b, in) &&
		PORT_CALL(math, sqrt, root, "v", in) && Mode_Arith('+', neg, root, num);
	Native_Free(a);
	Native_Free(b);
	ok = ok && Mode_VGet(v, "FrwTrust", a) && Mode_Arith('*', Mode_IntVal(c, 2), a, b) && Mode_VGet(v, "Fric", fric) &&
		Mode_Arith('*', b, fric, den) && Mode_Arith('/', num, den, out);
	return Mode_Done(&t, ok);
}

/* GetSpeedRampOffsets(v): (GetMaxSpeedTime(v) / 0.3, v.MaxSpeed / 19600.0) */
static byte Ht_SpeedRampOffsets(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t m = { 0 }, a = { 0 }, s = { 0 }, b = { 0 }, k;
	Native_SetNone(out);
	Native_SetFloat(&k, 0.3);
	byte ok = PORT_OWN(HangarTab, GetMaxSpeedTime, &m, "v", v) && Mode_Arith('/', &m, &k, &a) && Mode_VGet(v, "MaxSpeed", &s);
	Native_SetFloat(&k, 19600.0);
	ok = ok && Mode_Arith('/', &s, &k, &b);
	if (ok) {
		Native_SetEmpty(out, NV_TUPLE);
		*Native_Append(out) = a;
		*Native_Append(out) = b;
	} else {
		Native_Free(&a);
		Native_Free(&b);
	}
	Native_Free(&m);
	Native_Free(&s);
	return ok;
}

/* CmSec2Value(val): cm/s to the speed unit of the screen, int(val/28) */
static byte Ht_CmSec2Value(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t k, d = { 0 };
	byte ok = Mode_Arith('/', v, Mode_IntVal(&k, 28), &d) && Native_ToInt(&d, out);
	Native_Free(&d);
	return ok;
}

/* AddReferenceShipControls(id): the mini information of a hangar: the words of the three hangar texts, the ship
 * window, the speed, boost, hull and weight, the lines to the hangar */
static byte Ht_Reference(int id) {
	static const char *const texts[][2] = { { "ShipEdit_UncompletedShip", "UncompletedShipText_" },
		{ "ShipEdit_DestroyedShip", "DestroyedShipText_" }, { "ShipEdit_AvailableHangar", "AvailableHangarText_" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *base = Mode_V(&t), *y = Mode_V(&t), *y2 = Mode_V(&t), *text = Mode_V(&t);
	nativeVal_t *words = Mode_V(&t), *w = Mode_V(&t), *name = Mode_V(&t), *k = Mode_V(&t);
	if (!Mode_Import("string") || !Ht_G("baseMiniShipInfoX", base) || !Mode_Arith('+', base, Mode_IntVal(k, 72), x)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	for (int i = 0; ok && i < 3; i++) {
		Native_Free(base);
		Native_Free(y);
		ok = Ht_G("baseMiniShipInfoY", base) && Mode_Arith('-', base, Mode_IntVal(k, 30), y);
		Native_Free(text);
		Native_Free(words);
		if (ok) {
			Mode_Lang(texts[i][0], text);
			ok = Native_Split(text, NULL, words);
		}
		for (int j = 0; ok && j < words->n; j++) {
			ok = Native_SeqItem(words, j, w) && Sn_Cat(texts[i][1], w, NULL, name);
			ok = ok && Sn_DefV(id, "word", HT, 4, "name", name, "word", w, "x", x, "y", y) &&
				Mode_Arith('+', y, Mode_IntVal(k, 22), y2);
			if (ok) {
				Native_Free(y);
				*y = *y2;
				Native_SetNone(y2);
			}
			Native_Free(w);
			Native_Free(name);
		}
	}
	return Mode_Done(&t, ok && Sn_DefV(id, "reference", HT, 0));
}

/* SetHangarPanel(id): the Hangar panel (the layout of shipedit.def): the mini information, the hint, the emergency
 * ship and the nine hangars, the ship information, the money, Sell and Exit, the pilots, the ship type buttons */
static byte Ht_SetHangarPanel(int id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mp = Mode_V(&t), *sput = Mode_V(&t), *aux9 = Mode_V(&t), *n = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *l = Mode_V(&t), *name = Mode_V(&t), *model = Mode_V(&t), *nick = Mode_V(&t);
	nativeVal_t *k = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import(SE) || !Mode_Import("ShipMaker") || !Sn_From("LinkLR") || !Sn_From("LinkUD") ||
		!Ht_Reference(id)) {
		return Mode_Done(&t, 0);
	}
	int m = Sn_Multi(0);
	int s = m >= 0 ? Mode_ModTrue(SE, "fromSputnik") : -1;
	int a = s >= 0 ? Ht_GEq("currentHangar", 9) : -1;
	if (a < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(mp, m);
	Native_SetInt(sput, s);
	Native_SetInt(aux9, a);
	byte ok = Sn_DefV(id, "panel", HT, 3, "mp", mp, "sputnik", sput, "aux9", aux9);
	for (int i = 0; ok && i < 3; i++) {
		for (int j = 0; ok && j < 3; j++) {
			Native_SetInt(n, i * 3 + j + 1);
			Native_SetInt(x, 52 + 42 * j);
			Native_SetInt(y, 204 + 42 * i);
			ok = Sn_DefV(id, "hangar", HT, 4, "n", n, "x", x, "y", y, "sputnik", sput);
		}
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(Mode_IntVal(k, Mode_GetMoney()), money);
	ok = Sn_DefV(id, "info", HT, 3, "mp", mp, "sputnik", sput, "money", money);
	int pilots = ok ? Ht_Pilots() : -1;
	if (pilots < 0) {
		return Mode_Done(&t, 0);
	}
	if (pilots) {
		int cnt = Ht_Pilots_();
		ok = cnt >= 0;
		for (int i = 0; ok && i < cnt; i++) {
			Native_Free(l);
			Native_Free(name);
			Native_Free(model);
			ok = LvN_ModGet("ShipMaker", "PilotsList", l) && LvN_Item(l, i, name);
			Native_Free(l);
			Native_Free(x);
			ok = ok && LvN_ModGet("ShipMaker", "PilotsList", l) && LvN_Item(l, i, x) && Native_Slice(x, 2, 0x7fffffff, model);
			ok = ok && Sn_DefV(id, "pilot", HT, 2, "name", name, "model", model);
		}
		ok = ok && Ht_PilotVisible(id, 1);
		Native_SetStr(nick, "");
		int p = ok ? Ht_Pilots() : -1;
		if (p > 0) {
			Native_Free(nick);
			ok = Native_GVarGet("PlayerName", nick);
			if (ok && LvN_EqS(nick, "")) {
				Native_Free(nick);
				Mode_Lang("ShipEdit_Pilot", nick);
			}
		}
		ok = ok && p >= 0 && Sn_DefV(id, "pilots", HT, 1, "nick", nick);
	}
	int noship = 0;
	if (ok && pilots) {
		noship = Ht_AtCmp("editedShips", "currentHangar", "==", &ht_none);
	}
	if (!ok || noship < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(n, pilots);
	Native_SetInt(x, noship);
	return Mode_Done(&t, Sn_DefV(id, "links", HT, 3, "mp", mp, "pilots", n, "noship", x));
}

static const char *const ht_speedInfo[] = { "MaxSpeedText", "MaxSpeed", "BoostRechargeTime", "BoostRechargeTimeUnits",
	"BoostRechargeTimeText", "SpeedRamp1", "SpeedRamp2", NULL };

/* the offsets of the speed ramp of the ship: ScaleSpeedRamp(id, *GetSpeedRampOffsets(ship)) through the globals */
static byte Ht_Ramp(int id, const nativeVal_t *ship) {
	nativeVal_t off = { 0 }, a = { 0 }, b = { 0 };
	byte ok = PORT_OWN(HangarTab, GetSpeedRampOffsets, &off, "v", ship) && LvN_Item(&off, 0, &a) && LvN_Item(&off, 1, &b) &&
		PORT_OWN(HangarTab, ScaleSpeedRamp, NULL, "ivv", (long)id, &a, &b);
	Native_Free(&off);
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* ShowShipInfo(id, showAmmoInfo=1): the information of the ship on the platform (of the selected hangar on Sputnik's
 * screen): speed, hull, weight, boost, the speed ramp for a valid ship, the weapon counts */
static byte Ht_ShowInfo(int id, int ammo) {
	static const char *const mods[] = { "ShipMaker", SE, HT, "string" };
	static const char *const items[][2] = { { "NumVulcans", "nWeap00" }, { "NumDevastators", "nWeap03" },
		{ "NumTeslas", "nWeap02" }, { "NumATPCs", "nWeap04" }, { "NumSwarmMissiles", "nWeap01" },
		{ "NumInfernoMissiles", "nWeap05" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *l = Mode_V(&t), *k = Mode_V(&t), *v = Mode_V(&t), *w = Mode_V(&t), *x = Mode_V(&t);
	nativeVal_t *ton = Mode_V(&t), *e = Mode_V(&t);
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	int sput = Mode_ModTrue(SE, "fromSputnik");
	byte ok = sput > 0 ? LvN_ModGet(SE, "editedShips", l) && LvN_ModGet(HT, "currentHangar", k) && Mode_Sub(l, k, ship) :
		sput == 0 && Ht_G("prevShip", ship);
	int c = ok ? Mode_Cmp(ship, "!=", &ht_none) : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	ok = LvN_SetI(ship, "Active", 1) && Mode_VGet(ship, "MaxSpeed", v) && PORT_OWN(HangarTab, CmSec2Value, w, "v", v) &&
		Ht_SetRepr(id, "MaxSpeed", "Text", w);
	if (ok) {
		Script_SetNum(id, "Slash", "Visible", 1);
	}
	Native_Free(v);
	Native_Free(w);
	ok = ok && Mode_VGet(ship, "MaxLife", v) && Ht_SetRepr(id, "Hull", "Text", v);
	Native_Free(v);
	ok = ok && Mode_VGet(ship, "AcMass", v) && Ht_SetReprInt(id, "CurrentWeight", "Text", v);
	Native_Free(v);
	ok = ok && LvN_ModGet("ShipMaker", "ShipTon", l) && Mode_VGet(ship, "Model", v) && Native_Upper(v, w) && Mode_Sub(l, w, ton) &&
		LvN_Item(ton, 2, x) && Ht_SetReprInt(id, "MaxWeight", "Text", x);
	Native_Free(v);
	Native_Free(w);
	ok = ok && Mode_VGet(ship, "BoostTime", v) && Ht_BoostText(v, w) && Sn_SetC(id, "BoostRechargeTime", "Text", w);
	c = ok && Ht_AtCur("editedShips", e) ? Ht_Valid(e) : -1;
	if (c > 0) {
		c = Ht_AtCur("destroyedShips", v) ? LvN_EqI(v, 0) : -1;
		if (c == 0) {
			c = Sn_Multi(0);
		}
	}
	if (c > 0) {
		Native_Free(e);
		c = Ht_AtCur("editedShips", e) && Ht_Ramp(id, e) ? 1 : -1;
	}
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	Ht_Show(id, ht_speedInfo, c);
	for (int i = 0; ammo && i < 6; i++) {
		Native_Free(v);
		if (!Mode_VGet(ship, items[i][1], v) || !Ht_SetReprInt(id, items[i][0], "Text", v)) {
			return Mode_Done(&t, 0);
		}
	}
	sput = Mode_ModTrue(SE, "fromSputnik");
	return Mode_Done(&t, sput == 0 || (sput > 0 && LvN_SetI(ship, "Active", 0)));
}

/* HideShipInfo(id): the ship information empties */
static void Ht_HideInfo(int id) {
	static const char *const text[] = { "MaxSpeed", NULL, "Hull", "CurrentWeight", "MaxWeight", NULL, "NumVulcans",
		"NumDevastators", "NumInfernoMissiles", "NumSwarmMissiles", "NumTeslas", "NumATPCs" };
	static const char *const hide[] = { NULL, "MaxSpeedText", NULL, NULL, NULL, "Slash" };
	static const char *const rest[] = { "SpeedRamp1", "SpeedRamp2", "BoostRechargeTime", "BoostRechargeTimeText", NULL };
	for (int i = 0; i < 12; i++) {
		if (text[i]) {
			Script_SetStr(id, text[i], "Text", "");
		} else {
			Script_SetNum(id, hide[i], "Visible", 0);
		}
	}
	Ht_Show(id, rest, 0);
}

/* SetShipTypeButtonsState(id, state): the ship type button buys ("Build") or rebuilds ("Rebuild") */
static byte Ht_TypeState(int id, byte rebuild) {
	Script_SetNum(id, "ShipType", "SpriteIndex", rebuild ? 14 : 11);
	Script_SetNum(id, "ShipType", "FocusSpriteIndex", rebuild ? 15 : 12);
	return Sn_SetUseC(id, "ShipType", "Hint", rebuild ? "HINT_Hangar7" : "HINT_Hangar6", HT);
}

/* the weapon sprites (on) or the ammo sprites (off) of the ship information */
static void Ht_WeaponSprites(int id, long on) {
	static const char *const weapons[] = { "VulcanSpr", "DevastatorSpr", "TeslaSpr", "ATPCSpr", "InfernoSpr", "SwarmSpr",
		NULL };
	static const char *const ammo[] = { "ProjectilesAmmo", "EnergyAmmo", "MissilesAmmo", NULL };
	Ht_Show(id, weapons, on);
	Ht_Show(id, ammo, !on);
}

/* the slot counts of the ship information empty */
static void Ht_NoSlots(int id) {
	Script_SetStr(id, "NumSlotsParticleWeapons", "Text", "");
	Script_SetStr(id, "NumSlotsEnergyWeapons", "Text", "");
	Script_SetStr(id, "NumSlotsMissileWeapons", "Text", "");
}

/* SSound.SetPS(name, var, d), SSound.SetPS(name, var, i) and SSound.SetPS(name, var, v) through its ref */
static byte Ht_SetPSd(const char *name, const char *var, double d) {
	return PORT_CALL(SSound, SetPS, NULL, "ssd", name, var, d);
}

static byte Ht_SetPSi(const char *name, const char *var, long i) {
	return PORT_CALL(SSound, SetPS, NULL, "ssi", name, var, i);
}

static byte Ht_SetPSv(const char *name, const char *var, const nativeVal_t *v) {
	return PORT_CALL(SSound, SetPS, NULL, "ssv", name, var, v);
}

/* the hologram sounds: the start sound at the camera and the loop (holograms) */
static byte Ht_HologramSound(void) {
	static const char *const xyz[] = { "X", "Y", "Z" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *cn = Mode_V(&t), *cam = Mode_V(&t), *pos = Mode_V(&t), *snd = Mode_V(&t), *vol = Mode_V(&t), *c = Mode_V(&t);
	if (!Mode_Put(HT, "holograms", NULL, Mode_IntVal(c, 1))) {
		return Mode_Done(&t, 0);
	}
	Mode_CamName(0, cn);
	Native_SetFloat(vol, 0.35);
	byte ok = Mode_EntitySeam(cn, cam) && Mode_VGet(cam, "camPos", pos) && Mode_ModVar("Sound", "HologramStartSound", snd) &&
		Mode_PlaySoundSeam(pos, snd, vol) && PORT_CALL(SSound, CreatePS, NULL, "s", "EditHologramLoopSound");
	Native_Free(snd);
	ok = ok && Mode_ModVar("Sound", "HologramLoopSound", snd) && Ht_SetPSv("EditHologramLoopSound", "Sound", snd);
	for (int i = 0; ok && i < 3; i++) {
		Native_Free(pos);
		Native_Free(c);
		ok = Mode_VGet(cam, "camPos", pos) && LvN_Item(pos, i, c) && Ht_SetPSv("EditHologramLoopSound", xyz[i], c);
	}
	ok = ok && Ht_SetPSi("EditHologramLoopSound", "Loop", 1) && Ht_SetPSd("EditHologramLoopSound", "Volume", 0.17);
	return Mode_Done(&t, ok);
}

/* SelectShipToEdit(id): the plan of the ship type to buy (a hologram on the platform) or the destroyed ship to
 * rebuild, its slots and information; the other tabs are off; the hologram sound starts */
static byte Ht_SelectShipToEdit(int id) {
	static const char *const mods[] = { SE, HT, "Sound", "string" };
	static const char *const off[] = { "MaxSpeedText", "MaxSpeed", "Slash", "BoostRechargeTime", "BoostRechargeTimeText",
		NULL };
	static const char *const slots[][2] = { { "NumSlotsParticleWeapons", "nSlot00" }, { "NumSlotsMissileWeapons", "nSlot01" },
		{ "NumSlotsEnergyWeapons", "nSlot02" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *list = Mode_V(&t), *kind = Mode_V(&t), *up = Mode_V(&t), *key = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *ship = Mode_V(&t), *v = Mode_V(&t);
	if (!Sn_From("LinkUD")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Ht_Show(id, off, 0);
	int rebuild = Ht_Single();
	if (rebuild > 0) {
		rebuild = LvN_ModGet(SE, "auxShipsToEditList", list) && Ht_AtShip(list, x) ? LvN_EqS(x, "Rebuild") : -1;
		Native_Free(list);
	}
	byte ok = rebuild >= 0 && LvN_ModGet(SE, rebuild ? "shipsToEditList" : "auxShipsToEditList", list) && Ht_AtShip(list, kind) &&
		Native_Upper(kind, up) && Sn_Cat("SHIP_", up, NULL, key) && Mode_LangV(key, text);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Ht_Show(id, ht_shipType, 1);
	int build = rebuild == 0 ? 1 : Sn_Multi(0);
	ok = Sn_SetC(id, "ShipTypeName", "Text", text) && build >= 0;
	if (ok && build) {
		Native_Free(list);
		Native_Free(kind);
		ok = LvN_ModGet(SE, "auxShipsToEditList", list) && Ht_AtShip(list, kind) &&
			PORT_OWN(HangarTab, CreateEmptyShip, NULL, "v", kind) && Ht_TypeState(id, 0);
	} else if (ok) {
		ok = Ht_AtCur("editedShips", ship) && Mode_Put(HT, "prevShip", NULL, ship);
		Native_Free(ship);
		ok = ok && Ht_G("prevShip", ship) && LvN_SetI(ship, "Active", 1);
		Native_Free(ship);
		ok = ok && Ht_G("prevShip", ship) && LvN_SetI(ship, "RenderEffect", 1) &&
			PORT_CALL(ShipEdit, StopRotateLoop, NULL, "") && PORT_CALL(ShipEdit, StartRotateLoop, NULL, "") &&
			Ht_TypeState(id, 1);
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Ht_WeaponSprites(id, 0);
	for (int i = 0; i < 3; i++) {
		Native_Free(v);
		if (!Ht_ShipGet(slots[i][1], v) || !Ht_SetReprInt(id, slots[i][0], "Text", v)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Ht_ShowInfo(id, 0) || !Ht_TabsOn(id, 0)) {
		return Mode_Done(&t, 0);
	}
	int h = LvN_GTrue(HT, "holograms");
	return Mode_Done(&t, h > 0 || (h == 0 && Ht_HologramSound()));
}

/* PrevShipToEdit(id, control), NextShipToEdit(id, control): the ship type before or after the current one */
static byte Ht_ShipToEdit(int id, int step) {
	nativeVal_t list = { 0 }, x = { 0 }, cur = { 0 }, ship = { 0 };
	if (!Mode_Import(SE)) {
		return 0;
	}
	int rebuild = LvN_ModGet(SE, "auxShipsToEditList", &list) && Ht_AtShip(&list, &x) ? LvN_EqS(&x, "Rebuild") : -1;
	Native_Free(&list);
	Native_Free(&x);
	if (rebuild < 0 || !Mode_GOpI(HT, "currentShipToEdit", step < 0 ? '-' : '+', 1) || !Ht_G("currentShipToEdit", &cur)) {
		Native_Free(&cur);
		return 0;
	}
	int c;
	if (step < 0) {
		c = Mode_CmpI(&cur, "<", 0);
		int n = c > 0 ? (LvN_ModGet(SE, "auxShipsToEditList", &list) ? Mode_Len(&list) : -1) : 0;
		c = n < 0 ? -1 : c;
		Native_SetInt(&x, n - 1);
	} else {
		int n = LvN_ModGet(SE, "auxShipsToEditList", &list) ? Mode_Len(&list) : -1;
		c = n < 0 ? -1 : LvN_EqI(&cur, n);
		Native_SetInt(&x, 0);
	}
	Native_Free(&list);
	Native_Free(&cur);
	byte ok = c >= 0 && (c == 0 || Mode_Put(HT, "currentShipToEdit", NULL, &x));
	if (ok && rebuild == 0) {
		c = Ht_TypeState(id, 0) && Ht_G("prevShip", &ship) ? Mode_Cmp(&ship, "!=", &ht_none) : -1;
		ok = c == 0 || (c > 0 && Mode_VDel(&ship) && Mode_Put(HT, "prevShip", NULL, &ht_none));
	} else if (ok) {
		ok = Ht_TypeState(id, 1) && Ht_G("prevShip", &ship) && LvN_SetI(&ship, "Active", 0);
	}
	Native_Free(&ship);
	if (!ok || !Ht_SelectShipToEdit(id)) {
		return 0;
	}
	c = Ht_G("prevShip", &ship) ? Mode_Cmp(&ship, "!=", &ht_none) : -1;
	Native_Free(&ship);
	return c == 0 || (c > 0 && PORT_CALL(ShipEdit, StartRotateLoop, NULL, ""));
}

/* the hangar of the global name has no ship, or (single player) a destroyed one: 1, 0, -1 */
static int Ht_Empty(const char *name) {
	nativeVal_t x;
	int c = Ht_AtCmp("editedShips", name, "==", &ht_none);
	if (c == 0) {
		c = Ht_AtCmp("destroyedShips", name, "==", Mode_IntVal(&x, 1));
		if (c > 0) {
			c = Ht_Single();
		}
	}
	return c;
}

/* SScorer.Set(id, control, "Hint", Scrap.GetLangStr(key)) */
static byte Ht_ControlHint(int id, const nativeVal_t *control, const char *key) {
	nativeVal_t text = { 0 };
	Mode_Lang(key, &text);
	byte ok = Sn_Set(id, control, "Hint", &text);
	Native_Free(&text);
	return ok;
}

/* SScorer.SetDefault(id, "Hangar" + `currentHangar + 1`) */
static byte Ht_DefaultHangar(int id) {
	nativeVal_t cur = { 0 }, n = { 0 }, r = { 0 }, s = { 0 }, k;
	byte ok = Ht_G("currentHangar", &cur) && Mode_Arith('+', &cur, Mode_IntVal(&k, 1), &n);
	if (ok) {
		Mode_Repr(&n, &r);
		ok = Sn_Cat("Hangar", &r, NULL, &s) && PORT_CALL(SScorer, SetDefault, NULL, "iv", (long)id, &s);
	}
	Native_Free(&cur);
	Native_Free(&n);
	Native_Free(&r);
	Native_Free(&s);
	return ok;
}

/* Menu.LinkLR(id, a, b) of the hangars at the right side and b */
static byte Ht_RightSide(int id, const char *b) {
	return Sn_Link("LinkLR", id, "Hangar3", b) && Sn_Link("LinkLR", id, "Hangar9", b) && Sn_Link("LinkLR", id, "Hangar6", b);
}

/* SelectHangar(id, control="", skipanim=0): the hangar currentHangar is selected: its ship comes up on the platform
 * (the animation, or at once with skipanim), an empty or destroyed one shows the ship plans when the platform is up */
static byte Ht_SelectHangar(int id, const nativeVal_t *control, int skipanim) {
	static const char *const links[] = { "LinkLR", "LinkUD", "UnLinkUD", "UnLinkLR" };
	nativeVal_t a = { 0 }, b = { 0 }, x;
	for (int i = 0; i < 4; i++) {
		if (!Sn_From(links[i])) {
			return 0;
		}
	}
	if (!Mode_Import(SE) || !Ht_SetPSd("EditHologramLoopSound", "Volume", 0.0)) {
		return 0;
	}
	int c = Ht_G("prevHangar", &a) && Ht_G("currentHangar", &b) ? Mode_Cmp(&a, "==", &b) : -1;
	Native_Free(&a);
	Native_Free(&b);
	if (c != 0) {
		return c > 0;
	}
	c = Ht_AtG("editedShips", "prevHangar", &a) ? !Native_True(&a) : -1;
	Native_Free(&a);
	if (c == 0) {
		c = Ht_AtCmp("destroyedShips", "prevHangar", "==", Mode_IntVal(&x, 1));
		if (c > 0) {
			c = Ht_Single();
		}
	}
	if (c > 0) {
		c = Ht_G("prevShip", &a) ? Mode_Cmp(&a, "!=", &ht_none) : -1;
		c = c > 0 ? (LvN_SetI(&a, "Active", 0) ? 1 : -1) : c;
		Native_Free(&a);
	}
	if (c < 0) {
		return 0;
	}
	Ht_Show(id, ht_shipType, 0);
	int pilots = Ht_Pilots();
	if (pilots > 0) {
		int none = Ht_AtCmp("editedShips", "currentHangar", "==", &ht_none);
		if (none < 0 || (none > 0 && !Sn_Link("LinkLR", id, "NextShip", "PrevPilot"))) {
			return 0;
		}
		if (!none) {
			Script_SetStr(id, "PrevPilot", "Left", "Hangar6");
		}
		if (!Sn_Link("UnLinkUD", id, "PilotName", "Sell") || !Sn_Link("LinkUD", id, "PilotName", "Exit")) {
			return 0;
		}
		Script_SetStr(id, "Exit", "Left", "");
		Script_SetStr(id, "Exit", "Up", "PilotName");
	} else if (pilots < 0 || !Sn_Link("LinkLR", id, "NextShip", "Exit")) {
		return 0;
	}
	int empty = Ht_Empty("currentHangar");
	if (empty > 0) {
		static const char *const nums[] = { "NumVulcans", "NumDevastators", "NumInfernoMissiles", "NumSwarmMissiles",
			"NumTeslas", "NumATPCs" };
		static const char *const texts[] = { "MaxSpeed", "Hull", "CurrentWeight", "MaxWeight" };
		for (int i = 0; i < 6; i++) {
			Script_SetStr(id, nums[i], "Text", "");
		}
		c = Ht_AtCmp("editedShips", "prevHangar", "!=", &ht_none);
		if (c < 0) {
			return 0;
		}
		if (c) {
			for (int i = 0; i < 4; i++) {
				Script_SetStr(id, texts[i], "Text", "");
			}
			Ht_NoSlots(id);
		}
		Ht_SellActive(id, 0);
		Script_SetStr(id, "Hangar9", "Right", "PrevShip");
		Script_SetStr(id, "Hangar6", "Right", "PrevShip");
		Script_SetStr(id, "Hangar3", "Right", "PrevShip");
		int mp = Sn_Multi(0);
		if (mp < 0) {
			return 0;
		}
		if (!mp) {
			Script_SetStr(id, "HangarAux", "Right", "PrevShip");
		}
		c = Ht_AtCmp("editedShips", "prevHangar", "!=", &ht_none);
		if (c < 0) {
			return 0;
		}
		if (c) {
			Ht_HideInfo(id);
		}
		if (!Ht_PlatformButtons(id, 0) || !HT_SCORER(SetOnNext, id, "") || !HT_SCORER(SetOnPrev, id, "")) {
			return 0;
		}
		int lost = Ht_AtCmp("destroyedShips", "currentHangar", "==", Mode_IntVal(&x, 1));
		if (lost > 0) {
			lost = Ht_Single();
		}
		if (lost < 0 || (lost > 0 && !Ht_ControlHint(id, control, "HINT_Hangar13"))) {
			return 0;
		}
		if (skipanim) {
			return PORT_CALL(ShipEdit, ChangeShip, NULL, "vvi", &ht_none, &ht_none, 0L) &&
				PORT_CALL(ShipEdit, VerifyChangeShip, NULL, "vvi", &ht_none, &ht_none, 0L);
		}
		return PORT_CALL(ShipEdit, LaunchPlatformAnim, NULL, "");
	}
	if (empty < 0) {
		return 0;
	}
	Ht_WeaponSprites(id, 1);
	int notAux = Ht_GEq("currentHangar", 9);
	notAux = notAux < 0 ? -1 : !notAux;
	if (notAux > 0) {
		int p = Ht_Pilots();
		if (p < 0 || (p && (!Sn_Link("LinkUD", id, "PilotName", "Exit") || !Sn_Link("LinkUD", id, "PilotName", "Sell")))) {
			return 0;
		}
		Ht_SellActive(id, 1);
		if (!Ht_NextPrev(id, 1) || !Ht_RightSide(id, "Sell")) {
			return 0;
		}
		int mp = Sn_Multi(0);
		if (mp < 0) {
			return 0;
		}
		if (!mp) {
			Script_SetStr(id, "HangarAux", "Right", "Sell");
		}
		if (!Sn_Link("LinkLR", id, "Sell", "Exit") || !Ht_TabsOn(id, 1) || !Ht_TabRing(id, "LinkLR")) {
			return 0;
		}
	} else if (notAux == 0) {
		if (!Ht_PlatformButtons(id, 0) || !Mode_Import(SE) || !Ht_TabsOn(id, 0) || !Ht_TabRing(id, "UnLinkLR")) {
			return 0;
		}
		int mp = Sn_Multi(0);
		if (mp < 0 || (!mp && !Sn_Link("LinkLR", id, "HangarAux", "Exit")) || !Ht_RightSide(id, "Exit")) {
			return 0;
		}
		Ht_SellActive(id, 0);
		if (!Ht_NextPrev(id, 0)) {
			return 0;
		}
	} else {
		return 0;
	}
	Ht_NoSlots(id);
	if (!LvN_EqS(control, "HangarAux")) {
		int valid = Ht_AtCur("editedShips", &a) ? Ht_Valid(&a) : -1;
		Native_Free(&a);
		if (valid < 0 || !Ht_ControlHint(id, control, valid ? "HINT_Hangar12" : "HINT_Hangar14")) {
			return 0;
		}
	} else if (!Ht_ControlHint(id, control, "HINT_Hangar18")) {
		return 0;
	}
	Ht_HideInfo(id);
	if (LvN_EqS(control, "HangarAux") ? !HT_SCORER(SetDefault, id, "HangarAux") : !Ht_DefaultHangar(id)) {
		return 0;
	}
	return Ht_PlatformButtons(id, 0) && PORT_CALL(ShipEdit, LaunchPlatformAnim, NULL, "");
}

/* SelectItem(id, control): a hangar clicked: it becomes currentHangar and is selected */
static byte Ht_SelectItem(int id, const nativeVal_t *control) {
	nativeVal_t c, s = { 0 }, h = { 0 }, x;
	if (!Mode_Import(SE) || !Sn_From("LinkUD") || !Mode_Put(HT, "holograms", NULL, Mode_IntVal(&x, 0)) ||
		!Ht_SetPSi("EditHologramSound", "Loop", 0) || !Ht_SetPSd("EditHologramSound", "Volume", 0.0) ||
		!Mode_StopAllSounds()) {
		return 0;
	}
	int aux = LvN_EqS(control, "HangarAux");
	Native_Copy(&c, control);
	int last = aux ? 0 : Mode_Len(&c) - 1;
	byte ok = last >= -1;
	int is = aux;
	if (ok && !aux) {
		ok = Native_Slice(&c, 0, last, &s);
		is = ok && LvN_EqS(&s, "Hangar");
	}
	if (ok && is) {
		ok = Ht_G("currentHangar", &h) && Mode_Put(HT, "prevHangar", NULL, &h);
		Native_Free(&h);
		ok = ok && (aux ? Mode_Put(HT, "currentHangar", NULL, Mode_IntVal(&x, 9)) : Ht_ControlHangar(control, &h) &&
			Mode_Put(HT, "currentHangar", NULL, &h)) && Ht_SelectHangar(id, control, 0) && Ht_HideMini(id);
	}
	Native_Free(&c);
	Native_Free(&s);
	Native_Free(&h);
	return ok && Ht_UpdateHangars(id);
}

/* CreateEmptyShip(kind): the plan of a new ship of the model kind on the platform (prevShip) */
static byte Ht_CreateEmptyShip(const nativeVal_t *kind) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *ship = Mode_V(&t);
	nativeVal_t *ang = Mode_V(&t), *ton = Mode_V(&t), *life = Mode_V(&t), *name = Mode_V(&t), *list = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("ShipMaker") || !Mode_Import(SE)) {
		return Mode_Done(&t, 0);
	}
	byte ok = LvN_ModGet(SE, "editShipPosDummy", dummy) && LvN_Item(dummy, 1, pos) && LvN_Item(pos, 0, x) &&
		LvN_Item(pos, 1, y) && LvN_Item(pos, 2, z) && LvN_Create(kind, x, y, z, "Car", ship) &&
		Mode_Put(HT, "prevShip", NULL, ship);
	Native_Free(ship);
	ok = ok && Ht_G("prevShip", ship) && LvN_SetI(ship, "ActiveCMask", 0);
	Native_Free(ship);
	Native_Free(dummy);
	ok = ok && LvN_ModGet(SE, "editShipPosDummy", dummy) && Sn_FrontAng(dummy, NULL, ang) && Ht_G("prevShip", ship) &&
		Mode_VSet(ship, "Ang", ang);
	Native_Free(ship);
	ok = ok && PORT_CALL(ShipMaker, GetShipTon, ton, "v", kind) && LvN_Item(ton, 0, life) && Ht_G("prevShip", ship) &&
		Mode_VGet(ship, "Name", name) &&
		PORT_CALL(Vehicles, SetProperties, NULL, "vvvsssssssi", name, kind, life, "P_None", "", "", "", "",
			"0,0,0,0,0,0", "Pilot", 1L);
	Native_Free(ship);
	ok = ok && Ht_G("prevShip", ship) && LvN_SetI(ship, "RenderEffect", 1);
	Native_Free(ship);
	if (ok && Ht_G("prevShip", ship)) {
		Native_SetEmpty(list, NV_LIST);
		Native_Copy(Native_Append(list), ship);
		ok = PORT_CALL(ShipEdit, SetupForEdit, NULL, "v", list);
	} else {
		ok = 0;
	}
	return Mode_Done(&t, ok);
}

/* the link of the emergency ship to b in single player, of the right side to b */
static byte Ht_AuxRight(int id, const char *b) {
	int mp = Sn_Multi(0);
	return mp >= 0 && (mp || Sn_Link("LinkLR", id, "HangarAux", b)) && Ht_RightSide(id, b);
}

/* BuyShip(id, control): the ship type shown is bought (or the destroyed ship rebuilt) for the hangar */
static byte Ht_BuyShip(int id) {
	static const char *const mods[] = { SE, "ShipMaker", "string", "Sound" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *list = Mode_V(&t), *money = Mode_V(&t), *cost = Mode_V(&t), *left = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *x = Mode_V(&t), *aux = Mode_V(&t), *e = Mode_V(&t), *ship = Mode_V(&t), *k = Mode_V(&t);
	if (!Sn_From("LinkLR") || !Sn_From("LinkUD")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	int rebuild;
	int mp = Ht_ShipsList(list, &rebuild) ? Sn_Multi(0) : -1;
	if (mp < 0) {
		return Mode_Done(&t, 0);
	}
	if (!mp) {
		Native_SetInt(money, Mode_GetMoney());
		byte ok = rebuild ? Ht_G("currentHangar", cur) && Ht_ShipText(cur, text) && Ht_OwnTotalCost(text, cost) :
			Ht_ShipPrice(list, cost);
		int c = ok && Mode_Arith('-', money, cost, left) ? Mode_CmpI(left, "<", 0) : -1;
		if (c != 0) {
			return Mode_Done(&t, c > 0);
		}
		ok = Mode_SetMoneySeam(left) && Ht_SetRepr(id, "Money", "Text", left) && Ht_SetAtCur("destroyedShips", Mode_IntVal(k, 0));
		int n = ok && LvN_ModGet(SE, "auxShipsToEditList", aux) ? Mode_Len(aux) : -1;
		ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			Native_Free(aux);
			Native_Free(x);
			ok = LvN_ModGet(SE, "auxShipsToEditList", aux) && LvN_Item(aux, i, x);
			if (ok && LvN_EqS(x, "Rebuild")) {
				ok = Mode_Put(HT, "currentShipToEdit", NULL, Mode_IntVal(k, i));
				break;
			}
		}
		Native_Free(aux);
		Native_SetEmpty(e, NV_LIST);
		ok = ok && Mode_SetModVar(SE, "auxShipsToEditList", e);
		n = ok && LvN_ModGet(SE, "shipsToEditList", aux) ? Mode_Len(aux) : -1;
		ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			Native_Free(aux);
			Native_Free(x);
			ok = LvN_ModGet(SE, "shipsToEditList", aux) && LvN_Item(aux, i, x) && LvN_ModAppend(SE, "auxShipsToEditList", x);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_Put(HT, "holograms", NULL, Mode_IntVal(k, 0)) || !Ht_SetPSi("EditHologramLoopSound", "Loop", 0) ||
		!Ht_SetPSd("EditHologramLoopSound", "Volume", 0.0) || !Mode_StopAllSounds()) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (rebuild) {
		ok = Ht_G("prevShip", ship) && Mode_VSetFX(ship, "ShipEditBuild");
	} else {
		int c = Ht_AtCmp("editedShips", "currentHangar", "!=", &ht_none);
		ok = c == 0 || (c > 0 && Ht_AtCur("editedShips", e) && Mode_VDel(e));
		ok = ok && Ht_G("prevShip", ship) && Mode_VSetFX(ship, "ShipEditBuild");
		Native_Free(ship);
		ok = ok && Ht_G("prevShip", ship) && Ht_SetAtCur("editedShips", ship);
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Ht_WeaponSprites(id, 1);
	Ht_Show(id, ht_shipType, 0);
	Ht_NoSlots(id);
	if (!Ht_ShowInfo(id, 1) || !Ht_UpdateHangars(id) || !Ht_TabsOn(id, 1) || !Ht_TabRing(id, "LinkLR") || !Ht_NextPrev(id, 1)) {
		return Mode_Done(&t, 0);
	}
	Ht_SellActive(id, 1);
	if (!Mode_Sound("ShipEditLoadShipSound", "ShipEditLoadShipVolume") || !Ht_AuxRight(id, "Sell") ||
		!Sn_Link("LinkLR", id, "Sell", "Exit")) {
		return Mode_Done(&t, 0);
	}
	int p = Ht_Pilots();
	if (p < 0) {
		return Mode_Done(&t, 0);
	}
	if (p) {
		Script_SetStr(id, "PrevPilot", "Left", "Hangar6");
		if (!Sn_Link("LinkUD", id, "PilotName", "Sell")) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Ht_UpdateHangars(id) && Ht_DefaultHangar(id));
}

/* SellShip(id, control): the ship of the hangar is sold (its whole price in single player) */
static byte Ht_SellShip(int id) {
	static const char *const links[] = { "LinkLR", "LinkUD", "UnLinkLR", "UnLinkUD" };
	static const char *const vars[] = { "Highlight", "TextColorAlpha", "TextColorRed", "TextColorGreen", "TextColorBlue" };
	static const long vals[] = { 0, 255, 161, 161, 161 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *cost = Mode_V(&t), *sum = Mode_V(&t), *e = Mode_V(&t), *k = Mode_V(&t);
	nativeVal_t *tabs = Mode_V(&t), *rest = Mode_V(&t), *tab = Mode_V(&t), *name = Mode_V(&t), *cur = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t), *control = Mode_V(&t);
	for (int i = 0; i < 4; i++) {
		if (!Sn_From(links[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_Import(SE) || !Mode_Import("ShipMaker") || !Mode_Import("Sound")) {
		return Mode_Done(&t, 0);
	}
	int c = Ht_AtCmp("editedShips", "currentHangar", "!=", &ht_none);
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	int mp = Sn_Multi(0);
	if (mp < 0) {
		return Mode_Done(&t, 0);
	}
	if (!mp) {
		Native_SetInt(money, Mode_GetMoney());
		if (!Ht_G("shipTotalCost", cost) || !Mode_Arith('+', money, cost, sum) || !Mode_SetMoneySeam(sum) ||
			!Ht_SetRepr(id, "Money", "Text", sum) || !Ht_SetAtCur("destroyedShips", Mode_IntVal(k, 0))) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Ht_AtCur("editedShips", e) || !Mode_VDel(e) || !Ht_SetAtCur("editedShips", &ht_none) ||
		!Mode_Put(HT, "prevShip", NULL, &ht_none) || !Ht_UpdateHangars(id)) {
		return Mode_Done(&t, 0);
	}
	Ht_SellActive(id, 0);
	if (!Mode_Sound("ShipEditUnLoadSound", "ShipEditUnLoadVolume") || !Ht_RightSide(id, "Exit")) {
		return Mode_Done(&t, 0);
	}
	int p = Ht_Pilots();
	if (p < 0 || (p && (!Sn_Link("UnLinkUD", id, "PilotName", "Sell") || !Sn_Link("LinkUD", id, "PilotName", "Exit")))) {
		return Mode_Done(&t, 0);
	}
	Ht_HideInfo(id);
	/* the script unlinks the whole ring of tabs once per tab */
	byte ok = LvN_ModGet(SE, "tabs", tabs) && Sn_Rest(tabs, rest);
	int n = ok ? Native_ForCount(rest) : -1;
	ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(rest, i, tab) && Sn_Cat("ShipEditTab", tab, NULL, name);
		for (int j = 0; ok && j < 5; j++) {
			ok = Sn_Set(id, name, vars[j], Mode_IntVal(k, vals[j]));
		}
		Native_SetStr(k, "");
		Native_Free(tabs);
		ok = ok && Sn_Set(id, name, "OnAccept", k) && LvN_ModGet(SE, "tabs", tabs) && Sn_TabRing(id, tabs, "UnLinkLR");
		Native_Free(tab);
		Native_Free(name);
	}
	if (!ok || !Ht_NextPrev(id, 0) || !Mode_Put(HT, "prevHangar", NULL, Mode_IntVal(k, -1)) || !Ht_G("currentHangar", cur)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(cur, r);
	Native_Free(cur);
	ok = Sn_Cat("Hangar", r, NULL, control) && Ht_SelectHangar(id, control, 1) && Ht_G("currentHangar", cur) &&
		Mode_Put(HT, "prevHangar", NULL, cur);
	return Mode_Done(&t, ok);
}

/* ---- adapters: the argument formats of the C port (the errors of PyArg), the objects as native values ---- */

MODE_IFN(HT_ShowBoostPrediction, 2, Ht_BoostPrediction(id, &a[1]))
MODE_IFN(HT_DisableChangePlatformButtons, 1, Ht_Platform(id, 0))
MODE_IFN(HT_EnableChangePlatformButtons, 1, Ht_Platform(id, 1))
MODE_IFN(HT_PrevPilot, 2, Ht_Pilot(id, -1))
MODE_IFN(HT_NextPilot, 2, Ht_Pilot(id, 1))
MODE_IFN(HT_SetSellVisible, 2, Ht_Visible(id, &a[1], ht_sell, SE))
MODE_IFN(HT_SetExitVisible, 2, Ht_Visible(id, &a[1], ht_exit, NULL))
MODE_IFN(HT_SetShipTypeVisible, 2, Ht_Visible(id, &a[1], ht_shipType, NULL))
MODE_IFN(HT_SetPilotVisible, 2, Ht_Visible(id, &a[1], ht_pilot, NULL))
MODE_IFN(HT_SetSellActive, 2, (Ht_SellActive(id, Native_True(&a[1])), 1))
MODE_IFN(HT_HideShipMiniInfo, 3, Ht_HideMini(id))
MODE_IFN(HT_HideHangarPanel, 1, Ht_HideHangarPanel(id))
MODE_IFN(HT_UpdateHangars, 1, Ht_UpdateHangars(id))
MODE_IFN(HT_ShowHangarPanel, 1, Ht_ShowHangarPanel(id))
MODE_FN(HT, IsShipValid, 1, Ht_IsShipValid(&a[0], out))
MODE_IFN(HT_ShowShipMiniInfo, 3, Ht_ShowMini(id, &a[1]))
MODE_IFN(HT_ShowRebuildShipMoney, 3, Ht_ShowRebuildShipMoney(id, &a[1]))
MODE_IFN(HT_HideRebuildShipMoney, 3, Ht_HideRebuildShipMoney(id))
MODE_IFN(HT_ShowMoneyBuyingShip, 3, Ht_AuxShowMoney(id, "-"))
MODE_IFN(HT_ShowMoneySellingShip, 3, Ht_AuxShowMoney(id, "+"))
MODE_IFN(HT_HideMoney, 3, (Script_SetNum(id, "MoneyPredict", "Visible", 0), 1))
MODE_FN(HT, GetSpeedRampOffsets, 1, Ht_SpeedRampOffsets(&a[0], out))
MODE_FN(HT, GetMaxSpeedTime, 1, Ht_MaxSpeedTime(&a[0], out))
MODE_IFN(HT_SetHangarPanel, 1, Ht_SetHangarPanel(id))
MODE_FN(HT, CmSec2Value, 1, Ht_CmSec2Value(&a[0], out))
MODE_IFN(HT_HideShipInfo, 1, (Ht_HideInfo(id), 1))
MODE_IFN(HT_PrevShipToEdit, 2, Ht_ShipToEdit(id, -1))
MODE_IFN(HT_NextShipToEdit, 2, Ht_ShipToEdit(id, 1))
MODE_IFN(HT_SetShipTypeButtonsState, 2, LvN_EqS(&a[1], "Build") ? Ht_TypeState(id, 0) : LvN_EqS(&a[1], "Rebuild") ? Ht_TypeState(id, 1) : 1)
MODE_IFN(HT_SelectShipToEdit, 1, Ht_SelectShipToEdit(id))
MODE_IFN(HT_SelectItem, 2, Ht_SelectItem(id, &a[1]))
MODE_IFN(HT_BuyShip, 2, Ht_BuyShip(id))
MODE_IFN(HT_SellShip, 2, Ht_SellShip(id))
MODE_IFN(HT_AddReferenceShipControls, 1, Ht_Reference(id))
MODE_FN(HT, CreateEmptyShip, 1, Ht_CreateEmptyShip(&a[0]))

static byte HT_AuxShowMoney_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *symbol;
	int id;
	(void)out;
	return Mode_Parse(a, n, "is", &id, &symbol) && Ht_AuxShowMoney(id, symbol);
}

MODE_ADAPTER(HT, AuxShowMoney)

/* ComputeShipTotalCost(cad="prevShip") */
MODE_KW(HT, ComputeShipTotalCost, "|O", "cad") {
	Mode_KwDefS(v, given, 0, "prevShip");
	return Ht_TotalCost(&v[0], out);
}

/* ScaleSpeedRamp(id, offsetX, offsetY, isMiniShip=0) */
MODE_KWT(HT, ScaleSpeedRamp, "iOO|O", "id", "offsetX", "offsetY", "isMiniShip") {
	const nativeVal_t *ox, *oy, *mini;
	int id;
	return Mode_Parse(v, 4, "iOOO", &id, &ox, &oy, &mini) && Ht_ScaleSpeedRamp(id, ox, oy, given[3] ? mini : NULL);
}

/* ShowShipInfo(id, showAmmoInfo=1) */
MODE_KWT(HT, ShowShipInfo, "i|O", "id", "showAmmoInfo") {
	const nativeVal_t *ammo;
	int id;
	return Mode_Parse(v, 2, "iO", &id, &ammo) && Ht_ShowInfo(id, given[1] ? Native_True(ammo) : 1);
}

/* SelectHangar(id, control="", skipanim=0) */
MODE_KWT(HT, SelectHangar, "i|OO", "id", "control", "skipanim") {
	const nativeVal_t *control, *skip;
	int id;
	Mode_KwDefS(v, given, 1, "");
	return Mode_Parse(v, 3, "iOO", &id, &control, &skip) && Ht_SelectHangar(id, control, given[2] ? Native_True(skip) : 0);
}

PyMethodDef g_HangarTabMethods[] = {
	{ "ShowBoostPrediction", HT_ShowBoostPrediction, 1, NULL },
	{ "DisableChangePlatformButtons", HT_DisableChangePlatformButtons, 1, NULL },
	{ "EnableChangePlatformButtons", HT_EnableChangePlatformButtons, 1, NULL },
	{ "PrevPilot", HT_PrevPilot, 1, NULL },
	{ "NextPilot", HT_NextPilot, 1, NULL },
	{ "DrawBackground", (PyCFunction)(void (*)(void))HangarTab_DrawBackground, 3, NULL },
	{ "SetSellVisible", HT_SetSellVisible, 1, NULL },
	{ "SetSellActive", HT_SetSellActive, 1, NULL },
	{ "SetExitVisible", HT_SetExitVisible, 1, NULL },
	{ "SetShipTypeVisible", HT_SetShipTypeVisible, 1, NULL },
	{ "SetPilotVisible", HT_SetPilotVisible, 1, NULL },
	{ "HideHangarPanel", HT_HideHangarPanel, 1, NULL },
	{ "UpdateHangars", HT_UpdateHangars, 1, NULL },
	{ "ShowHangarPanel", HT_ShowHangarPanel, 1, NULL },
	{ "IsShipValid", HT_IsShipValid, 1, NULL },
	{ "ComputeShipTotalCost", (PyCFunction)(void (*)(void))HT_ComputeShipTotalCost, 3, NULL },
	{ "ShowRebuildShipMoney", HT_ShowRebuildShipMoney, 1, NULL },
	{ "HideRebuildShipMoney", HT_HideRebuildShipMoney, 1, NULL },
	{ "AuxShowMoney", HT_AuxShowMoney, 1, NULL },
	{ "ShowMoneyBuyingShip", HT_ShowMoneyBuyingShip, 1, NULL },
	{ "ShowMoneySellingShip", HT_ShowMoneySellingShip, 1, NULL },
	{ "HideMoney", HT_HideMoney, 1, NULL },
	{ "ScaleSpeedRamp", (PyCFunction)(void (*)(void))HT_ScaleSpeedRamp, 3, NULL },
	{ "GetSpeedRampOffsets", HT_GetSpeedRampOffsets, 1, NULL },
	{ "GetMaxSpeedTime", HT_GetMaxSpeedTime, 1, NULL },
	{ "SetHangarPanel", HT_SetHangarPanel, 1, NULL },
	{ "CmSec2Value", HT_CmSec2Value, 1, NULL },
	{ "ShowShipInfo", (PyCFunction)(void (*)(void))HT_ShowShipInfo, 3, NULL },
	{ "HideShipInfo", HT_HideShipInfo, 1, NULL },
	{ "PrevShipToEdit", HT_PrevShipToEdit, 1, NULL },
	{ "NextShipToEdit", HT_NextShipToEdit, 1, NULL },
	{ "SetShipTypeButtonsState", HT_SetShipTypeButtonsState, 1, NULL },
	{ "SelectShipToEdit", HT_SelectShipToEdit, 1, NULL },
	{ "SelectHangar", (PyCFunction)(void (*)(void))HT_SelectHangar, 3, NULL },
	{ "SelectItem", HT_SelectItem, 1, NULL },
	{ "CreateEmptyShip", HT_CreateEmptyShip, 1, NULL },
	{ "BuyShip", HT_BuyShip, 1, NULL },
	{ "SellShip", HT_SellShip, 1, NULL },
	{ "AddReferenceShipControls", HT_AddReferenceShipControls, 1, NULL },
	{ "ShowShipMiniInfo", HT_ShowShipMiniInfo, 1, NULL },
	{ "HideShipMiniInfo", HT_HideShipMiniInfo, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(HangarTab)
