/*
 * Native C helpers of the game mode ports (src/game/Script/ScriptModes.c): Net, Ffa and the online modes (NetModes.c),
 * the split screen (Split.c) and the single player super deals (SuperDeals.c). They call the engine in C on native
 * values. The engine functions the case files of these modules record or swap go through a seam (see
 * Native_Seam): Scrap.AddScheduledFunc, Scrap.DeleteScheduledFuncs, Scrap.Set, Scrap.Rand, Scrap.ConsoleError,
 * Scrap.Preload3DObject, SNet.SendUsrString, SNet.ModifyUsrData, SNet.IsClient, SNet.IsServer, SScorer.SetMsgText,
 * SScorer.SetLabelText, SScorer.SetMPFunc, SScorer.Set, SScorer.Fade, SInput.Bind, SSound.Play and SAI.GetStateVehicle
 * (Net seams Scrap.LoadLevel itself); SAI.SetStateVehicle and the odd values of SVec.Add and Rotate3D go through their
 * refs (stage 7). Imports stay with the module objects (OBJLIB 6). Every function returns 1, or 0 with the error of the
 * script raised.
 */
#ifndef SCRAP_SCRIPTMODES_H
#define SCRAP_SCRIPTMODES_H

#include "scrap/scriptnative.h"

/* ---- the values of a function ---- */

/* the native values one function works on: Mode_V takes one (None), Mode_Release frees those taken after a mark,
 * Mode_Done frees all and passes ok on */
#define MODE_VALS 48
typedef struct {
	nativeVal_t v[MODE_VALS];
	int n;
} modeVals_t;

nativeVal_t *Mode_V(modeVals_t *t);
void Mode_Release(modeVals_t *t, int mark);
byte Mode_Done(modeVals_t *t, byte ok);
/* a text value of t, an int (the ints in use stay in a table: an int owns nothing and never changes) */
nativeVal_t *Mode_S(modeVals_t *t, const char *s);
const nativeVal_t *Mode_I(modeVals_t *t, long i);
/* "<module>.<func>", a callback set by name */
nativeVal_t *Mode_Name(modeVals_t *t, const char *module, const char *func);
/* a + b + ... (n values) of the script into *out */
byte Mode_Cat(modeVals_t *t, nativeVal_t *out, int n, ...);

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

/* Scrap.UsrEntity(0): the entity of player 0, NULL for None */
cEntity *Mode_Usr(void);
/* Scrap.CreateEntity(name, x, y, z, cls): the entity (NULL for None); the TypeError of a name that is no text or a
 * coordinate that is no number */
byte Mode_Create(const nativeVal_t *name, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z,
	const char *cls, cEntity **out);
/* the same at a position tuple pos (pos[0], pos[1], pos[2] as the script indexes it) */
byte Mode_CreateAt(const nativeVal_t *name, const nativeVal_t *pos, const char *cls, cEntity **out);
/* Scrap.StartDummySearch(name, flag), Scrap.NextDummySearch() (None at the end, else (name, (x, y, z), (a, b),
 * flag)) */
void Mode_DummyStart(const char *name, int flag);
void Mode_DummyNext(nativeVal_t *out);
/* Scrap.Preload3DObject(name) */
byte Mode_Preload(const nativeVal_t *name);
/* print a, b, ... (n values, up to MODE_PRINT_MAX; Elevator.UseElevator prints 9) as the script's print statement
 * (OBJLIB 7: through Python's sys.stdout until the console takes C texts) */
#define MODE_PRINT_MAX 12
byte Mode_Print(const nativeVal_t *const *items, int n);
/* module.var = v of a Python object as it is (OBJLIB 5 (7e: a PyObject of an adapter); the value keeps its identity) */
byte Mode_SetModPy(const char *module, const char *var, PyObject *v);
/* e = Scrap.GetEntity(e.NextInSlot) of the loops over all entities (*e NULL at None) */
byte Mode_NextInSlot(cEntity **e);
/* handle-or-object helpers: o.var, o.var = v, o.SetModel(file) as the script does them on any value (an entity
 * handle in C, any other object the Python way with its errors) */
byte Mode_HGet(PyObject *o, const char *var, nativeVal_t *out);
byte Mode_HSet(PyObject *o, const char *var, const nativeVal_t *v);
byte Mode_HSetModel(PyObject *o, const char *module, const char *modelVar);
/* o.SetModel(file) with the file given */
byte Mode_HModel(PyObject *o, const nativeVal_t *file);
/* the same on a value (an entity handle, NV_ENT, in C without a Python object; another object the Python way; a native
 * value with the error of its getattr or setattr) */
byte Mode_VGet(const nativeVal_t *o, const char *var, nativeVal_t *out);
byte Mode_VSet(const nativeVal_t *o, const char *var, const nativeVal_t *v);
/* the entity of a handle the script holds by the name it was made with: var of it (an entity that is gone: the
 * console line and the SystemError of the handle) */
byte Mode_NameGet(const char *name, const char *var, nativeVal_t *out);
byte Mode_NameSet(const char *name, const char *var, const nativeVal_t *v);
/* var of the object at key of a dict variable of the module (dict[key].var), and its set */
byte Mode_AtGet(const char *module, const char *dict, const nativeVal_t *key, const char *var, nativeVal_t *out);
byte Mode_AtSet(const char *module, const char *dict, const nativeVal_t *key, const char *var, const nativeVal_t *v);

/* ---- engine calls through their seams ---- */

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
/* Scrap.AddScheduledFunc(time, func, args[, name]) through its seam: func a function value, args the n native values,
 * name NULL for none. The scheduler keeps a port function as its entry on the values, anything else as the value and
 * the tuple of the values; a recorder of the seam gets the value and the tuple */
byte Mode_ScheduleV(const nativeVal_t *time, const nativeVal_t *func, const nativeVal_t *args, int n,
	const nativeVal_t *name);
