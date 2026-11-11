/*
 * The script handle of an entity and its Python type cPyEntity (type object). A handle is a cEntityRef: a link
 * (Global/Nto1Links.h) in the ring of the entity, whose target becomes NULL when the entity is gone, and the name the
 * handle was made with. Native values hold it as the kind NV_ENT (scrap/scriptnative.h), with no Python object; the
 * cPyEntity object is its Python face (OBJLIB 7: until the tables go), made when the handle goes to Python.
 *
 * Identity: the handle is the cEntityRef. Two handles of one entity are two refs (as Scrap.GetEntity gave two
 * objects): not equal, two dict keys. A ref has at most one face at a time, so a handle that comes back from native
 * values is the same Python object as long as Python holds it (`is`, ==, dict keys and the address of repr stay).
 */
#ifndef SCRAP_PYENTITY_H
#define SCRAP_PYENTITY_H

#include "scrap/entity.h"
#include "scrap/python.h"

typedef struct cEntityRef {
	Nto1Node link;			/* target the cEntity, data the cEntityRef */
	char name[0x80];
	int holders;			/* the native values that hold the handle, and its face */
	struct cPyEntity *face;		/* the Python object of the handle while one lives (it counts as one holder), else NULL */
} cEntityRef;

typedef struct cPyEntity {
	PyObject_HEAD
	cEntityRef *ref;
} cPyEntity;

static inline cEntity *PyEntity_Get(PyObject *self) {
	return ((cPyEntity *)self)->ref->link.target;
}

/* a new handle of the entity with that name, held once (EntRef_Drop it): NULL without one */
cEntityRef *EntRef_New(const char *name);
/* one more holder of the handle: ref */
cEntityRef *EntRef_Keep(cEntityRef *ref);
/* a holder less; the last one takes the link out of the ring and frees the handle */
void EntRef_Drop(cEntityRef *ref);

/* o is an entity handle (cPyEntity) */
byte PyEntity_Check(PyObject *o);

/* the Python object of the handle (a new reference): its face, a new one when it has none; NULL with the error */
PyObject *PyEntity_Face(cEntityRef *ref);

/* the handle of the entity with that name as a new Python object, NULL without one (no Python error set) */
PyObject *PyEntity_New(const char *name);

/* a script variable of the entity from a Python value, as `handle.name = v` sets it: 0, or -1 with the message
 * printed (ref names the entity in it) */
int PyEntity_SetVar(cEntity *e, const char *ref, const char *name, PyObject *v);

#endif
