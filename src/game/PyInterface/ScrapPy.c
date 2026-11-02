/*
 * Python module Scrap (method table Scrap_methods, rva 0x42c610): engine functions for the game scripts.
 * Original file: PyInterface\\ScrapPy.cpp
 */
#include <math.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/python.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"

/* levelpath GetLevelPath() : Obtiene el path del level actual */
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

/* Achievement(n) : NULL for n < 0, else nothing; differs from the original on purpose (no store features): no GOG or
 * Steam achievement is unlocked */
PyObject *Scrap_Achievement(PyObject *self, PyObject *args) {
	int n;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &n) || n < 0) {
		return NULL;
	}
	return Py_ReturnNone();
}

cPy_Function g_CallFunc;

/* AddParamf(value) : float parameter for the function of SetCallFunc */
PyObject *Scrap_AddParamf(PyObject *self, PyObject *args) {
	double v;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &v)) {
		return NULL;
	}
	cPy_Function_AddParamF((&g_CallFunc), (float)v);
	return Py_ReturnNone();
}

/* AddParami(value) */
PyObject *Scrap_AddParami(PyObject *self, PyObject *args) {
	int v;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &v)) {
		return NULL;
	}
	cPy_Function_AddParamI((&g_CallFunc), v);
	return Py_ReturnNone();
}

/* AddParams(value) */
PyObject *Scrap_AddParams(PyObject *self, PyObject *args) {
	char *v;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &v)) {
		return NULL;
	}
	cPy_Function_AddParamS((&g_CallFunc), v);
	return Py_ReturnNone();
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
	Scheduler_Add(&g_pWorld->scheduler, (float)time, func, params, name);
	return Py_ReturnNone();
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
	return Py_ReturnNone();
}

/* CallElements(name) */
PyObject *Scrap_CallElements(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Elements_Call(&g_pWorld->elements, name);
	return Py_ReturnNone();
}

/* ClosePack() : forgets the open pack */
PyObject *Scrap_ClosePack(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cString_Set(g_PackName, "");
	return Py_ReturnNone();
}

/* ConsoleError(text) */
PyObject *Scrap_ConsoleError(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, text, 0);
	return Py_ReturnNone();
}

/* ConsoleOut(text) */
PyObject *Scrap_ConsoleOut(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(0xff0000, text, 0);
	return Py_ReturnNone();
}

/* CreateElements() */
PyObject *Scrap_CreateElements(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Elements_Create(&g_pWorld->elements);
	return Py_ReturnNone();
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
	return Py_ReturnNone();
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
	Scheduler_Delete(&g_pWorld->scheduler, name);
	return Py_ReturnNone();
}

/* DelSaveVars(prefix) */
PyObject *Scrap_DelSaveVars(PyObject *self, PyObject *args) {
	char *prefix;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &prefix)) {
		return NULL;
	}
	SaveVars_Delete(prefix);
	return Py_ReturnNone();
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
	return Py_BuildValue("s", v->base.description);
}

/* DeselectProfile() : not on PC */
PyObject *Scrap_DeselectProfile(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console, no Profile.\n", 0);
	return Py_ReturnNone();
}

/* EndLoading() */
PyObject *Scrap_EndLoading(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Loading_End();
	return Py_ReturnNone();
}

/* Execute() : Ejecuta una funcion callback (c++). */
PyObject *Scrap_Execute(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cPy_Function_Execute((&g_CallFunc), 0);
	return Py_ReturnNone();
}

/* Exit() sale del juego. */
PyObject *Scrap_Exit(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	g_pApp->vftable->Exit(g_pApp);
	return Py_ReturnNone();
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
	float level = g_pWorld->alarm.value;
	byte on = g_pWorld->alarm.on;
	int value = Alarm_GetValue(&g_pWorld->alarm);
	return Py_BuildValue("ifi", on != 0, (double)level, value);
}

/* GetAlarmChars() */
PyObject *Scrap_GetAlarmChars(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("ss", g_pWorld->alarm.chars, g_pWorld->alarm.lastChars);
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

/* GetFirst() : Retorna la primera entidad de la list o none */
PyObject *Scrap_GetFirst(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!cWorld_GetFirstEntity(g_pWorld)) {
		return Py_ReturnNone();
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
	return Py_BuildValue("i", Floor_Get(&g_pWorld->floor, y));
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

/* GetGogID() : None; differs from the original on purpose (no store features), which gave the GalaxyID */
PyObject *Scrap_GetGogID(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
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
		return Py_ReturnNone();
	}
	return Py_BuildValue("s", lang);
}

/* GetLockAlarm() */
PyObject *Scrap_GetLockAlarm(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Alarm_IsLocked(&g_pWorld->alarm) != 0);
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

/* GetSteamID() : None, as the original without Steam (no store features) */
PyObject *Scrap_GetSteamID(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* GetSteamTicket() : None, as the original without Steam (no store features) */
PyObject *Scrap_GetSteamTicket(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* GetTime() : Obtiene el tiempo del world en segundos */
PyObject *Scrap_GetTime(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("d", (double)Timer_GetTime(&g_pWorld->timer));
}

/* GetTimeSpeed() */
PyObject *Scrap_GetTimeSpeed(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("d", (double)g_pWorld->timer.speed);
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
	return Py_ReturnNone();
}

/* IsSteam() : None, as the original without Steam (no store features) */
PyObject *Scrap_IsSteam(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
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
	return Py_ReturnNone();
}

/* --- batch 3: the small functions of the module (L to Z) --- */

/* LoadGameVars(name) */
PyObject *Scrap_LoadGameVars(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("i", SaveGame_Load(name) != 0);
}

/* LoadLevel(level) */
PyObject *Scrap_LoadLevel(PyObject *self, PyObject *args) {
	char *level;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &level)) {
		return NULL;
	}
	CWinApp_LoadLevel(g_pApp, level);
	return Py_BuildValue("i", 1);
}

/* ModelInfo(name[, arg]) : arg default 1 */
PyObject *Scrap_ModelInfo(PyObject *self, PyObject *args) {
	char *name;
	int arg = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &arg)) {
		return NULL;
	}
	Model_Info(name, arg);
	return Py_ReturnNone();
}

/* MusicPlayer(name[, cmd[, volume]]) : cmd default "play", volume default 0 */
PyObject *Scrap_MusicPlayer(PyObject *self, PyObject *args) {
	char *name, *cmd = "play";
	double volume = 0.0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|sd", &name, &cmd, &volume)) {
		return NULL;
	}
	Music_Play(name, cmd, (float)volume);
	return Py_ReturnNone();
}

/* OpenPack(path) : the pack name becomes "path/" */
PyObject *Scrap_OpenPack(PyObject *self, PyObject *args) {
	char *path;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}
	cString_Printf(g_PackName, "%s/", path);
	return Py_ReturnNone();
}

/* OpenStore(appid) : NULL for appid < 0, else nothing, as the original without Steam (no store features; with Steam it
 * opened the store page in the overlay) */
PyObject *Scrap_OpenStore(PyObject *self, PyObject *args) {
	int appId;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &appId) || appId < 0) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* PreloadAnm(a, b) */
PyObject *Scrap_PreloadAnm(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	Anm_Preload(a, b);
	return Py_ReturnNone();
}

/* PreloadLibrary(a, b) */
PyObject *Scrap_PreloadLibrary(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	Pack_PreloadLibrary(a, b);
	return Py_ReturnNone();
}

/* Print(text) */
PyObject *Scrap_Print(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(0, text, 0);
	return Py_ReturnNone();
}

/* ProcessDVF(name, arg) */
PyObject *Scrap_ProcessDVF(PyObject *self, PyObject *args) {
	char *name;
	int arg;

	(void)self;
	if (!PyArg_ParseTuple(args, "si", &name, &arg)) {
		return NULL;
	}
	DVF_Process(name, arg);
	return Py_ReturnNone();
}

/* PythonCompileAll() */
PyObject *Scrap_PythonCompileAll(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Python_CompileAll();
	return Py_ReturnNone();
}

/* Round(v) : the larger of (int)v and (int)(v + 0.5), both truncated */
PyObject *Scrap_Round(PyObject *self, PyObject *args) {
	double v;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &v)) {
		return NULL;
	}
	int a = (int)v, b = (int)(v + 0.5);
	return Py_BuildValue("i", b > a ? b : a);
}

/* SaveConfig() */
PyObject *Scrap_SaveConfig(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Config_Save();
	return Py_BuildValue("i", 1);
}

/* SaveGameVars(name[, title]) : title default "Scrapland SaveGame" */
PyObject *Scrap_SaveGameVars(PyObject *self, PyObject *args) {
	char *name, *title = "Scrapland SaveGame";

	(void)self;
	if (!PyArg_ParseTuple(args, "s|s", &name, &title)) {
		return NULL;
	}
	return Py_BuildValue("i", SaveGame_Save(name, title) != 0);
}

/* ScreenShot(name) */
PyObject *Scrap_ScreenShot(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Screenshot_Take(name);
	return Py_ReturnNone();
}

/* SetAlarm(level) */
PyObject *Scrap_SetAlarm(PyObject *self, PyObject *args) {
	float level;

	(void)self;
	if (!PyArg_ParseTuple(args, "f", &level)) {
		return NULL;
	}
	Alarm_Set(&g_pWorld->alarm, level);
	return Py_ReturnNone();
}

/* SetAlarmChar(name) */
PyObject *Scrap_SetAlarmChar(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Alarm_SetChar(&g_pWorld->alarm, name);
	return Py_ReturnNone();
}

/* SetAlarmGrow(grow) */
PyObject *Scrap_SetAlarmGrow(PyObject *self, PyObject *args) {
	int grow;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &grow)) {
		return NULL;
	}
	Alarm_SetGrow(&g_pWorld->alarm, grow);
	return Py_ReturnNone();
}

/* SetCallFunc(func) : 1 if the function is valid */
PyObject *Scrap_SetCallFunc(PyObject *self, PyObject *args) {
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &func)) {
		return NULL;
	}
	cPy_Function_SetFunc((&g_CallFunc), func);
	return Py_BuildValue("i", cPy_Function_IsValid((&g_CallFunc)) ? 1 : 0);
}

/* SetCam(i, name) */
PyObject *Scrap_SetCam(PyObject *self, PyObject *args) {
	int i;
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &i, &name)) {
		return NULL;
	}
	return Py_BuildValue("i", cWorld_SetCam(g_pWorld, i, name));
}

/* SetLanguage(lang[, flag]) */
PyObject *Scrap_SetLanguage(PyObject *self, PyObject *args) {
	char *lang;
	int flag = 0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &lang, &flag)) {
		return NULL;
	}
	return Py_BuildValue("i", Lang_Set(lang, flag != 0) != 0);
}