/* the same at Scrap.GetTime() + delay with func a global of the module (its NameError when it is gone) */
byte Mode_Later(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* the same with func the attribute of the module */
byte Mode_LaterIn(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* Scrap.Set(name, v) */
byte Mode_GVarSet(const char *name, const nativeVal_t *v);
byte Mode_GVarSetS(const char *name, const char *s);
byte Mode_GVarSetN(const char *name, double d);
/* Scrap.Rand(a, b), on values and on two floats */
byte Mode_Rand(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
byte Mode_RandD(double a, double b, nativeVal_t *out);
/* Scrap.ConsoleError(text) */
byte Mode_ConsoleError(const nativeVal_t *text);
/* SSound.Play(Sound.<sound>, Sound.<volume>) (the names are variables of the Sound module) */
byte Mode_Sound(const char *sound, const char *volume);
/* SSound.Play(sound, volume) on values */
byte Mode_Play(const nativeVal_t *sound, const nativeVal_t *volume);
/* SScorer.Clear(id), SScorer.Add(id, name, type) */
byte Mode_ScorerClear(const nativeVal_t *id);
byte Mode_ScorerAdd(const nativeVal_t *id, const nativeVal_t *name, const char *type);
/* SInput.SetActionSet(name), Scrap.SetTimeSpeed(speed), SSound.LoadSound(file) */
void Mode_SetActionSet(const char *name);
void Mode_SetTimeSpeed(double speed);
byte Mode_LoadSound(const nativeVal_t *file);
/* SVec.Add(a, b) and SVec.Rotate3D(v, rot) of two vectors (three numbers each): a vector; other values go to the
 * engine function by name for its error */
byte Mode_VecAdd(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
byte Mode_Rotate(const nativeVal_t *v, const nativeVal_t *rot, nativeVal_t *out);
/* SAI.GetStateVehicle(name) */
byte Mode_AIState(const nativeVal_t *name, nativeVal_t *out);

/* SScorer.SetMsgText (msg 1) or SetLabelText of Scrap.GetLangStr(key) until Scrap.GetTime() + secs */
byte Mode_Label(byte msg, const char *key, long secs);

/* ---- engine calls of the item and weapon ports ---- */

/* Scrap.GetNetFlags() (a tuple of three ints) through its seam (fc-weap-rg swaps it) */
byte Mode_NetFlags(nativeVal_t *out);
/* SVec.Sub(a, b), SVec.Mod(a), SVec.Norm(a) and SVec.Prod(a, f) on vectors (three numbers) and a number; other values
 * go to the engine function by name for its error */
byte Mode_VecSub(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
byte Mode_VecMod(const nativeVal_t *a, nativeVal_t *out);
byte Mode_VecNorm(const nativeVal_t *a, nativeVal_t *out);
byte Mode_VecProd(const nativeVal_t *a, const nativeVal_t *f, nativeVal_t *out);
/* SVec.ModSqr(a) (other values by name) */
byte Mode_VecModSqr(const nativeVal_t *a, nativeVal_t *out);
/* SSound.PlaySound(pos, name, volume) (other values by name, as above) */
byte Mode_PlaySound(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume);
/* Scrap.TestSphere(x, y, z, r, list[, exclude]) (exclude NULL: not given): the name of the entity hit, "" for the
 * level, None (other values by name) */
byte Mode_TestSphere(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const nativeVal_t *r,
	const nativeVal_t *list, const nativeVal_t *exclude, nativeVal_t *out);
/* Scrap.TestLine(from, to, list): ((x, y, z), name or "") or None (other values by name) */
byte Mode_TestLine(const nativeVal_t *from, const nativeVal_t *to, const nativeVal_t *list, nativeVal_t *out);
/* Scrap.SphereCall(x, y, z, r, list, func): func ("Module.Func") called for each entity of the list in the sphere
 * (other values by name) */
byte Mode_SphereCall(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const nativeVal_t *r,
	const nativeVal_t *list, const nativeVal_t *func);
/* the methods SetModel(file), SetFX(type), IsType(type) and Del() of the handle of entity e; e NULL is the None of
 * Scrap.GetEntity (its AttributeError). IsType and Del give 1 or 0, -1 with the error */
byte Mode_SetModel(cEntity *e, const nativeVal_t *file);
byte Mode_SetFX(cEntity *e, const char *type);
int Mode_IsType(cEntity *e, const char *type);
int Mode_Del(cEntity *e);
/* Scrap.UsrEntity(0) through its seam (fc-mis2 swaps it): the handle (NV_ENT) or None */
byte Mode_UsrHandle(nativeVal_t *out);
/* the handle of entity e as a value (NV_ENT: a new handle, no Python object), None for NULL */
byte Mode_EntHandle(cEntity *e, nativeVal_t *out);
/* Scrap.GetEntity(name) as the handle value (NV_ENT) or None */
byte Mode_EntityH(const nativeVal_t *name, nativeVal_t *out);
/* h.IsType(type), h.Del(), h.SetModel(file) of a value of the script: an entity handle in C (None and a handle whose
 * entity is gone with the errors of the script), another object the Python way. IsType gives 1 or 0, -1 with the
 * error */
int Mode_VIsType(const nativeVal_t *h, const char *type);
byte Mode_VDel(const nativeVal_t *h);
byte Mode_VModel(const nativeVal_t *h, const nativeVal_t *file);
/* h.method as a bound method (OBJLIB 7: what the scheduler keeps, e.g. l.Del, the Python way until entities are C): a
 * new reference, NULL with the error */
PyObject *Mode_VBound(const nativeVal_t *h, const char *method);
/* Scrap.Preload3DObject(name, 1, 1, 1) as the item scripts call it (through the seam with those arguments) */
byte Mode_PreloadOnes(const nativeVal_t *name);
/* Scrap.EntityListGet(list, var): a text (None for none), a float or an int; the engine's console line and NULL result
 * for a missing list, its AttributeError for a missing variable */
byte Mode_ListGet(const char *list, const char *var, nativeVal_t *out);
/* SLogic.SetOnFloor(name) (the TypeError of "s") */
byte Mode_SetOnFloor(const nativeVal_t *name);
/* Scrap.GetAlarm()[0] (the alarm is on) through its seam (fc-pk-dt swaps it): 1 or 0, -1 with the error */
int Mode_AlarmOn(void);
/* the SScorer.Set the C ports make on a text or number directly (Script_SetStr of the text up to its first 0 byte,
 * Script_SetNum), other values through SScorer.Set by name (its errors) */
byte Mode_ScorerPut(int id, const char *item, const char *var, const nativeVal_t *v);
/* SScorer.Get(id, item, var): a text, float, int or None */
void Mode_ScorerGet(int id, const char *item, const char *var, nativeVal_t *out);
/* Scrap.GetMoney(), Scrap.SetMoney(money) (the TypeError of "i") */
int Mode_GetMoney(void);
byte Mode_SetMoney(const nativeVal_t *money);
/* Scrap.GetSaveVar(name) (None when it is not there), Scrap.SetSaveVar(name, value) (the TypeError of "s") */
void Mode_SaveVar(const char *name, nativeVal_t *out);
byte Mode_SetSaveVar(const char *name, const nativeVal_t *value);

/* ---- module state ---- */

/* `import name` in a function of a script (Script_Import: a pack module from sys.modules in C since batch 6b; OBJLIB 7:
 * the module objects) */
byte Mode_Import(const char *name);
/* the global name of the module into slot (freed first), as the functions read it */
byte Mode_Read(const char *module, const char *name, nativeVal_t *slot);
/* Mode_VGet into a slot that may hold a value (it is freed first) */
byte Mode_VRead(const nativeVal_t *h, const char *var, nativeVal_t *slot);

/* the PyObject of a module variable as its functions read it (a new reference; the NameError of a missing global) */
PyObject *Mode_GlobalPy(const char *module, const char *name);
/* module.var = v of another module, through the variable registry (scrap/modvars.h) */
byte Mode_SetModVar(const char *module, const char *var, const nativeVal_t *v);
/* module.var of another module into *out (any native value; a TypeError for other objects) */
byte Mode_ModVar(const char *module, const char *var, nativeVal_t *out);
/* var[key] = v and del var[key] of a dict variable of a module (the dict object changed in place, as the script
 * does: the order of its keys stays Python's), the native copy in *d changed the same way; key and v stay the
 * caller's */
byte Mode_DictPut(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key, const nativeVal_t *v);
byte Mode_DictDel(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key);
/* var[key] = v and del var[key] of a dict variable of another module (read as its attribute) */
byte Mode_ModDictPut(const char *module, const char *var, const nativeVal_t *key, const nativeVal_t *v);
byte Mode_ModDictDel(const char *module, const char *var, const nativeVal_t *key);
/* a variable of the module = v (a value given to the dict as a new object; plain values); slot (NULL for none) the C
 * copy */
byte Mode_Put(const char *module, const char *var, nativeVal_t *slot, const nativeVal_t *v);
/* the value of a global of the module as its functions read it (the NameError of a missing one) */
byte Mode_G(const char *module, const char *name, nativeVal_t *out);
/* var.keys() and var.has_key(key) of a variable of a module (global: its own global, else its attribute), the Python
 * methods of whatever value is there */
byte Mode_Keys(const char *module, const char *var, byte global, nativeVal_t *out);
int Mode_HasKey(const char *module, const char *var, byte global, const nativeVal_t *key);
/* list.index(x) of the script (a value that is no list has no index method) */
byte Mode_Index(const nativeVal_t *list, const nativeVal_t *x, nativeVal_t *out);
/* var.append(v) of a list variable of the module, in place (the method of whatever value is there, as the script
 * calls it) */
byte Mode_Append(const char *module, const char *var, const nativeVal_t *v);
/* var.method(v) of a variable of a module (global as for Mode_Keys), in place: append, remove, ... */
byte Mode_VarCall(const char *module, const char *var, byte global, const char *method, const nativeVal_t *v);
/* a, b, ... = v: the n items of a sequence of n (the errors of unpacking) */
byte Mode_Unpack(const nativeVal_t *v, int n, nativeVal_t *out);

/* ---- shared functions of the modes ---- */

/* PurgeServerMapList() of Net and Split: only the maps of ServerMapList the server type has (ModesMap of the
 * module) */
byte Mode_PurgeServerMapList(const char *module);

/* ---- adapters ---- */

/* OBJLIB 7e2: the macros below still make the adapter of a PyMethodDef table next to the entry, [[maybe_unused]]: a
 * file whose table is a function table (portFn_t, scrap/scriptnative.h) uses only the entry. They go with the last
 * PyMethodDef table of the ports. */

/* the n arguments of an adapter (the arity error of PyArg_ParseTuple, up to 11) as native values of t, one after the
 * other from *a (Native_Arg: an entity handle as NV_ENT, other objects as NV_OBJ) */
byte Mode_Args(PyObject *args, int n, modeVals_t *t, nativeVal_t **a);
/* the result of an adapter: NULL for 0, None when out is None, else out as a Python value; t freed */
PyObject *Mode_Out(modeVals_t *t, byte ok, const nativeVal_t *out);
/* a function of a module (prefix pre) with nargs arguments (a) and the core call; out is the result: pre_name_E, its
 * native entry (portEntry_t: the direct calls of other ports, stage 3), and pre_name, its adapter for the PyMethodDef
 * table (the arguments as native values, the entry, the result as Python) */
#define MODE_FN(pre, name, nargs, call) \
	static byte pre##_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)a; \
		(void)out; \
		return n == (nargs) ? (call) : Port_Arity(n, nargs); \
	} \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *a, *out = Mode_V(&t); \
		(void)self; \
		if (!Mode_Args(args, nargs, &t, &a)) { \
			return Mode_Done(&t, 0), NULL; \
		} \
		return Mode_Out(&t, pre##_##name##_E(a, nargs, out), out); \
	}
/* the value as the result of an adapter (freed), NULL for 0 */
PyObject *Mode_Return(byte ok, nativeVal_t *v);

/* ---- helpers of the native super deals (SuperDeals.c) ---- */

/* var.values() of a variable of a module (global as for Mode_Keys), the Python method of whatever value is there */
byte Mode_Values(const char *module, const char *var, byte global, nativeVal_t *out);
/* var.remove(v) of a list variable of the module (its global), in place: the errors of the method */
byte Mode_Remove(const char *module, const char *var, const nativeVal_t *v);
/* Scrap.EntityListSet(list, var, value) on texts: the engine's console line and NULL result for a missing list */
byte Mode_ListSet(const char *list, const char *var, const char *value);
/* the three numbers of a vector or a tuple of three numbers into x, as "(ddd)" or "(fff)" takes them: 0 for other
 * values (no error set) */
byte Mode_VecOf(const nativeVal_t *v, double *x);

/* ---- the ship editor tabs (WeaponsTab, EnginesTab) ---- */

/* `v` of the script as a text value into *out */
void Mode_Repr(const nativeVal_t *v, nativeVal_t *out);
/* the item name of a scorer call of the ports (Sf_Text): the C text of a text value (up to its first 0 byte), the
 * TypeError "argument 2: expected string found" for any other value */
byte Mode_ItemText(const nativeVal_t *v, const char **out);
/* `from module import name` in a function of a script (OBJLIB 6: the import; the link functions of Menu it gives are
 * called through their entries, Mode_LinkCall): a new reference, NULL with the ImportError */
PyObject *Mode_From(const char *module, const char *name);
/* the value of HangarTab.prevShip (the handle as NV_ENT, None, or what a test put there; Native_Free it): 0 with the
 * AttributeError of the module attribute */
byte Mode_PrevShip(nativeVal_t *ship);
/* HangarTab.prevShip.<var> and its set: the ship being edited, read as the attribute of HangarTab at every use
 * (OBJLIB 7: a handle in a variable of another module) */
byte Mode_ShipGet(const char *var, nativeVal_t *out);
byte Mode_ShipSet(const char *var, const nativeVal_t *v);
/* ShipMaker.<table>[string.upper(HangarTab.prevShip.Model)][i] (the model read before the table, as the C ports read
 * it) */
byte Mode_ShipTable(const char *table, long i, nativeVal_t *out);
/* string.replace(s, old, new) of three texts (splitfields and joinfields: every old from the left, not overlapping;
 * old not empty) into *out */
void Mode_Replace(const nativeVal_t *s, const char *old, const nativeVal_t *with, nativeVal_t *out);
/* the count of range(n) as its argument parser takes it ("l": an int, a long int that fits, a float cut; the errors of
 * PyArg_Parse) */
byte Mode_Range(const nativeVal_t *n, long *count);
/* SScorer.Get2DPos(pos): the screen place (u, v) of a world point (Mode_VecOf); any other value goes to the engine
 * function by name (its TypeError) */
byte Mode_Get2DPos(const nativeVal_t *pos, nativeVal_t *out);
/* SScorer.GetTextArea(font, text): (width, height) of the text in the font (the TypeError of "s" for a text that is no
 * text) */
byte Mode_TextArea(const char *font, const nativeVal_t *text, nativeVal_t *out);

/* ---- helpers of the native level scripts (src/game/Script/levels) ---- */

/* SAI.GetRandomVisibilityPoint(): a random point of the AI graph (crt_rand as the engine function), None without
 * points */
void Mode_RandomVisPoint(nativeVal_t *out);
/* Scrap.SetAlarm(level) through its seam (cases record it with _bon): the TypeError of "f" for a value that is no
 * number */
byte Mode_SetAlarm(const nativeVal_t *level);
/* Scrap.Set(name, i) with an int, as the scripts set it (Mode_GVarSetN sets a float) */
byte Mode_GVarSetI(const char *name, long i);

/* ---- the native level files of batch G (Orbit, Townhall, GamblinDen, Scrapyard) ---- */

/* the lines of a call table of the level scripts (Sf_Calls on native values): "Mod.Func|arg|..." calls the attribute
 * Func of the module Mod, "@Func|..." the global Func of module (a mission may replace it), with the values of LvN_Value
 * (up to 8): the function there through Port_Values (a port's function through its entry; an engine function by name,
 * OBJLIB 7, and a recorder of a case through Python); 0 with the error of the first line that fails */
byte Mode_Calls(const char *module, const char *const *lines);
/* o.name = value for each "name=value" of pairs (Sf_SetAttrs on a value: the values of LvN_Value) */
byte Mode_SetAttrs(const nativeVal_t *o, const char *const *pairs);
/* Speech.NISpeech(who) (Speech.NISpeech() for who NULL), Speech.AutoAddNI(key), Speech.RunNI() (PORT_CALL) */
byte Mode_NISpeech(const char *who, const char *key);
/* Scrap.GetEntity(name).var = "text", = i (the TypeError of None when there is no such entity) */
byte Mode_EntSetS(const char *name, const char *var, const char *s);
byte Mode_EntSetI(const char *name, const char *var, long i);
/* Scrap.SetLockAlarm(lock) through its seam (fc-r6b-or2 records it): the TypeError of "i" for a value that is no
 * number */
byte Mode_SetLockAlarm(const nativeVal_t *lock);
/* the same with an int */
byte Mode_SetLockAlarmI(long lock);
/* o.var of a value as any value (Mode_VGet takes only native values from an object that is no entity handle: here a
 * function or an instance in the attribute is an NV_OBJ, OBJLIB 7, a handle an NV_ENT); the errors of the attribute */
byte Mode_VObj(const nativeVal_t *o, const char *var, nativeVal_t *out);

/* (a, b, c) of three ints */
void Mode_Ints3(nativeVal_t *out, long a, long b, long c);
/* a list of n texts (n < 0: the texts up to the NULL that ends them) */
void Mode_Texts(nativeVal_t *out, const char *const *texts, int n);
/* Scrap.GetLangStr(key) % SInput.GetDefinedEntry(0, "Walk", action, "Mouse") (None for a key without an entry) */
byte Mode_KeyText(const char *key, const char *action, nativeVal_t *out);
/* CharsNPC.CharRes[key] (the character objects of CharsNPC, instances of its class) by reference into *out: 1, 0 with
 * the error and *out None */
byte Mode_CharRes(const nativeVal_t *key, nativeVal_t *out);
/* Scrap.SetSaveVar(name, value) of each pair of texts (a NULL name ends them) */
void Mode_Saves(const char *const *pairs);
/* int(Scrap.GetSaveVar(name)) into out (its ValueError) */
byte Mode_SaveInt(const char *name, nativeVal_t *out);
/* a op b of the script (op "==", "!=", "<", "<=", ">", ">="), a op i: 1, 0, -1 with the error of a compare that
 * failed */
int Mode_Cmp(const nativeVal_t *a, const char *op, const nativeVal_t *b);
int Mode_CmpI(const nativeVal_t *a, const char *op, long i);
/* Scrap.UsrEntity(0).ActCtrl == ctrl: 1, 0, -1 with the error */
int Mode_UsrIs(const char *ctrl);
/* Speech.OnSpch(msg, ans[, how]) (how NULL: two arguments) and Speech.StdAnsLst([texts]) of n texts (n < 0: up to
 * the NULL), PORT_CALL */
byte Mode_Spch(const char *msg, const char *ans, const char *how);
byte Mode_AnsLst(const char *const *texts, int n);
/* SaveGame.MMDesc(desc, flag), SaveGame.QWayPoint(), MissionsFuncs.SetMissionTarget(name) (None for NULL), all
 * PORT_CALL */
byte Mode_MMDesc(const char *desc, long flag);
byte Mode_WayPoint(void);
byte Mode_Target(const char *name);
/* Elevator.ConnectElevators([a], [b]), or Elevator.ConnectElevators([a, b], [], 0) for pair (PORT_CALL) */
byte Mode_Connect(const char *a, const char *b, byte pair);
/* var.attr = "text", = i of the global var of module (a handle) */
byte Mode_GSetS(const char *module, const char *var, const char *attr, const char *s);
byte Mode_GSetI(const char *module, const char *var, const char *attr, long i);
/* Scrap.AddScheduledFunc(time, func, args) with time a value and func the global of module (its NameError; the entry
 * of a port's own function, Port_Scheduled) */
byte Mode_AtG(const nativeVal_t *time, const char *module, const char *func, const nativeVal_t *args, int n);
/* a op b of the script as Native_Arith, with the long ints of 1.5.2 for * / % too (a case can put a long int into a
 * variable; OBJLIB 9: those go through the Python number functions) */
byte Mode_Arith(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* Scrap.Set(name, Scrap.Def(name)): the engine variable back to its default */
byte Mode_GVarSetDef(const char *name);

/* ---- the native level files of batch H (Commercial, Police, Press) ---- */

/* the sprite behind a number of a mission scorer (Sf_MissionSprite on values): Add, X, Y, ScaleX 30, ScaleY 19, Unit 1,
 * Alpha, Red, Green, Blue */
byte Mode_MissionSprite(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, long alpha, long r,
	long g, long b);
/* Scorer.MissionScorerBar_SetDefaults(id, item, the back colour of Scorer, main, black); NULL main or black: the colours
 * of Scorer (PORT_CALL) */
byte Mode_BarDefaults(const nativeVal_t *id, const char *item, const nativeVal_t *main, const nativeVal_t *black);
/* import PhoneCab; pc = PhoneCab.GetFree() (the global pc of pcModule right after when given), pc.UseFunc = use and the
 * cab as the mission target (targetFirst: the target before the UseFunc): the cab into out (NULL: dropped) */
byte Mode_PhoneCab(const char *use, byte targetFirst, const char *pcModule, nativeVal_t *out);
/* a ship at a free spawn point: SpwPos = OutMap.GetFreeSpawnPos() (nearest: GetNearestFreeSpawnPos()); Scrap.CreateEntity(
 * name, SpwPos[0] (nearest: SpwPos) [0], [1], [2], "Car") through its seam: the handle into out, SpwPos into spw (NULL:
 * dropped) */
byte Mode_SpawnCar(const char *name, byte nearest, nativeVal_t *out, nativeVal_t *spw);
/* a mission item dropped at pos (money, amplifier, data, matrix): the model and radius are globals of module (radius
 * NULL: 3500), with the take sound and a mission arrow above it named arrow (NULL: the item's name + "CheckArrow") */
byte Mode_Drop(const char *module, const nativeVal_t *pos, const char *name, const char *model, const char *radius, long onlyUser,
	const char *onTake, const char *sound, const char *arrow);
/* a mission item taken: itm.rTime = 20, its Del 5 s later, its arrow deleted now (the names of both) */
byte Mode_Taken(const nativeVal_t *item, const nativeVal_t *arrow);
/* InitStatus of an outdoor map while the boss controls the robots: police and traffic 5 (comSats: none when the level
 * Init.UpperMap is in ComSatsMissionsMapsFinished, and OutPolice.PoliceActive = 0), no ships of the traffic */
byte Mode_BossTraffic(byte comSats);
/* Scrap.DelSaveVars(prefix) through its seam (fr7b.py records it) */
byte Mode_DelSaveVars(const char *prefix);
/* del name of a global of the module (the NameError of the script when it is not there) */
byte Mode_DelGlobal(const char *module, const char *name);
/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func, args[, name]) with delay a value of the script and func the
 * global of module (its NameError; the entry of a port's own function, Port_Scheduled) */
byte Mode_LaterV(const nativeVal_t *delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* Scrap.TestLine(from, to, list) through its seam (fc-r6b-cm1 records it), else Mode_TestLine */
byte Mode_TestLineSeam(const nativeVal_t *from, const nativeVal_t *to, const nativeVal_t *list, nativeVal_t *out);
/* `lst = eval(Scrap.GetSaveVar(var)); if not (item in lst): lst.append(item); Scrap.SetSaveVar(var, `lst`)` */
byte Mode_SaveAdd(const char *module, const char *var, const char *item);
/* a blueprint of a list of the hangar: Mode_SaveAdd of item, and when it was new the message msg (NULL: none; delay:
 * RaceMaker.ShowSystemMessageDelay(text, 0.25), else import RaceMaker; RaceMaker.ShowSystemMessage(text)) and
 * NewBluePrintAvaliable + 1 */
byte Mode_Blueprint(const char *module, const char *var, const char *item, const char *msg, byte delay);
/* SScorer.SetMsgText(cad, Scrap.GetTime() + Speech.CalculateSpeechTime(cad)) */
byte Mode_SpeechMsg(const nativeVal_t *cad);
/* the colour Scorer.MissionScorerText_Color of a scorer item: SScorer.Set(id, item, "Red", colour[0]) and Green, Blue,
 * Alpha */
byte Mode_TextColour(const nativeVal_t *id, const char *item);
/* GateKeeper.DoorFoe(gk, GateKeeper.<group>) with the group object itself (OBJLIB 5 (9c: DoorFoe is called the Python
 * way); the group lists of GateKeeper keep their identity) */
byte Mode_DoorFoe(const char *gk, const char *group);
/* Scrap.GetLangStr(key) of a value (the TypeError of "s" for one that is no text) into out */
byte Mode_LangV(const nativeVal_t *key, nativeVal_t *out);
/* SAct.CreateClass(name), SAct.GetClass(name) or SAct.CreateAction(name) through their seams (fn the name of the SAct
 * function; the result dropped) */
byte Mode_SAct(const char *fn, const char *name);
/* SAct.SetCls(name, v) (cls 1) or SAct.SetAct(name, v) through their seams, v the text s or the int i for s NULL */
byte Mode_SActSet(byte cls, const char *name, const char *s, long i);
/* the same with v = Init.Path + rest (the files in the folder of the level) */
byte Mode_PathSet(byte cls, const char *name, const char *rest);
/* Scrap.CreateEntity(dummy[0], dummy[1][0], dummy[1][1], dummy[1][2], type) through its seam, then e.Ang = (dummy[2][0],
 * dummy[2][1], 0): the handle into e */
byte Mode_FromDummy(const nativeVal_t *dummy, const char *type, nativeVal_t *e);
/* CGCalls() of the missions, the crazy gambler's call after a mission (unless the save variable SuperDeal.Library is
 * SuperDeal_Twelfth): Speech.AddRemoteCom(msgs[i], "CrazyGambler") for the i < n that equals
 * MissionsFuncs.GetNumCrazyDealCompleted() */
byte Mode_CGCalls(const char *const *msgs, int n);
/* the global name of module = name op v of the script (op of Native_Arith: '+', '-', ...), and with an int i */
byte Mode_GOp(const char *module, const char *name, char op, const nativeVal_t *v);
byte Mode_GOpI(const char *module, const char *name, char op, long i);

/* SSound.LoadSound(file) through its seam (fc-r7b-pr1 records it), else Mode_LoadSound */
byte Mode_LoadSoundSeam(const nativeVal_t *file);
/* a list or tuple held as one NV_OBJ (it holds handles) turned into a native list or tuple of its items in place (the
 * items as Native_Arg makes them); any other value stays */
void Mode_Elems(nativeVal_t *v);
/* the count of items a loop of the script takes from *v (Native_ForCount) after Mode_Elems */
int Mode_Loop(nativeVal_t *v);

/* ---- helpers of batch I (the super deals and the native level files of batch I) ---- */

/* h.var = i, = "text", = d of a handle value */
byte Mode_VSetI(const nativeVal_t *h, const char *var, long i);
byte Mode_VSetS(const nativeVal_t *h, const char *var, const char *s);
byte Mode_VSetF(const nativeVal_t *h, const char *var, double d);
/* x[i] of the script (a dict looks the int up; an NV_OBJ, e.g. a list that holds entity handles, the Python way, the
 * item as Native_Arg makes it) */
byte Mode_Item(const nativeVal_t *x, long i, nativeVal_t *out);
/* module.var of another module as a value (any object: a handle as NV_ENT, a function, a module as NV_OBJ) */
byte Mode_ModGet(const char *module, const char *var, nativeVal_t *out);
/* module.var = None */
byte Mode_ModNone(const char *module, const char *var);
/* SScorer.Add(id, item, type) */
byte Mode_ScorerAddS(const nativeVal_t *id, const char *item, const char *type);
/* SScorer.Set(id, item, var, i) */
byte Mode_ScorerSetI(const nativeVal_t *id, const char *item, const char *var, long i);
/* SScorer.Get(id, item, var) (the TypeError of "i" for an id that is no number) */
byte Mode_ScorerGetV(const nativeVal_t *id, const char *item, const char *var, nativeVal_t *out);
/* the text item of a mission scorer in Horatio with a shadow at the left (Lv_Text): X, y, text, visible (NULL: not
 * set) */
byte Mode_LvText(const nativeVal_t *id, const char *item, const nativeVal_t *y, const nativeVal_t *text, const nativeVal_t *visible);
/* Mode_LvText at Scorer.MissionScorerText_Y + dy with Scrap.GetLangStr(key) + ":" (visible: Visible 1) */
byte Mode_LvLabel(const nativeVal_t *id, const char *item, long dy, const char *key, byte visible);
/* Scrap.SetSaveVar(name, text) */
byte Mode_SaveS(const char *name, const char *text);
/* module.var = [] */
byte Mode_ModEmpty(const char *module, const char *var);
/* module.var = src.name of another module as the object it is (a function of that module) */
byte Mode_ModTo(const char *module, const char *var, const char *src, const char *name);
/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, Speech.<func>, args[, name]) of n values (name NULL: none) */
byte Mode_LaterSpeech(const char *func, const nativeVal_t *delay, const nativeVal_t *args, int n, const char *name);
/* the same with Speech.AddRemoteCom and a float delay (no name) */
byte Mode_LaterComF(double delay, const nativeVal_t *args, int n);
/* `num` + "/" + `max` of two globals of module (num read first) */
byte Mode_CountText(const char *module, const char *num, const char *max, nativeVal_t *out);
/* the timer of a mission scorer counting TimeSpecialUse, with the function of its end (visible: Visible 1) */
byte Mode_LvTimer(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, const char *expired, byte visible);
/* bp = eval(Scrap.GetSaveVar(var)); bp = bp + [a, b]; Scrap.SetSaveVar(var, `bp`) (eval in the globals of module) */
byte Mode_SaveCat2(const char *module, const char *var, const char *a, const char *b);
/* the same with the n texts of texts */
byte Mode_SaveCat(const char *module, const char *var, const char *const *texts, int n);
/* `e and e.Active`: 1, 0, -1 */
int Mode_Active(const nativeVal_t *e);
/* `for name in names: e = Scrap.GetEntity(name); if e: e.Del()` */
byte Mode_DelNames(const nativeVal_t *names);
/* Scrap.SetSaveVar(name, the global var of module) */
byte Mode_SaveG(const char *name, const char *module, const char *var);
/* module.var = [src.name] of a function of another module */
byte Mode_ModList1(const char *module, const char *var, const char *src, const char *name);
/* module.var = [the global name of module] */
byte Mode_ModListG(const char *module, const char *var, const char *src, const char *name);
/* `if fn: fn()` of the global function fn of module (*called: it was set) */
byte Mode_CallIf(const char *module, const char *name, byte *called);
/* apply(MissionsFuncs.SetNextMission, NextMissionData) of the module (Mode_CbApplySeq: the entry while the attribute
 * holds the port's function) */
byte Mode_ApplyNext(const char *module);
/* a op the global name of the module: 1, 0, -1 */
int Mode_CmpG(const nativeVal_t *a, const char *op, const char *module, const char *name);
/* a == i of the script (Native_Compare can not fail on native values) */
byte Mode_EqI(const nativeVal_t *a, long i);
/* raise "text" (a string exception of the script, its own object): 0 */
byte Mode_RaiseText(const char *text);
/* `usr and usr.Name == name`: 1, 0, -1 */
int Mode_UsrNameIs(const nativeVal_t *usr, const nativeVal_t *name);
/* module.var = the global name of src as the object it is (a function: the one a mission put there; OBJLIB 5 (9c:
 * functions as values)) */
byte Mode_ModSetGlobal(const char *module, const char *var, const char *src, const char *name);
/* the global name of the module op v, op i: 1, 0, -1 (a missing global: its NameError) */
int Mode_GCmp(const char *module, const char *name, const char *op, const nativeVal_t *v);
int Mode_GCmpI(const char *module, const char *name, const char *op, long i);
/* `usr and usr.IsType("Car")`: 1, 0, -1 */
int Mode_UsrInCar(const nativeVal_t *usr);
/* `x in` the global list name: 1, 0, -1 */
int Mode_InG(const nativeVal_t *x, const char *module, const char *name);
/* if x in list: list.remove(x) of a global list of the module */
byte Mode_RemoveIn(const nativeVal_t *x, const char *module, const char *name);
/* e = Scrap.GetEntity(Scrap.EntityListGet(list, "First")) as the handle value (the start of the loops over an entity
 * list) */
byte Mode_FirstOf(const char *list, nativeVal_t *e);
/* itm.rTime = 20 and its Del 5 s later (a mission item taken) */
byte Mode_DelLater(const nativeVal_t *itm);
/* the start of UsePhoneCab(UsrName, CharName, optId, FullOption) of the missions (a): Speech.NPCName and PlayerName,
 * the audio com of the cab, the global pc of module the cab; the alarm state (1, 0, -1 after an error) */
int Mode_PhoneUse(const char *module, const nativeVal_t *a);
/* SScorer.Set(0, Scrap.Get("MissionScorerName"), "ForceMap", map) (the map of the mission scorer) */
byte Mode_ForceMap(const char *map);
/* `string.find(string.upper(v.Model), text) != -1` (the native twin of string.find, PORT_CALL): 1, 0, -1 */
int Mode_ModelHas(const nativeVal_t *v, const char *text);

/* ---- the hubs of batch J (Traffic, OutMap, Challenge, CharConversor, RaceMaker, Doors) ---- */

/* len(v) of the script with a list or tuple held as one NV_OBJ (it holds handles) made native first (Mode_Elems):
 * -1 with the TypeError of a value without length */
int Mode_Len(nativeVal_t *v);
/* v[key] of the script (an NV_OBJ, e.g. a list that holds handles, the Python way, the item as Native_Arg makes it) */
byte Mode_Sub(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out);
/* SAI.GetNextRacePoint(pos, minDist, maxDist) through its seam (fc-om-dt swaps it): a vector, None outdoors without
 * a point (a pos that is no three numbers goes to the engine function by name for its error) */
byte Mode_RacePoint(const nativeVal_t *pos, long minDist, long maxDist, nativeVal_t *out);
/* Scrap.TestSphere(pos[0], pos[1], pos[2], r, list) through its seam (fc-om-dt swaps it), else Mode_TestSphere */
byte Mode_TestSpherePos(const nativeVal_t *pos, long r, const char *list, nativeVal_t *out);
/* Scrap.StartDummySearch(name, flag) and Scrap.NextDummySearch() through their seams (fc-om-dt, fc-abi and fc-acc-dt
 * swap them) */
byte Mode_DummyStartSeam(const char *name, long flag);
byte Mode_DummyNextSeam(nativeVal_t *out);
/* Scrap.SetGrid(x, y, z) of three ints through its seam (fc-om-dt records it) */
byte Mode_SetGrid(long x, long y, long z);
/* Scrap.GetEntity(name) through its seam (fc-om-dt swaps it): the handle value or None */
byte Mode_EntitySeam(const nativeVal_t *name, nativeVal_t *out);
/* SInput.SetActionSet(name) through its seam (fc-om-dt and the _bon lists of fr6b.py record it) */
byte Mode_ActionSet(const char *name);
/* Scrap.SetMoney(money) and Scrap.IncSaveVar(name) through their seams (fc-r7b-jy and fc-r7c-dt6 record them) */
byte Mode_SetMoneySeam(const nativeVal_t *money);
byte Mode_IncSaveVar(const char *name);
/* Scrap.SetTimeSpeed(speed) through its seam (fc-r7b-jy records it) */
byte Mode_TimeSpeed(double speed);
/* Scrap.GetCam(i): the name of the camera of player i (None when there is none) */
void Mode_CamName(int i, nativeVal_t *out);
/* Scrap.GetFloor(y): the floor of the map at the height (the TypeError of "f") */
byte Mode_Floor(const nativeVal_t *y, nativeVal_t *out);
/* abs(v) of the script (PyNumber_Absolute: ints, long ints, floats; the errors of 1.5.2) */
byte Mode_Abs(const nativeVal_t *v, nativeVal_t *out);
/* f(args...) of the script on a value f (a function or method held as NV_OBJ, or the TypeError of a value that is no
 * callable) with the n values of args: the result into out (NULL: dropped); the entry of a port's own function
 * (Mode_CbArgs, a site per call) */
#define Mode_CallArgs(f, out, args, n) Mode_CbArgs(PORT_VSITE(), f, out, args, n)
/* -v of the script (PyNumber_Negative: ints, long ints, floats; the errors of 1.5.2) */
byte Mode_Neg(const nativeVal_t *v, nativeVal_t *out);

/* SScorer.Fade(id, (r, g, b, a), (r, g, b, a), time) with the time a value of the script (the recorders see the int the
 * script passes; the TypeError of "f" for one that is no number); Mode_Fade passes a float */
byte Mode_FadeV(long id, const int *from, const int *to, const nativeVal_t *time);

/* ---- the hubs of batch K (CharAct, Alarm, NewsPanel, Scene, Speech) ---- */

/* x in seq of the script (Native_Contains; a list held as one NV_OBJ or an NV_OBJ x the Python way): 1 or 0, -1 with
 * the error */
int Mode_In(const nativeVal_t *seq, const nativeVal_t *x);
/* h.SetFX(type) of a value of the script (an entity handle in C, None and a handle whose entity is gone with the errors
 * of the script, another object the Python way) */
byte Mode_VSetFX(const nativeVal_t *h, const char *type);

/* SAct.<fn>(name) with the name a value of the script, through the seam: fn GetClass, CreateClass, CreateAction, GetAct
 * or DelClass, the result into out (the TypeError of "s" for a name that is no text) */
byte Mode_SActV(const char *fn, const nativeVal_t *name, nativeVal_t *out);
/* SInput.SetActionSet(name) of a value through its seam; SInput.GetActionSet() as a text value */
byte Mode_ActionSetV(const nativeVal_t *name);
void Mode_ActionSetName(nativeVal_t *out);
/* Scrap.OpenPack(path), and Scrap.ClosePack() for path NULL, through their seams (fr7b.py records them) */
byte Mode_Pack(const nativeVal_t *path);
/* SScorer.SetMsgText(text, time) and SetLabelText(text, time) with the time a value of the script, through the seams */
byte Mode_MsgTextV(const nativeVal_t *text, const nativeVal_t *time);
byte Mode_LabelTextV(const nativeVal_t *text, const nativeVal_t *time);
/* SScorer.SetSpeechText(text, time, r, g, b), SetSpeechCallback(func), CancelSpeech() and SetHeadMonitor(a, b, c)
 * through their seams (the cases record them) */
byte Mode_SpeechText(const nativeVal_t *text, const nativeVal_t *time, const nativeVal_t *r, const nativeVal_t *g,
	const nativeVal_t *b);
byte Mode_SpeechCallback(const char *func);
/* SScorer.SetSpeechCallback(func) with the function name a value of the script (the TypeError of "s") */
byte Mode_SpeechCallbackV(const nativeVal_t *func);
byte Mode_CancelSpeech(void);
byte Mode_HeadMonitor(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c);
/* the voice channels through their seams: SSound.StopVoice(ch), PlayVoice(ch[, text]) (text NULL: not given),
 * OpenVoice(file, ch[, arg]), VoiceRemain(ch, default) into out, SetVoiceVolume(ch, vol) and StopAllSounds() */
byte Mode_StopVoice(const nativeVal_t *ch);
byte Mode_PlayVoice(const nativeVal_t *ch, const nativeVal_t *text);
byte Mode_OpenVoice(const nativeVal_t *file, const nativeVal_t *ch, const nativeVal_t *arg);
byte Mode_VoiceRemain(const nativeVal_t *ch, const nativeVal_t *def, nativeVal_t *out);
byte Mode_VoiceVolume(const nativeVal_t *ch, const nativeVal_t *vol);
byte Mode_StopAllSounds(void);
/* Scrap.GetFirst(): the handle value of the first entity, None when there is none */
byte Mode_First(nativeVal_t *out);

/* dict[key] of a dict variable of the module as the object it is (a list in it changed in place): 1 with *out
 * (Native_Free it), 0 with the error (the NameError of the global, the errors of the subscript) */
byte Mode_AtVal(const char *module, const char *dict, const nativeVal_t *key, nativeVal_t *out);
/* the arguments of a keyword function (fmt and names of PyArg_ParseTupleAndKeywords, n of them, up to 8) as native
 * values into a (None when not given, given[i] 0 then; given may be NULL) */
byte Mode_Kw(PyObject *args, PyObject *kw, const char *fmt, char **names, int n, nativeVal_t *a, byte *given);
/* the n arguments of a keyword function freed and the result as the adapter returns it (out NULL or None: None; out
 * freed) */
PyObject *Mode_KwOut(nativeVal_t *a, int n, byte ok, nativeVal_t *out);

/* SScorer.PreloadTexture(name) through its seam */
byte Mode_PreloadTexture(const nativeVal_t *name);

/* apply(f, args) of the script on values: the result into out (NULL: dropped); the entry of a port's own function on
 * a tuple (Mode_CbApply, a site per call) */
#define Mode_Apply(f, args, out) Mode_CbApply(PORT_VSITE(), f, args, out)
/* the list of a global of the module changed in place as the C ports change it: v appended (first: inserted at 0), and
 * item i deleted (other modules and the cases hold the list) */
byte Mode_ListAddG(const char *module, const char *var, const nativeVal_t *v, byte first);
byte Mode_DelItemG(const char *module, const char *var, int i);
/* ---- the vehicle modules of batch L (Vehicles, VehiclesNPC, ShipMaker, SputnikInterface) ---- */

/* `if module.var:` of a variable of another module (any object, read as its attribute): 1, 0, -1 with the error */
int Mode_ModTrue(const char *module, const char *var);

/* module.var.append(gmod.name), the list changed in place */
byte Mode_AppendG(const char *module, const char *var, const char *gmod, const char *name);

/* SScorer.Set(0, item, var, v), and with an int */
byte Mode_Set0(const char *item, const char *var, const nativeVal_t *v);
byte Mode_Set0I(const char *item, const char *var, long i);
/* ---- the menus of batch O (MenuBase, MenuControls, MenuVideo, MenuVideoVars) ---- */

/* the failure of a call (ok 0) printed as the menus printed it ("Script: <module>.<func> failed" and the error;
 * OBJLIB 7): ok passed on */
byte Mode_Printed(byte ok, const char *module, const char *func);
/* PORT_CALL(Module, Func, NULL, fmt, ...) with its failure printed so (Script_Call, Mode_PrintedCall of a known name)
 * */
#define MODE_PRINTED(module, name, ...) \
	Mode_Printed(Port_Invoke(PORT_SITE(port_##module##_##name, 0), NULL, __VA_ARGS__), #module, #name)

/* Scrap.Get(name) through its seam: the value as Native_GVarGet gives it (the TypeError of "s" for a name that is no
 * text) */
byte Mode_GVarGetV(const nativeVal_t *name, nativeVal_t *out);
/* SScorer.Get(id, item, var) and SScorer.Add(id, name, type) with the id and the name values of the script, through
 * their seams (the TypeErrors of "iss") */
byte Mode_ScorerGetVal(const nativeVal_t *id, const nativeVal_t *item, const char *var, nativeVal_t *out);
byte Mode_ScorerAddV(const nativeVal_t *id, const nativeVal_t *name, const char *type);
/* SInput.ResetToDefault() through its seam */
byte Mode_ResetKeys(void);

/* a list or tuple and each of its items made native item by item where they are held as one NV_OBJ (Mode_Elems: the
 * menus of [text, function, ...] entries); the objects in them stay NV_OBJ */
void Mode_ElemsIn(nativeVal_t *v);

/* SSound.PlaySound(pos, name, volume) through its seam */
byte Mode_PlaySoundSeam(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume);

/* ---- the character hubs of batch M (MakeChar, Chars, CharFuncs) ---- */

/* module.var as Script_GetInt and Script_GetFloat read it: an int (a float cut) or a number, else def; no error (a
 * missing variable or module is def too) */
long Mode_ModGetI(const char *module, const char *var, long def);
double Mode_ModGetN(const char *module, const char *var, double def);
/* module.var = v and = i as Script_SetVar sets it (an error is dropped) */
void Mode_ModPut(const char *module, const char *var, const nativeVal_t *v);
void Mode_ModPutI(const char *module, const char *var, long i);

/* ---- batch P (CharsNPC, GameSounds, MenuMission) ---- */

/* the $names of a .def section as the dict object the runner takes, into *out (Native_Free it): the n pairs that
 * follow, each a name and a const nativeVal_t * (the names are short texts, up to MODE_DEFVARS). For sections that
 * share one dict (a let of the first is seen by the next); one section runs with Mode_DefRun */
void Mode_DefVars(nativeVal_t *out, int n, ...);

/* ---- the menus of batch Q (MenuMain, MenuHud) ---- */

/* SScorer.SetCinema(id, on, time), Scrap.GetCam(i) (the camera name or None) and Scrap.UsrEntity(id) (the handle value or
 * None; the NULL result of a player above 3) through their seams */
byte Mode_SetCinema(long id, long on, double time);
byte Mode_CamSeam(long i, nativeVal_t *out);
byte Mode_UsrSeam(long id, nativeVal_t *out);
/* module.func(args...) at the time (as a float) on the scheduler itself, not through Scrap.AddScheduledFunc (the menus
 * schedule this way: a recorder of the cases does not see it); name NULL for none; a missing function dropped. The
 * scheduler keeps the function as Mode_ScheduleV does */
byte Mode_SchedAt(double time, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* `if module.var: module.var(id)` of a callback variable (Menu.MissionScorerCall): the error of the call printed, every
 * error dropped */
void Mode_CallVarIf(const char *module, const char *var, long id);
/* Scrap.CreateEntity(type, x, y, z, name) with int places (the handle value or None) and Scrap.Exit() through their
 * seams */
byte Mode_CreateSeam(const char *type, long x, long y, long z, const char *name, nativeVal_t *out);
byte Mode_Exit(void);

/* ---- the multiplayer menus of batch R (MenuNet) ---- */

/* module.var as Script_GetString reads it: its text (up to the first 0 byte) cut to size, else "" (no error; a missing
 * variable or module is "" too); out */
const char *Mode_ModGetS(const char *module, const char *var, char *out, int size);

/* ---- the hubs of batch S (Parking, Accelerator, InitLevel) ---- */

/* eval(text) in the globals of module as the ports read the game's texts (a literal, or a bare name looked up in the
 * module): Mode_EvalSave */
byte Mode_EvalIn(const char *module, const nativeVal_t *text, nativeVal_t *out);

/* Scrap.EntityListSet(list, var, value) on texts through its seam, else Mode_ListSet */
byte Mode_ListSetSeam(const char *list, const char *var, const char *value);

/* ---- the hubs of batch T (MissionsFuncs, SaveGame) ---- */

/* SInput.GetActionSet() through its seam (fc-r6-ib swaps it): the name of the action set as a text value */
byte Mode_ActionSetGet(nativeVal_t *out);
/* eval(text) of a save variable in the globals of module, as Script_EvalIn reads it: a literal with the native reader,
 * any other text (a name, a name chain) and the errors through Script_EvalIn (OBJLIB 6), its result as Native_Arg makes
 * it */
byte Mode_EvalSave(const char *module, const nativeVal_t *text, nativeVal_t *out);
/* the literal of a Python text as eval reads it (a string without 0 bytes, after its leading blanks) with the native
 * reader: 1 with *out (Native_Free it), else 0 without an error and *out None */
byte Mode_LiteralPy(PyObject *text, nativeVal_t *out);
/* eval(text) of a game text in the globals of module as a Python object (a new reference, NULL with the error), for
 * the readers whose result stays a Python object: a literal of the native reader made the Python value; any other text
 * (a name, a name chain, the errors) and a value that holds a dict through Script_EvalIn (OBJLIB 6: Native_ToPy lays
 * a dict out in its slot order, where Python's table can order two colliding keys otherwise) */
PyObject *Mode_EvalPy(const char *module, PyObject *text);

/* ---- shared after stage 2 (the copies the batches wrote, joined) ---- */

/* print v, print text: Mode_Print of one value (the text of any length) */
byte Mode_PrintV(const nativeVal_t *v);
byte Mode_PrintS(const char *text);
/* module.func(id) by a name known at run time (MODE_PRINTEDCALL: the attribute through the entry of the port function
 * it holds, else the Python way, as Mode_CbAttr; MODE_PRINTED for a known name), the failure printed as Mode_Printed
 * prints it */
void Mode_PrintedCall(portSite_t *site, const char *module, const char *func, long id);
#define MODE_PRINTEDCALL(module, func, id) Mode_PrintedCall(PORT_VSITE(), module, func, id)
/* len(seq) as PySequence_Length takes it: -1 with its TypeError */
int Mode_SeqLen(const nativeVal_t *seq);
/* the tab editors (EnginesTab, WeaponsTab, HullTab, ShipEdit): Scrap.GetLangStr(key) % module.UseTxt; the Red, Green
 * and Blue of a menu item from c[0..2]; its Visible; fn(id, a, b) of a link function of Menu on texts of C; a sound of
 * the Sound module played, SSound.Play(Sound.<name>Sound, Sound.<name>Volume) */
byte Mode_LangUse(const char *module, const char *key, nativeVal_t *out);
void Mode_ItemColor(int id, const char *item, const int *c);
void Mode_ItemVisible(int id, const char *item, int v);
/* fn(id, a, b) of a link function of Menu (a from import): its entry while it is Menu's own (Mode_CbLink) */
#define Mode_LinkCall(fn, id, a, b) Mode_CbLink(PORT_VSITE(), fn, id, a, b)
byte Mode_SoundOf(const char *name);
/* the int value i in v (v returned); (a, b, c) of copies of three values; Scrap.Rand(a, b) as the C ports draw it
 * (the C directly); the entry of a script function that does nothing (any arguments, None) */
const nativeVal_t *Mode_IntVal(nativeVal_t *v, long i);
void Mode_Tuple3(nativeVal_t *out, const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c);
void Mode_RandF(double a, double b, nativeVal_t *out);
byte Mode_PyNone_E(const nativeVal_t *a, int n, nativeVal_t *out);
/* the Apply button of the option menus flashes once something changed (1) */
byte Mode_ApplyFlash(int id);
/* Scrap.StartDummySearch(name, flag) with the name a value of the script, through its seam (the TypeError of "s" for
 * a text with 0 bytes) */
byte Mode_DummyStartV(const nativeVal_t *name, long flag);
/* module.var = a text (of any length) as Script_SetVar sets it: an error is dropped */
void Mode_ModPutS(const char *module, const char *var, const char *s);
/* a == b, a != b, a > b, a < b, a >= b, a <= b of the script (op '=', '!', '>', '<', 'g', 'l'): 1 or 0, -1 with the
 * error (any error set after the compare, also for equal values); and against the int i */
int Mode_CmpOp(const nativeVal_t *a, const nativeVal_t *b, char op);
int Mode_CmpOpI(const nativeVal_t *a, long i, char op);
/* h.var as any value (Mode_VObj) into a slot that may hold one (it is freed first) */
byte Mode_VObjRead(const nativeVal_t *h, const char *var, nativeVal_t *slot);
/* the global name of module (a list) .append(h.Name), in place */
byte Mode_AppendName(const char *module, const char *name, const nativeVal_t *h);
/* h.Ang = (0, x, 0) */
byte Mode_SetAng(const nativeVal_t *h, const nativeVal_t *x);
/* the system message (Mode_SpeechMsg) of the language text of key */
byte Mode_LangMsg(const char *key);
/* the entry of a script function of two arguments that does nothing (the arity error of "OO", None) */
byte Mode_PyNone2_E(const nativeVal_t *a, int n, nativeVal_t *out);
/* Flag.var = v with Flag the global of the module (OBJLIB 7: the flag is the entity handle in the module dict) */
byte Mode_FlagSet(const char *module, const char *var, const nativeVal_t *v);
/* Scrap.GetEntity(Scrap.GetCam(0)) through the seam of GetEntity; its EntityLink = link */
byte Mode_Cam0(nativeVal_t *out);
byte Mode_CamLink(const nativeVal_t *link);
/* Scrap.GetEntity(<the global name var of the module>) as the handle value (None for none) */
byte Mode_GEntity(const char *module, const char *var, nativeVal_t *out);
/* Scrap.GetSaveVar(name) and Scrap.SetSaveVar(name, v) of a name value (the TypeError of "s"); Scrap.SetSaveVar(name,
 * Scrap.GetSaveVar(from)) (the TypeError of "s" for a missing one) */
byte Mode_SaveVarV(const nativeVal_t *name, nativeVal_t *out);
byte Mode_SetSaveV(const nativeVal_t *name, const nativeVal_t *v);
byte Mode_SaveCopy(const char *name, const char *from);

/* ---- the native entries of stage 3, batch A2b ---- */

/* 1 when min <= n <= max, else 0 with the TypeError of PyArg_ParseTuple for n arguments ("function requires at least
 * 2 arguments; 1 given") */
byte Mode_Arity(int n, int min, int max);
/* the least (*min) and the most (*max) arguments of a PyArg format: letters and groups at the top level, the least
 * those before the '|' */
void Mode_FmtCount(const char *fmt, int *min, int *max);
/* the arguments of an adapter with min required and up to max in all ("O..|O.." of PyArg_ParseTuple, max up to 11) as
 * native values of t, one after the other from *a (those not given None): the count given, -1 with the error */
int Mode_OptArgs(PyObject *args, int min, int max, modeVals_t *t, nativeVal_t **a);
/* MODE_FN for a function with optional arguments: the entry checks min <= n <= max, the adapter takes them with
 * Mode_OptArgs; call sees n, the count given, and MODE_OPT(i) is argument i or NULL when it is not given */
#define MODE_OPT(i) (n > (i) ? &a[i] : NULL)
#define MODE_FNOPT(pre, name, min, max, call) \
	static byte pre##_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)a; \
		(void)out; \
		return Mode_Arity(n, min, max) ? (call) : 0; \
	} \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *a, *out = Mode_V(&t); \
		(void)self; \
		int n = Mode_OptArgs(args, min, max, &t, &a); \
		if (n < 0) { \
			return Mode_Done(&t, 0), NULL; \
		} \
		return Mode_Out(&t, pre##_##name##_E(a, n, out), out); \
	}
/* MODE_FN for a function that does not look at its arguments (any count, none parsed) */
#define MODE_FNANY(pre, name, call) \
	static byte pre##_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)a; \
		(void)n; \
		(void)out; \
		return (call); \
	} \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *out = Mode_V(&t); \
		(void)self; \
		(void)args; \
		return Mode_Out(&t, pre##_##name##_E(NULL, 0, out), out); \
	}
/* ---- native entries of hand and keyword adapters (stage 3, batch A2a) ---- */

/* the number of v as "i" of PyArg reads it: Native_ArgInt, and an NV_OBJ through PyInt_AsLong as getargs.c has it
 * (an instance's __int__): 1 with *out, else 0 with the error */
byte Mode_ArgInt(const nativeVal_t *v, int *out);
/*
 * PyArg_ParseTuple on the n native arguments of an entry, with its arity and conversion errors: "i" into an int *
 * (Mode_ArgInt), "l" into a long * (the same, not cut), "d" into a double * (Native_ArgDouble, an NV_OBJ through
 * PyFloat_AsDouble), "f" into a float * (the same, cut to a float), "s" into a const char ** (the text of an NV_STR
 * without 0 bytes), "S" into a const nativeVal_t ** (an NV_STR of any bytes), "O" into a const nativeVal_t ** (the
 * argument itself), "|" before the optional ones (their outputs keep what they hold), "(...)" a sequence of that many
 * items ("i", "l", "d" and "f" items only). 1, or 0 with the error
 */
byte Mode_Parse(const nativeVal_t *a, int n, const char *fmt, ...);
/* the conversion errors of fmt on the n slots of a keyword call (Mode_KwSkip: a slot not given, its item skipped), in
 * slot order and without outputs (the errors vgetargskeywords gives before an unknown keyword): 1, or 0 with the
 * error */
byte Mode_ParseSlots(const nativeVal_t *a, int n, const char *fmt);
/* the adapter of a function whose entry checks its own arguments (Mode_Parse, Port_Arity): the arguments as native
 * values (Native_Arg), the entry, the result as Python (None for None, NULL for 0) */
PyObject *Mode_Entry(PyObject *args, portEntry_t entry);
/* the same for a method of an object (the methods of the entity handles): self as the first argument of the entry,
 * the arguments of the call after it (self NULL: Mode_Entry) */
PyObject *Mode_SelfEntry(PyObject *self, PyObject *args, portEntry_t entry);
/* the core of a keyword function: its arguments by slot, NULL for one not given (the core applies the default) */
typedef byte (*modeKwCore_t)(const nativeVal_t *const *a, nativeVal_t *out);
/* the entry of such a function: the n arguments given in order (the arity error of fmt), the others not given, and
 * an argument skipped with Mode_KwSkip not given (a required one: the arity error of the keywords) */
byte Mode_KwEntry(const nativeVal_t *a, int n, const char *fmt, modeKwCore_t core, nativeVal_t *out);
/* argument i of the n of a keyword entry is given: i < n and not skipped (Mode_KwSkip) */
byte Mode_KwGiven(const nativeVal_t *a, int n, int i);
/* a function with its adapter named fn whose first argument is an int id ("i"), nargs arguments in all (the others
 * native values in a): fn_E, its entry, and fn, its adapter */
#define MODE_IFN(fn, nargs, call) \
	static byte fn##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		int id; \
		(void)out; \
		return n != (nargs) ? Port_Arity(n, nargs) : Mode_ArgInt(&a[0], &id) && (call); \
	} \
	[[maybe_unused]] static PyObject *fn(PyObject *self, PyObject *args) { \
		(void)self; \
		return Mode_Entry(args, fn##_E); \
	}

/* ---- the native entries of stage 3 batch A1 (hand, keyword and PyArg adapters on their entries) ---- */

/* pre_name, the PyArg adapter of the entry pre_name_E (Mode_Entry) */
#define MODE_ADAPTER(pre, name) \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		return Mode_Entry(args, pre##_##name##_E); \
	}
/* an argument of a keyword entry that is not given (a keyword call skips it): NV_OBJ without an object, a value no
 * conversion makes; Mode_KwEntry and Mode_KwSlots take it as not given */
void Mode_KwSkip(nativeVal_t *v);
/* the n arguments of a keyword entry in the slots of fmt (one letter O per slot, | before the optional ones): v[i] the
 * value a[i] (a view, not freed) with given[i] 1, or None with given[i] 0 for one not given (i >= n or Mode_KwSkip);
 * the arity error of PyArg_ParseTupleAndKeywords for a count beyond the slots or a required one not given */
byte Mode_KwSlots(const nativeVal_t *a, int n, const char *fmt, nativeVal_t *v, byte *given);
/* only the arity errors of Mode_KwSlots: a keyword entry whose optional slots end the format (no gap after the n
 * arguments) parses a and n with Mode_Parse after it */
byte Mode_KwArity(const nativeVal_t *a, int n, const char *fmt);
/* slot i of Mode_KwSlots a default when it is not given: a text (shorter than NATIVE_TEXT), an int, a float */
void Mode_KwDefS(nativeVal_t *v, const byte *given, int i, const char *s);
void Mode_KwDefI(nativeVal_t *v, const byte *given, int i, long l);
void Mode_KwDefF(nativeVal_t *v, const byte *given, int i, double f);
/* a keyword adapter on its entry: the arguments by name (Mode_Kw with fmt and names), the ones not given as
 * Mode_KwSkip, the entry, the result as Mode_KwOut gives it */
PyObject *Mode_KwAdapter(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry);
/* the native twins of the functions of string (strop) and math that the ports call, through PORT_CALL(string, find,
 * ...): the same results and errors as Python 1.5.2's; no seam (OBJLIB 9: nothing replaces them), an argument that
 * holds an NV_OBJ and SCRAP_PORT_SLOW=1 call the module's function by name */
PORT_DECL(string, find)
PORT_DECL(string, replace)
PORT_DECL(string, atoi)
PORT_DECL(string, atof)
PORT_DECL(string, split)
PORT_DECL(string, splitfields)
PORT_DECL(math, sqrt)
PORT_DECL(math, fabs)
PORT_DECL(math, fmod)
/* a keyword function of a module (prefix pre): the format (letters O, | before the optional ones, up to 8) and the
 * names of its arguments, then the body of pre_name_K(v, given, out) on the slots of Mode_KwSlots (the defaults of the
 * script with Mode_KwDef*): pre_name_E, its entry, pre_name, its adapter (Mode_KwAdapter), pre_name_N, the names, and
 * pre_name_F, the format (the keyword calls through its ref, PORT_DEFK) */
#define MODE_KW(pre, name, fmt, ...) \
	static char *pre##_##name##_N[] = { __VA_ARGS__, NULL }; \
	[[maybe_unused]] static const char pre##_##name##_F[] = fmt; \
	static byte pre##_##name##_K([[maybe_unused]] nativeVal_t *v, [[maybe_unused]] const byte *given, \
		[[maybe_unused]] nativeVal_t *out); \
	static byte pre##_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		nativeVal_t v[8]; \
		byte given[8]; \
		return Mode_KwSlots(a, n, fmt, v, given) && pre##_##name##_K(v, given, out); \
	} \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args, PyObject *kw) { \
		(void)self; \
		return Mode_KwAdapter(args, kw, fmt, pre##_##name##_N, pre##_##name##_E); \
	} \
	static byte pre##_##name##_K([[maybe_unused]] nativeVal_t *v, [[maybe_unused]] const byte *given, \
		[[maybe_unused]] nativeVal_t *out)

/* ---- the menus of stage 3 batch B11 ---- */

/* the arguments (id[, control]) of an entry as Script_Args reads them: control "" when it is no text */
byte Mode_IdControl(const nativeVal_t *a, int n, int *id, const char **control);
/* a menu function (id[, control]) with its adapter named fn (Script_Args): fn_E, its entry, and fn, its adapter
 * (Mode_Entry); call sees id and control */
#define MODE_IDFN(fn, call) \
	static byte fn##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		int id; \
		const char *control; \
		(void)out; \
		return Mode_IdControl(a, n, &id, &control) && (call); \
	} \
	[[maybe_unused]] static PyObject *fn(PyObject *self, PyObject *args) { \
		(void)self; \
		return Mode_Entry(args, fn##_E); \
	}

/*
 * Calls of values (stage 4 of docs/objlib-plan.md, batch 4d; the Mode_Cb prefix): a function a variable, a table, a
 * from import or a parameter holds, called through the entry of the port function it is (Port_Value: by its adapter,
 * in any module), else through Python with the same arguments and errors (OBJLIB 7). site is the counter of the call
 * site (PORT_VSITE); the macros give each call site its own.
 *   Mode_CbArgs     f(args...) of a native value f with n values (Mode_CallArgs)
 *   Mode_CbApply    apply(f, args) of native values; Mode_CbApplyPy of a function object (MODE_CBAPPLY): a tuple
 *                   through the entry, any other args through Python (Script_CallObject: its TypeError)
 *   Mode_CbApplySeq apply(fn, args) as the builtin apply takes args (MODE_CBAPPLYSEQ): a tuple or a list through the
 *                   entry (more than PORT_ARGS items through Python), any other value through __builtin__.apply by
 *                   name (a text or another sequence, the TypeError of a value that is none)
 *   Mode_CbLink     fn(id, a, b) of a link function (Mode_LinkCall)
 *   Mode_CbGlobal   name(...) of the global of module (Native_CallGlobal: the NameError of the script; MODE_CBGLOBAL)
 *   Mode_CbAttr     module.name(...) by a name known at run time (Native_CallMod; MODE_CBATTR)
 * fmt and the arguments as Native_Call takes them (no keywords).
 */
byte Mode_CbArgs(portSite_t *site, const nativeVal_t *f, nativeVal_t *out, const nativeVal_t *args, int n);
byte Mode_CbApply(portSite_t *site, const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out);
byte Mode_CbApplyPy(portSite_t *site, PyObject *fn, const nativeVal_t *args, nativeVal_t *out);
byte Mode_CbApplySeq(portSite_t *site, PyObject *fn, const nativeVal_t *args, nativeVal_t *out);
byte Mode_CbLink(portSite_t *site, PyObject *fn, int id, const char *a, const char *b);
byte Mode_CbGlobal(portSite_t *site, const char *module, const char *name, nativeVal_t *out, const char *fmt, ...);
byte Mode_CbAttr(portSite_t *site, const char *module, const char *name, nativeVal_t *out, const char *fmt, ...);
#define MODE_CBAPPLY(fn, args, out) Mode_CbApplyPy(PORT_VSITE(), fn, args, out)
#define MODE_CBAPPLYSEQ(fn, args, out) Mode_CbApplySeq(PORT_VSITE(), fn, args, out)
#define MODE_CBGLOBAL(module, name, out, ...) Mode_CbGlobal(PORT_VSITE(), module, name, out, __VA_ARGS__)
#define MODE_CBATTR(module, name, out, ...) Mode_CbAttr(PORT_VSITE(), module, name, out, __VA_ARGS__)

/* ---- the state of the online and split screen modes on values (stage 6, batch 6f) ---- */

/* `if v:` of the script with the error the truth of an object can raise (PyObject_IsTrue; Native_True drops it): 1 or
 * 0, -1 with the error */
int Mode_Truth(const nativeVal_t *v);
/* h.SetModel(<the global modelVar of module>) of a value of the script (Mode_HSetModel: the handle looked at before
 * the global is read) */
byte Mode_VSetModel(const nativeVal_t *h, const char *module, const char *modelVar);
/* module.var = o.name as the object it is, o a value of the script (the module a variable holds; OBJLIB 5 (9c: a module
 * as a value); a list of that module keeps its identity) */
byte Mode_ModToV(const char *module, const char *var, const nativeVal_t *o, const char *name);
/* <the global var of module>.name as any value, for the module a variable holds (ModLib; OBJLIB 5 (9c: a module as a
 * value)): the attribute as PyObject_GetAttrString gives it (an object that fails without an exception, the handle of a
 * deleted entity, leaves none set: the caller's function then fails as a call without an error does; Mode_VObj sets
 * ceval's SystemError) */
byte Mode_GObjAttr(const char *module, const char *var, const char *name, nativeVal_t *out);
/* the global var of module = the handle of entity e (None for NULL; OBJLIB 7: the module keeps the handle) */
byte Mode_PutHandle(const char *module, const char *var, cEntity *e);
/* Flag.var with Flag the global of the module (Mode_FlagSet's reader; OBJLIB 7) */
byte Mode_FlagGet(const char *module, const char *var, nativeVal_t *out);

/* ---- the ports without Python objects (stage 6, batch 6g) ---- */

/* the adapter of a keyword function with a typed format (fmt and names of PyArg_ParseTupleAndKeywords; the letters
 * s, i, d, f and O, up to PORT_ARGS): the arguments parsed with fmt for its errors in their order, then the entry on
 * them as native values (a text, an int and a float as the parse converted them, an object as Native_Arg makes it,
 * one not given as Mode_KwSkip), the result as Mode_KwOut gives it */
PyObject *Mode_KwTyped(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry);
/* MODE_KW for a function with a typed format: the body gets the slots as they were given (Mode_KwSlots) and converts
 * them with Mode_Parse (the defaults of the script set first: Mode_KwDef*). MODE_KWBY is the form with the adapter
 * and the function named (pasted in the macro the file writes: a module name can be a macro of the file); MODE_REFKW
 * (below) uses it with the adapter by reference */
#define MODE_KWBY(adapter, fn, fmt, ...) MODE_KWBYF(adapter, fn, fmt, fmt, __VA_ARGS__)
/* MODE_KWBY with the text of fn_F given: "&" in front of the format for an entry that takes its arguments by
 * reference (MODE_REFKW: Port_KwCall hands the values over as they are) */
#define MODE_KWBYF(adapter, fn, fmt, ffmt, ...) \
	static char *fn##_N[] = { __VA_ARGS__, NULL }; \
	[[maybe_unused]] static const char fn##_F[] = ffmt; \
	static byte fn##_K([[maybe_unused]] nativeVal_t *v, [[maybe_unused]] const byte *given, \
		[[maybe_unused]] nativeVal_t *out); \
	static byte fn##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		nativeVal_t v[PORT_ARGS]; \
		byte given[PORT_ARGS]; \
		return Mode_KwSlots(a, n, fmt, v, given) && fn##_K(v, given, out); \
	} \
	[[maybe_unused]] static PyObject *fn(PyObject *self, PyObject *args, PyObject *kw) { \
		(void)self; \
		return adapter(args, kw, fmt, fn##_N, fn##_E); \
	} \
	static byte fn##_K([[maybe_unused]] nativeVal_t *v, [[maybe_unused]] const byte *given, \
		[[maybe_unused]] nativeVal_t *out)
#define MODE_KWT(pre, name, fmt, ...) MODE_KWBY(Mode_KwTyped, pre##_##name, fmt, __VA_ARGS__)
/* `from module import name` as a value (Mode_From: the ImportError of a name the module does not have; an object as
 * NV_OBJ) */
byte Mode_FromV(const char *module, const char *name, nativeVal_t *out);
/* v[lo:hi] of the script: Native_Slice, an NV_OBJ (an instance, a list that holds handles) the Python way
 * (PySequence_GetSlice, the result as Native_Arg makes it) */
byte Mode_Slice(const nativeVal_t *v, long lo, long hi, nativeVal_t *out);
/* v[a:b] is defined for the type of v (a text, a list, a tuple, an object whose type slices: the sq_slice slot) */
byte Mode_Sliceable(const nativeVal_t *v);

/*
 * Values held by reference: a list or dict that several holders share and change in place (a module variable other
 * modules and the cases hold, the item of such a list), a class instance, a function, a module. Mode_RefG and
 * Mode_RefMod give the variable's value as the registry holds it (an object as its NV_REF, a list or dict of plain
 * values too; Mode_G and Mode_ModGet give a copy of those); the Mode_Ref functions work on the object in place, with
 * the errors of the Python operation: a native object (NObj_Native) natively, a view or another Python value
 * (OBJLIB 7) the Python way, a value by value as the Python value it makes (a new object).
 */
/* the object o as a reference into out (o released): 0 for NULL, with its error. MODE_REFASK: the result of a port
 * function by reference (PORT_ASK: what a function put in its place returns keeps its identity) */
byte Mode_RefTake(PyObject *o, nativeVal_t *out);
#define MODE_REFASK(module, name, out, ...) Mode_RefTake(PORT_ASK(module, name, __VA_ARGS__), out)
/* the global name of module (its NameError) and module.var of another module (its AttributeError) by reference (the
 * registry's value: ModVar_GlobalVal, ModVar_ModAttrVal) */
byte Mode_RefG(const char *module, const char *name, nativeVal_t *out);
byte Mode_RefMod(const char *module, const char *var, nativeVal_t *out);
/* the value a reference holds as the functions read values (Native_Arg: a copy of a list, tuple or dict of plain
 * values, any other object stays the reference) */
void Mode_RefPlain(const nativeVal_t *o, nativeVal_t *out);
/* len(o) as PySequence_Length takes it (-1 with the error) and as PyObject_Length does (a dict too) */
int Mode_RefLen(const nativeVal_t *o);
int Mode_RefSize(const nativeVal_t *o);
/* o[i] as PySequence_GetItem gives it and o[key] of the script (PyObject_GetItem), the item by reference */
byte Mode_RefItem(const nativeVal_t *o, long i, nativeVal_t *out);
byte Mode_RefSub(const nativeVal_t *o, const nativeVal_t *key, nativeVal_t *out);
/* o[i] = v as PySequence_SetItem sets it, o[key] = v and del o[key] of the script, in place */
byte Mode_RefSetItem(const nativeVal_t *o, long i, const nativeVal_t *v);
byte Mode_RefSetSub(const nativeVal_t *o, const nativeVal_t *key, const nativeVal_t *v);
byte Mode_RefDelSub(const nativeVal_t *o, const nativeVal_t *key);
/* o.name by reference (PyObject_GetAttrString: no error is set where the object sets none), o.name = v */
byte Mode_RefAttr(const nativeVal_t *o, const char *name, nativeVal_t *out);
byte Mode_RefSetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v);
/* o.method(args...) of n values (an NV_OBJ argument is the object itself) as the ports call a method (Script_Method:
 * the method read as Mode_RefGetAttr reads an attribute), the result as Native_Arg makes it into out (NULL: dropped) */
