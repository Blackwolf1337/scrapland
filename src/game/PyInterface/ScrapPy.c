/*
 * Python module Scrap (function table Scrap_methods): engine functions for the game scripts.
 * Original file: PyInterface\\ScrapPy.cpp
 */
#include <math.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/nativeobj.h"
#include "scrap/scorer.h"
#include "scrap/players.h"
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

/* AddScheduledFunc(time,func,params[,name]) : Ejecuta un codigo en python en un instante de tiempo. The scheduler
 * keeps the function and the arguments as the values given */
API_FN(Scrap, AddScheduledFunc) {
	double time;
	const nativeVal_t *func, *params;
	const char *name = "";
	if (!Mode_Parse(a, n, "dOO|s", &time, &func, &params, &name)) {
		return 0;
	}
	Scheduler_Add(&g_pWorld->scheduler, (float)time, func, params, name);
	return 1;
}

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
	SaveVars_ScriptDelete(prefix);
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
API_FN(Scrap, GetLangEntries) {
	const char *name;
	int count = 3;
	if (!Mode_Parse(a, n, "s|i", &name, &count)) {
		return 0;
	}
	char ids[LANG_ENTRIES][LANG_ID];
	count = Lang_GetEntries(name, count, ids);
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0; i < count; i++) {
		Native_SetStr(Native_Append(out), ids[i]);
	}
	return 1;
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
API_FN(Scrap, GetSavedProfilesList) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : GetSavedProfilesList not supported on PC\n", 0);
	Native_SetEmpty(out, NV_LIST);
	return 1;
}

/* GetSaveGamesList(path) : [(description, slot), ...] */
API_FN(Scrap, GetSaveGamesList) {
	const char *path;
	if (!Mode_Parse(a, n, "s", &path)) {
		return 0;
	}
	cSaveGameInfo saves[SAVE_SLOTS];
	int count = SaveGames_GetList(path, saves);
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0; i < count; i++) {
		nativeVal_t *t = Native_Append(out);
		Native_SetEmpty(t, NV_TUPLE);
		Native_SetStr(Native_Append(t), saves[i].desc);
		Native_SetInt(Native_Append(t), saves[i].slot);
	}
	return 1;
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

/* SetTime(t) : returns modsupport.c BuildValue("") (None) like the original */
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
	cEntity *e = Players_Script(player);
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

/* the callback of the video of Scrap.VideoPlayer (a copy of the value), until it ends */
static nativeVal_t videoPlayerDone;

static void VideoPlayer_Done(int result) {
	nativeVal_t f = videoPlayerDone, r;
	Native_SetNone(&videoPlayerDone);
	Native_SetInt(&r, result);
	if (!Port_CallKept(NULL, &f, &r, 1, NULL)) {
		Native_ErrPrint();
	}
	Native_Free(&f);
}

/*
 * VideoPlayer(name[, cmd[, done]]) : cmd default "play". differs from the original on purpose (no blocking waits):
 * it returns at once, the video plays on and the frame waits for it; done(result) is called when it ends (an
 * addition: 0 not found, 1 played to the end or not opened, 2 stopped by a key). done is kept as the value given;
 * for "a|b" the video keeps its own copy of b until it plays
 */
API_FN(Scrap, VideoPlayer) {
	const char *name, *cmd = "play";
	const nativeVal_t *done = NULL;
	if (!Mode_Parse(a, n, "s|sO", &name, &cmd, &done)) {
		return 0;
	}
	if (done && done->type != NV_NONE && !NObj_Callable(done)) {
		Con_Warnf("VideoPlayer: done is not callable, ignored\n");
		done = NULL;
	}
	if (!done || done->type == NV_NONE || crt_stricmp(cmd, "play")) {
		Video_Play(name, cmd, 0.0f);
		return 1;
	}
	Native_Copy(&videoPlayerDone, done);
	Video_PlayThen(name, VideoPlayer_Done);
	return 1;
}

/* --- batch 4: engine variables, entities, files, random numbers, Steam apps --- */

/* Get(name) / Def(name) : current or default value of an engine variable in its type; NULL without exception for an
 * unknown name (golden records it): Native_GVarGet and Native_GVarDef */
API_FN(Scrap, Get) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Native_GVarGet(name, out);
}

API_FN(Scrap, Def) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Native_GVarDef(name, out);
}

/*
 * Set(name, value) : string ("ss") for string and script function variables, number ("sd") for int and float
 * ones. A read only variable gives 0 and a message; a wrong type or an unknown name NULL without exception (golden
 * records both). The value is read as "ss", else "sd" read it (Native_GVarStore)
 */
