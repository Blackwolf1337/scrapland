/*
 * The Items module in C (scripts/items.py, issue #2): the boxes of a level (life, ammunition and, in network games, the
 * weapon upgrades) from the DM_Item_ dummies, the upgrades a ship takes, drops and gets back, the boxes of network
 * clients (ClientCreate, hooked into Net.ClientCreate), the hints and the interactive objects of the missions. The
 * module variables (Upgrades, UpgradesRTime, UpgradesID, LastClientCreate, Hints, HINT_RADIUS) stay in the script,
 * other scripts read them. The script reads and writes ship variables by built names ("Upgrd0" + `i`) with eval and
 * exec; here they are attribute reads and writes, which raise the same errors.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* obj.<prefix><n> (a new reference) */
static PyObject *It_Var(PyObject *obj, const char *prefix, long n) {
	char name[0x20];
	Str_Printf(name, sizeof(name), "%s%ld", prefix, n);
	return PyObject_GetAttrString(obj, name);
}

/* SVec.Prod(SVec.Norm((Scrap.Rand(-1,1), Scrap.Rand(-1,1), Scrap.Rand(-1,1))), 100.0): a random speed of a dropped
 * upgrade (a new reference) */
static PyObject *It_RandomVel(void) {
	double x = Scrap_RandRange(-1, 1), y = Scrap_RandRange(-1, 1), z = Scrap_RandRange(-1, 1);
	PyObject *n = Script_Ask("SVec", "Norm", "((ddd))", x, y, z);
	PyObject *v = n ? Script_Ask("SVec", "Prod", "(Od)", n, 100.0) : NULL;
	Py_XDECREF(n);
	return v;
}

/* Upgrades[key] (a new reference, NULL with a KeyError like the script) */
static PyObject *It_Upgrade(PyObject *ups, PyObject *key) {
	return PyObject_GetItem(ups, key);
}

/* ---- the upgrades ---- */