byte Mode_RefCall(const nativeVal_t *o, const char *method, const nativeVal_t *args, int n, nativeVal_t *out);
/* PyList_Insert(o, at, v) and PyList_Append(o, v) (at < 0) as the C ports changed a list (the SystemError of a value
 * that is no list) */
byte Mode_RefInsert(const nativeVal_t *o, int at, const nativeVal_t *v);
/* module.var = v with a reference kept as the object it is (Mode_SetModPy) */
byte Mode_RefToMod(const char *module, const char *var, const nativeVal_t *v);
/* d.has_key(key) of a value of the script (the method of whatever value it is): 1, 0, -1 with the error */
int Mode_RefHasKey(const nativeVal_t *d, const nativeVal_t *key);
/* h.method as the bound method by reference (Mode_VBound: what the scheduler keeps, e.g. l.Del; OBJLIB 7) */
byte Mode_VBoundV(const nativeVal_t *h, const char *method, nativeVal_t *out);
/* eval(text) of a name with attributes and subscripts (NAME(.NAME | [n])*, as the game passes them) in a function of
 * the module of x: the variables and imports of x are the locals of the function, then the globals of the module and
 * the builtins (Script_NameChain; OBJLIB 6: the lookup on the dicts): 1 with *out (as Native_Arg makes it), 0 with the
 * error of the lookup, -1 without an error for a text that is no such name (*out None) */
