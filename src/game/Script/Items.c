/*
 * The Items module in C (scripts/items.py, issue #2): the boxes of a level (life, ammunition and, in network games, the
 * weapon upgrades) from the DM_Item_ dummies, the upgrades a ship takes, drops and gets back, the boxes of network
 * clients (ClientCreate, hooked into Net.ClientCreate), the hints and the interactive objects of the missions. The
 * module variables (Upgrades, UpgradesRTime, UpgradesID, LastClientCreate, Hints, HINT_RADIUS) stay in the script,
 * other scripts read them. The script reads and writes ship variables by built names ("Upgrd0" + `i`) with eval and
 * exec; here they are attribute reads and writes for weapon numbers, which raise the same errors (BaseOnTakeUpgrade
 * runs the script's text for any other iWeap).
 *
 * Native C (docs/objlib-plan.md, stages 2 and 3): the functions call the engine in C and work on C values; the
 * PyMethodDef entries are adapters. The variables are read where the script reads them as its globals; Upgrades is
 * changed in place on the dict object of the module (Mode_DictPut, OBJLIB 5), Hints stays a Python object (its items
 * hold the functions of the missions, OBJLIB 5 and 7). The module's own functions that the script calls as its
 * globals (CreateBox, InitResources, BrowseItemPos, SetInteractiveObjOnFloor) go through their refs (PORT_OWN);
 * LastClientCreate and the hook into Net are variables (OBJLIB 5), the functions of the hints values (OBJLIB 4); the
 * boxes are entity handles where they leave the module (OBJLIB 7).
 */
#include <string.h>

#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"

/* the 17 keys of the upgrades (1 to 17) the loops of the script look at */
#define IT_UPGRADES 17

/* ups.has_key(key) as PyMapping_HasKey answers it: ups[key] gives a value (any error is no) */
static int It_Has(const nativeVal_t *ups, const nativeVal_t *key) {
	nativeVal_t v;
	if (!Native_Subscript(ups, key, &v)) {
		PyErr_Clear();
		return 0;
	}
	Native_Free(&v);
	return 1;
}

/* ups[key][i] into *out */
static byte It_UpItem(const nativeVal_t *ups, const nativeVal_t *key, long i, nativeVal_t *out) {
	nativeVal_t up, k;
	if (!Native_Subscript(ups, key, &up)) {
		return 0;
	}
	Native_SetInt(&k, i);
	byte ok = Native_Subscript(&up, &k, out);
	Native_Free(&up);
	return ok;
}

/* ups[key] = (a, b), the dict object of the module changed in place (the native copy ups with it) */
static byte It_SetUp(nativeVal_t *ups, const nativeVal_t *key, const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t v;
	Native_SetEmpty(&v, NV_TUPLE);
	Native_Copy(Native_Append(&v), a);
	Native_Copy(Native_Append(&v), b);
	byte ok = Mode_DictPut("Items", "Upgrades", ups, key, &v);
	Native_Free(&v);
	return ok;
}

/* a == b of the script (the error of a compare that raised) */
static int It_Eq(const nativeVal_t *a, const nativeVal_t *b) {
	int c = Native_Compare(a, b);
	return PyErr_Occurred() ? -1 : c == 0;
}

/* SVec.Prod(SVec.Norm((Scrap.Rand(-1,1), Scrap.Rand(-1,1), Scrap.Rand(-1,1))), 100.0): a random speed of a dropped
 * upgrade */
static byte It_RandomVel(nativeVal_t *out) {
	nativeVal_t r, n, k;
	double x = Scrap_RandRange(-1, 1), y = Scrap_RandRange(-1, 1), z = Scrap_RandRange(-1, 1);
	Native_SetVec(&r, x, y, z);
	Native_SetFloat(&k, 100.0);
	return Mode_VecNorm(&r, &n) && Mode_VecProd(&n, &k, out);
}

/* "models/Items/" + Kind + "/" + Kind + ".m3d" */
static byte It_Model(modeVals_t *t, const nativeVal_t *kind, nativeVal_t *out) {
	return Mode_Cat(t, out, 5, Mode_S(t, "models/Items/"), kind, Mode_S(t, "/"), kind, Mode_S(t, ".m3d"));
}

/* ---- the upgrades ---- */

/* ResetUpgrade(UgName): an upgrade back at its place with full ammunition */
static byte It_ResetUpgrade(const nativeVal_t *name) {
	nativeVal_t ups = { 0 }, key, n = { 0 }, full;
	if (!Mode_G("Items", "Upgrades", &ups)) {
		return 0;
	}
	Native_SetInt(&full, 200);
	byte ok = 1;
	for (long i = 0; ok && i < IT_UPGRADES; i++) {
		Native_SetInt(&key, i + 1);
		if (!It_Has(&ups, &key)) {
			continue;
		}
		int same = It_UpItem(&ups, &key, 0, &n) ? It_Eq(&n, name) : -1;
		Native_Free(&n);
		if (same > 0) {
			cEntity *e;
			ok = Mode_Ent(name, &e) && Mode_SetS(e, Native_Text(name), "OnReset", "") && It_SetUp(&ups, &key, name, &full);
		} else {
			ok = same == 0;
		}
	}
	Native_Free(&ups);
	return ok;
}

