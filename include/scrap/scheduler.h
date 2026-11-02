/*
 * The scheduled Python functions of the world (original source World/ScheduledFunctions.cpp, Scrap.AddScheduledFunc):
 * a cScheduledFuncStack keeps them sorted by their time in a Global/DList.h list.
 */
#ifndef SCRAP_SCHEDULER_H
#define SCRAP_SCHEDULER_H

#include "scrap/dlist.h"
#include "scrap/worldparts.h"
#include "scrap/python.h"

typedef struct cScheduledFunc {
	PyObject *func;			/* 0x00 */
	PyObject *params;		/* 0x04 */
	float time;			/* 0x08 */
	char name[0x80];		/* 0x0c */
	byte active;			/* 0x8c 0 while it runs, so Scrap.RemoveScheduledFunc leaves it */
	byte pad_8d[3];
} cScheduledFunc;

extern const cScheduledFuncStackVtbl cScheduledFuncStack_vftable;
#define CSCHEDULEDFUNCSTACK_VFTABLE (&cScheduledFuncStack_vftable)

void cScheduledFunc_dtor(cScheduledFunc *self);
cScheduledFuncStack *cScheduledFuncStack_Delete(cScheduledFuncStack *self, byte flags);
void Scheduler_Add(cScheduledFuncStack *self, float time, PyObject *func, PyObject *params, const char *name);
/* the one named name, unless it is running */
void Scheduler_Delete(cScheduledFuncStack *self, const char *name);
void Scheduler_Clear(cScheduledFuncStack *self);
/* calls and removes the functions due before now */
void Scheduler_Run(cScheduledFuncStack *self, float now);
void cScheduledFunc_Set(cScheduledFunc *self, float time, PyObject *func, PyObject *params, const char *name);

/* the DList instance */
void SchedList_Add(cSchedList *list, cScheduledFunc *const *f);
/* in front of the iterator */
void SchedList_Insert(cSchedList *list, cScheduledFunc *const *f);
void SchedList_Clear(cSchedList *list);
void SchedList_RemoveIter(cSchedList *list);
void SchedList_RemoveHead(cSchedList *list);

#endif
