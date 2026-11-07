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
extern PyMethodDef g_BankMapMethods[];
extern PyMethodDef g_BankMapTreasureMethods[];
extern PyMethodDef g_BankCatchRudolphMethods[];
extern PyMethodDef g_BankGetDataPart2Methods[];
extern PyMethodDef g_BankInfiltrateBankMethods[];

/* Police (data01.packed levels/police) */
extern PyMethodDef g_PoliceMapMethods[];
extern PyMethodDef g_PoliceMapPlanesMethods[];
extern PyMethodDef g_PoliceMapSndMethods[];
extern PyMethodDef g_PoliceBeatPickusMethods[];
extern PyMethodDef g_PoliceFicusDeadMethods[];
extern PyMethodDef g_PoliceGetAuthorizationMethods[];
extern PyMethodDef g_PoliceInitEpilogueMethods[];
extern PyMethodDef g_PoliceJailBreakMethods[];
extern PyMethodDef g_PolicePoliceBossCheatsMethods[];

/* Townhall (data02.packed levels/townhall) */
extern PyMethodDef g_TownhallMapMethods[];
extern PyMethodDef g_TownhallMapSndMethods[];
extern PyMethodDef g_TownhallApologizeMayorMethods[];
extern PyMethodDef g_TownhallCatchTrurlMethods[];
extern PyMethodDef g_TownhallSheriffMayorMethods[];
extern PyMethodDef g_TownhallTalkToMayorMethods[];

/* Scrapyard (data02.packed levels/scrapyard) */
extern PyMethodDef g_ScrapyardMapMethods[];
extern PyMethodDef g_ScrapyardMapTriggersMethods[];
extern PyMethodDef g_ScrapyardAmbushMethods[];
extern PyMethodDef g_ScrapyardDefeatCrazyGambler2Methods[];
extern PyMethodDef g_ScrapyardFindDtritusMatrixMethods[];
extern PyMethodDef g_ScrapyardMeetBertoMethods[];
extern PyMethodDef g_ScrapyardMosquitoClueMethods[];
extern PyMethodDef g_ScrapyardStupidMercMethods[];
extern PyMethodDef g_ScrapyardStupidMercAgainMethods[];
extern PyMethodDef g_ScrapyardTalkToDeepThroatMethods[];

/* GDB (data03.packed levels/gdb) */
extern PyMethodDef g_GDBMapMethods[];
extern PyMethodDef g_GDBMapMatrixMethods[];
extern PyMethodDef g_GDBIntoTheGDBMethods[];
extern PyMethodDef g_GDBReinsertOwnMatrixMethods[];
extern PyMethodDef g_GDBReinsertOwnMatrix2Methods[];

/* ---- lines the level scripts share (src/game/Script/levels/LevelCommon.c); all do nothing after the frame failed */

/* `not Scrap.Get("IsXbox") or name` with name a global of the map script (the discardable characters): 1, 0, -1 */
int Lv_PcOr(scriptFrame_t *f, const char *module, const char *name);
/* CharsNPC.CharRes[name].CallbackFunc.append(func) with func the global of module, and the same after
 * CharsNPC.CreateMissionChar(name) */
void Lv_Callback(scriptFrame_t *f, const char *name, const char *module, const char *func);
void Lv_MissionChar(scriptFrame_t *f, const char *name, const char *module, const char *func);
/* Speech.PlayerName = usr; Speech.NPCName = ch */
void Lv_Speakers(scriptFrame_t *f, PyObject *usr, PyObject *ch);
/* Speech.OnEndOfSpeech = func (the global of module); Speech.OnEndOfSpeechParams = params (NULL for ()) */
void Lv_OnEnd(scriptFrame_t *f, const char *module, const char *func, PyObject *params);
/* a character stays where it is: LLogicDefaultState, LLogicDefaultTarget, LLogicState, LLogicTarget, then
 * LLogicIdle 1 and 0 (the lines the scripts repeat for every mission character) */
