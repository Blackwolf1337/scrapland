/*
 * Native C helpers of the game mode ports (src/game/Script/ScriptModes.c): Net, Ffa and the online modes
 * (NetModes.c), the split screen (Split.c) and the single player super deals (SuperDeals.c). They call the engine in
 * C on native values. The engine functions the case files of these modules record or swap go through a seam
 * (OBJLIB 7, see Native_Seam): Scrap.AddScheduledFunc, Scrap.DeleteScheduledFuncs, Scrap.Set, Scrap.Rand,
 * Scrap.ConsoleError, Scrap.Preload3DObject, SNet.SendUsrString, SNet.ModifyUsrData, SNet.IsClient, SNet.IsServer,
 * SScorer.SetMsgText, SScorer.SetLabelText, SScorer.SetMPFunc, SScorer.Set, SScorer.Fade, SInput.Bind, SSound.Play and
 * SAI.GetStateVehicle (Net seams Scrap.LoadLevel itself; SAI.SetStateVehicle and the odd values of SVec.Add and
 * Rotate3D are called by name). Imports stay with the module objects (OBJLIB 6). Every function returns 1, or 0 with
 * the error of the script raised.
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
/* print a, b, ... (n values, up to 8) as the script's print statement (OBJLIB 7: through Python's sys.stdout until
 * the console takes C texts) */
byte Mode_Print(const nativeVal_t *const *items, int n);
/* module.var = v of a Python object as it is (OBJLIB 5: a value passed between modules keeps its identity) */
byte Mode_SetModPy(const char *module, const char *var, PyObject *v);
/* e = Scrap.GetEntity(e.NextInSlot) of the loops over all entities (*e NULL at None) */
byte Mode_NextInSlot(cEntity **e);
/* the handle a script holds for the entity (OBJLIB 7: entities are Python handles in module variables and in the
 * arguments of other ports until entities are native values): a new reference, None for NULL */
PyObject *Mode_Handle(cEntity *e);
/* handle-or-object helpers: o.var, o.var = v, o.SetModel(file) as the script does them on any value (an entity
 * handle in C, any other object the Python way with its errors) */
byte Mode_HGet(PyObject *o, const char *var, nativeVal_t *out);
byte Mode_HSet(PyObject *o, const char *var, const nativeVal_t *v);
byte Mode_HSetModel(PyObject *o, const char *module, const char *modelVar);
/* o.SetModel(file) with the file given */
byte Mode_HModel(PyObject *o, const nativeVal_t *file);
/* the same on a value (an NV_OBJ handle, or a native value with the error of its getattr or setattr) */
byte Mode_VGet(const nativeVal_t *o, const char *var, nativeVal_t *out);
byte Mode_VSet(const nativeVal_t *o, const char *var, const nativeVal_t *v);
/* the entity of a handle the script holds by the name it was made with: var of it (an entity that is gone: the
 * console line and the SystemError of the handle) */
byte Mode_NameGet(const char *name, const char *var, nativeVal_t *out);
byte Mode_NameSet(const char *name, const char *var, const nativeVal_t *v);
/* var of the object at key of a dict variable of the module (dict[key].var), and its set */
byte Mode_AtGet(const char *module, const char *dict, const nativeVal_t *key, const char *var, nativeVal_t *out);
byte Mode_AtSet(const char *module, const char *dict, const nativeVal_t *key, const char *var, const nativeVal_t *v);

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
/* the same at Scrap.GetTime() + delay (func not taken over, name NULL for none) */
byte Mode_LaterPy(double delay, PyObject *func, const nativeVal_t *args, int n, const char *name);
/* the same with func a global of the module (its NameError when it is gone) */
byte Mode_Later(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name);
/* the same with func the attribute of the module (Script_LaterIn of the ports) */
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
/* SSound.Play(Sound.<sound>, Sound.<volume>) (OBJLIB 5: the names are variables of the Sound module) */
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
/* Scrap.UsrEntity(0) through its seam (fc-mis2 swaps it): the handle (NV_OBJ, OBJLIB 7) or None */
byte Mode_UsrHandle(nativeVal_t *out);
/* the handle of entity e as a value (NV_OBJ, OBJLIB 7), None for NULL */
byte Mode_EntHandle(cEntity *e, nativeVal_t *out);
/* Scrap.GetEntity(name) as the handle value (NV_OBJ, OBJLIB 7) or None */
byte Mode_EntityH(const nativeVal_t *name, nativeVal_t *out);
/* h.IsType(type), h.Del(), h.SetModel(file) of a value of the script: an entity handle in C (None and a handle whose
 * entity is gone with the errors of the script), another object the Python way. IsType gives 1 or 0, -1 with the
 * error */
