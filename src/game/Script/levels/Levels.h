/*
 * The level scripts in C (issue #2): the Map module, the libraries and the missions of a level, one file per level
 * (src/game/Script/levels/<Level>.c). Levels.c puts a table into the module when it loads in its level, after the
 * functions that are data (data/levels/<level>/<module>.def). The module variables and the module code stay in the
 * scripts; the C reads and sets them as globals of the module, like the functions of the script.
 */
#ifndef SCRAP_LEVELS_H
#define SCRAP_LEVELS_H

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* Bank (Data.packed levels/bank) */
extern const portFn_t g_BankMapMethods[];
extern const portFn_t g_BankMapTreasureMethods[];
extern const portFn_t g_BankCatchRudolphMethods[];
extern const portFn_t g_BankGetDataPart2Methods[];
extern const portFn_t g_BankInfiltrateBankMethods[];

/* Police (data01.packed levels/police) */
extern const portFn_t g_PoliceMapMethods[];
extern const portFn_t g_PoliceMapPlanesMethods[];
extern const portFn_t g_PoliceMapSndMethods[];
extern const portFn_t g_PoliceBeatPickusMethods[];
extern const portFn_t g_PoliceFicusDeadMethods[];
extern const portFn_t g_PoliceGetAuthorizationMethods[];
extern const portFn_t g_PoliceInitEpilogueMethods[];
extern const portFn_t g_PoliceJailBreakMethods[];
extern const portFn_t g_PolicePoliceBossCheatsMethods[];

/* Townhall (data02.packed levels/townhall) */
extern const portFn_t g_TownhallMapMethods[];
extern const portFn_t g_TownhallMapSndMethods[];
extern const portFn_t g_TownhallApologizeMayorMethods[];
extern const portFn_t g_TownhallCatchTrurlMethods[];
extern const portFn_t g_TownhallSheriffMayorMethods[];
extern const portFn_t g_TownhallTalkToMayorMethods[];

/* Scrapyard (data02.packed levels/scrapyard) */
extern const portFn_t g_ScrapyardMapMethods[];
extern const portFn_t g_ScrapyardMapTriggersMethods[];
extern const portFn_t g_ScrapyardAmbushMethods[];
extern const portFn_t g_ScrapyardDefeatCrazyGambler2Methods[];
extern const portFn_t g_ScrapyardFindDtritusMatrixMethods[];
extern const portFn_t g_ScrapyardMeetBertoMethods[];
extern const portFn_t g_ScrapyardMosquitoClueMethods[];
extern const portFn_t g_ScrapyardStupidMercMethods[];
extern const portFn_t g_ScrapyardStupidMercAgainMethods[];
extern const portFn_t g_ScrapyardTalkToDeepThroatMethods[];

/* GDB (data03.packed levels/gdb) */
extern const portFn_t g_GDBMapMethods[];
extern const portFn_t g_GDBMapMatrixMethods[];
extern const portFn_t g_GDBIntoTheGDBMethods[];
extern const portFn_t g_GDBReinsertOwnMatrixMethods[];
extern const portFn_t g_GDBReinsertOwnMatrix2Methods[];

/* ---- whole functions and data the level scripts share (src/game/Script/levels/LevelCommon.c) */

/* the colours black (0,0,0,255) and clear (0,0,0,0) of SScorer.Fade (Mode_Fade) */
extern const int lv_black[4], lv_clear[4];
/* a race of MapRace (no arguments): import RaceMaker; RaceMaker.StartPos = [] and the n points appended as
 * ((x, y, z), angle) (the entries of the races, MODE_FN) */
byte LvN_StartPos(const double (*pos)[4], int n);
/* Mission_DestroyCommSats1, 2 and 3: one script in three levels */
extern const portFn_t g_OutskirtsDestroyCommSats1Methods[];
extern const portFn_t g_CommercialDestroyCommSats2Methods[];
extern const portFn_t g_DownTownDestroyCommSats3Methods[];

/*
 * a sound element of the map (MapSnd.SndElements(Name, x, y, z, angx, angy), called by the engine for the map
 * elements): a point sound loop and/or an interval sound of the element, named Name + `cntr` (the counter of MapSnd)
 */
