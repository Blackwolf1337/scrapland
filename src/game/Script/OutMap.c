/*
 * The OutMap and InTraffic modules in C (scripts/vehicles/outmap.py and intraffic.py, issue #2). OutMap starts an
 * outdoor level (Init: the map limits, the spawn points, the grid and the clouds, then InitOutMap for the client, the
 * server, the split screen or the single player game) and finds free spawn points for the ships. InTraffic fills an
 * indoor level with the fake traffic of the map script.
 *
 * The module variables stay in the scripts, others read and set them: OutMap.StartPos (the spawn points, a list of
 * (position, angle)), iStartPos and SpawRes; InTraffic.N_TrafficList, M_TrafficList, H_TrafficList, their counts
 * N_NumTraffic, M_NumTraffic, H_NumTraffic and Path (the map scripts set them before LoadFakeTraffic and FakeTraffic).
 * Both modules are native C (docs/objlib-plan.md, stage 2, see their sections).
 */
#include "scrap/scrap.h"
#include "scrap/game.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/world.h"
#include "game/Script/levels/Levels.h"

/* ---- OutMap ---- */

/*
 * OutMap is native C (docs/objlib-plan.md, stage 2): its functions work on C values and read StartPos, iStartPos and
 * SpawRes where the script reads them and write them back at once (Ffa and the cases read and set them).
 * The engine calls the cases record or swap go through their seams (SAI.GetNextRacePoint, Scrap.TestSphere,
 * TestLine, UsrEntity, StartDummySearch, NextDummySearch, CreateEntity, GetEntity, Set, SetGrid, GetNetFlags and
 * SInput.SetActionSet); the player and the entities are handle values (OBJLIB 7). The other ports (Ffa, Split,
 * Accelerator, Chars, OutPolice, Traffic, PhoneCab, Elevator, Parking, MissionsFuncs, Vehicles, Items) are called
 * through their refs (PORT_CALL: directly while the attribute holds the port's function, else by name) and the
 * module's own functions through its globals as the script calls them (PORT_OWN, the same seam on the global: a
 * mission or a case may replace them).
 */

/* a global of the module as its functions read it (out None before) */
static byte Om_G(const char *name, nativeVal_t *out) {
	return Mode_G("OutMap", name, out);
}

/* x == None: 1, 0, -1 with the error */
static int Om_IsNone(const nativeVal_t *x) {
	nativeVal_t none;
	Native_SetNone(&none);
	return Mode_Cmp(x, "==", &none);
}

/* `pos and Scrap.TestSphere(pos[0], pos[1], pos[2], 1200, "vr") == None` (test 0: the sphere test only): no ship or
 * racer near the point. 1, 0, -1 with the error */
static int Om_FreePos(const nativeVal_t *pos, byte test) {
	nativeVal_t hit = { 0 };
	if (test && !Native_True(pos)) {
		return 0;
	}
	int r = Mode_TestSpherePos(pos, 1200, "vr", &hit) ? Om_IsNone(&hit) : -1;
	Native_Free(&hit);
	return r;
}

/* (v, 0) into out (v taken over) */
static void Om_Pair(nativeVal_t *v, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	*Native_Append(out) = *v;
	Native_SetNone(v);
	Native_SetInt(Native_Append(out), 0);
}

/* the item 0 of the result of one of the module's own functions without arguments, called at the site (PORT_OWN's
 * seam on the global); OM_OWN0(func, out) names the function */
static byte Om_Own0(portSite_t *site, nativeVal_t *out) {
	nativeVal_t r = { 0 };
	byte ok = Port_Invoke(site, &r, "") && LvN_Item(&r, 0, out);
	Native_Free(&r);
	return ok;
}

#define OM_OWN0(func, out) Om_Own0(PORT_SITE(PORT_REF(OutMap, func), 1), out)

/* BrowseSpawnPos(): the active DM_Spawn dummies into StartPos as (position, angle + 180) */
static byte Om_BrowseSpawnPos(void) {
	if (!Mode_DummyStartSeam("DM_Spawn*", 1)) {
		return 0;
	}
	for (;;) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *dummy = Mode_V(&t), *on = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *y = Mode_V(&t);
		nativeVal_t *e = Mode_V(&t), *item = Mode_V(&t);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		int some = Om_IsNone(dummy);
		if (some != 0) {
			return Mode_Done(&t, some > 0);
		}
		if (!LvN_Item(dummy, 3, on)) {
			return Mode_Done(&t, 0);
		}
		/* list.append with two arguments appends the tuple in Python 1.5.2 */
		if (Native_True(on)) {
			if (!LvN_Item(dummy, 1, pos) || !LvN_Item(dummy, 2, ang) || !LvN_Item(ang, 1, y) ||
				!Mode_Arith('+', y, Mode_I(&t, 180), e)) {
				return Mode_Done(&t, 0);
			}
			Native_SetEmpty(item, NV_TUPLE);
			Native_Copy(Native_Append(item), pos);
			Native_Copy(Native_Append(item), e);
			if (!Mode_Append("OutMap", "StartPos", item)) {
				return Mode_Done(&t, 0);
			}
		}
		Mode_Done(&t, 1);
	}
}