void Lv_Stay(scriptFrame_t *f, PyObject *e, long state, PyObject *target);
/* SScorer.Set(id, item, var, v) */
void Lv_ScorerSet(scriptFrame_t *f, PyObject *id, const char *item, const char *var, PyObject *v);
/* lst = eval(Scrap.GetSaveVar(var)) (in the globals of module): the list (borrowed, kept by the frame) */
PyObject *Lv_SaveList(scriptFrame_t *f, const char *var, const char *module);
/* lst.append(item); Scrap.SetSaveVar(var, `lst`) */
void Lv_SaveAppend(scriptFrame_t *f, const char *var, PyObject *lst, PyObject *item);
/* Scrap.GetSaveVar(name) == text: 1, 0, -1 */
int Lv_SaveIs(scriptFrame_t *f, const char *name, const char *text);
/* SScorer.Fade(0, (r,g,b,a), (r,g,b,a), time), with the colours black (0,0,0,255) and clear (0,0,0,0) */
extern const int lv_black[4], lv_clear[4];
void Lv_Fade(scriptFrame_t *f, const int from[4], const int to[4], double time);
/* Scrap.StartDummySearch(pattern, flag); dummy = Scrap.NextDummySearch() (borrowed) */
PyObject *Lv_Dummy(scriptFrame_t *f, const char *pattern, int flag);

/* InitStatus of the missions: import Traffic; Scrap.SetSaveVar("Traffic.AcShips", ships); Traffic.AcShips =
 * eval(Scrap.GetSaveVar("Traffic.AcShips")) (in the globals of module) */
byte Lv_AcShips(scriptFrame_t *f, const char *module, const char *ships);
/* import VehiclesNPC; sp = VehiclesNPC.cShipAI() with the "name=value" attributes of vars (Sf_SetAttrs): sp */
PyObject *Lv_Profile(scriptFrame_t *f, const char *module, const char *const *vars);
/* a ship at a free spawn point: SpwPos = OutMap.GetFreeSpawnPos(); Scrap.CreateEntity(name, SpwPos[0][0],
 * SpwPos[0][1], SpwPos[0][2], "Car"), or with nearest OutMap.GetNearestFreeSpawnPos() and SpwPos[0], [1], [2] */
PyObject *Lv_SpawnCar(scriptFrame_t *f, const char *name, byte nearest);
/* Lv_SpawnCar that also gives the spawn position SpwPos (borrowed, kept by the frame) */
PyObject *Lv_SpawnCarAt(scriptFrame_t *f, const char *name, byte nearest, PyObject **spw);
/* VehiclesNPC.SetStdProperties(e.Name, pd.Ship); VehiclesNPC.ModAIProff(e, pd) */
byte Lv_StdAI(scriptFrame_t *f, PyObject *e, PyObject *pd);
/* pd = profile() (a global of module) and Lv_StdAI(e, pd): pd */
PyObject *Lv_StdProfile(scriptFrame_t *f, const char *module, const char *profile, PyObject *e);
/* e.Life = e.MaxLife = the global life of module */
byte Lv_Life(scriptFrame_t *f, PyObject *e, const char *module, const char *life);
/* import PhoneCab; pc = PhoneCab.GetFree() with pc.UseFunc = use and the cab as the mission target (targetFirst: the
 * target is set before the UseFunc, as most missions do; pcModule: the global pc of that module is the cab right after
 * GetFree, NULL: a local): pc */
PyObject *Lv_PhoneCab(scriptFrame_t *f, const char *use, byte targetFirst, const char *pcModule);
/* InitStatus(LevelPath) of a mission with only the lines of Lv_AcShips */
PyObject *Lv_AcShipsStatus(PyObject *args, const char *module, const char *ships);
/* the profile function itself (no arguments, returns the Lv_Profile of vars) */
PyObject *Lv_ProfileFunc(PyObject *args, const char *module, const char *const *vars);
/* Scrap.SetSaveVar of pairs of name and value, ending with NULL */
void Lv_Saves(scriptFrame_t *f, const char *const *pairs);
/* a race of MapRace (no arguments): import RaceMaker; RaceMaker.StartPos = [] and the n points appended as
 * ((x, y, z), angle) */
