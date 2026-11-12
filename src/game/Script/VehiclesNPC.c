/*
 * The VehiclesNPC module in C (scripts/vehicles/vehiclesnpc.py, issue #2): the ships of the AI. The profiles of their
 * behaviour, the hired mercenaries by reputation and the ship classes of the traffic models are data
 * (data/vehicles/vehiclesnpc.def); here the table of the standard ships, the ships made with a profile, their set up
 * and the hired mercenaries. The class cShipAI, StdShips, AIProf, Mercs, UseMercs and lmigd stay variables of the
 * module, its code (the save variable StdShipAIProfile) stays in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters. The
 * variables are read where the script reads them as its globals and StdShips and AIProf changed in place (
 * Traffic, the level files, the police and the cases read them and put profiles into AIProf). The profile objects (a
 * cShipAI instance), the ships (handle values) and the hooks stay Python values (OBJLIB 7). Vehicles, OutMap, Parking
 * and Traffic are called through their refs (PORT_CALL: directly while the attribute holds the port's function, else by
 * name), the module's own functions through its globals as the script calls them (PORT_OWN, the same seam on the
 * global: a mission or a case may replace one). The class cShipAI is called as the module's variable (OBJLIB 5 (9c: a
 * class the .mod defines, no port function)), the scheduler takes the entry of the global InitMercs (Port_Scheduled),
 * SAI and SWeap go through their refs (stage 7) except SAI.EnableAIVehicle (by name, OBJLIB 7: see below),
 * Scrap.GetEntity, CreateEntity, ConsoleError and AddScheduledFunc go through their seams. The .def runner
 * takes its $names as a dict (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static const char npc_def[] = {
#embed "../../../data/vehicles/vehiclesnpc.def"
	, 0 };

#define NPC "VehiclesNPC"

/* "run notfound" of the profile chain: the profile was not in the file */
static byte g_NpcNotFound;

static void Npc_StepNotFound(int id, const nativeVal_t *args, int n) {
	(void)id;
	(void)args;
	(void)n;
	g_NpcNotFound = 1;
}

static const scriptStep_t npc_steps[] = {
	{ "notfound", Npc_StepNotFound },
	{ NULL, NULL },
};

/* a section of vehiclesnpc.def with the n values $names[i] = vals[i]: 0 with the error set */
static byte Npc_Run(const char *section, const char *const *names, const nativeVal_t *const *vals, int n) {
	return Mode_DefRunV(0, "vehicles/vehiclesnpc.def", section, npc_def, npc_steps, 1, names, vals, n);
}

/* StdShips[key] = (the texts and ints of fmt: s a text, i a long), the dict of the module changed in place */
static byte Npc_Std(const char *key, const char *fmt, ...) {
	va_list va;
	nativeVal_t k, v;
	Native_SetStr(&k, key);
	Native_SetEmpty(&v, NV_TUPLE);
	va_start(va, fmt);
	for (const char *p = fmt; *p; p++) {
		if (*p == 's') {
			const char *s = va_arg(va, const char *);
			Native_SetText(Native_Append(&v), s, (int)strlen(s));
		} else {
			Native_SetInt(Native_Append(&v), va_arg(va, long));
		}
	}
	va_end(va);
	byte ok = Mode_DictPut(NPC, "StdShips", NULL, &k, &v);
	Native_Free(&v);
	return ok;
}

