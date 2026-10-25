/*
 * Python module Scrap (method table Scrap_methods, rva 0x42c610): engine functions for the game scripts.
 * Original file: PyInterface\\ScrapPy.cpp
 */
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/python.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"

/* levelpath GetLevelPath() : Obtiene el path del nivel actual */
PyObject *Scrap_GetLevelPath(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", g_pApp->levelPath);
}

/* money Scrap.GetMoney() : Devuelve el liquido disponible. */
PyObject *Scrap_GetMoney(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->money);
}

/* --- batch 2: the small functions of the module (A to L) --- */

static PyObject *ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* Achievement(n) : unlocks an achievement (also on Steam); NULL for n < 0 */
PyObject *Scrap_Achievement(PyObject *self, PyObject *args) {
	int n;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &n) || n < 0) {
		return NULL;
	}
	Achievement_Unlock(n);
	if (g_IsSteam) {
		Steam_SetAchievement(n, 1);
	}
	return ReturnNone();
}

/* AddParamf(value) : float parameter for the function of SetCallFunc */
PyObject *Scrap_AddParamf(PyObject *self, PyObject *args) {
	double v;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &v)) {
		return NULL;
	}
	CallFunc_AddParamF(g_CallFunc, (float)v);
	return ReturnNone();
}

/* AddParami(value) */
PyObject *Scrap_AddParami(PyObject *self, PyObject *args) {
	int v;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &v)) {
		return NULL;
	}
	CallFunc_AddParamI(g_CallFunc, v);
	return ReturnNone();
}

/* AddParams(value) */
PyObject *Scrap_AddParams(PyObject *self, PyObject *args) {
	char *v;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &v)) {
		return NULL;
	}
	CallFunc_AddParamS(g_CallFunc, v);
	return ReturnNone();
}

/* AddScheduledFunc(time,func,params[,name]) : Ejecuta un codigo en python en un instante de tiempo. */
PyObject *Scrap_AddScheduledFunc(PyObject *self, PyObject *args) {
	double time;
	PyObject *func, *params;
	char *name = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "dOO|s", &time, &func, &params, &name)) {
		return NULL;
	}
	Scheduler_Add(g_pWorld->scheduler, (float)time, func, params, name);
	return ReturnNone();
}

/* BuildFont(name, size) */
PyObject *Scrap_BuildFont(PyObject *self, PyObject *args) {
	char *name;
	int size;

	(void)self;
	if (!PyArg_ParseTuple(args, "si", &name, &size)) {
		return NULL;
	}
	Font_Build(name, size);
	return ReturnNone();
}

/* CallElements(name) */
PyObject *Scrap_CallElements(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Elements_Call(g_pWorld->elements, name);
	return ReturnNone();
}

/* ClosePack() : forgets the open pack */
PyObject *Scrap_ClosePack(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cString_Set(g_PackName, "");
	return ReturnNone();
}

/* ConsoleError(text) */
PyObject *Scrap_ConsoleError(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, text, 0);
	return ReturnNone();
}

/* ConsoleOut(text) */
PyObject *Scrap_ConsoleOut(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(0xff0000, text, 0);
	return ReturnNone();
}

/* CreateElements() */
PyObject *Scrap_CreateElements(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Elements_Create(g_pWorld->elements);
	return ReturnNone();
}

/* CreateEntityList(listName) */
PyObject *Scrap_CreateEntityList(PyObject *self, PyObject *args) {
	char *listName;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &listName)) {
		return NULL;
	}
	cWorld *World = g_pWorld;
	CHECK(cWorld_NewEntityList(World, listName));
	return ReturnNone();
}

static PyObject *NoProfiles(const char *name) {
	Con_Print(CON_COLOR_ERROR, "ERROR : ", 0);
	Con_Print(CON_COLOR_ERROR, name, 0);
	Con_Print(CON_COLOR_ERROR, " : Player Profile system not supported on PC\n", 0);
	return Py_BuildValue("i", 1);
}

