/*
 * The weapon scripts in C (the scripts in scripts/weapons/, issue #2): Weapons and the ten weapon modules. Their Init and
 * LoadResources are data (data/weapons/weapons.def, one section pair per module); here the module variables Init sets,
 * the net hooks (each net weapon puts its ClientCreate and ClientAfterCreate in front of Net's), and the missiles of
 * the countermeasures Cloud, EMI and Sonic. File names, forces and times stay variables of the modules, the file and
 * other scripts read them.
 */
#include "scrap/script.h"

static const char weapons_def[] = {
#embed "../../../data/weapons/weapons.def"
	, 0 };

static const scriptStep_t weapons_steps[] = {
	{ "print", Script_StepPrint },
	{ NULL, NULL },
};

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* a section of weapons.def with $idWeap, $idAmmo and $server (NULL for those a section does not use); it stops where
 * the script stops: 0 with the error set */
static byte Weap_Run(const char *section, PyObject *idWeap, PyObject *idAmmo, PyObject *server) {
	PyObject *vars = PyDict_New();
	if (!vars) {
		return 0;
	}
	if ((idWeap && PyDict_SetItemString(vars, "idWeap", idWeap) < 0) ||
		(idAmmo && PyDict_SetItemString(vars, "idAmmo", idAmmo) < 0) ||
		(server && PyDict_SetItemString(vars, "server", server) < 0)) {
		Py_DECREF(vars);
		return 0;
	}
	byte ok = Script_RunFileTry(0, "weapons/weapons.def", section, weapons_def, weapons_steps, vars);
	Py_DECREF(vars);
	return ok;
}

/* `global name; name = "0"+`id`` */
static byte Weap_SetId(const char *mod, const char *name, PyObject *id) {
	return Script_SetGlobal(mod, name, Script_SPlus("0", Script_Repr(id)));
}

/* import Net; LastClientCreate = Net.ClientCreate; Net.ClientCreate = ClientCreate; the same for ClientAfterCreate */
static byte Weap_Hook(const char *mod) {
	return Script_Import((const char *const[]){ "Net", NULL }) && Script_Hook(mod, "Net", "ClientCreate", "LastClientCreate") &&
		Script_Hook(mod, "Net", "ClientAfterCreate", "LastClientAfterCreate");
}

/* the section <mod>Res of LoadResources (import Fx first, as the scripts do) */
static PyObject *Weap_LoadResources(const char *mod) {
	char section[0x20];
	if (!Script_Import((const char *const[]){ "Fx", NULL })) {
		return NULL;
	}
	Str_Printf(section, sizeof(section), "%sRes", mod);
	return Weap_Run(section, NULL, NULL, NULL) ? Py_ReturnNone() : NULL;
}

/* LoadResources() as Init calls it, through the module */
static byte Weap_CallLoad(const char *mod) {
	return Script_Try(mod, "LoadResources", "()");
}

/* Init(idWeap, idAmmo) of a weapon; net: the hooks first and $server for the fire functions (Swarm, Inferno) */
static PyObject *Weap_Init(const char *mod, PyObject *args, byte net) {
	PyObject *idWeap, *idAmmo;
	if (!PyArg_ParseTuple(args, "OO", &idWeap, &idAmmo) || (net && !Weap_Hook(mod)) ||
		!Weap_SetId(mod, "IDWeap", idWeap) || !Weap_SetId(mod, "IDAmmo", idAmmo) || !Weap_CallLoad(mod)) {
		return NULL;
	}
	PyObject *flags = NULL, *server = NULL;
	if (net) {
		flags = Script_Ask("Scrap", "GetNetFlags", "()");
		server = flags ? Script_Index(flags, 0) : NULL;
		if (!server) {
			Py_XDECREF(flags);
			return NULL;
		}
	}
	byte ok = Weap_Run(mod, idWeap, idAmmo, server);
	Py_XDECREF(server);
	Py_XDECREF(flags);
	return ok ? Py_ReturnNone() : NULL;
}

