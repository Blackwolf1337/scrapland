/*
 * The level scripts in C (issue #2): the Map module, the libraries and the missions of a level, one file per level
 * (src/game/Script/levels/<Level>.c). Levels.c puts a table into the module when it loads in its level, after the
 * functions that are data (data/levels/<level>/<module>.def). The module variables and the module code stay in the
 * scripts; the C reads and sets them as globals of the module, like the functions of the script.
 */
#ifndef SCRAP_LEVELS_H
#define SCRAP_LEVELS_H

#include "scrap/script.h"

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
/* SScorer.Set(id, item, var, v); the text item of a mission scorer in two parts around the line of its Text: Add, X
 * (Scorer.MissionScorerText_X), Y (MissionScorerText_Y + dy), Font, Effect, then Align and the colour */
void Lv_ScorerSet(scriptFrame_t *f, PyObject *id, const char *item, const char *var, PyObject *v);
void Lv_ScorerTextBegin(scriptFrame_t *f, PyObject *id, const char *item, long dy);
void Lv_ScorerTextEnd(scriptFrame_t *f, PyObject *id, const char *item);
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

#endif