typedef struct {
	const char *name;		/* the map element */
	byte pc;			/* the loop only when MapSnd.isPC */
	byte front;			/* the loop 300 in front of the element: SVec.Add(pos, SVec.Rotate3D((0,0,-300),(angx,angy,0))) */
	const char *loop;		/* Sound.SetPSLoop(name, pos, loop, vol[, pitch]) or NULL */
	double vol;
	int pitch;
	byte hasPitch;
	const char *beep;		/* Sound.SetPSInterval(name, pos, beep, imin, imax, ivol, 0) or NULL */
	const char *beepXbox;		/* the interval sound when not MapSnd.isPC (NULL: the same) */
	byte beepI;			/* the interval named "I" + Name + `cntr` */
	int imin, imax;
	double ivol;
} lvSndElement_t;

/*
 * MapPlanes of a level (the blueprints to photograph, the lists Naves and Motores of the module): Init(LevelPath) puts
 * a photo object at every DM_LocalPlane_ dummy whose blueprint is still to take; TakenPhoto(Name) moves the ship or
 * engine of a "Plano" photo to the hangar lists (the save variables ships and engines keep the module lists): 1, 0,
 * or None for another photo
 */
byte LvN_PlanesInit(const char *module);
byte LvN_PlanesPhoto(const nativeVal_t *name, const char *module, const char *ships, const char *engines, nativeVal_t *out);

/* SndElements(Name, x, y, z, angx, angy) of the MapSnd of a level with its table of elements (ends with a NULL name):
 * LvN_SndElements on the six arguments (MODE_FN) */
byte LvN_SndElements(const nativeVal_t *a, const lvSndElement_t *table);

/*
 * ---- native lines of the level scripts (docs/objlib-plan.md, stage 2) ----
 *
 * The lines the level scripts repeat, on native values (scrap/scriptmodes.h). Each returns 1, or 0
 * with the error of the script raised; conditions are 1, 0, or -1 with the error. Entities the script holds are
 * handle values (NV_OBJ, OBJLIB 7). Calls of other ports go through PORT_CALL (the port seam, OBJLIB 5 (9c)); the .def
 * sections, builtins and engine functions stay by name (OBJLIB 6, 9, 7), the functions of the level module through its
 * globals (a mission may replace one), the variables of the level module are read where the script reads them and
 * written through at once.
 */

/* by name: a .def section of the module through its global, a builtin or an engine function (OBJLIB 6, 9, 7) */
#define LV_OWN(mod, name, out, ...) Native_CallGlobal(mod, name, out, __VA_ARGS__)
#define LV_CALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* module.var of another module as a value (any object: a handle, a function, a module as NV_OBJ) */
#define LvN_ModGet Mode_ModGet
/* module.var = the global name of src as the object it is (a function: the one a mission put there; OBJLIB 5 (9c)) */
#define LvN_ModSetGlobal Mode_ModSetGlobal
/* module.var = None */
#define LvN_ModNone Mode_ModNone
/* module.var.method(v) of a variable of another module, in place (the method of whatever value is there), and
 * module.var.append(v) */
byte LvN_ModMethod(const char *module, const char *var, const char *method, const nativeVal_t *v);
byte LvN_ModAppend(const char *module, const char *var, const nativeVal_t *v);
/* module.var.append(name) with name the global of gmod (a function): the variable, then the global, then the append
 * method of whatever value is there (the list of another module in place) */
#define LvN_ModAppendG Mode_AppendG
/* the global name of module (a function, a list: any object) as a value */
byte LvN_GObj(const char *module, const char *name, nativeVal_t *out);
/* global name = name + add of the module; the new value into out (NULL: dropped) */
byte LvN_GAdd(const char *module, const char *name, long add, nativeVal_t *out);
/* global name = i, = "text" of the module */
byte LvN_PutI(const char *module, const char *name, long i);
byte LvN_PutS(const char *module, const char *name, const char *s);
/* `if name:` and `name == i` of a global of the module */
int LvN_GTrue(const char *module, const char *name);
int LvN_GEqI(const char *module, const char *name, long i);
/* a == i, a == "text" of the script (Native_Compare can not fail on native values) */
#define LvN_EqI Mode_EqI
byte LvN_EqS(const nativeVal_t *a, const char *s);
/* h.var = i, = "text", = d of a handle value */
#define LvN_SetI Mode_VSetI
#define LvN_SetS Mode_VSetS
#define LvN_SetF Mode_VSetF
/* x[i] of the script (a dict looks the int up; an NV_OBJ, e.g. a list that holds entity handles, the Python way, the
 * item as Native_Arg makes it) */