/* RestoreUpgrade(id): the upgrade back at its origin, free and with full ammunition */
static byte It_RestoreUpgrade(const nativeVal_t *id) {
	nativeVal_t ups = { 0 }, n = { 0 }, rt = { 0 }, v = { 0 }, full;
	if (!Mode_G("Items", "Upgrades", &ups)) {
		return 0;
	}
	byte ok = 1;
	if (It_Has(&ups, id)) {
		cEntity *e;
		Native_SetInt(&full, 200);
		ok = It_UpItem(&ups, id, 0, &n) && Mode_Ent(&n, &e) && Mode_G("Items", "UpgradesRTime", &rt);
		const char *ref = Native_Text(&n);
		ok = ok && Mode_SetS(e, ref, "Owner", "") && Mode_Set(e, ref, "BackTime", (Native_SetFloat(&v, 0.0), &v)) &&
			Mode_Set(e, ref, "rTime", &rt) && Mode_Get(e, ref, "Origin", &v) && Mode_Set(e, ref, "Pos", &v);
		Native_Free(&n);
		ok = ok && It_UpItem(&ups, id, 0, &n) && It_SetUp(&ups, id, &n, &full);
	}
	Native_Free(&ups);
	Native_Free(&n);
	Native_Free(&rt);
	Native_Free(&v);
	return ok;
}

/* DropUpgrade(VehicleName, id, Ammo): the upgrade id leaves the ship behind it with the ammunition left; the item's
 * handle (OBJLIB 7) or None in *out */
static byte It_DropUpgrade(const nativeVal_t *veh, const nativeVal_t *id, const nativeVal_t *ammo, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ups = Mode_V(&t), *zero = Mode_V(&t);
	Native_SetInt(zero, 0);
	if (!Mode_G("Items", "Upgrades", ups)) {
		return Mode_Done(&t, 0);
	}
	int nz = Native_Compare(id, zero);
	if (PyErr_Occurred()) {
		return Mode_Done(&t, 0);
	}
	if (!nz || !It_Has(ups, id)) {
		Native_SetNone(out);
		return Mode_Done(&t, 1);
	}
	cEntity *obj, *itm;
	nativeVal_t *n = Mode_V(&t), *vel = Mode_V(&t), *dir = Mode_V(&t), *pos = Mode_V(&t), *vr = Mode_V(&t), *ir = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t), *dist = Mode_V(&t), *back = Mode_V(&t), *at = Mode_V(&t), *speed = Mode_V(&t);
	nativeVal_t *rt = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Ent(veh, &obj) || !It_UpItem(ups, id, 0, n) || !Mode_Ent(n, &itm) || !It_SetUp(ups, id, n, ammo)) {
		return Mode_Done(&t, 0);
	}
	const char *oref = Native_Text(veh), *iref = Native_Text(n);
	Native_SetFloat(v, 0.1);
	byte ok = Mode_SetS(itm, iref, "Owner", "") && Mode_Get(obj, oref, "Vel", vel) && Mode_VecNorm(vel, dir) &&
		Mode_Get(obj, oref, "Pos", pos) && Mode_Get(obj, oref, "VRadius", vr) && Mode_Get(itm, iref, "Radius", ir) &&
		Native_Arith('+', vr, ir, sum) && Native_Arith('+', sum, Mode_I(&t, 10), dist) && Mode_VecProd(dir, dist, back) &&
		Mode_VecSub(pos, back, at) && Mode_Set(itm, iref, "Pos", at) && Mode_VecProd(dir, Mode_I(&t, -100), speed) &&
		Mode_Set(itm, iref, "Vel", speed) && Mode_G("Items", "UpgradesRTime", rt) && Mode_Set(itm, iref, "BackTime", rt) &&
		Mode_Set(itm, iref, "rTime", v) && Mode_SetS(itm, iref, "OnReset", "Items.ResetUpgrade") && Mode_EntHandle(itm, out);
	return Mode_Done(&t, ok);
}

/* the dropped item flies off in a random direction from the ship (itm: its handle, obj the ship of the script's
 * handle) */
static byte It_Scatter(const nativeVal_t *itm, cEntity *obj, const char *oref) {
	nativeVal_t vel = { 0 }, pos = { 0 };
	byte ok = It_RandomVel(&vel) && Mode_VSet(itm, "Vel", &vel) && Mode_Get(obj, oref, "Pos", &pos) && Mode_VSet(itm, "Pos", &pos);
	Native_Free(&vel);
	Native_Free(&pos);
	return ok;
}

/* obj.<prefix><i> of the ship's handle */
static byte It_ShipVar(cEntity *obj, const char *oref, const char *prefix, long i, nativeVal_t *out) {
	char name[0x20];
	Str_Printf(name, sizeof(name), "%s%ld", prefix, i);
	return Mode_Get(obj, oref, name, out);
}