API_FN(Scrap, Set) {
	const char *name;
	const nativeVal_t *v;
	if (!Mode_Parse(a, n, "sO", &name, &v)) {
		return 0;
	}
	int r = Native_GVarStore(name, v);
	return r >= 0 && Api_Build(out, "i", r);
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
API_FN(Scrap, Rand) {
	double da, db;
	return Mode_Parse(a, n, "dd", &da, &db) && Api_Build(out, "d", Scrap_RandRange(da, db));
}

/* FileExist(name) : looks in the packs and on disk */
API_FN(Scrap, FileExist) {
	const char *name;
	cFilePack pack;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cFilePack_ctor(&pack);
	Api_Build(out, "i", cFilePack_Exists(&pack.file.base, name));
	cFilePack_dtor(&pack);
	return 1;
}

/* GetEntity(name) : the entity handle, None if there is no such entity */
API_FN(Scrap, GetEntity) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (World_FindEntity(name)) {
		return Api_Handle(name, out);
	}
	return 1;
}

/* CreateEntity(className, x, y, z, name) : the handle of the new entity, None if it was not made */
API_FN(Scrap, CreateEntity) {
	const char *className, *name;
	double x, y, z;
	if (!Mode_Parse(a, n, "sddds", &className, &x, &y, &z, &name)) {
		return 0;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, className, (float)x, (float)y, (float)z, name);
	if (!e) {
		return 1;
	}
	return Api_Handle(e->name, out);
}

/* IsApp(appid) : NULL for appid < 0, else 1, as the original without Steam (no store features; with Steam it asked
 * whether the app is owned and installed) */
API_FN(Scrap, IsApp) {
	int appId;
	return Mode_Parse(a, n, "i", &appId) && appId >= 0 && Api_Build(out, "i", 1);
}

/* RunApp(exe, cmdLine) : starts a program; "" for the game itself */
API_FN(Scrap, RunApp) {
	const char *exe, *cmdLine;
	char path[128];
	if (!Mode_Parse(a, n, "ss", &exe, &cmdLine)) {
		return 0;
	}
	if (!*exe) {
		App_GetPath(path, sizeof(path));
		exe = path;
	}
	App_RunApp(g_pApp, exe, cmdLine, 0);
	return 1;
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

API_FN(Scrap, EntityListGet) {
	const char *listName, *var;
	char *s;
	double d;
	int i;
	if (!Mode_Parse(a, n, "ss", &listName, &var)) {
		return 0;
	}
	switch (EntityList_Get(listName, var, &s, &d, &i)) {
	case -1:
		return 0;
	case 1:
		return Api_Build(out, "s", s);
	case 2:
		return Api_Build(out, "d", d);
	case 3:
		return Api_Build(out, "i", i);
	}
	return Native_Raise(NE_ATTRIBUTE, "EntityListGet: list '%.64s' has no variable '%.64s'", listName, var);
}

/* EntityListSet(listName, var, value) : NULL without exception for a missing list (golden records it) */
API_FN(Scrap, EntityListSet) {
	const char *listName, *var, *value;
	if (!Mode_Parse(a, n, "sss", &listName, &var, &value)) {
		return 0;
	}
	void *list = cWorld_GetList(g_pWorld, listName);
	if (!list) {
		Con_Errorf("ERROR: EntityListSet '%s','%s','%s' list not found.\n", listName, var, value);
		return 0;
	}
	/* the setter only reads the text */
	char *text = (char *)value;
	return Api_Build(out, "i", cEntityList_Var(list, var, 1, &text, NULL, NULL));
}

/* GetMinCamDist(x, y, z) : sqrt in double of the squared distance, then float */
API_FN(Scrap, GetMinCamDist) {
	vec3_t pos;
	if (!Mode_Parse(a, n, "fff", &pos.x, &pos.y, &pos.z)) {
		return 0;
	}
	double d = (double)cWorld_GetMinCamDistSq(g_pWorld, &pos);
	return Api_Build(out, "d", (double)(float)sqrt(d));
}

/* GetVideoModes(i) : the number of modes for i < 0, else the name of mode i */
API_FN(Scrap, GetVideoModes) {
	int i = 0;
	char buf[256];
	if (!Mode_Parse(a, n, "i", &i)) {
		return 0;
	}
	int count = Video_GetModes(i, buf);
	if (i < 0) {
		return Api_Build(out, "i", count);
	}
	return Api_Build(out, "s", buf);
}

/* SetGrid(x, y, z) */
API_FN(Scrap, SetGrid) {
	double x, y, z;
	if (!Mode_Parse(a, n, "ddd", &x, &y, &z)) {
		return 0;
	}
	vec3_t size = { (float)x, (float)y, (float)z };
	Grid_Set(&g_pWorld->grid, &size);
	return Api_Build(out, "i", 1);
}

/* SetSaveProfile(a, b) : not on PC */
API_FN(Scrap, SetSaveProfile) {
	const char *first, *second;
	if (!Mode_Parse(a, n, "ss", &first, &second)) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, "ERROR : ", 0);
	Con_Print(CON_COLOR_ERROR, first, 0);
	Con_Print(CON_COLOR_ERROR, " , ", 0);
	Con_Print(CON_COLOR_ERROR, second, 0);
	Con_Print(CON_COLOR_ERROR, " : Player Profile system not supported on PC\n", 0);
	return Api_Build(out, "i", 1);
}

