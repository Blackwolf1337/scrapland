/*
 * The scheduled Python functions of the world (original source World/ScheduledFunctions.cpp, Scrap.AddScheduledFunc):
 * a cScheduledFuncStack keeps them sorted by their time in a Global/DList.h list.
 *
 * ScrapC: the function and its arguments are native values (stage 7e1). A port function given with a tuple of at most
 * PORT_ARGS items is kept as its native entry (Port_ScheduledOf, stage 4 of docs/objlib-plan.md), called as the Python
 * call of its adapter (Port_BoundCall) on the items; any other function value (a lambda, a recorder, a method, a
 * class, an engine function, everything under SCRAP_PORT_SLOW=1) is called natively on them (Port_CallKept).
 */
#ifndef SCRAP_SCHEDULER_H
#define SCRAP_SCHEDULER_H

#include "scrap/dlist.h"
#include "scrap/worldparts.h"
#include "scrap/scriptnative.h"

typedef struct cScheduledFunc {
	nativeVal_t func;		/* ScrapC: the function value (the Python callable of the original); None for an entry */
	nativeVal_t args;		/* ScrapC: its arguments, an NV_TUPLE; another value as given (the call's TypeError) */
	float time;
	char name[0x80];
	byte active;			/* 0 while it runs, so Scrap.RemoveScheduledFunc leaves it */
	/* ScrapC, the instance: the player bound when it was scheduled and that player's join (include/scrap/players.h),
	 * bound again for the call */
	byte player;
	uint16_t joined;
	/* ScrapC: a port entry, its ref and counters (Port_ScheduledOf), else NULL */
	portSite_t *site;
} cScheduledFunc;

/* func(*params) at time, both values copied: the entry when func is a port function and params a tuple (a vector: the
 * tuple of its three floats) of at most PORT_ARGS items, else the native call of the value; a params that is no tuple
 * fails at the call with the TypeError of ceval.c Eval_CallObject */
void Scheduler_Add(cScheduledFuncStack *self, float time, const nativeVal_t *func, const nativeVal_t *params,
	const char *name);
/* the one named name, unless it is running */
void Scheduler_Delete(cScheduledFuncStack *self, const char *name);
void Scheduler_Clear(cScheduledFuncStack *self);
/* calls and removes the functions due before now */
void Scheduler_Run(cScheduledFuncStack *self, float now);
/* the calls of Scheduler_Run by kind since the start or the last reset (the test channel's native ports) */
void Scheduler_Counts(unsigned long *value, unsigned long *entry, byte reset);

#endif