/* DropAll(VehicleName): a network ship that dies leaves all its upgrades */
static byte It_DropAll(const nativeVal_t *veh) {
	nativeVal_t split = { 0 }, zero;
	int off = Native_NetOff();
	if (off < 0 || !Native_GVarGet("SplitScreen", &split)) {
		return 0;
	}
	Native_SetInt(&zero, 0);
	int single = off && !Native_Compare(&split, &zero);
	Native_Free(&split);
	if (single) {
		return 1;
	}
	cEntity *obj, *obj2;
	if (!Mode_Ent(veh, &obj)) {
		return 0;
	}
	const char *oref = Native_Text(veh);
	long n = g_pWorld->weapons.count;
	for (long i = 0; i < n; i++) {
		nativeVal_t up = { 0 }, ammo = { 0 }, itm = { 0 };
		byte ok = It_ShipVar(obj, oref, "Upgrd0", i, &up) && Mode_Ent(veh, &obj2) && It_ShipVar(obj2, oref, "UAmmo0", i, &ammo) &&
			It_DropUpgrade(veh, &up, &ammo, &itm);
		if (ok && itm.type != NV_NONE) {
			ok = It_Scatter(&itm, obj, oref);
		}
		Native_Free(&up);
		Native_Free(&ammo);
		Native_Free(&itm);
		if (!ok) {
			return 0;
		}
	}
	/* those left, one never knows */
	nativeVal_t ups = { 0 }, key, full;
	if (!Mode_G("Items", "Upgrades", &ups)) {
		return 0;
	}
	Native_SetInt(&full, 200);
	byte ok = 1;
	for (long i = 0; ok && i < IT_UPGRADES; i++) {
		Native_SetInt(&key, i + 1);
		if (!It_Has(&ups, &key)) {
			continue;
		}
		nativeVal_t nm = { 0 }, owner = { 0 }, d = { 0 }, h = { 0 };
		cEntity *e = NULL;
		ok = It_UpItem(&ups, &key, 0, &nm) && Mode_Ent(&nm, &e) && Mode_Get(e, Native_Text(&nm), "Owner", &owner);
		int mine = ok ? It_Eq(&owner, veh) : -1;
		if (mine > 0) {
			ok = Mode_EntHandle(e, &h) && It_DropUpgrade(veh, &key, &full, &d) && It_Scatter(&h, obj, oref);
			/* the upgrades of the module may have changed */
			Native_Free(&ups);
			ok = ok && Mode_G("Items", "Upgrades", &ups);
		} else {
			ok = mine == 0;
		}
		Native_Free(&nm);
		Native_Free(&owner);
		Native_Free(&d);
		Native_Free(&h);
	}
	Native_Free(&ups);
	return ok;
}

/*
 * eval("obj." + prefix + `iWeap`) (exec of it + rest when rest is given) of BaseOnTakeUpgrade with its locals
 * (ItemName, VehicleName, iWeap, iupgrd and obj; Upgrades is a global of the module): for a
 * weapon number >= 0 the attribute of the ship (value set for exec), for any other value the text the script makes,
 * run in the module's globals (its SyntaxError and the like). The value of eval in *out
 */
static byte It_WeapVar(cEntity *obj, const char *oref, const char *prefix, const nativeVal_t *weap, const nativeVal_t *loc,
	const char *rest, const nativeVal_t *value, nativeVal_t *out) {
	if (weap->type == NV_INT && weap->i >= 0) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "%s%ld", prefix, weap->i);
		return rest ? Mode_Set(obj, oref, name, value) : Mode_Get(obj, oref, name, out);
	}
	/* STAGE 7: only a weapon number that is no int >= 0 (a call from the console; the item scripts pass 0 to 5) runs
	 * the text through the interpreter; it is Script_NotRead's error in the build without it */
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_BufAdd(&b, "obj.", 4);
	Native_BufAdd(&b, prefix, -1);
	Native_Repr(&b, weap, NR_REPR);
	if (rest) {
		Native_BufAdd(&b, rest, -1);
	}
	PyObject *m = PyImport_ImportModule("Items"), *l = m ? Native_ToPy(loc) : NULL, *h = l ? PyEntity_New(oref) : NULL;
	if (l && !h) {
		h = Py_ReturnNone();
	}
	PyObject *v = NULL;
	if (h && PyDict_SetItemString(l, "obj", h) == 0) {
#ifdef SCRAP_NO_PYTHON
		v = Script_NotRead("Items.BaseOnTakeUpgrade", b.p);
#else
		v = PyRun_String(b.p, rest ? Py_file_input : Py_eval_input, PyModule_GetDict(m), l);
#endif
	}
	Native_BufFree(&b);
	byte ok = v && (rest || Native_Arg(v, out));
	Py_XDECREF(v);
	Py_XDECREF(h);
	Py_XDECREF(l);
	Py_XDECREF(m);
	return ok;
}

/* BaseOnTakeUpgrade(ItemName, VehicleName, iWeap, iupgrd): a ship with the weapon takes its upgrade (and drops the one
 * it had) */