/* GetSpawnPos(): the next spawn point of StartPos, round */
static byte Om_GetSpawnPos(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *i = Mode_V(&t), *next = Mode_V(&t), *sp = Mode_V(&t), *n = Mode_V(&t), *r = Mode_V(&t);
	int len = -1;
	byte ok = Om_G("iStartPos", i) && Mode_Arith('+', i, Mode_I(&t, 1), next) && Om_G("StartPos", sp) &&
		(len = Mode_Len(sp)) >= 0;
	if (ok) {
		Native_SetInt(n, len);
		Native_Free(sp);
		Native_Free(i);
		ok = Mode_Arith('%', next, n, r) && Mode_Put("OutMap", "iStartPos", NULL, r) && Om_G("StartPos", sp) &&
			Om_G("iStartPos", i) && Mode_Sub(sp, i, out);
	}
	return Mode_Done(&t, ok);
}

/* GetFreeSpawnPos(): (position, 0), the free point nearest to the player that the player does not see */
static byte Om_GetFreeSpawnPos(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *pos = Mode_V(&t), *p = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, PORT_OWN(OutMap, GetFreeSpawnPosReal, out, ""));
	}
	if (!Mode_VObj(usr, "Pos", pos) || !PORT_OWN(OutMap, GetNearestFreeSpawnPos, p, "v", pos)) {
		return Mode_Done(&t, 0);
	}
	Om_Pair(p, out);
	return Mode_Done(&t, 1);
}

/* GetFreeSpawnPosReal(): (position, 0), a free race point after one of the next three spawn points */
static byte Om_GetFreeSpawnPosReal(nativeVal_t *out) {
	for (int i = 0; i < 3; i++) {
		nativeVal_t sp = { 0 }, at = { 0 }, pos = { 0 };
		int ok = PORT_OWN(OutMap, GetSpawnPos, &sp, "") && LvN_Item(&sp, 0, &at) &&
			Mode_RacePoint(&at, 0, 2000000, &pos) ? Om_FreePos(&pos, 1) : -1;
		Native_Free(&sp);
		Native_Free(&at);
		if (ok > 0) {
			Om_Pair(&pos, out);
		}
		Native_Free(&pos);
		if (ok) {
			return ok > 0;
		}
	}
	return PORT_OWN(OutMap, GetFreeSpawn, out, "");
}

/* GetNearestFreeSpawnPosReal(initialPos): a free race point after initialPos (seven tries) */
static byte Om_GetNearestFreeSpawnPosReal(const nativeVal_t *initial, nativeVal_t *out) {
	for (int i = 0; i < 7; i++) {
		nativeVal_t pos = { 0 };
		int ok = Mode_RacePoint(initial, 0, 2000000, &pos) ? Om_FreePos(&pos, 1) : -1;
		if (ok > 0) {
			*out = pos;
			return 1;
		}
		Native_Free(&pos);
		if (ok < 0) {
			return 0;
		}
	}
	return OM_OWN0(GetFreeSpawn, out);
}

/* GetFreeSpawn(): the first free spawn point (the next one when none is free); SpawRes the last TestSphere */
static byte Om_GetFreeSpawn(nativeVal_t *out) {
	nativeVal_t sp = { 0 };
	long count = 0;
	int n = Om_G("StartPos", &sp) ? Mode_Len(&sp) : -1;
	Native_Free(&sp);
	if (n < 0) {
		return 0;
	}
	Native_SetInt(&sp, n);
	if (!Mode_Range(&sp, &count)) {
		return 0;
	}
	for (long i = 0; i < count; i++) {
		nativeVal_t pos = { 0 }, at = { 0 }, res = { 0 };
		int ok = PORT_OWN(OutMap, GetSpawnPos, &pos, "") && LvN_Item(&pos, 0, &at) &&
			Mode_TestSpherePos(&at, 1200, "vr", &res) && Mode_Put("OutMap", "SpawRes", NULL, &res) &&
			(Native_Free(&res), Om_G("SpawRes", &res)) ? Om_IsNone(&res) : -1;
		Native_Free(&at);
		Native_Free(&res);
		if (ok > 0) {
			*out = pos;
			return 1;
		}
		Native_Free(&pos);
		if (ok < 0) {
			return 0;
		}
	}
	return PORT_OWN(OutMap, GetSpawnPos, out, "");
}

