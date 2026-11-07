/*
 * Native C helpers of the game mode ports (src/game/Script/ScriptModes.c): Net, Ffa and the online modes
 * (NetModes.c), the split screen (Split.c) and the single player super deals (SuperDeals.c). They call the engine in
 * C on native values. The engine functions the case files of these modules record or swap go through a seam
 * (OBJLIB 7, see Native_Seam): Scrap.AddScheduledFunc, Scrap.DeleteScheduledFuncs, Scrap.Set, Scrap.Rand,
 * Scrap.ConsoleError, SNet.SendUsrString, SNet.ModifyUsrData, SNet.IsClient, SNet.IsServer, SScorer.SetMsgText,
 * SScorer.SetLabelText, SScorer.SetMPFunc, SScorer.Set, SScorer.Fade, SInput.Bind, SSound.Play, SAI.GetStateVehicle
 * and SAI.SetStateVehicle. Every function returns 1, or 0 with the error of the script raised.
 */
#ifndef SCRAP_SCRIPTMODES_H
#define SCRAP_SCRIPTMODES_H

#include "scrap/scriptnative.h"

/* ---- engine reads ---- */

/* Scrap.GetTime() */
double Mode_Time(void);
/* Scrap.GetLangStr(key) into *out ("" and the console line of the engine when it is not there) */
void Mode_Lang(const char *key, nativeVal_t *out);
/* Scrap.GetLangStr(key) % args into *out (args NULL: the text alone) */
byte Mode_LangFmt(const char *key, const nativeVal_t *args, nativeVal_t *out);
/* Scrap.GetEntity(name) as a value: the entity, NULL for None (a name that is no text raises) */
byte Mode_Ent(const nativeVal_t *name, cEntity **out);
/* var of the handle of entity name; e NULL is the None that Scrap.GetEntity gave (its AttributeError) */
byte Mode_Get(cEntity *e, const char *name, const char *var, nativeVal_t *out);
/* var of the handle of entity name = v (e NULL: the TypeError of None) */
byte Mode_Set(cEntity *e, const char *name, const char *var, const nativeVal_t *v);
byte Mode_SetI(cEntity *e, const char *name, const char *var, long i);
byte Mode_SetS(cEntity *e, const char *name, const char *var, const char *s);
/* Scrap.GetEntity(name).var, the handle used at once */
byte Mode_EntGet(const nativeVal_t *name, const char *var, nativeVal_t *out);
byte Mode_EntSet(const nativeVal_t *name, const char *var, const nativeVal_t *v);
/* SInput.GetActionSet() is the action set name */
byte Mode_ActionSetIs(const char *name);

/* ---- engine calls through their seams (OBJLIB 7) ---- */

/* SNet.IsClient(), SNet.IsServer(): 1 or 0, -1 with the error */
int Mode_IsClient(void);
int Mode_IsServer(void);
/* SNet.SendUsrString(id, s) */
byte Mode_Send(const nativeVal_t *id, const nativeVal_t *s);
byte Mode_SendS(long id, const char *s);
/* SNet.ModifyUsrData(id) */
byte Mode_ModifyUsrData(const nativeVal_t *id);
/* SScorer.SetMsgText(text, time), SScorer.SetLabelText(text, time) */
byte Mode_MsgText(const nativeVal_t *text, double time);
byte Mode_LabelText(const nativeVal_t *text, double time);
/* SScorer.SetMPFunc(id, func) */
byte Mode_MPFunc(long id, const char *func);
/* SScorer.Set(id, item, var, v) */
byte Mode_ScorerSet(const nativeVal_t *id, const nativeVal_t *item, const char *var, const nativeVal_t *v);
/* SScorer.Fade(id, (r, g, b, a), (r, g, b, a), time) */
byte Mode_Fade(long id, const int *from, const int *to, double time);
/* SInput.Bind(id, set, action, func) */
byte Mode_Bind(const nativeVal_t *id, const char *set, const char *action, const char *func);
/* Scrap.DeleteScheduledFuncs(name) */
byte Mode_DelSched(const nativeVal_t *name);
byte Mode_DelSchedS(const char *name);
/* Scrap.AddScheduledFunc(time, func, args[, name]): func a Python callable (OBJLIB 4: the scheduler keeps Python
 * objects), args the tuple of the native values (n of them), name NULL for none */
byte Mode_Schedule(const nativeVal_t *time, PyObject *func, const nativeVal_t *args, int n, const nativeVal_t *name);
/* the same at Scrap.GetTime() + delay with func a global of the module (its NameError when it is gone) */
byte Mode_Later(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* Scrap.Set(name, v) */
byte Mode_GVarSet(const char *name, const nativeVal_t *v);
byte Mode_GVarSetS(const char *name, const char *s);
byte Mode_GVarSetN(const char *name, double d);
/* Scrap.Rand(a, b) */
byte Mode_Rand(double a, double b, nativeVal_t *out);
/* Scrap.ConsoleError(text) */
byte Mode_ConsoleError(const nativeVal_t *text);
/* SSound.Play(Sound.<sound>, Sound.<volume>) (OBJLIB 5: the names are variables of the Sound module) */
byte Mode_Sound(const char *sound, const char *volume);

/* ---- module state ---- */

/* the PyObject of a module variable as its functions read it (a new reference; the NameError of a missing global) */
PyObject *Mode_GlobalPy(const char *module, const char *name);
/* module.var = v of another module (OBJLIB 5: its variables are still in its dict) */
byte Mode_SetModVar(const char *module, const char *var, const nativeVal_t *v);
/* module.var of another module into *out (any native value; a TypeError for other objects) */
byte Mode_ModVar(const char *module, const char *var, nativeVal_t *out);
/* var[key] = v and del var[key] of a dict variable of a module (the dict object changed in place, as the script
 * does: the order of its keys stays Python's), the native copy in *d changed the same way; key and v stay the
 * caller's */
byte Mode_DictPut(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key, const nativeVal_t *v);
byte Mode_DictDel(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key);
/* a variable of the module = v (a value given to the dict as a new object; plain values) */
byte Mode_Put(const char *module, const char *var, nativeVal_t *slot, const nativeVal_t *v);

/* ---- shared functions of the modes ---- */

/* PurgeServerMapList() of Net and Split: only the maps of ServerMapList the server type has (ModesMap of the
 * module) */
byte Mode_PurgeServerMapList(const char *module);

/* ---- adapters ---- */

/* the native value of a Python argument: 1, else 0 with a TypeError naming the function (an object native values do
 * not hold) */
byte Mode_Arg(PyObject *o, const char *func, nativeVal_t *out);
/* Py_None for 1, NULL for 0 */
PyObject *Mode_Result(byte ok);
/* the value as the result of an adapter (freed), NULL for 0 */
PyObject *Mode_Return(byte ok, nativeVal_t *v);

#endif
