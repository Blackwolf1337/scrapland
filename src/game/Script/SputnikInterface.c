/*
 * The SputnikInterface module in C (scripts/shipedit/sputnikinterface.py, issue #2): the hangar screen Sputnik opens,
 * where the player picks the ship to fly (a destroyed one is rebuilt for rebuildCost). The ship editor itself
 * (ShipEdit, HangarTab) is called through its modules. offsetX, offsetY, rebuildCost and EndSputnikCallBack stay
 * variables of the module.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the function table rows name the entries.
 * rebuildCost and EndSputnikCallBack are read where the script reads them and the callback cleared at once.
 * The variables of ShipEdit and HangarTab (editedShips with the ship handles, destroyedShips, currentHangar,
 * fromSputnik) are read and set as their attributes, destroyedShips changed in place (OBJLIB 7). Menu,
 * ShipEdit, HangarTab, SaveGame and the module's own functions (through its attributes, as the script called them) go
 * through their refs (PORT_CALL, the seam OBJLIB 5 (9c)); the scorer calls too (stage 7), EndSputnikCallBack is
 * called as a value (OBJLIB 5 (9c)); SetMoney goes through its seam (fc-r7b-jy records it).
 */
#include "scrap/app.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#include "ShipEdit.h"

#define MOD "SputnikInterface"

/* AddDefaultPanelContent(id): the editor background with the Hangar tab only */
static byte SI_AddDefaultPanelContentN(const nativeVal_t *id) {
	if (!Mode_Import("ShipEdit")) {
		return 0;
	}
	nativeVal_t tabs;
	Native_SetEmpty(&tabs, NV_LIST);
	Native_SetStr(Native_Append(&tabs), "Hangar");
	byte ok = PORT_CALL(ShipEdit, AddShipEditTabs, NULL, "vv", id, &tabs) &&
		PORT_CALL(ShipEdit, AddDefaultPanelContent, NULL, "vi", id, 1L);
	Native_Free(&tabs);
	return ok;
}

/* ShipEdit.editedShips[i] (the list of the ship handles read as the attribute first, as the C port read it) */
static byte SI_Edited(const nativeVal_t *i, byte iOk, nativeVal_t *e) {
	nativeVal_t ships;
	byte ok = LvN_ModGet("ShipEdit", "editedShips", &ships) && iOk && Mode_Sub(&ships, i, e);
	Native_Free(&ships);
	return ok;
}

/* ShipEdit.editedShips[i].Active = 0 */
static byte SI_Stop(const nativeVal_t *i) {
	nativeVal_t e;
	Native_SetNone(&e);
	byte ok = SI_Edited(i, 1, &e) && LvN_SetI(&e, "Active", 0);
	Native_Free(&e);
	return ok;
}

/* the ship of the hangar the save game flew stops: if ShipEdit.editedShips[prev]: ....Active = 0 */
static byte SI_StopPrev(void) {
	nativeVal_t prev, e;
	Native_SetNone(&e);
	byte iOk = Mode_SaveInt("Hangar.iHangarShip", &prev);
	byte ok = SI_Edited(&prev, iOk, &e) && (!Native_True(&e) || SI_Stop(&prev));
	Native_Free(&e);
	Native_Free(&prev);
	return ok;
}

/* Exit through the module, as the C port calls it */
static byte SI_Exit(const nativeVal_t *id, const nativeVal_t *control) {
	return PORT_CALL(SputnikInterface, ExitSputnikInterface, NULL, "vv", id, control);
}

/* the save game flies hangar (its repr as the save variable) */
static byte SI_Fly(const nativeVal_t *hangar) {
	nativeVal_t rep;
	Mode_Repr(hangar, &rep);
	byte ok = Mode_SetSaveVar("Hangar.iHangarShip", &rep) && SI_Stop(hangar);
	Native_Free(&rep);
	return ok;
}

/* HangarTab.currentHangar into slot (freed first) */
static byte SI_Current(nativeVal_t *slot) {
	Native_Free(slot);
	return LvN_ModGet("HangarTab", "currentHangar", slot);
}