static byte It_BaseOnTake(const nativeVal_t *item, const nativeVal_t *veh, const nativeVal_t *weap, const nativeVal_t *upgrd) {
	modeVals_t t = { .n = 0 };
	cEntity *obj, *itm;
	if (!Mode_Ent(veh, &obj)) {
		return 0;
	}
	const char *oref = Native_Text(veh), *iref = Native_Text(item);
	/* the locals of the script's eval and exec texts */
	nativeVal_t *loc = Mode_V(&t), *has = Mode_V(&t), *old = Mode_V(&t), *ammo = Mode_V(&t), *d = Mode_V(&t);
	nativeVal_t *ups = Mode_V(&t), *left = Mode_V(&t), *tenth = Mode_V(&t);
	static const char *const names[] = { "ItemName", "VehicleName", "iWeap", "iupgrd" };
	const nativeVal_t *vals[] = { item, veh, weap, upgrd };
	Native_SetEmpty(loc, NV_DICT);
	for (int i = 0; i < 4; i++) {
		nativeVal_t k, v;
		Native_SetStr(&k, names[i]);
		Native_Copy(&v, vals[i]);
		Native_DictSet(loc, &k, &v);
	}
	if (!It_WeapVar(obj, oref, "isWpn0", weap, loc, NULL, NULL, has)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(has)) {
		return Mode_Done(&t, 1);
	}
	Native_SetFloat(tenth, 0.1);
	if (!Mode_Ent(item, &itm) || !Mode_Set(itm, iref, "rTime", tenth) || !Mode_Set(itm, iref, "Owner", veh) ||
		!It_WeapVar(obj, oref, "Upgrd0", weap, loc, NULL, NULL, old) || !It_WeapVar(obj, oref, "UAmmo0", weap, loc, NULL, NULL, ammo) ||
		!It_DropUpgrade(veh, old, ammo, d)) {
		return Mode_Done(&t, 0);
	}
	int known = Mode_HasKey("Items", "Upgrades", 1, upgrd);
	if (known < 0 || !Mode_G("Items", "Upgrades", ups)) {
		return Mode_Done(&t, 0);
	}
	if (!known && !It_SetUp(ups, upgrd, item, Mode_I(&t, 200))) {
		return Mode_Done(&t, 0);
	}
	if (!It_WeapVar(obj, oref, "Upgrd0", weap, loc, " = iupgrd", upgrd, NULL)) {
		return Mode_Done(&t, 0);
	}
	/* the value first, as the assignment evaluates it (exec of the text for another iWeap evaluates it itself) */
	byte num = weap->type == NV_INT && weap->i >= 0;
	if ((num && !It_UpItem(ups, upgrd, 1, left)) || !It_WeapVar(obj, oref, "UAmmo0", weap, loc, " = Upgrades[iupgrd][1]", left, NULL)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(left);
	Native_Free(ups);
	byte ok = Mode_G("Items", "Upgrades", ups) && It_UpItem(ups, upgrd, 1, left) && It_SetUp(ups, upgrd, item, left);
	return Mode_Done(&t, ok);
}

/* OnTake<id>(ItemName, VehicleName): the upgrades of the weapons: Inferno (5), ATPC (4), Devastator (3), Tesla (2),
 * Swarm (1), Vulcan (0) */
static byte It_OnTake(const nativeVal_t *a, long weap, long upgrd) {
	nativeVal_t w, u;
	Native_SetInt(&w, weap);
	Native_SetInt(&u, upgrd);
	return It_BaseOnTake(&a[0], &a[1], &w, &u);
}

MODE_FN(Items, ResetUpgrade, 1, It_ResetUpgrade(&a[0]))
MODE_FN(Items, RestoreUpgrade, 1, It_RestoreUpgrade(&a[0]))
MODE_FN(Items, DropAll, 1, It_DropAll(&a[0]))
MODE_FN(Items, DropUpgrade, 3, It_DropUpgrade(&a[0], &a[1], &a[2], out))
MODE_FN(Items, BaseOnTakeUpgrade, 4, It_BaseOnTake(&a[0], &a[1], &a[2], &a[3]))
MODE_FN(Items, OnTakeiXtr, 2, It_OnTake(a, 5, 12))
MODE_FN(Items, OnTakeiSiz, 2, It_OnTake(a, 5, 11))
MODE_FN(Items, OnTakeaBom, 2, It_OnTake(a, 4, 10))
MODE_FN(Items, OnTakeaSpd, 2, It_OnTake(a, 4, 9))
MODE_FN(Items, OnTakedAim, 2, It_OnTake(a, 3, 8))
MODE_FN(Items, OnTakedBom, 2, It_OnTake(a, 3, 7))
MODE_FN(Items, OnTaketAim, 2, It_OnTake(a, 2, 6))
MODE_FN(Items, OnTaketTra, 2, It_OnTake(a, 2, 5))
MODE_FN(Items, OnTakesRot, 2, It_OnTake(a, 1, 4))
MODE_FN(Items, OnTakesRck, 2, It_OnTake(a, 1, 3))
MODE_FN(Items, OnTakevAim, 2, It_OnTake(a, 0, 2))
MODE_FN(Items, OnTakevSpd, 2, It_OnTake(a, 0, 1))

/* ---- the boxes ---- */

/* SetSound(itm, ItemType): the take sound of the kind (Sound.ItemTake); itm the box handle the script gave */
static byte It_SetSound(const nativeVal_t *itm, const nativeVal_t *type) {
	nativeVal_t table = { 0 }, a = { 0 }, x = { 0 }, k;
	if (!Mode_Import("Sound") || !Mode_G("Sound", "ItemTake", &table)) {
		return 0;
	}
	byte ok = 1;
	if (It_Has(&table, type)) {
		ok = Native_Subscript(&table, type, &a) && Native_Subscript(&a, (Native_SetInt(&k, 0), &k), &x) &&
			Mode_VSet(itm, "TakeSound", &x);
		Native_Free(&x);
		ok = ok && Native_Subscript(&a, (Native_SetInt(&k, 1), &k), &x) && Mode_VSet(itm, "TakeSoundVol", &x);
	}
	Native_Free(&table);
	Native_Free(&a);
	Native_Free(&x);
	return ok;
}

MODE_FN(Items, SetSound, 2, It_SetSound(&a[0], &a[1]))

/* CreateBox(Name, x, y, z, Kind): a box of a kind with its model and network resource; its handle in *out */
static byte It_CreateBox(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = Mode_V(&t), *res = Mode_V(&t);
	cEntity *e;
	if (!Mode_Create(&a[0], &a[1], &a[2], &a[3], "Item", &e)) {
		return Mode_Done(&t, 0);
	}
	e = e ? World_FindEntity(e->name) : NULL;
	const char *ref = e ? e->name : "";
	byte ok = It_Model(&t, &a[4], m) && Mode_SetModel(e, m) && Native_Arith('+', Mode_S(&t, "Box:"), &a[4], res) &&
		Mode_Set(e, ref, "NetRes", res) && Mode_SetI(e, ref, "Radius", 3500) && Mode_EntHandle(e, out);
	return Mode_Done(&t, ok);
}

MODE_FN(Items, CreateBox, 5, It_CreateBox(a, out))

/* ClientCreate(EntityName, ResourceName): a box a network client is told to make, else the creation hooked before */
static byte It_ClientCreate(const nativeVal_t *name, const nativeVal_t *res) {
	nativeVal_t head = { 0 }, kind = { 0 }, l = { 0 }, zero;
	if (!Native_Slice(res, 0, 4, &head)) {
		return 0;
	}
	byte box = head.type == NV_STR && !strcmp(Native_Text(&head), "Box:");
	Native_Free(&head);
	if (!box) {
		/* OBJLIB 5: the creation that was in Net before, a variable of the module called as the value it holds */
		return Native_CallGlobal("Items", "LastClientCreate", NULL, "vv", name, res);
	}
	Native_SetInt(&zero, 0);
	/* the module's CreateBox (the script calls its global) */
	byte ok = Native_Slice(res, 4, Native_Size(res), &kind) &&
		PORT_OWN(Items, CreateBox, &l, "vvvvv", name, &zero, &zero, &zero, &kind) && It_SetSound(&l, &kind);
	Native_Free(&kind);
	Native_Free(&l);
	return ok;
}

MODE_FN(Items, ClientCreate, 2, It_ClientCreate(&a[0], &a[1]))

/* the box at a dummy of the level (pos its place, typo the kind): its items in the level logic or its take function */
static byte It_DummyBox(modeVals_t *t, const nativeVal_t *pos, const nativeVal_t *typo) {
	static const struct {
		const char *kind;
		const char *type;
		int value;
	} takes[] = { { "Life", "Life", 50 }, { "Part", "Ammo", 250 }, { "Miss", "Ammo", 200 }, { "Ener", "Ammo", 375 } };
	nativeVal_t *c[3], *nm = Mode_V(t), *l = Mode_V(t), *lname = Mode_V(t), *on = Mode_V(t);
	char buf[128];
	for (int i = 0; i < 3; i++) {
		c[i] = Mode_V(t);
		if (!Native_Subscript(pos, Mode_I(t, i), c[i])) {
			return 0;
		}
	}
	/* SNet.GetObjName() */
	Net_GetObjName(buf);
	Native_SetStr(nm, buf);
	/* the module's CreateBox (its global) */
	if (!PORT_OWN(Items, CreateBox, l, "vvvvv", nm, c[0], c[1], c[2], typo) || !It_SetSound(l, typo) ||
		!Mode_VGet(l, "Name", lname)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		if (Native_IsStr(typo, takes[i].kind)) {
			const char *s;
			if (!Native_ArgStr(lname, 2, &s)) {
				return 0;
			}
			Logic_AddItem(takes[i].type, s, takes[i].kind, takes[i].value);
			Str_Printf(buf, sizeof(buf), "!Take%s", takes[i].kind);
			return Mode_VSet(l, "OnTake", Mode_S(t, buf));
		}
	}
	return Native_Arith('+', Mode_S(t, "Items.OnTake"), typo, on) && Mode_VSet(l, "OnTake", on);
}

/* BrowseItemPos(): the boxes at the DM_Item_<kind> dummies (no upgrades offline, fewer in the demo) */
static byte It_BrowseItemPos(void) {
	static const char *const demoSkip[] = { "iXtr", "iSiz", "tAim", "tTra" };
	if (!Mode_Import("Init")) {
		return 0;
	}
	Mode_DummyStart("DM_Item_*", 1);
	for (;;) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *dummy = Mode_V(&t), *used = Mode_V(&t), *dname = Mode_V(&t), *typo = Mode_V(&t), *split = Mode_V(&t);
		nativeVal_t *ids = Mode_V(&t), *demo = Mode_V(&t), *pos = Mode_V(&t);
		Mode_DummyNext(dummy);
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		if (!Native_Subscript(dummy, Mode_I(&t, 3), used)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(used)) {
			Mode_Done(&t, 0);
			continue;
		}
		int off = Native_Subscript(dummy, Mode_I(&t, 0), dname) && Native_Slice(dname, 8, 12, typo) ? Native_NetOff() : -1;
		if (off < 0 || !Native_GVarGet("SplitScreen", split) || !Mode_G("Items", "UpgradesID", ids) || !Mode_G("Init", "isDemo", demo)) {
			return Mode_Done(&t, 0);
		}
		int single = off && !Native_Compare(split, Mode_I(&t, 0));
		int skip = single ? Native_Contains(ids, typo) : 0;
		if (skip < 0) {
			return Mode_Done(&t, 0);
		}
		if (!skip && Native_True(demo)) {
			for (int i = 0; i < 4 && !skip; i++) {
				skip = Native_IsStr(typo, demoSkip[i]);
			}
		}
		if (!skip && (!Native_Subscript(dummy, Mode_I(&t, 1), pos) || !It_DummyBox(&t, pos, typo))) {
			return Mode_Done(&t, 0);
		}
		Mode_Done(&t, 0);
	}
}

