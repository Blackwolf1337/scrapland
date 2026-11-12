/*
 * Python module SWeap (method table SWeap_methods): weapon and ammo descriptors of the world.
 * Index checks of the tables stop on int3 like the original (TABLE_AT).
 */
#include <string.h>

#include "scrap/python.h"
#include "scrap/scriptapi.h"
#include "scrap/weaps.h"
#include "scrap/world.h"

/* GetNWeap() : Obtiene el numero de armas en el juego */
API_FN(SWeap, GetNWeap) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", g_pWorld->weapons.count);
}

/* GetNAmmo() : Obtiene el numero de municiones en el juego */
API_FN(SWeap, GetNAmmo) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", g_pWorld->ammo.count);
}

/* InitWeap(num) : Inicializa las armas num es el numero de armas. None for num <= 0. */
API_FN(SWeap, InitWeap) {
	int num;
	if (!Mode_Parse(a, n, "i", &num)) {
		return 0;
	}
	if (num <= 0) {
		return 1;
	}
	WeapTable_Init(&g_pWorld->weapons, num);
	return Api_Build(out, "i", 1);
}

/* InitAmmo(num) : Inicializa las municiones del juego. No check of num here, the table checks it. */
API_FN(SWeap, InitAmmo) {
	int num;
	if (!Mode_Parse(a, n, "i", &num)) {
		return 0;
	}
	AmmoTable_Init(&g_pWorld->ammo, num);
	return Api_Build(out, "i", 1);
}

/* GetFAmmo(numammo,svar) : Obtiene un valor numerico de las municiones */
API_FN(SWeap, GetFAmmo) {
	int i;
	const char *var;
	float value;
	if (!Mode_Parse(a, n, "is", &i, &var)) {
		return 0;
	}
	return !cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, i), var, &value) || Api_Build(out, "d", (double)value);
}

/* GetSAmmo(numammo,svar) : Obtiene un valor cadena de las municiones */
API_FN(SWeap, GetSAmmo) {
	int i;
	const char *var;
	char *value;
	if (!Mode_Parse(a, n, "is", &i, &var)) {
		return 0;
	}
	return !cAmmoInfo_GetS(TABLE_AT(g_pWorld->ammo, i), var, &value) || Api_Build(out, "s", value);
}

/* SetFAmmo(numammo,svar,fvalue) : Pone un valor numerico de las municiones */
API_FN(SWeap, SetFAmmo) {
	int i;
	const char *var;
	double value;
	if (!Mode_Parse(a, n, "isd", &i, &var, &value)) {
		return 0;
	}
	return !cAmmoInfo_SetF(TABLE_AT(g_pWorld->ammo, i), var, (float)value) || Api_Build(out, "i", 1);
}

/*
 * SetSAmmo(numammo,svar,svalue) : Pone un valor cadena de las municiones. Unlike the other setters it returns
 * NULL without a Python exception for n >= count (signed compare first) and for an unknown var; Python then
 * reports SystemError "NULL result without error in call_object". Kept as the original does it: tools/test/golden
 * records both.
 */
API_FN(SWeap, SetSAmmo) {
	int i;
	const char *var, *value;
	return Mode_Parse(a, n, "iss", &i, &var, &value) && i < g_pWorld->ammo.count &&
		cAmmoInfo_SetS(TABLE_AT(g_pWorld->ammo, i), var, value) && Api_Build(out, "i", 1);
}

/* GetFWeap(numWeap,svar) : Obtiene un valor numerico de las armas */
API_FN(SWeap, GetFWeap) {
	int i;
	const char *var;
	float value;
	if (!Mode_Parse(a, n, "is", &i, &var)) {
		return 0;
	}
	return !cWeapInfo_GetF(TABLE_AT(g_pWorld->weapons, i), var, &value) || Api_Build(out, "d", (double)value);
}

/* GetSWeap(numWeap,svar) : Obtiene un valor cadena de las armas */
API_FN(SWeap, GetSWeap) {
	int i;
	const char *var;
	char *value;
	if (!Mode_Parse(a, n, "is", &i, &var)) {
		return 0;
	}
	return !cWeapInfo_GetS(TABLE_AT(g_pWorld->weapons, i), var, &value) || Api_Build(out, "s", value);
}

/* SetFWeap(numWeap,svar,fvalue) : Pone un valor numerico de las armas */
API_FN(SWeap, SetFWeap) {
	int i;
	const char *var;
	double value;
	if (!Mode_Parse(a, n, "isd", &i, &var, &value)) {
		return 0;
	}
	return !cWeapInfo_SetF(TABLE_AT(g_pWorld->weapons, i), var, (float)value) || Api_Build(out, "i", 1);
}

/* SetSWeap(numWeap,svar,svalue) : Pone un valor cadena de las armas */
API_FN(SWeap, SetSWeap) {
	int i;
	const char *var, *value;
	if (!Mode_Parse(a, n, "iss", &i, &var, &value)) {
		return 0;
	}
	return !cWeapInfo_SetS(TABLE_AT(g_pWorld->weapons, i), var, value) || Api_Build(out, "i", 1);
}

/* FillPriority() : Inizializa las prioridades de las armas */
API_FN(SWeap, FillPriority) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	cWorld_FillWeapPriority(g_pWorld);
	return 1;
}

/* GetFirstiWeap() : Obtiene el arma primera y por defecto */
API_FN(SWeap, GetFirstiWeap) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", g_pWorld->firstWeapon);
}

