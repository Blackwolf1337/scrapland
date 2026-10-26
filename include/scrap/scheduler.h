/*
 * The scheduled Python functions of the world (original source World/ScheduledFunctions.cpp, Scrap.AddScheduledFunc):
 * a cScheduledFuncStack keeps them sorted by their time in a Global/DList.h list.
 */
#ifndef SCRAP_SCHEDULER_H
#define SCRAP_SCHEDULER_H

#include "scrap/dlist.h"
#include "scrap/python.h"

typedef struct cScheduledFunc {
	PyObject *func;			/* 0x00 */
	PyObject *params;		/* 0x04 */
	float time;			/* 0x08 */
	char name[0x80];		/* 0x0c */
	byte active;			/* 0x8c 0 while it runs, so Scrap.RemoveScheduledFunc leaves it */
	byte pad_8d[3];
} cScheduledFunc;
SD_SIZE(cScheduledFunc, 0x90);

typedef struct cSchedNode {
	struct cSchedNode *next;	/* 0x0 */
	struct cSchedNode *prev;	/* 0x4 */
	cScheduledFunc *func;		/* 0x8 */
} cSchedNode;
SD_SIZE(cSchedNode, 0xc);

typedef DLIST(cSchedNode) cSchedList;

typedef struct cScheduledFuncStack {
	void *vftable;			/* 0x00 */
	cSchedList list;		/* 0x04 */
} cScheduledFuncStack;
SD_SIZE(cScheduledFuncStack, 0x1c);

cScheduledFuncStack *__thiscall cScheduledFuncStack_Delete(cScheduledFuncStack *self, byte flags);
void __thiscall Scheduler_Add(cScheduledFuncStack *self, float time, PyObject *func, PyObject *params, const char *name);
/* the one named name, unless it is running */
void __thiscall Scheduler_Delete(cScheduledFuncStack *self, const char *name);
void __fastcall Scheduler_Clear(cScheduledFuncStack *self);
/* calls and removes the functions due before now */
void __thiscall Scheduler_Run(cScheduledFuncStack *self, float now);
void __thiscall cScheduledFunc_Set(cScheduledFunc *self, float time, PyObject *func, PyObject *params, const char *name);
void __fastcall cScheduledFunc_Call(cScheduledFunc *self);

/* the DList instance */
void __thiscall SchedList_Add(cSchedList *list, cScheduledFunc *const *f);
/* in front of the iterator */
void __thiscall SchedList_Insert(cSchedList *list, cScheduledFunc *const *f);
void __fastcall SchedList_Clear(cSchedList *list);
void __fastcall SchedList_RemoveIter(cSchedList *list);
void __fastcall SchedList_RemoveHead(cSchedList *list);

#endif
