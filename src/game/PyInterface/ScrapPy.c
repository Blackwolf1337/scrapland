/*
 * Python module Scrap (method table Scrap_methods): engine functions for the game scripts.
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
#include "scrap/scriptapi.h"
#include "scrap/staticinit.h"

/* levelpath GetLevelPath() : Obtiene el path del level actual */
API_FN(Scrap, GetLevelPath) {
	return Mode_Parse(a, n, "") && Api_Build(out, "s", g_pApp->levelPath);
}

/* money Scrap.GetMoney() : Devuelve el liquido disponible. */
API_FN(Scrap, GetMoney) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", g_pWorld->money);
}

/* --- batch 2: the small functions of the module (A to L) --- */

/* Achievement(n) : NULL for n < 0, else nothing; differs from the original on purpose (no store features): no GOG or
 * Steam achievement is unlocked */
API_FN(Scrap, Achievement) {
	int id;
	return Mode_Parse(a, n, "i", &id) && id >= 0;
}

/* the function of Scrap.SetCallFunc with its parameters (AddParamX) for Scrap.Execute */
static cPy_Function g_CallFunc;

/* AddParamf(value) : float parameter for the function of SetCallFunc */
API_FN(Scrap, AddParamf) {
	double v;
	if (!Mode_Parse(a, n, "d", &v)) {
		return 0;
	}
	cPy_Function_AddParamF((&g_CallFunc), (float)v);
	return 1;
}

void CallFunc_ExecuteF(const char *func, const float *params, int n) {
	cPy_Function_SetFunc((&g_CallFunc), func);
	for (int i = 0; i < n; i++) {
		cPy_Function_AddParamF((&g_CallFunc), params[i]);
	}
	cPy_Function_Execute((&g_CallFunc), 0);
}

/* AddParami(value) */
API_FN(Scrap, AddParami) {
	int v;
	if (!Mode_Parse(a, n, "i", &v)) {
		return 0;
	}
	cPy_Function_AddParamI((&g_CallFunc), v);
	return 1;
}

/* AddParams(value) */
API_FN(Scrap, AddParams) {
	const char *v;
	if (!Mode_Parse(a, n, "s", &v)) {
		return 0;
	}
	cPy_Function_AddParamS((&g_CallFunc), v);
	return 1;
}

/* AddScheduledFunc(time,func,params[,name]) : Ejecuta un codigo en python en un instante de tiempo. OBJLIB 7: the
 * scheduler keeps the callable and the argument tuple as the objects given, so the entry is the adapter on Python
 * objects (MODE_PYENTRY: its ref takes the slow path) */
static PyObject *Scrap_AddScheduledFunc(PyObject *self, PyObject *args) {
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
MODE_PYENTRY(Scrap_AddScheduledFunc)

/* BuildFont(name, size) */
API_FN(Scrap, BuildFont) {
	const char *name;
	int size;
	if (!Mode_Parse(a, n, "si", &name, &size)) {
		return 0;
	}
	Font_Build(name, size);
	return 1;
}

/* CallElements(name) */
API_FN(Scrap, CallElements) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Elements_Call(&g_pWorld->elements, name);
	return 1;
}

/* ClosePack() : forgets the open pack */
API_FN(Scrap, ClosePack) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	cString_Set(g_PackName, "");
	return 1;
}

/* ConsoleError(text) */
API_FN(Scrap, ConsoleError) {
	const char *text;
	if (!Mode_Parse(a, n, "s", &text)) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, text, 0);
	return 1;
}

/* ConsoleOut(text) */
API_FN(Scrap, ConsoleOut) {
	const char *text;
	if (!Mode_Parse(a, n, "s", &text)) {
		return 0;
	}
	Con_Print(0xff0000, text, 0);
	return 1;
}

/* CreateElements() */
API_FN(Scrap, CreateElements) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Elements_Create(&g_pWorld->elements);
	return 1;
}

/* CreateEntityList(listName) */
API_FN(Scrap, CreateEntityList) {
	const char *listName;
	if (!Mode_Parse(a, n, "s", &listName)) {
		return 0;
	}
	cWorld *World = g_pWorld;
	CHECK(cWorld_NewEntityList(World, listName));
	return 1;
}

static byte NoProfiles(const char *name, nativeVal_t *out) {
	Con_Print(CON_COLOR_ERROR, "ERROR : ", 0);
	Con_Print(CON_COLOR_ERROR, name, 0);
	Con_Print(CON_COLOR_ERROR, " : Player Profile system not supported on PC\n", 0);
	return Api_Build(out, "i", 1);
}

/* CreateSaveProfile(name) : not on PC */
API_FN(Scrap, CreateSaveProfile) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && NoProfiles(name, out);
}

/* CreateSaveVar(name, value) */
API_FN(Scrap, CreateSaveVar) {
	const char *name, *value;
	return Mode_Parse(a, n, "ss", &name, &value) && Api_Build(out, "i", SaveVar_Create(name, value) != 0);
}

/* DebugInput(text) */
API_FN(Scrap, DebugInput) {
	const char *text;
	return Mode_Parse(a, n, "s", &text) && Api_Build(out, "s", App_DebugInput(g_pApp, text));
}

/* DeleteProfile(name) : not on PC */
API_FN(Scrap, DeleteProfile) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && NoProfiles(name, out);
}

/* DeleteScheduledFuncs(name) */
API_FN(Scrap, DeleteScheduledFuncs) {
	const char *name = NULL;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Scheduler_Delete(&g_pWorld->scheduler, name);
	return 1;
}

/* DelSaveVars(prefix) */
API_FN(Scrap, DelSaveVars) {
	const char *prefix;
	if (!Mode_Parse(a, n, "s", &prefix)) {
		return 0;
	}
	SaveVars_Delete(prefix);
	return 1;
}

/* Des(name) : description of an engine variable; NULL without exception if there is none (golden records it) */
API_FN(Scrap, Des) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cGVar *v = GVar_Find(name);
	if (!v) {
		Con_Errorf("ERROR: '%s' global variable not defined\n", name);
		return 0;
	}
	return Api_Build(out, "s", v->base.description);
}

/* DeselectProfile() : not on PC */
API_FN(Scrap, DeselectProfile) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console, no Profile.\n", 0);
	return 1;
}

/* EndLoading() */
API_FN(Scrap, EndLoading) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Loading_End();
	return 1;
}

/* Execute() : Ejecuta una funcion callback (c++). */
API_FN(Scrap, Execute) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	cPy_Function_Execute((&g_CallFunc), 0);
	return 1;
}

/* Exit() sale del juego. */
API_FN(Scrap, Exit) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	App_Exit(g_pApp);
	return 1;
}

/* ExtractPack(name) */
API_FN(Scrap, ExtractPack) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Api_Build(out, "i", Pack_Extract(name));
}

/* GetAlarm() : (on, level, value); level and on are read before the value */
API_FN(Scrap, GetAlarm) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	float level = g_pWorld->alarm.value;
	byte on = g_pWorld->alarm.on;
	int value = Alarm_GetValue(&g_pWorld->alarm);
	return Api_Build(out, "ifi", on != 0, (double)level, value);
}

/* GetAlarmChars() */
API_FN(Scrap, GetAlarmChars) {
	return Mode_Parse(a, n, "") && Api_Build(out, "ss", g_pWorld->alarm.chars, g_pWorld->alarm.lastChars);
}

/* GetCam(i) : name of the camera */
API_FN(Scrap, GetCam) {
	int i;
	return Mode_Parse(a, n, "i", &i) && Api_Build(out, "s", cWorld_GetCamName(g_pWorld, i));
}

/* GetFirst() : Retorna la primera entidad de la list o none */
API_FN(Scrap, GetFirst) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	if (!cWorld_GetFirstEntity(g_pWorld)) {
		return 1;
	}
	return Api_Handle(cWorld_GetFirstEntity(g_pWorld)->name, out);
}

/* GetFloor(y) */
API_FN(Scrap, GetFloor) {
	float y;
	return Mode_Parse(a, n, "f", &y) && Api_Build(out, "i", Floor_Get(&g_pWorld->floor, y));
}

/* GetFreeBlocks() : no Xbox, always 50000 */
API_FN(Scrap, GetFreeBlocks) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console.\n", 0);
	return Api_Build(out, "i", 50000);
}

/* GetGogID() : None; differs from the original on purpose (no store features), which gave the GalaxyID */
API_FN(Scrap, GetGogID) {
	return Mode_Parse(a, n, "");
}

/* GetLangEntries(name[, n]) : n default 3 */
static PyObject *Scrap_GetLangEntries(PyObject *self, PyObject *args) {
	char *name;
	int n = 3;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &n)) {
		return NULL;
	}
	return Lang_GetEntries(name, n);
}

/* GetLangStr(name) : "" and a warning when the string is missing */
API_FN(Scrap, GetLangStr) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	const char *s = Lang_GetStr(name);
	if (s) {
		return Api_Build(out, "s", s);
	}
	Con_Warnf("STRING NOT FOUND: '%s'\n", name);
	return Api_Build(out, "s", "");
}