/*
 * GetNearestFreeSpawnPos(initialPos=None) and GetFarthestFreeSpawnPos(initialPos=None): the free race point after a
 * spawn point nearest to (the player sees it) or farthest from initialPos, the player's position by default
 */
static byte Om_SpawnPosBy(const nativeVal_t *initialArg, byte nearest, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *initial = Mode_V(&t), *best = Mode_V(&t), *bestPos = Mode_V(&t), *list = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, nearest ? OM_OWN0(GetFreeSpawnPosReal, out) :
			PORT_OWN(OutMap, GetFreeSpawnPosReal, out, ""));
	}
	Native_SetFloat(best, nearest ? 2000000.0 : 0.0);
	if (initialArg && Native_True(initialArg)) {
		Native_Copy(initial, initialArg);
	} else if (!Mode_VObj(usr, "Pos", initial)) {
		return Mode_Done(&t, 0);
	}
	int n = Om_G("StartPos", list) ? Mode_Loop(list) : -1;
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *elem = Mode_V(&t), *at = Mode_V(&t), *pos = Mode_V(&t), *upos = Mode_V(&t);
		nativeVal_t *res = Mode_V(&t), *name = Mode_V(&t), *d = Mode_V(&t), *dist = Mode_V(&t), *list2 = Mode_V(&t);
		int ok = Native_SeqItem(list, i, elem) && LvN_Item(elem, 0, at) && Mode_RacePoint(at, 0, 2000000, pos) ?
			Om_FreePos(pos, 0) : -1;
		if (ok > 0 && nearest) {
			Native_SetStr(list2, "");
			ok = Mode_VObj(usr, "Pos", upos) && Mode_TestLineSeam(upos, pos, list2, res) ? Native_True(res) : -1;
			if (ok > 0) {
				ok = LvN_Item(res, 1, name) ? LvN_EqS(name, "") : -1;
			}
		}
		if (ok > 0) {
			ok = Mode_VecSub(initial, pos, d) && Mode_VecMod(d, dist) ? Mode_Cmp(dist, nearest ? "<" : ">", best) : -1;
			if (ok > 0) {
				Native_Free(best);
				Native_Free(bestPos);
				Native_Copy(best, dist);
				Native_Copy(bestPos, pos);
			}
		}
		if (ok < 0) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	int none = Om_IsNone(bestPos);
	if (none < 0) {
		return Mode_Done(&t, 0);
	}
	if (none) {
		return Mode_Done(&t, OM_OWN0(GetFreeSpawnPosReal, out));
	}
	Native_Copy(out, bestPos);
	return Mode_Done(&t, 1);
}

/* the end of CreatePlayers: the camera of Player1 and the action set of the racer */
static byte Om_PlayersCam(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *ply = Mode_V(&t), *pn = Mode_V(&t), *cn = Mode_V(&t), *z = Mode_V(&t);
	Native_SetFloat(z, 0.0);
	byte ok = LvN_Create(Mode_S(&t, "Player1Cam"), z, z, z, "FCam", cam) && Mode_EntitySeam(Mode_S(&t, "Player1"), ply) &&
		Mode_VGet(ply, "Name", pn) && Mode_VGet(cam, "Name", cn) &&
		PORT_CALL(Vehicles, SetActiveCam, NULL, "vvi", pn, cn, 0L) && Mode_ActionSet("Racer");
	return Mode_Done(&t, ok);
}