/* the standard ships of the traffic models of one list of Traffic: (model, "", "", "", "", "0,0,0,0,0,0", life, "") */
static byte Npc_StdTraffic(const char *list, long life) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *names = Mode_V(&t), *name = Mode_V(&t), *key = Mode_V(&t), *v = Mode_V(&t);
	if (!LvN_ModGet("Traffic", list, names)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(names);
	for (int i = 0; i < n; i++) {
		Native_Free(name);
		Native_Free(key);
		Native_Free(v);
		if (!Native_SeqItem(names, i, name) || !Native_Upper(name, key)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(v, NV_TUPLE);
		Native_Copy(Native_Append(v), name);
		for (int k = 0; k < 4; k++) {
			Native_SetStr(Native_Append(v), "");
		}
		Native_SetStr(Native_Append(v), "0,0,0,0,0,0");
		Native_SetInt(Native_Append(v), life);
		Native_SetStr(Native_Append(v), "");
		if (!Mode_DictPut(NPC, "StdShips", NULL, key, v)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* InitStdShips(): the ships of the characters (model and pilot), the scrapyard's and the traffic's (all the data) */
static byte Npc_InitStdShipsN(void) {
	static const char *const std[][3] = {
		{ "SPOLI1", "SPOLI1", "P_Police" }, { "SPOLI2", "SPOLI2", "P_Police" }, { "SPOLI3", "SPOLI3", "P_Police" },
		{ "SPOLI4", "SPOLI4", "P_Police" }, { "SPOLI5", "SPOLI5", "P_Police" }, { "SPOLI6", "SPOLIBOSS1", "P_Police" },
		{ "SPOLIBOSS1", "SPOLIBOSS1", "P_PoliceBoss" }, { "SBANKER1", "SBANKER1", "P_BankDirector" },
		{ "SBANKMASTER1", "SBANKMASTER1", "P_BankMaster" }, { "SCRAZYGAMBLER1", "SBANKMASTER1", "P_CrazyGambler" },
		{ "SMERC1", "SMERC1", "P_MercenaryB" }, { "SMERC2", "SMERC2", "P_MercenaryC" },
	};
	static const char *const std2[][3] = {
		{ "SMAYOR1", "SMAYOR1", "P_Mayor" }, { "SBERTO1", "SBERTO1", "P_Berto" }, { "SHUMP1", "SHUMP1", "P_Humphrey" },
		{ "SBOSS1", "SBOSS1", "P_Boss" }, { "SBETTY1", "SBETTY1", "P_Betty" }, { "SFUNC1", "SFUNC1", "P_Functionary" },
		{ "SBISHOP1", "SBISHOP1", "P_Bishop" }, { "SARCHBISHOP1", "SARCHBISHOP1", "P_Bishop" },
	};
	if (!Mode_Import("Init")) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(std) / sizeof(std[0]); i++) {
		if (!Npc_Std(std[i][0], "ss", std[i][1], std[i][2])) {
			return 0;
		}
	}
	int lonely = LvN_GTrue(NPC, "lmigd");
	if (lonely < 0) {
		return 0;
	}
	const char *mercA = lonely ? "P_MercenaryL" : "P_MercenaryA";
	if (!Npc_Std("SMERC3", "ss", "SMERC2", mercA) || !Npc_Std("SLMERC", "ss", "SMERC3", mercA)) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(std2) / sizeof(std2[0]); i++) {
		if (!Npc_Std(std2[i][0], "ss", std2[i][1], std2[i][2])) {
			return 0;
		}
	}
	int y = Mode_ModTrue("Init", "isScrapYard");
	if (y < 0 || (y && (!Npc_Std("MADHUNT", "ssssssis", "MadHunt", "", "", "", "", "7,0,0,7,0,0", 75L, "") ||
		!Npc_Std("SMOSQUITO", "ssssssis", "SMosquito", "", "", "", "", "3,0,0,0,0,0", 25L, "")))) {
		return 0;
	}
	return Mode_Import("Traffic") && Mode_Import("string") && Npc_StdTraffic("AcNormalShips", 50) &&
		Npc_StdTraffic("AcMediumShips", 300) && Npc_StdTraffic("AcHeavyShips", 500);
}

/* SpwPos[0][i] */
static byte Npc_SpwXYZ(const nativeVal_t *sp, nativeVal_t *xyz) {
	nativeVal_t at;
	Native_SetNone(&at);
	byte ok = LvN_Item(sp, 0, &at) && LvN_Item(&at, 0, &xyz[0]) && LvN_Item(&at, 1, &xyz[1]) && LvN_Item(&at, 2, &xyz[2]);
	Native_Free(&at);
	return ok;
}

/* MakeAI(TypeShip, Name="Traffic", FarPos=0, InitState=1): a ship of that type at a free spawn point (FarPos: one of
 * ten tries far from the cameras) with its profile */
static byte Npc_MakeAIN(const nativeVal_t *type, const nativeVal_t *name, const nativeVal_t *far, const nativeVal_t *init,
	nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *d = Mode_V(&t), *o1 = Mode_V(&t), *a = Mode_V(&t), *ang = Mode_V(&t), *xyz = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("ShipMaker") || !Mode_Import("OutMap")) {
		return Mode_Done(&t, 0);
	}
	int f = far ? Native_True(far) : 0;
	for (int i = 0; i < (f ? 10 : 1); i++) {
		Native_Free(sp);
		if (!PORT_CALL(OutMap, GetFreeSpawnPos, sp, "")) {
			return Mode_Done(&t, 0);
		}
		if (!f) {
			break;
		}
		for (int k = 0; k < 3; k++) {
			Native_Free(&xyz[k]);
		}
		Native_Free(d);
		if (!Npc_SpwXYZ(sp, xyz) || !PORT_CALL(Scrap, GetMinCamDist, d, "vvv", &xyz[0], &xyz[1], &xyz[2])) {
			return Mode_Done(&t, 0);
		}
		int c = Mode_CmpI(d, ">", 30000);
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
		if (c) {
			break;
		}
	}
	for (int k = 0; k < 3; k++) {
		Native_Free(&xyz[k]);
	}
	if (!Npc_SpwXYZ(sp, xyz) || !LvN_Create(name, &xyz[0], &xyz[1], &xyz[2], "Car", o1) || !LvN_Item(sp, 1, a)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(ang, NV_TUPLE);
	Native_SetInt(Native_Append(ang), 0);
	Native_Copy(Native_Append(ang), a);
	Native_SetInt(Native_Append(ang), 0);
	return Mode_Done(&t, Mode_VSet(o1, "Ang", ang) && PORT_OWN(VehiclesNPC, SetIAProf, out, "vvv", o1, type, init));
}

/* GetIAProf(TypeShip): the profile of the type, made once with the profile of the save variable StdShipAIProfile */
static byte Npc_GetIAProfN(const nativeVal_t *type, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prof = Mode_V(&t), *cls = Mode_V(&t), *std = Mode_V(&t);
	if (!Mode_G(NPC, "AIProf", prof)) {
		return Mode_Done(&t, 0);
	}
	int known = Mode_HasKey(NPC, "AIProf", 1, type);
	if (known < 0) {
		return Mode_Done(&t, 0);
	}
	if (known) {
		return Mode_Done(&t, Mode_Sub(prof, type, out));
	}
	/* OBJLIB 5 (9c): the class cShipAI, a variable of the module (the class of the .mod, no port function) */
	if (!LvN_GObj(NPC, "cShipAI", cls) || !Mode_CallArgs(cls, out, NULL, 0) || !Mode_VSet(out, "Ship", type)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("StdShipAIProfile", std);
	return Mode_Done(&t, PORT_OWN(VehiclesNPC, SetAIProfile, NULL, "vv", out, std) && Mode_DictPut(NPC, "AIProf", NULL, type, out));
}

/* ModAIProff(o1, pd, useLife=0): the AI of the ship from the profile (and its life by LifeMult) */
static byte Npc_ModAIProffN(const nativeVal_t *o1, const nativeVal_t *pd, const nativeVal_t *use) {
	static const char *const vars[] = { "FactorDamageGetAway", "ControlStrafe", "ControlBrake", "ControlBoost",
		"StupidPathFinding", "TimeMaxStrafe", "VelMaxStrafe", "ControlAim", "CadenceShoot", "AngAttack",
		"FactorDamageItemLife", "ControlBoostItemLife", "ControlBoostItemAmmo", "FactorItemAmmo", "CounterAttack",
		"ControlHookRace", "FactorHookMap", "ControlStrafeEvasion", "CadenceShootCM", "UsePercSonic", "UsePercCloud",
		"UsePercElectro", "UsePercHook", "UsePercHookGetAway" };
	enum { NVARS = sizeof(vars) / sizeof(vars[0]) };
	nativeVal_t a[NVARS + 2], f, l, m, life;
	byte ok;
	Native_SetNone(&f);
	Native_SetNone(&l);
	Native_SetNone(&m);
	Native_SetNone(&life);
	for (int i = 0; i < NVARS + 2; i++) {
		Native_SetNone(&a[i]);
	}
	Native_SetInt(&a[1], 1);
	ok = Mode_VObj(o1, "Name", &a[0]);
	for (int i = 0; ok && i < NVARS; i++) {
		ok = Mode_VObj(pd, vars[i], &a[i + 2]);
	}
	/* SAI.EnableAIVehicle by name with the 26 values (more than the fast path of a ref takes) */
	ok = ok && LvN_ModGet("SAI", "EnableAIVehicle", &f) && Mode_CallArgs(&f, NULL, a, NVARS + 2);
	if (ok && use && Native_True(use)) {
		ok = Mode_VObj(o1, "Life", &l) && Mode_VObj(pd, "LifeMult", &m) && Mode_Arith('*', &l, &m, &life) && Mode_VSet(o1, "Life", &life);
	}
	for (int i = 0; i < NVARS + 2; i++) {
		Native_Free(&a[i]);
	}
	Native_Free(&f);
	Native_Free(&l);
	Native_Free(&m);
	Native_Free(&life);
	return ok;
}

/* o1.Life = o1.Life * pd.LifeMult */
static byte Npc_LifeMult(const nativeVal_t *o1, const nativeVal_t *pd) {
	nativeVal_t l, m, life;
	Native_SetNone(&l);
	Native_SetNone(&m);
	Native_SetNone(&life);
	byte ok = Mode_VObj(o1, "Life", &l) && Mode_VObj(pd, "LifeMult", &m) && Mode_Arith('*', &l, &m, &life) && Mode_VSet(o1, "Life", &life);
	Native_Free(&l);
	Native_Free(&m);
	Native_Free(&life);
	return ok;
}

/* SetIAProf(o1, TypeShip, InitState=1): the ship gets the type's model and AI, its route, its life by LifeMult */
static byte Npc_SetIAProfN(const nativeVal_t *o1, const nativeVal_t *type, const nativeVal_t *init, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pd = Mode_V(&t), *name = Mode_V(&t), *ship = Mode_V(&t), *n2 = Mode_V(&t);
	if (!PORT_OWN(VehiclesNPC, GetIAProf, pd, "v", type) || !Mode_VObj(o1, "Name", name) || !Mode_VObj(pd, "Ship", ship) ||
		!PORT_OWN(VehiclesNPC, SetStdProperties, NULL, "vv", name, ship) ||
		!PORT_OWN(VehiclesNPC, ModAIProff, NULL, "vv", o1, pd)) {
		return Mode_Done(&t, 0);
	}
	if ((!init || Native_True(init)) && (!Mode_VObj(o1, "Name", n2) || !PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 3L, n2))) {
		return Mode_Done(&t, 0);
	}
	if (!Npc_LifeMult(o1, pd)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, o1);
	return Mode_Done(&t, 1);
}

/* SetAIProfile(sp, profileType): the attributes of the profile (vehiclesnpc.def), the console error of an unknown one */
static byte Npc_SetAIProfileN(const nativeVal_t *sp, const nativeVal_t *type) {
	g_NpcNotFound = 0;
	if (!Npc_Run("Profile", (const char *const[]){ "sp", "type" }, (const nativeVal_t *const[]){ sp, type }, 2)) {
		return 0;
	}
	if (!g_NpcNotFound) {
		return 1;
	}
	g_NpcNotFound = 0;
	modeVals_t t = { .n = 0 };
	nativeVal_t *msg = Mode_V(&t);
	return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "ERROR : SetAIProfile('"), type,
		Mode_S(&t, "')\n   |____\\ Profile not found.\n        /\n")) && Mode_ConsoleError(msg));
}

