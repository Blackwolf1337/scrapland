/*
 * The weapon scripts in C (the scripts in scripts/weapons/, issue #2): Weapons and the ten weapon modules. Their Init and
 * LoadResources are data (data/weapons/weapons.def, one section pair per module); here the module variables Init sets,
 * the net hooks (each net weapon puts its ClientCreate and ClientAfterCreate in front of Net's), and the missiles of
 * the countermeasures Cloud, EMI and Sonic. File names, forces and times stay variables of the modules, the file and
 * other scripts read them.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. The variables of the modules are read where the script reads them as its globals and IDWeap,
 * IDAmmo and SwarmPos written where it writes them (OBJLIB 5). The module's own functions a mission may replace are
 * called through their refs (directly while the module holds the port's function, else by name): Weapons.InitAmmo
 * and InitWeapons, LoadResources through the module (PORT_CALL), OnHit, OnServerHit and the creation functions the
 * hooks call through the module's globals (PORT_OWN). The net hooks (Script_Hook on Net's functions) and the function
 * that was in Net before stay variables (OBJLIB 5), the latter called through its entry (MODE_CBGLOBAL); the missiles
 * are entity handles where they leave the module (results, scheduled calls; OBJLIB 7).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/weaps.h"

static const char weapons_def[] = {
#embed "../../../data/weapons/weapons.def"
	, 0 };

static const scriptStep_t weapons_steps[] = {
	{ "print", Script_StepPrint },
	{ NULL, NULL },
};

/* a section of weapons.def with $idWeap, $idAmmo and $server (NULL for those a section does not use); it stops where
 * the script stops: 0 with the error set */
static byte Weap_Run(const char *section, const nativeVal_t *idWeap, const nativeVal_t *idAmmo, const nativeVal_t *server) {
	static const char *const all[] = { "idWeap", "idAmmo", "server" };
	const nativeVal_t *given[] = { idWeap, idAmmo, server }, *vals[3];
	const char *names[3];
	int n = 0;
	for (int i = 0; i < 3; i++) {
		if (given[i]) {
			names[n] = all[i];
			vals[n++] = given[i];
		}
	}
	return Mode_DefRunV(0, "weapons/weapons.def", section, weapons_def, weapons_steps, 1, names, vals, n);
}

/* `global name; name = "0"+`id`` */
static byte Weap_SetId(const char *mod, const char *name, const nativeVal_t *id) {
	nativeBuf_t b;
	nativeVal_t v;
	Native_BufInit(&b);
	Native_BufAdd(&b, "0", 1);
	Native_Repr(&b, id, NR_REPR);
	Native_SetText(&v, b.p, b.len);
	Native_BufFree(&b);
	byte ok = Mode_Put(mod, name, NULL, &v);
	Native_Free(&v);
	return ok;
}

/* import Net; LastClientCreate = Net.ClientCreate; Net.ClientCreate = ClientCreate; the same for ClientAfterCreate */
static byte Weap_Hook(const char *mod) {
	/* OBJLIB 5: the chains of Net's functions stay Python functions until they are C hook pointers */
	return Mode_Import("Net") && Script_Hook(mod, "Net", "ClientCreate", "LastClientCreate") &&
		Script_Hook(mod, "Net", "ClientAfterCreate", "LastClientAfterCreate");
}

/* the section <mod>Res of LoadResources (import Fx first, as the scripts do) */
static byte Weap_LoadResources(const char *mod) {
	char section[0x20];
	Str_Printf(section, sizeof(section), "%sRes", mod);
	return Mode_Import("Fx") && Weap_Run(section, NULL, NULL, NULL);
}

/* Init(idWeap, idAmmo) of a weapon; net: the hooks first and $server (GetNetFlags()[0]) for the fire functions
 * (Swarm, Inferno); load: LoadResources() as Init calls it (mod_CallLoad of WEAP_LOAD) */
static byte Weap_Init(const char *mod, const nativeVal_t *a, byte net, byte (*load)(void)) {
	if ((net && !Weap_Hook(mod)) || !Weap_SetId(mod, "IDWeap", &a[0]) || !Weap_SetId(mod, "IDAmmo", &a[1]) || !load()) {
		return 0;
	}
	nativeVal_t flags = { 0 }, server = { 0 }, zero;
	byte ok = 1;
	if (net) {
		Native_SetInt(&zero, 0);
		ok = Mode_NetFlags(&flags) && Native_Subscript(&flags, &zero, &server);
	}
	ok = ok && Weap_Run(mod, &a[0], &a[1], net ? &server : NULL);
	Native_Free(&flags);
	Native_Free(&server);
	return ok;
}