#define LvN_Item Mode_Item
/* the global name of module as a value whose items a loop takes one by one (Mode_Elems) */
byte LvN_GElems(const char *module, const char *name, nativeVal_t *out);
/* raise "text" (a string exception of the script): 0 */
#define LvN_RaiseText Mode_RaiseText
/* Scrap.StartDummySearch(pattern, flag); dummy = Scrap.NextDummySearch() */
void LvN_Dummy(const char *pattern, int flag, nativeVal_t *dummy);
/* (dummy[1][0], dummy[1][1], dummy[1][2]) and (dummy[2][0], dummy[2][1], 0) of a dummy search */
byte LvN_DummyPosAng(const nativeVal_t *dummy, nativeVal_t *pos, nativeVal_t *ang);
/* eval(Scrap.GetSaveVar(var)) in the globals of module */
byte LvN_SaveList(const char *var, const char *module, nativeVal_t *out);
/* lst.append(x) of a value of the script (a list in C; another value the Python way, its error) */
byte LvN_Append(nativeVal_t *lst, const nativeVal_t *x);
/* lst.append(item); Scrap.SetSaveVar(var, `lst`) */
byte LvN_SaveAppend(const char *var, nativeVal_t *lst, const nativeVal_t *item);
/* lst.remove(x) of a value (a list in C: the ValueError of 1.5.2 when x is not in it; another value the Python way,
 * its error) */
byte LvN_ListRemove(nativeVal_t *lst, const nativeVal_t *x);
/* Scrap.SetSaveVar(var, `v`) */
void LvN_SaveRepr(const char *var, const nativeVal_t *v);
/* Scrap.GetSaveVar(name) == text */
byte LvN_SaveIs(const char *name, const char *text);
/* Speech.AddRemoteCom(key, who[, then[, params]]) with then the global func of module (func NULL: two arguments,
 * params NULL: three; then read before Speech.AddRemoteCom, which the script reads first: the same unless both are
 * gone); LV_COM(key, who, Module, Func, params) with the function of a level module named by its ref */
byte LvN_Com(const char *key, const char *who, const char *module, const char *func, const nativeVal_t *params);
byte LvN_ComRef(const char *key, const char *who, const portRef_t *ref, const nativeVal_t *params);
#define LV_COM(key, who, module, name, params) LvN_ComRef(key, who, &PORT_LREF_(PORT_LEVEL, module##_##name), params)
/* the empty tuple () as the parameters of a callback */
const nativeVal_t *LvN_NoArgs(void);
/* Speech.PlayerName = usr; Speech.NPCName = ch */
byte LvN_Speakers(const nativeVal_t *usr, const nativeVal_t *ch);
/* Speech.OnEndOfSpeech = func (the global of module); Speech.OnEndOfSpeechParams = params (NULL for ()); LvN_OnEndRef
 * with the function of a ref (its module and name), LV_ONEND(Module, Func, params) of a level module (PORT_LEVEL);
 * the form with names for a function only known at run time */
byte LvN_OnEnd(const char *module, const char *func, const nativeVal_t *params);
byte LvN_OnEndRef(const portRef_t *ref, const nativeVal_t *params);
#define LV_ONEND(module, name, params) LvN_OnEndRef(&PORT_LREF_(PORT_LEVEL, module##_##name), params)
/* the lines of Lv_Stay on a handle value */
byte LvN_Stay(const nativeVal_t *e, long state, const nativeVal_t *target);
/* CharsNPC.CharRes[name].CallbackFunc.append(func) with func the global of module, and the same after
 * CharsNPC.CreateMissionChar(name); LV_CALLBACK(name, Module, Func) and LV_MISSIONCHAR(name, Module, Func) with the
 * function of a level module named by its ref */