/* CreatePlayers(): a test of the developers, a ship at each active DM_Spawn (the second one with AI) and the camera */
static byte Om_CreatePlayers(void) {
	static const char *const gvars[] = { "PlayerModel", "PlayerMaxLife", "PilotModel", "Motor0Model", "Motor1Model",
		"Motor2Model", "Motor3Model", "WeaponBayList", "PlayerName" };
	nativeVal_t ctr;
	if (!Mode_Import("Vehicles") || !Mode_DummyStartSeam("DM_Spawn*", 1)) {
		return 0;
	}
	Native_SetInt(&ctr, 1);
	for (;;) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *dummy = Mode_V(&t), *on = Mode_V(&t), *at = Mode_V(&t), *rep = Mode_V(&t), *name = Mode_V(&t);
		nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *o1 = Mode_V(&t), *desc = Mode_V(&t);
		nativeVal_t *ang = Mode_V(&t), *a = Mode_V(&t), *a1 = Mode_V(&t), *oname = Mode_V(&t), *p = Mode_V(&t);
		if (!Mode_DummyNextSeam(dummy)) {
			Native_Free(&ctr);
			return Mode_Done(&t, 0);
		}
		int some = Om_IsNone(dummy);
		if (some != 0) {
			Native_Free(&ctr);
			return Mode_Done(&t, some < 0 ? 0 : Om_PlayersCam());
		}
		if (!LvN_Item(dummy, 3, on)) {
			Native_Free(&ctr);
			return Mode_Done(&t, 0);
		}
		if (!Native_True(on)) {
			Mode_Done(&t, 1);
			continue;
		}
		Mode_Repr(&ctr, rep);
		byte ok = LvN_Item(dummy, 1, at) && Mode_Cat(&t, name, 2, Mode_S(&t, "Player"), rep) && LvN_Item(at, 0, x) &&
			LvN_Item(at, 1, y) && LvN_Item(at, 2, z) && LvN_Create(name, x, y, z, "Car", o1) &&
			LvN_SetS(o1, "TgType", "killer") && LvN_Item(dummy, 0, desc) && Mode_VSet(o1, "Descriptor", desc) &&
			LvN_Item(dummy, 2, a) && LvN_Item(a, 1, a1) && Mode_Arith('+', a1, Mode_I(&t, 180), ang);
		if (ok) {
			Native_SetEmpty(a, NV_TUPLE);
			Native_SetInt(Native_Append(a), 0);
			Native_Copy(Native_Append(a), ang);
			Native_SetInt(Native_Append(a), 0);
			ok = Mode_VSet(o1, "Ang", a) && Mode_VGet(o1, "Name", oname);
		}
		nativeVal_t g[9];
		int n = 0;
		for (; ok && n < 9; n++) {
			ok = Native_GVarGet(gvars[n], &g[n]);
		}
		if (!ok && n > 0) {
			Native_Free(&g[--n]);
		}
		ok = ok && PORT_CALL(Vehicles, SetProperties, NULL, "vvvvvvvvvv", oname, &g[0], &g[1], &g[2], &g[3], &g[4],
			&g[5], &g[6], &g[7], &g[8]);
		while (n-- > 0) {
			Native_Free(&g[n]);
		}
		if (ok && LvN_EqI(&ctr, 2)) {
			ok = LvN_SetI(o1, "EnableAI", 1) && LvN_SetS(o1, "Descriptor", "Bad boy");
		}
		ok = ok && Mode_Arith('+', &ctr, Mode_I(&t, 1), p);
		if (ok) {
			Native_Free(&ctr);
			Native_Copy(&ctr, p);
		}
		Mode_Done(&t, 1);
		if (!ok) {
			Native_Free(&ctr);
			return 0;
		}
	}
}

/* an InitOutMap step: import module (OBJLIB 6), then module.func() at the site of its ref; OM_START(module, func)
 * names them */
static byte Om_Start(const char *module, portSite_t *site) {
	return Mode_Import(module) && Port_Invoke(site, NULL, "");
}

#define OM_START(module, func) Om_Start(#module, PORT_SITE(PORT_REF(module, func), 0))

/* InitOutMap(): the players, cameras and scorers of a network client or server, the split screen or the single
 * player game with the police, the traffic, the phone cabs, the elevators and the parkings */