/* SetLockAlarm(lock) */
PyObject *Scrap_SetLockAlarm(PyObject *self, PyObject *args) {
	int lock;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &lock)) {
		return NULL;
	}
	Alarm_SetLock(&g_pWorld->alarm, lock != 0);
	return Py_ReturnNone();
}

/* SetMoney(money) */
PyObject *Scrap_SetMoney(PyObject *self, PyObject *args) {
	int money;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &money)) {
		return NULL;
	}
	g_pWorld->money = money;
	return Py_ReturnNone();
}

/* SetSaveVar(name, value) */
PyObject *Scrap_SetSaveVar(PyObject *self, PyObject *args) {
	char *name, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &name, &value)) {
		return NULL;
	}
	return Py_BuildValue("i", SaveVar_Set(name, value) != 0);
}

/* SetTime(t) : returns Py_BuildValue("") (None) like the original */
PyObject *Scrap_SetTime(PyObject *self, PyObject *args) {
	double t;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &t)) {
		return NULL;
	}
	Timer_SetTime(&g_pWorld->timer, (float)t);
	return Py_BuildValue("");
}

/* SetTimeSpeed(speed) */
PyObject *Scrap_SetTimeSpeed(PyObject *self, PyObject *args) {
	double speed;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &speed)) {
		return NULL;
	}
	Timer_SetSpeed(&g_pWorld->timer, (float)speed);
	return Py_BuildValue("");
}

/* SetVideoCurrentMode(mode) */
PyObject *Scrap_SetVideoCurrentMode(PyObject *self, PyObject *args) {
	int mode;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &mode)) {
		return NULL;
	}
	Video_SetCurrentMode(mode);
	return Py_ReturnNone();
}

/* ShowGVars([filter]) */
PyObject *Scrap_ShowGVars(PyObject *self, PyObject *args) {
	char *filter = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "|s", &filter)) {
		return NULL;
	}
	GVars_Show(filter);
	return Py_ReturnNone();
}

/* StartDummySearch(name[, flag]) : flag default 1 */
PyObject *Scrap_StartDummySearch(PyObject *self, PyObject *args) {
	char *name;
	int flag = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &flag)) {
		return NULL;
	}
	DummySearch_Start(name, flag != 0);
	return Py_ReturnNone();
}

/* SwitchMissionArrows(a, b) */
PyObject *Scrap_SwitchMissionArrows(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	MissionArrows_Switch(a, b);
	return Py_ReturnNone();
}

/* UsrEntity(player) : the entity of a local player; NULL without exception for a player above 3 */
PyObject *Scrap_UsrEntity(PyObject *self, PyObject *args) {
	uint32_t player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player) || player > 3) {
		return NULL;
	}
	cEntity *e = cWorld_GetUsrEntity(g_pWorld, (int)player);
	if (!e) {
		return Py_ReturnNone();
	}
	return PyEntity_New(e->name);
}

/* Ver() Obtiene la version del juego. */
PyObject *Scrap_Ver(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", Version_Get());
}

/* Verbose(text) : debug color */
PyObject *Scrap_Verbose(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(CON_COLOR_DEBUG, text, 0);
	return Py_ReturnNone();
}

/* the callback of the video of Scrap.VideoPlayer, until it ends */
static PyObject *videoPlayerDone;

static void VideoPlayer_Done(int result) {
	PyObject *f = videoPlayerDone;
	videoPlayerDone = NULL;
	PyObject *args = Py_BuildValue("(i)", result);
	PyObject *r = args ? PyEval_CallObject(f, args) : NULL;
	if (!r) {
		PyErr_Print();
	}
	Py_XDECREF(r);
	Py_XDECREF(args);
	Py_DECREF(f);
}

/*
 * VideoPlayer(name[, cmd[, done]]) : cmd default "play". differs from the original on purpose (no blocking waits):
 * it returns at once, the video plays on and the frame waits for it; done(result) is called when it ends (an
 * addition: 0 not found, 1 played to the end or not opened, 2 stopped by a key)
 */
PyObject *Scrap_VideoPlayer(PyObject *self, PyObject *args) {
	char *name, *cmd = "play";
	PyObject *done = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|sO", &name, &cmd, &done)) {
		return NULL;
	}
	if (done && done != Py_None && !PyCallable_Check(done)) {
		Con_Warnf("VideoPlayer: done is not callable, ignored\n");
		done = NULL;
	}
	if (!done || done == Py_None || crt_stricmp(cmd, "play")) {
		Video_Play(name, cmd, 0.0f);
		return Py_ReturnNone();
	}
	Py_INCREF(done);
	videoPlayerDone = done;
	Video_PlayThen(name, VideoPlayer_Done);
	return Py_ReturnNone();
}

/* --- batch 4: engine variables, entities, files, random numbers, Steam apps --- */

/* Get(name) / Def(name) : current or default value of an engine variable in its type */
static PyObject *GVarValue(PyObject *args, int def, int checkLine) {
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	cGVar *v = GVar_Find(name);
	if (!v) {
		Con_Errorf("ERROR: '%s' global variable not defined\n", name);
		return NULL;
	}
	switch (v->base.type) {
	case GVAR_STRING:
		return Py_BuildValue("s", def ? v->def.s : v->value.s);
	case GVAR_INT:
		return Py_BuildValue("i", def ? v->def.i : v->value.i);
	case GVAR_FLOAT:
		return Py_BuildValue("d", (double)(def ? v->def.f : v->value.f));
	case GVAR_FUNCTION:
		return Py_BuildValue("s", "<function>");
	case GVAR_SCRIPT_FUNCTION:
		return Py_BuildValue("s", "<Script function>");
	}
	/* CHECK(0) at line checkLine of the original */
	(void)checkLine;
	CHECK(0);
	return NULL;
}

PyObject *Scrap_Get(PyObject *self, PyObject *args) {
	(void)self;
	return GVarValue(args, 0, 0x11d);
}

PyObject *Scrap_Def(PyObject *self, PyObject *args) {
	(void)self;
	return GVarValue(args, 1, 0x13f);
}

/*
 * Set(name, value) : string ("ss") for string and script function variables, number ("sd") for int and float
 * ones. A read only variable gives 0 and a message; a wrong type or an unknown name NULL without exception.
 */
PyObject *Scrap_Set(PyObject *self, PyObject *args) {
	char *name, *s;
	double d;
	cGVar *v;

	(void)self;
	if (PyArg_ParseTuple(args, "ss", &name, &s)) {
		if (!(v = GVar_Find(name))) {
			goto notdefined;
		}
		if (v->base.type == GVAR_STRING) {
			if (v->base.flags & 1) {
				goto readonly;
			}
			cString_Set(&v->value.s, s);
			return Py_BuildValue("i", 1);
		}
		if (v->base.type != GVAR_SCRIPT_FUNCTION) {
			Con_Errorf("ERROR: '%s' global variable must be string\n", name);
			return NULL;
		}
		if (v->base.flags & 1) {
			goto readonly;
		}
		cPy_Function_SetFunc(GVar_Function(v), s);
		return Py_BuildValue("i", 1);
	}
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "sd", &name, &d)) {
		return NULL;
	}
	if (!(v = GVar_Find(name))) {
		goto notdefined;
	}
	if (v->base.type == GVAR_INT) {
		if (v->base.flags & 1) {
			goto readonly;
		}
		v->value.i = (int)d;
		return Py_BuildValue("i", 1);
	}
	if (v->base.type != GVAR_FLOAT) {
		Con_Errorf("ERROR: '%s' global variable must be integer or float\n", name);
		return NULL;
	}
	if (v->base.flags & 1) {
		goto readonly;
	}
	v->value.f = (float)d;
	return Py_BuildValue("i", 1);

readonly:
	Con_Errorf("ERROR: '%s' write only\n", name);
	return Py_BuildValue("i", 0);
notdefined:
	Con_Errorf("ERROR: '%s' global variable not defined\n", name);
	return NULL;
}

/* Rand(a, b) : random number between a and b (CRT rand); a result equal to a bound becomes the middle */
PyObject *Scrap_Rand(PyObject *self, PyObject *args) {
	double da, db;

	(void)self;
	if (!PyArg_ParseTuple(args, "dd", &da, &db)) {
		return NULL;
	}
	float a = (float)da, b = (float)db;
	int r = crt_rand();
	double v = (double)(float)((double)(r & 0x7fff) * (1.0 / 32767.0) * (double)(b - a) + (double)a);
	if (v == db || v == da) {
		v = (da + db) * 0.5;
	}
	return Py_BuildValue("d", v);
}

/* FileExist(name) : looks in the packs and on disk */
PyObject *Scrap_FileExist(PyObject *self, PyObject *args) {
	char *name;
	cFilePack pack;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	cFilePack_ctor(&pack);
	PyObject *r = Py_BuildValue("i", cFilePack_Exists(&pack.file.base, name));
	cFilePack_dtor(&pack);
	return r;
}

/* GetEntity(name) : the entity handle, None if there is no such entity */
PyObject *Scrap_GetEntity(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (World_FindEntity(name)) {
		return PyEntity_New(name);
	}
	return Py_ReturnNone();
}

/* CreateEntity(className, x, y, z, name) : the handle of the new entity, None if it was not made */
PyObject *Scrap_CreateEntity(PyObject *self, PyObject *args) {
	char *className, *name;
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "sddds", &className, &x, &y, &z, &name)) {
		return NULL;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, className, (float)x, (float)y, (float)z, name);
	if (!e) {
		return Py_ReturnNone();
	}
	return PyEntity_New(e->name);
}

/* IsApp(appid) : NULL for appid < 0, else 1, as the original without Steam (no store features; with Steam it asked
 * whether the app is owned and installed) */
PyObject *Scrap_IsApp(PyObject *self, PyObject *args) {
	int appId;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &appId) || appId < 0) {
		return NULL;
	}
	return Py_BuildValue("i", 1);
}

/* RunApp(exe, cmdLine) : starts a program; "" for the game itself */
PyObject *Scrap_RunApp(PyObject *self, PyObject *args) {
	char *exe, *cmdLine;
	char path[128];

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &exe, &cmdLine)) {
		return NULL;
	}
	if (!*exe) {
		App_GetPath(path, sizeof(path));
		exe = path;
	}
	g_pApp->vftable->RunApp(g_pApp, exe, cmdLine, 0);
	return Py_ReturnNone();
}

/* --- batch 5: entity lists, cameras, parking zones, video modes, grid --- */