/* GetLanguage() */
API_FN(Scrap, GetLanguage) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	const char *lang = Lang_GetLanguage();
	if (!lang) {
		return 1;
	}
	return Api_Build(out, "s", lang);
}

/* GetLockAlarm() */
API_FN(Scrap, GetLockAlarm) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Alarm_IsLocked(&g_pWorld->alarm) != 0);
}

/* GetNetFlags() : (bit 0, bit 1, bit 2) of the application flags */
API_FN(Scrap, GetNetFlags) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	uint32_t f = g_pApp->flags;
	return Api_Build(out, "iii", (int)(f & 1), (int)(f >> 1 & 1), (int)(f >> 2 & 1));
}

/* GetNewLevelPath() */
API_FN(Scrap, GetNewLevelPath) {
	return Mode_Parse(a, n, "") && Api_Build(out, "s", g_pApp->newLevelPath);
}

/* GetSavedProfilesList() : not on PC, empty list */
static PyObject *Scrap_GetSavedProfilesList(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : GetSavedProfilesList not supported on PC\n", 0);
	return PyList_New(0);
}

/* GetSaveGamesList(path) */
static PyObject *Scrap_GetSaveGamesList(PyObject *self, PyObject *args) {
	char *path;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}
	return SaveGames_GetList(path);
}

/* GetSaveVar(name) */
API_FN(Scrap, GetSaveVar) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Api_Build(out, "s", SaveVar_Get(name));
}

/* GetSteamID() : None, as the original without Steam (no store features) */
API_FN(Scrap, GetSteamID) {
	return Mode_Parse(a, n, "");
}

/* GetSteamTicket() : None, as the original without Steam (no store features) */
API_FN(Scrap, GetSteamTicket) {
	return Mode_Parse(a, n, "");
}

/* GetTime() : Obtiene el tiempo del world en segundos */
API_FN(Scrap, GetTime) {
	return Mode_Parse(a, n, "") && Api_Build(out, "d", (double)Timer_GetTime(&g_pWorld->timer));
}

/* GetTimeSpeed() */
API_FN(Scrap, GetTimeSpeed) {
	return Mode_Parse(a, n, "") && Api_Build(out, "d", (double)g_pWorld->timer.speed);
}

/* GetVideoCurrentMode() : Get Video Current Mode Index. No argument check. */
API_FN(Scrap, GetVideoCurrentMode) {
	return Api_Build(out, "i", Video_GetCurrentMode());
}

/* IncSaveVar(name[, n]) : n default 1 */
API_FN(Scrap, IncSaveVar) {
	const char *name;
	int inc = 1;
	return Mode_Parse(a, n, "s|i", &name, &inc) && Api_Build(out, "i", SaveVar_Inc(name, inc));
}

/* InitLoading() */
API_FN(Scrap, InitLoading) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Loading_Init();
	return 1;
}

/* IsSteam() : None, as the original without Steam (no store features) */
API_FN(Scrap, IsSteam) {
	return Mode_Parse(a, n, "");
}

/* LaunchDashboard(n) : no Xbox, 0 */
API_FN(Scrap, LaunchDashboard) {
	int id;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : no Xbox console, no Dashoboard.\n", 0);
	return Api_Build(out, "i", 0);
}

/* ListModels() */
API_FN(Scrap, ListModels) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Models_List();
	return 1;
}

/* --- batch 3: the small functions of the module (L to Z) --- */

/* LoadGameVars(name) */
API_FN(Scrap, LoadGameVars) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Api_Build(out, "i", SaveGame_Load(name) != 0);
}

/* LoadLevel(level) */
API_FN(Scrap, LoadLevel) {
	const char *level;
	if (!Mode_Parse(a, n, "s", &level)) {
		return 0;
	}
	CWinApp_LoadLevel(g_pApp, level);
	return Api_Build(out, "i", 1);
}

/* ModelInfo(name[, arg]) : arg default 1 */
API_FN(Scrap, ModelInfo) {
	const char *name;
	int arg = 1;
	if (!Mode_Parse(a, n, "s|i", &name, &arg)) {
		return 0;
	}
	Model_Info(name, arg);
	return 1;
}

/* MusicPlayer(name[, cmd[, volume]]) : cmd default "play", volume default 0 */
API_FN(Scrap, MusicPlayer) {
	const char *name, *cmd = "play";
	double volume = 0.0;
	if (!Mode_Parse(a, n, "s|sd", &name, &cmd, &volume)) {
		return 0;
	}
	Music_Play(name, cmd, (float)volume);
	return 1;
}

/* OpenPack(path) : the pack name becomes "path/" */
API_FN(Scrap, OpenPack) {
	const char *path;
	if (!Mode_Parse(a, n, "s", &path)) {
		return 0;
	}
	cString_Printf(g_PackName, "%s/", path);
	return 1;
}

/* OpenStore(appid) : NULL for appid < 0, else nothing, as the original without Steam (no store features; with Steam it
 * opened the store page in the overlay) */
API_FN(Scrap, OpenStore) {
	int appId;
	return Mode_Parse(a, n, "i", &appId) && appId >= 0;
}

/* PreloadAnm(a, b) */
API_FN(Scrap, PreloadAnm) {
	const char *model, *anim;
	if (!Mode_Parse(a, n, "ss", &model, &anim)) {
		return 0;
	}
	Anm_Preload(model, anim);
	return 1;
}

/* PreloadLibrary(a, b) */
API_FN(Scrap, PreloadLibrary) {
	const char *name, *file;
	if (!Mode_Parse(a, n, "ss", &name, &file)) {
		return 0;
	}
	Pack_PreloadLibrary(name, file);
	return 1;
}

/* Print(text) */
API_FN(Scrap, Print) {
	const char *text;
	if (!Mode_Parse(a, n, "s", &text)) {
		return 0;
	}
	Con_Print(0, text, 0);
	return 1;
}

/* ProcessDVF(name, arg) */
API_FN(Scrap, ProcessDVF) {
	const char *name;
	int arg;
	if (!Mode_Parse(a, n, "si", &name, &arg)) {
		return 0;
	}
	DVF_Process(name, arg);
	return 1;
}

/* PythonCompileAll() */
API_FN(Scrap, PythonCompileAll) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Python_CompileAll();
	return 1;
}

/* Round(v) : the larger of (int)v and (int)(v + 0.5), both truncated */
API_FN(Scrap, Round) {
	double v;
	if (!Mode_Parse(a, n, "d", &v)) {
		return 0;
	}
	int lo = (int)v, hi = (int)(v + 0.5);
	return Api_Build(out, "i", hi > lo ? hi : lo);
}

/* SaveConfig() */
API_FN(Scrap, SaveConfig) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Config_Save();
	return Api_Build(out, "i", 1);
}

/* SaveGameVars(name[, title]) : title default "Scrapland SaveGame" */
API_FN(Scrap, SaveGameVars) {
	const char *name, *title = "Scrapland SaveGame";
	return Mode_Parse(a, n, "s|s", &name, &title) && Api_Build(out, "i", SaveGame_Save(name, title) != 0);
}

/* ScreenShot(name) */
API_FN(Scrap, ScreenShot) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Screenshot_Take(name);
	return 1;
}

/* SetAlarm(level) */
API_FN(Scrap, SetAlarm) {
	float level;
	if (!Mode_Parse(a, n, "f", &level)) {
		return 0;
	}
	Alarm_Set(&g_pWorld->alarm, level);
	return 1;
}

/* SetAlarmChar(name) */
API_FN(Scrap, SetAlarmChar) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Alarm_SetChar(&g_pWorld->alarm, name);
	return 1;
}

/* SetAlarmGrow(grow) */
API_FN(Scrap, SetAlarmGrow) {
	int grow;
	if (!Mode_Parse(a, n, "i", &grow)) {
		return 0;
	}
	Alarm_SetGrow(&g_pWorld->alarm, grow);
	return 1;
}

/* SetCallFunc(func) : 1 if the function is valid */
API_FN(Scrap, SetCallFunc) {
	const char *func;
	if (!Mode_Parse(a, n, "s", &func)) {
		return 0;
	}
	cPy_Function_SetFunc((&g_CallFunc), func);
	return Api_Build(out, "i", cPy_Function_IsValid((&g_CallFunc)) ? 1 : 0);
}

/* SetCam(i, name) */
API_FN(Scrap, SetCam) {
	int i;
	const char *name;
	return Mode_Parse(a, n, "is", &i, &name) && Api_Build(out, "i", cWorld_SetCam(g_pWorld, i, name));
}

/* SetLanguage(lang[, flag]) */
API_FN(Scrap, SetLanguage) {
	const char *lang;
	int flag = 0;
	return Mode_Parse(a, n, "s|i", &lang, &flag) && Api_Build(out, "i", Lang_Set(lang, flag != 0) != 0);
}

/* SetLockAlarm(lock) */
API_FN(Scrap, SetLockAlarm) {
	int lock;
	if (!Mode_Parse(a, n, "i", &lock)) {
		return 0;
	}
	Alarm_SetLock(&g_pWorld->alarm, lock != 0);
	return 1;
}

