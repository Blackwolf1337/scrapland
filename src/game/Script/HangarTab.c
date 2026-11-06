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
 */
#include <string.h>

#include "scrap/scrap.h"
#include "ShipEdit.h"

#define G(name) Sf_Global(&f, HT, name)
#define GF(name) Sf_Global(f, HT, name)
#define SEA(name) Se_Mod(&f, SE, name)
#define SEAF(name) Se_Mod(f, SE, name)

/* ShipEdit.IsEditingMultiplayer() and not isXbox: the pilots of a network game (1, 0, -1) */
static int Ht_Pilots(scriptFrame_t *f) {
	int mp = Se_Multi(f, 0);
	if (mp > 0) {
		int x = Sf_True(f, GF("isXbox"));
		return x < 0 ? -1 : !x;
	}
	return mp;
}

/* int((`v - float(int(v))`)[2]): the first decimal of v as the script reads it; *whole int(v) */
static PyObject *Ht_Decimal(scriptFrame_t *f, PyObject *v, PyObject **whole, byte asInt) {
	*whole = Sf_ToInt(f, v);
	PyObject *fl = f->failed ? NULL : Sf_Keep(f, PyNumber_Float(*whole));
	PyObject *ch = Sf_Item(f, Sf_Repr(f, Sf_Op(f, v, '-', fl)), 2);
	return asInt ? Sf_ToInt(f, ch) : ch;
}

/* ShowBoostPrediction(id, newWeight): the arrow of the boost time a new weight gives (up, down or equal) */
static PyObject *HT_ShowBoostPrediction(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *weight, *curInt, *newInt;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &weight)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	static const char *const arrows[] = { "BoostUp", "BoostDown", "BoostEqual", "BoostBack" };
	for (int i = 0; i < 4; i++) {
		Se_SetI(&f, id, arrows[i], "Visible", 0);
	}
	PyObject *cur = Sf_Attr(&f, G("prevShip"), "BoostTime");
	PyObject *nb = Sf_Ask(&f, "ShipMaker", "GetBoostFactor", "(OO)", Sf_Attr(&f, G("prevShip"), "Model"), weight);
	PyObject *curDec = Ht_Decimal(&f, cur, &curInt, 1);
	PyObject *newDec = Ht_Decimal(&f, nb, &newInt, 1);
	const char *arrow = "BoostEqual";
	if (Sf_Compare(&f, newInt, "<", curInt) > 0) {
		arrow = "BoostDown";
	} else if (Sf_Compare(&f, newInt, ">", curInt) > 0) {
		arrow = "BoostUp";
	} else if (Sf_Compare(&f, newDec, "<", curDec) > 0) {
		arrow = "BoostDown";
	} else if (Sf_Compare(&f, newDec, ">", curDec) > 0) {
		arrow = "BoostUp";
	}
	Se_SetI(&f, id, arrow, "Visible", 1);
	Se_SetI(&f, id, "BoostBack", "Visible", 1);
	return Sf_None(&f);
}

/* for t in ShipEdit.tabs[1:]: SScorer.Set(id, "ShipEditTab" + t, "OnAccept", on and "ShipEdit.ChangeTab" or "") */
static void Ht_TabsAccept(scriptFrame_t *f, int id, byte on) {
	PyObject *rest = Sf_Slice(f, SEAF("tabs"), 1, 0x7fffffff);
	for (int i = 0; !f->failed; i++) {
		int mark = f->n;
		PyObject *t = Sf_Next(f, rest, i);
		if (!t) {
			break;
		}
		Se_SetOO(f, id, Sf_Cat(f, "sO", "ShipEditTab", t), "OnAccept", Sf_Str(f, on ? "ShipEdit.ChangeTab" : ""));
		Sf_Release(f, f->failed ? f->n : mark);
	}
}

/* the buttons that change the ship on the platform: OnAccept of the hangars, the tabs, Sell, Exit, the ship type
 * buttons and the pilot buttons; off while the platform moves */
static void Ht_PlatformButtons_(scriptFrame_t *f, int id, byte on) {
	for (int i = 0; i < 9; i++) {
		char name[0x10];
		Str_Printf(name, sizeof(name), "Hangar%d", i + 1);
		Se_SetS(f, id, name, "OnAccept", on ? "HangarTab.SelectItem" : "");
	}
	Se_SetS(f, id, "HangarAux", "OnAccept", on ? "HangarTab.SelectItem" : "");
	if (!on) {
		Ht_TabsAccept(f, id, 0);
	} else {
		int c = Sf_True(f, Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar")));
		if (c > 0) {
			c = Sf_Compare(f, GF("currentHangar"), "!=", Sf_Int(f, 9));
		}
		if (c > 0) {
			c = Sf_Compare(f, Sf_GetItem(f, SEAF("destroyedShips"), GF("currentHangar")), "!=", Sf_Int(f, 1));
		}
		if (c > 0) {
			Se_TabRing(f, id, SEAF("tabs"), "LinkLR");
			Ht_TabsAccept(f, id, 1);
		}
	}
	Se_SetS(f, id, "Sell", "OnAccept", on ? "HangarTab.SellShip" : "");
	Se_SetS(f, id, "Exit", "OnAccept", on ? "ShipEdit.ExitEditShipMenu" : "");
	Se_SetS(f, id, "ShipType", "OnAccept", on ? "HangarTab.BuyShip" : "");
	Se_SetS(f, id, "PrevShip", "OnAccept", on ? "HangarTab.PrevShipToEdit" : "");
	Se_SetS(f, id, "NextShip", "OnAccept", on ? "HangarTab.NextShipToEdit" : "");
	if (Ht_Pilots(f) > 0) {
		Se_SetS(f, id, "PrevPilot", "OnAccept", on ? "HangarTab.PrevPilot" : "");
		Se_SetS(f, id, "NextPilot", "OnAccept", on ? "HangarTab.NextPilot" : "");
	}
	if (!on) {
		Se_TabRing(f, id, SEAF("tabs"), "UnLinkLR");
	}
	Sf_Ask(f, "SScorer", "SetOnCancel", "(is)", id, on ? "ShipEdit.ExitEditShipMenu" : "");
}

static void Ht_PlatformButtons(scriptFrame_t *f, int id, byte on) {
	SE_SUB(f, Ht_PlatformButtons_(&g, id, on));
}

static PyObject *Ht_PlatformArgs(PyObject *args, byte on) {
	scriptFrame_t f = { 0 };
	int id;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_From(&f, "Menu", on ? "LinkLR" : "UnLinkLR");
	Sf_Import(&f, SE);
	Ht_PlatformButtons(&f, id, on);
	return Sf_None(&f);
}

static PyObject *HT_DisableChangePlatformButtons(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_PlatformArgs(args, 0);
}

static PyObject *HT_EnableChangePlatformButtons(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_PlatformArgs(args, 1);
}

/* PrevPilot(id, control), NextPilot(id, control): the head of the pilot before or after the current one */
static PyObject *Ht_Pilot(PyObject *args, int step) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	Se_SetOO(&f, id, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), G("currentPilot")), "Visible", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, HT, "currentPilot", Sf_OpI(&f, G("currentPilot"), step < 0 ? '-' : '+', 1));
	int n = step < 0 ? 0 : Sf_Len(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"));
	if (step < 0 && Sf_Compare(&f, G("currentPilot"), "<", Sf_Int(&f, 0)) > 0) {
		Sf_SetGlobal(&f, HT, "currentPilot", Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "ShipMaker", "PilotsList")) - 1));
	} else if (step > 0 && Sf_Compare(&f, G("currentPilot"), ">", Sf_Int(&f, n - 1)) > 0) {
		Sf_SetGlobal(&f, HT, "currentPilot", Sf_Int(&f, 0));
	}
	Se_SetOO(&f, id, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), G("currentPilot")), "Visible", Sf_Int(&f, 1));
	return Sf_None(&f);
}

static PyObject *HT_PrevPilot(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Pilot(args, -1);
}

static PyObject *HT_NextPilot(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Pilot(args, 1);
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

/* SetSellVisible, SetExitVisible, SetShipTypeVisible, SetPilotVisible (id, bVisible) */
static PyObject *Ht_Visible(PyObject *args, const char *const *items, const char *import) {
	scriptFrame_t f = { 0 };
	PyObject *v;
	int id;
	if (!PyArg_ParseTuple(args, "iO", &id, &v)) {
		return NULL;
	}
	if (import) {
		Sf_Import(&f, import);
	}
	Se_SetAll(&f, id, items, "Visible", v);
	return Sf_None(&f);
}

static PyObject *HT_SetSellVisible(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Visible(args, ht_sell, SE);
}

static PyObject *HT_SetExitVisible(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Visible(args, ht_exit, NULL);
}

static PyObject *HT_SetShipTypeVisible(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Visible(args, ht_shipType, NULL);
}

static PyObject *HT_SetPilotVisible(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_Visible(args, ht_pilot, NULL);
}

/* the same lists from C, with a number */
static void Ht_Show(scriptFrame_t *f, int id, const char *const *items, long v) {
	for (; *items; items++) {
		Se_SetI(f, id, *items, "Visible", v);
	}
}

/* SetSellActive(id, bActive): the Sell button works or is greyed */
static void Ht_SellActive(scriptFrame_t *f, int id, int on) {
	Se_SetI(f, id, "Sell", "HighLight", on);
	Se_SetI(f, id, "Sell", "W", on ? 35 : 0);
	Se_SetI(f, id, "Sell", "H", on ? 35 : 0);
	Se_SetI(f, id, "Sell", "SpriteIndex", on ? 8 : 10);
	Se_SetS(f, id, "Sell", "OnAccept", on ? "HangarTab.SellShip" : "");
}

static PyObject *HT_SetSellActive(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *v;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &v)) {
		return NULL;
	}
	int on = Sf_True(&f, v);
	if (on >= 0) {
		Ht_SellActive(&f, id, on);
	}
	return Sf_None(&f);
}

/* the words of the language text key (string.split), each the item prefix<word>: Visible v */
static void Ht_Words_(scriptFrame_t *f, int id, const char *key, const char *prefix, long v) {
	PyObject *words = Sf_Ask(f, "string", "split", "(O)", Sf_Str(f, f->failed ? "" : Script_Lang(key)));
	for (int i = 0; !f->failed; i++) {
		int mark = f->n;
		PyObject *w = Sf_Next(f, words, i);
		if (!w) {
			break;
		}
		Se_SetOO(f, id, Sf_Cat(f, "sO", prefix, w), "Visible", Sf_Int(f, v));
		Sf_Release(f, f->failed ? f->n : mark);
	}
}

static void Ht_Words(scriptFrame_t *f, int id, const char *key, const char *prefix, long v) {
	SE_SUB(f, Ht_Words_(&g, id, key, prefix, v));
}

