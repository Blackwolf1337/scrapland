/*
 * The Python type cPyEntity (type object rva 0x428e18): a script handle of an entity. The handle owns a link
 * (Global/Nto1Links.h) in the ring of the entity, its target becomes NULL when the entity is gone. The link keeps
 * the name the handle was made with.
 */
#ifndef SCRAP_PYENTITY_H
#define SCRAP_PYENTITY_H

#include "scrap/entity.h"
#include "scrap/python.h"

typedef struct cEntityRef {
	Nto1Node link;			/* 0x0 target the cEntity, data the cEntityRef */
	char name[0x80];		/* 0x10 */
} cEntityRef;
SD_OFFSET(cEntityRef, name, 0x10);
SD_SIZE(cEntityRef, 0x90);

typedef struct cPyEntity {
	PyObject_HEAD
	cEntityRef *ref;		/* 0x8 */
} cPyEntity;
SD_OFFSET(cPyEntity, ref, 0x8);

static inline cEntity *PyEntity_Get(PyObject *self) {
	return ((cPyEntity *)self)->ref->link.target;
}

#define cPyEntity_Type GAME_GLOBAL(PyTypeObject, cPyEntity_Type)
#define cPyEntity_methods (&GAME_GLOBAL_IN(PyMethodDef, cPyEntity_methods, 0))

/* the handle of the entity with that name, NULL without one (no Python error set) */
PyObject *PyEntity_New(const char *name);

#endif