/* Init(IdAmmo) of a countermeasure (Cloud, EMI, Sonic); EMI also keeps IDAmmo */
static PyObject *Weap_InitCounter(const char *mod, PyObject *args, byte keepId) {
	PyObject *idAmmo;
	if (!PyArg_ParseTuple(args, "O", &idAmmo) || (keepId && (!Script_Import((const char *const[]){ "Net", NULL }) ||
		!Weap_SetId(mod, "IDAmmo", idAmmo))) || !Weap_Hook(mod) || !Weap_CallLoad(mod)) {
		return NULL;
	}
	return Weap_Run(mod, NULL, idAmmo, NULL) ? Py_ReturnNone() : NULL;
}

/*
 * ClientCreate / ClientAfterCreate(EntityName, ResourceName) of a net weapon: its own resource by SWeap.NetExec(exec,
 * EntityName) (Swarm, Inferno) or by the module function fn (Cloud, EMI, Sonic; create: the name and six zeros), the
 * others to the function it replaced in Net (last)
 */
static PyObject *Weap_Client(const char *mod, PyObject *args, const char *exec, const char *fn, byte create,
	const char *last) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *res;
	if (!PyArg_ParseTuple(args, "OO", &name, &res)) {
		return NULL;
	}
	PyObject *mine = K(Script_Global(mod, "NetResName"));
	int eq = mine ? Script_Eq(res, mine) : -1;
	if (eq < 0) {
		FAIL();
	}
	if (eq && exec) {
		return Script_Done(&r, Script_Try("SWeap", "NetExec", "(sO)", exec, name) ? Py_ReturnNone() : NULL);
	}
	PyObject *f = K(Script_Global(mod, eq ? fn : last));
	PyObject *a = !f ? NULL : !eq ? Py_BuildValue("(OO)", name, res) :
		create ? Py_BuildValue("(Oiiiiii)", name, 0, 0, 0, 0, 0, 0) : Py_BuildValue("(O)", name);
	PyObject *v = a ? K(Script_Invoke(f, a)) : NULL;
	return Script_Done(&r, v ? Py_ReturnNone() : NULL);
}

/* ActivateHitType(v, Hittype) of Cloud and EMI */
static PyObject *Weap_ActivateHitType(PyObject *self, PyObject *args) {
	PyObject *v, *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &type)) {
		return NULL;
	}
	return Script_HSet(v, "HitType", Script_Ref(type)) ? Py_ReturnNone() : NULL;
}

/* ---- the missile of a countermeasure: the variables Create<mod> sets, in the order of the script ---- */

enum { WS_STR, WS_INT, WS_FLOAT, WS_GLOBAL, WS_ANG, WS_DEATH, WS_HITTYPE, WS_DETONATE, WS_MODEL };

typedef struct {
	int kind;
	const char *var;
	const char *s;
	double d;
} weapStep_t;

