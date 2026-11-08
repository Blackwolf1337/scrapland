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

/* ShipEdit.DrawBackground(id, elem, posX=64, ...), also SputnikInterface.DrawBackground (the same script code) */
PyObject *ShipEdit_DrawBackground(PyObject *self, PyObject *args, PyObject *kw);
/* HangarTab.DrawBackground(id, posX=64, ...): ShipEdit's DrawBackground without elem (the sprite Background) */
PyObject *HangarTab_DrawBackground(PyObject *self, PyObject *args, PyObject *kw);

/*
 * ---- native helpers (docs/objlib-plan.md, stage 2) ----
 *
 * The same lines for the functions that work on native values (scrap/scriptmodes.h): each returns 1, or 0 with the
 * error of the script raised; conditions 1, 0, or -1 with the error. The ships are handle values (NV_OBJ, OBJLIB 7),
 * the lists of ShipEdit and the variables of HangarTab stay in the module dicts (OBJLIB 5).
 */

/* ShipEdit.IsEditingMultiplayer() through its ref (own: as ShipEdit calls its own global): 1, 0, -1 */
int Sn_Multi(byte own);
/* SScorer.Set(id, item, var, v) as the script calls it by name (through the seam), item a value of the script (a text
 * without 0 bytes in C, any other value by name for the TypeError of SScorer.Set) or a C text */
byte Sn_Set(int id, const nativeVal_t *item, const char *var, const nativeVal_t *v);
byte Sn_SetC(int id, const char *item, const char *var, const nativeVal_t *v);
/* the same with Scrap.GetLangStr(key) % UseTxt of module (its global) */
byte Sn_SetUse(int id, const nativeVal_t *item, const char *var, const char *key, const char *module);
byte Sn_SetUseC(int id, const char *item, const char *var, const char *key, const char *module);
/* Menu.<fn>(id, a, b), fn named at run time, through the entry of the function Menu holds (MODE_CBATTR), of two C
 * texts or two values; `from Menu import name` (OBJLIB 6) */
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