/* Init(IdAmmo) of a countermeasure (Cloud, EMI, Sonic); EMI also keeps IDAmmo */
static byte Weap_InitCounter(const char *mod, const nativeVal_t *idAmmo, byte keepId, byte (*load)(void)) {
	if ((keepId && (!Mode_Import("Net") || !Weap_SetId(mod, "IDAmmo", idAmmo))) || !Weap_Hook(mod) || !load()) {
		return 0;
	}
	return Weap_Run(mod, NULL, idAmmo, NULL);
}

/* SWeap.NetExec(cmd, name) ("s|s") */
static byte Weap_NetExec(const char *cmd, const nativeVal_t *name) {
	const char *n;
	if (!Native_ArgStr(name, 2, &n)) {
		return 0;
	}
	Missile_NetExec(cmd, n);
	return 1;
}

/*
 * ClientCreate / ClientAfterCreate(EntityName, ResourceName) of a net weapon: its own resource by SWeap.NetExec(exec,
 * EntityName) (Swarm, Inferno) or by the call own of the module function (Cloud, EMI, Sonic, made by COUNTER), the
 * others to the function it replaced in Net (last)
 */
static byte Weap_Client(const char *mod, const nativeVal_t *a, const char *exec, byte (*own)(const nativeVal_t *name),
	const char *last) {
	nativeVal_t mine;
	if (!Mode_G(mod, "NetResName", &mine)) {
		return 0;
	}
	int eq = !Native_Compare(&a[1], &mine);
	Native_Free(&mine);
	if (Native_Failed()) {
		/* OBJLIB 7: the compare of an object that raised */
		return 0;
	}
	if (eq) {
		return exec ? Weap_NetExec(exec, &a[0]) : own(&a[0]);
	}
	/* OBJLIB 5: the function that was in Net, a variable of the module, through its entry */
	return MODE_CBGLOBAL(mod, last, NULL, "vv", &a[0], &a[1]);
}

/* ---- the missile of a countermeasure: the variables Create<mod> sets, in the order of the script ---- */

enum { WS_STR, WS_INT, WS_FLOAT, WS_GLOBAL, WS_ANG, WS_DEATH, WS_HITTYPE, WS_DETONATE, WS_MODEL };

typedef struct {
	int kind;
	const char *var;
	const char *s;
	double d;
} weapStep_t;

/* one step of Create<mod> on the missile e (ref: the name of its handle; ang: AngX, AngY, AngZ); 0 with the error */
static byte Weap_Step(const char *mod, cEntity *e, const char *ref, const weapStep_t *s, const nativeVal_t *ang) {
	nativeVal_t v = { 0 }, w = { 0 }, x = { 0 };
	byte ok;
	switch (s->kind) {
	case WS_STR:
		return Mode_SetS(e, ref, s->var, s->s);
	case WS_INT:
		return Mode_SetI(e, ref, s->var, (long)s->d);
	case WS_FLOAT:
		Native_SetFloat(&v, s->d);
		return Mode_Set(e, ref, s->var, &v);
	case WS_GLOBAL:
		ok = Mode_G(mod, s->s, &v) && Mode_Set(e, ref, s->var, &v);
		break;
	case WS_ANG:
		Native_SetEmpty(&v, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_Copy(Native_Append(&v), &ang[i]);
		}
		ok = Mode_Set(e, ref, s->var, &v);
		break;
	case WS_DEATH:
		/* 0.5*LifeTime/AmmoNeed */
		Native_SetFloat(&x, 0.5);
		ok = Mode_G(mod, "LifeTime", &v) && Native_Arith('*', &x, &v, &w);
		Native_Free(&v);
		ok = ok && Mode_G(mod, "AmmoNeed", &v) && Native_Arith('/', &w, &v, &x) && Mode_Set(e, ref, s->var, &x);
		break;
	case WS_HITTYPE: {
		/* the handle and the hit type to ActivateHitType in 0.2 s */
		nativeVal_t args[2] = { { 0 }, { 0 } };
		Native_SetStr(&args[1], s->s);
		ok = Mode_EntHandle(e, &args[0]) && Mode_LaterIn(0.2, mod, "ActivateHitType", args, 2, NULL);
		Native_Free(&args[0]);
		break;
	}
	case WS_DETONATE:
		ok = Mode_Get(e, ref, "Name", &v) && Mode_LaterIn(0.2, mod, "SetUpToDetonate", &v, 1, NULL);
		break;
	default:
		ok = Mode_G(mod, s->s, &v) && Mode_SetModel(e, &v);
		break;
	}
	Native_Free(&v);
	Native_Free(&w);
	Native_Free(&x);
	return ok;
}