int Mode_VIsType(const nativeVal_t *h, const char *type);
byte Mode_VDel(const nativeVal_t *h);
byte Mode_VModel(const nativeVal_t *h, const nativeVal_t *file);
/* h.method as a bound method (OBJLIB 4: what the scheduler keeps, e.g. l.Del): a new reference, NULL with the error */
PyObject *Mode_VBound(const nativeVal_t *h, const char *method);
/* Scrap.Preload3DObject(name, 1, 1, 1) as the item scripts call it (OBJLIB 7: through the seam with those arguments) */
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

/* `import name` in a function of a script (OBJLIB 6: module loading stays with the module objects) */
byte Mode_Import(const char *name);
/* the global name of the module into slot (freed first), as the functions read it */
byte Mode_Read(const char *module, const char *name, nativeVal_t *slot);

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
/* a, b, ... = v: the n items of a sequence of n (the errors of unpacking) */
byte Mode_Unpack(const nativeVal_t *v, int n, nativeVal_t *out);

/* ---- shared functions of the modes ---- */

/* PurgeServerMapList() of Net and Split: only the maps of ServerMapList the server type has (ModesMap of the
 * module) */
byte Mode_PurgeServerMapList(const char *module);

/* ---- adapters ---- */

/* the n arguments of an adapter (the arity error of PyArg_ParseTuple, up to 11) as native values of t, one after the
 * other from *a (Native_Arg: other objects as NV_OBJ) */
byte Mode_Args(PyObject *args, int n, modeVals_t *t, nativeVal_t **a);
/* the result of an adapter: NULL for 0, None when out is None, else out as a Python value; t freed */
PyObject *Mode_Out(modeVals_t *t, byte ok, const nativeVal_t *out);
/* the entry of a function of module mod (prefix pre) with nargs arguments (a) and the core call; out is the result */
#define MODE_FN(pre, name, nargs, call) \
	static PyObject *pre##_##name(PyObject *self, PyObject *args) { \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *a, *out = Mode_V(&t); \
		(void)self; \
		(void)out; \
		if (!Mode_Args(args, nargs, &t, &a)) { \
			return Mode_Done(&t, 0), NULL; \
		} \
		(void)a; \
		return Mode_Out(&t, call, out); \
	}
/* Py_None for 1, NULL for 0 */
PyObject *Mode_Result(byte ok);
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
/* `from module import name` in a function of a script (OBJLIB 3: the link functions of Menu by name): a new
 * reference, NULL with the ImportError */
PyObject *Mode_From(const char *module, const char *name);
/* the value of HangarTab.prevShip (the handle as NV_OBJ, None, or what a test put there; Native_Free it): 0 with the
 * AttributeError of the module attribute */
byte Mode_PrevShip(nativeVal_t *ship);
/* HangarTab.prevShip.<var> and its set: the ship being edited, read as the attribute of HangarTab at every use
 * (OBJLIB 5 and 7: a handle in a variable of another module) */
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
 * Func of the module Mod (OBJLIB 3 and 7: other ports and the engine by name, a case may record either), "@Func|..." the
 * global Func of module (a mission may replace it), with the values of LvN_Value (up to 8); 0 with the error of the
 * first line that fails */
byte Mode_Calls(const char *module, const char *const *lines);
/* o.name = value for each "name=value" of pairs (Sf_SetAttrs on a value: the values of LvN_Value) */
byte Mode_SetAttrs(const nativeVal_t *o, const char *const *pairs);
/* Speech.NISpeech(who) (Speech.NISpeech() for who NULL), Speech.AutoAddNI(key), Speech.RunNI() (OBJLIB 3) */
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
 * function, an instance or a handle in the attribute is an NV_OBJ, OBJLIB 7); the errors of the attribute */
byte Mode_VObj(const nativeVal_t *o, const char *var, nativeVal_t *out);