/* EntityListGet(listName, var) : value in its type; NULL without exception for a missing list or a bad type */
PyObject *Scrap_EntityListGet(PyObject *self, PyObject *args) {
	char *listName, *var, *s;
	double d;
	int i;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &listName, &var)) {
		return NULL;
	}
	void *list = cWorld_GetList(g_pWorld, listName);
	if (!list) {
		Con_Errorf("ERROR: EntityListGet '%s','%s' List Not Found.\n", listName, var);
		return NULL;
	}
	switch (cEntityList_Var(list, var, 0, &s, &d, &i)) {
	case 1:
		return Py_BuildValue("s", s);
	case 2:
		return Py_BuildValue("d", d);
	case 3:
		return Py_BuildValue("i", i);
	}
	Con_Errorf("ERROR: EntityListGet '%s','%s' Typecast error.\n", listName, var);
	return NULL;
}

/* EntityListSet(listName, var, value) */
PyObject *Scrap_EntityListSet(PyObject *self, PyObject *args) {
	char *listName, *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "sss", &listName, &var, &value)) {
		return NULL;
	}
	void *list = cWorld_GetList(g_pWorld, listName);
	if (!list) {
		Con_Errorf("ERROR: EntityListSet '%s','%s','%s' list not found.\n", listName, var, value);
		return NULL;
	}
	return Py_BuildValue("i", cEntityList_Var(list, var, 1, &value, NULL, NULL));
}

/* GetMinCamDist(x, y, z) : sqrt in double of the squared distance, then float */
PyObject *Scrap_GetMinCamDist(PyObject *self, PyObject *args) {
	vec3_t pos;

	(void)self;
	if (!PyArg_ParseTuple(args, "fff", &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	double d = (double)cWorld_GetMinCamDistSq(g_pWorld, &pos);
	return Py_BuildValue("d", (double)(float)sqrt(d));
}

/* GetVideoModes(i) : the number of modes for i < 0, else the name of mode i */
PyObject *Scrap_GetVideoModes(PyObject *self, PyObject *args) {
	int i = 0;
	char buf[256];

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &i)) {
		return NULL;
	}
	int n = Video_GetModes(i, buf);
	if (i < 0) {
		return Py_BuildValue("i", n);
	}
	return Py_BuildValue("s", buf);
}

/* SetGrid(x, y, z) */
PyObject *Scrap_SetGrid(PyObject *self, PyObject *args) {
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "ddd", &x, &y, &z)) {
		return NULL;
	}
	vec3_t size = { (float)x, (float)y, (float)z };
	Grid_Set(&g_pWorld->grid, &size);
	return Py_BuildValue("i", 1);
}

/* SetSaveProfile(a, b) : not on PC */
PyObject *Scrap_SetSaveProfile(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : ", 0);
	Con_Print(CON_COLOR_ERROR, a, 0);
	Con_Print(CON_COLOR_ERROR, " , ", 0);
	Con_Print(CON_COLOR_ERROR, b, 0);
	Con_Print(CON_COLOR_ERROR, " : Player Profile system not supported on PC\n", 0);
	return Py_BuildValue("i", 1);
}

/* Preload3DObject(name[, sx, sy, sz]) : scale default 1 */
PyObject *Scrap_Preload3DObject(PyObject *self, PyObject *args) {
	char *name;
	double sx = 1.0, sy = 1.0, sz = 1.0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|ddd", &name, &sx, &sy, &sz)) {
		return NULL;
	}
	vec3_t scale = { (float)sx, (float)sy, (float)sz };
	Object3D_Preload(name, &scale);
	return Py_ReturnNone();
}

/* AddParkingZone(flag, (x,y,z), name) */
PyObject *Scrap_AddParkingZone(PyObject *self, PyObject *args) {
	int flag;
	float x, y, z;
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "i(fff)s", &flag, &x, &y, &z, &name)) {
		return NULL;
	}
	vec3_t pos = { x, y, z };
	Parking_AddZone(&g_pWorld->logic.parking, flag != 0, &pos, name);
	return Py_ReturnNone();
}

/* GetNearestParked / GetFarestParked([flag[, (x,y,z)]]) : (name, (x, y, z)); default position: current camera */
static PyObject *FindParked(PyObject *args, int farest) {
	vec3_t pos = g_pWorld->camPos[g_pWorld->curCam];
	int flag = 1;

	if (!PyArg_ParseTuple(args, "|i(fff)", &flag, &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	vec3_t out = pos;
	const char *name = Parking_Find(&g_pWorld->logic.parking, flag != 0, &pos, &out, farest);
	if (!name) {
		return Py_BuildValue("s(fff)", "", 0.0, 0.0, 0.0);
	}
	return Py_BuildValue("s(fff)", name, (double)out.x, (double)out.y, (double)out.z);
}

PyObject *Scrap_GetNearestParked(PyObject *self, PyObject *args) {
	(void)self;
	return FindParked(args, 0);
}

PyObject *Scrap_GetFarestParked(PyObject *self, PyObject *args) {
	(void)self;
	return FindParked(args, 1);
}

/* --- batch 6: collision tests, debris, items, dummy search --- */

/*
 * TestLine((x1,y1,z1), (x2,y2,z2), listName) : ((x, y, z) hit, name) against the level and the entities of the
 * list; "" for a level hit only, None for no hit.
 */
PyObject *Scrap_TestLine(PyObject *self, PyObject *args) {
	double x1, y1, z1, x2, y2, z2;
	char *listName;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)(ddd)s", &x1, &y1, &z1, &x2, &y2, &z2, &listName)) {
		return NULL;
	}
	cEntity *target = NULL;
	vec3_t from = { (float)x1, (float)y1, (float)z1 };
	vec3_t to = { (float)x2, (float)y2, (float)z2 };
	vec3_t hit = to;
	byte levelHit = Engine_TestLine(&from, &to, &hit, 0);
	to = hit;
	cWorld_TestLineEntities(g_pWorld, 0, EntityLists_Get(listName), &from, &to, &hit, &target, 1);
	if (!target && !levelHit) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("(ddd)s", (double)hit.x, (double)hit.y, (double)hit.z, target ? target->name : "");
}

/* TestSphere(x, y, z, radius, listName[, exclude]) : name of the entity hit, "" for the level, None for none */
PyObject *Scrap_TestSphere(PyObject *self, PyObject *args) {
	double x, y, z, r;
	char *listName, *excludeName = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "dddds|s", &x, &y, &z, &r, &listName, &excludeName)) {
		return NULL;
	}
	cEntity *target = NULL;
	vec3_t c = { (float)x, (float)y, (float)z };
	byte levelHit = Engine_TestSphere(&c, &c, (float)r, 0, 0);
	cEntity *exclude = NULL;
	if (*excludeName) {
		HashNode *node = HashTable_Find(&g_pWorld->entities, excludeName, HashTable_Hash(&g_pWorld->entities,
			excludeName));
		if (node) {
			exclude = node->value;
		}
	}
	cWorld_TestSphereEntities(g_pWorld, exclude, EntityLists_Get(listName), &c, (float)r, &target, 1, 0);
	if (target) {
		return Py_BuildValue("s", target->name);
	}
	if (levelHit) {
		return Py_BuildValue("s", "");
	}
	return Py_ReturnNone();
}

/* SphereCall(x, y, z, radius, listName, func[, flag]) or ((x,y,z), radius, ...) : func for the entities inside */
PyObject *Scrap_SphereCall(PyObject *self, PyObject *args) {
	double x, y, z, r;
	char *listName, *func;
	int flag = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "ddddss|i", &x, &y, &z, &r, &listName, &func, &flag)) {
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "(ddd)dss|i", &x, &y, &z, &r, &listName, &func, &flag)) {
			return NULL;
		}
	}
	vec3_t c = { (float)x, (float)y, (float)z };
	return Py_BuildValue("i", cWorld_SphereCall(g_pWorld, EntityLists_Get(listName), &c, (float)r, func,
		flag != 0) != 0);
}

/* NextDummySearch() : (name, (x, y, z), (angx, angy) degrees, flag) of the next dummy, None at the end */
PyObject *Scrap_NextDummySearch(PyObject *self, PyObject *args) {
	char name[0x40];
	vec3_t pos;
	float ang[2];
	byte flag;

	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!DummySearch_Next(name, sizeof(name), &pos, ang, &flag)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("s(ddd)(dd)i", name, (double)pos.x, (double)pos.y, (double)pos.z,
		(double)ang[0] * DEG_PER_PI / M_PI_D, (double)ang[1] * DEG_PER_PI / M_PI_D, flag != 0);
}

/* SetDebrisSys(name[, n, radius, b, c, d, flag, e]) : n <= 0 deletes the system */
PyObject *Scrap_SetDebrisSys(PyObject *self, PyObject *args) {
	char *name;
	int n = 4, flag = 0, e = 0;
	double radius = 10000.0, b = 1.0, c = 1.0, d = (double)g_DebrisDefault;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|iddddii", &name, &n, &radius, &b, &c, &d, &flag, &e)) {
		return NULL;
	}
	if (n <= 0) {
		cWorld_DeleteDebrisSys(g_pWorld, name);
		return Py_ReturnNone();
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, name);
	sys->b = (float)b;
	sys->d = (float)d;
	float rf = (float)radius;
	sys->radiusSq = rf * rf;
	sys->c = (float)c;
	sys->flag = flag != 0;
	cDebrisSys_Init(sys, n, e);
	return Py_ReturnNone();
}

/* DropDebris(name, speed) : a debris element with the values of SetDebrisValue */
PyObject *Scrap_DropDebris(PyObject *self, PyObject *args) {
	char *name;
	double speed;

	(void)self;
	if (!PyArg_ParseTuple(args, "sd", &name, &speed)) {
		return NULL;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, name);
	cDebris *deb = cDebrisSys_NewElement(sys);
	for (int i = 0; i < 4; i++) {
		deb->drop[i] = g_DebrisDrop[i];
	}
	deb->life = (float)speed;
	cDebrisSys_Drop(sys, deb);
	return Py_ReturnNone();
}

/* SetDebrisValue(n, x, y, z) : 0 position, 1 angles (degrees), 2 velocity, 3 angle speed (degrees) */
PyObject *Scrap_SetDebrisValue(PyObject *self, PyObject *args) {
	int n;
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "iddd", &n, &x, &y, &z)) {
		return NULL;
	}
	switch (n) {
	case 0:
	case 2:
		g_DebrisDrop[n] = (vec3_t){ (float)x, (float)y, (float)z };
		break;
	case 1:
	case 3:
		g_DebrisDrop[n] = (vec3_t){ (float)(x * M_PI_D / DEG_PER_PI), (float)(y * M_PI_D / DEG_PER_PI),
			(float)(z * M_PI_D / DEG_PER_PI) };
		break;
	default:
		Con_Warnf("WARNING SetDebrisValue: %d not found\n", n);
		break;
	}
	return Py_ReturnNone();
}

/*
 * AddItem("Life", name, value) / AddItem("Ammo", name, kind, value) : kind "Part", "Miss" or "Ener" (0, 1, 2).
 * As compiled: another kind passes the string pointer of the first argument as kind, and another item type does
 * nothing (no argument check at all).
 */
