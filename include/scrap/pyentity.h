/*
 * The script handle of an entity (the cPyEntity object of the original). A handle is a cEntityRef: a link
 * (Global/Nto1Links.h) in the ring of the entity, whose target becomes NULL when the entity is gone, and the name the
 * handle was made with. Native values hold it as the kind NV_ENT (scrap/scriptnative.h).
 *
 * Identity: the handle is the cEntityRef. Two handles of one entity are two refs (as Scrap.GetEntity gave two
 * objects): not equal, two dict keys (`is`, ==, dict keys and the address of repr go by the ref).
 */
#ifndef SCRAP_PYENTITY_H
#define SCRAP_PYENTITY_H

#include "scrap/entity.h"

typedef struct cEntityRef {
	Nto1Node link;			/* target the cEntity, data the cEntityRef */
	char name[0x80];
	int holders;			/* the native values that hold the handle */
} cEntityRef;

/* a new handle of the entity with that name, held once (EntRef_Drop it): NULL without one */
cEntityRef *EntRef_New(const char *name);
/* one more holder of the handle: ref */
cEntityRef *EntRef_Keep(cEntityRef *ref);
/* a holder less; the last one takes the link out of the ring and frees the handle */
void EntRef_Drop(cEntityRef *ref);

#endif
