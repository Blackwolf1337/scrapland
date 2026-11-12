/*
 * The levels whose scripts are a few lines, in C (issue #2): TucZone (data02.packed, the keypad to Nano), Nano
 * (Data.packed, the test map of the police boss) and ShipEdit (data02.packed, the ship editor map). Their other
 * functions are data (the files in data/levels/tuczone, nano, shipedit).
 *
 * Native C (docs/objlib-plan.md, stage 2): the dummy search, the entities (Scrap.CreateEntity through its seam, the
 * handles as values, OBJLIB 7), the camera and the engine variables (Scrap.Set through its seam) in C; Init.Outdoor
 * and CharsNPC.AfterBrowseNPC are variables of other modules, set and changed in place (OBJLIB 5 (9c: AfterBrowseNPC
 * holds functions)).
 *
 * Direct calls (stage 3): Elevator.ConnectElevators and ShipEdit.EnterEditShipMenu go through their refs (PORT_CALL).
 * The NetModes bots put their own function into ShipEdit.EnterEditShipMenu; the call then takes the slow path to it.
 */
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

#define MAP "Map"

/* ---- TucZone ---- */

/* the level of the refs of this part (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL tuczone

/* Init(LevelPath): the keypad at its dummy that takes the player to Nano */
static byte Tuc_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dummy, p, name, x, y, z, kb, ang, ay, off, sum;
	(void)a;
	(void)out;
	Native_SetNone(&p);
	Native_SetNone(&x);
	Native_SetNone(&y);
	Native_SetNone(&z);
	Native_SetNone(&kb);
	Native_SetNone(&ang);
	Native_SetNone(&ay);
	Native_SetNone(&sum);
	LvN_Str(&name, "Elevator");
	Native_SetInt(&off, 180);
	LvN_Dummy("DM_Keypad_In", 1, &dummy);
	byte ok = LvN_Item(&dummy, 1, &p) && LvN_Item(&p, 0, &x) && LvN_Item(&p, 1, &y) && LvN_Item(&p, 2, &z) &&
		LvN_Create(&name, &x, &y, &z, "WithLife", &kb);
	/* kb.Ang = (0, dummy[2][1] + 180, 0) */
	Native_Free(&p);
	ok = ok && LvN_Item(&dummy, 2, &p) && LvN_Item(&p, 1, &ay) && Native_Arith('+', &ay, &off, &sum);
	if (ok) {
		Native_SetEmpty(&ang, NV_TUPLE);
		Native_SetInt(Native_Append(&ang), 0);
		Native_Copy(Native_Append(&ang), &sum);
		Native_SetInt(Native_Append(&ang), 0);
		ok = Mode_VSet(&kb, "Ang", &ang) && LvN_SetI(&kb, "VRadius", 60) && LvN_SetI(&kb, "ArrowCanUse", 1) &&
			LvN_SetI(&kb, "Life", 0) && LvN_SetI(&kb, "ArrnOpt", 0) && LvN_SetS(&kb, "ArrOpt0", "TucZone:Go to Nano") &&
			LvN_SetS(&kb, "UseFunc", "Map.LoadNano") && LvN_SetI(&kb, "ActiveCMask", 0);
	}
	Native_Free(&dummy);
	Native_Free(&p);
	Native_Free(&name);
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&z);
	Native_Free(&kb);
	Native_Free(&ang);
	Native_Free(&ay);
	Native_Free(&sum);
	return ok;
}

LV_FN(Tuc, InitPy, 1, Tuc_Init)

PyMethodDef g_TucZoneMapMethods[] = {
	{ "Init", Tuc_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Nano ---- */

#undef PORT_LEVEL
#define PORT_LEVEL nano

/* InitStatus(LevelPath): an indoor map; AfterBrowseNPC after the NPCs are browsed, the work place of the boss */
static byte Nano_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t zero, fn;
	(void)a;
	(void)out;
	Native_SetInt(&zero, 0);
	if (!Mode_Import("Init") || !Mode_Import("Chars") || !Mode_Import("CharsNPC") || !Mode_SetModVar("Init", "Outdoor", &zero) ||
		!LvN_GObj(MAP, "AfterBrowseNPC", &fn)) {
		return 0;
	}
	byte ok = LvN_ModAppend("CharsNPC", "AfterBrowseNPC", &fn) && Mode_GVarSetS("BossWorkPlace", "DM_Prueba_Boss");
	Native_Free(&fn);
	return ok;
}

/* Init(LevelPath): the two elevators */
static byte Nano_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t doors, none;
	(void)a;
	(void)out;
	Native_SetEmpty(&doors, NV_LIST);
	LvN_Str(Native_Append(&doors), "DM_Door_Elevator_00");
	LvN_Str(Native_Append(&doors), "DM_Door_Elevator_03");
	Native_SetEmpty(&none, NV_LIST);
	byte ok = Mode_Import("Elevator") && PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &doors, &none);
	Native_Free(&doors);
	Native_Free(&none);
	return ok;
}

LV_FN(Nano, InitStatusPy, 1, Nano_InitStatus)
LV_FN(Nano, InitPy, 1, Nano_Init)

PyMethodDef g_NanoMapMethods[] = {
	{ "InitStatus", Nano_InitStatusPy, 1, NULL },
	{ "Init", Nano_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- ShipEdit ---- */

#undef PORT_LEVEL
#define PORT_LEVEL shipedit

/* Init(LevelPath): the camera of the editor, the ship editor menu */
static byte Se_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t name, zero, cam, n;
	const char *s;
	(void)a;
	(void)out;
	if (!Mode_Import("ShipEdit")) {
		return 0;
	}
	LvN_Str(&name, "Camerita");
	Native_SetInt(&zero, 0);
	Native_SetNone(&n);
	byte ok = LvN_Create(&name, &zero, &zero, &zero, "Cam", &cam) && Mode_VGet(&cam, "Name", &n);
	/* Scrap.SetCam(0, cam.Name) ("is") */
	if (ok && Native_ArgStr(&n, 2, &s)) {
		cWorld_SetCam(g_pWorld, 0, s);
	} else {
		ok = 0;
	}
	ok = ok && LvN_SetInts3(&cam, "camPos", -6, -1243, 1451) && LvN_SetInts3(&cam, "camAng", 15, -180, 0) &&
		LvN_SetI(&cam, "Fov", 70) && Mode_GVarSetI("CameraCloudAlpha", 0) && PORT_CALL(ShipEdit, EnterEditShipMenu, NULL, "is", 0L, "");
	if (ok) {
		Mode_SetTimeSpeed(1);
		ok = Mode_GVarSetI("IsMapOutDoor", 0);
	}
	Native_Free(&name);
	Native_Free(&cam);
	Native_Free(&n);
	return ok;
}

LV_FN(Se, InitPy, 1, Se_Init)
/* MapSnd.Init(LevelPath): returns at once (the editor has no map sounds) */
LV_FN(Se, SndInitPy, 1, LvN_Pass)

PyMethodDef g_ShipEditMapMethods[] = {
	{ "Init", Se_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_ShipEditMapSndMethods[] = {
	{ "Init", Se_SndInitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Small)