PyObject *Scrap_AddItem(PyObject *self, PyObject *args) {
	char *type = PyString_AsString(PyTuple_GetItem(args, 0));
	char *name, *kindName;
	cItemDesc item;

	(void)self;
	if (!strcmp(type, "Life")) {
		cString_ctor(&item.name);
		if (!PyArg_ParseTuple(args, "ssi", &type, &name, &item.value)) {
			cString_dtor(&item.name);
			return NULL;
		}
		cString_Set(&item.name, name);
		g_pWorld->logic.items.vftable->AddLife(&g_pWorld->logic.items, &item);
		cString_dtor(&item.name);
	}
	else if (!strcmp(type, "Ammo")) {
		cString_ctor(&item.name);
		if (!PyArg_ParseTuple(args, "sssi", &type, &name, &kindName, &item.value)) {
			cString_dtor(&item.name);
			return NULL;
		}
		cString_Set(&item.name, name);
		int kind = (int)(intptr_t)type;
		if (!strcmp(kindName, "Part")) {
			kind = 0;
		}
		else if (!strcmp(kindName, "Miss")) {
			kind = 1;
		}
		else if (!strcmp(kindName, "Ener")) {
			kind = 2;
		}
		g_pWorld->logic.items.vftable->AddAmmo(&g_pWorld->logic.items, kind, &item);
		cString_dtor(&item.name);
	}
	return Py_ReturnNone();
}