/* HideShipMiniInfo(id, control, lostFocus): the mini information of a hangar hides */
static void Ht_HideMini_(scriptFrame_t *f, int id) {
	Sf_Import(f, "string");
	if (Sf_True(f, GF("miniShip")) > 0) {
		Sf_SetGlobal(f, HT, "miniShip", Py_None);
	}
	Ht_Words(f, id, "ShipEdit_AvailableHangar", "AvailableHangarText_", 0);
	Ht_Words(f, id, "ShipEdit_UncompletedShip", "UncompletedShipText_", 0);
	Ht_Words(f, id, "ShipEdit_DestroyedShip", "DestroyedShipText_", 0);
	static const char *const items[] = { "HangarShip", "MiniMaxSpeedText", "MiniMaxSpeed", "MiniSpeedRamp1",
		"MiniSpeedRamp2", "MiniBoostRechargeTime", "MiniBoostRechargeTimeText", "MiniHullText", "MiniHullValue",
		"MiniWeight", "MiniCurrentWeight", "MiniMaxWeight", "MiniSlashWeight", "MiniShipInfo", "HLine", "VLine", "MLine",
		"AuxShipText", NULL };
	Ht_Show(f, id, items, 0);
}

static void Ht_HideMini(scriptFrame_t *f, int id) {
	SE_SUB(f, Ht_HideMini_(&g, id));
}

static PyObject *HT_HideShipMiniInfo(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control, *lost;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Ht_HideMini(&f, id);
	return Sf_None(&f);
}

/* HideHangarPanel(id): the items of the Hangar panel hide */
static PyObject *HT_HideHangarPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, SE);
	Sf_Import(&f, "ShipMaker");
	static const char *const aux[] = { "HangarAux", "AuxShipText", "LittleShipAux", "nAux", NULL };
	Ht_Show(&f, id, aux, 0);
	for (int n = 1; n <= 9; n++) {
		static const char *const parts[] = { "LittleShip", "Hangar", "n" };
		for (int k = 0; k < 3; k++) {
			char name[0x20];
			Str_Printf(name, sizeof(name), "%s%d", parts[k], n);
			Se_SetI(&f, id, name, "Visible", 0);
		}
	}
	Se_SetOO(&f, id, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), G("currentPilot")), "Visible", Sf_Int(&f, 0));
	Ht_Show(&f, id, ht_pilot, 0);
	Ht_Show(&f, id, ht_sell, 0);
	Ht_HideMini(&f, id);
	Sf_From(&f, "Menu", "UnLinkLR");
	Se_Link(&f, "UnLinkLR", id, "Sell", "Exit");
	return Sf_None(&f);
}

/* the colour of an item: normal (83, 114, 193), selected (212, 200, 80) or destroyed (255, 32, 32) */
static void Ht_Color(scriptFrame_t *f, int id, const char *name, int kind) {
	static const long rgb[3][3] = { { 83, 114, 193 }, { 212, 200, 80 }, { 255, 32, 32 } };
	Se_SetI(f, id, name, "Red", rgb[kind][0]);
	Se_SetI(f, id, name, "Green", rgb[kind][1]);
	Se_SetI(f, id, name, "Blue", rgb[kind][2]);
}

/* IsShipValid(ship): the ship has engines (the lifeboat always counts): the NEng value that decided, 0 or 1 */
static PyObject *Ht_IsShipValid_(scriptFrame_t *f, PyObject *ship) {
	if (Sf_Compare(f, ship, "==", Py_None) != 0) {
		return f->failed ? NULL : Sf_Int(f, 0);
	}
	if (Sf_EqS(f, Sf_Attr(f, ship, "Model"), "SLifeBoat") != 0) {
		return f->failed ? NULL : Sf_Int(f, 1);
	}
	PyObject *has = Sf_Int(f, 0);
	for (int i = 0; i < 4 && !f->failed; i++) {
		char var[8];
		Str_Printf(var, sizeof(var), "NEng%d", i);
		has = Sf_Attr(f, ship, var);
		if (Sf_True(f, has) != 0) {
			break;
		}
	}
	return has;
}

static PyObject *Ht_IsShipValid(scriptFrame_t *f, PyObject *ship) {
	PyObject *r = NULL;
	if (!f->failed) {
		scriptFrame_t g = { 0 };
		r = Se_JoinValue(f, &g, Ht_IsShipValid_(&g, ship));
	}
	return r;
}

static PyObject *HT_IsShipValid(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *ship;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ship)) {
		return NULL;
	}
	return Sf_Return(&f, Ht_IsShipValid(&f, ship));
}

/* UpdateHangars(id): the hangars in their state: selected, empty, with a valid, incomplete or destroyed ship, and
 * their hints */
static void Ht_UpdateHangars_(scriptFrame_t *f, int id) {
	Sf_Import(f, SE);
	if (Se_Multi(f, 0) == 0) {
		Se_SetI(f, id, "HangarAux", "Visible", 1);
		Se_SetI(f, id, "AuxShipText", "Visible", 0);
		Se_SetI(f, id, "LittleShipAux", "Visible", 1);
		Se_SetI(f, id, "nAux", "Visible", 1);
	}
	Se_SetI(f, id, "LittleShipAux", "SpriteIndex", 4);
	Ht_Color(f, id, "LittleShipAux", 0);
	Se_SetI(f, id, "nAux", "SpriteIndex", 4);
	Ht_Color(f, id, "nAux", 0);
	int aux = Sf_EqI(f, GF("currentHangar"), 9);
	if (aux > 0) {
		Se_SetLang(f, id, "HangarAux", "Hint", "HINT_Hangar18");
		Se_SetI(f, id, "HangarAux", "SpriteIndex", 2);
		Se_SetO(f, id, "HangarAux", "Alpha", GF("SelectedColorAlpha"));
	} else if (aux == 0) {
		Se_SetUse(f, id, Sf_Str(f, "HangarAux"), "Hint", "HINT_Hangar17", HT);
		Se_SetI(f, id, "HangarAux", "SpriteIndex", 1);
		Se_SetO(f, id, "HangarAux", "Alpha", GF("NormalColorAlpha"));
	}
	if (aux >= 0) {
		Ht_Color(f, id, "LittleShipAux", aux);
		Ht_Color(f, id, "nAux", aux);
	}
	for (int n = 1; n <= 9 && !f->failed; n++) {
		int mark = f->n;
		char hangar[0x10], little[0x20], num[0x10];
		Str_Printf(hangar, sizeof(hangar), "Hangar%d", n);
		Str_Printf(little, sizeof(little), "LittleShip%d", n);
		Str_Printf(num, sizeof(num), "n%d", n);
		int none = Sf_Compare(f, Sf_Item(f, SEAF("editedShips"), n - 1), "==", Py_None);
		int exists = none < 0 ? -1 : !none;
		Se_SetI(f, id, hangar, "Visible", 1);
		Se_SetI(f, id, hangar, "Red", 255);
		Se_SetI(f, id, hangar, "Green", 255);
		Se_SetI(f, id, hangar, "Blue", 255);
		if (exists > 0) {
			Se_SetUse(f, id, Sf_Str(f, hangar), "Hint", "HINT_Hangar10", HT);
		} else if (exists == 0) {
			int sput = Sf_True(f, SEAF("fromSputnik"));
			if (sput > 0) {
				Se_SetLang(f, id, hangar, "Hint", "HINT_Hangar19");
			} else if (sput == 0) {
				int cur = Sf_EqI(f, GF("currentHangar"), n - 1);
				if (cur > 0) {
					Se_SetLang(f, id, hangar, "Hint", "HINT_Hangar11");
				} else if (cur == 0) {
					Se_SetUse(f, id, Sf_Str(f, hangar), "Hint", "HINT_Hangar10", HT);
				}
			}
		}
		int cur = Sf_Compare(f, Sf_Int(f, n - 1), "==", GF("currentHangar"));
		if (cur >= 0) {
			Se_SetI(f, id, hangar, "Visible", 1);
			Se_SetI(f, id, hangar, "SpriteIndex", cur ? 2 : 1);
			Se_SetO(f, id, hangar, "Alpha", GF(cur ? "SelectedColorAlpha" : "NormalColorAlpha"));
			Se_SetI(f, id, little, "Visible", 1);
			Se_SetI(f, id, little, "SpriteIndex", 4);
			Ht_Color(f, id, little, cur);
			Se_SetI(f, id, num, "Visible", 1);
			Ht_Color(f, id, num, cur);
		}
		if (exists >= 0) {
			Se_SetI(f, id, little, "Visible", exists);
		}
		if (exists > 0) {
			int lost = Sf_EqI(f, Sf_Item(f, SEAF("destroyedShips"), n - 1), 1);
			if (lost > 0) {
				int mp = Se_Multi(f, 0);
				lost = mp < 0 ? -1 : !mp;
			}
			if (lost > 0) {
				Se_SetI(f, id, little, "Visible", 1);
				Ht_Color(f, id, little, 2);
				Ht_Color(f, id, num, 2);
				int sput = Sf_True(f, SEAF("fromSputnik"));
				int here = sput == 0 ? Sf_EqI(f, GF("currentHangar"), n - 1) : 0;
				if (sput > 0 || here > 0) {
					Se_SetLang(f, id, hangar, "Hint", "HINT_Hangar13");
				} else if (here == 0 && !f->failed) {
					Se_SetUse(f, id, Sf_Str(f, hangar), "Hint", "HINT_Hangar10", HT);
				}
			} else if (lost == 0) {
				int valid = Sf_True(f, Ht_IsShipValid(f, Sf_Item(f, SEAF("editedShips"), n - 1)));
				int here = valid >= 0 ? Sf_EqI(f, GF("currentHangar"), n - 1) : -1;
				int sput = here >= 0 ? Sf_True(f, SEAF("fromSputnik")) : -1;
				if (sput > 0) {
					Se_SetLang(f, id, hangar, "Hint", valid ? "HINT_Hangar20" : "HINT_Hangar21");
				} else if (sput == 0 && here > 0) {
					Se_SetLang(f, id, hangar, "Hint", valid ? "HINT_Hangar15" : "HINT_Hangar14");
				} else if (sput == 0) {
					Se_SetUse(f, id, Sf_Str(f, hangar), "Hint", "HINT_Hangar10", HT);
				}
			}
		}
		Sf_Release(f, f->failed ? f->n : mark);
	}
}

static void Ht_UpdateHangars(scriptFrame_t *f, int id) {
	SE_SUB(f, Ht_UpdateHangars_(&g, id));
}

static PyObject *HT_UpdateHangars(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Ht_UpdateHangars(&f, id);
	return Sf_None(&f);
}

