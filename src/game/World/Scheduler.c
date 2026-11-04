/*
 * Scheduled Python functions, original source World/ScheduledFunctions.cpp (include/scrap/scheduler.h).
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

static void cScheduledFunc_Free(cScheduledFunc *f) {
	if (f) {
		Py_XDECREF(f->func);
		Py_XDECREF(f->params);
		operator_delete(f);
	}
}

static void cScheduledFunc_Set(cScheduledFunc *self, float time, PyObject *func, PyObject *params, const char *name) {
	CHECK(self->func == NULL);
	CHECK(self->params == NULL);
	CHECK(func != NULL);
	CHECK(params != NULL);
	self->time = time;
	Str_Copy(self->name, name, sizeof(self->name));
	self->params = params;
	self->func = func;
	Py_INCREF(func);
	Py_INCREF(params);
}

/* the call with the function marked as running */
static void cScheduledFunc_Run(cScheduledFunc *self) {
	self->active = 0;
	PyObject *r = PyEval_CallObject(self->func, self->params);
	if (!r) {
		PyErr_Print();
	} else {
		Py_DECREF(r);
	}
	self->active = 1;
}

void Scheduler_Add(cScheduledFuncStack *self, float time, PyObject *func, PyObject *params, const char *name) {
	cSchedList *list = &self->list;

	list->iter = list->head;
	cScheduledFunc **it = list->iter ? &list->iter->func : NULL;
	cScheduledFunc *f = operator_new(sizeof(*f));
	if (f) {
		f->func = NULL;
		f->params = NULL;
		f->time = 0.0f;
		f->name[0] = '\0';
		f->active = 1;
	}
	cScheduledFunc_Set(f, time, func, params, name);
	while (it) {
		if (time < (*it)->time) {
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

	list->iter = list->head;
	cScheduledFunc **it = list->iter ? &list->iter->func : NULL;
	while (it && (*it)->time < now) {
		cScheduledFunc *f = *it;
		cSchedNode *iter = list->iter;
		cScheduledFunc_Run(f);
		list->iter = iter;
		SchedList_RemoveHead(list);
		list->iter = list->head;
		it = list->iter ? &list->iter->func : NULL;
		cScheduledFunc_Free(f);
	}
}

void cScheduledFuncStack_dtor(void *stack) {
	cScheduledFuncStack *self = stack;

	Scheduler_Clear(self);
	SchedList_Clear(&self->list);
	DList_FreePool(&self->list);
}

/* the vtable of cScheduledFuncStack */