/* SetMoney(money) */
API_FN(Scrap, SetMoney) {
	int money;
	if (!Mode_Parse(a, n, "i", &money)) {
		return 0;
	}
	g_pWorld->money = money;
	return 1;
}

/* SetSaveVar(name, value) */
API_FN(Scrap, SetSaveVar) {
	const char *name, *value;
	return Mode_Parse(a, n, "ss", &name, &value) && Api_Build(out, "i", SaveVar_Set(name, value) != 0);
}

/* SetTime(t) : returns Py_BuildValue("") (None) like the original */
API_FN(Scrap, SetTime) {
	double t;
	if (!Mode_Parse(a, n, "d", &t)) {
		return 0;
	}
	Timer_SetTime(&g_pWorld->timer, (float)t);
	return Api_Build(out, "");
}

/* SetTimeSpeed(speed) */
API_FN(Scrap, SetTimeSpeed) {
	double speed;
	if (!Mode_Parse(a, n, "d", &speed)) {
		return 0;
	}
	Timer_SetSpeed(&g_pWorld->timer, (float)speed);
	return Api_Build(out, "");
}

/* SetVideoCurrentMode(mode) */
API_FN(Scrap, SetVideoCurrentMode) {
	int mode;
	if (!Mode_Parse(a, n, "i", &mode)) {
		return 0;
	}
	Video_SetCurrentMode(mode);
	return 1;
}

/* ShowGVars([filter]) */
API_FN(Scrap, ShowGVars) {
	const char *filter = NULL;
	if (!Mode_Parse(a, n, "|s", &filter)) {
		return 0;
	}
	GVars_Show(filter);
	return 1;
}

/* ConsoleExec(line) : runs a line of the console (Console_Execute); not in the original, the test path of the console
 * command table */
API_FN(Scrap, ConsoleExec) {
	const char *line;
	if (!Mode_Parse(a, n, "s", &line)) {
		return 0;
	}
	Console_Execute(line);
	return 1;
}

/* StartDummySearch(name[, flag]) : flag default 1 */
API_FN(Scrap, StartDummySearch) {
	const char *name;
	int flag = 1;
	if (!Mode_Parse(a, n, "s|i", &name, &flag)) {
		return 0;
	}
	DummySearch_Start(name, flag != 0);
	return 1;
}

/* SwitchMissionArrows(a, b) */
API_FN(Scrap, SwitchMissionArrows) {
	const char *on, *off;
	if (!Mode_Parse(a, n, "ss", &on, &off)) {
		return 0;
	}
	MissionArrows_Switch(on, off);
	return 1;
}

/* UsrEntity(player) : the entity of a local player; NULL without exception for a player above 3 (golden records it;
 * a negative player is above 3 as unsigned) */
API_FN(Scrap, UsrEntity) {
	int player;
	if (!Mode_Parse(a, n, "i", &player) || (uint32_t)player > 3) {
		return 0;
	}
	cEntity *e = cWorld_GetUsrEntity(g_pWorld, player);
	if (!e) {
		return 1;
	}
	return Api_Handle(e->name, out);
}

/* Ver() Obtiene la version del juego. */
API_FN(Scrap, Ver) {
	return Mode_Parse(a, n, "") && Api_Build(out, "s", Version_Get());
}

/* Verbose(text) : debug color */
API_FN(Scrap, Verbose) {
	const char *text;
	if (!Mode_Parse(a, n, "s", &text)) {
		return 0;
	}
	Con_Print(CON_COLOR_DEBUG, text, 0);
	return 1;
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
 * addition: 0 not found, 1 played to the end or not opened, 2 stopped by a key). OBJLIB 7: the entry is the adapter
 * on Python objects (MODE_PYENTRY, its ref takes the slow path): done is kept as the object given, and for "a|b"
 * Video_PlayFile keeps a pointer into the text of name until b plays
 */
static PyObject *Scrap_VideoPlayer(PyObject *self, PyObject *args) {
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
MODE_PYENTRY(Scrap_VideoPlayer)

/* --- batch 4: engine variables, entities, files, random numbers, Steam apps --- */

/* Get(name) / Def(name) : current or default value of an engine variable in its type; NULL without exception for an
 * unknown name (golden records it) */
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

static PyObject *Scrap_Get(PyObject *self, PyObject *args) {
	(void)self;
	return GVarValue(args, 0, 0x11d);
}

static PyObject *Scrap_Def(PyObject *self, PyObject *args) {
	(void)self;
	return GVarValue(args, 1, 0x13f);
}

/*
 * Set(name, value) : string ("ss") for string and script function variables, number ("sd") for int and float
 * ones. A read only variable gives 0 and a message; a wrong type or an unknown name NULL without exception (golden
 * records both).
 */
static PyObject *Scrap_Set(PyObject *self, PyObject *args) {
	char *name, *s;
	double d;
	int r;

	(void)self;
	if (PyArg_ParseTuple(args, "ss", &name, &s)) {
		r = GVar_SetStr(name, s);
	} else {
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "sd", &name, &d)) {
			return NULL;
		}
		r = GVar_SetNum(name, d);
	}
	return r < 0 ? NULL : Py_BuildValue("i", r);
}

/* a random number between a and b (CRT rand); a result equal to a bound becomes the middle */
double Scrap_RandRange(double da, double db) {
	float a = (float)da, b = (float)db;
	int r = crt_rand();
	double v = (double)(float)((double)(r & 0x7fff) * (1.0 / 32767.0) * (double)(b - a) + (double)a);
	if (v == db || v == da) {
		v = (da + db) * 0.5;
	}
	return v;
}

/* Rand(a, b) : random number between a and b */
static PyObject *Scrap_Rand(PyObject *self, PyObject *args) {
	double da, db;

	(void)self;
	if (!PyArg_ParseTuple(args, "dd", &da, &db)) {
		return NULL;
	}
	return Py_BuildValue("d", Scrap_RandRange(da, db));
}

/* FileExist(name) : looks in the packs and on disk */
static PyObject *Scrap_FileExist(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_GetEntity(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_CreateEntity(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_IsApp(PyObject *self, PyObject *args) {
	int appId;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &appId) || appId < 0) {
		return NULL;
	}
	return Py_BuildValue("i", 1);
}

/* RunApp(exe, cmdLine) : starts a program; "" for the game itself */
static PyObject *Scrap_RunApp(PyObject *self, PyObject *args) {
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
	App_RunApp(g_pApp, exe, cmdLine, 0);
	return Py_ReturnNone();
}

/* --- batch 5: entity lists, cameras, parking zones, video modes, grid --- */

/*
 * EntityListGet(listName, var) : value in its type. The original returns NULL without an exception (SystemError in
 * Python) for a missing list and for a variable the list cannot give. A missing list still does so: tools/test/golden
 * records it. differs from the original: an unknown variable raises an AttributeError.
 */
int EntityList_Get(const char *listName, const char *var, char **s, double *d, int *i) {
	/* differs from the original: the values start at 0, as in SScorer.Get */
	*s = NULL;
	*d = 0.0;
	*i = 0;
	void *list = cWorld_GetList(g_pWorld, listName);
	if (!list) {
		Con_Errorf("ERROR: EntityListGet '%s','%s' List Not Found.\n", listName, var);
		return -1;
	}
	int kind = cEntityList_Var(list, var, 0, s, d, i);
	if (kind < 1 || kind > 3) {
		Con_Errorf("ERROR: EntityListGet '%s','%s' Typecast error.\n", listName, var);
		return 0;
	}
	return kind;
}

static PyObject *Scrap_EntityListGet(PyObject *self, PyObject *args) {
	char *listName, *var, *s;
	double d;
	int i;
	char msg[0x100];

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &listName, &var)) {
		return NULL;
	}
	switch (EntityList_Get(listName, var, &s, &d, &i)) {
	case -1:
		return NULL;
	case 1:
		return Py_BuildValue("s", s);
	case 2:
		return Py_BuildValue("d", d);
	case 3:
		return Py_BuildValue("i", i);
	}
	Str_Printf(msg, sizeof(msg), "EntityListGet: list '%.64s' has no variable '%.64s'", listName, var);
	PyErr_SetString(PyExc_AttributeError, msg);
	return NULL;
}

