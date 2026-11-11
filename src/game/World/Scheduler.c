/*
 * Scheduled Python functions, original source World/ScheduledFunctions.cpp (include/scrap/scheduler.h), and in ScrapC
 * the native entries of port functions (stage 4 of docs/objlib-plan.md).
 */
#include <string.h>

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

/* OBJLIB 7: the calls of Scheduler_Run by kind, Python and port entry (the test channel) */
static unsigned long g_SchedulerCalls[2];

static void cScheduledFunc_Free(cScheduledFunc *f) {
	if (f) {
		Py_XDECREF(f->func);
		Py_XDECREF(f->params);
		Native_Free(&f->args);
		operator_delete(f);
	}
}

/* a new one at time named name, without a function */
static cScheduledFunc *cScheduledFunc_New(float time, const char *name) {
	cScheduledFunc *f = operator_new(sizeof(*f));
	CHECK(f != NULL);
	f->func = NULL;
	f->params = NULL;
	f->active = 1;
	f->site = NULL;
	Native_SetNone(&f->args);
	f->time = time;
	Str_Copy(f->name, name, sizeof(f->name));
	return f;
}

/* the call of a port entry as the Python call of its adapter: the items of the tuple as objects, the native values as
 * the adapter would get them; 1, or 0 with the error */
static byte cScheduledFunc_CallEntry(const cScheduledFunc *self) {
	nativeArg_t a[PORT_ARGS];
	if (!self->params) {
		return Port_BoundCall(self->site, NULL, self->args.items, self->args.n, NULL, NULL);
	}
	int n = PyTuple_Size(self->params);
	for (int i = 0; i < n; i++) {
		a[i].type = 'O';
		a[i].o = PyTuple_GetItem(self->params, i);
	}
	return Port_BoundCall(self->site, a, NULL, n, NULL, NULL);
}

/* the call with the function marked as running */
static void cScheduledFunc_Run(cScheduledFunc *self) {
	self->active = 0;
	if (self->site) {
		g_SchedulerCalls[1]++;
		if (!cScheduledFunc_CallEntry(self)) {
			Native_ErrPrint();
		}
	} else {
		g_SchedulerCalls[0]++;
		PyObject *r = PyEval_CallObject(self->func, self->params);
		if (!r) {
			Native_ErrPrint();
		} else {
			Py_DECREF(r);
		}
	}
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

void Scheduler_Add(cScheduledFuncStack *self, float time, PyObject *func, PyObject *params, const char *name) {
	CHECK(func != NULL);
	CHECK(params != NULL);
	cScheduledFunc *f = cScheduledFunc_New(time, name);
	if (PyTuple_Check(params) && PyTuple_Size(params) <= PORT_ARGS) {
		f->site = Port_Scheduled(func);
	}
	if (!f->site) {
		f->func = func;
		Py_INCREF(func);
	}
	f->params = params;
	Py_INCREF(params);
	Scheduler_Insert(self, f);
}

void Scheduler_AddEntry(cScheduledFuncStack *self, float time, portSite_t *site, const nativeVal_t *args, int n,
	const char *name) {
	CHECK(site != NULL);
	CHECK(n >= 0 && n <= PORT_ARGS);
	cScheduledFunc *f = cScheduledFunc_New(time, name);
	f->site = site;
	Native_SetEmpty(&f->args, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&f->args), &args[i]);
	}
	Scheduler_Insert(self, f);
}

void Scheduler_Counts(unsigned long *python, unsigned long *entry, byte reset) {
	*python = g_SchedulerCalls[0];
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