/* the default control of the hangar being flown: HangarAux, Hangar<n + 1>, else Hangar1 */
static byte SI_Default(const nativeVal_t *id, modeVals_t *t) {
	nativeVal_t *cur = Mode_V(t), *n = Mode_V(t), *rep = Mode_V(t), *hangar = Mode_V(t);
	if (!SI_Current(cur)) {
		return 0;
	}
	int aux = Mode_CmpI(cur, "==", 9);
	if (aux < 0) {
		return 0;
	}
	if (aux) {
		return PORT_CALL(SScorer, SetDefault, NULL, "vs", id, "HangarAux");
	}
	int pos = SI_Current(cur) ? Mode_CmpI(cur, ">=", 0) : -1;
	if (pos < 0) {
		return 0;
	}
	if (pos) {
		if (!SI_Current(cur) || !Native_Arith('+', cur, Mode_I(t, 1), n)) {
			return 0;
		}
		Mode_Repr(n, rep);
		if (!Mode_Cat(t, hangar, 2, Mode_S(t, "Hangar"), rep)) {
			return 0;
		}
		return PORT_CALL(SScorer, SetDefault, NULL, "vv", id, hangar);
	}
	return PORT_CALL(SScorer, SetDefault, NULL, "vs", id, "Hangar1");
}