PyObject *Lv_StartPos(PyObject *args, const double (*pos)[4], int n);
/* the colour Scorer.MissionScorerText_Color of a scorer item (Red, Green, Blue, Alpha) */
void Lv_Colour(scriptFrame_t *f, PyObject *id, const char *item);
/* Scorer.MissionScorerBar_SetDefaults(id, item, the back colour of Scorer, main, black); NULL main or black: the colours
 * of Scorer */
void Lv_BarDefaults(scriptFrame_t *f, PyObject *id, const char *item, PyObject *main, PyObject *black);
/* `usr and usr.Name == name`: 1, 0, -1 */
int Lv_UsrNameIs(scriptFrame_t *f, PyObject *usr, PyObject *name);
/* `a and b` of two values already there: 1, 0, -1 */
int Lv_And(scriptFrame_t *f, PyObject *a, PyObject *b);
/* Scrap.GetAlarm()[0] as a condition: 1, 0, -1 */
int Lv_Alarm(scriptFrame_t *f);
/* Speech.AddRemoteCom(key, who[, then[, params]]) (then NULL: two arguments; params NULL: three) */
byte Lv_Com(scriptFrame_t *f, const char *key, const char *who, PyObject *then, PyObject *params);
/* MissionsFuncs.SetNextMission(lib, map); SaveGame.MMDesc(desc, 1) */
byte Lv_Next(scriptFrame_t *f, const char *lib, const char *map, const char *desc);
/* Menu.MissionScorerCall = v */
byte Lv_ScorerCall(scriptFrame_t *f, PyObject *v);
/* SScorer.Set(0, item, var, v) */
byte Lv_Set0(scriptFrame_t *f, const char *item, const char *var, PyObject *v);
/* Scorer.MissionScorerText_X + d and Scorer.MissionScorerText_Y + d (d 0: the value itself); Scrap.GetLangStr(key) +
 * ":" */
PyObject *Lv_TextX(scriptFrame_t *f, long d);
PyObject *Lv_TextY(scriptFrame_t *f, long d);
PyObject *Lv_Label(scriptFrame_t *f, const char *key);
/* the text item of a mission scorer in Horatio with a shadow at the left, at Lv_TextX(0) and y (visible NULL: not set) */
byte Lv_Text(scriptFrame_t *f, PyObject *id, const char *item, PyObject *y, PyObject *text, PyObject *visible);
/* `num` + "/" + `max` of two globals of module (num read first) */
PyObject *Lv_Count(scriptFrame_t *f, const char *module, const char *num, const char *max);
/* SScorer.SetMsgText(cad, Scrap.GetTime() + Speech.CalculateSpeechTime(cad)) */
byte Lv_MsgText(scriptFrame_t *f, PyObject *cad);
/* Scrap.CreateEntity(name, p[0], p[1], p[2], type) with name a value */
PyObject *Lv_CreateAt(scriptFrame_t *f, PyObject *name, PyObject *p, const char *type);
/* `lst = eval(Scrap.GetSaveVar(var)); if not (item in lst): lst.append(item); Scrap.SetSaveVar(var, `lst`)` */
byte Lv_SaveAdd(scriptFrame_t *f, const char *module, const char *var, const char *item);
/* `bp = eval(Scrap.GetSaveVar(var)); bp = bp + items; Scrap.SetSaveVar(var, `bp`)` with items the Py_BuildValue of fmt */
byte Lv_SaveCat(scriptFrame_t *f, const char *module, const char *var, const char *fmt, ...);
/* a blueprint of a list of the hangar: Lv_SaveAdd of item, and when it was new the message msg (NULL: none; delay:
 * RaceMaker.ShowSystemMessageDelay(text, 0.25), else import RaceMaker; RaceMaker.ShowSystemMessage(text)) and
 * NewBluePrintAvaliable + 1 */
byte Lv_Blueprint(scriptFrame_t *f, const char *module, const char *var, const char *item, const char *msg, byte delay);
/* pc.UseFunc = "PhoneCab.DefUseAudioCom" with pc the global of module */
byte Lv_PcDefault(scriptFrame_t *f, const char *module);
/*
 * a mission item dropped at pos (money, amplifier, data, matrix): the model and radius are globals of module (radius
 * NULL: 3500), with the take sound and a mission arrow above it named arrow (NULL: the item's name + "CheckArrow")
 */
