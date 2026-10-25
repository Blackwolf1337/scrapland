/*
 * Python module Scrap (method table Scrap_methods, rva 0x42c610): engine functions for the game scripts.
 * Original file: PyInterface\\ScrapPy.cpp
 */
#include <math.h>
#include <string.h>

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
	cPy_Function_AddParamF(g_CallFunc, (float)v);
	return ReturnNone();
}

/* AddParami(value) */
PyObject *Scrap_AddParami(PyObject *self, PyObject *args) {
	int v;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &v)) {
		return NULL;
	}
	cPy_Function_AddParamI(g_CallFunc, v);
	return ReturnNone();
}

/* AddParams(value) */
PyObject *Scrap_AddParams(PyObject *self, PyObject *args) {
	char *v;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &v)) {
		return NULL;
	}
	cPy_Function_AddParamS(g_CallFunc, v);
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
	cPy_Function_Execute(g_CallFunc, 0);
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
	return ReturnNone();
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
	return ReturnNone();
}

/* OpenPack(path) : the pack name becomes "path/" */
PyObject *Scrap_OpenPack(PyObject *self, PyObject *args) {
	char *path;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}
	cString_Printf(g_PackName, "%s/", path);
	return ReturnNone();
}

/* OpenStore(appid) : Steam overlay store page; NULL for appid < 0, nothing without Steam */
PyObject *Scrap_OpenStore(PyObject *self, PyObject *args) {
	int appId;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &appId) || appId < 0) {
		return NULL;
	}
	if (g_IsSteam) {
		/* ISteamFriends::ActivateGameOverlayToStore (vtable 0x7c) of the context the exe keeps */
		void **ctx = g_SteamInternal_ContextInit(g_SteamFriendsContext);
		void **friends = ctx[2];
		typedef void(__thiscall * store_t)(void *, int, int);
		((store_t)(*(void ***)friends)[0x7c / 4])(friends, appId, 0);
	}
	return ReturnNone();
}

/* PreloadAnm(a, b) */
PyObject *Scrap_PreloadAnm(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	Anm_Preload(a, b);
	return ReturnNone();
}

/* PreloadLibrary(a, b) */
PyObject *Scrap_PreloadLibrary(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	Pack_PreloadLibrary(a, b);
	return ReturnNone();
}

/* Print(text) */
PyObject *Scrap_Print(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	Con_Print(0, text, 0);
	return ReturnNone();
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
	return ReturnNone();
}

/* PythonCompileAll() */
PyObject *Scrap_PythonCompileAll(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Python_CompileAll();
	return ReturnNone();
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
	return ReturnNone();
}

/* SetAlarm(level) */
PyObject *Scrap_SetAlarm(PyObject *self, PyObject *args) {
	float level;

	(void)self;
	if (!PyArg_ParseTuple(args, "f", &level)) {
		return NULL;
	}
	Alarm_Set(g_pWorld->alarm, level);
	return ReturnNone();
}

/* SetAlarmChar(name) */
PyObject *Scrap_SetAlarmChar(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	Alarm_SetChar(g_pWorld->alarm, name);
	return ReturnNone();
}

/* SetAlarmGrow(grow) */
PyObject *Scrap_SetAlarmGrow(PyObject *self, PyObject *args) {
	int grow;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &grow)) {
		return NULL;
	}
	Alarm_SetGrow(g_pWorld->alarm, grow);
	return ReturnNone();
}

/* SetCallFunc(func) : 1 if the function is valid */
PyObject *Scrap_SetCallFunc(PyObject *self, PyObject *args) {
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &func)) {
		return NULL;
	}
	cPy_Function_SetFunc(g_CallFunc, func);
	return Py_BuildValue("i", cPy_Function_IsValid(g_CallFunc) ? 1 : 0);
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
	Alarm_SetLock(g_pWorld->alarm, lock != 0);
	return ReturnNone();
}

/* SetMoney(money) */
PyObject *Scrap_SetMoney(PyObject *self, PyObject *args) {
	int money;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &money)) {
		return NULL;
	}
	g_pWorld->money = money;
	return ReturnNone();
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
	Timer_SetTime(g_pWorld->timer, (float)t);
	return Py_BuildValue("");
}

/* SetTimeSpeed(speed) */
PyObject *Scrap_SetTimeSpeed(PyObject *self, PyObject *args) {
	double speed;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &speed)) {
		return NULL;
	}
	Timer_SetSpeed(g_pWorld->timer, (float)speed);
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
	return ReturnNone();
}