static byte Om_InitOutMap(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *net = Mode_V(&t), *f = Mode_V(&t), *split = Mode_V(&t), *sv = Mode_V(&t), *stats = Mode_V(&t);
	if (!Mode_NetFlags(net) || !LvN_Item(net, 0, f)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(f)) {
		return Mode_Done(&t, OM_START(Ffa, InitClient));
	}
	int c = OM_START(Accelerator, Init) && Native_GVarGet("SplitScreen", split) ? Mode_CmpI(split, "!=", 0) : -1;
	if (c) {
		return Mode_Done(&t, c > 0 && OM_START(Split, Init));
	}
	Native_Free(f);
	if (!LvN_Item(net, 1, f)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(f)) {
		Native_Free(f);
		return Mode_Done(&t, Mode_Import("Ffa") && LvN_Item(net, 2, f) && PORT_CALL(Ffa, InitServer, NULL, "v", f));
	}
	if (!OM_START(Chars, CreateStartupPlayer) || !OM_START(OutPolice, Init) || !OM_START(Traffic, Init) ||
		!OM_START(PhoneCab, Init) || !Mode_GVarSetS("OnTaunt", "Speech.LaunchTaunt") ||
		!OM_START(Elevator, PlayerCreatedElevator) || !OM_START(Parking, Init) || !Mode_Import("MissionsFuncs")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("JackInTheBox.Active", sv);
	if (!LvN_EqS(sv, "1")) {
		return Mode_Done(&t, Mode_GVarSetI("SuperDealAvaliable", 0) && Mode_GVarSetI("EnergyBarActive", 0));
	}
	Native_SetInt(stats, 1);
	for (long i = 1; i <= 3; i++) {
		nativeVal_t res = { 0 }, a = { 0 }, b = { 0 };
		int lt = PORT_CALL(MissionsFuncs, CrazyDealStatus, &res, "i", i) && LvN_Item(&res, 1, &a) &&
			LvN_Item(&res, 2, &b) ? Mode_Cmp(&a, "<", &b) : -1;
		Native_Free(&res);
		Native_Free(&a);
		Native_Free(&b);
		if (lt < 0) {
			return Mode_Done(&t, 0);
		}
		if (lt) {
			Native_SetInt(stats, 0);
		}
	}
	return Mode_Done(&t, Mode_GVarSet("SuperDealAvaliable", stats) && Mode_GVarSetI("EnergyBarActive", 0));
}

/* Scrap.Set(var, dummy[1][1]) */
static byte Om_SetHeight(const char *var, const nativeVal_t *dummy) {
	nativeVal_t p = { 0 }, y = { 0 };
	byte ok = LvN_Item(dummy, 1, &p) && LvN_Item(&p, 1, &y) && Mode_GVarSet(var, &y);
	Native_Free(&p);
	Native_Free(&y);
	return ok;
}

/* BrowseAtmospherePos(): the height of the atmosphere and the top of the map from their dummies */
static byte Om_BrowseAtmospherePos(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *top = Mode_V(&t), *lim = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_DummyStartSeam("DM_Atmosphere", 0) || !Mode_DummyNextSeam(dummy) ||
		(Native_True(dummy) && !Om_SetHeight("AtmosphereLimit", dummy)) || !Mode_DummyStartSeam("DM_MapTop", 0) ||
		!Mode_DummyNextSeam(top)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(top)) {
		return Mode_Done(&t, Om_SetHeight("TopLimit", top));
	}
	return Mode_Done(&t, Native_GVarGet("AtmosphereLimit", lim) && Mode_Arith('+', lim, Mode_I(&t, 5000), v) &&
		Mode_GVarSet("TopLimit", v));
}

/* Init(LevelPath): the start of an outdoor level */
static byte Om_Init(const nativeVal_t *path) {
	static const struct {
		const char *var;
		long value;
	} clouds[] = { { "CameraCloudDistance", 2000 }, { "CameraCloudPlaneSize", 1000 }, { "CameraCloudThickness", 1000 },
		{ "CameraCloudAlpha", 240 }, { "CameraCloudRed", 128 }, { "CameraCloudGreen", 128 }, { "CameraCloudBlue", 140 } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *file = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Init") || !Mode_Cat(&t, file, 2, path, Mode_S(&t, "/2D/Map2D.tga")) ||
		!Mode_GVarSet("FileNameMap2D", file) || !Mode_GVarSetI("CharGrav", 3000) ||
		!PORT_OWN(OutMap, BrowseAtmospherePos, NULL, "") ||
		!PORT_OWN(OutMap, BrowseSpawnPos, NULL, "") || !Mode_GVarSetI("GridUseTopLimit", 1) ||
		!Mode_SetGrid(50000, 50000, 50000) || !Mode_GVarSetI("Max_Rad_Obj", 10000)) {
		return Mode_Done(&t, 0);
	}
	for (size_t i = 0; i < sizeof(clouds) / sizeof(clouds[0]); i++) {
		if (!Mode_GVarSetI(clouds[i].var, clouds[i].value)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, PORT_OWN(OutMap, InitOutMap, NULL, "") && Mode_Import("Items") &&
		PORT_CALL(Items, Init, NULL, "") &&
		Mode_PrintV(Mode_S(&t, "Map initialized as Outdor")));
}

/* the entries (the arity errors of the script's functions) */
MODE_FN(OutMap, BrowseSpawnPos, 0, Om_BrowseSpawnPos())
MODE_FN(OutMap, GetSpawnPos, 0, Om_GetSpawnPos(out))
MODE_FN(OutMap, GetFreeSpawnPos, 0, Om_GetFreeSpawnPos(out))
MODE_FN(OutMap, GetFreeSpawnPosReal, 0, Om_GetFreeSpawnPosReal(out))
MODE_FN(OutMap, GetNearestFreeSpawnPosReal, 1, Om_GetNearestFreeSpawnPosReal(&a[0], out))
MODE_FN(OutMap, GetFreeSpawn, 0, Om_GetFreeSpawn(out))
MODE_FN(OutMap, CreatePlayers, 0, Om_CreatePlayers())
MODE_FN(OutMap, InitOutMap, 0, Om_InitOutMap())
MODE_FN(OutMap, BrowseAtmospherePos, 0, Om_BrowseAtmospherePos())
MODE_FN(OutMap, Init, 1, Om_Init(&a[0]))

/* GetNearestFreeSpawnPos(initialPos) and GetFarthestFreeSpawnPos(initialPos), initialPos optional */
static byte OutMap_NearestK(const nativeVal_t *const *a, nativeVal_t *out) {
	return Om_SpawnPosBy(a[0], 1, out);
}

static byte OutMap_FarthestK(const nativeVal_t *const *a, nativeVal_t *out) {
	return Om_SpawnPosBy(a[0], 0, out);
}

static byte OutMap_GetNearestFreeSpawnPos_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "|O", OutMap_NearestK, out);
}