/* Create<mod>(Name, PosX, PosY, PosZ, AngX, AngY, AngZ): the missile entity with the steps */
static PyObject *Weap_Create(const char *mod, PyObject *args, const weapStep_t *steps) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *x, *y, *z, *ax, *ay, *az;
	if (!PyArg_ParseTuple(args, "OOOOOOO", &name, &x, &y, &z, &ax, &ay, &az)) {
		return NULL;
	}
	PyObject *l = K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Missile"));
	if (!l) {
		FAIL();
	}
	for (const weapStep_t *s = steps; s->kind >= 0; s++) {
		byte ok;
		switch (s->kind) {
		case WS_STR:
			ok = Script_HSet(l, s->var, PyString_FromString((char *)s->s));
			break;
		case WS_INT:
			ok = Script_HSet(l, s->var, PyInt_FromLong((long)s->d));
			break;
		case WS_FLOAT:
			ok = Script_HSet(l, s->var, PyFloat_FromDouble(s->d));
			break;
		case WS_GLOBAL:
			ok = Script_HSet(l, s->var, Script_Global(mod, s->s));
			break;
		case WS_ANG:
			ok = Script_HSet(l, s->var, Py_BuildValue("(OOO)", ax, ay, az));
			break;
		case WS_DEATH: {
			/* 0.5*LifeTime/AmmoNeed */
			PyObject *life = K(Script_Global(mod, "LifeTime")), *half = life ? K(PyFloat_FromDouble(0.5)) : NULL;
			PyObject *t = half ? K(PyNumber_Multiply(half, life)) : NULL, *need = t ? K(Script_Global(mod, "AmmoNeed")) : NULL;
			ok = need && Script_HSet(l, s->var, PyNumber_Divide(t, need));
			break;
		}
		case WS_HITTYPE:
			ok = Script_LaterIn(0.2, mod, "ActivateHitType", Py_BuildValue("(Os)", l, s->s), NULL);
			break;
		case WS_DETONATE: {
			PyObject *n = K(Script_GetAttr(l, "Name"));
			ok = n && Script_LaterIn(0.2, mod, "SetUpToDetonate", Py_BuildValue("(O)", n), NULL);
			break;
		}
		default: {
			PyObject *file = K(Script_Global(mod, s->s));
			ok = file && K(Script_Method(l, "SetModel", "(O)", file));
			break;
		}
		}
		if (!ok) {
			FAIL();
		}
	}
	return Script_Done(&r, Script_Ref(l));
}

#define WS_END { -1, NULL, NULL, 0 }

static const weapStep_t cloud_create[] = {
	{ WS_STR, "HitType", "None", 0 }, { WS_ANG, "Ang", NULL, 0 }, { WS_GLOBAL, "LifeTime", "LifeTime", 0 },
	{ WS_INT, "SwarmSpeed", NULL, 20000 }, { WS_INT, "HiddenSwarm", NULL, 1 }, { WS_INT, "HideMain", NULL, 1 },
	{ WS_INT, "NoWorldHit", NULL, 1 }, { WS_STR, "Type", "Cloud", 0 }, { WS_DEATH, "SwDthTime", NULL, 0 },
	{ WS_GLOBAL, "NetRes", "NetResName", 0 }, { WS_HITTYPE, NULL, "InOut", 0 }, { WS_STR, "snd00_name", "CloudLoop", 0 },
	{ WS_INT, "snd00_loop", NULL, 1 }, { WS_FLOAT, "snd00_vol0", NULL, 0.15 }, { WS_FLOAT, "snd00_time0", NULL, 0.5 },
	WS_END,
};

static const weapStep_t emi_create[] = {
	{ WS_STR, "HitType", "None", 0 }, { WS_ANG, "Ang", NULL, 0 }, { WS_GLOBAL, "LifeTime", "LifeTime", 0 },
	{ WS_INT, "SwarmSpeed", NULL, 20000 }, { WS_INT, "NoWorldHit", NULL, 1 }, { WS_STR, "Type", "Cloud", 0 },
	{ WS_DEATH, "SwDthTime", NULL, 0 }, { WS_GLOBAL, "NetRes", "NetResName", 0 }, { WS_HITTYPE, NULL, "Inside", 0 },
	{ WS_INT, "HiddenSwarm", NULL, 1 }, { WS_INT, "HideMain", NULL, 1 }, { WS_STR, "snd00_name", "EMILoop", 0 },
	{ WS_INT, "snd00_loop", NULL, 1 }, { WS_FLOAT, "snd00_vol0", NULL, 0.75 }, { WS_FLOAT, "snd00_time0", NULL, 0.5 },
	WS_END,
};