/* ShowGVars([filter]) */
PyObject *Scrap_ShowGVars(PyObject *self, PyObject *args) {
	char *filter = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "|s", &filter)) {
		return NULL;
	}
	GVars_Show(filter);
	return ReturnNone();
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
	return ReturnNone();
}

/* SwitchMissionArrows(a, b) */
PyObject *Scrap_SwitchMissionArrows(PyObject *self, PyObject *args) {
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &a, &b)) {
		return NULL;
	}
	MissionArrows_Switch(a, b);
	return ReturnNone();
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
		return ReturnNone();
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
	return ReturnNone();
}

/* VideoPlayer(name[, cmd]) : cmd default "play" */
PyObject *Scrap_VideoPlayer(PyObject *self, PyObject *args) {
	char *name, *cmd = "play";

	(void)self;
	if (!PyArg_ParseTuple(args, "s|s", &name, &cmd)) {
		return NULL;
	}
	Video_Play(name, cmd, 0.0f);
	return ReturnNone();
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
	switch (v->type) {
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
		if (v->type == GVAR_STRING) {
			if (v->flags & 1) {
				goto readonly;
			}
			cString_Set(&v->value.s, s);
			return Py_BuildValue("i", 1);
		}
		if (v->type != GVAR_SCRIPT_FUNCTION) {
			Con_Errorf("ERROR: '%s' global variable must be string\n", name);
			return NULL;
		}
		if (v->flags & 1) {
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
	if (v->type == GVAR_INT) {
		if (v->flags & 1) {
			goto readonly;
		}
		v->value.i = (int)d;
		return Py_BuildValue("i", 1);
	}
	if (v->type != GVAR_FLOAT) {
		Con_Errorf("ERROR: '%s' global variable must be integer or float\n", name);
		return NULL;
	}
	if (v->flags & 1) {
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
	PyObject *r = Py_BuildValue("i", cFilePack_Exists(&pack, name));
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
	return ReturnNone();
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
		return ReturnNone();
	}
	return PyEntity_New(e->name);
}

/* IsApp(appid) : 1 if the Steam app is owned and installed (always 1 without Steam); NULL for appid < 0 */
PyObject *Scrap_IsApp(PyObject *self, PyObject *args) {
	int appId;
	int r = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &appId) || appId < 0) {
		return NULL;
	}
	if (g_IsSteam) {
		typedef byte(__thiscall * appfunc_t)(void *, int);
		/* ISteamApps (context + 0x18): BIsSubscribedApp (vtable 0x18), BIsAppInstalled (vtable 0x4c) */
		void **apps = ((void ***)g_SteamInternal_ContextInit(g_SteamFriendsContext))[6];
		if (!((appfunc_t)(*(void ***)apps)[0x18 / 4])(apps, appId)) {
			r = 0;
		}
		else {
			apps = ((void ***)g_SteamInternal_ContextInit(g_SteamFriendsContext))[6];
			r = ((appfunc_t)(*(void ***)apps)[0x4c / 4])(apps, appId) ? 1 : 0;
		}
	}
	return Py_BuildValue("i", r);
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
	return ReturnNone();
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
	Grid_Set(g_pWorld->grid, &size);
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
	return ReturnNone();
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
	Parking_AddZone(g_pWorld->parking, flag != 0, &pos, name);
	return ReturnNone();
}

/* GetNearestParked / GetFarestParked([flag[, (x,y,z)]]) : (name, (x, y, z)); default position: current camera */
static PyObject *FindParked(PyObject *args, int farest) {
	vec3_t pos = g_pWorld->camPos[g_pWorld->curCam];
	int flag = 1;

	if (!PyArg_ParseTuple(args, "|i(fff)", &flag, &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	vec3_t out = pos;
	const char *name = Parking_Find(g_pWorld->parking, flag != 0, &pos, &out, farest);
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
		return ReturnNone();
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
	return ReturnNone();
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
		return ReturnNone();
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
		return ReturnNone();
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, name);
	sys->b = (float)b;
	sys->d = (float)d;
	float rf = (float)radius;
	sys->radiusSq = rf * rf;
	sys->c = (float)c;
	sys->flag = flag != 0;
	cDebrisSys_Init(sys, n, e);
	return ReturnNone();
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
	deb->speed = (float)speed;
	cDebrisSys_Drop(sys, deb);
	return ReturnNone();
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
	return ReturnNone();
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
		g_pWorld->items.vftable->AddLife(&g_pWorld->items, &item);
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
		g_pWorld->items.vftable->AddAmmo(&g_pWorld->items, kind, &item);
		cString_dtor(&item.name);
	}
	return ReturnNone();
}

SD_PY_MODULE(Scrap, 0x42c610)