/* SetStdProperties(EntityName, Model): the standard ship of the model (by Vehicles), its weapons checked, half ammo */
static byte Npc_SetStdPropertiesN(const nativeVal_t *name, const nativeVal_t *model) {
	static const int order[] = { 0, 6, 7, 1, 2, 3, 4, 5 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *std = Mode_V(&t), *up = Mode_V(&t), *a = Mode_V(&t), *key = Mode_V(&t), *title = Mode_V(&t), *o1 = Mode_V(&t);
	nativeVal_t *it = Mode_V(&t);
	for (int i = 1; i < 8; i++) {
		Mode_V(&t);
	}
	if (!Mode_Import("ShipMaker") || !Mode_Import("Vehicles") || !Mode_Import("string") || !Mode_Import("Parking") ||
		!Mode_G(NPC, "StdShips", std) || !Native_Upper(model, up) || !Mode_Sub(std, up, a)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Len(a);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (n == 2) {
		if (!LvN_Item(a, 0, &it[0]) || !LvN_Item(a, 1, &it[1]) ||
			!PORT_CALL(Vehicles, SetStdProperties, NULL, "vvv", name, &it[0], &it[1])) {
			return Mode_Done(&t, 0);
		}
	} else {
		for (int i = 0; i < 8; i++) {
			if (!LvN_Item(a, order[i], &it[i])) {
				return Mode_Done(&t, 0);
			}
		}
		Native_Free(up);
		if (!Native_Upper(model, up) || !Mode_Cat(&t, key, 2, Mode_S(&t, "SHIP_"), up) || !Mode_LangV(key, title) ||
			!PORT_CALL(Vehicles, SetProperties, NULL, "vvvvvvvvvv", name, &it[0], &it[1], &it[2], &it[3], &it[4],
				&it[5], &it[6], &it[7], title)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_EntitySeam(name, o1) || !PORT_CALL(Parking, CheckWeapons, NULL, "v", o1)) {
		return Mode_Done(&t, 0);
	}
	static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
	for (long i = 0; i < 3; i++) {
		nativeVal_t max, half;
		Native_SetNone(&max);
		Native_SetNone(&half);
		byte ok = PORT_CALL(SWeap, GetFAmmo, &max, "is", i, "Max") && Mode_Arith('/', &max, Mode_I(&t, 2), &half) &&
			Mode_VSet(o1, ammo[i], &half);
		Native_Free(&max);
		Native_Free(&half);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* MercOnDeath(LooserName, WinnerName): a hired mercenary dies and is no longer hired */
static byte Npc_MercOnDeathN(const nativeVal_t *looser, const nativeVal_t *winner) {
	nativeVal_t zero;
	const char *n;
	Native_SetStr(&zero, "0");
	return Mode_Import("Vehicles") && PORT_CALL(Vehicles, OnDeath, NULL, "vv", looser, winner) &&
		Native_ArgStr(looser, 1, &n) && Mode_SetSaveVar(n, &zero);
}

/* the hired mercenary of Mercs key (its save variable is "1") as a friendly ship with the profile of the reputation */
static byte Npc_Merc(const nativeVal_t *mercs, const nativeVal_t *key) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *var = Mode_V(&t), *hired = Mode_V(&t), *usr = Mode_V(&t), *sp = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *rptn = Mode_V(&t), *ship = Mode_V(&t);
	if (!Mode_Sub(mercs, key, var)) {
		return Mode_Done(&t, 0);
	}
	/* Scrap.GetSaveVar(var) with "" for a value that is no text, as the C port read it */
	Mode_SaveVar(var->type == NV_STR ? Native_Text(var) : "", hired);
	if (!Native_IsStr(hired, "1")) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Cat(&t, usr, 2, key, Mode_S(&t, "_usr")) || !PORT_OWN(VehiclesNPC, GetIAProf, sp, "v", usr)) {
		return Mode_Done(&t, 0);
	}
	int lonely = LvN_GTrue(NPC, "lmigd");
	if (lonely < 0 || !Mode_VSet(sp, "Ship", lonely && Native_IsStr(key, "SMERC2") ? Mode_S(&t, "SMERC3") : key) ||
		!Npc_Run("MercBase", (const char *const[]){ "sp" }, (const nativeVal_t *const[]){ sp }, 1)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("BadReputation", rep);
	return Mode_Done(&t, Native_ToInt(rep, rptn) &&
		Npc_Run("Merc", (const char *const[]){ "sp", "rptn" }, (const nativeVal_t *const[]){ sp, rptn }, 2) &&
		PORT_OWN(VehiclesNPC, MakeAI, ship, "vvii", usr, var, 0L, 0L) && LvN_SetS(ship, "EntityList", "Friends") &&
		LvN_SetS(ship, "OnDeath", "VehiclesNPC.MercOnDeath"));
}

/* InitMercs(): each hired mercenary of Mercs */
static byte Npc_InitMercsN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mercs = Mode_V(&t), *keys = Mode_V(&t), *key = Mode_V(&t);
	if (!Mode_G(NPC, "Mercs", mercs) || !Mode_Keys(NPC, "Mercs", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(keys);
	for (int i = 0; i < n; i++) {
		Native_Free(key);
		if (!Native_SeqItem(keys, i, key) || !Npc_Merc(mercs, key)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* the ship classes of the traffic models of one list (a section of vehiclesnpc.def per model) */
static byte Npc_Classes(const char *list, const char *section) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *names = Mode_V(&t), *name = Mode_V(&t);
	if (!LvN_ModGet("Traffic", list, names)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(names);
	for (int i = 0; i < n; i++) {
		Native_Free(name);
		if (!Native_SeqItem(names, i, name) || !Npc_Run(section, (const char *const[]){ "name" }, (const nativeVal_t *const[]){ name }, 1)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* Init(): the ship classes, the standard ships, the hired mercenaries (scheduled) outside the super deals */
static byte Npc_InitN(void) {
	if (!Mode_Import("ShipMaker") || !Mode_Import("Traffic") || !Mode_Import("Init") || !Mode_Import("string")) {
		return 0;
	}
	int y = Mode_ModTrue("Init", "isScrapYard");
	if (y < 0 || (y && !Npc_Run("ScrapYard", NULL, NULL, 0)) || !PORT_CALL(Traffic, SetPath, NULL, "") ||
		!Npc_Classes("AcNormalShips", "Normal") || !Npc_Classes("AcMediumShips", "Medium") || !Npc_Classes("AcHeavyShips", "Heavy") ||
		!PORT_CALL(Traffic, ResetPath, NULL, "") || !PORT_OWN(VehiclesNPC, InitStdShips, NULL, "")) {
		return 0;
	}
	int u = LvN_GTrue(NPC, "UseMercs");
	if (u <= 0) {
		return u == 0;
	}
	modeVals_t t = { .n = 0 };
	nativeVal_t *map = Mode_V(&t), *deals = Mode_V(&t), *time = Mode_V(&t), *name = Mode_V(&t);
	if (!LvN_ModGet("Init", "UpperMap", map) || !LvN_ModGet("Init", "SUPERDEALSMAPS", deals)) {
		return Mode_Done(&t, 0);
	}
	Mode_Elems(deals);
	int in = Native_Contains(deals, map);
	if (in < 0) {
		return Mode_Done(&t, 0);
	}
	if (in || Native_IsStr(map, "JUNKYARD")) {
		return Mode_Done(&t, 1);
	}
	/* the global (a mission may replace it): the scheduler takes the entry while it is the port's own function */
	nativeVal_t *f = Mode_V(&t);
	byte ok = Mode_G(NPC, "InitMercs", f);
	Native_SetInt(time, 0);
	Native_SetStr(name, "InitMercs");
	return Mode_Done(&t, ok && Mode_ScheduleV(time, f, NULL, 0, name));
}

/* the functions with keyword arguments: one core each on the arguments by slot (NULL for one not given), the adapter
 * (Mode_KwCall) and the entry (Mode_KwEntry) */
static byte Npc_MakeAIK(const nativeVal_t *const *a, nativeVal_t *out) {
	nativeVal_t name, init;
	Native_SetStr(&name, "Traffic");
	Native_SetInt(&init, 1);
	return Npc_MakeAIN(a[0], a[1] ? a[1] : &name, a[2], a[3] ? a[3] : &init, out);
}

static byte Npc_ModAIProffK(const nativeVal_t *const *a, nativeVal_t *out) {
	(void)out;
	return Npc_ModAIProffN(a[0], a[1], a[2]);
}

static byte Npc_SetIAProfK(const nativeVal_t *const *a, nativeVal_t *out) {
	return Npc_SetIAProfN(a[0], a[1], a[2], out);
}

static byte VehiclesNPC_MakeAI_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "O|OOO", Npc_MakeAIK, out);
}

static char *VehiclesNPC_MakeAI_N[] = { "TypeShip", "Name", "FarPos", "InitState", NULL };
static const char VehiclesNPC_MakeAI_F[] = "O|OOO";

/* OBJLIB 7: a keyword adapter (the parameter names and the errors of PyArg_ParseTupleAndKeywords) */
static PyObject *VehiclesNPC_MakeAI(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "O|OOO", VehiclesNPC_MakeAI_N, Npc_MakeAIK);
}

static byte VehiclesNPC_ModAIProff_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "OO|O", Npc_ModAIProffK, out);
}

static char *VehiclesNPC_ModAIProff_N[] = { "o1", "pd", "useLife", NULL };
static const char VehiclesNPC_ModAIProff_F[] = "OO|O";

/* OBJLIB 7: a keyword adapter (the parameter names and the errors of PyArg_ParseTupleAndKeywords) */
static PyObject *VehiclesNPC_ModAIProff(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "OO|O", VehiclesNPC_ModAIProff_N, Npc_ModAIProffK);
}

static byte VehiclesNPC_SetIAProf_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "OO|O", Npc_SetIAProfK, out);
}

static char *VehiclesNPC_SetIAProf_N[] = { "o1", "TypeShip", "InitState", NULL };
static const char VehiclesNPC_SetIAProf_F[] = "OO|O";

/* OBJLIB 7: a keyword adapter (the parameter names and the errors of PyArg_ParseTupleAndKeywords) */
static PyObject *VehiclesNPC_SetIAProf(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "OO|O", VehiclesNPC_SetIAProf_N, Npc_SetIAProfK);
}

MODE_FN(VehiclesNPC, InitStdShips, 0, Npc_InitStdShipsN())
MODE_FN(VehiclesNPC, GetIAProf, 1, Npc_GetIAProfN(&a[0], out))
MODE_FN(VehiclesNPC, SetAIProfile, 2, Npc_SetAIProfileN(&a[0], &a[1]))
MODE_FN(VehiclesNPC, SetStdProperties, 2, Npc_SetStdPropertiesN(&a[0], &a[1]))
MODE_FN(VehiclesNPC, MercOnDeath, 2, Npc_MercOnDeathN(&a[0], &a[1]))
MODE_FN(VehiclesNPC, InitMercs, 0, Npc_InitMercsN())
MODE_FN(VehiclesNPC, Init, 0, Npc_InitN())

PyMethodDef g_VehiclesNPCMethods[] = {
	{ "InitStdShips", VehiclesNPC_InitStdShips, 1, NULL },
	{ "MakeAI", (PyCFunction)(void (*)(void))VehiclesNPC_MakeAI, 3, NULL },
	{ "GetIAProf", VehiclesNPC_GetIAProf, 1, NULL },
	{ "ModAIProff", (PyCFunction)(void (*)(void))VehiclesNPC_ModAIProff, 3, NULL },
	{ "SetIAProf", (PyCFunction)(void (*)(void))VehiclesNPC_SetIAProf, 3, NULL },
	{ "SetAIProfile", VehiclesNPC_SetAIProfile, 1, NULL },
	{ "SetStdProperties", VehiclesNPC_SetStdProperties, 1, NULL },
	{ "MercOnDeath", VehiclesNPC_MercOnDeath, 1, NULL },
	{ "InitMercs", VehiclesNPC_InitMercs, 1, NULL },
	{ "Init", VehiclesNPC_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(VehiclesNPC)