/* ShowHangarPanel(id): the items of the Hangar panel show; Sell works for an intact ship of a hangar */
static PyObject *HT_ShowHangarPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, SE);
	Sf_Import(&f, "ShipMaker");
	Sf_From(&f, "Menu", "LinkUD");
	Sf_From(&f, "Menu", "LinkLR");
	Se_SetOO(&f, id, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), G("currentPilot")), "Visible", Sf_Int(&f, 1));
	Ht_Show(&f, id, ht_pilot, 1);
	if (Ht_Pilots(&f) > 0) {
		Se_Link(&f, "LinkLR", id, "PrevPilot", "NextPilot");
		Se_Link(&f, "LinkUD", id, "NextPilot", "PilotName");
		Se_Link(&f, "LinkUD", id, "PrevPilot", "PilotName");
		Se_Link(&f, "LinkUD", id, "PilotName", "Sell");
		Se_Link(&f, "LinkUD", id, "PilotName", "Exit");
	}
	if (Sf_True(&f, SEA("fromSputnik")) == 0) {
		Ht_Show(&f, id, ht_sell, 1);
	}
	Ht_SellActive(&f, id, 0);
	int c = Sf_True(&f, SEA("fromSputnik"));
	c = c < 0 ? -1 : !c;
	if (c > 0) {
		c = Sf_Compare(&f, G("currentHangar"), ">", Sf_Int(&f, -1));
	}
	if (c > 0) {
		c = Sf_Compare(&f, G("currentHangar"), "<", Sf_Int(&f, 9));
	}
	if (c > 0) {
		c = Sf_Compare(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "!=", Py_None);
	}
	if (c > 0) {
		c = Sf_Compare(&f, Sf_GetItem(&f, SEA("destroyedShips"), G("currentHangar")), "!=", Sf_Int(&f, 1));
	}
	if (c > 0) {
		Ht_SellActive(&f, id, 1);
		Se_Link(&f, "LinkLR", id, "Sell", "Exit");
	}
	Ht_UpdateHangars(&f, id);
	return Sf_None(&f);
}

/* eval(text) in a function of HangarTab (its globals, locals the dict of the function's variables); a name with
 * attributes ("prevShip.Model") is looked up directly */
static PyObject *Ht_Eval(scriptFrame_t *f, PyObject *text, PyObject *locals) {
	PyObject *g = Sf_Mod(f, HT, "__dict__"), *v;
	if (g && text && PyString_Check(text) && PyDict_Check(locals) &&
		(int)strlen(PyString_AsString(text)) == PyString_Size(text)) {
		const char *s = PyString_AsString(text);
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		int found = Script_NameChain(locals, g, s, &v);
		if (found) {
			return Sf_Keep(f, found > 0 ? v : NULL);
		}
	}
	return Sf_Ask(f, "__builtin__", "eval", "(OOO)", text, g, locals);
}

/* ComputeShipTotalCost(cad="prevShip"): the price of the ship cad names (an expression the function evaluates): the
 * model, its engines, its weapons and the armour above the minimum */
static PyObject *Ht_TotalCost_(scriptFrame_t *f, PyObject *cad) {
	Sf_Import(f, SE);
	Sf_Import(f, "ShipMaker");
	Sf_Import(f, "string");
	PyObject *loc = Sf_Build(f, "{s:O}", "cad", cad);
	static const char *const mods[] = { SE, "ShipMaker", "string" };
	for (int i = 0; i < 3 && !f->failed; i++) {
		PyObject *m = Sf_Keep(f, PyImport_ImportModule((char *)mods[i]));
		Sf_SetItem(f, loc, Sf_Str(f, mods[i]), m);
	}
	PyObject *shipName = Ht_Eval(f, Sf_Cat(f, "Os", cad, ".Model"), loc);
	PyObject *cost = Sf_ToInt(f, Sf_Ask(f, "ShipMaker", "GetShipPrice", "(O)", shipName));
	int n = Sf_Len(f, Sf_GetItem(f, SEAF("shipsEnginesLayouts"), shipName));
	for (int i = 0; i < n && !f->failed; i++) {
		int mark = f->n;
		PyObject *num = Ht_Eval(f, Sf_Cat(f, "OsO", cad, ".NEng", Sf_ReprI(f, i)), loc);
		if (Sf_Compare(f, num, ">", Sf_Int(f, 0)) > 0) {
			PyObject *price = Sf_Ask(f, "ShipMaker", "GetEnginePrice", "(O)", Ht_Eval(f, Sf_Cat(f, "OsO", cad, ".MEng",
				Sf_ReprI(f, i)), loc));
			cost = Sf_Op(f, cost, '+', Sf_Op(f, num, '*', price));
		}
		Sf_ReleaseKeep(f, mark, &cost, f->failed ? 0 : 1);
	}
	PyObject *slotId = Sf_Build(f, "(ssss)", "A", "B", "C", "D");
	PyObject *weapId = Sf_Build(f, "{s:s,s:s,s:s,s:s,s:s,s:s}", "Vulcan", "00", "Devastator", "03", "Tesla", "02", "ATPC",
		"04", "Inferno", "05", "Swarm", "01");
	PyObject *keys = Sf_Method(f, weapId, "keys", "()");
	for (int k = 0; !f->failed; k++) {
		int mark = f->n;
		PyObject *weapon = Sf_Next(f, keys, k);
		if (!weapon) {
			break;
		}
		for (int i = 0; i < 2 && !f->failed; i++) {
			PyObject *wid = Sf_Op(f, Sf_Item(f, slotId, i), '+', Sf_GetItem(f, weapId, weapon));
			PyObject *loaded = Ht_Eval(f, Sf_Cat(f, "OsO", cad, ".Weap", wid), loc);
			if (Sf_True(f, loaded) > 0) {
				PyObject *wc = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "WeaponCosts"), Sf_Ask(f, "string", "upper", "(O)", weapon));
				cost = Sf_Op(f, cost, '+', wc);
			}
		}
		Sf_ReleaseKeep(f, mark, &cost, f->failed ? 0 : 1);
	}
	PyObject *hull = Sf_Op(f, Ht_Eval(f, Sf_Cat(f, "Os", cad, ".MaxLife"), loc), '-', Ht_Eval(f, Sf_Cat(f, "Os", cad,
		".MinLife"), loc));
	return Sf_Op(f, cost, '+', Sf_Op(f, hull, '*', Sf_Mod(f, "ShipMaker", "armorPieceCost")));
}

static PyObject *Ht_TotalCost(scriptFrame_t *f, PyObject *cad) {
	PyObject *r = NULL;
	if (!f->failed) {
		scriptFrame_t g = { 0 };
		r = Se_JoinValue(f, &g, Ht_TotalCost_(&g, cad));
	}
	return r;
}

static PyObject *HT_ComputeShipTotalCost(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "cad", NULL };
	scriptFrame_t f = { 0 };
	PyObject *cad = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &cad)) {
		return NULL;
	}
	return Sf_Return(&f, Ht_TotalCost(&f, cad ? Sf_Ref(&f, cad) : Sf_Str(&f, "prevShip")));
}

/* the money prediction: the text sign + cost, red (Green 0) when money - cost < 0 (money NULL: never red) */
static void Ht_Predict(scriptFrame_t *f, int id, const char *sign, PyObject *cost, PyObject *money) {
	Se_SetI(f, id, "MoneyPredict", "Green", 255);
	if (money && Sf_Compare(f, Sf_Op(f, money, '-', cost), "<", Sf_Int(f, 0)) > 0) {
		Se_SetI(f, id, "MoneyPredict", "Green", 0);
	}
	Se_SetO(f, id, "MoneyPredict", "Text", Sf_Cat(f, "sO", sign, Sf_Repr(f, cost)));
	Se_SetI(f, id, "MoneyPredict", "Visible", 1);
}

/* ShowShipMiniInfo(id, control, lostFocus): the mini information of the hangar under the cursor, linked to it by the
 * lines; the texts of an available hangar, a destroyed or an incomplete ship */