/* ResetUpgrade(UgName): an upgrade back at its place with full ammunition */
static PyObject *Items_ResetUpgrade(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *ups = K(Script_Global("Items", "Upgrades"));
	if (!ups) {
		FAIL();
	}
	for (long i = 0; i < 17; i++) {
		scriptRefs_t t = { .n = 0 };
		PyObject *key = Script_Keep(&t, PyInt_FromLong(i + 1));
		int has = key ? PyMapping_HasKey(ups, key) : -1;
		byte ok = has >= 0;
		if (has > 0) {
			PyObject *up = Script_Keep(&t, It_Upgrade(ups, key)), *n = up ? Script_Keep(&t, Script_Index(up, 0)) : NULL;
			int same = n ? Script_Eq(n, name) : -1;
			ok = same >= 0;
			if (same > 0) {
				PyObject *e = Script_Keep(&t, Script_Entity(name));
				PyObject *v = e && Script_HSet(e, "OnReset", PyString_FromString("")) ? Script_Keep(&t, Py_BuildValue("(Oi)", name, 200)) : NULL;
				ok = v && PyObject_SetItem(ups, key, v) == 0;
			}
		}
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* RestoreUpgrade(id): the upgrade back at its origin, free and with full ammunition */
static PyObject *Items_RestoreUpgrade(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *ups = K(Script_Global("Items", "Upgrades"));
	int has = ups ? PyMapping_HasKey(ups, id) : -1;
	if (has < 0) {
		FAIL();
	}
	if (has) {
		PyObject *up = K(It_Upgrade(ups, id)), *n = up ? K(Script_Index(up, 0)) : NULL;
		PyObject *itm = n ? K(Script_Entity(n)) : NULL, *rt = itm ? K(Script_Global("Items", "UpgradesRTime")) : NULL;
		PyObject *origin = rt && Script_HSet(itm, "Owner", PyString_FromString("")) &&
			Script_HSet(itm, "BackTime", PyFloat_FromDouble(0.0)) && Script_HSet(itm, "rTime", Script_Ref(rt)) ?
			K(PyObject_GetAttrString(itm, "Origin")) : NULL;
		PyObject *up2 = origin && Script_HSet(itm, "Pos", Script_Ref(origin)) ? K(It_Upgrade(ups, id)) : NULL;
		PyObject *n2 = up2 ? K(Script_Index(up2, 0)) : NULL, *v = n2 ? K(Py_BuildValue("(Oi)", n2, 200)) : NULL;
		if (!v || PyObject_SetItem(ups, id, v) < 0) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* DropUpgrade(VehicleName, id, Ammo): the upgrade id leaves the ship behind it with the ammunition left; the item
 * (a new reference) or None */
static PyObject *It_DropUpgrade(PyObject *veh, PyObject *id, PyObject *ammo) {
	scriptRefs_t r = { .n = 0 };
	PyObject *ups = K(Script_Global("Items", "Upgrades"));
	int nz = ups ? Script_NumCmp(id, 0) : -2;
	if (nz == -2) {
		FAIL();
	}
	int has = nz != 0 ? PyMapping_HasKey(ups, id) : 0;
	if (has < 0) {
		FAIL();
	}
	if (!has) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *obj = K(Script_Entity(veh)), *up = obj ? K(It_Upgrade(ups, id)) : NULL;
	PyObject *n = up ? K(Script_Index(up, 0)) : NULL, *itm = n ? K(Script_Entity(n)) : NULL;
	PyObject *v = itm ? K(Py_BuildValue("(OO)", n, ammo)) : NULL;
	if (!v || PyObject_SetItem(ups, id, v) < 0 || !Script_HSet(itm, "Owner", PyString_FromString(""))) {
		FAIL();
	}
	PyObject *ovel = K(PyObject_GetAttrString(obj, "Vel")), *vel = ovel ? K(Script_Ask("SVec", "Norm", "(O)", ovel)) : NULL;
	PyObject *pos = vel ? K(PyObject_GetAttrString(obj, "Pos")) : NULL;
	PyObject *vr = pos ? K(PyObject_GetAttrString(obj, "VRadius")) : NULL, *ir = vr ? K(PyObject_GetAttrString(itm, "Radius")) : NULL;
	PyObject *sum = ir ? K(PyNumber_Add(vr, ir)) : NULL, *ten = sum ? K(PyInt_FromLong(10)) : NULL;
	PyObject *dist = ten ? K(PyNumber_Add(sum, ten)) : NULL, *back = dist ? K(Script_Ask("SVec", "Prod", "(OO)", vel, dist)) : NULL;
	PyObject *at = back ? K(Script_Ask("SVec", "Sub", "(OO)", pos, back)) : NULL;
	PyObject *speed = at && Script_HSet(itm, "Pos", Script_Ref(at)) ? K(Script_Ask("SVec", "Prod", "(Oi)", vel, -100)) : NULL;
	PyObject *rt = speed && Script_HSet(itm, "Vel", Script_Ref(speed)) ? K(Script_Global("Items", "UpgradesRTime")) : NULL;
	if (!rt || !Script_HSet(itm, "BackTime", Script_Ref(rt)) || !Script_HSet(itm, "rTime", PyFloat_FromDouble(0.1)) ||
		!Script_HSet(itm, "OnReset", PyString_FromString("Items.ResetUpgrade"))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(itm));
}

static PyObject *Items_DropUpgrade(PyObject *self, PyObject *args) {
	PyObject *veh, *id, *ammo;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &veh, &id, &ammo)) {
		return NULL;
	}
	return It_DropUpgrade(veh, id, ammo);
}

/* the dropped item flies off in a random direction from the ship */
static byte It_Scatter(PyObject *itm, PyObject *obj) {
	PyObject *vel = It_RandomVel();
	PyObject *pos = vel && Script_HSet(itm, "Vel", vel) ? PyObject_GetAttrString(obj, "Pos") : NULL;
	return pos && Script_HSet(itm, "Pos", pos);
}

/* DropAll(VehicleName): a network ship that dies leaves all its upgrades */
static PyObject *Items_DropAll(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *veh;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &veh)) {
		return NULL;
	}
	PyObject *flags = K(Script_Ask("Scrap", "GetNetFlags", "()")), *off = flags ? K(Py_BuildValue("(iii)", 0, 0, 0)) : NULL;
	PyObject *split = off ? K(Script_Ask("Scrap", "Get", "(s)", "SplitScreen")) : NULL;
	if (!split) {
		FAIL();
	}
	if (Script_Eq(flags, off) > 0 && Script_NumCmp(split, 0) == 0) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *obj = K(Script_Entity(veh)), *nw = obj ? K(Script_Ask("SWeap", "GetNWeap", "()")) : NULL;
	long n;
	if (!nw || !Script_Range(nw, &n)) {
		FAIL();
	}
	for (long i = 0; i < n; i++) {
		scriptRefs_t t = { .n = 0 };
		PyObject *up = Script_Keep(&t, It_Var(obj, "Upgrd0", i));
		PyObject *obj2 = up ? Script_Keep(&t, Script_Entity(veh)) : NULL;
		PyObject *ammo = obj2 ? Script_Keep(&t, It_Var(obj2, "UAmmo0", i)) : NULL;
		PyObject *itm = ammo ? Script_Keep(&t, It_DropUpgrade(veh, up, ammo)) : NULL;
		byte ok = itm && (PyObject_IsTrue(itm) <= 0 || It_Scatter(itm, obj));
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	/* those left, one never knows */
	PyObject *ups = K(Script_Global("Items", "Upgrades"));
	if (!ups) {
		FAIL();
	}
	for (long i = 0; i < 17; i++) {
		scriptRefs_t t = { .n = 0 };
		PyObject *key = Script_Keep(&t, PyInt_FromLong(i + 1));
		int has = key ? PyMapping_HasKey(ups, key) : -1;
		byte ok = has >= 0;
		if (has > 0) {
			PyObject *up = Script_Keep(&t, It_Upgrade(ups, key)), *nm = up ? Script_Keep(&t, Script_Index(up, 0)) : NULL;
			PyObject *itm = nm ? Script_Keep(&t, Script_Entity(nm)) : NULL;
			PyObject *owner = itm ? Script_Keep(&t, PyObject_GetAttrString(itm, "Owner")) : NULL;
			int mine = owner ? Script_Eq(owner, veh) : -1;
			ok = mine >= 0;
			if (mine > 0) {
				PyObject *full = Script_Keep(&t, PyInt_FromLong(200));
				PyObject *d = full ? Script_Keep(&t, It_DropUpgrade(veh, key, full)) : NULL;
				ok = d && It_Scatter(itm, obj);
			}
		}
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* BaseOnTakeUpgrade(ItemName, VehicleName, iWeap, iupgrd): a ship with the weapon takes its upgrade (and drops the one
 * it had) */
static PyObject *It_BaseOnTake(PyObject *item, PyObject *veh, long weap, long upgrd) {
	scriptRefs_t r = { .n = 0 };
	PyObject *obj = K(Script_Entity(veh)), *has = obj ? K(It_Var(obj, "isWpn0", weap)) : NULL;
	int is = has ? Script_True(has) : -1;
	if (is <= 0) {
		return is < 0 ? Script_Done(&r, NULL) : Script_Done(&r, Py_ReturnNone());
	}
	PyObject *itm = K(Script_Entity(item));
	PyObject *old = itm && Script_HSet(itm, "rTime", PyFloat_FromDouble(0.1)) && Script_HSet(itm, "Owner", Script_Ref(veh)) ?
		K(It_Var(obj, "Upgrd0", weap)) : NULL;
	PyObject *ammo = old ? K(It_Var(obj, "UAmmo0", weap)) : NULL;
	PyObject *d = ammo ? K(It_DropUpgrade(veh, old, ammo)) : NULL;
	PyObject *ups = d ? K(Script_Global("Items", "Upgrades")) : NULL, *key = ups ? K(PyInt_FromLong(upgrd)) : NULL;
	int known = key ? PyMapping_HasKey(ups, key) : -1;
	if (known < 0) {
		FAIL();
	}
	if (!known) {
		PyObject *v = K(Py_BuildValue("(Oi)", item, 200));
		if (!v || PyObject_SetItem(ups, key, v) < 0) {
			FAIL();
		}
	}
	char name[0x20];
	Str_Printf(name, sizeof(name), "Upgrd0%ld", weap);
	if (!Script_HSet(obj, name, Script_Ref(key))) {
		FAIL();
	}
	PyObject *up = K(It_Upgrade(ups, key)), *left = up ? K(Script_Index(up, 1)) : NULL;
	Str_Printf(name, sizeof(name), "UAmmo0%ld", weap);
	if (!left || !Script_HSet(obj, name, Script_Ref(left))) {
		FAIL();
	}
	PyObject *up2 = K(It_Upgrade(ups, key)), *left2 = up2 ? K(Script_Index(up2, 1)) : NULL;
	PyObject *v = left2 ? K(Py_BuildValue("(OO)", item, left2)) : NULL;
	if (!v || PyObject_SetItem(ups, key, v) < 0) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

static PyObject *Items_BaseOnTakeUpgrade(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	long weap, upgrd;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOll", &item, &veh, &weap, &upgrd)) {
		return NULL;
	}
	return It_BaseOnTake(item, veh, weap, upgrd);
}

/* OnTake<id>(ItemName, VehicleName): the upgrades of the weapons: Inferno (5), ATPC (4), Devastator (3), Tesla (2),
 * Swarm (1), Vulcan (0) */
#define IT_TAKE(id, weap, upgrd) \
	static PyObject *Items_OnTake##id(PyObject *self, PyObject *args) { \
		PyObject *item, *veh, *res; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "OO", &item, &veh)) { \
			return NULL; \
		} \
		res = It_BaseOnTake(item, veh, weap, upgrd); \
		return res; \
	}

