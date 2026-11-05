/*
 * The Python type cPyEntity (type object): a script handle of an entity. The handle owns a link
 * (Global/Nto1Links.h) in the ring of the entity, its target becomes NULL when the entity is gone. The link keeps
 * the name the handle was made with.
 */
#ifndef SCRAP_PYENTITY_H
#define SCRAP_PYENTITY_H

#include "scrap/entity.h"
#include "scrap/python.h"

typedef struct cEntityRef {
	Nto1Node link;			/* target the cEntity, data the cEntityRef */
	char name[0x80];
} cEntityRef;

typedef struct cPyEntity {
	PyObject_HEAD
	cEntityRef *ref;
} cPyEntity;

static inline cEntity *PyEntity_Get(PyObject *self) {
	return ((cPyEntity *)self)->ref->link.target;
}

/* the handle of the entity with that name, NULL without one (no Python error set) */
PyObject *PyEntity_New(const char *name);

/* a script variable of the entity from a Python value, as `handle.name = v` sets it: 0, or -1 with the message
 * printed (ref names the entity in it) */
int PyEntity_SetVar(cEntity *e, const char *ref, const char *name, PyObject *v);

#endif