extern char str_PyDoc_Scrap_Achievement[];
extern char str_PyDoc_Scrap_AddItem[];
extern char str_PyDoc_Scrap_AddParamf[80];
extern char str_PyDoc_Scrap_AddParami[80];
extern char str_PyDoc_Scrap_AddParams[];
extern char str_PyDoc_Scrap_AddParkingZone[];
extern char str_PyDoc_Scrap_AddScheduledFunc[];
extern char str_PyDoc_Scrap_BuildFont[];
extern char str_PyDoc_Scrap_CallElements[];
extern char str_PyDoc_Scrap_ClosePack[44];
extern char str_PyDoc_Scrap_ConsoleError[];
extern char str_PyDoc_Scrap_ConsoleOut[72];
extern char str_PyDoc_Scrap_CreateElements[88];
extern char str_PyDoc_Scrap_CreateEntity[];
extern char str_PyDoc_Scrap_CreateEntityList[];
extern char str_PyDoc_Scrap_CreateSaveProfile[];
extern char str_PyDoc_Scrap_CreateSaveVar[];
extern char str_PyDoc_Scrap_DebugInput[68];
extern char str_PyDoc_Scrap_Def[];
extern char str_PyDoc_Scrap_DelSaveVars[];
extern char str_PyDoc_Scrap_DeleteProfile[64];
extern char str_PyDoc_Scrap_DeleteScheduledFuncs[];
extern char str_PyDoc_Scrap_Des[72];
extern char str_PyDoc_Scrap_DeselectProfile[];
extern char str_PyDoc_Scrap_DropDebris[];
extern char str_PyDoc_Scrap_EndLoading[];
extern char str_PyDoc_Scrap_EntityListGet[];
extern char str_PyDoc_Scrap_EntityListSet[];
extern char str_PyDoc_Scrap_Execute[];
extern char str_PyDoc_Scrap_Exit[28];
extern char str_PyDoc_Scrap_ExtractPack[64];
extern char str_PyDoc_Scrap_FileExist[];
extern char str_PyDoc_Scrap_Get[];
extern char str_PyDoc_Scrap_GetAlarm[];
extern char str_PyDoc_Scrap_GetAlarmChars[];
extern char str_PyDoc_Scrap_GetCam[];
extern char str_PyDoc_Scrap_GetEntity[];
extern char str_PyDoc_Scrap_GetFarestParked[];
extern char str_PyDoc_Scrap_GetFirst[68];
extern char str_PyDoc_Scrap_GetFloor[];
extern char str_PyDoc_Scrap_GetFreeBlocks[];
extern char str_PyDoc_Scrap_GetGogID[];
extern char str_PyDoc_Scrap_GetLangEntries[];
extern char str_PyDoc_Scrap_GetLangStr[];
extern char str_PyDoc_Scrap_GetLanguage[];
extern char str_PyDoc_Scrap_GetLevelPath[];
extern char str_PyDoc_Scrap_GetLockAlarm[];
extern char str_PyDoc_Scrap_GetMinCamDist[];
extern char str_PyDoc_Scrap_GetMoney[];
extern char str_PyDoc_Scrap_GetNearestParked[104];
extern char str_PyDoc_Scrap_GetNetFlags[];
extern char str_PyDoc_Scrap_GetNewLevelPath[];
extern char str_PyDoc_Scrap_GetSaveGamesList[296];
extern char str_PyDoc_Scrap_GetSaveVar[72];
extern char str_PyDoc_Scrap_GetSavedProfilesList[];
extern char str_PyDoc_Scrap_GetSteamID[];
extern char str_PyDoc_Scrap_GetSteamTicket[];
extern char str_PyDoc_Scrap_GetTime[];
extern char str_PyDoc_Scrap_GetTimeSpeed[];
extern char str_PyDoc_Scrap_GetVideoCurrentMode[];
extern char str_PyDoc_Scrap_GetVideoModes[];
extern char str_PyDoc_Scrap_IncSaveVar[];
extern char str_PyDoc_Scrap_InitLoading[56];
extern char str_PyDoc_Scrap_IsApp[];
extern char str_PyDoc_Scrap_IsSteam[];
extern char str_PyDoc_Scrap_LaunchDashboard[];
extern char str_PyDoc_Scrap_ListModels[];
extern char str_PyDoc_Scrap_LoadGameVars[60];
extern char str_PyDoc_Scrap_LoadLevel[];
extern char str_PyDoc_Scrap_ModelInfo[];
extern char str_PyDoc_Scrap_MusicPlayer[];
extern char str_PyDoc_Scrap_NextDummySearch[];
extern char str_PyDoc_Scrap_OpenPack[];
extern char str_PyDoc_Scrap_OpenStore[];
extern char str_PyDoc_Scrap_Preload3DObject[];
extern char str_PyDoc_Scrap_PreloadAnm[];
extern char str_PyDoc_Scrap_PreloadLibrary[96];
extern char str_PyDoc_Scrap_Print[];
extern char str_PyDoc_Scrap_ProcessDVF[];
extern char str_PyDoc_Scrap_PythonCompileAll[];
extern char str_PyDoc_Scrap_Rand[];
extern char str_PyDoc_Scrap_Round[];
extern char str_PyDoc_Scrap_RunApp[];
extern char str_PyDoc_Scrap_SaveConfig[64];
extern char str_PyDoc_Scrap_SaveGameVars[88];
extern char str_PyDoc_Scrap_ScreenShot[];
extern char str_PyDoc_Scrap_Set[];
extern char str_PyDoc_Scrap_SetAlarm[];
extern char str_PyDoc_Scrap_SetAlarmChar[];
extern char str_PyDoc_Scrap_SetAlarmGrow[];
extern char str_PyDoc_Scrap_SetCallFunc[];
extern char str_PyDoc_Scrap_SetCam[];
extern char str_PyDoc_Scrap_SetDebrisSys[];
extern char str_PyDoc_Scrap_SetDebrisValue[96];
extern char str_PyDoc_Scrap_SetGrid[];
extern char str_PyDoc_Scrap_SetLanguage[120];
extern char str_PyDoc_Scrap_SetLockAlarm[60];
extern char str_PyDoc_Scrap_SetMoney[60];
extern char str_PyDoc_Scrap_SetSaveProfile[];
extern char str_PyDoc_Scrap_SetSaveVar[];
extern char str_PyDoc_Scrap_SetTime[];
extern char str_PyDoc_Scrap_SetTimeSpeed[];
extern char str_PyDoc_Scrap_SetVideoCurrentMode[];
extern char str_PyDoc_Scrap_ShowGVars[72];
extern char str_PyDoc_Scrap_SphereCall[];
extern char str_PyDoc_Scrap_StartDummySearch[];
extern char str_PyDoc_Scrap_SwitchMissionArrows[];
extern char str_PyDoc_Scrap_TestLine[];
extern char str_PyDoc_Scrap_TestSphere[128];
extern char str_PyDoc_Scrap_UsrEntity[];
extern char str_PyDoc_Scrap_Ver[40];
extern char str_PyDoc_Scrap_Verbose[];
extern char str_PyDoc_Scrap_VideoPlayer[];
extern const char str_PyMethod_SScorer_Get[];
extern const char str_PyMethod_SScorer_Set[];
extern const char str_PyMethod_Scrap_Achievement[];
extern const char str_PyMethod_Scrap_AddItem[];
extern const char str_PyMethod_Scrap_AddParamf[];
extern const char str_PyMethod_Scrap_AddParami[];
extern const char str_PyMethod_Scrap_AddParams[];
extern const char str_PyMethod_Scrap_AddParkingZone[];
extern const char str_PyMethod_Scrap_AddScheduledFunc[];
extern const char str_PyMethod_Scrap_BuildFont[];
extern const char str_PyMethod_Scrap_CallElements[];
extern const char str_PyMethod_Scrap_ClosePack[];
extern const char str_PyMethod_Scrap_ConsoleError[];
extern const char str_PyMethod_Scrap_ConsoleOut[];
extern const char str_PyMethod_Scrap_CreateElements[];
extern const char str_PyMethod_Scrap_CreateEntity[];
extern const char str_PyMethod_Scrap_CreateEntityList[];
extern const char str_PyMethod_Scrap_CreateSaveProfile[];
extern const char str_PyMethod_Scrap_CreateSaveVar[];
extern const char str_PyMethod_Scrap_DebugInput[];
extern const char str_PyMethod_Scrap_Def[];
extern const char str_PyMethod_Scrap_DelSaveVars[];
extern const char str_PyMethod_Scrap_DeleteProfile[];
extern const char str_PyMethod_Scrap_DeleteScheduledFuncs[];
extern const char str_PyMethod_Scrap_Des[];
extern const char str_PyMethod_Scrap_DeselectProfile[];
extern const char str_PyMethod_Scrap_DropDebris[];
extern const char str_PyMethod_Scrap_EndLoading[];
extern const char str_PyMethod_Scrap_EntityListGet[];
extern const char str_PyMethod_Scrap_EntityListSet[];
extern const char str_PyMethod_Scrap_Execute[];
extern const char str_PyMethod_Scrap_Exit[];
extern const char str_PyMethod_Scrap_ExtractPack[];
extern const char str_PyMethod_Scrap_FileExist[];
extern const char str_PyMethod_Scrap_GetAlarm[];
extern const char str_PyMethod_Scrap_GetAlarmChars[];
extern const char str_PyMethod_Scrap_GetCam[];
extern const char str_PyMethod_Scrap_GetEntity[];
extern const char str_PyMethod_Scrap_GetFarestParked[];
extern const char str_PyMethod_Scrap_GetFirst[];
extern const char str_PyMethod_Scrap_GetFloor[];
extern const char str_PyMethod_Scrap_GetFreeBlocks[];
extern const char str_PyMethod_Scrap_GetGogID[];
extern const char str_PyMethod_Scrap_GetLangEntries[];
extern const char str_PyMethod_Scrap_GetLangStr[];
extern const char str_PyMethod_Scrap_GetLanguage[];
extern const char str_PyMethod_Scrap_GetLevelPath[];
extern const char str_PyMethod_Scrap_GetLockAlarm[];
extern const char str_PyMethod_Scrap_GetMinCamDist[];
extern const char str_PyMethod_Scrap_GetMoney[];
extern const char str_PyMethod_Scrap_GetNearestParked[];
extern const char str_PyMethod_Scrap_GetNetFlags[];
extern const char str_PyMethod_Scrap_GetNewLevelPath[];
extern const char str_PyMethod_Scrap_GetSaveGamesList[];
extern const char str_PyMethod_Scrap_GetSaveVar[];
extern const char str_PyMethod_Scrap_GetSavedProfilesList[];
extern const char str_PyMethod_Scrap_GetSteamID[];
extern const char str_PyMethod_Scrap_GetSteamTicket[];
extern const char str_PyMethod_Scrap_GetTime[];
extern const char str_PyMethod_Scrap_GetTimeSpeed[];
extern const char str_PyMethod_Scrap_GetVideoCurrentMode[];
extern const char str_PyMethod_Scrap_GetVideoModes[];
extern const char str_PyMethod_Scrap_IncSaveVar[];
extern const char str_PyMethod_Scrap_InitLoading[];
extern const char str_PyMethod_Scrap_IsApp[];
extern const char str_PyMethod_Scrap_IsSteam[];
extern const char str_PyMethod_Scrap_LaunchDashboard[];
extern const char str_PyMethod_Scrap_ListModels[];
extern const char str_PyMethod_Scrap_LoadGameVars[];
extern const char str_PyMethod_Scrap_LoadLevel[];
extern const char str_PyMethod_Scrap_ModelInfo[];
extern const char str_PyMethod_Scrap_MusicPlayer[];
extern const char str_PyMethod_Scrap_NextDummySearch[];
extern const char str_PyMethod_Scrap_OpenPack[];
extern const char str_PyMethod_Scrap_OpenStore[];
extern const char str_PyMethod_Scrap_Preload3DObject[];
extern const char str_PyMethod_Scrap_PreloadAnm[];
extern const char str_PyMethod_Scrap_PreloadLibrary[];
extern const char str_PyMethod_Scrap_Print[];
extern const char str_PyMethod_Scrap_ProcessDVF[];
extern const char str_PyMethod_Scrap_PythonCompileAll[];
extern const char str_PyMethod_Scrap_Rand[];
extern const char str_PyMethod_Scrap_Round[];
extern const char str_PyMethod_Scrap_RunApp[];
extern const char str_PyMethod_Scrap_SaveConfig[];
extern const char str_PyMethod_Scrap_SaveGameVars[];
extern const char str_PyMethod_Scrap_ScreenShot[];
extern const char str_PyMethod_Scrap_SetAlarm[];
extern const char str_PyMethod_Scrap_SetAlarmChar[];
extern const char str_PyMethod_Scrap_SetAlarmGrow[];
extern const char str_PyMethod_Scrap_SetCallFunc[];
extern const char str_PyMethod_Scrap_SetCam[];
extern const char str_PyMethod_Scrap_SetDebrisSys[];
extern const char str_PyMethod_Scrap_SetDebrisValue[];
extern const char str_PyMethod_Scrap_SetGrid[];
extern const char str_PyMethod_Scrap_SetLanguage[];
extern const char str_PyMethod_Scrap_SetLockAlarm[];
extern const char str_PyMethod_Scrap_SetMoney[];
extern const char str_PyMethod_Scrap_SetSaveProfile[];
extern const char str_PyMethod_Scrap_SetSaveVar[];
extern const char str_PyMethod_Scrap_SetTime[];
extern const char str_PyMethod_Scrap_SetTimeSpeed[];
extern const char str_PyMethod_Scrap_SetVideoCurrentMode[];
extern const char str_PyMethod_Scrap_ShowGVars[];
extern const char str_PyMethod_Scrap_SphereCall[];
extern const char str_PyMethod_Scrap_StartDummySearch[];
extern const char str_PyMethod_Scrap_SwitchMissionArrows[];
extern const char str_PyMethod_Scrap_TestLine[];
extern const char str_PyMethod_Scrap_TestSphere[];
extern const char str_PyMethod_Scrap_UsrEntity[];
extern const char str_PyMethod_Scrap_Ver[];
extern const char str_PyMethod_Scrap_Verbose[];
extern const char str_PyMethod_Scrap_VideoPlayer[];
PyMethodDef Scrap_methods[115] = {
	{
		.ml_name = (void *)str_PyMethod_Scrap_Print,
		.ml_meth = (void *)Scrap_Print,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Print,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ConsoleOut,
		.ml_meth = (void *)Scrap_ConsoleOut,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ConsoleOut,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ConsoleError,
		.ml_meth = (void *)Scrap_ConsoleError,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ConsoleError,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DebugInput,
		.ml_meth = (void *)Scrap_DebugInput,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DebugInput,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Verbose,
		.ml_meth = (void *)Scrap_Verbose,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Verbose,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Ver,
		.ml_meth = (void *)Scrap_Ver,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Ver,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ExtractPack,
		.ml_meth = (void *)Scrap_ExtractPack,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ExtractPack,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_OpenPack,
		.ml_meth = (void *)Scrap_OpenPack,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_OpenPack,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ClosePack,
		.ml_meth = (void *)Scrap_ClosePack,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ClosePack,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_PreloadLibrary,
		.ml_meth = (void *)Scrap_PreloadLibrary,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_PreloadLibrary,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Preload3DObject,
		.ml_meth = (void *)Scrap_Preload3DObject,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Preload3DObject,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_PreloadAnm,
		.ml_meth = (void *)Scrap_PreloadAnm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_PreloadAnm,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_PythonCompileAll,
		.ml_meth = (void *)Scrap_PythonCompileAll,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_PythonCompileAll,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Get,
		.ml_meth = (void *)Scrap_Get,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Get,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Def,
		.ml_meth = (void *)Scrap_Def,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Def,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Des,
		.ml_meth = (void *)Scrap_Des,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Des,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Set,
		.ml_meth = (void *)Scrap_Set,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Set,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ShowGVars,
		.ml_meth = (void *)Scrap_ShowGVars,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ShowGVars,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SwitchMissionArrows,
		.ml_meth = (void *)Scrap_SwitchMissionArrows,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SwitchMissionArrows,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetTime,
		.ml_meth = (void *)Scrap_GetTime,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetTime,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetTimeSpeed,
		.ml_meth = (void *)Scrap_GetTimeSpeed,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetTimeSpeed,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetTime,
		.ml_meth = (void *)Scrap_SetTime,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetTime,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetTimeSpeed,
		.ml_meth = (void *)Scrap_SetTimeSpeed,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetTimeSpeed,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddScheduledFunc,
		.ml_meth = (void *)Scrap_AddScheduledFunc,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddScheduledFunc,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DeleteScheduledFuncs,
		.ml_meth = (void *)Scrap_DeleteScheduledFuncs,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DeleteScheduledFuncs,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetCam,
		.ml_meth = (void *)Scrap_SetCam,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetCam,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetCam,
		.ml_meth = (void *)Scrap_GetCam,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetCam,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetMinCamDist,
		.ml_meth = (void *)Scrap_GetMinCamDist,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetMinCamDist,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetEntity,
		.ml_meth = (void *)Scrap_GetEntity,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetEntity,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetFirst,
		.ml_meth = (void *)Scrap_GetFirst,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetFirst,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CreateEntity,
		.ml_meth = (void *)Scrap_CreateEntity,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CreateEntity,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_UsrEntity,
		.ml_meth = (void *)Scrap_UsrEntity,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_UsrEntity,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_EntityListGet,
		.ml_meth = (void *)Scrap_EntityListGet,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_EntityListGet,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_EntityListSet,
		.ml_meth = (void *)Scrap_EntityListSet,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_EntityListSet,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CreateEntityList,
		.ml_meth = (void *)Scrap_CreateEntityList,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CreateEntityList,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_LoadLevel,
		.ml_meth = (void *)Scrap_LoadLevel,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_LoadLevel,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetLevelPath,
		.ml_meth = (void *)Scrap_GetLevelPath,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetLevelPath,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetNewLevelPath,
		.ml_meth = (void *)Scrap_GetNewLevelPath,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetNewLevelPath,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetGrid,
		.ml_meth = (void *)Scrap_SetGrid,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetGrid,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetFloor,
		.ml_meth = (void *)Scrap_GetFloor,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetFloor,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CreateElements,
		.ml_meth = (void *)Scrap_CreateElements,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CreateElements,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CallElements,
		.ml_meth = (void *)Scrap_CallElements,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CallElements,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetNetFlags,
		.ml_meth = (void *)Scrap_GetNetFlags,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetNetFlags,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_StartDummySearch,
		.ml_meth = (void *)Scrap_StartDummySearch,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_StartDummySearch,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_NextDummySearch,
		.ml_meth = (void *)Scrap_NextDummySearch,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_NextDummySearch,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Rand,
		.ml_meth = (void *)Scrap_Rand,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Rand,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Round,
		.ml_meth = (void *)Scrap_Round,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Round,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetDebrisSys,
		.ml_meth = (void *)Scrap_SetDebrisSys,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetDebrisSys,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DropDebris,
		.ml_meth = (void *)Scrap_DropDebris,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DropDebris,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetDebrisValue,
		.ml_meth = (void *)Scrap_SetDebrisValue,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetDebrisValue,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ModelInfo,
		.ml_meth = (void *)Scrap_ModelInfo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ModelInfo,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ListModels,
		.ml_meth = (void *)Scrap_ListModels,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ListModels,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_BuildFont,
		.ml_meth = (void *)Scrap_BuildFont,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_BuildFont,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ScreenShot,
		.ml_meth = (void *)Scrap_ScreenShot,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ScreenShot,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_ProcessDVF,
		.ml_meth = (void *)Scrap_ProcessDVF,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_ProcessDVF,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_VideoPlayer,
		.ml_meth = (void *)Scrap_VideoPlayer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_VideoPlayer,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_MusicPlayer,
		.ml_meth = (void *)Scrap_MusicPlayer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_MusicPlayer,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetVideoCurrentMode,
		.ml_meth = (void *)Scrap_GetVideoCurrentMode,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetVideoCurrentMode,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetVideoCurrentMode,
		.ml_meth = (void *)Scrap_SetVideoCurrentMode,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetVideoCurrentMode,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetVideoModes,
		.ml_meth = (void *)Scrap_GetVideoModes,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetVideoModes,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_TestLine,
		.ml_meth = (void *)Scrap_TestLine,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_TestLine,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_TestSphere,
		.ml_meth = (void *)Scrap_TestSphere,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_TestSphere,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SphereCall,
		.ml_meth = (void *)Scrap_SphereCall,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SphereCall,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetCallFunc,
		.ml_meth = (void *)Scrap_SetCallFunc,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetCallFunc,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddParamf,
		.ml_meth = (void *)Scrap_AddParamf,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddParamf,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddParami,
		.ml_meth = (void *)Scrap_AddParami,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddParami,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddParams,
		.ml_meth = (void *)Scrap_AddParams,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddParams,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Execute,
		.ml_meth = (void *)Scrap_Execute,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Execute,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DelSaveVars,
		.ml_meth = (void *)Scrap_DelSaveVars,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DelSaveVars,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CreateSaveVar,
		.ml_meth = (void *)Scrap_CreateSaveVar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CreateSaveVar,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetSaveVar,
		.ml_meth = (void *)Scrap_GetSaveVar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetSaveVar,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetSaveVar,
		.ml_meth = (void *)Scrap_SetSaveVar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetSaveVar,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_IncSaveVar,
		.ml_meth = (void *)Scrap_IncSaveVar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_IncSaveVar,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_LoadGameVars,
		.ml_meth = (void *)Scrap_LoadGameVars,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_LoadGameVars,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_CreateSaveProfile,
		.ml_meth = (void *)Scrap_CreateSaveProfile,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_CreateSaveProfile,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DeleteProfile,
		.ml_meth = (void *)Scrap_DeleteProfile,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DeleteProfile,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetSaveProfile,
		.ml_meth = (void *)Scrap_SetSaveProfile,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetSaveProfile,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetSavedProfilesList,
		.ml_meth = (void *)Scrap_GetSavedProfilesList,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetSavedProfilesList,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetFreeBlocks,
		.ml_meth = (void *)Scrap_GetFreeBlocks,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetFreeBlocks,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetFreeBlocks,
		.ml_meth = (void *)Scrap_GetFreeBlocks,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetFreeBlocks,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_LaunchDashboard,
		.ml_meth = (void *)Scrap_LaunchDashboard,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_LaunchDashboard,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_DeselectProfile,
		.ml_meth = (void *)Scrap_DeselectProfile,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_DeselectProfile,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SaveGameVars,
		.ml_meth = (void *)Scrap_SaveGameVars,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SaveGameVars,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SaveConfig,
		.ml_meth = (void *)Scrap_SaveConfig,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SaveConfig,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetSaveGamesList,
		.ml_meth = (void *)Scrap_GetSaveGamesList,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetSaveGamesList,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetLanguage,
		.ml_meth = (void *)Scrap_SetLanguage,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetLanguage,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetLanguage,
		.ml_meth = (void *)Scrap_GetLanguage,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetLanguage,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetLangStr,
		.ml_meth = (void *)Scrap_GetLangStr,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetLangStr,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetLangEntries,
		.ml_meth = (void *)Scrap_GetLangEntries,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetLangEntries,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetAlarm,
		.ml_meth = (void *)Scrap_GetAlarm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetAlarm,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetAlarm,
		.ml_meth = (void *)Scrap_SetAlarm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetAlarm,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetAlarmGrow,
		.ml_meth = (void *)Scrap_SetAlarmGrow,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetAlarmGrow,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetAlarmChars,
		.ml_meth = (void *)Scrap_GetAlarmChars,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetAlarmChars,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetAlarmChar,
		.ml_meth = (void *)Scrap_SetAlarmChar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetAlarmChar,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetLockAlarm,
		.ml_meth = (void *)Scrap_GetLockAlarm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetLockAlarm,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetLockAlarm,
		.ml_meth = (void *)Scrap_SetLockAlarm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetLockAlarm,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetMoney,
		.ml_meth = (void *)Scrap_GetMoney,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetMoney,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_SetMoney,
		.ml_meth = (void *)Scrap_SetMoney,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_SetMoney,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddParkingZone,
		.ml_meth = (void *)Scrap_AddParkingZone,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddParkingZone,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetFarestParked,
		.ml_meth = (void *)Scrap_GetFarestParked,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetFarestParked,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetNearestParked,
		.ml_meth = (void *)Scrap_GetNearestParked,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetNearestParked,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_AddItem,
		.ml_meth = (void *)Scrap_AddItem,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_AddItem,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_InitLoading,
		.ml_meth = (void *)Scrap_InitLoading,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_InitLoading,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_EndLoading,
		.ml_meth = (void *)Scrap_EndLoading,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_EndLoading,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Exit,
		.ml_meth = (void *)Scrap_Exit,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Exit,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_FileExist,
		.ml_meth = (void *)Scrap_FileExist,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_FileExist,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_RunApp,
		.ml_meth = (void *)Scrap_RunApp,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_RunApp,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_Achievement,
		.ml_meth = (void *)Scrap_Achievement,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_Achievement,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_OpenStore,
		.ml_meth = (void *)Scrap_OpenStore,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_OpenStore,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_IsApp,
		.ml_meth = (void *)Scrap_IsApp,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_IsApp,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetSteamID,
		.ml_meth = (void *)Scrap_GetSteamID,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetSteamID,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetSteamTicket,
		.ml_meth = (void *)Scrap_GetSteamTicket,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetSteamTicket,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_IsSteam,
		.ml_meth = (void *)Scrap_IsSteam,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_IsSteam,
	},
	{
		.ml_name = (void *)str_PyMethod_Scrap_GetGogID,
		.ml_meth = (void *)Scrap_GetGogID,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_Scrap_GetGogID,
	},
};

