/*
 * The scheduled Python functions of the world (original source World/ScheduledFunctions.cpp, Scrap.AddScheduledFunc):
 * a cScheduledFuncStack keeps them sorted by their time in a Global/DList.h list.
 *
 * ScrapC (stage 4 of docs/objlib-plan.md): a function that is the port's own C function when it is scheduled
 * (Port_Scheduled) is kept as its native entry, called as the Python call of its adapter (Port_BoundCall) on the
 * native values it was given (Scheduler_AddEntry) or on the items of its tuple. Anything else (a recorder, a lambda,
 * a bound method, an engine function, everything under SCRAP_PORT_SLOW=1) is kept as the Python callable and tuple.
 */
#ifndef SCRAP_SCHEDULER_H
#define SCRAP_SCHEDULER_H

#include "scrap/dlist.h"
#include "scrap/worldparts.h"
#include "scrap/python.h"
#include "scrap/scriptnative.h"

typedef struct cScheduledFunc {
	PyObject *func;			/* the Python callable; NULL for a port entry */
	PyObject *params;		/* its tuple, or the tuple of a port entry's arguments; NULL for native ones */
	float time;
	char name[0x80];
	byte active;			/* 0 while it runs, so Scrap.RemoveScheduledFunc leaves it */
	byte pad_8d[3];
	/* ScrapC: a port entry, its ref and counters (Port_Scheduled), else NULL */
	portSite_t *site;
	/* ScrapC: the native arguments of a port entry (an NV_TUPLE) when params is NULL, else None */
	nativeVal_t args;
} cScheduledFunc;

/* func(*params) at time: the entry when func is the port's own function and params a tuple of at most PORT_ARGS
 * items, else the Python call */
void Scheduler_Add(cScheduledFuncStack *self, float time, PyObject *func, PyObject *params, const char *name);
/* the entry of site (Port_Scheduled) at time on the n values args (copied) */
void Scheduler_AddEntry(cScheduledFuncStack *self, float time, portSite_t *site, const nativeVal_t *args, int n,
	const char *name);
/* the one named name, unless it is running */
void Scheduler_Delete(cScheduledFuncStack *self, const char *name);
void Scheduler_Clear(cScheduledFuncStack *self);
/* calls and removes the functions due before now */
void Scheduler_Run(cScheduledFuncStack *self, float now);
/* OBJLIB 7: the calls of Scheduler_Run by kind since the start or the last reset (the test channel's native ports) */
void Scheduler_Counts(unsigned long *python, unsigned long *entry, byte reset);

#endif