/* Preload3DObject(name[, sx, sy, sz]) : scale default 1 */
API_FN(Scrap, Preload3DObject) {
	const char *name;
	double sx = 1.0, sy = 1.0, sz = 1.0;
	if (!Mode_Parse(a, n, "s|ddd", &name, &sx, &sy, &sz)) {
		return 0;
	}
	vec3_t scale = { (float)sx, (float)sy, (float)sz };
	Object3D_Preload(name, &scale);
	return 1;
}

/* AddParkingZone(flag, (x,y,z), name) */
API_FN(Scrap, AddParkingZone) {
	int flag;
	float x, y, z;
	const char *name;
	if (!Mode_Parse(a, n, "i(fff)s", &flag, &x, &y, &z, &name)) {
		return 0;
	}
	vec3_t pos = { x, y, z };
	Parking_AddZone(&g_pWorld->logic.parking, flag != 0, &pos, name);
	return 1;
}

/* GetNearestParked / GetFarestParked([flag[, (x,y,z)]]) : (name, (x, y, z)); default position: current camera */
static byte FindParked(const nativeVal_t *a, int n, nativeVal_t *out, int farest) {
	vec3_t pos = g_pWorld->camPos[g_pWorld->curCam];
	int flag = 1;

	if (!Mode_Parse(a, n, "|i(fff)", &flag, &pos.x, &pos.y, &pos.z)) {
		return 0;
	}
	vec3_t found = pos;
	const char *name = Parking_Find(&g_pWorld->logic.parking, flag != 0, &pos, &found, farest);
	if (!name) {
		return Api_Build(out, "s(fff)", "", 0.0, 0.0, 0.0);
	}
	return Api_Build(out, "s(fff)", name, (double)found.x, (double)found.y, (double)found.z);
}

API_FN(Scrap, GetNearestParked) {
	return FindParked(a, n, out, 0);
}