SD_PY_MODULE(Scrap, 0x42c610)

const char str_PyMethod_Scrap_ScreenShot[] = "ScreenShot";

const char str_PyMethod_Scrap_ProcessDVF[] = "ProcessDVF";

const char str_PyMethod_Scrap_VideoPlayer[] = "VideoPlayer";

const char str_PyMethod_Scrap_MusicPlayer[] = "MusicPlayer";

const char str_PyMethod_Scrap_GetVideoCurrentMode[] = "GetVideoCurrentMode";

const char str_PyMethod_Scrap_SetVideoCurrentMode[] = "SetVideoCurrentMode";

const char str_PyMethod_Scrap_GetVideoModes[] = "GetVideoModes";

const char str_PyMethod_Scrap_TestLine[] = "TestLine";

const char str_PyMethod_Scrap_TestSphere[] = "TestSphere";

const char str_PyMethod_Scrap_SphereCall[] = "SphereCall";

const char str_PyMethod_Scrap_SetCallFunc[] = "SetCallFunc";

const char str_PyMethod_Scrap_AddParamf[] = "AddParamf";

const char str_PyMethod_Scrap_AddParami[] = "AddParami";

const char str_PyMethod_Scrap_AddParams[] = "AddParams";

const char str_PyMethod_Scrap_Execute[] = "Execute";

const char str_PyMethod_Scrap_DelSaveVars[] = "DelSaveVars";

const char str_PyMethod_Scrap_CreateSaveVar[] = "CreateSaveVar";

const char str_PyMethod_Scrap_GetSaveVar[] = "GetSaveVar";

const char str_PyMethod_Scrap_SetSaveVar[] = "SetSaveVar";

const char str_PyMethod_Scrap_IncSaveVar[] = "IncSaveVar";

const char str_PyMethod_Scrap_LoadGameVars[] = "LoadGameVars";

const char str_PyMethod_Scrap_CreateSaveProfile[] = "CreateSaveProfile";

const char str_PyMethod_Scrap_DeleteProfile[] = "DeleteProfile";

const char str_PyMethod_Scrap_SetSaveProfile[] = "SetSaveProfile";

const char str_PyMethod_Scrap_GetSavedProfilesList[] = "GetSavedProfilesList";

const char str_PyMethod_Scrap_GetFreeBlocks[] = "GetFreeBlocks";

const char str_PyMethod_Scrap_LaunchDashboard[] = "LaunchDashboard";

const char str_PyMethod_Scrap_DeselectProfile[] = "DeselectProfile";

const char str_PyMethod_Scrap_SaveGameVars[] = "SaveGameVars";

const char str_PyMethod_Scrap_SaveConfig[] = "SaveConfig";

const char str_PyMethod_Scrap_GetSaveGamesList[] = "GetSaveGamesList";

const char str_PyMethod_Scrap_SetLanguage[] = "SetLanguage";

const char str_PyMethod_Scrap_GetLanguage[] = "GetLanguage";

const char str_PyMethod_Scrap_GetLangStr[] = "GetLangStr";

const char str_PyMethod_Scrap_GetLangEntries[] = "GetLangEntries";

const char str_PyMethod_Scrap_GetAlarm[] = "GetAlarm";

const char str_PyMethod_Scrap_SetAlarm[] = "SetAlarm";

const char str_PyMethod_Scrap_SetAlarmGrow[] = "SetAlarmGrow";

const char str_PyMethod_Scrap_GetAlarmChars[] = "GetAlarmChars";

const char str_PyMethod_Scrap_SetAlarmChar[] = "SetAlarmChar";

const char str_PyMethod_Scrap_GetLockAlarm[] = "GetLockAlarm";

const char str_PyMethod_Scrap_SetLockAlarm[] = "SetLockAlarm";

const char str_PyMethod_Scrap_GetMoney[] = "GetMoney";

const char str_PyMethod_Scrap_SetMoney[] = "SetMoney";

const char str_PyMethod_Scrap_AddParkingZone[] = "AddParkingZone";

const char str_PyMethod_Scrap_GetFarestParked[] = "GetFarestParked";

const char str_PyMethod_Scrap_GetNearestParked[] = "GetNearestParked";

const char str_PyMethod_Scrap_AddItem[] = "AddItem";

const char str_PyMethod_Scrap_InitLoading[] = "InitLoading";

const char str_PyMethod_Scrap_EndLoading[] = "EndLoading";

const char str_PyMethod_Scrap_Exit[] = "Exit";

const char str_PyMethod_Scrap_FileExist[] = "FileExist";

const char str_PyMethod_Scrap_RunApp[] = "RunApp";

const char str_PyMethod_Scrap_Achievement[] = "Achievement";

const char str_PyMethod_Scrap_OpenStore[] = "OpenStore";

const char str_PyMethod_Scrap_IsApp[] = "IsApp";