byte Lv_Drop(scriptFrame_t *f, const char *module, PyObject *pos, const char *name, const char *model, const char *radius,
	int onlyUser, const char *onTake, const char *sound, const char *arrow);
/* a mission item taken: itm.rTime = 20, deleted 5 s later, its arrow deleted now (the names of both) */
byte Lv_Taken(scriptFrame_t *f, PyObject *item, PyObject *arrow);

/* Scrap.GetLangStr(key) % SInput.GetDefinedEntry(0, "Walk", action, "Mouse") */
PyObject *Lv_KeyText(scriptFrame_t *f, const char *key, const char *action);
/* InitStatus of an outdoor map while the boss controls the robots: police and traffic 5 (comSats: none when the level
 * Init.UpperMap is in ComSatsMissionsMapsFinished, and OutPolice.PoliceActive = 0), no ships of the traffic */
void Lv_BossTraffic(scriptFrame_t *f, byte comSats);
/* Mission_DestroyCommSats1, 2 and 3: one script in three levels */
extern PyMethodDef g_OutskirtsDestroyCommSats1Methods[];
extern PyMethodDef g_CommercialDestroyCommSats2Methods[];
extern PyMethodDef g_DownTownDestroyCommSats3Methods[];

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
PyObject *Lv_PlanesInit(PyObject *args, const char *module);
PyObject *Lv_PlanesPhoto(PyObject *args, const char *module, const char *ships, const char *engines);

/* SndElements(Name, x, y, z, angx, angy) of the MapSnd of a level with its table of elements (ends with a NULL name) */
PyObject *Lv_SndElements(PyObject *args, const lvSndElement_t *table);

/*
 * ---- native lines of the level scripts (docs/objlib-plan.md, stage 2) ----
 *
 * The same lines for the levels whose functions work on native values (scrap/scriptmodes.h). Each returns 1, or 0
 * with the error of the script raised; conditions are 1, 0, or -1 with the error. Entities the script holds are
 * handle values (NV_OBJ, OBJLIB 7). Calls of other ports go by name (OBJLIB 3), the functions of the level module
 * through its globals (a mission may replace one), the variables of the level module are read where the script reads
 * them and written through at once (OBJLIB 5). The frame helpers above stay for the levels still on Sf_ frames.
 */

/* a function of the module through its global (a mission may replace it); another port by name (OBJLIB 3) */
#define LV_OWN(mod, name, out, ...) Native_CallGlobal(mod, name, out, __VA_ARGS__)
#define LV_CALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* module.var of another module as a value (any object: a handle, a function, a module as NV_OBJ) */
#define LvN_ModGet Mode_ModGet
/* module.var = the global name of src as the object it is (a function: the one a mission put there; OBJLIB 5) */
byte LvN_ModSetGlobal(const char *module, const char *var, const char *src, const char *name);
/* module.var = None */
#define LvN_ModNone Mode_ModNone
/* module.var.method(v) of a variable of another module, in place (the method of whatever value is there), and
 * module.var.append(v) */
byte LvN_ModMethod(const char *module, const char *var, const char *method, const nativeVal_t *v);
byte LvN_ModAppend(const char *module, const char *var, const nativeVal_t *v);
/* module.var.append(name) with name the global of gmod (a function): the variable, then the global, then the append
 * method of whatever value is there (OBJLIB 5: the list of another module in place) */
byte LvN_ModAppendG(const char *module, const char *var, const char *gmod, const char *name);
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
byte LvN_EqI(const nativeVal_t *a, long i);
byte LvN_EqS(const nativeVal_t *a, const char *s);
/* h.var = i, = "text", = d of a handle value */
#define LvN_SetI Mode_VSetI
#define LvN_SetS Mode_VSetS
#define LvN_SetF Mode_VSetF
/* x[i] of the script (a dict looks the int up; an NV_OBJ, e.g. a list that holds entity handles, the Python way, the
 * item as Native_Arg makes it) */