IT_TAKE(iXtr, 5, 12)
IT_TAKE(iSiz, 5, 11)
IT_TAKE(aBom, 4, 10)
IT_TAKE(aSpd, 4, 9)
IT_TAKE(dAim, 3, 8)
IT_TAKE(dBom, 3, 7)
IT_TAKE(tAim, 2, 6)
IT_TAKE(tTra, 2, 5)
IT_TAKE(sRot, 1, 4)
IT_TAKE(sRck, 1, 3)
IT_TAKE(vAim, 0, 2)
IT_TAKE(vSpd, 0, 1)

/* ---- the boxes ---- */

/* SetSound(itm, ItemType): the take sound of the kind (Sound.ItemTake) */
static byte It_SetSound(PyObject *itm, PyObject *type) {
	scriptRefs_t r = { .n = 0 };
	PyObject *table = Script_Import((const char *const[]){ "Sound", NULL }) ? K(Script_Global("Sound", "ItemTake")) : NULL;
	int has = table ? PyMapping_HasKey(table, type) : -1;
	byte ok = has >= 0;
	if (has > 0) {
		PyObject *a = K(PyObject_GetItem(table, type)), *snd = a ? K(Script_Index(a, 0)) : NULL;
		PyObject *vol = snd && Script_HSet(itm, "TakeSound", Script_Ref(snd)) ? K(Script_Index(a, 1)) : NULL;
		ok = vol && Script_HSet(itm, "TakeSoundVol", Script_Ref(vol));
	}
	Script_Done(&r, NULL);
	return ok;
}