/* InitResources(): the box models and network resources (the upgrades only in multiplayer) */
static byte It_InitResources(void) {
	static const char *const kinds[] = { "Ener", "Miss", "Part", "Life" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *multi = Mode_V(&t), *itms = Mode_V(&t), *ids = Mode_V(&t), *all = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_G("Init", "MultiPlayer", multi)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(itms, NV_LIST);
	for (int i = 0; i < 4; i++) {
		Native_SetStr(Native_Append(itms), kinds[i]);
	}
	if (Native_True(multi)) {
		if (!Mode_G("Items", "UpgradesID", ids) || !Native_Arith('+', itms, ids, all)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(itms);
		*itms = *all;
		Native_SetNone(all);
	}
	int n = Native_ForCount(itms);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		int mark = t.n;
		nativeVal_t *kind = Mode_V(&t), *m3d = Mode_V(&t), *res = Mode_V(&t);
		const char *s;
		ok = Native_SeqItem(itms, i, kind) && It_Model(&t, kind, m3d) && Native_Arith('+', Mode_S(&t, "Box:"), kind, res) &&
			Mode_PreloadOnes(m3d) && Native_ArgStr(res, 1, &s);
		if (ok && Net_IsServer()) {
			/* SNet.AddResource(res) */
			Net_AddResource(s);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, ok);
}

/* Init(): the resources, the box creation of network clients hooked in, the boxes of the level (not on clients) */
static byte It_Init(void) {
	if (!Mode_Import("Net")) {
		return 0;
	}
	/* the module's functions as its globals; OBJLIB 5: Net's ClientCreate as a value of both modules */
	if (!PORT_OWN(Items, InitResources, NULL, "")) {
		return 0;
	}
	PyObject *last = Mode_GlobalPy("Net", "ClientCreate");
	byte ok = last && Script_SetGlobal("Items", "LastClientCreate", last);
	PyObject *mine = ok ? Mode_GlobalPy("Items", "ClientCreate") : NULL;
	ok = mine && Script_SetGlobal("Net", "ClientCreate", mine);
	nativeVal_t flags = { 0 }, first = { 0 }, zero;
	Native_SetInt(&zero, 0);
	ok = ok && Mode_NetFlags(&flags) && Native_Subscript(&flags, &zero, &first);
	int client = ok ? Native_Compare(&first, &zero) : 0;
	Native_Free(&flags);
	Native_Free(&first);
	if (!ok || PyErr_Occurred()) {
		return 0;
	}
	return client || PORT_OWN(Items, BrowseItemPos, NULL, "");
}

/* the functions without arguments: the script ignores what it is given (no arity check) */
MODE_FNANY(Items, BrowseItemPos, It_BrowseItemPos())
MODE_FNANY(Items, InitResources, It_InitResources())
MODE_FNANY(Items, Init, It_Init())

/* ---- hints and interactive objects ---- */

/* hints[item][i] of the Hints dict object (OBJLIB 5 and 7: its items hold functions) as a value */
static byte It_HintItem(PyObject *hints, const nativeVal_t *item, int i, nativeVal_t *out) {
	PyObject *k = Native_ToPy(item), *h = k ? PyObject_GetItem(hints, k) : NULL, *v = h ? Script_Index(h, i) : NULL;
	byte ok = v && Native_Arg(v, out);
	Py_XDECREF(v);
	Py_XDECREF(h);
	Py_XDECREF(k);
	return ok;
}

/* ShowHint(ItemName, CharName): the text of a hint item when the player takes it; its function; gone when once */
static byte It_ShowHint(const nativeVal_t *item) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *text = Mode_V(&t), *time = Mode_V(&t), *now = Mode_V(&t), *when = Mode_V(&t), *extra = Mode_V(&t);
	nativeVal_t *once = Mode_V(&t), *h = Mode_V(&t);
	cEntity *e;
	const char *ref = Native_Text(item);
	if (!Mode_Ent(item, &e) || !Mode_SetI(e, ref, "rTime", 10) || !Mode_SetI(e, ref, "AppearsIfFree", 1) ||
		!Mode_EntHandle(e, h)) {
		return Mode_Done(&t, 0);
	}
	PyObject *hints = Mode_GlobalPy("Items", "Hints");
	Native_SetFloat(now, Mode_Time());
	byte ok = hints && It_HintItem(hints, item, 0, text) && It_HintItem(hints, item, 1, time) &&
		Native_Arith('+', now, time, when);
	if (ok) {
		/* SScorer.SetMsgText(text, when) with when as the number it is */
		double d;
		ok = Native_ArgDouble(when, &d) && Mode_MsgText(text, d);
	}
	ok = ok && It_HintItem(hints, item, 3, extra);
	if (ok && Native_True(extra)) {
		/* OBJLIB 4: the function of the mission (any value the Python way) */
		PyObject *f = Native_ToPy(extra);
		ok = f && Native_Call(f, NULL, "v", item);
		Py_XDECREF(f);
	}
	ok = ok && It_HintItem(hints, item, 2, once);
	if (ok && Native_True(once)) {
		/* i.Del() of the handle: the entity may be gone after the function */
		PyObject *k = Native_ToPy(item);
		ok = k && PyObject_DelItem(hints, k) == 0;
		Py_XDECREF(k);
		e = PyEntity_Get(h->obj);
		ok = ok && (e ? Mode_Del(e) >= 0 : Native_EntVar(NULL, ref, "Del", now));
	}
	Py_XDECREF(hints);
	return Mode_Done(&t, ok);
}

MODE_FN(Items, ShowHint, 2, It_ShowHint(&a[0]))

/* Hint(Pos, Text, time=10, delit=0, ExtraFunc=None): an invisible item that shows a text to the player near it (the
 * arguments not given NULL, their defaults here) */
static byte It_Hint(const nativeVal_t *pos, const nativeVal_t *text, const nativeVal_t *time, const nativeVal_t *delit,
	const nativeVal_t *extra) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *c[3], *name = Mode_V(&t), *rad = Mode_V(&t), *hn = Mode_V(&t), *v = Mode_V(&t);
	for (int i = 0; i < 3; i++) {
		c[i] = Mode_V(&t);
		if (!Native_Subscript(pos, Mode_I(&t, i), c[i])) {
			return Mode_Done(&t, 0);
		}
	}
	cEntity *e;
	Native_SetStr(hn, "Hint");
	if (!Mode_Create(hn, c[0], c[1], c[2], "Item", &e)) {
		return Mode_Done(&t, 0);
	}
	e = e ? World_FindEntity(e->name) : NULL;
	const char *ref = e ? e->name : "";
	Native_SetEmpty(v, NV_TUPLE);
	Native_Copy(Native_Append(v), text);
	Native_Copy(Native_Append(v), time ? time : Mode_I(&t, 10));
	Native_Copy(Native_Append(v), delit ? delit : Mode_I(&t, 0));
	if (extra) {
		Native_Copy(Native_Append(v), extra);
	} else {
		Native_SetNone(Native_Append(v));
	}
	byte ok = Mode_G("Items", "HINT_RADIUS", rad) && Mode_Set(e, ref, "Radius", rad) && Mode_SetI(e, ref, "OnlyUser", 1) &&
		Mode_SetS(e, ref, "OnTake", "Items.ShowHint") && Mode_Get(e, ref, "Name", name) &&
		Mode_DictPut("Items", "Hints", NULL, name, v);
	return Mode_Done(&t, ok);
}