/* EntityListSet(listName, var, value) : NULL without exception for a missing list (golden records it) */
static PyObject *Scrap_EntityListSet(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_GetMinCamDist(PyObject *self, PyObject *args) {
	vec3_t pos;

	(void)self;
	if (!PyArg_ParseTuple(args, "fff", &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	double d = (double)cWorld_GetMinCamDistSq(g_pWorld, &pos);
	return Py_BuildValue("d", (double)(float)sqrt(d));
}

/* GetVideoModes(i) : the number of modes for i < 0, else the name of mode i */
static PyObject *Scrap_GetVideoModes(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_SetGrid(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_SetSaveProfile(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_Preload3DObject(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_AddParkingZone(PyObject *self, PyObject *args) {
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

static PyObject *Scrap_GetNearestParked(PyObject *self, PyObject *args) {
	(void)self;
	return FindParked(args, 0);
}

static PyObject *Scrap_GetFarestParked(PyObject *self, PyObject *args) {
	(void)self;
	return FindParked(args, 1);
}

/* --- batch 6: collision tests, debris, items, dummy search --- */

/*
 * TestLine((x1,y1,z1), (x2,y2,z2), listName) : ((x, y, z) hit, name) against the level and the entities of the
 * list; "" for a level hit only, None for no hit.
 */
byte World_TestLine(const vec3_t *from, const vec3_t *to, const char *list, vec3_t *hit, cEntity **target) {
	vec3_t end = *to;
	*target = NULL;
	*hit = end;
	byte levelHit = Engine_TestLine(from, &end, hit, 0);
	end = *hit;
	cWorld_TestLineEntities(g_pWorld, 0, EntityLists_Get(list), from, &end, hit, target, 1);
	return *target || levelHit;
}

static PyObject *Scrap_TestLine(PyObject *self, PyObject *args) {
	double x1, y1, z1, x2, y2, z2;
	char *listName;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)(ddd)s", &x1, &y1, &z1, &x2, &y2, &z2, &listName)) {
		return NULL;
	}
	cEntity *target;
	vec3_t from = { (float)x1, (float)y1, (float)z1 };
	vec3_t to = { (float)x2, (float)y2, (float)z2 };
	vec3_t hit;
	if (!World_TestLine(&from, &to, listName, &hit, &target)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("(ddd)s", (double)hit.x, (double)hit.y, (double)hit.z, target ? target->name : "");
}

/* TestSphere(x, y, z, radius, listName[, exclude]) : name of the entity hit, "" for the level, None for none */
cEntity *World_TestSphere(const vec3_t *c, float r, const char *list, const char *exclude, byte *levelHit) {
	cEntity *target = NULL;
	*levelHit = Engine_TestSphere(c, c, r, 0, 0);
	cEntity *ex = NULL;
	if (*exclude) {
		HashNode *node = HashTable_Find(&g_pWorld->entities, exclude, HashTable_Hash(&g_pWorld->entities, exclude));
		if (node) {
			ex = node->value;
		}
	}
	cWorld_TestSphereEntities(g_pWorld, ex, EntityLists_Get(list), c, r, &target, 1, 0);
	return target;
}

static PyObject *Scrap_TestSphere(PyObject *self, PyObject *args) {
	double x, y, z, r;
	char *listName, *excludeName = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "dddds|s", &x, &y, &z, &r, &listName, &excludeName)) {
		return NULL;
	}
	vec3_t c = { (float)x, (float)y, (float)z };
	byte levelHit;
	cEntity *target = World_TestSphere(&c, (float)r, listName, excludeName, &levelHit);
	if (target) {
		return Py_BuildValue("s", target->name);
	}
	if (levelHit) {
		return Py_BuildValue("s", "");
	}
	return Py_ReturnNone();
}

/* SphereCall(x, y, z, radius, listName, func[, flag]) or ((x,y,z), radius, ...) : func for the entities inside */
static PyObject *Scrap_SphereCall(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_NextDummySearch(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_SetDebrisSys(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_DropDebris(PyObject *self, PyObject *args) {
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
static PyObject *Scrap_SetDebrisValue(PyObject *self, PyObject *args) {
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
void Logic_AddItem(const char *type, const char *name, const char *kindName, int value) {
	cItemDesc item;
	cString_ctor(&item.name);
	cString_Set(&item.name, name);
	item.value = value;
	if (!strcmp(type, "Life")) {
		cInfItem_AddLife(&g_pWorld->logic.items, &item);
	}
	else {
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
		cInfItem_AddAmmo(&g_pWorld->logic.items, kind, &item);
	}
	cString_dtor(&item.name);
}

static PyObject *Scrap_AddItem(PyObject *self, PyObject *args) {
	char *type = PyString_AsString(PyTuple_GetItem(args, 0));
	char *name, *kindName;
	int value;

	(void)self;
	if (!strcmp(type, "Life")) {
		if (!PyArg_ParseTuple(args, "ssi", &type, &name, &value)) {
			return NULL;
		}
		Logic_AddItem(type, name, NULL, value);
	}
	else if (!strcmp(type, "Ammo")) {
		if (!PyArg_ParseTuple(args, "sssi", &type, &name, &kindName, &value)) {
			return NULL;
		}
		Logic_AddItem(type, name, kindName, value);
	}
	return Py_ReturnNone();
}

static char str_PyDoc_Scrap_Achievement[];
static char str_PyDoc_Scrap_AddItem[];
static char str_PyDoc_Scrap_AddParamf[80];
static char str_PyDoc_Scrap_AddParami[80];
static char str_PyDoc_Scrap_AddParams[];
static char str_PyDoc_Scrap_AddParkingZone[];
static char str_PyDoc_Scrap_AddScheduledFunc[];
static char str_PyDoc_Scrap_BuildFont[];
static char str_PyDoc_Scrap_CallElements[];
static char str_PyDoc_Scrap_ClosePack[44];
static char str_PyDoc_Scrap_ConsoleError[];
static char str_PyDoc_Scrap_ConsoleOut[72];
static char str_PyDoc_Scrap_CreateElements[88];
static char str_PyDoc_Scrap_CreateEntity[];
static char str_PyDoc_Scrap_CreateEntityList[];
static char str_PyDoc_Scrap_CreateSaveProfile[];
static char str_PyDoc_Scrap_CreateSaveVar[];
static char str_PyDoc_Scrap_DebugInput[68];
static char str_PyDoc_Scrap_Def[];
static char str_PyDoc_Scrap_DelSaveVars[];
static char str_PyDoc_Scrap_DeleteProfile[64];
static char str_PyDoc_Scrap_DeleteScheduledFuncs[];
static char str_PyDoc_Scrap_Des[72];
static char str_PyDoc_Scrap_DeselectProfile[];
static char str_PyDoc_Scrap_DropDebris[];
static char str_PyDoc_Scrap_EndLoading[];
static char str_PyDoc_Scrap_EntityListGet[];
static char str_PyDoc_Scrap_EntityListSet[];
static char str_PyDoc_Scrap_Execute[];
static char str_PyDoc_Scrap_Exit[28];
static char str_PyDoc_Scrap_ExtractPack[64];
static char str_PyDoc_Scrap_FileExist[];
static char str_PyDoc_Scrap_Get[];
static char str_PyDoc_Scrap_GetAlarm[];
static char str_PyDoc_Scrap_GetAlarmChars[];
static char str_PyDoc_Scrap_GetCam[];
static char str_PyDoc_Scrap_GetEntity[];
static char str_PyDoc_Scrap_GetFarestParked[];
static char str_PyDoc_Scrap_GetFirst[68];
static char str_PyDoc_Scrap_GetFloor[];
static char str_PyDoc_Scrap_GetFreeBlocks[];
static char str_PyDoc_Scrap_GetGogID[];
static char str_PyDoc_Scrap_GetLangEntries[];
static char str_PyDoc_Scrap_GetLangStr[];
static char str_PyDoc_Scrap_GetLanguage[];
static char str_PyDoc_Scrap_GetLevelPath[];
static char str_PyDoc_Scrap_GetLockAlarm[];
static char str_PyDoc_Scrap_GetMinCamDist[];
static char str_PyDoc_Scrap_GetMoney[];
static char str_PyDoc_Scrap_GetNearestParked[104];
static char str_PyDoc_Scrap_GetNetFlags[];
static char str_PyDoc_Scrap_GetNewLevelPath[];
static char str_PyDoc_Scrap_GetSaveGamesList[296];
static char str_PyDoc_Scrap_GetSaveVar[72];
static char str_PyDoc_Scrap_GetSavedProfilesList[];
static char str_PyDoc_Scrap_GetSteamID[];
static char str_PyDoc_Scrap_GetSteamTicket[];
static char str_PyDoc_Scrap_GetTime[];
static char str_PyDoc_Scrap_GetTimeSpeed[];
static char str_PyDoc_Scrap_GetVideoCurrentMode[];
static char str_PyDoc_Scrap_GetVideoModes[];
static char str_PyDoc_Scrap_IncSaveVar[];
static char str_PyDoc_Scrap_InitLoading[56];
static char str_PyDoc_Scrap_IsApp[];
static char str_PyDoc_Scrap_IsSteam[];
static char str_PyDoc_Scrap_LaunchDashboard[];
static char str_PyDoc_Scrap_ListModels[];
static char str_PyDoc_Scrap_LoadGameVars[60];
static char str_PyDoc_Scrap_LoadLevel[];
static char str_PyDoc_Scrap_ModelInfo[];
static char str_PyDoc_Scrap_MusicPlayer[];
static char str_PyDoc_Scrap_NextDummySearch[];
static char str_PyDoc_Scrap_OpenPack[];
static char str_PyDoc_Scrap_OpenStore[];
static char str_PyDoc_Scrap_Preload3DObject[];
static char str_PyDoc_Scrap_PreloadAnm[];
static char str_PyDoc_Scrap_PreloadLibrary[96];
static char str_PyDoc_Scrap_Print[];
static char str_PyDoc_Scrap_ProcessDVF[];
static char str_PyDoc_Scrap_PythonCompileAll[];
static char str_PyDoc_Scrap_Rand[];
static char str_PyDoc_Scrap_Round[];
static char str_PyDoc_Scrap_RunApp[];
static char str_PyDoc_Scrap_SaveConfig[64];
static char str_PyDoc_Scrap_SaveGameVars[88];
static char str_PyDoc_Scrap_ScreenShot[];
static char str_PyDoc_Scrap_Set[];
static char str_PyDoc_Scrap_SetAlarm[];
static char str_PyDoc_Scrap_SetAlarmChar[];
static char str_PyDoc_Scrap_SetAlarmGrow[];
static char str_PyDoc_Scrap_SetCallFunc[];
static char str_PyDoc_Scrap_SetCam[];
static char str_PyDoc_Scrap_SetDebrisSys[];
static char str_PyDoc_Scrap_SetDebrisValue[96];
static char str_PyDoc_Scrap_SetGrid[];
static char str_PyDoc_Scrap_SetLanguage[120];
static char str_PyDoc_Scrap_SetLockAlarm[60];
static char str_PyDoc_Scrap_SetMoney[60];
static char str_PyDoc_Scrap_SetSaveProfile[];
static char str_PyDoc_Scrap_SetSaveVar[];
static char str_PyDoc_Scrap_SetTime[];
static char str_PyDoc_Scrap_SetTimeSpeed[];
static char str_PyDoc_Scrap_SetVideoCurrentMode[];
static char str_PyDoc_Scrap_ShowGVars[72];
static char str_PyDoc_Scrap_SphereCall[];
static char str_PyDoc_Scrap_StartDummySearch[];
static char str_PyDoc_Scrap_SwitchMissionArrows[];
static char str_PyDoc_Scrap_TestLine[];
static char str_PyDoc_Scrap_TestSphere[128];
static char str_PyDoc_Scrap_UsrEntity[];
static char str_PyDoc_Scrap_Ver[40];
static char str_PyDoc_Scrap_Verbose[];
static char str_PyDoc_Scrap_VideoPlayer[];
extern const char str_PyMethod_SScorer_Get[];
extern const char str_PyMethod_SScorer_Set[];
static const char str_PyMethod_Scrap_Achievement[];
static const char str_PyMethod_Scrap_AddItem[];
static const char str_PyMethod_Scrap_AddParamf[];
static const char str_PyMethod_Scrap_AddParami[];
static const char str_PyMethod_Scrap_AddParams[];
static const char str_PyMethod_Scrap_AddParkingZone[];
static const char str_PyMethod_Scrap_AddScheduledFunc[];
static const char str_PyMethod_Scrap_BuildFont[];
static const char str_PyMethod_Scrap_CallElements[];
static const char str_PyMethod_Scrap_ClosePack[];
static const char str_PyMethod_Scrap_ConsoleError[];
static const char str_PyMethod_Scrap_ConsoleOut[];
static const char str_PyMethod_Scrap_CreateElements[];
static const char str_PyMethod_Scrap_CreateEntity[];
static const char str_PyMethod_Scrap_CreateEntityList[];
static const char str_PyMethod_Scrap_CreateSaveProfile[];
static const char str_PyMethod_Scrap_CreateSaveVar[];
static const char str_PyMethod_Scrap_DebugInput[];
static const char str_PyMethod_Scrap_Def[];
static const char str_PyMethod_Scrap_DelSaveVars[];
static const char str_PyMethod_Scrap_DeleteProfile[];
static const char str_PyMethod_Scrap_DeleteScheduledFuncs[];
static const char str_PyMethod_Scrap_Des[];
static const char str_PyMethod_Scrap_DeselectProfile[];
static const char str_PyMethod_Scrap_DropDebris[];
static const char str_PyMethod_Scrap_EndLoading[];
static const char str_PyMethod_Scrap_EntityListGet[];
static const char str_PyMethod_Scrap_EntityListSet[];
static const char str_PyMethod_Scrap_Execute[];
static const char str_PyMethod_Scrap_Exit[];
static const char str_PyMethod_Scrap_ExtractPack[];
static const char str_PyMethod_Scrap_FileExist[];
static const char str_PyMethod_Scrap_GetAlarm[];
static const char str_PyMethod_Scrap_GetAlarmChars[];
static const char str_PyMethod_Scrap_GetCam[];
static const char str_PyMethod_Scrap_GetEntity[];
static const char str_PyMethod_Scrap_GetFarestParked[];
static const char str_PyMethod_Scrap_GetFirst[];
static const char str_PyMethod_Scrap_GetFloor[];
static const char str_PyMethod_Scrap_GetFreeBlocks[];
static const char str_PyMethod_Scrap_GetGogID[];
static const char str_PyMethod_Scrap_GetLangEntries[];
static const char str_PyMethod_Scrap_GetLangStr[];
static const char str_PyMethod_Scrap_GetLanguage[];
static const char str_PyMethod_Scrap_GetLevelPath[];
static const char str_PyMethod_Scrap_GetLockAlarm[];
static const char str_PyMethod_Scrap_GetMinCamDist[];
static const char str_PyMethod_Scrap_GetMoney[];
static const char str_PyMethod_Scrap_GetNearestParked[];
static const char str_PyMethod_Scrap_GetNetFlags[];
static const char str_PyMethod_Scrap_GetNewLevelPath[];
static const char str_PyMethod_Scrap_GetSaveGamesList[];
static const char str_PyMethod_Scrap_GetSaveVar[];
static const char str_PyMethod_Scrap_GetSavedProfilesList[];
static const char str_PyMethod_Scrap_GetSteamID[];
static const char str_PyMethod_Scrap_GetSteamTicket[];
static const char str_PyMethod_Scrap_GetTime[];
static const char str_PyMethod_Scrap_GetTimeSpeed[];
static const char str_PyMethod_Scrap_GetVideoCurrentMode[];
static const char str_PyMethod_Scrap_GetVideoModes[];
static const char str_PyMethod_Scrap_IncSaveVar[];
static const char str_PyMethod_Scrap_InitLoading[];
static const char str_PyMethod_Scrap_IsApp[];
static const char str_PyMethod_Scrap_IsSteam[];
static const char str_PyMethod_Scrap_LaunchDashboard[];
static const char str_PyMethod_Scrap_ListModels[];
static const char str_PyMethod_Scrap_LoadGameVars[];
static const char str_PyMethod_Scrap_LoadLevel[];
static const char str_PyMethod_Scrap_ModelInfo[];
static const char str_PyMethod_Scrap_MusicPlayer[];
static const char str_PyMethod_Scrap_NextDummySearch[];
static const char str_PyMethod_Scrap_OpenPack[];
static const char str_PyMethod_Scrap_OpenStore[];
static const char str_PyMethod_Scrap_Preload3DObject[];
static const char str_PyMethod_Scrap_PreloadAnm[];
static const char str_PyMethod_Scrap_PreloadLibrary[];
static const char str_PyMethod_Scrap_Print[];
static const char str_PyMethod_Scrap_ProcessDVF[];
static const char str_PyMethod_Scrap_PythonCompileAll[];
static const char str_PyMethod_Scrap_Rand[];
static const char str_PyMethod_Scrap_Round[];
static const char str_PyMethod_Scrap_RunApp[];
static const char str_PyMethod_Scrap_SaveConfig[];
static const char str_PyMethod_Scrap_SaveGameVars[];
static const char str_PyMethod_Scrap_ScreenShot[];
static const char str_PyMethod_Scrap_SetAlarm[];
static const char str_PyMethod_Scrap_SetAlarmChar[];
static const char str_PyMethod_Scrap_SetAlarmGrow[];
static const char str_PyMethod_Scrap_SetCallFunc[];
static const char str_PyMethod_Scrap_SetCam[];
static const char str_PyMethod_Scrap_SetDebrisSys[];
static const char str_PyMethod_Scrap_SetDebrisValue[];
static const char str_PyMethod_Scrap_SetGrid[];
static const char str_PyMethod_Scrap_SetLanguage[];
static const char str_PyMethod_Scrap_SetLockAlarm[];
static const char str_PyMethod_Scrap_SetMoney[];
static const char str_PyMethod_Scrap_SetSaveProfile[];
static const char str_PyMethod_Scrap_SetSaveVar[];
static const char str_PyMethod_Scrap_SetTime[];
static const char str_PyMethod_Scrap_SetTimeSpeed[];
static const char str_PyMethod_Scrap_SetVideoCurrentMode[];
static const char str_PyMethod_Scrap_ShowGVars[];
static const char str_PyMethod_Scrap_SphereCall[];
static const char str_PyMethod_Scrap_StartDummySearch[];
static const char str_PyMethod_Scrap_SwitchMissionArrows[];
static const char str_PyMethod_Scrap_TestLine[];
static const char str_PyMethod_Scrap_TestSphere[];
static const char str_PyMethod_Scrap_UsrEntity[];
static const char str_PyMethod_Scrap_Ver[];
static const char str_PyMethod_Scrap_Verbose[];
static const char str_PyMethod_Scrap_VideoPlayer[];
static PyMethodDef Scrap_methods[116] = {
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
	{
		.ml_name = "ConsoleExec",
		.ml_meth = (void *)Scrap_ConsoleExec,
		.ml_flags = 1,
		.ml_doc = "ConsoleExec(line) : runs a line of the console",
	},
};

SD_PY_MODULE(Scrap, 0x42c610)

static const char str_PyMethod_Scrap_ScreenShot[] = "ScreenShot";
static const char str_PyMethod_Scrap_ProcessDVF[] = "ProcessDVF";
static const char str_PyMethod_Scrap_VideoPlayer[] = "VideoPlayer";
static const char str_PyMethod_Scrap_MusicPlayer[] = "MusicPlayer";
static const char str_PyMethod_Scrap_GetVideoCurrentMode[] = "GetVideoCurrentMode";
static const char str_PyMethod_Scrap_SetVideoCurrentMode[] = "SetVideoCurrentMode";
static const char str_PyMethod_Scrap_GetVideoModes[] = "GetVideoModes";
static const char str_PyMethod_Scrap_TestLine[] = "TestLine";
static const char str_PyMethod_Scrap_TestSphere[] = "TestSphere";
static const char str_PyMethod_Scrap_SphereCall[] = "SphereCall";
static const char str_PyMethod_Scrap_SetCallFunc[] = "SetCallFunc";
static const char str_PyMethod_Scrap_AddParamf[] = "AddParamf";
static const char str_PyMethod_Scrap_AddParami[] = "AddParami";
static const char str_PyMethod_Scrap_AddParams[] = "AddParams";
static const char str_PyMethod_Scrap_Execute[] = "Execute";
static const char str_PyMethod_Scrap_DelSaveVars[] = "DelSaveVars";
static const char str_PyMethod_Scrap_CreateSaveVar[] = "CreateSaveVar";
static const char str_PyMethod_Scrap_GetSaveVar[] = "GetSaveVar";
static const char str_PyMethod_Scrap_SetSaveVar[] = "SetSaveVar";
static const char str_PyMethod_Scrap_IncSaveVar[] = "IncSaveVar";
static const char str_PyMethod_Scrap_LoadGameVars[] = "LoadGameVars";
static const char str_PyMethod_Scrap_CreateSaveProfile[] = "CreateSaveProfile";
static const char str_PyMethod_Scrap_DeleteProfile[] = "DeleteProfile";
static const char str_PyMethod_Scrap_SetSaveProfile[] = "SetSaveProfile";
static const char str_PyMethod_Scrap_GetSavedProfilesList[] = "GetSavedProfilesList";
static const char str_PyMethod_Scrap_GetFreeBlocks[] = "GetFreeBlocks";
static const char str_PyMethod_Scrap_LaunchDashboard[] = "LaunchDashboard";
static const char str_PyMethod_Scrap_DeselectProfile[] = "DeselectProfile";
static const char str_PyMethod_Scrap_SaveGameVars[] = "SaveGameVars";
static const char str_PyMethod_Scrap_SaveConfig[] = "SaveConfig";
static const char str_PyMethod_Scrap_GetSaveGamesList[] = "GetSaveGamesList";
static const char str_PyMethod_Scrap_SetLanguage[] = "SetLanguage";
static const char str_PyMethod_Scrap_GetLanguage[] = "GetLanguage";
static const char str_PyMethod_Scrap_GetLangStr[] = "GetLangStr";
static const char str_PyMethod_Scrap_GetLangEntries[] = "GetLangEntries";
static const char str_PyMethod_Scrap_GetAlarm[] = "GetAlarm";
static const char str_PyMethod_Scrap_SetAlarm[] = "SetAlarm";
static const char str_PyMethod_Scrap_SetAlarmGrow[] = "SetAlarmGrow";
static const char str_PyMethod_Scrap_GetAlarmChars[] = "GetAlarmChars";
static const char str_PyMethod_Scrap_SetAlarmChar[] = "SetAlarmChar";
static const char str_PyMethod_Scrap_GetLockAlarm[] = "GetLockAlarm";
static const char str_PyMethod_Scrap_SetLockAlarm[] = "SetLockAlarm";
static const char str_PyMethod_Scrap_GetMoney[] = "GetMoney";
static const char str_PyMethod_Scrap_SetMoney[] = "SetMoney";
static const char str_PyMethod_Scrap_AddParkingZone[] = "AddParkingZone";
static const char str_PyMethod_Scrap_GetFarestParked[] = "GetFarestParked";
static const char str_PyMethod_Scrap_GetNearestParked[] = "GetNearestParked";
static const char str_PyMethod_Scrap_AddItem[] = "AddItem";
static const char str_PyMethod_Scrap_InitLoading[] = "InitLoading";
static const char str_PyMethod_Scrap_EndLoading[] = "EndLoading";
static const char str_PyMethod_Scrap_Exit[] = "Exit";
static const char str_PyMethod_Scrap_FileExist[] = "FileExist";
static const char str_PyMethod_Scrap_RunApp[] = "RunApp";
static const char str_PyMethod_Scrap_Achievement[] = "Achievement";
static const char str_PyMethod_Scrap_OpenStore[] = "OpenStore";
static const char str_PyMethod_Scrap_IsApp[] = "IsApp";
static const char str_PyMethod_Scrap_GetSteamID[] = "GetSteamID";
static const char str_PyMethod_Scrap_GetSteamTicket[] = "GetSteamTicket";
static const char str_PyMethod_Scrap_IsSteam[] = "IsSteam";
static const char str_PyMethod_Scrap_GetGogID[] = "GetGogID";
static const char str_PyMethod_Scrap_Print[] = "Print";
static const char str_PyMethod_Scrap_ConsoleOut[] = "ConsoleOut";
static const char str_PyMethod_Scrap_ConsoleError[] = "ConsoleError";
static const char str_PyMethod_Scrap_DebugInput[] = "DebugInput";
static const char str_PyMethod_Scrap_Verbose[] = "Verbose";
static const char str_PyMethod_Scrap_Ver[] = "Ver";

static char str_PyDoc_Scrap_EntityListGet[] =
	"(value)Scrap.EntityListGet(listName, varName) :  Obtiene el valor de una variable de una lista de en"
	"tidades";

static const char str_PyMethod_Scrap_ExtractPack[] = "ExtractPack";
static const char str_PyMethod_Scrap_OpenPack[] = "OpenPack";
static const char str_PyMethod_Scrap_ClosePack[] = "ClosePack";

static char str_PyDoc_Scrap_CreateEntityList[] = "CreateEntityList(listName) :  Crea una lista de entidades";

static const char str_PyMethod_Scrap_PreloadLibrary[] = "PreloadLibrary";

static char str_PyDoc_Scrap_SetCam[] = "SetCam(i,name) :  Activa una camara";

static const char str_PyMethod_Scrap_Preload3DObject[] = "Preload3DObject";

static char str_PyDoc_Scrap_GetCam[] = "name GetCam(i) :  Obtiene el nombre de una entidad camara";

static const char str_PyMethod_Scrap_PreloadAnm[] = "PreloadAnm";
static const char str_PyMethod_Scrap_PythonCompileAll[] = "PythonCompileAll";

static char str_PyDoc_Scrap_GetMinCamDist[] =
	"GetMinCamDist(x,y,z) :  Obtiene la distancia de un punto a la camara";

const char str_PyMethod_SScorer_Get[] = "Get";
static const char str_PyMethod_Scrap_Def[] = "Def";

static char str_PyDoc_Scrap_LoadLevel[] = "LoadLevel(string name) :  Carga un nivel en el siguiente frame";

static const char str_PyMethod_Scrap_Des[] = "Des";
const char str_PyMethod_SScorer_Set[] = "Set";

static char str_PyDoc_Scrap_GetLevelPath[] = "levelpath GetLevelPath() :  Obtiene el path del nivel actual";

static const char str_PyMethod_Scrap_ShowGVars[] = "ShowGVars";
static const char str_PyMethod_Scrap_SwitchMissionArrows[] = "SwitchMissionArrows";

static char str_PyDoc_Scrap_GetNewLevelPath[] =
	"levelpath GetNewLevelPath() :  Obtiene el path del próximo nivel";

static const char str_PyMethod_Scrap_GetTime[] = "GetTime";
static const char str_PyMethod_Scrap_GetTimeSpeed[] = "GetTimeSpeed";

static char str_PyDoc_Scrap_GetFloor[] = "i GetFloor(y) :  Obtiene el numero de piso en el que esta.";

static const char str_PyMethod_Scrap_SetTime[] = "SetTime";

static char str_PyDoc_Scrap_CreateElements[88] =
	"CreateElements() :  Crea una lista de elementos estaticos que pertenecen al mapa.";

static const char str_PyMethod_Scrap_SetTimeSpeed[] = "SetTimeSpeed";
static const char str_PyMethod_Scrap_AddScheduledFunc[] = "AddScheduledFunc";

static char str_PyDoc_Scrap_CallElements[] =
	"CallElements('Function') :  llama una funcion Function(Name,x,y,z,angx,angy)";

static const char str_PyMethod_Scrap_DeleteScheduledFuncs[] = "DeleteScheduledFuncs";
static const char str_PyMethod_Scrap_SetCam[] = "SetCam";

static char str_PyDoc_Scrap_SetGrid[] = "SetGrid(x,y,z) :  Modifica las dimensiones del grid de colision";

static const char str_PyMethod_Scrap_GetCam[] = "GetCam";
static const char str_PyMethod_Scrap_GetMinCamDist[] = "GetMinCamDist";

static char str_PyDoc_Scrap_GetNetFlags[] =
	"GetNetFlags() :  Obtiene la tupla de flags : client, server, dedicated";

static const char str_PyMethod_Scrap_GetEntity[] = "GetEntity";
static const char str_PyMethod_Scrap_GetFirst[] = "GetFirst";

static char str_PyDoc_Scrap_Rand[] = "Rand(min,max) :  Obtiene un numero seudo-aleatorio entre (min,max)";

static const char str_PyMethod_Scrap_CreateEntity[] = "CreateEntity";

static char str_PyDoc_Scrap_Round[] = "Round(num) : Redondea un numero real al entero más cercano.";

static const char str_PyMethod_Scrap_UsrEntity[] = "UsrEntity";
static const char str_PyMethod_Scrap_EntityListGet[] = "EntityListGet";

static char str_PyDoc_Scrap_NextDummySearch[] =
	"(s(ddd)(dd)i) NextDummySearch() : Obtiene el proximo dummy, sino None";

static const char str_PyMethod_Scrap_EntityListSet[] = "EntityListSet";
static const char str_PyMethod_Scrap_CreateEntityList[] = "CreateEntityList";

static char str_PyDoc_Scrap_StartDummySearch[] =
	"StartDummySearch(name,usewildcards) : Inicia la busqueda de dummies en el mapa";

static const char str_PyMethod_Scrap_LoadLevel[] = "LoadLevel";
static const char str_PyMethod_Scrap_GetLevelPath[] = "GetLevelPath";

static char str_PyDoc_Scrap_ModelInfo[] =
	"ModelInfo(name) : Muestra información sobre la jerarquía de nodos del modelo";

static const char str_PyMethod_Scrap_GetNewLevelPath[] = "GetNewLevelPath";

static char str_PyDoc_Scrap_ListModels[] = "ListModels() : Muestra una lista de modelos y escenas";

static const char str_PyMethod_Scrap_SetGrid[] = "SetGrid";
static const char str_PyMethod_Scrap_GetFloor[] = "GetFloor";

static char str_PyDoc_Scrap_BuildFont[] =
	"BuildFont(name,size) : Construye un archivo de fuente de letras a partir de un .TGA dado";

static const char str_PyMethod_Scrap_CreateElements[] = "CreateElements";
static const char str_PyMethod_Scrap_CallElements[] = "CallElements";

static char str_PyDoc_Scrap_ScreenShot[] = "ScreenShot(filename) : Screenshot of the current frame";

static const char str_PyMethod_Scrap_GetNetFlags[] = "GetNetFlags";
static const char str_PyMethod_Scrap_StartDummySearch[] = "StartDummySearch";

static char str_PyDoc_Scrap_ProcessDVF[] = "ProcessDVF(filename,command) : send a dvf command";

static const char str_PyMethod_Scrap_NextDummySearch[] = "NextDummySearch";

static char str_PyDoc_Scrap_VideoPlayer[] = "VideoPlayer(filename[,command]) : play video file";

static const char str_PyMethod_Scrap_Rand[] = "Rand";

static char str_PyDoc_Scrap_MusicPlayer[] = "MusicPlayer(filename[,command,param1]) : play music file";

static const char str_PyMethod_Scrap_Round[] = "Round";
static const char str_PyMethod_Scrap_SetDebrisSys[] = "SetDebrisSys";

static char str_PyDoc_Scrap_GetVideoCurrentMode[] = "GetVideoCurrentMode() : Get Video Current Mode Index";

static const char str_PyMethod_Scrap_DropDebris[] = "DropDebris";
static const char str_PyMethod_Scrap_SetDebrisValue[] = "SetDebrisValue";

static char str_PyDoc_Scrap_SetVideoCurrentMode[] = "SetVideoCurrentMode() : Set Video Current Mode Index";

static const char str_PyMethod_Scrap_ModelInfo[] = "ModelInfo";
static const char str_PyMethod_Scrap_ListModels[] = "ListModels";

static char str_PyDoc_Scrap_GetVideoModes[] = "GetVideoModes(NumMode) : Get Video Mode Info";

static const char str_PyMethod_Scrap_BuildFont[] = "BuildFont";

static char str_PyDoc_Scrap_SetDebrisSys[] =
	"SetDebrisSys(name,initialnumber,MaxDist,Friction,Scale,Grav,bounce)  crea/modifica un sistema de deb"
	"ris";

static char str_PyDoc_Scrap_DropDebris[] =
	"DropDebris(name,size) : Lanza un objeto que cae y rebota hasta que desaparece";

static char str_PyDoc_Scrap_SetDebrisValue[96] =
	"SetDebrisValue(id,x,y,z) : id = 0:Posicion, 1:Angulos, 2:Velocidad, 3:Velocidad de rotacion";

static char str_PyDoc_Scrap_TestLine[] =
	"((x,y,z),s) TestLine((x,y,z),(fz,fy,fz),'strmask') : testea una linea y devuelve el nombre de la ent"
	"idad y punto de colision  o '' si es el mapa o None";

static char str_PyDoc_Scrap_TestSphere[128] =
	"Scrap.TestSphere(x,y,z,radius,´strmask´) : testea una esfera y devuelve el nombre de la entidad o ''"
	" si es el mapa o None";

static char str_PyDoc_Scrap_SphereCall[] =
	"Scrap.SphereCall(x,y,z,radius,´strmask´,´callback´,[IgnoreGeometry=1]) : testea una esfera y llama a"
	" ´callback´ por cada entidad que colisiona";

static char str_PyDoc_Scrap_SetCallFunc[] =
	"Scrap.SetCallFunc('!funcname') : Especifica la funcion callback (c++) a llamar. (1 si existe)";

static char str_PyDoc_Scrap_AddParamf[80] =
	"Scrap.AddParamf(Float) : Incluye un parametro a una funcion callback (c++).";

static char str_PyDoc_Scrap_AddParami[80] =
	"Scrap.AddParami(int) : Incluye un parametro a una funcion callback (c++).";

static char str_PyDoc_Scrap_AddParams[] =
	"Scrap.AddParams(string) : Incluye un parametro a una funcion callback (c++).";

static char str_PyDoc_Scrap_Execute[] = "Scrap.Execute() : Ejecuta una funcion callback (c++).";

static char str_PyDoc_Scrap_DelSaveVars[] = "Scrap.DelSaveVars(str) : Elimina las <str>* variables";

static char str_PyDoc_Scrap_CreateSaveVar[] =
	"bool Scrap.CreateSaveVar(Name,DefValue) : Crea una nueva variable (si puede)";

static char str_PyDoc_Scrap_SetSaveVar[] =
	"bool Scrap.SetSaveVar(Name,DefValue) : Modifica una variable (si puede)";

static char str_PyDoc_Scrap_GetSaveVar[72] =
	"value Scrap.GetSaveVar(Name) : obtiene una variable o devuelve none";

static char str_PyDoc_Scrap_SaveGameVars[88] =
	"Scrap.SaveGameVars(str, str) : Salva un juego en un archivo, con un nombre opcional";

static char str_PyDoc_Scrap_IncSaveVar[] =
	"int Scrap.IncSaveVar(str[,value]) : incrementa un contador de estadisticas";

static char str_PyDoc_Scrap_LoadGameVars[60] = "Scrap.LoadGameVars(str) : Carga un juego en un archivo";

static char str_PyDoc_Scrap_GetSavedProfilesList[] =
	"[(Directorio, nombre)] Scrap.GetSavedProfilesList() : Devuelve una lista de tuplas donde para cada t"
	"upla el primer elemento indica el directorio del profile y el segundo elemento indica el nombre del "
	"profile. La lista está ordenada por el criterio de microsoft ";

static char str_PyDoc_Scrap_CreateSaveProfile[] =
	"int Scrap.CreateSaveProfile(str) : Crea un Save profile para X-Box y pone como actual.";

static char str_PyDoc_Scrap_DeleteProfile[64] = "int Scrap.DeleteProfile(str) : Borra un profile de X-Box.";

static char str_PyDoc_Scrap_GetFreeBlocks[] =
	"int Scrap.GetFreeBlocks() : obtiene los bloques libres en el disco duro.";

static char str_PyDoc_Scrap_DeselectProfile[] = "Scrap.DeselectProfile() : deselecciona el profile actual.";

static char str_PyDoc_Scrap_LaunchDashboard[] =
	"int Scrap.LaunchDashboard() : sale del juego y ejecuta el dashboard.";

static char str_PyDoc_Scrap_SetSaveProfile[] =
	"int Scrap.SetSaveProfile(path,name) : Carga un profile de jugador.";

static char str_PyDoc_Scrap_DebugInput[68] = "string DebugInput() :  Detiene todo para iniciar la depuracion";

static char str_PyDoc_Scrap_GetSaveGamesList[296] =
	" [(fecha, slot)] Scrap.GetSaveGamesList() : Devuelve una lista de tuplas donde para cada tupla el pr"
	"imer elemento indica el nombre (fecha) del archivo y el segundo elemento indica el slot del que se l"
	"eerá la partida. La lista está ordenada por la fecha, de la más reciente a la más antigua.";

static char str_PyDoc_Scrap_Print[] = "Print(string name) :  Muestra un mensaje por la consola siempre";

static char str_PyDoc_Scrap_SaveConfig[64] = "res Scrap.SaveConfig() : Guarda el fichero de configuración";

static char str_PyDoc_Scrap_ConsoleOut[72] =
	"ConsoleOut(string name) :  Muestra un mensaje pyton por la consola";

static char str_PyDoc_Scrap_SetLanguage[120] =
	"res Scrap.SetLanguage(Name[,forcetoreload]) : Modifica la lengua actual. false si ya es la lengua la"
	" selecionada";

static char str_PyDoc_Scrap_ConsoleError[] =
	"ConsoleError(string name) :  Muestra un mensaje de error por la consola";

static char str_PyDoc_Scrap_Verbose[] =
	"Verbose(string name) :  Muestra un mensaje de parloteo por la consola";

static char str_PyDoc_Scrap_GetLanguage[] =
	"lang Scrap.GetLanguage() : Obtiene la lengua actual. None si no fue inicializada.";

static char str_PyDoc_Scrap_Exit[28] = "Exit() sale del juego.";

static char str_PyDoc_Scrap_GetLangStr[] =
	"str Scrap.GetLangStr(Name) : Obtiene una cadena de lenguaje. '' si no existe.";

static char str_PyDoc_Scrap_FileExist[] = "FileExist() check for file existence.";

static char str_PyDoc_Scrap_GetLangEntries[] =
	"[str] Scrap.GetLangEntries(Name) : Obtiene un subconjunto de entradas de la tabla de lenguaje que em"
	"piecen por 'Name'.";

static char str_PyDoc_Scrap_Ver[40] = "Ver() Obtiene la version del juego.";

static char str_PyDoc_Scrap_GetAlarm[] =
	"(active,dialpos,growcode) Scrap.GetAlarm() : Obtiene informacion de la alarma.";

static char str_PyDoc_Scrap_ExtractPack[64] = "ExtractPack(string PackPath) :  Extrae un archivo *.packed";

static char str_PyDoc_Scrap_SetAlarm[] =
	"Scrap.SetAlarm(dialpos) : pone la alarma en una posicion (0 desactiva, 1 activa).";

static char str_PyDoc_Scrap_OpenPack[] = "OpenPack(string PackPath) :  Abre un archivo *.packed";

static char str_PyDoc_Scrap_SetAlarmGrow[] = "Scrap.SetAlarmGrow(growcode) : -1 baja, 0 se mantiene, 1 sube.";

static char str_PyDoc_Scrap_ClosePack[44] = "ClosePack() :  Abre un archivo *.packed";

static char str_PyDoc_Scrap_GetAlarmChars[] =
	"(Actual, El_que_se_busca) Scrap.GetAlarmChars() : Obtiene los tipos de personaje que se buscan.";

static char str_PyDoc_Scrap_PreloadLibrary[96] =
	"PreloadLibrary(LibraryName,CompiledFile) :  Precarga una libreria de un archivo empaquetado";

static char str_PyDoc_Scrap_SetAlarmChar[] =
	"Scrap.SetAlarmChar(El_que_se_busca) : Modifica el personaje que se busca.";

static char str_PyDoc_Scrap_SetLockAlarm[60] = "Scrap.SetLockAlarm(1/0) : Bloquea/desbloquea la alarma";

static char str_PyDoc_Scrap_Preload3DObject[] =
	"Preload3DObject(filename,scalex,scaley,scalez) :  Precarga un objeto 3D";

static char str_PyDoc_Scrap_GetLockAlarm[] = "Scrap.GetLockAlarm() : verdadero si la alarma esta bloqueada";

static char str_PyDoc_Scrap_PreloadAnm[] = "PreloadAnm(ObjFilename,AnmFilename) :  Precarga una animacion";

static char str_PyDoc_Scrap_GetMoney[] = "money Scrap.GetMoney() : Devuelve el liquido disponible.";

static char str_PyDoc_Scrap_PythonCompileAll[] =
	"PythonCompileAll() :  compila recursivamente los archivos .py";

static char str_PyDoc_Scrap_SetMoney[60] = "Scrap.SetMoney(money) : Determina el liquido disponible";

static char str_PyDoc_Scrap_ShowGVars[72] = "ShowGVars() :  Muestra una lista de todas las variables globales";

static char str_PyDoc_Scrap_AddParkingZone[] =
	"Scrap.AddParkingZone(occupied,pos,name) : Agrega un sitio de parking.";

static char str_PyDoc_Scrap_GetFarestParked[] =
	"Entity,Pos =  Scrap.GetFarestParked([isparked[,fromPos]]) : Obtiene la nave aparcada mas lejana.";

static char str_PyDoc_Scrap_Get[] = "Get('GlobalVar') :  Obtiene el valor de una variable global";

static char str_PyDoc_Scrap_GetNearestParked[104] =
	"Entity,Pos = Scrap.GetNearestParked([isparked[,fromPos]]) : Obtiene la nave aparcada mas cercana.";

static char str_PyDoc_Scrap_Def[] = "Def('GlobalVar') :  Obtiene el valor por defecto de una variable global";

static char str_PyDoc_Scrap_Des[72] = "Des('GlobalVar') :  Obtiene el descriptor de una variable global";

static char str_PyDoc_Scrap_AddItem[] =
	"Scrap.AddItem(Life, string name, int life) : Add Item Life.\n"
	" Scrap.AddItem(Ammo, string name, int typeAmmo, int ammo) : Add Item Ammo.";

static char str_PyDoc_Scrap_SwitchMissionArrows[] =
	"SwitchMissionArrows(MainMissionFile,TargetMissionFile) : Cambia las flechas de mision primaria y sec"
	"undaria (Modelos)";

static char str_PyDoc_Scrap_InitLoading[56] = "InitLoading() Inicia la pantalla de carga rapida.";

static char str_PyDoc_Scrap_Set[] = "Set('GlobalVar',val) :  Modifica el valor de una variable global";

static char str_PyDoc_Scrap_EndLoading[] = "EndLoading() Inicia la pantalla de carga rapida.";

static char str_PyDoc_Scrap_GetTime[] = "GetTime() :  Obtiene el tiempo del mundo en segundos";

static char str_PyDoc_Scrap_RunApp[] = "RunApp() Ejecuta un programa externo.";

static char str_PyDoc_Scrap_GetTimeSpeed[] = "GetTimeSpeed() :  Obtiene la velocidad del tiempo del mundo.";

static char str_PyDoc_Scrap_Achievement[] = "Achievement() marca un logro como consegido";

static char str_PyDoc_Scrap_SetTime[] = "SetTime(Time) :  Cambia el tiempo del mundo en segundos";

static char str_PyDoc_Scrap_OpenStore[] = "OpenStore() abre la store en una apicacion";

static char str_PyDoc_Scrap_SetTimeSpeed[] =
	"SetTimeSpeed(TimeSpeed) :  Cambia la velocidad del tiempo del mundo.";

static char str_PyDoc_Scrap_IsApp[] = "IsApp() si tienes una apicacion";

static char str_PyDoc_Scrap_AddScheduledFunc[] =
	"AddScheduledFunc(time,func,params[,name]) : Ejecuta un codigo en python en un instante de tiempo.";

static char str_PyDoc_Scrap_GetSteamID[] = "GetSteamID() id de steam";

static char str_PyDoc_Scrap_DeleteScheduledFuncs[] =
	"DeleteScheduledFuncs(name) : Borra las scheduled funcs creadas con un nombre dado.";

static char str_PyDoc_Scrap_IsSteam[] = "IsSteam() Es steam";

static char str_PyDoc_Scrap_GetEntity[] = "GetEntity(string name) :  Retorna una entidad";

static char str_PyDoc_Scrap_GetSteamTicket[] = "GetSteamTicket() ticket de autenticacion de steam";

static char str_PyDoc_Scrap_GetGogID[] = "GetGogID() id de gog";

static char str_PyDoc_Scrap_GetFirst[68] = "GetFirst() :  Retorna la primera entuidad de la lista o none";

static char str_PyDoc_Scrap_UsrEntity[] =
	"UsrEntity(ictr) :  Retorna una entidad controlada por usario (personaje o nave)";

static char str_PyDoc_Scrap_CreateEntity[] = "CreateEntity(name,x,y,z,type) :  Crea una una entidad";

static char str_PyDoc_Scrap_EntityListSet[] =
	"EntityListSet(listName, varName, value) :  Modifica el valor de una variable de una lista de entidad"
	"es";

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_CALL(Fini_cPy_Function_CallFunc, &g_CallFunc, cPy_Function_dtor)
INIT_OBJECT(Init_cPy_Function_CallFunc, Fini_cPy_Function_CallFunc, &g_CallFunc, cPy_Function_ctor)

PORTS(PyInterface_ScrapPy)