/* CreateSaveProfile(name) : not on PC */
PyObject *Scrap_CreateSaveProfile(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return NoProfiles(name);
}

/* CreateSaveVar(name, value) */
PyObject *Scrap_CreateSaveVar(PyObject *self, PyObject *args) {
	char *name, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &name, &value)) {
		return NULL;
	}
	return Py_BuildValue("i", SaveVar_Create(name, value) != 0);
}

/* DebugInput(text) */
PyObject *Scrap_DebugInput(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	return Py_BuildValue("s", g_pApp->vftable->DebugInput(g_pApp, text));
}

/* DeleteProfile(name) : not on PC */
PyObject *Scrap_DeleteProfile(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return NoProfiles(name);
}

/* DeleteScheduledFuncs(name) */
PyObject *Scrap_DeleteScheduledFuncs(PyObject *self, PyObject *args) {
	char *name = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Scheduler_Delete(g_pWorld->scheduler, name);
	return ReturnNone();
}

/* DelSaveVars(prefix) */
PyObject *Scrap_DelSaveVars(PyObject *self, PyObject *args) {
	char *prefix;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &prefix)) {
		return NULL;
	}
	SaveVars_Delete(prefix);
	return ReturnNone();
}

/* Des(name) : description of an engine variable; NULL without exception if there is none */
PyObject *Scrap_Des(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	cGVar *v = GVar_Find(name);
	if (!v) {
		Con_Errorf("ERROR: '%s' global variable not defined\n", name);
		return NULL;
	}
	return Py_BuildValue("s", v->description);
}

/* DeselectProfile() : not on PC */
PyObject *Scrap_DeselectProfile(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console, no Profile.\n", 0);
	return ReturnNone();
}

/* EndLoading() */
PyObject *Scrap_EndLoading(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Loading_End();
	return ReturnNone();
}

/* Execute() : Ejecuta una funcion callback (c++). */
PyObject *Scrap_Execute(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	CallFunc_Execute(g_CallFunc, 0);
	return ReturnNone();
}

/* Exit() sale del juego. */
PyObject *Scrap_Exit(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	g_pApp->vftable->Exit(g_pApp);
	return ReturnNone();
}

/* ExtractPack(name) */
PyObject *Scrap_ExtractPack(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("i", Pack_Extract(name));
}

/* GetAlarm() : (on, level, value); level and on are read before the value */
PyObject *Scrap_GetAlarm(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	float level = g_pWorld->alarmLevel;
	byte on = g_pWorld->alarmOn;
	int value = Alarm_GetValue(g_pWorld->alarm);
	return Py_BuildValue("ifi", on != 0, (double)level, value);
}

/* GetAlarmChars() */
PyObject *Scrap_GetAlarmChars(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("ss", g_pWorld->alarmChars, g_pWorld->alarmChars2);
}

/* GetCam(i) : name of the camera */
PyObject *Scrap_GetCam(PyObject *self, PyObject *args) {
	int i;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &i)) {
		return NULL;
	}
	return Py_BuildValue("s", cWorld_GetCamName(g_pWorld, i));
}

/* GetFirst() : Retorna la primera entidad de la lista o none */
PyObject *Scrap_GetFirst(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!cWorld_GetFirstEntity(g_pWorld)) {
		return ReturnNone();
	}
	return PyEntity_New(cWorld_GetFirstEntity(g_pWorld)->name);
}

/* GetFloor(y) */
PyObject *Scrap_GetFloor(PyObject *self, PyObject *args) {
	float y;

	(void)self;
	if (!PyArg_ParseTuple(args, "f", &y)) {
		return NULL;
	}
	return Py_BuildValue("i", Floor_Get(g_pWorld->floor, y));
}

/* GetFreeBlocks() : no Xbox, always 50000 */
PyObject *Scrap_GetFreeBlocks(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console.\n", 0);
	return Py_BuildValue("i", 50000);
}