static void Ht_ShowMini_(scriptFrame_t *f, int id, PyObject *control) {
	Sf_Import(f, SE);
	Sf_Import(f, "ShipMaker");
	Sf_Import(f, "string");
	PyObject *i = NULL;
	int isAux = Sf_EqS(f, control, "HangarAux");
	if (isAux > 0) {
		if (Se_Multi(f, 0) == 0) {
			Se_SetI(f, id, "AuxShipText", "Visible", 1);
		}
		Sf_SetGlobal(f, HT, "miniShip", Sf_Item(f, SEAF("editedShips"), 9));
		int t = Sf_True(f, GF("miniShip"));
		int stop = t < 0 ? -1 : !t;
		if (stop == 0) {
			stop = Sf_EqI(f, GF("currentHangar"), 9);
		}
		if (stop != 0) {
			return;
		}
	} else if (isAux == 0) {
		Se_SetI(f, id, "AuxShipText", "Visible", 0);
		int l = Sf_Len(f, control) - 1;
		i = Sf_OpI(f, Sf_ToInt(f, Sf_Item(f, control, l)), '-', 1);
		Sf_SetGlobal(f, HT, "miniShip", Sf_GetItem(f, SEAF("editedShips"), i));
		if (Sf_Compare(f, i, "==", GF("currentHangar")) != 0) {
			return;
		}
	}
	Se_SetI(f, id, "MiniShipInfo", "Visible", 1);
	if (Sf_True(f, GF("miniShip")) > 0) {
		Se_SetI(f, id, "MiniMaxSpeedText", "Visible", 0);
		Se_SetI(f, id, "MiniMaxSpeed", "Visible", 0);
		Se_SetO(f, id, "HangarShip", "TargetName", Sf_Attr(f, GF("miniShip"), "Name"));
		Se_SetI(f, id, "HangarShip", "Visible", 1);
		PyObject *speed = Sf_ToInt(f, Sf_OpI(f, Sf_Attr(f, GF("miniShip"), "MaxSpeed"), '/', 28));
		Se_SetO(f, id, "MiniMaxSpeed", "Text", Sf_Repr(f, speed));
		Se_SetI(f, id, "MiniHullText", "Visible", 1);
		Se_SetO(f, id, "MiniHullValue", "Text", Sf_Repr(f, Sf_Attr(f, GF("miniShip"), "MaxLife")));
		Se_SetI(f, id, "MiniHullValue", "Visible", 1);
		Se_SetI(f, id, "MiniWeight", "Visible", 1);
		Se_SetO(f, id, "MiniCurrentWeight", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, GF("miniShip"), "AcMass"))));
		Se_SetI(f, id, "MiniCurrentWeight", "Visible", 1);
		PyObject *ton = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "ShipTon"), Sf_Ask(f, "string", "upper", "(O)",
			Sf_Attr(f, GF("miniShip"), "Model")));
		Se_SetO(f, id, "MiniMaxWeight", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Item(f, ton, 2))));
		Se_SetI(f, id, "MiniMaxWeight", "Visible", 1);
		Se_SetI(f, id, "MiniSlashWeight", "Visible", 1);
		PyObject *whole;
		PyObject *dec = Ht_Decimal(f, Sf_Attr(f, GF("miniShip"), "BoostTime"), &whole, 0);
		Se_SetO(f, id, "MiniBoostRechargeTime", "Text", Sf_Cat(f, "OsO", Sf_Repr(f, whole), ".", dec));
		if (Sf_True(f, Ht_IsShipValid(f, GF("miniShip"))) > 0) {
			static const char *const show[] = { "MiniMaxSpeedText", "MiniMaxSpeed", "MiniSpeedRamp1", "MiniSpeedRamp2",
				"MiniBoostRechargeTime", "MiniBoostRechargeTimeText", NULL };
			Ht_Show(f, id, show, 1);
			PyObject *off = Sf_Own(f, HT, "GetSpeedRampOffsets", "(O)", GF("miniShip"));
			Sf_Own(f, HT, "ScaleSpeedRamp", "(iOOi)", id, Sf_Item(f, off, 0), Sf_Item(f, off, 1), 1);
		}
	}
	PyObject *hangarName = isAux > 0 ? Sf_Str(f, "HangarAux") : Sf_Cat(f, "sO", "Hangar", Sf_Repr(f, Sf_OpI(f, i, '+', 1)));
	PyObject *sx = Sf_Op(f, Sf_Ask(f, "SScorer", "Get", "(iOs)", id, hangarName, "X"), '+', Sf_OpI(f,
		Sf_Ask(f, "SScorer", "Get", "(iOs)", id, hangarName, "W"), '/', 2));
	PyObject *sy = Sf_Op(f, Sf_Ask(f, "SScorer", "Get", "(iOs)", id, hangarName, "Y"), '+', Sf_OpI(f,
		Sf_Ask(f, "SScorer", "Get", "(iOs)", id, hangarName, "H"), '/', 2));
	PyObject *ex = Sf_Op(f, Sf_Ask(f, "SScorer", "Get", "(iss)", id, "MiniShipInfo", "X"), '+', Sf_OpI(f,
		Sf_Ask(f, "SScorer", "Get", "(iss)", id, "MiniShipInfo", "W"), '/', 2));
	PyObject *ey = Sf_Op(f, Sf_Ask(f, "SScorer", "Get", "(iss)", id, "MiniShipInfo", "Y"), '+',
		Sf_Ask(f, "SScorer", "Get", "(iss)", id, "MiniShipInfo", "H"));
	Se_SetO(f, id, "VLine", "X", ex);
	Se_SetO(f, id, "VLine", "Y", ey);
	Se_SetI(f, id, "VLine", "ScaleX", 2);
	Se_SetO(f, id, "VLine", "ScaleY", Sf_OpI(f, Sf_Op(f, sy, '-', ey), '+', 2));
	Se_SetI(f, id, "VLine", "Visible", 1);
	int invert = Sf_Compare(f, sx, ">", ex);
	if (invert > 0) {
		PyObject *t = ex;
		ex = sx;
		sx = t;
	}
	Se_SetO(f, id, "HLine", "X", sx);
	Se_SetO(f, id, "HLine", "Y", sy);
	Se_SetO(f, id, "HLine", "ScaleX", Sf_Op(f, ex, '-', sx));
	Se_SetI(f, id, "HLine", "ScaleY", 2);
	Se_SetI(f, id, "HLine", "Visible", 1);
	Se_SetO(f, id, "MLine", "X", invert > 0 ? ex : sx);
	Se_SetO(f, id, "MLine", "Y", Sf_OpI(f, sy, '+', 1));
	Se_SetI(f, id, "MLine", "Visible", 1);
	if (isAux == 0) {
		int valid = Sf_True(f, Ht_IsShipValid(f, GF("miniShip")));
		int ship = valid >= 0 ? Sf_True(f, GF("miniShip")) : -1;
		int lost = 0;
		if (ship > 0) {
			int mp = Se_Multi(f, 0);
			lost = mp == 0 ? Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), i), 1) : mp < 0 ? -1 : 0;
		}
		if (ship == 0) {
			Ht_Words(f, id, "ShipEdit_AvailableHangar", "AvailableHangarText_", 1);
		} else if (lost > 0) {
			Ht_Words(f, id, "ShipEdit_DestroyedShip", "DestroyedShipText_", 1);
		} else if (lost == 0 && valid == 0) {
			Ht_Words(f, id, "ShipEdit_UncompletedShip", "UncompletedShipText_", 1);
		}
		if (Se_Multi(f, 0) == 0) {
			Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), i), 0);
		}
	}
}

static void Ht_ShowMini(scriptFrame_t *f, int id, PyObject *control) {
	SE_SUB(f, Ht_ShowMini_(&g, id, control));
}

static PyObject *HT_ShowShipMiniInfo(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control, *lost;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Ht_ShowMini(&f, id, control);
	return Sf_None(&f);
}

/* ShowRebuildShipMoney(id, control, lostFocus): the cost of rebuilding the destroyed ship of the hangar (Sputnik's
 * screen), then its mini information */
static PyObject *HT_ShowRebuildShipMoney(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control, *lost;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Sf_Import(&f, "SputnikInterface");
	Se_SetI(&f, id, "MoneyPredict", "Green", 255);
	PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
	int l = Sf_Len(&f, control) - 1;
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, Sf_Item(&f, control, l)), '-', 1);
	PyObject *cost = Sf_Own(&f, HT, "ComputeShipTotalCost", "(O)", Sf_Cat(&f, "sOs", "ShipEdit.editedShips[", Sf_Repr(&f, n),
		"]"));
	Ht_Predict(&f, id, "-", cost, money);
	Sf_SetMod(&f, "SputnikInterface", "rebuildCost", cost);
	Ht_ShowMini(&f, id, control);
	return Sf_None(&f);
}

/* HideRebuildShipMoney(id, control, lostFocus) */
static PyObject *HT_HideRebuildShipMoney(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control, *lost;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	Se_SetI(&f, id, "MoneyPredict", "Visible", 0);
	Ht_HideMini(&f, id);
	return Sf_None(&f);
}

/* the ship list of the type buttons: auxShipsToEditList, or shipsToEditList while it is empty; *rebuild: the current
 * entry is the destroyed ship */
static PyObject *Ht_ShipsList(scriptFrame_t *f, int *rebuild) {
	*rebuild = 0;
	int empty = Sf_Compare(f, SEAF("auxShipsToEditList"), "==", Sf_Keep(f, PyList_New(0)));
	if (empty > 0) {
		return SEAF("shipsToEditList");
	}
	PyObject *list = SEAF("auxShipsToEditList");
	if (Sf_EqS(f, Sf_GetItem(f, list, GF("currentShipToEdit")), "Rebuild") > 0) {
		*rebuild = 1;
	}
	return list;
}

/* AuxShowMoney(id, symbol): the money a new ship costs ("-") or a sold or rebuilt one gives or costs (its whole
 * price, shipTotalCost) */
static void Ht_AuxShowMoney_(scriptFrame_t *f, int id, const char *symbol) {
	Sf_Import(f, SE);
	Sf_Import(f, "ShipMaker");
	Sf_Import(f, "string");
	byte minus = !strcmp(symbol, "-");
	PyObject *money = Sf_Ask(f, "Scrap", "GetMoney", "()");
	int rebuild;
	PyObject *list = Ht_ShipsList(f, &rebuild);
	PyObject *cost;
	if (minus && !rebuild) {
		cost = Sf_ToInt(f, Sf_Ask(f, "ShipMaker", "GetShipPrice", "(O)", Sf_GetItem(f, list, GF("currentShipToEdit"))));
	} else {
		if (Sf_True(f, GF("prevShip")) == 0) {
			return;
		}
		cost = Ht_TotalCost(f, Sf_Str(f, "prevShip"));
		Sf_SetGlobal(f, HT, "shipTotalCost", cost);
	}
	Ht_Predict(f, id, symbol, cost, minus ? money : NULL);
}

static void Ht_AuxShowMoney(scriptFrame_t *f, int id, const char *symbol) {
	SE_SUB(f, Ht_AuxShowMoney_(&g, id, symbol));
}

static PyObject *Ht_MoneyArgs(PyObject *args, const char *symbol) {
	scriptFrame_t f = { 0 };
	PyObject *control, *lost;
	int id;
	if (!PyArg_ParseTuple(args, "iOO", &id, &control, &lost)) {
		return NULL;
	}
	if (symbol) {
		Ht_AuxShowMoney(&f, id, symbol);
	} else {
		Se_SetI(&f, id, "MoneyPredict", "Visible", 0);
	}
	return Sf_None(&f);
}

static PyObject *HT_AuxShowMoney(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	char *symbol;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "is", &id, &symbol)) {
		return NULL;
	}
	Ht_AuxShowMoney(&f, id, symbol);
	return Sf_None(&f);
}

static PyObject *HT_ShowMoneyBuyingShip(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_MoneyArgs(args, "-");
}

static PyObject *HT_ShowMoneySellingShip(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_MoneyArgs(args, "+");
}

static PyObject *HT_HideMoney(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_MoneyArgs(args, NULL);
}

/* ScaleSpeedRamp(id, offsetX, offsetY, isMiniShip=0): the speed ramp of the ship information (or of the mini
 * information): the time to the top speed (offsetX, at most 1) and the top speed (offsetY), both 0 to 1 */
static PyObject *HT_ScaleSpeedRamp(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "offsetX", "offsetY", "isMiniShip", NULL };
	scriptFrame_t f = { 0 };
	PyObject *ox, *oy, *mini = NULL;
	int id;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "iOO|O", names, &id, &ox, &oy, &mini)) {
		return NULL;
	}
	if (Sf_Compare(&f, ox, ">", Sf_Float(&f, 1.0)) > 0) {
		ox = Sf_Float(&f, 1.0);
	}
	int isMini = mini ? Sf_True(&f, mini) : 0;
	PyObject *one = Sf_Int(&f, 1), *two = Sf_Int(&f, 2);
	if (isMini > 0) {
		PyObject *sx = Sf_Float(&f, 20.0 / 32.0), *sy = Sf_Float(&f, 53.0 / 64.0);
		Se_SetO(&f, id, "MiniSpeedRamp1", "X", Sf_OpI(&f, G("baseMiniShipInfoX"), '+', 93));
		Se_SetO(&f, id, "MiniSpeedRamp1", "Y", Sf_Op(&f, Sf_OpI(&f, G("baseMiniShipInfoY"), '-', 51), '+', Sf_Op(&f,
			Sf_Op(&f, one, '-', oy), '*', Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "MiniSpeedRamp1", "H"))));
		Se_SetO(&f, id, "MiniSpeedRamp1", "ScaleX", Sf_Op(&f, Sf_Op(&f, two, '*', ox), '*', sx));
		Se_SetO(&f, id, "MiniSpeedRamp1", "ScaleY", Sf_Op(&f, oy, '*', sy));
		PyObject *w = Sf_Op(&f, Sf_OpI(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "MiniSpeedRamp1", "W"), '*', 2), '*', ox);
		Se_SetO(&f, id, "MiniSpeedRamp2", "X", Sf_Op(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "MiniSpeedRamp1", "X"), '+',
			w));
		Se_SetO(&f, id, "MiniSpeedRamp2", "Y", Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "MiniSpeedRamp1", "Y"));
		Se_SetO(&f, id, "MiniSpeedRamp2", "ScaleX", Sf_Op(&f, Sf_OpI(&f, Sf_Op(&f, one, '-', ox), '*', 2), '*', sx));
		Se_SetO(&f, id, "MiniSpeedRamp2", "ScaleY", Sf_Op(&f, oy, '*', sy));
	} else if (isMini == 0) {
		Se_SetO(&f, id, "SpeedRamp1", "X", Sf_Op(&f, G("baseShipInfoX"), '+', G("speedRampX")));
		Se_SetO(&f, id, "SpeedRamp1", "Y", Sf_Op(&f, Sf_Op(&f, G("baseShipInfoY"), '+', G("speedRampY")), '+', Sf_Op(&f,
			Sf_Op(&f, one, '-', oy), '*', Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "H"))));
		Se_SetO(&f, id, "SpeedRamp1", "ScaleX", Sf_Op(&f, two, '*', ox));
		Se_SetO(&f, id, "SpeedRamp1", "ScaleY", oy);
		Se_SetO(&f, id, "SpeedRamp2", "ScaleX", Sf_OpI(&f, Sf_Op(&f, one, '-', ox), '*', 2));
		Se_SetO(&f, id, "SpeedRamp2", "ScaleY", oy);
		PyObject *w = Sf_Op(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "W"), '*',
			Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "ScaleX"));
		Se_SetO(&f, id, "SpeedRamp2", "X", Sf_Op(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "X"), '+', w));
		Se_SetO(&f, id, "SpeedRamp2", "Y", Sf_Ask(&f, "SScorer", "Get", "(iss)", id, "SpeedRamp1", "Y"));
	}
	return Sf_None(&f);
}