static char *OutMap_GetNearestFreeSpawnPos_N[] = { "initialPos", NULL };
static const char OutMap_GetNearestFreeSpawnPos_F[] = "|O";

static byte OutMap_GetFarthestFreeSpawnPos_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "|O", OutMap_FarthestK, out);
}

static char *OutMap_GetFarthestFreeSpawnPos_N[] = { "initialPos", NULL };
static const char OutMap_GetFarthestFreeSpawnPos_F[] = "|O";

const portFn_t g_OutMapMethods[] = {
	PORT_FN(BrowseSpawnPos, OutMap_BrowseSpawnPos),
	PORT_FN(GetSpawnPos, OutMap_GetSpawnPos),
	PORT_FN(GetFreeSpawnPos, OutMap_GetFreeSpawnPos),
	PORT_FN(GetFreeSpawnPosReal, OutMap_GetFreeSpawnPosReal),
	PORT_FN(GetNearestFreeSpawnPosReal, OutMap_GetNearestFreeSpawnPosReal),
	PORT_FN(GetFreeSpawn, OutMap_GetFreeSpawn),
	PORT_FNK(GetNearestFreeSpawnPos, OutMap_GetNearestFreeSpawnPos),
	PORT_FNK(GetFarthestFreeSpawnPos, OutMap_GetFarthestFreeSpawnPos),
	PORT_FN(CreatePlayers, OutMap_CreatePlayers),
	PORT_FN(InitOutMap, OutMap_InitOutMap),
	PORT_FN(BrowseAtmospherePos, OutMap_BrowseAtmospherePos),
	PORT_FN(Init, OutMap_Init),
	PORT_FNEND
};

/* ---- InTraffic ---- */

/*
 * InTraffic is native C (docs/objlib-plan.md, stage 2): its functions work on C values and read the variables of the
 * module where the script reads them. The engine calls fc-snd-th records (SWeap.InitAmmo, InitWeap,
 * SetFWeap, SSound.LoadSound, Scrap.Preload3DObject) and Scrap.CreateEntity and Scrap.Set go through their seams
 * (OBJLIB 7); the traffic ships are handle values (OBJLIB 7), their SetModel is called as a bound method (OBJLIB 7).
 * Fx.SetUpShipMotors goes through its ref (PORT_CALL), the module's own functions through its globals (PORT_OWN);
 * SAI.SetStateVehicle goes through its ref (stage 7), SAI.EnableAIVehicle by name (13 arguments, more than a port
 * site takes).
 */

/* a global of the module as its functions read it (out None before) */
static byte It_G(const char *name, nativeVal_t *out) {
	return Mode_G("InTraffic", name, out);
}

/* GetNam(name): the model file of a traffic ship */
static byte It_GetNam(const nativeVal_t *name, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t);
	byte ok = It_G("Path", path) && Mode_Cat(&t, out, 3, path, name, Mode_S(&t, ".M3D"));
	return Mode_Done(&t, ok);
}