static PyObject *Items_SetSound(PyObject *self, PyObject *args) {
	PyObject *itm, *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &itm, &type)) {
		return NULL;
	}
	return It_SetSound(itm, type) ? Py_ReturnNone() : NULL;
}

/* CreateBox(Name, x, y, z, Kind): a box of a kind with its model and network resource (a new reference) */
static PyObject *Items_CreateBox(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *x, *y, *z, *kind;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOO", &name, &x, &y, &z, &kind)) {
		return NULL;
	}
	PyObject *l = K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Item"));
	/* "models/Items/" + Kind + "/" + Kind + ".m3d" */
	PyObject *m = l ? K(Script_PlusS(Script_Plus(Script_PlusS(Script_SPlus("models/Items/", kind), "/"), kind), ".m3d")) : NULL;
	PyObject *set = m ? K(Script_Method(l, "SetModel", "(O)", m)) : NULL;
	if (!set || !Script_HSet(l, "NetRes", Script_SPlus("Box:", kind)) || !Script_HSet(l, "Radius", PyInt_FromLong(3500))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(l));
}

/* the module's CreateBox (the script calls its global) */
static PyObject *It_CreateBox(PyObject *name, PyObject *x, PyObject *y, PyObject *z, PyObject *kind) {
	PyObject *f = Script_Global("Items", "CreateBox");
	PyObject *l = f ? Script_Invoke(f, Py_BuildValue("(OOOOO)", name, x, y, z, kind)) : NULL;
	Py_XDECREF(f);
	return l;
}