/* GetMaxSpeedTime(v): the time the ship v needs to reach its top speed */
static PyObject *Ht_MaxSpeedTime_(scriptFrame_t *f, PyObject *v) {
	Sf_Import(f, "math");
	PyObject *ft = Sf_Attr(f, v, "FrwTrust");
	PyObject *neg = f->failed ? NULL : Sf_Keep(f, PyNumber_Negative(ft));
	PyObject *sq = Sf_Op(f, Sf_Attr(f, v, "FrwTrust"), '*', Sf_Attr(f, v, "FrwTrust"));
	PyObject *k = Sf_Op(f, Sf_Op(f, Sf_Op(f, Sf_Int(f, 4), '*', Sf_Attr(f, v, "FrwTrust")), '*', Sf_Attr(f, v, "Fric")), '*',
		Sf_Attr(f, v, "MaxSpeed"));
	PyObject *in = Sf_Op(f, sq, '+', k);
	PyObject *num = Sf_Op(f, neg, '+', Sf_Ask(f, "math", "sqrt", "(O)", in));
	PyObject *den = Sf_Op(f, Sf_Op(f, Sf_Int(f, 2), '*', Sf_Attr(f, v, "FrwTrust")), '*', Sf_Attr(f, v, "Fric"));
	return Sf_Op(f, num, '/', den);
}

static PyObject *Ht_MaxSpeedTime(scriptFrame_t *f, PyObject *v) {
	PyObject *r = NULL;
	if (!f->failed) {
		scriptFrame_t g = { 0 };
		r = Se_JoinValue(f, &g, Ht_MaxSpeedTime_(&g, v));
	}
	return r;
}

static PyObject *HT_GetMaxSpeedTime(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *v;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	return Sf_Return(&f, Ht_MaxSpeedTime(&f, v));
}

/* GetSpeedRampOffsets(v): (GetMaxSpeedTime(v) / 0.3, v.MaxSpeed / 19600.0) */
static PyObject *HT_GetSpeedRampOffsets(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *v;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	PyObject *a = Sf_Op(&f, Sf_Own(&f, HT, "GetMaxSpeedTime", "(O)", v), '/', Sf_Float(&f, 0.3));
	PyObject *b = Sf_Op(&f, Sf_Attr(&f, v, "MaxSpeed"), '/', Sf_Float(&f, 19600.0));
	return Sf_Return(&f, Sf_Build(&f, "(OO)", a, b));
}

/* CmSec2Value(val): cm/s to the speed unit of the screen, int(val/28) */
static PyObject *HT_CmSec2Value(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *v;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	return Sf_Return(&f, Sf_ToInt(&f, Sf_OpI(&f, v, '/', 28)));
}

/* AddReferenceShipControls(id): the mini information of a hangar: the words of the three hangar texts, the ship
 * window, the speed, boost, hull and weight, the lines to the hangar */
static void Ht_Reference_(scriptFrame_t *f, int id) {
	Sf_Import(f, "string");
	PyObject *x = Sf_Op(f, GF("baseMiniShipInfoX"), '+', Sf_OpI(f, Sf_Int(f, 144), '/', 2));
	static const char *const texts[][2] = { { "ShipEdit_UncompletedShip", "UncompletedShipText_" },
		{ "ShipEdit_DestroyedShip", "DestroyedShipText_" }, { "ShipEdit_AvailableHangar", "AvailableHangarText_" } };
	for (int k = 0; k < 3 && !f->failed; k++) {
		PyObject *y = Sf_OpI(f, GF("baseMiniShipInfoY"), '-', 30);
		PyObject *words = Sf_Ask(f, "string", "split", "(O)", Sf_Str(f, Script_Lang(texts[k][0])));
		for (int i = 0; !f->failed; i++) {
			int mark = f->n;
			PyObject *w = Sf_Next(f, words, i);
			if (!w) {
				break;
			}
			PyObject *vars = Sf_Build(f, "{s:O,s:O,s:O,s:O}", "name", Sf_Cat(f, "sO", texts[k][1], w), "word", w, "x", x, "y", y);
			Se_Def(f, id, "word", vars, HT);
			y = Sf_OpI(f, y, '+', 22);
			Sf_ReleaseKeep(f, mark, &y, f->failed ? 0 : 1);
		}
	}
	Se_Def(f, id, "reference", NULL, HT);
}

static void Ht_Reference(scriptFrame_t *f, int id) {
	SE_SUB(f, Ht_Reference_(&g, id));
}

static PyObject *HT_AddReferenceShipControls(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Ht_Reference(&f, id);
	return Sf_None(&f);
}

/* SetHangarPanel(id): the Hangar panel (the layout of shipedit.def): the mini information, the hint, the emergency
 * ship and the nine hangars, the ship information, the money, Sell and Exit, the pilots, the ship type buttons */
static PyObject *HT_SetHangarPanel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, SE);
	Sf_Import(&f, "ShipMaker");
	Sf_From(&f, "Menu", "LinkLR");
	Sf_From(&f, "Menu", "LinkUD");
	Ht_Reference(&f, id);
	int mp = Se_Multi(&f, 0);
	int sput = Sf_True(&f, SEA("fromSputnik"));
	int aux9 = Sf_EqI(&f, G("currentHangar"), 9);
	PyObject *vars = Sf_Build(&f, "{s:i,s:i,s:i}", "mp", mp, "sputnik", sput, "aux9", aux9);
	Se_Def(&f, id, "panel", vars, HT);
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			PyObject *v = Sf_Build(&f, "{s:i,s:i,s:i,s:i}", "n", i * 3 + j + 1, "x", 52 + 42 * j, "y", 204 + 42 * i, "sputnik",
				sput);
			Se_Def(&f, id, "hangar", v, HT);
		}
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	vars = Sf_Build(&f, "{s:i,s:i,s:O}", "mp", mp, "sputnik", sput, "money", Sf_Repr(&f, Sf_Ask(&f, "Scrap", "GetMoney",
		"()")));
	Se_Def(&f, id, "info", vars, HT);
	int pilots = Ht_Pilots(&f);
	if (pilots > 0) {
		PyObject *list = Sf_Mod(&f, "ShipMaker", "PilotsList");
		int n = Sf_Len(&f, list);
		for (int i = 0; i < n && !f.failed; i++) {
			int mark = f.n;
			PyObject *name = Sf_Item(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), i);
			PyObject *model = Sf_Slice(&f, Sf_Item(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), i), 2, 0x7fffffff);
			Se_Def(&f, id, "pilot", Sf_Build(&f, "{s:O,s:O}", "name", name, "model", model), HT);
			Sf_Release(&f, f.failed ? f.n : mark);
		}
		Se_SetOO(&f, id, Sf_GetItem(&f, Sf_Mod(&f, "ShipMaker", "PilotsList"), G("currentPilot")), "Visible", Sf_Int(&f, 1));
		PyObject *nick = Sf_Str(&f, "");
		if (Ht_Pilots(&f) > 0) {
			nick = Sf_Ask(&f, "Scrap", "Get", "(s)", "PlayerName");
			if (Sf_EqS(&f, nick, "") > 0) {
				nick = Sf_Str(&f, Script_Lang("ShipEdit_Pilot"));
			}
		}
		Se_Def(&f, id, "pilots", Sf_Build(&f, "{s:O}", "nick", nick), HT);
	}
	int noship = 0;
	if (pilots > 0) {
		noship = Sf_Compare(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "==", Py_None);
	}
	vars = Sf_Build(&f, "{s:i,s:i,s:i}", "mp", mp, "pilots", pilots > 0, "noship", noship);
	Se_Def(&f, id, "links", vars, HT);
	return Sf_None(&f);
}

/* the boost time as the screen shows it: int(t) "." the first decimal */
static PyObject *Ht_BoostText(scriptFrame_t *f, PyObject *ship) {
	PyObject *whole;
	PyObject *dec = Ht_Decimal(f, Sf_Attr(f, ship, "BoostTime"), &whole, 0);
	return Sf_Cat(f, "OsO", Sf_Repr(f, whole), ".", dec);
}

static const char *const ht_speedInfo[] = { "MaxSpeedText", "MaxSpeed", "BoostRechargeTime", "BoostRechargeTimeUnits",
	"BoostRechargeTimeText", "SpeedRamp1", "SpeedRamp2", NULL };

/* ShowShipInfo(id, showAmmoInfo=1): the information of the ship on the platform (of the selected hangar on Sputnik's
 * screen): speed, hull, weight, boost, the speed ramp for a valid ship, the weapon counts */
