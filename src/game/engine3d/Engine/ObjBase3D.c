/*
 * The base of the 3D objects with a node, original source Engine/objbase3d.cpp (include/scrap/sistpart.h).
 */
#include "scrap/sistpart.h"

void __thiscall CObjBase3D_SetNodo(IObjeto3D *self, CNodo3D *nodo) {
	ObjRef **slot = (ObjRef **)((byte *)self + 8);

	if (nodo) {
		nodo->Refs++;
	}
	CHECK(!*slot || (*slot)->Refs > 1);
	ObjRef_Drop(slot);
	*slot = (ObjRef *)nodo;
}
