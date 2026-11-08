/*
 * The scripts of the level Junkyard in C (data03.packed levels/junkyard/scripts, issue #2): the Map module (the
 * junk points of the scrap collector, the elevator to the level the player came from), MapShipEdit (the keypad of
 * the ship editor and the elevator platform that only takes a valid ship) and Mission_RustyFirstContact (Berto brings
 * Dtritus, the first ship with the editor tutorial, the scene with Rusty and Sputnik). The plain functions are data
 * (data/levels/junkyard/<module>.def): MapShipEdit.InitStatus, Mission_RustyFirstContact.LoadMecanicScene,
 * RustyFirstSpeech, OnEndOfShipEditTutorial, TutorialMsg and TutorialContinue. MapSnd is data.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, the alarm and blueprint save
 * variables), MapShipEdit (LastPlayer, ArrowMissionState), Mission_RustyFirstContact (kb, rusty, Params).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of the
 * modules are read where the script reads them and written through at once (OBJLIB 5); the keypad, Rusty, the player
 * and HangarTab.prevShip are handle values (OBJLIB 7). Init, Elevator, OutMap, Traffic, ShipEdit, HangarTab, Menu,
 * MapShipEdit, Chars, SaveGame, MissionsFuncs, Scene, MakeChar, SputnikSound, CreditsText, CharsNPC and the functions
 * of the module go by name (OBJLIB 3), the scene tables too (Mode_Calls: each "Module.Func" by name). fc-r7b-jy
 * records many engine functions, so these go through their seams (OBJLIB 7): Scrap.AddScheduledFunc, CreateEntity,
 * CreateEntityList, Set, SetMoney, SetTimeSpeed, UsrEntity, ConsoleError, SScorer.Add, Set, SetDefault and
 * SetMsgText, SInput.SetActionSet, SSound.SetMusic and the SAct functions of the scene; Scrap.OpenPack,
 * PreloadLibrary and ClosePack (Mission_RustyFirstContact.InitStatus) and the default form of Scrap.Def stay by name.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "Levels.h"

#define MAP "Map"
#define MSE "MapShipEdit"
#define RF "Mission_RustyFirstContact"

/* ---- engine calls through their seams (OBJLIB 7: fc-r7b-jy records them) ---- */

/* the hook a case put at module.name: called (ok in *ok, 1), or 0 when the engine's own C function is there */
#define JY_SEAM(mod, name, ...) \
	PyObject *hook; \
	int seam = Native_Seam(mod, name, &hook); \
	if (seam) { \
		return seam > 0 && Native_HookCall(hook, NULL, __VA_ARGS__); \
	}

/* Scrap.SetMoney(i) */
static byte Jy_SetMoney(long i) {
	nativeVal_t v = { 0 };
	Native_SetInt(&v, i);
	return Mode_SetMoneySeam(&v);
}

/* Scrap.SetTimeSpeed(i) */
static byte Jy_TimeSpeed(long i) {
	JY_SEAM("Scrap", "SetTimeSpeed", "i", i);
	Mode_SetTimeSpeed((double)i);
	return 1;
}

/* SScorer.Add(id, item, type, flag) */
static byte Jy_ScorerAdd(const nativeVal_t *id, const char *item, const char *type, long flag) {
	JY_SEAM("SScorer", "Add", "vssi", id, item, type, flag);
	int i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(i);
	if (sc) {
		cScorer_AddItem(sc, item, type, flag != 0);
	}
	return 1;
}

/* SScorer.SetDefault(id, item) */
static byte Jy_SetDefault(const nativeVal_t *id, const char *item) {
	JY_SEAM("SScorer", "SetDefault", "vs", id, item);
	int i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(i);
	if (sc) {
		cScorer_SetDefault(sc, item);
	}
	return 1;
}

/* Scrap.CreateEntityList(name) (the TypeError of "s"; the CHECK of the engine) */
static byte Jy_NewList(const nativeVal_t *name) {
	JY_SEAM("Scrap", "CreateEntityList", "v", name);
	const char *n;
	if (!Native_ArgStr(name, 1, &n)) {
		return 0;
	}
	CHECK(cWorld_NewEntityList(g_pWorld, n));
	return 1;
}

/* ---- shared lines ---- */

/* Scrap.GetEntity(name).Del() */
static byte Jy_EntDel(const char *name) {
	nativeVal_t e = { 0 };
	Native_SetNone(&e);
	byte ok = LvN_Entity(name, &e) && Mode_VDel(&e);
	Native_Free(&e);
	return ok;
}

/* Scrap.GetEntity(Scrap.GetCam(0)).EntityLink = link */
static byte Jy_CamLink(const char *link) {
	nativeVal_t name = { 0 }, cam = { 0 };
	const char *s = cWorld_GetCamName(g_pWorld, 0);
	if (s) {
		LvN_Str(&name, s);
	} else {
		Native_SetNone(&name);
	}
	Native_SetNone(&cam);
	byte ok = Mode_EntityH(&name, &cam) && LvN_SetS(&cam, "EntityLink", link);
	Native_Free(&name);
	Native_Free(&cam);
	return ok;
}

/* Scrap.UsrEntity(0).Name */
static byte Jy_UsrName(nativeVal_t *out) {
	nativeVal_t usr = { 0 };
	Native_SetNone(out);
	byte ok = Mode_UsrHandle(&usr) && Mode_VGet(&usr, "Name", out);
	Native_Free(&usr);
	return ok;
}

/* the values a, b (the arguments of a function) as one tuple (Params) */
static void Jy_Tuple(nativeVal_t *out, const nativeVal_t *a, int n) {
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(out), &a[i]);
	}
}

/* time + d into out (time a float of the script) */
static byte Jy_At(const nativeVal_t *time, double d, nativeVal_t *out) {
	nativeVal_t x = { 0 };
	Native_SetFloat(&x, d);
	Native_SetNone(out);
	return Mode_Arith('+', time, &x, out);
}

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map, the junkyard; the editor elevator, the atmosphere, no traffic ships, the junk
 * points the scrap collector goes to */
static byte JunkyardMap_InitStatus(void) {
	static const char *const lists[] = { "AcNormalShips", "AcMediumShips", "AcHeavyShips" };
	nativeVal_t v = { 0 }, list = { 0 };
	Native_SetInt(&v, 0);
	if (!Mode_Import("Init") || !Mode_SetModVar("Init", "Outdoor", &v)) {
		return 0;
	}
	Native_SetInt(&v, 1);
	if (!Mode_SetModVar("Init", "isJunkYard", &v) || !Mode_Import("Elevator") ||
		!LV_CALL("Elevator", "SetDef", NULL, "ss", "EditShip", "DM_Door_EditShip_00") || !Mode_Import("OutMap") ||
		!LV_CALL("OutMap", "BrowseAtmospherePos", NULL, "") || !Mode_Import("Traffic")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		Native_SetEmpty(&v, NV_LIST);
		if (!Mode_SetModVar("Traffic", lists[i], &v)) {
			return 0;
		}
	}
	if (!Native_GVarGet("JunkPointsListName", &list)) {
		return 0;
	}
	byte ok = Jy_NewList(&list);
	if (ok) {
		Mode_DummyStart("DM_JunkPoint_*", 1);
	}
	while (ok) {
		nativeVal_t dummy = { 0 }, pos = { 0 }, name = { 0 }, c[3] = { 0 }, l = { 0 }, ang = { 0 }, a = { 0 };
		Mode_DummyNext(&dummy);
		if (dummy.type == NV_NONE) {
			break;
		}
		Native_SetNone(&pos);
		Native_SetNone(&name);
		Native_SetNone(&l);
		Native_SetNone(&ang);
		Native_SetNone(&a);
		for (int i = 0; i < 3; i++) {
			Native_SetNone(&c[i]);
		}
		ok = LvN_Item(&dummy, 1, &pos) && LvN_Item(&dummy, 0, &name) && LvN_Item(&pos, 0, &c[0]) && LvN_Item(&pos, 1, &c[1]) &&
			LvN_Item(&pos, 2, &c[2]) && LvN_Create(&name, &c[0], &c[1], &c[2], "Item", &l) && LvN_Item(&dummy, 2, &ang);
		if (ok) {
			Native_SetEmpty(&a, NV_TUPLE);
			ok = LvN_Item(&ang, 0, Native_Append(&a)) && LvN_Item(&ang, 1, Native_Append(&a));
			if (ok) {
				Native_SetInt(Native_Append(&a), 0);
			}
		}
		ok = ok && Mode_VSet(&l, "Ang", &a) && Mode_VSet(&l, "EntityList", &list) && LvN_SetI(&l, "Radius", 100);
		Native_Free(&dummy);
		Native_Free(&pos);
		Native_Free(&name);
		Native_Free(&l);
		Native_Free(&ang);
		Native_Free(&a);
		for (int i = 0; i < 3; i++) {
			Native_Free(&c[i]);
		}
	}
	Native_Free(&list);
	return ok;
}