static void Ht_ShowInfo_(scriptFrame_t *f, int id, int ammo) {
	Sf_Import(f, "ShipMaker");
	Sf_Import(f, SE);
	Sf_Import(f, HT);
	Sf_Import(f, "string");
	int sput = Sf_True(f, SEAF("fromSputnik"));
	PyObject *ship = sput > 0 ? Sf_GetItem(f, SEAF("editedShips"), Se_Mod(f, HT, "currentHangar")) : sput == 0 ? GF("prevShip") :
		NULL;
	if (Sf_Compare(f, ship, "!=", Py_None) <= 0) {
		return;
	}
	Sf_SetAttrI(f, ship, "Active", 1);
	Se_SetO(f, id, "MaxSpeed", "Text", Sf_Repr(f, Sf_Own(f, HT, "CmSec2Value", "(O)", Sf_Attr(f, ship, "MaxSpeed"))));
	Se_SetI(f, id, "Slash", "Visible", 1);
	Se_SetO(f, id, "Hull", "Text", Sf_Repr(f, Sf_Attr(f, ship, "MaxLife")));
	Se_SetO(f, id, "CurrentWeight", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, ship, "AcMass"))));
	PyObject *ton = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "ShipTon"), Sf_Ask(f, "string", "upper", "(O)", Sf_Attr(f, ship,
		"Model")));
	Se_SetO(f, id, "MaxWeight", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Item(f, ton, 2))));
	Se_SetO(f, id, "BoostRechargeTime", "Text", Ht_BoostText(f, ship));
	int c = Sf_True(f, Ht_IsShipValid(f, Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar"))));
	if (c > 0) {
		c = Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), GF("currentHangar")), 0);
		if (c == 0) {
			c = Se_Multi(f, 0);
		}
	}
	if (c > 0) {
		PyObject *off = Sf_Own(f, HT, "GetSpeedRampOffsets", "(O)", Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar")));
		Sf_Own(f, HT, "ScaleSpeedRamp", "(iOO)", id, Sf_Item(f, off, 0), Sf_Item(f, off, 1));
	}
	if (c >= 0) {
		Ht_Show(f, id, ht_speedInfo, c);
	}
	if (ammo) {
		static const char *const items[][2] = { { "NumVulcans", "nWeap00" }, { "NumDevastators", "nWeap03" },
			{ "NumTeslas", "nWeap02" }, { "NumATPCs", "nWeap04" }, { "NumSwarmMissiles", "nWeap01" },
			{ "NumInfernoMissiles", "nWeap05" } };
		for (int i = 0; i < 6; i++) {
			Se_SetO(f, id, items[i][0], "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, ship, items[i][1]))));
		}
	}
	if (Sf_True(f, SEAF("fromSputnik")) > 0) {
		Sf_SetAttrI(f, ship, "Active", 0);
	}
}

static void Ht_ShowInfo(scriptFrame_t *f, int id, int ammo) {
	SE_SUB(f, Ht_ShowInfo_(&g, id, ammo));
}

static PyObject *HT_ShowShipInfo(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "showAmmoInfo", NULL };
	scriptFrame_t f = { 0 };
	PyObject *ammo = NULL;
	int id;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "i|O", names, &id, &ammo)) {
		return NULL;
	}
	int a = ammo ? Sf_True(&f, ammo) : 1;
	if (a >= 0) {
		Ht_ShowInfo(&f, id, a);
	}
	return Sf_None(&f);
}

/* HideShipInfo(id): the ship information empties */
static void Ht_HideInfo(scriptFrame_t *f, int id) {
	static const char *const text[] = { "MaxSpeed", NULL, "Hull", "CurrentWeight", "MaxWeight", NULL, "NumVulcans",
		"NumDevastators", "NumInfernoMissiles", "NumSwarmMissiles", "NumTeslas", "NumATPCs" };
	static const char *const hide[] = { NULL, "MaxSpeedText", NULL, NULL, NULL, "Slash" };
	for (int i = 0; i < 12; i++) {
		if (text[i]) {
			Se_SetS(f, id, text[i], "Text", "");
		} else {
			Se_SetI(f, id, hide[i], "Visible", 0);
		}
	}
	static const char *const rest[] = { "SpeedRamp1", "SpeedRamp2", "BoostRechargeTime", "BoostRechargeTimeText", NULL };
	Ht_Show(f, id, rest, 0);
}

static PyObject *HT_HideShipInfo(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Ht_HideInfo(&f, id);
	return Sf_None(&f);
}

/* SetShipTypeButtonsState(id, state): the ship type button buys ("Build") or rebuilds ("Rebuild") */
static void Ht_TypeState(scriptFrame_t *f, int id, byte rebuild) {
	Se_SetI(f, id, "ShipType", "SpriteIndex", rebuild ? 14 : 11);
	Se_SetI(f, id, "ShipType", "FocusSpriteIndex", rebuild ? 15 : 12);
	Se_SetUse(f, id, Sf_Str(f, "ShipType"), "Hint", rebuild ? "HINT_Hangar7" : "HINT_Hangar6", HT);
}

static PyObject *HT_SetShipTypeButtonsState(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *state;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &state)) {
		return NULL;
	}
	if (Sf_EqS(&f, state, "Build") > 0) {
		Ht_TypeState(&f, id, 0);
	} else if (Sf_EqS(&f, state, "Rebuild") > 0) {
		Ht_TypeState(&f, id, 1);
	}
	return Sf_None(&f);
}

/* the weapon sprites (on) or the ammo sprites (off) of the ship information */
static void Ht_WeaponSprites(scriptFrame_t *f, int id, long on) {
	static const char *const weapons[] = { "VulcanSpr", "DevastatorSpr", "TeslaSpr", "ATPCSpr", "InfernoSpr", "SwarmSpr",
		NULL };
	static const char *const ammo[] = { "ProjectilesAmmo", "EnergyAmmo", "MissilesAmmo", NULL };
	Ht_Show(f, id, weapons, on);
	Ht_Show(f, id, ammo, !on);
}

/* the slot counts of the ship information empty */
static void Ht_NoSlots(scriptFrame_t *f, int id) {
	Se_SetS(f, id, "NumSlotsParticleWeapons", "Text", "");
	Se_SetS(f, id, "NumSlotsEnergyWeapons", "Text", "");
	Se_SetS(f, id, "NumSlotsMissileWeapons", "Text", "");
}

/* SelectShipToEdit(id): the plan of the ship type to buy (a hologram on the platform) or the destroyed ship to
 * rebuild, its slots and information; the other tabs are off; the hologram sound starts */
static void Ht_SelectShipToEdit_(scriptFrame_t *f, int id) {
	Sf_From(f, "Menu", "LinkUD");
	static const char *const mods[] = { SE, HT, "Sound", "string" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(f, mods[i]);
	}
	static const char *const off[] = { "MaxSpeedText", "MaxSpeed", "Slash", "BoostRechargeTime", "BoostRechargeTimeText",
		NULL };
	Ht_Show(f, id, off, 0);
	int rebuild = Se_Multi(f, 0);
	rebuild = rebuild < 0 ? -1 : !rebuild;
	if (rebuild > 0) {
		rebuild = Sf_EqS(f, Sf_GetItem(f, SEAF("auxShipsToEditList"), GF("currentShipToEdit")), "Rebuild");
	}
	PyObject *list = rebuild > 0 ? SEAF("shipsToEditList") : SEAF("auxShipsToEditList");
	PyObject *text = Sf_Lang(f, Sf_Cat(f, "sO", "SHIP_", Sf_Ask(f, "string", "upper", "(O)", Sf_GetItem(f, list,
		GF("currentShipToEdit")))));
	Ht_Show(f, id, ht_shipType, 1);
	Se_SetO(f, id, "ShipTypeName", "Text", text);
	int build = rebuild == 0 ? 1 : Se_Multi(f, 0);
	if (build > 0) {
		Sf_Own(f, HT, "CreateEmptyShip", "(O)", Sf_GetItem(f, SEAF("auxShipsToEditList"), GF("currentShipToEdit")));
		Ht_TypeState(f, id, 0);
	} else if (build == 0) {
		Sf_SetGlobal(f, HT, "prevShip", Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar")));
		Sf_SetAttrI(f, GF("prevShip"), "Active", 1);
		Sf_SetAttrI(f, GF("prevShip"), "RenderEffect", 1);
		Sf_Ask(f, SE, "StopRotateLoop", "()");
		Sf_Ask(f, SE, "StartRotateLoop", "()");
		Ht_TypeState(f, id, 1);
	}
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_WeaponSprites(f, id, 0);
	Se_SetO(f, id, "NumSlotsParticleWeapons", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, GF("prevShip"), "nSlot00"))));
	Se_SetO(f, id, "NumSlotsMissileWeapons", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, GF("prevShip"), "nSlot01"))));
	Se_SetO(f, id, "NumSlotsEnergyWeapons", "Text", Sf_Repr(f, Sf_ToInt(f, Sf_Attr(f, GF("prevShip"), "nSlot02"))));
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_ShowInfo(f, id, 0);
	Se_TabsOn(f, id, SEAF("tabs"), 0);
	if (Sf_True(f, GF("holograms")) == 0) {
		Sf_SetGlobal(f, HT, "holograms", Sf_Int(f, 1));
		PyObject *cam = Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
		Sf_Ask(f, "SSound", "PlaySound", "(OOd)", Sf_Attr(f, cam, "camPos"), Sf_Mod(f, "Sound", "HologramStartSound"), 0.35);
		Sf_Ask(f, "SSound", "CreatePS", "(s)", "EditHologramLoopSound");
		Sf_Ask(f, "SSound", "SetPS", "(ssO)", "EditHologramLoopSound", "Sound", Sf_Mod(f, "Sound", "HologramLoopSound"));
		static const char *const xyz[] = { "X", "Y", "Z" };
		for (int i = 0; i < 3; i++) {
			Sf_Ask(f, "SSound", "SetPS", "(ssO)", "EditHologramLoopSound", xyz[i], Sf_Item(f, Sf_Attr(f, cam, "camPos"), i));
		}
		Sf_Ask(f, "SSound", "SetPS", "(ssi)", "EditHologramLoopSound", "Loop", 1);
		Sf_Ask(f, "SSound", "SetPS", "(ssd)", "EditHologramLoopSound", "Volume", 0.17);
	}
}

static void Ht_SelectShipToEdit(scriptFrame_t *f, int id) {
	SE_SUB(f, Ht_SelectShipToEdit_(&g, id));
}

static PyObject *HT_SelectShipToEdit(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Ht_SelectShipToEdit(&f, id);
	return Sf_None(&f);
}

/* PrevShipToEdit(id, control), NextShipToEdit(id, control): the ship type before or after the current one */
static PyObject *Ht_ShipToEdit(PyObject *args, int step) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, SE);
	int rebuild = Sf_EqS(&f, Sf_GetItem(&f, SEA("auxShipsToEditList"), G("currentShipToEdit")), "Rebuild");
	Sf_SetGlobal(&f, HT, "currentShipToEdit", Sf_OpI(&f, G("currentShipToEdit"), step < 0 ? '-' : '+', 1));
	if (step < 0) {
		if (Sf_Compare(&f, G("currentShipToEdit"), "<", Sf_Int(&f, 0)) > 0) {
			Sf_SetGlobal(&f, HT, "currentShipToEdit", Sf_Int(&f, Sf_Len(&f, SEA("auxShipsToEditList")) - 1));
		}
	} else {
		int n = Sf_Len(&f, SEA("auxShipsToEditList"));
		if (n >= 0 && Sf_EqI(&f, G("currentShipToEdit"), n) > 0) {
			Sf_SetGlobal(&f, HT, "currentShipToEdit", Sf_Int(&f, 0));
		}
	}
	if (rebuild == 0) {
		Ht_TypeState(&f, id, 0);
		if (Sf_Compare(&f, G("prevShip"), "!=", Py_None) > 0) {
			Sf_Method(&f, G("prevShip"), "Del", "()");
			Sf_SetGlobal(&f, HT, "prevShip", Py_None);
		}
	} else if (rebuild > 0) {
		Ht_TypeState(&f, id, 1);
		Sf_SetAttrI(&f, G("prevShip"), "Active", 0);
	}
	Ht_SelectShipToEdit(&f, id);
	if (Sf_Compare(&f, G("prevShip"), "!=", Py_None) > 0) {
		Sf_Ask(&f, SE, "StartRotateLoop", "()");
	}
	return Sf_None(&f);
}