static const weapStep_t sonic_create[] = {
	{ WS_STR, "HitType", "None", 0 }, { WS_INT, "HiddenSwarm", NULL, 1 }, { WS_INT, "NoWorldHit", NULL, 1 },
	{ WS_ANG, "Ang", NULL, 0 }, { WS_GLOBAL, "LifeTime", "LifeTime", 0 }, { WS_INT, "SwarmSpeed", NULL, 20000 },
	{ WS_MODEL, NULL, "MineFile", 0 }, { WS_STR, "Type", "Linear", 0 }, { WS_GLOBAL, "NetRes", "NetResName", 0 },
	{ WS_DETONATE, NULL, NULL, 0 }, { WS_STR, "snd00_name", "SonicLoop", 0 }, { WS_INT, "snd00_loop", NULL, 1 },
	{ WS_FLOAT, "snd00_vol0", NULL, 0.75 }, { WS_FLOAT, "snd00_time0", NULL, 0.5 },
	WS_END,
};

/*
 * the start of SetUp<mod>(Name): l = Scrap.GetEntity(Name), the parent looked up (unused), l.nSwarm = l.NetnSwarm,
 * l.Radius = 100; NULL with the error set
 */
static PyObject *Weap_SetUpStart(PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *l = K(Script_Entity(name)), *parent = l ? K(Script_GetAttr(l, "Parent")) : NULL;
	PyObject *ent = parent ? K(Script_Entity(parent)) : NULL;
	if (!ent || !Script_HSet(l, "nSwarm", Script_GetAttr(l, "NetnSwarm")) ||
		!Script_HSet(l, "Radius", PyInt_FromLong(100))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(l));
}

/* ---- Weapons ---- */

static PyObject *Weapons_InitAmmo(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Weap_Run("Ammo", NULL, NULL, NULL) ? Py_ReturnNone() : NULL;
}

static PyObject *Weapons_InitWeapons(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Weap_Run("Weapons", NULL, NULL, NULL) ? Py_ReturnNone() : NULL;
}

static PyObject *Weapons_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Script_Try("Weapons", "InitAmmo", "()") && Script_Try("Weapons", "InitWeapons", "()") ? Py_ReturnNone() : NULL;
}

