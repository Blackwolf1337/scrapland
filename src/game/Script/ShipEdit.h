/*
 * The ship editor in C (ShipEdit.c and HangarTab.c, scripts/shipedit/shipedit.py and hangartab.py): what the two
 * modules share. The layout of the tabs, the background and the Hangar panel is data/menus/shipedit.def.
 */
#ifndef SCRAP_SHIPEDIT_H
#define SCRAP_SHIPEDIT_H

#include "scrap/script.h"

#define SE "ShipEdit"
#define HT "HangarTab"

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

/* HangarTab.DrawBackground(id, posX=64, ...): ShipEdit's DrawBackground without elem (the sprite Background) */
PyObject *HangarTab_DrawBackground(PyObject *self, PyObject *args, PyObject *kw);

/* a section of data/menus/shipedit.def with the values of vars (a dict, NULL for none); UseTxt of module for the step
 * use: 0 with the error set, the frame failed */
byte Se_Def(scriptFrame_t *f, int id, const char *section, PyObject *vars, const char *module);

#endif
