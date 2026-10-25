/*
 * Action classes of animated objects (original source: Actions/). A class holds named variables and actions;
 * SAct works on one "current" class and action at a time.
 */
#ifndef SCRAP_ACTIONS_H
#define SCRAP_ACTIONS_H

#include "scrap/game.h"

typedef struct cActClass cActClass;
typedef struct cActClassList cActClassList;
typedef struct cAction cAction;

/* type of a variable as returned by the Var functions */
enum { ACTVAR_NONE = 0, ACTVAR_STRING = 1, ACTVAR_DOUBLE = 2, ACTVAR_INT = 3 };

cActClass *__thiscall cActClassList_Create(cActClassList *list, const char *name);
cActClass *__thiscall cActClassList_Get(cActClassList *list, const char *name);
byte __thiscall cActClassList_Del(cActClassList *list, const char *name);

cAction *__thiscall cActClass_CreateAction(cActClass *cls, const char *name);
cAction *__thiscall cActClass_GetAction(cActClass *cls, const char *name);
/* set != 0: writes the string *s, or the number *d / *i when s is NULL; set == 0: reads into all three.
   Returns the ACTVAR_ type (0 if not found). */
int __thiscall cActClass_Var(cActClass *cls, const char *name, int set, char **s, double *d, int *i);
int __thiscall cAction_Var(cAction *act, const char *name, int set, char **s, double *d, int *i);

#define g_pActClassList GAME_DATA(cActClassList *, 0x444080)
#define g_pAuxActClass GAME_DATA(cActClass *, 0x444084)	/* SAct's current class */
#define g_pAuxAction GAME_DATA(cAction *, 0x444088)	/* SAct's current action */

#endif