/* SWeap.<name>(n) (InitAmmo, InitWeap) through its seam (the hook of a case, as the seams below) */
static byte It_InitTable(const char *name, void *table, byte weap) {
	nativeHook_t hook;
	int seam = Native_Seam("SWeap", name, &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "i", 1L);
	}
	if (weap) {
		WeapTable_Init(table, 1);
	} else {
		AmmoTable_Init(table, 1);
	}
	return 1;
}

/* FakeWeap(): one weapon without ammunition for the traffic */
static byte It_FakeWeap(void) {
	nativeHook_t hook;
	if (!It_InitTable("InitAmmo", &g_pWorld->ammo, 0) || !It_InitTable("InitWeap", &g_pWorld->weapons, 1)) {
		return 0;
	}
	/* SWeap.SetFWeap(0, "iAmmo", 0) */
	int seam = Native_Seam("SWeap", "SetFWeap", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "isi", 0L, "iAmmo", 0L);
	}
	cWeapInfo_SetF(TABLE_AT(g_pWorld->weapons, 0), "iAmmo", 0.0f);
	return 1;
}

/* SSound.LoadSound(file) through its seam */
static byte It_LoadSound(const char *file) {
	nativeHook_t hook;
	nativeVal_t v;
	int seam = Native_Seam("SSound", "LoadSound", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "s", file);
	}
	Native_SetStr(&v, file);
	return Mode_LoadSound(&v);
}

/* LoadFakeTraffic(): the engine sound, the weapon and the models of the three traffic lists */
static byte It_LoadFakeTraffic(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *m = Mode_V(&t), *h = Mode_V(&t), *nm = Mode_V(&t), *all = Mode_V(&t);
	/* the module's FakeWeap as its global */
	if (!It_LoadSound("Sounds/Vehicles/Engines/EngDiesel98CC.wav") ||
		!PORT_OWN(InTraffic, FakeWeap, NULL, "") || !It_G("N_TrafficList", n) || !It_G("M_TrafficList", m) ||
		!Native_Arith('+', n, m, nm) || !It_G("H_TrafficList", h) || !Native_Arith('+', nm, h, all)) {
		return Mode_Done(&t, 0);
	}
	/* for name in all */
	int count = Native_ForCount(all);
	if (count < 0) {
		Native_Raise(NE_TYPE, "loop over non-sequence");
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < count; i++) {
		nativeVal_t name = { 0 }, file = { 0 };
		byte ok = Native_SeqItem(all, i, &name) && PORT_OWN(InTraffic, GetNam, &file, "v", &name) &&
			Mode_PreloadOnes(&file);
		Native_Free(&name);
		Native_Free(&file);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* Scrap.CreateEntity(name, 0, 0, 0, "Car") through its seam (fr6.py swaps it): the handle value or None */
static byte It_Create(const nativeVal_t *name, nativeVal_t *out) {
	nativeHook_t hook;
	int seam = Native_Seam("Scrap", "CreateEntity", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, out, "viiis", name, 0L, 0L, 0L, "Car");
	}
	nativeVal_t zero;
	cEntity *e;
	Native_SetInt(&zero, 0);
	return Mode_Create(name, &zero, &zero, &zero, "Car", &e) && Mode_EntHandle(e, out);
}

/* the ship i of CreateList: a random model of the list, its type, motors, AI and sound */
static byte It_Ship(long i, const nativeVal_t *list, const nativeVal_t *typo) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = Mode_V(&t), *rep = Mode_V(&t), *name = Mode_V(&t), *v = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *k = Mode_V(&t), *model = Mode_V(&t), *vname = Mode_V(&t), *vname2 = Mode_V(&t);
	Native_SetInt(num, i);
	Mode_Repr(num, rep);
	if (!Native_Arith('+', Mode_S(&t, "FakeTraffic"), rep, name) || !It_Create(name, v)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 7: fn = v.SetModel (a bound method of the entity), taken before the model is picked as the script does */
	nativeVal_t *fn = Mode_V(&t);
	int n = Mode_VBoundV(v, "SetModel", fn) ? Native_Size(list) : -1;
	if (n >= 0) {
		Native_SetFloat(r, Scrap_RandRange(0.0, (double)n - 0.1));
	}
	/* SAI.EnableAIVehicle by name: 13 arguments, a port site takes at most PORT_ARGS */
	byte ok = n >= 0 && Native_ToInt(r, k) && Native_Subscript(list, k, model) && Mode_CallArgs(fn, NULL, model, 1) &&
		Mode_VSet(v, "Traf_Type", typo) && PORT_CALL(Fx, SetUpShipMotors, NULL, "v", v) &&
		Mode_VGet(v, "Name", vname) &&
		Native_CallMod("SAI", "EnableAIVehicle", NULL, "vidiiiiidiidd", vname, 1L, 0.1, 2L, 0L, 1L, 0L, 4L, 0.5, 3L, 4L,
			0.785, 0.25) &&
		Mode_VGet(v, "Name", vname2) && PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 4L, vname2) &&
		Mode_VSet(v, "Traf_Active", Mode_I(&t, 1)) && PORT_OWN(InTraffic, SetSound, NULL, "v", v);
	return Mode_Done(&t, ok);
}