static PyObject *HT_PrevShipToEdit(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_ShipToEdit(args, -1);
}

static PyObject *HT_NextShipToEdit(PyObject *self, PyObject *args) {
	(void)self;
	return Ht_ShipToEdit(args, 1);
}

/* the hangar n of ShipEdit.editedShips has no ship, or (single player) a destroyed one: 1, 0, -1 */
static int Ht_Empty(scriptFrame_t *f, PyObject *n) {
	int c = Sf_Compare(f, Sf_GetItem(f, SEAF("editedShips"), n), "==", Py_None);
	if (c == 0) {
		c = Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), n), 1);
		if (c > 0) {
			int mp = Se_Multi(f, 0);
			c = mp < 0 ? -1 : !mp;
		}
	}
	return c;
}

/* SelectHangar(id, control="", skipanim=0): the hangar currentHangar is selected: its ship comes up on the platform
 * (the animation, or at once with skipanim), an empty or destroyed one shows the ship plans when the platform is up */
static void Ht_SelectHangar_(scriptFrame_t *f, int id, PyObject *control, int skipanim) {
	static const char *const links[] = { "LinkLR", "LinkUD", "UnLinkUD", "UnLinkLR" };
	for (int i = 0; i < 4; i++) {
		Sf_From(f, "Menu", links[i]);
	}
	Sf_Import(f, SE);
	Sf_Ask(f, "SSound", "SetPS", "(ssd)", "EditHologramLoopSound", "Volume", 0.0);
	if (Sf_Compare(f, GF("prevHangar"), "==", GF("currentHangar")) != 0) {
		return;
	}
	int c = Sf_True(f, Sf_GetItem(f, SEAF("editedShips"), GF("prevHangar")));
	c = c < 0 ? -1 : !c;
	if (c == 0) {
		c = Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), GF("prevHangar")), 1);
		if (c > 0) {
			int mp = Se_Multi(f, 0);
			c = mp < 0 ? -1 : !mp;
		}
	}
	if (c > 0 && Sf_Compare(f, GF("prevShip"), "!=", Py_None) > 0) {
		Sf_SetAttrI(f, GF("prevShip"), "Active", 0);
	}
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_Show(f, id, ht_shipType, 0);
	int pilots = Ht_Pilots(f);
	if (pilots > 0) {
		if (Sf_Compare(f, Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar")), "==", Py_None) > 0) {
			Se_Link(f, "LinkLR", id, "NextShip", "PrevPilot");
		} else {
			Se_SetS(f, id, "PrevPilot", "Left", "Hangar6");
		}
		Se_Link(f, "UnLinkUD", id, "PilotName", "Sell");
		Se_Link(f, "LinkUD", id, "PilotName", "Exit");
		Se_SetS(f, id, "Exit", "Left", "");
		Se_SetS(f, id, "Exit", "Up", "PilotName");
	} else if (pilots == 0) {
		Se_Link(f, "LinkLR", id, "NextShip", "Exit");
	}
	Sf_Release(f, f->failed ? f->n : 0);
	int empty = Ht_Empty(f, GF("currentHangar"));
	if (empty > 0) {
		static const char *const nums[] = { "NumVulcans", "NumDevastators", "NumInfernoMissiles", "NumSwarmMissiles",
			"NumTeslas", "NumATPCs" };
		for (int i = 0; i < 6; i++) {
			Se_SetS(f, id, nums[i], "Text", "");
		}
		if (Sf_Compare(f, Sf_GetItem(f, SEAF("editedShips"), GF("prevHangar")), "!=", Py_None) > 0) {
			static const char *const texts[] = { "MaxSpeed", "Hull", "CurrentWeight", "MaxWeight" };
			for (int i = 0; i < 4; i++) {
				Se_SetS(f, id, texts[i], "Text", "");
			}
			Ht_NoSlots(f, id);
		}
		Ht_SellActive(f, id, 0);
		Se_SetS(f, id, "Hangar9", "Right", "PrevShip");
		Se_SetS(f, id, "Hangar6", "Right", "PrevShip");
		Se_SetS(f, id, "Hangar3", "Right", "PrevShip");
		if (Se_Multi(f, 0) == 0) {
			Se_SetS(f, id, "HangarAux", "Right", "PrevShip");
		}
		if (Sf_Compare(f, Sf_GetItem(f, SEAF("editedShips"), GF("prevHangar")), "!=", Py_None) > 0) {
			Ht_HideInfo(f, id);
		}
		Ht_PlatformButtons(f, id, 0);
		Sf_Ask(f, "SScorer", "SetOnNext", "(is)", id, "");
		Sf_Ask(f, "SScorer", "SetOnPrev", "(is)", id, "");
		int lost = Sf_EqI(f, Sf_GetItem(f, SEAF("destroyedShips"), GF("currentHangar")), 1);
		if (lost > 0) {
			int mp = Se_Multi(f, 0);
			lost = mp < 0 ? -1 : !mp;
		}
		if (lost > 0) {
			Se_SetOO(f, id, control, "Hint", Sf_Str(f, Script_Lang("HINT_Hangar13")));
		}
		if (skipanim) {
			Sf_Ask(f, SE, "ChangeShip", "(OOi)", Py_None, Py_None, 0);
			Sf_Ask(f, SE, "VerifyChangeShip", "(OOi)", Py_None, Py_None, 0);
		} else {
			Sf_Ask(f, SE, "LaunchPlatformAnim", "()");
		}
		return;
	}
	if (empty < 0) {
		return;
	}
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_WeaponSprites(f, id, 1);
	int notAux = Sf_Compare(f, GF("currentHangar"), "!=", Sf_Int(f, 9));
	if (notAux > 0) {
		if (Ht_Pilots(f) > 0) {
			Se_Link(f, "LinkUD", id, "PilotName", "Exit");
			Se_Link(f, "LinkUD", id, "PilotName", "Sell");
		}
		Ht_SellActive(f, id, 1);
		Sf_Ask(f, "SScorer", "SetOnNext", "(is)", id, "ShipEdit.ChangeTabRight");
		Sf_Ask(f, "SScorer", "SetOnPrev", "(is)", id, "ShipEdit.ChangeTabLeft");
		Se_Link(f, "LinkLR", id, "Hangar3", "Sell");
		Se_Link(f, "LinkLR", id, "Hangar9", "Sell");
		Se_Link(f, "LinkLR", id, "Hangar6", "Sell");
		if (Se_Multi(f, 0) == 0) {
			Se_SetS(f, id, "HangarAux", "Right", "Sell");
		}
		Se_Link(f, "LinkLR", id, "Sell", "Exit");
		Se_TabsOn(f, id, SEAF("tabs"), 1);
		Se_TabRing(f, id, SEAF("tabs"), "LinkLR");
	} else if (notAux == 0) {
		Ht_PlatformButtons(f, id, 0);
		Sf_Import(f, SE);
		Se_TabsOn(f, id, SEAF("tabs"), 0);
		Se_TabRing(f, id, SEAF("tabs"), "UnLinkLR");
		if (Se_Multi(f, 0) == 0) {
			Se_Link(f, "LinkLR", id, "HangarAux", "Exit");
		}
		Se_Link(f, "LinkLR", id, "Hangar3", "Exit");
		Se_Link(f, "LinkLR", id, "Hangar9", "Exit");
		Se_Link(f, "LinkLR", id, "Hangar6", "Exit");
		Ht_SellActive(f, id, 0);
		Sf_Ask(f, "SScorer", "SetOnNext", "(is)", id, "");
		Sf_Ask(f, "SScorer", "SetOnPrev", "(is)", id, "");
	}
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_NoSlots(f, id);
	int isAux = Sf_Compare(f, control, "!=", Sf_Str(f, "HangarAux"));
	if (isAux > 0) {
		int valid = Sf_True(f, Ht_IsShipValid(f, Sf_GetItem(f, SEAF("editedShips"), GF("currentHangar"))));
		if (valid >= 0) {
			Se_SetOO(f, id, control, "Hint", Sf_Str(f, Script_Lang(valid ? "HINT_Hangar12" : "HINT_Hangar14")));
		}
	} else if (isAux == 0) {
		Se_SetOO(f, id, control, "Hint", Sf_Str(f, Script_Lang("HINT_Hangar18")));
	}
	Sf_Release(f, f->failed ? f->n : 0);
	Ht_HideInfo(f, id);
	if (Sf_EqS(f, control, "HangarAux") > 0) {
		Sf_Ask(f, "SScorer", "SetDefault", "(is)", id, "HangarAux");
	} else {
		Sf_Ask(f, "SScorer", "SetDefault", "(iO)", id, Sf_Cat(f, "sO", "Hangar", Sf_Repr(f, Sf_OpI(f, GF("currentHangar"), '+',
			1))));
	}
	Ht_PlatformButtons(f, id, 0);
	Sf_Ask(f, SE, "LaunchPlatformAnim", "()");
}

static void Ht_SelectHangar(scriptFrame_t *f, int id, PyObject *control, int skipanim) {
	SE_SUB(f, Ht_SelectHangar_(&g, id, control, skipanim));
}

static PyObject *HT_SelectHangar(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "control", "skipanim", NULL };
	scriptFrame_t f = { 0 };
	PyObject *control = NULL, *skip = NULL;
	int id;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "i|OO", names, &id, &control, &skip)) {
		return NULL;
	}
	control = control ? Sf_Ref(&f, control) : Sf_Str(&f, "");
	int s = skip ? Sf_True(&f, skip) : 0;
	if (s >= 0) {
		Ht_SelectHangar(&f, id, control, s);
	}
	return Sf_None(&f);
}