/* ClientCreate(EntityName, ResourceName): a box a network client is told to make, else the creation hooked before */
static PyObject *Items_ClientCreate(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *res;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &res)) {
		return NULL;
	}
	PyObject *head = K(PySequence_GetSlice(res, 0, 4));
	if (!head) {
		FAIL();
	}
	if (Script_Is(head, "Box:")) {
		PyObject *kind = K(PySequence_GetSlice(res, 4, PySequence_Length(res))), *zero = kind ? K(PyInt_FromLong(0)) : NULL;
		PyObject *l = zero ? K(It_CreateBox(name, zero, zero, zero, kind)) : NULL;
		if (!l || !It_SetSound(l, kind)) {
			FAIL();
		}
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *last = K(Script_Global("Items", "LastClientCreate"));
	PyObject *v = last ? K(Script_Invoke(last, Py_BuildValue("(OO)", name, res))) : NULL;
	if (!v) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* BrowseItemPos(): the boxes at the DM_Item_<kind> dummies (no upgrades offline, fewer in the demo) */
static PyObject *Items_BrowseItemPos(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Init", NULL }) || !Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Item_*", 1)) {
		return NULL;
	}
	for (;;) {
		scriptRefs_t t = { .n = 0 };
		PyObject *dummy = Script_Keep(&t, Script_Ask("Scrap", "NextDummySearch", "()"));
		if (!dummy) {
			Script_Done(&t, NULL);
			FAIL();
		}
		if (dummy == Py_None) {
			Script_Done(&t, NULL);
			break;
		}
		PyObject *used = Script_Keep(&t, Script_Index(dummy, 3));
		int on = used ? Script_True(used) : -1;
		byte ok = on >= 0;
		if (on > 0) {
			PyObject *dname = Script_Keep(&t, Script_Index(dummy, 0)), *typo = dname ? Script_Keep(&t, PySequence_GetSlice(dname, 8, 12)) : NULL;
			PyObject *flags = typo ? Script_Keep(&t, Script_Ask("Scrap", "GetNetFlags", "()")) : NULL;
			PyObject *off = flags ? Script_Keep(&t, Py_BuildValue("(iii)", 0, 0, 0)) : NULL;
			PyObject *split = off ? Script_Keep(&t, Script_Ask("Scrap", "Get", "(s)", "SplitScreen")) : NULL;
			PyObject *ids = split ? Script_Keep(&t, Script_Global("Items", "UpgradesID")) : NULL;
			PyObject *demo = ids ? Script_Keep(&t, Script_Global("Init", "isDemo")) : NULL;
			int single = demo ? Script_Eq(flags, off) > 0 && Script_NumCmp(split, 0) == 0 : -1;
			int skip = single > 0 ? PySequence_Contains(ids, typo) : 0;
			if (single < 0 || skip < 0) {
				Script_Done(&t, NULL);
				FAIL();
			}
			if (!skip && Script_True(demo) > 0) {
				skip = Script_In(typo, (const char *const[]){ "iXtr", "iSiz", "tAim", "tTra", NULL });
			}
			if (!skip) {
				PyObject *pos = Script_Keep(&t, Script_Index(dummy, 1));
				PyObject *x = pos ? Script_Keep(&t, Script_Index(pos, 0)) : NULL, *y = x ? Script_Keep(&t, Script_Index(pos, 1)) : NULL;
				PyObject *z = y ? Script_Keep(&t, Script_Index(pos, 2)) : NULL;
				PyObject *nm = z ? Script_Keep(&t, Script_Ask("SNet", "GetObjName", "()")) : NULL;
				PyObject *l = nm ? Script_Keep(&t, It_CreateBox(nm, x, y, z, typo)) : NULL;
				PyObject *lname = l && It_SetSound(l, typo) ? Script_Keep(&t, PyObject_GetAttrString(l, "Name")) : NULL;
				ok = lname != NULL;
				if (ok && Script_Is(typo, "Life")) {
					ok = Script_Try("Scrap", "AddItem", "(sOi)", "Life", lname, 50) && Script_HSet(l, "OnTake", PyString_FromString("!TakeLife"));
				} else if (ok && Script_Is(typo, "Part")) {
					ok = Script_Try("Scrap", "AddItem", "(sOOi)", "Ammo", lname, typo, 250) && Script_HSet(l, "OnTake", PyString_FromString("!TakePart"));
				} else if (ok && Script_Is(typo, "Miss")) {
					ok = Script_Try("Scrap", "AddItem", "(sOOi)", "Ammo", lname, typo, 200) && Script_HSet(l, "OnTake", PyString_FromString("!TakeMiss"));
				} else if (ok && Script_Is(typo, "Ener")) {
					ok = Script_Try("Scrap", "AddItem", "(sOOi)", "Ammo", lname, typo, 375) && Script_HSet(l, "OnTake", PyString_FromString("!TakeEner"));
				} else if (ok) {
					ok = Script_HSet(l, "OnTake", Script_SPlus("Items.OnTake", typo));
				}
			}
		}
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* InitResources(): the box models and network resources (the upgrades only in multiplayer) */
static PyObject *Items_InitResources(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *multi = Script_Import((const char *const[]){ "Init", NULL }) ? K(Script_Global("Init", "MultiPlayer")) : NULL;
	PyObject *itms = multi ? K(Py_BuildValue("[ssss]", "Ener", "Miss", "Part", "Life")) : NULL;
	int m = itms ? Script_True(multi) : -1;
	if (m < 0) {
		FAIL();
	}
	if (m) {
		PyObject *ids = K(Script_Global("Items", "UpgradesID"));
		itms = ids ? K(PySequence_Concat(itms, ids)) : NULL;
		if (!itms) {
			FAIL();
		}
	}
	PyObject *kind;
	int st;
	for (int i = 0; (st = Script_Next(itms, i, &kind)) > 0; i++) {
		scriptRefs_t t = { .n = 0 };
		Script_Keep(&t, kind);
		PyObject *m3d = Script_Keep(&t, Script_PlusS(Script_Plus(Script_PlusS(Script_SPlus("models/Items/", kind), "/"), kind), ".m3d"));
		PyObject *res = m3d ? Script_Keep(&t, Script_SPlus("Box:", kind)) : NULL;
		byte ok = res && Script_Try("Scrap", "Preload3DObject", "(Oiii)", m3d, 1, 1, 1) && Script_Try("SNet", "AddResource", "(O)", res);
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	if (st < 0) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* Init(): the resources, the box creation of network clients hooked in, the boxes of the level (not on clients) */
static PyObject *Items_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Net", NULL })) {
		return NULL;
	}
	PyObject *f = K(Script_Global("Items", "InitResources")), *v = f ? K(Script_Invoke(f, PyTuple_New(0))) : NULL;
	PyObject *last = v ? K(Script_Global("Net", "ClientCreate")) : NULL;
	PyObject *mine = last && Script_SetGlobal("Items", "LastClientCreate", Script_Ref(last)) ? K(Script_Global("Items", "ClientCreate")) : NULL;
	PyObject *flags = mine && Script_SetGlobal("Net", "ClientCreate", Script_Ref(mine)) ? K(Script_Ask("Scrap", "GetNetFlags", "()")) : NULL;
	PyObject *first = flags ? K(Script_Index(flags, 0)) : NULL;
	if (!first) {
		FAIL();
	}
	if (Script_NumCmp(first, 0) == 0) {
		PyObject *b = K(Script_Global("Items", "BrowseItemPos")), *w = b ? K(Script_Invoke(b, PyTuple_New(0))) : NULL;
		if (!w) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ---- hints and interactive objects ---- */

/* ShowHint(ItemName, CharName): the text of a hint item when the player takes it; its function; gone when once */
static PyObject *Items_ShowHint(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *item, *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &ch)) {
		return NULL;
	}
	PyObject *i = K(Script_Entity(item));
	PyObject *hints = i && Script_HSet(i, "rTime", PyInt_FromLong(10)) && Script_HSet(i, "AppearsIfFree", PyInt_FromLong(1)) ?
		K(Script_Global("Items", "Hints")) : NULL;
	PyObject *h = hints ? K(PyObject_GetItem(hints, item)) : NULL;
	PyObject *text = h ? K(Script_Index(h, 0)) : NULL, *time = text ? K(Script_Index(h, 1)) : NULL;
	PyObject *when = time ? K(PyNumber_Add(K(PyFloat_FromDouble((double)Timer_GetTime(&g_pWorld->timer))), time)) : NULL;
	if (!when || !Script_Try("SScorer", "SetMsgText", "(OO)", text, when)) {
		FAIL();
	}
	PyObject *h2 = K(PyObject_GetItem(hints, item)), *extra = h2 ? K(Script_Index(h2, 3)) : NULL;
	int call = extra ? Script_True(extra) : -1;
	if (call < 0 || (call && !K(Script_Invoke(extra, Py_BuildValue("(O)", item))))) {
		FAIL();
	}
	PyObject *h3 = K(PyObject_GetItem(hints, item)), *once = h3 ? K(Script_Index(h3, 2)) : NULL;
	int del = once ? Script_True(once) : -1;
	if (del < 0 || (del && (PyObject_DelItem(hints, item) < 0 || !K(Script_Method(i, "Del", "()"))))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* Hint(Pos, Text, time=10, delit=0, ExtraFunc=None): an invisible item that shows a text to the player near it */
static PyObject *Items_Hint(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *pos, *text, *time = NULL, *delit = NULL, *extra = Py_None;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|OOO", &pos, &text, &time, &delit, &extra)) {
		return NULL;
	}
	if (!time) {
		time = K(PyInt_FromLong(10));
	}
	if (!delit) {
		delit = K(PyInt_FromLong(0));
	}
	PyObject *x = K(Script_Index(pos, 0)), *y = x ? K(Script_Index(pos, 1)) : NULL, *z = y ? K(Script_Index(pos, 2)) : NULL;
	PyObject *l = z ? K(Script_Ask("Scrap", "CreateEntity", "(sOOOs)", "Hint", x, y, z, "Item")) : NULL;
	PyObject *rad = l ? K(Script_Global("Items", "HINT_RADIUS")) : NULL;
	PyObject *hints = rad && Script_HSet(l, "Radius", Script_Ref(rad)) && Script_HSet(l, "OnlyUser", PyInt_FromLong(1)) &&
		Script_HSet(l, "OnTake", PyString_FromString("Items.ShowHint")) ? K(Script_Global("Items", "Hints")) : NULL;
	PyObject *name = hints ? K(PyObject_GetAttrString(l, "Name")) : NULL;
	PyObject *v = name ? K(Py_BuildValue("(OOOO)", text, time, delit, extra)) : NULL;
	if (!v || PyObject_SetItem(hints, name, v) < 0) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetInteractiveObjOnFloor(c): the object on the floor below it */
static byte It_OnFloor(PyObject *c) {
	scriptRefs_t r = { .n = 0 };
	PyObject *pos = K(PyObject_GetAttrString(c, "Pos")), *down = pos ? K(Script_Ask("SVec", "Add", "(O(iii))", pos, 0, -10000, 0)) : NULL;
	PyObject *res = down ? K(Script_Ask("Scrap", "TestLine", "(OOs)", pos, down, "")) : NULL;
	int hit = res ? Script_True(res) : -1;
	byte ok = hit >= 0;
	if (hit > 0) {
		PyObject *p = K(Script_Index(res, 0)), *p0 = p ? K(Script_Index(p, 0)) : NULL;
		PyObject *p1 = p0 ? K(Script_Index(p, 1)) : NULL, *p2 = p1 ? K(Script_Index(p, 2)) : NULL;
		PyObject *vr = p2 ? K(PyObject_GetAttrString(c, "VRadius")) : NULL, *y = vr ? K(PyNumber_Add(p1, vr)) : NULL;
		ok = y && Script_HSet(c, "Pos", Py_BuildValue("(OOO)", p0, y, p2));
	} else if (hit == 0) {
		PyObject *name = K(PyObject_GetAttrString(c, "Name"));
		PyObject *msg = name ? K(Script_PlusS(Script_SPlus("B R U T A L    E R R O R : ", name), " was created in an invalid position.\n")) : NULL;
		ok = msg && Script_Try("Scrap", "ConsoleError", "(O)", msg);
	}
	Script_Done(&r, NULL);
	return ok;
}

static PyObject *Items_SetInteractiveObjOnFloor(PyObject *self, PyObject *args) {
	PyObject *c;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &c)) {
		return NULL;
	}
	return It_OnFloor(c) ? Py_ReturnNone() : NULL;
}

/* CreateInteractiveObj(Dummy, Radius): an object of the missions at the dummy DM_Misc_<Dummy>, on the floor (a new
 * reference, None when the dummy is missing) */
static PyObject *Items_CreateInteractiveObj(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *dname, *radius;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &dname, &radius)) {
		return NULL;
	}
	PyObject *pat = K(Script_SPlus("DM_Misc_", dname));
	PyObject *dummy = pat && Script_Try("Scrap", "StartDummySearch", "(Oi)", pat, 0) ? K(Script_Ask("Scrap", "NextDummySearch", "()")) : NULL;
	int found = dummy ? Script_True(dummy) : -1;
	if (found < 0) {
		FAIL();
	}
	if (!found) {
		PyObject *msg = K(Script_PlusS(Script_SPlus("ERROR: <DM_Misc_", dname), "> not found\n"));
		if (!msg || !Script_Try("Scrap", "ConsoleError", "(O)", msg)) {
			FAIL();
		}
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *n = K(Script_Index(dummy, 0)), *pos = n ? K(Script_Index(dummy, 1)) : NULL;
	PyObject *x = pos ? K(Script_Index(pos, 0)) : NULL, *y = x ? K(Script_Index(pos, 1)) : NULL, *z = y ? K(Script_Index(pos, 2)) : NULL;
	PyObject *l = z ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", n, x, y, z, "WithLife")) : NULL;
	PyObject *f = l && Script_HSet(l, "VRadius", Script_Ref(radius)) ? K(Script_Global("Items", "SetInteractiveObjOnFloor")) : NULL;
	if (!f || !K(Script_Invoke(f, Py_BuildValue("(O)", l)))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(l));
}

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
