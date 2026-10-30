/*
 * The base of the 3D objects with a node, original source Engine/objbase3d.cpp (include/scrap/particlesystem.h).
 */
#include "scrap/particlesystem.h"

void __thiscall CObjBase3D_SetNode(CObjBase3D *self, CNode3D *node) {
	ObjRef **slot = (ObjRef **)&self->node;

	if (node) {
		node->Refs++;
	}
	CHECK(!*slot || (*slot)->Refs > 1);
	ObjRef_Drop(slot);
	*slot = (ObjRef *)node;
}