/* (a, b, c) of three ints */
void Mode_Ints3(nativeVal_t *out, long a, long b, long c);
/* a list of n texts (n < 0: the texts up to the NULL that ends them) */
void Mode_Texts(nativeVal_t *out, const char *const *texts, int n);
/* Scrap.GetLangStr(key) % SInput.GetDefinedEntry(0, "Walk", action, "Mouse") (None for a key without an entry) */
byte Mode_KeyText(const char *key, const char *action, nativeVal_t *out);
/* CharsNPC.CharRes[key] (OBJLIB 5: the character objects of CharsNPC): a new reference, NULL with the error */
PyObject *Mode_CharRes(const nativeVal_t *key);
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
 * the NULL), OBJLIB 3 */
byte Mode_Spch(const char *msg, const char *ans, const char *how);
byte Mode_AnsLst(const char *const *texts, int n);
/* SaveGame.MMDesc(desc, flag), SaveGame.QWayPoint(), MissionsFuncs.SetMissionTarget(name) (None for NULL), OBJLIB 3 */
byte Mode_MMDesc(const char *desc, long flag);
byte Mode_WayPoint(void);
byte Mode_Target(const char *name);
/* Elevator.ConnectElevators([a], [b]), or Elevator.ConnectElevators([a, b], [], 0) for pair (OBJLIB 3) */
byte Mode_Connect(const char *a, const char *b, byte pair);
/* var.attr = "text" of the global var of module (a handle) */
byte Mode_GSetS(const char *module, const char *var, const char *attr, const char *s);
/* Scrap.AddScheduledFunc(time, func, args) with time a value and func the global of module (its NameError; OBJLIB 4) */
byte Mode_AtG(const nativeVal_t *time, const char *module, const char *func, const nativeVal_t *args, int n);
/* a op b of the script as Native_Arith, with the long ints of 1.5.2 for * / % too (a case can put a long int into a
 * variable; OBJLIB 9: those go through the Python number functions) */
byte Mode_Arith(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* Scrap.Set(name, Scrap.Def(name)): the engine variable back to its default */
byte Mode_GVarSetDef(const char *name);

/* ---- the native level files of batch H (Commercial, Police, Press) ---- */

/* SScorer.Set(0, item, var, v) and the same with an int, through the seam */
byte Mode_Set0(const char *item, const char *var, const nativeVal_t *v);
byte Mode_Set0I(const char *item, const char *var, long i);
/* Scorer.MissionScorerText_X + d, Scorer.MissionScorerText_Y + d (d 0: the value itself) */
byte Mode_TextX(long d, nativeVal_t *out);
byte Mode_TextY(long d, nativeVal_t *out);
/* a text of a mission scorer as the missions set it (Sf_MissionText on values): SScorer.Add(id, item, type) (type NULL:
 * none), X, Y, Font, Effect (NULL: none), Text, Align, the colour Scorer.MissionScorerText_Color and Visible (NULL: none) */
byte Mode_MissionText(const nativeVal_t *id, const char *item, const char *type, const nativeVal_t *x, const nativeVal_t *y,
	const char *font, const char *effect, const nativeVal_t *text, const char *align, const nativeVal_t *visible);
/* the sprite behind a number of a mission scorer (Sf_MissionSprite on values): Add, X, Y, ScaleX 30, ScaleY 19, Unit 1,
 * Alpha, Red, Green, Blue */
byte Mode_MissionSprite(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, long alpha, long r,
	long g, long b);
/* Scorer.MissionScorerBar_SetDefaults(id, item, the back colour of Scorer, main, black); NULL main or black: the colours
 * of Scorer (OBJLIB 3) */
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
/* import Traffic; Scrap.SetSaveVar("Traffic.AcShips", ships); Traffic.AcShips = eval(Scrap.GetSaveVar("Traffic.AcShips"))
 * in the globals of module */
byte Mode_AcShips(const char *module, const char *ships);
/* Scrap.DelSaveVars(prefix) through its seam (fr7b.py records it) */
byte Mode_DelSaveVars(const char *prefix);
/* del name of a global of the module (the NameError of the script when it is not there) */
byte Mode_DelGlobal(const char *module, const char *name);
/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func, args[, name]) with delay a value of the script and func the
 * global of module (its NameError; OBJLIB 4) */
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
/* module.name(args..., **kw) with keyword arguments (OBJLIB 3: the calls with keywords stay Python): the n native
 * arguments and kw a new reference to the dict that the call takes over (NULL: its error is set); the result into out
 * (NULL: dropped) */
byte Mode_CallKw(const char *module, const char *name, nativeVal_t *out, const nativeVal_t *args, int n, PyObject *kw);
/* GateKeeper.DoorFoe(gk, GateKeeper.<group>) with the group object itself (OBJLIB 5: the group lists of GateKeeper keep
 * their identity) */