/* "OO|OOO" */
MODE_FNOPT(Items, Hint, 2, 5, It_Hint(&a[0], &a[1], MODE_OPT(2), MODE_OPT(3), MODE_OPT(4)))

/* SetInteractiveObjOnFloor(c): the object on the floor below it (c a handle the script gave, any value the Python way) */
static byte It_OnFloor(const nativeVal_t *c) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *off = Mode_V(&t), *down = Mode_V(&t), *res = Mode_V(&t), *p = Mode_V(&t);
	nativeVal_t *vr = Mode_V(&t), *y = Mode_V(&t), *np = Mode_V(&t), *name = Mode_V(&t), *msg = Mode_V(&t);
	Native_SetEmpty(off, NV_TUPLE);
	Native_SetInt(Native_Append(off), 0);
	Native_SetInt(Native_Append(off), -10000);
	Native_SetInt(Native_Append(off), 0);
	if (!Mode_VGet(c, "Pos", pos) || !Mode_VecAdd(pos, off, down) || !Mode_TestLine(pos, down, Mode_S(&t, ""), res)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(res)) {
		nativeVal_t *pc[3];
		byte ok = Native_Subscript(res, Mode_I(&t, 0), p);
		for (int i = 0; ok && i < 3; i++) {
			pc[i] = Mode_V(&t);
			ok = Native_Subscript(p, Mode_I(&t, i), pc[i]);
		}
		ok = ok && Mode_VGet(c, "VRadius", vr) && Native_Arith('+', pc[1], vr, y);
		if (ok) {
			Native_SetEmpty(np, NV_TUPLE);
			Native_Copy(Native_Append(np), pc[0]);
			Native_Copy(Native_Append(np), y);
			Native_Copy(Native_Append(np), pc[2]);
		}
		return Mode_Done(&t, ok && Mode_VSet(c, "Pos", np));
	}
	byte ok = Mode_VGet(c, "Name", name) && Mode_Cat(&t, msg, 3, Mode_S(&t, "B R U T A L    E R R O R : "), name,
		Mode_S(&t, " was created in an invalid position.\n")) && Mode_ConsoleError(msg);
	return Mode_Done(&t, ok);
}