/* SelectItem(id, control): a hangar clicked: it becomes currentHangar and is selected */
static PyObject *HT_SelectItem(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, SE);
	Sf_From(&f, "Menu", "LinkUD");
	Sf_SetGlobal(&f, HT, "holograms", Sf_Int(&f, 0));
	Sf_Ask(&f, "SSound", "SetPS", "(ssi)", "EditHologramSound", "Loop", 0);
	Sf_Ask(&f, "SSound", "SetPS", "(ssd)", "EditHologramSound", "Volume", 0.0);
	Sf_Ask(&f, "SSound", "StopAllSounds", "()");
	int aux = Sf_EqS(&f, control, "HangarAux");
	int last = aux == 0 ? Sf_Len(&f, control) - 1 : 0;
	if (aux > 0 || (aux == 0 && Sf_EqS(&f, Sf_Slice(&f, control, 0, last), "Hangar") > 0)) {
		Sf_SetGlobal(&f, HT, "prevHangar", G("currentHangar"));
		Sf_SetGlobal(&f, HT, "currentHangar", aux ? Sf_Int(&f, 9) : Sf_OpI(&f, Sf_ToInt(&f, Sf_Item(&f, control, last)), '-', 1));
		Ht_SelectHangar(&f, id, control, 0);
		Ht_HideMini(&f, id);
	}
	Ht_UpdateHangars(&f, id);
	return Sf_None(&f);
}

/* CreateEmptyShip(kind): the plan of a new ship of the model kind on the platform (prevShip) */
static PyObject *HT_CreateEmptyShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *kind;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &kind)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, SE);
	PyObject *pos = Sf_Item(&f, SEA("editShipPosDummy"), 1);
	Sf_SetGlobal(&f, HT, "prevShip", Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", kind, Sf_Item(&f, pos, 0), Sf_Item(&f,
		pos, 1), Sf_Item(&f, pos, 2), "Car"));
	Sf_SetAttrI(&f, G("prevShip"), "ActiveCMask", 0);
	Sf_SetAttr(&f, G("prevShip"), "Ang", Se_FrontAng(&f, SEA("editShipPosDummy"), NULL));
	PyObject *life = Sf_Item(&f, Sf_Ask(&f, "ShipMaker", "GetShipTon", "(O)", kind), 0);
	Sf_Ask(&f, "Vehicles", "SetProperties", "(OOOsssssssi)", Sf_Attr(&f, G("prevShip"), "Name"), kind, life, "P_None", "", "",
		"", "", "0,0,0,0,0,0", "Pilot", 1);
	Sf_SetAttrI(&f, G("prevShip"), "RenderEffect", 1);
	Sf_Ask(&f, SE, "SetupForEdit", "([O])", G("prevShip"));
	return Sf_None(&f);
}

/* BuyShip(id, control): the ship type shown is bought (or the destroyed ship rebuilt) for the hangar */
static PyObject *HT_BuyShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	Sf_From(&f, "Menu", "LinkLR");
	Sf_From(&f, "Menu", "LinkUD");
	static const char *const mods[] = { SE, "ShipMaker", "string", "Sound" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	int rebuild;
	PyObject *list = Ht_ShipsList(&f, &rebuild);
	if (Se_Multi(&f, 0) == 0) {
		PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
		PyObject *cost;
		if (rebuild) {
			cost = Sf_Own(&f, HT, "ComputeShipTotalCost", "(O)", Sf_Cat(&f, "sOs", "ShipEdit.editedShips[", Sf_Repr(&f,
				G("currentHangar")), "]"));
		} else {
			cost = Sf_ToInt(&f, Sf_Ask(&f, "ShipMaker", "GetShipPrice", "(O)", Sf_GetItem(&f, list, G("currentShipToEdit"))));
		}
		if (Sf_Compare(&f, Sf_Op(&f, money, '-', cost), "<", Sf_Int(&f, 0)) != 0) {
			return Sf_None(&f);
		}
		PyObject *left = Sf_Op(&f, money, '-', cost);
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", left);
		Se_SetO(&f, id, "Money", "Text", Sf_Repr(&f, left));
		Sf_SetItem(&f, SEA("destroyedShips"), G("currentHangar"), Sf_Int(&f, 0));
		int n = Sf_Len(&f, SEA("auxShipsToEditList"));
		for (int i = 0; i < n && !f.failed; i++) {
			int mark = f.n;
			if (Sf_EqS(&f, Sf_Item(&f, SEA("auxShipsToEditList"), i), "Rebuild") > 0) {
				Sf_SetGlobal(&f, HT, "currentShipToEdit", Sf_Int(&f, i));
				break;
			}
			Sf_Release(&f, mark);
		}
		Sf_SetMod(&f, SE, "auxShipsToEditList", Sf_Keep(&f, PyList_New(0)));
		n = Sf_Len(&f, SEA("shipsToEditList"));
		for (int i = 0; i < n && !f.failed; i++) {
			int mark = f.n;
			Sf_Method(&f, SEA("auxShipsToEditList"), "append", "(O)", Sf_Item(&f, SEA("shipsToEditList"), i));
			Sf_Release(&f, f.failed ? f.n : mark);
		}
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_SetGlobal(&f, HT, "holograms", Sf_Int(&f, 0));
	Sf_Ask(&f, "SSound", "SetPS", "(ssi)", "EditHologramLoopSound", "Loop", 0);
	Sf_Ask(&f, "SSound", "SetPS", "(ssd)", "EditHologramLoopSound", "Volume", 0.0);
	Sf_Ask(&f, "SSound", "StopAllSounds", "()");
	if (rebuild) {
		Sf_Method(&f, G("prevShip"), "SetFX", "(s)", "ShipEditBuild");
	} else {
		if (Sf_Compare(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "!=", Py_None) > 0) {
			Sf_Method(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "Del", "()");
		}
		Sf_Method(&f, G("prevShip"), "SetFX", "(s)", "ShipEditBuild");
		Sf_SetItem(&f, SEA("editedShips"), G("currentHangar"), G("prevShip"));
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Ht_WeaponSprites(&f, id, 1);
	Ht_Show(&f, id, ht_shipType, 0);
	Ht_NoSlots(&f, id);
	Ht_ShowInfo(&f, id, 1);
	Ht_UpdateHangars(&f, id);
	Se_TabsOn(&f, id, SEA("tabs"), 1);
	Se_TabRing(&f, id, SEA("tabs"), "LinkLR");
	Sf_Ask(&f, "SScorer", "SetOnNext", "(is)", id, "ShipEdit.ChangeTabRight");
	Sf_Ask(&f, "SScorer", "SetOnPrev", "(is)", id, "ShipEdit.ChangeTabLeft");
	Ht_SellActive(&f, id, 1);
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Ask(&f, "SSound", "Play", "(OO)", Sf_Mod(&f, "Sound", "ShipEditLoadShipSound"), Sf_Mod(&f, "Sound",
		"ShipEditLoadShipVolume"));
	if (Se_Multi(&f, 0) == 0) {
		Se_Link(&f, "LinkLR", id, "HangarAux", "Sell");
	}
	Se_Link(&f, "LinkLR", id, "Hangar3", "Sell");
	Se_Link(&f, "LinkLR", id, "Hangar9", "Sell");
	Se_Link(&f, "LinkLR", id, "Hangar6", "Sell");
	Se_Link(&f, "LinkLR", id, "Sell", "Exit");
	if (Ht_Pilots(&f) > 0) {
		Se_SetS(&f, id, "PrevPilot", "Left", "Hangar6");
		Se_Link(&f, "LinkUD", id, "PilotName", "Sell");
	}
	Ht_UpdateHangars(&f, id);
	Sf_Ask(&f, "SScorer", "SetDefault", "(iO)", id, Sf_Cat(&f, "sO", "Hangar", Sf_Repr(&f, Sf_OpI(&f, G("currentHangar"), '+',
		1))));
	return Sf_None(&f);
}

/* SellShip(id, control): the ship of the hangar is sold (its whole price in single player) */
static PyObject *HT_SellShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	PyObject *control;
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO", &id, &control)) {
		return NULL;
	}
	static const char *const links[] = { "LinkLR", "LinkUD", "UnLinkLR", "UnLinkUD" };
	for (int i = 0; i < 4; i++) {
		Sf_From(&f, "Menu", links[i]);
	}
	Sf_Import(&f, SE);
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "Sound");
	if (Sf_Compare(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "!=", Py_None) <= 0) {
		return Sf_None(&f);
	}
	if (Se_Multi(&f, 0) == 0) {
		PyObject *money = Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', G("shipTotalCost"));
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", money);
		Se_SetO(&f, id, "Money", "Text", Sf_Repr(&f, money));
		Sf_SetItem(&f, SEA("destroyedShips"), G("currentHangar"), Sf_Int(&f, 0));
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Sf_Method(&f, Sf_GetItem(&f, SEA("editedShips"), G("currentHangar")), "Del", "()");
	Sf_SetItem(&f, SEA("editedShips"), G("currentHangar"), Py_None);
	Sf_SetGlobal(&f, HT, "prevShip", Py_None);
	Ht_UpdateHangars(&f, id);
	Ht_SellActive(&f, id, 0);
	Sf_Ask(&f, "SSound", "Play", "(OO)", Sf_Mod(&f, "Sound", "ShipEditUnLoadSound"), Sf_Mod(&f, "Sound",
		"ShipEditUnLoadVolume"));
	Se_Link(&f, "LinkLR", id, "Hangar3", "Exit");
	Se_Link(&f, "LinkLR", id, "Hangar9", "Exit");
	Se_Link(&f, "LinkLR", id, "Hangar6", "Exit");
	if (Ht_Pilots(&f) > 0) {
		Se_Link(&f, "UnLinkUD", id, "PilotName", "Sell");
		Se_Link(&f, "LinkUD", id, "PilotName", "Exit");
	}
	Sf_Release(&f, f.failed ? f.n : 0);
	Ht_HideInfo(&f, id);
	/* the script unlinks the whole ring of tabs once per tab */
	PyObject *rest = Sf_Slice(&f, SEA("tabs"), 1, 0x7fffffff);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *t = Sf_Next(&f, rest, i);
		if (!t) {
			break;
		}
		PyObject *name = Sf_Cat(&f, "sO", "ShipEditTab", t);
		static const char *const vars[] = { "Highlight", "TextColorAlpha", "TextColorRed", "TextColorGreen",
			"TextColorBlue" };
		static const long vals[] = { 0, 255, 161, 161, 161 };
		for (int k = 0; k < 5; k++) {
			Se_SetOO(&f, id, name, vars[k], Sf_Int(&f, vals[k]));
		}
		Se_SetOO(&f, id, name, "OnAccept", Sf_Str(&f, ""));
		Se_TabRing(&f, id, SEA("tabs"), "UnLinkLR");
		Sf_Release(&f, f.failed ? f.n : mark);
	}
	Sf_Ask(&f, "SScorer", "SetOnNext", "(is)", id, "");
	Sf_Ask(&f, "SScorer", "SetOnPrev", "(is)", id, "");
	Sf_SetGlobal(&f, HT, "prevHangar", Sf_Int(&f, -1));
	Ht_SelectHangar(&f, id, Sf_Cat(&f, "sO", "Hangar", Sf_Repr(&f, G("currentHangar"))), 1);
	Sf_SetGlobal(&f, HT, "prevHangar", G("currentHangar"));
	return Sf_None(&f);
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