/* Init(LevelPath): the editor elevator goes back to the previous level; no energy bar, no special action, the music */
static byte JunkyardMap_Init(void) {
	nativeVal_t save = { 0 }, prev = { 0 }, a = { 0 }, b = { 0 };
	if (!Mode_Import("Elevator")) {
		return 0;
	}
	Mode_SaveVar("PrevMap", &save);
	Native_SetNone(&prev);
	byte ok = Native_Slice(&save, 7, 0x7fffffff, &prev);
	if (ok) {
		Native_SetEmpty(&a, NV_LIST);
		LvN_Str(Native_Append(&a), "DM_Door_EditShip_00");
		Native_SetEmpty(&b, NV_LIST);
		Native_Copy(Native_Append(&b), &prev);
		ok = LV_CALL("Elevator", "ConnectElevators", NULL, "vv", &a, &b);
		Native_Free(&a);
		Native_Free(&b);
	}
	Native_Free(&save);
	Native_Free(&prev);
	return ok && Mode_GVarSetI("EnergyBarActive", 0) && Mode_GVarSetI("SpecialActionActive", 0) && LvN_SetMusic("Music/Track1.ogg", 1.0);
}

LV_CORE0(JunkyardMap_InitStatus)
LV_CORE0(JunkyardMap_Init)
LV_FN(JunkyardMap, InitStatusPy, 1, JunkyardMap_InitStatus_C)
LV_FN(JunkyardMap, InitPy, 1, JunkyardMap_Init_C)