MODE_FN(Items, SetInteractiveObjOnFloor, 1, It_OnFloor(&a[0]))

/* CreateInteractiveObj(Dummy, Radius): an object of the missions at the dummy DM_Misc_<Dummy>, on the floor; its
 * handle (None when the dummy is missing) in *out */
static byte It_CreateObj(const nativeVal_t *dname, const nativeVal_t *radius, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pat = Mode_V(&t), *dummy = Mode_V(&t), *msg = Mode_V(&t), *n = Mode_V(&t), *pos = Mode_V(&t), *c[3];
	const char *s;
	if (!Native_Arith('+', Mode_S(&t, "DM_Misc_"), dname, pat) || !Native_ArgStr(pat, 1, &s)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyStart(s, 0);
	Mode_DummyNext(dummy);
	if (!Native_True(dummy)) {
		byte ok = Mode_Cat(&t, msg, 3, Mode_S(&t, "ERROR: <DM_Misc_"), dname, Mode_S(&t, "> not found\n")) &&
			Mode_ConsoleError(msg);
		Native_SetNone(out);
		return Mode_Done(&t, ok);
	}
	byte ok = Native_Subscript(dummy, Mode_I(&t, 0), n) && Native_Subscript(dummy, Mode_I(&t, 1), pos);
	for (int i = 0; i < 3; i++) {
		c[i] = Mode_V(&t);
		ok = ok && Native_Subscript(pos, Mode_I(&t, i), c[i]);
	}
	cEntity *e = NULL;
	ok = ok && Mode_Create(n, c[0], c[1], c[2], "WithLife", &e);
	e = e ? World_FindEntity(e->name) : NULL;
	ok = ok && Mode_Set(e, e ? e->name : "", "VRadius", radius) && Mode_EntHandle(e, out) &&
		PORT_OWN(Items, SetInteractiveObjOnFloor, NULL, "v", out);
	return Mode_Done(&t, ok);
}

MODE_FN(Items, CreateInteractiveObj, 2, It_CreateObj(&a[0], &a[1], out))

PyMethodDef g_ItemsMethods[] = {
	{ "ResetUpgrade", Items_ResetUpgrade, 1, NULL },
	{ "RestoreUpgrade", Items_RestoreUpgrade, 1, NULL },
	{ "DropAll", Items_DropAll, 1, NULL },
	{ "DropUpgrade", Items_DropUpgrade, 1, NULL },
	{ "BaseOnTakeUpgrade", Items_BaseOnTakeUpgrade, 1, NULL },
	{ "OnTakeiXtr", Items_OnTakeiXtr, 1, NULL },
	{ "OnTakeiSiz", Items_OnTakeiSiz, 1, NULL },
	{ "OnTakeaBom", Items_OnTakeaBom, 1, NULL },
	{ "OnTakeaSpd", Items_OnTakeaSpd, 1, NULL },
	{ "OnTakedAim", Items_OnTakedAim, 1, NULL },
	{ "OnTakedBom", Items_OnTakedBom, 1, NULL },
	{ "OnTaketAim", Items_OnTaketAim, 1, NULL },
	{ "OnTaketTra", Items_OnTaketTra, 1, NULL },
	{ "OnTakesRot", Items_OnTakesRot, 1, NULL },
	{ "OnTakesRck", Items_OnTakesRck, 1, NULL },
	{ "OnTakevAim", Items_OnTakevAim, 1, NULL },
	{ "OnTakevSpd", Items_OnTakevSpd, 1, NULL },
	{ "SetSound", Items_SetSound, 1, NULL },
	{ "ClientCreate", Items_ClientCreate, 1, NULL },
	{ "CreateBox", Items_CreateBox, 1, NULL },
	{ "BrowseItemPos", Items_BrowseItemPos, 1, NULL },
	{ "InitResources", Items_InitResources, 1, NULL },
	{ "Init", Items_Init, 1, NULL },
	{ "ShowHint", Items_ShowHint, 1, NULL },
	{ "Hint", Items_Hint, 1, NULL },
	{ "CreateInteractiveObj", Items_CreateInteractiveObj, 1, NULL },
	{ "SetInteractiveObjOnFloor", Items_SetInteractiveObjOnFloor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Items)