int Mode_EvalChain(const execScope_t *x, const char *text, nativeVal_t *out);
/* the module name as a value (Script_Module: its object by reference, OBJLIB 7; the ImportError when it is not
 * there; *out None then), e.g. for dir(module) */
byte Mode_RefModule(const char *name, nativeVal_t *out);
/* the pending error is a KeyError (the except KeyError of a script): dropped (1), any other error stays (0) */
byte Mode_CatchKeyError(void);
/* str() of each of the n values (up to MODE_PRINT_MAX), then all written to sys.stdout with a blank between and a
 * newline after, without the soft space of the print statement (what the C port of Speech.AbortNPCSpeech printed;
 * OBJLIB 7: Python's stdout); 0 with the error of a str or a write (none set when sys.stdout is gone) */
byte Mode_WriteLine(const nativeVal_t *const *items, int n);
/* o.name as the ports read an attribute (Script_GetAttr: the rules of a module's attributes, the SystemError of an
 * object that fails without an error), by reference */
byte Mode_RefGetAttr(const nativeVal_t *o, const char *name, nativeVal_t *out);
/* Mode_Entry with every argument by reference (an NV_OBJ that holds the caller's object, also a number, a text or a
 * list of plain values): the adapter of a function that keeps or changes the object it is given (OBJLIB 5 (7e:
 * adapters) and 7). MODE_REFADAPTER(pre, name) is that adapter of the entry pre_name_E */