byte Mode_DoorFoe(const char *gk, const char *group);
/* SSound.PlaySound(pos, name, volume) through its seam (fr7b.py records it), else Mode_PlaySound */
byte Mode_PlaySoundSeam(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume);
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
/* the crazy gambler's call after a mission (unless the save variable SuperDeal.Library is SuperDeal_Twelfth):
 * Speech.AddRemoteCom(msgs[n], "CrazyGambler") for n = MissionsFuncs.GetNumCrazyDealCompleted() from 0 to 3 */
byte Mode_CGCalls(const char *const *msgs);
/* module.var.append(name) with name the global of gmod (a function; OBJLIB 5: the list of another module changed in
 * place) */
byte Mode_AppendG(const char *module, const char *var, const char *gmod, const char *name);
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
/* v is a text and its C text (up to its first 0 byte) is text (Script_Is of the ports) */
byte Mode_IsText(const nativeVal_t *v, const char *text);
/* Scrap.SetTimeSpeed(speed) through its seam (fc-r7b-jy records it) */
byte Mode_TimeSpeed(double speed);
/* Scrap.GetCam(i): the name of the camera of player i (None when there is none) */
void Mode_CamName(int i, nativeVal_t *out);
/* Scrap.GetFloor(y): the floor of the map at the height (the TypeError of "f") */
byte Mode_Floor(const nativeVal_t *y, nativeVal_t *out);
/* abs(v) of the script (PyNumber_Absolute: ints, long ints, floats; the errors of 1.5.2) */
byte Mode_Abs(const nativeVal_t *v, nativeVal_t *out);
/* f(args...) of the script on a value f (a function or method held as NV_OBJ, or the TypeError of a value that is no
 * callable) with the n values of args: the result into out (NULL: dropped) */
byte Mode_CallArgs(const nativeVal_t *f, nativeVal_t *out, const nativeVal_t *args, int n);
/* -v of the script (PyNumber_Negative: ints, long ints, floats; the errors of 1.5.2) */
byte Mode_Neg(const nativeVal_t *v, nativeVal_t *out);

/* SScorer.Fade(id, (r, g, b, a), (r, g, b, a), time) with the time a value of the script (the recorders see the int the
 * script passes; the TypeError of "f" for one that is no number), else Mode_Fade */
byte Mode_FadeV(long id, const int *from, const int *to, const nativeVal_t *time);
/* print a, b, ... as Mode_Print with up to MODE_PRINT_MAX values (Elevator.UseElevator prints 9) */
#define MODE_PRINT_MAX 12
byte Mode_PrintN(const nativeVal_t *const *items, int n);

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

/* dict[key] of a dict variable of the module as the object it is (OBJLIB 5: a list in it changed in place): a new
 * reference, NULL with the error (the NameError of the global, the errors of the subscript) */
PyObject *Mode_AtPy(const char *module, const char *dict, const nativeVal_t *key);
/* the arguments of a keyword function (fmt and names of PyArg_ParseTupleAndKeywords, n of them, up to 8) as native
 * values into a (None when not given, given[i] 0 then; given may be NULL) */
byte Mode_Kw(PyObject *args, PyObject *kw, const char *fmt, char **names, int n, nativeVal_t *a, byte *given);
/* the n arguments of a keyword function freed and the result as the adapter returns it (out NULL or None: None; out
 * freed) */
PyObject *Mode_KwOut(nativeVal_t *a, int n, byte ok, nativeVal_t *out);

/* SScorer.PreloadTexture(name) through its seam */
byte Mode_PreloadTexture(const nativeVal_t *name);

/* apply(f, args) of the script on values (OBJLIB 3, 4: the objects called the Python way): the result into out (NULL:
 * dropped) */
byte Mode_Apply(const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out);
/* the list of a global of the module changed in place as the C ports change it: v appended (first: inserted at 0), and
 * item i deleted (OBJLIB 5: other modules and the cases hold the list) */
byte Mode_ListAddG(const char *module, const char *var, const nativeVal_t *v, byte first);
byte Mode_DelItemG(const char *module, const char *var, int i);
/* ---- the vehicle modules of batch L (Vehicles, VehiclesNPC, ShipMaker, SputnikInterface) ---- */

/* `if module.var:` of a variable of another module (any object, read as its attribute): 1, 0, -1 with the error */
int Mode_ModTrue(const char *module, const char *var);

#endif