/* CreateList(TrafficList, NumTraffic, typo): NumTraffic ships FakeTraffic<i> of random models of the list, driven by
 * the AI (the names start at 0 for each list, as in the script) */
static byte It_CreateList(const nativeVal_t *list, const nativeVal_t *num, const nativeVal_t *typo) {
	long count = 0;
	if (!Mode_Import("Fx") || !Mode_Range(num, &count)) {
		return 0;
	}
	for (long i = 0; i < count; i++) {
		if (!It_Ship(i, list, typo)) {
			return 0;
		}
	}
	return 1;
}

/* FakeTraffic(): the light, medium and heavy traffic of the lists, with the indoor model path */
static byte It_FakeTraffic(void) {
	static const char *const lists[][3] = { { "N_TrafficList", "N_NumTraffic", "Light" },
		{ "M_TrafficList", "M_NumTraffic", "Medium" }, { "H_TrafficList", "H_NumTraffic", "Heavy" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *path = Mode_V(&t), *fmt = Mode_V(&t);
	if (!Native_GVarGet("ShipPathFmt", prev) || !It_G("Path", path) || !Mode_Cat(&t, fmt, 2, path, Mode_S(&t, "%s.m3d")) ||
		!Mode_GVarSet("ShipPathFmt", fmt)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t list = { 0 }, num = { 0 };
		/* the module's CreateList as its global */
		byte ok = It_G(lists[i][0], &list) && It_G(lists[i][1], &num) &&
			PORT_OWN(InTraffic, CreateList, NULL, "vvs", &list, &num, lists[i][2]);
		Native_Free(&list);
		Native_Free(&num);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_GVarSet("ShipPathFmt", prev));
}

/* SetSound(v): the engine loop of a traffic ship */
static byte It_SetSound(const nativeVal_t *v) {
	nativeVal_t name, x;
	Native_SetStr(&name, "EngDiesel98CC");
	if (!Mode_VSet(v, "snd00_name", &name)) {
		return 0;
	}
	static const char *const ints[3] = { "snd00_loop", "snd00_ini", "snd00_end" };
	static const long values[3] = { 1, 1000, 15000 };
	for (int i = 0; i < 3; i++) {
		Native_SetInt(&x, values[i]);
		if (!Mode_VSet(v, ints[i], &x)) {
			return 0;
		}
	}
	Native_SetFloat(&x, 1.0);
	if (!Mode_VSet(v, "snd00_dopp", &x)) {
		return 0;
	}
	Native_SetFloat(&x, 0.08);
	return Mode_VSet(v, "snd00_vol", &x);
}

/* the entries */
MODE_FN(It, GetNamPy, 1, It_GetNam(&a[0], out))
MODE_FN(InTraffic, FakeWeap, 0, It_FakeWeap())
MODE_FN(InTraffic, LoadFakeTraffic, 0, It_LoadFakeTraffic())
MODE_FN(It, CreateListPy, 3, It_CreateList(&a[0], &a[1], &a[2]))
MODE_FN(InTraffic, FakeTraffic, 0, It_FakeTraffic())
MODE_FN(It, SetSoundPy, 1, It_SetSound(&a[0]))

const portFn_t g_InTrafficMethods[] = {
	PORT_FN(GetNam, It_GetNamPy),
	PORT_FN(FakeWeap, InTraffic_FakeWeap),
	PORT_FN(LoadFakeTraffic, InTraffic_LoadFakeTraffic),
	PORT_FN(CreateList, It_CreateListPy),
	PORT_FN(FakeTraffic, InTraffic_FakeTraffic),
	PORT_FN(SetSound, It_SetSoundPy),
	PORT_FNEND
};

PORTS(OutMap)