PyObject *Mode_RefEntry(PyObject *args, portEntry_t entry);
#define MODE_REFADAPTER(pre, name) \
	[[maybe_unused]] static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		return Mode_RefEntry(args, pre##_##name##_E); \
	}
/* item i as a for loop of the script takes it from o (Script_Next): 1 with the item by reference, 0 at the IndexError
 * that ends the loop, -1 with another error */
int Mode_RefNext(const nativeVal_t *o, int i, nativeVal_t *out);
/* the operators of the script on any values the Python way, the result by reference (string.py's functions work on
 * whatever they are given): a op b for '+', '-', '*', '/', '%' (PyNumber_Add and the others); cmp(a, b) as -1, 0 or 1,
 * -2 with the error (PyObject_Compare); repr(o); o[lo:hi] (PySequence_GetSlice); o is None */
byte Mode_RefOp(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
int Mode_RefCmp(const nativeVal_t *a, const nativeVal_t *b);
byte Mode_RefRepr(const nativeVal_t *o, nativeVal_t *out);
byte Mode_RefSlice(const nativeVal_t *o, long lo, long hi, nativeVal_t *out);
byte Mode_RefIsNone(const nativeVal_t *o);
/* Mode_KwTyped with the O arguments by reference (the keyword adapter of a function that works on the objects it is
 * given, as MODE_REFADAPTER) */
PyObject *Mode_RefKwAdapter(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry);
/* MODE_KWT with that adapter */
#define MODE_REFKW(pre, name, fmt, ...) MODE_KWBYF(Mode_RefKwAdapter, pre##_##name, fmt, "&" fmt, __VA_ARGS__)
/* f(args...) of a value by reference with n values: a native object natively (NObj_Call), any other the Python call
 * of the object (Script_CallObject on the tuple of the values: a bound method, a function of a table; Mode_CallArgs
 * takes the entry of a port function), the result by reference into out (NULL: dropped) */
byte Mode_RefApply(const nativeVal_t *f, const nativeVal_t *args, int n, nativeVal_t *out);
/* del o.name of the script (PyObject_SetAttrString with no value: the AttributeError of a missing one) */
byte Mode_RefDelAttr(const nativeVal_t *o, const char *name);
/* long(v) of the script (OBJLIB 9: PyNumber_Long, the result as Native_Arg makes it) */
byte Mode_ToLong(const nativeVal_t *v, nativeVal_t *out);

/* ---- the menus, the ship editor and the other ports on the native helpers (stage 6, batch 6h) ---- */

/* Script_RunFile (stop: Script_RunFileTry) of a .def section with its $names given as values: the n pairs that follow,
 * each a name and a const nativeVal_t * (up to MODE_DEFVARS; the runner takes them as a dict object, Mode_DefVars).
 * 0 as the runner gives it */
#define MODE_DEFVARS 24
struct scriptStep_s;
byte Mode_DefRun(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, int n, ...);
/* the same with the pairs as a va_list (for a file's own variadic run function) */
byte Mode_DefRunVa(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, int n, va_list va);
/* the same with the names and the values as arrays */
byte Mode_DefRunV(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, const char *const *names, const nativeVal_t *const *vals, int n);
/* fn(id, a, b) of a link function of Menu held as a value (Mode_LinkCall: its entry while it is Menu's own) */
byte Mode_CbLinkV(portSite_t *site, const nativeVal_t *fn, int id, const char *a, const char *b);
#define Mode_LinkCallV(fn, id, a, b) Mode_CbLinkV(PORT_VSITE(), fn, id, a, b)
/* str(v) of the script added to b: a native value as Native_Repr NR_STR writes it, an object's str the Python way
 * (OBJLIB 7): 0 with the error of its __str__ left set and nothing added */
byte Mode_StrBuf(const nativeVal_t *v, nativeBuf_t *b);
/* the int of a value as PyInt_AsLong gives it (an int, a float cut, a long int that fits, an object's __int__;
 * OBJLIB 7): -1 with the error set for the rest */
long Mode_AsLong(const nativeVal_t *v);
/* a $name of the .def file that runs = v, for its steps (Script_SetValue) */
void Mode_SetValue(const char *name, const nativeVal_t *v);
/* fn(args...) at the time (as a float) on the scheduler itself, with the function a value (Mode_SchedAt reads it from
 * the module; the entry of a port function, else the call of the value on the tuple of the values): always 1 */
byte Mode_SchedFn(double time, const nativeVal_t *fn, const nativeVal_t *args, int n, const char *name);
/* var.method() of a variable of a module (global as for Mode_Keys), in place, the result dropped (sort of a list;
 * Mode_VarCall takes one argument) */
byte Mode_VarCall0(const char *module, const char *var, byte global, const char *method);
/* a & b of the script into *out (None when it fails): two ints in C, other values through PyNumber_And (OBJLIB 9:
 * long ints, the TypeError of other operands) */
byte Mode_And(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* a * b of the script the Python way for every operand, into *out (None when it fails; OBJLIB 9: PyNumber_Multiply
 * on whatever the values are; Mode_Arith multiplies plain numbers in C) */
byte Mode_Times(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* module.var[key] of a variable of another module (read as its attribute) as any value (an instance, a function as
 * NV_OBJ, a handle as NV_ENT; the errors of the attribute and of the subscript): Native_Free it; *out None when it
 * fails */
byte Mode_ModAt(const char *module, const char *var, const nativeVal_t *key, nativeVal_t *out);
/* a + b of the script the Python way for every operand, into *out (None when it fails; OBJLIB 9: PyNumber_Add, an
 * instance's __add__ or __radd__; Native_Arith concatenates native values in C) */
byte Mode_Plus(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* v[key] = x of the script on a native value (OBJLIB 9: the item assignment of the Python value with its errors; *v
 * is the changed value then) */
byte Mode_SetItem(nativeVal_t *v, const nativeVal_t *key, const nativeVal_t *x);
/* a text value pasted between quotes reads back as itself (Script_IsQuotable: the exec lines the ports make into
 * direct sets); 0 for any other value */
byte Mode_Quotable(const nativeVal_t *text);
/* apply(f, args) as the builtin apply takes args, with the function a value (Mode_CbApplySeq) */
byte Mode_CbApplySeqV(portSite_t *site, const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out);
#define MODE_CBAPPLYSEQV(f, args, out) Mode_CbApplySeqV(PORT_VSITE(), f, args, out)
/* int(v) of the script into *out: Native_ToInt (numbers and texts), an object through PyNumber_Int (OBJLIB 9: an
 * instance's __int__) */
byte Mode_Int(const nativeVal_t *v, nativeVal_t *out);
/* `v` of the script as a text value with the error of an object's __repr__ (NObj_Repr; Mode_Repr drops it): 0 with
 * that error and *out None */
byte Mode_ReprV(const nativeVal_t *v, nativeVal_t *out);
/* the global var of module = src.name of another module as the object it is (OBJLIB 5 (9c: functions as values): a
 * function a module keeps to put back later; Mode_ModTo sets the attribute of another module) */
byte Mode_PutFromMod(const char *module, const char *var, const char *src, const char *name);
/* var.method(a, b) of a variable of a module (global as for Mode_Keys), in place, the result dropped (the append of
 * 1.5.2 with two arguments, which appends the pair) */
byte Mode_VarCall2(const char *module, const char *var, byte global, const char *method, const nativeVal_t *a,
	const nativeVal_t *b);

#endif