const char str_PyMethod_Scrap_GetSteamID[] = "GetSteamID";

const char str_PyMethod_Scrap_GetSteamTicket[] = "GetSteamTicket";

const char str_PyMethod_Scrap_IsSteam[] = "IsSteam";

const char str_PyMethod_Scrap_GetGogID[] = "GetGogID";

const char str_PyMethod_Scrap_Print[] = "Print";

const char str_PyMethod_Scrap_ConsoleOut[] = "ConsoleOut";

const char str_PyMethod_Scrap_ConsoleError[] = "ConsoleError";

const char str_PyMethod_Scrap_DebugInput[] = "DebugInput";

const char str_PyMethod_Scrap_Verbose[] = "Verbose";

const char str_PyMethod_Scrap_Ver[] = "Ver";

char str_PyDoc_Scrap_EntityListGet[] =
	"(value)Scrap.EntityListGet(listName, varName) :  Obtiene el valor de una variable de una lista de en"
	"tidades";

const char str_PyMethod_Scrap_ExtractPack[] = "ExtractPack";

const char str_PyMethod_Scrap_OpenPack[] = "OpenPack";

const char str_PyMethod_Scrap_ClosePack[] = "ClosePack";

char str_PyDoc_Scrap_CreateEntityList[] = "CreateEntityList(listName) :  Crea una lista de entidades";

const char str_PyMethod_Scrap_PreloadLibrary[] = "PreloadLibrary";

char str_PyDoc_Scrap_SetCam[] = "SetCam(i,name) :  Activa una camara";

const char str_PyMethod_Scrap_Preload3DObject[] = "Preload3DObject";

char str_PyDoc_Scrap_GetCam[] = "name GetCam(i) :  Obtiene el nombre de una entidad camara";

const char str_PyMethod_Scrap_PreloadAnm[] = "PreloadAnm";

const char str_PyMethod_Scrap_PythonCompileAll[] = "PythonCompileAll";

char str_PyDoc_Scrap_GetMinCamDist[] =
	"GetMinCamDist(x,y,z) :  Obtiene la distancia de un punto a la camara";

const char str_PyMethod_SScorer_Get[] = "Get";

const char str_PyMethod_Scrap_Def[] = "Def";

char str_PyDoc_Scrap_LoadLevel[] = "LoadLevel(string name) :  Carga un nivel en el siguiente frame";

const char str_PyMethod_Scrap_Des[] = "Des";

const char str_PyMethod_SScorer_Set[] = "Set";

char str_PyDoc_Scrap_GetLevelPath[] = "levelpath GetLevelPath() :  Obtiene el path del nivel actual";

const char str_PyMethod_Scrap_ShowGVars[] = "ShowGVars";

const char str_PyMethod_Scrap_SwitchMissionArrows[] = "SwitchMissionArrows";

char str_PyDoc_Scrap_GetNewLevelPath[] =
	"levelpath GetNewLevelPath() :  Obtiene el path del próximo nivel";

const char str_PyMethod_Scrap_GetTime[] = "GetTime";

const char str_PyMethod_Scrap_GetTimeSpeed[] = "GetTimeSpeed";

char str_PyDoc_Scrap_GetFloor[] = "i GetFloor(y) :  Obtiene el numero de piso en el que esta.";

const char str_PyMethod_Scrap_SetTime[] = "SetTime";

char str_PyDoc_Scrap_CreateElements[88] =
	"CreateElements() :  Crea una lista de elementos estaticos que pertenecen al mapa.";

const char str_PyMethod_Scrap_SetTimeSpeed[] = "SetTimeSpeed";

const char str_PyMethod_Scrap_AddScheduledFunc[] = "AddScheduledFunc";

char str_PyDoc_Scrap_CallElements[] =
	"CallElements('Function') :  llama una funcion Function(Name,x,y,z,angx,angy)";

const char str_PyMethod_Scrap_DeleteScheduledFuncs[] = "DeleteScheduledFuncs";

const char str_PyMethod_Scrap_SetCam[] = "SetCam";

char str_PyDoc_Scrap_SetGrid[] = "SetGrid(x,y,z) :  Modifica las dimensiones del grid de colision";

const char str_PyMethod_Scrap_GetCam[] = "GetCam";

const char str_PyMethod_Scrap_GetMinCamDist[] = "GetMinCamDist";

char str_PyDoc_Scrap_GetNetFlags[] =
	"GetNetFlags() :  Obtiene la tupla de flags : client, server, dedicated";

const char str_PyMethod_Scrap_GetEntity[] = "GetEntity";

const char str_PyMethod_Scrap_GetFirst[] = "GetFirst";

char str_PyDoc_Scrap_Rand[] = "Rand(min,max) :  Obtiene un numero seudo-aleatorio entre (min,max)";

const char str_PyMethod_Scrap_CreateEntity[] = "CreateEntity";

char str_PyDoc_Scrap_Round[] = "Round(num) : Redondea un numero real al entero más cercano.";

const char str_PyMethod_Scrap_UsrEntity[] = "UsrEntity";

const char str_PyMethod_Scrap_EntityListGet[] = "EntityListGet";

char str_PyDoc_Scrap_NextDummySearch[] =
	"(s(ddd)(dd)i) NextDummySearch() : Obtiene el proximo dummy, sino None";

const char str_PyMethod_Scrap_EntityListSet[] = "EntityListSet";

const char str_PyMethod_Scrap_CreateEntityList[] = "CreateEntityList";

char str_PyDoc_Scrap_StartDummySearch[] =
	"StartDummySearch(name,usewildcards) : Inicia la busqueda de dummies en el mapa";

const char str_PyMethod_Scrap_LoadLevel[] = "LoadLevel";

const char str_PyMethod_Scrap_GetLevelPath[] = "GetLevelPath";

char str_PyDoc_Scrap_ModelInfo[] =
	"ModelInfo(name) : Muestra información sobre la jerarquía de nodos del modelo";

const char str_PyMethod_Scrap_GetNewLevelPath[] = "GetNewLevelPath";

char str_PyDoc_Scrap_ListModels[] = "ListModels() : Muestra una lista de modelos y escenas";

const char str_PyMethod_Scrap_SetGrid[] = "SetGrid";

const char str_PyMethod_Scrap_GetFloor[] = "GetFloor";

char str_PyDoc_Scrap_BuildFont[] =
	"BuildFont(name,size) : Construye un archivo de fuente de letras a partir de un .TGA dado";

const char str_PyMethod_Scrap_CreateElements[] = "CreateElements";

const char str_PyMethod_Scrap_CallElements[] = "CallElements";

char str_PyDoc_Scrap_ScreenShot[] = "ScreenShot(filename) : Screenshot of the current frame";

const char str_PyMethod_Scrap_GetNetFlags[] = "GetNetFlags";

const char str_PyMethod_Scrap_StartDummySearch[] = "StartDummySearch";

char str_PyDoc_Scrap_ProcessDVF[] = "ProcessDVF(filename,command) : send a dvf command";

const char str_PyMethod_Scrap_NextDummySearch[] = "NextDummySearch";

char str_PyDoc_Scrap_VideoPlayer[] = "VideoPlayer(filename[,command]) : play video file";

const char str_PyMethod_Scrap_Rand[] = "Rand";

char str_PyDoc_Scrap_MusicPlayer[] = "MusicPlayer(filename[,command,param1]) : play music file";

const char str_PyMethod_Scrap_Round[] = "Round";

const char str_PyMethod_Scrap_SetDebrisSys[] = "SetDebrisSys";

char str_PyDoc_Scrap_GetVideoCurrentMode[] = "GetVideoCurrentMode() : Get Video Current Mode Index";

const char str_PyMethod_Scrap_DropDebris[] = "DropDebris";

const char str_PyMethod_Scrap_SetDebrisValue[] = "SetDebrisValue";

char str_PyDoc_Scrap_SetVideoCurrentMode[] = "SetVideoCurrentMode() : Set Video Current Mode Index";

const char str_PyMethod_Scrap_ModelInfo[] = "ModelInfo";

const char str_PyMethod_Scrap_ListModels[] = "ListModels";

char str_PyDoc_Scrap_GetVideoModes[] = "GetVideoModes(NumMode) : Get Video Mode Info";

const char str_PyMethod_Scrap_BuildFont[] = "BuildFont";

char str_PyDoc_Scrap_SetDebrisSys[] =
	"SetDebrisSys(name,initialnumber,MaxDist,Friction,Scale,Grav,bounce)  crea/modifica un sistema de deb"
	"ris";

char str_PyDoc_Scrap_DropDebris[] =
	"DropDebris(name,size) : Lanza un objeto que cae y rebota hasta que desaparece";

char str_PyDoc_Scrap_SetDebrisValue[96] =
	"SetDebrisValue(id,x,y,z) : id = 0:Posicion, 1:Angulos, 2:Velocidad, 3:Velocidad de rotacion";

char str_PyDoc_Scrap_TestLine[] =
	"((x,y,z),s) TestLine((x,y,z),(fz,fy,fz),'strmask') : testea una linea y devuelve el nombre de la ent"
	"idad y punto de colision  o '' si es el mapa o None";

char str_PyDoc_Scrap_TestSphere[128] =
	"Scrap.TestSphere(x,y,z,radius,´strmask´) : testea una esfera y devuelve el nombre de la entidad o ''"
	" si es el mapa o None";

char str_PyDoc_Scrap_SphereCall[] =
	"Scrap.SphereCall(x,y,z,radius,´strmask´,´callback´,[IgnoreGeometry=1]) : testea una esfera y llama a"
	" ´callback´ por cada entidad que colisiona";

char str_PyDoc_Scrap_SetCallFunc[] =
	"Scrap.SetCallFunc('!funcname') : Especifica la funcion callback (c++) a llamar. (1 si existe)";

char str_PyDoc_Scrap_AddParamf[80] =
	"Scrap.AddParamf(Float) : Incluye un parametro a una funcion callback (c++).";

char str_PyDoc_Scrap_AddParami[80] =
	"Scrap.AddParami(int) : Incluye un parametro a una funcion callback (c++).";

char str_PyDoc_Scrap_AddParams[] =
	"Scrap.AddParams(string) : Incluye un parametro a una funcion callback (c++).";

char str_PyDoc_Scrap_Execute[] = "Scrap.Execute() : Ejecuta una funcion callback (c++).";

char str_PyDoc_Scrap_DelSaveVars[] = "Scrap.DelSaveVars(str) : Elimina las <str>* variables";

char str_PyDoc_Scrap_CreateSaveVar[] =
	"bool Scrap.CreateSaveVar(Name,DefValue) : Crea una nueva variable (si puede)";

char str_PyDoc_Scrap_SetSaveVar[] =
	"bool Scrap.SetSaveVar(Name,DefValue) : Modifica una variable (si puede)";