/*
 * NetExec(cmd[,name]) : Rutinas de red de los misiles. Unknown commands do nothing. Without the second argument
 * the original passes an uninitialized pointer; here it is NULL.
 */
void Missile_NetExec(const char *cmd, const char *name) {
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	if (!strcmp(cmd, "CreateSwarm")) {
		Swarm_NetCreate(name, zero, zero);
	}
	else if (!strcmp(cmd, "SetUpSwarm")) {
		Swarm_NetSetUp(name);
	}
	else if (!strcmp(cmd, "CreateInferno")) {
		Inferno_NetCreate(name, zero, zero);
	}
	else if (!strcmp(cmd, "SetUpInferno")) {
		Inferno_NetSetUp(name);
	}
}

API_FN(SWeap, NetExec) {
	const char *cmd, *name = NULL;
	if (!Mode_Parse(a, n, "s|s", &cmd, &name)) {
		return 0;
	}
	Missile_NetExec(cmd, name);
	return 1;
}


static char str_PyDoc_SWeap_GetNWeap[] = "GetNWeap() : Obtiene el numero de armas en el juego";


static char str_PyDoc_SWeap_GetNAmmo[] = "GetNAmmo() : Obtiene el numero de municiones en el juego";


static char str_PyDoc_SWeap_InitWeap[] = "InitWeap(num) : Inicializa las armas num es el numero de armas";


static char str_PyDoc_SWeap_InitAmmo[56] = "InitAmmo(num) : Inicializa las municiones del juego";


static char str_PyDoc_SWeap_GetFAmmo[] =
	"GetFAmmo(numammo,svar) : Obtiene un valor numerico de las municiones";


static char str_PyDoc_SWeap_GetSAmmo[72] =
	"GetSAmmo(numammo,svar) : Obtiene un valor cadena de las municiones";


static char str_PyDoc_SWeap_SetFAmmo[80] =
	"GetFAmmo(numammo,svar,fvalue) : Pone un valor numerico de las municiones";


static char str_PyDoc_SWeap_SetSAmmo[] =
	"SetSAmmo(numammo,svar,svalue) : Pone un valor cadena de las municiones";


static char str_PyDoc_SWeap_GetFWeap[] =
	"GetFWeap(numWeap,svar) : Obtiene un valor numerico de las municiones";


static char str_PyDoc_SWeap_GetSWeap[72] =
	"GetSWeap(numWeap,svar) : Obtiene un valor cadena de las municiones";


static char str_PyDoc_SWeap_SetFWeap[80] =
	"GetFWeap(numWeap,svar,fvalue) : Pone un valor numerico de las municiones";

static char str_PyDoc_SWeap_FillPriority[];
static char str_PyDoc_SWeap_GetFirstiWeap[];
static char str_PyDoc_SWeap_NetExec[];
static char str_PyDoc_SWeap_SetSWeap[];
static const portFn_t SWeap_methods[] = {
	PORT_FND(GetNWeap, SWeap_GetNWeap, str_PyDoc_SWeap_GetNWeap),
	PORT_FND(GetNAmmo, SWeap_GetNAmmo, str_PyDoc_SWeap_GetNAmmo),
	PORT_FND(InitWeap, SWeap_InitWeap, str_PyDoc_SWeap_InitWeap),
	PORT_FND(InitAmmo, SWeap_InitAmmo, str_PyDoc_SWeap_InitAmmo),
	PORT_FND(GetFAmmo, SWeap_GetFAmmo, str_PyDoc_SWeap_GetFAmmo),
	PORT_FND(GetSAmmo, SWeap_GetSAmmo, str_PyDoc_SWeap_GetSAmmo),
	PORT_FND(SetFAmmo, SWeap_SetFAmmo, str_PyDoc_SWeap_SetFAmmo),
	PORT_FND(SetSAmmo, SWeap_SetSAmmo, str_PyDoc_SWeap_SetSAmmo),
	PORT_FND(GetFWeap, SWeap_GetFWeap, str_PyDoc_SWeap_GetFWeap),
	PORT_FND(GetSWeap, SWeap_GetSWeap, str_PyDoc_SWeap_GetSWeap),
	PORT_FND(SetFWeap, SWeap_SetFWeap, str_PyDoc_SWeap_SetFWeap),
	PORT_FND(SetSWeap, SWeap_SetSWeap, str_PyDoc_SWeap_SetSWeap),
	PORT_FND(FillPriority, SWeap_FillPriority, str_PyDoc_SWeap_FillPriority),
	PORT_FND(GetFirstiWeap, SWeap_GetFirstiWeap, str_PyDoc_SWeap_GetFirstiWeap),
	PORT_FND(NetExec, SWeap_NetExec, str_PyDoc_SWeap_NetExec),
	PORT_FNEND
};

SD_PY_MODULE(SWeap, 0x430128)

static char str_PyDoc_SWeap_SetSWeap[] =
	"SetSWeap(numammo,svar,svalue) : Pone un valor cadena de las municiones";

static char str_PyDoc_SWeap_FillPriority[] = "FillPriority() : Inizializa las prioridades de las armas";

static char str_PyDoc_SWeap_GetFirstiWeap[] = "GetFirstiWeap() : Obtiene el arma primera y por defecto";

static char str_PyDoc_SWeap_NetExec[] = "NetExec() : Rutinas de red de los misiles";

PORTS(PyInterface_SWeapPy)