byte LvN_Callback(const char *name, const char *module, const char *func);
byte LvN_MissionChar(const char *name, const char *module, const char *func);
byte LvN_CallbackRef(const char *name, const portRef_t *ref);
byte LvN_MissionCharRef(const char *name, const portRef_t *ref);
#define LV_CALLBACK(name, module, func) LvN_CallbackRef(name, &PORT_LREF_(PORT_LEVEL, module##_##func))
#define LV_MISSIONCHAR(name, module, func) LvN_MissionCharRef(name, &PORT_LREF_(PORT_LEVEL, module##_##func))
/* `not Scrap.Get("IsXbox") or name` with name a global of module */
int LvN_PcOr(const char *module, const char *name);
/* the text item of a mission scorer in two parts around the line of its Text: Add, X (Scorer.MissionScorerText_X), Y
 * (MissionScorerText_Y + dy), Font, Effect, then Align and the colour */
byte LvN_ScorerTextBegin(const nativeVal_t *id, const char *item, long dy);
byte LvN_ScorerTextEnd(const nativeVal_t *id, const char *item);
/* SScorer.Set(id, item, var, v) with a C item name, and with a C text v */
byte LvN_ScorerSet(const nativeVal_t *id, const char *item, const char *var, const nativeVal_t *v);
byte LvN_ScorerSetS(const nativeVal_t *id, const char *item, const char *var, const char *s);
/* SScorer.Set(0, item, var, v) and the same with an int */
#define LvN_Set0 Mode_Set0
#define LvN_Set0I Mode_Set0I
/* module.var = i of another module */
byte LvN_ModSetI(const char *module, const char *var, long i);
/* e.Life = e.MaxLife = the global life of module */
byte LvN_Life(const nativeVal_t *e, const char *module, const char *life);
/* a value of a table of the level scripts as Sf_Value reads it (an int, a float, "%Module.name", "^Module.Func:text",
 * "'text" or else a text) */
byte LvN_Value(const char *text, int len, nativeVal_t *out);
/* import VehiclesNPC; sp = VehiclesNPC.cShipAI() with the "name=value" attributes of vars: sp (an object) */
byte LvN_Profile(const char *const *vars, nativeVal_t *out);
/* VehiclesNPC.SetStdProperties(e.Name, pd.Ship); VehiclesNPC.ModAIProff(e, pd) */
byte LvN_StdAI(const nativeVal_t *e, const nativeVal_t *pd);
/* pd = profile() (a global of module) and LvN_StdAI(e, pd) */
byte LvN_StdProfile(const char *module, const char *profile, const nativeVal_t *e, nativeVal_t *pd);
/* `usr and usr.Name == name` */
#define LvN_UsrNameIs Mode_UsrNameIs
/* Menu.DrawScorer(0, SInput.GetActionSet()) */
byte LvN_Draw(void);
/* Menu.MissionScorerCall = None, = the global func of module (LvN_ScorerRef: of a ref; LV_SCORERG(Module, Func) of a
 * level module, PORT_LEVEL) */
byte LvN_ScorerOff(void);
byte LvN_ScorerCallG(const char *module, const char *func);
byte LvN_ScorerRef(const portRef_t *ref);
#define LV_SCORERG(module, name) LvN_ScorerRef(&PORT_LREF_(PORT_LEVEL, module##_##name))
/* MissionsFuncs.SetNextMission(lib, map); SaveGame.MMDesc(desc, 1) */
byte LvN_Next(const char *lib, const char *map, const char *desc);
/* Scrap.GetEntity(name) of a C name as the handle value */
byte LvN_Entity(const char *name, nativeVal_t *out);
/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func, args) with func an object */
byte LvN_LaterObj(double delay, const nativeVal_t *func, const nativeVal_t *args, int n);
/* Scrap.CreateEntity(name, x, y, z, cls) and the same at pos[0], pos[1], pos[2], through its seam (fr6.py swaps it):
 * the handle value, None when the engine made none */
byte LvN_Create(const nativeVal_t *name, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const char *cls,
	nativeVal_t *out);