char str_PyDoc_Scrap_GetSaveVar[72] =
	"value Scrap.GetSaveVar(Name) : obtiene una variable o devuelve none";

char str_PyDoc_Scrap_SaveGameVars[88] =
	"Scrap.SaveGameVars(str, str) : Salva un juego en un archivo, con un nombre opcional";

char str_PyDoc_Scrap_IncSaveVar[] =
	"int Scrap.IncSaveVar(str[,value]) : incrementa un contador de estadisticas";

char str_PyDoc_Scrap_LoadGameVars[60] = "Scrap.LoadGameVars(str) : Carga un juego en un archivo";

char str_PyDoc_Scrap_GetSavedProfilesList[] =
	"[(Directorio, nombre)] Scrap.GetSavedProfilesList() : Devuelve una lista de tuplas donde para cada t"
	"upla el primer elemento indica el directorio del profile y el segundo elemento indica el nombre del "
	"profile. La lista está ordenada por el criterio de microsoft ";

char str_PyDoc_Scrap_CreateSaveProfile[] =
	"int Scrap.CreateSaveProfile(str) : Crea un Save profile para X-Box y pone como actual.";

char str_PyDoc_Scrap_DeleteProfile[64] = "int Scrap.DeleteProfile(str) : Borra un profile de X-Box.";

char str_PyDoc_Scrap_GetFreeBlocks[] =
	"int Scrap.GetFreeBlocks() : obtiene los bloques libres en el disco duro.";

char str_PyDoc_Scrap_DeselectProfile[] = "Scrap.DeselectProfile() : deselecciona el profile actual.";

char str_PyDoc_Scrap_LaunchDashboard[] =
	"int Scrap.LaunchDashboard() : sale del juego y ejecuta el dashboard.";

char str_PyDoc_Scrap_SetSaveProfile[] =
	"int Scrap.SetSaveProfile(path,name) : Carga un profile de jugador.";

char str_PyDoc_Scrap_DebugInput[68] = "string DebugInput() :  Detiene todo para iniciar la depuracion";

char str_PyDoc_Scrap_GetSaveGamesList[296] =
	" [(fecha, slot)] Scrap.GetSaveGamesList() : Devuelve una lista de tuplas donde para cada tupla el pr"
	"imer elemento indica el nombre (fecha) del archivo y el segundo elemento indica el slot del que se l"
	"eerá la partida. La lista está ordenada por la fecha, de la más reciente a la más antigua.";

char str_PyDoc_Scrap_Print[] = "Print(string name) :  Muestra un mensaje por la consola siempre";

char str_PyDoc_Scrap_SaveConfig[64] = "res Scrap.SaveConfig() : Guarda el fichero de configuración";

char str_PyDoc_Scrap_ConsoleOut[72] =
	"ConsoleOut(string name) :  Muestra un mensaje pyton por la consola";

char str_PyDoc_Scrap_SetLanguage[120] =
	"res Scrap.SetLanguage(Name[,forcetoreload]) : Modifica la lengua actual. false si ya es la lengua la"
	" selecionada";

char str_PyDoc_Scrap_ConsoleError[] =
	"ConsoleError(string name) :  Muestra un mensaje de error por la consola";

char str_PyDoc_Scrap_Verbose[] =
	"Verbose(string name) :  Muestra un mensaje de parloteo por la consola";

char str_PyDoc_Scrap_GetLanguage[] =
	"lang Scrap.GetLanguage() : Obtiene la lengua actual. None si no fue inicializada.";

char str_PyDoc_Scrap_Exit[28] = "Exit() sale del juego.";

char str_PyDoc_Scrap_GetLangStr[] =
	"str Scrap.GetLangStr(Name) : Obtiene una cadena de lenguaje. '' si no existe.";

char str_PyDoc_Scrap_FileExist[] = "FileExist() check for file existence.";

char str_PyDoc_Scrap_GetLangEntries[] =
	"[str] Scrap.GetLangEntries(Name) : Obtiene un subconjunto de entradas de la tabla de lenguaje que em"
	"piecen por 'Name'.";

char str_PyDoc_Scrap_Ver[40] = "Ver() Obtiene la version del juego.";

char str_PyDoc_Scrap_GetAlarm[] =
	"(active,dialpos,growcode) Scrap.GetAlarm() : Obtiene informacion de la alarma.";

char str_PyDoc_Scrap_ExtractPack[64] = "ExtractPack(string PackPath) :  Extrae un archivo *.packed";

char str_PyDoc_Scrap_SetAlarm[] =
	"Scrap.SetAlarm(dialpos) : pone la alarma en una posicion (0 desactiva, 1 activa).";

char str_PyDoc_Scrap_OpenPack[] = "OpenPack(string PackPath) :  Abre un archivo *.packed";

char str_PyDoc_Scrap_SetAlarmGrow[] = "Scrap.SetAlarmGrow(growcode) : -1 baja, 0 se mantiene, 1 sube.";

char str_PyDoc_Scrap_ClosePack[44] = "ClosePack() :  Abre un archivo *.packed";

char str_PyDoc_Scrap_GetAlarmChars[] =
	"(Actual, El_que_se_busca) Scrap.GetAlarmChars() : Obtiene los tipos de personaje que se buscan.";

char str_PyDoc_Scrap_PreloadLibrary[96] =
	"PreloadLibrary(LibraryName,CompiledFile) :  Precarga una libreria de un archivo empaquetado";

char str_PyDoc_Scrap_SetAlarmChar[] =
	"Scrap.SetAlarmChar(El_que_se_busca) : Modifica el personaje que se busca.";

char str_PyDoc_Scrap_SetLockAlarm[60] = "Scrap.SetLockAlarm(1/0) : Bloquea/desbloquea la alarma";

char str_PyDoc_Scrap_Preload3DObject[] =
	"Preload3DObject(filename,scalex,scaley,scalez) :  Precarga un objeto 3D";

char str_PyDoc_Scrap_GetLockAlarm[] = "Scrap.GetLockAlarm() : verdadero si la alarma esta bloqueada";

char str_PyDoc_Scrap_PreloadAnm[] = "PreloadAnm(ObjFilename,AnmFilename) :  Precarga una animacion";

char str_PyDoc_Scrap_GetMoney[] = "money Scrap.GetMoney() : Devuelve el liquido disponible.";

char str_PyDoc_Scrap_PythonCompileAll[] =
	"PythonCompileAll() :  compila recursivamente los archivos .py";

char str_PyDoc_Scrap_SetMoney[60] = "Scrap.SetMoney(money) : Determina el liquido disponible";

char str_PyDoc_Scrap_ShowGVars[72] = "ShowGVars() :  Muestra una lista de todas las variables globales";

char str_PyDoc_Scrap_AddParkingZone[] =
	"Scrap.AddParkingZone(occupied,pos,name) : Agrega un sitio de parking.";

char str_PyDoc_Scrap_GetFarestParked[] =
	"Entity,Pos =  Scrap.GetFarestParked([isparked[,fromPos]]) : Obtiene la nave aparcada mas lejana.";

char str_PyDoc_Scrap_Get[] = "Get('GlobalVar') :  Obtiene el valor de una variable global";

char str_PyDoc_Scrap_GetNearestParked[104] =
	"Entity,Pos = Scrap.GetNearestParked([isparked[,fromPos]]) : Obtiene la nave aparcada mas cercana.";

char str_PyDoc_Scrap_Def[] = "Def('GlobalVar') :  Obtiene el valor por defecto de una variable global";

char str_PyDoc_Scrap_Des[72] = "Des('GlobalVar') :  Obtiene el descriptor de una variable global";

char str_PyDoc_Scrap_AddItem[] =
	"Scrap.AddItem(Life, string name, int life) : Add Item Life.\n"
	" Scrap.AddItem(Ammo, string name, int typeAmmo, int ammo) : Add Item Ammo.";

char str_PyDoc_Scrap_SwitchMissionArrows[] =
	"SwitchMissionArrows(MainMissionFile,TargetMissionFile) : Cambia las flechas de mision primaria y sec"
	"undaria (Modelos)";

char str_PyDoc_Scrap_InitLoading[56] = "InitLoading() Inicia la pantalla de carga rapida.";

char str_PyDoc_Scrap_Set[] = "Set('GlobalVar',val) :  Modifica el valor de una variable global";

char str_PyDoc_Scrap_EndLoading[] = "EndLoading() Inicia la pantalla de carga rapida.";

char str_PyDoc_Scrap_GetTime[] = "GetTime() :  Obtiene el tiempo del mundo en segundos";

char str_PyDoc_Scrap_RunApp[] = "RunApp() Ejecuta un programa externo.";

char str_PyDoc_Scrap_GetTimeSpeed[] = "GetTimeSpeed() :  Obtiene la velocidad del tiempo del mundo.";

char str_PyDoc_Scrap_Achievement[] = "Achievement() marca un logro como consegido";

char str_PyDoc_Scrap_SetTime[] = "SetTime(Time) :  Cambia el tiempo del mundo en segundos";

char str_PyDoc_Scrap_OpenStore[] = "OpenStore() abre la store en una apicacion";

char str_PyDoc_Scrap_SetTimeSpeed[] =
	"SetTimeSpeed(TimeSpeed) :  Cambia la velocidad del tiempo del mundo.";

char str_PyDoc_Scrap_IsApp[] = "IsApp() si tienes una apicacion";

char str_PyDoc_Scrap_AddScheduledFunc[] =
	"AddScheduledFunc(time,func,params[,name]) : Ejecuta un codigo en python en un instante de tiempo.";

char str_PyDoc_Scrap_GetSteamID[] = "GetSteamID() id de steam";

char str_PyDoc_Scrap_DeleteScheduledFuncs[] =
	"DeleteScheduledFuncs(name) : Borra las scheduled funcs creadas con un nombre dado.";

char str_PyDoc_Scrap_IsSteam[] = "IsSteam() Es steam";

char str_PyDoc_Scrap_GetEntity[] = "GetEntity(string name) :  Retorna una entidad";

char str_PyDoc_Scrap_GetSteamTicket[] = "GetSteamTicket() ticket de autenticacion de steam";

char str_PyDoc_Scrap_GetGogID[] = "GetGogID() id de gog";

char str_PyDoc_Scrap_GetFirst[68] = "GetFirst() :  Retorna la primera entuidad de la lista o none";

char str_PyDoc_Scrap_UsrEntity[] =
	"UsrEntity(ictr) :  Retorna una entidad controlada por usario (personaje o nave)";

char str_PyDoc_Scrap_CreateEntity[] = "CreateEntity(name,x,y,z,type) :  Crea una una entidad";

char str_PyDoc_Scrap_EntityListSet[] =
	"EntityListSet(listName, varName, value) :  Modifica el valor de una variable de una lista de entidad"
	"es";