API_FN(Scrap, GetFarestParked) {
	return FindParked(a, n, out, 1);
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

API_FN(Scrap, TestLine) {
	double x1, y1, z1, x2, y2, z2;
	const char *listName;
	if (!Mode_Parse(a, n, "(ddd)(ddd)s", &x1, &y1, &z1, &x2, &y2, &z2, &listName)) {
		return 0;
	}
	cEntity *target;
	vec3_t from = { (float)x1, (float)y1, (float)z1 };
	vec3_t to = { (float)x2, (float)y2, (float)z2 };
	vec3_t hit;
	if (!World_TestLine(&from, &to, listName, &hit, &target)) {
		return 1;
	}
	return Api_Build(out, "(ddd)s", (double)hit.x, (double)hit.y, (double)hit.z, target ? target->name : "");
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

API_FN(Scrap, TestSphere) {
	double x, y, z, r;
	const char *listName, *excludeName = "";
	if (!Mode_Parse(a, n, "dddds|s", &x, &y, &z, &r, &listName, &excludeName)) {
		return 0;
	}
	vec3_t c = { (float)x, (float)y, (float)z };
	byte levelHit;
	cEntity *target = World_TestSphere(&c, (float)r, listName, excludeName, &levelHit);
	if (target) {
		return Api_Build(out, "s", target->name);
	}
	if (levelHit) {
		return Api_Build(out, "s", "");
	}
	return 1;
}

/* SphereCall(x, y, z, radius, listName, func[, flag]) or ((x,y,z), radius, ...) : func for the entities inside */
API_FN(Scrap, SphereCall) {
	double x, y, z, r;
	const char *listName, *func;
	int flag = 1;
	if (!Mode_Parse(a, n, "ddddss|i", &x, &y, &z, &r, &listName, &func, &flag)) {
		Native_ErrClear();
		if (!Mode_Parse(a, n, "(ddd)dss|i", &x, &y, &z, &r, &listName, &func, &flag)) {
			return 0;
		}
	}
	vec3_t c = { (float)x, (float)y, (float)z };
	return Api_Build(out, "i", cWorld_SphereCall(g_pWorld, EntityLists_Get(listName), &c, (float)r, func,
		flag != 0) != 0);
}

/* NextDummySearch() : (name, (x, y, z), (angx, angy) degrees, flag) of the next dummy, None at the end */
API_FN(Scrap, NextDummySearch) {
	char name[0x40];
	vec3_t pos;
	float ang[2];
	byte flag;
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	if (!DummySearch_Next(name, sizeof(name), &pos, ang, &flag)) {
		return 1;
	}
	return Api_Build(out, "s(ddd)(dd)i", name, (double)pos.x, (double)pos.y, (double)pos.z,
		(double)ang[0] * DEG_PER_PI / M_PI_D, (double)ang[1] * DEG_PER_PI / M_PI_D, flag != 0);
}

/* SetDebrisSys(name[, n, radius, b, c, d, flag, e]) : n <= 0 deletes the system */
API_FN(Scrap, SetDebrisSys) {
	const char *name;
	int count = 4, flag = 0, e = 0;
	double radius = 10000.0, b = 1.0, c = 1.0, d = (double)g_DebrisDefault;
	if (!Mode_Parse(a, n, "s|iddddii", &name, &count, &radius, &b, &c, &d, &flag, &e)) {
		return 0;
	}
	if (count <= 0) {
		cWorld_DeleteDebrisSys(g_pWorld, name);
		return 1;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, name);
	sys->b = (float)b;
	sys->d = (float)d;
	float rf = (float)radius;
	sys->radiusSq = rf * rf;
	sys->c = (float)c;
	sys->flag = flag != 0;
	cDebrisSys_Init(sys, count, e);
	return 1;
}

/* DropDebris(name, speed) : a debris element with the values of SetDebrisValue */
API_FN(Scrap, DropDebris) {
	const char *name;
	double speed;
	if (!Mode_Parse(a, n, "sd", &name, &speed)) {
		return 0;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, name);
	cDebris *deb = cDebrisSys_NewElement(sys);
	for (int i = 0; i < 4; i++) {
		deb->drop[i] = g_DebrisDrop[i];
	}
	deb->life = (float)speed;
	cDebrisSys_Drop(sys, deb);
	return 1;
}

/* SetDebrisValue(n, x, y, z) : 0 position, 1 angles (degrees), 2 velocity, 3 angle speed (degrees) */
API_FN(Scrap, SetDebrisValue) {
	int which;
	double x, y, z;
	if (!Mode_Parse(a, n, "iddd", &which, &x, &y, &z)) {
		return 0;
	}
	switch (which) {
	case 0:
	case 2:
		g_DebrisDrop[which] = (vec3_t){ (float)x, (float)y, (float)z };
		break;
	case 1:
	case 3:
		g_DebrisDrop[which] = (vec3_t){ (float)(x * M_PI_D / DEG_PER_PI), (float)(y * M_PI_D / DEG_PER_PI),
			(float)(z * M_PI_D / DEG_PER_PI) };
		break;
	default:
		Con_Warnf("WARNING SetDebrisValue: %d not found\n", which);
		break;
	}
	return 1;
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

/* the type is the text of the first argument up to its first 0 byte (stringobject.c String_AsString), the formats after
 * it. differs from the original: without arguments, or with a first one that is no text, the adapter gave strcmp a NULL
 * (a crash); the entry raises the error tupleobject.c Tuple_GetItem or stringobject.c String_AsString had set */
API_FN(Scrap, AddItem) {
	const char *type, *name, *kindName;
	int value;
	if (n < 1) {
		return Native_Raise(NE_INDEX, "tuple index out of range");
	}
	if (a[0].type != NV_STR) {
		return Native_Raise(NE_SYSTEM, "bad argument to internal function");
	}
	type = Native_Text(&a[0]);
	if (!strcmp(type, "Life")) {
		if (!Mode_Parse(a, n, "ssi", &type, &name, &value)) {
			return 0;
		}
		Logic_AddItem(type, name, NULL, value);
	}
	else if (!strcmp(type, "Ammo")) {
		if (!Mode_Parse(a, n, "sssi", &type, &name, &kindName, &value)) {
			return 0;
		}
		Logic_AddItem(type, name, kindName, value);
	}
	return 1;
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
static const portFn_t Scrap_methods[] = {
	PORT_FND(Print, Scrap_Print, str_PyDoc_Scrap_Print),
	PORT_FND(ConsoleOut, Scrap_ConsoleOut, str_PyDoc_Scrap_ConsoleOut),
	PORT_FND(ConsoleError, Scrap_ConsoleError, str_PyDoc_Scrap_ConsoleError),
	PORT_FND(DebugInput, Scrap_DebugInput, str_PyDoc_Scrap_DebugInput),
	PORT_FND(Verbose, Scrap_Verbose, str_PyDoc_Scrap_Verbose),
	PORT_FND(Ver, Scrap_Ver, str_PyDoc_Scrap_Ver),
	PORT_FND(ExtractPack, Scrap_ExtractPack, str_PyDoc_Scrap_ExtractPack),
	PORT_FND(OpenPack, Scrap_OpenPack, str_PyDoc_Scrap_OpenPack),
	PORT_FND(ClosePack, Scrap_ClosePack, str_PyDoc_Scrap_ClosePack),
	PORT_FND(PreloadLibrary, Scrap_PreloadLibrary, str_PyDoc_Scrap_PreloadLibrary),
	PORT_FND(Preload3DObject, Scrap_Preload3DObject, str_PyDoc_Scrap_Preload3DObject),
	PORT_FND(PreloadAnm, Scrap_PreloadAnm, str_PyDoc_Scrap_PreloadAnm),
	PORT_FND(PythonCompileAll, Scrap_PythonCompileAll, str_PyDoc_Scrap_PythonCompileAll),
	PORT_FND(Get, Scrap_Get, str_PyDoc_Scrap_Get),
	PORT_FND(Def, Scrap_Def, str_PyDoc_Scrap_Def),
	PORT_FND(Des, Scrap_Des, str_PyDoc_Scrap_Des),
	PORT_FND(Set, Scrap_Set, str_PyDoc_Scrap_Set),
	PORT_FND(ShowGVars, Scrap_ShowGVars, str_PyDoc_Scrap_ShowGVars),
	PORT_FND(SwitchMissionArrows, Scrap_SwitchMissionArrows, str_PyDoc_Scrap_SwitchMissionArrows),
	PORT_FND(GetTime, Scrap_GetTime, str_PyDoc_Scrap_GetTime),
	PORT_FND(GetTimeSpeed, Scrap_GetTimeSpeed, str_PyDoc_Scrap_GetTimeSpeed),
	PORT_FND(SetTime, Scrap_SetTime, str_PyDoc_Scrap_SetTime),
	PORT_FND(SetTimeSpeed, Scrap_SetTimeSpeed, str_PyDoc_Scrap_SetTimeSpeed),
	PORT_FND(AddScheduledFunc, Scrap_AddScheduledFunc, str_PyDoc_Scrap_AddScheduledFunc),
	PORT_FND(DeleteScheduledFuncs, Scrap_DeleteScheduledFuncs, str_PyDoc_Scrap_DeleteScheduledFuncs),
	PORT_FND(SetCam, Scrap_SetCam, str_PyDoc_Scrap_SetCam),
	PORT_FND(GetCam, Scrap_GetCam, str_PyDoc_Scrap_GetCam),
	PORT_FND(GetMinCamDist, Scrap_GetMinCamDist, str_PyDoc_Scrap_GetMinCamDist),
	PORT_FND(GetEntity, Scrap_GetEntity, str_PyDoc_Scrap_GetEntity),
	PORT_FND(GetFirst, Scrap_GetFirst, str_PyDoc_Scrap_GetFirst),
	PORT_FND(CreateEntity, Scrap_CreateEntity, str_PyDoc_Scrap_CreateEntity),
	PORT_FND(UsrEntity, Scrap_UsrEntity, str_PyDoc_Scrap_UsrEntity),
	PORT_FND(EntityListGet, Scrap_EntityListGet, str_PyDoc_Scrap_EntityListGet),
	PORT_FND(EntityListSet, Scrap_EntityListSet, str_PyDoc_Scrap_EntityListSet),
	PORT_FND(CreateEntityList, Scrap_CreateEntityList, str_PyDoc_Scrap_CreateEntityList),
	PORT_FND(LoadLevel, Scrap_LoadLevel, str_PyDoc_Scrap_LoadLevel),
	PORT_FND(GetLevelPath, Scrap_GetLevelPath, str_PyDoc_Scrap_GetLevelPath),
	PORT_FND(GetNewLevelPath, Scrap_GetNewLevelPath, str_PyDoc_Scrap_GetNewLevelPath),
	PORT_FND(SetGrid, Scrap_SetGrid, str_PyDoc_Scrap_SetGrid),
	PORT_FND(GetFloor, Scrap_GetFloor, str_PyDoc_Scrap_GetFloor),
	PORT_FND(CreateElements, Scrap_CreateElements, str_PyDoc_Scrap_CreateElements),
	PORT_FND(CallElements, Scrap_CallElements, str_PyDoc_Scrap_CallElements),
	PORT_FND(GetNetFlags, Scrap_GetNetFlags, str_PyDoc_Scrap_GetNetFlags),
	PORT_FND(StartDummySearch, Scrap_StartDummySearch, str_PyDoc_Scrap_StartDummySearch),
	PORT_FND(NextDummySearch, Scrap_NextDummySearch, str_PyDoc_Scrap_NextDummySearch),
	PORT_FND(Rand, Scrap_Rand, str_PyDoc_Scrap_Rand),
	PORT_FND(Round, Scrap_Round, str_PyDoc_Scrap_Round),
	PORT_FND(SetDebrisSys, Scrap_SetDebrisSys, str_PyDoc_Scrap_SetDebrisSys),
	PORT_FND(DropDebris, Scrap_DropDebris, str_PyDoc_Scrap_DropDebris),
	PORT_FND(SetDebrisValue, Scrap_SetDebrisValue, str_PyDoc_Scrap_SetDebrisValue),
	PORT_FND(ModelInfo, Scrap_ModelInfo, str_PyDoc_Scrap_ModelInfo),
	PORT_FND(ListModels, Scrap_ListModels, str_PyDoc_Scrap_ListModels),
	PORT_FND(BuildFont, Scrap_BuildFont, str_PyDoc_Scrap_BuildFont),
	PORT_FND(ScreenShot, Scrap_ScreenShot, str_PyDoc_Scrap_ScreenShot),
	PORT_FND(ProcessDVF, Scrap_ProcessDVF, str_PyDoc_Scrap_ProcessDVF),
	PORT_FND(VideoPlayer, Scrap_VideoPlayer, str_PyDoc_Scrap_VideoPlayer),
	PORT_FND(MusicPlayer, Scrap_MusicPlayer, str_PyDoc_Scrap_MusicPlayer),
	PORT_FND(GetVideoCurrentMode, Scrap_GetVideoCurrentMode, str_PyDoc_Scrap_GetVideoCurrentMode),
	PORT_FND(SetVideoCurrentMode, Scrap_SetVideoCurrentMode, str_PyDoc_Scrap_SetVideoCurrentMode),
	PORT_FND(GetVideoModes, Scrap_GetVideoModes, str_PyDoc_Scrap_GetVideoModes),
	PORT_FND(TestLine, Scrap_TestLine, str_PyDoc_Scrap_TestLine),
	PORT_FND(TestSphere, Scrap_TestSphere, str_PyDoc_Scrap_TestSphere),
	PORT_FND(SphereCall, Scrap_SphereCall, str_PyDoc_Scrap_SphereCall),
	PORT_FND(SetCallFunc, Scrap_SetCallFunc, str_PyDoc_Scrap_SetCallFunc),
	PORT_FND(AddParamf, Scrap_AddParamf, str_PyDoc_Scrap_AddParamf),
	PORT_FND(AddParami, Scrap_AddParami, str_PyDoc_Scrap_AddParami),
	PORT_FND(AddParams, Scrap_AddParams, str_PyDoc_Scrap_AddParams),
	PORT_FND(Execute, Scrap_Execute, str_PyDoc_Scrap_Execute),
	PORT_FND(DelSaveVars, Scrap_DelSaveVars, str_PyDoc_Scrap_DelSaveVars),
	PORT_FND(CreateSaveVar, Scrap_CreateSaveVar, str_PyDoc_Scrap_CreateSaveVar),
	PORT_FND(GetSaveVar, Scrap_GetSaveVar, str_PyDoc_Scrap_GetSaveVar),
	PORT_FND(SetSaveVar, Scrap_SetSaveVar, str_PyDoc_Scrap_SetSaveVar),
	PORT_FND(IncSaveVar, Scrap_IncSaveVar, str_PyDoc_Scrap_IncSaveVar),
	PORT_FND(LoadGameVars, Scrap_LoadGameVars, str_PyDoc_Scrap_LoadGameVars),
	PORT_FND(CreateSaveProfile, Scrap_CreateSaveProfile, str_PyDoc_Scrap_CreateSaveProfile),
	PORT_FND(DeleteProfile, Scrap_DeleteProfile, str_PyDoc_Scrap_DeleteProfile),
	PORT_FND(SetSaveProfile, Scrap_SetSaveProfile, str_PyDoc_Scrap_SetSaveProfile),
	PORT_FND(GetSavedProfilesList, Scrap_GetSavedProfilesList, str_PyDoc_Scrap_GetSavedProfilesList),
	PORT_FND(GetFreeBlocks, Scrap_GetFreeBlocks, str_PyDoc_Scrap_GetFreeBlocks),
	PORT_FND(GetFreeBlocks, Scrap_GetFreeBlocks, str_PyDoc_Scrap_GetFreeBlocks),
	PORT_FND(LaunchDashboard, Scrap_LaunchDashboard, str_PyDoc_Scrap_LaunchDashboard),
	PORT_FND(DeselectProfile, Scrap_DeselectProfile, str_PyDoc_Scrap_DeselectProfile),
	PORT_FND(SaveGameVars, Scrap_SaveGameVars, str_PyDoc_Scrap_SaveGameVars),
	PORT_FND(SaveConfig, Scrap_SaveConfig, str_PyDoc_Scrap_SaveConfig),
	PORT_FND(GetSaveGamesList, Scrap_GetSaveGamesList, str_PyDoc_Scrap_GetSaveGamesList),
	PORT_FND(SetLanguage, Scrap_SetLanguage, str_PyDoc_Scrap_SetLanguage),
	PORT_FND(GetLanguage, Scrap_GetLanguage, str_PyDoc_Scrap_GetLanguage),
	PORT_FND(GetLangStr, Scrap_GetLangStr, str_PyDoc_Scrap_GetLangStr),
	PORT_FND(GetLangEntries, Scrap_GetLangEntries, str_PyDoc_Scrap_GetLangEntries),
	PORT_FND(GetAlarm, Scrap_GetAlarm, str_PyDoc_Scrap_GetAlarm),
	PORT_FND(SetAlarm, Scrap_SetAlarm, str_PyDoc_Scrap_SetAlarm),
	PORT_FND(SetAlarmGrow, Scrap_SetAlarmGrow, str_PyDoc_Scrap_SetAlarmGrow),
	PORT_FND(GetAlarmChars, Scrap_GetAlarmChars, str_PyDoc_Scrap_GetAlarmChars),
	PORT_FND(SetAlarmChar, Scrap_SetAlarmChar, str_PyDoc_Scrap_SetAlarmChar),
	PORT_FND(GetLockAlarm, Scrap_GetLockAlarm, str_PyDoc_Scrap_GetLockAlarm),
	PORT_FND(SetLockAlarm, Scrap_SetLockAlarm, str_PyDoc_Scrap_SetLockAlarm),
	PORT_FND(GetMoney, Scrap_GetMoney, str_PyDoc_Scrap_GetMoney),
	PORT_FND(SetMoney, Scrap_SetMoney, str_PyDoc_Scrap_SetMoney),
	PORT_FND(AddParkingZone, Scrap_AddParkingZone, str_PyDoc_Scrap_AddParkingZone),
	PORT_FND(GetFarestParked, Scrap_GetFarestParked, str_PyDoc_Scrap_GetFarestParked),
	PORT_FND(GetNearestParked, Scrap_GetNearestParked, str_PyDoc_Scrap_GetNearestParked),
	PORT_FND(AddItem, Scrap_AddItem, str_PyDoc_Scrap_AddItem),
	PORT_FND(InitLoading, Scrap_InitLoading, str_PyDoc_Scrap_InitLoading),
	PORT_FND(EndLoading, Scrap_EndLoading, str_PyDoc_Scrap_EndLoading),
	PORT_FND(Exit, Scrap_Exit, str_PyDoc_Scrap_Exit),
	PORT_FND(FileExist, Scrap_FileExist, str_PyDoc_Scrap_FileExist),
	PORT_FND(RunApp, Scrap_RunApp, str_PyDoc_Scrap_RunApp),
	PORT_FND(Achievement, Scrap_Achievement, str_PyDoc_Scrap_Achievement),
	PORT_FND(OpenStore, Scrap_OpenStore, str_PyDoc_Scrap_OpenStore),
	PORT_FND(IsApp, Scrap_IsApp, str_PyDoc_Scrap_IsApp),
	PORT_FND(GetSteamID, Scrap_GetSteamID, str_PyDoc_Scrap_GetSteamID),
	PORT_FND(GetSteamTicket, Scrap_GetSteamTicket, str_PyDoc_Scrap_GetSteamTicket),
	PORT_FND(IsSteam, Scrap_IsSteam, str_PyDoc_Scrap_IsSteam),
	PORT_FND(GetGogID, Scrap_GetGogID, str_PyDoc_Scrap_GetGogID),
	PORT_FND(ConsoleExec, Scrap_ConsoleExec,
		"ConsoleExec(line) : runs a line of the console"),
	PORT_FNEND
};

SD_PY_MODULE(Scrap, 0x42c610)


static char str_PyDoc_Scrap_EntityListGet[] =
	"(value)Scrap.EntityListGet(listName, varName) :  Obtiene el valor de una variable de una lista de en"
	"tidades";


static char str_PyDoc_Scrap_CreateEntityList[] = "CreateEntityList(listName) :  Crea una lista de entidades";


static char str_PyDoc_Scrap_SetCam[] = "SetCam(i,name) :  Activa una camara";


static char str_PyDoc_Scrap_GetCam[] = "name GetCam(i) :  Obtiene el nombre de una entidad camara";


static char str_PyDoc_Scrap_GetMinCamDist[] =
	"GetMinCamDist(x,y,z) :  Obtiene la distancia de un punto a la camara";


static char str_PyDoc_Scrap_LoadLevel[] = "LoadLevel(string name) :  Carga un nivel en el siguiente frame";


static char str_PyDoc_Scrap_GetLevelPath[] = "levelpath GetLevelPath() :  Obtiene el path del nivel actual";


static char str_PyDoc_Scrap_GetNewLevelPath[] =
	"levelpath GetNewLevelPath() :  Obtiene el path del próximo nivel";


static char str_PyDoc_Scrap_GetFloor[] = "i GetFloor(y) :  Obtiene el numero de piso en el que esta.";


static char str_PyDoc_Scrap_CreateElements[88] =
	"CreateElements() :  Crea una lista de elementos estaticos que pertenecen al mapa.";


static char str_PyDoc_Scrap_CallElements[] =
	"CallElements('Function') :  llama una funcion Function(Name,x,y,z,angx,angy)";


static char str_PyDoc_Scrap_SetGrid[] = "SetGrid(x,y,z) :  Modifica las dimensiones del grid de colision";


static char str_PyDoc_Scrap_GetNetFlags[] =
	"GetNetFlags() :  Obtiene la tupla de flags : client, server, dedicated";


static char str_PyDoc_Scrap_Rand[] = "Rand(min,max) :  Obtiene un numero seudo-aleatorio entre (min,max)";


static char str_PyDoc_Scrap_Round[] = "Round(num) : Redondea un numero real al entero más cercano.";


static char str_PyDoc_Scrap_NextDummySearch[] =
	"(s(ddd)(dd)i) NextDummySearch() : Obtiene el proximo dummy, sino None";


static char str_PyDoc_Scrap_StartDummySearch[] =
	"StartDummySearch(name,usewildcards) : Inicia la busqueda de dummies en el mapa";


static char str_PyDoc_Scrap_ModelInfo[] =
	"ModelInfo(name) : Muestra información sobre la jerarquía de nodos del modelo";


static char str_PyDoc_Scrap_ListModels[] = "ListModels() : Muestra una lista de modelos y escenas";


static char str_PyDoc_Scrap_BuildFont[] =
	"BuildFont(name,size) : Construye un archivo de fuente de letras a partir de un .TGA dado";


static char str_PyDoc_Scrap_ScreenShot[] = "ScreenShot(filename) : Screenshot of the current frame";


static char str_PyDoc_Scrap_ProcessDVF[] = "ProcessDVF(filename,command) : send a dvf command";


static char str_PyDoc_Scrap_VideoPlayer[] = "VideoPlayer(filename[,command]) : play video file";


static char str_PyDoc_Scrap_MusicPlayer[] = "MusicPlayer(filename[,command,param1]) : play music file";


static char str_PyDoc_Scrap_GetVideoCurrentMode[] = "GetVideoCurrentMode() : Get Video Current Mode Index";


static char str_PyDoc_Scrap_SetVideoCurrentMode[] = "SetVideoCurrentMode() : Set Video Current Mode Index";


static char str_PyDoc_Scrap_GetVideoModes[] = "GetVideoModes(NumMode) : Get Video Mode Info";


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

PORTS(Engine_ScrapPy)
