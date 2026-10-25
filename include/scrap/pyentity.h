/*
 * The Python type cPyEntity (type object rva 0x428e18): a script handle of an entity. The handle points to a
 * reference object whose entity pointer becomes NULL when the entity is gone.
 */
#ifndef SCRAP_PYENTITY_H
#define SCRAP_PYENTITY_H

#include "scrap/entity.h"
#include "scrap/python.h"

typedef struct cEntityRef {
	byte pad_0[0x8];
	cEntity *entity;		/* 0x8 */
} cEntityRef;
SD_OFFSET(cEntityRef, entity, 0x8);

typedef struct cPyEntity {
	PyObject_HEAD
	cEntityRef *ref;		/* 0x8 */
} cPyEntity;
SD_OFFSET(cPyEntity, ref, 0x8);

static inline cEntity *PyEntity_Get(PyObject *self) {
	return ((cPyEntity *)self)->ref->entity;
}

#endif