/* EnterSputnikInterface(id): the hangars of the save game, the one being flown selected */
static byte SI_EnterSputnikInterfaceN(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hint = Mode_V(&t), *cad = Mode_V(&t), *text = Mode_V(&t), *lost = Mode_V(&t), *cur = Mode_V(&t);
	Loading_Init();
	if (!Mode_Import("Menu") || !Mode_Import("ShipEdit") || !Mode_Import("HangarTab") ||
		!PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) || !PORT_CALL(Menu, StartNewMenu, NULL, "v", id) ||
		!PORT_CALL(SputnikInterface, AddDefaultPanelContent, NULL, "v", id)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("HINT_NoMoney", hint);
	if (!PORT_CALL(SScorer, SetOnSpecialHint, NULL, "vsv", id, "ShipEdit.ShowSpecialHint", hint) ||
		!PORT_CALL(ShipEdit, CleanEditedShips, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < 9; i++) {
		char var[0x20];
		Str_Printf(var, sizeof(var), "Hangar.HangarShip%ld", i + 1);
		Native_Free(cad);
		Mode_SaveVar(var, cad);
		if (!Native_IsStr(cad, "") && !PORT_CALL(ShipEdit, AssignShip, NULL, "vi", cad, i)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(cad);
	Mode_SaveVar("Hangar.HangarShipAux", cad);
	if (!PORT_CALL(ShipEdit, AssignShip, NULL, "vi", cad, 9L)) {
		return Mode_Done(&t, 0);
	}
	LvN_ModSetI("ShipEdit", "fromSputnik", 1);
	Mode_SaveVar("Hangar.DestroyedShips", text);
	if (!Native_EvalText(MOD, text, lost)) {
		return Mode_Done(&t, 0);
	}
	/* ShipEdit.destroyedShips = lost; ShipEdit.destroyedShips[9] = 0 on the object the module holds */
	Mode_SetModVar("ShipEdit", "destroyedShips", lost);
	if (!Mode_ModDictPut("ShipEdit", "destroyedShips", Mode_I(&t, 9), Mode_I(&t, 0)) ||
		!Mode_SaveInt("Hangar.iHangarShip", cur)) {
		return Mode_Done(&t, 0);
	}
	Mode_SetModVar("HangarTab", "currentHangar", cur);
	if (!PORT_CALL(HangarTab, SetHangarPanel, NULL, "v", id) ||
		!PORT_CALL(HangarTab, SetSellVisible, NULL, "vi", id, 0L) || !SI_Default(id, &t) || !SI_Current(cur) ||
		!SI_Stop(cur) || !PORT_CALL(HangarTab, ShowHangarPanel, NULL, "v", id) ||
		!PORT_CALL(HangarTab, ShowShipInfo, NULL, "v", id) ||
		!PORT_CALL(SScorer, SetOnCancel, NULL, "vs", id, "SputnikInterface.ExitSputnikInterface")) {
		return Mode_Done(&t, 0);
	}
	Loading_End();
	return Mode_Done(&t, 1);
}

/* ExitSputnikInterface(id, control): the money and the destroyed ships saved, back to the game, the callback once */
static byte SI_ExitSputnikInterfaceN(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lost = Mode_V(&t), *text = Mode_V(&t), *cb = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_Import("HangarTab") || !Mode_Import("Menu") || !Mode_Import("SaveGame") || !Mode_Import("ShipEdit") ||
		!PORT_CALL(SaveGame, SaveMoney, NULL, "") || !LvN_ModGet("ShipEdit", "destroyedShips", lost)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(lost, text);
	if (!Mode_SetSaveVar("Hangar.DestroyedShips", text) || !PORT_CALL(Menu, BackToGame, NULL, "v", id) ||
		!Mode_G(MOD, "EndSputnikCallBack", cb)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(cb) || (Mode_CallArgs(cb, NULL, NULL, 0) && Mode_Put(MOD, "EndSputnikCallBack", NULL, none)));
}

/* Scrap.SetMoney(money) through its seam; a long int through the ref (its conversion of the engine's "i" with the
 * OverflowError of a long that does not fit) */
static byte SI_SetMoney(const nativeVal_t *money) {
	return money->type == NV_LONG ? PORT_CALL(Scrap, SetMoney, NULL, "v", money) : Mode_SetMoneySeam(money);
}

/* ShipEdit.destroyedShips[hangar] into d (freed first) */
static byte SI_Lost(const nativeVal_t *hangar, nativeVal_t *d) {
	nativeVal_t lost;
	Native_Free(d);
	byte ok = LvN_ModGet("ShipEdit", "destroyedShips", &lost) && Mode_Sub(&lost, hangar, d);
	Native_Free(&lost);
	return ok;
}

/* SelectShip(id, control): the hangar picked (HangarAux, Hangar1..9): the same, an intact valid ship, or a rebuild */
static byte SI_SelectShipN(const nativeVal_t *id, const nativeVal_t *control) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *last = Mode_V(&t), *n = Mode_V(&t), *hangar = Mode_V(&t), *cur = Mode_V(&t), *d = Mode_V(&t);
	nativeVal_t *e = Mode_V(&t), *v = Mode_V(&t), *money = Mode_V(&t), *cost = Mode_V(&t), *left = Mode_V(&t);
	if (!Mode_Import("HangarTab") || !Mode_Import("ShipEdit")) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(control, "HangarAux")) {
		return Mode_Done(&t, SI_StopPrev() && Mode_SetSaveVar("Hangar.iHangarShip", Mode_S(&t, "9")) && SI_Stop(Mode_I(&t, 9)) &&
			SI_Exit(id, control));
	}
	int len = Native_Size(control);
	if (len < 0 || !Native_SeqItem(control, len - 1, last) || !Native_ToInt(last, n) || !Native_Arith('-', n, Mode_I(&t, 1), hangar) ||
		!SI_Current(cur)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(hangar, cur)) {
		return Mode_Done(&t, SI_Exit(id, control));
	}
	if (!SI_Lost(hangar, d)) {
		return Mode_Done(&t, 0);
	}
	int valid = 0;
	if (!Native_Compare(d, Mode_I(&t, 0))) {
		if (!SI_Edited(hangar, 1, e) || !PORT_CALL(HangarTab, IsShipValid, v, "v", e)) {
			return Mode_Done(&t, 0);
		}
		valid = Native_True(v);
	}
	if (valid) {
		return Mode_Done(&t, SI_StopPrev() && SI_Fly(hangar) && SI_Exit(id, control));
	}
	if (!SI_Lost(hangar, d)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(d, Mode_I(&t, 1))) {
		return Mode_Done(&t, 1);
	}
	Native_SetInt(money, Mode_GetMoney());
	if (!Mode_G(MOD, "rebuildCost", cost)) {
		return Mode_Done(&t, 0);
	}
	int rich = Mode_Cmp(money, ">=", cost);
	if (rich <= 0) {
		return Mode_Done(&t, rich == 0);
	}
	return Mode_Done(&t, SI_StopPrev() && Native_Arith('-', money, cost, left) && SI_SetMoney(left) && SI_Fly(hangar) &&
		Mode_ModDictPut("ShipEdit", "destroyedShips", hangar, Mode_I(&t, 0)) && SI_Exit(id, control));
}

MODE_FN(SI, AddDefaultPanelContent, 1, SI_AddDefaultPanelContentN(&a[0]))
MODE_FN(SI, EnterSputnikInterface, 1, SI_EnterSputnikInterfaceN(&a[0]))
MODE_FN(SI, ExitSputnikInterface, 2, SI_ExitSputnikInterfaceN(&a[0]))
MODE_FN(SI, SelectShip, 2, SI_SelectShipN(&a[0], &a[1]))

const portFn_t g_SputnikInterfaceMethods[] = {
	PORT_FNK(DrawBackground, ShipEdit_DrawBackground),
	PORT_FN(AddDefaultPanelContent, SI_AddDefaultPanelContent),
	PORT_FN(EnterSputnikInterface, SI_EnterSputnikInterface),
	PORT_FN(ExitSputnikInterface, SI_ExitSputnikInterface),
	PORT_FN(SelectShip, SI_SelectShip),
	PORT_FNEND
};

PORTS(SputnikInterface)
