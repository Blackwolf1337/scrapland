/*
 * Scheduled Python functions, original source World/ScheduledFunctions.cpp (include/scrap/scheduler.h); in ScrapC
 * function values and the native entries of port functions (stage 4 of docs/objlib-plan.md).
 */
#include <string.h>

#include "scrap/players.h"
#include "scrap/scheduler.h"
#include "scrap/world.h"

/* the DList instance */
static void SchedList_Add(cSchedList *list, cScheduledFunc *const *f) {
	cSchedNode *node = DList_NewNode(list, sizeof(*node));
	node->func = *f;
	DList_LinkFirst(list, node);
}

/* in front of the iterator */
static void SchedList_Insert(cSchedList *list, cScheduledFunc *const *f) {
	cSchedNode *head = list->head;

	if (list->iter == head) {
		SchedList_Add(list, f);
		return;
	}
	CHECK(head != NULL);
	CHECK(list->iter != NULL);
	list->head = list->iter;
	SchedList_Add(list, f);
	list->head = head;
}

static void SchedList_Clear(cSchedList *list) {
	DList_Clear(list);
}

static void SchedList_RemoveIter(cSchedList *list) {
	DList_RemoveIter(list);
}

static void SchedList_RemoveHead(cSchedList *list) {
	DList_RemoveHead(list);
}

/* the calls of Scheduler_Run by kind, value and port entry (the test channel) */
static unsigned long g_SchedulerCalls[2];

static void cScheduledFunc_Free(cScheduledFunc *f) {
	if (f) {
		Native_Free(&f->func);
		Native_Free(&f->args);
		operator_delete(f);
	}
}

/* a new one at time named name, without a function */
static cScheduledFunc *cScheduledFunc_New(float time, const char *name) {
	cScheduledFunc *f = operator_new(sizeof(*f));
	CHECK(f != NULL);
	Native_SetNone(&f->func);
	Native_SetNone(&f->args);
	f->active = 1;
	f->site = NULL;
	f->player = (byte)Players_Bound();
	f->joined = Players_Joined(f->player);
	f->time = time;
	Str_Copy(f->name, name, sizeof(f->name));
	return f;
}

/* the call with the function marked as running */
static void cScheduledFunc_Run(cScheduledFunc *self) {
	byte ok;
	int bound = Players_Bound();
	self->active = 0;
	g_SchedulerCalls[self->site != NULL]++;
	Players_BindJoined(self->player, self->joined);
	if (self->args.type != NV_TUPLE) {
		ok = Native_Raise(NE_TYPE, "argument list must be a tuple");
	} else {
		ok = Port_CallKept(self->site, &self->func, self->args.items, self->args.n, NULL);
	}
	if (!ok) {
		Native_ErrPrint();
	}
	Players_Bind(bound);
	self->active = 1;
}

/* into the list after the ones of the same time or before */
static void Scheduler_Insert(cScheduledFuncStack *self, cScheduledFunc *f) {
	cSchedList *list = &self->list;

	list->iter = list->head;
	cScheduledFunc **it = list->iter ? &list->iter->func : NULL;
	while (it) {
		if (f->time < (*it)->time) {
			SchedList_Insert(list, &f);
			return;
		}
		list->iter = list->iter->next;
		if (list->iter == list->head) {
			break;
		}
		it = &list->iter->func;
	}
	SchedList_Add(list, &f);
	if (list->head) {
		list->head = list->head->next;
	}
}

void Scheduler_Add(cScheduledFuncStack *self, float time, const nativeVal_t *func, const nativeVal_t *params,
	const char *name) {
	cScheduledFunc *f = cScheduledFunc_New(time, name);
	if (params->type == NV_VEC) {
		Native_SetEmpty(&f->args, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(&f->args), params->v[i]);
		}
	} else {
		Native_Copy(&f->args, params);
	}
	if (f->args.type == NV_TUPLE && f->args.n <= PORT_ARGS) {
		f->site = Port_ScheduledOf(func);
	}
	if (!f->site) {
		Native_Copy(&f->func, func);
	}
	Scheduler_Insert(self, f);
}

void Scheduler_Counts(unsigned long *value, unsigned long *entry, byte reset) {
	*value = g_SchedulerCalls[0];
	*entry = g_SchedulerCalls[1];
	if (reset) {
		memset(g_SchedulerCalls, 0, sizeof(g_SchedulerCalls));
	}
}

void Scheduler_Clear(cScheduledFuncStack *self) {
	cSchedList *list = &self->list;

	for (list->iter = list->head; list->iter; list->iter = list->head) {
		cScheduledFunc_Free(list->iter->func);
		SchedList_RemoveHead(list);
	}
}

void Scheduler_Delete(cScheduledFuncStack *self, const char *name) {
	cSchedList *list = &self->list;

	list->iter = list->head;
	while (list->iter) {
		cScheduledFunc *f = list->iter->func;
		if (!strcmp(name, f->name) && f->active) {
			cScheduledFunc_Free(f);
			SchedList_RemoveIter(list);
		} else {
			list->iter = list->iter->next;
			if (list->iter == list->head) {
				return;
			}
		}
	}
}

void Scheduler_Run(cScheduledFuncStack *self, float now) {
	cSchedList *list = &self->list;

	/* differs from the original: the entry leaves the list before its call. The original removed the head after the
	 * call, which is a function the call scheduled before it (time 0) if there is one, and freed the running one
	 * while its node stayed in the list. Scheduler_Delete passes over a running entry, so leaving the list keeps
	 * DeleteScheduledFuncs during the call as it was. */
	while (list->head && list->head->func->time < now) {
		cScheduledFunc *f = list->head->func;
		SchedList_RemoveHead(list);
		cScheduledFunc_Run(f);
		cScheduledFunc_Free(f);
	}
	list->iter = list->head;
}

void cScheduledFuncStack_dtor(void *stack) {
	cScheduledFuncStack *self = stack;

	Scheduler_Clear(self);
	SchedList_Clear(&self->list);
	DList_FreePool(&self->list);
}

/* the vtable of cScheduledFuncStack */
