/*
 * The ship editor in C (ShipEdit.c and HangarTab.c, scripts/shipedit/shipedit.py and hangartab.py): what the two
 * modules share. The layout of the tabs, the background and the Hangar panel is data/menus/shipedit.def.
 */
#ifndef SCRAP_SHIPEDIT_H
#define SCRAP_SHIPEDIT_H

#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define SE "ShipEdit"
#define HT "HangarTab"

/* a helper that is a function of the script runs in its own frame g, so its values go when it returns like the locals
 * of the Python function: SE_SUB(f, Helper(&g, ...)); an error in g fails f. Se_JoinValue keeps the result v of g in f */
#define SE_SUB(f, call) do { if (!(f)->failed) { scriptFrame_t g = { 0 }; call; Se_Join((f), &g); } } while (0)
void Se_Join(scriptFrame_t *f, scriptFrame_t *g);
PyObject *Se_JoinValue(scriptFrame_t *f, scriptFrame_t *g, PyObject *v);

/* SScorer.Set(id, item, var, value) of a script line (nothing after the frame failed): a number, a text, a Python
 * value (item a C text or a Python value), the language text key and key % UseTxt of module (its global) */
void Se_SetI(scriptFrame_t *f, int id, const char *item, const char *var, double v);
void Se_SetS(scriptFrame_t *f, int id, const char *item, const char *var, const char *s);
void Se_SetO(scriptFrame_t *f, int id, const char *item, const char *var, PyObject *v);
void Se_SetOO(scriptFrame_t *f, int id, PyObject *item, const char *var, PyObject *v);
void Se_SetLang(scriptFrame_t *f, int id, const char *item, const char *var, const char *key);
void Se_SetUse(scriptFrame_t *f, int id, PyObject *item, const char *var, const char *key, const char *module);
/* the same value onto the variable of each item of the list (it ends with NULL) */
void Se_SetAll(scriptFrame_t *f, int id, const char *const *items, const char *var, PyObject *v);

/* Menu.LinkLR / LinkUD / UnLinkLR / UnLinkUD (fn) of two items */
void Se_Link(scriptFrame_t *f, const char *fn, int id, const char *a, const char *b);
void Se_LinkO(scriptFrame_t *f, const char *fn, int id, PyObject *a, PyObject *b);
/* the tabs of the list tabs: the ring of links (fn "LinkLR" or "UnLinkLR"), and the tabs after the first one switched
 * on (ChangeTab callable, white text) or off (grey, no callback) */
void Se_TabRing(scriptFrame_t *f, int id, PyObject *tabs, const char *fn);
void Se_TabsOn(scriptFrame_t *f, int id, PyObject *tabs, byte on);

/* ShipEdit.IsEditingMultiplayer() as the modules call it (their own global, or the attribute of ShipEdit): 1, 0, -1 */
int Se_Multi(scriptFrame_t *f, byte own);
/* the module variable of the other module: its attribute (HangarTab.prevShip from ShipEdit) */
PyObject *Se_Mod(scriptFrame_t *f, const char *module, const char *name);

/* (dummy[2][0], dummy[2][1] + 180 + extra, 0) of editShipPosDummy: the ship faces the camera (extra NULL for none) */
PyObject *Se_FrontAng(scriptFrame_t *f, PyObject *dummy, PyObject *extra);

/* ShipEdit.DrawBackground(id, elem, posX=64, ...), also SputnikInterface.DrawBackground (the same script code) */
PyObject *ShipEdit_DrawBackground(PyObject *self, PyObject *args, PyObject *kw);
/* HangarTab.DrawBackground(id, posX=64, ...): ShipEdit's DrawBackground without elem (the sprite Background) */
PyObject *HangarTab_DrawBackground(PyObject *self, PyObject *args, PyObject *kw);

/* a section of data/menus/shipedit.def with the values of vars (a dict, NULL for none); UseTxt of module for the step
 * use: 0 with the error set, the frame failed */
byte Se_Def(scriptFrame_t *f, int id, const char *section, PyObject *vars, const char *module);

/*
 * ---- native helpers (docs/objlib-plan.md, stage 2) ----
 *
 * The same lines for the functions that work on native values (scrap/scriptmodes.h): each returns 1, or 0 with the
 * error of the script raised; conditions 1, 0, or -1 with the error. The ships are handle values (NV_OBJ, OBJLIB 7),
 * the lists of ShipEdit and the variables of HangarTab stay in the module dicts (OBJLIB 5), the calls of the modules
 * by name (OBJLIB 3).
 */

/* ShipEdit.IsEditingMultiplayer() by name (own: as ShipEdit calls its own global): 1, 0, -1 */
int Sn_Multi(byte own);
/* SScorer.Set(id, item, var, v) as the script calls it by name (through the seam), item a value of the script (a text
 * without 0 bytes in C, any other value by name for the TypeError of SScorer.Set) or a C text */
byte Sn_Set(int id, const nativeVal_t *item, const char *var, const nativeVal_t *v);
byte Sn_SetC(int id, const char *item, const char *var, const nativeVal_t *v);
/* the same with Scrap.GetLangStr(key) % UseTxt of module (its global) */
byte Sn_SetUse(int id, const nativeVal_t *item, const char *var, const char *key, const char *module);
byte Sn_SetUseC(int id, const char *item, const char *var, const char *key, const char *module);
/* Menu.<fn>(id, a, b) by name (OBJLIB 3) of two C texts or two values; `from Menu import name` (OBJLIB 6) */
byte Sn_Link(const char *fn, int id, const char *a, const char *b);
byte Sn_LinkV(const char *fn, int id, const nativeVal_t *a, const nativeVal_t *b);
byte Sn_From(const char *name);
/* prefix + v (+ suffix when given) of the script into out; an empty prefix is no operand (v + suffix) */
byte Sn_Cat(const char *prefix, const nativeVal_t *v, const char *suffix, nativeVal_t *out);
/* v[1:] of the script (a list that holds handles too) */
byte Sn_Rest(const nativeVal_t *v, nativeVal_t *out);
/* the ring of links of the tabs (fn "LinkLR" or "UnLinkLR"), the tabs after the first one on or off */
byte Sn_TabRing(int id, const nativeVal_t *tabs, const char *fn);
byte Sn_TabsOn(int id, const nativeVal_t *tabs, byte on);
/* module.var[key] (any item: a handle, None) of a list or dict of another module */
byte Sn_At(const char *module, const char *var, const nativeVal_t *key, nativeVal_t *out);
/* (dummy[2][0], dummy[2][1] + 180 + extra, 0) (extra NULL for none) */
byte Sn_FrontAng(const nativeVal_t *dummy, const nativeVal_t *extra, nativeVal_t *out);
/* the $names of a section: n pairs of a C name and a value (const nativeVal_t *); a new dict, NULL with the error */
PyObject *Sn_Vars(int n, ...);
/* a section of shipedit.def with the values of vars (borrowed, NULL for none), UseTxt of module for the step use */
byte Sn_Def(int id, const char *section, PyObject *vars, const char *module);

#endif