/* GetGogID() */
PyObject *Scrap_GetGogID(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", Gog_GetID());
}

/* GetLangEntries(name[, n]) : n default 3 */
PyObject *Scrap_GetLangEntries(PyObject *self, PyObject *args) {
	char *name;
	int n = 3;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &n)) {
		return NULL;
	}
	return Lang_GetEntries(name, n);
}

/* GetLangStr(name) : "" and a warning when the string is missing */
PyObject *Scrap_GetLangStr(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	const char *s = Lang_GetStr(name);
	if (s) {
		return Py_BuildValue("s", s);
	}
	Con_Warnf("STRING NOT FOUND: '%s'\n", name);
	return Py_BuildValue("s", "");
}

/* GetLanguage() */
PyObject *Scrap_GetLanguage(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	const char *lang = Lang_GetLanguage();
	if (!lang) {
		return ReturnNone();
	}
	return Py_BuildValue("s", lang);
}

/* GetLockAlarm() */
PyObject *Scrap_GetLockAlarm(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Alarm_IsLocked(g_pWorld->alarm) != 0);
}

/* GetNetFlags() : (bit 0, bit 1, bit 2) of the application flags */
PyObject *Scrap_GetNetFlags(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	uint32_t f = g_pApp->flags;
	return Py_BuildValue("iii", (int)(f & 1), (int)(f >> 1 & 1), (int)(f >> 2 & 1));
}

/* GetNewLevelPath() */
PyObject *Scrap_GetNewLevelPath(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", g_pApp->newLevelPath);
}

/* GetSavedProfilesList() : not on PC, empty list */
PyObject *Scrap_GetSavedProfilesList(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : GetSavedProfilesList not supported on PC\n", 0);
	return PyList_New(0);
}

/* GetSaveGamesList(path) */
PyObject *Scrap_GetSaveGamesList(PyObject *self, PyObject *args) {
	char *path;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}
	return SaveGames_GetList(path);
}

/* GetSaveVar(name) */
PyObject *Scrap_GetSaveVar(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("s", SaveVar_Get(name));
}

/* GetSteamID() : None without Steam */
PyObject *Scrap_GetSteamID(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!g_IsSteam) {
		return ReturnNone();
	}
	return Py_BuildValue("s", Steam_GetID());
}

/* GetSteamTicket() : None without Steam */
PyObject *Scrap_GetSteamTicket(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!g_IsSteam) {
		return ReturnNone();
	}
	return Py_BuildValue("s", Steam_GetTicket());
}

/* GetTime() : Obtiene el tiempo del mundo en segundos */
PyObject *Scrap_GetTime(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("d", (double)Timer_GetTime(g_pWorld->timer));
}

/* GetTimeSpeed() */
PyObject *Scrap_GetTimeSpeed(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("d", (double)g_pWorld->timeSpeed);
}

/* GetVideoCurrentMode() : Get Video Current Mode Index. No argument check. */
PyObject *Scrap_GetVideoCurrentMode(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_BuildValue("i", Video_GetCurrentMode());
}

/* IncSaveVar(name[, n]) : n default 1 */
PyObject *Scrap_IncSaveVar(PyObject *self, PyObject *args) {
	char *name;
	int n = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &n)) {
		return NULL;
	}
	return Py_BuildValue("i", SaveVar_Inc(name, n));
}

/* InitLoading() */
PyObject *Scrap_InitLoading(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Loading_Init();
	return ReturnNone();
}

/* IsSteam() : 1 with Steam, else None */
PyObject *Scrap_IsSteam(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!g_IsSteam) {
		return ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/* LaunchDashboard(n) : no Xbox, 0 */
PyObject *Scrap_LaunchDashboard(PyObject *self, PyObject *args) {
	int n;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &n)) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console, no Dashoboard.\n", 0);
	return Py_BuildValue("i", 0);
}

/* ListModels() */
PyObject *Scrap_ListModels(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Models_List();
	return ReturnNone();
}