byte LvN_CreateAt(const nativeVal_t *name, const nativeVal_t *pos, const char *cls, nativeVal_t *out);
/* Scrap.GetLangStr(key) + ":" */
void LvN_Label(const char *key, nativeVal_t *out);
/* the SAct functions the scene cases record (fr6b.py _BSCEN), through their seams, on the current class and
 * action as SActPy.c: GetClass, CreateClass, GetAction and CreateAction give 1 or 0 (None for an action without a
 * class), GetAct the variable (None when there is none) */
byte LvN_GetClass(const char *name, nativeVal_t *out);
byte LvN_CreateClass(const char *name, nativeVal_t *out);
byte LvN_GetAction(const char *name, nativeVal_t *out);
byte LvN_CreateAction(const char *name, nativeVal_t *out);
byte LvN_GetAct(const char *name, nativeVal_t *out);
byte LvN_SetAct(const char *name, const nativeVal_t *v);
byte LvN_SetCls(const char *name, const nativeVal_t *v);
/* SSound.SetMusic(name, volume) through its seam (fr6b.py _BREC records it) */
byte LvN_SetMusic(const char *name, double volume);
/* Scorer.MissionScorerText_X + d, Scorer.MissionScorerText_Y + d (d 0: the value itself) */
byte LvN_TextX(long d, nativeVal_t *out);
byte LvN_TextY(long d, nativeVal_t *out);
/* a text of a mission scorer as the missions set it (Sf_MissionText on values): SScorer.Add(id, item, type) (type
 * NULL: none), X, Y, Font, Effect (NULL: none), Text, Align, the colour Scorer.MissionScorerText_Color and Visible
 * (NULL: none) */
byte LvN_MissionText(const nativeVal_t *id, const char *item, const char *type, const nativeVal_t *x, const nativeVal_t *y,
	const char *font, const char *effect, const nativeVal_t *text, const char *align, const nativeVal_t *visible);
/* the core of a level function (the LV_FN form) that reads none of its arguments, and one on its arguments */
#define LV_CORE0(fn) \
	static byte fn##_C(const nativeVal_t *a, nativeVal_t *out) { \
		(void)a; \
		(void)out; \
		return fn(); \
	}
#define LV_COREA(fn) \
	static byte fn##_C(const nativeVal_t *a, nativeVal_t *out) { \
		(void)out; \
		return fn(a); \
	}
/* SLogic.SetOnFloor(name) and SSound.PlaySound(pos, name, volume) through their seams (fc-tp-map swaps them) */
byte LvN_SetOnFloor(const nativeVal_t *name);
#define LvN_PlaySound Mode_PlaySoundSeam
/* import Traffic; Scrap.SetSaveVar("Traffic.AcShips", ships); Traffic.AcShips = eval(Scrap.GetSaveVar(...)) in the
 * globals of module */