#define LvN_Item Mode_Item
/* a value whose items a loop takes one by one: a list or tuple held as one NV_OBJ (it holds handles) as a native list
 * or tuple of its items as Native_Arg makes them, any other value copied; the global name of module the same way */
void LvN_Elems(const nativeVal_t *v, nativeVal_t *out);
byte LvN_GElems(const char *module, const char *name, nativeVal_t *out);
/* raise "text" (a string exception of the script; OBJLIB 7: the Python exception until errors are a C state): 0 */
byte LvN_RaiseText(const char *text);
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
 * params NULL: three; the function Speech.AddRemoteCom read before then, as the script reads them) */
byte LvN_Com(const char *key, const char *who, const char *module, const char *func, const nativeVal_t *params);
/* the empty tuple () as the parameters of a callback */
const nativeVal_t *LvN_NoArgs(void);
/* Speech.PlayerName = usr; Speech.NPCName = ch */
byte LvN_Speakers(const nativeVal_t *usr, const nativeVal_t *ch);
/* Speech.OnEndOfSpeech = func (the global of module); Speech.OnEndOfSpeechParams = params (NULL for ()) */
byte LvN_OnEnd(const char *module, const char *func, const nativeVal_t *params);
/* the lines of Lv_Stay on a handle value */
byte LvN_Stay(const nativeVal_t *e, long state, const nativeVal_t *target);
/* CharsNPC.CharRes[name].CallbackFunc.append(func) with func the global of module, and the same after
 * CharsNPC.CreateMissionChar(name) */
byte LvN_Callback(const char *name, const char *module, const char *func);
byte LvN_MissionChar(const char *name, const char *module, const char *func);
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
byte LvN_Set0(const char *item, const char *var, const nativeVal_t *v);
byte LvN_Set0I(const char *item, const char *var, long i);
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
int LvN_UsrNameIs(const nativeVal_t *usr, const nativeVal_t *name);
/* Menu.DrawScorer(0, SInput.GetActionSet()) */
byte LvN_Draw(void);
/* Menu.MissionScorerCall = None, = the global func of module */
byte LvN_ScorerOff(void);
byte LvN_ScorerCallG(const char *module, const char *func);
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
/* the SAct functions the scene cases record (fr6b.py _BSCEN), through their seams (OBJLIB 7), on the current class and
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
byte LvN_PlaySound(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume);
/* import Traffic; Scrap.SetSaveVar("Traffic.AcShips", ships); Traffic.AcShips = eval(Scrap.GetSaveVar(...)) in the
 * globals of module */
byte LvN_AcShips(const char *module, const char *ships);
/* module.var = a list of texts (NULL ends them) of another module (OBJLIB 5) */
byte LvN_ModTexts(const char *module, const char *var, const char *const *texts);
/* h.var = (x, y, z) of three ints */
byte LvN_SetInts3(const nativeVal_t *h, const char *var, long x, long y, long z);
/* e.var = prefix + Scrap.GetLangStr(key) */
byte LvN_SetLangOpt(const nativeVal_t *e, const char *var, const char *prefix, const char *key);
/* MissionsFuncs.ActivateTarget(var, on) of a global var of the module (a handle) */
byte LvN_GActivate(const char *module, const char *var, long on);
/* Speech.SpeechRoutines[key] = fn */
byte LvN_Routine(const char *key, const nativeVal_t *fn);
/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func, args) with delay a value (the time added first, as the
 * scripts do) and func the global of module (its NameError; OBJLIB 4) */
byte LvN_LaterV(const nativeVal_t *delay, const char *module, const char *func, const nativeVal_t *args, int n);
/* the same with delay a global of module too (read first) */
byte LvN_LaterG(const char *module, const char *delay, const char *func, const nativeVal_t *args, int n);
/* the parameters of the native helpers: an int, a text */
void LvN_Int(nativeVal_t *v, long i);
void LvN_Str(nativeVal_t *v, const char *s);
/* the adapter of a level function: parses the n arguments, runs core(a, out), the result as Python (None for a core
 * that leaves out None) */
#define LV_FN(pre, name, nargs, core) MODE_FN(pre, name, nargs, core(a, out))

#endif