PyMethodDef g_JunkyardMapMethods[] = {
	{ "InitStatus", JunkyardMap_InitStatusPy, METH_VARARGS, NULL },
	{ "Init", JunkyardMap_InitPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapShipEdit ---- */

/* EnterEditShip(UsrName, CharName, optId, FullOption): the ship editor opens; the keypad off, the player frozen */
static byte MapShipEdit_EnterEditShip(void) {
	nativeVal_t kb = { 0 }, v = { 0 }, last = { 0 };
	if (!Mode_Import("ShipEdit") || !Mode_Import("Menu") || !LV_CALL("Menu", "InitMenuSys", NULL, "i", 0L) || !Jy_TimeSpeed(1) ||
		!LV_CALL("ShipEdit", "EnterEditShipMenu", NULL, "is", 0L, "") || !LvN_Set0I("Cursor", "Visible", 0)) {
		return 0;
	}
	Native_SetNone(&v);
	Native_SetNone(&last);
	byte ok = LvN_Entity("DM_EditKeyboard", &kb) && Mode_VGet(&kb, "ArrowCanMission", &v) && Mode_Put(MSE, "ArrowMissionState", NULL, &v) &&
		LvN_SetI(&kb, "ArrowCanMission", 0) && LvN_SetI(&kb, "ArrowCanUse", 0) && Mode_UsrHandle(&last) &&
		Mode_Put(MSE, "LastPlayer", NULL, &last) && LvN_SetI(&last, "UsrControl", -1);
	Native_Free(&kb);
	Native_Free(&v);
	Native_Free(&last);
	return ok;
}

/* SetInteract(o) */
static byte MapShipEdit_SetInteract(const nativeVal_t *a) {
	return LvN_SetI(&a[0], "ArrowCanUse", 1);
}

/* OnEndOfShipEdit(id, control): back to the game; the keypad usable again after 0.25 s, its mission arrow back */
static byte MapShipEdit_OnEndOfShipEdit(const nativeVal_t *a) {
	nativeVal_t now = { 0 }, when = { 0 }, kb = { 0 }, state = { 0 };
	if (!Mode_Import("HangarTab") || !Mode_Import("Parking") || !Mode_Import("Menu") || !LV_CALL("Menu", "BackToGame", NULL, "v", &a[0])) {
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&kb);
	Native_SetNone(&state);
	byte ok = Jy_At(&now, 0.25, &when) && LvN_Entity("DM_EditKeyboard", &kb) && Mode_AtG(&when, MSE, "SetInteract", &kb, 1) &&
		Mode_GSetI(MSE, "LastPlayer", "UsrControl", 0) && Mode_G(MSE, "ArrowMissionState", &state);
	if (ok && state.type != NV_NONE) {
		Native_Free(&kb);
		ok = LvN_Entity("DM_EditKeyboard", &kb) && Mode_VSet(&kb, "ArrowCanMission", &state);
	}
	Native_Free(&when);
	Native_Free(&kb);
	Native_Free(&state);
	return ok;
}

/* CanUseElevator(UsrName, ElevatorName, optId, FullOption): 1 with a valid ship on the platform, else 0 and why */
static byte MapShipEdit_CanUseElevator(nativeVal_t *out) {
	nativeVal_t ship = { 0 }, valid = { 0 }, text = { 0 };
	const char *why = NULL;
	Native_SetNone(&valid);
	if (!Mode_Import("HangarTab") || !LvN_ModGet("HangarTab", "prevShip", &ship)) {
		return 0;
	}
	byte ok = 1;
	if (ship.type == NV_NONE) {
		why = "JUNKYARD_Exit1";
	} else {
		Native_Free(&ship);
		ok = LvN_ModGet("HangarTab", "prevShip", &ship) && LV_CALL("HangarTab", "IsShipValid", &valid, "v", &ship);
		if (ok && !Native_True(&valid)) {
			why = "JUNKYARD_Exit2";
		}
	}
	if (ok && why) {
		Mode_Lang(why, &text);
		ok = Mode_MsgText(&text, Mode_Time() + 5);
		Native_Free(&text);
	}
	Native_Free(&ship);
	Native_Free(&valid);
	Native_SetInt(out, why ? 0 : 1);
	return ok;
}

/* UseElevator(UsrName, ElevatorName, optId, FullOption): the characters of the junkyard go, then the elevator */
static byte MapShipEdit_UseElevator(const nativeVal_t *a) {
	static const char *const chars[] = { "Rusty_0", "Maintenance_00", "SputnikJunkyard_0", "SputnikJunkyard_1", "SputnikJunkyard_2" };
	nativeVal_t r = { 0 };
	Native_SetNone(&r);
	byte ok = Mode_Import("Elevator") && LV_OWN(MSE, "CanUseElevator", &r, "vvvv", &a[0], &a[1], &a[2], &a[3]);
	if (ok && Native_True(&r)) {
		for (int i = 0; i < 5 && ok; i++) {
			nativeVal_t e = { 0 };
			ok = LvN_Entity(chars[i], &e) && (!Native_True(&e) || Mode_VDel(&e));
			Native_Free(&e);
		}
		ok = ok && LV_CALL("Elevator", "UseElevator", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
	}
	Native_Free(&r);
	return ok;
}

/* Init(LevelPath): the keypad of the editor, the player's ship on the platform, the platform's use */
static byte MapShipEdit_Init(void) {
	nativeVal_t dummy = { 0 }, p = { 0 }, c[3] = { 0 }, name = { 0 }, kb = { 0 }, d2 = { 0 }, ay = { 0 }, v = { 0 };
	nativeVal_t ship = { 0 }, posd = { 0 }, plat = { 0 };
	Native_SetNone(&p);
	Native_SetNone(&kb);
	Native_SetNone(&d2);
	Native_SetNone(&ay);
	Native_SetNone(&v);
	Native_SetNone(&ship);
	Native_SetNone(&posd);
	Native_SetNone(&plat);
	for (int i = 0; i < 3; i++) {
		Native_SetNone(&c[i]);
	}
	LvN_Dummy("DM_EditKeyboard", 0, &dummy);
	LvN_Str(&name, "DM_EditKeyboard");
	byte ok = LvN_Item(&dummy, 1, &p) && LvN_Item(&p, 0, &c[0]) && LvN_Item(&p, 1, &c[1]) && LvN_Item(&p, 2, &c[2]) &&
		LvN_Create(&name, &c[0], &c[1], &c[2], "WithLife", &kb) && LvN_Item(&dummy, 2, &d2) && LvN_Item(&d2, 1, &ay);
	if (ok) {
		nativeVal_t a = { 0 }, k = { 0 };
		Native_SetInt(&k, 180);
		Native_SetEmpty(&a, NV_TUPLE);
		Native_SetInt(Native_Append(&a), 0);
		ok = Mode_Arith('+', &ay, &k, Native_Append(&a));
		if (ok) {
			Native_SetInt(Native_Append(&a), 0);
			ok = Mode_VSet(&kb, "Ang", &a);
		}
		Native_Free(&a);
	}
	ok = ok && LvN_SetI(&kb, "VRadius", 60) && LvN_SetI(&kb, "ArrowCanUse", 1) && LvN_SetI(&kb, "Life", 0) &&
		LvN_SetI(&kb, "ArrnOpt", 0) && LvN_SetLangOpt(&kb, "ArrOpt0", "EditShip:", "HINT_Junkyard_ShipEdit") &&
		LvN_SetS(&kb, "UseFunc", "MapShipEdit.EnterEditShip") &&
		LvN_SetI(&kb, "ActiveCMask", 0) && LvN_SetI(&kb, "RadarShow", 2) && LvN_SetI(&kb, "ArrowCanMission", 1) &&
		Native_GVarGet("ArrowTargetFile", &v) && Mode_VSet(&kb, "ArrowModMission", &v) && Mode_Import("ShipEdit") &&
		Mode_Import("Parking") && Mode_Import("HangarTab") && LvN_ModGet("ShipEdit", "editShipPosDummy", &posd);
	byte missing = ok && posd.type == NV_NONE;
	if (ok && !missing) {
		ok = LvN_ModGet("ShipEdit", "editPlatform", &plat);
		missing = ok && plat.type == NV_NONE;
	}
	if (ok && missing) {
		Native_Free(&v);
		LvN_Str(&v, "No existe el dummy 'DM_Door_EditShip_*'\n");
		ok = Mode_ConsoleError(&v);
	} else if (ok) {
		ok = LV_CALL("ShipEdit", "CreatePlayerShip", NULL, "") && LvN_ModGet("HangarTab", "prevShip", &ship);
		if (ok && ship.type != NV_NONE) {
			nativeVal_t pos = { 0 }, l = { 0 };
			Native_SetNone(&pos);
			Native_Free(&posd);
			Native_Free(&ship);
			ok = LvN_ModGet("ShipEdit", "editShipPosDummy", &posd) && LvN_Item(&posd, 1, &pos) && LvN_ModGet("HangarTab", "prevShip", &ship) &&
				Mode_VSet(&ship, "Pos", &pos);
			Native_Free(&ship);
			ok = ok && LvN_ModGet("HangarTab", "prevShip", &ship) && LvN_SetI(&ship, "ActiveCMask", 1);
			Native_Free(&ship);
			if (ok && LvN_ModGet("HangarTab", "prevShip", &ship)) {
				Native_SetEmpty(&l, NV_LIST);
				Native_Copy(Native_Append(&l), &ship);
				ok = LV_CALL("ShipEdit", "SetupForEdit", NULL, "v", &l);
				Native_Free(&l);
			} else {
				ok = 0;
			}
			Native_Free(&pos);
		}
		Native_Free(&plat);
		ok = ok && LvN_ModGet("ShipEdit", "editPlatform", &plat) && LvN_SetS(&plat, "UseFunc", "MapShipEdit.UseElevator");
	}
	Native_Free(&dummy);
	Native_Free(&p);
	Native_Free(&name);
	Native_Free(&kb);
	Native_Free(&d2);
	Native_Free(&ay);
	Native_Free(&v);
	Native_Free(&ship);
	Native_Free(&posd);
	Native_Free(&plat);
	for (int i = 0; i < 3; i++) {
		Native_Free(&c[i]);
	}
	return ok;
}

LV_CORE0(MapShipEdit_EnterEditShip)
LV_COREA(MapShipEdit_SetInteract)
LV_COREA(MapShipEdit_OnEndOfShipEdit)
LV_COREA(MapShipEdit_UseElevator)
LV_CORE0(MapShipEdit_Init)

static byte MapShipEdit_CanUseElevator_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	return MapShipEdit_CanUseElevator(out);
}

LV_FN(MapShipEdit, EnterEditShipPy, 4, MapShipEdit_EnterEditShip_C)
LV_FN(MapShipEdit, SetInteractPy, 1, MapShipEdit_SetInteract_C)
LV_FN(MapShipEdit, OnEndOfShipEditPy, 2, MapShipEdit_OnEndOfShipEdit_C)
LV_FN(MapShipEdit, CanUseElevatorPy, 4, MapShipEdit_CanUseElevator_C)
LV_FN(MapShipEdit, UseElevatorPy, 4, MapShipEdit_UseElevator_C)
LV_FN(MapShipEdit, InitPy, 1, MapShipEdit_Init_C)

PyMethodDef g_JunkyardMapShipEditMethods[] = {
	{ "EnterEditShip", MapShipEdit_EnterEditShipPy, METH_VARARGS, NULL },
	{ "SetInteract", MapShipEdit_SetInteractPy, METH_VARARGS, NULL },
	{ "OnEndOfShipEdit", MapShipEdit_OnEndOfShipEditPy, METH_VARARGS, NULL },
	{ "CanUseElevator", MapShipEdit_CanUseElevatorPy, METH_VARARGS, NULL },
	{ "UseElevator", MapShipEdit_UseElevatorPy, METH_VARARGS, NULL },
	{ "Init", MapShipEdit_InitPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_RustyFirstContact ---- */

/* InitStatus(LevelPath): Berto's resources; coming from the orbit counts as from Outskirts; the credits library */
static byte Rusty_InitStatus(void) {
	if (!Mode_Import("Chars") || !LV_CALL("Chars", "AddNewCharRes", NULL, "s", "Berto")) {
		return 0;
	}
	if (LvN_SaveIs("PrevMap", "Levels/Orbit")) {
		SaveVar_Set("ComeFrom", "DoorEditShip");
		SaveVar_Set("PrevMap", "Levels/Outskirts");
	}
	/* OBJLIB 7: the pack functions by name (fc-r7b-jy records them) */
	return LV_CALL("Scrap", "OpenPack", NULL, "s", "Scripts/Scorer") &&
		LV_CALL("Scrap", "PreloadLibrary", NULL, "ss", "CreditsText", "CreditsText.pyc") && LV_CALL("Scrap", "ClosePack", NULL, "");
}

/* Init(LevelPath): arriving with Berto (Rusty is the target, the editor waits) or straight to the editor; the final
 * scene, the credits */
static byte Rusty_Init(void) {
	nativeVal_t kb = { 0 }, rusty = { 0 }, fn = { 0 };
	Native_SetNone(&rusty);
	Native_SetNone(&fn);
	byte ok = LvN_Entity("DM_EditKeyboard", &kb) && Mode_Put(RF, "kb", NULL, &kb) && LvN_Entity("Rusty_0", &rusty) &&
		Mode_Put(RF, "rusty", NULL, &rusty);
	Native_Free(&kb);
	Native_Free(&rusty);
	if (!ok) {
		return 0;
	}
	if (!LvN_SaveIs("ComeFrom", "")) {
		ok = Jy_SetMoney(1100) && LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_TalkToRusty_MissionText1", 1L) &&
			Mode_GSetI(RF, "kb", "ArrowCanUse", 0) && Mode_GSetS(RF, "kb", "UseFunc", "") && LvN_GActivate(RF, "rusty", 1) &&
			LvN_GActivate(RF, "kb", 0) && LvN_GObj(RF, "RustyFirstSpeech", &fn) && LvN_Routine("Rusty", &fn) &&
			LV_OWN(RF, "CreateFakeBertoShip", NULL, "") && LV_OWN(RF, "LoadMecanicScene", NULL, "");
	} else {
		ok = LV_OWN(RF, "InitBuildShip", NULL, "") && LvN_GActivate(RF, "kb", 0) &&
			LV_OWN(RF, "OnEndOfShipEditFirstTime", NULL, "is", -1L, "");
	}
	Native_Free(&fn);
	return ok && LV_OWN(RF, "LoadMecanicFinalScene", NULL, "") && Mode_Import("CreditsText") && LV_CALL("CreditsText", "Init", NULL, "");
}

/* CreateFakeBertoShip(): Berto's ship and Berto on the editor platform; the platform's arrival starts the scene */
static byte Rusty_CreateFakeBertoShip(void) {
	nativeVal_t epl = { 0 }, pos = { 0 }, c[3] = { 0 }, name = { 0 }, v = { 0 }, model = { 0 }, en = { 0 };
	nativeVal_t link = { 0 }, sfx = { 0 }, ch = { 0 }, floor = { 0 }, off = { 0 };
	const nativeVal_t *vals[] = { &pos, &c[0], &c[1], &c[2], &v, &en, &link, &ch, &floor, &off };
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_SetNone((nativeVal_t *)vals[i]);
	}
	LvN_Str(&name, "FakeBertoShip");
	LvN_Str(&model, "SBerto1");
	LvN_Str(&sfx, ",DC_ShipLink");
	byte ok = LvN_Entity("DM_Door_EditShip_00", &epl) && Mode_VGet(&epl, "Pos", &pos) && LvN_Item(&pos, 0, &c[0]) &&
		LvN_Item(&pos, 1, &c[1]) && LvN_Item(&pos, 2, &c[2]) && LvN_Create(&name, &c[0], &c[1], &c[2], "Car", &v) &&
		Mode_VModel(&v, &model) && LvN_SetS(&v, "MEng0", "MBerto1") && LvN_SetS(&v, "MEng1", "MBerto1") &&
		LvN_SetS(&v, "MEng2", "MBerto1") && Mode_VGet(&epl, "Name", &en) && Mode_Arith('+', &en, &sfx, &link) &&
		Mode_VSet(&v, "LinkModel", &link) && LvN_SetI(&v, "OffMul", 1) && LvN_SetI(&v, "UseLinkAng", 1);
	for (int i = 0; i < 3; i++) {
		Native_Free(&c[i]);
	}
	Native_Free(&name);
	Native_Free(&en);
	Native_Free(&link);
	Native_Free(&sfx);
	LvN_Str(&name, "FakeBerto");
	ok = ok && LvN_Item(&pos, 0, &c[0]) && LvN_Item(&pos, 1, &c[1]) && LvN_Item(&pos, 2, &c[2]) &&
		LvN_Create(&name, &c[0], &c[1], &c[2], "WalkChar", &ch) && LvN_SetS(&ch, "ActCtrl", "Berto") && Mode_VGet(&epl, "Name", &en);
	if (ok) {
		nativeVal_t t = { 0 };
		LvN_Str(&sfx, ",");
		Native_SetNone(&t);
		ok = Mode_Arith('+', &en, &sfx, &t);
		Native_Free(&sfx);
		LvN_Str(&sfx, "DC_Platform");
		ok = ok && Mode_Arith('+', &t, &sfx, &link) && Mode_VSet(&ch, "LinkModel", &link) && Mode_VGet(&ch, "FloorDist", &floor);
		Native_Free(&t);
		Native_Free(&sfx);
	}
	if (ok) {
		Native_SetEmpty(&off, NV_TUPLE);
		Native_SetInt(Native_Append(&off), 0);
		Native_Copy(Native_Append(&off), &floor);
		Native_SetInt(Native_Append(&off), -200);
		ok = Mode_VSet(&ch, "OffModPos", &off) && LvN_SetI(&ch, "OffMul", 1) && LvN_SetI(&ch, "UseLinkAng", 1) &&
			LvN_ModSetGlobal("Elevator", "OnElevatorReachDown", RF, "StartInitialScene");
	}
	Native_Free(&epl);
	Native_Free(&name);
	Native_Free(&model);
	for (unsigned i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
		Native_Free((nativeVal_t *)vals[i]);
	}
	return ok;
}

/* ShowExtras(): the extras of the junkyard (the three Sputniks and the maintenance robot) come alive */
static byte Rusty_ShowExtras(void) {
	static const char *const names[] = { "SputnikJunkyard_0", "SputnikJunkyard_1", "SputnikJunkyard_2", "Maintenance_00" };
	for (int i = 0; i < 4; i++) {
		nativeVal_t e = { 0 };
		byte ok = LvN_Entity(names[i], &e) && LvN_SetI(&e, "Active", 1) && LvN_SetI(&e, "TimeSpeed", 1);
		Native_Free(&e);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* StartInitialScene(el): the platform is down: the player in place, the scene with Berto, Rusty and Sputnik */
static byte Rusty_StartInitialScene(void) {
	nativeVal_t usr = { 0 }, p = { 0 };
	if (!LvN_ModNone("Elevator", "OnElevatorReachDown") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	Native_SetVec(&p, -618.68, -1940.08, 955.28);
	byte ok = Mode_VSet(&usr, "Pos", &p);
	if (ok) {
		Native_SetEmpty(&p, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetInt(Native_Append(&p), 0);
		}
		ok = Mode_VSet(&usr, "Ang", &p);
		Native_Free(&p);
	}
	Native_Free(&usr);
	return ok && LvN_ModSetGlobal("Scene", "EndOfSceneCallback", RF, "BertoGoHome") && LV_CALL("Scene", "Play", NULL, "s", "Mecanic") &&
		Mode_Import("SputnikSound") && LV_CALL("SputnikSound", "InitLoops", NULL, "s", "MecanicSputnik") &&
		LV_OWN(RF, "ShowExtras", NULL, "") && Mode_EntSetI("FakeBertoShip", "Active", 1);
}

static const char *const mecanic_final_actions[] = {
	"Scene.Load|MecanicFinal",
	"Scene.ActorAction|0|Dtritus|Walk",
	"Scene.ActorAction|27|Dtritus|Turn",
	"Scene.ActorAction|55|Dtritus|Relax",
	"Scene.ActorAction|260|Dtritus|Talk",
	"Scene.ActorAction|300|Dtritus|Relax",
	"Scene.ActorAction|550|Dtritus|Talk",
	"Scene.ActorAction|610|Dtritus|Turn",
	"Scene.ActorAction|630|Dtritus|WalkHi",
	"Scene.ActorAction|0|Rusty|Walk",
	"Scene.ActorAction|60|Rusty|Talk",
	"Scene.ActorAction|160|Rusty|TalkPoint",
	"Scene.ActorAction|210|Rusty|Relax",
	"Scene.ActorAction|305|Rusty|Talk",
	"Scene.ActorAction|365|Rusty|Talk0",
	"Scene.ActorAction|405|Rusty|Talk",
	"Scene.ActorAction|455|Rusty|Talk2",
	"Scene.ActorAction|505|Rusty|Talk",
	"Scene.ActorAction|545|Rusty|Relax",
	"Scene.ActorAction|615|Rusty|Turn",
	"Scene.ActorAction|640|Rusty|Hi",
	"Scene.ActorAction|675|Rusty|Relax",
	"Scene.ActorAction|0|Sputnik|Relax",
	"Scene.ActorAction|260|Sputnik|Relax3",
	"Scene.ActorAction|305|Sputnik|Relax",
	NULL,
};

static const char *const mecanic_final_actors[] = {
	"SAct.SetAct|EventName|FXMecanicFinalSputnik",
	"SAct.SetAct|EventFunc|Mission_RustyFirstContact.FXMecanicFinalSputnik",
	"Scene.AddText|60|Mission_Scene_BuildFirstShip_Rusty_1|15",
	"Scene.AddText|260|Mission_Scene_BuildFirstShip_Rusty_2|2",
	"Scene.AddText|305|Mission_Scene_BuildFirstShip_Rusty_3|10",
	"Scene.AddText|550|Mission_Scene_BuildFirstShip_Rusty_4|2",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk2|Talk2|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk3|Talk3|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk4|Talk4|1",
	"Scene.CreateLocalAction|Dtritus|0|WalkHi|WalkHi|1",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Rusty|Rusty",
	"MakeChar.CreateRelaxAction|Rusty|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|Rusty|0|TalkPoint|TalkPoint|1",
	"Scene.CreateLocalAction|Rusty|0|Hi|Hi|1",
	"Scene.SetDefSounds|Rusty|Rusty",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Sputnik|Sputnik",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax0|Relax0|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax1|Relax1|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax2|Relax2|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax3|Relax3|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax4|Relax4|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax5|Relax5|1",
	"Scene.SetDefSounds|Sputnik|Sputnik",
	"SAct.SetCls|DefAction|Relax",
	NULL,
};

/* LoadMecanicFinalScene(): the scene of the farewell with Rusty, Sputnik's effect at frame 499 (the interpolation
 * time is put back by FXMecanicFinalSputnik, where the script has that line) */
static byte Rusty_LoadMecanicFinalScene(void) {
	nativeVal_t t = { 0 };
	Native_SetNone(&t);
	byte ok = Mode_Import("Scene") && Mode_Import("MakeChar") && Mode_Calls(RF, mecanic_final_actions) &&
		LvN_GetAct("TimeOfFrame499", &t) && LvN_SetAct("AddEventAtTime", &t) && Mode_Calls(RF, mecanic_final_actors);
	Native_Free(&t);
	return ok;
}

/* Scrap.Def(name) (OBJLIB 7: the default of a float variable in C, any other form by name) */
static byte Jy_Def(const char *name, nativeVal_t *out) {
	cGVar *v = GVar_Find(name);
	Native_SetNone(out);
	if (v && v->base.type == GVAR_FLOAT) {
		Native_SetFloat(out, (double)v->def.f);
		return 1;
	}
	return LV_CALL("Scrap", "Def", out, "s", name);
}

/* FXMecanicFinalSputnik(EntityName, EventName, Time): Sputnik's effect in the scene; the default interpolation time */
static byte Rusty_FXMecanicFinalSputnik(void) {
	nativeVal_t name = { 0 }, d = { 0 };
	cEntity *e;
	LvN_Str(&name, "MecanicFinalSputnik");
	byte ok = Mode_Ent(&name, &e) && (!e || Mode_SetFX(e, "CharacterSputnikSceneMecanicFinal"));
	Native_Free(&name);
	ok = ok && Jy_Def("DefInterpTime", &d) && Mode_GVarSet("DefInterpTime", &d);
	Native_Free(&d);
	return ok;
}

/* BertoGoHome(usr): the end of the scene: Berto leaves on the platform, the camera follows it, no control */
static byte Rusty_BertoGoHome(void) {
	nativeVal_t reach = { 0 }, v = { 0 }, k = { 0 }, set = { 0 };
	Native_SetNone(&reach);
	LvN_Str(&set, "Inactive");
	byte ok = LV_CALL("Elevator", "ElevatorActiveToGet", NULL, "s", "DM_Door_EditShip_00") &&
		LvN_ModSetGlobal("Elevator", "OnElevatorReachUp", RF, "BackDownBerto") && Mode_ActionSet("Inactive") &&
		LV_CALL("Menu", "DrawScorer", NULL, "iv", 0L, &set) && Jy_CamLink("DM_Door_EditShip_00") &&
		LvN_ModGet("Elevator", "TELEPORTER_UP_REACH", &reach);
	Native_Free(&set);
	if (ok) {
		nativeVal_t *p;
		Native_SetEmpty(&v, NV_TUPLE);
		LvN_Str(Native_Append(&v), "Player0");
		p = Native_Append(&v);
		Native_SetEmpty(p, NV_TUPLE);
		Native_Copy(Native_Append(p), &reach);
		Native_SetNone(Native_Append(p));
		Native_SetInt(Native_Append(&v), 0);
		LvN_Str(&k, "DM_Door_EditShip_00");
		ok = Mode_ModDictPut("Elevator", "LinkedElevator", &k, &v);
		Native_Free(&k);
		Native_Free(&v);
	}
	Native_Free(&reach);
	return ok;
}

/* BackDownBerto(elv): up: Berto and his ship go, the platform comes down empty */
static byte Rusty_BackDownBerto(const nativeVal_t *a) {
	return LvN_SetS(&a[0], "Action", "Down") && LvN_SetS(&a[0], "DefLoop", "Closed") && LvN_ModNone("Elevator", "OnElevatorReachUp") &&
		Jy_EntDel("FakeBertoShip") && Jy_EntDel("FakeBerto") && LvN_ModSetGlobal("Elevator", "OnElevatorReachDown", RF, "EndOfBertoBye");
}

/* EndOfBertoBye(elv): down again: the player walks, the camera is free */
static byte Rusty_EndOfBertoBye(void) {
	nativeVal_t set = { 0 };
	LvN_Str(&set, "Walk");
	byte ok = Mode_ActionSet("Walk") && LV_CALL("Menu", "DrawScorer", NULL, "iv", 0L, &set) && Jy_CamLink("") &&
		LvN_ModNone("Elevator", "OnElevatorReachDown");
	Native_Free(&set);
	return ok;
}

/* StartInitBuildShip(): after Rusty's talk: the editor keypad is the target, Rusty talks as usual, a way point */
static byte Rusty_StartInitBuildShip(void) {
	nativeVal_t kb = { 0 }, gen = { 0 }, path = { 0 };
	Native_SetNone(&kb);
	Native_SetNone(&gen);
	Native_SetNone(&path);
	SaveVar_Set("Hangar.iHangarShip", "-1");
	byte ok = LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_BuildFirstShip_MissionText2", 1L) && Mode_GSetI(RF, "kb", "ArrowCanUse", 1) &&
		Mode_GSetI(RF, "kb", "ArrnOpt", 0) && Mode_G(RF, "kb", &kb) &&
		LvN_SetLangOpt(&kb, "ArrOpt0", "EditShip:", "HINT_Junkyard_ShipEdit") && LvN_GActivate(RF, "rusty", 0) &&
		LvN_ModGet("CharsNPC", "GenericNPCSpeech", &gen) && LvN_Routine("Rusty", &gen) && Mode_Import("Init") &&
		LvN_ModGet("Init", "Path", &path) &&
		LV_CALL("SaveGame", "MarkWayPoint", NULL, "vss", &path, "DM_Player_Spawn_", "") && LV_OWN(RF, "InitBuildShip", NULL, "");
	Native_Free(&kb);
	Native_Free(&gen);
	Native_Free(&path);
	return ok;
}

/* InitBuildShip(): the keypad opens the editor the first time, with its own end */
static byte Rusty_InitBuildShip(void) {
	return Mode_GSetS(RF, "kb", "UseFunc", "Mission_RustyFirstContact.EnterEditShipFirstTime") &&
		LvN_ModSetGlobal("ShipEdit", "EndShipEditCallBack", RF, "OnEndOfShipEditFirstTime") && LvN_GActivate(RF, "kb", 1);
}

/* EnterEditShipFirstTime(UsrName, CharName, optId, FullOption): the question whether to see the tutorial */
static byte Rusty_EnterEditShipFirstTime(void) {
	nativeVal_t q = { 0 }, zero = { 0 };
	if (!Mode_Import("Menu") || !LvN_GActivate(RF, "kb", 0) || !LV_CALL("Menu", "InitMenuSys", NULL, "i", 0L)) {
		return 0;
	}
	PyObject *fn = Script_Attr("Menu", "YesNoMenu");
	if (!fn) {
		return 0;
	}
	Mode_Lang("Mission_BuildFirstShip_Tut_Question", &q);
	Native_SetInt(&zero, 0);
	byte ok = Native_HookCall(fn, NULL, "ivss", 0L, &q, "Mission_RustyFirstContact.EnterEditShipTutorial",
		"Mission_RustyFirstContact.NoTutorialEnterEditShip") && LvN_Set0I("Cursor", "Visible", 0) && Jy_SetDefault(&zero, "Yes");
	Native_Free(&q);
	return ok;
}

/* NoTutorialEnterEditShip(id, control) */
static byte Rusty_NoTutorialEnterEditShip(const nativeVal_t *a) {
	nativeVal_t name = { 0 };
	if (!Mode_Import("Menu") || !LV_CALL("Menu", "BackToGame", NULL, "vs", &a[0], "")) {
		return 0;
	}
	PyObject *fn = Script_Attr("MapShipEdit", "EnterEditShip");
	if (!fn) {
		return 0;
	}
	if (!Jy_UsrName(&name)) {
		Py_DECREF(fn);
		return 0;
	}
	byte ok = Native_HookCall(fn, NULL, "vsss", &name, "", "", "");
	Native_Free(&name);
	return ok;
}

/* OnEndOfShipEditFirstTime(id, control): with a ship built it is the target and the platform ends the mission, else
 * the keypad again (id -1: only that, from Init) */
static byte Rusty_OnEndOfShipEditFirstTime(const nativeVal_t *a) {
	nativeVal_t el = { 0 }, ship = { 0 }, minus = { 0 };
	Native_SetNone(&ship);
	byte ok = LvN_Entity("DM_Door_EditShip_00", &el) && LvN_ModGet("HangarTab", "prevShip", &ship);
	if (ok && Native_True(&ship)) {
		Native_Free(&ship);
		ok = LvN_ModGet("HangarTab", "prevShip", &ship) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &ship, 1L) &&
			LvN_SetS(&el, "UseFunc", "Mission_RustyFirstContact.UseElevatorFirstTime");
	} else if (ok) {
		ok = LvN_GActivate(RF, "kb", 1) && LvN_SetS(&el, "UseFunc", "MapShipEdit.UseElevator");
	}
	Native_SetInt(&minus, -1);
	ok = ok && (!Native_Compare(&a[0], &minus) || LV_CALL("MapShipEdit", "OnEndOfShipEdit", NULL, "vv", &a[0], &a[1]));
	Native_Free(&el);
	Native_Free(&ship);
	return ok;
}

/* UseElevatorFirstTime(UsrName, ElevatorName, optId, FullOption): with a valid ship the final scene, then the
 * mission ends (the arguments kept in Params for the elevator) */
static byte Rusty_UseElevatorFirstTime(const nativeVal_t *a) {
	nativeVal_t params = { 0 }, r = { 0 }, ship = { 0 };
	Native_SetNone(&r);
	Native_SetNone(&ship);
	if (!Mode_Import("Menu") || !Mode_Import("MapShipEdit")) {
		return 0;
	}
	Jy_Tuple(&params, a, 4);
	byte ok = Mode_Put(RF, "Params", NULL, &params) && LV_CALL("MapShipEdit", "CanUseElevator", &r, "vvvv", &a[0], &a[1], &a[2], &a[3]);
	if (ok && Native_True(&r)) {
		ok = LvN_ModSetGlobal("Scene", "EndOfSceneCallback", RF, "EndOfMission") && LV_CALL("Scene", "Play", NULL, "s", "MecanicFinal") &&
			Mode_Import("SputnikSound") && LV_CALL("SputnikSound", "InitLoops", NULL, "s", "MecanicFinalSputnik") &&
			LV_OWN(RF, "ShowExtras", NULL, "") && LvN_ModGet("HangarTab", "prevShip", &ship) && LvN_SetI(&ship, "Active", 1);
	}
	Native_Free(&params);
	Native_Free(&r);
	Native_Free(&ship);
	return ok;
}

/* EndOfMission(usr): saved, the next mission in Outskirts, out by the elevator */
static byte Rusty_EndOfMission(void) {
	nativeVal_t params = { 0 };
	if (!Mode_Import("MapShipEdit") || !LV_CALL("SaveGame", "QWayPoint", NULL, "") ||
		!LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_GoToOutskirts", "Outskirts")) {
		return 0;
	}
	PyObject *fn = Script_Attr("MapShipEdit", "UseElevator");
	if (!fn) {
		return 0;
	}
	byte ok = Mode_G(RF, "Params", &params) && LV_CALL("__builtin__", "apply", NULL, "Ov", fn, &params);
	Native_Free(&params);
	Py_DECREF(fn);
	return ok;
}

/* EnterEditShipTutorial(id, control): the editor with the tutorial: the menu keys off, the messages, the steps
 * (selling the ships in the hangars first when there are any) */
static byte Rusty_EnterEditShipTutorial(const nativeVal_t *a) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "EnginesTab", "WeaponsTab", "HullTab" };
	nativeVal_t name = { 0 }, time = { 0 }, at = { 0 }, args[3] = { 0 };
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetNone(&at);
	byte ok = LV_CALL("Menu", "BackToGame", NULL, "vs", &a[0], "") && Jy_UsrName(&name);
	if (!ok) {
		return 0;
	}
	ok = LV_CALL("MapShipEdit", "EnterEditShip", NULL, "vsss", &name, "", "", "") &&
		LvN_ModSetGlobal("ShipEdit", "EndShipEditCallBack", RF, "OnEndOfShipEditTutorial") && Mode_ActionSet("Inactive");
	Native_Free(&name);
	if (ok) {
		nativeVal_t e = { 0 };
		LvN_Str(&e, "");
		ok = Mode_GVarSet("EscapeEvent", &e) && Mode_GVarSet("MainMissionEvent", &e);
		Native_Free(&e);
	}
	ok = ok && LV_OWN(RF, "TutorialMsgInit", NULL, "vv", &a[0], &a[1]);
	if (!ok) {
		return 0;
	}
	Native_SetFloat(&time, Mode_Time());
	ok = Jy_SetDefault(&a[0], "ShipEditTabHangar") && LV_CALL("ShipEdit", "ChangeTab", NULL, "vs", &a[0], "ShipEditTabHangar");
	int sell = 0;
	for (int i = 0; i < 9 && ok; i++) {
		nativeVal_t ships = { 0 }, s = { 0 };
		Native_SetNone(&s);
		ok = LvN_ModGet("ShipEdit", "editedShips", &ships) && LvN_Item(&ships, i, &s);
		if (ok && s.type != NV_NONE) {
			sell = 1;
		}
		Native_Free(&ships);
		Native_Free(&s);
	}
	if (ok) {
		Native_Copy(&args[0], &a[0]);
		Native_Copy(&args[1], &a[1]);
		Native_SetInt(&args[2], sell ? 0 : 1);
		ok = Jy_At(&time, 1.0, &at) && Mode_AtG(&at, RF, "EditShipTutorial_Step", args, 3);
		for (int i = 0; i < 3; i++) {
			Native_Free(&args[i]);
		}
	}
	Native_Free(&at);
	return ok;
}

/* the two typing texts of the tutorial: Add, Text, X 320, Y, CentralText, the colour without alpha, hidden, no cursor */
static byte Jy_TypingText(const nativeVal_t *id, const char *item, const nativeVal_t *text, long y) {
	static const char *const rgb[] = { "Red", "Green", "Blue" };
	byte ok = Jy_ScorerAdd(id, item, "TextTyping", 1) && LvN_ScorerSet(id, item, "Text", text) && Mode_ScorerSetI(id, item, "X", 320) &&
		Mode_ScorerSetI(id, item, "Y", y) && Mode_ScorerSetI(id, item, "CentralText", 1);
	for (int i = 0; i < 3 && ok; i++) {
		nativeVal_t c = { 0 }, k = { 0 };
		Native_SetNone(&k);
		ok = LvN_ModGet("Scorer", "MissionScorerText_Color", &c) && LvN_Item(&c, i, &k) && LvN_ScorerSet(id, item, rgb[i], &k);
		Native_Free(&c);
		Native_Free(&k);
	}
	return ok && Mode_ScorerSetI(id, item, "Visible", 0) && Mode_ScorerSetI(id, item, "CursorAtInit", 0) && Mode_ScorerSetI(id, item, "CursorAtEnd", 0);
}

/* TutorialMsgInit(id, control): the panel and the two texts of the tutorial messages, hidden */
static byte Rusty_TutorialMsgInit(const nativeVal_t *a) {
	static const char *const vars[] = { "BlendingMode", "X", "Y", "SizeX", "SizeY", "Type", "Alpha", "Red", "Green", "Blue", "Visible" };
	static const long vals[] = { 4, 46, 215 - 45, 640 - 46 * 2, 120, 4, 192, 16, 32, 128, 0 };
	const nativeVal_t *id = &a[0];
	nativeVal_t empty = { 0 }, t = { 0 }, use = { 0 }, text = { 0 };
	if (!Mode_Import("Menu") || !Mode_Import("ShipEdit") || !Mode_Import("Scorer") || !Jy_ScorerAdd(id, "TutorialMsgBack", "Tab", 1) ||
		!LV_CALL("Menu", "SetTabDefaults", NULL, "vs", id, "TutorialMsgBack")) {
		return 0;
	}
	for (int i = 0; i < 11; i++) {
		if (!Mode_ScorerSetI(id, "TutorialMsgBack", vars[i], vals[i])) {
			return 0;
		}
	}
	LvN_Str(&empty, "");
	byte ok = Jy_TypingText(id, "TutorialMsg", &empty, 215);
	Native_Free(&empty);
	Native_SetNone(&use);
	Native_SetNone(&text);
	Mode_Lang("Mission_BuildFirstShip_Tut_Continue", &t);
	ok = ok && LvN_ModGet("ShipEdit", "UseTxt", &use) && Mode_Arith('%', &t, &use, &text) &&
		Jy_TypingText(id, "TutorialMsgContinue", &text, 215 + 30);
	Native_Free(&t);
	Native_Free(&use);
	Native_Free(&text);
	return ok;
}

/* a step of the tutorial: (when, module, function, item) scheduled as module.function(id, item), when * tf after the
 * step's time; a NULL module ends the table */
typedef struct {
	double when;
	const char *module;
	const char *func;
	const char *item;
} jyStep_t;

/* Scrap.AddScheduledFunc(time + k, module.func, (id, item)) with item a text value */
static byte Jy_SchedItem(const nativeVal_t *time, double k, const char *module, const char *func, const nativeVal_t *id,
	const nativeVal_t *item) {
	nativeVal_t at = { 0 }, args[2] = { 0 };
	Native_Copy(&args[0], id);
	Native_Copy(&args[1], item);
	byte ok = Jy_At(time, k, &at);
	if (ok) {
		PyObject *fn = Script_Attr(module, func);
		ok = fn && Mode_Schedule(&at, fn, args, 2, NULL);
		Py_XDECREF(fn);
	}
	Native_Free(&at);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

static byte Jy_SchedItemS(const nativeVal_t *time, double k, const char *module, const char *func, const nativeVal_t *id,
	const char *item) {
	nativeVal_t it = { 0 };
	LvN_Str(&it, item);
	byte ok = Jy_SchedItem(time, k, module, func, id, &it);
	Native_Free(&it);
	return ok;
}

static byte Jy_Steps(const nativeVal_t *time, double tf, const nativeVal_t *id, const jyStep_t *steps) {
	for (; steps->module; steps++) {
		if (!Jy_SchedItemS(time, steps->when * tf, steps->module, steps->func, id, steps->item)) {
			return 0;
		}
	}
	return 1;
}

static const jyStep_t step_hangar[] = {
	{ 1.0, "SScorer", "SetDefault", "Hangar1" },
	{ 2.0, "HangarTab", "SelectItem", "Hangar1" },
	{ 2.01, "SScorer", "SetDefault", "Hangar1" },
	{ 3.5, "SScorer", "SetDefault", "ShipType" },
	{ 5.5, "HangarTab", "BuyShip", "BuyShip" },
	{ 5.51, "SScorer", "SetDefault", "Hangar1" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_engines[] = {
	{ 1.0, "SScorer", "SetDefault", "ShipEditTabHangar" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabEngines" },
	{ 3.0, "ShipEdit", "ChangeTab", "ShipEditTabEngines" },
	{ 4.5, "SScorer", "SetDefault", "Engine1" },
	{ 6.0, "EnginesTab", "PutEngine", "Engine1" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_weapons[] = {
	{ 1.0, "SScorer", "SetDefault", "ShipEditTabEngines" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabWeapons" },
	{ 3.0, "ShipEdit", "ChangeTab", "ShipEditTabWeapons" },
	{ 4.0, "SScorer", "SetDefault", "Vulcan" },
	{ 5.5, "WeaponsTab", "SelectWeapon", "Vulcan" },
	{ 7.0, "SScorer", "SetDefault", "VulcanSlot0" },
	{ 8.5, "WeaponsTab", "LoadWeapon", "VulcanSlot0" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_hull[] = {
	{ 1.0, "SScorer", "SetDefault", "Vulcan" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabWeapons" },
	{ 3.0, "SScorer", "SetDefault", "ShipEditTabHull" },
	{ 4.0, "ShipEdit", "ChangeTab", "ShipEditTabHull" },
	{ 5.0, "SScorer", "SetDefault", "HullUp" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_exit[] = {
	{ 1.0, "SScorer", "SetDefault", "Exit" },
	{ 3.0, "ShipEdit", "ExitEditShipMenu", "Exit" },
	{ 0, NULL, NULL, NULL },
};

/* TutorialMsg(id, control, Scrap.GetLangStr(key)) of the module */
static byte Jy_Msg(const nativeVal_t *a, const char *key) {
	nativeVal_t t = { 0 };
	Mode_Lang(key, &t);
	byte ok = LV_OWN(RF, "TutorialMsg", NULL, "vvv", &a[0], &a[1], &t);
	Native_Free(&t);
	return ok;
}

/* the next step at time + k: EditShipTutorial_Step(id, control, step) */
static byte Jy_Next(const nativeVal_t *a, const nativeVal_t *time, double k, long step) {
	nativeVal_t at = { 0 }, args[3] = { 0 };
	Native_Copy(&args[0], &a[0]);
	Native_Copy(&args[1], &a[1]);
	Native_SetInt(&args[2], step);
	byte ok = Jy_At(time, k, &at) && Mode_AtG(&at, RF, "EditShipTutorial_Step", args, 3);
	Native_Free(&at);
	for (int i = 0; i < 3; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}

/* step 0: the ships of the hangars sold one after the other (time moves on per ship) */
static byte Jy_Recycle(const nativeVal_t *a, nativeVal_t *time, double tf) {
	if (!Jy_Msg(a, "Mission_BuildFirstShip_Tut_Recycle")) {
		return 0;
	}
	byte ok = 1;
	for (int n = 0; n < 9 && ok; n++) {
		nativeVal_t ships = { 0 }, s = { 0 }, num = { 0 }, rep = { 0 }, line = { 0 }, hangar = { 0 };
		Native_SetNone(&s);
		Native_SetNone(&line);
		Native_SetNone(&hangar);
		ok = LvN_ModGet("ShipEdit", "editedShips", &ships) && LvN_Item(&ships, n, &s);
		Native_Free(&ships);
		if (ok && s.type != NV_NONE) {
			nativeVal_t p = { 0 }, next = { 0 };
			Native_SetInt(&num, n + 1);
			Mode_Repr(&num, &rep);
			LvN_Str(&p, "Recycle Hangar");
			ok = Mode_Arith('+', &p, &rep, &line);
			Native_Free(&p);
			if (ok) {
				LvN_Str(&p, "Hangar");
				ok = Mode_PrintV(&line) && Mode_Arith('+', &p, &rep, &hangar) &&
					Jy_SchedItem(time, 0.0 * tf, "SScorer", "SetDefault", &a[0], &hangar) &&
					Jy_SchedItem(time, 1.0 * tf, "HangarTab", "SelectItem", &a[0], &hangar) &&
					Jy_SchedItemS(time, 3.5 * tf, "SScorer", "SetDefault", &a[0], "Sell") &&
					Jy_SchedItemS(time, 5.0 * tf, "HangarTab", "SellShip", &a[0], "Sell") &&
					Jy_SchedItem(time, 5.01 * tf, "SScorer", "SetDefault", &a[0], &hangar) && Jy_At(time, 6.5 * tf, &next);
				Native_Free(&p);
				if (ok) {
					Native_Free(time);
					*time = next;
				}
			}
			Native_Free(&rep);
		}
		Native_Free(&s);
		Native_Free(&line);
		Native_Free(&hangar);
	}
	return ok && Jy_Next(a, time, 1.0 * tf, 1);
}

/* EditShipTutorial_Step(id, control, step): the message of the step and the clicks of the editor it shows, scheduled
 * (tf 2 s per unit), then the next step */
static byte Rusty_EditShipTutorial_Step(const nativeVal_t *a) {
	static const char *const mods[] = { "Menu", "ShipEdit", "HangarTab", "EnginesTab", "WeaponsTab", "HullTab" };
	static const char *const msgs[] = { "Mission_BuildFirstShip_Tut_Hangars", "Mission_BuildFirstShip_Tut_Engines",
		"Mission_BuildFirstShip_Tut_Weapons", "Mission_BuildFirstShip_Tut_Hull", "Mission_BuildFirstShip_Tut_Exit" };
	static const jyStep_t *const tables[] = { step_hangar, step_engines, step_weapons, step_hull, step_exit };
	static const double next[] = { 7.5, 8.0, 10.5, 9.5, 0 };
	const double tf = 2.0;
	const nativeVal_t *step = &a[2];
	nativeVal_t time = { 0 };
	for (int i = 0; i < 6; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetFloat(&time, Mode_Time());
	byte ok = !LvN_EqI(step, 0) || Jy_Recycle(a, &time, tf);
	int which = -1;
	for (int i = 0; i < 5 && which < 0 && ok; i++) {
		if (LvN_EqI(step, i + 1)) {
			which = i;
		}
	}
	if (ok && which >= 0) {
		ok = Jy_Msg(a, msgs[which]) && Jy_Steps(&time, tf, &a[0], tables[which]);
		if (which == 3) {
			nativeVal_t base = { 0 };
			Native_SetNone(&base);
			ok = ok && Jy_At(&time, 5.5 * tf, &base);
			for (int i = 0; i < 40 && ok; i++) {
				ok = Jy_SchedItemS(&base, i * 0.05 * tf, "HullTab", "IncrementHull", &a[0], "HullUp");
			}
			Native_Free(&base);
		}
		if (ok && next[which] > 0) {
			ok = Jy_Next(a, &time, next[which] * tf, which + 2);
		}
	}
	Native_Free(&time);
	return ok;
}

LV_CORE0(Rusty_InitStatus)
LV_CORE0(Rusty_Init)
LV_CORE0(Rusty_CreateFakeBertoShip)
LV_CORE0(Rusty_ShowExtras)
LV_CORE0(Rusty_StartInitialScene)
LV_CORE0(Rusty_LoadMecanicFinalScene)
LV_CORE0(Rusty_FXMecanicFinalSputnik)
LV_CORE0(Rusty_BertoGoHome)
LV_COREA(Rusty_BackDownBerto)
LV_CORE0(Rusty_EndOfBertoBye)
LV_CORE0(Rusty_StartInitBuildShip)
LV_CORE0(Rusty_InitBuildShip)
LV_CORE0(Rusty_EnterEditShipFirstTime)
LV_COREA(Rusty_NoTutorialEnterEditShip)
LV_COREA(Rusty_OnEndOfShipEditFirstTime)
LV_COREA(Rusty_UseElevatorFirstTime)
LV_CORE0(Rusty_EndOfMission)
LV_COREA(Rusty_EnterEditShipTutorial)
LV_COREA(Rusty_TutorialMsgInit)
LV_COREA(Rusty_EditShipTutorial_Step)

LV_FN(Rusty, InitStatusPy, 1, Rusty_InitStatus_C)
LV_FN(Rusty, InitPy, 1, Rusty_Init_C)
LV_FN(Rusty, CreateFakeBertoShipPy, 0, Rusty_CreateFakeBertoShip_C)
LV_FN(Rusty, ShowExtrasPy, 0, Rusty_ShowExtras_C)
LV_FN(Rusty, StartInitialScenePy, 1, Rusty_StartInitialScene_C)
LV_FN(Rusty, LoadMecanicFinalScenePy, 0, Rusty_LoadMecanicFinalScene_C)
LV_FN(Rusty, FXMecanicFinalSputnikPy, 3, Rusty_FXMecanicFinalSputnik_C)
LV_FN(Rusty, BertoGoHomePy, 1, Rusty_BertoGoHome_C)
LV_FN(Rusty, BackDownBertoPy, 1, Rusty_BackDownBerto_C)
LV_FN(Rusty, EndOfBertoByePy, 1, Rusty_EndOfBertoBye_C)
LV_FN(Rusty, StartInitBuildShipPy, 0, Rusty_StartInitBuildShip_C)
LV_FN(Rusty, InitBuildShipPy, 0, Rusty_InitBuildShip_C)
LV_FN(Rusty, EnterEditShipFirstTimePy, 4, Rusty_EnterEditShipFirstTime_C)
LV_FN(Rusty, NoTutorialEnterEditShipPy, 2, Rusty_NoTutorialEnterEditShip_C)
LV_FN(Rusty, OnEndOfShipEditFirstTimePy, 2, Rusty_OnEndOfShipEditFirstTime_C)
LV_FN(Rusty, UseElevatorFirstTimePy, 4, Rusty_UseElevatorFirstTime_C)
LV_FN(Rusty, EndOfMissionPy, 1, Rusty_EndOfMission_C)
LV_FN(Rusty, EnterEditShipTutorialPy, 2, Rusty_EnterEditShipTutorial_C)
LV_FN(Rusty, TutorialMsgInitPy, 2, Rusty_TutorialMsgInit_C)
LV_FN(Rusty, EditShipTutorial_StepPy, 3, Rusty_EditShipTutorial_Step_C)

PyMethodDef g_JunkyardRustyFirstContactMethods[] = {
	{ "InitStatus", Rusty_InitStatusPy, METH_VARARGS, NULL },
	{ "Init", Rusty_InitPy, METH_VARARGS, NULL },
	{ "CreateFakeBertoShip", Rusty_CreateFakeBertoShipPy, METH_VARARGS, NULL },
	{ "ShowExtras", Rusty_ShowExtrasPy, METH_VARARGS, NULL },
	{ "StartInitialScene", Rusty_StartInitialScenePy, METH_VARARGS, NULL },
	{ "LoadMecanicFinalScene", Rusty_LoadMecanicFinalScenePy, METH_VARARGS, NULL },
	{ "FXMecanicFinalSputnik", Rusty_FXMecanicFinalSputnikPy, METH_VARARGS, NULL },
	{ "BertoGoHome", Rusty_BertoGoHomePy, METH_VARARGS, NULL },
	{ "BackDownBerto", Rusty_BackDownBertoPy, METH_VARARGS, NULL },
	{ "EndOfBertoBye", Rusty_EndOfBertoByePy, METH_VARARGS, NULL },
	{ "StartInitBuildShip", Rusty_StartInitBuildShipPy, METH_VARARGS, NULL },
	{ "InitBuildShip", Rusty_InitBuildShipPy, METH_VARARGS, NULL },
	{ "EnterEditShipFirstTime", Rusty_EnterEditShipFirstTimePy, METH_VARARGS, NULL },
	{ "NoTutorialEnterEditShip", Rusty_NoTutorialEnterEditShipPy, METH_VARARGS, NULL },
	{ "OnEndOfShipEditFirstTime", Rusty_OnEndOfShipEditFirstTimePy, METH_VARARGS, NULL },
	{ "UseElevatorFirstTime", Rusty_UseElevatorFirstTimePy, METH_VARARGS, NULL },
	{ "EndOfMission", Rusty_EndOfMissionPy, METH_VARARGS, NULL },
	{ "EnterEditShipTutorial", Rusty_EnterEditShipTutorialPy, METH_VARARGS, NULL },
	{ "TutorialMsgInit", Rusty_TutorialMsgInitPy, METH_VARARGS, NULL },
	{ "EditShipTutorial_Step", Rusty_EditShipTutorial_StepPy, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Junkyard)