byte LvN_AcShips(const char *module, const char *ships);
/* module.var = a list of texts (NULL ends them) of another module */
byte LvN_ModTexts(const char *module, const char *var, const char *const *texts);
/* h.var = (x, y, z) of three ints */
byte LvN_SetInts3(const nativeVal_t *h, const char *var, long x, long y, long z);
/* e.var = prefix + Scrap.GetLangStr(key) */
byte LvN_SetLangOpt(const nativeVal_t *e, const char *var, const char *prefix, const char *key);
/* MissionsFuncs.ActivateTarget(v, on), of a global var of the module (a handle) */
byte LvN_Activate(const nativeVal_t *v, long on);
byte LvN_GActivate(const char *module, const char *var, long on);
/* Speech.SpeechRoutines[key] = fn */
byte LvN_Routine(const char *key, const nativeVal_t *fn);
/* Mode_LaterV with delay a global of module (read first) */
byte LvN_LaterG(const char *module, const char *delay, const char *func, const nativeVal_t *args, int n);
/* pc.UseFunc = "PhoneCab.DefUseAudioCom" of the global pc of the module: the phone cab back to normal */
byte LvN_PcDefault(const char *module);
/* MissionsFuncs.SetMissionTarget(v), of h.Name, of the global name of module (a handle) .Name */
byte LvN_Target(const nativeVal_t *v);
byte LvN_TargetName(const nativeVal_t *h);
byte LvN_TargetG(const char *module, const char *name);
/* the global handle name of module .var into out */
byte LvN_GAttr(const char *module, const char *name, const char *var, nativeVal_t *out);
/* the global name of module = None */
byte LvN_PutNone(const char *module, const char *name);
/* h.var = the global name of module */
byte LvN_SetG(const nativeVal_t *h, const char *var, const char *module, const char *name);
/* module.var = v of another module as Script_SetVar sets it: an error is dropped */
#define LvN_SetVar Mode_ModPut
/* the player is there (usr into *usr): 1, 0, -1 */
int LvN_Usr(nativeVal_t *usr);
/* `usr.ActCtrl == ctrl` of a handle: 1, 0, -1 */
int LvN_CtrlIs(const nativeVal_t *usr, const char *ctrl);
/* `text + v` of the script into out */
byte LvN_Cat(const char *text, const nativeVal_t *v, nativeVal_t *out);
/* the empty text "" as a value */
const nativeVal_t *LvN_Empty(void);
/* h.var = Scrap.GetLangStr(key) */
byte LvN_SetLang(const nativeVal_t *h, const char *var, const char *key);
/* CharsNPC.SetReposChars(name, n) */
byte LvN_Repos(const char *name, long n);
/* the scan bar ScanRemainingTime of mission scorer 0 blinks (lost: Blink 1, IsAutoUpdatable 0) or runs */
byte LvN_ScanBlink(int lost);
/* InTraffic.<list> = the n models (n -1: up to NULL) and InTraffic.<num> = count */
byte LvN_InTraffic(const char *list, const char *const *models, int n, const char *num, long count);
/*
 * StartCameraTravelling() of the missions that start at missionStartWaypoint of module (DownTown's DistractHumphrey,
 * Outskirts' FunctionaryHunting): the player's ship flies to the waypoint while a camera path comes close to it, the
 * scene text text, AbortCameraTravelling(UsrName, prevCamera) of module scheduled at the end of the path (again: the
 * script reads missionStartWaypoint again for each use of its Ang and Pos)
 */
byte LvN_CamTravel(const char *module, const char *text, byte again);
/* AbortCameraTravelling(UsrEntityName, prevCamera) of those missions: the player's control, the menus and the camera
 * back (racerScorer: RacerScorer.Create(0) too), then LaunchMission() of module */
byte LvN_CamTravelEnd(const nativeVal_t *a, const char *module, byte racerScorer);
/* import RaceMaker; RaceMaker.ShowSystemMessage(Scrap.GetLangStr(key)) */
byte LvN_SystemMessage(const char *key);
/* ShowBoostMessage() of the Humphrey races (DownTown's HumphreyRace, Outskirts' Demo_HumphreyRace): the callback of
 * Parking that module.prevSwitchVehicleMissionCallback holds called when set and put back, the message of the boost key */
byte LvN_BoostMessage(const char *module);
/* ClosePressHangar() of the same races: the door of the press hangar closes, RaceMaker.OnCheckPointCallback = None */
byte LvN_ClosePressHangar(void);
/* Speech.SpeechRoutines[kind](UsrName, CharName, optId, FullOption) with the four arguments a */
byte LvN_RoutineCall(const char *kind, const nativeVal_t *a);
/* SAI.SetStateChar(3, human.Name, usr.Name, 1.0) with human the global of module: the human follows the player */
byte LvN_HumanFollow(const char *module, const nativeVal_t *usr);
/* the core of a level function that does nothing (MapRace.Init, MapSnd.Init of the editor, ...) */
byte LvN_Pass(const nativeVal_t *a, nativeVal_t *out);
/* SetOnFloor(v.Name) of a handle value (its Name read as any value) */
byte LvN_SetOnFloorOf(const nativeVal_t *v);
/* the parameters of the native helpers: an int, a text */
void LvN_Int(nativeVal_t *v, long i);
void LvN_Str(nativeVal_t *v, const char *s);
/* the adapter of a level function: parses the n arguments, runs core(a, out), the result as Python (None for a core
 * that leaves out None) */
#define LV_FN(pre, name, nargs, core) MODE_FN(pre, name, nargs, core(a, out))

#endif