PyMethodDef g_WeaponsMethods[] = {
	{ "InitAmmo", Weapons_InitAmmo, 1, NULL },
	{ "InitWeapons", Weapons_InitWeapons, 1, NULL },
	{ "Init", Weapons_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the weapons: LoadResources and Init(idWeap, idAmmo) ---- */

#define WEAPON(mod, net) \
	static PyObject *mod##_LoadResources(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Weap_LoadResources(#mod); \
	} \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_Init(#mod, args, net); \
	}

WEAPON(Vulcan, 0)
WEAPON(Devastator, 0)
WEAPON(Tesla, 0)
WEAPON(ATPC, 0)
WEAPON(Laser, 0)
WEAPON(Swarm, 1)
WEAPON(Inferno, 1)

PyMethodDef g_VulcanMethods[] = {
	{ "LoadResources", Vulcan_LoadResources, 1, NULL }, { "Init", Vulcan_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
PyMethodDef g_DevastatorMethods[] = {
	{ "LoadResources", Devastator_LoadResources, 1, NULL }, { "Init", Devastator_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
PyMethodDef g_TeslaMethods[] = {
	{ "LoadResources", Tesla_LoadResources, 1, NULL }, { "Init", Tesla_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
PyMethodDef g_ATPCMethods[] = {
	{ "LoadResources", ATPC_LoadResources, 1, NULL }, { "Init", ATPC_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
PyMethodDef g_LaserMethods[] = {
	{ "LoadResources", Laser_LoadResources, 1, NULL }, { "Init", Laser_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

/* the missiles of Swarm and Inferno are made by the engine (SWeap.NetExec) */
#define NET_EXEC(mod) \
	static PyObject *mod##_ClientCreate(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_Client(#mod, args, "Create" #mod, NULL, 0, "LastClientCreate"); \
	} \
	static PyObject *mod##_ClientAfterCreate(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_Client(#mod, args, "SetUp" #mod, NULL, 0, "LastClientAfterCreate"); \
	} \
	PyMethodDef g_##mod##Methods[] = { { "ClientCreate", mod##_ClientCreate, 1, NULL }, \
		{ "ClientAfterCreate", mod##_ClientAfterCreate, 1, NULL }, { "LoadResources", mod##_LoadResources, 1, NULL }, \
		{ "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

NET_EXEC(Swarm)
NET_EXEC(Inferno)

/* ---- the countermeasures: their missiles are made here (Create<mod>, SetUp<mod>) ---- */

#define COUNTER(mod, keepId) \
	static PyObject *mod##_ClientCreate(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_Client(#mod, args, NULL, "Create" #mod, 1, "LastClientCreate"); \
	} \
	static PyObject *mod##_ClientAfterCreate(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_Client(#mod, args, NULL, "SetUp" #mod, 0, "LastClientAfterCreate"); \
	} \
	static PyObject *mod##_LoadResources(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Weap_LoadResources(#mod); \
	} \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		return Weap_InitCounter(#mod, args, keepId); \
	}

COUNTER(Cloud, 0)
COUNTER(EMI, 1)
COUNTER(Sonic, 0)

static PyObject *Cloud_CreateCloud(PyObject *self, PyObject *args) {
	(void)self;
	return Weap_Create("Cloud", args, cloud_create);
}

/* SetUpCloud(Name): TopRadius 600 per missile, the Pos read (unused), the effect */
static PyObject *Cloud_SetUpCloud(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	PyObject *l = K(Weap_SetUpStart(args));
	PyObject *n = l ? K(Script_GetAttr(l, "nSwarm")) : NULL, *six = n ? K(PyInt_FromLong(600)) : NULL;
	if (!six || !Script_HSet(l, "TopRadius", PyNumber_Multiply(six, n)) || !Script_HSet(l, "RadiusGrow", PyInt_FromLong(1500)) ||
		!K(Script_GetAttr(l, "Pos")) || !K(Script_Method(l, "SetFX", "(s)", "Cloud"))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(l));
}

PyMethodDef g_CloudMethods[] = {
	{ "ClientCreate", Cloud_ClientCreate, 1, NULL },
	{ "ClientAfterCreate", Cloud_ClientAfterCreate, 1, NULL },
	{ "ActivateHitType", Weap_ActivateHitType, 1, NULL },
	{ "CreateCloud", Cloud_CreateCloud, 1, NULL },
	{ "SetUpCloud", Cloud_SetUpCloud, 1, NULL },
	{ "LoadResources", Cloud_LoadResources, 1, NULL },
	{ "Init", Cloud_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

static PyObject *EMI_CreateEMI(PyObject *self, PyObject *args) {
	(void)self;
	return Weap_Create("EMI", args, emi_create);
}

/* SetUpEMI(Name): TopRadius Radius per missile, it grows to it in a third */
static PyObject *EMI_SetUpEMI(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	PyObject *l = K(Weap_SetUpStart(args));
	PyObject *radius = l ? K(Script_Global("EMI", "Radius")) : NULL, *n = radius ? K(Script_GetAttr(l, "nSwarm")) : NULL;
	if (!n || !Script_HSet(l, "TopRadius", PyNumber_Multiply(radius, n))) {
		FAIL();
	}
	PyObject *top = K(Script_GetAttr(l, "TopRadius")), *three = top ? K(PyInt_FromLong(3)) : NULL;
	if (!three || !Script_HSet(l, "RadiusGrow", PyNumber_Divide(top, three)) ||
		!K(Script_Method(l, "SetFX", "(s)", "EMI"))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(l));
}

PyMethodDef g_EMIMethods[] = {
	{ "ClientCreate", EMI_ClientCreate, 1, NULL },
	{ "ClientAfterCreate", EMI_ClientAfterCreate, 1, NULL },
	{ "ActivateHitType", Weap_ActivateHitType, 1, NULL },
	{ "CreateEMI", EMI_CreateEMI, 1, NULL },
	{ "SetUpEMI", EMI_SetUpEMI, 1, NULL },
	{ "LoadResources", EMI_LoadResources, 1, NULL },
	{ "Init", EMI_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* SonicHit(VehicleName): the vehicle pushed away from SwarmPos, harder near it (at least 700) */
static PyObject *Sonic_SonicHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(name));
	int live = v ? Script_True(v) : -1;
	if (live <= 0) {
		return Script_Done(&r, live < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *pos = K(Script_GetAttr(v, "Pos")), *at = pos ? K(Script_Global("Sonic", "SwarmPos")) : NULL;
	PyObject *vec = at ? K(Script_Ask("SVec", "Sub", "(OO)", pos, at)) : NULL;
	PyObject *xr = vec ? K(Script_Global("Sonic", "XP_Radius")) : NULL, *len = xr ? K(Script_Ask("SVec", "Mod", "(O)", vec)) : NULL;
	PyObject *d = len ? K(PyNumber_Subtract(xr, len)) : NULL, *mod = d ? K(PyNumber_Divide(d, xr)) : NULL;
	PyObject *pow = mod ? K(Script_Global("Sonic", "SwarmPow")) : NULL, *mass = pow ? K(Script_GetAttr(v, "Mass")) : NULL;
	PyObject *k = mass ? K(PyInt_FromLong(1000)) : NULL, *tons = k ? K(PyNumber_Divide(mass, k)) : NULL;
	PyObject *add = tons ? K(PyNumber_Divide(pow, tons)) : NULL, *speed = add ? K(PyNumber_Multiply(mod, add)) : NULL;
	int low = speed ? Script_NumCmp(speed, 700) : -2;
	if (low == -2) {
		FAIL();
	}
	if (low < 0) {
		speed = K(PyInt_FromLong(700));
	}
	PyObject *vel = K(Script_GetAttr(v, "Vel")), *norm = vel ? K(Script_Ask("SVec", "Norm", "(O)", vec)) : NULL;
	PyObject *push = norm ? K(Script_Ask("SVec", "Prod", "(OO)", norm, speed)) : NULL;
	if (!push || !Script_HSet(v, "Vel", Script_Ask("SVec", "Add", "(OO)", vel, push))) {
		FAIL();
	}
	PyObject *car = K(Script_Method(v, "IsType", "(s)", "Car"));
	int isCar = car ? Script_True(car) : -1;
	if (isCar < 0 || (isCar && !Script_HSet(v, "ForceVel", PyInt_FromLong(1)))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* OnHit(swarm, winner, looser): the boom at the mine and the explosion effect there */
static PyObject *Sonic_OnHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *swarm, *winner, *looser;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &swarm, &winner, &looser)) {
		return NULL;
	}
	PyObject *s = K(Script_Entity(swarm)), *orig = s ? K(Script_GetAttr(s, "Pos")) : NULL;
	if (!orig || !Script_Try("SSound", "PlaySound", "(Osd)", orig, "SonicBoom", 1.0) ||
		!Script_Try("Scrap", "SetCallFunc", "(s)", "!SonicXplde")) {
		FAIL();
	}
	for (int i = 0; i < 3; i++) {
		PyObject *c = K(Script_Index(orig, i));
		if (!c || !Script_Try("Scrap", "AddParamf", "(O)", c)) {
			FAIL();
		}
	}
	return Script_Done(&r, Script_Try("Scrap", "Execute", "()") ? Py_ReturnNone() : NULL);
}

/* OnServerHit(swarm, winner, looser): the push of everything in reach (SonicHit), then OnHit */
static PyObject *Sonic_OnServerHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *swarm, *winner, *looser;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &swarm, &winner, &looser)) {
		return NULL;
	}
	PyObject *s = K(Script_Entity(swarm));
	int none = s ? Script_Eq(s, Py_None) : -1;
	if (none < 0) {
		FAIL();
	}
	if (none) {
		PyObject *a = K(PyString_FromString("MISTERIUS HORROR: THE MINE FAILS, MUST BE FIXED BEFORE"));
		PyObject *b[] = { K(PyString_FromString("OnServerHit('")), swarm, K(PyString_FromString("','")), winner,
			K(PyString_FromString("','")), looser, K(PyString_FromString("')")) };
		byte ok = a && b[0] && b[2] && b[4] && b[6] && Script_Print(&a, 1) && Script_Print(b, 7);
		return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
	}
	PyObject *pos = K(Script_GetAttr(s, "Pos"));
	if (!pos || !Script_SetGlobal("Sonic", "SwarmPos", Script_Ref(pos))) {
		FAIL();
	}
	PyObject *c[3];
	for (int i = 0; i < 3; i++) {
		c[i] = K(Script_Index(pos, i));
		if (!c[i]) {
			FAIL();
		}
	}
	PyObject *xr = K(Script_Global("Sonic", "XP_Radius"));
	PyObject *f = xr && Script_Try("Scrap", "SphereCall", "(OOOOss)", c[0], c[1], c[2], xr, "VW", "Sonic.SonicHit") ?
		K(Script_Global("Sonic", "OnHit")) : NULL;
	PyObject *a = f ? Py_BuildValue("(OOO)", swarm, winner, looser) : NULL;
	return Script_Done(&r, a && K(Script_Invoke(f, a)) ? Py_ReturnNone() : NULL);
}

/* OnDeath(LooserName, WinnerName): a live mine goes off */
static PyObject *Sonic_OnDeath(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	PyObject *s = K(Script_Entity(looser)), *active = s ? K(Script_GetAttr(s, "Active")) : NULL;
	int on = active ? Script_True(active) : -1;
	if (on < 0) {
		FAIL();
	}
	if (on) {
		PyObject *f = Script_HSet(s, "Active", PyInt_FromLong(0)) ? K(Script_Global("Sonic", "OnServerHit")) : NULL;
		PyObject *a = f ? Py_BuildValue("(OOs)", looser, winner, "") : NULL;
		if (!a || !K(Script_Invoke(f, a))) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetUpToDetonate(EntityName): the mine hits from now on */
static PyObject *Sonic_SetUpToDetonate(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *l = K(Script_Entity(name));
	byte ok = l && Script_HSet(l, "HitType", PyString_FromString("Hit")) && Script_HSet(l, "VRadius", PyInt_FromLong(100));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *Sonic_CreateSonic(PyObject *self, PyObject *args) {
	(void)self;
	return Weap_Create("Sonic", args, sonic_create);
}

/* SetUpSonic(Name): the mine's size, its hit function and its 3 tons */
static PyObject *Sonic_SetUpSonic(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	PyObject *l = K(Weap_SetUpStart(args));
	byte ok = l && Script_HSet(l, "TopRadius", PyInt_FromLong(2300)) && Script_HSet(l, "RadiusGrow", PyInt_FromLong(1000)) &&
		Script_HSet(l, "OnHit", PyString_FromString("Sonic.OnHit")) && Script_HSet(l, "Mass", PyInt_FromLong(3000));
	return Script_Done(&r, ok ? Script_Ref(l) : NULL);
}

PyMethodDef g_SonicMethods[] = {
	{ "ClientCreate", Sonic_ClientCreate, 1, NULL },
	{ "ClientAfterCreate", Sonic_ClientAfterCreate, 1, NULL },
	{ "SonicHit", Sonic_SonicHit, 1, NULL },
	{ "OnHit", Sonic_OnHit, 1, NULL },
	{ "OnServerHit", Sonic_OnServerHit, 1, NULL },
	{ "OnDeath", Sonic_OnDeath, 1, NULL },
	{ "SetUpToDetonate", Sonic_SetUpToDetonate, 1, NULL },
	{ "CreateSonic", Sonic_CreateSonic, 1, NULL },
	{ "SetUpSonic", Sonic_SetUpSonic, 1, NULL },
	{ "LoadResources", Sonic_LoadResources, 1, NULL },
	{ "Init", Sonic_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