/* Create<mod>(Name, PosX, PosY, PosZ, AngX, AngY, AngZ): the missile entity with the steps; its handle in *out */
static byte Weap_Create(const char *mod, const nativeVal_t *a, const weapStep_t *steps, nativeVal_t *out) {
	cEntity *e;
	if (!Mode_Create(&a[0], &a[1], &a[2], &a[3], "Missile", &e)) {
		return 0;
	}
	/* the handle of the script finds the entity by its name */
	e = e ? World_FindEntity(e->name) : NULL;
	const char *ref = e ? e->name : "";
	for (const weapStep_t *s = steps; s->kind >= 0; s++) {
		if (!Weap_Step(mod, e, ref, s, &a[4])) {
			return 0;
		}
	}
	return Mode_EntHandle(e, out);
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
 * l.Radius = 100; *e the missile (its handle is made for Name)
 */
static byte Weap_SetUpStart(const nativeVal_t *name, cEntity **e) {
	nativeVal_t parent = { 0 }, n = { 0 };
	cEntity *p;
	const char *ref = Native_Text(name);
	byte ok = Mode_Ent(name, e) && Mode_Get(*e, ref, "Parent", &parent) && Mode_Ent(&parent, &p) &&
		Mode_Get(*e, ref, "NetnSwarm", &n) && Mode_Set(*e, ref, "nSwarm", &n) && Mode_SetI(*e, ref, "Radius", 100);
	Native_Free(&parent);
	Native_Free(&n);
	return ok;
}

/* l.var = k * l.nSwarm (k first, as the script multiplies) */
static byte Weap_PerMissile(cEntity *e, const char *ref, const char *var, const nativeVal_t *k) {
	nativeVal_t n = { 0 }, v = { 0 };
	byte ok = Mode_Get(e, ref, "nSwarm", &n) && Native_Arith('*', k, &n, &v) && Mode_Set(e, ref, var, &v);
	Native_Free(&n);
	Native_Free(&v);
	return ok;
}

/* ---- Weapons ---- */

/* Init(): InitAmmo and InitWeapons through the module */
static byte Weapons_InitBoth(void) {
	return PORT_CALL(Weapons, InitAmmo, NULL, "") && PORT_CALL(Weapons, InitWeapons, NULL, "");
}

/* the functions without arguments: the scripts ignore what they are given (no arity check, MODE_FNANY) */
MODE_FNANY(Weapons, InitAmmo, Weap_Run("Ammo", NULL, NULL, NULL))
MODE_FNANY(Weapons, InitWeapons, Weap_Run("Weapons", NULL, NULL, NULL))
MODE_FNANY(Weapons, Init, Weapons_InitBoth())

PyMethodDef g_WeaponsMethods[] = {
	{ "InitAmmo", Weapons_InitAmmo, 1, NULL },
	{ "InitWeapons", Weapons_InitWeapons, 1, NULL },
	{ "Init", Weapons_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* LoadResources() of a module (no arity check), and mod_CallLoad, the call of Init through the module (a mission may
 * replace it) */
#define WEAP_LOAD(mod) \
	MODE_FNANY(mod, LoadResources, Weap_LoadResources(#mod)) \
	static byte mod##_CallLoad(void) { \
		return PORT_CALL(mod, LoadResources, NULL, ""); \
	}

/* ---- the weapons: LoadResources and Init(idWeap, idAmmo) ---- */

#define WEAPON(mod, net) \
	WEAP_LOAD(mod) \
	MODE_FN(mod, Init, 2, Weap_Init(#mod, a, net, mod##_CallLoad))

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
	MODE_FN(mod, ClientCreate, 2, Weap_Client(#mod, a, "Create" #mod, NULL, "LastClientCreate")) \
	MODE_FN(mod, ClientAfterCreate, 2, Weap_Client(#mod, a, "SetUp" #mod, NULL, "LastClientAfterCreate")) \
	PyMethodDef g_##mod##Methods[] = { { "ClientCreate", mod##_ClientCreate, 1, NULL }, \
		{ "ClientAfterCreate", mod##_ClientAfterCreate, 1, NULL }, { "LoadResources", mod##_LoadResources, 1, NULL }, \
		{ "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

NET_EXEC(Swarm)
NET_EXEC(Inferno)

/* ---- the countermeasures: their missiles are made here (Create<mod>, SetUp<mod>) ---- */

/* the hooks call Create<mod>(EntityName, 0, 0, 0, 0, 0, 0) and SetUp<mod>(EntityName) through the module's globals (a
 * mission may replace them) */
#define COUNTER(mod, keepId) \
	static byte mod##_OwnCreate(const nativeVal_t *name) { \
		return PORT_OWN(mod, Create##mod, NULL, "viiiiii", name, 0L, 0L, 0L, 0L, 0L, 0L); \
	} \
	static byte mod##_OwnSetUp(const nativeVal_t *name) { \
		return PORT_OWN(mod, SetUp##mod, NULL, "v", name); \
	} \
	MODE_FN(mod, ClientCreate, 2, Weap_Client(#mod, a, NULL, mod##_OwnCreate, "LastClientCreate")) \
	MODE_FN(mod, ClientAfterCreate, 2, Weap_Client(#mod, a, NULL, mod##_OwnSetUp, "LastClientAfterCreate")) \
	WEAP_LOAD(mod) \
	MODE_FN(mod, Init, 1, Weap_InitCounter(#mod, a, keepId, mod##_CallLoad))

COUNTER(Cloud, 0)
COUNTER(EMI, 1)
COUNTER(Sonic, 0)

/* ActivateHitType(v, Hittype) of Cloud and EMI: v the handle the creation scheduled (any value the Python way) */
MODE_FN(Weap, ActivateHitType, 2, Mode_VSet(&a[0], "HitType", &a[1]))

MODE_FN(Cloud, CreateCloud, 7, Weap_Create("Cloud", a, cloud_create, out))

/* SetUpCloud(Name): TopRadius 600 per missile, the Pos read (unused), the effect */
static byte Cloud_SetUp(const nativeVal_t *name, nativeVal_t *out) {
	cEntity *e;
	nativeVal_t k, pos = { 0 };
	if (!Weap_SetUpStart(name, &e)) {
		return 0;
	}
	const char *ref = Native_Text(name);
	Native_SetInt(&k, 600);
	byte ok = Weap_PerMissile(e, ref, "TopRadius", &k) && Mode_SetI(e, ref, "RadiusGrow", 1500) &&
		Mode_Get(e, ref, "Pos", &pos) && Mode_SetFX(e, "Cloud") && Mode_EntHandle(e, out);
	Native_Free(&pos);
	return ok;
}

MODE_FN(Cloud, SetUpCloud, 1, Cloud_SetUp(&a[0], out))

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

MODE_FN(EMI, CreateEMI, 7, Weap_Create("EMI", a, emi_create, out))

/* SetUpEMI(Name): TopRadius Radius per missile, it grows to it in a third */
static byte EMI_SetUp(const nativeVal_t *name, nativeVal_t *out) {
	cEntity *e;
	nativeVal_t radius = { 0 }, top = { 0 }, three, grow = { 0 };
	if (!Weap_SetUpStart(name, &e)) {
		return 0;
	}
	const char *ref = Native_Text(name);
	Native_SetInt(&three, 3);
	byte ok = Mode_G("EMI", "Radius", &radius) && Weap_PerMissile(e, ref, "TopRadius", &radius) &&
		Mode_Get(e, ref, "TopRadius", &top) && Native_Arith('/', &top, &three, &grow) &&
		Mode_Set(e, ref, "RadiusGrow", &grow) && Mode_SetFX(e, "EMI") && Mode_EntHandle(e, out);
	Native_Free(&radius);
	Native_Free(&top);
	Native_Free(&grow);
	return ok;
}

MODE_FN(EMI, SetUpEMI, 1, EMI_SetUp(&a[0], out))

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

/* the values of SonicHit, freed together */
#define SONIC_VALS 16

/* SonicHit(VehicleName): the vehicle pushed away from SwarmPos, harder near it (at least 700) */
static byte Sonic_Hit(const nativeVal_t *name) {
	cEntity *e;
	if (!Mode_Ent(name, &e)) {
		return 0;
	}
	if (!e) {
		return 1;
	}
	const char *ref = Native_Text(name);
	enum { POS, AT, VEC, XR, LEN, D, MOD, POW, MASS, K, TONS, ADD, SPEED, VEL, NORM, PUSH };
	nativeVal_t v[SONIC_VALS] = { { 0 } }, sum = { 0 };
	Native_SetInt(&v[K], 1000);
	byte ok = Mode_Get(e, ref, "Pos", &v[POS]) && Mode_G("Sonic", "SwarmPos", &v[AT]) && Mode_VecSub(&v[POS], &v[AT], &v[VEC]) &&
		Mode_G("Sonic", "XP_Radius", &v[XR]) && Mode_VecMod(&v[VEC], &v[LEN]) && Native_Arith('-', &v[XR], &v[LEN], &v[D]) &&
		Native_Arith('/', &v[D], &v[XR], &v[MOD]) && Mode_G("Sonic", "SwarmPow", &v[POW]) &&
		Mode_Get(e, ref, "Mass", &v[MASS]) && Native_Arith('/', &v[MASS], &v[K], &v[TONS]) &&
		Native_Arith('/', &v[POW], &v[TONS], &v[ADD]) && Native_Arith('*', &v[MOD], &v[ADD], &v[SPEED]);
	if (ok) {
		nativeVal_t low;
		Native_SetInt(&low, 700);
		if (Native_Compare(&v[SPEED], &low) < 0) {
			Native_Free(&v[SPEED]);
			v[SPEED] = low;
		}
		ok = !Native_Failed();
	}
	ok = ok && Mode_Get(e, ref, "Vel", &v[VEL]) && Mode_VecNorm(&v[VEC], &v[NORM]) &&
		Mode_VecProd(&v[NORM], &v[SPEED], &v[PUSH]) && Mode_VecAdd(&v[VEL], &v[PUSH], &sum) && Mode_Set(e, ref, "Vel", &sum);
	int car = ok ? Mode_IsType(e, "Car") : -1;
	ok = car > 0 ? Mode_SetI(e, ref, "ForceVel", 1) : car == 0;
	for (int i = 0; i < SONIC_VALS; i++) {
		Native_Free(&v[i]);
	}
	Native_Free(&sum);
	return ok;
}

MODE_FN(Sonic, SonicHit, 1, Sonic_Hit(&a[0]))

/* Scrap.SetCallFunc("!SonicXplde"), Scrap.AddParamf of pos[0], pos[1], pos[2], Scrap.Execute() */
static byte Sonic_Explode(const nativeVal_t *pos) {
	if (pos->type == NV_VEC) {
		const float p[3] = { (float)pos->v[0], (float)pos->v[1], (float)pos->v[2] };
		CallFunc_ExecuteF("!SonicXplde", p, 3);
		return 1;
	}
	/* another value the engine functions take through their refs, with their errors */
	if (!PORT_CALL(Scrap, SetCallFunc, NULL, "s", "!SonicXplde")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t k, c;
		Native_SetInt(&k, i);
		if (!Native_Subscript(pos, &k, &c)) {
			return 0;
		}
		byte ok = PORT_CALL(Scrap, AddParamf, NULL, "v", &c);
		Native_Free(&c);
		if (!ok) {
			return 0;
		}
	}
	return PORT_CALL(Scrap, Execute, NULL, "");
}

/* OnHit(swarm, winner, looser): the boom at the mine and the explosion effect there */
static byte Sonic_OnHitAt(const nativeVal_t *swarm) {
	cEntity *e;
	nativeVal_t orig = { 0 }, snd, vol;
	Native_SetStr(&snd, "SonicBoom");
	Native_SetFloat(&vol, 1.0);
	byte ok = Mode_Ent(swarm, &e) && Mode_Get(e, Native_Text(swarm), "Pos", &orig) && Mode_PlaySound(&orig, &snd, &vol) &&
		Sonic_Explode(&orig);
	Native_Free(&orig);
	return ok;
}

MODE_FN(Sonic, OnHit, 3, Sonic_OnHitAt(&a[0]))

/* OnServerHit(swarm, winner, looser): the push of everything in reach (SonicHit), then OnHit */
static byte Sonic_OnServerHitAt(const nativeVal_t *a) {
	cEntity *e;
	if (!Mode_Ent(&a[0], &e)) {
		return 0;
	}
	if (!e) {
		nativeVal_t t[4];
		Native_SetStr(&t[0], "MISTERIUS HORROR: THE MINE FAILS, MUST BE FIXED BEFORE");
		Native_SetStr(&t[1], "OnServerHit('");
		Native_SetStr(&t[2], "','");
		Native_SetStr(&t[3], "')");
		const nativeVal_t *head[] = { &t[0] }, *line[] = { &t[1], &a[0], &t[2], &a[1], &t[2], &a[2], &t[3] };
		return Mode_Print(head, 1) && Mode_Print(line, 7);
	}
	nativeVal_t pos = { 0 }, c[3] = { { 0 }, { 0 }, { 0 } }, xr = { 0 }, list, fn;
	byte ok = Mode_Get(e, Native_Text(&a[0]), "Pos", &pos) && Mode_Put("Sonic", "SwarmPos", NULL, &pos);
	for (int i = 0; ok && i < 3; i++) {
		nativeVal_t k;
		Native_SetInt(&k, i);
		ok = Native_Subscript(&pos, &k, &c[i]);
	}
	Native_SetStr(&list, "VW");
	Native_SetStr(&fn, "Sonic.SonicHit");
	ok = ok && Mode_G("Sonic", "XP_Radius", &xr) && Mode_SphereCall(&c[0], &c[1], &c[2], &xr, &list, &fn);
	Native_Free(&pos);
	Native_Free(&xr);
	for (int i = 0; i < 3; i++) {
		Native_Free(&c[i]);
	}
	if (!ok) {
		return 0;
	}
	/* OnHit through the module's global (a mission may replace it) */
	return PORT_OWN(Sonic, OnHit, NULL, "vvv", &a[0], &a[1], &a[2]);
}

MODE_FN(Sonic, OnServerHit, 3, Sonic_OnServerHitAt(a))

/* OnDeath(LooserName, WinnerName): a live mine goes off */
static byte Sonic_OnDeathOf(const nativeVal_t *a) {
	cEntity *e;
	nativeVal_t active = { 0 };
	if (!Mode_Ent(&a[0], &e) || !Mode_Get(e, Native_Text(&a[0]), "Active", &active)) {
		return 0;
	}
	byte on = Native_True(&active);
	Native_Free(&active);
	if (!on) {
		return 1;
	}
	if (!Mode_SetI(e, Native_Text(&a[0]), "Active", 0)) {
		return 0;
	}
	/* OnServerHit through the module's global */
	return PORT_OWN(Sonic, OnServerHit, NULL, "vvs", &a[0], &a[1], "");
}

MODE_FN(Sonic, OnDeath, 2, Sonic_OnDeathOf(a))

/* SetUpToDetonate(EntityName): the mine hits from now on */
static byte Sonic_Detonate(const nativeVal_t *name) {
	cEntity *e;
	const char *ref = Native_Text(name);
	return Mode_Ent(name, &e) && Mode_SetS(e, ref, "HitType", "Hit") && Mode_SetI(e, ref, "VRadius", 100);
}

MODE_FN(Sonic, SetUpToDetonate, 1, Sonic_Detonate(&a[0]))

MODE_FN(Sonic, CreateSonic, 7, Weap_Create("Sonic", a, sonic_create, out))

/* SetUpSonic(Name): the mine's size, its hit function and its 3 tons */
static byte Sonic_SetUp(const nativeVal_t *name, nativeVal_t *out) {
	cEntity *e;
	const char *ref = Native_Text(name);
	return Weap_SetUpStart(name, &e) && Mode_SetI(e, ref, "TopRadius", 2300) && Mode_SetI(e, ref, "RadiusGrow", 1000) &&
		Mode_SetS(e, ref, "OnHit", "Sonic.OnHit") && Mode_SetI(e, ref, "Mass", 3000) && Mode_EntHandle(e, out);
}

MODE_FN(Sonic, SetUpSonic, 1, Sonic_SetUp(&a[0], out))

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

PORTS(Weapons)
